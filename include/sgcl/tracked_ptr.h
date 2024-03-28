//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "priv/tracked.h"
#include "priv/tracked_ptr.h"
#include "unique_ptr.h"
#include "types.h"

namespace sgcl {    
    template<class T>
    class tracked_ptr : Priv::Tracked {
    public:
        using element_type = std::remove_extent_t<T>;

        constexpr tracked_ptr() {};
        constexpr tracked_ptr(std::nullptr_t) {}

        template<class U, std::enable_if_t<!std::is_array_v<T> && std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p)
        : _val(static_cast<element_type*>(p)) {
        }

        tracked_ptr(const tracked_ptr& p)
        : tracked_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        tracked_ptr(const root_ptr<U>& p)
        : tracked_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        tracked_ptr(const tracked_ptr<U>& p)
        : tracked_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        explicit tracked_ptr(unique_ptr<U>&& u) {
            auto p = u.release();
            _ptr().force_store(static_cast<element_type*>(p));
        }

        ~tracked_ptr() {
            _ptr().store(nullptr);
        }

        tracked_ptr& operator=(std::nullptr_t) noexcept {
            _ptr().store(nullptr);
            return *this;
        }

        tracked_ptr& operator=(const tracked_ptr& p) noexcept {
            _ptr().store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        tracked_ptr& operator=(const root_ptr<U>& p) noexcept {
            _ptr().store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _ptr().store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        tracked_ptr& operator=(unique_ptr<U>&& u) noexcept {
            auto p = u.release();
            _ptr().force_store(static_cast<element_type*>(p));
            return *this;
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

        void swap(tracked_ptr& p) noexcept {
            auto l = get();
            *this = p.get();
            p = l;
        }

        unique_ptr<T> clone() const {
            return (element_type*)_ptr().clone();
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        root_ptr<U> as() const noexcept {
            if (is<U>()) {
                auto address = _ptr().base_address();
                return root_ptr<U>((U*)address);
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return _ptr().template type_info<T>();
        }

        metadata*& metadata() const noexcept {
            return _ptr().template metadata<std::remove_cv_t<T>>();
        }

        constexpr bool is_array() const noexcept {
            return _ptr().template is_array<T>();
        }

    private:
        using Type = T;

        template<class U, std::enable_if_t<std::is_array_v<T> && std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p)
        : _val(static_cast<element_type*>(p)) {
        }

        Priv::Tracked_ptr& _ptr() noexcept {
            return _val;
        }

        const Priv::Tracked_ptr& _ptr() const noexcept {
            return _val;
        }

        Priv::Tracked_ptr _val;

        template<class> friend class atomic;
        template<class> friend class root_ptr;
        template<class> friend class tracked_container;
    };

    template<class T, class U>
    inline bool operator==(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        return a.get() == b.get();
    }

    template<class T>
    inline bool operator==(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return !a;
    }

    template<class T>
    inline bool operator==(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return !a;
    }

    template<class T, class U>
    inline bool operator!=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        return !(a == b);
    }

    template<class T>
    inline bool operator!=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return (bool)a;
    }

    template<class T>
    inline bool operator!=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return (bool)a;
    }

    template<class T, class U>
    inline bool operator<(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        using V = typename std::common_type<T*, U*>::type;
        return std::less<V>()(a.get(), b.get());
    }

    template<class T>
    inline bool operator<(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return std::less<T*>()(a.get(), nullptr);
    }

    template<class T>
    inline bool operator<(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return std::less<T*>()(nullptr, a.get());
    }

    template<class T, class U>
    inline bool operator<=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        return !(b < a);
    }

    template<class T>
    inline bool operator<=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return !(nullptr < a);
    }

    template<class T>
    inline bool operator<=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return !(a < nullptr);
    }

    template<class T, class U>
    inline bool operator>(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        return (b < a);
    }

    template<class T>
    inline bool operator>(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return nullptr < a;
    }

    template<class T>
    inline bool operator>(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return a < nullptr;
    }

    template<class T, class U>
    inline bool operator>=(const tracked_ptr<T>& a, const tracked_ptr<U>& b) noexcept {
        return !(a < b);
    }

    template<class T>
    inline bool operator>=(const tracked_ptr<T>& a, std::nullptr_t) noexcept {
        return !(a < nullptr);
    }

    template<class T>
    inline bool operator>=(std::nullptr_t, const tracked_ptr<T>& a) noexcept {
        return !(nullptr < a);
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T> || std::is_same_v<std::remove_cv_t<U>, void>, int> = 0>
    inline root_ptr<T> static_pointer_cast(const tracked_ptr<U>& r) noexcept {
        return root_ptr<T>(static_cast<typename tracked_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> const_pointer_cast(const tracked_ptr<U>& r) noexcept {
        return root_ptr<T>(const_cast<typename tracked_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& r) noexcept {
        return root_ptr<T>(dynamic_cast<typename tracked_ptr<T>::element_type*>(r.get()));
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline root_ptr<T> reinterpret_pointer_cast(const tracked_ptr<U>& r) noexcept {
        return root_ptr<T>(reinterpret_cast<typename tracked_ptr<T>::element_type*>(r.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p) {
        s << p.get();
        return s;
    }
}
