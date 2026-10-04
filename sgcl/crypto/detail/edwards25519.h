//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "field25519.h"
#include "../secure_zero.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

// The group of Ed25519 (RFC 8032 §5.1): the twisted Edwards curve
// -x² + y² = 1 + d·x²·y² over the field of field25519.h, d = -121665/121666,
// with the base point B of order L = 2^252 + 27742317777372353535851937790883648493,
// and the integers modulo L the scalars live in.
//
// Points are kept in the coordinates of Hisil, Wong, Carter and Dawson
// ("Twisted Edwards curves revisited", 2008), whose addition for a = -1 is
// complete on this curve (d is not a square): one formula for every pair
// of points, the neutral element and a point with itself included, so no
// case is ever told apart by a branch.
//
//   GeP2      (X : Y : Z), x = X/Z, y = Y/Z — what a doubling starts from
//   GeP3      (X : Y : Z : T), plus T = XY/Z — what an addition takes
//   GeP1P1    ((X : Z), (Y : T)), x = X/Z, y = Y/T — what a doubling or
//             an addition gives, before it is turned into one of the two
//             above (three or four multiplications)
//   GeCached  (Y + X, Y - X, Z, 2d·T) — a point prepared to be added
//   GeNiels   (y + x, y - x, 2d·x·y) — an affine point prepared to be
//             added, as the tables of multiples of B hold it
//
// What is secret. The scalar of a private key and the nonce of a
// signature are: ge_scalarmult_base and the scalar functions sc_reduce and
// sc_muladd take the same time and touch the same memory whatever they
// are, the table of multiples of B read whole with masks. A public key, a
// signature and a message are not: ge_decode (the decompression of a
// point from its bytes), ge_double_scalarmult_vartime (the verification's
// [k]A + [S]B, its windows chosen by the public scalars) and
// sc_is_canonical take branches on their data, as every implementation's
// verification does.
namespace sgcl::crypto::detail {
    // d, 2d, sqrt(-1) and the base point, as field elements; the limbs
    // are the values' own, computed from their definitions
    inline constexpr Fe ed_d = {{0x34dca135978a3, 0x1a8283b156ebd, 0x5e7a26001c029, 0x739c663a03cbb, 0x52036cee2b6ff}};
    inline constexpr Fe ed_d2 = {{0x69b9426b2f159, 0x35050762add7a, 0x3cf44c0038052, 0x6738cc7407977, 0x2406d9dc56dff}};
    inline constexpr Fe ed_sqrtm1 = {{0x61b274a0ea0b0, 0x0d5a5fc8f189d, 0x7ef5e9cbd0c60, 0x78595a6804c9e, 0x2b8324804fc1d}};
    inline constexpr Fe ed_base_x = {{0x62d608f25d51a, 0x412a4b4f6592a, 0x75b7171a4b31d, 0x1ff60527118fe, 0x216936d3cd6e5}};
    inline constexpr Fe ed_base_y = {{0x6666666666658, 0x4cccccccccccc, 0x1999999999999, 0x3333333333333, 0x6666666666666}};

    struct GeP2 {
        Fe X, Y, Z;
    };

    struct GeP3 {
        Fe X, Y, Z, T;
    };

    struct GeP1P1 {
        Fe X, Y, Z, T;
    };

    struct GeCached {
        Fe YplusX, YminusX, Z, T2d;
    };

    struct GeNiels {
        Fe yplusx, yminusx, xy2d;
    };

    SGCL_INLINE_HOT GeP3 ge_identity() noexcept {
        return GeP3{fe_zero(), fe_one(), fe_one(), fe_zero()};
    }

    SGCL_INLINE_HOT GeP3 ge_base() noexcept {
        return GeP3{ed_base_x, ed_base_y, fe_one(), fe_mul(ed_base_x, ed_base_y)};
    }

    SGCL_INLINE_HOT GeP2 ge_to_p2(const GeP1P1& p) noexcept {
        return GeP2{fe_mul(p.X, p.T), fe_mul(p.Y, p.Z), fe_mul(p.Z, p.T)};
    }

    SGCL_INLINE_HOT GeP3 ge_to_p3(const GeP1P1& p) noexcept {
        return GeP3{fe_mul(p.X, p.T), fe_mul(p.Y, p.Z), fe_mul(p.Z, p.T), fe_mul(p.X, p.Y)};
    }

