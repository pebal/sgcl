//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "pointer_pool.h"

namespace sgcl::detail {
    class BlockAllocator {
    public:
        using PointerPool = PointerPool<Block::PageCount, config::PageSize>;

        ~BlockAllocator() noexcept {
            DataPage* page = nullptr;
            while (!_pointer_pool.is_empty()) {
                auto data = (DataPage*)_pointer_pool.alloc();
                data->next = page;
                page = data;
            }
            free(page, false);
        }

        DataPage* alloc() {
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

        static void remove_empty() {
            free(nullptr);
        }

        static void free(DataPage* page, bool remove_empty = true) noexcept {
            if (page) {
                auto last = page;
                while(last->next) {
                    last = last->next;
                }
                last->next = _empty_pages.exchange(nullptr, std::memory_order_relaxed);
            } else {
                page = _empty_pages.exchange(nullptr, std::memory_order_relaxed);
            }
            Block* block = nullptr;
            if (remove_empty) {
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

    private:
        inline static std::atomic<DataPage*> _empty_pages = {nullptr};
        PointerPool _pointer_pool;
    };
}
