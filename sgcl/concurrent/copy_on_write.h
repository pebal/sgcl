//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "../core/detail/backoff.h"

#include <type_traits>
#include <utility>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A value read by many threads and replaced by few, whole: the
    // copy-on-write of Java's CopyOnWriteArrayList, for any copyable T. The
    // value lives in a managed object of its own and is never modified
    // there: a reader loads the pointer, one atomic load, and has an
    // immutable snapshot that stays what it is, and alive, for as long
    // as the reader holds it; a writer copies the value, changes the copy
    // and swings the pointer with a compare-exchange, and the value it
    // replaced is garbage once the last snapshot of it is dropped. No
    // lock on either side, no reference count on the snapshot, no reader
    // ever waits and no writer ever waits for a reader: what an RCU or a
    // shared_ptr swapped under a lock is built to approximate, in three
    // words of code, because the collector answers the one question those
    // exist for, when the old value may be freed. A writer that loses the
    // exchange to another writer copies again: writers are meant to be
    // rare next to readers (a configuration, a routing table, a list of
    // listeners), and with many of them a mutex serializes them cheaper.
    // The container holds one word, the atomic pointer; it lives where a
    // tracked_ptr may (on a stack or inside a managed object), and so do
    // its snapshots.
    template<class T>
    class copy_on_write {
        static_assert(std::is_copy_constructible_v<T>, "copy_on_write copies the value on every update");

    public:
        using value_type = T;
        // The current value, immutable, held alive: a pointer of the
        // container's kind to const T
        using snapshot = tracked_ptr<const T>;

        copy_on_write()
        : _value(make_tracked<T>()) {
        }

        explicit copy_on_write(const T& value)
        : _value(make_tracked<T>(value)) {
        }

        explicit copy_on_write(T&& value)
        : _value(make_tracked<T>(std::move(value))) {
        }

        template<class... A>
        explicit copy_on_write(std::in_place_t, A&&... a)
        : _value(make_tracked<T>(std::forward<A>(a)...)) {
        }

        copy_on_write(const copy_on_write&) = delete;
        copy_on_write& operator=(const copy_on_write&) = delete;

        // The current value: one atomic load
        snapshot load() const noexcept {
            return _value.load(std::memory_order_acquire);
        }

        operator snapshot() const noexcept {
            return load();
        }

        // Replaces the value, whole: a store of a new managed object
        void store(const T& value) {
            _value.store(make_tracked<T>(value), std::memory_order_release);
        }

        void store(T&& value) {
            _value.store(make_tracked<T>(std::move(value)), std::memory_order_release);
        }

        copy_on_write& operator=(const T& value) {
            store(value);
            return *this;
        }

        copy_on_write& operator=(T&& value) {
            store(std::move(value));
            return *this;
        }

        // Changes the value: f(T&) on a copy of the current one, which
        // replaces it with a compare-exchange; copied and changed again
        // when another writer got in between, so f is called once or
        // more, on copies nobody else sees. Returns the value installed.
        template<class F>
        snapshot update(F&& f) {
            detail::Backoff<> backoff;   // after a lost exchange: writers at one word, each a copy the poorer
            tracked_ptr<T> old = _value.load(std::memory_order_acquire);
            for (;;) {
                tracked_ptr<T> next = make_tracked<T>(*old);
                f(*next);
                if (_value.compare_exchange_strong(old, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return next;
                }
                backoff();
            }
        }

        // Replaces the value if it is still the one `expected` is a
        // snapshot of; otherwise `expected` becomes the current snapshot
        bool compare_exchange(snapshot& expected, const T& desired) {
            return _compare_exchange(expected, make_tracked<T>(desired));
        }

        bool compare_exchange(snapshot& expected, T&& desired) {
            return _compare_exchange(expected, make_tracked<T>(std::move(desired)));
        }

    private:
        bool _compare_exchange(snapshot& expected, tracked_ptr<T> next) {
            tracked_ptr<T> e = const_pointer_cast<T>(tracked_ptr<const T>(expected));
            if (_value.compare_exchange_strong(e, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            expected = e;
            return false;
        }

        atomic<tracked_ptr<T>> _value;
    };

    // Deduction: the copy_on_write of the value it starts with
    template<class T>
    copy_on_write(T) -> copy_on_write<T>;
}
