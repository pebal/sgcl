//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../secure_zero.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// Arithmetic modulo an odd number of N 64-bit words in Montgomery form:
// the fields of the NIST curves (modulo p) and their scalars (modulo n).
// An element is N words, least significant first, always fully reduced
// (below the modulus). Multiplication is CIOS (Koç, Acar and Kaliski,
// "Analyzing and comparing Montgomery multiplication algorithms", 1996)
// over 64x64 -> 128-bit products.
//
// Constant time: no operation branches on or indexes by an element's
// value. The one conditional step of every operation — subtracting the
// modulus once more, adding it back after a borrow — is a select through a
// mask made from the carry, the mask passed through an empty asm so that
// the compiler cannot turn it back into a branch. Exponentiation (inverse,
// square root) branches on the bits of the exponent, which is a public
// constant of the modulus (m - 2, (m + 1) / 4), never on the base.
// Loops over the words of an element unrolled whole: the counts are
// constants, and a loop left rolled costs a third more on P-384
#if defined(__clang__)
#define SGCL_CRYPTO_UNROLL _Pragma("clang loop unroll(full)")
#elif defined(__GNUC__)
#define SGCL_CRYPTO_UNROLL _Pragma("GCC unroll 16")
#else
#define SGCL_CRYPTO_UNROLL
#endif

namespace sgcl::crypto::detail {
    using u128 = unsigned __int128;

    // A word the optimizer may not reason about (see constant_time.h's
    // barrier): what every mask made from a secret goes through
    constexpr uint64_t ct_barrier(uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        if (!std::is_constant_evaluated()) {
            __asm__("" : "+r"(v));
        }
#endif
        return v;
    }

    // All ones when x is 0, zero otherwise
    constexpr uint64_t ct_zero_mask(uint64_t x) noexcept {
        return ct_barrier(((x | (0 - x)) >> 63) - 1);
    }

    // All ones when a == b, zero otherwise
    constexpr uint64_t ct_eq_mask(uint64_t a, uint64_t b) noexcept {
        return ct_zero_mask(a ^ b);
    }

    // All ones when bit is 1 (bit is 0 or 1)
    constexpr uint64_t ct_bit_mask(uint64_t bit) noexcept {
        return ct_barrier(0 - bit);
    }

    template<size_t N>
    using limbs = std::array<uint64_t, N>;

    // The words of a hexadecimal constant, most significant digit first as
    // the standards print them: limbs_from_hex<4>("ffffffff00000001...")
    template<size_t N, size_t L>
    consteval limbs<N> limbs_from_hex(const char (&hex)[L]) {
        static_assert(L - 1 == 16 * N, "a constant of N words is 16N hexadecimal digits");
        limbs<N> r{};
        for (size_t i = 0; i < L - 1; ++i) {
            char c = hex[i];
            uint64_t d = c >= '0' && c <= '9' ? uint64_t(c - '0') : c >= 'a' && c <= 'f' ? uint64_t(c - 'a' + 10) : ~uint64_t(0);
            if (d > 15) {
                throw "not a hexadecimal digit";
            }
            size_t bit = 4 * (L - 2 - i);
            r[bit / 64] |= d << (bit % 64);
        }
        return r;
    }

    // a - b over N words; the borrow out
    template<size_t N>
    constexpr uint64_t limbs_sub(limbs<N>& r, const limbs<N>& a, const limbs<N>& b) noexcept {
        uint64_t borrow = 0;
        for (size_t i = 0; i < N; ++i) {
            u128 t = u128(a[i]) - b[i] - borrow;
            r[i] = uint64_t(t);
            borrow = uint64_t(t >> 64) & 1;
        }
        return borrow;
    }

    // a + b over N words; the carry out
    template<size_t N>
    constexpr uint64_t limbs_add(limbs<N>& r, const limbs<N>& a, const limbs<N>& b) noexcept {
        uint64_t carry = 0;
        for (size_t i = 0; i < N; ++i) {
            u128 t = u128(a[i]) + b[i] + carry;
            r[i] = uint64_t(t);
            carry = uint64_t(t >> 64);
        }
        return carry;
    }

    // All ones when a < b, compared in constant time
    template<size_t N>
    constexpr uint64_t limbs_less_mask(const limbs<N>& a, const limbs<N>& b) noexcept {
        limbs<N> t{};
        return ct_bit_mask(limbs_sub(t, a, b));
    }

    // All ones when every word of a is 0
    template<size_t N>
    constexpr uint64_t limbs_zero_mask(const limbs<N>& a) noexcept {
        uint64_t x = 0;
        for (size_t i = 0; i < N; ++i) {
            x |= a[i];
        }
        return ct_zero_mask(x);
    }

