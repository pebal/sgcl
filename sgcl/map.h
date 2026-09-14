//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/rb_tree.h"

#include <functional>
#include <stdexcept>

namespace sgcl {
    // std::map on a garbage-collected red-black tree (detail/rb_tree.h).
    // The container holds a tracked pointer, so it lives on the stack or
    // inside a managed object only; its iterators are raw node pointers
    // (trivially copyable, storable anywhere) that stay valid for as long
    // as the element is in the container, exactly as in std.
    template<class Key, class T, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    class map : public detail::RbTree<detail::MapTraits<Key, T, Compare, false, Ptr>> {
        using Base = detail::RbTree<detail::MapTraits<Key, T, Compare, false, Ptr>>;

    public:
        using key_type = Key;
        using mapped_type = T;
        using typename Base::value_type;
        using typename Base::iterator;
        using typename Base::const_iterator;
        using typename Base::insert_return_type;

        using Base::Base;
        using Base::insert;

        map() = default;
        map(const map&) = default;
        map(map&&) = default;
        map& operator=(const map&) = default;
        map& operator=(map&&) = default;

        map& operator=(std::initializer_list<value_type> ilist) {
            Base::operator=(ilist);
            return *this;
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        std::pair<iterator, bool> insert(P&& value) {
            return this->emplace(std::forward<P>(value));
        }

        template<class P> requires std::is_constructible_v<value_type, P&&>
        iterator insert(const_iterator hint, P&& value) {
            return this->emplace_hint(hint, std::forward<P>(value));
        }

        template<class M>
        std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
            return _insert_or_assign(key, std::forward<M>(obj));
        }

        template<class M>
        std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
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
        std::pair<iterator, bool> try_emplace(const key_type& key, A&&... a) {
            return _try_emplace(key, std::forward<A>(a)...);
        }

        template<class... A>
        std::pair<iterator, bool> try_emplace(key_type&& key, A&&... a) {
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
                throw std::out_of_range("sgcl::map::at");
            }
            return it->second;
        }

        const mapped_type& at(const key_type& key) const {
            auto it = this->find(key);
            if (it == this->end()) {
                throw std::out_of_range("sgcl::map::at");
            }
            return it->second;
        }

        mapped_type& operator[](const key_type& key) {
            return _try_emplace(key).first->second;
        }

        mapped_type& operator[](key_type&& key) {
            return _try_emplace(std::move(key)).first->second;
        }

    private:
        template<class K, class... A>
        std::pair<iterator, bool> _try_emplace(K&& key, A&&... a) {
            this->_ensure_header();
            auto pos = this->_unique_pos(key);
            if (pos.existing) {
                return {iterator(pos.existing), false};
            }
            return {this->_insert_at(pos, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<A>(a)...)), true};
        }

        template<class K, class... A>
        iterator _try_emplace_hint(const_iterator hint, K&& key, A&&... a) {
            this->_ensure_header();
            auto pos = this->_unique_hint_pos(this->_raw(hint), key);
            if (pos.existing) {
                return iterator(pos.existing);
            }
            return this->_insert_at(pos, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<A>(a)...));
        }

        template<class K, class M>
        std::pair<iterator, bool> _insert_or_assign(K&& key, M&& obj) {
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
             class Compare = std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>, template<class> class Ptr = tracked_ptr>
    map(InputIt, InputIt, Compare = Compare())
        -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
               typename std::iterator_traits<InputIt>::value_type::second_type, Compare, Ptr>;

    template<class Key, class T, class Compare = std::less<Key>, template<class> class Ptr = tracked_ptr>
    map(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> map<Key, T, Compare, Ptr>;

    template<class Key, class T, class Compare, template<class> class Ptr>
    void swap(map<Key, T, Compare, Ptr>& lhs, map<Key, T, Compare, Ptr>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    template<class Key, class T, class Compare, template<class> class Ptr, class Pred>
    typename map<Key, T, Compare, Ptr>::size_type erase_if(map<Key, T, Compare, Ptr>& c, Pred pred) {
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
