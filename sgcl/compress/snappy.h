//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/codec_stream.h"
#include "detail/snappy.h"
#include "../io/detail/bytes.h"

namespace sgcl::compress {
    // Snappy: Google's byte-aligned LZ77 with no entropy coding and no
    // levels, made for speed. Two formats: the framing format (.sz, HTTP's
    // x-snappy-framed), chunks of up to 64 KB each with the masked CRC-32C
    // of its data, which compress, decompress and the streams speak; and the
    // block format alone, a varint of the length and the elements, which
    // LevelDB, Parquet, Cassandra, Prometheus and gRPC carry, as
    // compress_block and decompress_block. A framed stream may hold several
    // streams one after another; they are read as one.
    class snappy {
    public:
        using error = compress::error;

        class writer;
        class reader;

        static vector<byte> compress(const slice<const byte>& data) noexcept {
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(detail::snappy_bound(data.size()) + data.size() / detail::SnappyFragment * 8 + 16);
            detail::snappy_compress_framed(out, detail::bytes(data), data.size());
            return detail::to_vector(out.data(), out.size());
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text) noexcept {
            return compress(io::detail::bytes_of(text));
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text) noexcept {
            return compress(slice<const byte>(text));
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept {
            return decompress(data, limits{});
        }

        // Every chunk, every stream, up to the limit's size; every chunk's
        // checksum checked
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            return detail::snappy_decompress_framed(detail::bytes(data), data.size(), l);
        }

        // The block format: what snappy::Compress makes
        static vector<byte> compress_block(const slice<const byte>& data) noexcept {
            std::vector<uint8_t> out(detail::snappy_bound(data.size()) + detail::SnappyOutSlack);
            detail::LentSnappy c;
            const uint8_t* end = c->compress(detail::bytes(data), data.size(), out.data());
            return detail::to_vector(out.data(), size_t(end - out.data()));
        }

        SGCL_INLINE_HOT static vector<byte> compress_block(const string& text) noexcept {
            return compress_block(io::detail::bytes_of(text));
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress_block(const T& text) noexcept {
            return compress_block(slice<const byte>(text));
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress_block(const slice<const byte>& data) noexcept {
            return decompress_block(data, limits{});
        }

        // A block: its length, from its head, checked against the limit
        // before anything is made; then exactly that many bytes
        static expected<vector<byte>, error> decompress_block(const slice<const byte>& data, const limits& l) noexcept {
            return detail::snappy_decompress_block(detail::bytes(data), data.size(), l);
        }

        // The length a block decompresses to, read from its head
        // (snappy::GetUncompressedLength)
        static expected<uint64_t, error> decompressed_size(const slice<const byte>& block) noexcept {
            uint64_t length;
            if (!detail::snappy_varint(detail::bytes(block), block.size(), length)) {
                return unexpected<error>(error(block.size() < 5 ? errc::unexpected_end : errc::corrupt, 0,
                                               string("snappy: a block's length that cannot be read")));
            }
            return length;
        }
    };

    // What is written, compressed into out in the framing format, a chunk
    // of 64 KB at a time; flush() writes the chunk so far, so that a reader
    // can decode everything written. The first failure of out is kept:
    // every write and close after it gives it at once and writes nothing,
    // so a stream may be written freely and checked once, at the close,
    // which leaves out open.
    class snappy::writer final
    : public detail::CodecWriter<detail::SnappyFramedEncoder, detail::NoOptions> {
    public:
        writer(writer&&) noexcept = default;   // the other left closed (detail/codec_stream.h)

        SGCL_INLINE_HOT writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit writer(const io::writer& out) noexcept
        : CodecWriter(out, detail::NoOptions{}) {
        }
    };

    // What the framed data read from in decompresses to. It reads its input
    // 64 KB at a time, so it may read past the end of the Snappy data. A
    // chunk comes out once the whole of it has been read; a failure is the
    // error of the read that reaches it and of every read after,
    // last_error() the whole of it.
    class snappy::reader final
    : public detail::CodecReader<detail::SnappyFramedDecoder, detail::NoOptions> {
    public:
        reader(reader&&) noexcept = default;   // the other left closed (detail/codec_stream.h)

        SGCL_INLINE_HOT reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit reader(const io::reader& in) noexcept
        : CodecReader(in, detail::NoOptions{}, limits{}) {
        }
    };
}
