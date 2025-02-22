//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "stack_ptr.h"
#include "tracked_ptr.h"
#include "make_tracked.h"

namespace sgcl {
    namespace detail {
        inline auto& global_tmp_ptr() {
            static thread_local auto p = make_tracked<TrackedPtr<void>>();
            return *p;
        }
    }

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
            auto l = (Type*)_ptr().load(m);
            return StackPtr<Type>(l);
        }

        void store(const TrackedPtr<Type>& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _protect_value(_val);
            _ptr().store(p.get(), m);
        }

        void store(UniquePtr<Type>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _protect_value(_val);
            _ptr().store(p.release(), m);
        }

        void exchange(TrackedPtr<Type>& p, const std::memory_order m = std::memory_order_seq_cst) {
            _protect_value(_val);
            auto l = _ptr().exchange(p.get(), m);
            p._ptr().store(l);
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            detail::global_tmp_ptr() = _val;
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e._ptr().store(l);
                detail::global_tmp_ptr() = nullptr;
                return false;
            }
            _protect_value(detail::global_tmp_ptr());
            detail::global_tmp_ptr() = nullptr;
            return true;
        }

        bool compare_exchange_strong(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            detail::global_tmp_ptr() = _val;
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e._ptr().store(l);
                detail::global_tmp_ptr() = nullptr;
                return false;
            }
            _protect_value(detail::global_tmp_ptr());
            detail::global_tmp_ptr() = nullptr;
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) {
            detail::global_tmp_ptr() = _val;
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e._ptr().store(l);
                detail::global_tmp_ptr() = nullptr;
                return false;
            }
            _protect_value(detail::global_tmp_ptr());
            detail::global_tmp_ptr() = nullptr;
            return true;
        }

        bool compare_exchange_weak(TrackedPtr<Type>& e, const TrackedPtr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            detail::global_tmp_ptr() = _val;
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e._ptr().store(l);
                detail::global_tmp_ptr() = nullptr;
                return false;
            }
            _protect_value(detail::global_tmp_ptr());
            detail::global_tmp_ptr() = nullptr;
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

        template<class Ptr>
        inline static void _protect_value(const Ptr& v) {
            if (v) {
                auto p = make_tracked<TrackedPtr<Type>>((Type*)v.get());
                detail::Page::set_state<detail::State::ReachableAtomic>(p.release());
            }
        }

        value_type _val;
    };
}
