//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../encoding/base64.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What the authentications of HTTP read and compute (RFC 9110 §11): the
// challenges of WWW-Authenticate and the credentials of Authorization (an
// auth-scheme and its token68 or auth-params), Basic (RFC 7617) and Digest
// (RFC 7616: MD5, SHA-256 and SHA-512/256, their -sess forms, qop auth and
// auth-int, userhash, username* of RFC 8187). MD5 is here for Digest's
// legacy servers alone (RFC 1321): it is no hash to trust for anything
// else, and crypto has none.
namespace sgcl::net::http::detail {
    // MD5 (RFC 1321): the 64 steps of the four rounds over 512-bit blocks
    class Md5 {
    public:
        void update(const void* data, size_t n) noexcept {
            const auto* p = static_cast<const unsigned char*>(data);
            _length += uint64_t(n);
            if (_used) {
                const size_t k = std::min(n, size_t(64) - _used);
                sgcl::detail::copy_bytes(_block + _used, p, k);
                _used += k;
                p += k;
                n -= k;
                if (_used < 64) {
                    return;
                }
                _compress(_block);
                _used = 0;
            }
            while (n >= 64) {
                _compress(p);
                p += 64;
                n -= 64;
            }
            if (n) {
                sgcl::detail::copy_bytes(_block, p, n);
                _used = n;
            }
        }

