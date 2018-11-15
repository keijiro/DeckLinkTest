#include <cassert>
#include <cstdio>
#include <stdexcept>
#include "DeckLinkAPI_h.h"

class NotificationCallback : public IDeckLinkInputCallback
{
public:
    NotificationCallback(IDeckLinkInput* input) : refCount_(1), input_(input) {}

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

int main()
{
    // COM initialization
    auto result = CoInitialize(nullptr);
    if (FAILED(result)) throw std::runtime_error("CoInitialize failed.");

    // Device iterator
    IDeckLinkIterator* iterator;
    result = CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL, IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator));
    if (FAILED(result)) throw std::runtime_error("IDeckLinkIterator instantiation failed.");

    // The first device
    IDeckLink* deckLink;
    result = iterator->Next(&deckLink);
    if (FAILED(result)) throw std::runtime_error("Couldn't find DeckLink device.");

    // Attributes
    IDeckLinkAttributes* attributes;
    result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&attributes);
    if (FAILED(result)) throw std::runtime_error("IDeckLinkAttributes instantiation failed.");

    // Check if the device supports input format detection.
    BOOL supported;
    result = attributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported);
    if (FAILED(result) || supported == FALSE) throw std::runtime_error("Device doesn't support input format detection.");

    // Input interface retrieval
    IDeckLinkInput* input;
    result = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&input);
    if (FAILED(result)) throw std::runtime_error("IDeckLinkInput instantiation failed.");

    // Notification callback
    auto notificationCallback = new NotificationCallback(input);
    result = input->SetCallback(notificationCallback);
    if (FAILED(result)) throw std::runtime_error("Couldn't set notification callback.");

    // Video input with a default video mode
    result = input->EnableVideoInput(bmdModeNTSC, bmdFormat10BitYUV, bmdVideoInputEnableFormatDetection);
    if (FAILED(result)) throw std::runtime_error("Failed to enable video input.");

    // Video output
    IDeckLinkOutput* output;
    result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&output);
    assert(SUCCEEDED(result));

    // Enable video output with 1080i59.94
    result = output->EnableVideoOutput(bmdModeHD1080i5994, bmdVideoOutputFlagDefault);
    assert(SUCCEEDED(result));

    // Start streaming and wait for user interaction.
    result = input->StartStreams();
    if (FAILED(result)) throw std::runtime_error("Failed to start streaming.");

    //    std::getchar();

    for (auto i = 0;; i++)
    {
        IDeckLinkMutableVideoFrame* frame;
        output->CreateVideoFrame(1920, 1080, 1920 * 4, bmdFormat8BitARGB, 0, &frame);
        std::uint32_t* buffer;
        frame->GetBytes((void**)&buffer);
        for (auto y = 0; y < 1080; y++)
            for (auto x = 0; x < 1920; x++)
                *(buffer++) = ((x + i) & 0xff) * 0x10101 + 0x7f000000;
        output->DisplayVideoFrameSync(frame);
        frame->Release();
    }

    input->StopStreams();

    // Finalization
    input->DisableVideoInput();
    iterator->Release();
    deckLink->Release();
    attributes->Release();
    input->Release();
    notificationCallback->Release();
    output->Release();
    return 0;
}
