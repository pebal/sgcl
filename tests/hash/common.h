//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the hash module share: the patterns of bytes the oracle
// (tools/hash_oracle.go) hashed, made again the same way, and a result of
// any hasher as text for comparing and for the failure message.
#pragma once

#include "tests/types.h"
#include "sgcl/hash/hash.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace hash_test {
    namespace hash = sgcl::hash;

    // The generator of the random pattern: splitmix64, as the oracle has it
    struct splitmix {
        uint64_t s;

        uint64_t next() {
            s += 0x9e3779b97f4a7c15ull;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            return z ^ (z >> 31);
        }
    };

    // 0 random (splitmix64 from 1, each word's bytes little-endian), 1 zeros,
    // 2 0xFF, 3 rising (i mod 256). Each is a prefix of its longer self
    inline std::vector<unsigned char> pattern(int kind, size_t n) {
        std::vector<unsigned char> b(n + 8);
        if (kind == 0) {
            splitmix r {1};
            for (size_t i = 0; i < n; i += 8) {
                uint64_t w = r.next();
                for (int k = 0; k < 8; ++k) {
                    b[i + k] = (unsigned char)(w >> (8 * k));
                }
            }
        } else if (kind == 2) {
            std::fill(b.begin(), b.end(), 0xff);
        } else if (kind == 3) {
            for (size_t i = 0; i < b.size(); ++i) {
                b[i] = (unsigned char)i;
            }
        }
        b.resize(n);
        return b;
    }

    inline sgcl::slice<const byte> bytes(const unsigned char* p, size_t n) {
        return sgcl::slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    inline sgcl::slice<const byte> bytes(const std::vector<unsigned char>& v) {
        return bytes(v.data(), v.size());
    }

    // A result as text: a number in hex, a digest as its bytes in hex
    inline std::string text(uint32_t v) {
        char s[16];
        std::snprintf(s, sizeof s, "%08x", v);
        return s;
    }

    inline std::string text(uint64_t v) {
        char s[24];
        std::snprintf(s, sizeof s, "%016llx", (unsigned long long)v);
        return s;
    }

    template<size_t N>
    std::string text(const sgcl::array<byte, N>& d) {
        std::string out;
        char s[4];
        for (auto b : d) {
            std::snprintf(s, sizeof s, "%02x", unsigned(b));
            out += s;
        }
        return out;
    }

    // Two words of a 128-bit result, the high first, as a digest reads
    inline std::string text128(uint64_t high, uint64_t low) {
        return text(high) + text(low);
    }

    // The key the tests give a siphash, the one type of the module with no
    // hasher made without an argument: 00 01 … 0f, the key of the paper's
    // vectors
    inline sgcl::array<byte, 16> sip_key() {
        sgcl::array<byte, 16> k;
        for (size_t i = 0; i < 16; ++i) {
            k[i] = byte(i);
        }
        return k;
    }

    // A new hasher of type H, and the one-shot hash of data by H: the key
    // above for a siphash, nothing for the rest
    template<class H>
    H fresh() {
        if constexpr (std::is_same_v<H, hash::siphash>) {
            return H(sip_key());
        } else {
            return H();
        }
    }

    template<class H, class Data>
    auto one(const Data& data) {
        if constexpr (std::is_same_v<H, hash::siphash>) {
            return H::of(data, sip_key());
        } else {
            return H::of(data);
        }
    }

    template<class H>
    std::string of(const unsigned char* p, size_t n) {
        return text(one<H>(bytes(p, n)));
    }

    template<class H>
    std::string of(const std::vector<unsigned char>& v) {
        return of<H>(v.data(), v.size());
    }

    using All = ::testing::Types<hash::crc32, hash::crc32c, hash::crc64, hash::crc64_iso, hash::adler32,
                                 hash::fnv32, hash::fnv32a, hash::fnv64, hash::fnv64a, hash::fnv128, hash::fnv128a,
                                 hash::xxh3_64, hash::xxh3_128, hash::maphash, hash::siphash>;
}
