//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../secret.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The text under JOSE (RFC 7515-7519): base64url without padding in both
// directions, in constant time where the bytes are a secret, and a scanner
// that finds the members of a JSON object where they lie in the text, so
// that the private members of a JWK are decoded from the caller's bytes
// straight into a secret_bytes and never pass through managed memory (the
// public members go on to encoding::json, which validates them).
namespace sgcl::crypto::detail {
    // --- base64url ---------------------------------------------------------

    // The value of one character of the alphabet of RFC 4648 §5 and whether
    // it is one, without a table or a branch on the character: each range
    // is a mask from the sign of a difference
    SGCL_INLINE_HOT uint32_t b64url_value(uint32_t c, uint32_t& bad) noexcept {
        // all ones when lo <= c <= hi (c below 256): neither difference wraps
        auto in = [c](uint32_t lo, uint32_t hi) noexcept {
            return (((c - lo) | (hi - c)) >> 31) - 1u;
        };
        const uint32_t upper = in('A', 'Z');
        const uint32_t lower = in('a', 'z');
        const uint32_t digit = in('0', '9');
        const uint32_t minus = in('-', '-');
        const uint32_t under = in('_', '_');
        const uint32_t v = (upper & (c - 'A')) | (lower & (c - 'a' + 26)) | (digit & (c - '0' + 52)) | (minus & 62u) | (under & 63u);
        bad |= ~(upper | lower | digit | minus | under) & 1u;
        return v;
    }

    // The character of a value 0..63, without a table: 'A' + v moved on at
    // 26, 52, 62 and 63 by masks (the sums taken modulo 256)
    SGCL_INLINE_HOT char b64url_char(uint32_t v) noexcept {
        auto from = [v](uint32_t n) noexcept {
            return ((v - n) >> 31) - 1u;   // all ones when v >= n
        };
        uint32_t c = v + 'A';
        c += from(26) & uint32_t('a' - 'A' - 26);
        c += from(52) & uint32_t('0' - 'a' - 26 + 256);
        c += from(62) & uint32_t('-' - '0' - 10 + 256);
        c += from(63) & uint32_t('_' - '-' - 1);
        return static_cast<char>(c & 0xFF);
    }

    SGCL_INLINE_HOT constexpr size_t b64url_size(size_t n) noexcept {
        return n / 3 * 4 + (n % 3 == 0 ? 0 : n % 3 + 1);
    }

    // The length a text of n characters decodes to; SIZE_MAX for a length
    // no base64url without padding has (4k + 1)
    SGCL_INLINE_HOT constexpr size_t b64url_decoded_size(size_t n) noexcept {
        return n % 4 == 1 ? SIZE_MAX : n / 4 * 3 + (n % 4 == 0 ? 0 : n % 4 - 1);
    }

    // n bytes as base64url into out (b64url_size(n) characters), every
    // character made without a table: the bytes may be a secret
    inline void b64url_encode(char* out, const unsigned char* in, size_t n) noexcept {
        size_t i = 0;
        for (; i + 3 <= n; i += 3) {
            const uint32_t w = uint32_t(in[i]) << 16 | uint32_t(in[i + 1]) << 8 | in[i + 2];
            *out++ = b64url_char(w >> 18);
            *out++ = b64url_char(w >> 12 & 63);
            *out++ = b64url_char(w >> 6 & 63);
            *out++ = b64url_char(w & 63);
        }
        if (n - i == 1) {
            const uint32_t w = uint32_t(in[i]) << 16;
            *out++ = b64url_char(w >> 18);
            *out++ = b64url_char(w >> 12 & 63);
        } else if (n - i == 2) {
            const uint32_t w = uint32_t(in[i]) << 16 | uint32_t(in[i + 1]) << 8;
            *out++ = b64url_char(w >> 18);
            *out++ = b64url_char(w >> 12 & 63);
            *out++ = b64url_char(w >> 6 & 63);
        }
    }

