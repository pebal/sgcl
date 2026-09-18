//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "types.h"

#include <atomic>
#include <bit>
#include <typeinfo>
#include <vector>

namespace sgcl::detail {
    // Where the pointers are in an object of one type: one bit per word of
    // the object, built by the collector by elimination. The map starts full
    // (every word may be a pointer); a word found holding a non-zero value
    // that is not a managed address proves its offset is data, and the
    // offset is removed for good. Pointer fields only ever hold null or a
    // managed address, so they never drop out; fields that are always zero
    // stay and cost a load per object. Owned and written by the collector
    // thread only, after construction.
    // Words are atomic because the sweep may run on several threads, each
    // removing offsets; readers take relaxed loads (a stale bit only means
    // one more zero read). A conservative type (type_info.h: Conservative)
    // keeps its map full for good: the collector does not remove offsets.
    struct ChildPointers {
        ChildPointers(bool may_contain, size_t object_size, const std::type_info& type, bool conservative = false) noexcept
        : type(type)
        , conservative(conservative)
        , map(may_contain ? (object_size / sizeof(RawPointer) + 63) / 64 : 0)
#if !defined(NDEBUG)
        , removed(map.size())
#endif
        {
            auto count = may_contain ? object_size / sizeof(RawPointer) : 0;
            for (size_t i = 0; i < count; ++i) {
                map[i / 64].fetch_or(uint64_t(1) << (i % 64), std::memory_order_relaxed);
            }
            any.store(count != 0, std::memory_order_relaxed);
        }

        uint64_t word(size_t w) const noexcept {
            return map[w].load(std::memory_order_relaxed);
        }

        void remove(size_t offset) noexcept {
            map[offset / 64].fetch_and(~(uint64_t(1) << (offset % 64)), std::memory_order_relaxed);
            bool left = false;
            for (auto& w : map) {
                left |= w.load(std::memory_order_relaxed) != 0;
            }
            any.store(left, std::memory_order_relaxed);
#if !defined(NDEBUG)
            removed[offset / 64].fetch_or(uint64_t(1) << (offset % 64), std::memory_order_relaxed);
#endif
        }

        const std::type_info& type;
        const bool conservative;                  // never an offset removed
        std::vector<std::atomic<uint64_t>> map;   // bit set: the word at that offset may hold a pointer
        std::atomic<bool> any = {false};          // some bit is still set
#if !defined(NDEBUG)
        std::vector<std::atomic<uint64_t>> removed;
        std::atomic<bool> warned = {false};
#endif
    };
}
