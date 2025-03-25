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
#include <type_traits>

namespace sgcl::detail {
    class MakerBase {
    protected:
        template<class T, class ...A>
        static void _construct(void* p, A&&... a) {
            if constexpr(sizeof...(A)) {
                new(p) T(std::forward<A>(a)...);
            } else {
                new(p) T;
            }
        }

        template<class T, class ...A>
        static void _construct_and_register(void* p, A&&... a) {
            Page::set_state<State::UniqueLock>(p);
            try {
                _construct<T>(p, std::forward<A>(a)...);
            }
            catch (...) {
                Page::set_state<State::BadAlloc>(p);
                throw;
            }
        }
    };

    template<class T>
    class Maker : MakerBase {
    public:       
        template<class ...A>
        static void construct(void* p, A&&... a) {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            if constexpr(Info::MayContainTracked) {
                auto& thread = current_thread();
                auto range_guard = thread.use_alloc_range({(uintptr_t)(p), sizeof(T)});
                if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                    auto count = sizeof(Type) / sizeof(RawPointer);
                    auto mem = (RawPointer*)p;
                    for (int i = 0; i < count; ++i) {
                        mem[i].store((void*)size_t(1), std::memory_order_relaxed);
                    }
                    auto range_guard = thread.use_child_pointers({(uintptr_t)p, &Info::child_pointers.map});
                    _construct<Type>(p, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                } else {
                    _construct<Type>(p, std::forward<A>(a)...);
                }
            } else {
                _construct<Type>(p, std::forward<A>(a)...);
            }
        }

        inline static void destroy(T* p) noexcept {
            if constexpr(!std::is_trivially_destructible_v<T> && std::is_destructible_v<T>) {
                std::destroy_at(p);
            }
        }

        template<class ...A>
        static UniquePtr<T> make_tracked(A&&... a) {
            return _make(std::forward<A>(a)...);
        }

