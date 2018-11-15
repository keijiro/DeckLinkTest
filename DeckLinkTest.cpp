#include "Common.h"
#include "InputCallback.h"

namespace decklink_test
{
    class Test
    {
    public:

        Test()
        {
            // COM initialization
            AssertSuccess(CoInitialize(nullptr));

            // Retrieve the first device.
            IDeckLinkIterator* iterator;
            AssertSuccess(CoCreateInstance(
                CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
                IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator)
            ));
            AssertSuccess(iterator->Next(&deckLink_));
            iterator->Release();

            // Retrieve an input/output interface from the device.
            AssertSuccess(deckLink_->QueryInterface(
                IID_IDeckLinkInput, reinterpret_cast<void**>(&input_)
            ));

            AssertSuccess(deckLink_->QueryInterface(
                IID_IDeckLinkOutput, reinterpret_cast<void**>(&output_)
            ));

            // Set the input callback.
            inputCallback_ = new InputCallback(input_);
            AssertSuccess(input_->SetCallback(inputCallback_));
        }

        ~Test()
        {
            // Finalization
            input_->Release();
            output_->Release();
            inputCallback_->Release();
            deckLink_->Release();
        }

        void Run()
        {
            // Enable the video input with a default video mode.
            AssertSuccess(input_->EnableVideoInput(
                bmdModeNTSC, bmdFormat10BitYUV,
                bmdVideoInputEnableFormatDetection
            ));

            // Enable video output with 1080i59.94
            AssertSuccess(output_->EnableVideoOutput(
                bmdModeHD1080i5994, bmdVideoOutputFlagDefault
            ));

            // Start streaming
            AssertSuccess(input_->StartStreams());

            for (auto i = 0;; i++)
            {
                IDeckLinkMutableVideoFrame* frame;
                output_->CreateVideoFrame(1920, 1080, 1920 * 4, bmdFormat8BitARGB, 0, &frame);

                std::uint32_t* buffer;
                frame->GetBytes(reinterpret_cast<void**>(&buffer));

                for (auto y = 0; y < 1080; y++)
                    for (auto x = 0; x < 1920; x++)
                        *(buffer++) = ((x + i) & 0xff) * 0x10101 + 0x7f000000;

                output_->DisplayVideoFrameSync(frame);

                frame->Release();
            }

            // Stop and disable
            input_->StopStreams();
            input_->DisableVideoInput();
            output_->DisableVideoOutput();
        }

    private:

        IDeckLink* deckLink_;
        IDeckLinkInput* input_;
        InputCallback* inputCallback_;
        IDeckLinkOutput* output_;
    };
}

int main()
{
    decklink_test::Test().Run();
    return 0;
}
