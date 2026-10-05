//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "ec_field.h"
#include "../secure_zero.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

// The NIST prime curves P-256 and P-384 (FIPS 186-5, SP 800-186 §3.2.1):
// y^2 = x^3 - 3x + b over the integers modulo a prime p, a group of prime
// order n with cofactor 1. Points are projective (X:Y:Z), x = X/Z and
// y = Y/Z, the identity (0:1:0); every coordinate is a field element in
// Montgomery form.
//
// Addition and doubling are the complete formulas of Renes, Costello and
// Batina ("Complete addition formulas for prime order elliptic curves",
// EUROCRYPT 2016), Algorithms 4, 5 and 6 for a = -3: one sequence of
// field operations for every pair of points — the identity, a point added
// to itself, a point added to its negative — so that nothing branches on
// which case a secret point is.
//
// Multiplication by a scalar goes five bits at a time in Booth's signed
// digits, the multiple for each window taken from a table by reading every
// entry and keeping the one whose index matches through a mask (no index
// into memory depends on the scalar) and negated through a mask. A point
// given at run time has a table of its sixteen multiples made per call and
// its doublings run in Jacobian coordinates; the generator has one table
// per window, made once (52 windows of sixteen affine points for P-256,
// 52 KB; 77 for P-384, 116 KB), so that k*G is additions alone.
namespace sgcl::crypto::detail {
    struct P256 {
        static constexpr size_t words = 4;
        static constexpr size_t size = 32;   // bytes of a coordinate and of a scalar

        struct field {
            static constexpr size_t words = 4;
            static constexpr limbs<4> modulus = limbs_from_hex<4>("ffffffff00000001000000000000000000000000ffffffffffffffffffffffff");
        };

        struct scalar {
            static constexpr size_t words = 4;
            static constexpr limbs<4> modulus = limbs_from_hex<4>("ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551");
        };

        static constexpr limbs<4> b = limbs_from_hex<4>("5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b");
        static constexpr limbs<4> gx = limbs_from_hex<4>("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296");
        static constexpr limbs<4> gy = limbs_from_hex<4>("4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");

        // prime256v1, 1.2.840.10045.3.1.7 (RFC 5480 §2.1.1.1), the body of
        // the DER OBJECT IDENTIFIER
        static constexpr unsigned char oid[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
        static constexpr const char* name = "P-256";
    };

    struct P384 {
        static constexpr size_t words = 6;
        static constexpr size_t size = 48;

        struct field {
            static constexpr size_t words = 6;
            static constexpr limbs<6> modulus = limbs_from_hex<6>("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff");
        };

        struct scalar {
            static constexpr size_t words = 6;
            static constexpr limbs<6> modulus = limbs_from_hex<6>("ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973");
        };

        static constexpr limbs<6> b = limbs_from_hex<6>("b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef");
        static constexpr limbs<6> gx = limbs_from_hex<6>("aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7");
        static constexpr limbs<6> gy = limbs_from_hex<6>("3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f");

        // secp384r1, 1.3.132.0.34
        static constexpr unsigned char oid[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
        static constexpr const char* name = "P-384";
    };

    template<class C>
    struct Curve {
        static constexpr size_t N = C::words;
        static constexpr size_t size = C::size;

        using F = Mont<typename C::field>;
        using S = Mont<typename C::scalar>;
        using fe = limbs<N>;

        struct point {
            fe x, y, z;
        };

        struct affine {
            fe x, y;
        };

        static constexpr fe coeff_b = F::to_mont(C::b);   // b, in Montgomery form
        static constexpr fe gx = F::to_mont(C::gx);
        static constexpr fe gy = F::to_mont(C::gy);

        SGCL_INLINE_HOT static point identity() noexcept {
            return point{fe{}, F::one(), fe{}};
        }

        SGCL_INLINE_HOT static point generator() noexcept {
            return point{gx, gy, F::one()};
        }

        // All ones when p is the identity (Z = 0)
        SGCL_INLINE_HOT static uint64_t identity_mask(const point& p) noexcept {
            return limbs_zero_mask(p.z);
        }