    // r = mask ? a : b, word by word
    template<size_t N>
    constexpr void limbs_select(limbs<N>& r, uint64_t mask, const limbs<N>& a, const limbs<N>& b) noexcept {
        for (size_t i = 0; i < N; ++i) {
            r[i] = (a[i] & mask) | (b[i] & ~mask);
        }
    }

    // Big-endian bytes (8N of them) to words, and back: how SEC 1 and the
    // standards write field elements and scalars
    template<size_t N>
    constexpr limbs<N> limbs_from_be(const unsigned char* p) noexcept {
        limbs<N> r{};
        for (size_t i = 0; i < N; ++i) {
            uint64_t w = 0;
            for (size_t j = 0; j < 8; ++j) {
                w = w << 8 | p[8 * (N - 1 - i) + j];
            }
            r[i] = w;
        }
        return r;
    }

    template<size_t N>
    constexpr void limbs_to_be(unsigned char* p, const limbs<N>& a) noexcept {
        for (size_t i = 0; i < N; ++i) {
            uint64_t w = a[N - 1 - i];
            for (size_t j = 0; j < 8; ++j) {
                p[8 * i + j] = static_cast<unsigned char>(w >> (56 - 8 * j));
            }
        }
    }

    // What Montgomery arithmetic modulo m needs, derived from m at compile
    // time: -m^-1 mod 2^64, R mod m (the form of 1) and R^2 mod m (what
    // takes a number into the form), R = 2^(64N); and the two exponents of
    // the prime's inverse and square root
    template<size_t N>
    struct MontConstants {
        limbs<N> m;
        uint64_t m0inv;
        limbs<N> one;       // R mod m
        limbs<N> r2;        // R^2 mod m
        limbs<N> m_minus_2; // Fermat's inverse: a^(m-2)
        limbs<N> sqrt_exp;  // (m + 1) / 4: the square root when m = 3 mod 4
    };

    template<size_t N>
    consteval MontConstants<N> mont_constants(const limbs<N>& m) {
        MontConstants<N> c{};
        c.m = m;
        // m0^-1 mod 2^64 by Newton's iteration: each step doubles the bits
        uint64_t inv = 1;
        for (int i = 0; i < 7; ++i) {
            inv *= 2 - m[0] * inv;
        }
        c.m0inv = 0 - inv;
        // 2^k mod m by doubling 1, k = 64N and then 128N
        limbs<N> r{};
        r[0] = 1;
        for (size_t k = 1; k <= 128 * N; ++k) {
            limbs<N> d{};
            uint64_t carry = limbs_add(d, r, r);
            limbs<N> s{};
            uint64_t borrow = limbs_sub(s, d, m);
            r = (carry || !borrow) ? s : d;
            if (k == 64 * N) {
                c.one = r;
            }
        }
        c.r2 = r;
        limbs<N> two{};
        two[0] = 2;
        limbs_sub(c.m_minus_2, m, two);
        limbs<N> one{};
        one[0] = 1;
        limbs<N> m1{};
        uint64_t top = limbs_add(m1, m, one);
        for (size_t i = 0; i < N; ++i) {
            uint64_t next = i + 1 < N ? m1[i + 1] : top;
            c.sqrt_exp[i] = m1[i] >> 2 | next << 62;
        }
        return c;
    }

    // The arithmetic modulo Params::modulus, Params a type with
    // static constexpr size_t words and static constexpr limbs<words> modulus
    template<class Params>
    struct Mont {
        static constexpr size_t N = Params::words;
        static constexpr MontConstants<N> k = mont_constants<N>(Params::modulus);

        using element = limbs<N>;

        // r = t - m when t (N words and a top word hi, below 2m) is at
        // least m, r = t otherwise
        static constexpr void reduce_once(element& r, const element& t, uint64_t hi) noexcept {
            element u{};
            uint64_t borrow = limbs_sub(u, t, k.m);
            // the whole difference is negative when hi is 0 and a borrow
            // came out of the low words: keep t then
            uint64_t keep = ct_bit_mask(~hi & borrow & 1);
            limbs_select(r, keep, t, u);
        }

        static constexpr void add(element& r, const element& a, const element& b) noexcept {
            element t{};
            uint64_t carry = limbs_add(t, a, b);
            reduce_once(r, t, carry);
        }

