//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "div.h"

#include <bit>
#include <cstdint>
#include <vector>

// The number theory of magnitudes: the greatest common divisor by
// Lehmer's method and the inverse modulo a number by the same method with
// the cofactor carried along (Knuth, TAOCP vol. 2, 4.5.2, algorithm L),
// Montgomery's multiplication (Montgomery, "Modular multiplication
// without trial division", 1985) and the exponentiation over it with
// sliding windows, and the two halves of the Baillie–PSW test: the strong
// probable-prime test of Miller and Rabin and the strong Lucas test with
// Selfridge's parameters (Baillie and Wagstaff, "Lucas pseudoprimes",
// 1980). The algorithms that are sums of whole-number operations — the
// power, the square root, the factorial — are in big_integer.h, written in
// the operators of the class.
namespace sgcl::math::detail {
    // The greatest common divisor of two words, by the binary method
    constexpr Limb gcd_word(Limb a, Limb b) noexcept {
        if (!a) {
            return b;
        }
        if (!b) {
            return a;
        }
        int shift = std::countr_zero(a | b);
        a >>= std::countr_zero(a);
        while (b) {
            b >>= std::countr_zero(b);
            if (a > b) {
                std::swap(a, b);
            }
            b -= a;
        }
        return a << shift;
    }

    // The 64 bits of x[0..n) that start at bit h (zeros past the top)
    inline Limb bits_at(const Limb* x, size_t n, size_t h) noexcept {
        size_t i = h / 64;
        unsigned s = unsigned(h % 64);
        if (i >= n) {
            return 0;
        }
        Limb lo = x[i] >> s;
        if (s && i + 1 < n) {
            lo |= x[i + 1] << (64 - s);
        }
        return lo;
    }

    // The steps of Euclid's algorithm that the top 62 bits of u and v
    // decide on their own, as the matrix (a b; c d) that takes (u, v) to
    // the pair those steps reach: u' = a·u + b·v, v' = c·u + d·v. The
    // simulation stops at the first quotient the two bounds of Knuth's
    // algorithm L do not agree on; b == 0 means no step was certain, and
    // the caller divides.
    struct Cosequence {
        int64_t a;
        int64_t b;
        int64_t c;
        int64_t d;
        size_t steps;
    };

    inline Cosequence lehmer_simulate(Limb uh, Limb vh) noexcept {
        int64_t a = 1;
        int64_t b = 0;
        int64_t c = 0;
        int64_t d = 1;
        auto x = int64_t(uh);
        auto y = int64_t(vh);
        size_t steps = 0;
        for (;;) {
            int64_t yc = y + c;
            int64_t yd = y + d;
            if (yc <= 0 || yd <= 0) {
                break;
            }
            int64_t q = (x + a) / yc;
            if (q != (x + b) / yd) {
                break;
            }
            int64_t t = a - q * c;
            a = c;
            c = t;
            t = b - q * d;
            b = d;
            d = t;
            t = x - q * y;
            x = y;
            y = t;
            ++steps;
        }
        return {a, b, c, d, steps};
    }

