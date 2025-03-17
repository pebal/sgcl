//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array.h"
#include "thread.h"
#include "type_info.h"
#include "unique_ptr.h"

namespace sgcl::detail {
    template<class T, class ...A>
    inline T* construct(void* p, A&&... a) {
        Page::set_state<State::UniqueLock>(p);
        try {
            if constexpr(sizeof...(A)) {
                return new(p) T(std::forward<A>(a)...);
            } else {
                return new(p) T;
            }
        }
        catch (...) {
            Page::set_state<State::BadAlloc>(p);
            throw;
        }
    }

    template<class T>
    class Maker {
    public:
        template<class ...A>
        static UniquePtr<T> make_tracked(A&&... a) {
            return _make(std::forward<A>(a)...);
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;

        template<class ...A>
        static UniquePtr<T> _make(A&&... a) {
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc();
            Type* ptr;
            if constexpr(!std::is_trivially_default_constructible_v<T>) {
                auto range_guard = thread.use_alloc_range({(uintptr_t)(mem), sizeof(T)});
                if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::memset(mem, 0xFF, sizeof(T));
                    auto child_guard = thread.use_child_pointers({(uintptr_t)mem, &Info::child_pointers.map});
                    ptr = construct<Type>(mem, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                } else {
                    std::memset(mem, 0, sizeof(T));
                    ptr = construct<Type>(mem, std::forward<A>(a)...);
                }
            } else {
                ptr = construct<Type>(mem, std::forward<A>(a)...);
            }
            return UniquePtr<T>(ptr);
        }
    };

    class ArrayMaker {
    protected:
        template<class T>
        static UniquePtr<void> _make(size_t size, size_t count) {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(size);
            auto ptr = construct<Type>(mem, count);
            return UniquePtr<void>(ptr->data);
        }

        template<size_t N = 1>
        static UniquePtr<void> _make_array(size_t count, size_t object_size) {
            if (object_size * count <= N && sizeof(Array<N>) <= PageDataSize) {
                return _make<Array<N>>(0, count);
            } else {
                if constexpr(sizeof(Array<N>) < PageDataSize) {
                    return _make_array<std::max(N * 3 / 2, N + 1)>(count, object_size);
                } else {
                    if (object_size * count <= PageDataSize - sizeof(ArrayBase)) {
                        return _make<Array<PageDataSize - sizeof(ArrayBase)>>(0, count);
                    } else {
                        return _make<Array<>>(object_size * count + sizeof(ArrayBase) - sizeof(Array<>), count);
                    }
                }
            }
        }
    };

    template<class T>
    class Maker<T[]> : ArrayMaker {
    public:
        static UniquePtr<T[]> make_tracked(size_t count) {
            auto p = _make_array<>(count, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array);
            return UniquePtr<T[]>((T*)p.release());
        }

        template<class ...A>
        static UniquePtr<T[]> make_tracked(size_t count, A&&... a) {
            auto p = _make_array<>(count, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array, std::forward<A>(a)...);
            return UniquePtr<T[]>((T*)p.release());
        }

        static UniquePtr<T[]> make_tracked(std::initializer_list<T> list) {
            auto p = _make_array<>(list.size(), sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array, list);
            return UniquePtr<T[]>((T*)p.release());
        }

        static UniquePtr<T[]> make_tracked(ArrayBase* a) {
            auto p = _make_array<>(a->count, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array, a);
            return UniquePtr<T[]>((T*)p.release());
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;

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

        static void _init(void* data, size_t i, size_t size, ArrayBase* a) {
            auto src = (const Type*)(a + 1);
            for (; i < size; ++i) {
                new((Type*)data + i) Type(src[i]);
            }
        }

        template<class... A>
        static void _init_data(Array<>& array, A&&... a) {
            if constexpr(Info::MayContainTracked) {
                int offset;
                auto& thread = current_thread();
                auto range_guard = thread.use_alloc_range({(uintptr_t)(array.data), sizeof(Type) * array.count});
                if (array.count && !Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::memset(array.data, 0xFF, sizeof(Type));
                    std::memset((void*)((Type*)array.data + 1), 0, sizeof(Type) * (array.count - 1));
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    auto child_guard = thread.use_child_pointers({(uintptr_t)array.data, &Info::child_pointers.map});
                    _init(array.data, 0, 1, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                    offset = 1;
                } else {
                    std::memset(array.data, 0, sizeof(Type) * array.count);
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    offset = 0;
                }
                if constexpr(Info::IsTracked) {
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
