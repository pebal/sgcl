//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/codec.h"
#include "detail/radix.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../io/stream.h"

#include <cstddef>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    // Base64 (RFC 4648 sections 4 and 5): three bytes as four characters of
    // an alphabet of 64. A codec is a value — the alphabet, the padding or
    // none, strict or lenient — with encode and decode as its methods; the
    // four alphabets Go names are constants, and a program with an
    // alphabet of its own makes one:
    //
    //     base64::standard.encode("ala:sekret")        // "YWxhOnNla3JldA=="
    //     base64::raw_url.decode(part)                 // a JWT's segment
    //     base64 mine("...64 characters...", nullopt); // no padding
    //
    // The decoding is strict by default: a character outside the alphabet
    // is an error (section 3.3), a line ending included, and so are bits
    // past the data in the last character, which would let two texts
    // decode to the same bytes (section 3.5) — what a signature or a key
    // cannot have. lenient() lets both through, for MIME and the older
    // encoders that wrap lines. A padded codec wants its padding; a codec
    // without_padding() refuses it.
    class base64 {
    public:
        using error = encoding::error;

        class encoder;
        class decoder;

        // RFC 4648 section 4 with '=', section 5 (- and _ for + and /) with
        // '=', and both without padding (a JWT's is raw_url)
        static const base64 standard;
        static const base64 url;
        static const base64 raw_standard;
        static const base64 raw_url;

        // An alphabet of 64 characters, all different, no '\r' or '\n' and
        // not the padding; the padding, or nullopt for none. An alphabet
        // that breaks this is invalid_argument (in a constant, an
        // error at compile time).
        constexpr base64(const char (&alphabet)[65], optional<char> padding = '=')
        : _radix("base64", alphabet, padding ? int(uint8_t(*padding)) : -1) {
        }

        constexpr base64 without_padding() const noexcept {
            return base64(_radix.without_padding());
        }

        // A decoding that skips '\r' and '\n' anywhere and takes any bits
        // past the data in the last character
        constexpr base64 lenient() const noexcept {
            return base64(_radix.lenient());
        }

        constexpr bool padded() const noexcept {
            return _radix.padded();
        }

        constexpr bool is_lenient() const noexcept {
            return _radix.is_lenient();
        }

        string encode(const slice<const byte>& data) const {
            return detail::encode_text(_radix, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        // The bytes of a text: "user:password" for Basic authentication
        string encode(const string& text) const {
            return detail::encode_text(_radix, reinterpret_cast<const uint8_t*>(text.data()), text.size());
        }

        expected<vector<byte>, error> decode(const string& text) const {
            return detail::decode_text(_radix, text);
        }

        // The characters n bytes take (SIZE_MAX when no size_t holds them),
        // and the most bytes n characters decode to
        constexpr size_t encoded_size(size_t n) const noexcept {
            return _radix.encoded_size(n);
        }

        constexpr size_t max_decoded_size(size_t n) const noexcept {
            return _radix.max_decoded_size(n);
        }

        // Into the caller's buffer, nothing allocated: out holds at least
        // encoded_size(data.size()) characters, or max_decoded_size of the
        // text's size bytes — a smaller one is length_error. The
        // characters or the bytes written.
        size_t encode_to(const slice<char>& out, const slice<const byte>& data) const {
            return detail::encode_into(_radix, out, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        expected<size_t, error> decode_to(const slice<byte>& out, const string& text) const {
            return detail::decode_to(_radix, out, text);
        }

        // A writer that encodes what is written to it into out (close()
        // writes the last group and leaves out open), and a reader of the
        // bytes the text of in decodes to
        tracked_ptr<encoder> encoder_to(const io::writer& out) const;
        tracked_ptr<decoder> decoder_from(const io::reader& in) const;

    private:
        constexpr explicit base64(const detail::Radix<6>& r) noexcept
        : _radix(r) {
        }

        detail::Radix<6> _radix;
    };

    inline constexpr base64 base64::standard {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
    inline constexpr base64 base64::url {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"};
    inline constexpr base64 base64::raw_standard = base64::standard.without_padding();
    inline constexpr base64 base64::raw_url = base64::url.without_padding();

    class base64::encoder final
    : public detail::CodecWriter<detail::Radix<6>> {
    public:
        using CodecWriter::CodecWriter;
    };

    class base64::decoder final
    : public detail::CodecReader<detail::Radix<6>> {
    public:
        using CodecReader::CodecReader;
    };

    inline tracked_ptr<base64::encoder> base64::encoder_to(const io::writer& out) const {
        return make_tracked<encoder>(_radix, out);
    }

    inline tracked_ptr<base64::decoder> base64::decoder_from(const io::reader& in) const {
        return make_tracked<decoder>(_radix, in);
    }
}
