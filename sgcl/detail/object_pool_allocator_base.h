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
        ObjectPoolAllocatorBase(BlockAllocator& ba, std::atomic<Page*>& pages, PointerPoolBase& pa, Page*& pb, std::atomic_flag& lock) noexcept
        : ObjectAllocatorBase(pages)
        , _block_allocator(ba)
        , _pointer_pool(pa)
        , _pages_buffer(pb)
        , _lock(lock) {
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
                _current_page->unused_occur.store(true, std::memory_order_release);
            }
            std::atomic_thread_fence(std::memory_order_release);
        }

        void* alloc(size_t = 0) {
            if  (_pointer_pool.is_empty()) {
                Page* page = nullptr;
                if (!_lock.test_and_set(std::memory_order_acquire)) {
                    page = _pages_buffer;
                    if (page) {
                        _pages_buffer = page->next_empty;
                    }
                    _lock.clear(std::memory_order_release);
                }
                if (page) {
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
        Page*& _pages_buffer;
        std::atomic_flag& _lock;
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
            std::atomic_thread_fence(std::memory_order_release);
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

        static void _free(Page* pages, Page*& pages_buffer, std::atomic_flag& lock) noexcept {
            Page* empty_pages = nullptr;
            for (int i = 0; i < 2 ; ++i) {
                _remove_empty(pages, empty_pages);
                while (lock.test_and_set(std::memory_order_acquire));
                std::swap(pages, pages_buffer);
                lock.clear(std::memory_order_release);
            }
            if (pages) {
                auto last = pages;
                while(last->next_empty) {
                    last = last->next_empty;
                }
                while (lock.test_and_set(std::memory_order_acquire));
                last->next_empty = pages_buffer;
                pages_buffer = pages;
                lock.clear(std::memory_order_release);
            }
            if (empty_pages) {
                _free(empty_pages);
            }
        }
    };
}
