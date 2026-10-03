//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/split_list.h"

#include <functional>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A lock-free hash set shared by any number of threads: the
    // split-ordered list of map
    // (map.h has the account of the algorithm and the
    // rules) with the key as the element. find, contains and count are
    // wait-free once the key's bucket has its dummy node; insert, emplace and erase lock-free and linearizable; the
    // iteration weakly consistent, an iterator holding its node. The
    // elements are const, as in std::unordered_set.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set
    : public detail::SplitList<detail::ConcurrentSetTraits<Key, Hash, KeyEqual>> {
        using Base = detail::SplitList<detail::ConcurrentSetTraits<Key, Hash, KeyEqual>>;
        static_assert(detail::nothrow_function_object<Hash, const Key&>, "sgcl::concurrent::set: Hash must be noexcept");
        static_assert(detail::nothrow_function_object<KeyEqual, const Key&, const Key&>, "sgcl::concurrent::set: KeyEqual must be noexcept");

    public:
        using typename Base::value_type;
        using typename Base::iterator;

        using Base::Base;
        using Base::insert;

        set() = default;
    };
}
