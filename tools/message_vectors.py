#!/usr/bin/env python3
# The vectors of tests/txt/message_format.cpp: messages of ICU's
# MessageFormat answered by ICU 78 (tools/message_oracle.cpp) and by the
# library (tools/message_driver.cpp). A case goes into the vectors when
# both agree; the others are written to <work>/disagree.txt, to be read:
# what ICU 78 says from CLDR 48 the library says from CLDR 46. Not part of
# the build:
#
#   tools/message_vectors.py <message_oracle> <message_driver> <work dir> > tests/txt/message_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(20261006)
ORACLE, DRIVER, WORK = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(WORK, exist_ok=True)

LOCALES = ["en", "pl", "de", "fr", "ru", "ar", "ja", "cs", "cy", "he", "pt", "es", "it", "hi", "zh", "ko", "tr",
           "uk", "lt", "sl", "en-GB", "de-CH", "sr-Latn", "fa", "bn", "ga"]
INTS = [0, 1, 2, 3, 4, 5, 7, 11, 12, 21, 22, 25, 100, 101, 102, 111, 1000, 1234567, -1, -2, -5, -1000]
DOUBLES = [0.5, 1.0, 1.5, 2.25, -3.75, 1234.5678, 0.255, 1e10, 1e-7, 0.001, 99.995, 1.005, 12345678.9]
TEXTS = ["x", "abc", "ąę", "female", "male", "other", "one", ""]
DATES = [0, 1700000000000, -86400000, 1234567890123, 1767225600000, 951782400000]

NUMBER_STYLES = ["", ", integer", ", percent", ", currency", ", #,##0.00", ",#,##0.00", ", 0.###", ", #%", ",00",
                 ", #,##0.0#;(#)", ", ¤#,##0.00", ", '#'0", ", 0.00E0", ",#,##0'x'", ", -0.0", ", @@#",
                 ", ::percent", ", ::.00", ", ::currency/EUR", ", ::compact-short", ", ::compact-long",
                 ", ::group-off", ", ::sign-always", ", ::precision-integer", ", ::@@@", ", ::integer-width/+000",
                 ", ::scale/100", ", ::%x100", ", ::currency/JPY unit-width-iso-code",
                 ", ::unit-width-full-name currency/USD", ", ::rounding-mode-floor .0", ", ::.0# sign-except-zero",
                 ", ::K", ", ::currency/PLN .00 unit-width-narrow", ", ::permille", ", ::,_ .00", ", ::+! ."]
DATE_STYLES = ["", ", short", ", medium", ", long", ", full", ", ::yMMMd", ", ::jm", ", ::EEEE", ", yyyy-MM-dd",
               ", HH:mm", ",  d MMM y", ", ::yMMMMEEEEd", ", ::Hms"]
TEXT_BITS = ["it''s", "'{'", "'{x}'", "don't", "a'b", "'}'", "''{x}''", " ", "!", "ż", "a#b", "{}"[0:0], "‘q’"]


def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace(";", "\\;").replace("=", "\\=")


def arg_spec(args):
    parts = []
    for name, (t, v) in args.items():
        parts.append("%s=%s:%s" % (esc(name), t, esc(str(v))))
    return ";".join(parts)


def rand_number():
    return ("i", R.choice(INTS)) if R.random() < 0.6 else ("d", repr(R.choice(DOUBLES)))


def plural_cases(kind, depth):
    keys = ["one", "other"] if kind == "selectordinal" else ["zero", "one", "two", "few", "many", "other"]
    chosen = [k for k in keys if R.random() < 0.6 and k != "other"]
    out = []
    if R.random() < 0.4:
        out.append("=%d" % R.choice([0, 1, 2, 5]))
    out.extend(chosen)
    out.append("other")
    R.shuffle(out)
    return " ".join("%s {%s}" % (k, sub_message(depth + 1, True, k)) for k in out)


