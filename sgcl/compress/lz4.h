//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/codec_stream.h"
#include "detail/lz4_frame.h"
#include "../io/detail/bytes.h"

#include <stdexcept>
#include <string>

namespace sgcl::compress {
    // LZ4 (.lz4): the frame format of lz4 1.6 and later, and the block
    // format alone. The fastest of the module's formats both ways, at a
    // lower ratio than DEFLATE: a byte-aligned LZ77 with no entropy coding.
    // The levels are lz4's own: 1 the fast compressor (the default), 2 two
    // tables, 3..9 hash chains (lz4hc), 10..12 an optimal parser, and -N
    // the fast one with acceleration N (lz4 --fast=N). The frame's options default to
    // what the lz4 command writes: blocks of 4 MB, independent, the
    // content's checksum. Frames one after another are read as one, as
    // lz4 -d reads them, skippable frames passed over, and the legacy
    // frames of lz4 -l read.
    class lz4 {
    public:
        using error = compress::error;

        // How hard the compressor works, on lz4's scale (1 fast, 2 two
        // tables, 3..9 chains, 10..12 the optimal parser, -N acceleration
        // N): an int converts to
        // it, so that options take `{.level = 9}`; a value outside
        // -65537..-1 and 1..12 is invalid_argument (in a constant, an error
        // at compile time)
        class level {
        public:
            static constexpr int fastest = -65537;   // the most acceleration (lz4 --fast=65537)
            static constexpr int standard = 1;       // lz4's default: one probe a position
            static constexpr int high = 9;           // lz4 -9, lz4hc's default
            static constexpr int smallest = 12;      // lz4 -12, the optimal parser at its fullest

            SGCL_INLINE_HOT constexpr level() noexcept
            : _value(standard) {
            }

            SGCL_INLINE_HOT constexpr level(int n)
            : _value(n) {
                if (n < -65537 || n > 12 || n == 0) {
                    throw std::invalid_argument("compress::lz4::level: 1..12, or -1..-65537 for acceleration");
                }
            }

            SGCL_INLINE_HOT constexpr int value() const noexcept {
                return _value;
            }

            friend constexpr bool operator==(level a, level b) noexcept = default;

        private:
            int _value;
        };

        // The largest block of a frame (its BD byte): what a reader holds
        // at once, and the unit a writer compresses
        enum class block_size : uint8_t {
            kb64 = 4,
            kb256 = 5,
            mb1 = 6,
            mb4 = 7
        };

        struct options {
            lz4::level level;
            lz4::block_size block_size = lz4::block_size::mb4;
            bool linked_blocks = false;      // a block may refer to the 64 KB before it (lz4 -BD)
            bool block_checksum = false;     // the XXH32 of every block (lz4 -BX)
            bool content_checksum = true;    // the XXH32 of the content, at the end
            slice<const byte> dictionary;    // compressed against its last 64 KB; read with the same bytes
            uint32_t dictionary_id = 0;      // written in the header when not 0; read: checked when both are
        };

        class writer;
        class reader;

        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data) noexcept {
            return _compress(data, options{});
        }