    // Both halves of a step of Lehmer's method in one pass over u and v:
    // t = a·u + b·v and w = c·u + d·v, where a and b have opposite signs
    // (or one is zero), and so have c and d, and both results are known
    // not to be negative (u' and v' of the pair the cosequence reached).
    // u and v are read once for the four products; each result is a
    // product added and one subtracted, carried in two chains and a
    // borrow. t and w have room for n limbs, u and v are of n limbs (v
    // with zeros at the top); the normalized lengths into tn and wn.
    inline void lehmer_update(Limb* t, size_t& tn, Limb* w, size_t& wn, const Limb* u, const Limb* v, size_t n, const Cosequence& cs) noexcept {
        // Which of each pair is added: the positive one (or the other
        // when it is zero)
        bool t_adds_u = cs.a > cs.b;
        bool w_adds_u = cs.c > cs.d;
        auto magnitude = [](int64_t x) {
            return x < 0 ? Limb(0) - Limb(x) : Limb(x);
        };
        Limb ta = magnitude(t_adds_u ? cs.a : cs.b);
        Limb ts = magnitude(t_adds_u ? cs.b : cs.a);
        Limb wa = magnitude(w_adds_u ? cs.c : cs.d);
        Limb ws = magnitude(w_adds_u ? cs.d : cs.c);
        Limb tca = 0;
        Limb tcs = 0;
        Limb tb = 0;
        Limb wca = 0;
        Limb wcs = 0;
        Limb wb = 0;
        for (size_t i = 0; i < n; ++i) {
            Limb ui = u[i];
            Limb vi = v[i];
            Wide pa = Wide(ta) * (t_adds_u ? ui : vi) + tca;
            Wide ps = Wide(ts) * (t_adds_u ? vi : ui) + tcs;
            tca = Limb(pa >> 64);
            tcs = Limb(ps >> 64);
            t[i] = sub_borrow_in_register(Limb(pa), Limb(ps), tb);
            Wide qa = Wide(wa) * (w_adds_u ? ui : vi) + wca;
            Wide qs = Wide(ws) * (w_adds_u ? vi : ui) + wcs;
            wca = Limb(qa >> 64);
            wcs = Limb(qs >> 64);
            w[i] = sub_borrow_in_register(Limb(qa), Limb(qs), wb);
        }
        assert(tca - tcs - tb == 0 && wca - wcs - wb == 0);
        tn = normalized(t, n);
        wn = normalized(w, n);
    }

    // The cofactors' half of the same step: st = |a|·su + |b|·sv and
    // sw = |c|·su + |d|·sv in one pass, sums of magnitudes (the terms of
    // each have one sign); su and sv of n limbs, st and sw of n + 1, the
    // normalized lengths into stn and swn
    inline void lehmer_update_sums(Limb* st, size_t& stn, Limb* sw, size_t& swn, const Limb* su, const Limb* sv, size_t n, const Cosequence& cs) noexcept {
        auto magnitude = [](int64_t x) {
            return x < 0 ? Limb(0) - Limb(x) : Limb(x);
        };
        Limb a = magnitude(cs.a);
        Limb b = magnitude(cs.b);
        Limb c = magnitude(cs.c);
        Limb d = magnitude(cs.d);
        Limb tc = 0;
        Limb wc = 0;
        for (size_t i = 0; i < n; ++i) {
            // |a|·x + |b|·y + carry < 2^128: each product is below 2^126
            Wide pt = Wide(a) * su[i] + Wide(b) * sv[i] + tc;
            Wide pw = Wide(c) * su[i] + Wide(d) * sv[i] + wc;
            st[i] = Limb(pt);
            sw[i] = Limb(pw);
            tc = Limb(pt >> 64);
            wc = Limb(pw >> 64);
        }
        st[n] = tc;
        sw[n] = wc;
        stn = normalized(st, n + 1);
        swn = normalized(sw, n + 1);
    }

    // The top 62 bits of u and the bits of v at the same place, for the
    // simulation: a u of 62 bits or fewer is taken whole
    inline void lehmer_tops(const Limb* u, size_t un, const Limb* v, size_t vn, Limb& uh, Limb& vh) noexcept {
        size_t bits = un * 64 - size_t(std::countl_zero(u[un - 1]));
        size_t h = bits > 62 ? bits - 62 : 0;
        uh = bits_at(u, un, h);
        vh = bits_at(v, vn, h);
    }

