//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../tracked_ptr.h"
#include "../unique_ptr.h"
#include "pointer.h"
#include "thread.h"

#include <atomic>

namespace sgcl::detail {
    // The operations of an atomic tracked word, for the four atomics
    // (atomic.h, atomic_ref.h): Derived supplies the word (_ptr(), a
    // detail::Pointer&), and Value is the type the operations return
    // outside, tracked_ptr<T> or gc::tracked_ptr<T> (a parameter, not
    // Derived::value_type: Derived is incomplete where the base is
    // instantiated). Inside, sgcl::tracked_ptr only: a value passed is
    // taken by value, a copy on the stack that roots the target for the
    // length of the call (a gc::tracked_ptr argument converts to the word
    // it holds and is copied from it, with no check of a location); an
    // expected value binds as the word the caller's pointer holds its
    // object by, wherever that pointer is; _load builds the value on the
    // stack under the hazard pointer, as an sgcl::tracked_ptr without a
    // check, or, for load(), as the Value itself (one construction, with
    // the check of a gc::tracked_ptr where Value is one). No ABA: a node
    // is never reused while a thread holds it.
    template<class Derived, class T, class Value>
    class AtomicWord {
    public:
        static constexpr bool is_always_lock_free = RawPointer::is_always_lock_free;

        bool is_lock_free() const noexcept {
            return _ptr().is_lock_free();
        }

        Value load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            return _load<Value>(m);
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
        // the value (V: tracked_ptr<T>, or the Value of load()) is built
        // while it is.
        template<class V = tracked_ptr<T>>
        V _load(const std::memory_order m) const noexcept {
            auto& thread = current_thread();
            std::memory_order order = m > std::memory_order::acquire ? m : std::memory_order::acquire;
            auto l = (T*)_ptr().load(order);
            T* t;
            do {
                t = l;
                thread.set_hazard_pointer(l);
                l = (T*)_ptr().load(order);
            } while(l != t);
            V p(l, OnRegisteredThread{});
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
