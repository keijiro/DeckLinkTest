#pragma once

#include "Common.h"
#include "MemoryBackedFrame.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

class Receiver final : public IDeckLinkInputCallback
{
public:

    Receiver(IDeckLinkInput* input)
        : refCount_(1), input_(input)
    {
        AssertSuccess(CoCreateInstance(
            CLSID_CDeckLinkVideoConversion, nullptr, CLSCTX_ALL,
            IID_IDeckLinkVideoConversion, reinterpret_cast<void**>(&converter_)
        ));
    }

    ~Receiver()
    {
        while (!frameQueue_.empty())
        {
            frameQueue_.front()->Release();
            frameQueue_.pop();
        }
        converter_->Release();
    }

    MemoryBackedFrame* PopFrameSync()
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCondition_.wait(lock);

        if (frameQueue_.empty()) return nullptr;
        
        auto frame = frameQueue_.front();
        frameQueue_.pop();
        return frame;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        if (iid == IID_IUnknown)
        {
            *ppv = this;
            return S_OK;
        }

        if (iid == IID_IDeckLinkInputCallback)
        {
            *ppv = (IDeckLinkInputCallback*)this;
            return S_OK;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return refCount_.fetch_add(1);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto val = refCount_.fetch_sub(1);
        if (val == 0) delete this;
        return val;
    }

    HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
        BMDVideoInputFormatChangedEvents notificationEvents,
        IDeckLinkDisplayMode* newDisplayMode,
        BMDDetectedVideoInputFormatFlags detectedSignalFlags
    ) override
    {
        input_->PauseStreams();
        input_->EnableVideoInput(
            newDisplayMode->GetDisplayMode(),
            FormatFlagsToPixelFormat(detectedSignalFlags),
            bmdVideoInputEnableFormatDetection
        );
        input_->FlushStreams();
        input_->StartStreams();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(
        IDeckLinkVideoInputFrame* videoFrame,
        IDeckLinkAudioInputPacket* audioPacket
    ) override
    {
        if (videoFrame != nullptr)
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            frameQueue_.push(new MemoryBackedFrame(videoFrame, converter_));
            queueCondition_.notify_all();
        }
        return S_OK;
    }

private:

    std::atomic<ULONG> refCount_;

    IDeckLinkInput* input_;
    IDeckLinkVideoConversion* converter_;

    std::queue<MemoryBackedFrame*> frameQueue_;
    std::condition_variable queueCondition_;
    std::mutex queueMutex_;

    static BMDPixelFormat FormatFlagsToPixelFormat(BMDDetectedVideoInputFormatFlags flags)
    {
        return flags == bmdDetectedVideoInputRGB444 ? bmdFormat10BitRGB : bmdFormat10BitYUV;
    }
};
