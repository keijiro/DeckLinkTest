#pragma once

#include "Common.h"
#include "MemoryBackedFrame.h"
#include <atomic>
#include <mutex>
#include <queue>

class Receiver final : public IDeckLinkInputCallback
{
public:

    // Constructor/destructor

    Receiver()
        : refCount_(1), input_(nullptr), converter_(nullptr)
    {
        // Create a format converter instance.
        AssertSuccess(CoCreateInstance(
            CLSID_CDeckLinkVideoConversion, nullptr, CLSCTX_ALL,
            IID_IDeckLinkVideoConversion, reinterpret_cast<void**>(&converter_)
        ));
    }

    ~Receiver()
    {
        assert(input_ == nullptr); // The input should have been stopped.

        // Destroy the converter instance.
        converter_->Release();
    }

    // Public methods

    void StartReceiving(IDeckLinkInput* input)
    {
        assert(input_ == nullptr);

        // Start depending the input object.
        input_ = input;
        input_->AddRef();

        // Start getting callback from the input object.
        AssertSuccess(input_->SetCallback(this));

        // Enable the video input with a default video mode
        // (it will be changed by input mode detection).
        AssertSuccess(input_->EnableVideoInput(
            bmdModeNTSC, bmdFormat10BitYUV,
            bmdVideoInputEnableFormatDetection
        ));

        // Start the input stream.
        AssertSuccess(input_->StartStreams());
    }

    void StopReceiving()
    {
        assert(input_ != nullptr);

        // Stop the input stream.
        AssertSuccess(input_->StopStreams());
        AssertSuccess(input_->SetCallback(nullptr));
        AssertSuccess(input_->DisableVideoInput());

        // Dispose all the queued frames.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!frameQueue_.empty())
            {
                frameQueue_.front()->Release();
                frameQueue_.pop();
            }
        }

        // Release the input object.
        input_->Release();
        input_ = nullptr;
    }

    size_t CountQueuedFrames() const
    {
        return frameQueue_.size();
    }

    MemoryBackedFrame* PopFrame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto frame = frameQueue_.front();
        frameQueue_.pop();
        return frame;
    }

    // IUnknown implementation

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        if (iid == IID_IUnknown)
        {
            *ppv = this;
            AddRef();
            return S_OK;
        }

        if (iid == IID_IDeckLinkInputCallback)
        {
            *ppv = (IDeckLinkInputCallback*)this;
            AddRef();
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
        if (val == 1) delete this;
        return val;
    }

    // IDeckLinkInputCallback implementation

    HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
        BMDVideoInputFormatChangedEvents events,
        IDeckLinkDisplayMode* mode,
        BMDDetectedVideoInputFormatFlags flags
    ) override
    {
        // Switch to the notified display mode.
        input_->PauseStreams();
        input_->EnableVideoInput(
            mode->GetDisplayMode(),
            bmdFormat10BitYUV,
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
            // Convert and push the arrived frame to the frame queue.
            auto frame = new MemoryBackedFrame(
                videoFrame->GetWidth(), videoFrame->GetHeight()
            );
            AssertSuccess(converter_->ConvertFrame(videoFrame, frame));
            std::lock_guard<std::mutex> lock(mutex_);
            frameQueue_.push(frame);
        }
        return S_OK;
    }

private:

    std::atomic<ULONG> refCount_;
    IDeckLinkInput* input_;
    IDeckLinkVideoConversion* converter_;
    std::queue<MemoryBackedFrame*> frameQueue_;
    std::mutex mutex_;
};
