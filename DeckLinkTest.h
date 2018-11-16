#pragma once

#include "Common.h"
#include "MemoryBackedFrame.h"
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

        // Enable video output with 1080i59.94.
        AssertSuccess(output_->EnableVideoOutput(bmdModeHD1080i5994, bmdVideoOutputFlagDefault));

        // Start an input stream.
        AssertSuccess(input_->StartStreams());

        // Launch a sender thread.
        std::thread t([=](){ SenderThread(); });

        // Wait for user interaction.
        std::puts("Press return to stop.");
        std::getchar();

        // Stop the input stream.
        AssertSuccess(input_->StopStreams());

        // Terminate the sender thread.
        receiver_->StopReceiving();
        t.join();

        // Disable the video input/output.
        input_->DisableVideoInput();
        output_->DisableVideoOutput();
    }

private:

    IDeckLinkInput* input_;
    IDeckLinkOutput* output_;
    Receiver* receiver_;

    void SenderThread()
    {
        const auto frameDuration = static_cast<BMDTimeValue>(Config::TimeScale * 2 / 59.94);

        // Start scheduled playback immediately.
        AssertSuccess(output_->StartScheduledPlayback(0, Config::TimeScale, 1));

        for (auto count = 0u;; count++)
        {
            // Retrieve a frame from the receiver frame queue.
            auto* frame = receiver_->PopFrameSync();

            // It returns null when the stream was terminated.
            if (frame == nullptr) break;
            
            // Determine the output frame time with adding the output latency.
            auto time = static_cast<BMDTimeValue>(Config::TimeScale * 2 * (count + Config::OutputLatency) / 59.94);

            // Schedule the output frame.
            AssertSuccess(output_->ScheduleVideoFrame(frame, time, frameDuration, Config::TimeScale));

            // The ownership of the frame was moved to output_, so release it.
            frame->Release();
        }

        // Stop the scheduled playback.
        output_->StopScheduledPlayback(0, nullptr, Config::TimeScale);
    }
};
