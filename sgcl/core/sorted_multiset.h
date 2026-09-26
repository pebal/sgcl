//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/rb_tree.h"

#include <functional>

namespace sgcl {
    // std::multiset on a garbage-collected red-black tree (detail/rb_tree.h).
    // The container holds a tracked pointer, so it lives on the stack or
    // inside a managed object only; its iterators are raw node pointers
    // (trivially copyable, storable anywhere) that stay valid for as long
    // as the element is in the container, exactly as in std.
    template<class Key, class Compare = std::less<Key>>
    class sorted_multiset
    : public detail::RbTree<detail::SetTraits<Key, Compare, true>>
    , public mixin::bidirectional<sorted_multiset<Key, Compare>>
    , public mixin::comparable<sorted_multiset<Key, Compare>>
    , public mixin::enumerable<sorted_multiset<Key, Compare>>
    , public mixin::equatable<sorted_multiset<Key, Compare>> {
        using Base = detail::RbTree<detail::SetTraits<Key, Compare, true>>;

    public:
        using key_type = Key;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;

        using Base::Base;

        // By the key, the container's own, in place of mixin::enumerable's walk
        using Base::contains;

        // The smallest and the largest element are the ends of the order,
        // O(1), in place of mixin::enumerable's walk (hidden, the overloads with
        // a comparator too: the container orders by its own comparator)

        const value_type& min() const noexcept {
            return *this->begin();
        }

        const value_type& max() const noexcept {
            return *this->rbegin();
        }

        sorted_multiset() = default;
        sorted_multiset(const sorted_multiset&) = default;
        sorted_multiset(sorted_multiset&&) = default;
        sorted_multiset& operator=(const sorted_multiset&) = default;
        sorted_multiset& operator=(sorted_multiset&&) = default;

        sorted_multiset& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }
    };

    template<std::input_iterator InputIt, class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>>
    sorted_multiset(InputIt, InputIt, Compare = Compare()) -> sorted_multiset<typename std::iterator_traits<InputIt>::value_type, Compare>;

    template<class Key, class Compare = std::less<Key>>
    sorted_multiset(std::initializer_list<Key>, Compare = Compare()) -> sorted_multiset<Key, Compare>;

    template<class Key, class Compare>
    void swap(sorted_multiset<Key, Compare>& lhs, sorted_multiset<Key, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class Compare, class Pred>
    typename sorted_multiset<Key, Compare>::size_type erase_if(sorted_multiset<Key, Compare>& c, Pred pred) {
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
