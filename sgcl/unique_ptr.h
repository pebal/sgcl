//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/array_ptr.h"
#include "detail/pointer.h"
#include "detail/unique_ptr.h"
#include "types.h"

namespace sgcl {
    template<class T>
    class unique_ptr : public detail::UniquePtr<T> {
        using Base = detail::UniquePtr<T>;

    public:
        using element_type = T;
        using Base::operator=;

        unique_ptr() = default;

        constexpr unique_ptr(std::nullptr_t) noexcept
        :Base(nullptr) {
        };

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, T*>, int> = 0>
        unique_ptr(detail::UniquePtr<U>&& p) noexcept
        : Base(p.release()) {
        }

        operator unique_ptr<void>&() noexcept {
            return reinterpret_cast<unique_ptr<void>&>(*this);
        }

        operator const unique_ptr<void>&() const noexcept {
            return reinterpret_cast<const unique_ptr<void>&>(*this);
        }

        void* get_base() const noexcept {
            return detail::Pointer::data_base_address_of(this->get());
        }

        unique_ptr clone() const {
            auto p = this->get();
            return (element_type*)detail::Pointer::clone(p);
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        unique_ptr<U> as() noexcept {
            if (is<U>()) {
                auto base =  detail::Pointer::data_base_address_of(this->release());
                return unique_ptr<U>((typename unique_ptr<U>::element_type*)base);
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return detail::Pointer::type_info<element_type>(this->get());
        }

        template<class M = void>
        M* metadata() const noexcept {
            return (M*)detail::Pointer::metadata<element_type>(this->get());
        }

        constexpr bool is_array() const noexcept {
            return detail::Pointer::is_array(this->get());
        }

        size_t object_size() const noexcept {
            return detail::Pointer::object_size(this->get());
        }

    private:
        unique_ptr(element_type* p) noexcept
        : Base(p) {
        }

        template<class> friend class tracked_ptr;
        template<class> friend class unique_ptr;
        template<class U, class V> friend unique_ptr<U> static_pointer_cast(unique_ptr<V>&&) noexcept;
        template<class U, class V> friend unique_ptr<U> dynamic_pointer_cast(unique_ptr<V>&&) noexcept;
        template<class U, class V> friend unique_ptr<U> const_pointer_cast(unique_ptr<V>&&) noexcept;
    };

    template<class T>
    class unique_ptr<T[]> : public detail::ArrayPtr<T, unique_ptr<T>> {
        using Base = detail::ArrayPtr<T, unique_ptr<T>>;

    public:
        using Base::Base;
        using Base::operator=;

        unique_ptr<T[]> clone() const {
            return unique_ptr<T[]>(unique_ptr<T>::clone());
        }
    };

    template<class T, class U>
    inline unique_ptr<T> static_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(static_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }

    template<class T, class U>
    inline unique_ptr<T> const_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(const_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }

    template<class T, class U>
    inline unique_ptr<T> dynamic_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(dynamic_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }
}
