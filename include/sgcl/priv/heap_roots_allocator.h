//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "data_page.h"
#include "pointer_pool.h"

namespace sgcl {
    namespace Priv {
        struct Heap_roots_allocator {
            static constexpr size_t PointerCount = PageSize / sizeof(Pointer);
            using Page = std::array<Pointer, PointerCount>;
            using Pointer_pool = Priv::Pointer_pool<PointerCount, sizeof(Pointer)>;

            struct Page_node {
                Page_node* next = {nullptr};
                Page page = {};
            };

            constexpr Heap_roots_allocator() noexcept
                :_pointer_pool({nullptr, nullptr}) {
            }

            ~Heap_roots_allocator() noexcept {
                for(auto pointer_pool : _pointer_pool) {
                    if (pointer_pool) {
                        auto& pool = pointer_pool->is_empty() ? _global_empty_pointer_pool : _global_pointer_pool;
                        pointer_pool->next = pool.load(std::memory_order_acquire);
                        while(!pool.compare_exchange_weak(pointer_pool->next, pointer_pool, std::memory_order_release, std::memory_order_relaxed));
                    }
                }
            }

            Tracked_ptr* alloc() {
                auto [pool1, pool2] = _pointer_pool;
                if (pool1 && !pool1->is_empty()) {
                    return (Tracked_ptr*)pool1->alloc();
                }
                if (pool2 && !pool2->is_empty()) {
                    _pointer_pool = {pool2, pool1};
                    return (Tracked_ptr*)pool2->alloc();
                }
                auto new_pool = _global_pointer_pool.load(std::memory_order_acquire);
                while(new_pool && !_global_pointer_pool.compare_exchange_weak(new_pool, new_pool->next, std::memory_order_release, std::memory_order_acquire));
                if (!new_pool) {
                    auto node = new Page_node;
                    new_pool = new Pointer_pool(node->page.data());
                    node->next = pages.load(std::memory_order_acquire);
                    while(!pages.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed));
                }
                if (pool1) {
                    if (pool2) {
                        pool2->next = _global_empty_pointer_pool.load(std::memory_order_acquire);
                        while(!_global_empty_pointer_pool.compare_exchange_weak(pool2->next, pool2, std::memory_order_release, std::memory_order_relaxed));
                    }
                    pool2 = pool1;
                }
                pool1 = new_pool;
                _pointer_pool = {pool1, pool2};
                return (Tracked_ptr*)pool1->alloc();
            }

            void free(Tracked_ptr* p) noexcept {
                auto [pool1, pool2] = _pointer_pool;
                if (pool1 && !pool1->is_full()) {
                    pool1->free(p);
                    return;
                }
                if (pool2 && !pool2->is_full()) {
                    pool2->free(p);
                    _pointer_pool = {pool2, pool1};
                    return;
                }
                auto new_pool = _global_empty_pointer_pool.load(std::memory_order_acquire);
                while(new_pool && !_global_empty_pointer_pool.compare_exchange_weak(new_pool, new_pool->next, std::memory_order_relaxed, std::memory_order_acquire));
                if (!new_pool) {
                    new_pool = new Pointer_pool();
                }
                if (pool1) {
                    if (pool2) {
                        pool2->next = _global_pointer_pool.load(std::memory_order_acquire);
                        while(!_global_pointer_pool.compare_exchange_weak(pool2->next, pool2, std::memory_order_release, std::memory_order_relaxed));
                    }
                    pool2 = pool1;
                }
                pool1 = new_pool;
                _pointer_pool = {pool1, pool2};
                pool1->free(p);
            }

            inline static std::atomic<Page_node*> pages = {nullptr};

        private:
            std::array<Pointer_pool*, 2> _pointer_pool;
            inline static std::atomic<Pointer_pool*> _global_pointer_pool = {nullptr};
            inline static std::atomic<Pointer_pool*> _global_empty_pointer_pool = {nullptr};
        };
    }
}
