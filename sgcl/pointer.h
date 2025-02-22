//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer.h"
#include "detail/tracked.h"
#include "array_ptr.h"
#include "concepts.h"
#include "unique_ptr.h"
#include "unsafe_ptr.h"
#include "types.h"

namespace sgcl {
    template<class T, PointerPolicy Policy>
    class VoidPointer : detail::Tracked {
    public:
        operator Pointer<void, PointerPolicy::Tracked>&() noexcept {
            auto p = static_cast<Pointer<T, Policy>*>(this);
            return reinterpret_cast<Pointer<void, PointerPolicy::Tracked>&>(p->_ptr());
        }

        operator const Pointer<void, PointerPolicy::Tracked>&() const noexcept {
            auto p = static_cast<const Pointer<T, Policy>*>(this);
            return reinterpret_cast<const Pointer<void, PointerPolicy::Tracked>&>(p->_ptr());
        }
    };

    template<PointerPolicy Policy>
    class VoidPointer<void, Policy> : detail::Tracked {
    };

    template<class T, PointerPolicy Policy>
    class Pointer : public VoidPointer<T, Policy> {
    public:
        using element_type = T;

        constexpr Pointer() noexcept {
            if constexpr(Policy == PointerPolicy::Stack) {
                _raw_ptr = (detail::Pointer*)detail::current_thread().stack_allocator->alloc(this);
            }
        };

