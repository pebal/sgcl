//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/anchor.h"
#include "detail/slot.h"
#include "detail/synth_three_way.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <list>
#include <utility>

namespace sgcl {
    // A std::list over managed nodes: a circular doubly linked list around a
    // managed sentinel. The links are tracked_ptrs, so a rooted sentinel
    // keeps every node alive and the list may walk them through raw
    // pointers. Each element lives in a detail::Slot of its node and is
    // constructed at insertion and destroyed at removal, as in std::list;
    // the node itself is reclaimed later by the collector. The sentinel is
    // created on first use, so a default-constructed list allocates nothing.
    // The list holds a tracked_ptr: it may live on a stack or inside a
    // managed object only. An iterator is a raw node pointer, trivially
    // copyable and at home in any container: the list roots every linked
    // node, and an iterator to an erased element is invalid, as in std.
    template<class T>
    class list {
        struct NodeBase {
            tracked_ptr<NodeBase> prev;
            tracked_ptr<NodeBase> next;
        };

        struct Node : NodeBase {
            detail::Slot<T> slot;
        };

        using Link = tracked_ptr<NodeBase>;

        static detail::Slot<T>& _slot(NodeBase* node) noexcept {
            return static_cast<Node*>(node)->slot;
        }

        static T& _value(NodeBase* node) noexcept {
            return static_cast<Node*>(node)->slot.value;
        }

        // A raw pointer to the node; stepping and dereferencing are plain
        // loads, with no write barrier.
        template<class U>
        class Iterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = std::remove_const_t<U>;
            using difference_type = ptrdiff_t;
            using pointer = U*;
            using reference = U&;

            Iterator() noexcept = default;

            reference operator*() const noexcept {
                return _value(_node);
            }

            pointer operator->() const noexcept {
                return &_value(_node);
            }

            Iterator& operator++() noexcept {
                _node = _node->next.get();
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator tmp = *this;
                _node = _node->next.get();
                return tmp;
            }

            Iterator& operator--() noexcept {
                _node = _node->prev.get();
                return *this;
            }

            Iterator operator--(int) noexcept {
                Iterator tmp = *this;
                _node = _node->prev.get();
                return tmp;
            }

            operator Iterator<const value_type>() const noexcept requires (!std::is_const_v<U>) {
                return Iterator<const value_type>(_node);
            }

        private:
            NodeBase* _node = nullptr;

            explicit Iterator(NodeBase* node) noexcept
            : _node(node) {
            }

            friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._node == rhs._node;
            }

