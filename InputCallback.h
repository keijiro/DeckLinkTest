#pragma once

#include "Common.h"
#include <cstdio>

namespace decklink_test
{
    class InputCallback : public IDeckLinkInputCallback
    {
    public:
        InputCallback(IDeckLinkInput* input)
        : refCount_(1), input_(input)
        {
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

        static BMDPixelFormat FormatFlagsToPixelFormat(
            BMDDetectedVideoInputFormatFlags flags
        )
        {
            return flags == bmdDetectedVideoInputRGB444 ?
                bmdFormat10BitRGB : bmdFormat10BitYUV;
        }

        HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
            BMDVideoInputFormatChangedEvents notificationEvents,
            IDeckLinkDisplayMode* newDisplayMode,
            BMDDetectedVideoInputFormatFlags detectedSignalFlags
        ) override
        {
            BSTR name;
            newDisplayMode->GetName(&name);
            std::printf("Video input: %S\n", name);
            SysFreeString(name);

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
            if (videoFrame == nullptr) return S_OK;

            static int counter = 0;

            if (counter++ % 15 == 0)
            {
                std::printf(
                    "w:%d, h:%d, fmt:%x, flg:%x\n",
                    videoFrame->GetWidth(),
                    videoFrame->GetHeight(),
                    videoFrame->GetPixelFormat(),
                    videoFrame->GetFlags()
                );
            }

            return S_OK;
        }

    private:
        IDeckLinkInput* input_;
        ULONG refCount_;
    };

}