        constexpr Pointer(std::nullptr_t) noexcept
        : Pointer() {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
        explicit Pointer(U* p) noexcept
        : Pointer() {
            assert(!p || !detail::Page::is_unique(p));
            assert(!p || !detail::Pointer::is_array(p));
            _ptr().store(static_cast<T*>(p));
        }

        explicit(Policy == PointerPolicy::Tracked) Pointer(const Pointer& p) noexcept
        : Pointer() {
            _ptr().store(p.get());
        }

        template<class U, PointerPolicy P, std::enable_if_t<std::is_convertible_v<typename Pointer<U, P>::element_type*, T*>, int> = 0>
        explicit(Policy == PointerPolicy::Tracked) Pointer(const Pointer<U, P>& p) noexcept
        : Pointer() {
            _ptr().store(static_cast<T*>(p.get()));
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UniquePtr<U>::element_type*, T*>, int> = 0>
        explicit(Policy == PointerPolicy::Tracked) Pointer(UniquePtr<U>&& u) noexcept
        : Pointer() {
            _ptr().store(static_cast<T*>(u.release()));
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UnsafePtr<U>::element_type*, T*>, int> = 0>
        explicit(Policy == PointerPolicy::Tracked) Pointer(const UnsafePtr<U>& p) noexcept
        : Pointer() {
            assert(!detail::Page::is_unique(p.get()));
            _ptr().store(static_cast<T*>(p.get()));
        }

        ~Pointer() noexcept {
            _ptr().store(nullptr);
        }

        Pointer& operator=(std::nullptr_t) noexcept {
            _ptr().store(nullptr);
            return *this;
        }

        Pointer& operator=(const Pointer& p) noexcept {
            _ptr().store(p.get());
            return *this;
        }

        template<class U, PointerPolicy P, std::enable_if_t<std::is_convertible_v<typename Pointer<U, P>::element_type*, T*>, int> = 0>
        Pointer& operator=(const Pointer<U, P>& p) noexcept {
            _ptr().store(static_cast<T*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UniquePtr<U>::element_type*, T*>, int> = 0>
        Pointer& operator=(UniquePtr<U>&& u) noexcept {
            auto p = u.release();
            _ptr().store(static_cast<T*>(p));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename UnsafePtr<U>::element_type*, T*>, int> = 0>
        Pointer& operator=(const UnsafePtr<U>& p) noexcept {
            _ptr().store(static_cast<T*>(p.get()));
            return *this;
        }

        operator Pointer<T, PointerPolicy::Tracked>&() noexcept {
            return reinterpret_cast<Pointer<T, PointerPolicy::Tracked>&>(_ptr());
        }

        operator const Pointer<T, PointerPolicy::Tracked>&() const noexcept {
            return reinterpret_cast<const Pointer<T, PointerPolicy::Tracked>&>(_ptr());
        }

        explicit operator bool() const noexcept {
            return (get() != nullptr);
        }

        template <class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        template <class U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        T* get() const noexcept {
            return (T*)_ptr().load();
        }

        void* get_base() const noexcept {
            return _ptr().data_base_address();
        }

        void reset() noexcept {
            _ptr().store(nullptr);
        }

        template<PointerPolicy P>
        void swap(Pointer<T, P>& p) noexcept {
            Pointer<T, PointerPolicy::Stack> t = *this;
            *this = p;
            p = t;
        }

        UniquePtr<T> clone() const {
            return (T*)_ptr().clone();
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        Pointer<U, PointerPolicy::Stack> as() const noexcept {
            if (is<U>()) {
                return Pointer<U, PointerPolicy::Stack>((typename Pointer<U, PointerPolicy::Stack>::element_type*)get_base());
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return _ptr().template type_info<T>();
        }

        template<class M = void>
        M* metadata() const noexcept {
            return (M*)_ptr().template metadata<T>();
        }

        constexpr bool is_array() const noexcept {
            return _ptr().is_array();
        }

        size_t object_size() const noexcept {
            return _ptr().object_size();
        }

    protected:
        detail::Pointer& _ptr() noexcept {
            if constexpr(Policy == PointerPolicy::Tracked) {
                return _raw_ptr;
            } else {
                return *_raw_ptr;
            }
        }

        const detail::Pointer& _ptr() const noexcept {
            if constexpr(Policy == PointerPolicy::Tracked) {
                return _raw_ptr;
            } else {
                return *_raw_ptr;
            }
        }

        std::conditional_t<Policy == PointerPolicy::Tracked, detail::Pointer, detail::Pointer*> _raw_ptr;

        template<TrackedPointer> friend class Atomic;
        template<class, PointerPolicy> friend class VoidPointer;
        //template<class, RootPolicy> friend class root_ptr;
    };

    template<class T, PointerPolicy Policy>
    class Pointer<T[], Policy> : public ArrayPtr<T, Pointer<T, Policy>> {
        using Base = ArrayPtr<T, Pointer<T, Policy>>;

    public:
        using Base::Base;
        using Base::operator=;

        operator Pointer<T[], PointerPolicy::Tracked>&() noexcept {
            return reinterpret_cast<Pointer<T[], PointerPolicy::Tracked>&>(this->_ptr());
        }

        operator const Pointer<T[], PointerPolicy::Tracked>&() const noexcept {
            return reinterpret_cast<const Pointer<T[], PointerPolicy::Tracked>&>(this->_ptr());
        }
    };

    template<IsPointer T, IsPointer U>
    inline auto operator<=>(const T& l, const U& r) noexcept {
        using Y = typename std::common_type<decltype(l.get()), decltype(r.get())>::type;
        return static_cast<Y>(l.get()) <=> static_cast<Y>(r.get());
    }
    template<IsPointer T, IsPointer U>
    inline auto operator==(const T& l, const U& r) noexcept {
        return (l <=> r) == 0;
    }
    template<IsPointer T, IsPointer U>
    inline auto operator!=(const T& l, const U& r) noexcept {
        return (l <=> r) != 0;
    }
    template<IsPointer T, IsPointer U>
    inline auto operator<(const T& l, const U& r) noexcept {
        return (l <=> r) < 0;
    }
    template<IsPointer T, IsPointer U>
    inline auto operator<=(const T& l, const U& r) noexcept {
        return (l <=> r) <= 0;
    }
    template<IsPointer T, IsPointer U>
    inline auto operator>(const T& l, const U& r) noexcept {
        return (l <=> r) > 0;
    }
    template<IsPointer T, IsPointer U>
    inline auto operator>=(const T& l, const U& r) noexcept {
        return (l <=> r) >= 0;
    }

    template<IsPointer T>
    inline auto operator<=>(const T& l, std::nullptr_t) noexcept {
        return l.get() <=> static_cast<decltype(l.get())>(nullptr);
    }
    template<IsPointer T>
    inline bool operator==(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) == 0;
    }
    template<IsPointer T>
    inline bool operator!=(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) != 0;
    }
    template<IsPointer T>
    inline bool operator<(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) < 0;
    }
    template<IsPointer T>
    inline bool operator<=(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) <= 0;
    }
    template<IsPointer T>
    inline bool operator>(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) > 0;
    }
    template<IsPointer T>
    inline bool operator>=(const T& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) >= 0;
    }

    template<IsPointer T>
    inline auto operator<=>(std::nullptr_t, const T& r) noexcept {
        return static_cast<decltype(r.get())>(nullptr) <=> r.get();
    }
    template<IsPointer T>
    inline bool operator==(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) == 0;
    }
    template<IsPointer T>
    inline bool operator!=(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) != 0;
    }
    template<IsPointer T>
    inline bool operator<(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) < 0;
    }
    template<IsPointer T>
    inline bool operator<=(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) <= 0;
    }
    template<IsPointer T>
    inline bool operator>(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) > 0;
    }
    template<IsPointer T>
    inline bool operator>=(std::nullptr_t, const T& r) noexcept {
        return (nullptr <=> r) >= 0;
    }

    template<class T, class U, PointerPolicy P>
    inline Pointer<T, PointerPolicy::Stack> static_pointer_cast(const Pointer<U, P>& p) noexcept {
        return Pointer<T, PointerPolicy::Stack>(static_cast<typename Pointer<T, P>::element_type*>(p.get()));
    }

    template<class T, class U, PointerPolicy P>
    inline Pointer<T, PointerPolicy::Stack> const_pointer_cast(const Pointer<U, P>& p) noexcept {
        return Pointer<T, PointerPolicy::Stack>(const_cast<typename Pointer<T, P>::element_type*>(p.get()));
    }

    template<class T, class U, PointerPolicy P>
    inline Pointer<T, PointerPolicy::Stack> dynamic_pointer_cast(const Pointer<U, P>& p) noexcept {
        return Pointer<T, PointerPolicy::Stack>(dynamic_cast<typename Pointer<T, P>::element_type*>(p.get()));
    }

    template<class T, PointerPolicy P>
    std::ostream& operator<<(std::ostream& s, const Pointer<T, P>& p) {
        s << p.get();
        return s;
    }

    template<class T>
    inline void* get_metadata() {
        return detail::TypeInfo<T>::user_metadata;
    }

    template<class T>
    inline void set_metadata(void* m) {
        detail::TypeInfo<T>::user_metadata = m;
    }
}