        static constexpr void sub(element& r, const element& a, const element& b) noexcept {
            element t{};
            uint64_t borrow = limbs_sub(t, a, b);
            uint64_t mask = ct_bit_mask(borrow);
            element m{};
            for (size_t i = 0; i < N; ++i) {
                m[i] = k.m[i] & mask;
            }
            limbs_add(r, t, m);
        }

        // r = a * b / R mod m
        static constexpr void mul(element& r, const element& a, const element& b) noexcept {
            uint64_t t[N + 2] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    u128 x = u128(a[j]) * b[i] + t[j] + c;
                    t[j] = uint64_t(x);
                    c = uint64_t(x >> 64);
                }
                u128 x = u128(t[N]) + c;
                t[N] = uint64_t(x);
                t[N + 1] = uint64_t(x >> 64);
                uint64_t q = t[0] * k.m0inv;
                x = u128(q) * k.m[0] + t[0];
                c = uint64_t(x >> 64);
                SGCL_CRYPTO_UNROLL
                for (size_t j = 1; j < N; ++j) {
                    x = u128(q) * k.m[j] + t[j] + c;
                    t[j - 1] = uint64_t(x);
                    c = uint64_t(x >> 64);
                }
                x = u128(t[N]) + c;
                t[N - 1] = uint64_t(x);
                t[N] = t[N + 1] + uint64_t(x >> 64);
            }
            element low{};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                low[i] = t[i];
            }
            reduce_once(r, low, t[N]);
        }

        // r = a^2 / R mod m: the product first (each cross product once,
        // then doubled, then the squares added: N(N+1)/2 products against
        // mul's N^2), then N steps of Montgomery's reduction, a word each
        static constexpr void sqr(element& r, const element& a) noexcept {
            uint64_t t[2 * N] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < N; ++j) {
                    u128 x = u128(a[j]) * a[i] + t[i + j] + c;
                    t[i + j] = uint64_t(x);
                    c = uint64_t(x >> 64);
                }
                t[i + N] = c;
            }
            uint64_t hi = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 2 * N; ++i) {
                uint64_t v = t[i];
                t[i] = v << 1 | hi;
                hi = v >> 63;
            }
            uint64_t c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                u128 s = u128(a[i]) * a[i];
                u128 x = u128(t[2 * i]) + uint64_t(s) + c;
                t[2 * i] = uint64_t(x);
                x = u128(t[2 * i + 1]) + uint64_t(s >> 64) + uint64_t(x >> 64);
                t[2 * i + 1] = uint64_t(x);
                c = uint64_t(x >> 64);
            }
            // the square is below m^2 < 2^(128N): no word above t[2N - 1]
            uint64_t top = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                uint64_t q = t[i] * k.m0inv;
                uint64_t cc = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    u128 x = u128(q) * k.m[j] + t[i + j] + cc;
                    t[i + j] = uint64_t(x);
                    cc = uint64_t(x >> 64);
                }
                u128 x = u128(t[i + N]) + cc + top;
                t[i + N] = uint64_t(x);
                top = uint64_t(x >> 64);
            }
            element high{};
            for (size_t i = 0; i < N; ++i) {
                high[i] = t[N + i];
            }
            reduce_once(r, high, top);
        }

        // Into the form (a R mod m) and out of it; a below m
        static constexpr element to_mont(const element& a) noexcept {
            element r{};
            mul(r, a, k.r2);
            return r;
        }

        static constexpr element from_mont(const element& a) noexcept {
            element one{};
            one[0] = 1;
            element r{};
            mul(r, a, one);
            return r;
        }

        static constexpr element one() noexcept {
            return k.one;
        }

        // a^e for an exponent e that is public (a constant of the
        // modulus): a window of four bits, sixteen powers of a
        static element pow_public(const element& a, const element& e) noexcept {
            element table[16]{};
            table[0] = k.one;
            table[1] = a;
            for (size_t i = 2; i < 16; ++i) {
                mul(table[i], table[i - 1], a);
            }
            element r = k.one;
            bool started = false;
            for (size_t w = 16 * N; w-- > 0;) {
                unsigned nibble = unsigned(e[w / 16] >> (4 * (w % 16))) & 15;
                if (started) {
                    sqr(r, r);
                    sqr(r, r);
                    sqr(r, r);
                    sqr(r, r);
                }
                if (nibble != 0) {
                    mul(r, r, table[nibble]);
                    started = true;
                }
            }
            secure_zero(table, sizeof table);
            return r;
        }

        // a^-1 by Fermat's little theorem (m prime): a^(m-2), with no
        // branch on a; 0 gives 0
        static element inverse(const element& a) noexcept {
            return pow_public(a, k.m_minus_2);
        }
    };
}