def sub_message(depth, in_plural, key=""):
    bits = []
    for _ in range(R.randint(1, 3)):
        r = R.random()
        if in_plural and r < 0.35:
            bits.append("#")
        elif r < 0.6:
            bits.append(R.choice(["file", "plik", key, "x y", "'#'" if in_plural else "q", R.choice(TEXT_BITS)]))
        elif depth < 2 and r < 0.75:
            bits.append("{g, select, female {she} male {he} other {they}}")
        elif depth < 2 and r < 0.85:
            bits.append("{m, plural, one {# m} other {# ms}}")
        else:
            bits.append("{s}")
    return " ".join(bits)


def random_case():
    loc = R.choice(LOCALES)
    args = {}
    parts = []
    for _ in range(R.randint(1, 4)):
        r = R.random()
        if r < 0.15:
            parts.append(R.choice(TEXT_BITS) or "x")
        elif r < 0.35:
            parts.append("{n, number%s}" % R.choice(NUMBER_STYLES))
            args["n"] = rand_number()
        elif r < 0.5:
            t = R.choice(["date", "time"])
            parts.append("{d, %s%s}" % (t, R.choice(DATE_STYLES)))
            args["d"] = ("m", R.choice(DATES))
        elif r < 0.75:
            off = " offset:1" if R.random() < 0.2 else ""
            parts.append("{n, plural,%s %s}" % (off, plural_cases("plural", 0)))
            args["n"] = rand_number()
        elif r < 0.85:
            parts.append("{o, selectordinal, %s}" % plural_cases("selectordinal", 0))
            args["o"] = ("i", R.choice(INTS))
        elif r < 0.95:
            parts.append("{g, select, female {she} male {he} other {they}}")
        else:
            parts.append("{s}")
    for name, gen in [("g", lambda: ("s", R.choice(TEXTS))), ("m", rand_number), ("s", lambda: ("s", R.choice(TEXTS)))]:
        if R.random() < 0.9:
            args[name] = gen()
    if R.random() < 0.05:
        args.pop(R.choice(list(args)), None)   # a missing argument
    return loc, " ".join(parts), args


FIXED = [
    ("en", "Hello, {name}!", {"name": ("s", "Ada")}),
    ("en", "{0} and {1}", {"0": ("s", "a"), "1": ("i", 5)}),
    ("en", "{n}", {"n": ("i", 1234567)}),
    ("de", "{n}", {"n": ("d", "1234.5678")}),
    ("en", "it's '{'literal'}' and ''", {}),
    ("en", "a '{b}' c '{'d", {}),
    ("en", "trailing '", {}),
    ("en", "lone } brace", {}),
    ("en", "{n, plural, offset:1 =0 {nobody} =1 {only {who}} one {{who} and # other} other {{who} and # others}}",
     {"n": ("i", 3), "who": ("s", "Ann")}),
    ("en", "{n, plural, offset:1 =0 {nobody} =1 {only {who}} one {{who} and # other} other {{who} and # others}}",
     {"n": ("i", 2), "who": ("s", "Ann")}),
    ("en", "{n, plural, one {'#' is #} other {'#' are #}}", {"n": ("i", 5)}),
    ("en", "{n, plural, other {{g, select, other {# stays #}}}}", {"n": ("i", 5), "g": ("s", "x")}),
    ("en", "{n,plural,one{a}other{b}}", {"n": ("i", 1)}),
    ("en", "{ n , number , integer }", {"n": ("d", "2.5")}),
    ("en", "{ n , number , integer }", {"n": ("d", "3.5")}),
    ("en", "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", {"n": ("i", 23)}),
    ("en", "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", {"n": ("i", 111)}),
    ("cy", "{n, plural, zero {z} one {o} two {t} few {f} many {m} other {x}}", {"n": ("i", 6)}),
    ("ar", "{n, plural, zero {z} one {o} two {t} few {f} many {m} other {x}} #", {"n": ("i", 103)}),
    ("pl", "{n, plural, one {# plik} few {# pliki} many {# plików} other {# pliku}}", {"n": ("d", "1.5")}),
    ("en", "{n, plural, one {one} other {other}}", {"n": ("d", "1.0")}),
    ("en", "{n, plural, one {one} other {other}}", {"n": ("d", "1.0001")}),
    ("en", "{n, plural, =1.5 {exact} other {#}}", {"n": ("d", "1.5")}),
    ("en", "{missing} and {n, plural, other {#}}", {}),
    ("en", "{n, number, ::currency/EUR}", {"n": ("d", "-1234.5")}),
    ("fr", "{n, number, percent}", {"n": ("d", "0.256")}),
    ("en", "{d, date, full} {d, time, full}", {"d": ("m", 1700000000000)}),
    ("en", "{d, date, ::yMMMd}", {"d": ("s", "2026-10-06T14:05:00+02:00")}),
    ("en", "{d, date, 'week' w, yyyy}", {"d": ("m", 1700000000000)}),
    ("en", "{s, select, a {A} other {O}}", {"s": ("i", 5)}),
    ("en", "{n, number}", {"n": ("s", "abc")}),
    ("en", "", {}),
    # refused
    ("en", "{", {}), ("en", "{n", {}), ("en", "{n,}", {}), ("en", "{n, foo}", {}), ("en", "{n, plural, one {x}}", {}),
    ("en", "{n, select}", {}), ("en", "{01}", {}), ("en", "{n, number, ::bogus}", {}),
    ("en", "{n, plural, offset:1}", {}), ("en", "{n, plural, =x {a} other {b}}", {}),
    ("en", "{n, plural, one {a} offset:1 other {b}}", {}), ("en", "{n, plural, other {a}", {}),
    ("en", "{n, select, other a}", {}), ("en", "{a b}", {}), ("en", "{n, plural, other {a}} }", {}),
    ("en", "{, number}", {}), ("en", "{n, date, ::}", {}),
]

