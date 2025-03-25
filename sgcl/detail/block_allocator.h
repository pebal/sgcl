//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "memory_counters.h"
#include "merge_sort.h"
#include "pointer_pool.h"
#include <functional>
#include <iostream>
#include <thread>

namespace sgcl::detail {
    class BlockAllocator {
    public:
        ~BlockAllocator() noexcept {
            DataPage* pages = nullptr;
            while (!_pointer_pool.is_empty()) {
                auto data = (DataPage*)_pointer_pool.alloc();
                data->next = pages;
                pages = data;
            }
            free(pages, true);
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

        static void release_empty() noexcept {
            DataPage* pages = _empty_pages.exchange(nullptr, std::memory_order_relaxed);
            Block* blocks = _remove_empty(pages);
            pages = merge_sort<&DataPage::next>(pages);
            _empty_pages.store(pages, std::memory_order_release);
            _release_empty(blocks);
        }

        static void free(DataPage* pages, bool destructor = false) noexcept {
            size_t free_count = 0;
            auto page = pages;
            while(page) {
                ++free_count;
                page = page->next;
            }
            Block* blocks = _remove_empty(pages);
            _release_empty(blocks);
            if (pages) {
                pages = merge_sort<&DataPage::next>(pages);
                auto last = pages;
                while(last->next) {
                    last = last->next;
                }
                last->next = _empty_pages.load(std::memory_order_relaxed);
                while(!_empty_pages.compare_exchange_weak(last->next, pages, std::memory_order_release, std::memory_order_relaxed));
            }
            if (free_count && !destructor) {
                auto size = config::PageSize * free_count;
                MemoryCounters::update_free(free_count, size);
                auto alloc_count = MemoryCounters::last_alloc_count();
                if (free_count * 4 > alloc_count + 64) {
                    force_short_sleep();
                }
            }
        }

    private:
        inline static std::atomic<DataPage*> _empty_pages = {nullptr};
        PointerPool<Block::PageCount, config::PageSize> _pointer_pool;

        static Block* _remove_empty(DataPage*& pages) noexcept {
            Block* blocks = nullptr;
            auto page = pages;
            while(page) {
                Block* block = page->block;
                if (!block->page_count) {
                    block->next = blocks;
                    blocks = block;
                }
                ++block->page_count;
                page = page->next;
            }
            DataPage* prev = nullptr;
            page = pages;
            while(page) {
                auto next = page->next;
                Block* b = page->block;
                assert(b->page_count <= Block::PageCount);
                if (b->page_count == Block::PageCount) {
                    if (!prev) {
                        pages = next;
                    } else {
                        prev->next = next;
                    }
                } else {
                    prev = page;
                }
                page = next;
            }
            return blocks;
        }

        static void _release_empty(Block* block) noexcept {
            std::vector<Block*> blocks_to_delete;
            while(block) {
                auto next = block->next;
                if (block->page_count == Block::PageCount) {
                    blocks_to_delete.push_back(block);
                } else {
                    block->page_count = 0;
                }
                block = next;
            }
            if (blocks_to_delete.size()) {
                destroyer_threads().emplace_back(
                    std::thread([blocks = std::move(blocks_to_delete)] {
                        for (auto block: blocks) {
                            delete block;
                        }
                    })
                );
            }
        }
    };
}
