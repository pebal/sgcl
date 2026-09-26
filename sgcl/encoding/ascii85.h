//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/codec.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // Ascii85 as the btoa tool writes it and Go's encoding/ascii85
        // reads it: four bytes as a number of 32 bits written in five
        // digits of base 85, the digits the characters '!' (0) to 'u' (84);
        // a group of four zero bytes is the one character 'z'; the last
        // group of n bytes (1 to 3) is padded with zeros, encoded, and cut
        // to its first n + 1 characters, which the decoder pads back with
        // 'u'. No "<~" and "~>" around it (that is Adobe's variant, a
        // matter of the file that carries it). The codec interface of
        // codec.h: the same text and stream helpers as base64.
        struct Ascii85 {
            static constexpr size_t GroupBytes = 4;
            static constexpr size_t MaxGroupChars = 5;
            static constexpr size_t MaxGroupBytes = 4;

            const char* name() const noexcept {
                return "ascii85";
            }

            // Every group five characters, the last n + 1: the bound, which
            // a 'z' only makes smaller. SIZE_MAX when no size_t holds it.
            constexpr size_t encode_bound(size_t n) const noexcept {
                size_t groups = n / 4;
                size_t tail = n % 4 ? n % 4 + 1 : 0;
                if (groups > (SIZE_MAX - tail) / 5) {
                    return SIZE_MAX;
                }
                return groups * 5 + tail;
            }

            // A 'z' is four bytes from one character, every other
            // character at most four fifths of a byte, a short group
            // included (k characters: k - 1 bytes)
            size_t decode_bound(const char* p, size_t n) const noexcept {
                size_t z = size_t(std::count(p, p + n, 'z'));
                return (n - z) / 5 * 4 + (n - z) % 5 + z * 4;
            }

            void encode_groups(const uint8_t*& in, const uint8_t* in_end, char*& out, char* out_end) const noexcept {
                auto p = in;
                auto o = out;
                while (in_end - p >= 4 && out_end - o >= 5) {
                    uint32_t v = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
                    p += 4;
                    if (v == 0) {
                        *o++ = 'z';
                        continue;
                    }
                    for (int i = 4; i >= 0; --i) {
                        o[i] = char('!' + v % 85);
                        v /= 85;
                    }
                    o += 5;
                }
                in = p;
                out = o;
            }

            void encode_final(const uint8_t* in, size_t n, char*& out) const noexcept {
                if (n == 0) {
                    return;
                }
                uint32_t v = 0;
                for (size_t i = 0; i < 4; ++i) {
                    v = v << 8 | (i < n ? in[i] : 0);
                }
                char digits[5];
                for (int i = 4; i >= 0; --i) {
                    digits[i] = char('!' + v % 85);
                    v /= 85;
                }
                for (size_t i = 0; i <= n; ++i) {
                    *out++ = digits[i];
                }
            }

            struct decoding {
                uint64_t acc = 0;
                uint64_t offset = 0;
                uint8_t count = 0;
            };

            // Any byte up to the space is skipped (the format wraps lines
            // and Go reads it so); 'z' stands only between groups; a group
            // worth more than 32 bits is out_of_range at its fifth
            // character — no encoder writes one, and Go, which takes it
            // modulo 2^32, reads it as some other bytes.
            FeedStatus feed(decoding& d, const char*& in, const char* in_end, uint8_t*& out, uint8_t* out_end, optional<error>& e) const {
                auto p = in;
                auto o = out;
                auto status = FeedStatus::more;
                for (; p != in_end; ++p) {
                    auto c = uint8_t(*p);
                    uint64_t at = d.offset + uint64_t(p - in);
                    if (c <= ' ') {
                        continue;
                    }
                    if (c == 'z') {
                        if (d.count != 0) {
                            e = error(errc::syntax, at, string("'z' inside a group"));
                            status = FeedStatus::failed;
                            break;
                        }
                        if (out_end - o < 4) {
                            status = FeedStatus::room;
                            break;
                        }
                        o[0] = o[1] = o[2] = o[3] = 0;
                        o += 4;
                        continue;
                    }
                    if (c < '!' || c > 'u') {
                        e = error(errc::invalid_character, at, string("invalid character " + quoted_byte(c)));
                        status = FeedStatus::failed;
                        break;
                    }
                    if (d.count == 4 && out_end - o < 4) {
                        status = FeedStatus::room;
                        break;
                    }
                    d.acc = d.acc * 85 + (c - '!');
                    if (++d.count == 5) {
                        if (d.acc > 0xFFFFFFFFu) {
                            e = error(errc::out_of_range, at, string("a group past 32 bits"));
                            status = FeedStatus::failed;
                            break;
                        }
                        _emit(d.acc, 4, o);
                        d.acc = 0;
                        d.count = 0;
                    }
                }
                d.offset += uint64_t(p - in);
                in = p;
                out = o;
                return status;
            }

            FeedStatus finish(decoding& d, uint8_t*& out, uint8_t* out_end, optional<error>& e) const {
                if (d.count == 0) {
                    return FeedStatus::more;
                }
                if (d.count == 1) {
                    e = error(errc::unexpected_end, d.offset, string("the input ends inside a byte"));
                    return FeedStatus::failed;
                }
                uint64_t v = d.acc;
                for (size_t i = d.count; i < 5; ++i) {
                    v = v * 85 + 84;
                }
                if (v > 0xFFFFFFFFu) {
                    e = error(errc::out_of_range, d.offset, string("the last group past 32 bits"));
                    return FeedStatus::failed;
                }
                if (size_t(out_end - out) < size_t(d.count - 1)) {
                    return FeedStatus::room;
                }
                _emit(v, d.count - 1, out);
                d.count = 0;
                d.acc = 0;
                return FeedStatus::more;
            }

        private:
            static void _emit(uint64_t v, size_t n, uint8_t*& o) noexcept {
                for (size_t i = 0; i < n; ++i) {
                    o[i] = uint8_t(v >> (24 - 8 * i));
                }
                o += n;
            }
        };
    }

    // Ascii85, the btoa variant Go reads and writes: four bytes as five
    // characters from '!' to 'u', four zero bytes as 'z', a quarter more
    // text for the bytes where base64 takes a third. Nothing to choose, so
    // everything is static:
    //
    //     ascii85::encode("Hello")      // "87cURDZ"
    //     ascii85::decode("87cURD]i")   // "Hello " as bytes
    //
    // The decoding skips white space and every control character, as Go's
    // does, and refuses a group that is worth more than 32 bits, which Go
    // takes modulo 2^32.
    class ascii85 {
    public:
        using error = encoding::error;

        class encoder;
        class decoder;

        static string encode(const slice<const byte>& data) {
            return detail::encode_text(detail::Ascii85{}, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        // The bytes of a text
        static string encode(const string& text) {
            return detail::encode_text(detail::Ascii85{}, reinterpret_cast<const uint8_t*>(text.data()), text.size());
        }

        static expected<vector<byte>, error> decode(const string& text) {
            return detail::decode_text(detail::Ascii85{}, text);
        }

        // The most characters n bytes take (a 'z' makes it fewer) and the
        // most bytes n characters decode to (every one a 'z'): bounds, as
        // base64's sizes are for its padding-free forms
        static constexpr size_t max_encoded_size(size_t n) noexcept {
            return detail::Ascii85{}.encode_bound(n);
        }

        static constexpr size_t max_decoded_size(size_t n) noexcept {
            return n > SIZE_MAX / 4 ? SIZE_MAX : n * 4;
        }

        // Into the caller's buffer, nothing allocated: out holds at least
        // max_encoded_size(data.size()) characters, or max_decoded_size of
        // the text's size bytes (the bound of the text itself suffices) —
        // a smaller one is length_error. The characters or the bytes
        // written.
        static size_t encode_to(const slice<char>& out, const slice<const byte>& data) {
            return detail::encode_into(detail::Ascii85{}, out, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        static expected<size_t, error> decode_to(const slice<byte>& out, const string& text) {
            return detail::decode_to(detail::Ascii85{}, out, text);
        }

        // A writer that encodes what is written to it into out (close()
        // writes the last group and leaves out open), and a reader of the
        // bytes the text of in decodes to
        static tracked_ptr<encoder> encoder_to(const io::writer& out);
        static tracked_ptr<decoder> decoder_from(const io::reader& in);
    };

    class ascii85::encoder final
    : public detail::CodecWriter<detail::Ascii85> {
    public:
        using CodecWriter::CodecWriter;
    };

    class ascii85::decoder final
    : public detail::CodecReader<detail::Ascii85> {
    public:
        using CodecReader::CodecReader;
    };

    inline tracked_ptr<ascii85::encoder> ascii85::encoder_to(const io::writer& out) {
        return make_tracked<encoder>(detail::Ascii85{}, out);
    }

    inline tracked_ptr<ascii85::decoder> ascii85::decoder_from(const io::reader& in) {
        return make_tracked<decoder>(detail::Ascii85{}, in);
    }
}
