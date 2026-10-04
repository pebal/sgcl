//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer.h"
#include "detail/unique_ptr.h"
#include "types.h"

namespace sgcl {
    template<class T>
    class unique_ptr
    : public detail::UniquePtr<T> {
        using Base = detail::UniquePtr<T>;

    public:
        using element_type = T;
        using deleter_type = typename Base::deleter_type;

        unique_ptr() = default;

        // reset() nulls the word before it runs the deleter (the standard
        // orders it so), which the destructor of std::unique_ptr need not:
        // a dead member leaves its slot's pointer offset null for the next
        // object of the type, constructed on it without any zeroing
        // (maker.h: _init), as a tracked_ptr's destructor does.
        SGCL_INLINE_HOT ~unique_ptr() noexcept {
            this->reset();
        }

        unique_ptr(unique_ptr&&) noexcept = default;
        unique_ptr& operator=(unique_ptr&&) noexcept = default;

        SGCL_INLINE_HOT constexpr unique_ptr(std::nullptr_t) noexcept
        :Base(nullptr) {
        };

        // From a pointer to a derived class, the ownership moved (a
        // unique_ptr<U> is a std::unique_ptr<U, deleter_type>)
        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        SGCL_INLINE_HOT unique_ptr(std::unique_ptr<U, deleter_type>&& p) noexcept
        : Base(static_cast<element_type*>(p.release())) {
        }

        // The assignments return this unique_ptr, as std's return theirs:
        // of a pointer to a derived class, the ownership moved, and of null
        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        SGCL_INLINE_HOT unique_ptr& operator=(std::unique_ptr<U, deleter_type>&& p) noexcept {
            Base::operator=(std::move(p));
            return *this;
        }

        SGCL_INLINE_HOT unique_ptr& operator=(std::nullptr_t) noexcept {
            Base::operator=(nullptr);
            return *this;
        }

        // The same word as a unique_ptr<void>, for the containers
        SGCL_INLINE_HOT operator unique_ptr<void>&() noexcept {
            return *(unique_ptr<void>*)(this);
        }

        SGCL_INLINE_HOT operator const unique_ptr<void>&() const noexcept {
            return *(const unique_ptr<void>*)(this);
        }

        // The dynamic type from the page, as for tracked_ptr (is<U>() false
        // when empty); as<U>() moves the ownership into the result (null,
        // and nothing moved, when empty or the object is not a U)
        template<class U>
        SGCL_INLINE_HOT bool is() const noexcept {
            return detail::Pointer::type_info<detail::NoObject>(this->get()) == typeid(U);
        }

        template<class U>
        SGCL_INLINE_HOT unique_ptr<U> as() noexcept {
            if (this->get() && is<U>()) {   // the null test first: it drops is<U>()'s null arm, the code as before
                auto base =  detail::Pointer::data_base_address_of(this->release());
                return unique_ptr<U>((typename unique_ptr<U>::element_type*)base);
            } else {
                return {nullptr};
            }
        }

        SGCL_INLINE_HOT const std::type_info& type() const noexcept {
            return detail::Pointer::type_info<element_type>(this->get());
        }

    private:
        SGCL_INLINE_HOT unique_ptr(element_type* p) noexcept
        : Base(p) {
        }

        template<class> friend class tracked_ptr;
        template<class> friend class unique_ptr;
        template<class U, class V> friend unique_ptr<U> static_pointer_cast(unique_ptr<V>&&) noexcept;
        template<class U, class V> friend unique_ptr<U> dynamic_pointer_cast(unique_ptr<V>&&) noexcept;
        template<class U, class V> friend unique_ptr<U> const_pointer_cast(unique_ptr<V>&&) noexcept;
    };

    // Arrays are not a public type (see tracked_ptr.h).
    template<class T>
    class unique_ptr<T[]>;

    // The casts, moving the ownership: a unique_ptr has one owner
    template<class T, class U>
    SGCL_INLINE_HOT unique_ptr<T> static_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(static_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }

    template<class T, class U>
    SGCL_INLINE_HOT unique_ptr<T> const_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(const_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }

    template<class T, class U>
    SGCL_INLINE_HOT unique_ptr<T> dynamic_pointer_cast(unique_ptr<U>&& r) noexcept {
        return unique_ptr<T>(dynamic_cast<typename unique_ptr<T>::element_type*>(r.release()));
    }
}

namespace std {
    template<class T>
    struct hash<sgcl::unique_ptr<T>> {
        SGCL_INLINE_HOT std::size_t operator()(const sgcl::unique_ptr<T>& p) const noexcept {
            return std::hash<T*>{}(p.get());
        }
    };
}
