#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The oracle for sgcl/math/big_integer.h: Python's int, which is exact and
# has no limit once the limit on digits is lifted, asked the questions
# tests/math/big_integer.cpp asks, its answers written out as a C++ header
# the test includes. Run it from the root of the tree:
#
#     python3 tools/math_vectors.py > tests/math/vectors.h
#
# An oracle checks only the cases it is given, so they are named here one
# by one rather than drawn at random and hoped for:
#
#   - the edges of the small representation: INT64_MIN, INT64_MAX, one
#     either side of each, -INT64_MIN (the first value in limbs),
#     UINT64_MAX and its negation;
#   - powers of the limb: 2^64k and 2^64k +- 1, limbs of all ones, zeros
#     in the middle of a number;
#   - zero, one, minus one, small numbers of both signs;
#   - every operation over every pair of those, so each sign combination
#     and each length combination is there;
#   - random operands of 1 to 40 limbs with fixed seeds, lengths chosen
#     so that 1 x n, n/2 x n and n x n all occur, with limbs of all ones
#     and a top limb of one mixed in;
#   - Knuth's division: divisors that need normalizing and ones that do
#     not, dividends shorter than the divisor, and the add-back step
#     (D6), which random data reaches about twice in 2^64 quotient limbs —
#     every case listed under add_back is checked by a model of
#     algorithm D below to take that step at least once;
#   - shifts by 0, 1, 63, 64, 65, 127, 128, 129, 200 and 1000, both ways,
#     negative values rounding down;
#   - the bits of negative numbers against Python's own two's complement;
#   - to_double: 2^53 +- 1 and the ties around it, ties and near-ties
#     above 2^64 decided by a bit in a lower limb, with and without the top
#     limb's top bit set, the largest finite double and the first value
#     past it, and their negations; Python's float(int) rounds half to
#     even, as the conversion here must;
#   - from a double: zeros of both signs, halves, 2^52 + 0.5, 2^63 and
#     -2^63 (the edge of the small representation), 2^1023, the largest
#     double, a subnormal;
#   - text in every base from 2 to 36, both ways.
import math
import random
import sys

sys.set_int_max_str_digits(0)

B = 1 << 64
I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1


def hx(x):
    return ("-" if x < 0 else "") + format(abs(x), "x")


def lit(s):
    return '"' + s + '"'


def cdouble(d):
    if math.isinf(d):
        return "-Inf" if d < 0 else "Inf"
    return float.hex(d)


def to_double(x):
    try:
        return float(x)
    except OverflowError:
        return -math.inf if x < 0 else math.inf


def edges():
    v = [
        0, 1, -1, 2, -2, 3, 7, -7, 10, -10, 1000000007,
        I64_MAX, I64_MAX - 1, I64_MIN, I64_MIN + 1, I64_MIN - 1, I64_MAX + 1, I64_MAX + 2,
        B - 1, -(B - 1), B, -B, B + 1, B - 2,
        B ** 2, B ** 2 - 1, B ** 2 + 1, -(B ** 2),
        B ** 3 - 1, B ** 2 + 1 + B ** 3,           # all ones; zeros in the middle
        (B - 1) * B ** 2 + (B - 1),                  # ones, a zero limb, ones
        10 ** 19, 10 ** 20, -(10 ** 38),
        0x0123456789abcdef, -0x7edcba9876543210fedcba98,
    ]
    return v


def random_operands(rng):
    out = []
    lengths = [1, 2, 3, 4, 5, 8, 13, 20, 40]
    for n in lengths:
        for kind in range(3):
            if kind == 0:
                limbs = [rng.getrandbits(64) for _ in range(n)]
            elif kind == 1:
                limbs = [B - 1] * n
            else:
                limbs = [rng.getrandbits(64) for _ in range(n - 1)] + [1]
            x = sum(l << (64 * i) for i, l in enumerate(limbs))
            if rng.random() < 0.5:
                x = -x
            out.append(x)
    return out