        // One frame, the content's size in its header. Options out of range
        // are the program's mistake: std::invalid_argument (the writer's
        // first write reports it as errc::invalid_argument)
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            auto s = detail::Lz4Settings::of(o);
            if (s.error) {
                throw std::invalid_argument(std::string("compress::lz4: ") + s.error);
            }
            return _compress(data, o);
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text) noexcept {
            return _compress(io::detail::bytes_of(text), options{});
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text, const options& o) {
            return compress(io::detail::bytes_of(text), o);
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text) noexcept {
            return _compress(slice<const byte>(text), options{});
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text, const options& o) {
            return compress(slice<const byte>(text), o);
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept {
            return decompress(data, options{}, limits{});
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            return decompress(data, options{}, l);
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o) noexcept {
            return decompress(data, o, limits{});
        }

        // Every frame, one after another, up to the limit's size; every
        // checksum the frames carry checked. Of the options only the
        // dictionary and its id are read
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o, const limits& l) noexcept {
            const uint8_t* p = detail::bytes(data);
            if (o.dictionary.empty()) {
                return detail::lz4_decompress_all(p, data.size(), l);
            }
            detail::Lz4FrameDecoder d(o, l);
            return detail::decompress_with(d, p, data.size(), l, data.size() * 4);
        }

        // The block format alone: no header, no checksum, no size, what
        // LZ4_compress_default and LZ4_compress_HC make. Of the options the
        // level and the dictionary are read
        SGCL_INLINE_HOT static vector<byte> compress_block(const slice<const byte>& data) noexcept {
            return compress_block(data, options{});
        }

        static vector<byte> compress_block(const slice<const byte>& data, const options& o) noexcept {
            auto s = detail::Lz4Settings::of(o);
            return detail::lz4_compress_raw(detail::bytes(data), data.size(), s.level, s.dictionary);
        }

        // A block of the block format, into at most max_size bytes (the
        // size the caller kept beside it, as LZ4_decompress_safe's
        // capacity): more is errc::too_large
        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress_block(const slice<const byte>& data, size_t max_size) noexcept {
            return decompress_block(data, max_size, options{});
        }

        static expected<vector<byte>, error> decompress_block(const slice<const byte>& data, size_t max_size, const options& o) noexcept {
            auto s = detail::Lz4Settings::of(o);
            return detail::lz4_decompress_raw(detail::bytes(data), data.size(), max_size, s.dictionary);
        }

    private:
        static vector<byte> _compress(const slice<const byte>& data, const options& o) noexcept {
            const uint8_t* p = detail::bytes(data);
            const size_t n = data.size();
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(detail::lz4_bound(n) + 64);
            if (o.dictionary.empty()) {
                detail::lz4_compress_frame(out, detail::Lz4Settings::of(o), p, n);
            } else {
                detail::Lz4FrameEncoder enc(o);
                const uint64_t size = n;
                enc.start(out, &size);
                enc.write(p, n, out);
                enc.finish(out);
            }
            return detail::to_vector(out.data(), out.size());
        }
    };

    // What is written, compressed into out as one LZ4 frame, a block at a
    // time (the options' block size); flush() writes the block so far, so
    // that a reader can decode everything written. The first failure — of
    // out, or of options out of range — is kept: every write and close
    // after it gives it at once and writes nothing, so a stream may be
    // written freely and checked once, at the close, which writes the end
    // mark and the content's checksum and leaves out open.
    class lz4::writer final
    : public detail::CodecWriter<detail::Lz4FrameEncoder, lz4::options> {
    public:
        writer(writer&&) noexcept = default;   // the other left closed (detail/codec_stream.h)

        SGCL_INLINE_HOT writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit writer(const io::writer& out) noexcept
        : CodecWriter(out, options{}) {
        }

        SGCL_INLINE_HOT writer(const io::writer& out, const options& o) noexcept
        : CodecWriter(out, o) {
        }
    };

    // What the data read from in decompresses to, every frame of it. It
    // reads its input 64 KB at a time, so it may read past the end of the
    // LZ4 data. A block comes out once the whole of it has been read; a
    // failure is the error of the read that reaches it and of every read
    // after, last_error() the whole of it.
    class lz4::reader final
    : public detail::CodecReader<detail::Lz4FrameDecoder, lz4::options> {
    public:
        reader(reader&&) noexcept = default;   // the other left closed (detail/codec_stream.h)

        SGCL_INLINE_HOT reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit reader(const io::reader& in) noexcept
        : CodecReader(in, options{}, limits{}) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, const options& o) noexcept
        : CodecReader(in, o, limits{}) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, const limits& l) noexcept
        : CodecReader(in, options{}, l) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, const options& o, const limits& l) noexcept
        : CodecReader(in, o, l) {
        }
    };
}
