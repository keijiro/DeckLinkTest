#pragma once

#include "DeckLinkAPI_h.h"
#include <cassert>
#include <cinttypes>

namespace decklink_test
{
    void AssertSuccess(HRESULT result) { assert(SUCCEEDED(result)); }
}
