//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_metadata.h"
#include "thread.h"

namespace sgcl::detail {
    class Pointer {
    public:
        Pointer() noexcept {
#if !defined(NDEBUG)
            if (_val == size_t(1)) {
#else
            if (_val) {
#endif
                static_assert(sizeof(Pointer) == sizeof(RawPointer));
                auto& thread = current_thread();
                auto& pointers = thread.child_pointers;
                assert(pointers.map && "Objects that are not roots cannot be allocated on the stack or unmanaged heap");
                auto offset = ((uintptr_t)&_ptr - pointers.base) / sizeof(Pointer);
                assert(offset / 8 < pointers.map->size());
                (*pointers.map)[offset / 8].fetch_or(1 << (offset % 8), std::memory_order_relaxed);
                store(nullptr);
            }
            else {
#if !defined(NDEBUG)
                assert(_val == 0 && "Objects that are not roots cannot be allocated on the stack or unmanaged heap");
#endif
            }
        }

        Pointer(std::nullptr_t) noexcept
        : Pointer() {
        }

        Pointer(const void* p) noexcept
        : Pointer() {
            store(p);
        }

        Pointer(const Pointer& p) noexcept
        : Pointer() {
            store(p.load());
        }

        Pointer& operator=(const Pointer& p) noexcept {
            store(p.load());
            return *this;
        }

        void* load() const noexcept {
            return _ptr.load(std::memory_order_relaxed);
        }

        void* load(const std::memory_order m) const noexcept {
            return _ptr.load(m);
        }

        void store(std::nullptr_t) noexcept {
            _ptr.store(nullptr, std::memory_order_release);
        }

        void store(const void* p) noexcept {
            store_no_update(p);
            _update(p);
        }

        void store(std::nullptr_t, const std::memory_order m) noexcept {
            _ptr.store(nullptr, m);
        }

        void store(const void* p, const std::memory_order m) noexcept {
            _ptr.store(const_cast<void*>(p), m);
            _update(p);
        }

        void store_no_update(const void* p) noexcept {
            _ptr.store(const_cast<void*>(p), std::memory_order_release);
        }

        bool compare_exchange_strong(void*& o, std::nullptr_t, const std::memory_order m) noexcept {
            return _ptr.compare_exchange_strong(o, nullptr, m);
        }

        bool compare_exchange_strong(void*& o, const void* n, const std::memory_order m) noexcept {
            auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), m);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_strong(void*& o, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            return _ptr.compare_exchange_strong(o, nullptr, s, f);
        }

        bool compare_exchange_strong(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
            auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), s, f);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_weak(void*& o, std::nullptr_t, const std::memory_order m) noexcept {
            return _ptr.compare_exchange_weak(o, nullptr, m);
        }

        bool compare_exchange_weak(void*& o, const void* n, const std::memory_order m) noexcept {
            auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), m);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_weak(void*& o, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            return _ptr.compare_exchange_weak(o, nullptr, s, f);
        }

        bool compare_exchange_weak(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
            auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), s, f);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool is_lock_free() const noexcept {
            return _ptr.is_lock_free();
        }

        void notify_one() noexcept {
            _ptr.notify_one();
        }

        void notify_all() noexcept {
            _ptr.notify_all();
        }

        void wait(const void* p, std::memory_order m) const noexcept {
            _ptr.wait(const_cast<void*>(p), m);
        }

        inline static void* base_address_of(const void* p) noexcept {
            return p ? Page::base_address_of(p) : nullptr;
        }

        void* base_address() const noexcept {
            auto p = load();
            return p ? base_address_of(p) : nullptr;
        }

        inline static void* data_base_address_of(const void* p) noexcept {
            auto data = base_address_of(p);
            return Page::metadata_of(p).is_array ? ((ArrayBase*)data) + 1 : data;
        }

        void* data_base_address() const noexcept {
            auto p = load();
            return p ? data_base_address_of(p) : nullptr;
        }

        template<class T>
        inline static void* metadata(const void* p) noexcept {
            using Info = TypeInfo<T>;
            if (p) {
                auto metadata = Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (ArrayBase*)Page::base_address_of(p);
                    auto metadata = array->metadata.load(std::memory_order_relaxed);
                    return metadata->user_metadata;
                } else {
                    return metadata.user_metadata;
                }
            } else {
                return Info::user_metadata;
            }
        }

        template<class T>
        void* metadata() const noexcept {
            auto p = load();
            return metadata<T>(p);
        }

        template<class T>
        inline static const std::type_info& type_info(const void* p) noexcept {
            if (p) {
                auto metadata = Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (ArrayBase*)Page::base_address_of(p);
                    auto metadata = array->metadata.load(std::memory_order_relaxed);
                    return metadata->type_info;
                } else {
                    return Page::metadata_of(p).type_info;
                }
            } else {
                return typeid(T);
            }
        }

        template<class T>
        const std::type_info& type_info() const noexcept {
            auto p = load();
            return type_info<T>(p);
        }

        inline static bool is_array(const void* p) noexcept {
            return p ? Page::metadata_of(p).is_array : false;
        }

        bool is_array() const noexcept {
            auto p = load();
            return is_array(p);
        }

        inline static size_t object_size(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    auto metadata = array->metadata.load(std::memory_order_relaxed);
                    return metadata->object_size;
                } else {
                    return metadata.object_size;
                }
            }
            return 0;
        }

        size_t object_size() const noexcept {
            auto p = load();
            return object_size(p);
        }

        inline static size_t size(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return array->size;
                } else {
                    return 1;
                }
            }
            return 0;
        }

        inline static size_t* size_ptr(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return &array->size;
                } else {
                    return nullptr;
                }
            }
            return nullptr;
        }

        inline static size_t capacity(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return array->capacity;
                } else {
                    return 1;
                }
            }
            return 0;
        }

        inline static const size_t* capacity_ptr(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return &array->capacity;
                } else {
                    return nullptr;
                }
            }
            return nullptr;
        }

    private:
        static void _update(const void* p) noexcept {
            if (p) {
                Page::set_state<State::Reachable>(p);
            }
        }

        union {
            size_t _val;
            RawPointer _ptr;
        };
    };
}
