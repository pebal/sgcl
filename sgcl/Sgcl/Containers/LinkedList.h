//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LinkedList<T> (doubly linked, a managed node per element) and
// ForwardList<T> (singly linked,
// one word per node less, no count).
#pragma once

#include "../../containers/forward_list.h"
#include "../../containers/list.h"
#include "MSequence.h"

#include <algorithm>
#include <ranges>

namespace Sgcl {
    template<class T>
    class LinkedList : public MSequence<LinkedList<T>> {   // the algorithms as members
    public:
        using ValueType = T;
        using InnerType = sgcl::list<T>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;             // bidirectional, a raw node pointer
        using ConstIterator = typename InnerType::const_iterator;

        LinkedList() noexcept = default;

        explicit LinkedList(SizeType count)
        : _l(count) {
        }

        LinkedList(SizeType count, const T& value)
        : _l(count, value) {
        }

        template<std::input_iterator It>
        LinkedList(It first, It last)
        : _l(first, last) {
        }

        // From a range of what the elements are made of (the Pieces of a
        // String, a view, another container)
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, LinkedList>) && (!std::is_same_v<std::remove_cvref_t<R>, InnerType>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit LinkedList(R&& r)
        : _l(std::ranges::begin(r), std::ranges::end(r)) {
        }

        LinkedList(std::initializer_list<T> il)
        : _l(il) {
        }

        explicit LinkedList(InnerType l) noexcept
        : _l(std::move(l)) {
        }

        LinkedList(const LinkedList&) = default;
        LinkedList(LinkedList&&) noexcept = default;
        LinkedList& operator=(const LinkedList&) = default;
        LinkedList& operator=(LinkedList&&) noexcept = default;

        LinkedList& operator=(std::initializer_list<T> il) {
            _l = il;
            return *this;
        }

        // The contents replaced
        void Assign(SizeType count, const T& value) {
            _l.assign(count, value);
        }

        template<std::input_iterator It>
        void Assign(It first, It last) {
            _l.assign(first, last);
        }

        void Assign(std::initializer_list<T> il) {
            _l.assign(il);
        }

        T& First() noexcept {
            return _l.front();
        }

        const T& First() const noexcept {
            return _l.front();
        }

        T& Last() noexcept {
            return _l.back();
        }

        const T& Last() const noexcept {
            return _l.back();
        }

        SizeType Count() const noexcept {
            return _l.size();
        }

        bool IsEmpty() const noexcept {
            return _l.empty();
        }

        void AddFirst(const T& value) {
            _l.push_front(value);
        }

        void AddFirst(T&& value) {
            _l.push_front(std::move(value));
        }

        void AddLast(const T& value) {
            _l.push_back(value);
        }

        void AddLast(T&& value) {
            _l.push_back(std::move(value));
        }

        template<class... A>
        T& EmplaceFirst(A&&... a) {
            return _l.emplace_front(std::forward<A>(a)...);
        }

        template<class... A>
        T& EmplaceLast(A&&... a) {
            return _l.emplace_back(std::forward<A>(a)...);
        }

        // Before or after the element an iterator names (end(list) for
        // the back): the iterator of the new element
        Iterator AddBefore(ConstIterator pos, const T& value) {
            return _l.insert(pos, value);
        }

        Iterator AddBefore(ConstIterator pos, T&& value) {
            return _l.insert(pos, std::move(value));
        }

        Iterator AddBefore(ConstIterator pos, SizeType count, const T& value) {
            return _l.insert(pos, count, value);
        }

        Iterator AddBefore(ConstIterator pos, std::initializer_list<T> il) {
            return _l.insert(pos, il);
        }

        template<class... A>
        Iterator EmplaceBefore(ConstIterator pos, A&&... a) {
            return _l.emplace(pos, std::forward<A>(a)...);
        }

        Iterator AddAfter(ConstIterator pos, const T& value) {
            return _l.insert(std::next(pos), value);
        }

        Iterator AddAfter(ConstIterator pos, T&& value) {
            return _l.insert(std::next(pos), std::move(value));
        }

        void RemoveFirst() noexcept {
            _l.pop_front();
        }

        void RemoveLast() noexcept {
            _l.pop_back();
        }

        // The element an iterator names removed: the iterator of the next
        Iterator RemoveAt(ConstIterator pos) {
            return _l.erase(pos);
        }

        Iterator RemoveRange(ConstIterator first, ConstIterator last) {
            return _l.erase(first, last);
        }

        // Every element equal to `value` removed: how many
        SizeType Remove(const T& value) {
            return _l.remove(value);
        }

        template<class Pred>
        SizeType RemoveAll(Pred pred) {
            return _l.remove_if(pred);
        }

        void Clear() noexcept {
            _l.clear();
        }

        void Resize(SizeType n) {
            _l.resize(n);
        }

        void Resize(SizeType n, const T& value) {
            _l.resize(n, value);
        }

        // The nodes of `other` moved to the end of this list, in order
        void Append(LinkedList&& other) noexcept {
            _l.splice(_l.end(), std::move(other._l));
        }

        void Prepend(LinkedList&& other) noexcept {
            _l.splice(_l.begin(), std::move(other._l));
        }

        // The nodes of `other`, the one `it` names, or those in [first,
        // last), moved before `pos`: no element copied, every iterator
        // still valid; `other` may be this list
        void Splice(ConstIterator pos, LinkedList& other) noexcept {
            _l.splice(pos, other._l);
        }

        void Splice(ConstIterator pos, LinkedList& other, ConstIterator it) noexcept {
            _l.splice(pos, other._l, it);
        }

        void Splice(ConstIterator pos, LinkedList& other, ConstIterator first, ConstIterator last) noexcept {
            _l.splice(pos, other._l, first, last);
        }

        // Two sorted lists into one, the nodes of `other` moved
        void Merge(LinkedList&& other) {
            _l.merge(std::move(other._l));
        }

        template<class Compare>
        void Merge(LinkedList&& other, Compare cmp) {
            _l.merge(std::move(other._l), cmp);
        }

        void Reverse() noexcept {
            _l.reverse();
        }

        void Sort() {
            _l.sort();
        }

        template<class Compare>
        void Sort(Compare cmp) {
            _l.sort(cmp);
        }

        // Consecutive equal elements reduced to one: how many were removed
        SizeType Unique() {
            return _l.unique();
        }

        template<class Pred>
        SizeType Unique(Pred pred) {
            return _l.unique(pred);
        }

        void Swap(LinkedList& o) noexcept {
            _l.swap(o._l);
        }

        InnerType& Inner() noexcept {
            return _l;
        }

        const InnerType& Inner() const noexcept {
            return _l;
        }

        friend bool operator==(const LinkedList& a, const LinkedList& b) {
            return a._l == b._l;
        }

        friend auto operator<=>(const LinkedList& a, const LinkedList& b) {
            return a._l <=> b._l;
        }

    private:
        InnerType _l;
    };

