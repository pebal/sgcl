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
// the fields of the NIST curves (modulo p) and their scalars (modulo n);
// P-521's field, modulo the Mersenne prime 2^521 - 1, keeps its elements
// as they are and reduces a product by folding its high bits onto its low
// ones (p521_reduce), behind the same interface.
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
    SGCL_INLINE_HOT constexpr uint64_t ct_barrier(uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        if (!std::is_constant_evaluated()) {
            __asm__("" : "+r"(v));
        }
#endif
        return v;
    }

    // All ones when x is 0, zero otherwise
    SGCL_INLINE_HOT constexpr uint64_t ct_zero_mask(uint64_t x) noexcept {
        return ct_barrier(((x | (0 - x)) >> 63) - 1);
    }

    // All ones when a == b, zero otherwise
    SGCL_INLINE_HOT constexpr uint64_t ct_eq_mask(uint64_t a, uint64_t b) noexcept {
        return ct_zero_mask(a ^ b);
    }

    // All ones when bit is 1 (bit is 0 or 1)
    SGCL_INLINE_HOT constexpr uint64_t ct_bit_mask(uint64_t bit) noexcept {
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

    // a + b + carry and a - b - borrow, the carry or borrow (0 or 1) in and
    // out through the last argument. By the compiler's builtin where it has
    // one: a run of these is then one chain of add- or subtract-with-carry
    // instructions, the carry in the flags, where a 128-bit sum takes it
    // into a register and back each time. Neither branches
    SGCL_INLINE_HOT constexpr uint64_t add_carry(uint64_t a, uint64_t b, uint64_t& carry) noexcept {
#if defined(__has_builtin)
#if __has_builtin(__builtin_addcll)
        if (!std::is_constant_evaluated()) {
            unsigned long long out;
            uint64_t s = __builtin_addcll(a, b, carry, &out);
            carry = out;
            return s;
        }
#endif
#endif
        u128 t = u128(a) + b + carry;
        carry = uint64_t(t >> 64);
        return uint64_t(t);
    }

    SGCL_INLINE_HOT constexpr uint64_t sub_borrow(uint64_t a, uint64_t b, uint64_t& borrow) noexcept {
#if defined(__has_builtin)
#if __has_builtin(__builtin_subcll)
        if (!std::is_constant_evaluated()) {
            unsigned long long out;
            uint64_t d = __builtin_subcll(a, b, borrow, &out);
            borrow = out;
            return d;
        }
#endif
#endif
        u128 t = u128(a) - b - borrow;
        borrow = uint64_t(t >> 64) & 1;
        return uint64_t(t);
    }

    // hi:lo = a * b
    SGCL_INLINE_HOT constexpr uint64_t mul_wide(uint64_t a, uint64_t b, uint64_t& lo) noexcept {
        u128 p = u128(a) * b;
        lo = uint64_t(p);
        return uint64_t(p >> 64);
    }

    // a - b over N words; the borrow out
    template<size_t N>
    SGCL_INLINE_HOT constexpr uint64_t limbs_sub(limbs<N>& r, const limbs<N>& a, const limbs<N>& b) noexcept {
        uint64_t borrow = 0;
        SGCL_CRYPTO_UNROLL
        for (size_t i = 0; i < N; ++i) {
            r[i] = sub_borrow(a[i], b[i], borrow);
        }
        return borrow;
    }

    // a + b over N words; the carry out
    template<size_t N>
    SGCL_INLINE_HOT constexpr uint64_t limbs_add(limbs<N>& r, const limbs<N>& a, const limbs<N>& b) noexcept {
        uint64_t carry = 0;
        SGCL_CRYPTO_UNROLL
        for (size_t i = 0; i < N; ++i) {
            r[i] = add_carry(a[i], b[i], carry);
        }
        return carry;
    }

    // All ones when a < b, compared in constant time
    template<size_t N>
    SGCL_INLINE_HOT constexpr uint64_t limbs_less_mask(const limbs<N>& a, const limbs<N>& b) noexcept {
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
    SGCL_INLINE_HOT constexpr void limbs_select(limbs<N>& r, uint64_t mask, const limbs<N>& a, const limbs<N>& b) noexcept {
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

    // Whether m is P-521's prime, the Mersenne number 2^521 - 1
    template<size_t N>
    constexpr bool is_p521_prime(const limbs<N>& m) noexcept {
        if constexpr (N != 9) {
            return false;
        } else {
            for (size_t i = 0; i < 8; ++i) {
                if (m[i] != ~uint64_t(0)) {
                    return false;
                }
            }
            return m[8] == 0x1ff;
        }
    }

    // The constants of the arithmetic modulo m: Montgomery's, but for
    // P-521's prime, whose elements are kept as they are (R = 1: the form
    // of 1 is 1, and into the form is a product with 1) and multiplied
    // with a reduction of their own
    template<size_t N>
    consteval MontConstants<N> field_constants(const limbs<N>& m) {
        MontConstants<N> c = mont_constants<N>(m);
        if (is_p521_prime<N>(m)) {
            c.one = limbs<N>{};
            c.one[0] = 1;
            c.r2 = c.one;
        }
        return c;
    }

    // The arithmetic modulo Params::modulus, Params a type with
    // static constexpr size_t words and static constexpr limbs<words> modulus
    template<class Params>
    struct Mont {
        static constexpr size_t N = Params::words;
        static constexpr MontConstants<N> k = field_constants<N>(Params::modulus);

        // P-521's prime 2^521 - 1: its elements kept as they are, the
        // products reduced by folding (p521_reduce)
        static constexpr bool p521_prime = is_p521_prime<N>(Params::modulus);

        using element = limbs<N>;

        // r = t - m when t (N words and a top word hi, below 2m) is at
        // least m, r = t otherwise
        SGCL_INLINE_HOT static constexpr void reduce_once(element& r, const element& t, uint64_t hi) noexcept {
            element u{};
            uint64_t borrow = limbs_sub(u, t, k.m);
            // the whole difference is negative when hi is 0 and a borrow
            // came out of the low words: keep t then
            uint64_t keep = ct_bit_mask(~hi & borrow & 1);
            limbs_select(r, keep, t, u);
        }

        SGCL_INLINE_HOT static constexpr void add(element& r, const element& a, const element& b) noexcept {
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

        // Whether m is P-256's prime p = 2^256 - 2^224 + 2^192 + 2^96 - 1,
        // whose multiplication and squaring have a reduction of their own
        static constexpr bool p256_prime = N == 4 && k.m[0] == ~uint64_t(0) && k.m[1] == 0xffffffff && k.m[2] == 0 && k.m[3] == 0xffffffff00000001;

        // Montgomery's reduction of t (below p^2, eight words) modulo
        // P-256's prime: -p^-1 mod 2^64 is 1, so each step's multiplier q
        // is the lowest word t_i itself, and t + q p 2^(64i) needs one
        // product alone. q (2^64 - 1) clears word i with a carry of q;
        // q (2^32 - 1) and that carry make q 2^32, q << 32 into word i + 1
        // and q >> 32 into word i + 2; the top word of p, 2^64 - 2^32 + 1,
        // is one product into words i + 3 and i + 4. That product's high
        // word is at most 2^64 - 2^32, so the carry out of a step joins the
        // next step's high word with no overflow. The result is below 2p:
        // one subtraction of p at most
        SGCL_INLINE_HOT static constexpr void p256_reduce(element& r, uint64_t (&t)[8]) noexcept {
            uint64_t carry_word = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 4; ++i) {
                uint64_t q = t[i];
                uint64_t lo = 0;
                uint64_t hi = mul_wide(q, 0xffffffff00000001, lo) + carry_word;
                uint64_t c = 0;
                t[i + 1] = add_carry(t[i + 1], q << 32, c);
                t[i + 2] = add_carry(t[i + 2], q >> 32, c);
                t[i + 3] = add_carry(t[i + 3], lo, c);
                t[i + 4] = add_carry(t[i + 4], hi, c);
                carry_word = c;
            }
            element high{t[4], t[5], t[6], t[7]};
            reduce_once(r, high, carry_word);
        }

        // a * b over P-256's prime: the product row by row, the low words
        // of a row's four products added in one chain of carries and the
        // high words in another, then p256_reduce
        SGCL_INLINE_HOT static constexpr void p256_mul(element& r, const element& a, const element& b) noexcept {
            uint64_t t[8] = {};
            uint64_t lo[4] = {};
            uint64_t hi[4] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t j = 0; j < 4; ++j) {
                hi[j] = mul_wide(a[j], b[0], lo[j]);
            }
            uint64_t c = 0;
            t[0] = lo[0];
            t[1] = add_carry(lo[1], hi[0], c);
            t[2] = add_carry(lo[2], hi[1], c);
            t[3] = add_carry(lo[3], hi[2], c);
            t[4] = hi[3] + c;   // a high word is at most 2^64 - 2
            SGCL_CRYPTO_UNROLL
            for (size_t i = 1; i < 4; ++i) {
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 4; ++j) {
                    hi[j] = mul_wide(a[j], b[i], lo[j]);
                }
                c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 4; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                t[i + 4] = c;
                c = 0;
                // no carry out: the sum so far is below 2^(64(i + 5))
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 4; ++j) {
                    t[i + 1 + j] = add_carry(t[i + 1 + j], hi[j], c);
                }
            }
            p256_reduce(r, t);
        }

        // a^2 over P-256's prime: the six cross products, doubled, the four
        // squares added, then p256_reduce
        SGCL_INLINE_HOT static constexpr void p256_sqr(element& r, const element& a) noexcept {
            uint64_t t[8] = {};
            uint64_t l1 = 0, l2 = 0, l3 = 0;
            uint64_t h1 = mul_wide(a[1], a[0], l1);
            uint64_t h2 = mul_wide(a[2], a[0], l2);
            uint64_t h3 = mul_wide(a[3], a[0], l3);
            uint64_t c = 0;
            t[1] = l1;
            t[2] = add_carry(l2, h1, c);
            t[3] = add_carry(l3, h2, c);
            t[4] = h3 + c;
            uint64_t lx = 0, ly = 0, lz = 0;
            uint64_t hx = mul_wide(a[2], a[1], lx);
            uint64_t hy = mul_wide(a[3], a[1], ly);
            uint64_t hz = mul_wide(a[3], a[2], lz);
            c = 0;
            t[3] = add_carry(t[3], lx, c);
            t[4] = add_carry(t[4], ly, c);
            t[5] = c;
            c = 0;
            t[4] = add_carry(t[4], hx, c);
            t[5] = add_carry(t[5], hy, c);
            c = 0;
            t[5] = add_carry(t[5], lz, c);
            t[6] = hz + c;
            // doubled: the cross products are below 2^511
            c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 1; i < 7; ++i) {
                t[i] = add_carry(t[i], t[i], c);
            }
            t[7] = c;
            uint64_t s[8] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 4; ++i) {
                s[2 * i + 1] = mul_wide(a[i], a[i], s[2 * i]);
            }
            c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 8; ++i) {
                t[i] = add_carry(t[i], s[i], c);
            }
            p256_reduce(r, t);
        }

        // t (below p^2, eighteen words) modulo 2^521 - 1: t = lo + 2^521 hi
        // with lo and hi below 2^521, and 2^521 = 1 modulo p, so t = lo + hi,
        // below 2^522; folded once more, below 2^521 + 1; then p
        // subtracted through a mask when the sum is p or 2^521. No branch
        SGCL_INLINE_HOT static constexpr void p521_reduce(element& r, const uint64_t (&t)[18]) noexcept {
            element s{};
            uint64_t c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 9; ++i) {
                // word i of hi: bits 521 + 64 i up
                uint64_t hi = t[8 + i] >> 9 | (i + 9 < 18 ? t[9 + i] << 55 : 0);
                uint64_t lo = i < 8 ? t[i] : (t[8] & 0x1ff);
                s[i] = add_carry(lo, hi, c);
            }
            // s below 2^522: bit 521 (word 8, bit 9) folded back as 1
            uint64_t top = s[8] >> 9;
            s[8] &= 0x1ff;
            c = 0;
            s[0] = add_carry(s[0], top, c);
            SGCL_CRYPTO_UNROLL
            for (size_t i = 1; i < 9; ++i) {
                s[i] = add_carry(s[i], 0, c);
            }
            // now below 2^521 + 1: s - p is s + 1 - 2^521, kept when s >= p
            element d{};
            uint64_t borrow = limbs_sub(d, s, k.m);
            limbs_select(r, ct_bit_mask(borrow), s, d);
        }

        SGCL_INLINE_HOT static constexpr void p521_mul(element& r, const element& a, const element& b) noexcept {
            uint64_t t[18] = {};
            uint64_t lo[9] = {};
            uint64_t hi[9] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 9; ++i) {
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 9; ++j) {
                    hi[j] = mul_wide(a[j], b[i], lo[j]);
                }
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 9; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                t[i + 9] = c;
                c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < 9; ++j) {
                    t[i + 1 + j] = add_carry(t[i + 1 + j], hi[j], c);
                }
            }
            p521_reduce(r, t);
        }

        SGCL_INLINE_HOT static constexpr void p521_sqr(element& r, const element& a) noexcept {
            uint64_t t[18] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i + 1 < 9; ++i) {
                uint64_t lo[9] = {};
                uint64_t hi[9] = {};
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < 9; ++j) {
                    hi[j] = mul_wide(a[j], a[i], lo[j]);
                }
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < 9; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                t[i + 9] = c;
                c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < 9; ++j) {
                    t[i + j + 1] = add_carry(t[i + j + 1], hi[j], c);
                }
            }
            uint64_t c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 1; i < 18; ++i) {
                t[i] = add_carry(t[i], t[i], c);
            }
            uint64_t sq[18] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 9; ++i) {
                sq[2 * i + 1] = mul_wide(a[i], a[i], sq[2 * i]);
            }
            c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 18; ++i) {
                t[i] = add_carry(t[i], sq[i], c);
            }
            p521_reduce(r, t);
        }

        // Montgomery's reduction of t (below m^2, 2N words), N steps of a
        // word each: q = t_i (-m^-1) mod 2^64, t + q m 2^(64i), the low
        // words of q m added in one chain of carries and the high words in
        // another. The first chain's carry joins the high word of q m's top
        // product (at most m_(N-1) - 1, no overflow); the second's, a word
        // above the step, is kept aside and all of them added in one chain
        // at the end (no step reads those words for its q). The result is
        // below 2m: one subtraction of m at most
        SGCL_INLINE_HOT static constexpr void reduce_wide(element& r, uint64_t (&t)[2 * N]) noexcept {
            uint64_t pending[N] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                uint64_t q = t[i] * k.m0inv;
                uint64_t lo[N] = {};
                uint64_t hi[N] = {};
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    hi[j] = mul_wide(q, k.m[j], lo[j]);
                }
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                hi[N - 1] += c;
                c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    t[i + 1 + j] = add_carry(t[i + 1 + j], hi[j], c);
                }
                pending[i] = c;   // into word i + N + 1
            }
            uint64_t c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i + 1 < N; ++i) {
                t[N + 1 + i] = add_carry(t[N + 1 + i], pending[i], c);
            }
            uint64_t top = pending[N - 1] + c;
            element high{};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                high[i] = t[N + i];
            }
            reduce_once(r, high, top);
        }

        // r = a * b / R mod m: the product row by row, the low words of a
        // row's N products added in one chain of carries and the high
        // words in another, then reduce_wide (P-256's prime p256_mul)
        static constexpr void mul(element& r, const element& a, const element& b) noexcept {
            if constexpr (p256_prime) {
                p256_mul(r, a, b);
                return;
            } else if constexpr (p521_prime) {
                p521_mul(r, a, b);
                return;
            }
            uint64_t t[2 * N] = {};
            uint64_t lo[N] = {};
            uint64_t hi[N] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    hi[j] = mul_wide(a[j], b[i], lo[j]);
                }
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                t[i + N] = c;
                c = 0;
                // no carry out: the sum so far is below 2^(64(i + N + 1))
                SGCL_CRYPTO_UNROLL
                for (size_t j = 0; j < N; ++j) {
                    t[i + 1 + j] = add_carry(t[i + 1 + j], hi[j], c);
                }
            }
            reduce_wide(r, t);
        }

        // r = a^2 / R mod m: the product first (each cross product once,
        // then doubled, then the squares added: N(N+1)/2 products against
        // mul's N^2), then reduce_wide (P-256's prime p256_sqr)
        static constexpr void sqr(element& r, const element& a) noexcept {
            if constexpr (p256_prime) {
                p256_sqr(r, a);
                return;
            } else if constexpr (p521_prime) {
                p521_sqr(r, a);
                return;
            }
            uint64_t t[2 * N] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i + 1 < N; ++i) {
                uint64_t lo[N] = {};
                uint64_t hi[N] = {};
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < N; ++j) {
                    hi[j] = mul_wide(a[j], a[i], lo[j]);
                }
                uint64_t c = 0;
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < N; ++j) {
                    t[i + j] = add_carry(t[i + j], lo[j], c);
                }
                t[i + N] = c;
                c = 0;
                // no carry out: the cross products so far are below
                // 2^(64(i + N + 1))
                SGCL_CRYPTO_UNROLL
                for (size_t j = i + 1; j < N; ++j) {
                    t[i + j + 1] = add_carry(t[i + j + 1], hi[j], c);
                }
            }
            // doubled: the cross products are below 2^(128N - 1)
            uint64_t c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 1; i < 2 * N; ++i) {
                t[i] = add_carry(t[i], t[i], c);
            }
            uint64_t s[2 * N] = {};
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < N; ++i) {
                s[2 * i + 1] = mul_wide(a[i], a[i], s[2 * i]);
            }
            // the square is below m^2 < 2^(128N): no carry out
            c = 0;
            SGCL_CRYPTO_UNROLL
            for (size_t i = 0; i < 2 * N; ++i) {
                t[i] = add_carry(t[i], s[i], c);
            }
            reduce_wide(r, t);
        }

        // Into the form (a R mod m) and out of it; a below m
        SGCL_INLINE_HOT static constexpr element to_mont(const element& a) noexcept {
            element r{};
            mul(r, a, k.r2);
            return r;
        }

        SGCL_INLINE_HOT static constexpr element from_mont(const element& a) noexcept {
            element one{};
            one[0] = 1;
            element r{};
            mul(r, a, one);
            return r;
        }

        SGCL_INLINE_HOT static constexpr element one() noexcept {
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
        SGCL_INLINE_HOT static element inverse(const element& a) noexcept {
            return pow_public(a, k.m_minus_2);
        }
    };
}
