//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "root_ptr.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"

namespace sgcl {
    template<class T>
    struct unsafe_ptr {
    public:
        using element_type = std::remove_extent_t<T>;

        constexpr unsafe_ptr() = delete;
        unsafe_ptr(const unsafe_ptr& p) = default;

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        unsafe_ptr(const unsafe_ptr<U>& p)
        : _ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        unsafe_ptr(const root_ptr<U>& p)
        : _ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        unsafe_ptr(const tracked_ptr<U>& p)
        : _ptr(p.get()) {
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
            auto array = (Priv::Array_base*)Priv::Tracked_ptr::base_address_of(_ptr);
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
            return _ptr;
        }

        unique_ptr<T> clone() const {
            return (element_type*)Priv::Tracked_ptr::clone(_ptr);
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U, std::enable_if_t<!std::is_array_v<U>, int> = 0>
        root_ptr<U> as() const noexcept {
            if (is<U>()) {
                auto address = Priv::Tracked_ptr::base_address_of(_ptr);
                return root_ptr<U>((U*)address);
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return Priv::Tracked_ptr::type_info<T>(_ptr);
        }

        metadata*& metadata() const noexcept {
            return Priv::Tracked_ptr::metadata<T>(_ptr);
        }

        constexpr bool is_array() const noexcept {
            return Priv::Tracked_ptr::is_array<T>(_ptr);
        }

    private:
        element_type* const _ptr;
    };

    template<class T, class U>
    inline bool operator==(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        return a.get() == b.get();
    }

    template<class T>
    inline bool operator==(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return !a;
    }

    template<class T>
    inline bool operator==(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return !a;
    }

    template<class T, class U>
    inline bool operator!=(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        return !(a == b);
    }

    template<class T>
    inline bool operator!=(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return (bool)a;
    }

    template<class T>
    inline bool operator!=(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return (bool)a;
    }

    template<class T, class U>
    inline bool operator<(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        using V = typename std::common_type<T*, U*>::type;
        return std::less<V>()(a.get(), b.get());
    }

    template<class T>
    inline bool operator<(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return std::less<T*>()(a.get(), nullptr);
    }

    template<class T>
    inline bool operator<(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return std::less<T*>()(nullptr, a.get());
    }

    template<class T, class U>
    inline bool operator<=(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        return !(b < a);
    }

    template<class T>
    inline bool operator<=(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return !(nullptr < a);
    }

    template<class T>
    inline bool operator<=(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return !(a < nullptr);
    }

    template<class T, class U>
    inline bool operator>(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        return (b < a);
    }

    template<class T>
    inline bool operator>(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return nullptr < a;
    }

    template<class T>
    inline bool operator>(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return a < nullptr;
    }

    template<class T, class U>
    inline bool operator>=(const unsafe_ptr<T>& a, const unsafe_ptr<U>& b) noexcept {
        return !(a < b);
    }

    template<class T>
    inline bool operator>=(const unsafe_ptr<T>& a, std::nullptr_t) noexcept {
        return !(a < nullptr);
    }

    template<class T>
    inline bool operator>=(std::nullptr_t, const unsafe_ptr<T>& a) noexcept {
        return !(nullptr < a);
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T> || std::is_same_v<std::remove_cv_t<U>, void>, int> = 0>
    inline unsafe_ptr<T> static_pointer_cast(const unsafe_ptr<U>& r) noexcept {
        return (unsafe_ptr<T>&)static_cast<typename unsafe_ptr<T>::element_type*>(r.get());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unsafe_ptr<T> const_pointer_cast(const unsafe_ptr<U>& r) noexcept {
        return (unsafe_ptr<T>&)const_cast<typename unsafe_ptr<T>::element_type*>(r.get());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unsafe_ptr<T> dynamic_pointer_cast(const unsafe_ptr<U>& r) noexcept {
        return (unsafe_ptr<T>&)dynamic_cast<typename unsafe_ptr<T>::element_type*>(r.get());
    }

    template<class T, class U, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    inline unsafe_ptr<T> reinterpret_pointer_cast(const unsafe_ptr<U>& r) noexcept {
        return (unsafe_ptr<T>&)reinterpret_cast<typename unsafe_ptr<T>::element_type*>(r.get());
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const unsafe_ptr<T>& p) {
        s << p.get();
        return s;
    }
}