        SGCL_INLINE_HOT static void select(point& r, uint64_t mask, const point& a, const point& other) noexcept {
            limbs_select(r.x, mask, a.x, other.x);
            limbs_select(r.y, mask, a.y, other.y);
            limbs_select(r.z, mask, a.z, other.z);
        }

        // Algorithm 4 of Renes-Costello-Batina: p + q for any two points;
        // r may be p or q
        static void add(point& r, const point& p, const point& q) noexcept {
            fe t0, t1, t2, t3, t4, x3, y3, z3;
            F::mul(t0, p.x, q.x);
            F::mul(t1, p.y, q.y);
            F::mul(t2, p.z, q.z);
            F::add(t3, p.x, p.y);
            F::add(t4, q.x, q.y);
            F::mul(t3, t3, t4);
            F::add(t4, t0, t1);
            F::sub(t3, t3, t4);
            F::add(t4, p.y, p.z);
            F::add(x3, q.y, q.z);
            F::mul(t4, t4, x3);
            F::add(x3, t1, t2);
            F::sub(t4, t4, x3);
            F::add(x3, p.x, p.z);
            F::add(y3, q.x, q.z);
            F::mul(x3, x3, y3);
            F::add(y3, t0, t2);
            F::sub(y3, x3, y3);
            F::mul(z3, coeff_b, t2);
            F::sub(x3, y3, z3);
            F::add(z3, x3, x3);
            F::add(x3, x3, z3);
            F::sub(z3, t1, x3);
            F::add(x3, t1, x3);
            F::mul(y3, coeff_b, y3);
            F::add(t1, t2, t2);
            F::add(t2, t1, t2);
            F::sub(y3, y3, t2);
            F::sub(y3, y3, t0);
            F::add(t1, y3, y3);
            F::add(y3, t1, y3);
            F::add(t1, t0, t0);
            F::add(t0, t1, t0);
            F::sub(t0, t0, t2);
            F::mul(t1, t4, y3);
            F::mul(t2, t0, y3);
            F::mul(y3, x3, z3);
            F::add(y3, y3, t2);
            F::mul(x3, t3, x3);
            F::sub(x3, x3, t1);
            F::mul(z3, t4, z3);
            F::mul(t1, t3, t0);
            F::add(z3, z3, t1);
            r.x = x3;
            r.y = y3;
            r.z = z3;
        }

        // Algorithm 5: p + q for q affine (Z = 1) and not the identity,
        // any p; r may be p
        static void add_mixed(point& r, const point& p, const affine& q) noexcept {
            fe t0, t1, t2, t3, t4, x3, y3, z3;
            F::mul(t0, p.x, q.x);
            F::mul(t1, p.y, q.y);
            F::add(t3, q.x, q.y);
            F::add(t4, p.x, p.y);
            F::mul(t3, t3, t4);
            F::add(t4, t0, t1);
            F::sub(t3, t3, t4);
            F::mul(t4, q.y, p.z);
            F::add(t4, t4, p.y);
            F::mul(y3, q.x, p.z);
            F::add(y3, y3, p.x);
            F::mul(z3, coeff_b, p.z);
            F::sub(x3, y3, z3);
            F::add(z3, x3, x3);
            F::add(x3, x3, z3);
            F::sub(z3, t1, x3);
            F::add(x3, t1, x3);
            F::mul(y3, coeff_b, y3);
            F::add(t1, p.z, p.z);
            F::add(t2, t1, p.z);
            F::sub(y3, y3, t2);
            F::sub(y3, y3, t0);
            F::add(t1, y3, y3);
            F::add(y3, t1, y3);
            F::add(t1, t0, t0);
            F::add(t0, t1, t0);
            F::sub(t0, t0, t2);
            F::mul(t1, t4, y3);
            F::mul(t2, t0, y3);
            F::mul(y3, x3, z3);
            F::add(y3, y3, t2);
            F::mul(x3, t3, x3);
            F::sub(x3, x3, t1);
            F::mul(z3, t4, z3);
            F::mul(t1, t3, t0);
            F::add(z3, z3, t1);
            r.x = x3;
            r.y = y3;
            r.z = z3;
        }

