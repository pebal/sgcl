//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/array.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/make_tracked.h"
#include "../../core/vector.h"
#include "../../core/slice.h"
#include "../../core/string.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace sgcl::io {
    namespace detail {
        // The bytes of a string, as a slice that holds it; of a text
        // slice, the same owner; of a literal, none
        SGCL_INLINE_HOT slice<const byte> bytes_of(const string& s) noexcept {
            return as_bytes(s.as_slice());
        }

        SGCL_INLINE_HOT slice<const byte> bytes_of(const slice<const char>& s) noexcept {
            return as_bytes(s);
        }

        SGCL_INLINE_HOT slice<const byte> bytes_of(const char* s) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(s), std::char_traits<char>::length(s));
        }

        SGCL_INLINE_HOT slice<const byte> bytes_of(std::string_view s) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
        }

        // The bytes as characters: a std view (for the algorithms), a
        // slice of the same owner, a new string
        SGCL_INLINE_HOT std::string_view chars_of(const slice<const byte>& b) noexcept {
            return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
        }

        SGCL_INLINE_HOT slice<const char> text_slice_of(const slice<const byte>& b) noexcept {
            return slice<const char>(b.owner(), reinterpret_cast<const char*>(b.data()), b.size());
        }

        SGCL_INLINE_HOT string text_of(const slice<const byte>& b) {
            return string(chars_of(b));
        }

        // The block copy() moves data through. A call that does not wait
        // keeps it on its stack (StackCopyBlock, 32 KB; a thread of the
        // library has 512 KB at least): every read into it is over when the
        // call returns. A task's is managed (CopyBlock): the read it hands
        // the block to may run on the blocking pool and outlive the frame of
        // a task let go of meanwhile, so the slice the read is given holds
        // the block, and the pool never writes into freed memory.
        using CopyBlock = array<byte, config::io_copy_buffer_size>;

        // io's block (8 KB): a buffered reader's, and the first of an
        // async_read_all
        using IoBlock = array<byte, config::io_buffer_size>;
        using StackCopyBlock = std::array<byte, config::io_copy_buffer_size>;

        // Plain memory handed to a task's write: a copy in a managed block
        // that the slice the write is given holds. An async operation is
        // never given a slice without an owner into plain memory: a write
        // of a stream with no async_write of its own runs on the blocking
        // pool, which may outlive the frame of a task let go of, and would
        // read the memory freed with it (a writer's gathered text, in the
        // frame). The block is 8 or 32 KB, past that a buffer of the
        // write's size; kept for the next write once the one before is over
        // (done()), a new one made while it may still be read.
        class AsyncStage {
        public:
            SGCL_INLINE_HOT slice<const byte> stage(const slice<const byte>& data) noexcept {
                const size_t n = data.size();
                if (_in_flight || _block.size() < n) {
                    _block = _room(n);
                }
                if (n) {
                    sgcl::detail::copy_bytes(_block.data(), data.data(), n);
                }
                _in_flight = true;
                return slice<const byte>(_block.owner(), _block.data(), n);
            }

            // The write of the last stage is over: its block may be used again
            SGCL_INLINE_HOT void done() noexcept {
                _in_flight = false;
            }

            // A managed block of n bytes at least: 8 KB, 32 KB, or a buffer of n
            SGCL_INLINE_HOT static slice<byte> room(size_t n) noexcept {
                return _room(n);
            }

        private:
            static slice<byte> _room(size_t n) noexcept {
                if (n <= config::io_buffer_size) {
                    tracked_ptr b = make_tracked<IoBlock>();
                    return slice<byte>(tracked_ptr<const void>(b), b->data(), b->size());
                }
                if (n <= config::io_copy_buffer_size) {
                    tracked_ptr b = make_tracked<CopyBlock>();
                    return slice<byte>(tracked_ptr<const void>(b), b->data(), b->size());
                }
                vector<byte> v(n);
                return v.as_slice();
            }

            slice<byte> _block;
            bool _in_flight = false;
        };
    }
}
