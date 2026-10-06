#!/usr/bin/env python3
# The vectors of tests/txt/catalog.cpp, answered by GNU gettext: random .po
# catalogs compiled by msgfmt and looked up through libintl
# (tools/gettext_oracle.c), and random plural rules evaluated by libintl
# from .mo files written here. Not part of the build; needs msgfmt and the
# oracle binary:
#
#   tools/gettext_vectors.py <gettext_oracle> <work dir> > tests/txt/catalog_vectors.h
import os
import random
import subprocess
import sys

R = random.Random(20261006)
ORACLE, WORK = sys.argv[1], sys.argv[2]
LOCALEDIR = os.path.join(WORK, "locale")
MESSAGES = os.path.join(LOCALEDIR, "xx", "LC_MESSAGES")
os.makedirs(MESSAGES, exist_ok=True)

RULES = [
    "nplurals=1; plural=0;",
    "nplurals=2; plural=n != 1;",
    "nplurals=2; plural=(n > 1);",
    "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2;",
    "nplurals=3; plural=n==1 ? 0 : n==2 ? 1 : 2;",
    "nplurals=3; plural=n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2;",
    "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && (n%100<10 || n%100>=20) ? 1 : 2;",
    "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;",
    "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2;",
    "nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);",
    "nplurals=4; plural=n%100==1 ? 0 : n%100==2 ? 1 : n%100==3 || n%100==4 ? 2 : 3;",
    "nplurals=6; plural=n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5;",
    "nplurals = 2 ;  plural = ( n != 1 ) ;",
]
NS = [0, 1, 2, 3, 4, 5, 7, 11, 12, 14, 21, 22, 25, 100, 101, 102, 111, 1000000, 4294967296, 18446744073709551615]
WORDS = ["Open", "Close", "Save", "file", "files", "item", "%d item", "%d items", "Ąę łódź", "日本語", "Ж", "a\nb",
         "tab\there", 'say "hi"', "back\\slash", "x", "menu", "dialog", "ﬁ", "😀", "trailing ", " leading",
         "multi\nline\ntext", "?", "''", "Ünïcödé"]


def rand_text(ends=(False, False)):
    # no line feed at either end unless asked (msgfmt wants a translation's ends to be its id's)
    k = R.random()
    if k < 0.6:
        t = R.choice(WORDS)
    elif k < 0.9:
        t = "".join(R.choice("abcdefghijklmnopqrstuvwxyz ĄŁŻ日本\n\t\"\\%") for _ in range(R.randint(1, 12)))
    else:
        t = R.choice(WORDS) + " " + R.choice(WORDS)
    t = t.strip("\n") or "y"
    return ("\n" if ends[0] else "") + t + ("\n" if ends[1] else "")


def po_units(s):
    # the escaped form of s as units: a character or a whole escape
    out = []
    for i, ch in enumerate(s):
        nxt = s[i + 1] if i + 1 < len(s) else ""
        if ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ord(ch) > 127 and R.random() < 0.15:
            out.extend("\\%03o" % b for b in ch.encode())
        elif ch.isalpha() and ord(ch) < 128 and nxt not in "0123456789abcdefABCDEF" and R.random() < 0.05:
            # (gettext's \x takes every hex digit after it: never one before a hex digit, which would make a byte
            # that is not UTF-8, and libintl hands back the id when its conversion fails)
            out.append("\\x%02x" % ord(ch))
        else:
            out.append(ch)
    return out


def po_string(keyword, s):
    units = po_units(s)
    if len(units) > 4 and R.random() < 0.3:
        pieces, cur = [], ""
        for u in units:
            cur += u
            if R.random() < 0.25:
                pieces.append(cur)
                cur = ""
        pieces.append(cur)
        return '%s ""\n' % keyword + "".join('"%s"\n' % p for p in pieces if p)
    return '%s "%s"\n' % (keyword, "".join(units))


def mark(t, tag):
    return t[:-1] + tag + "\n" if t.endswith("\n") else t + tag