def trunc_div(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def trunc_rem(a, b):
    return a - trunc_div(a, b) * b


# A model of Knuth's algorithm D in base 2^64, the way div.h does it: how
# many times the add-back step is taken for a / b
def knuth_add_backs(a, b):
    n = (abs(b).bit_length() + 63) // 64
    if n < 2 or abs(a) < abs(b):
        return 0
    m = (abs(a).bit_length() + 63) // 64 - n
    s = 64 * n - abs(b).bit_length()
    V = abs(b) << s
    U = abs(a) << s
    vl = [(V >> (64 * i)) & (B - 1) for i in range(n)]
    ul = [(U >> (64 * i)) & (B - 1) for i in range(m + n + 1)]
    adds = 0
    for j in range(m, -1, -1):
        num = ul[j + n] * B + ul[j + n - 1]
        qhat, rhat = divmod(num, vl[-1])
        while qhat >= B or qhat * vl[-2] > B * rhat + ul[j + n - 2]:
            qhat -= 1
            rhat += vl[-1]
            if rhat >= B:
                break
        part = sum(ul[j + i] << (64 * i) for i in range(n + 1)) - qhat * V
        if part < 0:
            adds += 1
            part += V
        for i in range(n + 1):
            ul[j + i] = (part >> (64 * i)) & (B - 1)
    return adds


def add_back_cases():
    cases = []
    top = [1 << 63, (1 << 63) + 1, B - 1, 1 << 62, 3]
    for t in top:
        for v0 in [1, 2]:
            for u in [(0, 0, 0, 1), (0, 0, 1 << 63, (1 << 63) - 1), (0, 1, 0, B - 1), (B - 1, 0, 0, 2), (0, 0, 0, 0, 1)]:
                b = v0 + t * B ** 2
                a = sum(l << (64 * i) for i, l in enumerate(u))
                if knuth_add_backs(a, b):
                    cases.append((a, b))
    # Longer: a divisor of a one, zeros and a top limb of 2^63, whose
    # second limb is zero so that the test of D3 cannot see the one at the
    # bottom; B^n over it estimates 2 where the quotient is 1
    for n in [3, 5, 9, 17]:
        b = 1 + (1 << 63) * B ** (n - 1)
        cases.append((B ** n, b))
        cases.append((5 * B ** (n + 2), b))
        # And one that needs a shift of normalization first
        c = 1 + (1 << 62) * B ** (n - 1)
        for a in [B ** n >> 1, B ** n, 3 * B ** (n + 1) >> 1]:
            if knuth_add_backs(a, c):
                cases.append((a, c))
    for a, b in cases:
        assert knuth_add_backs(a, b) > 0
    assert len(cases) >= 20, len(cases)
    out = []
    for a, b in cases:
        for sa in (1, -1):
            for sb in (1, -1):
                out.append((sa * a, sb * b))
    return out


def binary_rows(pairs):
    rows = []
    for a, b in pairs:
        r = [hx(a), hx(b), hx(a + b), hx(a - b), hx(a * b)]
        if b:
            r += [hx(trunc_div(a, b)), hx(trunc_rem(a, b)), hx(a % abs(b))]
        else:
            r += ["", "", ""]
        r += [hx(a & b), hx(a | b), hx(a ^ b)]
        rows.append(r)
    return rows


def main():
    rng = random.Random(20260924)
    e = edges()
    randoms = random_operands(rng)
    pairs = [(a, b) for a in e for b in e]
    for i in range(len(randoms)):
        for j in range(len(randoms)):
            if rng.random() < 0.1 or i == j:
                pairs.append((randoms[i], randoms[j]))
    for x in randoms:
        for y in [1, -1, 3, I64_MIN, B - 1, 10 ** 19]:
            pairs.append((x, y))
            pairs.append((y, x))
    # Dividends and divisors a limb apart and equal in length, both
    # normalized and not
    for n in [2, 3, 7]:
        for k in [0, 1, 2]:
            b = rng.getrandbits(64 * n) | (1 << (64 * n - 1 - k))
            b &= (1 << (64 * n - k)) - 1
            a = rng.getrandbits(64 * (n + 3))
            pairs.append((a, b))
            pairs.append((-a, b))
            pairs.append((b, a))

    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py from Python's int; do not edit.")
    w("// Numbers are hexadecimal with a leading minus for a negative one.")
    w("#pragma once")
    w("")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("#include <limits>")
    w("")
    w("namespace math_vectors {")
    w("    inline constexpr double Inf = std::numeric_limits<double>::infinity();")
    w("")
    w("    // a, b, a + b, a - b, a * b, a / b, a % b, a mod |b| (empty for b == 0), a & b, a | b, a ^ b")
    w("    struct Binary { const char* a; const char* b; const char* sum; const char* difference; const char* product;")
    w("                    const char* quotient; const char* remainder; const char* mod; const char* and_; const char* or_; const char* xor_; };")
    w("    inline constexpr Binary binary[] = {")
    for r in binary_rows(pairs):
        w("        {" + ", ".join(lit(x) for x in r) + "},")
    w("    };")
    w("")
    w("    // Divisions that take Knuth's add-back step (D6) at least once")
    w("    struct AddBack { const char* a; const char* b; const char* quotient; const char* remainder; };")
    w("    inline constexpr AddBack add_back[] = {")
    for a, b in add_back_cases():
        w("        {" + ", ".join(lit(x) for x in [hx(a), hx(b), hx(trunc_div(a, b)), hx(trunc_rem(a, b))]) + "},")
    w("    };")
    w("")
    unary_ops = e + randoms
    w("    // a, -a, ~a, bit_length, trailing_zeros, to_double")
    w("    struct Unary { const char* a; const char* negated; const char* complement; size_t bit_length; size_t trailing_zeros; double to_double; };")
    w("    inline constexpr Unary unary[] = {")
    doubles = [
        2 ** 53 - 1, 2 ** 53, 2 ** 53 + 1, 2 ** 53 + 2, 2 ** 53 + 3, 2 ** 54 + 2, 2 ** 54 + 6,
        B - 1, B + 1, 2 ** 64 + 2 ** 11, 2 ** 64 + 2 ** 12, 2 ** 64 + 2 ** 11 + 1,
        (2 ** 53 + 1) * 2 ** 100, (2 ** 53 + 1) * 2 ** 100 + 1, (2 ** 53 + 3) * 2 ** 100,
        2 ** 200 + 2 ** 147, 2 ** 200 + 2 ** 147 + 1, 2 ** 200 + 2 ** 148 + 2 ** 147,
        0x8000000000000400 * B, 0x8000000000000400 * B + 1, 0x8000000000000c00 * B,
        0x8000000000000400 * B ** 2 + 1, 0x8000000000000400 * B ** 3 + B,
        2 ** 1024 - 2 ** 971, 2 ** 1024 - 2 ** 970 - 1, 2 ** 1024 - 2 ** 970, 2 ** 1024, 2 ** 5000,
    ]
    doubles += [-x for x in doubles]
    for x in unary_ops + doubles:
        tz = (abs(x) & -abs(x)).bit_length() - 1 if x else 0
        w("        {" + ", ".join([lit(hx(x)), lit(hx(-x)), lit(hx(~x)), str(abs(x).bit_length()), str(tz), cdouble(to_double(x))]) + "},")
    w("    };")
    w("")
    w("    // a, index, bit index of a in two's complement")
    w("    struct Bit { const char* a; size_t index; bool bit; };")
    w("    inline constexpr Bit bits[] = {")
    for x in unary_ops:
        for i in [0, 1, 2, 62, 63, 64, 65, 127, 128, 129, 200, 1000, 2600]:
            w("        {" + ", ".join([lit(hx(x)), str(i), "true" if (x >> i) & 1 else "false"]) + "},")
    w("    };")
    w("")
    w("    // a, bits, a << bits, a >> bits")
    w("    struct Shift { const char* a; int64_t bits; const char* left; const char* right; };")
    w("    inline constexpr Shift shifts[] = {")
    for x in unary_ops:
        for k in [0, 1, 63, 64, 65, 127, 128, 129, 200, 1000]:
            w("        {" + ", ".join([lit(hx(x)), str(k), lit(hx(x << k)), lit(hx(x >> k))]) + "},")
    w("    };")
    w("")
    w("    // a, base, the digits of a in the base")
    w("    struct Text { const char* a; int base; const char* text; };")
    w("    inline constexpr Text texts[] = {")
    text_values = [0, 1, -1, 35, -36, I64_MIN, I64_MAX, B - 1, B, -(2 ** 127 + 12345),
                   3 ** 200, -(7 ** 150), 10 ** 19, 10 ** 19 - 1, 10 ** 38, 36 ** 40 - 1] + randoms[:12]
    for x in text_values:
        for base in range(2, 37):
            digits = ""
            m = abs(x)
            while m:
                m, d = divmod(m, base)
                digits = "0123456789abcdefghijklmnopqrstuvwxyz"[d] + digits
            digits = digits or "0"
            w("        {" + ", ".join([lit(hx(x)), str(base), lit(("-" if x < 0 else "") + digits)]) + "},")
    w("    };")
    w("")
    w("    // A long decimal: 3^20000")
    w("    inline constexpr const char* power_of_three_hex = " + lit(hx(3 ** 20000)) + ";")
    w("    inline constexpr const char* power_of_three_decimal = " + lit(str(3 ** 20000)) + ";")
    w("")
    w("    // a double, the whole number it is cut to")
    w("    struct FromDouble { double value; const char* whole; };")
    w("    inline constexpr FromDouble from_double[] = {")
    ds = [0.0, -0.0, 0.5, -0.5, 1.5, -1.5, 0.999999, 2.0 ** 52 + 0.5, 2.0 ** 53, 2.0 ** 62, 2.0 ** 63 - 1024,
          2.0 ** 63, -2.0 ** 63, 2.0 ** 63 + 2048, -(2.0 ** 63 + 2048), 2.0 ** 64, -2.0 ** 64, 2.0 ** 1023,
          sys.float_info.max, -sys.float_info.max, 5e-324, -5e-324, 1e30, -1e30, 123456789.987, 1e300 / 7]
    for d in ds:
        w("        {" + cdouble(d) + ", " + lit(hx(int(d))) + "},")
    w("    };")
    w("}")
    print("\n".join(out))


# The operands of the long cases are not written out: both sides make
# them from a seed with splitmix64 (tests/math/fast.cpp has the same
# generator), and the answers are compared through FNV-1a of their
# hexadecimal text, so a product of ten thousand limbs costs a line.
M64 = B - 1


def splitmix(state):
    state = (state + 0x9e3779b97f4a7c15) & M64
    z = state
    z = ((z ^ (z >> 30)) * 0xbf58476d1ce4e5b9) & M64
    z = ((z ^ (z >> 27)) * 0x94d049bb133111eb) & M64
    return state, z ^ (z >> 31)


# n limbs from the seed, the top one never zero; shape 0 random limbs,
# 1 all ones, 2 random with an eighth of the limbs all ones and an eighth
# zero (runs of carries and borrows)
def operand(seed, n, shape, negative=False):
    s = seed
    limbs = []
    for _ in range(n):
        s, x = splitmix(s)
        if shape == 1:
            x = M64
        elif shape == 2:
            s, y = splitmix(s)
            if y % 8 == 0:
                x = M64
            elif y % 8 == 1:
                x = 0
        limbs.append(x)
    if limbs[-1] == 0:
        limbs[-1] = 1
    v = 0
    for l in reversed(limbs):
        v = (v << 64) | l
    return -v if negative else v


def fnv(text):
    h = 0xcbf29ce484222325
    for c in text.encode():
        h ^= c
        h = (h * 0x100000001b3) & M64
    return h


def digits_text(seed, count, base):
    s = seed
    out = []
    for i in range(count):
        s, x = splitmix(s)
        d = x % base
        if i == 0 and d == 0:
            d = 1
        out.append("0123456789abcdefghijklmnopqrstuvwxyz"[d])
    return "".join(out)


def to_base(x, base):
    if base == 10:
        return str(x)
    if base == 16:
        return hx(x)
    digits = []
    m = abs(x)
    while m:
        m, d = divmod(m, base)
        digits.append("0123456789abcdefghijklmnopqrstuvwxyz"[d])
    return ("-" if x < 0 else "") + ("".join(reversed(digits)) or "0")


def fast():
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py fast from Python's int; do not edit.")
    w("// The operands are made from their seeds by splitmix64 on both sides; the")
    w("// answers are FNV-1a 64 of their hexadecimal text (decimal where named).")
    w("#pragma once")
    w("")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w("namespace fast_vectors {")
    # Products and squares: every length around every threshold the
    # algorithms could plausibly take, balanced and not, and long ones
    lengths = [1, 2, 3, 4, 5, 7, 8, 16, 31, 32, 33, 47, 48, 49, 63, 64, 65, 100, 119, 120, 121,
               159, 160, 161, 199, 200, 201, 256, 300, 399, 400, 401, 500, 777, 1000, 1500, 3000, 10000]
    pairs = []
    for n in lengths:
        pairs.append((n, n))
        if n > 1:
            pairs.append((n, n - 1))
            pairs.append((n, (n + 1) // 2))
            pairs.append((n, n // 2 + 2))
        pairs.append((2 * n, n))
        pairs.append((3 * n + 1, n))
    pairs += [(10000, 37), (5000, 3000), (20000, 20000), (30000, 11000), (100000, 100000)]
    w("    // a of an limbs times b of bn, and a squared: seeds, shapes and signs")
    w("    struct Product { size_t an; size_t bn; uint64_t sa; uint64_t sb; int shape_a; int shape_b; bool na; bool nb;")
    w("                     uint64_t product; uint64_t square; };")
    w("    inline constexpr Product products[] = {")
    k = 0
    for an, bn in pairs:
        for shape_a, shape_b, na, nb in [(0, 0, False, False), (2, 1, True, False), (1, 2, False, True), (1, 1, True, True)]:
            if an >= 3000 and (shape_a, shape_b) != (0, 0):
                continue
            k += 1
            sa, sb = 1000 + k, 5000 + k
            a = operand(sa, an, shape_a, na)
            b = operand(sb, bn, shape_b, nb)
            w("        {%d, %d, %d, %d, %d, %d, %s, %s, 0x%xull, 0x%xull}," % (
                an, bn, sa, sb, shape_a, shape_b, "true" if na else "false", "true" if nb else "false",
                fnv(hx(a * b)), fnv(hx(a * a))))
    w("    };")
    w("")
    # Divisions: dividends of every length ratio the recursive division
    # meets, divisors around its threshold and long
    dlengths = [2, 3, 5, 16, 39, 40, 41, 79, 80, 81, 100, 159, 160, 161, 200, 500, 1000, 3000]
    dpairs = []
    for n in dlengths:
        for an in [n, n + 1, n + 40, n + n // 2, 2 * n - 1, 2 * n, 2 * n + 1, 3 * n, 5 * n + 3]:
            dpairs.append((an, n))
    dpairs += [(20000, 10000), (30000, 7000), (10000, 1)]
    w("    // a of an limbs over b of bn: the quotient cut towards zero and the remainder")
    w("    struct Division { size_t an; size_t bn; uint64_t sa; uint64_t sb; int shape_a; int shape_b; bool na; bool nb;")
    w("                      uint64_t quotient; uint64_t remainder; };")
    w("    inline constexpr Division divisions[] = {")
    for an, bn in dpairs:
        for shape_a, shape_b, na, nb in [(0, 0, False, False), (2, 1, True, False), (1, 2, False, True), (0, 2, True, True)]:
            if an >= 3000 and (shape_a, shape_b) != (0, 0):
                continue
            k += 1
            sa, sb = 1000 + k, 5000 + k
            a = operand(sa, an, shape_a, na)
            b = operand(sb, bn, shape_b, nb)
            q = trunc_div(a, b)
            r = a - q * b
            w("        {%d, %d, %d, %d, %d, %d, %s, %s, 0x%xull, 0x%xull}," % (
                an, bn, sa, sb, shape_a, shape_b, "true" if na else "false", "true" if nb else "false",
                fnv(hx(q)), fnv(hx(r))))
    w("    };")
    w("")
    # Text: a number of n limbs in a base, and digits in a base read back
    w("    // The number of n limbs from the seed in the base: its length, FNV-1a of")
    w("    // the digits, and the first and last twenty of them")
    w("    struct Written { size_t n; uint64_t seed; int shape; int base; size_t length; uint64_t text; const char* head; const char* tail; };")
    w("    inline constexpr Written written[] = {")
    cases = [(n, 10) for n in [1, 2, 3, 10, 23, 24, 25, 47, 48, 49, 50, 100, 500, 1000, 3000, 10000, 52000]]
    cases += [(n, b) for n in [30, 700, 5000] for b in [3, 7, 12, 36]]
    for n, base in cases:
        for shape in [0, 1, 2]:
            if n >= 3000 and shape:
                continue
            k += 1
            x = operand(9000 + k, n, shape, shape == 2)
            t = to_base(x, base)
            w("        {%d, %d, %d, %d, %d, 0x%xull, \"%s\", \"%s\"}," % (n, 9000 + k, shape, base, len(t), fnv(t), t[:20], t[-20:]))
    w("    };")
    w("")
    w("    // count digits of the base from the seed (the first never 0): the")
    w("    // value, FNV-1a of it in hexadecimal")
    w("    struct Read { size_t count; uint64_t seed; int base; uint64_t value; };")
    w("    inline constexpr Read read[] = {")
    rcases = [(c, 10) for c in [1, 19, 20, 38, 455, 456, 457, 1000, 10000, 100000, 1000000]]
    rcases += [(c, b) for c in [50, 3000, 60000] for b in [3, 7, 12, 36]]
    for count, base in rcases:
        k += 1
        t = digits_text(20000 + k, count, base)
        w("        {%d, %d, %d, 0x%xull}," % (count, 20000 + k, base, fnv(hx(int(t, base)))))
    w("    };")
    w("}")
    print("\n".join(out))


def number():
    rng = random.Random(20260925)
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py number from Python's int and math; do")
    w("// not edit. Numbers are hexadecimal with a leading minus; a long answer is")
    w("// FNV-1a 64 of its hexadecimal text, and its operand made from a seed as in")
    w("// fast_vectors.h.")
    w("#pragma once")
    w("")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w("namespace number_vectors {")
    small = [0, 1, -1, 2, -2, 3, -3, 10, -10, 7, B - 1, -(B + 1), 2 ** 63, 3 ** 50, -(5 ** 40),
             operand(1, 3, 0), -operand(2, 2, 2), operand(3, 1, 1)]
    w("    // a, e, FNV-1a of a^e and its length in hexadecimal")
    w("    struct Pow { const char* a; int64_t e; uint64_t result; size_t length; };")
    w("    inline constexpr Pow pow[] = {")
    for a in small:
        for e in [0, 1, 2, 3, 5, 63, 64, 65, 100, 1000, 12345]:
            if abs(a) > 2 ** 70 and e > 1000:
                continue
            r = hx(a ** e)
            w("        {%s, %d, 0x%xull, %d}," % (lit(hx(a)), e, fnv(r), len(r)))
    w("    };")
    w("")
    # Square roots: edges, squares and their neighbours, long operands
    roots = [0, 1, 2, 3, 4, 8, 15, 16, 17, 99, 100, 101, I64_MAX, B - 1, B, B + 1, 2 ** 126 - 1, 2 ** 126,
             2 ** 127 - 1, 2 ** 128 - 1, 2 ** 128, 2 ** 128 + 1]
    for k in [65, 127, 128, 129, 200, 255, 256, 257, 1000, 4097]:
        s0 = rng.getrandbits(k) | (1 << (k - 1))
        roots += [s0 * s0 - 1, s0 * s0, s0 * s0 + 1, (s0 + 1) ** 2 - 1, 2 ** (2 * k) - 1, 2 ** (2 * k - 1)]
    w("    // a, isqrt(a)")
    w("    struct Sqrt { const char* a; const char* root; };")
    w("    inline constexpr Sqrt sqrt[] = {")
    for x in roots:
        w("        {%s, %s}," % (lit(hx(x)), lit(hx(math.isqrt(x)))))
    w("    };")
    w("")
    w("    // The operand of n limbs from the seed (shape 0), FNV-1a of its isqrt")
    w("    struct LongSqrt { size_t n; uint64_t seed; uint64_t root; };")
    w("    inline constexpr LongSqrt long_sqrt[] = {")
    for n in [3, 10, 21, 22, 23, 50, 100, 101, 500, 1000, 3000, 10000]:
        x = operand(30000 + n, n, 0)
        w("        {%d, %d, 0x%xull}," % (n, 30000 + n, fnv(hx(math.isqrt(x)))))
    w("    };")
    w("")
    # gcd and lcm: zeros, signs, one dividing the other, coprime, a common
    # factor of any length
    pairs = []
    base = [0, 1, -1, 6, -4, 12, B - 1, B, -(B * 3), 2 ** 127 - 1, 2 ** 200, 3 ** 150]
    pairs += [(a, b) for a in base for b in base]
    for na, nb, ng in [(1, 1, 1), (2, 2, 1), (3, 2, 2), (5, 5, 3), (10, 7, 4), (20, 20, 10), (40, 3, 20)]:
        for rep in range(3):
            g = rng.getrandbits(64 * ng) | 1
            x = rng.getrandbits(64 * na) | 1
            y = rng.getrandbits(64 * nb) | 3
            if rep == 1:
                g = 1
            if rep == 2:
                g <<= rng.randrange(0, 200)
            a = g * x * (-1 if rng.random() < 0.5 else 1)
            b = g * y * (-1 if rng.random() < 0.5 else 1)
            pairs.append((a, b))
    # Fibonacci neighbours: the most steps of Euclid for their length
    f0, f1 = 0, 1
    for i in range(3000):
        f0, f1 = f1, f0 + f1
        if i in (90, 91, 92, 93, 180, 500, 1000, 2999):
            pairs.append((f1, f0))
    w("    // a, b, FNV-1a of gcd(a, b) and of lcm(a, b) in hexadecimal")
    w("    struct Gcd { const char* a; const char* b; uint64_t gcd; uint64_t lcm; };")
    w("    inline constexpr Gcd gcd[] = {")
    for a, b in pairs:
        w("        {%s, %s, 0x%xull, 0x%xull}," % (lit(hx(a)), lit(hx(b)), fnv(hx(math.gcd(a, b))), fnv(hx(math.lcm(a, b)))))
    w("    };")
    w("")
    # Long ones from seeds: a = g·x and b = -g·y (shape 2 for x, so runs of
    # ones and zeros), g of ng limbs shifted up by `shift` bits
    w("    // a = (g << shift)·x, b = -(g << shift)·y, of the operands g (ng limbs),")
    w("    // x (na) and y (nb) from seed, seed + 1 and seed + 2; FNV-1a of gcd and lcm")
    w("    struct LongGcd { size_t na; size_t nb; size_t ng; int shift; uint64_t seed; uint64_t gcd; uint64_t lcm; };")
    w("    inline constexpr LongGcd long_gcd[] = {")
    k = 0
    for na, nb, ng in [(100, 100, 1), (100, 90, 50), (300, 300, 150), (1000, 1000, 1), (1000, 800, 300),
                       (3000, 3000, 1000), (2, 2000, 1), (2000, 2, 1), (500, 500, 1)]:
        for shift in [0, 77]:
            k += 1
            seed = 40000 + 10 * k
            g = operand(seed, ng, 0) << shift
            a = g * operand(seed + 1, na, 2)
            b = -(g * operand(seed + 2, nb, 2))
            w("        {%d, %d, %d, %d, %d, 0x%xull, 0x%xull}," % (na, nb, ng, shift, seed, fnv(hx(math.gcd(a, b))), fnv(hx(math.lcm(a, b)))))
    w("    };")
    w("")
    # mod_pow: m = 1, 2, even, odd of one limb and many, RSA sizes, the
    # exponent 0 and 1, a negative base, a base above m
    cases = []
    for m in [1, 2, 3, 4, 10, 97, B - 59, B - 1, 2 ** 64 + 13, 2 ** 100, 2 ** 127 - 1, 10 ** 40, 2 ** 521 - 1]:
        for a in [0, 1, -1, 2, 3, -7, B + 5, 3 ** 90]:
            for e in [0, 1, 2, 65537, B + 1, 3 ** 100]:
                cases.append((a, e, m))
    for bits in [512, 1024, 2048, 3072, 4096]:
        for rep in range(2):
            m = rng.getrandbits(bits) | (1 << (bits - 1)) | 1
            if rep == 1:
                m -= 1   # even
            a = rng.getrandbits(bits + 13)
            cases.append((a, 65537, m))
            cases.append((a, rng.getrandbits(bits), m))
            cases.append((-a, rng.getrandbits(64), m))
    w("    // a, e, m, a^e mod m")
    w("    struct ModPow { const char* a; const char* e; const char* m; const char* result; };")
    w("    inline constexpr ModPow mod_pow[] = {")
    for a, e, m in cases:
        w("        {%s, %s, %s, %s}," % (lit(hx(a)), lit(hx(e)), lit(hx(m)), lit(hx(pow(a, e, m)))))
    w("    };")
    w("")
    # mod_inverse: invertible and not, signs of both, m of one limb and many
    inv = []
    for m in [1, -1, 2, 3, -7, 10, 12, B - 59, -(B - 59), B, 2 ** 127 - 1, 10 ** 40, 2 ** 521 - 1, 2 ** 1000]:
        for a in [0, 1, -1, 2, 3, 5, -6, 10, B + 7, -(3 ** 100), 2 ** 400 + 1]:
            inv.append((a, m))
    for bits in [100, 256, 1024, 2048, 4096, 20000]:
        for rep in range(3 if bits < 20000 else 1):
            m = rng.getrandbits(bits) | (1 << (bits - 1))
            a = rng.getrandbits(bits + 7)
            if rep == 2:
                g = rng.getrandbits(40) | 1
                m, a = m * g, a * g
            inv.append((a, m))
            inv.append((-a, -m))
    w("    // a, m, the inverse of a modulo |m| in [0, |m|), or empty when there is none")
    w("    struct Inverse { const char* a; const char* m; const char* inverse; };")
    w("    inline constexpr Inverse mod_inverse[] = {")
    for a, m in inv:
        try:
            r = hx(pow(a, -1, abs(m)))
        except ValueError:
            r = ""
        w("        {%s, %s, %s}," % (lit(hx(a)), lit(hx(m)), lit(r)))
    w("    };")
    w("")
    w("    // n, FNV-1a of n! in hexadecimal and its length")
    w("    struct Factorial { int64_t n; uint64_t result; size_t length; };")
    w("    inline constexpr Factorial factorial[] = {")
    for n in [0, 1, 2, 3, 4, 20, 21, 25, 31, 32, 33, 34, 63, 64, 65, 100, 1000, 10000, 100000]:
        r = hx(math.factorial(n))
        w("        {%d, 0x%xull, %d}," % (n, fnv(r), len(r)))
    w("    };")
    w("")
    w("    // n, k, FNV-1a of C(n, k) in hexadecimal")
    w("    struct Binomial { int64_t n; int64_t k; uint64_t result; };")
    w("    inline constexpr Binomial binomial[] = {")
    for n, k in [(0, 0), (1, 0), (1, 1), (1, 2), (5, 2), (5, 3), (5, 6), (10, 5), (52, 5), (100, 50), (1000, 1),
                 (1000, 999), (1000, 500), (10000, 3000), (100000, 50000), (2 ** 40, 3), (2 ** 62, 2),
                 (9223372036854775807, 1), (9223372036854775807, 2), (9223372036854775807, 9223372036854775806),
                 (67, 33), (68, 34)]:
        w("        {%dll, %dll, 0x%xull}," % (n, k, fnv(hx(math.comb(n, k)))))
    w("    };")
    w("}")
    print("\n".join(out))


def half_away(fr, places):
    # The digits of |fr|·10^places rounded half away from zero, as the
    # text to_decimal writes: the definition of the rounding, not a copy
    # of the code under test
    n, d = abs(fr.numerator) * 10 ** places, fr.denominator
    q, r = divmod(n, d)
    if 2 * r >= d:
        q += 1
    t = str(q)
    if places:
        t = t.rjust(places + 1, "0")
        t = t[:-places] + "." + t[-places:]
    return ("-" if fr < 0 else "") + t


def rational_vectors():
    from fractions import Fraction as F
    rng = random.Random(20260926)
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py rational from Python's fractions and")
    w("// float; do not edit. A fraction is its numerator and denominator in")
    w("// decimal, the sign on the numerator.")
    w("#pragma once")
    w("")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("#include <limits>")
    w("")
    w("namespace rational_vectors {")
    w("    inline constexpr double Inf = std::numeric_limits<double>::infinity();")
    w("")
    values = [F(0), F(1), F(-1), F(1, 2), F(-1, 2), F(1, 3), F(-2, 3), F(22, 7), F(355, 113), F(7), F(-12),
              F(1, 10), F(3, 4), F(2 ** 64 + 1, 3), F(-(2 ** 64), 2 ** 64 + 1), F(10 ** 30, 7 ** 20),
              F(-(3 ** 60), 2 ** 90), F(2 ** 127 - 1, 2 ** 61 - 1)]
    for _ in range(8):
        n = rng.getrandbits(rng.choice([8, 64, 130])) * rng.choice([1, -1])
        d = rng.getrandbits(rng.choice([8, 64, 130])) + 1
        values.append(F(n, d))

    def fr(x):
        return '"%d", "%d"' % (x.numerator, x.denominator)

    w("    // a, b and a + b, a - b, a * b, a / b (empty when b is 0), and the sign of a <=> b")
    w("    struct Pair { const char* an; const char* ad; const char* bn; const char* bd;")
    w("                  const char* sn; const char* sd; const char* dn; const char* dd; const char* pn; const char* pd;")
    w("                  const char* qn; const char* qd; int order; };")
    w("    inline constexpr Pair pairs[] = {")
    for a in values:
        for b in values:
            q = '"", ""' if b == 0 else fr(a / b)
            order = (a > b) - (a < b)
            w("        {%s, %s, %s, %s, %s, %s, %d}," % (fr(a), fr(b), fr(a + b), fr(a - b), fr(a * b), q, order))
    w("    };")
    w("")
    w("    // a, floor, ceil, to_double, to_decimal at 0, 1, 2, 3 and 10 places")
    w("    struct Single { const char* n; const char* d; const char* floor; const char* ceil; double to_double;")
    w("                    const char* places[5]; };")
    w("    inline constexpr Single singles[] = {")
    extra = [F(5, 2), F(-5, 2), F(1, 8), F(-1, 8), F(15, 1000), F(-15, 1000), F(-1, 3000), F(1, 2000), F(-1, 2000),
             F(999, 1000), F(-9995, 10000), F(1, 20)]
    for a in values + extra:
        places = [half_away(a, p) for p in (0, 1, 2, 3, 10)]
        w("        {%s, \"%d\", \"%d\", %s, {%s}}," % (fr(a), math.floor(a), math.ceil(a), cdouble(float(a)),
                                                ", ".join(lit(x) for x in places)))
    w("    };")
    w("")
    # The rounding to a double at its hard places: ties to even in normal
    # and subnormal ranges, the edges of the range, and random fractions
    doubles = [F(2 ** 53 + 1), F(2 ** 54 + 1, 2), F(2 ** 53 + 3, 2), F(1, 3), F(2, 3), F(1, 10), F(-1, 10),
               F(1, 2 ** 1074), F(1, 2 ** 1075), F(3, 2 ** 1076), F(1, 2 ** 1076), F(3, 2 ** 1075), F(5, 2 ** 1076),
               F(2 ** 53 + 1, 2 ** (1074 + 53)), F(2 ** 52 - 1, 2 ** 1074 * 2 ** 52), F(1, 2 ** 1022) - F(1, 2 ** 1076),
               F(2 ** 1024 - 2 ** 970), F(2 ** 1024 - 2 ** 970, 1) - F(1, 3), F(2 ** 1025, 3), F(10 ** 400, 3),
               F(1, 10 ** 400), F(-1, 10 ** 400), F(-(10 ** 400), 7), F(2 ** 1024 - 2 ** 971), F(2 ** 1024 - 2 ** 971 + 2 ** 969, 1),
               F(7, 2 ** 1080), F(123456789, 10 ** 330)]
    for _ in range(60):
        n = rng.getrandbits(rng.choice([10, 53, 54, 64, 100, 200, 1100]))
        d = rng.getrandbits(rng.choice([10, 53, 54, 64, 100, 200, 1100])) + 1
        doubles.append(F(n, d) * rng.choice([1, -1]))
    w("    // a fraction and the double nearest it")
    w("    struct Double { const char* n; const char* d; double value; };")
    w("    inline constexpr Double to_double[] = {")
    for a in doubles:
        try:
            v = float(a)
        except OverflowError:
            v = math.inf if a > 0 else -math.inf
        w("        {%s, %s}," % (fr(a), cdouble(v)))
    w("    };")
    w("")
    w("    // a double and the fraction it is exactly")
    w("    struct FromDouble { double value; const char* n; const char* d; };")
    w("    inline constexpr FromDouble from_double[] = {")
    ds = [0.0, -0.0, 1.0, -1.0, 0.5, 0.1, -0.1, 1 / 3, 2.5, 123.456, 1e300, -1e300, 1e-300, 5e-324, -5e-324,
          2.2250738585072014e-308, 2.225073858507201e-308, sys.float_info.max, 2.0 ** 63, 2.0 ** 64 + 4096, 3.0 * 2 ** -60]
    for d in ds:
        x = F(d)
        w("        {%s, %s}," % (cdouble(d), fr(x)))
    w("    };")
    w("")
    texts = ["0", "-0", "+0", "5", "-5", "+5", "3/4", "-3/4", "+3/4", "6/8", "0/7", "-0/7", "1/1",
             "007/014", "0.125", "-0.125", ".5", "-.5", "7.", "1.5e-3", "1.5E-3", "2e10", "2E+10", "-1e-0", "1e0",
             "123456789012345678901234567890/987654321098765432109876543210", "0.000000000000000000000000000001",
             "1e-100", "3.14159e2", "1e1000000", "1e-1000000", "12345678901234567890.0987654321e-5",
             "100000000000000000000000000000000000000000000000000/3"]
    w("    // text, the fraction it reads as")
    w("    struct Parsed { const char* text; const char* n; const char* d; };")
    w("    inline constexpr Parsed parsed[] = {")
    for t in texts:
        x = F(t)
        if abs(x.numerator) > 10 ** 400 or x.denominator > 10 ** 400:
            continue   # the millionth powers are checked by their size in the test
        w("        {%s, %s}," % (lit(t), fr(x)))
    w("    };")
    w("}")
    print("\n".join(out))


# The oracle for sgcl/math/decimal.h: Python's decimal (libmpdec), in
# contexts of the largest precision and exponents for the exact operations,
# of the precision asked for the division to digits and the square root,
# and Python's fractions for the division to a scale and the rounding of a
# fraction, rounded here by the definitions of the modes (a value between
# two whole numbers goes to the one the mode names; a tie only for the
# three half modes). A decimal is written as its unscaled part and scale,
# so the oracle checks the representation as well as the value.
DECIMAL_MODES = ["ROUND_HALF_EVEN", "ROUND_HALF_UP", "ROUND_HALF_DOWN", "ROUND_UP", "ROUND_DOWN", "ROUND_CEILING",
                 "ROUND_FLOOR"]


def round_fraction(fr, mode):
    from fractions import Fraction as F
    q = fr.numerator // fr.denominator   # floor
    if fr == q:
        return q
    negative = fr < 0
    toward = q + 1 if negative else q
    away = q if negative else q + 1
    if mode == "ROUND_DOWN":
        return toward
    if mode == "ROUND_UP":
        return away
    if mode == "ROUND_CEILING":
        return q + 1
    if mode == "ROUND_FLOOR":
        return q
    d = abs(fr - toward)
    if d < F(1, 2):
        return toward
    if d > F(1, 2):
        return away
    if mode == "ROUND_HALF_UP":
        return away
    if mode == "ROUND_HALF_DOWN":
        return toward
    return toward if toward % 2 == 0 else away


def decimal_vectors():
    import decimal as dm
    from fractions import Fraction as F
    rng = random.Random(20261005)
    exact = dm.Context(prec=dm.MAX_PREC, Emax=dm.MAX_EMAX, Emin=dm.MIN_EMIN, traps=[dm.InvalidOperation])

    def ctx(prec, mode):
        return dm.Context(prec=prec, rounding=getattr(dm, mode), Emax=dm.MAX_EMAX, Emin=dm.MIN_EMIN,
                          traps=[dm.InvalidOperation])

    def rep(d):
        sign, digits, exponent = d.as_tuple()
        u = int("".join(map(str, digits)))
        return '"%d", %d' % (-u if sign else u, -exponent)

    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py decimal from Python's decimal and")
    w("// fractions; do not edit. A decimal is its unscaled part in decimal and its")
    w("// scale; the modes are sgcl::rounding's order (half_even, half_up,")
    w("// half_down, up, down, ceiling, floor).")
    w("#pragma once")
    w("")
    w("#include <limits>")
    w("")
    w("namespace decimal_vectors {")
    w("    inline constexpr double Inf = std::numeric_limits<double>::infinity();")
    w("")
    texts = ["0", "0.00", "1", "-1", "0.1", "1.50", "-2.675", "1e3", "-7.5E10", "0.000001", "123456789012345678",
             "-1234567890123456789", "9223372036854775807", "-9223372036854775808", "9223372036854775808",
             "3.14159265358979323846264338327950288", "1e-30", "99999999999999999.99", "-0.5", "2.5", "1E+20",
             "-18446744073709551616.000"]
    for bits, lo, hi in [(8, -3, 5), (60, -5, 20), (64, 0, 25), (130, -10, 40), (200, 0, 60)]:
        for _ in range(2):
            u = rng.getrandbits(bits) * rng.choice([1, -1])
            s = rng.randint(lo, hi)
            texts.append(str(dm.Decimal(u).scaleb(-s, exact)) if s else str(u))
    values = [dm.Decimal(t) for t in texts]

    w("    // a text as parse reads it, and the decimal (unscaled, scale)")
    w("    struct Parsed { const char* text; const char* unscaled; int scale; };")
    w("    inline constexpr Parsed parsed[] = {")
    for t, v in zip(texts, values):
        w("        {%s, %s}," % (lit(t), rep(v)))
    for t in ["+0", "-0", ".5", "7.", "-.25e+2", "1.250E-3", "00012.3400", "1e+1000000", "-1e-1000000", "5e0",
              "123456789012345678901234567890.123456789", "0.000000000000000000000000000000000001"]:
        w("        {%s, %s}," % (lit(t), rep(dm.Decimal(t).copy_abs() if dm.Decimal(t).is_zero() else dm.Decimal(t))))
    w("    };")
    w("")
    w("    // a, b; a + b, a - b, a * b; a % b and the quotient cut towards zero (empty when b")
    w("    // is 0); the order of a and b; a.div(b, 10) and a.div_precision(b, 28), half-even;")
    w("    // a.div(b, 3, mode) and a.div_precision(b, 7, mode) for every mode")
    w("    struct Pair { int a; int b; const char* sum; int sum_scale; const char* difference; int difference_scale;")
    w("                  const char* product; int product_scale; const char* remainder; int remainder_scale;")
    w("                  const char* quotient; int order; const char* div10; int div10_scale; const char* div28;")
    w("                  int div28_scale; const char* div3[7]; const char* div7[7]; int div7_scale[7]; };")
    w("    inline constexpr Pair pairs[] = {")
    for i, a in enumerate(values):
        for j, b in enumerate(values):
            row = [str(i), str(j), rep(exact.add(a, b)), rep(exact.subtract(a, b)), rep(exact.multiply(a, b))]
            fa = F(a)
            fb = F(b)
            if b.is_zero():
                row += ['"", 0', '""']
            else:
                row += [rep(exact.remainder(a, b)), lit(str(int(exact.divide_int(a, b))))]
            row.append(str((fa > fb) - (fa < fb)))
            if b.is_zero():
                row += ['"", 0', '"", 0', "{}", "{}", "{}"]
            else:
                row.append('"%d", 10' % round_fraction(fa / fb * 10 ** 10, "ROUND_HALF_EVEN"))
                row.append(rep(ctx(28, "ROUND_HALF_EVEN").divide(a, b)))
                row.append("{" + ", ".join(lit(str(round_fraction(fa / fb * 1000, m))) for m in DECIMAL_MODES) + "}")
                d7 = [ctx(7, m).divide(a, b) for m in DECIMAL_MODES]
                row.append("{" + ", ".join(rep(d).split(",")[0] for d in d7) + "}")
                row.append("{" + ", ".join(rep(d).split(",")[1].strip() for d in d7) + "}")
            w("        {%s}," % ", ".join(row))
    w("    };")
    w("    inline constexpr const char* values[] = {%s};" % ", ".join(lit(t) for t in texts))
    w("")
    extra = ["2.5", "-2.5", "0.125", "-0.125", "1.005", "9.995", "-99.95", "0.05", "15", "-25", "0.4999", "0.5001"]
    singles = texts + extra
    scales = [-1, 0, 1, 2]
    w("    // a; plain and scientific text; the double; its digits; rescaled to -1, 0, 1, 2 and")
    w("    // rounded to 1 and 3 digits in every mode; the square root to 1, 5, 28 and 50 digits,")
    w("    // half-even (empty for a negative value); trim_scale")
    w("    struct Single { const char* text; const char* plain; const char* scientific; double to_double; int digits;")
    w("                    const char* rescaled[4][7]; const char* rounded1[7]; int rounded1_scale[7];")
    w("                    const char* rounded3[7]; int rounded3_scale[7]; const char* root[4]; int root_scale[4];")
    w("                    const char* trimmed; int trimmed_scale; };")
    w("    inline constexpr Single singles[] = {")
    for t in singles:
        a = dm.Decimal(t)
        row = [lit(t), lit(format(a, "f")), lit(format(a, "e")), cdouble(float(a)), str(len(a.as_tuple().digits))]
        resc = []
        for s in scales:
            q = dm.Decimal((0, (1,), -s))
            resc.append("{" + ", ".join(lit(rep(a.quantize(q, rounding=getattr(dm, m), context=exact)).split(",")[0][1:-1]) for m in DECIMAL_MODES) + "}")
        row.append("{" + ", ".join(resc) + "}")
        for p in (1, 3):
            r = [ctx(p, m).plus(a) for m in DECIMAL_MODES]
            row.append("{" + ", ".join(rep(x).split(",")[0] for x in r) + "}")
            row.append("{" + ", ".join(rep(x).split(",")[1].strip() for x in r) + "}")
        if a < 0:
            row += ['{"", "", "", ""}', "{0, 0, 0, 0}"]
        else:
            r = [ctx(p, "ROUND_HALF_EVEN").sqrt(a) for p in (1, 5, 28, 50)]
            row.append("{" + ", ".join(rep(x).split(",")[0] for x in r) + "}")
            row.append("{" + ", ".join(rep(x).split(",")[1].strip() for x in r) + "}")
        n = a.normalize(exact)
        if n.as_tuple().exponent > 0 or n.is_zero():
            n = n.quantize(dm.Decimal(1), context=exact)
        row.append(rep(n))
        w("        {%s}," % ", ".join(row))
    w("    };")
    w("")
    w("    // a double, the decimal it is exactly, and the shortest that reads back")
    w("    struct FromDouble { double value; const char* exact; int exact_scale; const char* shortest; int shortest_scale; };")
    w("    inline constexpr FromDouble from_double[] = {")
    doubles = [0.1, -0.1, 0.5, 1.0, 100.0, 1e16, 1e22, 1e23, 123456789012345680000.0, 2.0 ** -1074, -(2.0 ** -1022),
               1.7976931348623157e308, 3.141592653589793, 1 / 3, 5e-324, 2.0 ** 63, -(2.0 ** 64), 0.30000000000000004]
    for _ in range(12):
        doubles.append(struct_double(rng))
    for x in doubles:
        e = dm.Decimal(x)
        sh = dm.Decimal(repr(x)).normalize(exact)
        if sh.as_tuple().exponent > 0:
            sh = sh.quantize(dm.Decimal(1), context=exact)
        w("        {%s, %s, %s}," % (cdouble(x), rep(e), rep(sh)))
    w("    };")
    w("")
    w("    // a text and the double nearest it")
    w("    struct ToDouble { const char* text; double value; };")
    w("    inline constexpr ToDouble to_double[] = {")
    for t in ["9007199254740993", "9007199254740995", "1e23", "8.98846567431158e307", "1.7976931348623157e308",
              "1.7976931348623158e308", "1.7976931348623159e308", "2e308", "4.9406564584124654e-324",
              "2.4703282292062327e-324", "2.4703282292062328e-324", "1e-400", "0.1", "-123.456e-10",
              "2.2250738585072011e-308", "2.2250738585072012e-308", "1" + "0" * 400 + "e-400",
              "0." + "3" * 900, "7.2057594037927933e16", "123456789012345678901234567890"]:
        v = float(dm.Decimal(t))
        w("        {%s, %s}," % (lit(t), cdouble(v)))
    w("    };")
    w("")
    w("    // the fraction n/d rounded to 4 places and to -2 in every mode")
    w("    struct FromRational { const char* n; const char* d; const char* places4[7]; const char* places_minus2[7]; };")
    w("    inline constexpr FromRational from_rational[] = {")
    for fr in [F(2, 3), F(-2, 3), F(1, 8), F(-5, 16), F(12345, 2), F(-1, 20000), F(10 ** 25 + 1, 7), F(250, 1),
               F(-150, 1), F(49999, 1000)]:
        p4 = [round_fraction(fr * 10 ** 4, m) for m in DECIMAL_MODES]
        pm = [round_fraction(fr / 100, m) for m in DECIMAL_MODES]
        w("        {\"%d\", \"%d\", {%s}, {%s}}," % (fr.numerator, fr.denominator, ", ".join(lit(str(x)) for x in p4),
                                                ", ".join(lit(str(x)) for x in pm)))
    w("    };")
    w("")
    w("    // the long cases: 1/7 to 1000 digits, the root of 2 to 1000, of 10 to 2000,")
    w("    // the format of 1/3 at 300 places")
    w("    inline constexpr const char* seventh_1000 = %s;" % lit(rep(ctx(1000, "ROUND_HALF_EVEN").divide(1, 7)).split(",")[0][1:-1]))
    w("    inline constexpr const char* root2_1000 = %s;" % lit(rep(ctx(1000, "ROUND_HALF_EVEN").sqrt(2)).split(",")[0][1:-1]))
    w("    inline constexpr const char* root10_2000 = %s;" % lit(rep(ctx(2000, "ROUND_HALF_EVEN").sqrt(10)).split(",")[0][1:-1]))
    w("}")
    print("\n".join(out))


# The oracle for sgcl/math/geometry.h and algebra.h: the same formulas
# computed by Python in double (and the inverses and determinants exactly,
# in fractions, from matrices of numbers a float holds), written out for the
# test to compare within float's rounding. The formulas are the textbook
# ones, written here independently: the product by rows and columns, the
# determinant by permutations, the inverse by Gauss-Jordan elimination,
# Rodrigues' rotation, the camera and projections from their definitions
# (depth 0 to 1), the quaternion product and slerp by the angle.
def algebra_vectors():
    from fractions import Fraction as F
    import itertools
    rng = random.Random(20261006)
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py algebra from Python's float and")
    w("// fractions; do not edit. Matrices are written row by row.")
    w("#pragma once")
    w("")
    w("namespace algebra_vectors {")

    def num():
        return rng.randint(-64, 64) / 8

    def mat(n):
        return [[num() for _ in range(n)] for _ in range(n)]

    def mul(a, b):
        n = len(a)
        return [[sum(a[i][k] * b[k][j] for k in range(n)) for j in range(n)] for i in range(n)]

    def det(a):
        n = len(a)
        total = F(0)
        for perm in itertools.permutations(range(n)):
            sign = 1
            for i in range(n):
                for j in range(i + 1, n):
                    if perm[i] > perm[j]:
                        sign = -sign
            prod = F(1)
            for i in range(n):
                prod *= F(a[i][perm[i]])
            total += sign * prod
        return total

    def inverse(a):
        n = len(a)
        m = [[F(a[i][j]) for j in range(n)] + [F(int(i == j)) for j in range(n)] for i in range(n)]
        for c in range(n):
            p = next((r for r in range(c, n) if m[r][c] != 0), None)
            if p is None:
                return None
            m[c], m[p] = m[p], m[c]
            pv = m[c][c]
            m[c] = [x / pv for x in m[c]]
            for r in range(n):
                if r != c and m[r][c] != 0:
                    f = m[r][c]
                    m[r] = [x - f * y for x, y in zip(m[r], m[c])]
        return [[float(m[i][n + j]) for j in range(n)] for i in range(n)]

    def flat(m):
        return ", ".join(repr(float(x)) for row in m for x in row)

    w("    // a, b, a * b, det(a), the inverse of a (all zeros and false when singular)")
    for n in (3, 4):
        w("    struct Mat%d { float a[%d]; float b[%d]; float product[%d]; double det; bool invertible; float inverse[%d]; };"
          % (n, n * n, n * n, n * n, n * n))
        w("    inline constexpr Mat%d mat%d[] = {" % (n, n))
        cases = [mat(n) for _ in range(40)]
        sing = mat(n)
        sing[1] = [2 * x for x in sing[0]]
        cases.append(sing)
        cases.append([[float(i == j) for j in range(n)] for i in range(n)])
        for a in cases:
            b = mat(n)
            inv = inverse(a)
            w("        {{%s}, {%s}, {%s}, %r, %s, {%s}}," % (flat(a), flat(b), flat(mul(a, b)), float(det(a)),
                                                       "true" if inv else "false",
                                                       flat(inv) if inv else ", ".join(["0"] * (n * n))))
        w("    };")
    w("")

    def norm(v):
        l = math.sqrt(sum(x * x for x in v))
        return [x / l for x in v]

    def cross(a, b):
        return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b))

    w("    // an axis, an angle: the rotation matrix (Rodrigues), and the quaternion")
    w("    struct Rotation { float axis[3]; float angle; float matrix[16]; float quaternion[4]; float v[3]; float turned[3]; };")
    w("    inline constexpr Rotation rotations[] = {")
    for _ in range(20):
        axis = [num() or 1.0 for _ in range(3)]
        angle = rng.uniform(-6.3, 6.3)
        n = norm(axis)
        c, s_ = math.cos(angle), math.sin(angle)
        t = 1 - c
        x, y, z = n
        r = [[t * x * x + c, t * x * y - s_ * z, t * x * z + s_ * y, 0],
             [t * x * y + s_ * z, t * y * y + c, t * y * z - s_ * x, 0],
             [t * x * z - s_ * y, t * y * z + s_ * x, t * z * z + c, 0],
             [0, 0, 0, 1]]
        q = [x * math.sin(angle / 2), y * math.sin(angle / 2), z * math.sin(angle / 2), math.cos(angle / 2)]
        v = [num() for _ in range(3)]
        turned = [sum(r[i][j] * v[j] for j in range(3)) for i in range(3)]
        w("        {{%s}, %r, {%s}, {%s}, {%s}, {%s}}," % (", ".join(map(repr, axis)), angle, flat(r),
                                                      ", ".join(map(repr, q)), ", ".join(map(repr, v)),
                                                      ", ".join(map(repr, turned))))
    w("    };")
    w("")
    w("    // eye, target, up: the view matrix; fov, aspect, near, far: the perspective matrix")
    w("    struct Camera { float eye[3]; float target[3]; float up[3]; float view[16]; float fov; float aspect;")
    w("                    float near; float far; float projection[16]; };")
    w("    inline constexpr Camera cameras[] = {")
    for _ in range(12):
        eye = [rng.uniform(-10, 10) for _ in range(3)]
        target = [rng.uniform(-10, 10) for _ in range(3)]
        up = [0.0, 1.0, 0.0] if rng.random() < 0.5 else norm([rng.uniform(-1, 1) for _ in range(3)])
        f = norm([b - a for a, b in zip(eye, target)])
        s_ = norm(cross(f, up))
        u = cross(s_, f)
        view = [[s_[0], s_[1], s_[2], -dot(s_, eye)], [u[0], u[1], u[2], -dot(u, eye)],
                [-f[0], -f[1], -f[2], dot(f, eye)], [0, 0, 0, 1]]
        fov = rng.uniform(0.3, 2.5)
        aspect = rng.uniform(0.5, 2.5)
        near = rng.uniform(0.01, 1)
        far = near + rng.uniform(1, 1000)
        yk = 1 / math.tan(fov / 2)
        proj = [[yk / aspect, 0, 0, 0], [0, yk, 0, 0], [0, 0, far / (near - far), near * far / (near - far)],
                [0, 0, -1, 0]]
        w("        {{%s}, {%s}, {%s}, {%s}, %r, %r, %r, %r, {%s}}," % (", ".join(map(repr, eye)), ", ".join(map(repr, target)),
                                                                  ", ".join(map(repr, up)), flat(view), fov, aspect,
                                                                  near, far, flat(proj)))
    w("    };")
    w("")
    w("    // two unit quaternions, t: slerp along the shorter arc")
    w("    struct Slerp { float a[4]; float b[4]; float t; float result[4]; };")
    w("    inline constexpr Slerp slerps[] = {")
    for _ in range(30):
        a = norm([rng.uniform(-1, 1) for _ in range(4)])
        b = norm([rng.uniform(-1, 1) for _ in range(4)])
        t = rng.random()
        cosine = dot(a, b)
        e = b
        if cosine < 0:
            cosine = -cosine
            e = [-x for x in b]
        ang = math.acos(min(1.0, cosine))
        ka = math.sin((1 - t) * ang) / math.sin(ang)
        kb = math.sin(t * ang) / math.sin(ang)
        r = [ka * x + kb * y for x, y in zip(a, e)]
        w("        {{%s}, {%s}, %r, {%s}}," % (", ".join(map(repr, a)), ", ".join(map(repr, b)), t, ", ".join(map(repr, r))))
    w("    };")
    w("")
    w("    // affine l, r: l * r, the inverse of l, a rect and its box under l")
    w("    struct Affine { float l[6]; float r[6]; float product[6]; float inverse[6]; float rect[4]; float box[4]; };")
    w("    inline constexpr Affine affines[] = {")
    for _ in range(30):
        l = [num() for _ in range(6)]
        r = [num() for _ in range(6)]
        if l[0] * l[3] - l[1] * l[2] == 0:
            l[0] += 1
        a, b, c, d, e, f = l
        A, B, C, D, E, Fv = r
        prod = [a * A + c * B, b * A + d * B, a * C + c * D, b * C + d * D, a * E + c * Fv + e, b * E + d * Fv + f]
        det_ = F(a) * F(d) - F(b) * F(c)
        ia, ib, ic, id_ = F(d) / det_, -F(b) / det_, -F(c) / det_, F(a) / det_
        inv = [ia, ib, ic, id_, -(ia * F(e) + ic * F(f)), -(ib * F(e) + id_ * F(f))]
        rx, ry = num(), num()
        rw, rh = abs(num()) + 0.5, abs(num()) + 0.5
        corners = [(rx, ry), (rx + rw, ry), (rx, ry + rh), (rx + rw, ry + rh)]
        pts = [(a * x + c * y + e, b * x + d * y + f) for x, y in corners]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        box = [min(xs), min(ys), max(xs) - min(xs), max(ys) - min(ys)]
        w("        {{%s}, {%s}, {%s}, {%s}, {%s}, {%s}}," % (", ".join(map(repr, l)), ", ".join(map(repr, r)),
                                                        ", ".join(map(repr, prod)), ", ".join(repr(float(x)) for x in inv),
                                                        ", ".join(map(repr, [rx, ry, rw, rh])), ", ".join(map(repr, box))))
    w("    };")
    w("}")
    print("\n".join(out))


# The oracle for sgcl/math/interpolation.h and bezier.h: the easing
# functions as easings.net publishes Penner's equations and CSS Easing
# defines cubic-bezier() and steps(), computed here in double (the curve of
# cubic-bezier inverted by bisection to 1e-15, not Newton's method); the
# Bézier curves by Bernstein's polynomials (not de Casteljau's construction),
# their bounds by sampling a million parameters and polishing the extremes
# by golden-section search, their lengths by composite Gauss-Legendre over a
# thousand pieces.
def curves_vectors():
    rng = random.Random(20261007)
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py curves from Python's float; do not edit.")
    w("#pragma once")
    w("")
    w("namespace curves_vectors {")
    pi = math.pi
    c1 = 1.70158
    c2 = c1 * 1.525
    c3 = c1 + 1
    c4 = 2 * pi / 3
    c5 = 2 * pi / 4.5

    def bounce(x):
        n1, d1 = 7.5625, 2.75
        if x < 1 / d1:
            return n1 * x * x
        elif x < 2 / d1:
            x -= 1.5 / d1
            return n1 * x * x + 0.75
        elif x < 2.5 / d1:
            x -= 2.25 / d1
            return n1 * x * x + 0.9375
        x -= 2.625 / d1
        return n1 * x * x + 0.984375

    named = [
        ("ease_in_sine", lambda x: 1 - math.cos(x * pi / 2)),
        ("ease_out_sine", lambda x: math.sin(x * pi / 2)),
        ("ease_in_out_sine", lambda x: -(math.cos(pi * x) - 1) / 2),
        ("ease_in_quad", lambda x: x ** 2),
        ("ease_out_quad", lambda x: 1 - (1 - x) ** 2),
        ("ease_in_out_quad", lambda x: 2 * x * x if x < 0.5 else 1 - (-2 * x + 2) ** 2 / 2),
        ("ease_in_cubic", lambda x: x ** 3),
        ("ease_out_cubic", lambda x: 1 - (1 - x) ** 3),
        ("ease_in_out_cubic", lambda x: 4 * x ** 3 if x < 0.5 else 1 - (-2 * x + 2) ** 3 / 2),
        ("ease_in_quart", lambda x: x ** 4),
        ("ease_out_quart", lambda x: 1 - (1 - x) ** 4),
        ("ease_in_out_quart", lambda x: 8 * x ** 4 if x < 0.5 else 1 - (-2 * x + 2) ** 4 / 2),
        ("ease_in_quint", lambda x: x ** 5),
        ("ease_out_quint", lambda x: 1 - (1 - x) ** 5),
        ("ease_in_out_quint", lambda x: 16 * x ** 5 if x < 0.5 else 1 - (-2 * x + 2) ** 5 / 2),
        ("ease_in_expo", lambda x: 0 if x == 0 else 2 ** (10 * x - 10)),
        ("ease_out_expo", lambda x: 1 if x == 1 else 1 - 2 ** (-10 * x)),
        ("ease_in_out_expo", lambda x: 0 if x == 0 else 1 if x == 1 else 2 ** (20 * x - 10) / 2 if x < 0.5
            else (2 - 2 ** (-20 * x + 10)) / 2),
        ("ease_in_circ", lambda x: 1 - math.sqrt(1 - x ** 2)),
        ("ease_out_circ", lambda x: math.sqrt(1 - (x - 1) ** 2)),
        ("ease_in_out_circ", lambda x: (1 - math.sqrt(1 - (2 * x) ** 2)) / 2 if x < 0.5
            else (math.sqrt(1 - (-2 * x + 2) ** 2) + 1) / 2),
        ("ease_in_back", lambda x: c3 * x ** 3 - c1 * x ** 2),
        ("ease_out_back", lambda x: 1 + c3 * (x - 1) ** 3 + c1 * (x - 1) ** 2),
        ("ease_in_out_back", lambda x: ((2 * x) ** 2 * ((c2 + 1) * 2 * x - c2)) / 2 if x < 0.5
            else ((2 * x - 2) ** 2 * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2),
        ("ease_in_elastic", lambda x: 0 if x == 0 else 1 if x == 1 else -2 ** (10 * x - 10) * math.sin((x * 10 - 10.75) * c4)),
        ("ease_out_elastic", lambda x: 0 if x == 0 else 1 if x == 1 else 2 ** (-10 * x) * math.sin((x * 10 - 0.75) * c4) + 1),
        ("ease_in_out_elastic", lambda x: 0 if x == 0 else 1 if x == 1 else
            -(2 ** (20 * x - 10) * math.sin((20 * x - 11.125) * c5)) / 2 if x < 0.5
            else (2 ** (-20 * x + 10) * math.sin((20 * x - 11.125) * c5)) / 2 + 1),
        ("ease_in_bounce", lambda x: 1 - bounce(1 - x)),
        ("ease_out_bounce", bounce),
        ("ease_in_out_bounce", lambda x: (1 - bounce(1 - 2 * x)) / 2 if x < 0.5 else (1 + bounce(2 * x - 1)) / 2),
    ]
    xs = [0.0, 1.0, 0.5, 0.25, 0.75, 0.1, 0.9, 0.01, 0.99, 0.3636, 0.7272, 0.9090] + [rng.random() for _ in range(20)]
    xs = [float(struct_float(x)) for x in xs]
    w("    inline constexpr float progress[] = {%s};" % ", ".join(repr(x) for x in xs))
    w("    inline constexpr int progress_count = %d;" % len(xs))
    w("")
    w("    // the named easing (its member's name) and its value at each progress")
    w("    struct Named { const char* name; double values[%d]; };" % len(xs))
    w("    inline constexpr Named named[] = {")
    for name, f in named:
        w("        {%s, {%s}}," % (lit(name), ", ".join(repr(f(x)) for x in xs)))
    w("    };")
    w("")

    def bezier_y(x1, y1, x2, y2, x):
        # in fractions: where the curve's x stands still a float bisection
        # cannot tell the sides apart
        from fractions import Fraction as F
        x1, y1, x2, y2, x = F(x1), F(y1), F(x2), F(y2), F(x)

        def bx(t):
            return 3 * (1 - t) ** 2 * t * x1 + 3 * (1 - t) * t * t * x2 + t ** 3
        lo, hi = F(0), F(1)
        for _ in range(64):
            mid = (lo + hi) / 2
            if bx(mid) < x:
                lo = mid
            else:
                hi = mid
        t = (lo + hi) / 2
        return float(3 * (1 - t) ** 2 * t * y1 + 3 * (1 - t) * t * t * y2 + t ** 3)

    beziers = [(0.25, 0.1, 0.25, 1.0), (0.42, 0.0, 1.0, 1.0), (0.0, 0.0, 0.58, 1.0), (0.42, 0.0, 0.58, 1.0),
               (0.68, -0.6, 0.32, 1.6), (0.0, 1.0, 1.0, 0.0), (1.0, 0.0, 0.0, 1.0), (0.5, 0.5, 0.5, 0.5),
               (0.0, 0.0, 0.0, 0.0), (1.0, 1.0, 1.0, 1.0)]
    for _ in range(10):
        beziers.append((rng.random(), rng.uniform(-1, 2), rng.random(), rng.uniform(-1, 2)))
    beziers = [tuple(struct_float(v) for v in b) for b in beziers]
    w("    // cubic-bezier(x1, y1, x2, y2) at each progress")
    w("    struct Bezier { float p[4]; double values[%d]; };" % len(xs))
    w("    inline constexpr Bezier beziers[] = {")
    for b in beziers:
        w("        {{%s}, {%s}}," % (", ".join(repr(v) for v in b), ", ".join(repr(bezier_y(*b, x)) for x in xs)))
    w("    };")
    w("")

    def steps(n, pos, x):
        jumps = n + (pos == 3) - (pos == 2)
        step = math.floor(x * n)
        if pos in (0, 3):
            step += 1
        step = max(0, min(step, jumps))
        return step / jumps
    w("    // steps(n, position) (jump_start, jump_end, jump_none, jump_both) at each progress")
    w("    struct Steps { int count; int position; double values[%d]; };" % len(xs))
    w("    inline constexpr Steps steps[] = {")
    for n in (1, 2, 3, 4, 5, 10):
        for pos in range(4):
            if pos == 2 and n < 2:
                continue
            w("        {%d, %d, {%s}}," % (n, pos, ", ".join(repr(steps(n, pos, x)) for x in xs)))
    w("    };")
    w("")

    def bern(ps, t):
        n = len(ps) - 1
        x = sum(math.comb(n, i) * (1 - t) ** (n - i) * t ** i * p[0] for i, p in enumerate(ps))
        y = sum(math.comb(n, i) * (1 - t) ** (n - i) * t ** i * p[1] for i, p in enumerate(ps))
        return x, y

    def dbern(ps, t):
        n = len(ps) - 1
        qs = [(n * (ps[i + 1][0] - ps[i][0]), n * (ps[i + 1][1] - ps[i][1])) for i in range(n)]
        return bern(qs, t)

    def bounds(ps):
        samples = 20000
        lo = [min(ps[0][k], ps[-1][k]) for k in range(2)]
        hi = [max(ps[0][k], ps[-1][k]) for k in range(2)]
        for k in range(2):
            vals = [bern(ps, i / samples)[k] for i in range(samples + 1)]
            for i in range(1, samples):
                for sign, best in ((1, lo), (-1, hi)):
                    if sign * vals[i] <= sign * vals[i - 1] and sign * vals[i] <= sign * vals[i + 1]:
                        a, b = (i - 1) / samples, (i + 1) / samples
                        g = (math.sqrt(5) - 1) / 2
                        for _ in range(100):
                            c = b - g * (b - a)
                            d = a + g * (b - a)
                            if sign * bern(ps, c)[k] < sign * bern(ps, d)[k]:
                                b = d
                            else:
                                a = c
                        v = bern(ps, (a + b) / 2)[k]
                        if sign > 0:
                            best[k] = min(best[k], v)
                        else:
                            best[k] = max(best[k], v)
        return [lo[0], lo[1], hi[0] - lo[0], hi[1] - lo[1]]

    def length(ps):
        xg = [0.0, 0.5384693101056831, -0.5384693101056831, 0.9061798459386640, -0.9061798459386640]
        wg = [0.5688888888888889, 0.4786286704993665, 0.4786286704993665, 0.2369268850561891, 0.2369268850561891]
        pieces = 1000
        total = 0.0
        for i in range(pieces):
            a, b = i / pieces, (i + 1) / pieces
            for xi, wi in zip(xg, wg):
                t = (a + b) / 2 + (b - a) / 2 * xi
                dx, dy = dbern(ps, t)
                total += wi * math.hypot(dx, dy) * (b - a) / 2
        return total

    def coord():
        return float(struct_float(rng.uniform(-100, 100)))

    w("    // control points (x, y), then at t, the derivative at t, the bounds, the length")
    for deg in (2, 3):
        name = "quadratics" if deg == 2 else "cubics"
        w("    struct Curve%d { float p[%d]; float t; double at[2]; double derivative[2]; double bounds[4]; double length; };"
          % (deg, 2 * (deg + 1)))
        w("    inline constexpr Curve%d %s[] = {" % (deg, name))
        curves = []
        if deg == 3:
            curves += [[(0, 0), (0, 100), (100, 100), (100, 0)], [(0, 0), (100, 100), (0, 100), (100, 0)],
                       [(0, 0), (1, 1), (2, 2), (3, 3)], [(5, 5), (5, 5), (5, 5), (5, 5)],
                       [(0, 0), (300, 0), (-200, 0), (100, 0)]]
        else:
            curves += [[(0, 0), (50, 100), (100, 0)], [(0, 0), (1, 1), (2, 2)], [(3, 3), (3, 3), (3, 3)],
                       [(0, 0), (100, 0), (-50, 0)]]
        for _ in range(16):
            curves.append([(coord(), coord()) for _ in range(deg + 1)])
        for ps in curves:
            ps = [(float(x), float(y)) for x, y in ps]
            t = float(struct_float(rng.random()))
            a = bern(ps, t)
            d = dbern(ps, t)
            w("        {{%s}, %r, {%r, %r}, {%r, %r}, {%s}, %r}," % (", ".join(repr(c) for p in ps for c in p), t, a[0], a[1],
                                                            d[0], d[1], ", ".join(repr(v) for v in bounds(ps)), length(ps)))
        w("    };")
    w("}")
    print("\n".join(out))


def struct_float(x):
    import struct
    return struct.unpack("<f", struct.pack("<f", x))[0]


# The oracle for sgcl/math/statistics.h: Python's statistics module (exact
# in fractions where it is: mean, variance, pvariance, stdev, pstdev, median,
# quantiles, mode, covariance, correlation, linear_regression) and the
# population skewness and excess kurtosis computed here in fractions from
# their definitions, g1 = m3/m2^(3/2), g2 = m4/m2^2 - 3.
def statistics_vectors():
    import statistics as st
    from fractions import Fraction as F
    rng = random.Random(20261008)
    out = []
    w = out.append
    w("//------------------------------------------------------------------------------")
    w("// SGCL: a C++20 application platform")
    w("// Copyright (c) 2022-2026 Sebastian Nibisz")
    w("// SPDX-License-Identifier: Apache-2.0")
    w("//------------------------------------------------------------------------------")
    w("// Generated by tools/math_vectors.py statistics from Python's statistics")
    w("// and fractions; do not edit.")
    w("#pragma once")
    w("")
    w("#include <cstddef>")
    w("")
    w("namespace statistics_vectors {")
    sets = {
        "small": [2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0],
        "two": [1.0, 3.0],
        "one": [42.5],
        "integers": [float(rng.randint(-5, 5)) for _ in range(101)],
        "normal": [rng.gauss(10, 3) for _ in range(1000)],
        "offset": [1e9 + rng.random() for _ in range(500)],
        "skewed": [rng.expovariate(0.5) for _ in range(777)],
        "tiny": [rng.uniform(-1e-70, 1e-70) for _ in range(64)],
        "mixed": [rng.choice([-1e6, 1, 1e-6, 3.5, -2.25]) * rng.random() for _ in range(300)],
    }
    names = list(sets)

    def central(data, k):
        fs = [F(x) for x in data]
        mean = sum(fs) / len(fs)
        return sum((x - mean) ** k for x in fs) / len(fs)

    for name in names:
        data = sets[name]
        w("    inline constexpr double %s[] = {%s};" % (name, ", ".join(repr(x) for x in data)))
    w("")
    w("    struct Set { const char* name; const double* data; size_t size; double mean; double median; double variance;")
    w("                 double pvariance; double stdev; double pstdev; double skewness; double kurtosis; double mode;")
    w("                 double quartiles[3]; double deciles[9]; double percentiles[99]; };")
    w("    inline constexpr Set sets[] = {")
    nan = "__builtin_nan(\"\")"
    for name in names:
        data = sets[name]
        n = len(data)
        var = repr(st.variance(data)) if n > 1 else nan
        sd = repr(st.stdev(data)) if n > 1 else nan
        m2 = central(data, 2)
        m3 = central(data, 3)
        skew = repr(math.copysign(math.sqrt(float(m3 * m3 / m2 ** 3)), m3)) if m2 else nan
        kurt = repr(float(central(data, 4) / m2 ** 2 - 3)) if m2 else nan
        def qs(k):
            if n < 2:
                return ", ".join([repr(data[0])] * (k - 1))
            return ", ".join(repr(x) for x in st.quantiles(data, n=k, method="inclusive"))
        w("        {%s, %s, %d, %r, %r, %s, %r, %s, %r, %s, %s, %r, {%s}, {%s}, {%s}}," % (
            lit(name), name, n, st.fmean(data), st.median(data), var, st.pvariance(data), sd, st.pstdev(data), skew, kurt,
            st.mode(data), qs(4), qs(10), qs(100)))
    w("    };")
    w("")
    w("    // x, y: covariance, correlation, slope, intercept")
    pairs = []
    xs = [rng.gauss(0, 1) for _ in range(400)]
    ys = [2.5 * x + 1 + rng.gauss(0, 0.5) for x in xs]
    pairs.append(("linear", xs, ys))
    xs2 = [float(i) for i in range(50)]
    ys2 = [math.sin(i) for i in range(50)]
    pairs.append(("sine", xs2, ys2))
    xs3 = [1e6 + rng.random() for _ in range(300)]
    ys3 = [-x * 3 + rng.random() for x in xs3]
    pairs.append(("offset", xs3, ys3))
    for name, x, y in pairs:
        w("    inline constexpr double pair_%s_x[] = {%s};" % (name, ", ".join(repr(v) for v in x)))
        w("    inline constexpr double pair_%s_y[] = {%s};" % (name, ", ".join(repr(v) for v in y)))
    w("    struct Pair { const char* name; const double* x; const double* y; size_t size; double covariance;")
    w("                  double correlation; double slope; double intercept; };")
    w("    inline constexpr Pair pairs[] = {")
    for name, x, y in pairs:
        r = st.linear_regression(x, y)
        w("        {%s, pair_%s_x, pair_%s_y, %d, %r, %r, %r, %r}," % (lit(name), name, name, len(x), st.covariance(x, y),
                                                               st.correlation(x, y), r.slope, r.intercept))
    w("    };")
    w("}")
    print("\n".join(out))


def struct_double(rng):
    import struct
    while True:
        x = struct.unpack("<d", struct.pack("<Q", rng.getrandbits(64)))[0]
        if math.isfinite(x):
            return x


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "rational":
        rational_vectors()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "statistics":
        statistics_vectors()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "curves":
        curves_vectors()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "algebra":
        algebra_vectors()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "decimal":
        decimal_vectors()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "number":
        number()
        sys.exit(0)
    if len(sys.argv) > 1 and sys.argv[1] == "fast":
        fast()
    else:
        main()
