//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/hash_table.h"
#include "mixin/mixin.h"

namespace sgcl {
    // std::unordered_map over managed nodes (detail/hash_table.h). The map
    // and its node handles hold tracked pointers: they live on a stack or
    // inside a managed object, never in unmanaged memory. An iterator is
    // one raw node pointer and may live anywhere (a std::vector of
    // iterators is fine): its node is rooted by the map while the element
    // is in it, and an iterator to an erased element is invalid as in std.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map
    : public detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, true>>
    , public mixin::enumerable<map<Key, T, Hash, KeyEqual>>
    , public mixin::lookup<map<Key, T, Hash, KeyEqual>> {
        using Base = detail::HashTable<detail::HashMapTraits<Key, T, Hash, KeyEqual, true>>;

        static_assert(detail::nothrow_function_object<Hash, const Key&>, "sgcl::map: Hash must be noexcept");
        static_assert(detail::nothrow_function_object<KeyEqual, const Key&, const Key&>, "sgcl::map: KeyEqual must be noexcept");

    public:
        using key_type = Key;
        using mapped_type = T;
        using value_type = typename Base::value_type;
        using size_type = typename Base::size_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;

        using typename Base::insert_return_type;

        using Base::Base;

        // By the key, the container's own, in place of mixin::enumerable's walk
        using Base::contains;

        SGCL_INLINE_HOT map& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        template<class M>
        SGCL_INLINE_HOT pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) noexcept(std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, M&&> && std::is_nothrow_assignable_v<T&, M&&>) {
            return _insert_or_assign(key, std::forward<M>(obj));
        }

        template<class M>
        SGCL_INLINE_HOT pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) noexcept(std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_constructible_v<T, M&&> && std::is_nothrow_assignable_v<T&, M&&>) {
            return _insert_or_assign(std::move(key), std::forward<M>(obj));
        }

        template<class M>
        SGCL_INLINE_HOT iterator insert_or_assign(const_iterator, const key_type& key, M&& obj) noexcept(std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, M&&> && std::is_nothrow_assignable_v<T&, M&&>) {
            return _insert_or_assign(key, std::forward<M>(obj)).first;
        }

        template<class M>
        SGCL_INLINE_HOT iterator insert_or_assign(const_iterator, key_type&& key, M&& obj) noexcept(std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_constructible_v<T, M&&> && std::is_nothrow_assignable_v<T&, M&&>) {
            return _insert_or_assign(std::move(key), std::forward<M>(obj)).first;
        }

        // The mapped value is constructed in place from the arguments, and
        // only when the key is new.
        template<class... A>
        SGCL_INLINE_HOT pair<iterator, bool> try_emplace(const key_type& key, A&&... a) noexcept(std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A&&...>) {
            auto [node, inserted] = Base::_try_emplace(key, std::forward<A>(a)...);
            return {Base::_make_iterator(node), inserted};
        }

        template<class... A>
        SGCL_INLINE_HOT pair<iterator, bool> try_emplace(key_type&& key, A&&... a) noexcept(std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_constructible_v<T, A&&...>) {
            auto [node, inserted] = Base::_try_emplace(std::move(key), std::forward<A>(a)...);
            return {Base::_make_iterator(node), inserted};
        }

        template<class... A>
        SGCL_INLINE_HOT iterator try_emplace(const_iterator, const key_type& key, A&&... a) noexcept(std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<T, A&&...>) {
            return try_emplace(key, std::forward<A>(a)...).first;
        }

        template<class... A>
        SGCL_INLINE_HOT iterator try_emplace(const_iterator, key_type&& key, A&&... a) noexcept(std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_constructible_v<T, A&&...>) {
            return try_emplace(std::move(key), std::forward<A>(a)...).first;
        }

        SGCL_INLINE_HOT mapped_type& at(const key_type& key) {
            return _at(key);
        }

        SGCL_INLINE_HOT const mapped_type& at(const key_type& key) const {
            return _at(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT mapped_type& at(const K& key) {
            return _at(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT const mapped_type& at(const K& key) const {
            return _at(key);
        }

        SGCL_INLINE_HOT mapped_type& operator[](const key_type& key) noexcept(std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_default_constructible_v<T>) {
            return Base::_try_emplace(key).first->slot.value.second;
        }

        SGCL_INLINE_HOT mapped_type& operator[](key_type&& key) noexcept(std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_default_constructible_v<T>) {
            return Base::_try_emplace(std::move(key)).first->slot.value.second;
        }

        // The value under the key, moved out, and the element erased;
        // nothing when the key is absent: what Java's remove and C#'s
        // Remove(key, out value) hand back
        SGCL_INLINE_HOT optional<mapped_type> take(const key_type& key) noexcept(std::is_nothrow_move_constructible_v<T>) {
            return Base::_take(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT optional<mapped_type> take(const K& key) noexcept(noexcept(Base::_take(key))) {
            return Base::_take(key);
        }

    private:
        template<class K, class M>
        SGCL_INLINE_HOT pair<iterator, bool> _insert_or_assign(K&& key, M&& obj) noexcept(std::is_nothrow_constructible_v<Key, K&&> && std::is_nothrow_constructible_v<T, M&&> && std::is_nothrow_assignable_v<T&, M&&>) {
            auto [node, inserted] = Base::_try_emplace(std::forward<K>(key), std::forward<M>(obj));
            if (!inserted) {
                node->slot.value.second = std::forward<M>(obj);
            }
            return {Base::_make_iterator(node), inserted};
        }

        template<class K>
        SGCL_INLINE_HOT mapped_type& _at(const K& key) const {
            auto node = Base::_find(key);
            if (!node) {
                throw out_of_range("sgcl::map::at");
            }
            return node->slot.value.second;
        }

        SGCL_INLINE_HOT friend bool operator==(const map& lhs, const map& rhs) {
            return lhs._equal_to(rhs);
        }

        SGCL_INLINE_HOT friend void swap(map& lhs, map& rhs) noexcept(noexcept(lhs.swap(rhs))) {
            lhs.swap(rhs);
        }

        template<class K, class V, class H, class E, class Pred>
        friend size_t erase_if(map<K, V, H, E>& c, Pred pred);
    };

    template<std::input_iterator InputIt,
             class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
             class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
    map(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                         typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    map(std::initializer_list<pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
        -> map<Key, T, Hash, KeyEqual>;

    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    SGCL_INLINE_HOT size_t erase_if(map<Key, T, Hash, KeyEqual>& c, Pred pred) {
        return c._erase_if(pred);
    }
}

namespace std {
    using sgcl::erase_if;
}
