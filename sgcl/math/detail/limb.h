//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

// The arithmetic of one limb, a word of 64 bits, that C++ does not give:
// the full product of two words, the sum and the difference with a carry
// in and out, and the division of two words by one. Everything above
// (nat.h, div.h, convert.h) is written in these, so the platform's
// wide multiply is named here once. Only 64-bit targets with unsigned
// __int128 (clang, gcc) are supported; the module says so in big_integer.h.
namespace sgcl::math::detail {
    using Limb = uint64_t;
    using Wide = unsigned __int128;

    static_assert(sizeof(void*) == 8, "sgcl::math needs a 64-bit target");

    constexpr unsigned LimbBits = 64;

    // hi:lo = a * b
    constexpr Limb mul_wide(Limb a, Limb b, Limb& lo) noexcept {
        Wide p = Wide(a) * b;
        lo = Limb(p);
        return Limb(p >> 64);
    }

    // a + b + carry, the carry out in `carry` (0 or 1). By the compiler's
    // builtin where it has one: a run of these in a row is then one chain
    // of add-with-carry instructions, the carry in the flags, where the
    // 128-bit sum takes the carry out into a register and back each time
    // (a loop over 100 limbs 100 ns against 46, the loops of nat.h)
    constexpr Limb add_carry(Limb a, Limb b, Limb& carry) noexcept {
#if __has_builtin(__builtin_addcll)
        unsigned long long out;
        Limb s = __builtin_addcll(a, b, carry, &out);
        carry = out;
        return s;
#else
        Wide s = Wide(a) + b + carry;
        carry = Limb(s >> 64);
        return Limb(s);
#endif
    }

    // a - b - borrow in plain arithmetic, the borrow out in a register:
    // for a loop that carries two borrows at once (lehmer_update), which
    // the one set of flags cannot hold together, and where the builtin
    // below measured 5 to 8 percent slower
    constexpr Limb sub_borrow_in_register(Limb a, Limb b, Limb& borrow) noexcept {
        Limb d = a - b;
        Limb out1 = a < b;
        Limb r = d - borrow;
        Limb out2 = d < borrow;
        borrow = out1 | out2;
        return r;
    }

    // a - b - borrow, the borrow out in `borrow` (0 or 1)
    constexpr Limb sub_borrow(Limb a, Limb b, Limb& borrow) noexcept {
#if __has_builtin(__builtin_subcll)
        unsigned long long out;
        Limb d = __builtin_subcll(a, b, borrow, &out);
        borrow = out;
        return d;
#else
        Limb d = a - b;
        Limb out1 = a < b;
        Limb r = d - borrow;
        Limb out2 = d < borrow;
        borrow = out1 | out2;
        return r;
#endif
    }

    // A divisor of one limb made ready for many divisions of two limbs by
    // it: shifted until its top bit is set, and its reciprocal
    // floor((2^128 - 1) / d) - 2^64 computed once, so that each division
    // after is two multiplications and a correction — the method of
    // Möller and Granlund, "Improved division by invariant integers"
    // (2011), algorithm 4. The one real division is here, in the
    // constructor; the compiler's own of 128 by 64 bits is a call into
    // the runtime that costs tens of nanoseconds, which a loop over the
    // limbs of a long number cannot afford for every limb.
    struct Divisor {
        Limb d;         // normalized: the top bit set
        Limb inverse;
        unsigned shift; // how far the divisor given was shifted left

        constexpr explicit Divisor(Limb divisor) noexcept
        : d(divisor << std::countl_zero(divisor))
        , inverse(Limb(((Wide(~(divisor << std::countl_zero(divisor))) << 64) | ~Limb(0)) / (divisor << std::countl_zero(divisor))))
        , shift(unsigned(std::countl_zero(divisor))) {
        }

        // (u1:u0) / d for a normalized d and u1 < d: the quotient, and
        // the remainder in `r`
        constexpr Limb divide(Limb u1, Limb u0, Limb& r) const noexcept {
            Wide q = Wide(inverse) * u1;
            q += (Wide(u1 + 1) << 64) | u0;
            Limb q1 = Limb(q >> 64);
            Limb q0 = Limb(q);
            Limb rem = u0 - q1 * d;
            if (rem > q0) {
                --q1;
                rem += d;
            }
            if (rem >= d) [[unlikely]] {
                ++q1;
                rem -= d;
            }
            r = rem;
            return q1;
        }
    };
}