        std::array<unsigned char, 16> digest() noexcept {
            const uint64_t bits = _length * 8;
            static const unsigned char pad[64] = {0x80};
            update(pad, 1 + (_used < 56 ? 55 - _used : 119 - _used));
            unsigned char len[8];
            for (int i = 0; i < 8; ++i) {
                len[i] = static_cast<unsigned char>(bits >> (8 * i));
            }
            update(len, 8);
            std::array<unsigned char, 16> out;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    out[4 * i + j] = static_cast<unsigned char>(_s[i] >> (8 * j));
                }
            }
            return out;
        }

    private:
        uint32_t _s[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
        unsigned char _block[64] = {};
        size_t _used = 0;
        uint64_t _length = 0;

        static constexpr uint32_t rotl(uint32_t x, int c) noexcept {
            return (x << c) | (x >> (32 - c));
        }

        void _compress(const unsigned char* b) noexcept {
            // T[i] = floor(2^32 * |sin(i + 1)|) and the shifts of RFC 1321 §3.4
            static constexpr uint32_t T[64] = {
                0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
            static constexpr int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                          5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                          4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                          6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
            uint32_t m[16];
            for (int i = 0; i < 16; ++i) {
                m[i] = uint32_t(b[4 * i]) | uint32_t(b[4 * i + 1]) << 8 | uint32_t(b[4 * i + 2]) << 16 | uint32_t(b[4 * i + 3]) << 24;
            }
            uint32_t a = _s[0], bb = _s[1], c = _s[2], d = _s[3];
            for (int i = 0; i < 64; ++i) {
                uint32_t f;
                int g;
                if (i < 16) {
                    f = (bb & c) | (~bb & d);
                    g = i;
                } else if (i < 32) {
                    f = (d & bb) | (~d & c);
                    g = (5 * i + 1) & 15;
                } else if (i < 48) {
                    f = bb ^ c ^ d;
                    g = (3 * i + 5) & 15;
                } else {
                    f = c ^ (bb | ~d);
                    g = (7 * i) & 15;
                }
                const uint32_t next = d;
                d = c;
                c = bb;
                bb = bb + rotl(a + f + T[i] + m[g], S[i]);
                a = next;
            }
            _s[0] += a;
            _s[1] += bb;
            _s[2] += c;
            _s[3] += d;
        }
    };

    // The algorithms of Digest (RFC 7616 §3.2), the -sess forms with a flag
    enum class DigestHash : uint8_t { md5, sha256, sha512_256 };

    struct DigestAlgorithm {
        DigestHash hash = DigestHash::md5;
        bool sess = false;
    };

    // "MD5", "SHA-256-sess"... without case (the RFC's tokens are compared
    // so by the implementations); nullopt for another
    inline optional<DigestAlgorithm> digest_algorithm(std::string_view a) noexcept {
        DigestAlgorithm out;
        if (a.size() > 5 && iequal(a.substr(a.size() - 5), "-sess")) {
            out.sess = true;
            a.remove_suffix(5);
        }
        if (iequal(a, "MD5")) {
            out.hash = DigestHash::md5;
        } else if (iequal(a, "SHA-256")) {
            out.hash = DigestHash::sha256;
        } else if (iequal(a, "SHA-512-256")) {
            out.hash = DigestHash::sha512_256;
        } else {
            return nullopt;
        }
        return out;
    }

    inline std::string_view digest_algorithm_name(DigestAlgorithm a) noexcept {
        switch (a.hash) {
            case DigestHash::md5:
                return a.sess ? "MD5-sess" : "MD5";
            case DigestHash::sha256:
                return a.sess ? "SHA-256-sess" : "SHA-256";
            case DigestHash::sha512_256:
                return a.sess ? "SHA-512-256-sess" : "SHA-512-256";
        }
        return "MD5";
    }

    inline void append_hex_bytes(std::string& out, const unsigned char* p, size_t n) {
        static constexpr char digits[] = "0123456789abcdef";
        for (size_t i = 0; i < n; ++i) {
            out += digits[p[i] >> 4];
            out += digits[p[i] & 15];
        }
    }

    // H(data) of the algorithm in lower-case hex (RFC 7616 §3.4.1, the
    // KD and H of the RFC)
    inline std::string digest_hex(DigestHash h, std::string_view data) {
        std::string out;
        const slice<const byte> in(reinterpret_cast<const byte*>(data.data()), data.size());
        switch (h) {
            case DigestHash::md5: {
                Md5 m;
                m.update(data.data(), data.size());
                auto d = m.digest();
                append_hex_bytes(out, d.data(), d.size());
                break;
            }
            case DigestHash::sha256: {
                auto d = crypto::sha256::of(in);
                append_hex_bytes(out, reinterpret_cast<const unsigned char*>(d.data()), d.size());
                break;
            }
            case DigestHash::sha512_256: {
                auto d = crypto::sha512_256::of(in);
                append_hex_bytes(out, reinterpret_cast<const unsigned char*>(d.data()), d.size());
                break;
            }
        }
        return out;
    }

    // What Digest's response is computed of (RFC 7616 §3.4.1-§3.4.3)
    struct DigestInput {
        DigestAlgorithm algorithm;
        std::string_view user;        // the name as it is (userhash applies to the field, not here)
        std::string_view realm;
        std::string_view password;
        std::string_view method;
        std::string_view uri;         // the request-target as sent
        std::string_view nonce;
        std::string_view cnonce;
        std::string_view nc;          // 8 hex digits
        std::string_view qop;         // "auth", "auth-int", or "" (RFC 2069's form)
        std::string_view body_hash;   // H(entity-body), for auth-int
    };

    // HA1: H(user:realm:password), or for -sess H(that:nonce:cnonce)
    inline std::string digest_ha1(const DigestInput& in) {
        std::string a1;
        a1.reserve(in.user.size() + in.realm.size() + in.password.size() + 2);
        a1 += in.user;
        a1 += ':';
        a1 += in.realm;
        a1 += ':';
        a1 += in.password;
        std::string ha1 = digest_hex(in.algorithm.hash, a1);
        if (in.algorithm.sess) {
            ha1 = digest_hex(in.algorithm.hash, ha1 + ":" + std::string(in.nonce) + ":" + std::string(in.cnonce));
        }
        return ha1;
    }

    // The response: KD(HA1, nonce:nc:cnonce:qop:HA2), HA2 = H(method:uri)
    // or, for auth-int, H(method:uri:H(body)); without qop, H(HA1:nonce:HA2)
    inline std::string digest_response(const DigestInput& in) {
        const std::string ha1 = digest_ha1(in);
        std::string a2 = std::string(in.method) + ":" + std::string(in.uri);
        if (in.qop == "auth-int") {
            a2 += ':';
            a2 += in.body_hash;
        }
        const std::string ha2 = digest_hex(in.algorithm.hash, a2);
        std::string kd = ha1 + ":" + std::string(in.nonce) + ":";
        if (!in.qop.empty()) {
            kd += in.nc;
            kd += ':';
            kd += in.cnonce;
            kd += ':';
            kd += in.qop;
            kd += ':';
        }
        kd += ha2;
        return digest_hex(in.algorithm.hash, kd);
    }

    // One challenge of WWW-Authenticate, or the credentials of
    // Authorization: the scheme, and its token68 or its parameters (names in
    // lower case, values unquoted)
    struct AuthChallenge {
        std::string scheme;
        std::string token68;
        std::vector<std::pair<std::string, std::string>> params;

        const std::string* param(std::string_view name) const noexcept {
            for (auto& p : params) {
                if (iequal(p.first, name)) {
                    return &p.second;
                }
            }
            return nullptr;
        }
    };

    SGCL_INLINE_HOT constexpr bool token68_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
               c == '~' || c == '+' || c == '/';
    }

    // A quoted-string at s (s[0] == '"'), unescaped (RFC 9110 §5.6.4): its
    // value and the length read; nullopt when it does not end
    inline optional<std::string> read_quoted(std::string_view s, size_t& used) {
        std::string out;
        for (size_t i = 1; i < s.size(); ++i) {
            const char c = s[i];
            if (c == '"') {
                used = i + 1;
                return out;
            }
            // qdtext and quoted-pair: HTAB, SP, VCHAR, obs-text; never another
            // control or DEL, escaped or not
            auto allowed = [](unsigned char u) { return u == '\t' || (u >= 0x20 && u != 0x7F); };
            if (c == '\\') {
                if (i + 1 == s.size() || !allowed((unsigned char)s[i + 1])) {
                    return nullopt;
                }
                out += s[++i];
                continue;
            }
            if (!allowed((unsigned char)c)) {
                return nullopt;
            }
            out += c;
        }
        return nullopt;
    }

    // The challenges of a field value, in their order (RFC 9110 §11.6.1:
    // challenge = auth-scheme [ 1*SP ( token68 / #auth-param ) ], several in
    // one field separated by commas). A challenge read wrong ends the list:
    // what was read before it is given
    inline std::vector<AuthChallenge> parse_challenges(std::string_view v) {
        std::vector<AuthChallenge> out;
        size_t at = 0;
        auto skip_ws = [&] {
            while (at < v.size() && (v[at] == ' ' || v[at] == '\t')) {
                ++at;
            }
        };
        auto skip_list = [&] {   // OWS and empty list members
            while (at < v.size() && (v[at] == ' ' || v[at] == '\t' || v[at] == ',')) {
                ++at;
            }
        };
        auto token_at = [&](size_t from) {
            size_t e = from;
            while (e < v.size() && token_char(uint8_t(v[e]))) {
                ++e;
            }
            return e;
        };
        skip_list();
        while (at < v.size()) {
            const size_t e = token_at(at);
            if (e == at) {
                return out;
            }
            AuthChallenge c;
            c.scheme.assign(v.substr(at, e - at));
            at = e;
            // params or token68 after at least one space
            size_t before = at;
            skip_ws();
            if (at == before || at >= v.size() || v[at] == ',') {
                out.push_back(std::move(c));
                skip_list();
                continue;
            }
            // token68 (its characters, then '='s, then OWS and a comma or the
            // end) when it reads as one; "name=value" never does, since a
            // value follows its '='
            size_t t = at;
            while (t < v.size() && token68_char(v[t])) {
                ++t;
            }
            size_t after = t;
            while (after < v.size() && v[after] == '=') {
                ++after;
            }
            const size_t end68 = after;
            while (after < v.size() && (v[after] == ' ' || v[after] == '\t')) {
                ++after;
            }
            if (t > at && (after == v.size() || v[after] == ',')) {
                c.token68.assign(v.substr(at, end68 - at));
                at = after;
                out.push_back(std::move(c));
                skip_list();
                continue;
            }
            // auth-params: name BWS "=" BWS ( token / quoted-string ), by commas
            for (;;) {
                skip_ws();
                const size_t ne = token_at(at);
                if (ne == at) {
                    break;
                }
                size_t p = ne;
                while (p < v.size() && (v[p] == ' ' || v[p] == '\t')) {
                    ++p;
                }
                if (p >= v.size() || v[p] != '=') {
                    break;   // a new challenge's scheme, not a param
                }
                std::string name(v.substr(at, ne - at));
                at = p + 1;
                skip_ws();
                std::string value;
                if (at < v.size() && v[at] == '"') {
                    size_t used = 0;
                    auto q = read_quoted(v.substr(at), used);
                    if (!q) {
                        out.push_back(std::move(c));
                        return out;
                    }
                    value = std::move(*q);
                    at += used;
                } else {
                    const size_t ve = token_at(at);
                    value.assign(v.substr(at, ve - at));
                    at = ve;
                }
                for (auto& ch : name) {
                    ch = ascii_lower(ch);
                }
                c.params.emplace_back(std::move(name), std::move(value));
                skip_ws();
                if (at < v.size() && v[at] == ',') {
                    ++at;
                    skip_ws();
                    // the next item: a param (name=) or a new challenge (scheme)
                    const size_t ne2 = token_at(at);
                    size_t p2 = ne2;
                    while (p2 < v.size() && (v[p2] == ' ' || v[p2] == '\t')) {
                        ++p2;
                    }
                    if (ne2 > at && p2 < v.size() && v[p2] == '=') {
                        continue;
                    }
                    break;
                }
                break;
            }
            out.push_back(std::move(c));
            skip_list();
        }
        return out;
    }

    // A value as a quoted-string: '"' and '\' escaped
    inline void append_quoted(std::string& out, std::string_view v) {
        out += '"';
        for (char c : v) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        out += '"';
    }

    // RFC 8187's ext-value of UTF-8 text: UTF-8''%XX... of everything but
    // attr-char
    inline std::string encode_ext_value(std::string_view v) {
        std::string out = "UTF-8''";
        static constexpr char digits[] = "0123456789ABCDEF";
        for (unsigned char c : v) {
            const bool attr = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '!' || c == '#' ||
                              c == '$' || c == '&' || c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' ||
                              c == '~';
            if (attr) {
                out += char(c);
            } else {
                out += '%';
                out += digits[c >> 4];
                out += digits[c & 15];
            }
        }
        return out;
    }

    // An ext-value read back, of UTF-8 alone (RFC 7616 §3.4.4 allows no
    // other); nullopt for anything else
    inline optional<std::string> decode_ext_value(std::string_view v) {
        const size_t q1 = v.find('\'');
        if (q1 == std::string_view::npos) {
            return nullopt;
        }
        const size_t q2 = v.find('\'', q1 + 1);
        if (q2 == std::string_view::npos || !iequal(v.substr(0, q1), "UTF-8")) {
            return nullopt;
        }
        std::string out;
        for (size_t i = q2 + 1; i < v.size(); ++i) {
            if (v[i] == '%') {
                if (i + 2 >= v.size()) {
                    return nullopt;
                }
                auto hex = [](char c) -> int {
                    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                };
                const int a = hex(v[i + 1]), b = hex(v[i + 2]);
                if (a < 0 || b < 0) {
                    return nullopt;
                }
                out += char(a * 16 + b);
                i += 2;
            } else {
                out += v[i];
            }
        }
        return out;
    }

    // Basic's credentials (RFC 7617): base64 of user:password
    inline string basic_credentials(std::string_view user, std::string_view password) {
        std::string pair;
        pair.reserve(user.size() + password.size() + 1);
        pair += user;
        pair += ':';
        pair += password;
        return string::concat("Basic ", encoding::base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(pair.data()), pair.size())));
    }

    // Basic's credentials of an Authorization value read back: the user
    // and the password (the first colon parts them; RFC 7617 §2); nullopt
    // for another scheme or a token that is not base64 of one
    inline optional<std::pair<std::string, std::string>> read_basic(std::string_view v) {
        v = trim_ows(v);
        if (v.size() < 6 || !iequal(v.substr(0, 5), "basic") || (v[5] != ' ' && v[5] != '\t')) {
            return nullopt;
        }
        std::string_view token = trim_ows(v.substr(6));
        auto raw = encoding::base64::standard.decode(string(token));
        if (!raw) {
            return nullopt;
        }
        std::string_view text(reinterpret_cast<const char*>(raw->data()), raw->size());
        const size_t colon = text.find(':');
        if (colon == std::string_view::npos) {
            return nullopt;
        }
        return std::pair<std::string, std::string>(std::string(text.substr(0, colon)), std::string(text.substr(colon + 1)));
    }
}
