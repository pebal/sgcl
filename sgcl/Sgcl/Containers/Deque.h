//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Deque<T>: both ends in constant time, positions
// in constant time, the blocks on the managed heap.
#pragma once

#include "../../containers/deque.h"
#include "List.h"
#include "MSequence.h"

#include <algorithm>
#include <ranges>

namespace Sgcl {
    template<class T>
    class Deque : public MSequence<Deque<T>> {   // the algorithms as members
    public:
        using ValueType = T;
        using InnerType = sgcl::deque<T>;
        using SizeType = size_t;

        Deque() noexcept = default;

        explicit Deque(SizeType count)
        : _d(count) {
        }

        Deque(SizeType count, const T& value)
        : _d(count, value) {
        }

        template<std::input_iterator It>
        Deque(It first, It last)
        : _d(first, last) {
        }

        // From a range of what the elements are made of (the Pieces of a
        // String, a view, another container)
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, Deque>) && (!std::is_same_v<std::remove_cvref_t<R>, InnerType>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit Deque(R&& r)
        : _d(std::ranges::begin(r), std::ranges::end(r)) {
        }

        Deque(std::initializer_list<T> il)
        : _d(il) {
        }

        explicit Deque(InnerType d) noexcept
        : _d(std::move(d)) {
        }

        Deque(const Deque&) = default;
        Deque(Deque&&) noexcept = default;
        Deque& operator=(const Deque&) = default;
        Deque& operator=(Deque&&) noexcept = default;

        Deque& operator=(std::initializer_list<T> il) {
            _d = il;
            return *this;
        }

        void Assign(SizeType count, const T& value) {
            _d.assign(count, value);
        }

        template<std::input_iterator It>
        void Assign(It first, It last) {
            _d.assign(first, last);
        }

        void Assign(std::initializer_list<T> il) {
            _d.assign(il);
        }

        T& operator[](SizeType i) noexcept {
            return _d[i];
        }

        const T& operator[](SizeType i) const noexcept {
            return _d[i];
        }

        T& First() noexcept {
            return _d.front();
        }

        const T& First() const noexcept {
            return _d.front();
        }

        T& Last() noexcept {
            return _d.back();
        }

        const T& Last() const noexcept {
            return _d.back();
        }

        SizeType Count() const noexcept {
            return _d.size();
        }

        bool IsEmpty() const noexcept {
            return _d.empty();
        }

        void AddFirst(const T& value) {
            _d.push_front(value);
        }

        void AddFirst(T&& value) {
            _d.push_front(std::move(value));
        }

        void AddLast(const T& value) {
            _d.push_back(value);
        }

        void AddLast(T&& value) {
            _d.push_back(std::move(value));
        }

        template<class... A>
        T& EmplaceFirst(A&&... a) {
            return _d.emplace_front(std::forward<A>(a)...);
        }

        template<class... A>
        T& EmplaceLast(A&&... a) {
            return _d.emplace_back(std::forward<A>(a)...);
        }

        void Insert(SizeType i, const T& value) {
            _d.insert(_d.begin() + i, value);
        }

        void Insert(SizeType i, T&& value) {
            _d.insert(_d.begin() + i, std::move(value));
        }

        void Insert(SizeType i, SizeType count, const T& value) {
            _d.insert(_d.begin() + i, count, value);
        }

        void Insert(SizeType i, std::initializer_list<T> il) {
            _d.insert(_d.begin() + i, il);
        }

        template<std::ranges::input_range R>
        void InsertRange(SizeType i, R&& r) {
            _d.insert(_d.begin() + i, std::ranges::begin(r), std::ranges::end(r));
        }

        template<class... A>
        T& EmplaceAt(SizeType i, A&&... a) {
            return *_d.emplace(_d.begin() + i, std::forward<A>(a)...);
        }

        void RemoveFirst() noexcept {
            _d.pop_front();
        }

        void RemoveLast() noexcept {
            _d.pop_back();
        }

        void RemoveAt(SizeType i) {
            _d.erase(_d.begin() + i);
        }

        void RemoveRange(SizeType i, SizeType n) {
            _d.erase(_d.begin() + i, _d.begin() + i + n);
        }

        bool Remove(const T& value) {
            auto it = std::find(_d.begin(), _d.end(), value);
            if (it == _d.end()) {
                return false;
            }
            _d.erase(it);
            return true;
        }

        template<class U>
        requires std::equality_comparable_with<const T&, const U&>
        SizeType RemoveAll(const U& value) {
            return erase(_d, value);
        }

        template<class Pred>
        requires std::predicate<Pred&, const T&>
        SizeType RemoveAll(Pred pred) {
            return erase_if(_d, pred);
        }

        void Clear() noexcept {
            _d.clear();
        }

        // The spare blocks dropped: the map holds exactly the blocks in use
        void Shrink() {
            _d.shrink_to_fit();
        }

        void Resize(SizeType n) {
            _d.resize(n);
        }

        void Resize(SizeType n, const T& value) {
            _d.resize(n, value);
        }

        void Swap(Deque& o) noexcept {
            _d.swap(o._d);
        }

        InnerType& Inner() noexcept {
            return _d;
        }

        const InnerType& Inner() const noexcept {
            return _d;
        }

        friend bool operator==(const Deque& a, const Deque& b) {
            return a._d == b._d;
        }

        friend auto operator<=>(const Deque& a, const Deque& b) {
            return a._d <=> b._d;
        }

    private:
        InnerType _d;
    };

    template<class T>
    auto begin(Deque<T>& d) noexcept {
        return d.Inner().begin();
    }

    template<class T>
    auto end(Deque<T>& d) noexcept {
        return d.Inner().end();
    }

    template<class T>
    auto begin(const Deque<T>& d) noexcept {
        return d.Inner().begin();
    }

    template<class T>
    auto end(const Deque<T>& d) noexcept {
        return d.Inner().end();
    }

    template<class T>
    void swap(Deque<T>& a, Deque<T>& b) noexcept {
        a.Swap(b);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

