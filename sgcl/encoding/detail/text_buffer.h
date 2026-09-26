//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/array.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/detail/maker.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // Characters in managed memory that grow: the block a reader of a
    // text format reads its input into, and the buffer a decoded token is
    // laid out in, both handed out as slices that hold them. The first
    // capacity is the io block (8 KB of array<byte, N>, eight to a
    // page, the pool io's buffers use); a text that outgrows it moves to a
    // managed buffer of twice the size, and so on — a token longer than
    // the block is the exception, never the rule. Growing keeps the
    // characters; a slice handed out before still holds the old buffer.
    class TextBuffer {
    public:
        using Block = array<byte, config::io_buffer_size>;

        char* data() noexcept {
            return _data;
        }

        const char* data() const noexcept {
            return _data;
        }

        size_t size() const noexcept {
            return _size;
        }

        size_t capacity() const noexcept {
            return _capacity;
        }

        const tracked_ptr<const void>& owner() const noexcept {
            return _owner;
        }

        void clear() noexcept {
            _size = 0;
        }

        // The size set by the caller that wrote into [size(), capacity())
        void resize(size_t n) noexcept {
            _size = n;
        }

        void push_back(char c) {
            if (_size == _capacity) {
                reserve(_size + 1);
            }
            _data[_size++] = c;
        }

        void append(const char* p, size_t n) {
            if (n > _capacity - _size) {
                reserve(_size + n);
            }
            copy_bytes(_data + _size, p, n);
            _size += n;
        }

        void append(std::string_view s) {
            append(s.data(), s.size());
        }

        // Room for n characters at least, what is there kept
        void reserve(size_t n) {
            if (n <= _capacity) {
                return;
            }
            if (_capacity == 0 && n <= config::io_buffer_size) {
                auto block = make_tracked<Block>();
                char* p = reinterpret_cast<char*>(block->data());
                _owner = tracked_ptr<const void>(std::move(block));
                _data = p;
                _capacity = config::io_buffer_size;
                return;
            }
            size_t capacity = std::max({n, _capacity * 2, size_t(config::io_buffer_size)});
            auto buffer = unique_ptr<char>(Maker<char[]>::make_tracked_data(capacity));
            char* p = buffer.get();
            copy_bytes(p, _data, _size);
            _owner = tracked_ptr<const void>(std::move(buffer));
            _data = p;
            _capacity = capacity;
        }

        // The characters [from, size()) moved to the front
        void drop_front(size_t from) noexcept {
            if (from) {
                std::memmove(_data, _data + from, _size - from);
                _size -= from;
            }
        }

        slice<const char> view(size_t from, size_t n) const noexcept {
            return slice<const char>(_owner, _data + from, n);
        }

    private:
        tracked_ptr<const void> _owner;
        char* _data = nullptr;
        size_t _size = 0;
        size_t _capacity = 0;
    };
}
