//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../io/buffered.h"
#include "../io/error.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // A number of N bytes read from and written to bytes in one order.
        // A loop of bytes and shifts, which the compiler turns into one
        // load or store and, for the order the processor does not use, one
        // byte swap.
        template<bool Big>
        class ByteOrder {
        public:
            // The number at the front of the bytes, which hold at least its
            // size (a precondition, as for operator[]: checked by assert)
            static uint16_t read_u16(const slice<const byte>& at) noexcept {
                return uint16_t(_read(at, 2));
            }

            static uint32_t read_u32(const slice<const byte>& at) noexcept {
                return uint32_t(_read(at, 4));
            }

            static uint64_t read_u64(const slice<const byte>& at) noexcept {
                return _read(at, 8);
            }

            // The number into the front of the bytes, which hold its size
            static void write_u16(const slice<byte>& at, uint16_t v) noexcept {
                _write(at, v, 2);
            }

            static void write_u32(const slice<byte>& at, uint32_t v) noexcept {
                _write(at, v, 4);
            }

            static void write_u64(const slice<byte>& at, uint64_t v) noexcept {
                _write(at, v, 8);
            }

            // The number added at the back of the vector
            static void append_u16(vector<byte>& out, uint16_t v) {
                _append(out, v, 2);
            }

            static void append_u32(vector<byte>& out, uint32_t v) {
                _append(out, v, 4);
            }

            static void append_u64(vector<byte>& out, uint64_t v) {
                _append(out, v, 8);
            }

        protected:
            ByteOrder() = default;

        private:
            static uint64_t _read(const slice<const byte>& at, size_t n) noexcept {
                assert(at.size() >= n);
                auto p = reinterpret_cast<const uint8_t*>(at.data());
                uint64_t v = 0;
                for (size_t i = 0; i < n; ++i) {
                    v |= uint64_t(p[i]) << (8 * (Big ? n - 1 - i : i));
                }
                return v;
            }

            static void _put(uint8_t* p, uint64_t v, size_t n) noexcept {
                for (size_t i = 0; i < n; ++i) {
                    p[i] = uint8_t(v >> (8 * (Big ? n - 1 - i : i)));
                }
            }

            static void _write(const slice<byte>& at, uint64_t v, size_t n) noexcept {
                assert(at.size() >= n);
                _put(reinterpret_cast<uint8_t*>(at.data()), v, n);
            }

            static void _append(vector<byte>& out, uint64_t v, size_t n) {
                uint8_t b[8];
                _put(b, v, n);
                auto first = reinterpret_cast<const byte*>(b);
                out.insert(out.end(), first, first + n);
            }
        };
    }

    // The numbers of a binary format in the order it gives them: network
    // order, PNG, Java's streams (big_endian); the processors of today,
    // ZIP, most file formats of the PC (little_endian). read_ takes the
    // number from the front of the bytes, write_ puts it there, append_
    // adds it at the back of a vector:
    //
    //     big_endian::write_u32(header, 0xCAFEBABE);
    //     auto size = little_endian::read_u32(entry.subspan(18));
    class big_endian final
    : public detail::ByteOrder<true> {
    };

    class little_endian final
    : public detail::ByteOrder<false> {
    };

    // A variable-length integer as Go and Protocol Buffers write it
    // (LEB128 without a sign): seven bits a byte, the low ones first, the
    // high bit set on every byte but the last; a small number is one byte,
    // the largest ten. A signed one is zigzagged first, 0, -1, 1, -2 to 0,
    // 1, 2, 3, so that a small negative number is short too.
    //
    // A read of more than 64 bits (a tenth byte above 1, or an eleventh)
    // is out_of_range at the byte that makes it so; bytes that end before
    // the number does are unexpected_end at their end. A longer encoding
    // of a number than it needs (0x80 0x00 for 0) is read, as Go reads it.
    class varint {
    public:
        using error = encoding::error;

        static constexpr size_t max_size = 10;

        static void append(vector<byte>& out, uint64_t v) {
            uint8_t b[max_size];
            size_t n = _put(b, v);
            auto first = reinterpret_cast<const byte*>(b);
            out.insert(out.end(), first, first + n);
        }

        static void append_signed(vector<byte>& out, int64_t v) {
            append(out, _zigzag(v));
        }

        // Into the front of the bytes, which hold the number (max_size
        // always does): the bytes written
        static size_t write(const slice<byte>& at, uint64_t v) noexcept {
            uint8_t b[max_size];
            size_t n = _put(b, v);
            assert(at.size() >= n);
            auto p = reinterpret_cast<uint8_t*>(at.data());
            for (size_t i = 0; i < n; ++i) {
                p[i] = b[i];
            }
            return n;
        }

        static size_t write_signed(const slice<byte>& at, int64_t v) noexcept {
            return write(at, _zigzag(v));
        }

        // The number at the front of the bytes and the bytes it took
        static expected<pair<uint64_t, size_t>, error> read(const slice<const byte>& at) {
            auto p = reinterpret_cast<const uint8_t*>(at.data());
            size_t n = at.size() < max_size ? at.size() : max_size;
            uint64_t v = 0;
            for (size_t i = 0; i < n; ++i) {
                uint8_t b = p[i];
                if (i == max_size - 1 && b > 1) {
                    return unexpected<error>(error(errc::out_of_range, i, string("a varint past 64 bits")));
                }
                v |= uint64_t(b & 0x7F) << (7 * i);
                if (b < 0x80) {
                    return pair<uint64_t, size_t>(v, i + 1);
                }
            }
            return unexpected<error>(error(errc::unexpected_end, at.size(), string("the bytes end inside a varint")));
        }

        static expected<pair<int64_t, size_t>, error> read_signed(const slice<const byte>& at) {
            auto r = read(at);
            if (!r) {
                return unexpected<error>(std::move(r.error()));
            }
            return pair<int64_t, size_t>(_unzigzag(r->first), r->second);
        }

        // The next number of a stream: nullopt at the end of the stream
        // before its first byte (a loop over a stream of numbers ends
        // there), io::errc::unexpected_eof when the stream ends inside
        // one, out_of_range of the encoding category past 64 bits
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        static expected<optional<uint64_t>, io::error> read(io::buffered_reader& in) {
            return _block_read(in);
        }

        // The reader by reference, as io's functions take a stream: the read
        // moves its position on, and a copy would be another reader. A reader
        // in a managed object is kept by the task's frame, which the
        // collector traces conservatively (core/coroutine.h); one on the
        // caller's stack must outlive the task
        static async::task<expected<optional<uint64_t>, io::error>> async_read(io::buffered_reader& in) {
            return _co_read(in);
        }

        // `read_signed(...)` on this thread, `co_await async_read_signed(...)` in a task
        static expected<optional<int64_t>, io::error> read_signed(io::buffered_reader& in) {
            return _block_read_signed(in);
        }

        static async::task<expected<optional<int64_t>, io::error>> async_read_signed(io::buffered_reader& in) {
            return _co_read_signed(in);
        }

    private:
        static uint64_t _zigzag(int64_t v) noexcept {
            return uint64_t(v) << 1 ^ uint64_t(v >> 63);
        }

        static int64_t _unzigzag(uint64_t u) noexcept {
            return int64_t(u >> 1 ^ (0 - (u & 1)));
        }

        static size_t _put(uint8_t* b, uint64_t v) noexcept {
            size_t n = 0;
            while (v >= 0x80) {
                b[n++] = uint8_t(v) | 0x80;
                v >>= 7;
            }
            b[n++] = uint8_t(v);
            return n;
        }

        // One step of a read from a stream, the same for both forms: the
        // read of the byte i and the byte, and then the number when it ends
        // there, the error of the read or of the number, or nothing to go
        // on. The end of the stream is the end of the numbers before the
        // first byte (nullopt) and unexpected_eof inside one; the tenth
        // byte ends the number or fails, so the loop is bounded.
        static optional<expected<optional<uint64_t>, io::error>> _step(uint64_t& v, size_t i, const expected<size_t, io::error>& r, byte byte) {
            using out = expected<optional<uint64_t>, io::error>;
            if (!r) {
                return out(io::detail::fail(r));
            }
            if (*r == 0) {
                if (i == 0) {
                    return out(optional<uint64_t>());
                }
                return out(io::detail::fail(io::error(io::errc::unexpected_eof, "read", "varint")));
            }
            auto b = uint8_t(byte);
            if (i == max_size - 1 && b > 1) {
                return out(io::detail::fail(io::error(make_error_code(errc::out_of_range), "read", "varint")));
            }
            v |= uint64_t(b & 0x7F) << (7 * i);
            if (b < 0x80) {
                return out(optional<uint64_t>(v));
            }
            return nullopt;
        }

        static expected<optional<int64_t>, io::error> _signed(const expected<optional<uint64_t>, io::error>& r) {
            if (!r) {
                return io::detail::fail(r);
            }
            if (!*r) {
                return optional<int64_t>();
            }
            return optional<int64_t>(_unzigzag(**r));
        }

        // the two halves of the operations above: a thread's and a task's
        static expected<optional<uint64_t>, io::error> _block_read(io::buffered_reader& in)  {
            uint64_t v = 0;
            for (size_t i = 0;; ++i) {
                byte b {};
                if (auto r = _step(v, i, in.read(slice<byte>(&b, 1)), b)) {
                    return std::move(*r);
                }
            }
        }

        static expected<optional<int64_t>, io::error> _block_read_signed(io::buffered_reader& in)  {
            return _signed(_block_read(in));
        }

        static async::task<expected<optional<uint64_t>, io::error>> _co_read(io::buffered_reader& in)  {
            uint64_t v = 0;
            for (size_t i = 0;; ++i) {
                byte b {};
                if (auto r = _step(v, i, co_await in.async_read(slice<byte>(&b, 1)), b)) {
                    co_return std::move(*r);
                }
            }
        }

        static async::task<expected<optional<int64_t>, io::error>> _co_read_signed(io::buffered_reader& in)  {
            co_return _signed(co_await _co_read(in));
        }
    };
}
