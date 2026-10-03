//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
    // Derived::find(key) — an iterator, end() when absent, as every find
    // of the library — or over Derived::_value_of(key), a pointer to the
    // value, null when absent, where a map gives one because its iterator
    // costs more than the pointer (immutable's carries a path of nodes;
    // _value_of private, the mixin a friend) — and the declaration that
    // Derived is a map: req::lookup<R> is "R carries lookup". The value
    // comes back as a copy in an optional (get), as a pointer into the
    // map (try_get), or as a default (value_or), one search each and no
    // exception; keys() and values() are views over the map's own range
    // of pairs (for_each is mixin::enumerable's, over the pairs). A key of
    // another type is accepted wherever the map's find is transparent; a
    // read is as noexcept as that find (or _value_of) with the key. A
    // map with several values per key (multimap) gives the first by get
    // and all of them by values_of, which exists where equal_range does.
    template<class Derived>
    class lookup {
    public:
        // The types of Derived are named in the bodies only (deduced
        // returns): a signature is instantiated with the class, when
        // Derived is not yet complete
        template<class K>
        auto get(const K& key) const noexcept(std::is_nothrow_copy_constructible_v<typename Derived::mapped_type> && _nothrow_found<const Derived, K>()) {
            using M = typename Derived::mapped_type;
            auto p = _found(key);
            return p ? optional<M>(*p) : optional<M>();
        }

        template<class K>
        auto try_get(const K& key) noexcept(_nothrow_found<Derived, K>()) {
            return _found(key);
        }

        template<class K>
        auto try_get(const K& key) const noexcept(_nothrow_found<const Derived, K>()) {
            return _found(key);
        }

        template<class K, class U>
        auto value_or(const K& key, U&& fallback) const noexcept(std::is_nothrow_copy_constructible_v<typename Derived::mapped_type> && std::is_nothrow_constructible_v<typename Derived::mapped_type, U&&> && _nothrow_found<const Derived, K>()) {
            using M = typename Derived::mapped_type;
            auto p = _found(key);
            return p ? M(*p) : static_cast<M>(std::forward<U>(fallback));
        }

        template<class K>
        bool contains_key(const K& key) const noexcept(_nothrow_found<const Derived, K>()) {
            return _found(key) != nullptr;
        }

        // The keys and the values as ranges over the map, in its order
        auto keys() const noexcept {
            return std::views::keys(_self());
        }

        auto values() noexcept {
            return std::views::values(_self());
        }

        auto values() const noexcept {
            return std::views::values(_self());
        }

        // Every value under the key, as a range: what a multimap holds there
        template<class K>
        auto values_of(const K& key) noexcept(noexcept(std::declval<Derived&>().equal_range(key))) requires requires(Derived& d) { d.equal_range(key); } {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

        template<class K>
        auto values_of(const K& key) const noexcept(noexcept(std::declval<const Derived&>().equal_range(key))) requires requires(const Derived& d) { d.equal_range(key); } {
            auto [first, last] = _self().equal_range(key);
            return std::ranges::subrange(first, last) | std::views::values;
        }

    protected:
        lookup() = default;
        ~lookup() = default;

    private:
        Derived& _self() noexcept { return static_cast<Derived&>(*this); }
        const Derived& _self() const noexcept { return static_cast<const Derived&>(*this); }

        // Whether _found with a K cannot throw: the map's _value_of or find
        // with it (a transparent hash, equality or comparison with another
        // type may throw; with the key type they are required noexcept)
        template<class D, class K>
        static constexpr bool _nothrow_found() noexcept {
            if constexpr (requires(D& d, const K& k) { d._value_of(k); }) {
                return noexcept(std::declval<D&>()._value_of(std::declval<const K&>()));
            } else {
                return noexcept(std::declval<D&>().find(std::declval<const K&>()));
            }
        }

        // The value under the key as a pointer, null when absent: through
        // the map's _value_of where it has one, else through find
        template<class K>
        auto _found(const K& key) noexcept(_nothrow_found<Derived, K>()) {
            if constexpr (requires(Derived& d) { d._value_of(key); }) {
                return _self()._value_of(key);
            } else {
                auto found = _self().find(key);
                return found == _self().end() ? nullptr : &found->second;
            }
        }

        template<class K>
        auto _found(const K& key) const noexcept(_nothrow_found<const Derived, K>()) {
            if constexpr (requires(const Derived& d) { d._value_of(key); }) {
                return _self()._value_of(key);
            } else {
                auto found = _self().find(key);
                return found == _self().end() ? nullptr : &found->second;
            }
        }
    };
}
