//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/maker.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"
#include "limb.h"

#include <array>
#include <utility>

// The managed object the limbs of a big_integer live in: one word of
// flags (LimbShared) and the limbs, least significant first — the length
// and the sign are in the big_integer (big_integer.h), so a result
// allocated for the longest it could be and found shorter keeps its object
// as it is. Made
// the way the characters of a string are (core/detail/string_data.h):
// an object of the smallest size class that holds the limbs, each class
// its own type and so its own pool, and past the largest class (about
// 62 KB) a managed buffer from a range of pages. A limb is a number, so
// the objects are never traced — a trivial type the trait
// MayContainTracked says no to — and never zeroed: every limb a big_integer
// reads was written by the operation that made it.
namespace sgcl::math::detail {
    // The flag of the object's first word: a second big_integer has the
    // object (a copy, a negation), so neither may change it in place.
    // Set once, by the copy, and only read after that; atomic, since a
    // value can be copied by threads that each only read it
    constexpr Limb LimbShared = 1;

    // An object of a size class: N limbs, written after the allocation
    template<size_t N>
    struct LimbSlot {
        Limb limbs[N];
    };

    // The element of the buffer a long number lives in: a limb of its own
    // type, so that the buffers of numbers are told apart from the other
    // buffers of words in the heap's statistics and dumps
    struct LimbWord {
        Limb value;
    };

    // The classes: every length up to 8 limbs, then by half again up to
    // what fits in a page
    constexpr size_t LimbSmallClasses = 8;

    constexpr size_t limb_large_class(size_t i) noexcept {
        size_t c = LimbSmallClasses;
        for (size_t k = 0; k <= i; ++k) {
            c = c * 3 / 2;
        }
        return c;
    }

    constexpr size_t LimbLargeClasses = [] {
        size_t i = 0;
        while (limb_large_class(i) * sizeof(Limb) + 64 <= sgcl::detail::PageDataSize) {
            ++i;
        }
        return i;
    }();

    using LimbObject = unique_ptr<void>;

    template<size_t N>
    LimbObject make_limb_slot() {
        return LimbObject(sgcl::detail::Maker<LimbSlot<N>>::make_tracked_data());
    }

    using MakeLimbs = LimbObject (*)();

    template<size_t... Is>
    constexpr std::array<MakeLimbs, sizeof...(Is)> limb_small_entries(std::index_sequence<Is...>) {
        return {&make_limb_slot<Is + 1>...};
    }

    template<size_t... Is>
    constexpr std::array<MakeLimbs, sizeof...(Is)> limb_large_entries(std::index_sequence<Is...>) {
        return {&make_limb_slot<limb_large_class(Is)>...};
    }

    inline constexpr auto LimbSmallTable = limb_small_entries(std::make_index_sequence<LimbSmallClasses>());
    inline constexpr auto LimbLargeTable = limb_large_entries(std::make_index_sequence<LimbLargeClasses>());

    inline constexpr auto LimbLargeSizes = [] {
        std::array<size_t, LimbLargeClasses> sizes{};
        for (size_t i = 0; i < LimbLargeClasses; ++i) {
            sizes[i] = limb_large_class(i);
        }
        return sizes;
    }();

    // A new object for at least n limbs (n > 0), not yet anybody's: the
    // raw word, alive by the state of its slot (the allocator hands it out
    // locked, as make_tracked does), which is what the result of an
    // operation is written into before it becomes the word of a big_integer
    struct LimbMaker {
        using Slot = LimbObject;

        // The limbs an object made for n has: its class's, which is n or
        // more, and n itself past the classes
        static constexpr size_t capacity(size_t n) noexcept {
            if (n <= LimbSmallClasses) {
                return n;
            }
            for (size_t i = 0; i < LimbLargeClasses; ++i) {
                if (n <= LimbLargeSizes[i]) {
                    return LimbLargeSizes[i];
                }
            }
            return n;
        }

        static Slot make(size_t n) {
            if (n <= LimbSmallClasses) {
                return LimbSmallTable[n - 1]();
            }
            for (size_t i = 0; i < LimbLargeClasses; ++i) {
                if (n <= LimbLargeSizes[i]) {
                    return LimbLargeTable[i]();
                }
            }
            return Slot(sgcl::detail::Maker<LimbWord[]>::make_tracked_data(n));
        }
    };
}
