//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "memory_counters.h"
#include "pointer_pool.h"
#include <functional>

namespace sgcl::detail {
    class BlockAllocator {
    public:
        ~BlockAllocator() noexcept {
            DataPage* page = nullptr;
            while (!_pointer_pool.is_empty()) {
                auto data = (DataPage*)_pointer_pool.alloc();
                data->next = page;
                page = data;
            }
            free(page, true);
        }

        DataPage* alloc() {
            using WakingUp = std::unique_ptr<size_t, std::function<void(size_t*)>>;
            WakingUp waking_up;
            MemoryCounters::update_alloc(1, config::PageSize);
            auto alloc_count = MemoryCounters::alloc_count();
            auto live_count = MemoryCounters::live_count();
            if (alloc_count * 4 > live_count + 64) {
                waking_up = WakingUp(&live_count, [](size_t*){ waking_up_collector(); });
            }
            if (_pointer_pool.is_empty()) {
                DataPage* page = _empty_pages.load(std::memory_order_acquire);
                if (page && _empty_pages.compare_exchange_strong(page, page->next, std::memory_order_relaxed)) {
                    return page;
                }
                auto block = new Block;
                _pointer_pool.fill(block + 1);
            }
            return (DataPage*)_pointer_pool.alloc();
        }

        static void remove_empty() noexcept {
            DataPage* page = _empty_pages.exchange(nullptr, std::memory_order_relaxed);
            if (page) {
                Block* block = nullptr;
                auto p = page;
                while(p) {
                    Block* b = p->block;
                    if (!b->page_count) {
                        b->next = block;
                        block = b;
                    }
                    ++b->page_count;
                    p = p->next;
                }
                DataPage* prev = nullptr;
                p = page;
                while(p) {
                    auto next = p->next;
                    Block* b = p->block;
                    if (b->page_count == Block::PageCount) {
                        if (!prev) {
                            page = next;
                        } else {
                            prev->next = next;
                        }
                    } else {
                        prev = p;
                    }
                    p = next;
                }
                _empty_pages.store(page, std::memory_order_release);
                while(block) {
                    auto next = block->next;
                    if (block->page_count == Block::PageCount) {
                        delete block;
                    } else {
                        block->page_count = 0;
                    }
                    block = next;
                }
            }
        }

        static void free(DataPage* page, bool destructor = false) noexcept {
            if (page) {
                size_t count = 1;
                auto last = page;
                while(last->next) {
                    ++count;
                    last = last->next;
                }
                last->next = _empty_pages.load(std::memory_order_relaxed);
                while(!_empty_pages.compare_exchange_weak(last->next, page, std::memory_order_release, std::memory_order_relaxed));
                if (!destructor) {
                    MemoryCounters::update_free(count, config::PageSize * count);
                    auto free_count = MemoryCounters::free_count();
                    auto live_count = MemoryCounters::live_count();
                    if (free_count * 4 > live_count + 64) {
                        waking_up_collector();
                    }
                }
            }
        }

    private:
        inline static std::atomic<DataPage*> _empty_pages = {nullptr};
        PointerPool<Block::PageCount, config::PageSize> _pointer_pool;
    };
}
