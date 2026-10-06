#!/usr/bin/env python3
# The vectors of tests/txt/distance.cpp: random pairs of texts — of a few
# letters (repeats, transpositions), past ASCII, invalid UTF-8, long ones that
# cross the 64-unit words — measured by the textbook tables written here from
# the definitions (Levenshtein; the optimal string alignment; Damerau's
# distance by Lowrance and Wagner's full table; the longest common
# subsequence; Jaro and Winkler) and by the library (tools/distance_driver.cpp).
# Prints a summary on stderr and the vectors on stdout:
#
#   tools/distance_vectors.py <distance_driver> > tests/txt/distance_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(int(os.environ.get("SEED", "20261006")))
DRIVER = sys.argv[1]
N = int(os.environ.get("N", "1500"))


def units(b, as_bytes):
    if as_bytes:
        return list(b)
    out, i = [], 0
    while i < len(b):
        c = b[i]
        n = 1 if c < 0x80 else 2 if c >> 5 == 6 else 3 if c >> 4 == 14 else 4 if c >> 3 == 30 else 0
        if n:
            try:
                ch = b[i:i + n].decode("utf-8")
                if len(ch) == 1:
                    out.append(ord(ch))
                    i += n
                    continue
            except UnicodeDecodeError:
                pass
        out.append(0x110000 + c)   # an ill-formed byte, a unit of its own
        i += 1
    return out


def levenshtein(a, b):
    prev = list(range(len(b) + 1))
    for i in range(1, len(a) + 1):
        cur = [i] + [0] * len(b)
        for j in range(1, len(b) + 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1]))
        prev = cur
    return prev[len(b)]


def osa(a, b):
    d = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a) + 1):
        d[i][0] = i
    for j in range(len(b) + 1):
        d[0][j] = j
    for i in range(1, len(a) + 1):
        for j in range(1, len(b) + 1):
            d[i][j] = min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] != b[j - 1]))
            if i > 1 and j > 1 and a[i - 1] == b[j - 2] and a[i - 2] == b[j - 1]:
                d[i][j] = min(d[i][j], d[i - 2][j - 2] + 1)
    return d[len(a)][len(b)]


def damerau(a, b):
    # Lowrance and Wagner: the full table, a row and a column of infinity around it
    inf = len(a) + len(b)
    da = {}
    d = [[0] * (len(b) + 2) for _ in range(len(a) + 2)]
    d[0][0] = inf
    for i in range(len(a) + 1):
        d[i + 1][0] = inf
        d[i + 1][1] = i
    for j in range(len(b) + 1):
        d[0][j + 1] = inf
        d[1][j + 1] = j
    for i in range(1, len(a) + 1):
        db = 0
        for j in range(1, len(b) + 1):
            k = da.get(b[j - 1], 0)
            l = db
            cost = 1
            if a[i - 1] == b[j - 1]:
                cost = 0
                db = j
            d[i + 1][j + 1] = min(d[i][j] + cost, d[i + 1][j] + 1, d[i][j + 1] + 1,
                                  d[k][l] + (i - k - 1) + 1 + (j - l - 1))
        da[a[i - 1]] = i
    return d[len(a) + 1][len(b) + 1]


def lcs(a, b):
    prev = [0] * (len(b) + 1)
    for i in range(len(a)):
        cur = [0] * (len(b) + 1)
        for j in range(len(b)):
            cur[j + 1] = prev[j] + 1 if a[i] == b[j] else max(prev[j + 1], cur[j])
        prev = cur
    return prev[len(b)]


