//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "nat.h"

#include <algorithm>
#include <cassert>

// The fast multiplications of magnitudes: Karatsuba's (three products of
// half the length for four) and Toom's in three parts (five products of a
// third of the length for nine), each with a square of its own that needs
// fewer products still, over the schoolbook ones of nat.h below their
// thresholds. Written from Knuth, TAOCP vol. 2, 4.3.3, and for the
// interpolation of Toom-3 from Bodrato and Zanoni, "Integer and
// polynomial multiplication: towards optimal Toom-Cook matrices" (2007):
// the points 0, 1, -1, -2 and infinity, and the sequence of exact
// divisions by 2 and 3 it gives.
namespace sgcl::math::detail {
    // Where each algorithm takes over, in limbs of the shorter operand
    // (of the one operand for a square, of the divisor and the quotient
    // for a division, of the number for the conversions). Set by
    // measurement against the algorithm below each (bench_math); the
    // tests lower them to a few limbs so that every road is taken on
    // small numbers and checked against the schoolbook one. Nothing but a
    // test writes them.
    struct Thresholds {
        size_t karatsuba = 22;
        size_t toom3 = 160;
        size_t square_karatsuba = 40;
        size_t square_toom3 = 220;
        size_t burnikel_ziegler = 20;
        size_t to_string = 32;
        size_t parse = 100;
    };

    inline Thresholds thresholds;

    // Working memory handed down a recursion: each call takes what it
    // needs from the front before it calls deeper, and a call deeper
    // takes from what is left, so the calls one after another at the same
    // depth use the same limbs. The size is worked out beforehand
    // (mul_scratch); a debug build checks it is enough.
    class Arena {
    public:
        Arena(Limb* p, size_t n) noexcept
        : _p(p)
        , _left(n) {
        }

        Limb* take(size_t n) noexcept {
            assert(n <= _left);
            Limb* r = _p;
            _p += n;
            _left -= n;
            return r;
        }

    private:
        Limb* _p;
        size_t _left;
    };

    // Enough working memory for a product whose longer operand has n
    // limbs: a level of Karatsuba takes at most 4n + 5 limbs and one of
    // Toom-3 4n + 20, and hands on at most n/2 + 1 to the level below;
    // the split of an unbalanced product takes 2m for a shorter operand
    // of m <= (n + 1)/2 and hands on m
    inline size_t mul_scratch(size_t n) noexcept {
        size_t s = 0;
        for (;;) {
            s += 4 * n + 24;
            if (n <= 2) {
                return s;
            }
            n = n / 2 + 1;
        }
    }

    // The length once the zeros at the top are dropped, and the operands
    // of the recursions below compared and subtracted at their true
    // lengths: |x - y| into r of n limbs (x, y of at most n), whether y
    // was the larger returned
    inline bool abs_diff(Limb* r, size_t n, const Limb* x, size_t xn, const Limb* y, size_t yn) noexcept {
        xn = normalized(x, xn);
        yn = normalized(y, yn);
        bool swapped = compare(x, xn, y, yn) < 0;
        if (swapped) {
            std::swap(x, y);
            std::swap(xn, yn);
        }
        sub(r, x, xn, y, yn);
        std::memset(r + xn, 0, (n - xn) * sizeof(Limb));
        return swapped;
    }

    // r = x + y for numbers with signs (a magnitude and whether it is
    // negative), into r of n limbs, which must hold the result; the sign
    // of the result returned (never negative for zero). r may be x or y.
    inline bool signed_add(Limb* r, size_t n, const Limb* x, size_t xn, bool xneg, const Limb* y, size_t yn, bool yneg) noexcept {
        xn = normalized(x, xn);
        yn = normalized(y, yn);
        if (xneg == yneg) {
            if (xn < yn) {
                std::swap(x, y);
                std::swap(xn, yn);
            }
            Limb carry = add_n(r, x, y, yn);
            for (size_t i = yn; i < xn; ++i) {
                Limb s = x[i] + carry;
                carry = s < carry;
                r[i] = s;
            }
            if (xn < n) {
                r[xn++] = carry;
            } else {
                assert(!carry);
            }
            std::memset(r + xn, 0, (n - xn) * sizeof(Limb));
            return xn && xneg && normalized(r, xn);
        }
        int c = compare(x, xn, y, yn);
        if (c < 0) {
            std::swap(x, y);
            std::swap(xn, yn);
            xneg = yneg;
        }
        sub(r, x, xn, y, yn);
        std::memset(r + xn, 0, (n - xn) * sizeof(Limb));
        return c != 0 && xneg;
    }

