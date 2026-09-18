//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PersistentList<T>: the persistent vector of Clojure and Scala. Every
// change (Add, Set, RemoveLast) returns a new list and leaves this one as
// it was, the two sharing everything but the path that changed; any
// number of threads read any version without a lock. A value of four
// words; a version is published through a CopyOnWrite or an Atomic.
#pragma once

#include "../../concurrent/persistent_vector.h"

#include <initializer_list>
#include <iterator>

namespace Sgcl {
    template<class T>
    class PersistentList {
    public:
        using ValueType = T;
        using InnerType = sgcl::persistent_vector<T>;
        using SizeType = size_t;
        using Iterator = typename InnerType::const_iterator;
        using ConstIterator = typename InnerType::const_iterator;

        PersistentList() noexcept = default;

        template<std::input_iterator It>
        PersistentList(It first, It last)
        : _v(first, last) {
        }

        PersistentList(std::initializer_list<T> il)
        : _v(il) {
        }

        explicit PersistentList(InnerType v) noexcept
        : _v(std::move(v)) {
        }

        PersistentList(const PersistentList&) noexcept = default;
        PersistentList(PersistentList&&) noexcept = default;
        PersistentList& operator=(const PersistentList&) noexcept = default;
        PersistentList& operator=(PersistentList&&) noexcept = default;

        // The elements: unchecked, as List's are; At checks
        const T& operator[](SizeType i) const noexcept {
            return _v[i];
        }

        const T& At(SizeType i) const {
            return _v.at(i);
        }

        const T& First() const noexcept {
            return _v.front();
        }

        const T& Last() const noexcept {
            return _v.back();
        }

        // The size
        SizeType Count() const noexcept {
            return _v.size();
        }

        bool IsEmpty() const noexcept {
            return _v.empty();
        }

        // The list with `value` after its last element: this one unchanged
        PersistentList Add(const T& value) const {
            return PersistentList(_v.push_back(value));
        }

        PersistentList Add(T&& value) const {
            return PersistentList(_v.push_back(std::move(value)));
        }

        template<class... A>
        PersistentList Emplace(A&&... a) const {
            return PersistentList(_v.emplace_back(std::forward<A>(a)...));
        }

        // The list with the element at `i` replaced
        PersistentList Set(SizeType i, const T& value) const {
            return PersistentList(_v.set(i, value));
        }

        PersistentList Set(SizeType i, T&& value) const {
            return PersistentList(_v.set(i, std::move(value)));
        }

        // The list without its last element
        PersistentList RemoveLast() const {
            return PersistentList(_v.pop_back());
        }

        InnerType& Inner() noexcept {
            return _v;
        }

        const InnerType& Inner() const noexcept {
            return _v;
        }

        friend bool operator==(const PersistentList& a, const PersistentList& b) {
            return a._v == b._v;
        }

        friend bool operator!=(const PersistentList& a, const PersistentList& b) {
            return !(a._v == b._v);
        }

    private:
        InnerType _v;
    };

    template<std::input_iterator It>
    PersistentList(It, It) -> PersistentList<std::iter_value_t<It>>;

    template<class T>
    auto begin(const PersistentList<T>& l) noexcept {
        return l.Inner().begin();
    }

    template<class T>
    auto end(const PersistentList<T>& l) noexcept {
        return l.Inner().end();
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
