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
            using Pointer_pool = Priv::Pointer_pool<Page_info<T>::ObjectCount, sizeof(std::conditional_t<std::is_same_v<std::remove_cv_t<T>, void>, char, T>)>;

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

            Page* _create_page_parameters(DataPage* data) override {
                auto mem = ::operator new(Page_info<T>::HeaderSize);
                auto page = new(mem) Page(data->block, (T*)data->data);
                return page;
            }
        };
    }
}
