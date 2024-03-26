//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/collector.h"
#include "tracked_ptr.h"

namespace sgcl {   
    template<class T>
    class root_ptr {
        static constexpr ptrdiff_t MaxStackOffset = 1024;
        static constexpr uintptr_t HeapRootFlag = 1;
        static constexpr uintptr_t ClearMask = ~HeapRootFlag;

    public:
        using element_type = std::remove_extent_t<T>;

        root_ptr() {
            if (!_stack_detected()) {
                auto ref = Priv::Current_thread().heap_roots_allocator->alloc();
                _ref = _set_flag(ref, HeapRootFlag);
            } else {
                _ref = Priv::Current_thread().stack_roots_allocator->alloc(this);
            }
        }

        root_ptr(std::nullptr_t)
        : root_ptr() {
        }

        template<class U, std::enable_if_t<!std::is_array_v<T> && std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit root_ptr(U* p)
        : root_ptr() {
            _ptr().store(static_cast<element_type*>(p));
        }

        root_ptr(const root_ptr& p)
        : root_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr(const root_ptr<U>& p)
        : root_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr(const tracked_ptr<U>& p)
        : root_ptr(p.get()) {
        }

        root_ptr(root_ptr&& p)
        : root_ptr(std::move(p), p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr(root_ptr<U>&& p)
        : root_ptr(std::move(p), static_cast<element_type*>(p.get())) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr(unique_ptr<U>&& u)
        : root_ptr() {
            auto p = u.release();
            _ptr().force_store(static_cast<element_type*>(p));
        }

        ~root_ptr() {
            if (_ref) {
                _ptr().store(nullptr);
                if (_is_heap_root() && !Priv::Collector::aborted()) {
                    Priv::Current_thread().heap_roots_allocator->free(_ref);
                }
            }
        }

        root_ptr& operator=(std::nullptr_t) noexcept {
            _ptr().store(nullptr);
            return *this;
        }

        root_ptr& operator=(const root_ptr& p) noexcept {
            _ptr().store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr& operator=(const root_ptr<U>& p) noexcept {
            _ptr().store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _ptr().store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr& operator=(std::unique_ptr<U, unique_deleter>&& u) noexcept {
            auto p = u.release();
            _ptr().force_store(static_cast<element_type*>(p));
            return *this;
        }

        operator tracked_ptr<T>&() noexcept {
            return reinterpret_cast<tracked_ptr<T>&>(_ptr());
        }

        operator const tracked_ptr<T>&() const noexcept {
            return reinterpret_cast<const tracked_ptr<T>&>(_ptr());
        }

        explicit operator bool() const noexcept {
            return (get() != nullptr);
        }

        template <class U = T, std::enable_if_t<!std::disjunction_v<std::is_void<U>, std::is_array<U>>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        template <class U = T, std::enable_if_t<!std::disjunction_v<std::is_void<U>, std::is_array<U>>, int> = 0>
        U* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        template <class U = T, class E = element_type, std::enable_if_t<std::is_array_v<U>, int> = 0>
        E& operator[](size_t i) const noexcept {
            assert(get() != nullptr);
            assert(i < size());
            return get()[i];
        }

        template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
        size_t size() const noexcept {
            auto array = (Priv::Array_base*)_ptr().base_address();
            return array ? array->count : 0;
        }

        template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
        element_type* begin() const noexcept {
            return get();
        }

        template <class U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
        element_type* end() const noexcept {
            return begin() + size();
        }

        element_type* get() const noexcept {
            return (element_type*)_ptr().load();
        }

        void reset() noexcept {
            _ptr().store(nullptr);
        }

        void swap(root_ptr& p) noexcept {
            auto l = get();
            *this = p.get();
            p = l;
        }

        template <class U = T, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        unique_ptr<T> clone() const {
            return static_cast<const tracked_ptr<T>&>(*this).template clone<U>();
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        bool is() const noexcept {
            return static_cast<const tracked_ptr<T>&>(*this).template is<U>();
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        root_ptr<U> as() const noexcept {
            return static_cast<const tracked_ptr<T>&>(*this).template as<U>();
        }

        const std::type_info& type() const noexcept {
            return static_cast<const tracked_ptr<T>&>(*this).type();
        }

        metadata*& metadata() const noexcept {
            return static_cast<const tracked_ptr<T>&>(*this).metadata();
        }

        constexpr bool is_array() const noexcept {
            return _ptr().template is_array<T>();
        }

    private:
        using Type = T;

        template<class U, std::enable_if_t<std::is_array_v<T> && std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit root_ptr(U* p)
        : root_ptr() {
            _ptr().store(static_cast<element_type*>(p));
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        root_ptr(root_ptr<U>&& r, const void* p) {
            if (!_stack_detected()) {
                if (r._is_heap_root()) {
                    _ref = r._ref;
                    r._ref = nullptr;
                } else {
                    auto ref = Priv::Current_thread().heap_roots_allocator->alloc();
                    ref->store(p);
                    _ref = _set_flag(ref, HeapRootFlag);
                }
            } else {
                _ref = Priv::Current_thread().stack_roots_allocator->alloc(this);
                _ref->store(p);
            }
        }

        bool _is_heap_root() const noexcept {
            return (uintptr_t)_ref & HeapRootFlag;
        }

        Priv::Tracked_ptr& _ptr() noexcept {
            return *_remove_flags(_ref);
        }

        const Priv::Tracked_ptr& _ptr() const noexcept {
            return *_remove_flags(_ref);
        }

        bool _stack_detected() const noexcept {
            uintptr_t this_addr = (uintptr_t)this;
            uintptr_t stack_addr = (uintptr_t)&this_addr;
            ptrdiff_t offset = this_addr - stack_addr;
            return -MaxStackOffset < offset && offset < MaxStackOffset;
        }

        template<class U>
        static U _set_flag(U p, uintptr_t f) noexcept {
            auto v = (uintptr_t)p | f;
            return (U)v;
        }

        template<class U>
        static constexpr U _remove_flags(U p) noexcept {
            auto v = (uintptr_t)p & ClearMask;
            return (U)v;
        }

        bool is_lock_free() const noexcept {
            return _ptr()->is_lock_free();
        }

        Priv::Tracked_ptr* _ref;

        template<class> friend class atomic;
        template<class> friend class root_container;

        template<class U, class V, std::enable_if_t<!std::is_array_v<U> || std::is_same_v<std::remove_cv_t<V>, void>, int>>
        friend root_ptr<U> static_pointer_cast(const root_ptr<V>&) noexcept;

        template<class U, class V, std::enable_if_t<!std::is_array_v<U> || std::is_same_v<std::remove_cv_t<V>, void>, int>>
        friend root_ptr<U> static_pointer_cast(const tracked_ptr<V>&) noexcept;
    };

    template<class T, class U>
    inline bool operator==(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        return a.get() == b.get();
    }

    template<class T>
    inline bool operator==(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return !a;
    }

    template<class T>
    inline bool operator==(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return !a;
    }

    template<class T, class U>
    inline bool operator!=(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        return !(a == b);
    }

    template<class T>
    inline bool operator!=(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return (bool)a;
    }

    template<class T>
    inline bool operator!=(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return (bool)a;
    }

    template<class T, class U>
    inline bool operator<(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        using V = typename std::common_type<T*, U*>::type;
        return std::less<V>()(a.get(), b.get());
    }

    template<class T>
    inline bool operator<(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return std::less<T*>()(a.get(), nullptr);
    }

    template<class T>
    inline bool operator<(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return std::less<T*>()(nullptr, a.get());
    }

    template<class T, class U>
    inline bool operator<=(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        return !(b < a);
    }

    template<class T>
    inline bool operator<=(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return !(nullptr < a);
    }

    template<class T>
    inline bool operator<=(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return !(a < nullptr);
    }

    template<class T, class U>
    inline bool operator>(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        return (b < a);
    }

    template<class T>
    inline bool operator>(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return nullptr < a;
    }

    template<class T>
    inline bool operator>(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return a < nullptr;
    }

    template<class T, class U>
    inline bool operator>=(const root_ptr<T>& a, const root_ptr<U>& b) noexcept {
        return !(a < b);
    }

    template<class T>
    inline bool operator>=(const root_ptr<T>& a, std::nullptr_t) noexcept {
        return !(a < nullptr);
    }

    template<class T>
    inline bool operator>=(std::nullptr_t, const root_ptr<T>& a) noexcept {
        return !(nullptr < a);
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T> || std::is_same_v<std::remove_cv_t<U>, void>, int> = 0>
    inline root_ptr<T> static_pointer_cast(const root_ptr<U>& r) noexcept {
        return root_ptr<T>(static_cast<typename root_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> const_pointer_cast(const root_ptr<U>& r) noexcept {
        return root_ptr<T>(const_cast<typename root_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> dynamic_pointer_cast(const root_ptr<U>& r) noexcept {
        return root_ptr<T>(dynamic_cast<typename root_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> reinterpret_pointer_cast(const root_ptr<U>& r) noexcept {
        return root_ptr<T>(reinterpret_cast<typename root_ptr<T>::element_type*>(r.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const root_ptr<T>& p) {
        s << p.get();
        return s;
    }
}