    // The greatest common divisor of a and b, both normalized and not
    // zero, into r (room for the shorter's length); its length returned
    inline size_t gcd_lehmer(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) {
        size_t n = std::max(an, bn) + 1;
        Scratch s(4 * n);
        Limb* u = s.get();
        Limb* v = u + n;
        Limb* t = v + n;
        Limb* w = t + n;
        if (compare(a, an, b, bn) < 0) {
            std::swap(a, b);
            std::swap(an, bn);
        }
        std::memcpy(u, a, an * sizeof(Limb));
        std::memcpy(v, b, bn * sizeof(Limb));
        size_t un = an;
        size_t vn = bn;
        for (;;) {
            if (!vn) {
                std::memcpy(r, u, un * sizeof(Limb));
                return un;
            }
            if (vn == 1) {
                Limb x = un == 1 ? u[0] % v[0] : mod_1(u, un, Divisor(v[0]));
                r[0] = gcd_word(v[0], x);
                return 1;
            }
            Cosequence cs{1, 0, 0, 1, 0};
            if (un <= vn + 1) {
                Limb uh;
                Limb vh;
                lehmer_tops(u, un, v, vn, uh, vh);
                cs = lehmer_simulate(uh, vh);
            }
            if (!cs.b) {
                // One step of Euclid: u mod v, the quotient into w
                divide(w, t, u, un, v, vn);
                std::swap(u, v);
                std::swap(v, t);
                un = vn;
                vn = normalized(v, vn);
            } else {
                std::memset(v + vn, 0, (un - vn) * sizeof(Limb));
                size_t tn;
                size_t wn;
                lehmer_update(t, tn, w, wn, u, v, un, cs);
                std::swap(u, t);
                std::swap(v, w);
                un = tn;
                vn = wn;
            }
        }
    }

    // The inverse of a modulo m (1 <= a < m, both normalized) into r
    // (room for mn limbs): its length, or 0 when gcd(a, m) is not 1. The
    // same steps as gcd_lehmer on (m, a), with the coefficient s of each
    // remainder (remainder ≡ s·a mod m) carried along. The coefficients of
    // two consecutive remainders have opposite signs, and so have a and b
    // (and c and d) of every cosequence, so each new coefficient is a sum
    // of magnitudes and the sign follows.
    inline size_t inverse_lehmer(Limb* r, const Limb* a, size_t an, const Limb* m, size_t mn) {
        size_t n = mn + 1;
        size_t sn = mn + 2;
        Scratch s(4 * n + 4 * sn + 2 * n + 2);
        Limb* u = s.get();
        Limb* v = u + n;
        Limb* t = v + n;
        Limb* w = t + n;
        Limb* su = w + n;
        Limb* sv = su + sn;
        Limb* st = sv + sn;
        Limb* sw = st + sn;
        Limb* q = sw + sn;                  // a quotient, n limbs
        std::memcpy(u, m, mn * sizeof(Limb));
        std::memcpy(v, a, an * sizeof(Limb));
        size_t un = mn;
        size_t vn = an;
        size_t sun = 0;      // su = 0
        bool su_negative = false;
        sv[0] = 1;
        size_t svn = 1;      // sv = 1
        bool sv_negative = false;
        for (;;) {
            if (!vn) {
                if (un != 1 || u[0] != 1) {
                    return 0;
                }
                if (su_negative) {
                    // m - |su|, |su| < m
                    sub(r, m, mn, su, sun);
                    return normalized(r, mn);
                }
                std::memcpy(r, su, sun * sizeof(Limb));
                return sun;
            }
            Cosequence cs{1, 0, 0, 1, 0};
            if (un <= vn + 1) {
                Limb uh;
                Limb vh;
                lehmer_tops(u, un, v, vn, uh, vh);
                cs = lehmer_simulate(uh, vh);
            }
            if (!cs.b) {
                // Euclid's step with a quotient of any length: t = u - q·v,
                // st = su - q·sv, which is |su| + q·|sv| with su's sign
                size_t qn;
                if (vn == 1) {
                    Limb rem = div_1(q, u, un, Divisor(v[0]));
                    qn = normalized(q, un);
                    t[0] = rem;
                    std::memset(t + 1, 0, (n - 1) * sizeof(Limb));
                } else {
                    divide(q, t, u, un, v, vn);
                    qn = normalized(q, un - vn + 1);
                }
                size_t tn = normalized(t, vn);
                // st = |su| + q·|sv|
                size_t pn = qn + svn;
                Scratch ps(pn + 1);
                Limb* p = ps.get();
                if (qn && svn) {
                    mul(p, q, qn, sv, svn);
                } else {
                    pn = 0;
                }
                pn = normalized(p, pn);
                std::memset(st, 0, sn * sizeof(Limb));
                if (pn >= sun) {
                    std::memcpy(st, p, pn * sizeof(Limb));
                    Limb carry = add_in(st, sn, su, sun);
                    assert(!carry);
                    (void)carry;
                } else {
                    std::memcpy(st, su, sun * sizeof(Limb));
                    Limb carry = add_in(st, sn, p, pn);
                    assert(!carry);
                    (void)carry;
                }
                size_t stn = normalized(st, sn);
                bool st_negative = sun ? su_negative : !sv_negative;
                std::swap(u, v);
                std::swap(v, t);
                un = vn;
                vn = tn;
                std::swap(su, sv);
                std::swap(sv, st);
                su_negative = sv_negative;
                sun = svn;
                sv_negative = st_negative;
                svn = stn;
            } else {
                std::memset(v + vn, 0, (un - vn) * sizeof(Limb));
                size_t tn;
                size_t wn;
                lehmer_update(t, tn, w, wn, u, v, un, cs);
                // The new coefficients: a·su + b·sv and c·su + d·sv, each
                // a sum of magnitudes with the sign of either term
                auto sign_of = [&](int64_t cu, int64_t cv) {
                    if (sun && cu) {
                        return (cu < 0) != su_negative;
                    }
                    return (cv < 0) != sv_negative;
                };
                bool nt = sign_of(cs.a, cs.b);
                bool nw = sign_of(cs.c, cs.d);
                size_t cn = std::max(sun, svn);
                std::memset(su + sun, 0, (cn - sun) * sizeof(Limb));
                std::memset(sv + svn, 0, (cn - svn) * sizeof(Limb));
                size_t stn;
                size_t swn;
                lehmer_update_sums(st, stn, sw, swn, su, sv, cn, cs);
                std::swap(u, t);
                std::swap(v, w);
                un = tn;
                vn = wn;
                std::swap(su, st);
                std::swap(sv, sw);
                sun = stn;
                svn = swn;
                su_negative = stn && nt;
                sv_negative = swn && nw;
            }
        }
    }

