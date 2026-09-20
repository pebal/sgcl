//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "../req.h"

#include <ranges>
#include <type_traits>

namespace sgcl::mixin {
    // lookup<Derived>: a map read by its key, as members over
    // Derived::find(key) — an iterator, end() when absent, on the mutable
    // maps; a pointer to the value, null when absent, on the immutable
    // ones (the two conventions told apart on the result of find, in the
    // method, where Derived is complete) — and the declaration that
    // Derived is a map: req::lookup<R> is "R carries lookup". The value
    // comes back as a copy in an optional (get), as a pointer into the
    // map (try_get), or as a default (value_or), one search each and no
    // exception; keys() and values() are views over the map's own range
    // of pairs (for_each is mixin::enumerable's, over the pairs). A key of
    // another type is accepted wherever the map's find is transparent. A
    // map with several values per key (multimap) gives the first by get
    // and all of them by values_of, which exists where equal_range does.
    template<class Derived>
    class lookup {
    public:
        // The types of Derived are named in the bodies only (deduced
        // returns): a signature is instantiated with the class, when
        // Derived is not yet complete
        template<class K>
        auto get(const K& key) const {
            using M = typename Derived::mapped_type;
            auto p = _found(key);
            return p ? optional<M>(*p) : optional<M>();
        }

        template<class K>
        auto try_get(const K& key) noexcept {
            return _found(key);
        }

        template<class K>
        auto try_get(const K& key) const noexcept {
            return _found(key);
        }

        template<class K, class U>
        auto value_or(const K& key, U&& fallback) const {
            using M = typename Derived::mapped_type;
            auto p = _found(key);
            return p ? M(*p) : static_cast<M>(std::forward<U>(fallback));
        }

        template<class K>
        bool contains_key(const K& key) const {
            return _found(key) != nullptr;
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
        auto values_of(const K& key) requires requires(Derived& d) { d.equal_range(key); } {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

        template<class K>
        auto values_of(const K& key) const requires requires(const Derived& d) { d.equal_range(key); } {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

    protected:
        lookup() = default;
        ~lookup() = default;

    private:
        Derived& _self() noexcept { return static_cast<Derived&>(*this); }
        const Derived& _self() const noexcept { return static_cast<const Derived&>(*this); }

        // The value under the key as a pointer, null when absent, whichever
        // convention the map's find follows
        template<class K>
        auto _found(const K& key) noexcept {
            auto found = _self().find(key);
            if constexpr(std::is_pointer_v<decltype(found)>) {
                return found;
            } else {
                return found == _self().end() ? nullptr : &found->second;
            }
        }

        template<class K>
        auto _found(const K& key) const noexcept {
            auto found = _self().find(key);
            if constexpr(std::is_pointer_v<decltype(found)>) {
                return found;
            } else {
                return found == _self().end() ? nullptr : &found->second;
            }
        }
    };
}
