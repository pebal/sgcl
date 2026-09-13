//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

#include <atomic>
#include <cstddef>

namespace sgcl::detail {
    // The header of a managed buffer; the elements follow it at
    // sizeof(ArrayBase): 16 bytes, aligned to 16, so that elements with
    // alignas(16) (SIMD vectors, __int128) are placed correctly and every
    // slot of a page of buffers stays aligned. The collector knows the
    // capacity (it traces every slot: the buffer is zeroed, an unconstructed
    // element holds null pointers) and nothing else: which elements are
    // constructed is the container's business, so is destroying them
    // (README, "Containers"). No destructor ever runs on a buffer.
    // No constructor either: the maker writes both fields into the slot
    // before the allocator publishes it (maker.h, the `init` of alloc()),
    // so that the collector, which reads them once it has seen the slot's
    // state (a release store, an acquire fence on its side), never sees
    // the header of the slot's last buffer; the type stays trivial, which
    // keeps its pointer map empty (the collector traces a buffer by the
    // metadata in its header).
    struct alignas(16) ArrayBase {
        ArrayMetadata* metadata;
        size_t capacity;
    };
}
