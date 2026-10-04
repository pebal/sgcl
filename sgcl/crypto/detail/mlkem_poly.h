//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <array>
#include <cstddef>
#include <cstdint>

// The ring of ML-KEM (FIPS 203 §2.3, §4.3): polynomials of 256
// coefficients in Z_q, q = 3329, modulo X^256 + 1, and their number-
// theoretic transform. A coefficient is a uint16_t in [0, q), always
// reduced: every operation takes reduced values and gives one. What is
// secret goes through all of this (the private vector s, the errors, the
// message), so nothing here branches on a value or divides one: a
// product is reduced by Barrett's multiplication, a conditional
// subtraction is a mask, Compress is a multiplication and a shift rather
// than the division by q of its definition (a division takes a time that
// depends on its operand on some processors: KyberSlash, 2024). The
// constants ζ^BitRev7(i) and ζ^(2·BitRev7(i)+1) are computed here from ζ =
// 17, not typed in (FIPS 203 Appendix A lists them; the tests compare).
namespace sgcl::crypto::detail::mlkem {
    inline constexpr uint16_t Q = 3329;
    inline constexpr size_t N = 256;

    using Poly = std::array<uint16_t, N>;

    // a mod q for a < 2^24 (a product of two reduced values is below
    // 3329² < 2^24): Barrett with m = ⌊2^36 / q⌋, the quotient off by at
    // most one below, which the masked subtraction corrects
    SGCL_INLINE_HOT constexpr uint16_t reduce(uint32_t a) noexcept {
        constexpr uint64_t M = (uint64_t(1) << 36) / Q;
        uint32_t t = uint32_t((uint64_t(a) * M) >> 36);
        uint32_t r = a - t * Q;                              // [0, 2q)
        r -= Q & uint32_t(-int32_t((int32_t(Q - 1) - int32_t(r)) >> 31 & 1));   // r >= q: minus q
        return uint16_t(r);
    }

    // a + b, a − b and a·b mod q, of reduced values
    SGCL_INLINE_HOT constexpr uint16_t add(uint16_t a, uint16_t b) noexcept {
        uint32_t r = uint32_t(a) + b;                        // [0, 2q)
        r -= Q & uint32_t(-int32_t((int32_t(Q - 1) - int32_t(r)) >> 31 & 1));
        return uint16_t(r);
    }

    SGCL_INLINE_HOT constexpr uint16_t sub(uint16_t a, uint16_t b) noexcept {
        uint32_t r = uint32_t(a) + Q - b;                    // [1, 2q)
        r -= Q & uint32_t(-int32_t((int32_t(Q - 1) - int32_t(r)) >> 31 & 1));
        return uint16_t(r);
    }

    SGCL_INLINE_HOT constexpr uint16_t mul(uint16_t a, uint16_t b) noexcept {
        return reduce(uint32_t(a) * b);
    }

    // BitRev7 of FIPS 203 §2.3: the seven bits of i reversed
    constexpr unsigned bitrev7(unsigned i) noexcept {
        unsigned r = 0;
        for (unsigned k = 0; k < 7; ++k) {
            r |= ((i >> k) & 1) << (6 - k);
        }
        return r;
    }

    constexpr uint16_t power(uint16_t base, unsigned e) noexcept {
        uint16_t r = 1;
        for (unsigned k = 0; k < e; ++k) {
            r = mul(r, base);
        }
        return r;
    }

    // ζ^BitRev7(i) for the transform (i < 128), ζ^(2·BitRev7(i)+1) for the
    // products of degree-one pieces (i < 128)
    struct Tables {
        uint16_t zetas[128] = {};
        uint16_t gammas[128] = {};

        constexpr Tables() noexcept {
            for (unsigned i = 0; i < 128; ++i) {
                zetas[i] = power(17, bitrev7(i));
                gammas[i] = power(17, 2 * bitrev7(i) + 1);
            }
        }
    };

    inline constexpr Tables tables;

    // 128^−1 mod q: the factor Algorithm 10 ends with
    inline constexpr uint16_t InverseOf128 = 3303;