    SGCL_INLINE_HOT GeP2 ge_to_p2(const GeP3& p) noexcept {
        return GeP2{p.X, p.Y, p.Z};
    }

    SGCL_INLINE_HOT GeCached ge_to_cached(const GeP3& p) noexcept {
        return GeCached{fe_add(p.Y, p.X), fe_sub(p.Y, p.X), p.Z, fe_mul(p.T, ed_d2)};
    }

    // 2P (dbl-2008-hwcd with a = -1)
    inline GeP1P1 ge_double(const GeP2& p) noexcept {
        Fe xx = fe_sq(p.X);
        Fe yy = fe_sq(p.Y);
        Fe zz2 = fe_add(fe_sq(p.Z), fe_sq(p.Z));
        Fe xy2 = fe_sq(fe_add(p.X, p.Y));
        GeP1P1 r;
        r.Y = fe_add(yy, xx);
        r.Z = fe_sub(yy, xx);
        r.X = fe_sub(xy2, r.Y);
        r.T = fe_sub(zz2, r.Z);
        return r;
    }

    SGCL_INLINE_HOT GeP1P1 ge_double(const GeP3& p) noexcept {
        return ge_double(ge_to_p2(p));
    }

    // P + Q (add-2008-hwcd-3 with k = 2d)
    inline GeP1P1 ge_add(const GeP3& p, const GeCached& q) noexcept {
        Fe a = fe_mul(fe_sub(p.Y, p.X), q.YminusX);
        Fe b = fe_mul(fe_add(p.Y, p.X), q.YplusX);
        Fe c = fe_mul(p.T, q.T2d);
        Fe zz = fe_mul(p.Z, q.Z);
        Fe d = fe_add(zz, zz);
        return GeP1P1{fe_sub(b, a), fe_add(b, a), fe_add(d, c), fe_sub(d, c)};
    }

    // P - Q: Q's negation (-x, y) swaps Y + X and Y - X and negates T
    inline GeP1P1 ge_sub(const GeP3& p, const GeCached& q) noexcept {
        Fe a = fe_mul(fe_sub(p.Y, p.X), q.YplusX);
        Fe b = fe_mul(fe_add(p.Y, p.X), q.YminusX);
        Fe c = fe_mul(p.T, q.T2d);
        Fe zz = fe_mul(p.Z, q.Z);
        Fe d = fe_add(zz, zz);
        return GeP1P1{fe_sub(b, a), fe_add(b, a), fe_sub(d, c), fe_add(d, c)};
    }

    // P + Q for an affine Q (Z = 1)
    SGCL_INLINE_HOT GeP1P1 ge_madd(const GeP3& p, const GeNiels& q) noexcept {
        Fe a = fe_mul(fe_sub(p.Y, p.X), q.yminusx);
        Fe b = fe_mul(fe_add(p.Y, p.X), q.yplusx);
        Fe c = fe_mul(p.T, q.xy2d);
        Fe d = fe_add(p.Z, p.Z);
        return GeP1P1{fe_sub(b, a), fe_add(b, a), fe_add(d, c), fe_sub(d, c)};
    }

    SGCL_INLINE_HOT GeP1P1 ge_msub(const GeP3& p, const GeNiels& q) noexcept {
        Fe a = fe_mul(fe_sub(p.Y, p.X), q.yplusx);
        Fe b = fe_mul(fe_add(p.Y, p.X), q.yminusx);
        Fe c = fe_mul(p.T, q.xy2d);
        Fe d = fe_add(p.Z, p.Z);
        return GeP1P1{fe_sub(b, a), fe_add(b, a), fe_sub(d, c), fe_add(d, c)};
    }

    SGCL_INLINE_HOT GeP3 ge_neg(const GeP3& p) noexcept {
        return GeP3{fe_neg(p.X), p.Y, p.Z, fe_neg(p.T)};
    }

