//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "tracked_ptr.h"
#include "root_ptr.h"

namespace sgcl {
    template<class T>
    class atomic {
        using Type = typename T::Type;

    public:
        using value_type = T;

        atomic() {}

        atomic(std::nullptr_t)
            : _val(nullptr) {
        }

        atomic(const root_ptr<Type>& p)
            : _val(p) {
        }

        atomic(const tracked_ptr<Type>& p)
            : _val(p) {
        }

        atomic(unique_ptr<Type>&& p)
            : _val(std::move(p)) {
        }

        atomic(const atomic&) = delete;
        atomic& operator =(const atomic&) = delete;

        void operator=(const value_type& p) noexcept {
            store(p);
        }

        static constexpr bool is_always_lock_free = Priv::Pointer::is_always_lock_free;

        bool is_lock_free() const noexcept {
            return _ptr()._is_lock_free();
        }

        root_ptr<Type> load(const std::memory_order m = std::memory_order_seq_cst) const {
            root_ptr<Type> t;
            auto l = _ptr().load_atomic(m);
            t._ptr().store_no_update(l);
            return t;
        }

        void store(const tracked_ptr<Type>& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().update_atomic();
            _ptr().store(p.get(), m);
        }

        void store(unique_ptr<Type>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().update_atomic();
            _ptr().store(p.release(), m);
        }

        void exchange(tracked_ptr<Type>& p, const std::memory_order m = std::memory_order_seq_cst) {
            _ptr().update_atomic();
            auto l = _ptr().exchange(p.get(), m);
            p._ptr().store(l);
        }

        bool compare_exchange_strong(tracked_ptr<Type>& e, const tracked_ptr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().update_atomic();
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), m)) {
                e._ptr().store(l);
                return false;
            }
            return true;
        }

        bool compare_exchange_strong(tracked_ptr<Type>& e, const tracked_ptr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            _ptr().update_atomic();
            void* l = e.get();
            if (!_ptr().compare_exchange_strong(l, n.get(), s, f)) {
                e._ptr().store(l);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<Type>& e, const tracked_ptr<Type>& n, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            _ptr().update_atomic();
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), m)) {
                e._ptr().store(l);
                return false;
            }
            return true;
        }

        bool compare_exchange_weak(tracked_ptr<Type>& e, const tracked_ptr<Type>& n, const std::memory_order s, const std::memory_order f) noexcept {
            _ptr().update_atomic();
            void* l = e.get();
            if (!_ptr().compare_exchange_weak(l, n.get(), s, f)) {
                e._ptr().store(l);
                return false;
            }
            return true;
        }

        operator value_type() const noexcept {
            return load();
        }

    private:
        Priv::Tracked_ptr& _ptr() noexcept {
            return _val._ptr();
        }

        const Priv::Tracked_ptr& _ptr() const noexcept {
            return _val._ptr();
        }

        value_type _val;
    };
}
