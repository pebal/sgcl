//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "syntax.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../encoding/detail/mail_text.h"
#include "../../../encoding/quoted_printable.h"
#include "../../../txt/encoding.h"

#include <cstdint>
#include <string>
#include <string_view>

// The decodings the IMAP side needs of a message's text, over the ones of
// encoding::email: the encoded words of a header (RFC 2047) in UTF-8, a
// body's Content-Transfer-Encoding undone (base64, quoted-printable) for
// BINARY and for SEARCH BODY, a charset to UTF-8; and the base64 of SASL.
namespace sgcl::net::imap::detail {
    SGCL_INLINE_HOT constexpr int base64_value(unsigned char c) noexcept {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    }

    // Base64 as MIME writes it: whatever is not of the alphabet skipped
    // (line breaks, spaces), the padding ending it; false for bits that
    // cannot be the end of the data
    inline bool decode_base64(std::string_view s, std::string& out) {
        out.reserve(out.size() + s.size() / 4 * 3);
        uint32_t bits = 0;
        int n = 0;
        for (unsigned char c : s) {
            if (c == '=') {
                break;
            }
            const int v = base64_value(c);
            if (v < 0) {
                continue;
            }
            bits = (bits << 6) | uint32_t(v);
            n += 6;
            if (n >= 8) {
                n -= 8;
                out += char((bits >> n) & 0xFF);
            }
        }
        return n < 6;
    }

    inline void encode_base64(std::string_view s, std::string& out) {
        static constexpr char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t i = 0;
        for (; i + 3 <= s.size(); i += 3) {
            const uint32_t v = (uint32_t((unsigned char)s[i]) << 16) | (uint32_t((unsigned char)s[i + 1]) << 8) | uint32_t((unsigned char)s[i + 2]);
            out += A[v >> 18];
            out += A[(v >> 12) & 63];
            out += A[(v >> 6) & 63];
            out += A[v & 63];
        }
        if (i + 1 == s.size()) {
            const uint32_t v = uint32_t((unsigned char)s[i]) << 16;
            out += A[v >> 18];
            out += A[(v >> 12) & 63];
            out += "==";
        } else if (i + 2 == s.size()) {
            const uint32_t v = (uint32_t((unsigned char)s[i]) << 16) | (uint32_t((unsigned char)s[i + 1]) << 8);
            out += A[v >> 18];
            out += A[(v >> 12) & 63];
            out += A[(v >> 6) & 63];
            out += '=';
        }
    }

    // Quoted-printable (RFC 2045 §6.7) as encoding::quoted_printable
    // reads it leniently: an "=" of no meaning kept as it is
    inline void decode_quoted_printable(std::string_view s, std::string& out) {
        encoding::detail::QpDecoder d;
        d.lenient = true;
        d.feed(reinterpret_cast<const uint8_t*>(s.data()), s.size(), out);
        d.finish(out);
    }

    // Bytes in a charset as UTF-8: the conversion of encoding::email; a
    // charset it does not know, or bytes that are not the UTF-8 they claim
    // to be, read as UTF-8 with the invalid bytes replaced
    inline std::string to_utf8(std::string_view bytes, std::string_view charset) {
        std::string out;
        if (encoding::detail::mail_charset_to_utf8(bytes, charset, out) && valid_utf8(out)) {
            return out;
        }
        const slice<const byte> b(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
        string s = txt::decode(b, txt::encoding::utf8);
        return std::string(s.view());
    }

    // The encoded words of a header's value (RFC 2047) in UTF-8, as
    // encoding::email decodes them: the white space between two adjacent
    // encoded words dropped, adjacent words of one charset converted
    // together, a word of a charset not known left as it was written
    inline std::string decode_words(std::string_view s) {
        return encoding::detail::mail_decode_words(s);
    }
}
