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

        // Enable video output with 1080i59.94.
        AssertSuccess(output_->EnableVideoOutput(
            bmdModeHD1080i5994, bmdVideoOutputFlagDefault
        ));

        // Start streaming with a sender thread.
        AssertSuccess(input_->StartStreams());
        terminate_ = false;
        std::thread t([=]() { SenderThread(); });

        // Wait for user interaction.
        getchar();

        // Stop the sender thread.
        terminate_ = true;
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

    bool terminate_;

    void SenderThread()
    {
        for (auto i = 0; !terminate_; i++)
        {
            receiver_->LockBuffer();

            std::uint32_t* p_i;
            std::uint32_t* p_o;
            receiveBuffer_->GetBytes(reinterpret_cast<void**>(&p_i));
            sendBuffer_->GetBytes(reinterpret_cast<void**>(&p_o));

            for (auto y = 0; y < 1080 / 2; y++)
                for (auto x = 0; x < 1920; x++)
                    *(p_o++) = *(p_i++) + ((x + i) & 0xff) * 0x10101 + 0x7f000000;

            receiver_->UnlockBuffer();

            output_->DisplayVideoFrameSync(sendBuffer_);
        }
    }
};
