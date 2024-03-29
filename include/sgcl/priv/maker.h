//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "array.h"
#include "unique_ptr.h"

#include <cstring>

namespace sgcl {
    namespace Priv {
        void Collector_init();

        template<class T>
        struct Maker {
            template<class ...A>
            static Unique_ptr<T> make_tracked(A&&... a) {
                return _make(std::forward<A>(a)...);
            }

        private:
            template<class ...A>
            static Unique_ptr<T> _make(A&&... a) {
                using Info = Page_info<std::remove_cv_t<T>>;
                Collector_init();
                auto& thread = Current_thread();
                auto& allocator = thread.alocator<std::remove_cv_t<T>>();
                auto mem = allocator.alloc();
                if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::memset(mem, 0xFF, sizeof(T));
                } else {
                    std::memset(mem, 0, sizeof(T));
                }
                auto old_pointers = thread.child_pointers;
                thread.child_pointers = {(uintptr_t)mem, &Info::child_pointers.map};
                std::remove_cv_t<T>* ptr;
                try {
                    Page::set_state(mem, State::UniqueLock);
                    if constexpr(sizeof...(A)) {
                        ptr = const_cast<std::remove_cv_t<T>*>(new(mem) T(std::forward<A>(a)...));
                    } else {
                        ptr = const_cast<std::remove_cv_t<T>*>(new(mem) T);
                    }
                    Info::child_pointers.final.store(true, std::memory_order_release);
                    thread.update_allocated(sizeof(T));
                }
                catch (...) {
                    Page::set_state(mem, State::BadAlloc);
                    thread.child_pointers = old_pointers;
                    throw;
                }
                thread.child_pointers = old_pointers;
                return Unique_ptr<T>(ptr);
            }
        };

        struct Maker_base {
            template<class U>
            static Unique_ptr<void> _make(size_t size, size_t count) {
                using Info = Page_info<std::remove_cv_t<U>>;
                Collector_init();
                auto& thread = Current_thread();
                auto& allocator = thread.alocator<std::remove_cv_t<U>>();
                auto mem = allocator.alloc(size);
                std::remove_cv_t<U>* ptr;
                try {
                    Page::set_state(mem, State::UniqueLock);
                    ptr = const_cast<std::remove_cv_t<U>*>(new(mem) U(count));
                    Info::child_pointers.final.store(true, std::memory_order_release);
                    thread.update_allocated(sizeof(U) + size);
                }
                catch (...) {
                    Page::set_state(mem, State::BadAlloc);
                    throw;
                }
                return Unique_ptr<void>(ptr->data);
            }

            template<size_t N = 1>
            static Unique_ptr<void> _make_array(size_t count, size_t object_size) {
                if (object_size * count <= N && sizeof(Array<N>) <= PageDataSize) {
                    return _make<Array<N>>(0, count);
                } else {
                    if constexpr(sizeof(Array<N>) < PageDataSize) {
                        return _make_array<std::max(N * 3 / 2, N + 1)>(count, object_size);
                    } else {
                        if (object_size * count <= PageDataSize - sizeof(Array_base)) {
                            return _make<Array<PageDataSize - sizeof(Array_base)>>(0, count);
                        } else {
                            return _make<Array<>>(object_size * count + sizeof(Array_base) - sizeof(Array<>), count);
                        }
                    }
                }
            }
        };

        template<class T>
        struct Maker<T[]> : Maker_base {
            static Unique_ptr<T[]> make_tracked(size_t count) {
                if (count) {
                    auto p = _make_array<>(count, sizeof(T));
                    auto array = (Array<>*)((Array_base*)p.get() - 1);
                    _init_data(*array);
                    return Unique_ptr<T[]>((T*)p.release());
                }
                return nullptr;
            }
            static Unique_ptr<T[]> make_tracked(size_t count, const T& v) {
                if (count) {
                    auto p = _make_array<>(count, sizeof(T));
                    auto array = (Array<>*)((Array_base*)p.get() - 1);
                    _init_data(*array, v);
                    return Unique_ptr<T[]>((T*)p.release());
                }
                return nullptr;
            }

        private:
            template<class... A>
            static void _init_data(Array<>& array, A&&... a) {
                using Info = Page_info<std::remove_cv_t<T>>;
                if constexpr(!std::is_trivial_v<T>) {
                    auto& thread = Current_thread();
                    if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                        std::memset(array.data, 0xFF, sizeof(T));
                        std::memset((T*)array.data + 1, 0, sizeof(T) * (array.count - 1));
                    } else {
                        std::memset(array.data, 0, sizeof(T) * array.count);
                    }
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    auto old_pointers = thread.child_pointers;
                    thread.child_pointers = {(uintptr_t)array.data, &Info::child_pointers.map};
                    try {
                        if constexpr(sizeof...(A)) {
                            new(array.data) T(std::forward<A>(a)...);
                        } else {
                            new(array.data) T;
                        }
                        Info::child_pointers.final.store(true, std::memory_order_release);
                    }
                    catch (...) {
                        thread.child_pointers = old_pointers;
                        throw;
                    }
                    thread.child_pointers = old_pointers;
                    if constexpr(std::is_base_of_v<Tracked, T>) {
                        if constexpr(sizeof...(A)) {
                            for (size_t i = 1; i < array.count; ++i) {
                                new((T*)array.data + i) T(std::forward<A>(a)...);
                            }
                        }
                    } else {
                        for (size_t i = 1; i < array.count; ++i) {
                            if constexpr(sizeof...(A)) {
                                new(((T*)array.data + i)) T(std::forward<A>(a)...);
                            } else {
                                new(((T*)array.data + i)) T;
                            }
                        }
                    }
                } else {
                    Info::child_pointers.final.store(true, std::memory_order_relaxed);
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    if constexpr(sizeof...(A)) {
                        for (size_t i = 0; i < array.count; ++i) {
                            new(const_cast<std::remove_cv_t<T>*>((T*)array.data + i)) T(std::forward<A>(a)...);
                        }
                    }
                }
            }
        };
    }
}
