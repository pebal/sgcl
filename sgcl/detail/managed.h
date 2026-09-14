//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The type an element of a container is stored as, in the container's
    // managed buffer or node. A type that names a `tracked_type` (gc.h:
    // gc::tracked_ptr names sgcl::tracked_ptr) is stored as that: inside
    // managed memory the two are one word, the check of the address at
    // construction and the test of the mode at every access are for
    // nothing, and a reference to the stored word as the element's type is
    // exact, the word being in the tracked mode by construction. Any other
    // type is stored as itself.
    template<class T, class = void>
    struct Managed {
        using type = T;
    };

    template<class T>
    struct Managed<T, std::void_t<typename T::tracked_type>> {
        using type = typename T::tracked_type;
        static_assert(sizeof(type) == sizeof(T) && alignof(type) == alignof(T));
    };

    template<class T>
    using managed_t = typename Managed<T>::type;

    // The same for the value of an associative container: a pair is
    // stored with each of its types mapped
    template<class T>
    struct ManagedValue {
        using type = managed_t<T>;
    };

    template<class Key, class T>
    struct ManagedValue<std::pair<const Key, T>> {
        using type = std::pair<const managed_t<Key>, managed_t<T>>;
        static_assert(sizeof(type) == sizeof(std::pair<const Key, T>) && alignof(type) == alignof(std::pair<const Key, T>));
    };

    template<class T>
    using managed_value_t = typename ManagedValue<T>::type;
}
