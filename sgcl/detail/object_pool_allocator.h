//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cell_block.h"
#include "object_pool_allocator_base.h"

namespace sgcl::detail {
    template<class T>
    class ObjectPoolAllocator : public ObjectPoolAllocatorBase {
    public:
        using ValueType = typename TypeInfo<T>::Type;
        using IsPoolAllocator = std::true_type;

        constexpr ObjectPoolAllocator(PageAllocator& a, std::atomic<Page*>& pages) noexcept
            : ObjectPoolAllocatorBase(a, pages, _pages_buffer) {
        }

        ~ObjectPoolAllocator() noexcept override {
            auto& slab = TypeInfo<T>::header_slab();
            while (_header_count) {
                slab.free(_headers[--_header_count]);
            }
        }

        static void free(Page* pages) noexcept {
            _free(pages, _pages_buffer);
        }

    private:
        inline static std::atomic<Page*> _pages_buffer = {nullptr};

        // Headers cached per thread and type, refilled from the slab in
        // batches so that page turnover on many threads does not serialize
        // on the slab mutex.
        static constexpr unsigned HeaderCacheSize = 8;
        void* _headers[HeaderCacheSize];
        unsigned _header_count = 0;

        Page* _create_page_parameters(void* data) override {
            if (!_header_count) {
                TypeInfo<T>::header_slab().alloc(_headers, HeaderCacheSize);
                _header_count = HeaderCacheSize;
            }
            auto mem = _headers[--_header_count];
            // a page of blocks of cells: every word its own address, the
            // free state of every slot of every block, once (cell_block.h)
            if constexpr(std::is_same_v<ValueType, CellBlock>) {
                CellBlock::fill_page(data, config::PageSize);
            }
            return new(mem) Page((ValueType*)data);
        }
    };
}