        // Algorithm 6: 2p for any point; r may be p
        static void dbl(point& r, const point& p) noexcept {
            fe t0, t1, t2, t3, x3, y3, z3;
            F::sqr(t0, p.x);
            F::sqr(t1, p.y);
            F::sqr(t2, p.z);
            F::mul(t3, p.x, p.y);
            F::add(t3, t3, t3);
            F::mul(z3, p.x, p.z);
            F::add(z3, z3, z3);
            F::mul(y3, coeff_b, t2);
            F::sub(y3, y3, z3);
            F::add(x3, y3, y3);
            F::add(y3, x3, y3);
            F::sub(x3, t1, y3);
            F::add(y3, t1, y3);
            F::mul(y3, x3, y3);
            F::mul(x3, x3, t3);
            F::add(t3, t2, t2);
            F::add(t2, t2, t3);
            F::mul(z3, coeff_b, z3);
            F::sub(z3, z3, t2);
            F::sub(z3, z3, t0);
            F::add(t3, z3, z3);
            F::add(z3, z3, t3);
            F::add(t3, t0, t0);
            F::add(t0, t3, t0);
            F::sub(t0, t0, t2);
            F::mul(t0, t0, z3);
            F::add(y3, y3, t0);
            F::mul(t0, p.y, p.z);
            F::add(t0, t0, t0);
            F::mul(z3, t0, z3);
            F::sub(x3, x3, z3);
            F::mul(z3, t0, t1);
            F::add(z3, z3, z3);
            F::add(z3, z3, z3);
            r.x = x3;
            r.y = y3;
            r.z = z3;
        }

        // The affine coordinates of p (Montgomery form), through one
        // inversion of Z; the identity gives (0, 0), which is on no curve
        // here (b is not 0), and identity_mask says which it was
        SGCL_INLINE_HOT static affine to_affine(const point& p) noexcept {
            fe zi = F::inverse(p.z);
            affine a;
            F::mul(a.x, p.x, zi);
            F::mul(a.y, p.y, zi);
            return a;
        }

        // x^3 - 3x + b
        static fe rhs(const fe& x) noexcept {
            fe x3, t;
            F::sqr(x3, x);
            F::mul(x3, x3, x);
            F::add(t, x, x);
            F::add(t, t, x);
            F::sub(x3, x3, t);
            F::add(x3, x3, coeff_b);
            return x3;
        }

        // Whether (x, y), in Montgomery form, satisfies the curve's
        // equation; the coordinates of a public key, not a secret
        static bool on_curve(const fe& x, const fe& y) noexcept {
            fe y2;
            F::sqr(y2, y);
            fe r = rhs(x);
            fe d;
            F::sub(d, y2, r);
            return limbs_zero_mask(d) != 0;
        }

        // A square root of a (Montgomery form) when there is one: p = 3
        // mod 4 for both curves, so it is a^((p+1)/4), checked by squaring
        static bool sqrt(fe& r, const fe& a) noexcept {
            r = F::pow_public(a, F::k.sqrt_exp);
            fe s;
            F::sqr(s, r);
            fe d;
            F::sub(d, s, a);
            return limbs_zero_mask(d) != 0;
        }

        // Jacobian coordinates (X:Y:Z), x = X/Z^2 and y = Y/Z^3, for the
        // runs of doublings between two additions only: a doubling there
        // is 3M + 5S (dbl-2001-b of the Explicit-Formulas Database, a = -3)
        // against Algorithm 6's 10M + 3S. The formula has no exceptional
        // case on these curves (no point of order 2); the identity, Z = 0,
        // doubles to Z = 0. Every addition stays the complete Algorithm 4,
        // on points taken back to projective coordinates
        struct jacobian {
            fe x, y, z;
        };

