//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/array_ptr.h"
#include "detail/pointer.h"
#include "detail/tracked.h"
#include "make_tracked.h"
#include "unique_ptr.h"
#include "types.h"

namespace sgcl {
    template<class T>
    class tracked_ptr : detail::Tracked {
#ifdef SGCL_ARCH_X86_64
        static constexpr uintptr_t StackFlag = uintptr_t(1) << 63;
        static constexpr uintptr_t ExternalHeapFlag = uintptr_t(1) << 62;
#else
        static constexpr uintptr_t StackFlag = 1;
        static constexpr uintptr_t ExternalHeapFlag = 2;
#endif
        static constexpr uintptr_t ClearMask = ~(StackFlag | ExternalHeapFlag);

    public:
        using element_type = T;

        tracked_ptr() noexcept {
            if (!_set_ref_if_not_external_heap()) {
                auto ptr = make_tracked<detail::Pointer>();
                detail::Pointer* ref = ptr.release();
                _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
            }
        };

        constexpr tracked_ptr(std::nullptr_t)
        : tracked_ptr() {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p) noexcept
        : tracked_ptr() {
            assert(!p || !detail::Page::is_unique(p));
            _ptr()->store(static_cast<element_type*>(p));
        }

        tracked_ptr(const tracked_ptr& p) noexcept
        : tracked_ptr() {
            _ptr()->store(p.get());
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(const tracked_ptr<U>& p) noexcept
        : tracked_ptr() {
            _ptr()->store(static_cast<element_type*>(p.get()));
        }

        tracked_ptr(tracked_ptr&& p) noexcept {
            if (!_set_ref_if_not_external_heap()) {
                if (p.allocated_on_external_heap()) {
                    _raw_ptr_ref = p._raw_ptr_ref;
                    p._raw_ptr_ref = _set_flag(decltype(_raw_ptr_ref)(nullptr), ExternalHeapFlag);
                } else {
                    auto ptr = make_tracked<detail::Pointer>();
                    auto ref = ptr.release();
                    _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
                    ref->store(p.get());
                }
            } else {
                _ptr()->store(p.get());
            }
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(tracked_ptr<U>&& p) noexcept {
            if (!_set_ref_if_not_external_heap()) {
                if (p.allocated_on_external_heap()) {
                    _raw_ptr_ref = p._raw_ptr_ref;
                    _ptr()->store_no_update(static_cast<element_type*>(p.get()));
                    p._raw_ptr_ref = _set_flag(decltype(_raw_ptr_ref)(nullptr), ExternalHeapFlag);
                } else {
                    auto ptr = make_tracked<detail::Pointer>();
                    auto ref = ptr.release();
                    _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
                    ref->store(static_cast<element_type*>(p.get()));
                }
            } else {
                _ptr()->store(static_cast<element_type*>(p.get()));
            }
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(unique_ptr<U>&& u) noexcept
        : tracked_ptr() {
            _ptr()->store(static_cast<element_type*>(u.release()));
        }

        ~tracked_ptr() noexcept {
            if (allocated_on_external_heap()) {
                if (auto ptr = _ptr()) {
                    detail::Page::set_state<detail::State::Destroyed>(ptr);
                }
            } else {
                _ptr()->store(nullptr);
            }
        }

        tracked_ptr& operator=(std::nullptr_t) noexcept {
            _ptr()->store(nullptr);
            return *this;
        }

        tracked_ptr& operator=(const tracked_ptr& p) noexcept {
            _ptr()->store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _ptr()->store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(unique_ptr<U>&& u) noexcept {
            auto p = u.release();
            _ptr()->store(static_cast<element_type*>(p));
            return *this;
        }

        tracked_ptr& operator=(tracked_ptr&& p) noexcept {
            if (auto ptr = _ptr()) {
                ptr->store(p.get());
            } else {
                if (p.allocated_on_external_heap()) {
                    _raw_ptr_ref = p._raw_ptr_ref;
                    p._raw_ptr_ref = _set_flag(decltype(_raw_ptr_ref)(nullptr), ExternalHeapFlag);
                } else {
                    auto ptr = make_tracked<detail::Pointer>();
                    auto ref = ptr.release();
                    _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
                    ref->store(p.get());
                }
            }
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept {
            if (auto ptr = _ptr()) {
                ptr->store(static_cast<element_type*>(p.get()));
            } else {
                if (p.allocated_on_external_heap()) {
                    _raw_ptr_ref = p._raw_ptr_ref;
                    _ptr()->store_no_update(static_cast<element_type*>(p.get()));
                    p._raw_ptr_ref = _set_flag(decltype(_raw_ptr_ref)(nullptr), ExternalHeapFlag);
                } else {
                    auto ptr = make_tracked<detail::Pointer>();
                    auto ref = ptr.release();
                    _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
                    ref->store(static_cast<element_type*>(p.get()));
                }
            }
            return *this;
        }

        operator tracked_ptr<void>&() noexcept {
            return *(tracked_ptr<void>*)(this);
        }

        operator const tracked_ptr<void>&() const noexcept {
            return *(const tracked_ptr<void>*)(this);
        }

        explicit operator bool() const noexcept {
            return (get() != nullptr);
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        element_type* get() const noexcept {
            return (element_type*)_ptr()->load();
        }

        void* get_base() const noexcept {
            return _ptr()->data_base_address();
        }

        void reset() noexcept {
            if (allocated_on_external_heap() && !_ptr()) {
                auto ptr = make_tracked<detail::Pointer>();
                auto ref = ptr.release();
                _raw_ptr_ref = _set_flag(ref, ExternalHeapFlag);
            }
            _ptr()->store(nullptr);
        }

        void swap(tracked_ptr& p) noexcept {
            if (allocated_on_external_heap() && p.allocated_on_external_heap()) {
                std::swap(_raw_ptr_ref, p._raw_ptr_ref);
            } else {
                tracked_ptr<element_type> t = *this;
                *this = p;
                p = t;
            }
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        tracked_ptr<U> as() const noexcept {
            if (is<U>()) {
                return tracked_ptr<U>((typename tracked_ptr<U>::element_type*)get_base());
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return _ptr()->template type_info<element_type>();
        }

        template<class M = void>
        M* metadata() const noexcept {
            return (M*)_ptr()->template metadata<element_type>();
        }

        bool is_array() const noexcept {
            return _ptr()->is_array();
        }

        size_t object_size() const noexcept {
            return _ptr()->object_size();
        }

        bool allocated_on_heap() const noexcept {
            return !((uintptr_t)_raw_ptr_ref & (StackFlag | ExternalHeapFlag));
        }

        bool allocated_on_stack() const noexcept {
            return (uintptr_t)_raw_ptr_ref & StackFlag;
        }

        bool allocated_on_external_heap() const noexcept {
            return (uintptr_t)_raw_ptr_ref & ExternalHeapFlag;
        }

    protected:
        static constexpr auto _set_flag(auto p, uintptr_t f) noexcept {
#ifdef SGCL_ARCH_X86_64
            assert(!((uintptr_t)p & ~ClearMask) && "Cannot use SGCL_ARCH_X86_64");
#endif
            auto v = (uintptr_t)p | f;
            return (decltype(p))v;
        }

        static constexpr auto _remove_flags(auto p) noexcept {
            auto v = (uintptr_t)p & ClearMask;
            return (decltype(p))v;
        }

        constexpr bool this_on_stack() const noexcept {
            uintptr_t this_addr = (uintptr_t)this;
            uintptr_t stack_addr = (uintptr_t)&this_addr;
            ptrdiff_t offset = this_addr - stack_addr;
            return std::abs(offset) <= config::MaxOffsetForStackDetection;
        }

        constexpr bool this_on_heap(std::pair<uintptr_t, uintptr_t> state) const noexcept {
            return ((uintptr_t)this - state.first) < state.second;
        }

#ifdef SGCL_ARCH_X86_64
        detail::Pointer* _ptr() noexcept {
#if defined(__GNUC__) || defined(__clang__)
            if (__builtin_expect(allocated_on_heap(), 1)) {
#else
            if (allocated_on_heap()) {
#endif
                return &_raw_ptr;
            }
            return _remove_flags(_raw_ptr_ref);
        }

        const detail::Pointer* _ptr() const noexcept {
#if defined(__GNUC__) || defined(__clang__)
            if (__builtin_expect(allocated_on_heap(), 1)) {
#else
            if (allocated_on_heap()) {
#endif
                return &_raw_ptr;
            }
            return _remove_flags(_raw_ptr_ref);
        }

        union {
            detail::Pointer* _raw_ptr_ref;
            detail::Pointer _raw_ptr;
        };
#else
        detail::Pointer* _ptr() noexcept {
            return _remove_flags(_raw_ptr_ref);
        }

        const detail::Pointer* _ptr() const noexcept {
            return _remove_flags(_raw_ptr_ref);
        }

        detail::Pointer* _raw_ptr_ref;
        union {
            detail::Pointer _raw_ptr;
        };
#endif

    private:
        bool _set_ref_if_not_external_heap() noexcept {
            auto& thread = detail::current_thread();
            if (this_on_stack()) {
                auto ref = thread.stack_allocator->alloc(this);
                _raw_ptr_ref = _set_flag(ref, StackFlag);
            } else {
                if (this_on_heap(thread.alloc_range)) {
#ifndef SGCL_ARCH_X86_64
                    _raw_ptr_ref = &_raw_ptr;
#else
                    assert(!((uintptr_t)&_raw_ptr & ~ClearMask) && "Cannot use SGCL_ARCH_X86_64");
#endif
                    new (&_raw_ptr) detail::Pointer();
                } else {
                    return false;
                }
            }
            return true;
        };

        template<class> friend class atomic;
        template<class> friend class atomic_ref;
        template<class> friend class tracked_ptr;
        template<class> friend class detail::Maker;
    };

    template<class T>
    class tracked_ptr<T[]> : public detail::ArrayPtr<T, tracked_ptr<T>> {
        using Base = detail::ArrayPtr<T, tracked_ptr<T>>;

    public:
        using Base::Base;
        using Base::operator=;
    };

    template <typename T>
    tracked_ptr(T*) -> tracked_ptr<T>;

    template <typename T>
    tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;

    template <typename T>
    tracked_ptr(unique_ptr<T>&&) -> tracked_ptr<T>;

    template<class T, class U>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        using Y = typename std::common_type<decltype(l.get()), decltype(r.get())>::type;
        return static_cast<Y>(l.get()) <=> static_cast<Y>(r.get());
    }

    template<class T, class U>
    inline bool operator==(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        return (l <=> r) == 0;
    }

    template<class T>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return l.get() <=> static_cast<decltype(l.get())>(nullptr);
    }

    template<class T>
    inline bool operator==(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return (l <=> nullptr) == 0;
    }

    template<class T>
    inline std::strong_ordering operator<=>(std::nullptr_t, const tracked_ptr<T>& r) noexcept {
        return static_cast<decltype(r.get())>(nullptr) <=> r.get();
    }

    template<class T>
    inline bool operator==(std::nullptr_t, const tracked_ptr<T>& r) noexcept {
        return (nullptr <=> r) == 0;
    }

    template<class T, class U>
    inline tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(static_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(const_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(dynamic_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p) {
        s << p.get();
        return s;
    }

    template<class T>
    inline void* get_metadata() noexcept {
        return detail::TypeInfo<T>::user_metadata;
    }

    template<class T>
    inline void set_metadata(void* m) noexcept {
        detail::TypeInfo<T>::user_metadata = m;
    }
}

namespace std {
    template<class T>
    struct hash<sgcl::tracked_ptr<T>> {
        std::size_t operator()(const sgcl::tracked_ptr<T>& p) const noexcept {
            return std::hash<T*>{}(p.get());
        }
    };
}
