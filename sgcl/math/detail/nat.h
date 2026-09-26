//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "limb.h"

#include <cstring>
#include <memory>

// The algorithms on magnitudes: runs of limbs, least significant first,
// given as a pointer and a length, with no sign and no ownership. A
// big_integer (big_integer.h) hands its limbs here as raw pointers for the
// length of one operation, which is safe because the operands are held
// by the caller for that whole time, and writes the result into a
// buffer it has just allocated and nobody else sees yet. "Normalized"
// means the top limb is not zero; a length of zero is the number zero.
// Nothing here allocates on the managed heap, and nothing throws.
namespace sgcl::math::detail {
    // The length once the zero limbs at the top are dropped
    inline size_t normalized(const Limb* a, size_t n) noexcept {
        while (n && !a[n - 1]) {
            --n;
        }
        return n;
    }

    // -1, 0, 1 as |a| is below, equal to or above |b|; both normalized
    inline int compare(const Limb* a, size_t an, const Limb* b, size_t bn) noexcept {
        if (an != bn) {
            return an < bn ? -1 : 1;
        }
        for (size_t i = an; i-- > 0;) {
            if (a[i] != b[i]) {
                return a[i] < b[i] ? -1 : 1;
            }
        }
        return 0;
    }

    // r = a + b for an >= bn; r has room for an + 1 limbs, which are all
    // written (the last is the carry). r may be a or b.
    inline Limb add_n(Limb* r, const Limb* a, const Limb* b, size_t n) noexcept;
    inline Limb sub_n(Limb* r, const Limb* a, const Limb* b, size_t n) noexcept;

    inline void add(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) noexcept {
        Limb carry = add_n(r, a, b, bn);
        size_t i = bn;
        for (; i < an; ++i) {
            Limb s = a[i] + carry;
            carry = s < carry;
            r[i] = s;
        }
        r[an] = carry;
    }

    // r = a - b for |a| >= |b| (an >= bn); r has room for an limbs. r
    // may be a or b.
    inline void sub(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) noexcept {
        Limb borrow = sub_n(r, a, b, bn);
        size_t i = bn;
        for (; i < an; ++i) {
            Limb d = a[i] - borrow;
            borrow = a[i] < borrow;
            r[i] = d;
        }
    }

    // r = a + 1; r has room for n + 1 limbs, all written
    inline void add_one(Limb* r, const Limb* a, size_t n) noexcept {
        Limb carry = 1;
        for (size_t i = 0; i < n; ++i) {
            Limb s = a[i] + carry;
            carry = s < carry;
            r[i] = s;
        }
        r[n] = carry;
    }

    // r = a - 1 for a nonzero a; r has room for n limbs
    inline void sub_one(Limb* r, const Limb* a, size_t n) noexcept {
        Limb borrow = 1;
        for (size_t i = 0; i < n; ++i) {
            Limb d = a[i] - borrow;
            borrow = a[i] < borrow;
            r[i] = d;
        }
    }

    // The loops of a number by one limb go eight limbs a step: the eight
    // products first, each on its own, then the sums in one chain of
    // add-with-carry (two for mul_add_1 and mul_sub_1, one taking the
    // products' high halves in, one adding into r), the carries folded into
    // one limb at the end of the step. A limb at a time, every sum waits
    // for the carry of the one before it through the multiplication: over
    // 100 limbs mul_1 58 -> 32 ns, mul_add_1 95 -> 48. The fold never
    // overflows: r + a * m + carry < 2^128 for limbs below 2^64.
    constexpr size_t LimbStep = 8;