        // (X:Y:Z) projective is (XZ : YZ^2 : Z) Jacobian
        SGCL_INLINE_HOT static void to_jacobian(jacobian& j, const point& p) noexcept {
            fe z2;
            F::sqr(z2, p.z);
            F::mul(j.x, p.x, p.z);
            F::mul(j.y, p.y, z2);
            j.z = p.z;
        }

        // (X:Y:Z) Jacobian is (XZ : Y : Z^3) projective; Z = 0 (the
        // identity, whatever X and Y became) is made (0:1:0) through a mask
        SGCL_INLINE_HOT static void from_jacobian(point& p, const jacobian& j) noexcept {
            fe z2;
            F::sqr(z2, j.z);
            F::mul(p.z, z2, j.z);
            F::mul(p.x, j.x, j.z);
            limbs_select(p.y, limbs_zero_mask(j.z), F::one(), j.y);
        }

        // 2j in place: delta = Z^2, gamma = Y^2, beta = X gamma,
        // alpha = 3 (X - delta)(X + delta), X3 = alpha^2 - 8 beta,
        // Z3 = (Y + Z)^2 - gamma - delta, Y3 = alpha (4 beta - X3) - 8 gamma^2
        static void dbl_jacobian(jacobian& j) noexcept {
            fe delta, gamma, beta, alpha, t, u;
            F::sqr(delta, j.z);
            F::sqr(gamma, j.y);
            F::mul(beta, j.x, gamma);
            F::sub(t, j.x, delta);
            F::add(u, j.x, delta);
            F::mul(alpha, t, u);
            F::add(t, alpha, alpha);
            F::add(alpha, t, alpha);
            F::add(t, j.y, j.z);
            F::sqr(t, t);
            F::sub(t, t, gamma);
            F::sub(j.z, t, delta);
            F::add(beta, beta, beta);
            F::add(beta, beta, beta);        // 4 beta
            F::sqr(t, alpha);
            F::add(u, beta, beta);           // 8 beta
            F::sub(j.x, t, u);
            F::sub(t, beta, j.x);
            F::mul(t, alpha, t);
            F::sqr(gamma, gamma);
            F::add(gamma, gamma, gamma);
            F::add(gamma, gamma, gamma);
            F::add(gamma, gamma, gamma);     // 8 gamma^2
            F::sub(j.y, t, gamma);
        }

        // p doubled five times, through Jacobian coordinates
        static void dbl5(point& p) noexcept {
            jacobian j;
            to_jacobian(j, p);
            for (int i = 0; i < 5; ++i) {
                dbl_jacobian(j);
            }
            from_jacobian(p, j);
        }

        // Booth's recoding of a scalar into signed digits of five bits
        // (windows of five with one bit of overlap): k = sum d_i 32^i with
        // d_i = b(5i..5i+4) + b(5i-1) - 32 b(5i+4), each in [-16, 16], so
        // that a table of sixteen multiples serves the negative digits too
        // (a negated point is its y negated). 8 * size / 5 + 1 windows:
        // 52 for P-256, 77 for P-384. The window index is public; the
        // digit, its sign and its size are secret, and made with no branch
        static constexpr size_t booth_windows = 8 * size / 5 + 1;

        SGCL_INLINE_HOT static unsigned bit(const unsigned char* k, size_t pos) noexcept {
            return pos < 8 * size ? (k[size - 1 - pos / 8] >> (pos % 8)) & 1u : 0u;
        }

        static void booth_digit(const unsigned char* k, size_t i, uint64_t& magnitude, uint64_t& negative) noexcept {
            uint64_t v = 0;
            for (unsigned j = 0; j < 5; ++j) {
                v |= uint64_t(bit(k, 5 * i + j)) << j;
            }
            v += i == 0 ? 0 : bit(k, 5 * i - 1);   // 0..32
            negative = bit(k, 5 * i + 4);
            uint64_t m = ct_bit_mask(negative);
            magnitude = ((32 - v) & m) | (v & ~m);
        }

