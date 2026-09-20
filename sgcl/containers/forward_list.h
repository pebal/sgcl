//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/slot.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "../core/mixin/mixin.h"
#include "detail/anchor.h"

#include <compare>
#include <forward_list>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>

namespace sgcl {
    // Singly linked nodes on the managed heap behind a sentinel node, the
    // one before_begin() addresses; every list owns a sentinel from its
    // construction on, so insert_after and erase_after work the same at any
    // position. The sentinel is a bare link: it has no element, so nothing
    // is ever constructed or destroyed in it. Elements are constructed and
    // destroyed by the list itself, as std::forward_list does; the nodes
    // are reclaimed by the collector. Relinking operations (splice_after,
    // merge, sort, reverse) move nodes, never elements, and every node stays
    // reachable through a tracked_ptr (a link or a stack local) while it is
    // being moved. An iterator is a raw node pointer, trivially copyable and
    // at home in any container: the list roots every linked node, and an
    // iterator to an erased element is invalid, as in std.
    template<class T>
    class forward_list
    : public m_enumerable<forward_list<T>>
    , public m_equatable<forward_list<T>>
    , public m_comparable<forward_list<T>>
    , public m_ordered<forward_list<T>>
    , public m_sequence<forward_list<T>> {
        struct NodeBase {
            tracked_ptr<NodeBase> next;
        };

        struct Node : NodeBase {
            detail::Slot<T> slot;
        };

        using Link = tracked_ptr<NodeBase>;

        // The sentinel is never passed here: every node behind a link that
        // is not the sentinel's own is a Node with a constructed element.
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
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
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

            operator Iterator<const value_type>() const noexcept {
                return Iterator<const value_type>(_node);
            }

        private:
            explicit Iterator(NodeBase* node) noexcept
            : _node(node) {
            }

            NodeBase* _node = nullptr;

            friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._node == rhs._node;
            }