    // r = a * m, the limb carried out returned; r may be a
    inline Limb mul_1(Limb* r, const Limb* a, size_t n, Limb m) noexcept {
        Limb carry = 0;
        size_t i = 0;
        for (; i + LimbStep <= n; i += LimbStep) {
            Limb lo[LimbStep];
            Limb hi[LimbStep];
            for (size_t k = 0; k < LimbStep; ++k) {
                hi[k] = mul_wide(a[i + k], m, lo[k]);
            }
            Limb c = 0;
            for (size_t k = 0; k < LimbStep; ++k) {
                r[i + k] = add_carry(lo[k], carry, c);
                carry = hi[k];
            }
            carry += c;
        }
        for (; i < n; ++i) {
            Limb lo;
            Limb hi = mul_wide(a[i], m, lo);
            lo += carry;
            carry = hi + (lo < carry);
            r[i] = lo;
        }
        return carry;
    }

    // r += a * m over n limbs of r, the limb carried out returned
    inline Limb mul_add_1(Limb* r, const Limb* a, size_t n, Limb m) noexcept {
        Limb carry = 0;
        size_t i = 0;
        for (; i + LimbStep <= n; i += LimbStep) {
            Limb lo[LimbStep];
            Limb hi[LimbStep];
            for (size_t k = 0; k < LimbStep; ++k) {
                hi[k] = mul_wide(a[i + k], m, lo[k]);
            }
            Limb ca = 0;
            Limb cb = 0;
            for (size_t k = 0; k < LimbStep; ++k) {
                Limb p = add_carry(lo[k], carry, ca);
                carry = hi[k];
                r[i + k] = add_carry(r[i + k], p, cb);
            }
            carry += ca + cb;
        }
        for (; i < n; ++i) {
            Wide p = Wide(a[i]) * m + r[i] + carry;
            r[i] = Limb(p);
            carry = Limb(p >> 64);
        }
        return carry;
    }

    // r -= a * m over n limbs of r, the limb borrowed out returned
    inline Limb mul_sub_1(Limb* r, const Limb* a, size_t n, Limb m) noexcept {
        Limb borrow = 0;
        size_t i = 0;
        for (; i + LimbStep <= n; i += LimbStep) {
            Limb lo[LimbStep];
            Limb hi[LimbStep];
            for (size_t k = 0; k < LimbStep; ++k) {
                hi[k] = mul_wide(a[i + k], m, lo[k]);
            }
            Limb ca = 0;
            Limb cb = 0;
            for (size_t k = 0; k < LimbStep; ++k) {
                Limb p = add_carry(lo[k], borrow, ca);
                borrow = hi[k];
                r[i + k] = sub_borrow(r[i + k], p, cb);
            }
            borrow += ca + cb;
        }
        for (; i < n; ++i) {
            Limb lo;
            Limb hi = mul_wide(a[i], m, lo);
            lo += borrow;
            hi += lo < borrow;
            Limb d = r[i] - lo;
            hi += d > r[i];
            r[i] = d;
            borrow = hi;
        }
        return borrow;
    }

    // r = a * b, the schoolbook way; r has room for an + bn limbs, all
    // written, and is neither a nor b. The fast multiplications (mul.h)
    // come down to this below their thresholds.
    inline void mul_basecase(Limb* r, const Limb* a, size_t an, const Limb* b, size_t bn) noexcept {
        if (an < bn) {
            std::swap(a, b);
            std::swap(an, bn);
        }
        if (!bn) {
            std::memset(r, 0, an * sizeof(Limb));
            return;
        }
        r[an] = mul_1(r, a, an, b[0]);
        for (size_t j = 1; j < bn; ++j) {
            r[an + j] = mul_add_1(r + j, a, an, b[j]);
        }
    }