    // Arithmetic modulo an odd m of n limbs in Montgomery's form: a
    // residue x stands for x·R mod m (R = 2^(64n)), so that a product needs
    // no division — x·y·R^-1 is the product and a reduction (REDC) of n
    // multiplications of m by a limb. The working memory is the object's
    // own, so a long exponentiation allocates nothing after it is made;
    // an object is used by one thread at a time.
    class Montgomery {
    public:
        Montgomery(const Limb* m, size_t n)
        : _n(n)
        , _m(m, m + n)
        , _t(2 * n + 1)
        , _scratch(mul_scratch(n)) {
            // m^-1 mod 2^64 by Newton's iteration, three bits right to
            // start (m·m ≡ 1 mod 8 for an odd m), twice as many each step
            Limb inverse = m[0];
            for (int i = 0; i < 5; ++i) {
                inverse *= 2 - m[0] * inverse;
            }
            _minus_inverse = Limb(0) - inverse;
            _one.assign(n, 0);
            _minus_one.assign(n, 0);
            // R mod m, and m minus it (which stands for -1)
            to(_one.data(), nullptr, 0, true);
            detail::sub(_minus_one.data(), _m.data(), n, _one.data(), n);
        }

        size_t size() const noexcept {
            return _n;
        }

        const Limb* modulus() const noexcept {
            return _m.data();
        }

        const Limb* one() const noexcept {
            return _one.data();
        }

        const Limb* minus_one() const noexcept {
            return _minus_one.data();
        }

        // r = x·R mod m for any x of xn limbs (r of n limbs)
        void to(Limb* r, const Limb* x, size_t xn) {
            to(r, x, xn, false);
        }

        // r = x·R^-1 mod m: the ordinary number of a residue
        void from(Limb* r, const Limb* x) noexcept {
            std::memcpy(_t.data(), x, _n * sizeof(Limb));
            std::memset(_t.data() + _n, 0, _n * sizeof(Limb));
            reduce(r);
        }

