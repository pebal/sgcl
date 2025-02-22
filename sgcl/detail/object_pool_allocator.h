//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "object_pool_allocator_base.h"

namespace sgcl::detail {
    template<class T>
    class ObjectPoolAllocator : public ObjectPoolAllocatorBase {
    public:
        using ValueType = typename TypeInfo<T>::Type;
        using is_pool_allocator = std::true_type;

        constexpr ObjectPoolAllocator(BlockAllocator& a) noexcept
            : ObjectPoolAllocatorBase(a, _pointer_pool, _pages_buffer, _lock) {
        }

        static void free(Page* pages) noexcept {
            _free(pages, _pages_buffer, _lock);
        }

    private:
        using PointerPool = detail::PointerPool<TypeInfo<T>::ObjectCount, sizeof(std::conditional_t<std::is_void_v<ValueType>, char, ValueType>)>;

        PointerPool _pointer_pool;
        inline static Page* _pages_buffer = {nullptr};
        inline static std::atomic_flag _lock = ATOMIC_FLAG_INIT;

        Page* _create_page_parameters(DataPage* data) override {
            auto mem = ::operator new(TypeInfo<T>::HeaderSize);
            auto page = new(mem) Page(data->block, (ValueType*)data->data);
            return page;
        }
    };
}
