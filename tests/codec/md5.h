//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// MD5 (RFC 1321), for the tests only: libwebp-test-data lists the MD5 of what
// dwebp writes for each file, and the codec tests check the module's output
// against it. Not a hash the library offers (crypto has none of MD5's kind).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace codec_test {
    // The digest of the bytes as 32 lowercase hex digits
    inline std::string md5(const void* data, size_t size) {
        static const uint32_t s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                       5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        // T[i] = floor(2^32 × |sin(i + 1)|), RFC 1321 §3.4
        uint32_t t[64];
        for (int i = 0; i < 64; ++i) {
            t[i] = uint32_t(std::floor(std::fabs(std::sin(double(i + 1))) * 4294967296.0));
        }
        uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
        // the message, a 1 bit, zeros to 56 mod 64, the length in bits
        std::string m(static_cast<const char*>(data), size);
        m += char(0x80);
        while (m.size() % 64 != 56) {
            m += char(0);
        }
        const uint64_t bits = uint64_t(size) * 8;
        for (int i = 0; i < 8; ++i) {
            m += char(bits >> (8 * i));
        }
        auto rotl = [](uint32_t x, uint32_t c) {
            return (x << c) | (x >> (32 - c));
        };
        for (size_t off = 0; off < m.size(); off += 64) {
            uint32_t w[16];
            for (int i = 0; i < 16; ++i) {
                const auto* p = reinterpret_cast<const unsigned char*>(m.data() + off + 4 * i);
                w[i] = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
            }
            uint32_t a = a0, b = b0, c = c0, d = d0;
            for (int i = 0; i < 64; ++i) {
                uint32_t f;
                int g;
                if (i < 16) {
                    f = (b & c) | (~b & d);
                    g = i;
                } else if (i < 32) {
                    f = (d & b) | (~d & c);
                    g = (5 * i + 1) % 16;
                } else if (i < 48) {
                    f = b ^ c ^ d;
                    g = (3 * i + 5) % 16;
                } else {
                    f = c ^ (b | ~d);
                    g = (7 * i) % 16;
                }
                const uint32_t next = b + rotl(a + f + t[i] + w[g], s[i]);
                a = d;
                d = c;
                c = b;
                b = next;
            }
            a0 += a;
            b0 += b;
            c0 += c;
            d0 += d;
        }
        std::string hex;
        const char* digits = "0123456789abcdef";
        for (uint32_t v : {a0, b0, c0, d0}) {
            for (int i = 0; i < 4; ++i) {
                const unsigned byte = (v >> (8 * i)) & 0xff;
                hex += digits[byte >> 4];
                hex += digits[byte & 15];
            }
        }
        return hex;
    }
}
