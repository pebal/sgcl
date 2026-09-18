//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// List<T>: the contiguous list, its buffer on the
// managed heap. A value: a copy is a copy, sharing goes through a Ptr.
// Positions are unchecked, as the vector's are.
#pragma once

#include "../../containers/vector.h"
#include "MSequence.h"

#include <algorithm>
#include <ranges>

namespace Sgcl {
    template<class T>
    class List : public MSequence<List<T>> {   // the algorithms as members
    public:
        using ValueType = T;
        using InnerType = sgcl::vector<T>;
        using SizeType = size_t;
        using Iterator = typename InnerType::iterator;
        using ConstIterator = typename InnerType::const_iterator;

        List() noexcept = default;

        explicit List(SizeType count)
        : _v(count) {
        }

        List(SizeType count, const T& value)
        : _v(count, value) {
        }

        template<std::input_iterator It>
        List(It first, It last)
        : _v(first, last) {
        }

        // From a range of what the elements are made of (the Pieces of a
        // String, a view, another container)
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, List>) && (!std::is_same_v<std::remove_cvref_t<R>, InnerType>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit List(R&& r)
        : _v(std::ranges::begin(r), std::ranges::end(r)) {
        }

        List(std::initializer_list<T> il)
        : _v(il) {
        }

        explicit List(InnerType v) noexcept
        : _v(std::move(v)) {
        }

        List(const List&) = default;
        List(List&&) noexcept = default;
        List& operator=(const List&) = default;
        List& operator=(List&&) noexcept = default;

        List& operator=(std::initializer_list<T> il) {
            _v = il;
            return *this;
        }

        // The contents replaced
        void Assign(SizeType count, const T& value) {
            _v.assign(count, value);
        }

        template<std::input_iterator It>
        void Assign(It first, It last) {
            _v.assign(first, last);
        }

        void Assign(std::initializer_list<T> il) {
            _v.assign(il);
        }

        // The elements
        T& operator[](SizeType i) noexcept {
            return _v[i];
        }

        const T& operator[](SizeType i) const noexcept {
            return _v[i];
        }

        T& First() noexcept {
            return _v.front();
        }

        const T& First() const noexcept {
            return _v.front();
        }

        T& Last() noexcept {
            return _v.back();
        }

        const T& Last() const noexcept {
            return _v.back();
        }

        T* Data() noexcept {
            return _v.data();
        }

        const T* Data() const noexcept {
            return _v.data();
        }

        // The size
        SizeType Count() const noexcept {
            return _v.size();
        }

        bool IsEmpty() const noexcept {
            return _v.empty();
        }

        SizeType Capacity() const noexcept {
            return _v.capacity();
        }

        void Reserve(SizeType n) {
            _v.reserve(n);
        }

        void Resize(SizeType n) {
            _v.resize(n);
        }

        void Resize(SizeType n, const T& value) {
            _v.resize(n, value);
        }

        void Shrink() {
            _v.shrink_to_fit();
        }

        // Adding
        void Add(const T& value) {
            _v.push_back(value);
        }

        void Add(T&& value) {
            _v.push_back(std::move(value));
        }

        template<class... A>
        T& Emplace(A&&... a) {
            return _v.emplace_back(std::forward<A>(a)...);
        }

        template<std::ranges::input_range R>
        void AddRange(R&& r) {
            _v.insert(_v.end(), std::ranges::begin(r), std::ranges::end(r));
        }

        void AddRange(std::initializer_list<T> il) {
            _v.insert(_v.end(), il);
        }

        void Insert(SizeType i, const T& value) {
            _v.insert(_v.begin() + i, value);
        }

        void Insert(SizeType i, T&& value) {
            _v.insert(_v.begin() + i, std::move(value));
        }

        void Insert(SizeType i, SizeType count, const T& value) {
            _v.insert(_v.begin() + i, count, value);
        }

        void Insert(SizeType i, std::initializer_list<T> il) {
            _v.insert(_v.begin() + i, il);
        }

        template<class... A>
        T& EmplaceAt(SizeType i, A&&... a) {
            return *_v.emplace(_v.begin() + i, std::forward<A>(a)...);
        }

        template<std::ranges::input_range R>
        void InsertRange(SizeType i, R&& r) {
            _v.insert(_v.begin() + i, std::ranges::begin(r), std::ranges::end(r));
        }

        // Removing
        void RemoveAt(SizeType i) {
            _v.erase(_v.begin() + i);
        }

        void RemoveRange(SizeType i, SizeType n) {
            _v.erase(_v.begin() + i, _v.begin() + i + n);
        }

        void RemoveLast() noexcept {
            _v.pop_back();
        }

        // The first element equal to `value` removed: whether there was one
        bool Remove(const T& value) {
            auto it = std::find(_v.begin(), _v.end(), value);
            if (it == _v.end()) {
                return false;
            }
            _v.erase(it);
            return true;
        }

        // Every element equal to `value` removed: how many
        template<class U>
        requires std::equality_comparable_with<const T&, const U&>
        SizeType RemoveAll(const U& value) {
            return std::erase(_v, value);
        }

        // Every element the predicate accepts removed: how many
        template<class Pred>
        requires std::predicate<Pred&, const T&>
        SizeType RemoveAll(Pred pred) {
            return std::erase_if(_v, pred);
        }

        void Clear() noexcept {
            _v.clear();
        }

        void Swap(List& o) noexcept {
            _v.swap(o._v);
        }

        InnerType& Inner() noexcept {
            return _v;
        }

        const InnerType& Inner() const noexcept {
            return _v;
        }

        friend bool operator==(const List& a, const List& b) {
            return a._v == b._v;
        }

        friend auto operator<=>(const List& a, const List& b) {
            return a._v <=> b._v;
        }

    private:
        InnerType _v;
    };

    template<std::input_iterator It>
    List(It, It) -> List<std::iter_value_t<It>>;

    template<class T>
    auto begin(List<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(List<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    auto begin(const List<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(const List<T>& l) noexcept {
        return l.Inner().end();
    }

    template<class T>
    void swap(List<T>& a, List<T>& b) noexcept {
        a.Swap(b);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

