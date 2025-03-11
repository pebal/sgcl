//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "maker.h"
#include "page_info.h"

namespace sgcl::detail {
    template <class T>
    class RootContainerAllocator {
        using Info = PageInfo<T>;

        struct Data {
            char data[sizeof(T)];
        };

    public:
        using value_type         = T;
        using pointer            = value_type*;
        using const_pointer      = const value_type*;
        using void_pointer       = void*;
        using const_void_pointer = const void*;
        using difference_type    = ptrdiff_t;
        using size_type          = std::size_t;

        template <class U>
        struct rebind  {
            using other = RootContainerAllocator<U>;
        };

        RootContainerAllocator() noexcept = default;

        template <class U>
        RootContainerAllocator(const RootContainerAllocator<U>&) noexcept {}

        pointer allocate(size_type n)  {
            void* mem;
            if (n > 1) {
                auto data = Maker<Data[]>::make_tracked(n);
                mem = data.get();
                data.release();
                if constexpr(TypeInfo<T>::MayContainTracked) {
                    std::memset(mem, 0, sizeof(Data) * n);
                    auto array = (ArrayBase*)mem - 1;
                    array->metadata.store(&Info::array_metadata(), std::memory_order_release);
                }
            } else {
                auto data = Maker<Data>::make_tracked();
                mem = data.get();
                data.release();
                if constexpr(TypeInfo<T>::MayContainTracked) {
                    std::memset(mem, 0, sizeof(Data));
                }
            }
            return (pointer)mem;
        }

        void deallocate(pointer p, size_type) noexcept {
            Page::set_state<State::Destroyed>(p);
        }

        pointer allocate(size_type n, const_void_pointer) {
            return allocate(n);
        }

        template <class U, class ...A>
        void construct(U* p, A&& ...a)  {
            if constexpr(TypeInfo<U>::MayContainTracked) {
                if (!Info::child_pointers.final.load(std::memory_order_acquire) && alignof(U) >= alignof(RawPointer)) {
                    auto count = sizeof(U) / sizeof(RawPointer);
                    auto mem = (RawPointer*)p;
                    for (int i = 0; i < count; ++i) {
                        mem[i].store((void*)std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
                    }
                    auto range_guard = current_thread().use_child_pointers({(uintptr_t)p, &Info::child_pointers.map});
                    _construct(p, std::forward<A>(a)...);
                    Info::child_pointers.final.store(true, std::memory_order_release);
                } else {
                    _construct(p, std::forward<A>(a)...);
                }
            } else {
                _construct(p, std::forward<A>(a)...);
            }
        }

        void destroy(T* p) noexcept {
            std::destroy_at(p);
        }

        constexpr size_type max_size() const noexcept {
            return std::numeric_limits<size_type>::max();
        }

    private:
        template <class U, class ...A>
        void _construct(U* p, A&& ...a)  {
            if constexpr(sizeof...(A)) {
                new(p) U(std::forward<A>(a)...);
            } else {
                new(p) U;
            }
        }
    };

    template <class T>
    inline bool operator==(const RootContainerAllocator<T>&, const RootContainerAllocator<T>&) noexcept  {
        return true;
    }

    template <class T>
    inline bool operator!=(const RootContainerAllocator<T>&, const RootContainerAllocator<T>&) noexcept {
        return false;
    }
}