    // r = a * a, the schoolbook way but each product of two different
    // limbs taken once and doubled: about half the multiplications of
    // mul_basecase. r has room for 2n limbs, all written, and is not a.
    inline void sqr_basecase(Limb* r, const Limb* a, size_t n) noexcept {
        if (!n) {
            return;
        }
        if (n == 1) {
            r[1] = mul_wide(a[0], a[0], r[0]);
            return;
        }
        // The products below the diagonal, a[i]·a[j] for i < j, row by row:
        // row i starts at limb 2i + 1 and carries out into limb n + i
        r[0] = 0;
        r[n] = mul_1(r + 1, a + 1, n - 1, a[0]);
        for (size_t i = 1; i + 1 < n; ++i) {
            r[n + i] = mul_add_1(r + 2 * i + 1, a + i + 1, n - i - 1, a[i]);
        }
        r[2 * n - 1] = 0;
        // Twice that (it is below a²/2, so nothing leaves the top), and the
        // squares of the diagonal
        Limb top = 0;
        Limb carry = 0;
        for (size_t i = 0; i < n; ++i) {
            Limb lo;
            Limb hi = mul_wide(a[i], a[i], lo);
            Limb x0 = r[2 * i];
            Limb x1 = r[2 * i + 1];
            Limb d0 = (x0 << 1) | top;
            Limb d1 = (x1 << 1) | (x0 >> 63);
            top = x1 >> 63;
            r[2 * i] = add_carry(d0, lo, carry);
            r[2 * i + 1] = add_carry(d1, hi, carry);
        }
    }

    // r[0..n) = a + b over n limbs each, the carry out returned; r may be
    // a or b. Nothing is written past n.
    // Sixteen limbs a step, then eight: the carry stays in the flags
    // through the step and goes into a register once per step, not once
    // per limb (a loop over 100 limbs 63 ns one at a time, 46 eight at a
    // time, 41 sixteen at a time)
    inline Limb add_n(Limb* r, const Limb* a, const Limb* b, size_t n) noexcept {
        Limb carry = 0;
        size_t i = 0;
        for (; i + 16 <= n; i += 16) {
            for (size_t k = 0; k < 16; ++k) {
                r[i + k] = add_carry(a[i + k], b[i + k], carry);
            }
        }
        for (; i + 8 <= n; i += 8) {
            for (size_t k = 0; k < 8; ++k) {
                r[i + k] = add_carry(a[i + k], b[i + k], carry);
            }
        }
        for (; i < n; ++i) {
            r[i] = add_carry(a[i], b[i], carry);
        }
        return carry;
    }

    // r[0..n) = a - b over n limbs each, the borrow out returned; r may
    // be a or b
    inline Limb sub_n(Limb* r, const Limb* a, const Limb* b, size_t n) noexcept {
        Limb borrow = 0;
        size_t i = 0;
        for (; i + 16 <= n; i += 16) {
            for (size_t k = 0; k < 16; ++k) {
                r[i + k] = sub_borrow(a[i + k], b[i + k], borrow);
            }
        }
        for (; i + 8 <= n; i += 8) {
            for (size_t k = 0; k < 8; ++k) {
                r[i + k] = sub_borrow(a[i + k], b[i + k], borrow);
            }
        }
        for (; i < n; ++i) {
            r[i] = sub_borrow(a[i], b[i], borrow);
        }
        return borrow;
    }

    // r[0..rn) += x, the carry out of limb rn - 1 returned
    inline Limb add_1_in(Limb* r, size_t rn, Limb x) noexcept {
        for (size_t i = 0; x && i < rn; ++i) {
            Limb s = r[i] + x;
            x = s < x;
            r[i] = s;
        }
        return x;
    }

    // r[0..rn) -= x, the borrow out returned
    inline Limb sub_1_in(Limb* r, size_t rn, Limb x) noexcept {
        for (size_t i = 0; x && i < rn; ++i) {
            Limb d = r[i] - x;
            x = r[i] < x;
            r[i] = d;
        }
        return x;
    }

    // r[0..rn) += a[0..an) for an <= rn, the carry going on through r; the
    // carry out of the top returned
    inline Limb add_in(Limb* r, size_t rn, const Limb* a, size_t an) noexcept {
        Limb carry = add_n(r, r, a, an);
        return add_1_in(r + an, rn - an, carry);
    }

