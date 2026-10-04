//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/tracked_ptr.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace sgcl::codec::detail {
    // Where a decoder's bytes come from, one shape for memory and for a
    // stream, so that a decoder is written once over it:
    //   peek(want, at, got)  up to `want` bytes readable in place at `at`
    //                        (fewer when fewer are at hand; got 0 only at
    //                        the end); false when the source failed, the
    //                        error in failure
    //   consume(n)           the first n of them taken
    //   offset()             the bytes taken so far, for an error's offset
    // Memory is read in place, never copied; a stream through one block of
    // its own.

    struct MemoryInput {
        const uint8_t* begin;
        const uint8_t* at;
        const uint8_t* end;
        optional<error> failure;   // never set: memory does not fail

        SGCL_INLINE_HOT explicit MemoryInput(const slice<const byte>& data) noexcept
        : begin(reinterpret_cast<const uint8_t*>(data.data()))
        , at(begin)
        , end(begin + data.size()) {
        }

        SGCL_INLINE_HOT bool peek(size_t want, const uint8_t*& p, size_t& got) noexcept {
            p = at;
            got = std::min<size_t>(want, size_t(end - at));
            return true;
        }

        SGCL_INLINE_HOT void consume(size_t n) noexcept {
            at += n;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return uint64_t(at - begin);
        }

        // The first bytes, for sniffing: what there is, up to n
        SGCL_INLINE_HOT slice<const byte> head(size_t n) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(at), std::min<size_t>(n, size_t(end - at)));
        }
    };

    // The block a stream is read into: managed (the stream may be a task's,
    // whose read outlives a frame let go of), a type of its own, whole pages
    struct InputBlock {
        static constexpr size_t Size = 32768;
        static_assert(config::page_size % Size == 0, "a block of whole pages");
        byte bytes[Size];
    };

    struct ReaderInput {
        io::reader in;
        tracked_ptr<InputBlock> block;
        size_t pos = 0;
        size_t len = 0;
        uint64_t base = 0;   // the offset of the block's first byte
        bool eof = false;
        optional<error> failure;

        SGCL_INLINE_HOT explicit ReaderInput(const io::reader& r) noexcept
        : in(r)
        , block(make_tracked<InputBlock>()) {
        }

        bool peek(size_t want, const uint8_t*& p, size_t& got) {
            if (pos == len && !eof) {
                base += len;
                pos = len = 0;
                if (!_read_more()) {
                    return false;
                }
            }
            p = _data() + pos;
            got = std::min(want, len - pos);
            return true;
        }

        SGCL_INLINE_HOT void consume(size_t n) noexcept {
            pos += n;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return base + pos;
        }

        // The first bytes, for sniffing, read until n are held or the
        // stream ends; nullopt when it failed
        optional<slice<const byte>> head(size_t n) {
            if (pos > 0) {
                // the bytes kept overlap where they go when fewer are
                // dropped than kept
                sgcl::detail::move_bytes(_data(), _data() + pos, len - pos);
                base += pos;
                len -= pos;
                pos = 0;
            }
            n = std::min(n, InputBlock::Size);
            while (len < n && !eof) {
                if (!_read_more()) {
                    return nullopt;
                }
            }
            return slice<const byte>(reinterpret_cast<const byte*>(_data()), std::min(n, len));
        }

    private:
        SGCL_INLINE_HOT uint8_t* _data() noexcept {
            return reinterpret_cast<uint8_t*>(block->bytes);
        }

        // More bytes after the len held: false when the stream failed
        bool _read_more() {
            auto r = in.read(slice<byte>(tracked_ptr<const void>(block), block->bytes + len, InputBlock::Size - len));
            if (!r) {
                failure = error(r.error(), base + len);
                return false;
            }
            if (*r == 0) {
                eof = true;
            }
            len += *r;
            return true;
        }
    };

    // Whether reading an input can throw: memory's peek cannot, a stream's
    // reaches the program's io::reader. A decoder over an input is
    // noexcept(NothrowInput<Input>) where reading is its only throw
    template<class Input>
    inline constexpr bool NothrowInput = noexcept(std::declval<Input&>().peek(size_t(), std::declval<const uint8_t*&>(), std::declval<size_t&>()));
}

