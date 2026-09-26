//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/rb_tree.h"

#include <functional>
#include <stdexcept>

namespace sgcl {
    // std::map on a garbage-collected red-black tree (detail/rb_tree.h).
    // The container holds a tracked pointer, so it lives on the stack or
    // inside a managed object only; its iterators are raw node pointers
    // (trivially copyable, storable anywhere) that stay valid for as long
    // as the element is in the container, exactly as in std.
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_map
    : public detail::RbTree<detail::MapTraits<Key, T, Compare, false>>
    , public mixin::bidirectional<sorted_map<Key, T, Compare>>
    , public mixin::comparable<sorted_map<Key, T, Compare>>
    , public mixin::enumerable<sorted_map<Key, T, Compare>>
    , public mixin::equatable<sorted_map<Key, T, Compare>>
    , public mixin::lookup<sorted_map<Key, T, Compare>> {
        using Base = detail::RbTree<detail::MapTraits<Key, T, Compare, false>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;
        using typename Base::insert_return_type;

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
        using Base::insert;

        sorted_map() = default;
        sorted_map(const sorted_map&) = default;
        sorted_map(sorted_map&&) = default;
        sorted_map& operator=(const sorted_map&) = default;
        sorted_map& operator=(sorted_map&&) = default;

        sorted_map& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        pair<iterator, bool> insert(P&& value) {
            return this->emplace(std::forward<P>(value));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator hint, P&& value) {
            return this->emplace_hint(hint, std::forward<P>(value));
        }

        template<class M>
        pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
            return _insert_or_assign(key, std::forward<M>(obj));
        }

        template<class M>
        pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
            return _insert_or_assign(std::move(key), std::forward<M>(obj));
        }

        template<class M>
        iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj) {
            return _insert_or_assign_hint(hint, key, std::forward<M>(obj));
        }

        template<class M>
        iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj) {
            return _insert_or_assign_hint(hint, std::move(key), std::forward<M>(obj));
        }

        // The mapped value is built in place from the arguments: it need
        // not be movable.
        template<class... A>
        pair<iterator, bool> try_emplace(const key_type& key, A&&... a) {
            return _try_emplace(key, std::forward<A>(a)...);
        }

        template<class... A>
        pair<iterator, bool> try_emplace(key_type&& key, A&&... a) {
            return _try_emplace(std::move(key), std::forward<A>(a)...);
        }

        template<class... A>
        iterator try_emplace(const_iterator hint, const key_type& key, A&&... a) {
            return _try_emplace_hint(hint, key, std::forward<A>(a)...);
        }

        template<class... A>
        iterator try_emplace(const_iterator hint, key_type&& key, A&&... a) {
            return _try_emplace_hint(hint, std::move(key), std::forward<A>(a)...);
        }

        mapped_type& at(const key_type& key) {
            auto it = this->find(key);
            if (it == this->end()) {
                throw out_of_range("sgcl::sorted_map::at");
            }
            return it->second;
        }

        const mapped_type& at(const key_type& key) const {
            auto it = this->find(key);
            if (it == this->end()) {
                throw out_of_range("sgcl::sorted_map::at");
            }
            return it->second;
        }

        // With a key of another type the comparator takes (is_transparent):
        // a string_view for a string
        template<class K> requires detail::TransparentCompare<Compare>
        mapped_type& at(const K& key) {
            auto it = this->find(key);
            if (it == this->end()) {
                throw out_of_range("sgcl::sorted_map::at");
            }
            return it->second;
        }

        template<class K> requires detail::TransparentCompare<Compare>
        const mapped_type& at(const K& key) const {
            auto it = this->find(key);
            if (it == this->end()) {
                throw out_of_range("sgcl::sorted_map::at");
            }
            return it->second;
        }

        // The value under the key, moved out, and the element erased;
        // nothing when the key is absent: what Java's remove and C#'s
        // Remove(key, out value) hand back
        optional<mapped_type> take(const key_type& key) {
            return _take(key);
        }

        template<class K> requires detail::TransparentCompare<Compare>
        optional<mapped_type> take(const K& key) {
            return _take(key);
        }

        mapped_type& operator[](const key_type& key) {
            return _try_emplace(key).first->second;
        }

        mapped_type& operator[](key_type&& key) {
            return _try_emplace(std::move(key)).first->second;
        }

    private:
        template<class K>
        optional<mapped_type> _take(const K& key) {
            auto it = this->find(key);
            if (it == this->end()) {
                return nullopt;
            }
            optional<mapped_type> value(std::move(it->second));
            this->erase(it);
            return value;
        }

        template<class K, class... A>
        pair<iterator, bool> _try_emplace(K&& key, A&&... a) {
            this->_ensure_header();
            auto pos = this->_unique_pos(key);
            if (pos.existing) {
                return {iterator(pos.existing), false};
            }
            return {this->_insert_at(pos, std::piecewise_construct, forward_as_tuple(std::forward<K>(key)), forward_as_tuple(std::forward<A>(a)...)), true};
        }

        template<class K, class... A>
        iterator _try_emplace_hint(const_iterator hint, K&& key, A&&... a) {
            this->_ensure_header();
            auto pos = this->_unique_hint_pos(this->_raw(hint), key);
            if (pos.existing) {
                return iterator(pos.existing);
            }
            return this->_insert_at(pos, std::piecewise_construct, forward_as_tuple(std::forward<K>(key)), forward_as_tuple(std::forward<A>(a)...));
        }

        template<class K, class M>
        pair<iterator, bool> _insert_or_assign(K&& key, M&& obj) {
            this->_ensure_header();
            auto pos = this->_unique_pos(key);
            if (pos.existing) {
                this->_node(pos.existing)->slot.value.second = std::forward<M>(obj);
                return {iterator(pos.existing), false};
            }
            return {this->_insert_at(pos, std::forward<K>(key), std::forward<M>(obj)), true};
        }

        template<class K, class M>
        iterator _insert_or_assign_hint(const_iterator hint, K&& key, M&& obj) {
            this->_ensure_header();
            auto pos = this->_unique_hint_pos(this->_raw(hint), key);
            if (pos.existing) {
                this->_node(pos.existing)->slot.value.second = std::forward<M>(obj);
                return iterator(pos.existing);
            }
            return this->_insert_at(pos, std::forward<K>(key), std::forward<M>(obj));
        }
    };

    template<std::input_iterator InputIt,
             class Compare = std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    sorted_map(InputIt, InputIt, Compare = Compare())
        -> sorted_map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
               typename std::iterator_traits<InputIt>::value_type::second_type, Compare>;

    template<class Key, class T, class Compare = std::less<Key>>
    sorted_map(std::initializer_list<pair<Key, T>>, Compare = Compare()) -> sorted_map<Key, T, Compare>;

    template<class Key, class T, class Compare>
    void swap(sorted_map<Key, T, Compare>& lhs, sorted_map<Key, T, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class T, class Compare, class Pred>
    typename sorted_map<Key, T, Compare>::size_type erase_if(sorted_map<Key, T, Compare>& c, Pred pred) {
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
