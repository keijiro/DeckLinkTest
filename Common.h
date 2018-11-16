#pragma once

#include "DeckLinkAPI_h.h"
#include <cassert>
#include <cinttypes>
#include <tuple>

// Assert function for COM operations
void AssertSuccess(HRESULT result)
{
    assert(SUCCEEDED(result));
}

// Global configuration
class Config
{
public:
    static const BMDTimeScale TimeScale = 60000;
};

// Miscellaneous utilities
class Utility
{
public:

    // Retrieve an input/output pair from the first DeckLink device.
    static auto RetrieveDeckLinkInputOutput()
    {
        // Retrieve the first DeckLink device via an iterator.
        IDeckLinkIterator* iterator;
        AssertSuccess(CoCreateInstance(
            CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
            IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator)
        ));

        IDeckLink* device;
        AssertSuccess(iterator->Next(&device));

        // Retrieve an input/output interface from the device.
        IDeckLinkInput* input;
        AssertSuccess(device->QueryInterface(
            IID_IDeckLinkInput, reinterpret_cast<void**>(&input)
        ));

        IDeckLinkOutput* output;
        AssertSuccess(device->QueryInterface(
            IID_IDeckLinkOutput, reinterpret_cast<void**>(&output)
        ));

        // These references are no longer needed.
        iterator->Release();
        device->Release();

        return std::make_tuple(input, output);
    }
};