    // The 32 bytes of RFC 8032 §5.1.2: y little-endian, the low bit of x
    // in bit 255. One inversion, constant time
    SGCL_INLINE_HOT void ge_encode(unsigned char* s, const GeP2& p) noexcept {
        Fe zi = fe_invert(p.Z);
        Fe x = fe_mul(p.X, zi);
        Fe y = fe_mul(p.Y, zi);
        fe_to_bytes(s, y);
        s[31] ^= static_cast<unsigned char>(fe_is_negative(x) << 7);
    }

    SGCL_INLINE_HOT void ge_encode(unsigned char* s, const GeP3& p) noexcept {
        ge_encode(s, ge_to_p2(p));
    }

    // The point of 32 bytes, RFC 8032 §5.1.3 strictly: y must be below p
    // (a non-canonical y is refused, where Go's edwards25519 takes it
    // reduced), x = 0 with the sign bit set is refused, and so is a y with
    // no x on the curve. Variable time: the bytes are a public key or a
    // signature's R, public by definition.
    inline bool ge_decode(GeP3& out, const unsigned char* s) noexcept {
        Fe y = fe_from_bytes(s);
        unsigned char again[32];
        fe_to_bytes(again, y);
        unsigned char top = s[31] & 0x7f;
        if (std::memcmp(again, s, 31) != 0 || again[31] != top) {
            return false;   // y >= p
        }
        uint64_t sign = s[31] >> 7;
        // x² = (y² - 1) / (d·y² + 1) = u / v
        Fe yy = fe_sq(y);
        Fe u = fe_sub(yy, fe_one());
        Fe v = fe_add(fe_mul(yy, ed_d), fe_one());
        // x = u·v³·(u·v⁷)^((p - 5)/8)
        Fe v3 = fe_mul(fe_sq(v), v);
        Fe v7 = fe_mul(fe_sq(v3), v);
        Fe x = fe_mul(fe_mul(u, v3), fe_pow22523(fe_mul(u, v7)));
        Fe vxx = fe_mul(v, fe_sq(x));
        if (!fe_equal(vxx, u)) {
            if (!fe_equal(vxx, fe_neg(u))) {
                return false;   // no square root: y is not on the curve
            }
            x = fe_mul(x, ed_sqrtm1);
        }
        if (fe_is_zero(x) && sign == 1) {
            return false;   // -0
        }
        if (fe_is_negative(x) != sign) {
            x = fe_neg(x);
        }
        out = GeP3{x, y, fe_one(), fe_mul(x, y)};
        return true;
    }

    // --- the tables of multiples of B --------------------------------------

    // base[j][k] = (k + 1)·16^(2j)·B for the windows of the constant-time
    // multiplication, odd[i] = (2i + 1)·B for the verification's: 320
    // affine points, 38 KB, made once on first use (a static of a
    // function, which C++ makes once across threads) with one inversion
    // for all of them. Public data: B's multiples.
    struct GeBaseTables {
        GeNiels base[32][8];
        GeNiels odd[64];

        // noexcept: the one allocation of 64 KB, made once in a process,
        // failing ends the program (as the functions that need the table
        // cannot report it)
        GeBaseTables() noexcept {
            constexpr size_t count = 32 * 8 + 64;
            // the points in projective form and the prefix products, off
            // the stack (64 KB) and freed when the table is made
            std::unique_ptr<GeP3[]> points(new GeP3[count]);
            std::unique_ptr<Fe[]> prefix(new Fe[count]);
            GeP3 p = ge_base();
            size_t n = 0;
            for (int j = 0; j < 32; ++j) {
                GeCached pc = ge_to_cached(p);
                GeP3 q = p;
                for (int k = 0; k < 8; ++k) {
                    points[n++] = q;
                    q = ge_to_p3(ge_add(q, pc));
                }
                for (int i = 0; i < 8; ++i) {
                    p = ge_to_p3(ge_double(p));
                }
            }
            GeP3 b = ge_base();
            GeCached b2 = ge_to_cached(ge_to_p3(ge_double(b)));
            GeP3 q = b;
            for (int i = 0; i < 64; ++i) {
                points[n++] = q;
                q = ge_to_p3(ge_add(q, b2));
            }
            // one inversion for all the Zs (Montgomery's trick): prefix
            // products forward, then the inverse walked back
            Fe acc = fe_one();
            for (size_t i = 0; i < count; ++i) {
                prefix[i] = acc;
                acc = fe_mul(acc, points[i].Z);
            }
            Fe inv = fe_invert(acc);
            for (size_t i = count; i-- > 0;) {
                Fe zi = fe_mul(inv, prefix[i]);
                inv = fe_mul(inv, points[i].Z);
                Fe x = fe_mul(points[i].X, zi);
                Fe y = fe_mul(points[i].Y, zi);
                GeNiels g{fe_add(y, x), fe_sub(y, x), fe_mul(fe_mul(x, y), ed_d2)};
                if (i < 32 * 8) {
                    base[i / 8][i % 8] = g;
                } else {
                    odd[i - 32 * 8] = g;
                }
            }
        }
    };

