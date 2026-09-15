//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../weak_ptr.h"

#include <type_traits>

namespace sgcl::detail {
    template<class T>
    struct IsWeakPtr : std::false_type {};

    template<class T, template<class> class Ptr>
    struct IsWeakPtr<weak_ptr<T, Ptr>> : std::true_type {};

    template<class T, class = void>
    struct HasTrackedType : std::false_type {};

    template<class T>
    struct HasTrackedType<T, std::void_t<typename T::tracked_type>> : std::true_type {};

    // A pointer word: a tracked_ptr of either kind or a weak_ptr (a
    // tracked_ptr to its cell). One word that only ever holds null or an
    // address, whose destructor leaves null behind; so several of them
    // may share one word of storage in turn, and an offset that holds
    // nothing but pointer words never leaves a type's pointer map
    // (child_pointers.h). What variant.h and any.h keep apart from data.
    template<class T>
    inline constexpr bool IsPointerWord = std::is_base_of_v<Tracked, T> || HasTrackedType<T>::value || IsWeakPtr<T>::value;

    // Where a type goes in a storage shared with others: the pointer
    // word, a place of its own (it may hold tracked pointers next to
    // data, so its offsets are its own), or the data shared by the
    // types that cannot hold a pointer
    enum class Region : unsigned char { Word, Tracked, Data };

    template<class T>
    constexpr Region region_of() noexcept {
        if (IsPointerWord<T>) {
            return Region::Word;
        }
        if (MayContainTracked<T>::value) {
            return Region::Tracked;
        }
        return Region::Data;
    }

    // Raw storage of a size and an alignment; empty for size 0
    template<size_t Size, size_t Align>
    struct RawStorage {
        alignas(Align) unsigned char bytes[Size];
    };

    template<size_t Align>
    struct RawStorage<0, Align> {
        static constexpr unsigned char* bytes = nullptr;
    };
}