            template<class> friend class Iterator;
            friend class list;
        };

    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        list() noexcept
        : _size(0) {
        }

        explicit list(size_type count)
        : list() {
            resize(count);
        }

        list(size_type count, const T& value)
        : list() {
            insert(end(), count, value);
        }

        template<std::input_iterator InputIt>
        list(InputIt first, InputIt last)
        : list() {
            insert(end(), std::move(first), std::move(last));
        }

        list(std::initializer_list<T> ilist)
        : list() {
            insert(end(), ilist.begin(), ilist.end());
        }

        list(const list& other)
        : list() {
            insert(end(), other.begin(), other.end());
        }

        list(list&& other) noexcept
        : _sentinel(other._sentinel)
        , _size(other._size) {
            other._sentinel = nullptr;
            other._size = 0;
        }

        // The elements die here; the nodes and the sentinel are garbage for
        // the collector. When the list itself dies in a sweep, inside a
        // managed object, its nodes are garbage of the same sweep, possibly
        // destroyed already: they are left alone and destroy their own
        // elements (detail/thread.h: sweeping).
        ~list() {
            if (!detail::sweeping) {
                _destroy_all();
            }
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
                _sentinel = other._sentinel;
                _size = other._size;
                other._sentinel = nullptr;
                other._size = 0;
            }
            return *this;
        }

        list& operator=(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
            return *this;
        }

        void assign(size_type count, const T& value) {
            NodeBase* end = _end();
            NodeBase* node = end->next.get();
            for (; count && node != end; --count, node = node->next.get()) {
                _value(node) = value;
            }
            if (node == end) {
                _insert_n(end, count, value);
            } else {
                _erase(node, end);
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            NodeBase* end = _end();
            NodeBase* node = end->next.get();
            for (; first != last && node != end; ++first, node = node->next.get()) {
                _value(node) = *first;
            }
            if (node == end) {
                _insert_range(end, std::move(first), std::move(last));
            } else {
                _erase(node, end);
            }
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        reference front() {
            assert(!empty());
            return _value(_sentinel->next.get());
        }

        const_reference front() const {
            assert(!empty());
            return _value(_sentinel->next.get());
        }

        reference back() {
            assert(!empty());
            return _value(_sentinel->prev.get());
        }

        const_reference back() const {
            assert(!empty());
            return _value(_sentinel->prev.get());
        }

        iterator begin() noexcept {
            return iterator(_first());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_first());
        }

        iterator end() noexcept {
            return iterator(_sentinel.get());
        }

        const_iterator end() const noexcept {
            return const_iterator(_sentinel.get());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        void clear() noexcept {
            NodeBase* end = _sentinel.get();
            if (end) {
                _destroy_all();
                end->next = _sentinel;
                end->prev = _sentinel;
            }
        }

        template<class... A>
        iterator emplace(const_iterator pos, A&&... a) {
            NodeBase* p = _pos(pos);
            Link node;
            _make_node(node, std::forward<A>(a)...);
            _link(p, node);
            return iterator(node.get());
        }

        iterator insert(const_iterator pos, const T& value) {
            return emplace(pos, value);
        }

        iterator insert(const_iterator pos, T&& value) {
            return emplace(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_type count, const T& value) {
            if (!count) {
                return iterator(pos._node);
            }
            return iterator(_insert_n(_pos(pos), count, value));
        }

        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            if (first == last) {
                return iterator(pos._node);
            }
            return iterator(_insert_range(_pos(pos), std::move(first), std::move(last)));
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        iterator erase(const_iterator pos) {
            NodeBase* node = pos._node;
            if (!node || node == _sentinel.get()) {
                return iterator(node);
            }
            NodeBase* next = node->next.get();
            _erase_one(node);
            return iterator(next);
        }

        iterator erase(const_iterator first, const_iterator last) {
            return iterator(_erase(first._node, last._node));
        }

        void push_back(const T& value) {
            emplace_back(value);
        }

        void push_back(T&& value) {
            emplace_back(std::move(value));
        }

        template<class... A>
        reference emplace_back(A&&... a) {
            NodeBase* end = _end();
            Link node;
            _make_node(node, std::forward<A>(a)...);
            _link(end, node);
            return _value(node.get());
        }

        void push_front(const T& value) {
            emplace_front(value);
        }

        void push_front(T&& value) {
            emplace_front(std::move(value));
        }

        template<class... A>
        reference emplace_front(A&&... a) {
            NodeBase* end = _end();
            Link node;
            _make_node(node, std::forward<A>(a)...);
            _link(end->next.get(), node);
            return _value(node.get());
        }

        void pop_back() {
            assert(!empty());
            _erase_one(_sentinel->prev.get());
        }

        void pop_front() {
            assert(!empty());
            _erase_one(_sentinel->next.get());
        }

        void resize(size_type count) {
            if (count > _size) {
                NodeBase* end = _end();
                Chain chain;
                for (size_type n = count - _size; n; --n) {
                    chain.emplace_back();
                }
                _link_chain(end, chain);
            } else if (count < _size) {
                _erase(_nth(count), _sentinel.get());
            }
        }

        void resize(size_type count, const T& value) {
            if (count > _size) {
                _insert_n(_end(), count - _size, value);
            } else if (count < _size) {
                _erase(_nth(count), _sentinel.get());
            }
        }

        void swap(list& other) noexcept {
            _sentinel.swap(other._sentinel);
            std::swap(_size, other._size);
        }

        void merge(list& other) {
            merge(other, std::less<>());
        }

        void merge(list&& other) {
            merge(other, std::less<>());
        }

        template<class Compare>
        void merge(list&& other, Compare comp) {
            merge(other, comp);
        }

        // Stable: of equal elements, those of this list come first. Runs of
        // the other list move over in one relink; the sizes follow each
        // move, so a throwing comparator leaves two valid lists.
        template<class Compare>
        void merge(list& other, Compare comp) {
            if (this == &other || other.empty()) {
                return;
            }
            NodeBase* end = _end();
            NodeBase* other_end = other._sentinel.get();
            NodeBase* a = end->next.get();
            NodeBase* b = other_end->next.get();
            while (a != end && b != other_end) {
                if (comp(_value(b), _value(a))) {
                    NodeBase* run_end = b->next.get();
                    size_type n = 1;
                    while (run_end != other_end && comp(_value(run_end), _value(a))) {
                        run_end = run_end->next.get();
                        ++n;
                    }
                    _transfer(a, b, run_end->prev.get());
                    _size += n;
                    other._size -= n;
                    b = run_end;
                } else {
                    a = a->next.get();
                }
            }
            if (b != other_end) {
                _transfer(end, b, other_end->prev.get());
                _size += other._size;
                other._size = 0;
            }
        }

        void splice(const_iterator pos, list& other) {
            if (this == &other || other.empty()) {
                return;
            }
            NodeBase* p = _pos(pos);
            NodeBase* other_end = other._sentinel.get();
            _transfer(p, other_end->next.get(), other_end->prev.get());
            _size += other._size;
            other._size = 0;
        }

        void splice(const_iterator pos, list&& other) {
            splice(pos, other);
        }

        void splice(const_iterator pos, list& other, const_iterator it) {
            NodeBase* node = it._node;
            if (!node || node == other._sentinel.get()) {
                return;
            }
            NodeBase* p = _pos(pos);
            if (p == node || p == node->next.get()) {
                return;
            }
            _transfer(p, node, node);
            if (this != &other) {
                ++_size;
                --other._size;
            }
        }

        void splice(const_iterator pos, list&& other, const_iterator it) {
            splice(pos, other, it);
        }

        void splice(const_iterator pos, list& other, const_iterator first, const_iterator last) {
            NodeBase* f = first._node;
            NodeBase* l = last._node;
            if (f == l) {
                return;
            }
            NodeBase* p = _pos(pos);
            if (this != &other) {
                size_type n = 0;
                for (NodeBase* node = f; node != l; node = node->next.get()) {
                    ++n;
                }
                _size += n;
                other._size -= n;
            }
            _transfer(p, f, l->prev.get());
        }

        void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last) {
            splice(pos, other, first, last);
        }

        // A value that is an element of this list is removed last, after
        // the comparisons that read it.
        size_type remove(const T& value) {
            NodeBase* end = _sentinel.get();
            if (!end) {
                return 0;
            }
            NodeBase* self = nullptr;
            size_type removed = 0;
            NodeBase* node = end->next.get();
            while (node != end) {
                NodeBase* next = node->next.get();
                if (_value(node) == value) {
                    if (&_value(node) == &value) {
                        self = node;
                    } else {
                        _erase_one(node);
                        ++removed;
                    }
                }
                node = next;
            }
            if (self) {
                _erase_one(self);
                ++removed;
            }
            return removed;
        }

        template<class UnaryPredicate>
        size_type remove_if(UnaryPredicate pred) {
            NodeBase* end = _sentinel.get();
            if (!end) {
                return 0;
            }
            size_type removed = 0;
            NodeBase* node = end->next.get();
            while (node != end) {
                NodeBase* next = node->next.get();
                if (pred(_value(node))) {
                    _erase_one(node);
                    ++removed;
                }
                node = next;
            }
            return removed;
        }

        void reverse() noexcept {
            if (_size < 2) {
                return;
            }
            NodeBase* end = _sentinel.get();
            NodeBase* node = end;
            Link next;
            do {
                next = node->next;
                node->next = node->prev;
                node->prev = next;
                node = next.get();
            } while (node != end);
        }

        size_type unique() {
            return unique([](const T& a, const T& b) { return a == b; });
        }

        template<class BinaryPredicate>
        size_type unique(BinaryPredicate pred) {
            NodeBase* end = _sentinel.get();
            if (!end) {
                return 0;
            }
            size_type removed = 0;
            NodeBase* node = end->next.get();
            if (node == end) {
                return 0;
            }
            NodeBase* next = node->next.get();
            while (next != end) {
                if (pred(_value(node), _value(next))) {
                    _erase_one(next);
                    ++removed;
                } else {
                    node = next;
                }
                next = node->next.get();
            }
            return removed;
        }

        void sort() {
            sort(std::less<>());
        }

        // Stable merge sort in place: the nodes are relinked within the
        // list, never detached, so a throwing comparator leaves a valid
        // list of the same elements.
        template<class Compare>
        void sort(Compare comp) {
            if (_size < 2) {
                return;
            }
            NodeBase* end = _sentinel.get();
            _sort(end->next.get(), end, _size, comp);
        }

    private:
        Link _sentinel;
        size_type _size;

        // A run of new nodes not yet in any list, rooted by `first`. Until
        // it is linked in, its destructor destroys the elements: a throwing
        // element constructor leaves the list as it was.
        struct Chain {
            Link first;
            Link last;
            size_type count = 0;

            ~Chain() {
                for (NodeBase* node = first.get(); node; node = node->next.get()) {
                    _slot(node).destroy();
                }
            }

            template<class... A>
            void emplace_back(A&&... a) {
                Link node;
                _make_node(node, std::forward<A>(a)...);
                if (last) {
                    node->prev = last;
                    last->next = node;
                } else {
                    first = node;
                }
                last = node;
                ++count;
            }
        };

        NodeBase* _first() const noexcept {
            NodeBase* end = _sentinel.get();
            return end ? end->next.get() : nullptr;
        }

        NodeBase* _end() {
            if (!_sentinel) {
                _sentinel = make_tracked<NodeBase>();
                NodeBase* end = _sentinel.get();
                end->prev = _sentinel;
                end->next = _sentinel;
            }
            return _sentinel.get();
        }

        // The node an iterator names; end() of a list that had no sentinel
        // yet is null and means the end.
        NodeBase* _pos(const const_iterator& it) {
            NodeBase* end = _end();
            NodeBase* node = it._node;
            return node ? node : end;
        }

        NodeBase* _nth(size_type index) const noexcept {
            NodeBase* node = _sentinel.get();
            if (index < _size - index) {
                for (size_type i = 0; i <= index; ++i) {
                    node = node->next.get();
                }
            } else {
                for (size_type i = _size; i > index; --i) {
                    node = node->prev.get();
                }
            }
            return node;
        }

        template<class... A>
        static void _make_node(Link& node, A&&... a) {
            node = make_tracked<Node>();
            _slot(node.get()).construct(std::forward<A>(a)...);
        }

        void _link(NodeBase* pos, const Link& node) noexcept {
            NodeBase* n = node.get();
            NodeBase* before = pos->prev.get();
            n->prev.reset(before);   // the neighbours are linked: one store each, no reload
            n->next.reset(pos);
            before->next = node;
            pos->prev = node;
            ++_size;
        }

        // A removed node loses its links as well: the stack is scanned
        // conservatively, and a stale word pointing at one dead node must
        // not keep a chain of them alive through it. A null store has no
        // write barrier.
        void _unlink(NodeBase* node) noexcept {
            NodeBase* prev = node->prev.get();
            NodeBase* next = node->next.get();
            prev->next = node->next;
            next->prev = node->prev;
            node->prev = nullptr;
            node->next = nullptr;
            --_size;
        }

        // Destroys the element of one linked node and unlinks it. The node is
        // held by a tracked pointer on this stack meanwhile: the caller's raw
        // pointer may live in a register only, which the collector does not
        // see, and once unlinked the node has no other reference while its
        // links are cleared.
        void _erase_one(NodeBase* node) noexcept {
            detail::Anchor keep(node);
            _slot(node).destroy();
            _unlink(node);
        }

        NodeBase* _link_chain(NodeBase* pos, Chain& chain) noexcept {
            if (!chain.count) {
                return pos;
            }
            NodeBase* first = chain.first.get();
            NodeBase* last = chain.last.get();
            NodeBase* before = pos->prev.get();
            first->prev = pos->prev;
            last->next = before->next;
            before->next = chain.first;
            pos->prev = chain.last;
            _size += chain.count;
            chain.first = nullptr;
            return first;
        }

        NodeBase* _insert_n(NodeBase* pos, size_type count, const T& value) {
            Chain chain;
            for (; count; --count) {
                chain.emplace_back(value);
            }
            return _link_chain(pos, chain);
        }

        template<class InputIt>
        NodeBase* _insert_range(NodeBase* pos, InputIt first, InputIt last) {
            Chain chain;
            for (; first != last; ++first) {
                chain.emplace_back(*first);
            }
            return _link_chain(pos, chain);
        }

        NodeBase* _erase(NodeBase* first, NodeBase* last) {
            if (first == last) {
                return last;
            }
            detail::Anchor keep(first);   // rooted before the range leaves the list
            NodeBase* before = first->prev.get();
            before->next.reset(last);
            last->prev.reset(before);
            _release(first, last);
            return last;
        }

        // Destroys the elements of the detached nodes first..(last) and
        // drops their links (see _unlink). Each node is held by a tracked
        // pointer on this stack while it is touched: once its links are
        // gone, nothing else keeps it from a concurrent sweep.
        void _release(NodeBase* first, NodeBase* last) noexcept {
            detail::Anchor keep(first);
            auto node = first;
            while (node != last) {
                auto next = node->next.get();
                keep.reset(next);   // the next one rooted before this one lets go of it
                _slot(node).destroy();
                node->prev = nullptr;
                node->next = nullptr;
                node = next;
                --_size;
            }
        }

        void _destroy_all() noexcept {
            NodeBase* end = _sentinel.get();
            if (end) {
                _release(end->next.get(), end);
            }
        }

        // Moves the nodes first..last (inclusive) in front of pos, within
        // one list or between two; the sizes are the caller's. Every link
        // is copied from a link that already holds the target, so the only
        // temporaries are the two that break the cycle of six stores.
        static void _transfer(NodeBase* pos, NodeBase* first, NodeBase* last) noexcept {
            NodeBase* after = last->next.get();
            if (pos == first || pos == after) {
                return;
            }
            NodeBase* source = first->prev.get();
            NodeBase* before = pos->prev.get();
            Link f = source->next;
            Link l = after->prev;
            source->next = last->next;
            after->prev = first->prev;
            first->prev = pos->prev;
            last->next = before->next;
            before->next = f;
            pos->prev = l;
        }

        // Sorts [first, last) of n nodes; returns its new first node. The
        // node `last` never moves, so the ranges stay well defined.
        template<class Compare>
        static NodeBase* _sort(NodeBase* first, NodeBase* last, size_type n, Compare& comp) {
            if (n < 2) {
                return first;
            }
            size_type half = n / 2;
            NodeBase* mid = first;
            for (size_type i = 0; i < half; ++i) {
                mid = mid->next.get();
            }
            first = _sort(first, mid, half, comp);
            mid = _sort(mid, last, n - half, comp);
            return _merge(first, mid, last, comp);
        }

        // Merges the adjacent sorted ranges [a, b) and [b, last) in place;
        // returns the first node of the result.
        template<class Compare>
        static NodeBase* _merge(NodeBase* a, NodeBase* b, NodeBase* last, Compare& comp) {
            NodeBase* first = a;
            while (a != b && b != last) {
                if (comp(_value(b), _value(a))) {
                    NodeBase* run_end = b->next.get();
                    while (run_end != last && comp(_value(run_end), _value(a))) {
                        run_end = run_end->next.get();
                    }
                    _transfer(a, b, run_end->prev.get());
                    if (a == first) {
                        first = b;
                    }
                    b = run_end;
                } else {
                    a = a->next.get();
                }
            }
            return first;
        }
    };

    template<class T>
    bool operator==(const list<T>& lhs, const list<T>& rhs) {
        return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    template<class T>
    detail::synth_three_way_result<const T> operator<=>(const list<T>& lhs, const list<T>& rhs) {
        return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), detail::synth_three_way);
    }

    template<class T>
    void swap(list<T>& lhs, list<T>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template<typename T>
    class list<unique_ptr<T>> : public std::list<unique_ptr<T>> {
    public:
        using std::list<unique_ptr<T>>::list;
    };
}

namespace std {
    template<class T, class U>
    typename sgcl::list<T>::size_type erase(sgcl::list<T>& c, const U& value) {
        return c.remove_if([&](const auto& element) { return element == value; });
    }

    template<class T, class Pred>
    typename sgcl::list<T>::size_type erase_if(sgcl::list<T>& c, Pred pred) {
        return c.remove_if(pred);
    }
}