def make_catalog(index):
    rule = R.choice(RULES) if R.random() < 0.85 else None
    nplurals = int(rule.split("=")[1].split(";")[0]) if rule else 2
    lines = []
    if True:   # msgfmt wants a charset for text past ASCII
        if R.random() < 0.3:
            lines.append("# Translator comment\n#, fuzzy\n")
        header = "Project-Id-Version: t %d\\n" % index
        header += "Content-Type: text/plain; charset=UTF-8\\n"
        if rule:
            header += "Plural-Forms: %s\\n" % rule
        header += "Language: xx\\n"
        lines.append('msgid ""\nmsgstr ""\n' + "".join('"%s\\n"\n' % f for f in header.split("\\n") if f) + "\n")
    keys = set()
    entries = []
    for _ in range(R.randint(3, 25)):
        ctx = None
        r = R.random()
        if r < 0.15:
            ctx = ""
        elif r < 0.4:
            ctx = rand_text()
        ends = (R.random() < 0.08, R.random() < 0.08)
        mid = rand_text(ends)
        if mid == "" or (ctx, mid) in keys:
            continue
        keys.add((ctx, mid))
        plural = rand_text(ends) if R.random() < 0.4 else None
        state = R.random()
        fuzzy = state < 0.1
        untranslated = 0.1 <= state < 0.18
        first_empty = 0.18 <= state < 0.22
        e = ""
        if R.random() < 0.2:
            e += "#. extracted\n#: src/main.c:%d\n" % R.randint(1, 999)
        flags = []
        if fuzzy:
            flags.append("fuzzy")
        if R.random() < 0.2:
            flags.append("c-format")
        if flags:
            R.shuffle(flags)
            e += "#, " + ", ".join(flags) + "\n"
        if R.random() < 0.05:
            e += '#| msgid "old"\n'
        if ctx is not None:
            e += po_string("msgctxt", ctx)
        e += po_string("msgid", mid)
        if plural is not None:
            e += po_string("msgid_plural", plural)
            count = nplurals if R.random() < 0.85 else R.randint(1, nplurals + 1)
            for k in range(count):
                s = "" if untranslated or (first_empty and k == 0) else mark(rand_text(ends), "/%d" % k)
                e += po_string("msgstr[%d]" % k, s)
        else:
            e += po_string("msgstr", "" if untranslated or first_empty else mark(rand_text(ends), "~"))
        if R.random() < 0.05:
            e += '\n#~ msgid "obsolete %d"\n#~ msgstr "przestarzałe"\n' % len(entries)
        entries.append((ctx, mid, plural))
        lines.append(e + ("\n" if R.random() < 0.9 else ""))
    text = "".join(lines)
    if R.random() < 0.2:
        text = text.replace("\n", "\r\n")
    return text, entries


def c_literal(b):
    units = []
    for x in b:
        if x == 0x22:
            units.append('\\"')
        elif x == 0x5C:
            units.append("\\\\")
        elif 0x20 <= x < 0x7F and x != 0x3F:
            units.append(chr(x))
        else:
            units.append("\\%03o" % x)
    pieces, cur = [], ""
    for u in units:
        cur += u
        if len(cur) >= 100:
            pieces.append(cur)
            cur = ""
    if cur or not pieces:
        pieces.append(cur)
    return "\n        ".join('"%s"' % p for p in pieces)


