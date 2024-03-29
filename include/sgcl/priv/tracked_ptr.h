//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array_metadata.h"
#include "types.h"
#include "thread.h"

#include <cassert>

namespace sgcl {
    namespace Priv {
        class Tracked_ptr {
        public:
            Tracked_ptr() {
#if !defined(NDEBUG) || defined(SGCL_DEBUG)
                if (_val == std::numeric_limits<size_t>::max()) {
#else
                if (_val) {
#endif
                    auto& thread = Current_thread();
                    auto& pointers = thread.child_pointers;
                    assert(pointers.map && "Objects that are not roots cannot be allocated on the stack or unmanaged heap");
                    auto offset = ((uintptr_t)&_ptr - pointers.base) / sizeof(Pointer);
                    (*pointers.map)[offset / 8].fetch_or(1 << (offset % 8), std::memory_order_relaxed);
                    store(nullptr);
                }
                else {
#if !defined(NDEBUG) || defined(SGCL_DEBUG)
                    assert(_val == 0 && "Objects that are not roots cannot be allocated on the stack or unmanaged heap");
#endif
                }
            }

            Tracked_ptr(std::nullptr_t)
            : Tracked_ptr() {
            }

            Tracked_ptr(const void* p)
            : Tracked_ptr() {
                store(p);
            }

            Tracked_ptr(const Tracked_ptr& p)
            : Tracked_ptr() {
                store(p.load());
            }

            Tracked_ptr& operator=(const Tracked_ptr& p) {
                store(p.load());
                return *this;
            }

            void* load() const noexcept {
                return _ptr.load(std::memory_order_relaxed);
            }

            void* load_atomic(const std::memory_order m) const noexcept {
                auto p = _ptr.load(m);
                _update_atomic(p);
                return p;
            }

            void update_atomic() noexcept {
                auto p = _ptr.load(std::memory_order_relaxed);
                _update_atomic(p);
            }

            void store(std::nullptr_t) noexcept {
                _ptr.store(nullptr, std::memory_order_relaxed);
            }

            void store(const void* p) noexcept {
                store_no_update(p);
                _update(p);
            }

            void store_atomic(const void* p) noexcept {
                store_no_update(p);
                _update_atomic(p);
            }

            void store(const void* p, const std::memory_order m) noexcept {
                _ptr.store(const_cast<void*>(p), m);
                _update(p);
            }

            void store_no_update(const void* p) noexcept {
                _ptr.store(const_cast<void*>(p), std::memory_order_release);
            }

            void force_store(const void* p) noexcept {
                store_no_update(p);
                _force_update(p);
            }

            void* exchange(const void* p, const std::memory_order m) noexcept {
                auto l = _ptr.exchange(const_cast<void*>(p), m);
                _update(p);
                return l;
            }

            bool compare_exchange_strong(void*& o, const void* n, const std::memory_order m) noexcept {
                auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), m);
                _update(n);
                return res;
            }

            bool compare_exchange_strong(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
                auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), s, f);
                _update(n);
                return res;
            }

            bool compare_exchange_weak(void*& o, const void* n, const std::memory_order m) noexcept {
                auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), m);
                _update(n);
                return res;
            }

            bool compare_exchange_weak(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
                auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), s, f);
                _update(n);
                return res;
            }

            inline static void* base_address_of(void* p) noexcept {
                return p ? Page::base_address_of(p) : nullptr;
            }

            void* base_address() const noexcept {
                auto p = load();
                return p ? base_address_of(p) : nullptr;
            }

            bool is_lock_free() const noexcept {
                return _ptr.is_lock_free();
            }

            template<class T>
            inline static sgcl::metadata*& metadata(void* p) noexcept {
                using Info = Type_info<T>;
                if (p) {
                    auto metadata = Page::metadata_of(p);
                    if (metadata.is_array) {
                        auto array = (Array_base*)Page::base_address_of(p);
                        auto metadata = array->metadata.load(std::memory_order_relaxed);
                        return metadata->user_metadata;
                    } else {
                        return metadata.user_metadata;
                    }
                } else {
                    return Info::user_metadata();
                }
            }

            template<class T>
            sgcl::metadata*& metadata() const noexcept {
                auto p = load();
                return metadata<T>(p);
            }

            template<class T>
            inline static const std::type_info& type_info(void* p) noexcept {
                if (p) {
                    auto metadata = Page::metadata_of(p);
                    if (metadata.is_array) {
                        auto array = (Array_base*)Page::base_address_of(p);
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

            inline static void* clone(void* p) {
                if (p) {
                    auto metadata = Page::metadata_of(p);
                    if (metadata.is_array) {
                        auto array = (Array_base*)Page::base_address_of(p);
                        auto metadata = array->metadata.load(std::memory_order_relaxed);
                        auto c = metadata->clone(p);
                        return (void*)c;
                    } else {
                        auto c = metadata.clone(p);
                        return (void*)c;
                    }
                } else {
                    return nullptr;
                }
            }

            void* clone() const {
                auto p = load();
                return clone(p);
            }

            template<class T>
            inline static constexpr bool is_array(void* p) noexcept {
                using Type = std::remove_cv_t<T>;
                if constexpr(std::is_same_v<Type, void>) {
                    if (p) {
                        return Page::metadata_of(p).is_array;
                    } else {
                        return false;
                    }
                } else {
                    return std::is_array_v<T>;
                }
            }

            template<class T>
            constexpr bool is_array() const noexcept {
                auto p = load();
                return is_array<T>(p);
            }

        private:
            static void _force_update(const void* p) noexcept {
                if (p) {
                    Page::set_state(p, State::Reachable);
                }
            }

            static void _update(const void* p) noexcept {
                if (p) {
                    Page::update_state(p, State::Reachable);
                }
            }

            static void _update_atomic(const void* p) noexcept {
                if (p) {
                    Page::set_state(p, State::ReachableAtomic);
                }
            }

            union {
                size_t _val;
                Pointer _ptr;
            };
        };

        template<class T>
        void* Clone(const void* p) {
            using element_type = std::remove_extent_t<T>;
            if constexpr (std::is_copy_constructible_v<element_type>) {
                if (p) {
                    if constexpr(std::is_array_v<T>) {
                        if constexpr(std::is_copy_assignable_v<element_type>) {
                            auto array = (Array_base*)Page::base_address_of(p);
                            auto src = (const element_type*)(p);
                            auto dst = Maker<T>::make_tracked(array->count);
                            for (size_t i = 0; i < array->count; ++i) {
                                dst[i] = src[i];
                            }
                            return dst.release();
                        } else {
                            assert(!"[sgcl] clone(): no copy assignable");
                            return nullptr;
                        }
                    } else {
                        auto c = Maker<T>::make_tracked(*((const T*)p));
                        return c.release();
                    }
                }
                else {
                    return nullptr;
                }
            } else {
                std::ignore = p;
                assert(!"[sgcl] clone(): no copy constructor");
                return nullptr;
            }
        }
    }
}
