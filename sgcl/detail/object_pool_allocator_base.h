//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block_allocator.h"
#include "object_allocator_base.h"

namespace sgcl::detail {
    class ObjectPoolAllocatorBase : public ObjectAllocatorBase {
    public:
        ObjectPoolAllocatorBase(BlockAllocator& ba, std::atomic<Page*>& pages, PointerPoolBase& pa, std::atomic<Page*>& pb) noexcept
        : ObjectAllocatorBase(pages)
        , _block_allocator(ba)
        , _pointer_pool(pa)
        , _pages_buffer(pb) {
        }

        ~ObjectPoolAllocatorBase() noexcept override {
            bool unused = false;
            while (!_pointer_pool.is_empty()) {
                auto ptr = _pointer_pool.alloc();
                auto index = _current_page->index_of(ptr);
                _current_page->states()[index].store(State::Unused, std::memory_order_relaxed);
                unused = true;
            }
            if (unused) {
                _current_page->unused_occur.store(true, std::memory_order_relaxed);
            }
            std::atomic_thread_fence(std::memory_order_release);
        }

        void* alloc(size_t = 0) {
            if  (_pointer_pool.is_empty()) {
                Page* page = _pages_buffer.load(std::memory_order_acquire);
                if (page && _pages_buffer.compare_exchange_strong(page, page->next_empty, std::memory_order_relaxed)) {
                    _pointer_pool.fill(page);
                    page->on_empty_list.store(false, std::memory_order_release);
                } else {
                    page = _alloc_page();
                    _pointer_pool.fill((void*)(page->data));
                    page->next = _pages.load(std::memory_order_relaxed);
                    _pages.store(page, std::memory_order_release);
                }
                _current_page = page;
            }
            assert(!_pointer_pool.is_empty());
            return (void*)_pointer_pool.alloc();
        }

    private:
        BlockAllocator& _block_allocator;
        PointerPoolBase& _pointer_pool;
        std::atomic<Page*>& _pages_buffer;
        Page* _current_page = {nullptr};

        virtual Page* _create_page_parameters(DataPage*) = 0;

        Page* _alloc_page() {
            auto data = _block_allocator.alloc();
            auto page = _create_page_parameters(data);
            data->page = page;
            return page;
        }

    protected:
        static void _remove_empty(Page*& pages, Page*& empty_pages) noexcept {
            auto page = pages;
            Page* prev = nullptr;
            while(page) {
                auto next = page->next_empty;
                auto states = page->states();
                auto object_count = page->metadata->object_count;
                bool empty = true;
                for (unsigned i = 0; i < object_count; ++i) {
                    auto state = states[i].load(std::memory_order_relaxed);
                    if (state != State::Unused) {
                        empty = false;
                        break;
                    }
                }
                if (empty) {
                    page->next_empty = empty_pages;
                    empty_pages = page;
                    if (!prev) {
                        pages = next;
                    } else {
                        prev->next_empty = next;
                    }
                } else {
                    prev = page;
                }
                page = next;
            }
        }

        static void _free(Page* pages) noexcept {
            Page* page = pages;
            DataPage* empty = nullptr;
            while(page) {
                DataPage* data = (DataPage*)(page->data - sizeof(void*));
                data->block = page->block;
                page->is_used = false;
                data->next = empty;
                empty = data;
                page = page->next_empty;
            }
            BlockAllocator::free(empty);
        }

        static void _free(Page* pages, std::atomic<Page*>& pages_buffer) noexcept {
            Page* empty_pages = nullptr;
            for (int i = 0; i < 2 ; ++i) {
                _remove_empty(pages, empty_pages);
                pages = pages_buffer.exchange(pages, std::memory_order_relaxed);
            }
            if (pages) {
                auto last = pages;
                while(last->next_empty) {
                    last = last->next_empty;
                }
                last->next_empty = pages_buffer.load(std::memory_order_relaxed);
                while(!pages_buffer.compare_exchange_weak(last->next_empty, pages, std::memory_order_relaxed));
            }
            if (empty_pages) {
                _free(empty_pages);
            }
        }
    };
}
