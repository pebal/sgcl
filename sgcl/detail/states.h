//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"

#include <atomic>
#include <cstdint>

namespace sgcl::detail {
    // Eight slot states at once, SIMD within a 64-bit word: one aligned
    // load of eight state bytes (the states array follows the header, on
    // 8 bytes), the test done on every byte in parallel, the answer as 8
    // bits, bit i for state i, ready to combine with the flag words. The
    // states are 0, 1, 2, 4, 8, 16 and 32 (types.h), which the tests rely
    // on. The load is atomic like the byte stores it overlaps: a byte
    // stored meanwhile reads either old or new, never torn.
    struct States8 {
        static constexpr uint64_t Ones = 0x0101010101010101ull;
        static constexpr uint64_t Gather = 0x0102040810204080ull;

        static uint64_t load(const std::atomic<State>* states) noexcept {
            return std::atomic_ref<uint64_t>(*reinterpret_cast<uint64_t*>(const_cast<std::atomic<State>*>(states))).load(std::memory_order_relaxed);
        }

        // the eight bytes with value v stored at once
        static void store(std::atomic<State>* states, State v) noexcept {
            std::atomic_ref<uint64_t>(*reinterpret_cast<uint64_t*>(states)).store(uint64_t(v) * Ones, std::memory_order_relaxed);
        }

        // bit 0 of every byte of `b` collected into the low 8 bits
        static unsigned bits(uint64_t b) noexcept {
            return (unsigned)((b * Gather) >> 56);
        }

        // Reserved or Unused: the states from 16 up (a free slot)
        static unsigned free(uint64_t w) noexcept {
            return bits(((w >> 4) | (w >> 5)) & Ones);
        }

        // a state with a bit of CreatedMask: created and not yet freed
        static unsigned created(uint64_t w) noexcept {
            auto m = w & (0x0F * Ones);
            m |= m >> 2;
            m |= m >> 1;
            return bits(m & Ones);
        }

        // Reachable in the current cycle: Reachable with the current
        // parity (Fresh or not: a registered Fresh object of the current
        // parity was UniqueLock when registered and handed to a tracked_ptr
        // since, or allocated before the flip and handed to one after it,
        // Earlier), or UniqueLock of either parity
        static unsigned reachable(uint64_t w, State current) noexcept {
            return equal(w, current) | equal(w, State(current | State::Fresh)) | equal(w, State(current | State::Fresh | State::Earlier))
                 | equal(w, State::UniqueLock) | equal(w, State(State::UniqueLock | State::Parity));
        }

        // the bytes equal to v
        static unsigned equal(uint64_t w, State v) noexcept {
            auto x = w ^ (uint64_t(v) * Ones);
            auto zero = ~(((x & (0x7F * Ones)) + (0x7F * Ones)) | x | (0x7F * Ones));   // 0x80 in every zero byte
            return bits(zero >> 7);
        }
    };
}