    // Algorithm 9: the transform in place
    inline void ntt(Poly& f) noexcept {
        unsigned i = 1;
        for (unsigned len = 128; len >= 2; len /= 2) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                uint16_t zeta = tables.zetas[i++];
                for (unsigned j = start; j < start + len; ++j) {
                    uint16_t t = mul(zeta, f[j + len]);
                    f[j + len] = sub(f[j], t);
                    f[j] = add(f[j], t);
                }
            }
        }
    }

    // Algorithm 10: its inverse in place
    inline void ntt_inverse(Poly& f) noexcept {
        unsigned i = 127;
        for (unsigned len = 2; len <= 128; len *= 2) {
            for (unsigned start = 0; start < N; start += 2 * len) {
                uint16_t zeta = tables.zetas[i--];
                for (unsigned j = start; j < start + len; ++j) {
                    uint16_t t = f[j];
                    f[j] = add(t, f[j + len]);
                    f[j + len] = mul(zeta, sub(f[j + len], t));
                }
            }
        }
        for (auto& c : f) {
            c = mul(c, InverseOf128);
        }
    }

    // Algorithms 11 and 12: the product of two transforms, a product of
    // 128 polynomials of degree one modulo X² − γ each
    inline void multiply_ntts(Poly& h, const Poly& f, const Poly& g) noexcept {
        for (unsigned i = 0; i < 128; ++i) {
            uint16_t a0 = f[2 * i], a1 = f[2 * i + 1];
            uint16_t b0 = g[2 * i], b1 = g[2 * i + 1];
            uint16_t gamma = tables.gammas[i];
            h[2 * i] = add(mul(a0, b0), mul(mul(a1, b1), gamma));
            h[2 * i + 1] = add(mul(a0, b1), mul(a1, b0));
        }
    }

    // h += the product of two transforms: the inner products of the vectors
    inline void multiply_ntts_add(Poly& h, const Poly& f, const Poly& g) noexcept {
        for (unsigned i = 0; i < 128; ++i) {
            uint16_t a0 = f[2 * i], a1 = f[2 * i + 1];
            uint16_t b0 = g[2 * i], b1 = g[2 * i + 1];
            uint16_t gamma = tables.gammas[i];
            h[2 * i] = add(h[2 * i], add(mul(a0, b0), mul(mul(a1, b1), gamma)));
            h[2 * i + 1] = add(h[2 * i + 1], add(mul(a0, b1), mul(a1, b0)));
        }
    }

    inline void poly_add(Poly& h, const Poly& f) noexcept {
        for (size_t i = 0; i < N; ++i) {
            h[i] = add(h[i], f[i]);
        }
    }

    inline void poly_sub(Poly& h, const Poly& f) noexcept {
        for (size_t i = 0; i < N; ++i) {
            h[i] = sub(h[i], f[i]);
        }
    }

    // Compress_d (§4.2.1, 4.7): ⌈(2^d / q)·x⌋ mod 2^d, for 1 ≤ d ≤ 11. The
    // quotient of x·2^d by q by Barrett's multiplication, set right by a
    // masked correction, and rounded half up by the remainder's size: no
    // division, no branch
    constexpr uint16_t compress(uint16_t x, unsigned d) noexcept {
        constexpr uint64_t M = (uint64_t(1) << 36) / Q;
        uint32_t dividend = uint32_t(x) << d;                // < 2^23
        uint32_t quotient = uint32_t((uint64_t(dividend) * M) >> 36);
        uint32_t r = dividend - quotient * Q;                // [0, 2q)
        uint32_t over = uint32_t((int32_t(Q - 1) - int32_t(r)) >> 31) & 1;   // r >= q
        quotient += over;
        r -= Q & uint32_t(-int32_t(over));
        quotient += uint32_t((int32_t(Q / 2) - int32_t(r)) >> 31) & 1;         // r > q/2 (q odd: r ≥ 1665): up
        return uint16_t(quotient & ((1u << d) - 1));
    }

    // Decompress_d (4.8): ⌈(q / 2^d)·y⌋, for y < 2^d
    SGCL_INLINE_HOT constexpr uint16_t decompress(uint16_t y, unsigned d) noexcept {
        return uint16_t((uint32_t(y) * Q + (uint32_t(1) << (d - 1))) >> d);
    }

    // ByteEncode_d (Algorithm 5): 256 values of d bits, little-endian bit
    // after bit, into 32·d bytes. For d < 12 the values are below 2^d, for
    // d = 12 below q
    inline void byte_encode(uint8_t* out, const Poly& f, unsigned d) noexcept {
        uint64_t acc = 0;
        unsigned bits = 0;
        size_t o = 0;
        for (size_t i = 0; i < N; ++i) {
            acc |= uint64_t(f[i]) << bits;
            bits += d;
            while (bits >= 8) {
                out[o++] = uint8_t(acc);
                acc >>= 8;
                bits -= 8;
            }
        }
    }

    // ByteDecode_d (Algorithm 6): 32·d bytes into 256 values of d bits;
    // for d = 12 each value taken mod q (the encapsulation key's check,
    // §7.2, compares its encoding again with the bytes)
    inline void byte_decode(Poly& f, const uint8_t* in, unsigned d) noexcept {
        uint64_t acc = 0;
        unsigned bits = 0;
        size_t o = 0;
        const uint32_t mask = (uint32_t(1) << d) - 1;
        for (size_t i = 0; i < N; ++i) {
            while (bits < d) {
                acc |= uint64_t(in[o++]) << bits;
                bits += 8;
            }
            uint32_t v = uint32_t(acc) & mask;
            acc >>= d;
            bits -= d;
            f[i] = d == 12 ? reduce(v) : uint16_t(v);
        }
    }

    inline void poly_compress(Poly& f, unsigned d) noexcept {
        for (auto& c : f) {
            c = compress(c, d);
        }
    }

    inline void poly_decompress(Poly& f, unsigned d) noexcept {
        for (auto& c : f) {
            c = decompress(c, d);
        }
    }
}