        // r = x·y·R^-1 mod m; r may be x or y
        void mul(Limb* r, const Limb* x, const Limb* y) noexcept {
            detail::mul(_t.data(), x, _n, y, _n, Arena(_scratch.data(), _scratch.size()));
            reduce(r);
        }

        // r = x ± y mod m; r may be x or y
        void add(Limb* r, const Limb* x, const Limb* y) noexcept {
            Limb carry = add_n(r, x, y, _n);
            if (carry || compare_n(r, _m.data(), _n) >= 0) {
                sub_n(r, r, _m.data(), _n);
            }
        }

        void sub(Limb* r, const Limb* x, const Limb* y) noexcept {
            if (sub_n(r, x, y, _n)) {
                add_n(r, r, _m.data(), _n);
            }
        }

        // r = x / 2 mod m: an odd x has m added first (m is odd), and the
        // halving holds for residues as for numbers
        void half(Limb* r, const Limb* x) noexcept {
            Limb top = 0;
            if (x[0] & 1) {
                top = add_n(r, x, _m.data(), _n);
            } else if (r != x) {
                std::memcpy(r, x, _n * sizeof(Limb));
            }
            for (size_t i = 0; i < _n; ++i) {
                r[i] = (r[i] >> 1) | (i + 1 < _n ? r[i + 1] << 63 : top << 63);
            }
        }

        bool equal(const Limb* x, const Limb* y) const noexcept {
            return !compare_n(x, y, _n);
        }

        bool is_zero(const Limb* x) const noexcept {
            return !normalized(x, _n);
        }

    private:
        // (x·β^n) mod m, or R mod m itself
        void to(Limb* r, const Limb* x, size_t xn, bool unity) {
            size_t an = unity ? _n + 1 : _n + xn;
            Scratch as(an);
            Limb* a = as.get();
            std::memset(a, 0, _n * sizeof(Limb));
            if (unity) {
                a[_n] = 1;
            } else {
                std::memcpy(a + _n, x, xn * sizeof(Limb));
            }
            an = normalized(a, an);
            if (compare(a, an, _m.data(), _n) < 0) {
                std::memcpy(r, a, an * sizeof(Limb));
                std::memset(r + an, 0, (_n - an) * sizeof(Limb));
                return;
            }
            if (_n == 1) {
                Scratch qs(an);
                r[0] = div_1(qs.get(), a, an, Divisor(_m[0]));
                return;
            }
            Scratch qs(an - _n + 1);
            divide(qs.get(), r, a, an, _m.data(), _n);
        }

        // r = t·R^-1 mod m for the 2n limbs of t below m·R: each row adds
        // the multiple of m that clears the lowest limb, whose place then
        // keeps the row's carry for the sum at the end
        void reduce(Limb* r) noexcept {
            Limb* t = _t.data();
            const Limb* m = _m.data();
            for (size_t i = 0; i < _n; ++i) {
                Limb u = t[i] * _minus_inverse;
                t[i] = mul_add_1(t + i, m, _n, u);
            }
            Limb carry = add_n(r, t + _n, t, _n);
            if (carry || compare_n(r, m, _n) >= 0) {
                sub_n(r, r, m, _n);
            }
        }

        size_t _n;
        std::vector<Limb> _m;
        std::vector<Limb> _t;
        std::vector<Limb> _scratch;
        std::vector<Limb> _one;
        std::vector<Limb> _minus_one;
        Limb _minus_inverse = 0;
    };

