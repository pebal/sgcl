//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "pointer.h"
#include "array_ptr.h"
#include "unique_ptr.h"

namespace sgcl {
    template<class T>
    class UnsafePtr {
    public:
        using ElementType = T;
        using ValueType = T;

        constexpr UnsafePtr() noexcept
        : _raw_ptr(nullptr) {
        }

        constexpr UnsafePtr(std::nullptr_t) noexcept
        : _raw_ptr(nullptr) {
        }

        UnsafePtr(const UnsafePtr&) noexcept = default;
        UnsafePtr(UnsafePtr&&) noexcept = default;

        template<class U, PointerPolicy P, std::enable_if_t<std::is_convertible_v<typename Pointer<U, P>::ElementType*, T*>, int> = 0>
        UnsafePtr(const Pointer<U, P>& p) noexcept
        : _raw_ptr(p.get()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UnsafePtr<U>::ElementType*, T*>, int> = 0>
        UnsafePtr(UnsafePtr<U> p) noexcept
        : _raw_ptr(p.get()) {
        }

        UnsafePtr& operator=(const UnsafePtr&) = default;
        UnsafePtr& operator=(UnsafePtr&&) noexcept = default;

        UnsafePtr& operator=(std::nullptr_t) noexcept {
            _raw_ptr = nullptr;
            return *this;
        }

        template<class U, PointerPolicy P, std::enable_if_t<std::is_convertible_v<typename Pointer<U, P>::ElementType*, T*>, int> = 0>
        UnsafePtr& operator=(const Pointer<U, P>& p) noexcept {
            _raw_ptr = p.get();
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UnsafePtr<U>::ElementType*, T*>, int> = 0>
        UnsafePtr& operator=(UnsafePtr<U> p) noexcept {
            _raw_ptr = p.get();
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

        ElementType* get() const noexcept {
            return _raw_ptr;
        }

        void* get_base() const noexcept {
            return detail::Pointer::data_base_address_of(this->get());
        }

        void reset() noexcept {
            _raw_ptr = nullptr;
        }

        void swap(UnsafePtr& p) noexcept {
            std::swap(_raw_ptr, p._raw_ptr);
        }

        UniquePtr<ValueType> clone() const {
            return (ElementType*)detail::Pointer::clone(_raw_ptr);
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        UnsafePtr<U> as() const noexcept {
            if (is<U>()) {
                return UnsafePtr<U>((typename UnsafePtr<U>::ElementType*)get_base());
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return detail::Pointer::type_info<ValueType>(_raw_ptr);
        }

        template<class M = void>
        M* metadata() const noexcept {
            return (M*)detail::Pointer::metadata<ValueType>(_raw_ptr);
        }

        constexpr bool is_array() const noexcept {
            return detail::Pointer::is_array(_raw_ptr);
        }

        size_t object_size() const noexcept {
            return detail::Pointer::object_size(_raw_ptr);
        }

    protected:
        ElementType* _raw_ptr;

        template<class U>
        constexpr UnsafePtr(U* p) noexcept
        : _raw_ptr(p) {
        }

        template<class> friend class UnsafePtr;
    };

    template<class T>
    class UnsafePtr<T[]> : public ArrayPtr<T, UnsafePtr<T>> {
        using Base = ArrayPtr<T, UnsafePtr<T>>;

    public:
        using ValueType = T[];
        using Base::Base;

        using Base::operator=;
    };

    template<class T, class U>
    inline UnsafePtr<T> static_pointer_cast(UnsafePtr<U> r) noexcept {
        auto p = static_cast<typename UnsafePtr<T>::ElementType*>(r.get());
        return (UnsafePtr<T>&)p;
    }

    template<class T, class U>
    inline UnsafePtr<T> const_pointer_cast(UnsafePtr<U> r) noexcept {
        auto p = const_cast<typename UnsafePtr<T>::ElementType*>(r.get());
        return (UnsafePtr<T>&)p;
    }

    template<class T, class U>
    inline UnsafePtr<T> dynamic_pointer_cast(UnsafePtr<U> r) noexcept {
        auto p = dynamic_cast<typename UnsafePtr<T>::ElementType*>(r.get());
        return (UnsafePtr<T>&)p;
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, UnsafePtr<T> p) {
        s << p.get();
        return s;
    }

    template <class T, PointerPolicy P>
    UnsafePtr(Pointer<T, P>) -> UnsafePtr<T>;
}
