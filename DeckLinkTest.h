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

        // Frame receiver
        receiver_ = new Receiver(input_);
        AssertSuccess(input_->SetCallback(receiver_));
    }

    ~DeckLinkTest()
    {
        // Finalization
        input_->Release();
        output_->Release();
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
        AssertSuccess(output_->EnableVideoOutput(bmdModeHD1080i5994, bmdVideoOutputFlagDefault));
        AssertSuccess(output_->StartScheduledPlayback(0, Config::TimeScale, 1));

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
        output_->StopScheduledPlayback(0, nullptr, Config::TimeScale);
    }

private:

    IDeckLinkInput* input_;
    IDeckLinkOutput* output_;
    Receiver* receiver_;

    void SenderThread(bool& terminate)
    {
        const auto dur = static_cast<BMDTimeValue>(Config::TimeScale * 2 / 59.94);

        for (auto count = 0u; !terminate; count++)
        {
            auto* frame = receiver_->PopFrameSync();
            if (frame == nullptr) break;
            
            auto time = frame->GetFrameTime();
            time += static_cast<BMDTimeValue>(Config::TimeScale * 4 / 59.94);

            output_->ScheduleVideoFrame(frame, time, dur, Config::TimeScale);

            frame->Release();
        }
    }
};
