//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <mutex>
#include <new>

namespace sgcl::detail {
    // Fixed-size allocator for page headers of one type. Headers are taken by
    // mutator threads once per page and returned by the collector, so a mutex
    // is fine here; headers of one type end up in adjacent cache lines, which
    // marking appreciates. Memory is never returned to the system: the number
    // of headers is bounded by the number of pages ever in use at once.
    class HeaderSlab {
    public:
        static constexpr size_t Alignment = config::CacheLineSize;
        static constexpr size_t BlockSize = 0x10000;

        explicit HeaderSlab(size_t size) noexcept
        : _size((size + Alignment - 1) & ~(Alignment - 1)) {
        }

        void* alloc() {
            std::lock_guard<std::mutex> lock(_mutex);
            return _alloc_locked();
        }

        // Several at once for the per-thread caches: one lock per batch.
        void alloc(void** out, unsigned count) {
            std::lock_guard<std::mutex> lock(_mutex);
            for (unsigned i = 0; i < count; ++i) {
                out[i] = _alloc_locked();
            }
        }

        void free(void* p) noexcept {
            std::lock_guard<std::mutex> lock(_mutex);
            auto node = (Node*)p;
            node->next = _free;
            _free = node;
        }

    private:
        struct Node {
            Node* next;
        };

        void* _alloc_locked() {
            if (_free) {
                auto node = _free;
                _free = node->next;
                return node;
            }
            if (_left < _size) {
                auto bytes = _size > BlockSize ? _size : BlockSize;
                _next = (char*)::operator new(bytes, std::align_val_t(Alignment));
                _left = bytes;
            }
            auto p = _next;
            _next += _size;
            _left -= _size;
            return p;
        }

        const size_t _size;
        std::mutex _mutex;
        Node* _free = nullptr;
        char* _next = nullptr;
        size_t _left = 0;
    };
}