def esc_field(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def unesc(s):
    out, i = [], 0
    b = s.encode()
    res = bytearray()
    while i < len(b):
        if b[i] == 0x5C and i + 1 < len(b):
            c = chr(b[i + 1])
            if c == "t":
                res.append(9)
                i += 2
            elif c == "n":
                res.append(10)
                i += 2
            elif c == "x":
                res.append(int(b[i + 2:i + 4], 16))
                i += 4
            else:
                res.append(b[i + 1])
                i += 2
        else:
            res.append(b[i])
            i += 1
    return bytes(res)


def run_oracle(queries):
    inp = "".join("\t".join(esc_field(f) for f in q) + "\n" for q in queries)
    env = dict(os.environ, LANGUAGE="xx", LC_ALL="en_US.UTF-8")
    out = subprocess.run([ORACLE, LOCALEDIR], input=inp.encode(), capture_output=True, env=env, check=True).stdout
    return [unesc(line.decode("utf-8", "surrogateescape")) for line in out.split(b"\n")[:-1]]


cases, queries = [], []
for index in range(60):
    text, entries = make_catalog(index)
    domain = "c%d" % index
    po = os.path.join(WORK, domain + ".po")
    mo = os.path.join(MESSAGES, domain + ".mo")
    with open(po, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    r = subprocess.run(["msgfmt", "-o", mo, po], capture_output=True)
    if r.returncode != 0:
        sys.stderr.write("msgfmt refused %s: %s\n" % (po, r.stderr.decode()))
        continue
    with open(mo, "rb") as f:
        mobytes = f.read()
    cases.append((text.encode(), mobytes))
    c = len(cases) - 1
    qs = []
    probes = list(entries) + [(None, "missing %d" % k, "missings") for k in range(2)] + [("nope", entries[0][1] if entries else "x", None)]
    for ctx, mid, plural in probes:
        p = plural if plural is not None else "PLURAL"
        if ctx is None:
            qs.append((domain, "g", "", mid, "", "0"))
            for n in R.sample(NS, 4):
                qs.append((domain, "n", "", mid, p, str(n)))
        else:
            qs.append((domain, "p", ctx, mid, "", "0"))
            qs.append((domain, "g", "", mid, "", "0"))
            for n in R.sample(NS, 4):
                qs.append((domain, "np", ctx, mid, p, str(n)))
    answers = run_oracle(qs)
    for q, a in zip(qs, answers):
        queries.append((c, q, a))


# random plural rules, valid and not, evaluated by libintl from .mo files
def rand_expr(depth):
    k = R.random()
    if depth > 4 or k < 0.25:
        return R.choice(["n", "n", str(R.choice([0, 1, 2, 3, 4, 5, 10, 11, 19, 20, 100, 1000, 4294967296]))])
    if k < 0.35:
        return "!" + rand_expr(depth + 1)
    if k < 0.45:
        return "(" + rand_expr(depth + 1) + ")"
    if k < 0.55:
        return "%s ? %s : %s" % (rand_expr(depth + 1), rand_expr(depth + 1), rand_expr(depth + 1))
    op = R.choice(["*", "/", "%", "+", "-", "<", ">", "<=", ">=", "==", "!=", "&&", "||"])
    sp = R.choice(["", " "])
    return rand_expr(depth + 1) + sp + op + sp + rand_expr(depth + 1)


class DivZero(Exception):
    pass


def c_eval(expr, n):
    # a C evaluator over uint64 with gettext's grammar, to skip divisions by zero (a trap in libintl)
    toks, i = [], 0
    while i < len(expr):
        c = expr[i]
        if c in " \t":
            i += 1
            continue
        if expr[i:i + 2] in ("<=", ">=", "==", "!=", "&&", "||"):
            toks.append(expr[i:i + 2])
            i += 2
            continue
        if c.isdigit():
            j = i
            while j < len(expr) and expr[j].isdigit():
                j += 1
            toks.append(min(int(expr[i:j]), 2**64 - 1))
            i = j
            continue
        if c in ";\n":
            break
        toks.append(c)
        i += 1
    pos = [0]
    M = 2**64

    def peek():
        return toks[pos[0]] if pos[0] < len(toks) else None

    def take():
        t = peek()
        pos[0] += 1
        return t

    levels = [["||"], ["&&"], ["==", "!="], ["<", ">", "<=", ">="], ["+", "-"], ["*", "/", "%"]]

    def unary():
        t = take()
        if t == "!":
            f = unary()
            return lambda: int(not f())
        if t == "n":
            return lambda: n
        if isinstance(t, int):
            return lambda: t
        if t == "(":
            f = ternary()
            if take() != ")":
                raise SyntaxError
            return f
        raise SyntaxError

    def binary(level):
        if level == 6:
            return unary()
        f = binary(level + 1)
        while peek() in levels[level]:
            op = take()
            g = binary(level + 1)
            f = make(op, f, g)
        return f

    def make(op, f, g):
        def run():
            a = f()
            if op == "||":
                return 1 if a else int(bool(g()))
            if op == "&&":
                return 0 if not a else int(bool(g()))
            b = g()
            if op in "/%" and b == 0:
                raise DivZero
            return {"*": lambda: a * b % M, "/": lambda: a // b, "%": lambda: a % b, "+": lambda: (a + b) % M,
                    "-": lambda: (a - b) % M, "<": lambda: int(a < b), ">": lambda: int(a > b),
                    "<=": lambda: int(a <= b), ">=": lambda: int(a >= b), "==": lambda: int(a == b),
                    "!=": lambda: int(a != b)}[op]()
        return run

    def ternary():
        c = binary(0)
        if peek() == "?":
            take()
            y = ternary()
            if take() != ":":
                raise SyntaxError
            z = ternary()
            return lambda: y() if c() else z()
        return c

    try:
        f = ternary()
        if pos[0] != len(toks):
            raise SyntaxError
    except (SyntaxError, IndexError, TypeError):
        return None
    return f()


def write_mo(path, header):
    import struct
    keys = [b"", b"x\0xs"]
    vals = [header.encode(), b"\0".join(b"f%d" % k for k in range(10))]
    n = 2
    data = struct.pack("<7I", 0x950412de, 0, n, 28, 28 + 8 * n, 0, 28 + 16 * n)
    at = 28 + 16 * n
    otab, ttab, blob = b"", b"", b""
    for k in keys:
        otab += struct.pack("<2I", len(k), at + len(blob))
        blob += k + b"\0"
    for v in vals:
        ttab += struct.pack("<2I", len(v), at + len(blob))
        blob += v + b"\0"
    with open(path, "wb") as f:
        f.write(data + otab + ttab + blob)


rules = []
rule_queries = []
for k in range(400):
    expr = rand_expr(0)
    if R.random() < 0.1:   # damaged
        cut = R.randint(0, len(expr))
        expr = expr[:cut] + R.choice(["", "(", ")", "?", "=", "&", "#", "n n"]) + expr[cut + 1:]
    nplurals = R.randint(1, 8)
    header = "Content-Type: text/plain; charset=UTF-8\nPlural-Forms: nplurals=%d; plural=%s;\n" % (nplurals, expr)
    domain = "r%d" % k
    write_mo(os.path.join(MESSAGES, domain + ".mo"), header)
    for n in R.sample(NS, 6):
        try:
            c_eval(expr, n)
        except DivZero:
            continue
        except RecursionError:
            continue
        rule_queries.append((len(rules), (domain, "n", "", "x", "xs", str(n))))
    rules.append(header)
answers = run_oracle([q for _, q in rule_queries])
rule_vectors = []
for (r, q), a in zip(rule_queries, answers):
    a = a.decode()
    form = int(a[1:]) if a.startswith("f") else (-1 if a == "x" else -2)
    rule_vectors.append((r, int(q[5]), form))

print("//------------------------------------------------------------------------------")
print("// SGCL: a C++20 application platform")
print("// Copyright (c) 2022-2026 Sebastian Nibisz")
print("// SPDX-License-Identifier: Apache-2.0")
print("//------------------------------------------------------------------------------")
print("// Generated by tools/gettext_vectors.py from GNU gettext 1.0 (msgfmt, libintl):")
print("// do not edit. The .po catalogs, msgfmt's .mo of each, libintl's answers to")
print("// lookups in them, and libintl's form for random plural rules.")
print("#pragma once")
print()
print("#include <cstddef>")
print("#include <cstdint>")
print()
print("struct CatalogCase {")
print("    const char* po;")
print("    const char* mo;")
print("    size_t mo_size;")
print("};")
print()
print("inline const CatalogCase CatalogCases[] = {")
for po, mo in cases:
    print("    {%s,\n        %s,\n        %d}," % (c_literal(po), c_literal(mo), len(mo)))
print("};")
print()
print("// kind: g gettext, p pgettext, n ngettext, N npgettext")
print("struct CatalogQuery {")
print("    uint16_t c;")
print("    char kind;")
print("    const char* context;")
print("    const char* id;")
print("    const char* plural;")
print("    uint64_t n;")
print("    const char* answer;")
print("};")
print()
print("inline const CatalogQuery CatalogQueries[] = {")
for c, q, a in queries:
    kind = {"g": "g", "p": "p", "n": "n", "np": "N"}[q[1]]
    print("    {%d, '%s', %s, %s, %s, %sull, %s}," % (c, kind, c_literal(q[2].encode()), c_literal(q[3].encode()),
                                                c_literal(q[4].encode()), q[5], c_literal(a)))
print("};")
print()
print("// form: libintl's form of n, -1 the id handed back (\"x\")")
print("inline const char* const PluralRuleHeaders[] = {")
for h in rules:
    print("    %s," % c_literal(h.encode()))
print("};")
print()
print("struct PluralRuleCase {")
print("    uint16_t rule;")
print("    uint64_t n;")
print("    int form;")
print("};")
print()
print("inline const PluralRuleCase PluralRuleCases[] = {")
for r, n, form in rule_vectors:
    print("    {%d, %dull, %d}," % (r, n, form))
print("};")
sys.stderr.write("%d catalogs, %d queries, %d rules, %d rule cases\n" % (len(cases), len(queries), len(rules), len(rule_vectors)))
