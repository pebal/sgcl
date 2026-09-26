//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "mul.h"

// The division of magnitudes by a divisor of two limbs or more (one limb
// goes to div_1, nat.h): Knuth's algorithm D (The Art of Computer
// Programming, vol. 2, 4.3.1), quadratic, and above its threshold the
// recursive division of Burnikel and Ziegler ("Fast recursive division",
// 1998, in the form of Brent and Zimmermann, Modern Computer Arithmetic,
// 1.4.3), which costs two recursive divisions of half the size and two
// products of that size — as much as a few multiplications.
namespace sgcl::math::detail {
    // The loop of algorithm D over u[0..un) by a normalized v[0..vn) (the
    // top bit of its top limb set, vn >= 2), for u whose top vn limbs are
    // below v: the quotient into q[0 .. un - vn), the remainder left in
    // u[0..vn). q is not u.
    //
    // The estimate of each quotient limb from the top two limbs of the
    // remainder and the top limb of the divisor is at most two too large,
    // and the test against the divisor's second limb (D3) leaves it at
    // most one too large — the case that the multiplication and
    // subtraction (D4) find by going negative, and put right by adding
    // the divisor back once (D6). The add-back comes up about twice in
    // 2^64 digits and is tested by a case built for it
    // (tests/math/big_int.cpp, DivisionAddBack).
    inline void div_knuth_core(Limb* q, Limb* u, size_t un, const Limb* v, size_t vn) noexcept {
        const Limb top = v[vn - 1];
        const Limb next = v[vn - 2];
        const Divisor dv(top);
        for (size_t j = un - vn; j-- > 0;) {
            // D3: the estimate q̂ and its remainder r̂ from the top two limbs
            Limb u1 = u[j + vn];
            Limb u0 = u[j + vn - 1];
            Limb qhat;
            Limb rhat;
            bool rhat_big;   // r̂ >= 2^64: the test below cannot fail
            if (u1 >= top) {
                // u1 == top (the remainder is below the divisor): the
                // estimate would not fit a limb, so it is 2^64 - 1, and
                // r̂ = u1·2^64 + u0 - q̂·top = u0 + top
                qhat = ~Limb(0);
                rhat = u0 + top;
                rhat_big = rhat < u0;
            } else {
                qhat = dv.divide(u1, u0, rhat);
                rhat_big = false;
            }
            // q̂·v[n-2] > r̂·2^64 + u[j+n-2]: one too large, at most twice
            while (!rhat_big) {
                Limb plo;
                Limb phi = mul_wide(qhat, next, plo);
                Limb u2 = u[j + vn - 2];
                if (phi < rhat || (phi == rhat && plo <= u2)) {
                    break;
                }
                --qhat;
                Limb old = rhat;
                rhat += top;
                rhat_big = rhat < old;
            }
            // D4: u[j .. j+n] -= q̂·v
            Limb borrow = mul_sub_1(u + j, v, vn, qhat);
            Limb t = u[j + vn];
            u[j + vn] = t - borrow;
            if (t < borrow) {
                // D6: the estimate was one too large; add the divisor back
                --qhat;
                u[j + vn] += add_n(u + j, u + j, v, vn);
            }
            q[j] = qhat;
        }
    }

    // q = a / b and r = a % b, for a and b normalized, bn >= 2 and
    // an >= bn. q has room for an - bn + 1 limbs and r for bn, all
    // written; r may be null when only the quotient is wanted. Neither
    // may be a or b.
    //
    // Both are shifted first until the divisor's top bit is set (step
    // D1), in working memory of the call's own, the dividend with one
    // limb more, which the shift leaves below the divisor
    inline void div_knuth(Limb* q, Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) {
        unsigned s = unsigned(std::countl_zero(b[bn - 1]));
        Scratch us(an + 1);
        Scratch vs(bn);
        Limb* u = us.get();
        Limb* v = vs.get();
        if (s) {
            shift_left(v, b, bn, s);
            u[an] = shift_left(u, a, an, s);
        } else {
            std::memcpy(v, b, bn * sizeof(Limb));
            std::memcpy(u, a, an * sizeof(Limb));
            u[an] = 0;
        }
        div_knuth_core(q, u, an + 1, v, bn);
        if (r) {
            if (s) {
                shift_right(r, u, bn, s);
            } else {
                std::memcpy(r, u, bn * sizeof(Limb));
            }
        }
    }

    // r[0..rn) -= (x + top·β^xn)·y, where the product has at most rn + 1
    // limbs; what the subtraction borrowed past the top of r returned (0
    // when the result is not negative). t has room for xn + yn + 1 limbs.
    inline Limb sub_product(Limb* r, size_t rn, const Limb* x, size_t xn, Limb top, const Limb* y, size_t yn, Limb* t, Arena arena) noexcept {
        size_t tn = xn + yn;
        mul(t, x, xn, y, yn, arena);
        t[tn] = 0;
        if (top) {
            Limb carry = add_in(t + xn, yn + 1, y, yn);
            assert(!carry);
            (void)carry;
        }
        ++tn;
        if (tn <= rn) {
            return sub_in(r, rn, t, tn);
        }
        assert(tn == rn + 1);
        return sub_n(r, r, t, rn) + t[rn];
    }