        // k * p for a scalar k (size bytes, big-endian, any value — the
        // secret of an ECDH key, or a public u2 of a verification) and any
        // point p: the multiples 1..16 of p, then for each window from the
        // top five doublings and the addition of the multiple the digit
        // names, found by a scan of the whole table and negated through a
        // mask when the digit is negative; a digit of 0 adds the identity
        static point scalar_mult(const point& p, const unsigned char* k) noexcept {
            point table[17];
            table[0] = identity();
            table[1] = p;
            for (size_t i = 2; i <= 16; ++i) {
                if (i % 2 == 0) {
                    dbl(table[i], table[i / 2]);
                } else {
                    add(table[i], table[i - 1], p);
                }
            }
            point r = identity();
            for (size_t w = booth_windows; w-- > 0;) {
                if (w != booth_windows - 1) {
                    dbl5(r);
                }
                uint64_t mag, neg;
                booth_digit(k, w, mag, neg);
                point t = table[0];
                for (size_t j = 1; j <= 16; ++j) {
                    select(t, ct_eq_mask(j, mag), table[j], t);
                }
                fe ny;
                F::sub(ny, fe{}, t.y);
                limbs_select(t.y, ct_bit_mask(neg), ny, t.y);
                add(r, r, t);
            }
            secure_zero(table, sizeof table);
            return r;
        }

        // --- Variable time: public data alone (a signature's verification) ---
        //
        // u1 G + u2 Q in one run of doublings (Straus and Shamir's trick),
        // each scalar in its width-w non-adjacent form (odd digits, at
        // least w - 1 zeros after each), so that a digit of 0 costs
        // nothing: the generator's odd multiples up to 63 G in a table made
        // once (affine, mixed additions), Q's up to 15 Q made per call
        // (Jacobian). The doublings and additions are Jacobian, with the
        // cases their formulas leave out (the identity, a point added to
        // itself or to its negative) branched on: nothing here is secret

        SGCL_INLINE_HOT static bool is_zero(const fe& a) noexcept {
            return limbs_zero_mask(a) != 0;
        }

        SGCL_INLINE_HOT static jacobian jacobian_identity() noexcept {
            return jacobian{F::one(), F::one(), fe{}};
        }

        // p + q, q affine: madd-2007-bl (7M + 4S)
        static void add_affine_vartime(jacobian& p, const affine& q) noexcept {
            if (is_zero(p.z)) {
                p = jacobian{q.x, q.y, F::one()};
                return;
            }
            fe z1z1, u2, s2, h, rr;
            F::sqr(z1z1, p.z);
            F::mul(u2, q.x, z1z1);
            F::mul(s2, q.y, p.z);
            F::mul(s2, s2, z1z1);
            F::sub(h, u2, p.x);
            F::sub(rr, s2, p.y);
            if (is_zero(h)) {
                if (is_zero(rr)) {
                    dbl_jacobian(p);
                } else {
                    p = jacobian_identity();
                }
                return;
            }
            fe hh, i, j, v, t, x3, y3, z3;
            F::add(rr, rr, rr);
            F::sqr(hh, h);
            F::add(i, hh, hh);
            F::add(i, i, i);
            F::mul(j, h, i);
            F::mul(v, p.x, i);
            F::sqr(x3, rr);
            F::sub(x3, x3, j);
            F::sub(x3, x3, v);
            F::sub(x3, x3, v);
            F::sub(t, v, x3);
            F::mul(y3, rr, t);
            F::mul(t, p.y, j);
            F::add(t, t, t);
            F::sub(y3, y3, t);
            F::add(z3, p.z, h);
            F::sqr(z3, z3);
            F::sub(z3, z3, z1z1);
            F::sub(z3, z3, hh);
            p = jacobian{x3, y3, z3};
        }