def jaro(a, b):
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    window = max(max(len(a), len(b)) // 2 - 1, 0)
    fa, fb = [False] * len(a), [False] * len(b)
    m = 0
    for i in range(len(a)):
        for j in range(max(0, i - window), min(len(b), i + window + 1)):
            if not fb[j] and a[i] == b[j]:
                fa[i] = fb[j] = True
                m += 1
                break
    if m == 0:
        return 0.0
    ma = [a[i] for i in range(len(a)) if fa[i]]
    mb = [b[j] for j in range(len(b)) if fb[j]]
    t = sum(x != y for x, y in zip(ma, mb)) / 2
    return (m / len(a) + m / len(b) + (m - t) / m) / 3


def jaro_winkler(a, b):
    j = jaro(a, b)
    if j <= 0.7:
        return j
    p = 0
    while p < min(4, len(a), len(b)) and a[p] == b[p]:
        p += 1
    return j + p * 0.1 * (1 - j)


ALPHABETS = [b"ab", b"abc", b"abcd", b"kitten", "zażółć".encode(), "日本語".encode(), b"\xff\xc3a", b"ab\xc4\x85"]


def text(alpha, n):
    pieces = []
    for _ in range(n):
        k = R.randrange(len(alpha))
        # a whole code point of a multi-byte alphabet, or a byte
        s = alpha.decode("utf-8", "surrogateescape")
        pieces.append(R.choice(s).encode("utf-8", "surrogateescape"))
    return b"".join(pieces)


def mutate(t, alpha):
    s = list(t.decode("utf-8", "surrogateescape"))
    a = alpha.decode("utf-8", "surrogateescape")
    for _ in range(R.randint(0, 6)):
        op = R.random()
        if op < 0.25 and len(s) > 1:
            i = R.randrange(len(s) - 1)
            s[i], s[i + 1] = s[i + 1], s[i]
        elif op < 0.5:
            s.insert(R.randint(0, len(s)), R.choice(a))
        elif op < 0.75 and s:
            del s[R.randrange(len(s))]
        elif s:
            s[R.randrange(len(s))] = R.choice(a)
    return "".join(s).encode("utf-8", "surrogateescape")


cases = []
for k in range(N):
    alpha = R.choice(ALPHABETS)
    n = R.choice([0, 1, 2, 3, 5, 8, 12, 20, 40, 63, 64, 65, 70, 100, 130, 200])
    a = text(alpha, n)
    b = mutate(a, alpha) if R.random() < 0.7 else text(alpha, R.choice([0, 1, 5, 20, 64, 65, 129]))
    cases.append((a, b, R.random() < 0.2))

q = "".join("%s %s %s\n" % (a.hex() or "-", b.hex() or "-", "b" if by else "c") for a, b, by in cases)
answers = subprocess.run([DRIVER], input=q.encode(), capture_output=True, check=True).stdout.decode().split("\n")
bad = 0
rows = []
for (a, b, by), ans in zip(cases, answers):
    ua, ub = units(a, by), units(b, by)
    want = (levenshtein(ua, ub), osa(ua, ub), damerau(ua, ub), lcs(ua, ub), jaro(ua, ub), jaro_winkler(ua, ub))
    got = ans.split()
    ok = all(int(got[i]) == want[i] for i in range(4)) and all(abs(float(got[i]) - want[i]) < 1e-9 for i in (4, 5))
    if not ok:
        bad += 1
        if bad <= 10:
            sys.stderr.write("differs: %r %r bytes=%s want %r got %s\n" % (a, b, by, want, ans))
    rows.append((a, b, by) + want)


def c_literal(b):
    out = ['"']
    for ch in b:
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append("\\\\")
        elif 0x20 <= ch < 0x7F and ch != 0x3F:
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    out.append('"')
    return "".join(out)


print("//------------------------------------------------------------------------------")
print("// SGCL: a C++20 application platform")
print("// Copyright (c) 2022-2026 Sebastian Nibisz")
print("// SPDX-License-Identifier: Apache-2.0")
print("//------------------------------------------------------------------------------")
print("// Generated by tools/distance_vectors.py: do not edit. Each pair measured by")
print("// the textbook tables of the definitions.")
print("#pragma once")
print()
print("#include <cstddef>")
print()
print("struct DistanceVector {")
print("    const char* a;")
print("    const char* b;")
print("    bool bytes;")
print("    size_t levenshtein, osa, damerau, lcs;")
print("    double jaro, jaro_winkler;")
print("};")
print()
print("inline const DistanceVector DistanceVectors[] = {")
for a, b, by, l, o, d, c, j, w in rows:
    print("    {%s, %s, %s, %d, %d, %d, %d, %.17g, %.17g}," % (c_literal(a), c_literal(b), "true" if by else "false",
                                                           l, o, d, c, j, w))
print("};")
sys.stderr.write("%d cases, %d differ from the tables\n" % (len(rows), bad))
