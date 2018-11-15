#pragma once

#include "Common.h"
#include "Receiver.h"
#include <thread>

class DeckLinkTest
{
public:

    DeckLinkTest()
    {
        std::tie(input_, output_) = Utility::RetrieveDeckLinkInputOutput();

        // Receive/send buffer
        AssertSuccess(output_->CreateVideoFrame(
            1920, 1080, 1920 * 4, bmdFormat8BitARGB, 0, &receiveBuffer_
        ));
        AssertSuccess(output_->CreateVideoFrame(
            1920, 1080, 1920 * 4, bmdFormat8BitARGB, 0, &sendBuffer_
        ));

        // Frame receiver
        receiver_ = new Receiver(input_);
        receiver_->SetBuffer(receiveBuffer_);
        AssertSuccess(input_->SetCallback(receiver_));
    }

    ~DeckLinkTest()
    {
        // Finalization
        input_->Release();
        output_->Release();
        receiveBuffer_->Release();
        sendBuffer_->Release();
        receiver_->Release();
    }

    void Run()
    {
        // Enable the video input with a default video mode
        // (it will be changed by input mode detection).
        AssertSuccess(input_->EnableVideoInput(
            bmdModeNTSC, bmdFormat10BitYUV,
            bmdVideoInputEnableFormatDetection
        ));
        AssertSuccess(input_->StartStreams());

        // Enable video output with 1080i59.94.
        AssertSuccess(output_->EnableVideoOutput(
            bmdModeHD1080i5994, bmdVideoOutputFlagDefault
        ));

        // Start a sender thread, wait for user interaction, then stop it.
        auto terminate = false;
        std::thread t([&terminate, this](){ SenderThread(terminate); });

        getchar();

        terminate = true;
        t.join();

        // Stop and disable.
        input_->StopStreams();
        input_->DisableVideoInput();
        output_->DisableVideoOutput();
    }

private:

    IDeckLinkInput* input_;
    IDeckLinkOutput* output_;
    IDeckLinkMutableVideoFrame* receiveBuffer_;
    IDeckLinkMutableVideoFrame* sendBuffer_;
    Receiver* receiver_;

    void SenderThread(bool& terminate)
    {
        for (auto count = 0u; !terminate; count += 2)
        {
            receiver_->LockBuffer();

            std::uint32_t* p_i;
            std::uint32_t* p_o;

            receiveBuffer_->GetBytes(reinterpret_cast<void**>(&p_i));
            sendBuffer_->GetBytes(reinterpret_cast<void**>(&p_o));

            for (auto i = 0; i < 1920 * 1080; i++)
            {
                auto c = *(p_i++);
                auto r = (c >>  8) & 0xff;
                auto g = (c >> 16) & 0xff;
                auto b = (c >> 24) & 0xff;

                r = (r + count) & 0xff;
                g = (g + count) & 0xff;
                b = (b + count) & 0xff;

                *(p_o++) = 0xffu | (r << 8) | (g << 16) | (b << 24);
            }

            receiver_->UnlockBuffer();
            output_->DisplayVideoFrameSync(sendBuffer_);
        }
    }
};