            template<class> friend class Iterator;
            template<class> friend class forward_list;
        };

    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;

        forward_list()
        : _head(make_tracked<NodeBase>()) {
        }

        explicit forward_list(size_type count) requires std::default_initializable<T>
        : forward_list() {
            try {
                resize(count);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        forward_list(size_type count, const T& value)
        : forward_list() {
            try {
                assign(count, value);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        template<std::input_iterator InputIt>
        forward_list(InputIt first, InputIt last)
        : forward_list() {
            try {
                assign(first, last);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        // From a range of what the elements are made of (the pieces of a
        // string, a view, another container), as C++23's from_range: a
        // container is copied by its own constructor, not this one
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, forward_list>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit forward_list(R&& r)
        : forward_list(std::ranges::begin(r), std::ranges::end(r)) {
        }

        forward_list(std::initializer_list<T> ilist)
        : forward_list(ilist.begin(), ilist.end()) {
        }

        forward_list(const forward_list& other)
        : forward_list(other.begin(), other.end()) {
        }

        // Takes the chain of `other`, which keeps its sentinel and is empty.
        forward_list(forward_list&& other)
        : forward_list() {
            _head->next = other._head->next;
            other._head->next = nullptr;
        }

        // The elements are destroyed here, unless the list dies in a sweep:
        // its nodes are garbage of the same sweep, possibly destroyed
        // already, and destroy the elements they still hold themselves.
        ~forward_list() {
            if (_head && !detail::sweeping) {
                _release(_head->next.get(), nullptr);
            }
        }

        forward_list& operator=(const forward_list& other) {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        forward_list& operator=(forward_list&& other) noexcept {
            if (this != &other) {
                clear();
                _head->next = other._head->next;
                other._head->next = nullptr;
            }
            return *this;
        }

        forward_list& operator=(std::initializer_list<T> ilist) {
            assign(ilist);
            return *this;
        }

        void assign(size_type count, const T& value) {
            NodeBase* prev = _head.get();
            for (; count && prev->next; --count) {
                prev = prev->next.get();
                _value(prev) = value;
            }
            if (count) {
                _emplace_chain_after(prev, [&] { return count-- > 0; }, [&](detail::Slot<T>& slot) { slot.construct(value); });
            } else {
                _erase_after(prev, nullptr);
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            NodeBase* prev = _head.get();
            for (; first != last && prev->next; ++first) {
                prev = prev->next.get();
                _value(prev) = *first;
            }
            if (first != last) {
                _emplace_chain_after(prev, [&] { return first != last; }, [&](detail::Slot<T>& slot) { slot.construct(*first); ++first; });
            } else {
                _erase_after(prev, nullptr);
            }
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        reference front() {
            return _value(_head->next.get());
        }

        const_reference front() const {
            return _value(_head->next.get());
        }

        iterator before_begin() noexcept {
            return iterator(_head.get());
        }

        const_iterator before_begin() const noexcept {
            return const_iterator(_head.get());
        }

        const_iterator cbefore_begin() const noexcept {
            return before_begin();
        }

        iterator begin() noexcept {
            return iterator(_head->next.get());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_head->next.get());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator();
        }

        const_iterator end() const noexcept {
            return const_iterator();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        bool empty() const noexcept {
            return !_head->next;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        void clear() noexcept {
            _erase_after(_head.get(), nullptr);
        }

        iterator insert_after(const_iterator pos, const T& value) {
            return emplace_after(pos, value);
        }

        iterator insert_after(const_iterator pos, T&& value) {
            return emplace_after(pos, std::move(value));
        }

        iterator insert_after(const_iterator pos, size_type count, const T& value) {
            return _emplace_chain_after(pos._node, [&] { return count-- > 0; }, [&](detail::Slot<T>& slot) { slot.construct(value); });
        }

        template<std::input_iterator InputIt>
        iterator insert_after(const_iterator pos, InputIt first, InputIt last) {
            return _emplace_chain_after(pos._node, [&] { return first != last; }, [&](detail::Slot<T>& slot) { slot.construct(*first); ++first; });
        }

        iterator insert_after(const_iterator pos, std::initializer_list<T> ilist) {
            return insert_after(pos, ilist.begin(), ilist.end());
        }

        template<class... A>
        iterator emplace_after(const_iterator pos, A&&... a) {
            Link node;
            _make_node(node, std::forward<A>(a)...);
            node->next = pos._node->next;
            pos._node->next = node;
            return iterator(node.get());
        }

        iterator erase_after(const_iterator pos) {
            NodeBase* prev = pos._node;
            if (!prev->next) {
                return end();
            }
            _erase_one_after(prev);
            return iterator(prev->next.get());
        }

        iterator erase_after(const_iterator first, const_iterator last) {
            _erase_after(first._node, last._node);
            return iterator(last._node);
        }

        void push_front(const T& value) {
            emplace_front(value);
        }

        void push_front(T&& value) {
            emplace_front(std::move(value));
        }

        template<class... A>
        reference emplace_front(A&&... a) {
            return *emplace_after(before_begin(), std::forward<A>(a)...);
        }

        void pop_front() {
            _erase_one_after(_head.get());
        }

        void resize(size_type count) requires std::default_initializable<T> {
            NodeBase* prev = _head.get();
            for (; count && prev->next; --count) {
                prev = prev->next.get();
            }
            if (count) {
                _emplace_chain_after(prev, [&] { return count-- > 0; }, [](detail::Slot<T>& slot) { slot.construct(); });
            } else {
                _erase_after(prev, nullptr);
            }
        }

        void resize(size_type count, const value_type& value) {
            NodeBase* prev = _head.get();
            for (; count && prev->next; --count) {
                prev = prev->next.get();
            }
            if (count) {
                _emplace_chain_after(prev, [&] { return count-- > 0; }, [&](detail::Slot<T>& slot) { slot.construct(value); });
            } else {
                _erase_after(prev, nullptr);
            }
        }

        void swap(forward_list& other) noexcept {
            _head.swap(other._head);
        }

        void merge(forward_list& other) {
            std::less<T> comp;
            _merge(other, comp);
        }

        void merge(forward_list&& other) {
            std::less<T> comp;
            _merge(other, comp);
        }

        template<class Compare>
        void merge(forward_list& other, Compare comp) {
            _merge(other, comp);
        }

        template<class Compare>
        void merge(forward_list&& other, Compare comp) {
            _merge(other, comp);
        }

        void splice_after(const_iterator pos, forward_list& other) {
            if (&other == this || other.empty()) {
                return;
            }
            NodeBase* p = pos._node;
            Link first = other._head->next;
            NodeBase* last = first.get();
            while (last->next) {
                last = last->next.get();
            }
            last->next = p->next;
            p->next = first;
            other._head->next = nullptr;
        }

        void splice_after(const_iterator pos, forward_list&& other) {
            splice_after(pos, other);
        }

        // Moves the element after `it`.
        void splice_after(const_iterator pos, forward_list&, const_iterator it) {
            NodeBase* p = pos._node;
            NodeBase* i = it._node;
            NodeBase* n = i->next.get();
            if (!n || p == i || p == n) {
                return;
            }
            Link node = i->next;
            i->next = n->next;
            n->next = p->next;
            p->next = node;
        }

        void splice_after(const_iterator pos, forward_list&& other, const_iterator it) {
            splice_after(pos, other, it);
        }

        // Moves the elements in (first, last).
        void splice_after(const_iterator pos, forward_list&, const_iterator first, const_iterator last) {
            NodeBase* p = pos._node;
            NodeBase* f = first._node;
            NodeBase* l = last._node;
            if (f == l || f->next.get() == l) {
                return;
            }
            NodeBase* before_last = f->next.get();
            while (before_last->next.get() != l) {
                before_last = before_last->next.get();
            }
            Link chain = f->next;   // the one temporary that breaks the cycle of three stores
            f->next = before_last->next;
            before_last->next = p->next;
            p->next = chain;
        }

        void splice_after(const_iterator pos, forward_list&& other, const_iterator first, const_iterator last) {
            splice_after(pos, other, first, last);
        }

        size_type remove(const T& value) {
            return remove_if([&value](const T& v) { return v == value; });
        }

        template<class UnaryPredicate>
        size_type remove_if(UnaryPredicate pred) {
            size_type removed = 0;
            NodeBase* prev = _head.get();
            while (NodeBase* node = prev->next.get()) {
                if (pred(_value(node))) {
                    _erase_one_after(prev);
                    ++removed;
                } else {
                    prev = node;
                }
            }
            return removed;
        }

        size_type unique() {
            return unique(std::equal_to<T>());
        }

        template<class BinaryPredicate>
        size_type unique(BinaryPredicate pred) {
            size_type removed = 0;
            NodeBase* prev = _head->next.get();
            if (!prev) {
                return removed;
            }
            while (NodeBase* node = prev->next.get()) {
                if (pred(_value(prev), _value(node))) {
                    _erase_one_after(prev);
                    ++removed;
                } else {
                    prev = node;
                }
            }
            return removed;
        }

        // The sentinel's link always addresses the nodes not yet reversed,
        // `done` the reversed ones.
        void reverse() noexcept {
            NodeBase* head = _head.get();
            Link done;
            while (head->next) {
                Link node = head->next;
                head->next = node->next;
                node->next = done;
                done = node;
            }
            head->next = done;
        }

        void sort() {
            std::less<T> comp;
            _sort(comp);
        }

        template<class Compare>
        void sort(Compare comp) {
            _sort(comp);
        }

    private:
        tracked_ptr<NodeBase> _head;   // the sentinel, a bare NodeBase without an element: the root

        // A node is made and its element constructed in one step, so no
        // node ever holds an unconstructed element: when the constructor
        // throws, the slot marks the node Destroyed (the sweep then frees
        // it without a destructor) and the holder just drops it.
        template<class... A>
        static void _make_node(Link& node, A&&... a) {
            node = make_tracked<Node>();
            _slot(node.get()).construct(std::forward<A>(a)...);
        }

        // Destroys the elements of the detached nodes first..(last) and
        // drops their links (a stale word pointing at one dead node must
        // not keep a chain of them alive through it; a null store has no
        // write barrier). Two roots of this frame alternate along the walk:
        // a node is rooted from before its predecessor lets go of it,
        // through its element's destructor (user code, with the node
        // reachable from nowhere else), until its slot's Destroyed, the
        // last store to it (_erase_one_after). One root would not do:
        // moved on to the next node before this one's destructor runs, it
        // leaves the node to a raw local, possibly a register, which is no
        // root, and a sweep finding the node unmarked and not yet Destroyed
        // would run its destructor a second time. The cost is one
        // set_state<Reachable> per node.
        static void _release(NodeBase* first, NodeBase* last) noexcept {
            detail::Anchor a(first);
            detail::Anchor b(nullptr);
            detail::Anchor* held = &a;    // the root of the node being released
            detail::Anchor* ahead = &b;   // the root of the one after it
            NodeBase* node = first;
            while (node != last) {
                NodeBase* next = node->next.get();
                ahead->reset(next);   // the next one rooted before this one lets go of it
                node->next = nullptr;
                _slot(node).destroy();
                std::swap(held, ahead);   // the next one's root stays; the other is free for the one after
                node = next;
            }
        }

        // Unlinks the node after `prev` and destroys its element. The node
        // is held by a root of this frame meanwhile: the caller's raw
        // pointer may live in a register only, which the collector does not
        // see, and once unlinked the node has no other reference. Destroyed
        // is the last store to the node: the root is the frame's word and
        // the state Reachable together, and the slot's state overwrites the
        // latter, so a thread whose stack was scanned before the anchor was
        // set has no root of the node past that store, and a sweep may free
        // the slot.
        static void _erase_one_after(NodeBase* prev) noexcept {
            NodeBase* node = prev->next.get();
            detail::Anchor keep(node);
            prev->next = node->next;
            node->next = nullptr;
            _slot(node).destroy();
        }

        // Destroys the elements in (prev, last) and links prev to last; an
        // empty range, (prev, prev) included, is a no-op. The first node is
        // rooted before the range leaves the list; `last` is still linked
        // from the range when prev takes it.
        void _erase_after(NodeBase* prev, NodeBase* last) noexcept {
            if (prev == last) {
                return;
            }
            NodeBase* first = prev->next.get();
            detail::Anchor keep(first);
            prev->next.reset(last);
            _release(first, last);
        }

        // Builds a chain of nodes, one per element `construct` places into
        // the slot given while `more()` holds, and links it after `prev`.
        // Returns the last node inserted, or `prev` when there is none. The
        // chain is linked in only once complete: an exception destroys the
        // elements built so far and leaves the list as it was; the node
        // whose element failed to construct is Destroyed by its slot and
        // dropped with the local that holds it.
        template<class More, class Construct>
        iterator _emplace_chain_after(NodeBase* prev, More&& more, Construct&& construct) {
            Link first;
            NodeBase* tail = nullptr;
            try {
                while (more()) {
                    Link node = make_tracked<Node>();
                    construct(_slot(node.get()));
                    if (tail) {
                        tail->next = node;
                    } else {
                        first = node;
                    }
                    tail = node.get();
                }
            }
            catch (...) {
                _release(first.get(), nullptr);
                throw;
            }
            if (!tail) {
                return iterator(prev);
            }
            tail->next = prev->next;
            prev->next = first;
            return iterator(tail);
        }

        // Moves the nodes (before, last] to after pos, within one list or
        // between two; pos is not one of them. Every link is copied from a
        // link that already holds the target, but for the one temporary that
        // breaks the cycle of three stores.
        static void _transfer_after(NodeBase* pos, NodeBase* before, NodeBase* last) noexcept {
            Link rest = last->next;
            last->next = pos->next;
            pos->next = before->next;
            before->next = rest;
        }

        // Stable merge of the sorted `other` into this sorted list: of equal
        // elements, those of this list come first. Both lists are walked
        // through raw pointers, every node staying linked in one of them;
        // runs of the other list move over in one relink each, between
        // comparisons, so a throwing comparator leaves two valid lists.
        template<class Compare>
        void _merge(forward_list& other, Compare& comp) {
            if (this == &other || other.empty()) {
                return;
            }
            NodeBase* tail = _head.get();   // the last node merged so far
            NodeBase* other_head = other._head.get();
            while (tail->next && other_head->next) {
                NodeBase* a = tail->next.get();
                NodeBase* b = other_head->next.get();
                if (comp(_value(b), _value(a))) {
                    NodeBase* run_end = b;
                    while (run_end->next && comp(_value(run_end->next.get()), _value(a))) {
                        run_end = run_end->next.get();
                    }
                    _transfer_after(tail, other_head, run_end);
                    tail = run_end;
                } else {
                    tail = a;
                }
            }
            if (other_head->next) {
                tail->next = other_head->next;
                other_head->next = nullptr;
            }
        }

        // Bottom-up merge sort of the nodes, stable, as std::forward_list::sort
        template<class Compare>
        void _sort(Compare& comp) {
            size_type n = 0;
            for (NodeBase* node = _head->next.get(); node; node = node->next.get()) {
                ++n;
            }
            if (n > 1) {
                _sort_after(_head.get(), n, comp);
            }
        }

        // Stable merge sort in place of the n nodes after prev (n > 0);
        // returns the last of them. The nodes are relinked within the list,
        // never detached, so a throwing comparator leaves a valid list of
        // the same elements.
        template<class Compare>
        static NodeBase* _sort_after(NodeBase* prev, size_type n, Compare& comp) {
            if (n < 2) {
                return prev->next.get();
            }
            size_type half = n / 2;
            NodeBase* a_last = _sort_after(prev, half, comp);
            NodeBase* b_last = _sort_after(a_last, n - half, comp);
            return _merge_after(prev, a_last, b_last, comp);
        }

        // Merges the adjacent sorted ranges (prev, a_last] and (a_last,
        // b_last] in place; returns the last node of the result. Runs of the
        // second range move in front of the node of the first they go
        // before, one relink each; the node after both ranges never moves.
        template<class Compare>
        static NodeBase* _merge_after(NodeBase* prev, NodeBase* a_last, NodeBase* b_last, Compare& comp) {
            NodeBase* end = b_last->next.get();
            NodeBase* tail = prev;
            while (tail != a_last && a_last->next.get() != end) {
                NodeBase* a = tail->next.get();
                NodeBase* b = a_last->next.get();
                if (comp(_value(b), _value(a))) {
                    NodeBase* run_end = b;
                    while (run_end->next.get() != end && comp(_value(run_end->next.get()), _value(a))) {
                        run_end = run_end->next.get();
                    }
                    _transfer_after(tail, a_last, run_end);
                    tail = run_end;
                } else {
                    tail = a;
                }
            }
            return a_last->next.get() == end ? a_last : b_last;
        }

    };

    template<class T>
    class forward_list<unique_ptr<T>> : public std::forward_list<unique_ptr<T>> {
    public:
        using std::forward_list<unique_ptr<T>>::forward_list;
    };

    template<class T>
    inline void swap(forward_list<T>& lhs, forward_list<T>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template<class T, class Pred>
    inline typename forward_list<T>::size_type erase_if(forward_list<T>& c, Pred pred) {
        return c.remove_if(pred);
    }

    template<class T, class U>
    inline typename forward_list<T>::size_type erase(forward_list<T>& c, const U& value) {
        return c.remove_if([&value](const T& v) { return v == value; });
    }
}

namespace std {
    using sgcl::erase;
    using sgcl::erase_if;
}