cases = list(FIXED)
seen = set()
while len(cases) < 4000:
    loc, pattern, args = random_case()
    key = (loc, pattern, arg_spec(args))
    if key in seen:
        continue
    seen.add(key)
    cases.append((loc, pattern, args))

lines = "".join("%s\t%s\t%s\n" % (esc(l), esc(p), arg_spec(a)) for l, p, a in cases)
env = dict(os.environ, DYLD_LIBRARY_PATH="/opt/homebrew/opt/icu4c/lib")
icu = subprocess.run([ORACLE], input=lines.encode(), capture_output=True, env=env, check=True).stdout.decode().split("\n")
ours = subprocess.run([DRIVER], input=lines.encode(), capture_output=True, check=True).stdout.decode().split("\n")


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


agree, disagree = [], []
for (l, p, a), x, y in zip(cases, icu, ours):
    xi, yi = x.startswith("ERROR"), y.startswith("ERROR")
    if xi and yi:
        agree.append((l, p, a, None))
    elif not xi and not yi and x == y:
        agree.append((l, p, a, unesc(x)))
    else:
        disagree.append((l, p, arg_spec(a), x, y))

with open(os.path.join(WORK, "disagree.txt"), "w") as f:
    for d in disagree:
        f.write("%s | %s | %s\n  icu  %s\n  ours %s\n" % d)


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
print("// Generated by tools/message_vectors.py from ICU 78's MessageFormat: do not edit.")
print("// Messages, their arguments (tests/txt/message_args.h) and what ICU writes;")
print("// answer null where ICU refuses the message.")
print("#pragma once")
print()
print("struct MessageVector {")
print("    const char* locale;")
print("    const char* pattern;")
print("    const char* args;")
print("    const char* answer;")
print("};")
print()
print("inline const MessageVector MessageVectors[] = {")
for l, p, a, ans in agree:
    print("    {%s, %s, %s, %s}," % (c_literal(l), c_literal(p), c_literal(arg_spec(a)),
                                    "nullptr" if ans is None else c_literal(ans)))
print("};")
sys.stderr.write("%d cases: %d agree, %d disagree (%s)\n" % (len(cases), len(agree), len(disagree),
                                                             os.path.join(WORK, "disagree.txt")))