        template<class ...A>
        static UniquePtr<T> make_tracked_data() {
            return _make_data();
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;

        template<class ...A>
        static UniquePtr<T> _make(A&&... a) {
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc();
            if constexpr(Info::MayContainTracked) {
                auto range_guard = thread.use_alloc_range({(uintptr_t)(mem), sizeof(T)});
                if (!Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::fill_n((size_t*)mem, sizeof(T) / sizeof(size_t), size_t(1));
                    auto child_guard = thread.use_child_pointers({(uintptr_t)mem, &Info::child_pointers.map});
                    _construct_and_register<Type>(mem, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                } else {
                    std::memset(mem, 0, sizeof(T));
                    _construct_and_register<Type>(mem, std::forward<A>(a)...);
                }
            } else {
                _construct_and_register<Type>(mem, std::forward<A>(a)...);
            }
            return UniquePtr<T>((Type*)mem);
        }

        static UniquePtr<T> _make_data() {
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc();
            if constexpr(Info::MayContainTracked) {
                std::memset(mem, 0, sizeof(T));
            }
            Page::set_state<State::UniqueLock>(mem);
            return UniquePtr<T>((Type*)mem);
        }
    };

    class ArrayMaker : MakerBase {
    protected:
        template<class T>
        static UniquePtr<void> _make(size_t data_size, size_t size, size_t capacity) {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(data_size);
            _construct_and_register<Type>(mem, size, capacity);
            return UniquePtr<void>(((Type*)mem)->data);
        }

        template<size_t N = sizeof(uintptr_t)>
        static UniquePtr<void> _make_array(size_t size, size_t capacity, size_t object_size) {
            if (object_size * capacity <= N && sizeof(Array<N>) <= PageDataSize) {
                capacity = N / object_size;
                return _make<Array<N>>(0, size, capacity);
            } else {
                if constexpr(sizeof(Array<N>) < PageDataSize) {
                    static constexpr auto Size = (N * 3 / 2 + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
                    return _make_array<Size>(size, capacity, object_size);
                } else {
                    if (object_size * capacity <= PageDataSize - sizeof(ArrayBase)) {
                        auto capacity = (PageDataSize - sizeof(ArrayBase)) / object_size;
                        return _make<Array<PageDataSize - sizeof(ArrayBase)>>(0, size, capacity);
                    } else {
                        return _make<Array<>>(object_size * capacity + sizeof(ArrayBase) - sizeof(Array<>), size, capacity);
                    }
                }
            }
        }

        template<size_t N = sizeof(uintptr_t)>
        static UniquePtr<void> _make_array(size_t size, size_t object_size) {
            return _make_array(size, size, object_size);
        }
    };

    template<class T>
    class Maker<T[]> : ArrayMaker {
    public:
        static UniquePtr<T[]> make_tracked(size_t size) {
            auto p = _make_array<>(size, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array);
            return UniquePtr<T[]>((T*)p.release());
        }

        template<class ...A>
        static UniquePtr<T[]> make_tracked(size_t size, A&&... a) {
            auto p = _make_array<>(size, sizeof(T));
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
            auto p = _make_array<>(a->size, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            _init_data(*array, a);
            return UniquePtr<T[]>((T*)p.release());
        }

        static UniquePtr<T[]> make_tracked_data(size_t capacity) {
            auto p = _make_array<>(0, capacity, sizeof(T));
            auto array = (Array<>*)((ArrayBase*)p.get() - 1);
            if constexpr(Info::MayContainTracked) {
                std::memset(array->data, 0, sizeof(Type) * array->capacity);
            }
            array->metadata.store(&Info::array_metadata(), std::memory_order_release);
            return UniquePtr<T[]>((T*)p.release());
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;

        static void _init(Type* data, size_t i, size_t size) {
            for (; i < size; ++i) {
                new(data + i) Type;
            }
        }

        template<class ...A>
        static void _init(Type* data, size_t i, size_t size, A&&... a) {
            for (; i < size; ++i) {
                new(data + i) Type(std::forward<A>(a)...);
            }
        }

        static void _init(Type* data, size_t i, size_t size, std::initializer_list<T> list) {
            auto item = list.begin() + i;
            for (; i < size; ++i) {
                new(data + i) Type(*item);
                ++item;
            }
        }

        static void _init(Type* data, size_t i, size_t size, ArrayBase* a) {
            auto src = (const Type*)(a + 1);
            for (; i < size; ++i) {
                new(data + i) Type(src[i]);
            }
        }

        static void _init_tracked(Type* data, size_t i, size_t size) {
#ifndef SGCL_ARCH_X86_64
            for (; i < size; ++i) {
                auto tracked = data + i;
                tracked->_raw_ptr_ref = &tracked->_raw_ptr;
            }
#endif
        }

        template<class... A>
        static void _init_data(Array<>& array, A&&... a) {
            if constexpr(Info::MayContainTracked) {
                int offset;
                auto& thread = current_thread();
                auto range_guard = thread.use_alloc_range({(uintptr_t)(array.data), sizeof(Type) * array.capacity});
                if (array.size && !Info::child_pointers.final.load(std::memory_order_acquire)) {
                    std::fill_n((size_t*)array.data, sizeof(Type) / sizeof(size_t), size_t(1));
                    std::memset((void*)((Type*)array.data + 1), 0, sizeof(Type) * (array.capacity - 1));
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    auto child_guard = thread.use_child_pointers({(uintptr_t)array.data, &Info::child_pointers.map});
                    _init((Type*)array.data, 0, 1, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                    offset = 1;
                } else {
                    std::memset(array.data, 0, sizeof(Type) * array.capacity);
                    array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                    offset = 0;
                }
                if constexpr(Info::IsTracked) {
                    if constexpr(sizeof...(A)) {
                        _init((Type*)array.data, offset, array.size, std::forward<A>(a)...);
                    } else {
                        _init_tracked((Type*)array.data, offset, array.size);
                    }
                } else {
                    _init((Type*)array.data, offset, array.size, std::forward<A>(a)...);
                }
            } else {
                array.metadata.store(&Info::array_metadata(), std::memory_order_release);
                if constexpr(sizeof...(A) || !std::is_trivially_constructible_v<T>) {
                    _init((Type*)array.data, 0, array.size, std::forward<A>(a)...);
                }
            }
        }
    };
}
