//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
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
    template<class Key, class T, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    class multimap : public detail::RbTree<detail::MapTraits<Key, T, Compare, true, Ptr>> {
        using Base = detail::RbTree<detail::MapTraits<Key, T, Compare, true, Ptr>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;

        using Base::Base;
        using Base::insert;

        multimap() = default;
        multimap(const multimap&) = default;
        multimap(multimap&&) = default;
        multimap& operator=(const multimap&) = default;
        multimap& operator=(multimap&&) = default;

        multimap& operator=(std::initializer_list<value_type> ilist) {
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
             class Compare = std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>, template<class> class Ptr = tracked_ptr>
    multimap(InputIt, InputIt, Compare = Compare())
        -> multimap<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                    typename std::iterator_traits<InputIt>::value_type::second_type, Compare, Ptr>;

    template<class Key, class T, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    multimap(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> multimap<Key, T, Compare, Ptr>;

    template<class Key, class T, class Compare, template<class> class Ptr>
    void swap(multimap<Key, T, Compare, Ptr>& lhs, multimap<Key, T, Compare, Ptr>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class T, class Compare, template<class> class Ptr, class Pred>
    typename multimap<Key, T, Compare, Ptr>::size_type erase_if(multimap<Key, T, Compare, Ptr>& c, Pred pred) {
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
