//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "pointer_pool.h"

namespace sgcl {
    namespace Priv {
        struct Block_allocator {
            using Pointer_pool = Pointer_pool<Block::PageCount, PageSize>;

            ~Block_allocator() noexcept {
                DataPage* page = nullptr;
                while (!_pointer_pool.is_empty()) {
                    auto data = (DataPage*)_pointer_pool.alloc();
                    data->next = page;
                    page = data;
                }
                if (page) {
                    free(page);
                }
            }

            DataPage* alloc() {
                if (_pointer_pool.is_empty()) {
                    while (_lock.test_and_set(std::memory_order_acquire));
                    auto page = _empty_pages;
                    if (page) {
                        _empty_pages = page->next;
                    }
                    _lock.clear(std::memory_order_release);
                    if (page) {
                        return page;
                    } else {
                        auto block = new Block;
                        _pointer_pool.fill(block + 1);
                    }
                }
                return (DataPage*)_pointer_pool.alloc();
            }

            static void free(DataPage* page) {
                auto last = page;
                while(last->next) {
                    last = last->next;
                }
                while (_lock.test_and_set(std::memory_order_acquire));
                last->next = _empty_pages;
                _empty_pages = nullptr;
                _lock.clear(std::memory_order_release);
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
                while (_lock.test_and_set(std::memory_order_acquire));
                _empty_pages = page;
                _lock.clear(std::memory_order_release);
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
            inline static std::atomic_flag _lock = ATOMIC_FLAG_INIT;
            inline static DataPage* _empty_pages = {nullptr};
            Pointer_pool _pointer_pool;
        };
    }
}
