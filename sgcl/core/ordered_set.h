//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/hash_table.h"
#include "mixin/mixin.h"

namespace sgcl {
    // A hash set iterated in insertion order: Java's LinkedHashSet. The
    // interface of ordered_set over the same table (detail/hash_table.h),
    // with the elements on one more list in the order they were
    // inserted: begin to end walks it, both ways (bidirectional
    // iterators, rbegin), front is the oldest element and back the
    // newest, a copy keeps the order, an erase takes an element out of
    // it, a re-insert of a present element leaves it where it was;
    // to_back and to_front move an element to the end (the start) of the
    // order. Two words more per node than ordered_set. The set and its
    // node handles hold tracked pointers: they live on a stack or inside
    // a managed object; an iterator is one raw node pointer and may live
    // anywhere, invalid once its element is erased, as in std.
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_set
    : public detail::HashTable<detail::HashSetTraits<Key, Hash, KeyEqual, true, true>>
    , public mixin::enumerable<ordered_set<Key, Hash, KeyEqual>> {
        using Base = detail::HashTable<detail::HashSetTraits<Key, Hash, KeyEqual, true, true>>;

    public:
        using key_type = Key;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using Base::Base;

        // By the key, the container's own, in place of mixin::enumerable's walk
        using Base::contains;

        ordered_set& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        // The order, from the newest element back to the oldest
        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(this->end());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(this->begin());
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        // The oldest and the newest element (the set not empty)
        const value_type& front() const noexcept {
            return *this->begin();
        }

        const value_type& back() const noexcept {
            return *std::prev(this->end());
        }

        // The element moved to the end (the start) of the order: as if
        // inserted last (first) from now on; the iterator stays valid
        void to_back(const_iterator pos) noexcept {
            Base::_to_back(Base::_node_of(pos));
        }

        void to_front(const_iterator pos) noexcept {
            Base::_to_front(Base::_node_of(pos));
        }

    private:
        friend bool operator==(const ordered_set& lhs, const ordered_set& rhs) {
            return lhs._equal_to(rhs);
        }

        friend void swap(ordered_set& lhs, ordered_set& rhs) noexcept(noexcept(lhs.swap(rhs))) {
            lhs.swap(rhs);
        }

        template<class K, class H, class E, class Pred>
        friend size_t erase_if(ordered_set<K, H, E>& c, Pred pred);
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
             class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
    ordered_set(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> ordered_set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    ordered_set(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> ordered_set<Key, Hash, KeyEqual>;

    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(ordered_set<Key, Hash, KeyEqual>& c, Pred pred) {
        return c._erase_if(pred);
    }
}

namespace std {
    using sgcl::erase_if;
}
