//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// CopyOnWrite<T>: a value any number of
// threads read through immutable snapshots and a writer replaces whole
// (a copy, a change, a compare-exchange). Java's CopyOnWriteArrayList
// for any T.
#pragma once

#include "../../concurrent/copy_on_write.h"
#include "../Core/Ptr.h"

namespace Sgcl {
    template<class T>
    class CopyOnWrite {
    public:
        using ValueType = T;
        using InnerType = sgcl::copy_on_write<T>;
        using Snapshot = Ptr<const T>;   // the value as it was, alive while held

        CopyOnWrite()
        : _c() {
        }

        explicit CopyOnWrite(const T& value)
        : _c(value) {
        }

        explicit CopyOnWrite(T&& value)
        : _c(std::move(value)) {
        }

        template<class... A>
        explicit CopyOnWrite(std::in_place_t, A&&... a)
        : _c(std::in_place, std::forward<A>(a)...) {
        }

        CopyOnWrite(const CopyOnWrite&) = delete;
        CopyOnWrite& operator=(const CopyOnWrite&) = delete;

        // The current value: a snapshot that stays what it is
        Snapshot Load() const noexcept {
            return Snapshot(_c.load());
        }

        operator Snapshot() const noexcept {
            return Load();
        }

        void Store(const T& value) {
            _c.store(value);
        }

        void Store(T&& value) {
            _c.store(std::move(value));
        }

        CopyOnWrite& operator=(const T& value) {
            _c = value;
            return *this;
        }

        CopyOnWrite& operator=(T&& value) {
            _c = std::move(value);
            return *this;
        }

        // A copy of the value changed by f and installed, again when another
        // writer got in between: the snapshot installed
        template<class F>
        Snapshot Update(F&& f) {
            return Snapshot(_c.update(std::forward<F>(f)));
        }

        // The value replaced if it is still the one `expected` is a snapshot
        // of; otherwise `expected` becomes the current snapshot
        bool CompareExchange(Snapshot& expected, const T& desired) {
            return _c.compare_exchange(expected.Inner(), desired);
        }

        bool CompareExchange(Snapshot& expected, T&& desired) {
            return _c.compare_exchange(expected.Inner(), std::move(desired));
        }

        InnerType& Inner() noexcept {
            return _c;
        }

        const InnerType& Inner() const noexcept {
            return _c;
        }

    private:
        InnerType _c;
    };

    // Deduction: the CopyOnWrite of the value it starts with
    template<class T>
    CopyOnWrite(T) -> CopyOnWrite<T>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

