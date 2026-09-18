//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/pointer.h"
#include "../../core/detail/thread.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <atomic>

namespace sgcl::detail {
    // The operations of an atomic tracked word, for the atomics (atomic.h,
    // atomic_ref.h): Derived supplies the word (_ptr(), a detail::Pointer&),
    // and the operations take and return a tracked_ptr<T>: a value passed
    // is taken by value, a copy on the stack that roots the target for
    // the length of the call; an expected value binds as the word the
    // caller's pointer holds its object by (a root_ptr converts to it,
    // root_ptr.h); _load builds the value on the stack under the hazard
    // pointer, as a tracked_ptr without a check. No ABA: a node is never
    // reused while a thread holds it.
    template<class Derived, class T>
    class AtomicWord {
        using Value = tracked_ptr<T>;

    public:
        static constexpr bool is_always_lock_free = RawPointer::is_always_lock_free;

        // The interface of std::atomic<T*> on the word, the stores and the
        // successful exchanges through the barrier (pointer.h)
        bool is_lock_free() const noexcept {
            return _ptr().is_lock_free();
        }

        Value load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            return _load(m);
        }

        operator Value() const noexcept {
            return load();
        }

        void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(nullptr, m);
        }

        void store(unique_ptr<T>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store_released(p.release(), m);   // the barrier that takes the object out of the unique state
        }

        void store(tracked_ptr<T> p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.get(), m);
        }

        bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, m)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        // The word replaced, the old value held: the old object is under
        // the hazard pointer from before the exchange (the word read
        // twice around the hazard's store, as in _load) until the value
        // returned holds it
        Value exchange(tracked_ptr<T> n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            auto& thread = current_thread();
            std::memory_order order = m < std::memory_order::acq_rel ? std::memory_order::acq_rel : m;   // the exchange after the hazard's store must not pass it (_load): acquire and release both, RCsc on arm64 (casal)
            auto l = (T*)_ptr().load(std::memory_order_relaxed);
            for (;;) {
                thread.set_hazard_pointer(l);
                void* o = l;
                if (_ptr().compare_exchange_strong(o, n.get(), order)) {
                    break;
                }
                l = (T*)o;
            }
            Value p(l, OnRegisteredThread{});
            thread.clear_hazard_pointer();
            return p;
        }

        Value exchange(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            return exchange(tracked_ptr<T>(), m);
        }

        bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, s, f)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, m)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, s, f)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e = _load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        void notify_one() noexcept {
            _ptr().notify_one();
        }

        void notify_all() noexcept {
            _ptr().notify_all();
        }

        void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            _ptr().wait(nullptr, m);
        }

        void wait(tracked_ptr<T> p, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            _ptr().wait(p.get(), m);
        }

    protected:
        // The word read twice around the store of the hazard pointer, so
        // that the collector, which reads the hazards after a pass over
        // the states, either sees the hazard or sees the value in the
        // word: the object is held from the moment the load agrees, and
        // the value is built while it is. The load after the hazard's
        // store is seq_cst whatever the caller asked, as in weak_ptr::lock:
        // the store must be visible before the load is performed, which
        // acquire does not promise (on arm64 an acquire load is ldapr
        // since ARMv8.3, which the architecture lets pass an earlier
        // stlr; a seq_cst load is ldar, which it does not); the first
        // load is only the guess the loop starts from, so relaxed.
        Value _load(const std::memory_order) const noexcept {
            auto& thread = current_thread();
            auto l = (T*)_ptr().load(std::memory_order_relaxed);
            T* t;
            do {
                t = l;
                thread.set_hazard_pointer(l);
                l = (T*)_ptr().load(std::memory_order_seq_cst);
            } while(l != t);
            Value p(l, OnRegisteredThread{});
            thread.clear_hazard_pointer();
            return p;
        }

    private:
        Pointer& _ptr() noexcept {
            return static_cast<Derived*>(this)->_ptr();
        }

        const Pointer& _ptr() const noexcept {
            return static_cast<const Derived*>(this)->_ptr();
        }
    };
}