    // a[0 .. n+m) divided by the normalized b[0..n), m <= n: the quotient
    // into q[0..m) with its limb above (0 or 1) returned, the remainder
    // into a[0..n). Brent and Zimmermann's RecursiveDivRem: the top half
    // of the quotient from the top of the dividend over the top of the
    // divisor, corrected for the divisor's low half (at most a few times,
    // since the divisor is normalized); the bottom half the same way from
    // what that leaves.
    inline Limb div_bz_rec(Limb* q, Limb* a, const Limb* b, size_t n, size_t m, Arena arena) noexcept {
        if (m < std::max<size_t>(thresholds.burnikel_ziegler, 4)) {
            Limb top = 0;
            if (compare_n(a + m, b, n) >= 0) {
                sub_n(a + m, a + m, b, n);
                top = 1;
            }
            if (m) {
                div_knuth_core(q, a, n + m, b, n);
            }
            return top;
        }
        Limb* t = arena.take(n + 1);
        if (m < n) {
            // A quotient shorter than the divisor depends on the top of
            // both: s = n - m limbs of each dropped, the quotient of the
            // rest (2m over m) is at most a few too large, and the
            // divisor's low limbs, subtracted as below, find out by how
            // much. Without this the divisor would stay n - m limbs long
            // down to the leaves, and the leaves cost m·(n - m).
            size_t s = n - m;
            Limb top = div_bz_rec(q, a + s, b + s, m, m, arena);
            int64_t over = -int64_t(sub_product(a, n, q, m, top, b, s, t, arena));
            while (over < 0) {
                top -= sub_1_in(q, m, 1);
                over += int64_t(add_n(a, a, b, n));
            }
            return top;
        }
        size_t k = m / 2;
        // Q1 from a[2k ..) over B1 = b[k..n); its remainder into a[2k .. n+k)
        Limb t1 = div_bz_rec(q + k, a + 2 * k, b + k, n - k, m - k, arena);
        // a[0 .. n+k) = R1·β^2k + a[0..2k) - Q1·B0·β^k, made good by
        // adding β^k·B back while it is negative
        int64_t over = -int64_t(sub_product(a + k, n, q + k, m - k, t1, b, k, t, arena));
        while (over < 0) {
            t1 -= sub_1_in(q + k, m - k, 1);
            over += int64_t(add_n(a + k, a + k, b, n));
        }
        // Q0 from a[k .. n+k) over B1; its remainder into a[k..n)
        Limb t0 = div_bz_rec(q, a + k, b + k, n - k, k, arena);
        over = -int64_t(sub_product(a, n, q, k, t0, b, k, t, arena));
        t1 += add_1_in(q + k, m - k, t0);
        while (over < 0) {
            t1 -= sub_1_in(q, m, 1);
            over += int64_t(add_n(a, a, b, n));
        }
        return t1;
    }

    // The working memory of div_bz_rec for a divisor of n limbs and a
    // quotient of at most n: the products' buffers of every level (n + 1
    // at the top, a level that shortens the divisor to the quotient's
    // length and then levels that halve), and the multiplication's own
    inline size_t div_bz_scratch(size_t n) noexcept {
        return 4 * n + 256 + mul_scratch(n);
    }

    // The same contract as div_knuth, by Burnikel–Ziegler: the dividend,
    // shifted as in D1, is divided a block of the divisor's length of
    // quotient at a time from the top, each block by div_bz_rec — the
    // first one shorter when the quotient's length is not a multiple
    inline void div_bz(Limb* q, Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) {
        unsigned s = unsigned(std::countl_zero(b[bn - 1]));
        size_t un = an + 1;
        size_t qn = un - bn;
        Scratch us(un);
        Scratch vs(bn);
        size_t size = div_bz_scratch(bn);
        Scratch ws(size);
        Limb* u = us.get();
        Limb* v = vs.get();
        if (s) {
            shift_left(v, b, bn, s);
            u[an] = shift_left(u, a, an, s);
        } else {
            std::memcpy(v, b, bn * sizeof(Limb));
            std::memcpy(u, a, an * sizeof(Limb));
            u[an] = 0;
        }
        // The top bn limbs of u are below v (the shift left room in the
        // extra limb), and so is every remainder after, so no block has a
        // limb of quotient above it
        size_t first = qn % bn ? qn % bn : bn;
        size_t j = qn - first;
        Limb top = div_bz_rec(q + j, u + j, v, bn, first, Arena(ws.get(), size));
        assert(!top);
        while (j) {
            j -= bn;
            top = div_bz_rec(q + j, u + j, v, bn, bn, Arena(ws.get(), size));
            assert(!top);
        }
        (void)top;
        if (r) {
            if (s) {
                shift_right(r, u, bn, s);
            } else {
                std::memcpy(r, u, bn * sizeof(Limb));
            }
        }
    }

    // The division of the same contract by the road the lengths call for
    inline void divide(Limb* q, Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) {
        size_t t = std::max<size_t>(thresholds.burnikel_ziegler, 4);
        if (bn < t || an - bn + 1 < t) {
            div_knuth(q, r, a, an, b, bn);
        } else {
            div_bz(q, r, a, an, b, bn);
        }
    }
}