    template<std::input_iterator It>
    LinkedList(It, It) -> LinkedList<std::iter_value_t<It>>;

    template<class T>
    auto begin(LinkedList<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(LinkedList<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    auto begin(const LinkedList<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(const LinkedList<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    void swap(LinkedList<T>& a, LinkedList<T>& b) noexcept {
        a.Swap(b);
    }

    template<class T>
    class ForwardList : public MSequence<ForwardList<T>> {   // the algorithms as members
    public:
        using ValueType = T;
        using InnerType = sgcl::forward_list<T>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;             // forward, a raw node pointer
        using ConstIterator = typename InnerType::const_iterator;

        ForwardList() noexcept = default;

        explicit ForwardList(SizeType count)
        : _l(count) {
        }

        ForwardList(SizeType count, const T& value)
        : _l(count, value) {
        }

        template<std::input_iterator It>
        ForwardList(It first, It last)
        : _l(first, last) {
        }

        // From a range of what the elements are made of (the Pieces of a
        // String, a view, another container)
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, ForwardList>) && (!std::is_same_v<std::remove_cvref_t<R>, InnerType>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit ForwardList(R&& r)
        : _l(std::ranges::begin(r), std::ranges::end(r)) {
        }

        ForwardList(std::initializer_list<T> il)
        : _l(il) {
        }

        explicit ForwardList(InnerType l) noexcept
        : _l(std::move(l)) {
        }

        ForwardList(const ForwardList&) = default;
        ForwardList(ForwardList&&) noexcept = default;
        ForwardList& operator=(const ForwardList&) = default;
        ForwardList& operator=(ForwardList&&) noexcept = default;

        void Assign(SizeType count, const T& value) {
            _l.assign(count, value);
        }

        template<std::input_iterator It>
        void Assign(It first, It last) {
            _l.assign(first, last);
        }

        void Assign(std::initializer_list<T> il) {
            _l.assign(il);
        }

        // The position before the first element: for AddAfter at the front
        Iterator BeforeBegin() noexcept {
            return _l.before_begin();
        }

        ConstIterator BeforeBegin() const noexcept {
            return _l.before_begin();
        }

        T& First() noexcept {
            return _l.front();
        }

        const T& First() const noexcept {
            return _l.front();
        }

        bool IsEmpty() const noexcept {
            return _l.empty();
        }

        void AddFirst(const T& value) {
            _l.push_front(value);
        }

        void AddFirst(T&& value) {
            _l.push_front(std::move(value));
        }

        template<class... A>
        T& EmplaceFirst(A&&... a) {
            return _l.emplace_front(std::forward<A>(a)...);
        }

        // After the element an iterator names (BeforeBegin() for the
        // front): the iterator of the new element
        Iterator AddAfter(ConstIterator pos, const T& value) {
            return _l.insert_after(pos, value);
        }

        Iterator AddAfter(ConstIterator pos, T&& value) {
            return _l.insert_after(pos, std::move(value));
        }

        Iterator AddAfter(ConstIterator pos, SizeType count, const T& value) {
            return _l.insert_after(pos, count, value);
        }

        template<class... A>
        Iterator EmplaceAfter(ConstIterator pos, A&&... a) {
            return _l.emplace_after(pos, std::forward<A>(a)...);
        }

        void RemoveFirst() noexcept {
            _l.pop_front();
        }

        // The element after the one an iterator names removed: the iterator
        // of the one after that
        Iterator RemoveAfter(ConstIterator pos) {
            return _l.erase_after(pos);
        }

        Iterator RemoveAfter(ConstIterator first, ConstIterator last) {
            return _l.erase_after(first, last);
        }

        SizeType Remove(const T& value) {
            return _l.remove(value);
        }

        template<class Pred>
        SizeType RemoveAll(Pred pred) {
            return _l.remove_if(pred);
        }

        void Clear() noexcept {
            _l.clear();
        }

        void Resize(SizeType n) {
            _l.resize(n);
        }

        void Prepend(ForwardList&& other) noexcept {
            _l.splice_after(_l.before_begin(), std::move(other._l));
        }

        // The nodes of `other`, the one after `it`, or those in (first,
        // last), moved after `pos`
        void SpliceAfter(ConstIterator pos, ForwardList& other) noexcept {
            _l.splice_after(pos, other._l);
        }

        void SpliceAfter(ConstIterator pos, ForwardList& other, ConstIterator it) noexcept {
            _l.splice_after(pos, other._l, it);
        }

        void SpliceAfter(ConstIterator pos, ForwardList& other, ConstIterator first, ConstIterator last) noexcept {
            _l.splice_after(pos, other._l, first, last);
        }

        void Merge(ForwardList&& other) {
            _l.merge(std::move(other._l));
        }

        template<class Compare>
        void Merge(ForwardList&& other, Compare cmp) {
            _l.merge(std::move(other._l), cmp);
        }

        void Reverse() noexcept {
            _l.reverse();
        }

        void Sort() {
            _l.sort();
        }

        template<class Compare>
        void Sort(Compare cmp) {
            _l.sort(cmp);
        }

        SizeType Unique() {
            return _l.unique();
        }

        template<class Pred>
        SizeType Unique(Pred pred) {
            return _l.unique(pred);
        }

        void Swap(ForwardList& o) noexcept {
            _l.swap(o._l);
        }

        InnerType& Inner() noexcept {
            return _l;
        }

        const InnerType& Inner() const noexcept {
            return _l;
        }

        friend bool operator==(const ForwardList& a, const ForwardList& b) {
            return a._l == b._l;
        }

        friend auto operator<=>(const ForwardList& a, const ForwardList& b) {
            return a._l <=> b._l;
        }

    private:
        InnerType _l;
    };

    template<class T>
    auto begin(ForwardList<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(ForwardList<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    auto begin(const ForwardList<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(const ForwardList<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    void swap(ForwardList<T>& a, ForwardList<T>& b) noexcept {
        a.Swap(b);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