    // r[0..rn) -= a[0..an) for an <= rn, the borrow going on through r;
    // the borrow out of the top returned
    inline Limb sub_in(Limb* r, size_t rn, const Limb* a, size_t an) noexcept {
        Limb borrow = sub_n(r, r, a, an);
        return sub_1_in(r + an, rn - an, borrow);
    }

    // -1, 0, 1 as a[0..n) is below, equal to or above b[0..n), both of n
    // limbs and either with zeros at the top
    inline int compare_n(const Limb* a, const Limb* b, size_t n) noexcept {
        for (size_t i = n; i-- > 0;) {
            if (a[i] != b[i]) {
                return a[i] < b[i] ? -1 : 1;
            }
        }
        return 0;
    }

    // r = a << s for 0 < s < 64, the bits shifted out of the top returned;
    // r may be a
    inline Limb shift_left(Limb* r, const Limb* a, size_t n, unsigned s) noexcept {
        Limb out = 0;
        for (size_t i = n; i-- > 0;) {
            Limb x = a[i];
            if (i + 1 == n) {
                out = x >> (64 - s);
            }
            r[i] = (x << s) | (i ? a[i - 1] >> (64 - s) : 0);
        }
        return out;
    }

    // r = a >> s for 0 < s < 64; r may be a
    inline void shift_right(Limb* r, const Limb* a, size_t n, unsigned s) noexcept {
        for (size_t i = 0; i < n; ++i) {
            r[i] = (a[i] >> s) | (i + 1 < n ? a[i + 1] << (64 - s) : 0);
        }
    }

    // q = a / d for a single limb d != 0, the remainder returned; q may
    // be a. The dividend is shifted by the divisor's normalization as it
    // is read, one limb at a time, so neither is copied.
    inline Limb div_1(Limb* q, const Limb* a, size_t n, const Divisor& dv) noexcept {
        if (!n) {
            return 0;
        }
        unsigned s = dv.shift;
        Limb r;
        if (!s) {
            r = 0;
            for (size_t i = n; i-- > 0;) {
                q[i] = dv.divide(r, a[i], r);
            }
            return r;
        }
        r = a[n - 1] >> (64 - s);
        for (size_t i = n; i-- > 0;) {
            Limb u0 = (a[i] << s) | (i ? a[i - 1] >> (64 - s) : 0);
            q[i] = dv.divide(r, u0, r);
        }
        return r >> s;
    }

    // The remainder of a / d alone
    inline Limb mod_1(const Limb* a, size_t n, const Divisor& dv) noexcept {
        if (!n) {
            return 0;
        }
        unsigned s = dv.shift;
        Limb r = s ? a[n - 1] >> (64 - s) : 0;
        for (size_t i = n; i-- > 0;) {
            Limb u0 = s ? (a[i] << s) | (i ? a[i - 1] >> (64 - s) : 0) : a[i];
            dv.divide(r, u0, r);
        }
        return r >> s;
    }

    // How many zero bits are below the lowest one; n must be above zero
    // and the number nonzero
    inline size_t trailing_zeros(const Limb* a, size_t n) noexcept {
        size_t i = 0;
        while (i + 1 < n && !a[i]) {
            ++i;
        }
        return i * 64 + size_t(std::countr_zero(a[i]));
    }

    // Working memory that holds no pointer: on the stack while it is
    // small, from the ordinary heap past that. The collector never sees
    // it and has no reason to — limbs are numbers — and it dies with the
    // operation that wanted it, where a managed block would live on
    // until a sweep.
    class Scratch {
    public:
        explicit Scratch(size_t n)
        : _p(n <= Inline ? _inline : (_heap = std::make_unique_for_overwrite<Limb[]>(n)).get()) {
        }

        Scratch(const Scratch&) = delete;
        Scratch& operator=(const Scratch&) = delete;

        Limb* get() noexcept {
            return _p;
        }

    private:
        static constexpr size_t Inline = 64;
        Limb _inline[Inline];
        std::unique_ptr<Limb[]> _heap;
        Limb* _p;
    };
}
