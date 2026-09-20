//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/mixin/mixin.h"
#include "detail/hash_table.h"

namespace sgcl {
    // std::unordered_set over managed nodes (detail/hash_table.h). The set
    // and its node handles hold tracked pointers: they live on a stack or
    // inside a managed object, never in unmanaged memory. An iterator is
    // one raw node pointer and may live anywhere (a std::vector of
    // iterators is fine): its node is rooted by the set while the element
    // is in it, and an iterator to an erased element is invalid as in std.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_set
    : public detail::HashTable<detail::HashSetTraits<Key, Hash, KeyEqual, true>>
    , public m_enumerable<unordered_set<Key, Hash, KeyEqual>> {
        using Base = detail::HashTable<detail::HashSetTraits<Key, Hash, KeyEqual, true>>;

    public:
        using key_type = Key;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;

        using Base::Base;

        // By the key, the container's own, in place of m_enumerable's walk
        using Base::contains;

        unordered_set& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

    private:
        friend bool operator==(const unordered_set& lhs, const unordered_set& rhs) {
            return lhs._equal_to(rhs);
        }

        friend void swap(unordered_set& lhs, unordered_set& rhs) noexcept(noexcept(lhs.swap(rhs))) {
            lhs.swap(rhs);
        }

    public:
        template<class Pred>
        size_type erase_if_impl(Pred& pred) {
            return this->_erase_if(pred);
        }
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
             class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
    unordered_set(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> unordered_set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    unordered_set(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> unordered_set<Key, Hash, KeyEqual>;

    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(unordered_set<Key, Hash, KeyEqual>& c, Pred pred) {
        return c.erase_if_impl(pred);
    }
}

namespace std {
    using sgcl::erase_if;
}