    // a / 3 in place for an a that 3 divides: each limb of the quotient is
    // the limb of what is left times the inverse of 3 modulo 2^64, and
    // what that product takes above the limb is borrowed from the next
    // (exact division, Jebelean 1993)
    inline void divexact_3(Limb* a, size_t n) noexcept {
        constexpr Limb Inverse = 0xaaaaaaaaaaaaaaabull;   // 3 · Inverse ≡ 1 mod 2^64
        Limb borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            Limb x = a[i];
            Limb s = x - borrow;
            Limb under = x < borrow;
            Limb q = s * Inverse;
            a[i] = q;
            Limb lo;
            borrow = mul_wide(q, 3, lo) + under;
        }
        assert(!borrow);
    }

    // a >>= 1 in place, over n limbs
    inline void halve(Limb* a, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            a[i] = (a[i] >> 1) | (i + 1 < n ? a[i + 1] << 63 : 0);
        }
    }

    inline void mul_rec(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept;
    inline void sqr_rec(Limb* r, const Limb* a, size_t n, Arena arena) noexcept;

    // An operand much longer than the other: cut into pieces of the
    // shorter one's length, each piece's product added in at its place
    inline void mul_unbalanced(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept {
        Limb* t = arena.take(2 * bn);
        mul_rec(r, a, bn, b, bn, arena);
        for (size_t done = bn; done < an;) {
            size_t len = std::min(bn, an - done);
            mul_rec(t, b, bn, a + done, len, arena);
            // r[done .. done + bn) holds the top of what is there so far;
            // above it nothing has been written yet
            Limb carry = add_n(r + done, r + done, t, bn);
            std::memcpy(r + done + bn, t + bn, len * sizeof(Limb));
            carry = add_1_in(r + done + bn, len, carry);
            assert(!carry);
            done += len;
        }
    }

    // Karatsuba for an >= bn > (an + 1)/2: a = a1·β^m + a0 and b likewise
    // with m = ceil(an/2), and the middle term from one product of
    // differences — a0·b1 + a1·b0 = a0·b0 + a1·b1 + (a0 - a1)(b1 - b0) —
    // whose operands stay within m limbs, where their sums would not
    inline void mul_karatsuba(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept {
        size_t m = (an + 1) / 2;
        size_t n = an + bn;
        Limb* da = arena.take(m);
        Limb* db = arena.take(m);
        Limb* t = arena.take(2 * m);
        Limb* mid = arena.take(2 * m + 1);
        bool na = abs_diff(da, m, a, m, a + m, an - m);   // a0 - a1 < 0
        bool nb = abs_diff(db, m, b + m, bn - m, b, m);   // b1 - b0 < 0
        mul_rec(r, a, m, b, m, arena);
        mul_rec(r + 2 * m, a + m, an - m, b + m, bn - m, arena);
        mul_rec(t, da, m, db, m, arena);
        std::memcpy(mid, r, 2 * m * sizeof(Limb));
        mid[2 * m] = add_in(mid, 2 * m, r + 2 * m, n - 2 * m);
        if (na == nb) {
            mid[2 * m] += add_n(mid, mid, t, 2 * m);
        } else {
            mid[2 * m] -= sub_n(mid, mid, t, 2 * m);
        }
        Limb carry = add_in(r + m, n - m, mid, normalized(mid, 2 * m + 1));
        assert(!carry);
        (void)carry;
    }

    inline void sqr_karatsuba(Limb* r, const Limb* a, size_t n, Arena arena) noexcept {
        size_t m = (n + 1) / 2;
        Limb* da = arena.take(m);
        Limb* t = arena.take(2 * m);
        Limb* mid = arena.take(2 * m + 1);
        abs_diff(da, m, a, m, a + m, n - m);
        sqr_rec(r, a, m, arena);
        sqr_rec(r + 2 * m, a + m, n - m, arena);
        sqr_rec(t, da, m, arena);
        // 2·a0·a1 = a0² + a1² - (a0 - a1)²
        std::memcpy(mid, r, 2 * m * sizeof(Limb));
        mid[2 * m] = add_in(mid, 2 * m, r + 2 * m, 2 * n - 2 * m);
        mid[2 * m] -= sub_n(mid, mid, t, 2 * m);
        Limb carry = add_in(r + m, 2 * n - m, mid, normalized(mid, 2 * m + 1));
        assert(!carry);
        (void)carry;
    }

    // The values of a = a2·x² + a1·x + a0 (x = β^k) at 1, -1 and -2, each
    // into k + 1 limbs with its sign: a(1) = (a0 + a2) + a1, a(-1) =
    // (a0 + a2) - a1, a(-2) = 2(a(-1) + a2) - a0
    struct Toom3Points {
        Limb* at1;
        Limb* at_minus1;
        Limb* at_minus2;
        bool minus1_negative;
        bool minus2_negative;
    };

    inline Toom3Points toom3_evaluate(const Limb* a, size_t an, size_t k, Arena& arena) noexcept {
        size_t l = k + 1;
        Toom3Points p{arena.take(l), arena.take(l), arena.take(l), false, false};
        const Limb* a0 = a;
        const Limb* a1 = a + k;
        const Limb* a2 = a + 2 * k;
        size_t a2n = an - 2 * k;
        Limb* sum = p.at_minus2;   // a0 + a2, until a(-2) is written over it
        signed_add(sum, l, a0, k, false, a2, a2n, false);
        signed_add(p.at1, l, sum, l, false, a1, k, false);
        p.minus1_negative = signed_add(p.at_minus1, l, sum, l, false, a1, k, true);
        bool n2 = signed_add(p.at_minus2, l, p.at_minus1, l, p.minus1_negative, a2, a2n, false);
        Limb out = shift_left(p.at_minus2, p.at_minus2, l, 1);
        assert(!out);
        (void)out;
        p.minus2_negative = signed_add(p.at_minus2, l, p.at_minus2, l, n2, a0, k, true);
        return p;
    }

    // From the products at 1, -1 and -2 (in w1, wm1, wm2, of w limbs each,
    // with the signs of the last two) and the ones at 0 and infinity
    // already in place in r, the three middle coefficients, added in at
    // their places: r holds c0 in [0, 2k), c4 in [4k, n), and zeros between
    inline void toom3_interpolate(Limb* r, size_t n, size_t k, Limb* w1, Limb* wm1, bool nm1, Limb* wm2, bool nm2, size_t w) noexcept {
        const Limb* c0 = r;
        const Limb* c4 = r + 4 * k;
        size_t c4n = n - 4 * k;
        // r3 = (r(-2) - r(1)) / 3
        bool n3 = signed_add(wm2, w, wm2, w, nm2, w1, w, true);
        divexact_3(wm2, w);
        // r1 = (r(1) - r(-1)) / 2, never negative
        bool n1 = signed_add(w1, w, w1, w, false, wm1, w, !nm1);
        halve(w1, w);
        // r2 = r(-1) - c0
        bool n2 = signed_add(wm1, w, wm1, w, nm1, c0, 2 * k, true);
        // r3 = (r2 - r3) / 2 + 2·c4: this is c3
        n3 = signed_add(wm2, w, wm1, w, n2, wm2, w, !n3);
        halve(wm2, w);
        n3 = signed_add(wm2, w, wm2, w, n3, c4, c4n, false);
        n3 = signed_add(wm2, w, wm2, w, n3, c4, c4n, false);
        // r2 = r2 + r1 - c4: this is c2
        n2 = signed_add(wm1, w, wm1, w, n2, w1, w, n1);
        n2 = signed_add(wm1, w, wm1, w, n2, c4, c4n, true);
        // r1 = r1 - c3: this is c1
        n1 = signed_add(w1, w, w1, w, n1, wm2, w, !n3);
        assert(!n1 && !n2 && !n3);
        (void)n1;
        (void)n2;
        (void)n3;
        Limb carry = add_in(r + k, n - k, w1, normalized(w1, w));
        carry |= add_in(r + 2 * k, n - 2 * k, wm1, normalized(wm1, w));
        carry |= add_in(r + 3 * k, n - 3 * k, wm2, normalized(wm2, w));
        assert(!carry);
        (void)carry;
    }

    // Toom-3 for an >= bn > 2·ceil(an/3): both in three parts of k =
    // ceil(an/3) limbs (the top ones shorter), five products instead of
    // nine
    inline void mul_toom3(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept {
        size_t k = (an + 2) / 3;
        size_t n = an + bn;
        size_t l = k + 1;
        size_t w = 2 * l;
        Toom3Points pa = toom3_evaluate(a, an, k, arena);
        Toom3Points pb = toom3_evaluate(b, bn, k, arena);
        Limb* w1 = arena.take(w);
        Limb* wm1 = arena.take(w);
        Limb* wm2 = arena.take(w);
        mul_rec(r, a, k, b, k, arena);
        mul_rec(r + 4 * k, a + 2 * k, an - 2 * k, b + 2 * k, bn - 2 * k, arena);
        std::memset(r + 2 * k, 0, 2 * k * sizeof(Limb));
        mul_rec(w1, pa.at1, l, pb.at1, l, arena);
        mul_rec(wm1, pa.at_minus1, l, pb.at_minus1, l, arena);
        mul_rec(wm2, pa.at_minus2, l, pb.at_minus2, l, arena);
        bool nm1 = (pa.minus1_negative != pb.minus1_negative) && normalized(wm1, w);
        bool nm2 = (pa.minus2_negative != pb.minus2_negative) && normalized(wm2, w);
        toom3_interpolate(r, n, k, w1, wm1, nm1, wm2, nm2, w);
    }

    inline void sqr_toom3(Limb* r, const Limb* a, size_t an, Arena arena) noexcept {
        size_t k = (an + 2) / 3;
        size_t n = 2 * an;
        size_t l = k + 1;
        size_t w = 2 * l;
        Toom3Points pa = toom3_evaluate(a, an, k, arena);
        Limb* w1 = arena.take(w);
        Limb* wm1 = arena.take(w);
        Limb* wm2 = arena.take(w);
        sqr_rec(r, a, k, arena);
        sqr_rec(r + 4 * k, a + 2 * k, an - 2 * k, arena);
        std::memset(r + 2 * k, 0, 2 * k * sizeof(Limb));
        sqr_rec(w1, pa.at1, l, arena);
        sqr_rec(wm1, pa.at_minus1, l, arena);
        sqr_rec(wm2, pa.at_minus2, l, arena);
        toom3_interpolate(r, n, k, w1, wm1, false, wm2, false, w);
    }

    // r = a · b for an >= bn, r of an + bn limbs, all written, neither a
    // nor b; the road chosen by the shorter length
    inline void mul_rec(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept {
        assert(an >= bn);
        if (bn < std::max<size_t>(thresholds.karatsuba, 2)) {
            mul_basecase(r, a, an, b, bn);
        } else if (bn <= (an + 1) / 2) {
            mul_unbalanced(r, a, an, b, bn, arena);
        } else if (bn < std::max<size_t>(thresholds.toom3, 3) || bn <= 2 * ((an + 2) / 3)) {
            mul_karatsuba(r, a, an, b, bn, arena);
        } else {
            mul_toom3(r, a, an, b, bn, arena);
        }
    }

    inline void sqr_rec(Limb* r, const Limb* a, size_t n, Arena arena) noexcept {
        if (n < std::max<size_t>(thresholds.square_karatsuba, 2)) {
            sqr_basecase(r, a, n);
        } else if (n < std::max<size_t>(thresholds.square_toom3, 3)) {
            sqr_karatsuba(r, a, n, arena);
        } else {
            sqr_toom3(r, a, n, arena);
        }
    }

    // r = a · b in either order, r of an + bn limbs, all written, neither
    // a nor b; working memory from the arena given, which holds at least
    // mul_scratch(min(max(an, bn), 2·min(an, bn)))
    inline void mul(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn, Arena arena) noexcept {
        if (an < bn) {
            std::swap(a, b);
            std::swap(an, bn);
        }
        if (!bn) {
            std::memset(r, 0, an * sizeof(Limb));
            return;
        }
        if (a == b && an == bn) {
            sqr_rec(r, a, an, arena);
        } else {
            mul_rec(r, a, an, b, bn, arena);
        }
    }

    // The working memory mul above wants for operands of these lengths
    inline size_t mul_scratch(size_t an, size_t bn) noexcept {
        size_t longer = std::max(an, bn);
        size_t shorter = std::min(an, bn);
        return mul_scratch(std::min(longer, 2 * shorter));
    }

    // The same with its own working memory; the same operand twice is
    // squared
    inline void mul(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) {
        bool square = a == b && an == bn;
        if (square ? an < thresholds.square_karatsuba : std::min(an, bn) < thresholds.karatsuba) {
            if (square) {
                sqr_basecase(r, a, an);
            } else if (an >= bn) {
                mul_basecase(r, a, an, b, bn);
            } else {
                mul_basecase(r, b, bn, a, an);
            }
            return;
        }
        size_t size = mul_scratch(an, bn);
        Scratch s(size);
        mul(r, a, an, b, bn, Arena(s.get(), size));
    }
}
