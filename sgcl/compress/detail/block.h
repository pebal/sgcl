//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/vector.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::compress::detail {
    // The memory a task's read or write is given. The operation may run on
    // the blocking pool (a stream with no async operation of its own, a
    // file read at an offset) and outlive the frame, or the object, of a
    // task let go of meanwhile: its bytes are managed and the slice it is
    // given holds them, so that the pool never writes into (or reads from)
    // freed memory (io's rule, io/detail/bytes.h). A thread's call is over
    // when it returns: its memory stays plain.

    // A managed block of N bytes, N a divisor of the page (whole pages).
    // A struct of its own rather than sgcl::array<byte, N>, whose type
    // carries an index sequence of N numbers: its name, in the type's
    // RTTI and in every symbol of the functions made for it, is some
    // 200 KB at 32 KB (three such blocks added 3.7 MB of text to a program)
    template<size_t N>
    struct ByteBlock {
        static_assert(config::page_size % N == 0, "a block of whole pages");
        byte bytes[N];

        byte* data() noexcept {
            return bytes;
        }
    };

    // n managed bytes: up to Block, the smallest block that holds them of
    // 1 KB, 8 KB and Block bytes (a small zip entry's input takes no more);
    // past it, a managed vector's buffer (a gzip header or a pax record
    // past the block, a central directory)
    template<size_t Block>
    slice<byte> managed_bytes(size_t n) {
        static_assert(Block <= 32768, "a block the largest object holds");
        if constexpr (Block > 1024) {
            if (n <= 1024) {
                return managed_bytes<1024>(n);
            }
        }
        if constexpr (Block > 8192) {
            if (n <= 8192) {
                return managed_bytes<8192>(n);
            }
        }
        if (n <= Block) {
            tracked_ptr<ByteBlock<Block>> b = make_tracked<ByteBlock<Block>>();
            return slice<byte>(tracked_ptr<const void>(b), b->data(), n);
        }
        vector<byte> v(n);
        return v.as_slice();
    }

    // A reader's input: plain memory while a thread reads into it (made by
    // its first read), moved once into managed memory by the first read of
    // a task (to_managed), and managed from then on, grown or shrunk there;
    // the bytes held are kept either way. A reader only ever read by tasks
    // makes no plain memory at all. Every byte is written through room(),
    // so the memory is made there; before, data() points at nothing to read.
    template<size_t Block>
    class InputBuffer {
    public:
        explicit InputBuffer(size_t n) noexcept
        : _size(n) {
        }

        bool managed() const noexcept {
            return _managed.owned();
        }

        uint8_t* data() noexcept {
            return managed() ? reinterpret_cast<uint8_t*>(_managed.data()) : _plain.empty() ? &_none : _plain.data();
        }

        size_t size() const noexcept {
            return managed() ? _managed.size() : _size;
        }

        // n bytes, the first `keep` of them kept; plain memory given back
        // when it shrinks
        void resize(size_t n, size_t keep) {
            if (managed()) {
                if (n != _managed.size()) {
                    _replace(n, keep);
                }
                return;
            }
            if (!_plain.empty()) {
                bool shrink = n < _plain.size();
                _plain.resize(n);
                if (shrink) {
                    _plain.shrink_to_fit();
                }
            }
            _size = n;
        }

        // Managed from here on (a task's read comes), the first `keep`
        // bytes kept: a block of Block bytes, or more when keep needs it
        void to_managed(size_t keep) {
            if (!managed()) {
                _replace(std::max(std::min(_size, Block), keep), keep);
                _plain = std::vector<uint8_t>();
            }
        }

        // The bytes [from, from + n), for a read into them: a slice that
        // holds them when they are managed
        slice<byte> room(size_t from, size_t n) {
            if (managed()) {
                return _managed.subslice(from, n);
            }
            if (_plain.empty()) {
                _plain.resize(_size);
            }
            return slice<byte>(reinterpret_cast<byte*>(_plain.data() + from), n);
        }

    private:
        void _replace(size_t n, size_t keep) {
            auto s = managed_bytes<Block>(n);
            size_t held = managed() ? _managed.size() : _plain.size();
            if (size_t k = std::min({keep, n, held})) {
                sgcl::detail::copy_bytes(s.data(), data(), k);
            }
            _managed = s;
        }

        std::vector<uint8_t> _plain;   // a thread's, made by its first read
        size_t _size;                  // the plain memory's size, made or not
        slice<byte> _managed;          // the managed bytes, and their owner
        static inline uint8_t _none = 0;
    };

    // The encoder's output where a task's write takes it from: a managed
    // block the bytes are appended to in place and given to the write as
    // they are, a slice of the block (no copy into a stage); grown by
    // doubling (the bytes so far copied then), kept from one write to the
    // next. The interface is the part of std::vector<uint8_t> the formats
    // and the encoder use: push_back, insert at the end, size, clear.
    class ManagedOutput {
    public:
        struct End {};

        End end() const noexcept {
            return {};
        }

        size_t size() const noexcept {
            return _n;
        }

        bool empty() const noexcept {
            return _n == 0;
        }

        void clear() noexcept {
            _n = 0;
        }

        void push_back(uint8_t b) {
            _room(_n + 1);
            _block.data()[_n++] = byte(b);
        }

        template<class It>
        void insert(End, It first, It last) {
            const size_t n = size_t(std::distance(first, last));
            _room(_n + n);
            std::copy(first, last, reinterpret_cast<uint8_t*>(_block.data()) + _n);
            _n += n;
        }

        void append(const uint8_t* p, size_t n) {
            _room(_n + n);
            sgcl::detail::copy_bytes(_block.data() + _n, p, n);
            _n += n;
        }

        // The bytes so far, owned by the block
        slice<const byte> bytes() const noexcept {
            return slice<const byte>(_block).first(_n);
        }

    private:
        void _room(size_t need) {
            if (need > _block.size()) {
                const size_t cap = std::max({need, 2 * _block.size(), size_t(32768)});
                slice<byte> grown = managed_bytes<32768>(cap);
                sgcl::detail::copy_bytes(grown.data(), _block.data(), _n);
                _block = grown;
            }
        }

        slice<byte> _block;
        size_t _n = 0;
    };

    inline void append_bytes(ManagedOutput& out, const uint8_t* p, size_t n) {
        out.append(p, n);
    }

    inline void append_byte(ManagedOutput& out, uint8_t b) {
        out.push_back(b);
    }

    // A writer's bytes for a task's write: copied into a managed block the
    // writer keeps, made by the first such write and reused by the ones
    // after, each of which is over before the next is staged. The block is
    // 1, 8 or 32 KB, the smallest that holds the write (a small entry of an
    // archive takes no more), and past 32 KB a buffer of the write's size;
    // it grows when a write needs more.
    class OutputStage {
    public:
        slice<const byte> stage(const uint8_t* p, size_t n) {
            _room(n);
            sgcl::detail::copy_bytes(_block.data(), p, n);
            return _block.first(n);
        }

        slice<const byte> stage(const std::vector<uint8_t>& v) {
            return stage(v.data(), v.size());
        }

        // Two pieces one after another, in one write
        slice<const byte> stage(const std::vector<uint8_t>& a, const slice<const byte>& b) {
            _room(a.size() + b.size());
            sgcl::detail::copy_bytes(_block.data(), a.data(), a.size());
            sgcl::detail::copy_bytes(_block.data() + a.size(), b.data(), b.size());
            return _block.first(a.size() + b.size());
        }

    private:
        void _room(size_t n) {
            if (_block.size() < n) {
                n = std::max(n, 2 * _block.size());
                _block = n <= 1024 ? managed_bytes<1024>(1024) : n <= 8192 ? managed_bytes<8192>(8192) : managed_bytes<32768>(std::max<size_t>(n, 32768));
            }
        }

        slice<byte> _block;
    };
}
