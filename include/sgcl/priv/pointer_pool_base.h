//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "page.h"

namespace sgcl {
    namespace Priv {
        struct Pointer_pool_base {
            Pointer_pool_base(unsigned s, unsigned o)
                : _size(s)
                , _offset(o)
                , _position(s) {
            }
            void fill(void* data) {
                for(auto i = 0u; i < _size; ++i, data = (void*)((uintptr_t)data + _offset)) {
                    _indexes[i] = data;
                }
                _position = 0;
            }
            void fill(Page* page) {
                std::atomic_thread_fence(std::memory_order_acquire);
                auto data = page->data;
                auto object_size = page->metadata->object_size;
                auto states = page->states();
                auto count = page->metadata->object_count;
                [[maybe_unused]] bool unused = false;
                for(int i = count - 1; i >= 0; --i) {
                    if (states[i].load(std::memory_order_relaxed) == State::Unused) {
                        _indexes[--_position] = (void*)(data + i * object_size);
                        states[i].store(State::Reserved, std::memory_order_relaxed);
                        unused = true;
                    }
                }
                std::atomic_thread_fence(std::memory_order_release);
                assert(unused);
            }
            unsigned pointer_count() const noexcept {
                return _size - _position;
            }
            bool is_empty() const noexcept {
                return _position == _size;
            }
            bool is_full() const noexcept {
                return _position == 0;
            }
            void* alloc() noexcept {
                return _indexes[_position++];
            }
            void free(void* p) noexcept {
                _indexes[--_position] = p;
            }

        protected:
            void** _indexes;

        private:
            const unsigned _size;
            const unsigned _offset;
            unsigned _position;
        };
    }
}
