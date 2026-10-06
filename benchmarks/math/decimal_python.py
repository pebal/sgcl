#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The decimal cases of benchmarks/math/math.cpp over Python's decimal (the C
# implementation, libmpdec), under the same names:
#   decimal_python.py <op> [n]
# n is the number of digits of the operands, two of them after the point;
# the context's precision is n for ddiv and dsqrt, and large enough for the
# others to be exact. Each op is repeated, doubling the count, until a run
# takes half a second; prints nanoseconds per operation. The interpreter's
# own cost of a statement (tens of nanoseconds) is in every number.
import decimal
import random
import sys
import time

decimal.getcontext().prec = decimal.MAX_PREC
decimal.getcontext().Emax = decimal.MAX_EMAX
decimal.getcontext().Emin = decimal.MIN_EMIN
D = decimal.Decimal


def text(rng, n):
    t = str(rng.randint(1, 9)) + "".join(str(rng.randint(0, 9)) for _ in range(n - 1))
    return t[:-2] + "." + t[-2:] if n > 2 else t


def measure(f):
    count = 1
    while True:
        t0 = time.perf_counter()
        f(count)
        s = time.perf_counter() - t0
        if s > 0.5 or count > 1 << 40:
            return s * 1e9 / count
        count *= 2


def run(op, n):
    rng = random.Random(1)
    at = text(rng, n)
    a = D(at)
    b = D(text(rng, n))
    if op == "dadd":
        def f(count):
            for _ in range(count):
                a + b
    elif op == "daddmix":
        b4 = b.scaleb(-2)
        def f(count):
            for _ in range(count):
                a + b4
    elif op == "dmul":
        def f(count):
            for _ in range(count):
                a * b
    elif op == "dmoney":
        rate = D("0.0825")
        cents = D("0.01")
        def f(count):
            for _ in range(count):
                (a * rate).quantize(cents, rounding=decimal.ROUND_HALF_EVEN)
    elif op == "ddiv":
        ctx = decimal.Context(prec=n, Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN)
        def f(count):
            for _ in range(count):
                ctx.divide(a, b)
    elif op == "dsqrt":
        ctx = decimal.Context(prec=n, Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN)
        two = D(2)
        def f(count):
            for _ in range(count):
                ctx.sqrt(two)
    elif op == "dparse":
        def f(count):
            for _ in range(count):
                D(at)
    elif op == "dformat":
        def f(count):
            for _ in range(count):
                format(a, "f")
    elif op == "dsum":
        values = [D(text(rng, n)) for _ in range(1000)]
        def f(count):
            for _ in range(count):
                s = D(0)
                for v in values:
                    s += v
        return measure(f) / 1000
    else:
        return -1
    return measure(f)


if __name__ == "__main__":
    sys.set_int_max_str_digits(0)
    op = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    ns = run(op, n)
    if ns < 0:
        print("decimal_python.py: no op called " + op, file=sys.stderr)
        sys.exit(1)
    print("python op=%s n=%d ns/op=%.2f" % (op, n, ns))
