//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
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
    class multiset : public detail::RbTree<detail::SetTraits<Key, Compare, true>> {
        using Base = detail::RbTree<detail::SetTraits<Key, Compare, true>>;

    public:
        using key_type = Key;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;

        using Base::Base;

        multiset() = default;
        multiset(const multiset&) = default;
        multiset(multiset&&) = default;
        multiset& operator=(const multiset&) = default;
        multiset& operator=(multiset&&) = default;

        multiset& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }
    };

    template<std::input_iterator InputIt, class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>>
    multiset(InputIt, InputIt, Compare = Compare()) -> multiset<typename std::iterator_traits<InputIt>::value_type, Compare>;

    template<class Key, class Compare = std::less<Key>>
    multiset(std::initializer_list<Key>, Compare = Compare()) -> multiset<Key, Compare>;

    template<class Key, class Compare>
    void swap(multiset<Key, Compare>& lhs, multiset<Key, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class Compare, class Pred>
    typename multiset<Key, Compare>::size_type erase_if(multiset<Key, Compare>& c, Pred pred) {
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
