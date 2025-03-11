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
    class AtomicRef {
        using Type = typename T::ValueType;

    public:
        using ValueType = TrackedPtr<Type>;

        explicit AtomicRef(TrackedPtr<Type>& p) noexcept
        : ref(p) {
        }

        AtomicRef(const AtomicRef& a) noexcept
        : ref(a.ref) {
        }

        AtomicRef& operator=(const AtomicRef&) = delete;

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            store(nullptr);
            return nullptr;
        }

        void operator=(UniquePtr<Type>&& p) noexcept {
            store(std::move(p));
        }

        UnsafePtr<Type> operator=(UnsafePtr<Type> p) noexcept {
            store(p);
            return p;
        }

        operator StackPtr<Type>() const noexcept {
            return load();
        }

        static constexpr bool is_always_lock_free = detail::RawPointer::is_always_lock_free;
        static constexpr std::size_t required_alignment = alignof(detail::RawPointer);

        bool is_lock_free() const noexcept {
            return _ptr()._is_lock_free();
        }

        StackPtr<Type> load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
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

        void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(nullptr, m);
        }

        void store(UniquePtr<Type>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.release(), m);
        }

        void store(UnsafePtr<Type> p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.get(), m);
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, UnsafePtr<Type> n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, UnsafePtr<Type> n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, UnsafePtr<Type> n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, UnsafePtr<Type> n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        void notify_one() noexcept {
            ref._ptr().notify_one();
        }

        void notify_all() noexcept {
            ref._ptr().notify_all();
        }

        void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            ref._ptr().wait(nullptr, m);
        }

        void wait(UnsafePtr<Type> p, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            ref._ptr().wait(p.get(), m);
        }

        ValueType& ref;

    private:
        detail::Pointer& _ptr() noexcept {
            return ref._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return ref._ptr();
        }
    };

    template <class T, PointerPolicy P>
    AtomicRef(Pointer<T, P>) -> AtomicRef<Pointer<T, P>>;
}
