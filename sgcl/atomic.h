//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "stack_ptr.h"
#include "tracked_ptr.h"

namespace sgcl {
    template<TrackedPointer T>
    class Atomic {
        using Type = typename T::element_type;

    public:
        using value_type = T;

        Atomic() {}

        Atomic(std::nullptr_t)
            : _val(nullptr) {
        }

        template<PointerPolicy P>
        Atomic(const Pointer<Type, P>& p)
            : _val(p) {
        }

        Atomic(UniquePtr<Type>&& p)
            : _val(std::move(p)) {
        }

        Atomic(const Atomic&) = delete;
        Atomic& operator =(const Atomic&) = delete;

        void operator=(const value_type& p) noexcept {
            store(p);
        }

        static constexpr bool is_always_lock_free = detail::RawPointer::is_always_lock_free;

        bool is_lock_free() const noexcept {
            return _ptr()._is_lock_free();
        }

        StackPtr<Type> load(const std::memory_order m = std::memory_order_seq_cst) const {
            auto& thread = detail::current_thread();
            std::memory_order order = m > std::memory_order::acquire ? m : std::memory_order::acquire;
            auto l = (Type*)_ptr().load(order);
            Type* t;
            do {
                t = l;
                thread.set_hazard_pointer(l);
                l = (Type*)_ptr().load(order);
            } while(l != t);
            StackPtr<Type> p(l);
            thread.clear_hazard_pointer();
            return p;
        }

        void store(const TrackedPtr<Type>& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.get(), m);
        }

        void store(UniquePtr<Type>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.release(), m);
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        operator value_type() const noexcept {
            return load();
        }

    private:
        detail::Pointer& _ptr() noexcept {
            return _val._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return _val._ptr();
        }

        value_type _val;
    };
}
