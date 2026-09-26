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
    // Base32 (RFC 4648 sections 6 and 7): five bytes as eight characters
    // of an alphabet of 32 — letters of one case and digits, so a text a
    // person reads aloud or types, a name that must not care about case
    // on a file system, the secret of a one-time password. The same shape as
    // base64: a codec is a value, the two alphabets of the RFC are
    // constants, the decoding is strict by default and lenient() on
    // request, and without_padding() leaves the '=' out:
    //
    //     base32::standard.encode("foobar")         // "MZXW6YTBOI======"
    //     base32::hex.without_padding().decode(t)   // the extended hex alphabet
    //
    // The letters are upper case, as the RFC writes them; a lower case
    // letter is outside the alphabet.
    class base32 {
    public:
        using error = encoding::error;

        class encoder;
        class decoder;

        // RFC 4648 section 6, A-Z and 2-7; section 7, "base32hex", 0-9 and
        // A-V, which keeps the order of the bytes in the order of the text
        static const base32 standard;
        static const base32 hex;

        // An alphabet of 32 characters, all different, no '\r' or '\n' and
        // not the padding; the padding, or nullopt for none
        constexpr base32(const char (&alphabet)[33], optional<char> padding = '=')
        : _radix("base32", alphabet, padding ? int(uint8_t(*padding)) : -1) {
        }

        constexpr base32 without_padding() const noexcept {
            return base32(_radix.without_padding());
        }

        // A decoding that skips '\r' and '\n' anywhere and takes any bits
        // past the data in the last character
        constexpr base32 lenient() const noexcept {
            return base32(_radix.lenient());
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

        // The bytes of a text
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
        constexpr explicit base32(const detail::Radix<5>& r) noexcept
        : _radix(r) {
        }

        detail::Radix<5> _radix;
    };

    inline constexpr base32 base32::standard {"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"};
    inline constexpr base32 base32::hex {"0123456789ABCDEFGHIJKLMNOPQRSTUV"};

    class base32::encoder final
    : public detail::CodecWriter<detail::Radix<5>> {
    public:
        using CodecWriter::CodecWriter;
    };

    class base32::decoder final
    : public detail::CodecReader<detail::Radix<5>> {
    public:
        using CodecReader::CodecReader;
    };

    inline tracked_ptr<base32::encoder> base32::encoder_to(const io::writer& out) const {
        return make_tracked<encoder>(_radix, out);
    }

    inline tracked_ptr<base32::decoder> base32::decoder_from(const io::reader& in) const {
        return make_tracked<decoder>(_radix, in);
    }
}
