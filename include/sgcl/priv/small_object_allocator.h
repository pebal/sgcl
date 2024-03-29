//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "small_object_allocator_base.h"

namespace sgcl {
    namespace Priv {
        template<class T>
        struct Small_object_allocator : Small_object_allocator_base {
            using Info = Type_info<T>;
            using Type = typename Info::type;
            using Pointer_pool = Priv::Pointer_pool<Info::ObjectCount, sizeof(std::conditional_t<std::is_same_v<Type, void>, char, Type>)>;

            constexpr Small_object_allocator(Block_allocator& a) noexcept
                : Small_object_allocator_base(a, _pointer_pool, _pages_buffer, _lock) {
            }

            static void free(Page* pages) {
                _free(pages, _pages_buffer, _lock);
            }

        private:
            inline static Page* _pages_buffer = {nullptr};
            inline static std::atomic_flag _lock = ATOMIC_FLAG_INIT;
            Pointer_pool _pointer_pool;

            Page* _create_page_parameters(Data_page* data) override {
                auto mem = ::operator new(Info::HeaderSize);
                auto page = new(mem) Page(data->block, (Type*)data->data);
                return page;
            }
        };
    }
}
