//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/brotli_decode.h"
#include "detail/brotli_encode.h"
#include "detail/codec_stream.h"
#include "../io/detail/bytes.h"

#include <stdexcept>
#include <string>

namespace sgcl::compress {
    // Brotli (.br, RFC 7932): LZ77 with prefix codes chosen by context,
    // block types that switch them, and a static dictionary of words with
    // transforms; the format of HTTP's Content-Encoding: br and of WOFF2. The
    // levels are brotli's qualities: 0 to 11 as `brotli -q N` (11, the
    // command's default). A stream is the window's size and meta-blocks to
    // the last; it carries no checksum and no size.
    class brotli {
    public:
        using error = compress::error;

        // How hard the compressor works, on brotli's scale: an int converts
        // to it, so that options take `{.level = 5}`; a value outside 0..11
        // is invalid_argument (in a constant, an error at compile time)
        class level {
        public:
            static constexpr int fastest = 0;      // brotli -q 0
            static constexpr int standard = 11;    // the brotli command's default
            static constexpr int smallest = 11;    // brotli -q 11

            SGCL_INLINE_HOT constexpr level() noexcept
            : _value(standard) {
            }

            SGCL_INLINE_HOT constexpr level(int n)
            : _value(n) {
                if (n < 0 || n > 11) {
                    throw std::invalid_argument("compress::brotli::level: 0..11");
                }
            }

            SGCL_INLINE_HOT constexpr int value() const noexcept {
                return _value;
            }

            friend constexpr bool operator==(level a, level b) noexcept = default;

        private:
            int _value;
        };

        struct options {
            brotli::level level;
            uint8_t window_log = 22;   // a window of 2^window_log - 16 bytes, 10..24 (brotli's lgwin)
        };

        class writer;
        class reader;

        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data) noexcept {
            return _compress(data, options{});
        }

        // One stream. Options out of range are the program's mistake:
        // std::invalid_argument (the writer's first write reports it as
        // errc::invalid_argument)
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            auto s = detail::BrotliSettings::of(o);
            if (s.error) {
                throw std::invalid_argument(std::string("compress::brotli: ") + s.error);
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
            return decompress(data, limits{});
        }

        // The stream to its last meta-block, up to the limit's size; its
        // window held against max_memory; bytes after its end are corrupt
        // data (a stream has no frame for another to follow)
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            auto d = std::make_unique<detail::BrotliDecoder>(l);
            return d->decompress_all(detail::bytes(data), data.size(), l);
        }

    private:
        static vector<byte> _compress(const slice<const byte>& data, const options& o) noexcept {
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(data.size() / 3 + 64);
            detail::brotli_compress_all(out, detail::BrotliSettings::of(o), detail::bytes(data), data.size());
            return detail::to_vector(out.data(), out.size());
        }
    };

    // What is written, compressed into out as one brotli stream, a
    // meta-block at a time; flush() writes the meta-blocks so far and an
    // empty metadata block that ends on a byte, so that a reader can decode
    // everything written. The first failure — of out, or of options out of
    // range — is kept: every write and close after it gives it at once and
    // writes nothing, so a stream may be written freely and checked once, at
    // the close, which writes the last meta-block and leaves out open.
    class brotli::writer final
    : public detail::CodecWriter<detail::BrotliFrameEncoder, brotli::options> {
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

    // What the data read from in decompresses to. It reads its input 64 KB
    // at a time, so it may read past the end of the brotli data, and stops
    // at the stream's end. The reader holds the stream's window and 256 KB;
    // a failure is the error of the read that reaches it and of every read
    // after, last_error() the whole of it.
    class brotli::reader final
    : public detail::CodecReader<detail::BrotliDecoder, detail::NoOptions> {
    public:
        reader(reader&&) noexcept = default;   // the other left closed (detail/codec_stream.h)

        SGCL_INLINE_HOT reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit reader(const io::reader& in) noexcept
        : CodecReader(in, detail::NoOptions {}, limits{}) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, const limits& l) noexcept
        : CodecReader(in, detail::NoOptions {}, l) {
        }
    };
}
