//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "counter.h"

#include <atomic>

namespace sgcl::detail {
    class MemoryCounters {
    public:
        inline static void update_alloc(int64_t size) noexcept {
            _alloc_count.fetch_add(1, std::memory_order_relaxed);
            _alloc_size.fetch_add(size, std::memory_order_relaxed);
            _live_count.fetch_add(1, std::memory_order_relaxed);
            _live_size.fetch_add(size, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
        }
        inline static void update_free(int64_t size) noexcept {
            _free_count.fetch_add(1, std::memory_order_relaxed);
            _free_size.fetch_add(size, std::memory_order_relaxed);
            _live_count.fetch_sub(1, std::memory_order_relaxed);
            _live_size.fetch_sub(size, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
        }
        inline static Counter alloc_counter() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            return Counter(_alloc_count.load(std::memory_order_relaxed), _alloc_size.load(std::memory_order_relaxed));
        }
        inline static Counter free_counter() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            return Counter(_free_count.load(std::memory_order_relaxed), _free_size.load(std::memory_order_relaxed));
        }
        inline static Counter live_counter() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            return Counter(_live_count.load(std::memory_order_relaxed), _live_size.load(std::memory_order_relaxed));
        }
        inline static void reset_alloc() noexcept {
            _alloc_count.store(0, std::memory_order_relaxed);
            _alloc_size.store(0, std::memory_order_relaxed);
        }
        inline static void reset_free() noexcept {
            _free_count.store(0, std::memory_order_relaxed);
            _free_size.store(0, std::memory_order_relaxed);
        }
        inline static void reset_all() noexcept {
            reset_alloc();
            reset_free();
        }
    private:
        inline static std::atomic<size_t> _live_count = {0};
        inline static std::atomic<size_t> _live_size = {0};
        inline static std::atomic<size_t> _alloc_count = {0};
        inline static std::atomic<size_t> _alloc_size = {0};
        inline static std::atomic<size_t> _free_count = {0};
        inline static std::atomic<size_t> _free_size = {0};
    };
}
