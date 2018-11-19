#pragma once

#include "Common.h"
#include <atomic>
#include <vector>

class MemoryBackedFrame final : public IDeckLinkVideoFrame
{
public:

    MemoryBackedFrame(long width, long height)
        : refCount_(1)
    {
        width_ = width;
        height_ = height;
        memory_.resize(static_cast<std::size_t>(width_) * height_);
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

        if (iid == IID_IDeckLinkVideoFrame)
        {
            *ppv = (IDeckLinkVideoFrame*)this;
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

    // IDeckLinkVideoFrame implementation
    
    long STDMETHODCALLTYPE GetWidth() override
    {
        return width_;
    }

    long STDMETHODCALLTYPE GetHeight() override
    {
        return height_;
    }

    long STDMETHODCALLTYPE GetRowBytes() override
    {
        return width_ * sizeof(uint32_t);
    }

    BMDPixelFormat STDMETHODCALLTYPE GetPixelFormat() override
    {
        return bmdFormat8BitARGB;
    }

    BMDFrameFlags STDMETHODCALLTYPE GetFlags() override
    {
        return bmdFrameFlagDefault;
    }

    HRESULT STDMETHODCALLTYPE GetBytes(void** buffer)
    {
        *buffer = memory_.data();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode** timecode) override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetAncillaryData(IDeckLinkVideoFrameAncillary** ancillary) override
    {
        return E_NOTIMPL;
    }

private:

    std::atomic<ULONG> refCount_;
    std::vector<uint32_t> memory_;
    long width_;
    long height_;
};