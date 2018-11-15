#pragma once

#include "Common.h"
#include <cstdio>
#include <mutex>

class Receiver : public IDeckLinkInputCallback
{
public:

    Receiver(IDeckLinkInput* input)
    : refCount_(1), input_(input), buffer_(nullptr)
    {
        AssertSuccess(CoCreateInstance(
            CLSID_CDeckLinkVideoConversion, nullptr, CLSCTX_ALL,
            IID_IDeckLinkVideoConversion, reinterpret_cast<void**>(&converter_)
        ));
    }

    ~Receiver()
    {
        converter_->Release();
        if (buffer_ != nullptr) buffer_->Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto val = InterlockedDecrement(&refCount_);
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
        if (videoFrame != nullptr && buffer_ != nullptr)
        {
            bufferLock_.lock();
            converter_->ConvertFrame(videoFrame, buffer_);
            bufferLock_.unlock();
        }
        return S_OK;
    }

    void SetBuffer(IDeckLinkMutableVideoFrame* buffer)
    {
        if (buffer_ != nullptr) buffer->Release();
        buffer_ = buffer;
        if (buffer_ != nullptr) buffer->AddRef();
    }

    void LockBuffer()
    {
        bufferLock_.lock();
    }

    void UnlockBuffer()
    {
        bufferLock_.unlock();
    }

private:

    ULONG refCount_;
    IDeckLinkInput* input_;
    IDeckLinkVideoConversion* converter_;
    IDeckLinkMutableVideoFrame* buffer_;
    std::mutex bufferLock_;

    static BMDPixelFormat FormatFlagsToPixelFormat(
        BMDDetectedVideoInputFormatFlags flags
    )
    {
        return flags == bmdDetectedVideoInputRGB444 ?
            bmdFormat10BitRGB : bmdFormat10BitYUV;
    }
};
