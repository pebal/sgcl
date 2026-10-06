//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/codec_stream.h"
#include "detail/zstd_decode.h"
#include "detail/zstd_encode.h"
#include "../core/make_tracked.h"
#include "../io/detail/bytes.h"

#include <stdexcept>
#include <string>

namespace sgcl::compress {
    // Zstandard (.zst, RFC 8878): LZ77 with Huffman-coded literals and the
    // sequences coded by Finite State Entropy, the format of zstd and of
    // HTTP's Content-Encoding: zstd. The levels are zstd's own: 1..19 as the
    // command's -1..-19 (3 the default), 20..22 as --ultra, and negative
    // levels as --fast=N. A frame carries the content's size when it is
    // known and the XXH64 of the content; frames one after another are read
    // as one, skippable frames passed over, as zstd -d reads them. A
    // dictionary (zstd's format, from zstd --train, or raw content) is made
    // once and given to any number of calls and streams.
    class zstd {
    public:
        using error = compress::error;

        // How hard the compressor works, on zstd's scale: an int converts to
        // it, so that options take `{.level = 19}`; a value outside
        // -131072..-1 and 1..22 is invalid_argument (in a constant, an error
        // at compile time)
        class level {
        public:
            static constexpr int fastest = -131072;   // zstd --fast=131072
            static constexpr int standard = 3;        // zstd's default
            static constexpr int smallest = 19;       // the most without --ultra
            static constexpr int ultra = 22;          // zstd --ultra -22

            SGCL_INLINE_HOT constexpr level() noexcept
            : _value(standard) {
            }

            SGCL_INLINE_HOT constexpr level(int n)
            : _value(n) {
                if (n < -131072 || n > 22 || n == 0) {
                    throw std::invalid_argument("compress::zstd::level: 1..22, or -1..-131072 for --fast");
                }
            }

            SGCL_INLINE_HOT constexpr int value() const noexcept {
                return _value;
            }

            friend constexpr bool operator==(level a, level b) noexcept = default;

        private:
            int _value;
        };

        // A dictionary: zstd's format (its id, its entropy tables, three
        // offsets and its content) or raw content (any other bytes, id 0).
        // A handle: a copy shares the dictionary, which is never changed
        class dictionary {
            friend struct detail::ZstdDictionaryAccess;

        public:
            dictionary() noexcept = default;

            // parse(bytes).value(): a damaged dictionary of zstd's format
            // throws bad_expected_access<compress::error>
            explicit dictionary(const slice<const byte>& bytes)
            : dictionary(parse(bytes).value()) {
            }

            static expected<dictionary, error> parse(const slice<const byte>& bytes) noexcept {
                dictionary d;
                if (bytes.empty()) {
                    return d;
                }
                tracked_ptr<detail::ZstdDictionaryData> data = make_tracked<detail::ZstdDictionaryData>();
                if (const char* wrong = data->parse(detail::bytes(bytes), bytes.size())) {
                    return unexpected<error>(error(errc::corrupt, 0, string(wrong)));
                }
                d._state = data;
                return d;
            }

            // The id the dictionary names itself by; 0 for raw content or none
            SGCL_INLINE_HOT uint32_t id() const noexcept {
                return _state ? _state->id : 0;
            }

            // The bytes of its content
            SGCL_INLINE_HOT size_t size() const noexcept {
                return _state ? _state->content.size() : 0;
            }

            SGCL_INLINE_HOT bool empty() const noexcept {
                return !_state;
            }

        private:
            tracked_ptr<detail::ZstdDictionaryData> _state;
        };

        struct options {
            zstd::level level;
            bool checksum = true;               // the XXH64 of the content, its low 32 bits at the end
            bool content_size = true;           // compress() writes the size (a writer cannot know it)
            uint8_t window_log = 0;             // a window of 2^window_log bytes, 10..31; 0: the level's
            zstd::dictionary dictionary;
            bool write_dictionary_id = true;    // the dictionary's id in the frame, when it has one
        };

        class writer;
        class reader;

        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data) noexcept {
            return _compress(data, options{});
        }

        // One frame. Options out of range are the program's mistake:
        // std::invalid_argument (the writer's first write reports it as
        // errc::invalid_argument)
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            auto s = detail::ZstdSettings::of(o);
            if (s.error) {
                throw std::invalid_argument(std::string("compress::zstd: ") + s.error);
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
        // checksum checked; a frame's window held against max_memory. Of the
        // options only the dictionary is read
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o, const limits& l) noexcept {
            const uint8_t* p = detail::bytes(data);
            if (o.dictionary.empty()) {
                return detail::zstd_decompress_all(p, data.size(), l);
            }
            auto d = std::make_unique<detail::ZstdFrameDecoder>(o, l);
            return detail::decompress_with(*d, p, data.size(), l, data.size() * 4);
        }

        // The content size a frame's header gives, if it gives one
        static optional<uint64_t> content_size(const slice<const byte>& data) noexcept {
            detail::ZstdFrameHeader h;
            bool more = false;
            if (data.size() < 4 || detail::zstd_le32(detail::bytes(data)) != detail::ZstdMagic || h.read(detail::bytes(data), data.size(), more) || more ||
                !h.has_size) {
                return nullopt;
            }
            return h.content_size;
        }

        // The id of the dictionary a frame's header names; 0 for none
        static uint32_t dictionary_id(const slice<const byte>& data) noexcept {
            detail::ZstdFrameHeader h;
            bool more = false;
            if (data.size() < 4 || detail::zstd_le32(detail::bytes(data)) != detail::ZstdMagic || h.read(detail::bytes(data), data.size(), more) || more) {
                return 0;
            }
            return h.dictionary_id;
        }

    private:
        static vector<byte> _compress(const slice<const byte>& data, const options& o) noexcept {
            const uint8_t* p = detail::bytes(data);
            const size_t n = data.size();
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(n + n / 128 + 64);
            if (o.dictionary.empty()) {
                detail::zstd_compress_frame(out, detail::ZstdSettings::of(o), p, n);
            } else {
                auto enc = std::make_unique<detail::ZstdFrameEncoder>(o, n);
                const uint64_t size = n;
                enc->start(out, &size);
                enc->write(p, n, out);
                enc->finish(out);
            }
            return detail::to_vector(out.data(), out.size());
        }
    };

    // What is written, compressed into out as one zstd frame, a block of up
    // to 128 KB at a time; flush() writes the blocks so far, so that a
    // reader can decode everything written. The first failure — of out, or
    // of options out of range — is kept: every write and close after it
    // gives it at once and writes nothing, so a stream may be written freely
    // and checked once, at the close, which writes the last block and the
    // checksum and leaves out open.
    class zstd::writer final
    : public detail::CodecWriter<detail::ZstdFrameEncoder, zstd::options> {
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
    // zstd data. A block comes out once the whole of it has been read; the
    // reader holds twice the frame's window; a failure is the error of the
    // read that reaches it and of every read after, last_error() the whole
    // of it.
    class zstd::reader final
    : public detail::CodecReader<detail::ZstdFrameDecoder, zstd::options> {
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
