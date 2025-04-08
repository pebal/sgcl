//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "make_tracked.h"
#include "tracked_ptr.h"

#include <list>

namespace sgcl {
    template<class T>
    class list {
        struct Node;
        using NodePtr = tracked_ptr<Node>;
        using TailPtr = tracked_ptr<tracked_ptr<Node>>;

        struct Node {
            Node(const T& d)
            : data(d) {
            }

            Node(T&& d)
            : data(std::move(d)) {
            }

            template<class... A>
            Node(A&&... a)
            : data(std::forward<A>(a)...) {
            }


            T data;
            NodePtr prev;
            NodePtr next;
        };

        template<class U>
        class Iterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = U;
            using difference_type = ptrdiff_t;
            using pointer = U*;
            using const_pointer = const U*;
            using reference = U&;
            using const_reference = const U&;
            using reverse_iterator = std::reverse_iterator<Iterator>;

            Iterator() = default;

            explicit Iterator(std::nullptr_t, const TailPtr& tail) noexcept
            : _tail(tail) {
            }

            explicit Iterator(const NodePtr& node, const TailPtr& tail) noexcept
            : _node(node)
            , _tail(tail) {
            }

            reference operator*() const noexcept {
                return _node->data;
            }

            pointer operator->() const noexcept {
                return &_node->data;
            }

            Iterator& operator++() noexcept {
                if (_node) {
                    _node = _node->next;
                }
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            Iterator& operator--() noexcept {
                if (_node) {
                    _node = _node->prev;
                } else {
                    _node = *_tail;
                }
                return *this;
            }

            Iterator operator--(int) noexcept {
                Iterator tmp = *this;
                --(*this);
                return tmp;
            }

            bool operator==(const Iterator<std::remove_cv_t<T>>& other) const noexcept {
                return (_node <=> other._node) == 0;
            }

            std::strong_ordering operator<=>(const Iterator<std::remove_cv_t<T>>& other) const noexcept {
                return (_node <=> other._node);
            }

            bool operator==(const Iterator<const std::remove_cv_t<T>>& other) const noexcept {
                return (_node <=> other._node) == 0;
            }

            std::strong_ordering operator<=>(const Iterator<const std::remove_cv_t<T>>& other) const noexcept {
                return (_node <=> other._node);
            }

            reverse_iterator make_reverse_iterator() const noexcept {
                return reverse_iterator(*this);
            }

            operator Iterator<const value_type>&() const noexcept {
                return *(Iterator<const value_type>*)(this);
            }

        private:
            NodePtr _node;
            TailPtr _tail;

            template<class> friend class Iterator;
            template<class> friend class list;
        };

    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using iterator = Iterator<value_type>;
        using const_iterator = Iterator<const value_type>;
        using reverse_iterator = typename iterator::reverse_iterator;
        using const_reverse_iterator = typename const_iterator::reverse_iterator;

        constexpr list() noexcept
        : _size(0) {
        }

        explicit list(size_t n)
        : list() {
            assign(n, T());
        }

        list(size_t n, const T& value)
        : list() {
            assign(n, value);
        }

        list(std::initializer_list<T> ilist)
        : list() {
            assign(ilist);
        }

        template<std::input_iterator InputIt>
        list(InputIt first, InputIt last)
        : list() {
            assign(first, last);
        }

        list(const list& other)
        : list() {
            assign(other.begin(), other.end());
        }

        list(list&& other) noexcept
        : _head(std::move(other._head))
        , _tail_ptr(std::move(other._tail_ptr))
        , _size(other._size) {
            other._head = nullptr;
            other._tail_ptr = nullptr;
            other._size = 0;
        }

