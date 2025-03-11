//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer.h"
#include "detail/unique_ptr.h"
#include "array_ptr.h"
#include "types.h"

namespace sgcl {
    template<class T>
    class UniquePtr : public std::unique_ptr<T, detail::UniqueDeleter> {
        using Base = std::unique_ptr<T, detail::UniqueDeleter>;

    public:
        using ElementType = typename Base::element_type;
        using ValueType = T;

        using Base::operator=;

        UniquePtr() = default;

        constexpr UniquePtr(std::nullptr_t) noexcept
        :Base(nullptr) {
        };

        template<class U, std::enable_if_t<std::is_convertible_v<typename UniquePtr<U>::ElementType*, T*>, int> = 0>
        UniquePtr(detail::UniquePtr<U>&& p) noexcept
        : Base(p.release()) {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UniquePtr<U>::ElementType*, T*>, int> = 0>
        UniquePtr(UniquePtr<U>&& p) noexcept
        : Base(p.release()) {
        }

        operator UniquePtr<void>&() noexcept {
            return reinterpret_cast<UniquePtr<void>&>(*this);
        }

        operator const UniquePtr<void>&() const noexcept {
            return reinterpret_cast<const UniquePtr<void>&>(*this);
        }

        void* get_base() const noexcept {
            return detail::Pointer::data_base_address_of(this->get());
        }

        UniquePtr clone() const {
            auto p = this->get();
            return (ElementType*)detail::Pointer::clone(p);
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        UniquePtr<U> as() noexcept {
            if (is<U>()) {
                auto base =  detail::Pointer::data_base_address_of(this->release());
                return UniquePtr<U>((typename UniquePtr<U>::ElementType*)base);
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return detail::Pointer::type_info<ValueType>(this->get());
        }

        template<class M = void>
        M* metadata() const noexcept {
            return (M*)detail::Pointer::metadata<ValueType>(this->get());
        }

        constexpr bool is_array() const noexcept {
            return detail::Pointer::is_array(this->get());
        }

        size_t object_size() const noexcept {
            return detail::Pointer::object_size(this->get());
        }

    private:
        UniquePtr(ElementType* p) noexcept
        : Base(p) {
        }

        template<class, PointerPolicy> friend class Pointer;
        template<class> friend class UniquePtr;
        template<class> friend class UnsafePtr;
        template<class U, class V> friend UniquePtr<U> static_pointer_cast(UniquePtr<V>&&) noexcept;
        template<class U, class V> friend UniquePtr<U> dynamic_pointer_cast(UniquePtr<V>&&) noexcept;
        template<class U, class V> friend UniquePtr<U> const_pointer_cast(UniquePtr<V>&&) noexcept;
    };

    template<class T>
    class UniquePtr<T[]> : public ArrayPtr<T, UniquePtr<T>> {
        using Base = ArrayPtr<T, UniquePtr<T>>;

    public:
        using ValueType = T[];
        using Base::Base;

        using Base::operator=;
    };

    template<class T, class U>
    inline UniquePtr<T> static_pointer_cast(UniquePtr<U>&& r) noexcept {
        return UniquePtr<T>(static_cast<typename UniquePtr<T>::ElementType*>(r.release()));
    }

    template<class T, class U>
    inline UniquePtr<T> const_pointer_cast(UniquePtr<U>&& r) noexcept {
        return UniquePtr<T>(const_cast<typename UniquePtr<T>::ElementType*>(r.release()));
    }

    template<class T, class U>
    inline UniquePtr<T> dynamic_pointer_cast(UniquePtr<U>&& r) noexcept {
        return UniquePtr<T>(dynamic_cast<typename UniquePtr<T>::ElementType*>(r.release()));
    }
}
