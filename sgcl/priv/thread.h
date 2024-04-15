//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "child_pointers.h"
#include "heap_roots_allocator.h"
#include "large_object_allocator.h"
#include "small_object_allocator.h"
#include "stack_roots_allocator.h"

#include <thread>

#if SGCL_LOG_PRINT_LEVEL
#include <iostream>
#endif

namespace sgcl {
    namespace Priv {
        void Terminate_collector();
        struct Thread {
            struct Data {
                Data(Block_allocator* b, Stack_roots_allocator* s) noexcept
                : block_allocator(b)
                , stack_roots_allocator(s) {
                }
                std::unique_ptr<Block_allocator> block_allocator;
                std::unique_ptr<Stack_roots_allocator> stack_roots_allocator;
                std::atomic<bool> is_used = {true};
                std::atomic<int64_t> alloc_count = {0};
                std::atomic<int64_t> alloc_size = {0};
                Data* next = {nullptr};
                Data* next_unused = {nullptr};
            };

            struct Child_pointers {
                uintptr_t base;
                Priv::Child_pointers::Map* map;
            };

            static constexpr size_t TypePageCount = 64;
            static constexpr size_t TypePageSize = MaxTypeNumber / TypePageCount;

            Thread()
                : stack_roots_allocator(new Stack_roots_allocator)
                , heap_roots_allocator(new Heap_roots_allocator)
                , _block_allocator(new Block_allocator)
                , _data(new Data{_block_allocator, stack_roots_allocator}){
#if SGCL_LOG_PRINT_LEVEL >= 3
                std::cout << "[sgcl] start thread id: " << std::this_thread::get_id() << std::endl;
#endif
                _data->next = threads_data.load(std::memory_order_acquire);
                while(!threads_data.compare_exchange_weak(_data->next, _data, std::memory_order_release, std::memory_order_relaxed));
            }

            ~Thread() {
                _data->is_used.store(false, std::memory_order_release);
                if (std::this_thread::get_id() == main_thread_id) {
                    Terminate_collector();
                }
#if SGCL_LOG_PRINT_LEVEL >= 3
                std::cout << "[sgcl] stop thread id: " << std::this_thread::get_id() << std::endl;
#endif
            }

            template<class T>
            auto& alocator() {
                using Info = Type_info<T>;
                using Type = typename Info::type;
                if constexpr(std::is_same_v<typename Info::Object_allocator, Small_object_allocator<Type>>) {
                    static unsigned index = _type_index++;
                    assert(index < MaxTypeNumber);
                    auto& allocators = _allocators[index / TypePageSize];
                    if (!allocators) {
                        allocators.reset(new std::array<std::unique_ptr<Object_allocator>, TypePageSize>);
                    }
                    auto& alocator = (*allocators)[index % TypePageSize];
                    if (!alocator) {
                        alocator.reset(new Small_object_allocator<Type>(*_block_allocator));
                    }
                    return static_cast<Small_object_allocator<Type>&>(*alocator);
                }
                else {
                    static Large_object_allocator<Type> allocator;
                    return allocator;
                }
            }

            void update_allocated(size_t s) {
                auto v = _data->alloc_count.load(std::memory_order_relaxed);
                _data->alloc_count.store(v + 1, std::memory_order_relaxed);
                v = _data->alloc_size.load(std::memory_order_relaxed);
                _data->alloc_size.store(v + s, std::memory_order_relaxed);
            }

            Child_pointers child_pointers = {0, nullptr};
            inline static std::atomic<Data*> threads_data = {nullptr};
            inline static std::thread::id main_thread_id = {};

            Stack_roots_allocator* const stack_roots_allocator;
            const std::unique_ptr<Heap_roots_allocator> heap_roots_allocator;

            struct Range_guard {
                Range_guard(const Range_guard&) = delete;
                void operator=(const Range_guard&) = delete;

                constexpr Range_guard(Thread& t, const Child_pointers& cp) noexcept
                : thread(t)
                , old_pointers(t.child_pointers) {
                    thread.child_pointers = cp;
                }

                ~Range_guard() noexcept {
                    thread.child_pointers = old_pointers;
                }

                Thread& thread;
                const Child_pointers old_pointers;
            };

            Range_guard use_child_pointers(const Child_pointers& cp) noexcept {
                return {*this, cp};
            }

        private:
            Block_allocator* const _block_allocator;
            std::array<std::unique_ptr<std::array<std::unique_ptr<Object_allocator>, TypePageSize>>, TypePageCount> _allocators;
            Data* const _data;
            inline static std::atomic<int> _type_index = {0};
        };

        struct Main_thread_detector {
            Main_thread_detector() noexcept {
                Thread::main_thread_id = std::this_thread::get_id();
            }
        };

        inline static Main_thread_detector main_thread_detector;

        inline Thread& Current_thread() {
            static thread_local Thread instance;
            return instance;
        }
    }
}