        list& operator=(const list& other) {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        list& operator=(list&& other) noexcept {
            if (this != &other) {
                clear();
                _head = other._head;
                _tail_ptr = other._tail_ptr;
                _size = other._size;
                other._head = nullptr;
                other._tail_ptr = nullptr;
                other._size = 0;
            }
            return *this;
        }

        list& operator=(std::initializer_list<T> ilist) {
            assign(ilist);
            return *this;
        }

        void assign(size_t n, const T& value) {
            if (n) {
                auto node = _head;
                for (size_t i = 0; i < n; ++i) {
                    if (node) {
                        node->data = value;
                        node = node->next;
                    } else {
                        push_back(value);
                    }
                }
                if (node && node != _head) {
                    _tail() = node->prev;
                    node->prev->next = nullptr;
                    node->prev = nullptr;
                    _size = n;
                }
            } else {
                clear();
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            if (first != last) {
                size_t size = 0;
                auto node = _head;
                for (auto i = first; i != last; ++i) {
                    if (node) {
                        node->data = *i;
                        node = node->next;
                        ++size;
                    } else {
                        push_back(*i);
                    }
                }
                if (node && node != _head) {
                    _tail() = node->prev;
                    node->prev->next = nullptr;
                    node->prev = nullptr;
                    _size = size;
                }
            } else {
                clear();
            }
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        T& front() noexcept {
            return _head->data;
        }

        const T& front() const noexcept {
            return _head->data;
        }

        T& back() noexcept {
            return _tail()->data;
        }

        const T& back() const noexcept {
            return _tail()->data;
        }

        iterator begin() noexcept {
            return iterator(_head, _tail_ptr);
        }

        const_iterator begin() const noexcept {
            return cbegin();
        }

        iterator end() noexcept {
            return iterator(nullptr, _tail_ptr);
        }

        const_iterator end() const noexcept {
            return cend();
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(_head, _tail_ptr);
        }

        const_iterator cend() const noexcept {
            return const_iterator(nullptr, _tail_ptr);
        }

        reverse_iterator rbegin() noexcept {
            return end().make_reverse_iterator();
        }

        const_reverse_iterator rbegin() const noexcept {
            return crbegin();
        }

        reverse_iterator rend() noexcept {
            return begin().make_reverse_iterator();
        }

        const_reverse_iterator rend() const noexcept {
            return crend();
        }

        const_reverse_iterator crbegin() const noexcept {
            return cend().make_reverse_iterator();
        }

        const_reverse_iterator crend() const noexcept {
            return cbegin().make_reverse_iterator();
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        size_t size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<ptrdiff_t>::max();
        }

        void push_back(const T& value) {
            NodePtr node = make_tracked<Node>(value);
            _push_back(node);
        }

        void push_back(T&& value) {
            NodePtr node = make_tracked<Node>(std::move(value));
            _push_back(node);
        }

        template<class ...A>
        reference emplace_back(A&&... a) {
            NodePtr node = make_tracked<Node>(std::forward<A>(a)...);
            _push_back(node);
            return node->data;
        }

        void push_front(const T& value) {
            NodePtr node = make_tracked<Node>(value);
            _push_front(node);
        }

        void push_front(T&& value) {
            NodePtr node = make_tracked<Node>(std::move(value));
            _push_front(node);
        }

        template<class ...A>
        reference emplace_front(A&&... a) {
            NodePtr node = make_tracked<Node>(std::forward<A>(a)...);
            _push_front(node);
            return node->data;
        }

        void pop_back() noexcept {
            auto& tail = _tail();
            if (!tail) {
                return;
            }
            tail = tail->prev;
            if (tail) {
                tail->next = nullptr;
            } else {
                _head = nullptr;
            }
            --_size;
        }

        void pop_front() noexcept {
            if (!_head) {
                return;
            }
            _head = _head->next;
            if (_head) {
                _head->prev = nullptr;
            } else {
                _tail() = nullptr;
            }
            --_size;
        }

        template<class... A>
        iterator emplace(const const_iterator& pos, A&&... a) {
            if (pos == begin()) {
                emplace_front(std::forward<A>(a)...);
                return begin();
            } else if (pos == end()) {
                emplace_back(std::forward<A>(a)...);
                return --end();
            } else {
                const NodePtr& current = pos._node;
                tracked_ptr node = make_tracked<Node>(std::forward<A>(a)...);
                node->prev = current->prev;
                node->next = current;
                current->prev->next = node;
                current->prev = node;
                ++_size;
                return iterator(node, _tail_ptr);
            }
        }

        iterator insert(const const_iterator& pos, const T& value) {
            return emplace(pos, value);
        }

        iterator insert(const const_iterator& pos, T&& value) {
            return emplace(pos, std::move(value));
        }

        iterator insert(const const_iterator& pos, size_t count, const T& value) {
            if (!count) {
                return iterator(pos._node, pos._tail);
            }
            if (pos == begin()) {
                for (size_t i = 0; i < count; ++i) {
                    push_front(value);
                }
                return begin();
            } else if (pos == end()) {
                iterator it = --end();
                for (size_t i = 0; i < count; ++i) {
                    push_back(value);
                }
                return ++it;
            } else {
                const NodePtr& current = pos._node;
                NodePtr node;
                NodePtr first_insert;
                for (size_t i = 0; i < count; ++i) {
                    node = make_tracked<Node>(value);
                    node->prev = current->prev;
                    node->next = current;
                    current->prev->next = node;
                    current->prev = node;
                    if (!first_insert) {
                        first_insert = node;
                    }
                    ++_size;
                }
                return iterator(first_insert, _tail_ptr);
            }
        }

        template<std::input_iterator InputIt>
        iterator insert(const const_iterator& pos, InputIt first, InputIt last) {
            if (first == last) {
                return iterator(pos._node, pos._tail);
            }
            if (pos == begin()) {
                do {
                    --last;
                    push_front(*last);
                } while(last != first);
                return begin();
            } else if (pos == end()) {
                iterator it = --end();
                for (auto i = first; i != last; ++i) {
                    push_back(*i);
                }
                return ++it;
            } else {
                const NodePtr& current = pos._node;
                NodePtr node;
                NodePtr first_insert;
                for (auto i = first; i != last; ++i) {
                    node = make_tracked<Node>(*i);
                    node->prev = current->prev;
                    node->next = current;
                    current->prev->next = node;
                    current->prev = node;
                    if (!first_insert) {
                        first_insert = node;
                    }
                    ++_size;
                }
                return iterator(first_insert, _tail_ptr);
            }
        }

        iterator insert(const const_iterator& pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        iterator erase(const const_iterator& pos) noexcept {
            if (pos == end()) {
                return iterator(pos._node, pos._tail);
            }
            const NodePtr& node = pos._node;
            if (node->prev) {
                node->prev->next = node->next;
            } else {
                _head = node->next;
            }
            if (node->next) {
                node->next->prev = node->prev;
            } else {
                _tail() = node->prev;
            }
            --_size;
            return iterator(node->next, _tail_ptr);
        }

        iterator erase(const_iterator first, const const_iterator& last) noexcept {
            if (first == last) {
                return iterator(last._node, last._tail);
            }
            if (first._node->prev) {
                first._node->prev->next = last._node;
            } else {
                _head = last._node;
            }
            if (last._node) {
                last._node->prev = first._node->prev;
            } else {
                _tail() = first._node->prev;
            }
            while(first != last) {
                --_size;
                ++first;
            };
            return iterator(last._node, last._tail);
        }

        void resize(size_type count) {
            resize(count, T());
        }

        void resize(size_type count, const value_type& value) {
            if (count == 0) {
                clear();
                return;
            }
            if (count < _size) {
                _size = count;
                auto node = _head;
                while(--count) {
                    node = node->next;
                }
                _tail() = node;
                if (node->next) {
                    node->next->prev = nullptr;
                }
                node->next = nullptr;
            } else if (count > _size) {
                insert(end(), count - _size, value);
            }
        }

        void swap(list& other) noexcept {
            _head.swap(other._head);
            _tail_ptr.swap(other._tail_ptr);
            std::swap(_size, other._size);
        }

        void merge(list& other) noexcept {
            _merge(other, std::less<>());
        }

        void merge(list&& other) noexcept {
            _merge(other, std::less<>());
        }

        template<class Compare>
        void merge(list& other, Compare comp) noexcept {
            _merge(other, comp);
        }

        template<class Compare>
        void merge(list&& other, Compare comp) noexcept {
            _merge(other, comp);
        }

        void sort() noexcept {
            _sort(std::less<>());
        }

        template<class Compare>
        void sort(Compare comp) noexcept {
            _sort(comp);
        }

        void splice(const_iterator pos, list& other) noexcept {
            if (other.empty()) {
                return;
            }
            _size += other._size;
            _splice(pos._node, other._head, other._tail_ptr);
            other.clear();
        }

        void splice(const_iterator pos, list&& other) noexcept {
            splice(pos, other);
        }

        void splice(const_iterator pos, list& other, const_iterator it) noexcept {
            if (it == other.end()) {
                return;
            }
            _size += other._size;
            other.erase(it, other.end());
            _size -= other._size;
            _splice(pos._node, it._node, nullptr);
        }

        void splice(const_iterator pos, list&& other, const_iterator it) noexcept {
            splice(pos, other, it);
        }

        void splice(const_iterator pos, list& other, const_iterator first, const_iterator last) noexcept {
            if (first == last) {
                return;
            }
            _size += other._size;
            other.erase(first, last);
            _size -= other._size;
            _splice(pos._node, first._node, last._node);
        }

        void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last) noexcept {
            splice(pos, other, first, last);
        }

        size_type remove(const T& value) noexcept {
            return remove_if([&value](const T& data){ return data == value; });
        }

        template <class UnaryPredicate>
        size_type remove_if(UnaryPredicate p) noexcept {
            size_type removed = 0;
            NodePtr node = _head;
            NodePtr next;
            while(node) {
                next = node->next;
                if (p(node->data)) {
                    if (node->prev) {
                        node->prev->next = node->next;
                    } else {
                        _head = node->next;
                    }
                    if (node->next) {
                        node->next->prev = node->prev;
                    } else {
                        _tail() = node->prev;
                    }
                    ++removed;
                    --_size;
                }
                node = next;
            }
            return removed;
        }

        void reverse() noexcept {
            NodePtr node = _head;
            NodePtr temp;
            while(node) {
                temp = node->prev;
                node->prev = node->next;
                node->next = temp;
                node = node->prev;
            }
            std::swap(_head, _tail());
        }

        size_type unique() noexcept {
            return unique([](const T& a, const T& b) { return a == b; });
        }

        template <class BinaryPredicate>
        size_type unique(BinaryPredicate p) noexcept {
            size_type removed = 0;
            if (!_head) {
                return removed;
            }
            NodePtr node = _head;
            NodePtr dup;
            while (node && node->next) {
                if (p(node->data, node->next->data)) {
                    dup = node->next;
                    node->next = dup->next;
                    if (dup->next) {
                        dup->next->prev = node;
                    } else {
                        _tail() = node;
                    }
                    ++removed;
                    --_size;
                } else {
                    node = node->next;
                }
            }
            return removed;
        }

        void clear() noexcept {
            _head = nullptr;
            _tail() = nullptr;
            _size = 0;
        }

    private:
        NodePtr _head;
        TailPtr _tail_ptr;
        size_t _size;

        NodePtr& _tail() noexcept {
            if (!_tail_ptr) {
                _tail_ptr = make_tracked<tracked_ptr<Node>>();
            }
            return *_tail_ptr;
        }

        void _push_back(const NodePtr& node) {
            auto& tail = _tail();
            node->prev = tail;
            if (tail) {
                tail->next = node;
            } else {
                _head = node;
            }
            tail = node;
            ++_size;
        }

        void _push_front(const NodePtr& node) {
            node->next = _head;
            if (_head) {
                _head->prev = node;
            } else {
                _tail() = node;
            }
            _head = node;
            ++_size;
        }

        template <typename Compare>
        void _merge(list& other, Compare comp) noexcept {
            if (this == &other) {
                return;
            }
            if (other.empty()) {
                return;
            }
            if (empty()) {
                *this = std::move(other);
            }
            _size += other._size;
            NodePtr p = _head;
            NodePtr q = other._head;
            NodePtr next;
            while(p && q) {
                if (comp(q->data, p->data)) {
                    next = q->next;
                    q->prev = p->prev;
                    q->next = p;
                    if (p->prev) {
                        p->prev->next = q;
                    } else {
                        _head = q;
                    }
                    p->prev = q;
                    q = next;
                } else {
                    p = p->next;
                }
            }
            if (q) {
                auto& tail = _tail();
                tail->next = q;
                q->prev = tail;
                tail = other._tail();
            }
            other.clear();
        }

        template<class Compare>
        void _sort(Compare comp) noexcept {
            if (!_head || !(_head->next)) {
                return;
            }
            _head = _merge_sort(_head, comp);
            NodePtr node = _head;
            node->prev = nullptr;
            while(node->next) {
                node->next->prev = node;
                node = node->next;
            }
            _tail() = node;
        }
        
        template<class Compare>
        NodePtr _merge(NodePtr left, NodePtr right, Compare comp) noexcept {
            if (!left) {
                return right;
            }
            if (!right) {
                return left;
            }
            NodePtr result;
            if (comp(left->data, right->data)) {
                result = left;
                left = left->next;
            } else {
                result = right;
                right = right->next;
            }
            result->prev = nullptr;
            NodePtr current = result;
            while (left && right) {
                if (comp(left->data, right->data)) {
                    current->next = left;
                    left->prev = current;
                    current = left;
                    left = left->next;
                } else {
                    current->next = right;
                    right->prev = current;
                    current = right;
                    right = right->next;
                }
            }
            NodePtr rest = left ? left : right;
            if (rest) rest->prev = current;
            current->next = rest;
            return result;
        }

        template<class Compare>
        NodePtr _merge_sort(NodePtr head, Compare comp) noexcept {
            if (!head || !(head->next)) {
                return head;
            }
            NodePtr slow = head;
            NodePtr fast = head->next;
            while (fast && fast->next) {
                slow = slow->next;
                fast = fast->next->next;
            }
            NodePtr mid = slow->next;
            slow->next = nullptr;
            NodePtr left = _merge_sort(head, comp);
            NodePtr right = _merge_sort(mid, comp);
            return _merge(left, right, comp);
        }

        void _splice(NodePtr pos, NodePtr first, NodePtr last) noexcept {
            if (pos == _head) {
                if (_head) {
                    first->prev = nullptr;
                    last->next = _head;
                    _head->prev = last;
                } else {
                    first->prev = nullptr;
                    last->next = nullptr;
                    _tail() = last;
                }
                _head = first;
            } else if (pos == nullptr) {
                auto& tail = _tail();
                if (tail) {
                    tail->next = first;
                    first->prev = tail;
                } else {
                    first->prev = nullptr;
                    _head = first;
                }
                tail = last;
                last->next = nullptr;
            } else {
                NodePtr& before = pos->prev;
                before->next = first;
                first->prev = before;
                last->next = pos;
                pos->prev = last;
            }
        }
    };

    template <class T>
    bool operator==(const list<T>& lhs, const list<T>& rhs) noexcept {
        if (lhs.size() != rhs.size())
            return false;
        auto it1 = lhs.begin();
        auto it2 = rhs.begin();
        auto lend = lhs.end();
        auto rend = rhs.end();
        for (; it1 != lend; ++it1, ++it2) {
            if (!(*it1 == *it2)) {
                return false;
            }
        }
        return true;
    }

    template <class T>
    std::compare_three_way_result_t<T> operator<=>(const list<T>& lhs, const list<T>& rhs) noexcept {
        auto it1 = lhs.begin();
        auto it2 = rhs.begin();
        auto lend = lhs.end();
        auto rend = rhs.end();
        for (; it1 != lend; ++it1, ++it2) {
            if (auto cmp = (*it1 <=> *it2); cmp != 0)
                return cmp;
        }
        if (lhs.size() < rhs.size()) {
            return std::strong_ordering::less;
        }
        if (lhs.size() > rhs.size()) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    template<typename T>
    class list<unique_ptr<T>> : public std::list<unique_ptr<T>> {
    public:
        using std::list<unique_ptr<T>>::list;
    };
}