    inline const GeBaseTables& ge_tables() noexcept {
        static const GeBaseTables tables;
        return tables;
    }

    // t = b·16^(2j)·B for a digit b in -8..8, read from the whole row with
    // masks: every entry is touched, whichever the digit
    inline void ge_select(GeNiels& t, const GeNiels (&row)[8], int8_t b) noexcept {
        uint64_t negative = uint64_t(uint8_t(b) >> 7);
        // |b| with no branch: b - 2b where b is negative
        uint64_t babs = uint64_t(uint8_t(int(b) - ((0 - int(negative)) & int(b)) * 2));
        t = GeNiels{fe_one(), fe_one(), fe_zero()};
        for (int k = 0; k < 8; ++k) {
            uint64_t m = mask_equal(babs, uint64_t(k + 1));
            fe_cmov(t.yplusx, row[k].yplusx, m);
            fe_cmov(t.yminusx, row[k].yminusx, m);
            fe_cmov(t.xy2d, row[k].xy2d, m);
        }
        // -t: y + x and y - x exchanged, 2dxy negated
        uint64_t m = mask_of(negative);
        fe_cswap(t.yplusx, t.yminusx, m);
        fe_cmov(t.xy2d, fe_neg(t.xy2d), m);
    }

    // a·B for a secret scalar a of 32 bytes below 2^255, in constant time:
    // a written in 64 signed digits of 4 bits (-8..7, the top one up to
    // 8), a = Σ e_i·16^i; the odd digits summed first from the rows, the sum
    // times 16, then the even ones added.
    inline GeP3 ge_scalarmult_base(const unsigned char* a) noexcept {
        assert((a[31] & 0x80) == 0 && "ge_scalarmult_base: a scalar of 2^255 or more");
        const GeBaseTables& tables = ge_tables();
        int8_t e[64];
        for (int i = 0; i < 32; ++i) {
            e[2 * i] = int8_t(a[i] & 15);
            e[2 * i + 1] = int8_t(a[i] >> 4);
        }
        int8_t carry = 0;
        for (int i = 0; i < 63; ++i) {
            e[i] = int8_t(e[i] + carry);
            carry = int8_t((e[i] + 8) >> 4);
            e[i] = int8_t(e[i] - carry * 16);
        }
        e[63] = int8_t(e[63] + carry);

        GeP3 h = ge_identity();
        GeNiels t;
        for (int i = 1; i < 64; i += 2) {
            ge_select(t, tables.base[i / 2], e[i]);
            h = ge_to_p3(ge_madd(h, t));
        }
        GeP2 s = ge_to_p2(ge_double(h));
        s = ge_to_p2(ge_double(s));
        s = ge_to_p2(ge_double(s));
        h = ge_to_p3(ge_double(s));
        for (int i = 0; i < 64; i += 2) {
            ge_select(t, tables.base[i / 2], e[i]);
            h = ge_to_p3(ge_madd(h, t));
        }
        secure_zero(e, sizeof e);
        secure_zero_object(t);
        return h;
    }

    // --- variable time: verification only ----------------------------------

