//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/hash_table.h"

namespace sgcl {
    // std::unordered_multimap over managed nodes (detail/hash_table.h).
    // Elements with equal keys are adjacent in the iteration order. The map
    // and its node handles hold tracked pointers: they live on a stack or
    // inside a managed object, never in unmanaged memory. An iterator is
    // one raw node pointer and may live anywhere (a std::vector of
    // iterators is fine): its node is rooted by the map while the element
    // is in it, and an iterator to an erased element is invalid as in std.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_multimap : public detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, false>> {
        using Base = detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, false>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;

        using Base::Base;

        unordered_multimap& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

    private:
        friend bool operator==(const unordered_multimap& lhs, const unordered_multimap& rhs) {
            return lhs._equal_to(rhs);
        }

        friend void swap(unordered_multimap& lhs, unordered_multimap& rhs) noexcept(noexcept(lhs.swap(rhs))) {
            lhs.swap(rhs);
        }

    public:
        template<class Pred>
        size_type erase_if_impl(Pred& pred) {
            return this->_erase_if(pred);
        }
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
             class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    unordered_multimap(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> unordered_multimap<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                              typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    unordered_multimap(std::initializer_list<std::pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> unordered_multimap<Key, T, Hash, KeyEqual>;

    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(unordered_multimap<Key, T, Hash, KeyEqual>& c, Pred pred) {
        return c.erase_if_impl(pred);
    }
}

namespace std {
    using sgcl::erase_if;
}