        // p + q: add-2007-bl (11M + 5S)
        static void add_vartime(jacobian& p, const jacobian& q) noexcept {
            if (is_zero(q.z)) {
                return;
            }
            if (is_zero(p.z)) {
                p = q;
                return;
            }
            fe z1z1, z2z2, u1, u2, s1, s2, h, rr;
            F::sqr(z1z1, p.z);
            F::sqr(z2z2, q.z);
            F::mul(u1, p.x, z2z2);
            F::mul(u2, q.x, z1z1);
            F::mul(s1, p.y, q.z);
            F::mul(s1, s1, z2z2);
            F::mul(s2, q.y, p.z);
            F::mul(s2, s2, z1z1);
            F::sub(h, u2, u1);
            F::sub(rr, s2, s1);
            if (is_zero(h)) {
                if (is_zero(rr)) {
                    dbl_jacobian(p);
                } else {
                    p = jacobian_identity();
                }
                return;
            }
            fe i, j, v, t, x3, y3, z3;
            F::add(rr, rr, rr);
            F::add(i, h, h);
            F::sqr(i, i);
            F::mul(j, h, i);
            F::mul(v, u1, i);
            F::sqr(x3, rr);
            F::sub(x3, x3, j);
            F::sub(x3, x3, v);
            F::sub(x3, x3, v);
            F::sub(t, v, x3);
            F::mul(y3, rr, t);
            F::mul(t, s1, j);
            F::add(t, t, t);
            F::sub(y3, y3, t);
            F::add(z3, p.z, q.z);
            F::sqr(z3, z3);
            F::sub(z3, z3, z1z1);
            F::sub(z3, z3, z2z2);
            F::mul(z3, z3, h);
            p = jacobian{x3, y3, z3};
        }

        // The width-(w + 1) non-adjacent form of a public scalar (size
        // bytes, big-endian): digits d_i, odd or 0, |d_i| < 2^w, k = the
        // sum of d_i 2^i; 8 size + 1 of them
        static constexpr size_t wnaf_digits = 8 * size + 1;

        template<unsigned W>
        static void wnaf(int8_t (&d)[wnaf_digits], const unsigned char* k) noexcept {
            constexpr int half = 1 << W;
            constexpr int full = half << 1;
            // the low W + 1 bits not yet written as digits, with the carry
            // of the digits written negative
            int window = 0;
            for (unsigned j = 0; j <= W; ++j) {
                window |= int(bit(k, j)) << j;
            }
            for (size_t j = 0; j < wnaf_digits; ++j) {
                int digit = 0;
                if (window & 1) {
                    digit = (window & half) ? window - full : window;
                    window -= digit;
                }
                d[j] = int8_t(digit);
                window >>= 1;
                window += int(bit(k, j + W + 1)) << W;
            }
        }

        // The generator's odd multiples 1G, 3G, ..., 63G, affine
        struct OddTable {
            affine t[32];
        };

        static const OddTable& odd_table() noexcept {
            static const std::unique_ptr<OddTable> table = build_odd_table();
            return *table;
        }

        // Made projective by the complete formulas and turned affine
        // together, one inversion for all (Montgomery's trick)
        static std::unique_ptr<OddTable> build_odd_table() noexcept {
            point pts[32];
            point g2;
            dbl(g2, generator());
            pts[0] = generator();
            for (size_t i = 1; i < 32; ++i) {
                add(pts[i], pts[i - 1], g2);
            }
            fe prefix[32];
            fe acc = F::one();
            for (size_t i = 0; i < 32; ++i) {
                prefix[i] = acc;
                F::mul(acc, acc, pts[i].z);
            }
            fe inv = F::inverse(acc);
            auto table = std::make_unique<OddTable>();
            for (size_t i = 32; i-- > 0;) {
                fe zi;
                F::mul(zi, inv, prefix[i]);
                F::mul(inv, inv, pts[i].z);
                F::mul(table->t[i].x, pts[i].x, zi);
                F::mul(table->t[i].y, pts[i].y, zi);
            }
            return table;
        }