    // A scalar below 2^253 in signed digits of width w: every nonzero
    // digit odd, |digit| < 2^(w-1), at least w - 1 zeros after each. The
    // scalar is public (a signature's S, the hash k), so the digits and
    // the positions are found by branches
    inline void sc_signed_window(int8_t (&naf)[256], const unsigned char* s, int w) noexcept {
        assert((s[31] & 0xe0) == 0 && "sc_signed_window: a scalar of 2^253 or more");
        auto bit = [s](int i) -> int {
            return i < 256 ? (s[i >> 3] >> (i & 7)) & 1 : 0;
        };
        std::memset(naf, 0, sizeof naf);
        int carry = 0;
        int width = 1 << w;
        for (int pos = 0; pos < 256;) {
            if (bit(pos) == carry) {
                ++pos;
                continue;
            }
            int window = carry;
            for (int k = 0; k < w; ++k) {
                window += bit(pos + k) << k;
            }
            if (window & (width >> 1)) {
                naf[pos] = int8_t(window - width);
                carry = 1;
            } else {
                naf[pos] = int8_t(window);
                carry = 0;
            }
            pos += w;
        }
    }

    // a·A + b·B for public a and b below 2^253 (Straus: one run of
    // doublings, both scalars' windows added along it), A's odd multiples
    // up to 15A made here, B's up to 127B from the table. Variable time:
    // for the verification of a signature, where every input is public
    inline GeP2 ge_double_scalarmult_vartime(const unsigned char* a, const GeP3& A, const unsigned char* b) noexcept {
        const GeBaseTables& tables = ge_tables();
        int8_t na[256], nb[256];
        sc_signed_window(na, a, 5);
        sc_signed_window(nb, b, 8);

        GeCached ai[8];   // A, 3A, ..., 15A: each the one before plus 2A
        ai[0] = ge_to_cached(A);
        GeCached a2 = ge_to_cached(ge_to_p3(ge_double(A)));
        GeP3 prev = A;
        for (int i = 1; i < 8; ++i) {
            prev = ge_to_p3(ge_add(prev, a2));
            ai[i] = ge_to_cached(prev);
        }

        int i = 255;
        while (i >= 0 && na[i] == 0 && nb[i] == 0) {
            --i;
        }
        GeP2 r{fe_zero(), fe_one(), fe_one()};
        for (; i >= 0; --i) {
            GeP1P1 t = ge_double(r);
            if (na[i] > 0) {
                t = ge_add(ge_to_p3(t), ai[na[i] / 2]);
            } else if (na[i] < 0) {
                t = ge_sub(ge_to_p3(t), ai[(-na[i]) / 2]);
            }
            if (nb[i] > 0) {
                t = ge_madd(ge_to_p3(t), tables.odd[nb[i] / 2]);
            } else if (nb[i] < 0) {
                t = ge_msub(ge_to_p3(t), tables.odd[(-nb[i]) / 2]);
            }
            r = ge_to_p2(t);
        }
        return r;
    }

    // --- scalars modulo L -------------------------------------------------

    // L and floor(2^512 / L) in 64-bit limbs, little-endian, for Barrett's
    // reduction (Handbook of Applied Cryptography, 14.42, with b = 2^64
    // and k = 4)
    inline constexpr uint64_t sc_l[4] = {0x5812631a5cf5d3edull, 0x14def9dea2f79cd6ull, 0, 0x1000000000000000ull};
    inline constexpr uint64_t sc_mu[5] = {0xed9ce5a30a2c131bull, 0x2106215d086329a7ull, 0xffffffffffffffebull,
                                          0xffffffffffffffffull, 0x000000000000000full};

    // r = r - L where r >= L, in constant time (r has five limbs)
    inline void sc_reduce_once(uint64_t (&r)[5]) noexcept {
        uint64_t t[5];
        uint64_t borrow = 0;
        for (int i = 0; i < 5; ++i) {
            u128 d = u128(r[i]) - (i < 4 ? sc_l[i] : 0) - borrow;
            t[i] = uint64_t(d);
            borrow = uint64_t(d >> 64) & 1;
        }
        // no borrow: r was L or more, take r - L
        uint64_t keep = mask_of(borrow);
        for (int i = 0; i < 5; ++i) {
            r[i] = (r[i] & keep) | (t[i] & ~keep);
        }
    }

