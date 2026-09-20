//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "concepts.h"

#include <ranges>

namespace sgcl {
    // m_lookup<Derived>: a map read by its key, as members over
    // Derived::find(key) (an iterator, end() when absent), and the
    // declaration that Derived is a map: c_lookup<R> is "R carries
    // m_lookup". The value comes back as a copy in an optional (get), as
    // a pointer into the map (try_get), or as a default (value_or), one
    // search each and no exception; keys() and values() are views over
    // the map's own range (for_each is m_enumerable's, over the pairs). A
    // key of another type is accepted wherever the map's find is
    // transparent. A map with several values per key (multimap) gives
    // the first by get and all of them by values_of.
    template<class Derived>
    class m_lookup {
    public:
        // The types of Derived are named in the bodies only (deduced
        // returns): a signature is instantiated with the class, when
        // Derived is not yet complete
        template<class K>
        auto get(const K& key) const {
            using M = typename Derived::mapped_type;
            auto it = _self().find(key);
            return it == _self().end() ? optional<M>() : optional<M>(it->second);
        }

        template<class K>
        auto try_get(const K& key) noexcept {
            auto it = _self().find(key);
            return it == _self().end() ? nullptr : &it->second;
        }

        template<class K>
        auto try_get(const K& key) const noexcept {
            auto it = _self().find(key);
            return it == _self().end() ? nullptr : &it->second;
        }

        template<class K, class U>
        auto value_or(const K& key, U&& fallback) const {
            using M = typename Derived::mapped_type;
            auto it = _self().find(key);
            return it == _self().end() ? static_cast<M>(std::forward<U>(fallback)) : M(it->second);
        }

        template<class K>
        bool contains_key(const K& key) const {
            return _self().find(key) != _self().end();
        }

        // The keys and the values as ranges over the map, in its order
        auto keys() const {
            return std::views::keys(_self());
        }

        auto values() {
            return std::views::values(_self());
        }

        auto values() const {
            return std::views::values(_self());
        }

        // Every value under the key, as a range: what a multimap holds there
        template<class K>
        auto values_of(const K& key) {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

        template<class K>
        auto values_of(const K& key) const {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

    protected:
        m_lookup() = default;
        ~m_lookup() = default;

    private:
        Derived& _self() noexcept { return static_cast<Derived&>(*this); }
        const Derived& _self() const noexcept { return static_cast<const Derived&>(*this); }
    };
}
