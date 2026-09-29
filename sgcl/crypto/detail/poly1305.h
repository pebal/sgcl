//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "words.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// Poly1305 (RFC 8439 §2.5): a one-time authenticator. The key's first half
// r, clamped, is a point; the message, in 16-byte blocks each with a 1
// appended (2^128, or 2^(8·len) for a short last block), is a polynomial
// evaluated at r modulo p = 2^130 - 5 by Horner's rule (h = (h + m)·r), and
// the tag is h + s (the key's second half) modulo 2^128.
//
// The accumulator and r are held in three limbs of 44, 44 and 42 bits, so
// that a product of two limbs, and the sum of three such, fits the 128 bits
// of a wide multiplication (64 x 64 -> 128: one mul and one umulh on arm64,
// one mul on x86-64). 2^130 ≡ 5 modulo p, so a product that lands at 2^132
// comes back at the bottom times 20: the two limbs of r above the first
// are kept multiplied by 20 as well. Carries are propagated with shifts
// and masks, the final reduction below p selects with a mask computed from
// the sign of h - p: nothing branches on the key or the accumulator. Four
// blocks go a step, against r^4, r^3, r^2 and r computed with the key: one
// carry per four blocks, and three of the four products free of the chain
// through h.
namespace sgcl::crypto::detail {
    struct Poly1305 {
        static constexpr uint64_t mask44 = (uint64_t(1) << 44) - 1;
        static constexpr uint64_t mask42 = (uint64_t(1) << 42) - 1;

        // A number modulo p in limbs of 44, 44 and 42 bits (the top ones
        // may run a little over between steps)
        struct Limbs {
            uint64_t v0, v1, v2;
        };

        // A multiplier: its limbs, and the upper two times 20
        struct Power {
            uint64_t v0, v1, v2, s1, s2;
        };

        // A product before its carries: three 128-bit columns, native
        // 128-bit integers where the compiler has them (an add and an
        // add-with-carry each), two words otherwise
#if defined(__SIZEOF_INT128__)
        using Column = unsigned __int128;

        static void mac(Column& c, uint64_t a, uint64_t b) noexcept {
            c += Column(a) * b;
        }

        static uint64_t low(Column c) noexcept {
            return uint64_t(c);
        }

        static uint64_t shift(Column c, int n) noexcept {
            return uint64_t(c >> n);
        }

        static void add(Column& c, uint64_t v) noexcept {
            c += v;
        }
#else
        using Column = Wide;

        static void mac(Column& c, uint64_t a, uint64_t b) noexcept {
            add_wide(c, mul64(a, b));
        }

        static uint64_t low(Column c) noexcept {
            return c.lo;
        }

        static uint64_t shift(Column c, int n) noexcept {
            return shift_right(c, n);
        }

        static void add(Column& c, uint64_t v) noexcept {
            add_wide(c, {v, 0});
        }
#endif

        struct Product {
            Column d0, d1, d2;
        };

        Power powers[4];         // r, r^2, r^3, r^4
        Limbs h;                 // the accumulator
        uint64_t pad0, pad1;     // s, the key's second half
        unsigned char buffer[16];
        size_t buffered;

        static Power power(const Limbs& a) noexcept {
            return {a.v0, a.v1, a.v2, a.v1 * 20, a.v2 * 20};
        }

        // (a0 + a1·2^44 + a2·2^88)·(b0 + b1·2^44 + b2·2^88) added to the
        // columns, the products at 2^132 and above folded down times 20
        static void mul_add(Product& p, const Limbs& a, const Power& b) noexcept {
            mac(p.d0, a.v0, b.v0);
            mac(p.d0, a.v1, b.s2);
            mac(p.d0, a.v2, b.s1);
            mac(p.d1, a.v0, b.v1);
            mac(p.d1, a.v1, b.v0);
            mac(p.d1, a.v2, b.s2);
            mac(p.d2, a.v0, b.v2);
            mac(p.d2, a.v1, b.v1);
            mac(p.d2, a.v2, b.v0);
        }

        // The columns carried into limbs: 44, 44, 42 bits, the excess of
        // the top times 5 back at the bottom (2^130 ≡ 5)
        static Limbs carry(Product p) noexcept {
            Limbs a;
            uint64_t c = shift(p.d0, 44);
            a.v0 = low(p.d0) & mask44;
            add(p.d1, c);
            c = shift(p.d1, 44);
            a.v1 = low(p.d1) & mask44;
            add(p.d2, c);
            c = shift(p.d2, 42);
            a.v2 = low(p.d2) & mask42;
            a.v0 += c * 5;
            c = a.v0 >> 44;
            a.v0 &= mask44;
            a.v1 += c;
            return a;
        }

        static Limbs load(const unsigned char* p, uint64_t high) noexcept {
            uint64_t t0 = load_le64(p), t1 = load_le64(p + 8);
            return {t0 & mask44, (t0 >> 44 | t1 << 20) & mask44, (t1 >> 24) | high};
        }