    // x mod L for x of eight 64-bit limbs (below 2^512), in constant time
    inline void sc_reduce_limbs(uint64_t (&out)[4], const uint64_t (&x)[8]) noexcept {
        // q1 = x / 2^192, q3 = q1·mu / 2^320
        uint64_t prod[10] = {};
        for (int i = 0; i < 5; ++i) {
            uint64_t carry = 0;
            for (int j = 0; j < 5; ++j) {
                u128 t = u128(x[3 + i]) * sc_mu[j] + prod[i + j] + carry;
                prod[i + j] = uint64_t(t);
                carry = uint64_t(t >> 64);
            }
            prod[i + 5] = carry;
        }
        const uint64_t* q3 = prod + 5;
        // r2 = q3·L mod 2^320
        uint64_t r2[5] = {};
        for (int i = 0; i < 5; ++i) {
            uint64_t carry = 0;
            for (int j = 0; i + j < 5; ++j) {
                u128 t = u128(q3[i]) * (j < 4 ? sc_l[j] : 0) + r2[i + j] + carry;
                r2[i + j] = uint64_t(t);
                carry = uint64_t(t >> 64);
            }
        }
        // r = x mod 2^320 - r2 (mod 2^320): below 3L by the Handbook's
        // bound. For this L it is below 2L (the estimate q3 is at most one
        // short: its error is below frac(2^512 / L) ≈ 0.22 plus 2^-60), so
        // the second correction never subtracts; it stays, a few cycles,
        // so that the code is the Handbook's and not an argument about L
        uint64_t r[5];
        uint64_t borrow = 0;
        for (int i = 0; i < 5; ++i) {
            u128 d = u128(x[i]) - r2[i] - borrow;
            r[i] = uint64_t(d);
            borrow = uint64_t(d >> 64) & 1;
        }
        sc_reduce_once(r);
        sc_reduce_once(r);
        for (int i = 0; i < 4; ++i) {
            out[i] = r[i];
        }
        secure_zero(prod, sizeof prod);
        secure_zero(r2, sizeof r2);
        secure_zero(r, sizeof r);
    }

    // 64 bytes little-endian (a SHA-512 digest) reduced mod L to 32 bytes
    inline void sc_reduce(unsigned char* out, const unsigned char* in) noexcept {
        uint64_t x[8];
        for (int i = 0; i < 8; ++i) {
            x[i] = load_le64(in + 8 * i);
        }
        uint64_t r[4];
        sc_reduce_limbs(r, x);
        for (int i = 0; i < 4; ++i) {
            store_le64(out + 8 * i, r[i]);
        }
        secure_zero(x, sizeof x);
        secure_zero(r, sizeof r);
    }

    // (a·b + c) mod L for scalars of 32 bytes (a and b below 2^256, c below
    // 2^255 and a·b + c below 2^512), in constant time: a signature's S
    inline void sc_muladd(unsigned char* out, const unsigned char* a, const unsigned char* b, const unsigned char* c) noexcept {
        uint64_t al[4], bl[4];
        for (int i = 0; i < 4; ++i) {
            al[i] = load_le64(a + 8 * i);
            bl[i] = load_le64(b + 8 * i);
        }
        uint64_t x[8] = {};
        for (int i = 0; i < 4; ++i) {
            x[i] = load_le64(c + 8 * i);
        }
        for (int i = 0; i < 4; ++i) {
            uint64_t carry = 0;
            for (int j = 0; j < 4; ++j) {
                u128 t = u128(al[i]) * bl[j] + x[i + j] + carry;
                x[i + j] = uint64_t(t);
                carry = uint64_t(t >> 64);
            }
            // propagate into the limbs above: at most to x[7]
            for (int k = i + 4; k < 8; ++k) {
                u128 t = u128(x[k]) + carry;
                x[k] = uint64_t(t);
                carry = uint64_t(t >> 64);
            }
        }
        uint64_t r[4];
        sc_reduce_limbs(r, x);
        for (int i = 0; i < 4; ++i) {
            store_le64(out + 8 * i, r[i]);
        }
        secure_zero(al, sizeof al);
        secure_zero(bl, sizeof bl);
        secure_zero(x, sizeof x);
        secure_zero(r, sizeof r);
    }

    // Whether 32 bytes little-endian are below L: a signature's S (RFC
    // 8032 §5.1.7 requires 0 <= S < L). S is public; variable time
    inline bool sc_is_canonical(const unsigned char* s) noexcept {
        for (int i = 3; i >= 0; --i) {
            uint64_t w = load_le64(s + 8 * i);
            if (w != sc_l[i]) {
                return w < sc_l[i];
            }
        }
        return false;   // equal to L
    }
}
