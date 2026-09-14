//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
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
    template<class Key, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    class set : public detail::RbTree<detail::SetTraits<Key, Compare, false, Ptr>> {
        using Base = detail::RbTree<detail::SetTraits<Key, Compare, false, Ptr>>;

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

    template<std::input_iterator InputIt, class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>, template<class> class Ptr = tracked_ptr>
    set(InputIt, InputIt, Compare = Compare()) -> set<typename std::iterator_traits<InputIt>::value_type, Compare, Ptr>;

    template<class Key, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    set(std::initializer_list<Key>, Compare = Compare()) -> set<Key, Compare, Ptr>;

    template<class Key, class Compare, template<class> class Ptr>
    void swap(set<Key, Compare, Ptr>& lhs, set<Key, Compare, Ptr>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class Compare, template<class> class Ptr, class Pred>
    typename set<Key, Compare, Ptr>::size_type erase_if(set<Key, Compare, Ptr>& c, Pred pred) {
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
