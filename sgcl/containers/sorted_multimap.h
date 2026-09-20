//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/rb_tree.h"

#include <functional>

namespace sgcl {
    // std::multimap on a garbage-collected red-black tree (detail/rb_tree.h).
    // The container holds a tracked pointer, so it lives on the stack or
    // inside a managed object only; its iterators are raw node pointers
    // (trivially copyable, storable anywhere) that stay valid for as long
    // as the element is in the container, exactly as in std.
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_multimap
    : public detail::RbTree<detail::MapTraits<Key, T, Compare, true>>
    , public m_enumerable<sorted_multimap<Key, T, Compare>>
    , public m_bidirectional<sorted_multimap<Key, T, Compare>>
    , public m_equatable<sorted_multimap<Key, T, Compare>>
    , public m_comparable<sorted_multimap<Key, T, Compare>>
    , public m_lookup<sorted_multimap<Key, T, Compare>> {
        using Base = detail::RbTree<detail::MapTraits<Key, T, Compare, true>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;

        using Base::Base;

        // By the key, the container's own, in place of m_enumerable's walk
        using Base::contains;

        // The smallest and the largest element are the ends of the order,
        // O(1), in place of m_enumerable's walk (hidden, the overloads with
        // a comparator too: the container orders by its own comparator)

        const value_type& min() const noexcept {
            return *this->begin();
        }

        const value_type& max() const noexcept {
            return *this->rbegin();
        }
        using Base::insert;

        sorted_multimap() = default;
        sorted_multimap(const sorted_multimap&) = default;
        sorted_multimap(sorted_multimap&&) = default;
        sorted_multimap& operator=(const sorted_multimap&) = default;
        sorted_multimap& operator=(sorted_multimap&&) = default;

        sorted_multimap& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(P&& value) {
            return this->emplace(std::forward<P>(value));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator hint, P&& value) {
            return this->emplace_hint(hint, std::forward<P>(value));
        }
    };

    template<std::input_iterator InputIt,
             class Compare = std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    sorted_multimap(InputIt, InputIt, Compare = Compare())
        -> sorted_multimap<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                    typename std::iterator_traits<InputIt>::value_type::second_type, Compare>;

    template<class Key, class T, class Compare = std::less<Key>>
    sorted_multimap(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> sorted_multimap<Key, T, Compare>;

    template<class Key, class T, class Compare>
    void swap(sorted_multimap<Key, T, Compare>& lhs, sorted_multimap<Key, T, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class T, class Compare, class Pred>
    typename sorted_multimap<Key, T, Compare>::size_type erase_if(sorted_multimap<Key, T, Compare>& c, Pred pred) {
        auto old_size = c.size();
        for (auto it = c.begin(), last = c.end(); it != last;) {
            if (pred(*it)) {
                it = c.erase(it);
            } else {
                ++it;
            }
        }
        return old_size - c.size();
    }
}

namespace std {
    using sgcl::erase_if;
}