        // u1 G + u2 Q (scalars of size bytes, big-endian; q affine, on the
        // curve), Jacobian
        static jacobian double_mult_vartime(const unsigned char* u1, const affine& q, const unsigned char* u2) noexcept {
            int8_t d1[wnaf_digits];
            int8_t d2[wnaf_digits];
            wnaf<6>(d1, u1);
            wnaf<4>(d2, u2);
            jacobian qt[8];
            qt[0] = jacobian{q.x, q.y, F::one()};
            jacobian q2 = qt[0];
            dbl_jacobian(q2);
            for (size_t i = 1; i < 8; ++i) {
                qt[i] = qt[i - 1];
                add_vartime(qt[i], q2);
            }
            const OddTable& gt = odd_table();
            jacobian r = jacobian_identity();
            bool started = false;
            for (size_t i = wnaf_digits; i-- > 0;) {
                if (started) {
                    dbl_jacobian(r);
                }
                if (int g = d1[i]; g != 0) {
                    affine a = gt.t[(g < 0 ? -g : g) / 2];
                    if (g < 0) {
                        F::sub(a.y, fe{}, a.y);
                    }
                    add_affine_vartime(r, a);
                    started = true;
                }
                if (int h = d2[i]; h != 0) {
                    jacobian b = qt[(h < 0 ? -h : h) / 2];
                    if (h < 0) {
                        F::sub(b.y, fe{}, b.y);
                    }
                    add_vartime(r, b);
                    started = true;
                }
            }
            return r;
        }

        // The generator's table: base[w][j - 1] = j * 32^w * G, affine
        struct BaseTable {
            affine t[booth_windows][16];
        };

        static const BaseTable& base_table() noexcept {
            static const std::unique_ptr<BaseTable> table = build_base_table();
            return *table;
        }

        // The table's points made projective and turned affine together,
        // one inversion for all of them (Montgomery's trick). Public data:
        // multiples of the generator
        static std::unique_ptr<BaseTable> build_base_table() noexcept {
            constexpr size_t count = booth_windows * 16;
            auto points = std::make_unique<point[]>(count);
            point g = generator();
            for (size_t w = 0; w < booth_windows; ++w) {
                point* row = &points[w * 16];
                row[0] = g;
                for (size_t j = 1; j < 16; ++j) {
                    add(row[j], row[j - 1], g);
                }
                dbl(g, row[15]);   // 32 * g: the next window's base
            }
            auto prefix = std::make_unique<fe[]>(count);
            fe acc = F::one();
            for (size_t i = 0; i < count; ++i) {
                prefix[i] = acc;
                F::mul(acc, acc, points[i].z);
            }
            fe inv = F::inverse(acc);
            auto table = std::make_unique<BaseTable>();
            for (size_t i = count; i-- > 0;) {
                fe zi;
                F::mul(zi, inv, prefix[i]);          // 1 / z_i
                F::mul(inv, inv, points[i].z);       // 1 / (z_0 ... z_(i-1))
                affine& a = table->t[i / 16][i % 16];
                F::mul(a.x, points[i].x, zi);
                F::mul(a.y, points[i].y, zi);
            }
            return table;
        }

        // k * G for a scalar k (size bytes, big-endian, any value): one
        // mixed addition per window of the entry the digit names, found by
        // a scan of the window's sixteen entries and negated through a mask
        // when the digit is negative; a digit of 0 adds nothing, the sum
        // kept through a mask
        static point base_mult(const unsigned char* k) noexcept {
            const BaseTable& table = base_table();
            point r = identity();
            for (size_t w = 0; w < booth_windows; ++w) {
                uint64_t mag, neg;
                booth_digit(k, w, mag, neg);
                affine t{};
                for (size_t j = 0; j < 16; ++j) {
                    uint64_t m = ct_eq_mask(j + 1, mag);
                    limbs_select(t.x, m, table.t[w][j].x, t.x);
                    limbs_select(t.y, m, table.t[w][j].y, t.y);
                }
                fe ny;
                F::sub(ny, fe{}, t.y);
                limbs_select(t.y, ct_bit_mask(neg), ny, t.y);
                point s;
                add_mixed(s, r, t);
                select(r, ct_zero_mask(mag), r, s);
            }
            return r;
        }
    };
}
