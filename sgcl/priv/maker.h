//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
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

        template<class T, class ...A>
        inline T* Construct(void* p, A&&... a) {
            Page::set_state(p, State::UniqueLock);
            try {
                if constexpr(sizeof...(A)) {
                    return new(p) T(std::forward<A>(a)...);
                } else {
                    return new(p) T;
                }
            }
            catch (...) {
                Page::set_state(p, State::BadAlloc);
                throw;
            }
        }

        template<class T>
        struct Maker {
            template<class ...A>
            static Unique_ptr<T> make_tracked(A&&... a) {
                return _make(std::forward<A>(a)...);
            }

        private:
            using Info = Type_info<T>;
            using Type = typename Info::type;

            template<class ...A>
            static Unique_ptr<T> _make(A&&... a) {
                Collector_init();
                auto& thread = Current_thread();
                auto& allocator = thread.alocator<Type>();
                auto mem = allocator.alloc();
                Type* ptr;
                if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::memset(mem, 0xFF, sizeof(T));
                    auto range_guard = thread.use_child_pointers({(uintptr_t)mem, &Info::child_pointers.map});
                    ptr = Construct<Type>(mem, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                } else {
                    std::memset(mem, 0, sizeof(T));
                    ptr = Construct<Type>(mem, std::forward<A>(a)...);
                }
                thread.update_allocated(sizeof(T));
                return Unique_ptr<T>(ptr);
            }
        };

        struct Maker_base {
            template<class T>
            static Unique_ptr<void> _make(size_t size, size_t count) {
                using Info = Type_info<T>;
                using Type = typename Info::type;
                Collector_init();
                auto& thread = Current_thread();
                auto& allocator = thread.alocator<Type>();
                auto mem = allocator.alloc(size);
                auto ptr = Construct<Type>(mem, count);
                thread.update_allocated(sizeof(Type) + size);
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

            template<class ...A>
            static Unique_ptr<T[]> make_tracked(size_t count, A&&... a) {
                if (count) {
                    auto p = _make_array<>(count, sizeof(T));
                    auto array = (Array<>*)((Array_base*)p.get() - 1);
                    _init_data(*array, std::forward<A>(a)...);
                    return Unique_ptr<T[]>((T*)p.release());
                }
                return nullptr;
            }

            static Unique_ptr<T[]> make_tracked(std::initializer_list<T> l) {
                if (l.size()) {
                    auto p = _make_array<>(l.size(), sizeof(T));
                    auto array = (Array<>*)((Array_base*)p.get() - 1);
                    _init_data(*array, l);
                    return Unique_ptr<T[]>((T*)p.release());
                }
                return nullptr;
            }

        private:
            using Info = Type_info<T>;
            using Type = typename Info::type;

            static void _init(void* data, size_t i, size_t size) {
                for (; i < size; ++i) {
                    new((Type*)data + i) Type;
                }
            }

            template<class ...A>
            static void _init(void* data, size_t i, size_t size, A&&... a) {
                for (; i < size; ++i) {
                    new((Type*)data + i) Type(std::forward<A>(a)...);
                }
            }

            static void _init(void* data, size_t i, size_t size, std::initializer_list<T> list) {
                auto item = list.begin() + i;
                for (; i < size; ++i) {
                    new((Type*)data + i) Type(*item);
                    ++item;
                }
            }

            template<class... A>
            static void _init_data(Array<>& array, A&&... a) {
                if constexpr(!std::is_trivial_v<Type>) {
                    int offset;
                    if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                        std::memset(array.data, 0xFF, sizeof(Type));
                        std::memset((Type*)array.data + 1, 0, sizeof(Type) * (array.count - 1));
                        array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                        auto range_guard = Current_thread().use_child_pointers({(uintptr_t)array.data, &Info::child_pointers.map});
                        _init(array.data, 0, 1, std::forward<A>(a)...);
                        Info::child_pointers.final.store(true, std::memory_order_release);
                        offset = 1;
                    } else {
                        std::memset(array.data, 0, sizeof(Type) * array.count);
                        array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                        offset = 0;
                    }
                    if constexpr(std::is_base_of_v<Tracked, Type>) {
                        if constexpr(sizeof...(A)) {
                            _init(array.data, offset, array.count, std::forward<A>(a)...);
                        }
                    } else {
                        _init(array.data, offset, array.count, std::forward<A>(a)...);
                    }
                } else {
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    if constexpr(sizeof...(A)) {
                        _init(array.data, 0, array.count, std::forward<A>(a)...);
                    }
                }
            }
        };
    }
}
