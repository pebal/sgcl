//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "tracked_ptr.h"

namespace sgcl {
    template<class T>
    class atomic_ref<tracked_ptr<T>> {
    public:
        using value_type = tracked_ptr<T>;

        atomic_ref& operator=(const atomic_ref&) = delete;

        explicit atomic_ref(value_type& p) noexcept
        : ref(p) {
        }

        atomic_ref(const atomic_ref& a) noexcept
        : ref(a.ref) {
        }

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            store(nullptr);
            return nullptr;
        }

        void operator=(unique_ptr<T>&& p) noexcept {
            store(std::move(p));
        }

        value_type operator=(value_type p) noexcept {
            _ptr().store(p.get(), std::memory_order_seq_cst);
            return p;
        }

        operator value_type() const noexcept {
            return load();
        }

        static constexpr bool is_always_lock_free = detail::RawPointer::is_always_lock_free;
        static constexpr std::size_t required_alignment = alignof(detail::RawPointer);

        bool is_lock_free() const noexcept {
            return _ptr()._is_lock_free();
        }

        value_type load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            auto& thread = detail::current_thread();
            std::memory_order order = m > std::memory_order::acquire ? m : std::memory_order::acquire;
            auto l = (T*)_ptr().load(order);
            T* t;
            do {
                t = l;
                thread.set_hazard_pointer(l);
                l = (T*)_ptr().load(order);
            } while(l != t);
            value_type p(l);
            thread.clear_hazard_pointer();
            return p;
        }

        void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(nullptr, m);
        }

        void store(unique_ptr<T>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.release(), m);
        }

        void store(value_type p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().store(p.get(), m);
        }

        bool compare_exchange_strong(value_type& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(value_type& e, value_type n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(value_type& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, nullptr, s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(value_type& e, value_type n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(value_type& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(value_type& e, value_type n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(value_type& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, nullptr, s, f)) {
                e = load(std::memory_order_acquire);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(value_type& e, value_type n, const std::memory_order s, const std::memory_order f) noexcept {
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e = load(std::memory_order_acquire);
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

        void wait(value_type p, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            _ptr().wait(p.get(), m);
        }

        value_type& ref;

    private:
        detail::Pointer& _ptr() noexcept {
            return *ref._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *ref._ptr();
        }
    };

    template <class T>
    atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
}
