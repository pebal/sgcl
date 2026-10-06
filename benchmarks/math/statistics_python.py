#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The functions of a sequence of bench_math's statistics cases over Python's
# statistics module, under the same names:
#   statistics_python.py <op> [n]
# smean (fmean), smedian (median), spct (quantiles(n=100,
# method='inclusive')[98], the 0.99 quantile), each reported per value of n
# (a million by default) lognormal values. Each op is repeated, doubling the
# count, until a run takes half a second.
import random
import statistics
import sys
import time


def measure(f):
    count = 1
    while True:
        t0 = time.perf_counter()
        f(count)
        s = time.perf_counter() - t0
        if s > 0.5:
            return s * 1e9 / count
        count *= 2


if __name__ == "__main__":
    op = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 and int(sys.argv[2]) else 1000000
    rng = random.Random(5)
    values = [rng.lognormvariate(0, 1) for _ in range(n)]
    functions = {
        "smean": lambda: statistics.fmean(values),
        "smedian": lambda: statistics.median(values),
        "spct": lambda: statistics.quantiles(values, n=100, method="inclusive")[98],
    }
    if op not in functions:
        print("statistics_python.py: no op called " + op, file=sys.stderr)
        sys.exit(1)
    f = functions[op]

    def run(count):
        for _ in range(count):
            f()
    print("python op=%s n=%d ns/op=%.2f" % (op, n, measure(run) / n))
