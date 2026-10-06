//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// MD5 (RFC 1321), for APOP's digest (RFC 1939 §7) alone: broken as a hash
// of anything that matters, and kept out of the public API for that
namespace sgcl::net::pop3::detail {
    struct Md5 {
        uint32_t s[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
        unsigned char block[64] = {};
        size_t used = 0;
        uint64_t length = 0;

        static uint32_t rotl(uint32_t x, int c) noexcept {
            return (x << c) | (x >> (32 - c));
        }

        void compress(const unsigned char* p) noexcept {
            static constexpr uint32_t K[64] = {
                0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
            static constexpr int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                          5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                          4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                          6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
            uint32_t m[16];
            for (int i = 0; i < 16; ++i) {
                m[i] = uint32_t(p[4 * i]) | uint32_t(p[4 * i + 1]) << 8 | uint32_t(p[4 * i + 2]) << 16 | uint32_t(p[4 * i + 3]) << 24;
            }
            uint32_t a = s[0], b = s[1], c = s[2], d = s[3];
            for (int i = 0; i < 64; ++i) {
                uint32_t f;
                int g;
                if (i < 16) {
                    f = (b & c) | (~b & d);
                    g = i;
                } else if (i < 32) {
                    f = (d & b) | (~d & c);
                    g = (5 * i + 1) & 15;
                } else if (i < 48) {
                    f = b ^ c ^ d;
                    g = (3 * i + 5) & 15;
                } else {
                    f = c ^ (b | ~d);
                    g = (7 * i) & 15;
                }
                uint32_t t = d;
                d = c;
                c = b;
                b = b + rotl(a + f + K[i] + m[g], R[i]);
                a = t;
            }
            s[0] += a;
            s[1] += b;
            s[2] += c;
            s[3] += d;
        }

        void update(std::string_view data) noexcept {
            length += data.size();
            for (unsigned char ch : data) {
                block[used++] = ch;
                if (used == 64) {
                    compress(block);
                    used = 0;
                }
            }
        }

        // The digest in lower-case hex, as APOP sends it
        std::string hex() noexcept {
            uint64_t bits = length * 8;
            unsigned char pad = 0x80;
            update(std::string_view(reinterpret_cast<const char*>(&pad), 1));
            unsigned char zero = 0;
            while (used != 56) {
                update(std::string_view(reinterpret_cast<const char*>(&zero), 1));
            }
            unsigned char len[8];
            for (int i = 0; i < 8; ++i) {
                len[i] = static_cast<unsigned char>(bits >> (8 * i));
            }
            update(std::string_view(reinterpret_cast<const char*>(len), 8));
            static constexpr char digits[] = "0123456789abcdef";
            std::string out;
            for (uint32_t w : s) {
                for (int i = 0; i < 4; ++i) {
                    unsigned char b = static_cast<unsigned char>(w >> (8 * i));
                    out += digits[b >> 4];
                    out += digits[b & 15];
                }
            }
            return out;
        }
    };

    inline std::string md5_hex(std::string_view data) {
        Md5 m;
        m.update(data);
        return m.hex();
    }
}