    // r = x^e for a residue x (Montgomery's form) and an exponent e[0..en)
    // not zero, by sliding windows: the odd powers x, x^3, … x^(2^k - 1)
    // made once, then per window of the exponent's bits k squares and one
    // product, a run of zeros costing a square a bit
    inline void mod_pow_windows(Montgomery& mg, Limb* r, const Limb* x, const Limb* e, size_t en) {
        size_t n = mg.size();
        size_t bits = en * 64 - size_t(std::countl_zero(e[en - 1]));
        unsigned k = bits < 24 ? 1 : bits < 96 ? 3 : bits < 384 ? 4 : bits < 1536 ? 5 : 6;
        size_t count = size_t(1) << (k - 1);
        std::vector<Limb> table(count * n);
        std::memcpy(table.data(), x, n * sizeof(Limb));
        if (count > 1) {
            std::vector<Limb> square(n);
            mg.mul(square.data(), x, x);
            for (size_t i = 1; i < count; ++i) {
                mg.mul(table.data() + i * n, table.data() + (i - 1) * n, square.data());
            }
        }
        auto bit = [&](size_t i) {
            return (e[i / 64] >> (i % 64)) & 1;
        };
        bool started = false;
        size_t i = bits;
        while (i-- > 0) {
            if (!bit(i)) {
                if (started) {
                    mg.mul(r, r, r);
                }
                continue;
            }
            // The longest window from bit i down, at most k bits, ending in
            // a one
            size_t low = i + 1 > k ? i + 1 - k : 0;
            while (!bit(low)) {
                ++low;
            }
            Limb value = 0;
            for (size_t j = i + 1; j-- > low;) {
                value = value << 1 | bit(j);
            }
            if (started) {
                for (size_t j = low; j <= i; ++j) {
                    mg.mul(r, r, r);
                }
                mg.mul(r, r, table.data() + (value >> 1) * n);
            } else {
                std::memcpy(r, table.data() + (value >> 1) * n, n * sizeof(Limb));
                started = true;
            }
            i = low;
        }
    }

    // The strong probable-prime test to a base b (a residue in Montgomery's
    // form) of the odd n = d·2^s + 1 the Montgomery object is for: b^d is 1
    // or -1, or one of its squarings before the s-th is -1
    inline bool strong_probable_prime(Montgomery& mg, const Limb* base, const Limb* d, size_t dn, size_t s) {
        size_t n = mg.size();
        std::vector<Limb> x(n);
        mod_pow_windows(mg, x.data(), base, d, dn);
        if (mg.equal(x.data(), mg.one()) || mg.equal(x.data(), mg.minus_one())) {
            return true;
        }
        for (size_t i = 1; i < s; ++i) {
            mg.mul(x.data(), x.data(), x.data());
            if (mg.equal(x.data(), mg.minus_one())) {
                return true;
            }
            if (mg.equal(x.data(), mg.one())) {
                return false;
            }
        }
        return false;
    }

    // The Jacobi symbol (a/b) of two words, b odd
    constexpr int jacobi_word(Limb a, Limb b) noexcept {
        int result = 1;
        a %= b;
        while (a) {
            int z = std::countr_zero(a);
            a >>= z;
            if ((z & 1) && (b % 8 == 3 || b % 8 == 5)) {
                result = -result;
            }
            // (a/b) = (b/a), but for a and b both 3 modulo 4
            if (a % 4 == 3 && b % 4 == 3) {
                result = -result;
            }
            Limb t = b % a;
            b = a;
            a = t;
        }
        return b == 1 ? result : 0;
    }

    // (D/n) for a small odd D of either sign and an odd n of many limbs:
    // (-1/n) by n modulo 4, and (|D|/n) by reciprocity from n mod |D|
    inline int jacobi_small(int64_t d, const Limb* n, size_t nn) noexcept {
        int result = 1;
        Limb ad = d < 0 ? Limb(0) - Limb(d) : Limb(d);
        if (d < 0 && n[0] % 4 == 3) {
            result = -result;
        }
        if (ad == 1) {
            return result;
        }
        Limb rem = nn == 1 ? n[0] % ad : mod_1(n, nn, Divisor(ad));
        if (ad % 4 == 3 && n[0] % 4 == 3) {
            result = -result;
        }
        return result * jacobi_word(rem, ad);
    }

