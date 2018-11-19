#pragma once

#include "Common.h"
#include "Receiver.h"

class Sender final : public IDeckLinkVideoOutputCallback
{
public:

    // Constructor/destructor

    Sender()
        : refCount_(1), output_(nullptr), receiver_(nullptr), frameCount_(0)
    {
        blank_ = new MemoryBackedFrame(1920, 1080);
    }

    ~Sender()
    {
        // The output should have been stopped.
        assert(output_ == nullptr);
        assert(receiver_ == nullptr);

        // Release the internal objects.
        blank_->Release();
    }

    // Public methods

    void StartSending(IDeckLinkOutput* output, Receiver* receiver)
    {
        assert(output_ == nullptr);

        // Start depending the external objects.
        output_ = output;
        output_->AddRef();
        receiver_ = receiver;
        receiver_->AddRef();

        // Start getting callback from the output object.
        AssertSuccess(output_->SetScheduledFrameCompletionCallback(this));

        // Enable video output with 1080i59.94.
        AssertSuccess(output_->EnableVideoOutput(
            bmdModeHD1080i5994, bmdVideoOutputFlagDefault
        ));

        // Prerolling with blank frames.
        for (auto i = 0; i < Config::preroll; i++) ScheduleFrame(blank_);

        // Start scheduled playback.
        AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
    }

    void StopSending()
    {
        // Stop the output stream.
        output_->StopScheduledPlayback(0, nullptr, 1);
        output_->SetScheduledFrameCompletionCallback(nullptr);
        output_->DisableVideoOutput();

        // Release the external objects.
        receiver_->Release();
        receiver_ = nullptr;
        output_->Release();
        output_ = nullptr;
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

        if (iid == IID_IDeckLinkVideoOutputCallback)
        {
            *ppv = (IDeckLinkVideoOutputCallback*)this;
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

    // IDeckLinkVideoOutputCallback implementation

    HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(
        IDeckLinkVideoFrame *completedFrame,
        BMDOutputFrameCompletionResult result
    ) override
    {
        if (result == bmdOutputFrameDisplayedLate)
            std::printf("Frame %p was displayed late.\n", completedFrame);

        if (result == bmdOutputFrameDropped)
            std::printf("Frame %p was dropped.\n", completedFrame);

        // Skip a single frame when DisplayedLate was detected.
        if (result == bmdOutputFrameDisplayedLate)
        {
            if (receiver_->CountQueuedFrames() > 1)
                receiver_->PopFrame()->Release();
            frameCount_++;
        }

        if (receiver_->CountQueuedFrames() == 0)
        {
            // Send a blank frame when no frame is available in the input queue.
            ScheduleFrame(blank_);
        }
        else
        {
            // Retrieve a frame from the input queue and send it.
            auto frame = receiver_->PopFrame();
            ScheduleFrame(frame);
            frame->Release();
        }

        #if false
        unsigned int num;
        output_->GetBufferedVideoFrameCount(&num);
        std::printf("(in, out) = (%lld, %d)\n", receiver_->CountQueuedFrames(), num);
        #endif

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override
    {
        return S_OK;
    }

private:

    std::atomic<ULONG> refCount_;
    IDeckLinkOutput* output_;
    Receiver* receiver_;
    MemoryBackedFrame* blank_;
    uint64_t frameCount_;

    void ScheduleFrame(MemoryBackedFrame* frame)
    {
        auto time = static_cast<BMDTimeValue>(Config::TimeScale * 2 * frameCount_ / 59.94);
        auto duration = static_cast<BMDTimeValue>(Config::TimeScale * 2 / 59.94);
        output_->ScheduleVideoFrame(frame, time, duration, Config::TimeScale);
        frameCount_++;
    }
};