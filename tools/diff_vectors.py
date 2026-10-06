#!/usr/bin/env python3
# The checks and vectors of tests/txt/diff.cpp, against the tools of the
# system (tools/diff_driver.cpp answers for the library):
#   - edit scripts rebuild the new text, and Myers' are shortest (the LCS by a
#     dynamic program here);
#   - unified_diff applied by /usr/bin/patch gives the new text;
#   - apply_patch of diff -u output (the text shifted and its context altered,
#     to force offsets and fuzz) answers what /usr/bin/patch answers;
#   - merge3 answers what git merge-file answers (its markers and --diff3).
# Prints a summary on stderr and the vectors (patch and merge cases where the
# library and the tools agree) on stdout:
#
#   tools/diff_vectors.py <diff_driver> <work dir> > tests/txt/diff_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(int(os.environ.get("SEED", "20261006")))
DRIVER, WORK = sys.argv[1], sys.argv[2]
os.makedirs(WORK, exist_ok=True)
N = int(os.environ.get("N", "400"))


def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def unesc(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            c = s[i + 1]
            out.append("\t" if c == "t" else "\n" if c == "n" else c)
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def ask(questions):
    data = "".join("\t".join(esc(f) for f in q) + "\n" for q in questions).encode()
    out = subprocess.run([DRIVER], input=data, capture_output=True, check=True).stdout.decode()
    return [unesc(l) for l in out.split("\n")[:-1]]


WORDS = ["alpha", "beta", "gamma", "delta", "x", "y", "", "  indented", "}", "{", "return 0;", "ąę"]


def text(n):
    lines = [R.choice(WORDS) for _ in range(n)]
    t = "\n".join(lines)
    if R.random() < 0.85:
        t += "\n"
    return t


def mutate(t):
    lines = t.split("\n")
    for _ in range(R.randint(0, 5)):
        op = R.random()
        k = R.randint(0, len(lines))
        if op < 0.4 and lines:
            k = min(k, len(lines) - 1)
            lines[k] = R.choice(WORDS) + R.choice(["", "!", " 2"])
        elif op < 0.7:
            lines.insert(k, R.choice(WORDS))
        elif lines:
            del lines[min(k, len(lines) - 1)]
    out = "\n".join(lines)
    if R.random() < 0.1:
        out = out.rstrip("\n")
    return out


def lcs(a, b):
    n, m = len(a), len(b)
    prev = [0] * (m + 1)
    for i in range(n):
        cur = [0] * (m + 1)
        for j in range(m):
            cur[j + 1] = prev[j] + 1 if a[i] == b[j] else max(prev[j + 1], cur[j])
        prev = cur
    return prev[m]


def units_lines(t):
    return t.splitlines(keepends=True)


pairs = []
for _ in range(N):
    a = text(R.randint(0, 40))
    b = mutate(a) if R.random() < 0.8 else text(R.randint(0, 40))
    pairs.append((a, b))

problems = 0
# 1. edit scripts
for alg in ["myers", "patience"]:
    answers = ask([("E", a, b, alg) for a, b in pairs])
    for (a, b), ans in zip(pairs, answers):
        edits = [tuple(int(x) for x in e.split(",")) for e in ans.split(";") if e]
        A, B = a.encode(), b.encode()   # the edits are byte ranges
        rebuilt = b""
        pa = pb = 0
        ok = True
        for kind, ob, oe, nb, ne in edits:
            if ob != pa or nb != pb:
                ok = False
            if kind == 0:
                if A[ob:oe] != B[nb:ne]:
                    ok = False
                rebuilt += A[ob:oe]
            elif kind == 1:
                rebuilt += B[nb:ne]
            pa, pb = oe, ne
        if rebuilt != B or pa != len(A) or pb != len(B):
            ok = False
        la, lb = units_lines(a), units_lines(b)
        if alg == "myers":
            changed = sum(len(units_lines(A[ob:oe].decode())) for k, ob, oe, nb, ne in edits if k == 2) + \
                sum(len(units_lines(B[nb:ne].decode())) for k, ob, oe, nb, ne in edits if k == 1)
            if changed != len(la) + len(lb) - 2 * lcs(la, lb):
                ok = False
        if not ok:
            problems += 1
            sys.stderr.write("edit script wrong (%s): %r %r -> %s\n" % (alg, a, b, ans))

# 2. unified_diff applied by /usr/bin/patch
answers = ask([("U", a, b, R.choice(["myers", "patience"])) for a, b in pairs])
for k, ((a, b), patch) in enumerate(zip(pairs, answers)):
    if patch == "":
        if a != b:
            problems += 1
            sys.stderr.write("empty diff of different texts: %r %r\n" % (a, b))
        continue
    fa = os.path.join(WORK, "a%d" % k)
    open(fa, "w").write(a)
    fp = os.path.join(WORK, "p%d" % k)
    open(fp, "w").write(patch)
    r = subprocess.run(["patch", "-s", "-o", fa + ".out", fa, fp], capture_output=True)
    got = open(fa + ".out").read() if os.path.exists(fa + ".out") else None
    if r.returncode != 0 or got != b:
        problems += 1
        sys.stderr.write("patch(1) did not apply our diff: %r %r\n%s\n%s\n" % (a, b, patch, r.stderr.decode()))

# 3. apply_patch of diff -u output, the text moved and altered, against patch(1)
patch_cases = []
for k, (a, b) in enumerate(pairs):
    fa, fb = os.path.join(WORK, "ua%d" % k), os.path.join(WORK, "ub%d" % k)
    open(fa, "w").write(a)
    open(fb, "w").write(b)
    patch = subprocess.run(["diff", "-u", "--label", "a", "--label", "b", fa, fb], capture_output=True).stdout.decode()
    if not patch:
        continue
    target = a
    r = R.random()
    if r < 0.3:   # lines added in front: an offset
        target = "".join(R.choice(WORDS) + "\n" for _ in range(R.randint(1, 4))) + a
    elif r < 0.5:  # a context line changed: fuzz
        ls = a.split("\n")
        if len(ls) > 2:
            ls[R.randint(0, len(ls) - 1)] = "changed"
        target = "\n".join(ls)
    fuzz = R.choice([0, 1, 2, 2, 3])
    reverse = R.random() < 0.15
    t = os.path.join(WORK, "t%d" % k)
    open(t, "w").write(b if reverse else target)
    fp = os.path.join(WORK, "up%d" % k)
    open(fp, "w").write(patch)
    if os.path.exists(t + ".out"):
        os.unlink(t + ".out")
    args = ["patch", "-s", "-F", str(fuzz), "-o", t + ".out", "-r", t + ".rej"]
    if reverse:
        args.append("-R")
    rr = subprocess.run(args + [t, fp], capture_output=True)
    want = open(t + ".out").read() if rr.returncode == 0 and os.path.exists(t + ".out") else None
    patch_cases.append((b if reverse else target, patch, fuzz, reverse, want))
answers = ask([("P", t, p, str(f), "1" if rv else "0") for t, p, f, rv, w in patch_cases])
patch_agree = []
patch_diff = 0
for (t, p, f, rv, want), got in zip(patch_cases, answers):
    ours = None if got.startswith("ERROR") else got
    if ours == want:
        patch_agree.append((t, p, f, rv, want))
    else:
        patch_diff += 1
        if patch_diff <= 10:
            sys.stderr.write("apply_patch differs from patch(1): fuzz %d reverse %s\n--- text\n%s--- patch\n%s--- patch(1)\n%r\n--- ours\n%r\n"
                             % (f, rv, t, p, want, ours))

# 4. merge3 against git merge-file (its default markers, and --diff3)
merge_cases = []
for k in range(N):
    base = text(R.randint(0, 30))
    ours = mutate(base)
    theirs = mutate(base) if R.random() < 0.8 else ours
    merge_cases.append((base, ours, theirs, R.random() < 0.5))
answers = ask([("M" if d3 else "N", b, o, t) for b, o, t, d3 in merge_cases])
merge_agree = []
merge_diff = 0
for k, ((base, ours, theirs, d3), got) in enumerate(zip(merge_cases, answers)):
    fo, fb, ft = [os.path.join(WORK, "%s%d" % (n, k)) for n in ("mo", "mb", "mt")]
    open(fo, "w").write(ours)
    open(fb, "w").write(base)
    open(ft, "w").write(theirs)
    args = ["git", "merge-file", "-p", "-L", "ours", "-L", "base", "-L", "theirs"] + (["--diff3"] if d3 else [])
    r = subprocess.run(args + [fo, fb, ft], capture_output=True)
    want = r.stdout.decode()
    conflicts, _, text_ours = got.partition("|")
    if text_ours == want and int(conflicts) == r.returncode:
        merge_agree.append((base, ours, theirs, d3, int(conflicts), want))
    else:
        merge_diff += 1
        if merge_diff <= 10:
            sys.stderr.write("merge3 differs from git merge-file%s (%s vs %d conflicts):\n--- base\n%s--- ours\n%s--- theirs\n%s--- git\n%s--- ours\n%s\n"
                             % (" --diff3" if d3 else "", conflicts, r.returncode, base, ours, theirs, want, text_ours))


def c_literal(s):
    out = ['"']
    for ch in s.encode():
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
print("// Generated by tools/diff_vectors.py: do not edit. Patches of diff -u applied")
print("// by /usr/bin/patch (result null where it refuses), merges of git merge-file.")
print("#pragma once")
print()
print("#include <cstddef>")
print()
print("struct PatchVector {")
print("    const char* text;")
print("    const char* patch;")
print("    unsigned fuzz;")
print("    bool reverse;")
print("    const char* result;")
print("};")
print()
print("inline const PatchVector PatchVectors[] = {")
for t, p, f, rv, w in patch_agree:
    print("    {%s, %s, %d, %s, %s}," % (c_literal(t), c_literal(p), f, "true" if rv else "false",
                                      "nullptr" if w is None else c_literal(w)))
print("};")
print()
print("struct MergeVector {")
print("    const char* base;")
print("    const char* ours;")
print("    const char* theirs;")
print("    bool diff3;")
print("    unsigned conflicts;")
print("    const char* merged;   // git merge-file's, labels ours, base, theirs")
print("};")
print()
print("inline const MergeVector MergeVectors[] = {")
for b, o, t, d3, c, w in merge_agree:
    print("    {%s, %s, %s, %s, %d, %s}," % (c_literal(b), c_literal(o), c_literal(t), "true" if d3 else "false", c,
                                          c_literal(w)))
print("};")
sys.stderr.write("edit/unified problems %d; patches %d agree, %d differ; merges %d agree, %d differ\n"
                 % (problems, len(patch_agree), patch_diff, len(merge_agree), merge_diff))