    // The strong Lucas probable-prime test of an odd n that is not a
    // square, with Selfridge's parameters: D the first of 5, -7, 9, -11, …
    // with (D/n) = -1, P = 1, Q = (1 - D)/4. With n + 1 = d·2^s, n passes
    // when U_d ≡ 0 or V_(d·2^r) ≡ 0 for some r < s. The sequences are
    // walked over the bits of d by doubling — U_2k = U_k·V_k, V_2k =
    // V_k² - 2Q^k — and a step up — U_(k+1) = (U_k + V_k)/2, V_(k+1) =
    // (D·U_k + V_k)/2 — all in Montgomery's form. A D with (D/n) = 0 is a
    // factor, and n is not prime unless it is |D|.
    inline bool strong_lucas_probable_prime(Montgomery& mg, const Limb* np, size_t nn) {
        int64_t d = 5;
        for (;; d = d > 0 ? -(d + 2) : -d + 2) {
            int j = jacobi_small(d, np, nn);
            if (j == -1) {
                break;
            }
            if (j == 0) {
                Limb ad = d < 0 ? Limb(-d) : Limb(d);
                return nn == 1 && np[0] == ad;
            }
        }
        int64_t q = (1 - d) / 4;
        size_t n = mg.size();
        // n + 1 = k·2^s
        std::vector<Limb> k(nn + 1);
        std::memcpy(k.data(), np, nn * sizeof(Limb));
        k[nn] = add_1_in(k.data(), nn, 1);
        size_t kn = normalized(k.data(), nn + 1);
        size_t s = trailing_zeros(k.data(), kn);
        {
            size_t words = s / 64;
            unsigned bits = unsigned(s % 64);
            std::memmove(k.data(), k.data() + words, (kn - words) * sizeof(Limb));
            kn -= words;
            if (bits) {
                shift_right(k.data(), k.data(), kn, bits);
            }
            kn = normalized(k.data(), kn);
        }
        auto residue = [&](int64_t v, Limb* out) {
            Limb m = v < 0 ? Limb(0) - Limb(v) : Limb(v);
            mg.to(out, &m, 1);
            if (v < 0) {
                std::vector<Limb> zero(n, 0);
                mg.sub(out, zero.data(), out);
            }
        };
        std::vector<Limb> dm(n), qm(n), u(n), v(n), qk(n), t(n), w(n);
        residue(d, dm.data());
        residue(q, qm.data());
        // k = 1: U_1 = 1, V_1 = P = 1, Q^1 = Q
        std::memcpy(u.data(), mg.one(), n * sizeof(Limb));
        std::memcpy(v.data(), mg.one(), n * sizeof(Limb));
        std::memcpy(qk.data(), qm.data(), n * sizeof(Limb));
        size_t bits = kn * 64 - size_t(std::countl_zero(k[kn - 1]));
        for (size_t i = bits - 1; i-- > 0;) {
            // Doubling
            mg.mul(u.data(), u.data(), v.data());
            mg.mul(v.data(), v.data(), v.data());
            mg.add(t.data(), qk.data(), qk.data());
            mg.sub(v.data(), v.data(), t.data());
            mg.mul(qk.data(), qk.data(), qk.data());
            if ((k[i / 64] >> (i % 64)) & 1) {
                // A step up: U' = (U + V)/2, V' = (D·U + V)/2
                mg.add(t.data(), u.data(), v.data());
                mg.mul(w.data(), dm.data(), u.data());
                mg.add(w.data(), w.data(), v.data());
                mg.half(u.data(), t.data());
                mg.half(v.data(), w.data());
                mg.mul(qk.data(), qk.data(), qm.data());
            }
        }
        if (mg.is_zero(u.data()) || mg.is_zero(v.data())) {
            return true;
        }
        for (size_t r = 1; r < s; ++r) {
            // V_2k = V_k² - 2Q^k
            mg.mul(v.data(), v.data(), v.data());
            mg.add(t.data(), qk.data(), qk.data());
            mg.sub(v.data(), v.data(), t.data());
            if (mg.is_zero(v.data())) {
                return true;
            }
            mg.mul(qk.data(), qk.data(), qk.data());
        }
        return false;
    }
}
