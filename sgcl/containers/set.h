//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/rb_tree.h"

#include <functional>

namespace sgcl {
    // std::set on a garbage-collected red-black tree (detail/rb_tree.h).
    // The container holds a tracked pointer, so it lives on the stack or
    // inside a managed object only; its iterators are raw node pointers
    // (trivially copyable, storable anywhere) that stay valid for as long
    // as the element is in the container, exactly as in std.
    template<class Key, class Compare = std::less<Key>>
    class set : public detail::RbTree<detail::SetTraits<Key, Compare, false>> {
        using Base = detail::RbTree<detail::SetTraits<Key, Compare, false>>;

    public:
        using key_type = Key;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;
        using typename Base::insert_return_type;

        using Base::Base;

        set() = default;
        set(const set&) = default;
        set(set&&) = default;
        set& operator=(const set&) = default;
        set& operator=(set&&) = default;

        set& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }
    };

    template<std::input_iterator InputIt, class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>>
    set(InputIt, InputIt, Compare = Compare()) -> set<typename std::iterator_traits<InputIt>::value_type, Compare>;

    template<class Key, class Compare = std::less<Key>>
    set(std::initializer_list<Key>, Compare = Compare()) -> set<Key, Compare>;

    template<class Key, class Compare>
    void swap(set<Key, Compare>& lhs, set<Key, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class Compare, class Pred>
    typename set<Key, Compare>::size_type erase_if(set<Key, Compare>& c, Pred pred) {
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