        // §2.5: r &= 0x0ffffffc0ffffffc0ffffffc0fffffff; and r^2, r^3, r^4
        // for four blocks a step
        void init(const unsigned char* key) noexcept {
            uint64_t t0 = load_le64(key) & 0x0ffffffc0fffffffull;
            uint64_t t1 = load_le64(key + 8) & 0x0ffffffc0ffffffcull;
            Limbs r = {t0 & mask44, (t0 >> 44 | t1 << 20) & mask44, (t1 >> 24) & mask42};
            powers[0] = power(r);
            for (int i = 1; i < 4; ++i) {
                Product p = {};
                mul_add(p, r, powers[i - 1]);
                powers[i] = power(carry(p));
            }
            h = {0, 0, 0};
            pad0 = load_le64(key + 16);
            pad1 = load_le64(key + 24);
            buffered = 0;
        }

        // n whole blocks, each with `high` (2^128 as bit 40 of the third
        // limb, or 0 for a padded last block) added. Four blocks a step:
        // h' = (h + m1)·r^4 + m2·r^3 + m3·r^2 + m4·r, which is four steps of
        // Horner's rule, with one carry and three of the four products off
        // the chain that runs through h.
        void blocks(const unsigned char* p, size_t n, uint64_t high) noexcept {
            Limbs a = h;
            for (; n >= 4; n -= 4, p += 64) {
                Limbs m1 = load(p, high), m2 = load(p + 16, high), m3 = load(p + 32, high), m4 = load(p + 48, high);
                // the three products that do not wait for h first, so that
                // they run ahead of it
                Product d = {};
                mul_add(d, m4, powers[0]);
                mul_add(d, m3, powers[1]);
                mul_add(d, m2, powers[2]);
                mul_add(d, {a.v0 + m1.v0, a.v1 + m1.v1, a.v2 + m1.v2}, powers[3]);
                a = carry(d);
            }
            for (; n > 0; --n, p += 16) {
                Limbs m = load(p, high);
                Product d = {};
                mul_add(d, {a.v0 + m.v0, a.v1 + m.v1, a.v2 + m.v2}, powers[0]);
                a = carry(d);
            }
            h = a;
        }

        void update(const unsigned char* p, size_t n) noexcept {
            if (buffered > 0) {
                size_t take = 16 - buffered < n ? 16 - buffered : n;
                sgcl::detail::copy_bytes(buffer + buffered, p, take);
                buffered += take;
                p += take;
                n -= take;
                if (buffered < 16) {
                    return;
                }
                blocks(buffer, 1, uint64_t(1) << 40);
                buffered = 0;
            }
            size_t whole = n / 16;
            blocks(p, whole, uint64_t(1) << 40);
            p += whole * 16;
            n -= whole * 16;
            if (n > 0) {
                sgcl::detail::copy_bytes(buffer, p, n);
                buffered = n;
            }
        }

        // The tag; the state is wiped
        void finish(unsigned char* tag) noexcept {
            if (buffered > 0) {
                // the short last block: its bytes, a 1, zeros; no 2^128
                buffer[buffered] = 1;
                for (size_t i = buffered + 1; i < 16; ++i) {
                    buffer[i] = 0;
                }
                blocks(buffer, 1, 0);
            }
            uint64_t a0 = h.v0, a1 = h.v1, a2 = h.v2;
            // the carries through twice: then a < 2^130
            for (int i = 0; i < 2; ++i) {
                uint64_t c = a0 >> 44;
                a0 &= mask44;
                a1 += c;
                c = a1 >> 44;
                a1 &= mask44;
                a2 += c;
                c = a2 >> 42;
                a2 &= mask42;
                a0 += c * 5;
            }
            // once more without the wrap: every limb within its width, but
            // for the top one, which may reach 2^42 (a in [2^130, 2^130 + 5))
            uint64_t c = a0 >> 44;
            a0 &= mask44;
            a1 += c;
            c = a1 >> 44;
            a1 &= mask44;
            a2 += c;
            // g = a + 5 - 2^130; a >= p exactly when g does not borrow
            uint64_t g0 = a0 + 5;
            c = g0 >> 44;
            g0 &= mask44;
            uint64_t g1 = a1 + c;
            c = g1 >> 44;
            g1 &= mask44;
            uint64_t g2 = a2 + c - (uint64_t(1) << 42);
            const uint64_t use_g = (g2 >> 63) - 1;   // all ones when g2 did not borrow
            a0 = (a0 & ~use_g) | (g0 & use_g);
            a1 = (a1 & ~use_g) | (g1 & use_g);
            a2 = (a2 & ~use_g) | (g2 & mask42 & use_g);
            // the tag: a + s modulo 2^128
            uint64_t lo = a0 | a1 << 44;
            uint64_t hi = a1 >> 20 | a2 << 24;
            uint64_t t = lo + pad0;
            hi += pad1 + uint64_t(t < pad0);
            store_le64(tag, t);
            store_le64(tag + 8, hi);
            secure_zero_object(*this);
        }
    };
}