    // base64url without padding decoded into out (b64url_decoded_size(n)
    // bytes): false for a character outside the alphabet, a length of 4k +
    // 1, or bits left over that are not zero (the strict, canonical form).
    // Every character goes through the same steps, the verdict is read once
    // at the end: the text may be a secret
    inline bool b64url_decode(unsigned char* out, const char* in, size_t n) noexcept {
        if (n % 4 == 1) {
            return false;
        }
        uint32_t bad = 0;
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            const uint32_t w = b64url_value(uint8_t(in[i]), bad) << 18 | b64url_value(uint8_t(in[i + 1]), bad) << 12
                             | b64url_value(uint8_t(in[i + 2]), bad) << 6 | b64url_value(uint8_t(in[i + 3]), bad);
            *out++ = static_cast<unsigned char>(w >> 16);
            *out++ = static_cast<unsigned char>(w >> 8);
            *out++ = static_cast<unsigned char>(w);
        }
        if (n - i == 2) {
            const uint32_t w = b64url_value(uint8_t(in[i]), bad) << 18 | b64url_value(uint8_t(in[i + 1]), bad) << 12;
            bad |= (w >> 8 & 0xFF) != 0;
            *out++ = static_cast<unsigned char>(w >> 16);
        } else if (n - i == 3) {
            const uint32_t w = b64url_value(uint8_t(in[i]), bad) << 18 | b64url_value(uint8_t(in[i + 1]), bad) << 12
                             | b64url_value(uint8_t(in[i + 2]), bad) << 6;
            bad |= (w & 0xFF) != 0;
            *out++ = static_cast<unsigned char>(w >> 16);
            *out++ = static_cast<unsigned char>(w >> 8);
        }
        return bad == 0;
    }

    // Public bytes as a base64url string
    inline string b64url(const slice<const byte>& data) {
        std::string s(b64url_size(data.size()), '\0');
        b64url_encode(s.data(), reinterpret_cast<const unsigned char*>(data.data()), data.size());
        return string(std::string_view(s));
    }

    // Public text decoded; nullopt for anything but strict base64url
    inline optional<vector<byte>> b64url_bytes(std::string_view text) noexcept {
        const size_t n = b64url_decoded_size(text.size());
        if (n == SIZE_MAX) {
            return nullopt;
        }
        vector<byte> out(n);
        if (!b64url_decode(reinterpret_cast<unsigned char*>(out.data()), text.data(), text.size())) {
            return nullopt;
        }
        return out;
    }

    // Secret text decoded into a secret_bytes; nullopt as above
    inline optional<secret_bytes> b64url_secret(std::string_view text) noexcept {
        const size_t n = b64url_decoded_size(text.size());
        if (n == SIZE_MAX) {
            return nullopt;
        }
        secret_bytes out(n);
        if (!b64url_decode(reinterpret_cast<unsigned char*>(out.as_slice().data()), text.data(), text.size())) {
            return nullopt;
        }
        return out;
    }

    // --- a writer of text that holds a secret -----------------------------

    // Characters appended into a secret_bytes, which zeroes every block it
    // lets go of when it grows
    class SecretText {
    public:
        SGCL_INLINE_HOT void put(std::string_view s) noexcept {
            _room(s.size());
            sgcl::detail::copy_bytes(_buf.as_slice().data() + _n, s.data(), s.size());
            _n += s.size();
        }

        SGCL_INLINE_HOT void put_b64(const slice<const byte>& data) noexcept {
            const size_t k = b64url_size(data.size());
            _room(k);
            b64url_encode(reinterpret_cast<char*>(_buf.as_slice().data()) + _n, reinterpret_cast<const unsigned char*>(data.data()), data.size());
            _n += k;
        }

        SGCL_INLINE_HOT secret_bytes take() noexcept {
            _buf.resize(_n);
            return std::move(_buf);
        }

    private:
        secret_bytes _buf;
        size_t _n = 0;

        SGCL_INLINE_HOT void _room(size_t k) noexcept {
            if (_n + k > _buf.size()) {
                size_t c = _buf.size() < 256 ? 256 : _buf.size();
                while (c < _n + k) {
                    c *= 2;
                }
                _buf.resize(c);
            }
        }
    };

    // --- the members of a JSON object, where they lie ---------------------

    // One member: its key decoded (keys are not secrets), its value's text
    // as it stands, and the first character of the value ('"' a string,
    // '{' an object, '[' an array, anything else a literal or a number)
    struct JsonPlace {
        std::string key;
        std::string_view key_text;   // with its quotes, as written
        std::string_view value;      // as written, a string with its quotes
        char kind = 0;
    };

    class JsonPlaces {
    public:
        static constexpr int max_depth = 64;

        // The members of the one object that is the whole text (white space
        // around it); false for anything else, a key given twice included
        // (the private members of a key must have one meaning)
        static bool read(std::string_view text, std::vector<JsonPlace>& out) noexcept {
            JsonPlaces s(text);
            s._ws();
            if (!s._object_members(out)) {
                return false;
            }
            s._ws();
            if (s._p != s._end) {
                return false;
            }
            for (size_t i = 0; i < out.size(); ++i) {
                for (size_t j = i + 1; j < out.size(); ++j) {
                    if (out[i].key == out[j].key) {
                        return false;
                    }
                }
            }
            return true;
        }

        // The values of an array's text, as written; false for anything else
        static bool elements(std::string_view text, std::vector<std::string_view>& out) noexcept {
            JsonPlaces s(text);
            s._ws();
            if (s._p == s._end || *s._p != '[') {
                return false;
            }
            ++s._p;
            s._ws();
            if (s._p != s._end && *s._p == ']') {
                ++s._p;
                s._ws();
                return s._p == s._end;
            }
            for (;;) {
                s._ws();
                const char* from = s._p;
                if (!s._value(1)) {
                    return false;
                }
                out.push_back(std::string_view(from, size_t(s._p - from)));
                s._ws();
                if (s._p == s._end) {
                    return false;
                }
                if (*s._p == ',') {
                    ++s._p;
                    continue;
                }
                if (*s._p != ']') {
                    return false;
                }
                ++s._p;
                s._ws();
                return s._p == s._end;
            }
        }

        // The characters of a string value without its quotes, when it has
        // no escape (what every base64url member is); nullopt otherwise
        static optional<std::string_view> plain_string(std::string_view value) noexcept {
            if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
                return nullopt;
            }
            std::string_view inner = value.substr(1, value.size() - 2);
            if (inner.find('\\') != std::string_view::npos) {
                return nullopt;
            }
            return inner;
        }

    private:
        const char* _p;
        const char* _end;

        explicit JsonPlaces(std::string_view t) noexcept
        : _p(t.data()), _end(t.data() + t.size()) {
        }

        SGCL_INLINE_HOT void _ws() noexcept {
            while (_p != _end && (*_p == ' ' || *_p == '\t' || *_p == '\n' || *_p == '\r')) {
                ++_p;
            }
        }

        bool _object_members(std::vector<JsonPlace>& out) noexcept {
            if (_p == _end || *_p != '{') {
                return false;
            }
            ++_p;
            _ws();
            if (_p != _end && *_p == '}') {
                ++_p;
                return true;
            }
            for (;;) {
                _ws();
                JsonPlace m;
                const char* kfrom = _p;
                if (!_string(&m.key)) {
                    return false;
                }
                m.key_text = std::string_view(kfrom, size_t(_p - kfrom));
                _ws();
                if (_p == _end || *_p != ':') {
                    return false;
                }
                ++_p;
                _ws();
                const char* vfrom = _p;
                if (!_value(1)) {
                    return false;
                }
                m.value = std::string_view(vfrom, size_t(_p - vfrom));
                m.kind = *vfrom;
                out.push_back(std::move(m));
                _ws();
                if (_p == _end) {
                    return false;
                }
                if (*_p == ',') {
                    ++_p;
                    continue;
                }
                if (*_p != '}') {
                    return false;
                }
                ++_p;
                return true;
            }
        }

        // A string; its characters decoded into key when one is asked (a
        // key: \uXXXX of the Basic Multilingual Plane and of a surrogate
        // pair as UTF-8). A value's string is passed over: encoding::json
        // reads the public ones, the private ones are base64url without
        // escapes. The comparisons are with '"', '\\' and the control
        // characters, the same verdict for every character of base64url
        bool _string(std::string* key) noexcept {
            if (_p == _end || *_p != '"') {
                return false;
            }
            ++_p;
            while (_p != _end) {
                const unsigned char c = static_cast<unsigned char>(*_p);
                if (c == '"') {
                    ++_p;
                    return true;
                }
                if (c < 0x20) {
                    return false;
                }
                if (c != '\\') {
                    if (key) {
                        key->push_back(char(c));
                    }
                    ++_p;
                    continue;
                }
                ++_p;
                if (_p == _end) {
                    return false;
                }
                const char e = *_p++;
                char simple = 0;
                switch (e) {
                    case '"': simple = '"'; break;
                    case '\\': simple = '\\'; break;
                    case '/': simple = '/'; break;
                    case 'b': simple = '\b'; break;
                    case 'f': simple = '\f'; break;
                    case 'n': simple = '\n'; break;
                    case 'r': simple = '\r'; break;
                    case 't': simple = '\t'; break;
                    case 'u': break;
                    default: return false;
                }
                if (e != 'u') {
                    if (key) {
                        key->push_back(simple);
                    }
                    continue;
                }
                uint32_t cp = 0;
                if (!_hex4(cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp < 0xDC00) {
                    uint32_t lo = 0;
                    if (_end - _p < 6 || _p[0] != '\\' || _p[1] != 'u') {
                        return false;
                    }
                    _p += 2;
                    if (!_hex4(lo) || lo < 0xDC00 || lo >= 0xE000) {
                        return false;
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp < 0xE000) {
                    return false;
                }
                if (key) {
                    _utf8(*key, cp);
                }
            }
            return false;
        }

        bool _hex4(uint32_t& v) noexcept {
            if (_end - _p < 4) {
                return false;
            }
            for (int i = 0; i < 4; ++i) {
                const char h = *_p++;
                uint32_t d;
                if (h >= '0' && h <= '9') {
                    d = uint32_t(h - '0');
                } else if (h >= 'a' && h <= 'f') {
                    d = uint32_t(h - 'a' + 10);
                } else if (h >= 'A' && h <= 'F') {
                    d = uint32_t(h - 'A' + 10);
                } else {
                    return false;
                }
                v = v << 4 | d;
            }
            return true;
        }

        static void _utf8(std::string& s, uint32_t cp) noexcept {
            if (cp < 0x80) {
                s.push_back(char(cp));
            } else if (cp < 0x800) {
                s.push_back(char(0xC0 | cp >> 6));
                s.push_back(char(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                s.push_back(char(0xE0 | cp >> 12));
                s.push_back(char(0x80 | (cp >> 6 & 0x3F)));
                s.push_back(char(0x80 | (cp & 0x3F)));
            } else {
                s.push_back(char(0xF0 | cp >> 18));
                s.push_back(char(0x80 | (cp >> 12 & 0x3F)));
                s.push_back(char(0x80 | (cp >> 6 & 0x3F)));
                s.push_back(char(0x80 | (cp & 0x3F)));
            }
        }

        // Any value, passed over; a literal or a number is the run of the
        // characters they are made of (encoding::json judges it later)
        bool _value(int depth) noexcept {
            if (_p == _end || depth > max_depth) {
                return false;
            }
            const char c = *_p;
            if (c == '"') {
                return _string(nullptr);
            }
            if (c == '{' || c == '[') {
                const char close = c == '{' ? '}' : ']';
                ++_p;
                _ws();
                if (_p != _end && *_p == close) {
                    ++_p;
                    return true;
                }
                for (;;) {
                    _ws();
                    if (c == '{') {
                        if (!_string(nullptr)) {
                            return false;
                        }
                        _ws();
                        if (_p == _end || *_p != ':') {
                            return false;
                        }
                        ++_p;
                        _ws();
                    }
                    if (!_value(depth + 1)) {
                        return false;
                    }
                    _ws();
                    if (_p == _end) {
                        return false;
                    }
                    if (*_p == ',') {
                        ++_p;
                        continue;
                    }
                    if (*_p != close) {
                        return false;
                    }
                    ++_p;
                    return true;
                }
            }
            const char* from = _p;
            while (_p != _end) {
                const char d = *_p;
                const bool part = (d >= '0' && d <= '9') || (d >= 'a' && d <= 'z') || d == '-' || d == '+' || d == '.' || d == 'E';
                if (!part) {
                    break;
                }
                ++_p;
            }
            return _p != from;
        }
    };
}
