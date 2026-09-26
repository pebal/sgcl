#!/usr/bin/env python3
# Generates the Unicode tables of the library. Run from the root of the
# repository:  python3 tools/unicode_tables.py
#
#   sgcl/core/detail/unicode_tables.h     the simple case mappings
#   sgcl/txt/detail/property_enums.h     enum category, enum script
#   sgcl/txt/detail/property_tables.h    the general category, the scripts,
#                                         Extended_Pictographic, the decimal
#                                         digits, the wide code points
#
# Two sources, one version. What Python's unicodedata answers (the general
# category, the case mappings, East_Asian_Width) is taken from it; what it
# does not have (the scripts, the emoji properties, and the break
# properties of the text module's segmentation, to come) is read from the
# UCD files, which are downloaded once into .ucd/<version>/ (gitignored)
# and asserted to declare the same version as unicodedata — otherwise
# "one source" would drift apart the day the Python changes.
import bisect
import collections
import itertools
import functools
import json
import os
import re
import sys
import unicodedata
import urllib.request
import zipfile
from fractions import Fraction

VERSION = unicodedata.unidata_version
UCD = f"https://www.unicode.org/Public/{VERSION}/ucd/"
CACHE = os.path.join(".ucd", VERSION)
HEADER = """//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once
"""

#------------------------------------------------------------------------------
# The UCD files
#------------------------------------------------------------------------------
def ucd(name):
    # The file's lines, from the cache or from unicode.org; its header must
    # name this Unicode version, as "Scripts-16.0.0.txt" or "Version: 16.0"
    path = os.path.join(CACHE, name.replace("/", "-"))
    if not os.path.exists(path):
        os.makedirs(CACHE, exist_ok=True)
        print(f"  fetching {UCD}{name}", file=sys.stderr)
        with urllib.request.urlopen(UCD + name, timeout=30) as r:
            data = r.read().decode("utf-8")
        with open(path, "w") as f:
            f.write(data)
    text = open(path).read()
    head = "\n".join(text.split("\n")[:12])
    major_minor = ".".join(VERSION.split(".")[:2])
    # "Scripts-16.0.0.txt" in the UCD proper, "Emoji Version 16.0" in the
    # emoji files, which carry the emoji version rather than the file's name
    assert f"-{VERSION}.txt" in head or f"Version: {major_minor}" in head or f"Version {major_minor}" in head, \
        f"{name} does not declare Unicode {VERSION}: {head.splitlines()[0]}"
    return text.split("\n")

def properties(name, wanted=None):
    # A UCD property file as {value: [(lo, hi), ...]}: lines of the form
    # "0041..005A ; Latin # comment", a single code point without the ".."
    out = {}
    for line in ucd(name):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 2 or (wanted is not None and fields[1] not in wanted):
            continue
        points = fields[0].split("..")
        lo = int(points[0], 16)
        hi = int(points[-1], 16)
        out.setdefault(fields[1], []).append((lo, hi))
    for value in out:
        out[value] = coalesce(sorted(out[value]))
    return out

def derived(name, prop):
    # A derived file's lines carry the property before the value:
    # "0300..034E ; InCB; Extend # ..." — {value: [(lo, hi), ...]}
    out = {}
    for line in ucd(name):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 3 or fields[1] != prop:
            continue
        points = fields[0].split("..")
        out.setdefault(fields[2], []).append((int(points[0], 16), int(points[-1], 16)))
    for value in out:
        out[value] = coalesce(sorted(out[value]))
    return out

def mapping_file(path):
    # A file of the MAPPINGS directory: "0x80\t0x20AC\t# EURO SIGN". The
    # same site as the UCD, a different tree, and no version to assert —
    # these tables have not changed since the nineties.
    url = "https://www.unicode.org/Public/MAPPINGS/" + path
    local = os.path.join(CACHE, "MAPPINGS-" + path.replace("/", "-"))
    if not os.path.exists(local):
        os.makedirs(CACHE, exist_ok=True)
        print(f"  fetching {url}", file=sys.stderr)
        with urllib.request.urlopen(url, timeout=30) as r:
            data = r.read().decode("utf-8", "replace")
        with open(local, "w") as f:
            f.write(data)
    out = {}
    for line in open(local).read().split("\n"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = line.split()
        if len(fields) < 2 or not fields[1].lower().startswith("0x"):
            continue        # a byte with no character of its own
        out[int(fields[0], 16)] = int(fields[1], 16)
    return out

def ucd_uca(name):
    # The files of the Unicode Collation Algorithm live in a tree of their
    # own, beside the UCD and versioned with it
    path = os.path.join(CACHE, "UCA-" + name)
    if not os.path.exists(path):
        os.makedirs(CACHE, exist_ok=True)
        url = f"https://www.unicode.org/Public/UCA/{VERSION}/{name}"
        print(f"  fetching {url}", file=sys.stderr)
        with urllib.request.urlopen(url, timeout=60) as r:
            open(path, "w").write(r.read().decode("utf-8"))
    text = open(path).read()
    assert f"@version {VERSION}" in text or VERSION in text.split("\n")[0], name
    return text.split("\n")

def properties_of(name, prop):
    # The ranges of one property of a file whose lines are
    # "0041..005A ; Cased # ..."
    out = []
    for line in ucd(name):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 2 or fields[1] != prop:
            continue
        points = fields[0].split("..")
        out.append((int(points[0], 16), int(points[-1], 16)))
    return coalesce(sorted(out))

def break_tests(name):
    # A UCD break test file as [(code points, breaks)], where breaks[i] is
    # '1' when a boundary falls before the i-th code point and breaks[n]
    # closes the text: "÷ 0020 × 0308 ÷" is ([0x20, 0x308], "101")
    out = []
    for line in ucd(name):
        line = line.split("#")[0].strip()
        if not line:
            continue
        points, marks = [], ""
        for token in line.split():
            if token == "\u00f7":
                marks += "1"
            elif token == "\u00d7":
                marks += "0"
            else:
                points.append(int(token, 16))
        assert len(marks) == len(points) + 1, line
        out.append((points, marks))
    return out

def emit_break_tests(f, name, cases):
    f.write(f"    inline constexpr BreakCase {name}[] = {{\n")
    for points, marks in cases:
        text = "".join(f"\\U{c:08X}" for c in points)
        f.write(f'        {{U"{text}", "{marks}"}},\n')
    f.write("    };\n")

#------------------------------------------------------------------------------
# Ranges
#------------------------------------------------------------------------------
def coalesce(rs):
    # Adjacent and overlapping ranges joined
    out = []
    for lo, hi in rs:
        if out and lo <= out[-1][1] + 1:
            out[-1] = (out[-1][0], max(out[-1][1], hi))
        else:
            out.append((lo, hi))
    return out

def value_ranges(value_of, default):
    # [(lo, hi, value)] over the code points whose value is not the default
    out = []
    for c in range(0x110000):
        v = value_of(c)
        if v == default:
            continue
        if out and out[-1][1] == c - 1 and out[-1][2] == v:
            out[-1] = (out[-1][0], c, v)
        else:
            out.append((c, c, v))
    return out

def from_sets(sets, order, default):
    # The {value: [(lo, hi)]} of a property file as one sorted table
    out = []
    for value, rs in sets.items():
        if value == default:
            continue
        for lo, hi in rs:
            out.append((lo, hi, order[value]))
    out.sort()
    joined = []
    for lo, hi, v in out:
        if joined and joined[-1][1] == lo - 1 and joined[-1][2] == v:
            joined[-1] = (joined[-1][0], hi, v)
        else:
            joined.append((lo, hi, v))
    return joined

def check_values(table, value_of, default):
    # Every code point answers what the table says, and the code points
    # outside every range answer the default
    seen = {}
    for lo, hi, v in table:
        for c in range(lo, hi + 1):
            seen[c] = v
    for c in range(0x110000):
        assert seen.get(c, default) == value_of(c), f"{hex(c)}: {seen.get(c, default)} != {value_of(c)}"

# Every table is split at the end of the Basic Multilingual Plane: the
# ranges below U+10000 need sixteen bits a bound, not thirty-two, which
# halves the entry, and most of the ranges of every property are there.
# A range that straddles the boundary is cut in two, so neither search
# ever looks at the other table. The sizes are what the compiler lays
# out, padding and all.
BMP = 0x10000
ENTRY = {"Range16": 4, "Range32": 8, "ValueRange16": 6, "ValueRange32": 12,
         "CaseRange16": 12, "CaseRange32": 16}

def split(table):
    # [(lo, hi, ...)] -> (the entries below BMP, the entries above it)
    bmp, high = [], []
    for r in table:
        lo, hi, rest = r[0], r[1], r[2:]
        if lo < BMP:
            bmp.append((lo, min(hi, BMP - 1)) + rest)
        if hi >= BMP:
            high.append((max(lo, BMP), hi) + rest)
    return bmp, high

def emit_one(f, kind, name, table, pad):
    if not table:
        return "nullptr, 0"
    f.write(f"{pad}inline constexpr {kind} {name}[] = {{\n")
    for r in table:
        fields = ", ".join(f"0x{x:04X}" if i < 2 else str(x) for i, x in enumerate(r))
        f.write(f"{pad}    {{{fields}}},\n")
    f.write(f"{pad}}};\n")
    return f"{name}, {len(table)}"

_TRIE_CACHE = {}

def trie_size(table, runs=False):
    bmp, high = split(table)
    return trie_of(bmp, runs)[0] + len(high) * ENTRY["ValueRange32"]

def trie_of(pairs, runs):
    # the two-stage form of a table's Basic Multilingual Plane: the block
    # size that comes out smallest, the index, the blocks, and how wide
    # each of them has to be
    key = (tuple(map(tuple, pairs)), runs)
    if key in _TRIE_CACHE:
        return _TRIE_CACHE[key]
    values = [0] * BMP
    for row in pairs:
        lo, hi, value = row[0], row[1], row[2]
        for c in range(lo, min(hi, BMP - 1) + 1):
            values[c] = value + (c - lo) if runs else value
    best = None
    for shift in range(3, 9):
        block = 1 << shift
        seen, index = {}, []
        for base in range(0, BMP, block):
            key = tuple(values[base:base + block])
            if key not in seen:
                seen[key] = len(seen)
            index.append(seen[key])
        blocks = [0] * (len(seen) * block)
        for key, at in seen.items():
            blocks[at * block:(at + 1) * block] = key
        value_bytes = 1 if max(values) < 256 else 2
        index_bytes = 1 if len(seen) < 256 else 2
        size = len(index) * index_bytes + len(blocks) * value_bytes
        if best is None or size < best[0]:
            best = (size, shift, index, blocks, value_bytes, index_bytes)
    _TRIE_CACHE[key] = best
    return best

def emit_trie(f, name, table, comment="", indent=8, runs=False):
    # the plane where the text is, as a two-stage table; everything above
    # it as the ranges it always was
    pad = " " * indent
    bmp, high = split(table)
    size, shift, index, blocks, value_bytes, index_bytes = trie_of(bmp, runs)
    if runs:
        assert all(v for _, _, v in bmp), f"{name}: zero is the value that means absent"
    value_type = f"uint{8 * value_bytes}_t"
    index_type = f"uint{8 * index_bytes}_t"
    f.write(f"{pad}// {comment}\n" if comment else "")
    f.write(f"{pad}// {len(index)} blocks of {1 << shift}, {len(blocks) >> shift} of them different\n")
    for array, kind, rows in ((name + "Index", index_type, index), (name + "Blocks", value_type, blocks)):
        f.write(f"{pad}inline constexpr {kind} {array}[] = {{\n")
        for i in range(0, len(rows), 16):
            f.write(pad + "    " + " ".join(f"{v}," for v in rows[i:i + 16]) + "\n")
        f.write(f"{pad}}};\n")
    a = emit_one(f, "ValueRange32", name + "High", high, pad)
    f.write(f"{pad}inline constexpr Table<{value_type}, {index_type}, {shift}, "
            f"{'true' if runs else 'false'}> {name}"
            f"{{{name}Index, {name}Blocks, {a}}};\n")
    return size + len(high) * ENTRY["ValueRange32"]

def emit_bits(f, name, table, comment="", indent=8):
    # a set as bits, the same two stages as a table of values
    pad = " " * indent
    bmp, high = split(table)
    bits = bytearray(BMP)
    for lo, hi in ((r[0], r[1]) for r in bmp):
        for c in range(lo, min(hi, BMP - 1) + 1):
            bits[c] = 1
    best = None
    for shift in range(6, 11):
        block = 1 << shift
        seen, index = {}, []
        for base in range(0, BMP, block):
            key = bytes(bits[base:base + block])
            if key not in seen:
                seen[key] = len(seen)
            index.append(seen[key])
        index_bytes = 1 if len(seen) < 256 else 2
        size = len(index) * index_bytes + len(seen) * block // 8
        if best is None or size < best[0]:
            best = (size, shift, index, seen, index_bytes)
    size, shift, index, seen, index_bytes = best
    words = []
    for key in sorted(seen, key=seen.get):
        for i in range(0, len(key), 64):
            words.append(sum(1 << k for k, b in enumerate(key[i:i + 64]) if b))
    index_type = f"uint{8 * index_bytes}_t"
    f.write(f"{pad}// {comment}\n" if comment else "")
    f.write(f"{pad}// {len(index)} blocks of {1 << shift} bits, {len(seen)} of them different\n")
    f.write(f"{pad}inline constexpr {index_type} {name}Index[] = {{\n")
    for i in range(0, len(index), 24):
        f.write(pad + "    " + " ".join(f"{v}," for v in index[i:i + 24]) + "\n")
    f.write(f"{pad}}};\n")
    f.write(f"{pad}inline constexpr uint64_t {name}Blocks[] = {{\n")
    for i in range(0, len(words), 4):
        f.write(pad + "    " + " ".join(f"0x{w:016X}," for w in words[i:i + 4]) + "\n")
    f.write(f"{pad}}};\n")
    a = emit_one(f, "Range32", name + "High", high, pad)
    f.write(f"{pad}inline constexpr Set<{index_type}, {shift}> {name}"
            f"{{{name}Index, {name}Blocks, {a}}};\n")
    return size + len(high) * ENTRY["Range32"]

def emit_split(f, kind, name, table, comment="", indent=8):
    # kind is "Range", "ValueRange" or "CaseRange"; the two arrays and the
    # little structure that binds them, which is what the callers name
    pad = " " * indent
    bmp, high = split(table)
    f.write(f"{pad}// {comment}\n" if comment else "")
    a = emit_one(f, kind + "16", name + "Bmp", bmp, pad)
    b = emit_one(f, kind + "32", name + "High", high, pad)
    holder = {"Range": "Set", "ValueRange": "RangeTable", "CaseRange": "CaseTable"}[kind]
    f.write(f"{pad}inline constexpr {holder} {name}{{{a}, {b}}};\n")
    return len(bmp) * ENTRY[kind + "16"] + len(high) * ENTRY[kind + "32"]

def size_of(kind, table):
    bmp, high = split(table)
    return len(bmp) * ENTRY[kind + "16"] + len(high) * ENTRY[kind + "32"]

#------------------------------------------------------------------------------
# core: the simple case mappings
#------------------------------------------------------------------------------
# A simple mapping is one code point to one code point (what Go's
# unicode.ToLower and Java's Character.toLowerCase do); the full mappings
# that change the length (ß to SS) and the ones that depend on a language
# are not in the tables. A range is [lo, hi] with a delta, applied to every
# code point of the range (stride 1) or to every second one (stride 2:
# the alphabets where upper and lower alternate, Ǆ ǅ ǆ, Ā ā Ă ă).
SIMPLE_LOWER = {0x0130: 0x0069}   # İ → i (the full mapping adds U+0307)
SIMPLE_UPPER = {0x00DF: 0x00DF, 0x0149: 0x0149, 0x01F0: 0x01F0, 0x0390: 0x0390, 0x03B0: 0x03B0, 0x0587: 0x0587,
                0x1E96: 0x1E96, 0x1E97: 0x1E97, 0x1E98: 0x1E98, 0x1E99: 0x1E99, 0x1E9A: 0x1E9A, 0x1F50: 0x1F50,
                0x1F52: 0x1F52, 0x1F54: 0x1F54, 0x1F56: 0x1F56, 0x1FB6: 0x1FB6, 0x1FC6: 0x1FC6, 0x1FD2: 0x1FD2,
                0x1FD3: 0x1FD3, 0x1FD6: 0x1FD6, 0x1FD7: 0x1FD7, 0x1FE2: 0x1FE2, 0x1FE3: 0x1FE3, 0x1FE4: 0x1FE4,
                0x1FE6: 0x1FE6, 0x1FE7: 0x1FE7, 0x1FF6: 0x1FF6, 0xFB00: 0xFB00, 0xFB01: 0xFB01, 0xFB02: 0xFB02,
                0xFB03: 0xFB03, 0xFB04: 0xFB04, 0xFB05: 0xFB05, 0xFB06: 0xFB06, 0xFB13: 0xFB13, 0xFB14: 0xFB14,
                0xFB15: 0xFB15, 0xFB16: 0xFB16, 0xFB17: 0xFB17}
# The titlecase-only exceptions where full upper is one char but not the simple mapping:
# the Greek with ypogegrammeni (ᾳ U+1FB3 → full ΑΙ, simple ᾼ U+1FBC) and the like
SIMPLE_UPPER.update({0x1FB3: 0x1FBC, 0x1FC3: 0x1FCC, 0x1FF3: 0x1FFC, 0x1FBC: 0x1FBC, 0x1FCC: 0x1FCC, 0x1FFC: 0x1FFC, 0x1FB2: 0x1FB2, 0x1FB4: 0x1FB4, 0x1FC2: 0x1FC2,
                     0x1FC4: 0x1FC4, 0x1FF2: 0x1FF2, 0x1FF4: 0x1FF4, 0x1FB7: 0x1FB7, 0x1FC7: 0x1FC7, 0x1FF7: 0x1FF7})
for c in range(0x1F80, 0x1FB0):   # ᾀ..ᾯ: the pairs with ypogegrammeni, simple upper is the titlecase form
    if (c & 8):
        SIMPLE_UPPER[c] = c
    else:
        SIMPLE_UPPER[c] = c + 8

def simple(c, full, override):
    if c in override:
        return override[c]
    m = full(chr(c))
    return ord(m) if len(m) == 1 else c

def mapping(full, override):
    out = {}
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        m = simple(c, full, override)
        if m != c:
            out[c] = m - c
    return out

def case_ranges(m):
    # Greedy: extend a range while the delta holds at stride 1, else try stride 2
    keys = sorted(m)
    out = []
    i = 0
    while i < len(keys):
        lo = keys[i]; delta = m[lo]
        stride = 1
        hi = lo
        j = i + 1
        while j < len(keys) and keys[j] == hi + 1 and m[keys[j]] == delta:
            hi = keys[j]; j += 1
        if hi == lo:
            stride = 2
            j = i + 1
            while j < len(keys) and keys[j] == hi + 2 and m[keys[j]] == delta:
                hi = keys[j]; j += 1
            if hi == lo:
                stride = 1
        out.append((lo, hi, delta, stride))
        i = j
    return out

def check_case(m, rs):
    # Every code point of every range maps as the table says, and nothing outside
    seen = {}
    for lo, hi, delta, stride in rs:
        for c in range(lo, hi + 1, stride):
            seen[c] = delta
    assert seen == m, "the ranges do not reproduce the mapping"

def write_case_tables():
    for c in range(0x110000):   # every full mapping longer than one character has its simple mapping above
        if not 0xD800 <= c <= 0xDFFF:
            assert len(chr(c).lower()) == 1 or c in SIMPLE_LOWER, hex(c)
            assert len(chr(c).upper()) == 1 or c in SIMPLE_UPPER, hex(c)

    lower = mapping(str.lower, SIMPLE_LOWER)
    upper = mapping(str.upper, SIMPLE_UPPER)
    lr = case_ranges(lower); ur = case_ranges(upper)
    check_case(lower, lr); check_case(upper, ur)

    with open("sgcl/core/detail/unicode_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The simple case mappings as ranges: a code point c in [lo, hi] with
// (c - lo) % stride == 0 maps to c + delta. {len(lr)} ranges to lower over
// {len(lower)} code points, {len(ur)} to upper over {len(upper)}; each
// table split at the end of the Basic Multilingual Plane.

#include <cstddef>
#include <cstdint>

namespace sgcl::detail {{
    inline constexpr const char* UnicodeVersion = "{VERSION}";

    // Split at the end of the Basic Multilingual Plane: almost every
    // range is there, and there a bound needs sixteen bits. The delta
    // keeps thirty-two: fifteen ranges of the plane map across a distance
    // no int16_t holds (the Cherokee block, +38864, and the letters of
    // A7xx, down to -42561).
    struct CaseRange16 {{
        uint16_t lo;
        uint16_t hi;
        int32_t delta;
        uint8_t stride;
    }};

    struct CaseRange32 {{
        char32_t lo;
        char32_t hi;
        int32_t delta;
        uint8_t stride;
    }};

    struct CaseTable {{
        const CaseRange16* bmp;
        size_t bmp_size;
        const CaseRange32* high;
        size_t high_size;
    }};

    namespace unicode_tables {{
""")
        emit_split(f, "CaseRange", "ToLower", lr)
        f.write("\n")
        emit_split(f, "CaseRange", "ToUpper", ur)
        f.write("    }\n}\n")
    return {"ToLower": size_of("CaseRange", lr), "ToUpper": size_of("CaseRange", ur)}

#------------------------------------------------------------------------------
# text: the properties of a code point
#------------------------------------------------------------------------------
# The general category, spelled out as ICU spells it, with the UCD's
# abbreviation beside it. unassigned is zero, so that the table carries no
# range for the unassigned code points — two thirds of the space.
CATEGORIES = [
    ("Cn", "unassigned"), ("Lu", "uppercase_letter"), ("Ll", "lowercase_letter"), ("Lt", "titlecase_letter"),
    ("Lm", "modifier_letter"), ("Lo", "other_letter"), ("Mn", "nonspacing_mark"), ("Mc", "spacing_mark"),
    ("Me", "enclosing_mark"), ("Nd", "decimal_number"), ("Nl", "letter_number"), ("No", "other_number"),
    ("Pc", "connector_punctuation"), ("Pd", "dash_punctuation"), ("Ps", "open_punctuation"),
    ("Pe", "close_punctuation"), ("Pi", "initial_punctuation"), ("Pf", "final_punctuation"),
    ("Po", "other_punctuation"), ("Sm", "math_symbol"), ("Sc", "currency_symbol"), ("Sk", "modifier_symbol"),
    ("So", "other_symbol"), ("Zs", "space_separator"), ("Zl", "line_separator"), ("Zp", "paragraph_separator"),
    ("Cc", "control"), ("Cf", "format"), ("Cs", "surrogate"), ("Co", "private_use"),
]
CATEGORY_VALUE = {abbr: i for i, (abbr, _) in enumerate(CATEGORIES)}

def script_name(ucd_name):
    # Old_Permic -> old_permic, Nko -> nko, Zzzz's alias Unknown -> unknown
    return ucd_name.lower()

def write_property_tables():
    sizes = {}

    # The general category, from unicodedata
    category = value_ranges(lambda c: CATEGORY_VALUE[unicodedata.category(chr(c))], 0)
    check_values(category, lambda c: CATEGORY_VALUE[unicodedata.category(chr(c))], 0)
    sizes["Category"] = trie_size(category)

    # The scripts, from Scripts.txt; Unknown is zero and carries no range
    scripts = properties("Scripts.txt")
    names = sorted(scripts)
    # Unknown is the file's @missing value: it lists no range for it
    assert "Unknown" not in names and "Common" in names and "Inherited" in names
    order = {"Unknown": 0}
    for i, name in enumerate(names):
        order[name] = i + 1
    script = from_sets(scripts, order, "Unknown")
    sizes["Script"] = trie_size(script)

    # Extended_Pictographic, from emoji-data.txt: what is_emoji answers and
    # what rule GB11 of the segmentation will need
    pictographic = properties("emoji/emoji-data.txt", {"Extended_Pictographic"})["Extended_Pictographic"]
    sizes["ExtendedPictographic"] = size_of("Range", pictographic)

    # The decimal digits: a range's value is the digit of its first code
    # point, the rest following it (every Nd block is ten in a row)
    digits = []
    for c in range(0x110000):
        v = unicodedata.decimal(chr(c), -1)
        if v < 0:
            continue
        if digits and digits[-1][1] == c - 1 and digits[-1][2] + (c - digits[-1][0]) == v:
            digits[-1] = (digits[-1][0], c, digits[-1][2])
        else:
            digits.append((c, c, v))
    for lo, hi, base in digits:
        for c in range(lo, hi + 1):
            assert unicodedata.decimal(chr(c)) == base + c - lo, hex(c)
    sizes["DecimalDigit"] = size_of("ValueRange", digits)

    # East_Asian_Width W and F: the two-column code points
    wide = coalesce([(c, c) for c in range(0x110000) if unicodedata.east_asian_width(chr(c)) in ("W", "F")])
    sizes["Wide"] = size_of("Range", wide)

    with open("sgcl/txt/detail/property_enums.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The names of the general category and of the scripts. Both start at the
// value the tables leave out — unassigned, unknown — so that a code point
// no range covers answers it.

#include <cstdint>

namespace sgcl::txt::detail {{
    enum class category : uint8_t {{
""")
        for abbr, name in CATEGORIES:
            f.write(f"        {name},{' ' * max(1, 24 - len(name))}// {abbr}\n")
        f.write("    };\n\n    enum class script : uint16_t {\n")
        for name in ["Unknown"] + names:
            f.write(f"        {script_name(name)},\n")
        f.write("    };\n}\n")

    with open("sgcl/txt/detail/property_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The properties of a code point as sorted ranges, searched binarily
// (detail/tables.h). A ValueRange's value is the property's enumerator,
// except in DecimalDigit, where it is the digit of lo.

#include "tables.h"

namespace sgcl::txt::detail::property_tables {{
""")
        emit_trie(f, "Category", category, f"the general category: {len(category)} ranges", indent=4)
        f.write("\n")
        emit_trie(f, "Script", script, f"the scripts: {len(script)} ranges over {len(names)} scripts", indent=4)
        f.write("\n")
        emit_bits(f, "ExtendedPictographic", pictographic, f"the emoji: {len(pictographic)} ranges", indent=4)
        f.write("\n")
        emit_split(f, "ValueRange", "DecimalDigit", digits, f"the decimal digits: {len(digits)} blocks", indent=4)
        f.write("\n")
        emit_bits(f, "Wide", wide, f"East_Asian_Width W and F: {len(wide)} ranges", indent=4)
        f.write("}\n")
    return sizes

#------------------------------------------------------------------------------
# txt: the boundaries
#------------------------------------------------------------------------------
# The grapheme cluster break property (UAX #29) and the Indic conjunct
# break (rule GB9c). "other" is zero in both, so the tables carry no range
# for it — most of the space.
GCB = ["other", "cr", "lf", "control", "extend", "zwj", "regional_indicator", "prepend",
       "spacing_mark", "l", "v", "t", "lv", "lvt"]
GCB_UCD = {"CR": "cr", "LF": "lf", "Control": "control", "Extend": "extend", "ZWJ": "zwj",
           "Regional_Indicator": "regional_indicator", "Prepend": "prepend", "SpacingMark": "spacing_mark",
           "L": "l", "V": "v", "T": "t", "LV": "lv", "LVT": "lvt"}
INCB = ["other", "linker", "consonant", "extend"]
INCB_UCD = {"Linker": "linker", "Consonant": "consonant", "Extend": "extend"}

# Word_Break (UAX #29). "other" is zero here too.
WB = ["other", "cr", "lf", "newline", "extend", "zwj", "regional_indicator", "format", "katakana",
      "aletter", "hebrew_letter", "mid_letter", "mid_num", "mid_num_let", "single_quote", "double_quote",
      "numeric", "extend_num_let", "wseg_space"]
WB_UCD = {"CR": "cr", "LF": "lf", "Newline": "newline", "Extend": "extend", "ZWJ": "zwj",
          "Regional_Indicator": "regional_indicator", "Format": "format", "Katakana": "katakana",
          "ALetter": "aletter", "Hebrew_Letter": "hebrew_letter", "MidLetter": "mid_letter",
          "MidNum": "mid_num", "MidNumLet": "mid_num_let", "Single_Quote": "single_quote",
          "Double_Quote": "double_quote", "Numeric": "numeric", "ExtendNumLet": "extend_num_let",
          "WSegSpace": "wseg_space"}

# Sentence_Break (UAX #29). "other" is zero here too.
SB = ["other", "cr", "lf", "sep", "extend", "format", "sp", "lower", "upper", "oletter",
      "numeric", "aterm", "scontinue", "sterm", "close"]
SB_UCD = {"CR": "cr", "LF": "lf", "Sep": "sep", "Extend": "extend", "Format": "format", "Sp": "sp",
          "Lower": "lower", "Upper": "upper", "OLetter": "oletter", "Numeric": "numeric",
          "ATerm": "aterm", "SContinue": "scontinue", "STerm": "sterm", "Close": "close"}

# The names a pattern of regex.h may write inside \p{...}. Nothing is
# worked out here: the two-letter codes of the categories and the long
# names of the scripts are the enums of property_enums.h written out as
# strings, in the same order, so that a position in the table is a value
# of the enum. They are a table rather than a switch because the parser
# has to go from a name to a value, and C++ has no way of asking an
# enumerator for its own name.
def write_regex_tables():
    scripts = properties("Scripts.txt")
    names = ["Unknown"] + sorted(scripts)
    with open("sgcl/txt/detail/regex_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The names a pattern may write inside \\p{{...}}: the two-letter codes of
// the general categories and the long names of the scripts, in the order
// of the enums of property_enums.h, so that a position here is a value
// there. A name is compared loosely — case and the separators ignored —
// so the script names are given as the enum spells them: Sign_Writing,
// SignWriting and signwriting all ask the same question.

#include "property_enums.h"

#include <string_view>

namespace sgcl::txt::detail {{
    inline constexpr std::string_view CategoryNames[] = {{
""")
        for abbr, name in CATEGORIES:
            f.write(f'        "{abbr}",{" " * 7}// {name}\n')
        f.write("    };\n\n    inline constexpr std::string_view ScriptNames[] = {\n")
        for name in names:
            f.write(f'        "{script_name(name)}",\n')
        f.write("    };\n}\n")
    print(f"  nazwy regex: {len(CATEGORIES)} kategorii, {len(names)} pism")


# Line_Break (UAX #14), with rule LB1 applied here rather than at run
# time: AI, SG, XX and the unassigned resolve to AL, CJ to NS, and SA to
# CM when it is a combining mark and to AL otherwise. What is left is
# what the rules speak about. "al" is zero: the default of LB1.
LB = ["al", "bk", "cr", "lf", "nl", "sp", "zw", "zwj", "cm", "wj", "gl", "b2", "ba", "bb", "hy", "cb",
      "cl", "cp", "ex", "in", "ns", "op", "qu", "is", "nu", "po", "pr", "sy", "eb", "em", "h2", "h3",
      "hl", "id", "jl", "jt", "jv", "ri", "ak", "ap", "as", "vf", "vi"]
LB_UCD = {"BK": "bk", "CR": "cr", "LF": "lf", "NL": "nl", "SP": "sp", "ZW": "zw", "ZWJ": "zwj",
          "CM": "cm", "WJ": "wj", "GL": "gl", "B2": "b2", "BA": "ba", "BB": "bb", "HY": "hy",
          "CB": "cb", "CL": "cl", "CP": "cp", "EX": "ex", "IN": "in", "NS": "ns", "OP": "op",
          "QU": "qu", "IS": "is", "NU": "nu", "PO": "po", "PR": "pr", "SY": "sy", "EB": "eb",
          "EM": "em", "H2": "h2", "H3": "h3", "HL": "hl", "ID": "id", "JL": "jl", "JT": "jt",
          "JV": "jv", "RI": "ri", "AK": "ak", "AP": "ap", "AS": "as", "VF": "vf", "VI": "vi",
          "AI": "al", "SG": "al", "XX": "al", "AL": "al", "CJ": "ns"}

def write_segment_tables():
    sizes = {}
    gcb_sets = properties("auxiliary/GraphemeBreakProperty.txt")
    assert set(gcb_sets) <= set(GCB_UCD), sorted(set(gcb_sets) - set(GCB_UCD))
    gcb = from_sets(gcb_sets, {k: GCB.index(v) for k, v in GCB_UCD.items()}, None)
    sizes["GraphemeBreak"] = trie_size(gcb)

    incb_sets = derived("DerivedCoreProperties.txt", "InCB")
    assert set(incb_sets) == set(INCB_UCD), sorted(incb_sets)
    incb = from_sets(incb_sets, {k: INCB.index(v) for k, v in INCB_UCD.items()}, None)
    sizes["IndicConjunctBreak"] = trie_size(incb)

    wb_sets = properties("auxiliary/WordBreakProperty.txt")
    assert set(wb_sets) <= set(WB_UCD), sorted(set(wb_sets) - set(WB_UCD))
    wb = from_sets(wb_sets, {k: WB.index(v) for k, v in WB_UCD.items()}, None)
    sizes["WordBreak"] = trie_size(wb)

    sb_sets = properties("auxiliary/SentenceBreakProperty.txt")
    assert set(sb_sets) <= set(SB_UCD), sorted(set(sb_sets) - set(SB_UCD))
    sb = from_sets(sb_sets, {k: SB.index(v) for k, v in SB_UCD.items()}, None)
    sizes["SentenceBreak"] = trie_size(sb)

    # LB1: SA is a combining mark or a letter, by its general category
    lb_sets = properties("LineBreak.txt")
    assert set(lb_sets) <= set(LB_UCD) | {"SA"}, sorted(set(lb_sets) - set(LB_UCD) - {"SA"})
    for lo, hi in lb_sets.pop("SA"):
        for c in range(lo, hi + 1):
            lb_sets.setdefault("CM" if unicodedata.category(chr(c)) in ("Mn", "Mc") else "AL", []).append((c, c))
    for value in lb_sets:
        lb_sets[value] = coalesce(sorted(lb_sets[value]))
    lb = from_sets(lb_sets, {k: LB.index(v) for k, v in LB_UCD.items()}, "AL")
    sizes["LineBreak"] = trie_size(lb)

    # $EastAsian of UAX #14: F, W and H, where columns wants only F and W
    east = coalesce([(c, c) for c in range(0x110000) if unicodedata.east_asian_width(chr(c)) in ("W", "F", "H")])
    sizes["EastAsian"] = size_of("Range", east)

    with open("sgcl/txt/detail/segment_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The break properties of UAX #29 as sorted ranges (detail/tables.h).
// The default value of each property is zero and carries no range.

#include "tables.h"

namespace sgcl::txt::detail {{
    // Grapheme_Cluster_Break
    enum class gcb : uint8_t {{
""")
        for name in GCB:
            f.write(f"        {name},\n")
        f.write("    };\n\n    // Indic_Conjunct_Break, for rule GB9c\n    enum class incb : uint8_t {\n")
        for name in INCB:
            f.write(f"        {name},\n")
        f.write("    };\n\n    // Word_Break\n    enum class wb : uint8_t {\n")
        for name in WB:
            f.write(f"        {name},\n")
        f.write("    };\n\n    // Sentence_Break\n    enum class sb : uint8_t {\n")
        for name in SB:
            f.write(f"        {name},\n")
        f.write("    };\n\n    // Line_Break, with rule LB1 resolved\n    enum class lb : uint8_t {\n")
        for name in LB:
            f.write(f"        {name},\n")
        f.write("    };\n\n    namespace segment_tables {\n")
        emit_trie(f, "GraphemeBreak", gcb, f"Grapheme_Cluster_Break: {len(gcb)} ranges")
        f.write("\n")
        emit_trie(f, "IndicConjunctBreak", incb, f"Indic_Conjunct_Break: {len(incb)} ranges")
        f.write("\n")
        emit_trie(f, "WordBreak", wb, f"Word_Break: {len(wb)} ranges")
        f.write("\n")
        emit_trie(f, "SentenceBreak", sb, f"Sentence_Break: {len(sb)} ranges")
        f.write("\n")
        emit_trie(f, "LineBreak", lb, f"Line_Break: {len(lb)} ranges")
        f.write("\n")
        emit_bits(f, "EastAsian", east, f"$EastAsian of UAX #14 (ea=F, W, H): {len(east)} ranges")
        f.write("    }\n}\n")

    cases = break_tests("auxiliary/GraphemeBreakTest.txt")
    words = break_tests("auxiliary/WordBreakTest.txt")
    sentences = break_tests("auxiliary/SentenceBreakTest.txt")
    lines = break_tests("auxiliary/LineBreakTest.txt")
    with open("tests/txt/break_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The break test files of the UCD, the oracle the segmentation is held
// to: every case is a sequence of code points and, for each position
// before a code point and at the end, whether a boundary falls there.

namespace ucd {{
    struct BreakCase {{
        const char32_t* text;
        const char* breaks;   // one character per position, '1' a boundary
    }};

""")
        emit_break_tests(f, "GraphemeBreakTests", cases)
        f.write("\n")
        emit_break_tests(f, "WordBreakTests", words)
        f.write("\n")
        emit_break_tests(f, "SentenceBreakTests", sentences)
        f.write("\n")
        emit_break_tests(f, "LineBreakTests", lines)
        f.write("}\n")
    print(f"  test cases: graphemes {len(cases)}, words {len(words)}, sentences {len(sentences)}, lines {len(lines)}")
    return sizes

#------------------------------------------------------------------------------
# txt: normalization (UAX #15)
#------------------------------------------------------------------------------
# The quick check properties, two bits a form: 0 yes, 1 no, 2 maybe. A code
# point no range covers is yes for all four, which is the default and the
# reason the tables carry no range for it.
QC_FORMS = ["NFC_QC", "NFD_QC", "NFKC_QC", "NFKD_QC"]
QC_VALUE = {"N": 1, "M": 2}

def utf16(points):
    # The pool is UTF-16: a supplementary code point is a surrogate pair
    out = []
    for c in points:
        if c < 0x10000:
            out.append(c)
        else:
            c -= 0x10000
            out.append(0xD800 + (c >> 10))
            out.append(0xDC00 + (c & 0x3FF))
    return out

def emit_decomp(f, name, mapping, comment):
    # One pool for the whole table, the index split at the end of the BMP
    pool, index = [], []
    for cp in sorted(mapping):
        units = utf16(mapping[cp])
        assert len(units) < 256, hex(cp)
        index.append((cp, len(pool), len(units)))
        pool += units
    assert len(pool) < 0x10000, len(pool)
    bmp = [(cp, at, n) for cp, at, n in index if cp < BMP]
    high = [(cp, at, n) for cp, at, n in index if cp >= BMP]
    f.write(f"        // {comment}: {len(index)} code points, {len(pool)} units in the pool\n")
    f.write(f"        inline constexpr char16_t {name}Pool[] = {{\n")
    for i in range(0, len(pool), 12):
        f.write("            " + " ".join(f"0x{u:04X}," for u in pool[i:i + 12]) + "\n")
    f.write("        };\n")
    for kind, rows in (("Decomp16", bmp), ("Decomp32", high)):
        f.write(f"        inline constexpr {kind} {name}{'Bmp' if kind == 'Decomp16' else 'High'}[] = {{\n")
        for cp, at, n in rows:
            f.write(f"            {{0x{cp:04X}, {at}, {n}}},\n")
        f.write("        };\n")
    f.write(f"        inline constexpr DecompTable {name}{{{name}Bmp, {len(bmp)}, "
            f"{name}High, {len(high)}, {name}Pool}};\n")
    return len(pool) * 2 + len(bmp) * 6 + len(high) * 8

def emit_composition(f, name, pairs, comment):
    bmp = [p for p in pairs if p[0] < BMP and p[1] < BMP and p[2] < BMP]
    high = [p for p in pairs if p not in bmp]
    assert all(p[0] >= BMP or p[1] >= BMP or p[2] >= BMP for p in high)
    f.write(f"        // {comment}: {len(pairs)} pairs\n")
    for kind, rows, suffix in (("Composed16", bmp, "Bmp"), ("Composed32", high, "High")):
        f.write(f"        inline constexpr {kind} {name}{suffix}[] = {{\n")
        for a, b, c in rows:
            f.write(f"            {{0x{a:04X}, 0x{b:04X}, 0x{c:04X}}},\n")
        f.write("        };\n")
    f.write(f"        inline constexpr CompositionTable {name}{{{name}Bmp, {len(bmp)}, {name}High, {len(high)}}};\n")
    return len(bmp) * 6 + len(high) * 12

def write_normalize_tables():
    sizes = {}
    canonical, compat, ccc = {}, {}, {}
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        ch = chr(c)
        k = unicodedata.combining(ch)
        if k:
            ccc[c] = k
        d = unicodedata.decomposition(ch)
        if not d:
            continue
        if d.startswith("<"):
            compat[c] = [int(x, 16) for x in d.split(">")[1].split()]
        else:
            canonical[c] = [int(x, 16) for x in d.split()]

    # The Hangul syllables decompose by arithmetic, not by a table, and
    # Python leaves them out of the decompositions for the same reason
    assert not any(0xAC00 <= c <= 0xD7A3 for c in canonical)

    # A primary composite is a canonical pair that NFC puts back together:
    # the composition exclusions fall out of asking rather than of a list
    pairs = sorted((v[0], v[1], c) for c, v in canonical.items()
                   if len(v) == 2 and unicodedata.normalize("NFC", chr(v[0]) + chr(v[1])) == chr(c))
    for a, b, c in pairs:
        assert unicodedata.normalize("NFC", chr(a) + chr(b)) == chr(c)

    # The two bounds the header answers without a table
    assert min(ccc) == 0x300, hex(min(ccc))
    assert min(list(canonical) + list(compat)) == 0xA0

    combining = value_ranges(lambda c: ccc.get(c, 0), 0)
    check_values(combining, lambda c: ccc.get(c, 0), 0)
    sizes["CombiningClass"] = trie_size(combining)

    qc_sets = derived_quick_check()
    quick = value_ranges(lambda c: qc_sets.get(c, 0), 0)
    check_values(quick, lambda c: qc_sets.get(c, 0), 0)
    sizes["QuickCheck"] = trie_size(quick)

    with open("sgcl/txt/detail/normalize_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// What UAX #15 needs: the canonical combining class, the two kinds of
// decomposition with their pools, the pairs that compose, and the quick
// check properties. The Hangul syllables are not here — they decompose
// and compose by arithmetic.

#include "tables.h"

namespace sgcl::txt::detail {{
    // Two bits a form, in the order NFC, NFD, NFKC, NFKD: 0 the text is
    // in that form, 1 it is not, 2 it may be and only the full
    // normalization can say
    inline constexpr unsigned QuickCheckYes = 0;
    inline constexpr unsigned QuickCheckNo = 1;
    inline constexpr unsigned QuickCheckMaybe = 2;

    namespace normalize_tables {{
""")
        emit_trie(f, "CombiningClass", combining,
                   f"the canonical combining class: {len(combining)} ranges over {len(ccc)} code points")
        f.write("\n")
        emit_trie(f, "QuickCheck", quick,
                   f"the quick check properties, two bits a form: {len(quick)} ranges")
        f.write("\n")
        # Whether a code point comes apart at all, asked before the
        # decomposition is looked up: hardly any code point does, and the
        # walk that takes a text apart asks about every one of them
        sizes["HasDecomposition"] = emit_bits(f, "HasCanonical", coalesce([(c, c) for c in sorted(canonical)]),
                                              f"the code points that decompose canonically: {len(canonical)}")
        f.write("\n")
        sizes["HasDecomposition"] += emit_bits(f, "HasCompat", coalesce([(c, c) for c in sorted(compat)]),
                                               f"and by compatibility: {len(compat)}")
        f.write("\n")
        sizes["CanonicalDecomposition"] = emit_decomp(f, "CanonicalDecomposition", canonical, "the canonical decompositions")
        f.write("\n")
        sizes["CompatDecomposition"] = emit_decomp(f, "CompatDecomposition", compat, "the compatibility decompositions")
        f.write("\n")
        sizes["Composition"] = emit_composition(f, "Composition", pairs, "the primary composites")
        f.write("    }\n}\n")

    cases = normalization_tests()
    with open("tests/txt/normalization_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// NormalizationTest.txt: every line is five forms of the same text —
// source, NFC, NFD, NFKC, NFKD — and part one names the code points
// whose own forms differ from themselves.

namespace ucd {{
    struct NormalizationCase {{
        const char32_t* source;
        const char32_t* nfc;
        const char32_t* nfd;
        const char32_t* nfkc;
        const char32_t* nfkd;
        bool part1;   // the line is one of part one's single code points
    }};

    inline constexpr NormalizationCase NormalizationTests[] = {{
""")
        for row, part1 in cases:
            cols = ", ".join('U"' + "".join(f"\\U{c:08X}" for c in col) + '"' for col in row)
            f.write(f"        {{{cols}, {'true' if part1 else 'false'}}},\n")
        f.write("    };\n}\n")
    print(f"  NormalizationTest.txt: {len(cases)} cases")
    return sizes

def derived_quick_check():
    # DerivedNormalizationProps.txt names only the code points that are
    # not simply in the form; everything else is yes
    out = {}
    for i, prop in enumerate(QC_FORMS):
        for value, rs in derived("DerivedNormalizationProps.txt", prop).items():
            for lo, hi in rs:
                for c in range(lo, hi + 1):
                    out[c] = out.get(c, 0) | (QC_VALUE[value] << (2 * i))
    return out

def normalization_tests():
    out = []
    part = 0
    for line in ucd("NormalizationTest.txt"):
        if line.startswith("@Part"):
            part = int(line[5])
            continue
        line = line.split("#")[0].strip()
        if not line:
            continue
        cols = [[int(x, 16) for x in col.split()] for col in line.split(";")[:5]]
        assert len(cols) == 5, line
        out.append((cols, part == 1))
    return out

#------------------------------------------------------------------------------
# txt: the full case mappings (UAX #21, SpecialCasing.txt)
#------------------------------------------------------------------------------
# core maps one code point to one (the simple mapping, note 133). Here are
# the code points whose full mapping is something else: ß to SS, ﬁ to FI,
# the Greek with ypogegrammeni, and the folding that follows the upper
# case rather than the lower. There are about five hundred of them, so
# the tables hold only the differences and everything else falls through
# to unicode::to_lower and unicode::to_upper.
def simple_lower(c):
    m = chr(c).lower()
    return ord(m) if len(m) == 1 else c

def simple_upper(c):
    m = chr(c).upper()
    return ord(m) if len(m) == 1 else c

def write_case_tables():
    sizes = {}
    full_lower, full_upper, full_title, fold = {}, {}, {}, {}
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        ch = chr(c)
        lo, up, ti, cf = ch.lower(), ch.upper(), ch.title(), ch.casefold()
        if len(lo) > 1 or ord(lo[0]) != simple_lower(c):
            full_lower[c] = [ord(x) for x in lo]
        if len(up) > 1 or ord(up[0]) != simple_upper(c):
            full_upper[c] = [ord(x) for x in up]
        if ti != up:
            full_title[c] = [ord(x) for x in ti]
        # against the simple mapping, which is what the header falls
        # through to — not against the full lower case, or İ would fold
        # to a bare i
        if len(cf) > 1 or ord(cf[0]) != simple_lower(c):
            fold[c] = [ord(x) for x in cf]

    # The conditional mappings of SpecialCasing.txt are not in the tables:
    # they depend on the language or on what stands around the letter, and
    # the header does them in code. What is asserted here is that the
    # three languages are the only ones the file names.
    languages = set()
    for line in ucd("SpecialCasing.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) > 4 and fields[4]:
            for word in fields[4].split():
                if word[0].islower() and len(word) <= 3:
                    languages.add(word)
    assert languages == {"tr", "az", "lt"}, languages

    # The three properties the conditional rules ask about: whether a
    # letter has a case (the sigma rule looks for one on either side),
    # whether a code point may stand between without breaking that look,
    # and whether a letter loses its dot when something is put above it
    cased = properties_of("DerivedCoreProperties.txt", "Cased")
    ignorable = properties_of("DerivedCoreProperties.txt", "Case_Ignorable")
    soft_dotted = properties_of("PropList.txt", "Soft_Dotted")

    with open("sgcl/txt/detail/case_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The full case mappings — only the code points whose mapping is not the
// simple one core already has ([unicode]). The conditional mappings of
// SpecialCasing.txt (Turkish, Azerbaijani, Lithuanian, and the sigma at
// the end of a word) are in the header, not here: they depend on the
// language or on what stands around the letter.

#include "tables.h"

namespace sgcl::txt::detail::case_tables {{
""")
        for name, rs, comment in (("Cased", cased, "the letters that have a case"),
                                  ("CaseIgnorable", ignorable, "what may stand between two of them"),
                                  ("SoftDotted", soft_dotted, "the letters whose dot a mark above removes")):
            sizes[name] = emit_bits(f, name, [(lo, hi) for lo, hi in rs], comment)
            f.write("\n")
        # case.h converts Latin text by shifting one bit rather than by
        # asking any of these, which holds only while none of them has
        # anything to say about ASCII — and while the simple mappings of
        # core agree with that shift
        for table in (full_lower, full_upper, full_title, fold):
            assert all(c >= 0x80 for c in table), sorted(c for c in table if c < 0x80)
        for c in range(0x80):
            lower = chr(c).lower() if "A" <= chr(c) <= "Z" else chr(c)
            upper = chr(c).upper() if "a" <= chr(c) <= "z" else chr(c)
            assert unicodedata.category(chr(c)) != "Lu" or lower == chr(c + 32)
            assert unicodedata.category(chr(c)) != "Ll" or upper == chr(c - 32)
        for name, mapping, comment in (("FullLower", full_lower, "the full lower case"),
                                       ("FullUpper", full_upper, "the full upper case"),
                                       ("FullTitle", full_title, "the title case, where it is not the upper case"),
                                       ("FullFold", fold, "the full case folding")):
            sizes[name] = emit_decomp(f, name, mapping, comment)
            f.write("\n")
        f.write("}\n")
    write_case_tests()
    print(f"  pełne mapowania: lower {len(full_lower)}, upper {len(full_upper)}, "
          f"title {len(full_title)}, fold {len(fold)}")
    return sizes

def write_case_tests():
    # Two oracles. Every code point that has a case at all, with what
    # Python makes of it — that walks the tables and the fall-through to
    # the simple mapping of core. And strings built around the conditions
    # of SpecialCasing.txt: the sigma at the end of a word (which Python
    # implements, so it answers for that one), the soft dotted letters,
    # and the marks above them.
    points = []
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        ch = chr(c)
        if ch.lower() != ch or ch.upper() != ch or ch.casefold() != ch:
            points.append(c)

    around = ["\u03A3", "\u03C2", "\u03C3"]
    contexts = ["", "a", "A", "\u0301", "'", " ", "1", "\u00DF"]
    strings = []
    for a in contexts:
        for s0 in around:
            for b in contexts:
                strings.append(a + s0 + b)
    strings += ["\u03A3\u03A3\u03A3", "A\u03A3", "A\u03A3B", "A\u03A3'", "A\u03A3\u0301",
                "stra\u00DFe", "STRASSE", "\uFB01ne", "\u0130stanbul", "i\u0307",
                "I\u0307", "j\u0307", "\u012F\u0307", "\u1E9E", "\u0149",
                "\u1F88", "\u1FFC", "\u01C5\u01C6\u01C4", "\u00B5", "\u017F"]

    def lit(t):
        return 'U"' + "".join(f"\\U{ord(x):08X}" for x in t) + '"'

    with open("tests/txt/case_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// What Python makes of the case of every code point that has one, and of
// the strings the conditions of SpecialCasing.txt are about. Python
// implements the rule of the final sigma, so it answers for that one too;
// the mappings that depend on a language are not here, since Python does
// not do them — those cases are written out in the test.

namespace ucd {{
    struct CaseCase {{
        const char32_t* source;
        const char32_t* lower;
        const char32_t* upper;
        const char32_t* fold;
    }};

    inline constexpr CaseCase CasePoints[] = {{
""")
        for c in points:
            ch = chr(c)
            f.write(f"        {{{lit(ch)}, {lit(ch.lower())}, {lit(ch.upper())}, {lit(ch.casefold())}}},\n")
        f.write("    };\n\n    inline constexpr CaseCase CaseStrings[] = {\n")
        for t in strings:
            f.write(f"        {{{lit(t)}, {lit(t.lower())}, {lit(t.upper())}, {lit(t.casefold())}}},\n")
        f.write("    };\n}\n")
    print(f"  wektory przypadków: {len(points)} code pointów, {len(strings)} ciągów")

#------------------------------------------------------------------------------
# txt: the encodings
#------------------------------------------------------------------------------
# The single byte encodings the Encoding Standard of the WHATWG lists —
# what a browser must understand, which is what a page or a file may
# still arrive in. The tables are read from the MAPPINGS files of
# unicode.org rather than from the standard's own indexes: those are a
# browser's compatibility rules, and this is a library.
#
# Everything comes out of one list: the enum, the preferred names, the
# aliases and the tables, so none of them can drift from the others.
# Aliases are matched with the dashes, underscores and spaces taken out
# and the case ignored, so one entry covers "ISO_8859-2" and "iso88592".
UNICODE_ENCODINGS = [
    ("utf8", "utf-8", ["utf8", "unicode-1-1-utf-8"]),
    ("utf16le", "utf-16le", ["utf-16", "ucs-2"]),
    ("utf16be", "utf-16be", []),
    ("utf32le", "utf-32le", ["utf-32", "ucs-4"]),
    ("utf32be", "utf-32be", []),
    ("ascii", "us-ascii", ["ascii", "ansi_x3.4-1968"]),
    ("latin1", "iso-8859-1", ["latin1", "l1", "cp819", "iso_8859-1"]),
]

SINGLE_BYTE = [
    ("ibm866", "VENDORS/MICSFT/PC/CP866.TXT", "ibm866", ["866", "cp866", "csibm866"]),
    ("iso8859_2", "ISO8859/8859-2.TXT", "iso-8859-2", ["latin2", "l2", "csisolatin2"]),
    ("iso8859_3", "ISO8859/8859-3.TXT", "iso-8859-3", ["latin3", "l3"]),
    ("iso8859_4", "ISO8859/8859-4.TXT", "iso-8859-4", ["latin4", "l4"]),
    ("iso8859_5", "ISO8859/8859-5.TXT", "iso-8859-5", ["cyrillic", "csisolatincyrillic"]),
    ("iso8859_6", "ISO8859/8859-6.TXT", "iso-8859-6", ["arabic", "asmo-708", "ecma-114"]),
    ("iso8859_7", "ISO8859/8859-7.TXT", "iso-8859-7", ["greek", "greek8", "ecma-118", "sun_eu_greek"]),
    ("iso8859_8", "ISO8859/8859-8.TXT", "iso-8859-8", ["hebrew", "iso-8859-8-i", "visual", "logical"]),
    ("iso8859_10", "ISO8859/8859-10.TXT", "iso-8859-10", ["latin6", "l6", "iso-ir-157"]),
    ("iso8859_13", "ISO8859/8859-13.TXT", "iso-8859-13", []),
    ("iso8859_14", "ISO8859/8859-14.TXT", "iso-8859-14", ["iso8859-14"]),
    ("iso8859_15", "ISO8859/8859-15.TXT", "iso-8859-15", ["latin9", "l9", "csisolatin9"]),
    ("iso8859_16", "ISO8859/8859-16.TXT", "iso-8859-16", []),
    ("koi8_r", "VENDORS/MISC/KOI8-R.TXT", "koi8-r", ["koi", "koi8", "cskoi8r"]),
    ("koi8_u", "VENDORS/MISC/KOI8-U.TXT", "koi8-u", ["koi8-ru"]),
    ("macintosh", "VENDORS/APPLE/ROMAN.TXT", "macintosh", ["mac", "csmacintosh", "x-mac-roman"]),
    ("windows874", "VENDORS/MICSFT/WINDOWS/CP874.TXT", "windows-874", ["iso-8859-11", "tis-620", "dos-874"]),
    ("windows1250", "VENDORS/MICSFT/WINDOWS/CP1250.TXT", "windows-1250", ["cp1250", "x-cp1250"]),
    ("windows1251", "VENDORS/MICSFT/WINDOWS/CP1251.TXT", "windows-1251", ["cp1251", "x-cp1251"]),
    ("windows1252", "VENDORS/MICSFT/WINDOWS/CP1252.TXT", "windows-1252", ["cp1252", "x-cp1252", "ansi_x3.4-1968-euro"]),
    ("windows1253", "VENDORS/MICSFT/WINDOWS/CP1253.TXT", "windows-1253", ["cp1253"]),
    ("windows1254", "VENDORS/MICSFT/WINDOWS/CP1254.TXT", "windows-1254", ["cp1254", "iso-8859-9", "latin5", "l5"]),
    ("windows1255", "VENDORS/MICSFT/WINDOWS/CP1255.TXT", "windows-1255", ["cp1255"]),
    ("windows1256", "VENDORS/MICSFT/WINDOWS/CP1256.TXT", "windows-1256", ["cp1256"]),
    ("windows1257", "VENDORS/MICSFT/WINDOWS/CP1257.TXT", "windows-1257", ["cp1257"]),
    ("windows1258", "VENDORS/MICSFT/WINDOWS/CP1258.TXT", "windows-1258", ["cp1258"]),
    ("x_mac_cyrillic", "VENDORS/APPLE/CYRILLIC.TXT", "x-mac-cyrillic", ["x-mac-ukrainian"]),
]

def write_encoding_tables():
    sizes = {}
    with open("sgcl/txt/detail/encoding_tables.h", "w") as f:
        f.write(HEADER + """
// Generated by tools/unicode_tables.py from the MAPPINGS files of
// unicode.org: do not edit. The set is the single byte encodings the
// Encoding Standard of the WHATWG lists; the tables are unicode.org's,
// not that standard's own indexes, which are a browser's compatibility
// rules. ISO-8859-1 has no table here — its byte is its code point —
// and neither has ASCII.

#include "tables.h"

namespace sgcl::txt::detail {
    enum class encoding : uint8_t {
""")
        for name, _, _ in UNICODE_ENCODINGS:
            f.write(f"        {name},\n")
        f.write("        // the single byte encodings, in the order of the tables below\n")
        for name, _, _, _ in SINGLE_BYTE:
            f.write(f"        {name},\n")
        f.write("    };\n\n")
        f.write(f"    inline constexpr uint8_t FirstSingleByte = {len(UNICODE_ENCODINGS)};\n\n")

        f.write("    // The name a header would use, one per value of the enum\n")
        f.write("    inline constexpr const char* EncodingNames[] = {\n")
        for _, preferred, _ in UNICODE_ENCODINGS:
            f.write(f'        "{preferred}",\n')
        for _, _, preferred, _ in SINGLE_BYTE:
            f.write(f'        "{preferred}",\n')
        f.write("    };\n\n")

        f.write("    struct EncodingAlias {\n        const char* name;\n        encoding value;\n    };\n\n")
        f.write("    // Every name a header may carry, the preferred one first\n")
        f.write("    inline constexpr EncodingAlias EncodingAliases[] = {\n")
        for name, preferred, aliases in UNICODE_ENCODINGS:
            for a in [preferred] + aliases:
                f.write(f'        {{"{a}", encoding::{name}}},\n')
        for name, _, preferred, aliases in SINGLE_BYTE:
            for a in [preferred] + aliases:
                f.write(f'        {{"{a}", encoding::{name}}},\n')
        f.write("    };\n\n")

        f.write("""    struct SingleByte {
        const char16_t* to_unicode;    // 256 of them, U+FFFD where the byte is nothing
        // and the way back: 256 slots, a code point in keys and its byte
        // beside it, the collisions walked forward from where they fall.
        // Zero is the empty slot — no encoding writes a code point under
        // 128 this way.
        const uint16_t* keys;
        const uint8_t* bytes;
    };

""")
        total = 0
        for name, path, preferred, _ in SINGLE_BYTE:
            table = mapping_file(path)
            assert all(b < 0x100 for b in table), name
            # Apple's files begin at 0x20 and say in prose that the bytes
            # below it are the ASCII controls; every encoding of this set
            # is ASCII underneath, which the assertion holds to
            for b in range(0x80):
                if b in table:
                    assert table[b] == b, (name, hex(b), hex(table[b]))
                else:
                    table[b] = b
            to_unicode = [table.get(b, 0xFFFD) for b in range(256)]
            back = sorted((c, b) for b, c in table.items() if c >= 0x80)
            f.write(f"    // {preferred}\n")
            f.write(f"    inline constexpr char16_t {name}_to[] = {{\n")
            for i in range(0, 256, 8):
                f.write("        " + " ".join(f"0x{c:04X}," for c in to_unicode[i:i + 8]) + "\n")
            f.write("    };\n")
            # The way back, from a code point to its byte, as a table of
            # 256 slots with the collisions walked forward from where they
            # fall. A sorted list wanted a binary search for every
            # character being written — 5.06 ns measured — and this wants
            # 1.13 probes on the average of the 27 encodings, 8 at the
            # worst; it is the same 768 bytes an encoding as the list it
            # replaces. Zero is the empty slot, which costs nothing: no
            # encoding maps a code point under 128 this way.
            keys, values = [0] * 256, [0] * 256
            for c, b in back:
                i = ((c * 2654435761) >> 24) & 255
                while keys[i]:
                    i = (i + 1) & 255
                keys[i], values[i] = c, b
            f.write(f"    inline constexpr uint16_t {name}_keys[] = {{\n")
            for i in range(0, 256, 8):
                f.write("        " + " ".join(f"0x{c:04X}," for c in keys[i:i + 8]) + "\n")
            f.write("    };\n")
            f.write(f"    inline constexpr uint8_t {name}_bytes[] = {{\n")
            for i in range(0, 256, 16):
                f.write("        " + " ".join(f"0x{b:02X}," for b in values[i:i + 16]) + "\n")
            f.write("    };\n\n")
            total += 256 * 2 + 256 * 2 + 256
        f.write("    // Indexed by the value of the enum less FirstSingleByte\n")
        f.write("    inline constexpr SingleByte SingleBytes[] = {\n")
        for name, _, _, _ in SINGLE_BYTE:
            f.write(f"        {{{name}_to, {name}_keys, {name}_bytes}},\n")
        f.write("    };\n}\n")
        sizes["SingleByteEncodings"] = total
    print(f"  kodowania jednobajtowe: {len(SINGLE_BYTE)}")
    return sizes

def write_encoding_tests():
    # The oracle is Python's own codecs, which come from the same files
    # but through another road: the tables here are read from MAPPINGS
    # and the ones there are built into the interpreter.
    # our name -> Python's codec; Python reaches the same tables by
    # another road, which is what makes it an oracle
    python_codec = {
        "ibm866": "cp866", "iso8859_2": "iso8859-2", "iso8859_3": "iso8859-3", "iso8859_4": "iso8859-4",
        "iso8859_5": "iso8859-5", "iso8859_6": "iso8859-6", "iso8859_7": "iso8859-7",
        "iso8859_8": "iso8859-8", "iso8859_10": "iso8859-10", "iso8859_13": "iso8859-13",
        "iso8859_14": "iso8859-14", "iso8859_15": "iso8859-15", "iso8859_16": "iso8859-16",
        "koi8_r": "koi8-r", "koi8_u": "koi8-u", "macintosh": "mac-roman", "windows874": "cp874",
        "windows1250": "cp1250", "windows1251": "cp1251", "windows1252": "cp1252",
        "windows1253": "cp1253", "windows1254": "cp1254", "windows1255": "cp1255",
        "windows1256": "cp1256", "windows1257": "cp1257", "windows1258": "cp1258",
        "x_mac_cyrillic": "mac-cyrillic", "latin1": "latin-1", "ascii": "ascii",
    }
    names = [(ours, python_codec[ours]) for ours, _, _, _ in SINGLE_BYTE] + \
            [("latin1", "latin-1"), ("ascii", "ascii")]
    texts = ["", "abc", "Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144",
             "\u00E9\u00E8\u00EA", "\u20AC 1,5", "\u65E5\u672C\u8A9E", "a\U0001F600b",
             "\u0104\u0106\u0118\u0141\u0143\u00D3\u015A\u0179\u017B", "line\nbreak\ttab"]
    with open("tests/txt/encoding_tests.h", "w") as f:
        f.write(HEADER + """
// Generated by tools/unicode_tables.py: what Python's codecs make of the
// same bytes and the same texts. The tables of the library are read from
// the MAPPINGS files of unicode.org; Python's are built into the
// interpreter, so the two roads are independent.

namespace ucd {
    struct ByteMapping {
        const char* encoding;
        const char32_t* points;   // 256 of them, U+FFFD where the byte means nothing
    };

    struct TextBytes {
        const char* encoding;
        const char32_t* text;
        const char* bytes;        // what Python encodes it to, '?' where it cannot
        size_t size;
    };

""")
        f.write("    inline constexpr ByteMapping ByteMappings[] = {\n")
        for ours, theirs in names:
            points = []
            for b in range(256):
                try:
                    points.append(ord(bytes([b]).decode(theirs)))
                except Exception:
                    points.append(0xFFFD)
            f.write(f'        {{"{ours}", U"' + "".join(f"\\U{c:08X}" for c in points) + '"},\n')
        f.write("    };\n\n    inline constexpr TextBytes TextsInBytes[] = {\n")
        for ours, theirs in names:
            for t in texts:
                raw = t.encode(theirs, "replace")
                lit = "".join(f"\\x{b:02X}" for b in raw)
                f.write(f'        {{"{ours}", U"' + "".join(f"\\U{ord(c):08X}" for c in t)
                        + f'", "{lit}", {len(raw)}}},\n')
        # and the encodings of Unicode itself
        for ours, theirs in [("utf16le", "utf-16-le"), ("utf16be", "utf-16-be"),
                             ("utf32le", "utf-32-le"), ("utf32be", "utf-32-be"), ("utf8", "utf-8")]:
            for t in texts:
                raw = t.encode(theirs)
                lit = "".join(f"\\x{b:02X}" for b in raw)
                f.write(f'        {{"{ours}", U"' + "".join(f"\\U{ord(c):08X}" for c in t)
                        + f'", "{lit}", {len(raw)}}},\n')
        f.write("    };\n}\n")
    print(f"  wektory kodowań: {len(names)} tablic bajtowych, "
          f"{len(texts) * (len(names) + 5)} tekstów")

#------------------------------------------------------------------------------
# txt: the bidirectional algorithm (UAX #9)
#------------------------------------------------------------------------------
# The class of every code point, and the brackets that must stay a pair.
# The classes come from DerivedBidiClass.txt rather than from Python,
# because the file assigns a class to the unassigned code points too —
# the ranges of Hebrew and Arabic are right to left before anything is
# put in them — and Python's unicodedata answers nothing for those.
BIDI = ["l", "r", "al", "en", "es", "et", "an", "cs", "nsm", "bn", "b", "s", "ws", "on",
        "lre", "lro", "rle", "rlo", "pdf", "lri", "rli", "fsi", "pdi"]
BIDI_UCD = {"L": "l", "R": "r", "AL": "al", "EN": "en", "ES": "es", "ET": "et", "AN": "an",
            "CS": "cs", "NSM": "nsm", "BN": "bn", "B": "b", "S": "s", "WS": "ws", "ON": "on",
            "LRE": "lre", "LRO": "lro", "RLE": "rle", "RLO": "rlo", "PDF": "pdf",
            "LRI": "lri", "RLI": "rli", "FSI": "fsi", "PDI": "pdi",
            "Left_To_Right": "l", "Right_To_Left": "r", "Arabic_Letter": "al",
            "European_Terminator": "et"}

def bidi_classes():
    # The explicit ranges, over the defaults of the @missing lines
    out = {}
    missing = []
    for line in ucd("extracted/DerivedBidiClass.txt"):
        if "@missing:" in line:
            body = line.split("@missing:")[1].strip()
            points, value = [x.strip() for x in body.split(";")]
            lo, hi = points.split("..")
            missing.append((int(lo, 16), int(hi, 16), BIDI_UCD[value]))
            continue
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 2 or fields[1] not in BIDI_UCD:
            continue
        points = fields[0].split("..")
        for c in range(int(points[0], 16), int(points[-1], 16) + 1):
            out[c] = BIDI_UCD[fields[1]]
    # the later @missing lines are the narrower ones and win
    def default_of(c):
        value = "l"
        for lo, hi, v in missing:
            if lo <= c <= hi:
                value = v
        return value
    return out, default_of

def write_bidi_tables():
    sizes = {}
    explicit, default_of = bidi_classes()
    def class_of(c):
        return BIDI.index(explicit.get(c) or default_of(c))
    table = value_ranges(class_of, 0)
    check_values(table, class_of, 0)
    sizes["BidiClass"] = trie_size(table)

    # The brackets of BD16: the one it pairs with, and whether it opens
    pairs = []
    for line in ucd("BidiBrackets.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 3:
            continue
        pairs.append((int(fields[0], 16), int(fields[1], 16), 1 if fields[2] == "o" else 0))
    pairs.sort()
    sizes["BidiBrackets"] = len(pairs) * 8

    # Rule L4: a character is drawn mirrored when it is resolved right to
    # left and its Bidi_Mirrored property is yes. Two questions, two
    # tables. Whether it is mirrored at all is Python's, UnicodeData
    # carrying the property in its ninth field; which glyph to draw is
    # BidiMirroring.txt, which only the file has. They are not the same
    # set — 126 of the 554 mirrored code points have no mirror of their
    # own, an integral sign being drawn the other way round without
    # there being a second one to name — so is_mirrored cannot be read
    # off the mapping.
    mirrored = [c for c in range(0x110000)
                if not (0xD800 <= c < 0xE000) and unicodedata.mirrored(chr(c))]
    mirror_ranges = coalesce([(c, c) for c in mirrored])
    mirror_bmp = [(lo, hi) for lo, hi in mirror_ranges if hi < 0x10000]
    mirror_high = [(lo, hi) for lo, hi in mirror_ranges if hi >= 0x10000]
    assert all(lo >= 0x10000 for lo, hi in mirror_high), "a range across the end of the BMP"

    glyphs = []
    for line in ucd("BidiMirroring.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 2:
            continue
        glyphs.append((int(fields[0], 16), int(fields[1], 16)))
    glyphs.sort()
    # Both halves of every pair are in the Basic Multilingual Plane, so
    # the table is uint16 and half the size it would be; and every one of
    # them is Bidi_Mirrored, which is what makes the set the wider one
    assert glyphs, "BidiMirroring.txt gave nothing"
    assert all(a < 0x10000 and b < 0x10000 for a, b in glyphs), "a mirror outside the BMP"
    assert all(a in set(mirrored) for a, _ in glyphs), "a mirror that is not Bidi_Mirrored"
    sizes["BidiMirroring"] = len(glyphs) * 4 + len(mirror_bmp) * 4 + len(mirror_high) * 8

    with open("sgcl/txt/detail/bidi_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The class of every code point (UAX #9), the brackets that must stay a
// pair (BD16) and the mirroring of rule L4. The classes come from
// DerivedBidiClass.txt and not from the assigned code points alone: the
// ranges of Hebrew and Arabic are right to left before anything is put
// in them.

#include "tables.h"

namespace sgcl::txt::detail {{
    enum class bidi : uint8_t {{
""")
        for name in BIDI:
            f.write(f"        {name},\n")
        f.write("""    };

    struct BracketPair {
        char32_t bracket;
        char32_t other;     // the one it pairs with
        uint8_t opens;      // 1 when it is the opening one
    };

    // The mirror of a code point, both halves of every pair being in the
    // Basic Multilingual Plane
    struct MirrorPair {
        uint16_t cp;
        uint16_t other;
    };

    namespace bidi_tables {
""")
        emit_trie(f, "BidiClass", table, f"Bidi_Class: {len(table)} ranges")
        f.write(f"\n        // the paired brackets: {len(pairs)} of them\n")
        f.write("        inline constexpr BracketPair Brackets[] = {\n")
        for a, b, o in pairs:
            f.write(f"            {{0x{a:04X}, 0x{b:04X}, {o}}},\n")
        f.write("        };\n")
        f.write(f"\n        // Bidi_Mirrored: {len(mirrored)} code points in "
                f"{len(mirror_bmp)} ranges below U+10000 and {len(mirror_high)} above\n")
        f.write("        inline constexpr Range16 MirroredBmp[] = {\n")
        for lo, hi in mirror_bmp:
            f.write(f"            {{0x{lo:04X}, 0x{hi:04X}}},\n")
        f.write("        };\n\n        inline constexpr Range32 MirroredHigh[] = {\n")
        for lo, hi in mirror_high:
            f.write(f"            {{0x{lo:04X}, 0x{hi:04X}}},\n")
        f.write("        };\n")
        f.write(f"\n        // BidiMirroring.txt: the {len(glyphs)} of them with a mirror"
                f" of their own\n")
        f.write("        inline constexpr MirrorPair Mirroring[] = {\n")
        for a, b in glyphs:
            f.write(f"            {{0x{a:04X}, 0x{b:04X}}},\n")
        f.write("        };\n    }\n}\n")
    print(f"  bidi: {len(table)} zakresów klas, {len(pairs)} par nawiasów, "
          f"{len(glyphs)} luster, {len(mirror_bmp) + len(mirror_high)} zakresów Bidi_Mirrored")
    return sizes

def write_bidi_tests(every=1):
    # BidiCharacterTest.txt: the code points, the direction asked for, the
    # level the paragraph resolved to, the level of each character and the
    # order they are displayed in. Kept as the file writes them, minus the
    # spaces, and parsed by the test — six million characters of struct
    # initialisers would be a different kind of problem.
    cases = []
    for line in ucd("BidiCharacterTest.txt"):
        line = line.split("#")[0].strip()
        if not line or line.startswith("@"):
            continue
        fields = line.split(";")
        if len(fields) != 5:
            continue
        cases.append(fields)
    kept = cases[::every]
    # BidiMirroring.txt as the file writes it, whole and never thinned:
    # it is 428 lines and the oracle for mirrored_of has to be the file
    # and not the table, which is made from it. The Bidi_Mirrored points
    # beside it come from where the table's set comes from, so they
    # check the lookup — the ranges, the split at the end of the plane,
    # the fast path below the parenthesis — and not the data.
    glyphs = []
    for line in ucd("BidiMirroring.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) >= 2:
            glyphs.append((int(fields[0], 16), int(fields[1], 16)))
    glyphs.sort()
    mirrored = [c for c in range(0x110000)
                if not (0xD800 <= c < 0xE000) and unicodedata.mirrored(chr(c))]
    with open("tests/txt/bidi_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// BidiCharacterTest.txt, {len(kept)} of its {len(cases)} cases. Every case
// is one string: the code points, the direction asked for (0 automatic,
// 1 left to right, 2 right to left), the level the paragraph resolved to,
// the level of every character ('x' where the character has none) and the
// order they are shown in.
//
// Then BidiMirroring.txt whole, all {len(glyphs)} of its lines, and the
// {len(mirrored)} code points whose Bidi_Mirrored property is yes.

namespace ucd {{
    inline constexpr const char* BidiCases[] = {{
""")
        for c in kept:
            f.write('        "' + ";".join(x.strip() for x in c) + '",\n')
        f.write("    };\n\n    // a code point and the mirror BidiMirroring.txt gives it\n")
        f.write("    inline constexpr char32_t BidiMirrorPairs[][2] = {\n")
        for a, b in glyphs:
            f.write(f"        {{0x{a:04X}, 0x{b:04X}}},\n")
        f.write("    };\n\n    // Bidi_Mirrored=Yes, which is the wider set\n")
        f.write("    inline constexpr char32_t BidiMirroredPoints[] = {\n")
        for i in range(0, len(mirrored), 8):
            f.write("        " + " ".join(f"0x{c:04X}," for c in mirrored[i:i + 8]) + "\n")
        f.write("    };\n}\n")
    print(f"  wektory bidi: {len(kept)} z {len(cases)}, {len(glyphs)} luster, "
          f"{len(mirrored)} znaków Bidi_Mirrored")

#------------------------------------------------------------------------------
# txt: collation (UTS #10)
#------------------------------------------------------------------------------
# DUCET is a map: a sequence of code points to a sequence of collation
# elements, each three weights. Written out as it stands it is 575 KB, and
# almost none of that is information — 73 per cent of its entries are one
# code point with one element whose second and third weights are the
# default ones, and the alphabets are laid out so that the weights run
# with the code points. Those collapse into 3281 ranges. What is left —
# the entries with several elements or weights of their own — goes into
# an index and a pool, and the contractions ("ch" as one letter) into a
# table of their own, 964 of them, 956 of two code points and 8 of three.
#
# Han and the unassigned are in no table at all: their weights are
# computed (UTS #10 section 10.1.3), which is why a file that covers all
# of Unicode has 39407 entries and not a million.
def collation_data():
    entries = []
    implicit = []
    variable = 0            # the heaviest weight the file marks with a star
    fixed = 1 << 24         # and the lightest it does not
    for line in ucd_uca("allkeys.txt"):
        if line.startswith("@implicitweights"):
            body = line.split()[1].rstrip(";")
            lo, hi = body.split("..")
            base = int(line.split(";")[1].split("#")[0].strip(), 16)
            implicit.append((int(lo, 16), int(hi, 16), base))
            continue
        line = line.split("#")[0].split("%")[0].strip()
        if not line or line.startswith("@"):
            continue
        left, right = line.split(";")
        points = [int(x, 16) for x in left.split()]
        elems = []
        # The star is the file's way of saying that the algorithm calls
        # this element variable — punctuation, a space, a symbol. It is
        # not carried into the tables: the variable elements are exactly
        # the ones that weigh no more than the heaviest of them, so one
        # comparison against that weight answers it where the elements
        # are read, and the assertion below is what makes that true.
        for star, a, b, c in re.findall(
                r"\[([*.])([0-9A-F]{4})\.([0-9A-F]{4})\.([0-9A-F]{4})\]", right):
            p, s, t = int(a, 16), int(b, 16), int(c, 16)
            if star == "*":
                variable = max(variable, p)
            elif p:
                fixed = min(fixed, p)
            elems.append((p, s, t))
        entries.append((points, elems))
    assert variable < fixed, (hex(variable), hex(fixed))
    return entries, implicit, variable

def write_collate_tables():
    sizes = {}
    tailorings = build_tailorings()
    entries, implicit, variable_top = collation_data()
    simple, other, contractions = [], [], []
    for points, elems in entries:
        if len(points) > 1:
            contractions.append((points, elems))
        elif len(elems) == 1 and elems[0][1] == 0x20 and elems[0][2] == 0x02:
            simple.append((points[0], elems[0][0]))
        else:
            other.append((points[0], elems))
    simple.sort()
    other.sort()
    contractions.sort()

    # the runs where the code point and the weight go up together
    runs = []
    for cp, primary in simple:
        if runs and runs[-1][1] + 1 == cp and runs[-1][2] + (runs[-1][1] - runs[-1][0]) + 1 == primary:
            runs[-1] = (runs[-1][0], cp, runs[-1][2])
        else:
            runs.append((cp, cp, primary))
    for lo, hi, first in runs:
        for c in range(lo, hi + 1):
            assert dict(simple)[c] == first + (c - lo)

    pool = []
    index = []
    for cp, elems in other:
        index.append((cp, len(pool), len(elems)))
        pool += elems
    assert len(pool) < 0x10000 and all(n < 256 for _, _, n in index)

    # Room for the case of a letter above the third weight. Where a
    # collator is asked about the case, the weights are read with two
    # bits of case over them and the weight itself two bits shorter, so
    # a tailored weight must not sit closer than four to its neighbour
    # in the same place; and no weight may reach the value that marks an
    # element shifted aside.
    tertiaries = collections.defaultdict(set)
    for entries_of in tailorings.values():
        for elems in entries_of.values():
            for p, sec, t in elems:
                tertiaries[(p, sec)].add(t)
    for group in tertiaries.values():
        weights = sorted(group)
        assert all(b - a >= 4 for a, b in zip(weights, weights[1:])), weights
        assert weights[-1] < 0xFFFF
    digit_zero = dict(simple)[0x30]

    cpool = []
    crows = []
    for points, elems in contractions:
        assert len(points) <= 3
        crows.append((points[0], points[1], points[2] if len(points) > 2 else 0, len(cpool), len(elems)))
        cpool += elems
    starters = sorted({p[0][0] for p in contractions})

    with open("sgcl/txt/detail/collate_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from the Unicode Collation
// Algorithm {VERSION}: do not edit. DUCET, the order everything else is a
// difference from: {len(entries)} entries and {sum(len(e[1]) for e in entries)} collation
// elements in the file, kept here as {len(runs)} ranges where the weight runs
// with the code point, an index of {len(index)} into a pool of {len(pool)}
// elements for the rest, and {len(contractions)} contractions.

#include "tables.h"

namespace sgcl::txt::detail {{
    // A collation element: the weight of the letter, of the accent, and
    // of the case. A weight of zero means the level ignores it.
    struct Weights {{
        uint16_t primary;
        uint16_t secondary;
        uint16_t tertiary;
    }};

    // What the algorithm calls variable — punctuation, spaces, symbols —
    // is exactly what weighs no more than this, so the flag the file
    // writes on every element is one comparison here
    inline constexpr uint16_t VariableTop = 0x{variable_top:04X};

    inline constexpr uint16_t DefaultSecondary = 0x0020;
    inline constexpr uint16_t DefaultTertiary = 0x0002;

    // The weight of the digit zero, where a run of digits read as a
    // number weighs when the numeric order is asked for: the places
    // between it and the digit one are free, no root weight falling
    // between two neighbouring ones.
    inline constexpr uint16_t DigitZero = 0x{digit_zero:04X};

    // An entry of the index: where its elements begin in the pool
    struct WeightRun {{
        char32_t cp;
        uint16_t at;
        uint8_t size;
    }};

    // A contraction: two or three code points that weigh as one letter
    struct Contraction {{
        char32_t first;
        char32_t second;
        char32_t third;      // zero when there are two
        uint16_t at;
        uint8_t size;
    }};

    // A range of code points whose weights are the base plus the distance
    struct ImplicitRange {{
        char32_t lo;
        char32_t hi;
        uint16_t base;
    }};

    // The comparison does not work on the weights above but on these,
    // which are the same weights with room left between them: a language
    // that wants ą between a and b needs somewhere to put it. The root's
    // weight is shifted, so between two neighbouring letters of the root
    // there are 65536 places at the first level, 128 at the second and
    // 1024 at the third — and a weight of zero, which means the level
    // passes the letter over, stays zero.
    inline constexpr int PrimaryShift = {SHIFT_P};
    inline constexpr int SecondaryShift = {SHIFT_S};
    inline constexpr int TertiaryShift = {SHIFT_T};

    struct Element {{
        uint32_t primary;
        uint16_t secondary;
        uint16_t tertiary;
    }};

    constexpr Element element_of(Weights w) noexcept {{
        return {{uint32_t(w.primary) << PrimaryShift,
                uint16_t(w.secondary << SecondaryShift),
                uint16_t(w.tertiary << TertiaryShift)}};
    }}

    // One code point a language weighs its own way
    struct TailoredPoint {{
        char32_t cp;
        uint32_t primary;
        uint16_t secondary;
        uint16_t tertiary;
    }};

    // Several of them that weigh as one letter: Hungarian's cs, Welsh's
    // ng, the ogonek Polish writes as a separate code point once the text
    // is decomposed
    struct TailoredSequence {{
        uint16_t at;             // into TailoringPoints
        uint8_t size;
        uint32_t primary;
        uint16_t secondary;
        uint16_t tertiary;
    }};

    // And what weighs as more than one letter, which is how a language
    // says "sort this as if it were written that way"
    struct TailoredExpansion {{
        uint16_t points_at;
        uint8_t points_size;
        uint8_t size;            // how many elements
        uint16_t at;             // into TailoringElements
    }};

    // A language's own order: three ranges of the three tables
    struct Tailoring {{
        uint32_t language;       // the subtag in four bytes, as locale holds it
        uint16_t points_at, points_size;
        uint16_t sequences_at, sequences_size;
        uint16_t expansions_at, expansions_size;
        // A bit for the low six of every code point an entry begins
        // with. A language moves a handful of letters — Polish eight —
        // and without this the three tables above are searched for every
        // character of every text to find that out. A bit that is clear
        // says no entry can begin here and settles it in a shift and a
        // test; a bit that is set says only that a search is worth
        // making.
        uint64_t starters;
        // And what the language asks for beside the order of its
        // letters. CLDR writes these four as settings rather than as
        // relations, and a collator made for the language starts with
        // them unless the caller says otherwise.
        uint8_t settings;
    }};

    inline constexpr uint8_t SettingShifted = 1;        // [alternate shifted]
    inline constexpr uint8_t SettingUpperFirst = 2;     // [caseFirst upper]
    inline constexpr uint8_t SettingCaseLevel = 4;      // [caseLevel on]
    inline constexpr uint8_t SettingBackwards = 8;      // [backwards 2]

    namespace collate_tables {{
""")
        primary_bytes = emit_trie(f, "Primary", [(lo, hi, first) for lo, hi, first in runs],
                   f"the weight of the letter, where it runs with the code point: {len(runs)} ranges",
                   runs=True)
        f.write(f"\n        // the rest: {len(index)} code points into {len(pool)} elements\n")
        f.write("        inline constexpr Weights Pool[] = {\n")
        for p, s, t in pool:
            f.write(f"            {{0x{p:04X}, 0x{s:04X}, 0x{t:04X}}},\n")
        f.write("        };\n")
        widest = max((n for _, _, n in index), default=0)
        assert widest <= MAX_ELEMENTS, f"a run of {widest} elements against MaxElements {MAX_ELEMENTS}"
        f.write("        inline constexpr WeightRun Runs[] = {\n")
        for cp, at, n in index:
            f.write(f"            {{0x{cp:04X}, {at}, {n}}},\n")
        f.write("        };\n\n")

        # Which row of the index a code point has, as a two-stage table:
        # the row numbers run with the code point wherever several
        # neighbours have entries, so they fit the same shape as the
        # weights. Without it every letter that is not a plain one — every
        # letter with an accent — paid a binary search over all the rows,
        # measured at 15.3 ns against 0.7 for a letter that lands in the
        # ranges, which is 12.7 ns a letter over a sentence.
        rows = []
        for row, (cp, _, _) in enumerate(index):
            if rows and rows[-1][1] + 1 == cp and rows[-1][2] + (rows[-1][1] - rows[-1][0]) + 1 == row + 1:
                rows[-1] = (rows[-1][0], cp, rows[-1][2])
            else:
                rows.append((cp, cp, row + 1))          # zero is "no row"
        for lo, hi, first in rows:
            for c in range(lo, hi + 1):
                assert index[first - 1 + (c - lo)][0] == c
        sizes["CollationRunIndex"] = emit_trie(f, "RunIndex", rows,
            f"the row of the index a code point has, or zero: {len(rows)} ranges", runs=True)
        f.write("\n")
        f.write(f"        // the contractions: {len(contractions)} of them, from {len(starters)} first code points\n")
        f.write("        inline constexpr Weights ContractionPool[] = {\n")
        for p, s, t in cpool:
            f.write(f"            {{0x{p:04X}, 0x{s:04X}, 0x{t:04X}}},\n")
        f.write("        };\n")
        widest = max((n for _, _, _, _, n in crows), default=0)
        assert widest <= MAX_ELEMENTS, f"a contraction of {widest} elements against MaxElements {MAX_ELEMENTS}"
        f.write("        inline constexpr Contraction Contractions[] = {\n")
        for a, b, c, at, n in crows:
            f.write(f"            {{0x{a:04X}, 0x{b:04X}, 0x{c:04X}, {at}, {n}}},\n")
        f.write("        };\n")
        emit_bits(f, "ContractionStarter", coalesce([(c, c) for c in starters]),
                  f"the code points a contraction can begin with: {len(starters)}")
        f.write("\n")
        # 10.1.3 asks the Unified_Ideograph property and the block, not a
        # range anyone may write out of memory: 2B73A looks like an
        # ideograph of extension C and is not one, the block ending at
        # 2B739, and the file puts it where the unassigned go
        unified = properties("PropList.txt", {"Unified_Ideograph"})["Unified_Ideograph"]
        core = [(lo, hi) for lo, hi in unified
                if (0x4E00 <= lo <= 0x9FFF) or (0xF900 <= lo <= 0xFAFF)]
        assert (0x2B73A, 0x2B73A) not in unified and any(lo == 0x2A700 and hi == 0x2B739 for lo, hi in unified)
        sizes["Ideographs"] = emit_bits(f, "UnifiedIdeograph", unified,
                                         f"the ideographs that weigh by arithmetic: {len(unified)} ranges")
        sizes["Ideographs"] += emit_bits(f, "CoreIdeograph", core,
                                          "and those of them in the two blocks that weigh before the rest")
        f.write("\n        // the ranges whose weights are computed rather than stored\n")
        f.write("        inline constexpr ImplicitRange Implicit[] = {\n")
        origin = {}
        for lo, hi, base in sorted(implicit):
            origin.setdefault(base, lo)
        for lo, hi, base in sorted(implicit):
            # Tangut is written as two ranges of one base and the distance
            # is counted from the first of them, so 18D00 weighs after
            # 18AFF rather than beside 17000
            f.write(f"            {{0x{lo:04X}, 0x{hi:04X}, 0x{base:04X}}},"
                    f"   // from 0x{origin[base]:04X}\n")
        assert origin[0xFB00] == 0x17000 and len([1 for _, _, b in implicit if b == 0xFB00]) == 2
        f.write("        };\n")
        sizes.update(write_collate_tailorings(f, tailorings, locale_settings()))
        f.write("    }\n}\n")

    sizes["CollationPrimary"] = primary_bytes
    sizes["CollationPool"] = len(pool) * 6 + len(index) * 8
    sizes["Contractions"] = len(cpool) * 6 + len(crows) * 16 + len(starters) * 4
    print(f"  DUCET: {len(runs)} zakresów, {len(index)} wpisów indeksu, {len(pool)} elementów, "
          f"{len(contractions)} kontrakcji")
    print(f"  tailoringi: {len(tailorings)} języków, "
          f"{sum(len(e) for e in tailorings.values())} wpisów")
    return sizes

#------------------------------------------------------------------------------
# collate: the tailorings, where a language parts from the root order
#------------------------------------------------------------------------------
# The root order puts every letter of every alphabet somewhere, and a
# language then moves a handful of them: Polish wants ą beside a rather
# than beside every other letter with an ogonek, Danish wants å at the
# end of the alphabet, Hungarian wants cs to be one letter between c and
# d. CLDR writes those differences as rules — &A<ą<<<Ą reads "ą comes
# after A, and Ą after ą but only by its case" — and this reads them and
# works out the weights they ask for.
CLDR = "46.0"
SHIFT_P, SHIFT_S, SHIFT_T = 16, 7, 10

# collate.h holds the elements of one letter in Element _pending[MaxElements]
# and fills it through a sink that does not test its bound, so the widest
# entry of any table that stands for a letter has to fit. It does, with room
# for two — the widest run is the 18 of U+FDFA, a contraction is at most
# three and the widest tailored expansion is 22 — and nothing said so until
# now. A twenty-fifth element in some future CLDR would have been a write
# past the end of a buffer on the stack, which is the kind of thing a
# generator is for. Raise both together or not at all.
MAX_ELEMENTS = 24
DEF_S, DEF_T = 0x0020 << SHIFT_S, 0x0002 << SHIFT_T

def cldr_zip():
    path = os.path.join(CACHE, f"cldr-common-{CLDR}.zip")
    if not os.path.exists(path):
        url = f"https://unicode.org/Public/cldr/{CLDR.split('.')[0]}/cldr-common-{CLDR}.zip"
        print(f"  fetching {url}", file=sys.stderr)
        with urllib.request.urlopen(url, timeout=300) as r:
            open(path, "wb").write(r.read())
    return zipfile.ZipFile(path)

def cldr_rules():
    # {locale: the rule text of its default collation}. A file holds
    # several: "search" is what a search box wants, "phonebook" and "eor"
    # are alternatives a caller has to ask for by name, and only the
    # default one is the language's order — German's standard collation is
    # the root order itself and its file carries three others.
    z = cldr_zip()
    raw = {}
    for name in z.namelist():
        if "/collation/" not in name or not name.endswith(".xml"):
            continue
        loc = name.split("/")[-1][:-4]
        xml = z.read(name).decode()
        default = re.search(r"<defaultCollation>(\w+)</defaultCollation>", xml)
        default = default.group(1) if default else "standard"
        text = ""
        for block in re.finditer(r'<collation type="([^"]+)"[^>]*?>(.*?)</collation\s*>', xml, re.S):
            if block.group(1) != default:
                continue
            text = "\n".join(re.findall(r"<cr>\s*<!\[CDATA\[(.*?)\]\]>", block.group(2), re.S))
        # the marks that make an RTL rule readable in an XML file are not
        # part of what it tailors: an anchor beginning with one is
        # completely ignorable and has no place to sort before
        raw[loc] = re.sub(r"#[^\n]*", "", text).translate({0x200E: None, 0x200F: None, 0x061C: None})
    def resolve(loc, seen=()):
        text = raw.get(loc, "")
        def one(m):
            target = m.group(1).split("-u-")[0]
            return "" if target in seen or target not in raw else resolve(target, seen + (loc,))
        return re.sub(r"\[import ([^\]]+)\]", one, text)
    return {loc: resolve(loc) for loc in raw}

#------------------------------------------------------------------------------
# the root order as the engine needs it
#------------------------------------------------------------------------------
class RootOrder:
    def __init__(self):
        entries, implicit, _ = collation_data()
        self.map = {tuple(pts): [(p << SHIFT_P, s << SHIFT_S, t << SHIFT_T) for p, s, t in el]
                    for pts, el in entries}
        self.longest = max(len(k) for k in self.map)
        self.implicit = sorted(implicit)
        self.origin = {}
        for lo, hi, base in self.implicit:
            self.origin.setdefault(base, lo)
        self.unified = properties("PropList.txt", {"Unified_Ideograph"})["Unified_Ideograph"]
        self.core = [(lo, hi) for lo, hi in self.unified
                     if 0x4E00 <= lo <= 0x9FFF or 0xF900 <= lo <= 0xFAFF]
        # what is already taken, level by level
        self.primaries = sorted({e[0] for el in self.map.values() for e in el if e[0]})
        self.secondaries = collections.defaultdict(set)
        self.tertiaries = collections.defaultdict(set)
        for el in self.map.values():
            for p, s, t in el:
                self.secondaries[p].add(s)
                self.tertiaries[(p, s)].add(t)

    def _implicit(self, c):
        for lo, hi, base in self.implicit:
            if lo <= c <= hi:
                return [(base << SHIFT_P, DEF_S, DEF_T),
                        (((c - self.origin[base]) | 0x8000) << SHIFT_P, 0, 0)]
        inside = lambda t: any(lo <= c <= hi for lo, hi in t)
        base = (0xFB40 if inside(self.core) else 0xFB80) if inside(self.unified) else 0xFBC0
        return [((base + (c >> 15)) << SHIFT_P, DEF_S, DEF_T),
                (((c & 0x7FFF) | 0x8000) << SHIFT_P, 0, 0)]

    def elements(self, points, tailored=None):
        points = tuple(points)
        out, i = [], 0
        longest = self.longest
        if tailored:
            longest = max(longest, max(len(k) for k in tailored))
        while i < len(points):
            hit = None
            for n in range(min(longest, len(points) - i), 0, -1):
                key = points[i:i + n]
                if tailored and key in tailored:
                    hit = (n, tailored[key]); break
                if key in self.map:
                    hit = (n, self.map[key]); break
            if hit:
                out += hit[1]; i += hit[0]
            else:
                out += self._implicit(points[i]); i += 1
        return out

#------------------------------------------------------------------------------
# the rules: a chain is a reset and the relations that hang off it
#------------------------------------------------------------------------------
IGNORABLE = {
    "first tertiary ignorable":  (0, 0, 0),
    "last tertiary ignorable":   (0, 0, 0),
    "first secondary ignorable": (0, 0, DEF_T),
    "last secondary ignorable":  (0, 0, DEF_T),
}

TOKEN = re.compile(r"(?P<special>\[[^\]]*\])|(?P<reset>&)"
                   r"|(?P<op><<<<\*|<<<\*|<<\*|<\*|=\*|<<<<|<<<|<<|<|=)"
                   r"|(?P<text>(?:'[^']*'|[^\s&<=\[\]*])+)|(?P<ws>\s+)")

ESCAPE = re.compile(r"\\u([0-9A-Fa-f]{4})|\\U([0-9A-Fa-f]{8})|\\x([0-9A-Fa-f]{2})|\\(.)")

def unescape(s):
    # the rules write a mark or a syntax character as \uXXXX rather than
    # as itself, which a reader of the file cannot tell from the letters
    # around it and a parser must
    def one(m):
        a, b, c, d = m.groups()
        return chr(int(a or b or c, 16)) if (a or b or c) else d
    return ESCAPE.sub(one, s)

def unquote(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "'":
            if i + 1 < len(s) and s[i + 1] == "'":     # '' is one quote
                out.append("'"); i += 2; continue
            j = i + 1
            while j < len(s) and s[j] != "'":
                out.append(s[j]); j += 1
            i = j + 1
        else:
            out.append(s[i]); i += 1
    return "".join(out)

class Skip(Exception):
    pass

def parse_chains(text):
    toks = [(m.lastgroup, m.group()) for m in TOKEN.finditer(text) if m.lastgroup != "ws"]
    chains, i = [], 0
    while i < len(toks):
        kind, val = toks[i]
        if kind != "reset":
            i += 1
            continue
        i += 1
        before, anchor = 0, None
        while i < len(toks) and toks[i][0] == "special":
            m = re.match(r"\[before (\d)\]", toks[i][1])
            if m:
                before = int(m.group(1))
            elif toks[i][1][1:-1] in IGNORABLE:
                anchor = toks[i][1][1:-1]       # a place before every letter
            else:
                raise Skip(toks[i][1])          # [first regular] and friends
            i += 1
        if anchor is None:
            if i >= len(toks) or toks[i][0] != "text":
                raise Skip("reset bez kotwicy")
            anchor = unescape(unquote(toks[i][1])); i += 1
        relations = []
        while i < len(toks) and toks[i][0] != "reset":
            if toks[i][0] == "special":
                i += 1
                continue
            if toks[i][0] != "op":
                i += 1
                continue
            op = toks[i][1]; i += 1
            level = {"<": 1, "<<": 2, "<<<": 3, "<<<<": 4, "=": 5}[op.rstrip("*")]
            if i >= len(toks) or toks[i][0] != "text":
                raise Skip("relacja bez celu")
            target = toks[i][1]; i += 1
            if op.endswith("*"):
                t = unescape(unquote(target))
                j = 0
                while j < len(t):
                    if j + 2 < len(t) and t[j + 1] == "-":
                        for c in range(ord(t[j]), ord(t[j + 2]) + 1):
                            relations.append((level, chr(c), ""))
                        j += 3
                    else:
                        relations.append((level, t[j], ""))
                        j += 1
            else:
                expansion = ""
                if "|" in target:
                    raise Skip("kontekst |")
                if "/" in target:
                    target, expansion = target.split("/", 1)
                relations.append((level, unescape(unquote(target)), unescape(unquote(expansion))))
        chains.append((before, anchor, relations))
    return chains

#------------------------------------------------------------------------------
# the weights: a relation puts its target in the space between what is
# already there. A run of relations at one level shares that space evenly
# rather than halving it, so a list of twenty letters after one anchor
# still fits where halving would run out after sixteen.
#------------------------------------------------------------------------------
def nfd(s):
    return tuple(ord(c) for c in unicodedata.normalize("NFD", s))

def above(xs, v):
    i = bisect.bisect_right(xs, v)
    return xs[i] if i < len(xs) else None

def below(xs, v):
    i = bisect.bisect_left(xs, v)
    return xs[i - 1] if i > 0 else None

STEP = {1: 1 << SHIFT_P, 2: 1 << SHIFT_S, 3: 1 << SHIFT_T}

def runs_of(relations):
    # which relations share one space: those of the same level with nothing
    # shallower between them
    at, runs, open_at = [None] * len(relations), [], {}
    for i, (level, _, _) in enumerate(relations):
        if level == 5:
            continue
        level = min(level, 3)
        for deeper in [l for l in open_at if l > level]:
            del open_at[deeper]
        if level not in open_at:
            runs.append([])
            open_at[level] = len(runs) - 1
        runs[open_at[level]].append(i)
        at[i] = open_at[level]
    return at, runs

# A weight is a fraction while the rules are applied and an integer only at
# the end. Halving a gap at every insertion runs out after ten when ten
# rules reach for the same place — Dzongkha does exactly that — and a rule
# never knows how many more will follow it, so the order is settled first
# and the numbers are handed out afterwards, spread evenly between the two
# root weights the tailored ones fall between.
class Tailoring:
    def __init__(self, root):
        self.root = root
        self.entries = {}                       # the points in NFD -> the elements
        self.primaries = list(root.primaries)
        self._sec_of, self._ter_of = {}, {}

    def sec(self, p):
        if p not in self._sec_of:
            self._sec_of[p] = sorted(self.root.secondaries.get(p) or {DEF_S})
        return self._sec_of[p]

    def ter(self, p, s):
        if (p, s) not in self._ter_of:
            self._ter_of[(p, s)] = sorted(self.root.tertiaries.get((p, s)) or {DEF_T})
        return self._ter_of[(p, s)]

    def elements(self, text):
        return self.root.elements(nfd(text), self.entries)

    def _space(self, cur, level, k, before):
        p, s, t = cur[-1]
        xs, here = ((self.primaries, p) if level == 1 else
                    (self.sec(p), s) if level == 2 else (self.ter(p, s), t))
        if before:
            hi, lo = here, below(xs, here)
            lo = 0 if lo is None else lo
        else:
            lo, hi = here, above(xs, here)
            hi = lo + STEP[level] if hi is None else hi
        values = [Fraction(lo) + Fraction(hi - lo) * (j + 1) / (k + 1) for j in range(k)]
        for v in values:
            bisect.insort(xs, v)
        return values

    @staticmethod
    def _place(cur, level, value):
        p, s, t = cur[-1]
        if level == 1:
            return cur[:-1] + [(value, DEF_S, DEF_T)]
        if level == 2:
            return cur[:-1] + [(p, value, DEF_T)]
        return cur[:-1] + [(p, s, value)]

    def apply(self, chain):
        before, anchor, relations = chain
        cur = [IGNORABLE[anchor]] if anchor in IGNORABLE else self.elements(anchor)
        if not cur:
            raise Skip("kotwica bez wag")
        at, runs = runs_of(relations)
        values = {}
        for i, (level, target, expansion) in enumerate(relations):
            if level == 5:
                new = list(cur)
            else:
                level = min(level, 3)
                run = at[i]
                if run not in values:
                    values[run] = self._space(cur, level, len(runs[run]), before if i == 0 else 0)
                new = self._place(cur, level, values[run][runs[run].index(i)])
            self.entries[nfd(target)] = new + (self.elements(expansion) if expansion else [])
            cur = new

    #--------------------------------------------------------------------------
    def settle(self):
        # the fractions become integers, evenly between the root weights
        # they lie between
        def integers(values, lo, hi, what):
            n = len(values)
            if hi - lo <= n:
                raise Skip(f"brak miejsca {what}: {hi - lo} na {n}")
            return {v: int(lo) + (int(hi) - int(lo)) * (i + 1) // (n + 1)
                    for i, v in enumerate(sorted(values))}

        def settle_level(pairs, roots, step, what):
            # pairs: the tailored values; roots: what is fixed around them
            out, groups = {}, collections.defaultdict(list)
            fixed = sorted(roots)
            for v in pairs:
                i = bisect.bisect_left(fixed, v)
                lo = fixed[i - 1] if i > 0 else 0
                hi = fixed[i] if i < len(fixed) else lo + step
                groups[(lo, hi)].append(v)
            for (lo, hi), vs in groups.items():
                out.update(integers(vs, lo, hi, what))
            return out

        elements = [e for el in self.entries.values() for e in el]
        tailored = {e[0] for e in elements if not isinstance(e[0], int)}
        primary = settle_level(tailored, self.root.primaries, 1 << SHIFT_P, "na pierwszym poziomie")
        fix_p = lambda p: primary.get(p, p)

        by_primary = collections.defaultdict(set)
        for p, s, t in elements:
            if not isinstance(s, int):
                by_primary[fix_p(p)].add(s)
        secondary = {}
        for p, vs in by_primary.items():
            roots = self.root.secondaries.get(p) or {DEF_S}
            secondary.update({(p, v): w for v, w in
                              settle_level(vs, roots, 1 << SHIFT_S, "na drugim poziomie").items()})
        fix_s = lambda p, s: secondary.get((fix_p(p), s), s)

        by_pair = collections.defaultdict(set)
        for p, s, t in elements:
            if not isinstance(t, int):
                by_pair[(fix_p(p), fix_s(p, s))].add(t)
        tertiary = {}
        for (p, s), vs in by_pair.items():
            roots = self.root.tertiaries.get((p, s)) or {DEF_T}
            tertiary.update({(p, s, v): w for v, w in
                             settle_level(vs, roots, 1 << SHIFT_T, "na trzecim poziomie").items()})

        out = {}
        for key, el in self.entries.items():
            settled = []
            for p, s, t in el:
                np = fix_p(p)
                ns = fix_s(p, s)
                settled.append((np, ns, tertiary.get((np, ns, t), t)))
            out[key] = settled
        self.entries = out


def packed_language(tag):
    # the language subtag in four bytes, the way txt::locale holds it
    out = 0
    for c in tag.split("_")[0].split("-")[0].lower():
        out = (out << 8) | ord(c)
    return out

def canonical_forms(key):
    # Every canonically equivalent way of writing a key that is not the
    # key itself: the composed form, and for a letter with several marks
    # the partly composed ones too. A rule is written on the decomposed
    # form, and text arrives in whichever form it arrives in.
    text = "".join(chr(c) for c in key)
    out = set()
    composed = unicodedata.normalize("NFC", text)
    if composed != text:
        out.add(tuple(ord(x) for x in composed))
    if len(key) > 2:
        base, marks = key[0], key[1:]
        for taken in range(1, len(marks)):
            for pick in itertools.combinations(range(len(marks)), taken):
                partly = unicodedata.normalize(
                    "NFC", chr(base) + "".join(chr(marks[i]) for i in pick))
                if len(partly) < 1 + taken:
                    rest = tuple(marks[i] for i in range(len(marks)) if i not in pick)
                    out.add(tuple(ord(x) for x in partly) + rest)
    out.discard(tuple(key))
    return out

def close_canonically(entries):
    # UTS #10's canonical closure. It changes no answer here — the slow
    # path decomposes every text before it looks anything up — but it is
    # what lets a composed letter be read where it stands instead of
    # being taken apart: the fast path of collate.h matches a key, and
    # without the composed keys a language's own letters would walk past
    # the rule that moves them.
    for key in list(entries):
        for form in canonical_forms(key):
            if form not in entries:
                entries[form] = entries[key]
    return entries

def build_tailorings():
    # every language CLDR writes rules for, worked out against the root
    # order. A file may hold several collations — "search" for a search
    # box, "phonebook", "eor" — and only the default one is the language's
    # order: German's standard collation is the root order itself.
    root = RootOrder()
    rules = cldr_rules()
    out, dropped = {}, []
    for loc in sorted(rules):
        if loc in ("zh", "ja", "ko", "root") or "_" in loc or not rules[loc].strip():
            # zh moves every ideograph and would cost three megabytes,
            # ja and ko nearly a hundred kilobytes each; a tag with a
            # script or a region cannot be told from its language by
            # txt::locale, which holds the language subtag alone
            continue
        t = Tailoring(root)
        try:
            for chain in parse_chains(rules[loc]):
                t.apply(chain)
            t.settle()
        except Skip as why:
            dropped.append((loc, str(why)))
            continue
        if t.entries:
            out[loc] = close_canonically(t.entries)
    # The collator settles the canonical order one combining sequence at
    # a time, and takes a sequence to end where a code point of class zero
    # begins — except for the few whose decomposition begins with a mark,
    # which belong with the sequence before them. collate.h names them one
    # by one, so this is where they are checked to be those three.
    joining = [c for c in range(0x110000)
               if not unicodedata.combining(chr(c))
               and unicodedata.decomposition(chr(c))
               and not unicodedata.decomposition(chr(c)).startswith("<")
               and unicodedata.combining(chr(int(unicodedata.decomposition(chr(c)).split()[0], 16)))]
    assert joining == [0x0F73, 0x0F75, 0x0F81], [hex(c) for c in joining]
    assert not dropped, dropped
    # a file whose only rule is a setting — Russian asks for the Cyrillic
    # letters to come before the Latin ones and for nothing else — has
    # nothing to tailor, but one that relates two letters and comes out
    # empty was misread
    relates = lambda text: any(c in re.sub(r"\[[^\]]*\]", "", text) for c in "<=")
    silent = [loc for loc in rules if relates(rules[loc]) and loc not in out
              and loc not in ("zh", "ja", "ko", "root") and "_" not in loc]
    assert not silent, f"reguły bez wpisów: {silent}"
    check_reordering(rules, out)
    return out

# The one setting of a language's rules that is read and not honoured,
# and the reason it may not be quiet about it: a setting silently thrown
# away is exactly the fault that the four before it were. [reorder] moves
# a whole script rather than a letter — Cyrillic before Latin for
# Belarusian, the ten Indic scripts in an order of its own for each of
# them — and honouring it means partitioning the space of first weights
# by script, which DUCET does not do: of the 18538 first weights that
# letters own, 72 are owned by letters of more than one script, and 165
# scripts fall into 182 runs of one. The partition exists in the CLDR
# root table (FractionalUCA writes it as a lead byte per script), which
# is a different weighting of the whole root from the DUCET this module
# is built on. So it is a change of the root order and not a setting,
# and until it is made this prints who is waiting for it.
REORDERING = (
    "ar as az be bn bo bs cu dz fa gu hi hr hy kk km kn kok ku ky mk ml mr my or "
    "pa ps si ta te th ug uk ur yi"
).split()

def check_reordering(rules, tailorings):
    asked = sorted(loc for loc in tailorings if "[reorder " in rules.get(loc, ""))
    # A language that starts or stops asking is a change worth seeing,
    # not a line that scrolls past
    assert asked == REORDERING, ("[reorder] — zmienił się zbiór języków",
                                 sorted(set(asked) ^ set(REORDERING)))
    print(f"  [reorder] czytany i niehonorowany: {len(asked)} z {len(tailorings)} języków "
          f"({' '.join(asked)})")
    return asked

SETTING_BITS = (("[alternate shifted]", 1), ("[caseFirst upper]", 2),
                ("[caseLevel on]", 4), ("[backwards 2]", 8))

def locale_settings():
    # The four settings of CLDR a collator can honour, as the bits the
    # table carries. A language whose rules ask for one of them gets it
    # from its locale rather than from the caller: Danish sorts capitals
    # first, Thai shifts its punctuation aside, Church Slavonic asks for
    # all of the first three and for its accents to be read from the end
    # of the word. What is written here and not honoured is [reorder],
    # which moves a whole script rather than a letter.
    rules = cldr_rules()
    out = {}
    for loc, text in rules.items():
        bits = 0
        for name, bit in SETTING_BITS:
            if name in text:
                bits |= bit
        if bits:
            out[loc] = bits
    return out

def write_collate_tailorings(f, tailorings, settings):
    points, sequences, expansions = [], [], []
    point_pool, element_pool = [], []
    index = []
    for loc, entries in sorted(tailorings.items(), key=lambda kv: packed_language(kv[0])):
        p0, s0, e0 = len(points), len(sequences), len(expansions)
        for key, elements in sorted(entries.items()):
            if len(elements) > 1:
                expansions.append((len(point_pool), len(key), len(elements), len(element_pool)))
                point_pool += list(key)
                element_pool += elements
            elif len(key) == 1:
                points.append((key[0],) + tuple(elements[0]))
            else:
                sequences.append((len(point_pool), len(key)) + tuple(elements[0]))
                point_pool += list(key)
        points[p0:] = sorted(points[p0:])
        starters = 0
        for key in entries:
            starters |= 1 << (key[0] & 63)
        index.append((packed_language(loc), p0, len(points) - p0, s0, len(sequences) - s0,
                      e0, len(expansions) - e0, loc, starters, settings.get(loc, 0)))
    assert len(point_pool) < 0x10000 and len(element_pool) < 0x10000
    assert max(len(sequences), len(expansions), len(points)) < 0x10000
    assert all(n < 256 for _, n, *_ in sequences) and all(n < 256 for _, n, m, _ in expansions)

    f.write(f"""
        //----------------------------------------------------------------
        // the tailorings: {len(tailorings)} languages, {sum(len(e) for e in tailorings.values())} entries
        //----------------------------------------------------------------
        // one code point that weighs its own way
        inline constexpr TailoredPoint TailoredPoints[] = {{
""")
    for cp, p, s, t in points:
        f.write(f"            {{0x{cp:04X}, 0x{p:08X}, 0x{s:04X}, 0x{t:04X}}},\n")
    f.write("        };\n\n        // several code points that weigh as one letter\n")
    f.write("        inline constexpr TailoredSequence TailoredSequences[] = {\n")
    for at, n, p, s, t in sequences:
        f.write(f"            {{{at}, {n}, 0x{p:08X}, 0x{s:04X}, 0x{t:04X}}},\n")
    f.write("        };\n\n        // and what weighs as more than one letter\n")
    widest = max((size for _, _, size, _ in expansions), default=0)
    assert widest <= MAX_ELEMENTS, f"an expansion of {widest} elements against MaxElements {MAX_ELEMENTS}"
    f.write("        inline constexpr TailoredExpansion TailoredExpansions[] = {\n")
    for at, n, size, el in expansions:
        f.write(f"            {{{at}, {n}, {size}, {el}}},\n")
    f.write("        };\n\n")
    f.write(f"        // the code points of the two tables above: {len(point_pool)}\n")
    f.write("        inline constexpr char32_t TailoringPoints[] = {\n")
    for i in range(0, len(point_pool), 8):
        f.write("            " + " ".join(f"0x{c:04X}," for c in point_pool[i:i + 8]) + "\n")
    f.write("        };\n\n")
    f.write(f"        // and the elements of the expansions: {len(element_pool)}\n")
    f.write("        inline constexpr Element TailoringElements[] = {\n")
    for p, s, t in element_pool:
        f.write(f"            {{0x{p:08X}, 0x{s:04X}, 0x{t:04X}}},\n")
    f.write("        };\n\n        // the languages, by the subtag txt::locale holds\n")
    f.write("        inline constexpr Tailoring Tailorings[] = {\n")
    for language, p0, pn, s0, sn, e0, en, loc, starters, bits in index:
        f.write(f"            {{0x{language:08X}, {p0}, {pn}, {s0}, {sn}, {e0}, {en}, "
                f"0x{starters:016X}ull, {bits}}},   // {loc}\n")
    f.write("        };\n")

    bits = [bin(row[8]).count("1") for row in index]
    print(f"  maski: średnio {sum(bits) / len(bits):.1f} z 64 bitów, najwięcej {max(bits)}, "
          f"najmniej {min(bits)}")
    return {"Tailorings": len(points) * 12 + len(sequences) * 12 + len(expansions) * 8
                          + len(point_pool) * 4 + len(element_pool) * 8 + len(index) * 32}


# What a language's rules touch, in the order ICU puts it. ICU is an
# implementation of the same standard from other hands and other data, so
# it is the oracle for the tailorings the way the UCD's own files are the
# oracle for everything else in this module. Its answers are written into
# the test header and committed, so an ordinary run of this generator does
# not need it — only --icu rewrites them, and that needs PyICU.
#
# The settings a language asks for are part of its order and are honoured
# on both sides: the table carries them (locale_settings) and ICU takes
# them from the same rules, so the four languages that ask for one of
# them are compared like any other. What is written in the rules and
# honoured by neither side is [reorder], which moves a whole script
# rather than a letter; it changes nothing among the letters of one
# script, which is all these texts are.
def write_collate_locale_tests():
    import icu
    tailorings = build_tailorings()
    rules = cldr_rules()
    kept, settings, absent = {}, {}, []
    for loc in tailorings:
        collator = icu.Collator.createInstance(icu.Locale(loc))
        if not collator.getRules():
            absent.append(loc)          # ICU carries no tailoring for it
            continue
        words = []
        for _, anchor, relations in parse_chains(rules[loc]):
            words += [anchor] + [target for _, target, _ in relations]
        words = [w for w in dict.fromkeys(words)
                 if w and w not in IGNORABLE and all(unicodedata.category(c) != "Cn" for c in w)]
        if len(words) > 1:
            kept[loc] = sorted(words, key=functools.cmp_to_key(collator.compare))
    with open("tests/txt/collate_locale_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py --icu from the collation rules of
// CLDR {CLDR} and the order ICU {icu.ICU_VERSION} puts them in: do not edit.
// {len(kept)} languages, {sum(len(v) for v in kept.values())} texts, each language's
// texts in the order ICU sorts them, so that every one of them must
// compare at or before the one after it.
//
// Left out: the {len(absent)} languages for which ICU has no tailoring of its
// own ({', '.join(sorted(absent))}).

namespace ucd {{
    struct LocaleOrder {{
        const char* language;
        const char* const* texts;
        size_t size;
    }};

""")
        for loc, words in sorted(kept.items()):
            f.write(f"    inline constexpr const char* Order_{loc}[] = {{\n")
            for w in words:
                f.write("        " + json.dumps(w, ensure_ascii=False) + ",\n")
            f.write("    };\n")
        f.write("\n    inline constexpr LocaleOrder LocaleOrders[] = {\n")
        for loc in sorted(kept):
            f.write(f'        {{"{loc}", Order_{loc}, std::size(Order_{loc})}},\n')
        f.write("    };\n}\n")
    print(f"  wektory locale: {len(kept)} języków, {sum(len(v) for v in kept.values())} tekstów; "
          f"pominięte {sorted(absent)} (ICU bez danych)")

# What the settings make of a list of texts, in the order ICU puts it.
# The settings are the same standard from other hands, and they are
# exactly where an implementation of them can be subtly wrong: a case
# level that outranks the wrong difference, a fourth level written into a
# key where the comparison does not look at one, a number that sorts by
# its digits rather than by its value. So the oracle is asked for the
# order and for the sign of every neighbouring pair, ties included, and
# both the comparison and the sort key are held to it.
OPTION_WORDS = [
    # case, accents, and the two together
    "resume", "résumé", "RESUME", "Resume", "RÉSUMÉ", "resumé", "Résumé",
    "cote", "côte", "coté", "côté", "COTE", "Coté",
    "ada", "Ala", "ALA", "ala", "zebra", "Zebra",
    # punctuation and spaces, which the fourth level is about
    "de luxe", "de-luxe", "deluxe", "De-Luxe", "DE LUXE", "de.luxe",
    "re-sume", "re sume", "re'sume",
    # numbers, which are a level of their own when they are asked for
    "plik2", "plik9", "plik10", "plik09", "plik100", "plik1000",
    "x2y", "x10y", "007", "7", "70", "0",
    # and a few letters a language moves
    "aa", "å", "ø", "æ", "ħaġar", "Ħaġar",
]

OPTION_COMBOS = [
    # (locale, strength, alternate, caseFirst, caseLevel, backwards, numeric)
    ("", "primary", "non-ignorable", "off", False, False, False),
    ("", "secondary", "non-ignorable", "off", False, False, False),
    ("", "tertiary", "non-ignorable", "off", False, False, False),
    ("", "quaternary", "non-ignorable", "off", False, False, False),
    ("", "primary", "shifted", "off", False, False, False),
    ("", "secondary", "shifted", "off", False, False, False),
    ("", "tertiary", "shifted", "off", False, False, False),
    ("", "quaternary", "shifted", "off", False, False, False),
    ("", "primary", "non-ignorable", "off", True, False, False),
    ("", "secondary", "non-ignorable", "off", True, False, False),
    ("", "tertiary", "non-ignorable", "off", True, False, False),
    ("", "primary", "non-ignorable", "upper", True, False, False),
    ("", "secondary", "non-ignorable", "upper", True, False, False),
    ("", "tertiary", "non-ignorable", "upper", True, False, False),
    ("", "tertiary", "non-ignorable", "upper", False, False, False),
    ("", "tertiary", "non-ignorable", "lower", False, False, False),
    ("", "secondary", "non-ignorable", "off", False, True, False),
    ("", "tertiary", "non-ignorable", "off", False, True, False),
    ("", "tertiary", "non-ignorable", "upper", True, True, False),
    ("", "primary", "non-ignorable", "off", False, False, True),
    ("", "secondary", "non-ignorable", "off", False, False, True),
    ("", "tertiary", "non-ignorable", "off", False, False, True),
    ("", "tertiary", "shifted", "upper", True, False, True),
    ("", "quaternary", "shifted", "off", True, True, True),
    # and the four languages whose own rules carry a setting
    ("da", "tertiary", None, None, None, None, False),
    ("mt", "tertiary", None, None, None, None, False),
    ("cu", "tertiary", None, None, None, None, False),
    ("th", "tertiary", None, None, None, None, False),
    ("da", "tertiary", None, "lower", None, None, False),
    ("pl", "tertiary", None, None, None, None, True),
]

def write_collate_option_tests():
    import icu
    strengths = {"primary": icu.Collator.PRIMARY, "secondary": icu.Collator.SECONDARY,
                 "tertiary": icu.Collator.TERTIARY, "quaternary": icu.Collator.QUATERNARY}
    alternate = {"non-ignorable": icu.UCollAttributeValue.NON_IGNORABLE,
                 "shifted": icu.UCollAttributeValue.SHIFTED}
    first = {"off": icu.UCollAttributeValue.OFF,
             "upper": icu.UCollAttributeValue.UPPER_FIRST,
             "lower": icu.UCollAttributeValue.LOWER_FIRST}
    rows, absent = [], []
    for loc, strength, alt, case_first, case_level, backwards, numeric in OPTION_COMBOS:
        c = icu.Collator.createInstance(icu.Locale(loc) if loc else icu.Locale.getRoot())
        if loc and not c.getRules():
            # ICU carries no tailoring of its own for that language, so
            # it cannot say what the language asks for: Church Slavonic
            # is in CLDR 46 with three settings and is not in the data
            # ICU 78 is built from
            absent.append(loc)
            continue
        c.setStrength(strengths[strength])
        if alt is not None:
            c.setAttribute(icu.UCollAttribute.ALTERNATE_HANDLING, alternate[alt])
        if case_first is not None:
            c.setAttribute(icu.UCollAttribute.CASE_FIRST, first[case_first])
        if case_level is not None:
            c.setAttribute(icu.UCollAttribute.CASE_LEVEL,
                           icu.UCollAttributeValue.ON if case_level else icu.UCollAttributeValue.OFF)
        if backwards is not None:
            c.setAttribute(icu.UCollAttribute.FRENCH_COLLATION,
                           icu.UCollAttributeValue.ON if backwards else icu.UCollAttributeValue.OFF)
        c.setAttribute(icu.UCollAttribute.NUMERIC_COLLATION,
                       icu.UCollAttributeValue.ON if numeric else icu.UCollAttributeValue.OFF)
        words = sorted(OPTION_WORDS, key=functools.cmp_to_key(c.compare))
        signs = [(-1 if c.compare(words[i], words[i + 1]) < 0 else 0)
                 for i in range(len(words) - 1)]
        assert all(c.compare(words[i], words[i + 1]) <= 0 for i in range(len(words) - 1))
        rows.append((loc, strength, alt, case_first, case_level, backwards, numeric, words, signs))
    names = {None: "as the language asks", "non-ignorable": "counted", "shifted": "shifted",
             "off": "natural", "upper": "upper_first", "lower": "lower_first"}
    with open("tests/txt/collate_option_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py --icu: do not edit. What ICU
// {icu.ICU_VERSION} makes of {len(OPTION_WORDS)} texts under {len(rows)} settings of the collator —
// the order it puts them in, and for every neighbouring pair whether it
// calls them different or equal. Both the comparison and the sort key
// are held to it, since a setting that changes one and not the other is
// the fault these are here to catch.
//
// A value of `Nothing` for a setting means the language's own answer:
// those rows are the languages whose CLDR rules carry a setting, where
// the collator is made with the locale alone. Left out: {', '.join(absent) or 'nothing'},
// for which ICU carries no tailoring of its own and so cannot be asked.

namespace ucd {{
    inline constexpr int8_t Nothing = -1;   // the setting the caller did not give

    struct OptionOrder {{
        const char* language;
        uint8_t strength;                   // 0 primary … 3 quaternary
        int8_t punctuation;                 // 0 counted, 1 shifted
        int8_t case_order;                  // 0 natural, 1 upper first, 2 lower first
        int8_t case_level;
        int8_t backwards;
        bool numeric;
        const char* const* texts;
        const int8_t* signs;                // -1 before the next text, 0 equal to it
        size_t size;
    }};

""")
        strength_of = {"primary": 0, "secondary": 1, "tertiary": 2, "quaternary": 3}
        punctuation_of = {None: -1, "non-ignorable": 0, "shifted": 1}
        case_of = {None: -1, "off": 0, "upper": 1, "lower": 2}
        flag_of = {None: -1, False: 0, True: 1}
        for n, (loc, strength, alt, case_first, case_level, backwards, numeric, words, signs) in enumerate(rows):
            f.write(f"    // {loc or 'root'}: {strength}, punctuation {names[alt]}, "
                    f"case {names[case_first]}"
                    f"{', case level' if case_level else ''}"
                    f"{', backwards' if backwards else ''}"
                    f"{', numeric' if numeric else ''}\n")
            f.write(f"    inline constexpr const char* OptionTexts{n}[] = {{\n")
            for w in words:
                f.write("        " + json.dumps(w, ensure_ascii=False) + ",\n")
            f.write("    };\n")
            f.write(f"    inline constexpr int8_t OptionSigns{n}[] = {{"
                    + ", ".join(str(x) for x in signs) + "};\n\n")
        f.write("    inline constexpr OptionOrder OptionOrders[] = {\n")
        for n, (loc, strength, alt, case_first, case_level, backwards, numeric, words, signs) in enumerate(rows):
            f.write(f'        {{"{loc}", {strength_of[strength]}, {punctuation_of[alt]}, '
                    f'{case_of[case_first]}, {flag_of[case_level]}, {flag_of[backwards]}, '
                    f'{"true" if numeric else "false"}, OptionTexts{n}, OptionSigns{n}, '
                    f'std::size(OptionTexts{n})}},\n')
        f.write("    };\n}\n")
    print(f"  wektory ustawień: {len(rows)} ustawień po {len(OPTION_WORDS)} tekstów; "
          f"pominięte {sorted(absent)} (ICU bez danych)")

def write_collate_tests(every=3):
    # CollationTest of the UCA: a list of texts, each of which must sort
    # at or after the one before it. The short form of the file is the
    # same test with the sort keys left out of the comments.
    import zipfile
    path = os.path.join(CACHE, "UCA-CollationTest.zip")
    if not os.path.exists(path):
        url = f"https://www.unicode.org/Public/UCA/{VERSION}/CollationTest.zip"
        print(f"  fetching {url}", file=sys.stderr)
        with urllib.request.urlopen(url, timeout=120) as r:
            open(path, "wb").write(r.read())
    data = zipfile.ZipFile(path).read("CollationTest/CollationTest_NON_IGNORABLE_SHORT.txt").decode()
    cases = [l.split("#")[0].split(";")[0].strip() for l in data.split("\n")
             if l and not l.startswith("#") and not l.startswith("@")]
    cases = [c for c in cases if c]
    # A lone surrogate is a code point the file names and UTF-8 cannot
    # write, so the library's string cannot hold one: those lines decode
    # to a replacement character and the order they test is not the
    # library's to keep
    lone = lambda c: any(0xD800 <= int(x, 16) <= 0xDFFF for x in c.split())
    surrogates = sum(1 for c in cases if lone(c))
    cases = [c for c in cases if not lone(c)]
    kept = cases[::every]
    with open("tests/txt/collate_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from the Unicode Collation
// Algorithm {VERSION}: do not edit. CollationTest, {len(kept)} of its
// {len(cases)} lines: every one of them must sort at or after the line
// before it, by the root order with the variable weights counted
// (NON_IGNORABLE). The {surrogates} lines that name a lone surrogate are left
// out: UTF-8 cannot write one, so a string cannot hold one. The whole
// file is run by hand before a change lands.

namespace ucd {{
    inline constexpr const char* CollationCases[] = {{
""")
        for c in kept:
            f.write(f'        "{c}",\n')
        f.write("    };\n}\n")
    print(f"  wektory collation: {len(kept)} z {len(cases)}")

# The vectors for regex.h, with Python's re as the oracle. Only where the
# two are defined to answer the same question: re backtracks, so it can be
# asked things this engine refuses, and on three points the two differ by
# design and the pattern is translated rather than the answers compared
# blind — '^' and '$' outside multiline are the edges of the whole text
# here where re's '$' also stands before a trailing newline, \z is re's
# \Z, and a named group is (?<name>) here and (?P<name>) there.
#
# The oracle has to be held to a clock. Random patterns over three atoms
# and a quantifier produce, every few hundred draws, one that sends re
# into an exponential search — the first run of this generator did not
# return at all — so every question put to it is given a quarter of a
# second and the pattern is dropped when it takes longer. The count of
# those is printed, because it is the number this whole engine exists for.
REGEX_ORACLE_LIMIT = 0.25

class OracleTooSlow(Exception):
    pass

def under_the_clock(f, *args):
    import signal
    def ring(sig, frame):
        raise OracleTooSlow()
    old = signal.signal(signal.SIGALRM, ring)
    signal.setitimer(signal.ITIMER_REAL, REGEX_ORACLE_LIMIT)
    try:
        return f(*args)
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, old)

def regex_vectors():
    import random, re as pyre
    rng = random.Random(20260923)
    atoms = ["a", "b", "c", ".", "[ab]", "[^a]", "[a-c]", r"\d", r"\w", r"\s",
             "ż", "x", " ", "1"]
    quants = ["", "", "", "*", "+", "?", "*?", "+?", "??", "{2}", "{1,3}", "{0,2}", "{2,}"]
    alphabet = ["a", "b", "c", " ", "1", "2", "ż", "\n", "x", "Δ", "б"]

    def piece(depth):
        r = rng.random()
        if depth < 3 and r < 0.18:
            inner = "|".join(sequence(depth + 1) for _ in range(rng.randint(2, 3)))
            head = rng.choice(["(", "(?:", "(?:"])
            return head + inner + ")"
        if depth < 3 and r < 0.30:
            return "(" + sequence(depth + 1) + ")"
        return rng.choice(atoms)

    # A quantifier is only put on something that must consume a code
    # point. A repetition whose body can match nothing is the one place
    # where the two engines answer differently by design — re stops such
    # a loop after the empty turn, where a machine that carries every
    # alternative at once prunes the empty turn and lets a wider one go
    # on — and those patterns belong in a test of their own, not in an
    # oracle that would report a difference of definition as a fault.
    def nullable(expr):
        try:
            return pyre.compile(expr).fullmatch("") is not None
        except pyre.error:
            return True

    def sequence(depth):
        out = []
        for _ in range(rng.randint(1, 4)):
            one = piece(depth)
            quant = rng.choice(quants)
            if quant and nullable(one):
                quant = ""
            out.append(one + quant)
        return "".join(out)

    def pattern():
        p = sequence(0)
        r = rng.random()
        if r < 0.08:
            p = "^" + p
        elif r < 0.14:
            p = p + "$"
        return p

    def text():
        return "".join(rng.choice(alphabet) for _ in range(rng.randint(0, 14)))

    def to_python(p, multiline):
        out = []
        i = 0
        in_class = False
        while i < len(p):
            c = p[i]
            if c == "\\" and i + 1 < len(p):
                out.append(p[i:i + 2])
                i += 2
                continue
            if c == "[":
                in_class = True
            elif c == "]":
                in_class = False
            if not in_class and not multiline and c == "^":
                out.append(r"\A")
            elif not in_class and not multiline and c == "$":
                out.append(r"\Z")
            else:
                out.append(c)
            i += 1
        return "".join(out)

    # re counts in code points and this engine in bytes, since a slice of
    # a text is bytes of it; the first run of the generator compared the
    # two straight and every case with a non-ASCII character before the
    # match came out wrong
    def offsets(t):
        out = [0]
        for c in t:
            out.append(out[-1] + len(c.encode("utf-8")))
        return out

    def spans(m, at):
        parts = ["%d:%d" % (at[m.start()], at[m.end()])]
        for g in range(1, (m.re.groups or 0) + 1):
            parts.append("x" if m.start(g) < 0
                         else "%d:%d" % (at[m.start(g)], at[m.end(g)]))
        return ";".join(parts)

    finds, alls, splits = [], [], []
    slow = 0
    slowest = []
    while len(finds) < 3000:
        p = pattern()
        flags = rng.choice(["", "", "", "i", "s", "m", "im"])
        py = 0
        if "i" in flags:
            py |= pyre.I
        if "m" in flags:
            py |= pyre.M
        if "s" in flags:
            py |= pyre.S
        try:
            rx = pyre.compile(to_python(p, "m" in flags), py)
        except pyre.error:
            continue
        t = text()
        want_all = len(alls) < 800
        want_split = len(splits) < 600 and rx.groups == 0

        at = offsets(t)

        def ask():
            m = rx.search(t)
            one = spans(m, at) if m else "-"
            # Where a pattern can match nothing the two walk the
            # occurrences by different rules — re tries the same position
            # again demanding something wider, RE2 and this engine move
            # on by a code point — so such a pattern is asked for its
            # first match only and left out of the other two sets
            found = list(rx.finditer(t))
            nothing = any(k.start() == k.end() for k in found)
            every = None if nothing or not want_all else \
                ",".join("%d:%d" % (at[k.start()], at[k.end()]) for k in found)
            pieces = None if nothing or not want_split else "\x01".join(rx.split(t))
            return one, every, pieces

        try:
            one, every, pieces = under_the_clock(ask)
        except OracleTooSlow:
            slow += 1
            if len(slowest) < 6:
                slowest.append((p, t))
            continue
        finds.append((p, flags, t, one))
        if every is not None:
            alls.append((p, flags, t, every))
        if pieces is not None:
            splits.append((p, flags, t, pieces))
    return finds, alls, splits, slow, slowest


def emit_bytes(s):
    # every printable ASCII byte as itself and every other as three octal
    # digits: an octal escape takes at most three of them, so a digit
    # after one is a digit and not part of it
    out = []
    for b in s.encode("utf-8"):
        if 0x20 <= b < 0x7F and b not in (0x22, 0x5C):
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def write_regex_tests():
    finds, alls, splits, slow, slowest = regex_vectors()
    with open("tests/txt/regex_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py with Python's re as the oracle: do
// not edit. {len(finds)} patterns asked for their first match, {len(alls)} for every
// match and {len(splits)} for the pieces a split leaves, over random patterns and
// random texts drawn from an alphabet chosen for the edges — an accented
// letter, a Greek and a Cyrillic one, a line feed, a space and a digit.
//
// Only what the two engines are defined to answer the same way is here.
// re backtracks and can be asked things this engine refuses by design, so
// no backreference and no lookaround appears; and on three points the
// pattern is translated rather than the answers compared blind: '^' and
// '$' outside multiline are the edges of the whole text here where re's
// '$' also stands before a trailing newline, \\z is re's \\Z, and a named
// group is (?<name>) here and (?P<name>) there. The rest — leftmost-first,
// greedy against lazy, which group took part in a match, where a
// zero-width match leaves the search — is the same question and is
// compared exactly. The flags are given apart from the pattern and the
// test writes them as (?ims) in front of it.
//
// {slow} patterns were drawn and thrown away because re did not answer about
// them within a quarter of a second. They are the reason this engine is
// written the way it is; the first run of the generator, with no clock on
// the oracle, did not return at all.
//
// A span is written begin:end, the whole match first and then one to a
// group, 'x' for a group that took no part; "-" is no match at all. The
// pieces of a split are joined by \\001, which no text here contains.
//
// No quantifier stands on something that can match nothing: a repetition
// with a body like that is the one place where the two engines differ by
// definition — re stops the loop after the empty turn, where a machine
// carrying every alternative at once prunes the empty turn and lets a
// wider one go on, so (?:a*|b)* finds "aab" here and "aa" there — and
// those patterns are in a test of their own rather than here.
//
// A pattern that can match nothing appears in the first set only. The two
// engines walk the occurrences of such a pattern by different rules — re
// tries the same position again demanding a wider match, where RE2, Go
// and this engine move on by a code point — and that is a difference of
// the walk and not of the matching, so it is left out rather than papered
// over.

namespace ucd {{
    struct RegexCase {{
        const char* pattern;
        const char* flags;
        const char* text;
        const char* expect;
    }};

    inline constexpr RegexCase RegexFindCases[] = {{
""")
        for p, fl, t, e in finds:
            f.write("        {%s, \"%s\", %s, %s},\n" % (emit_bytes(p), fl, emit_bytes(t), emit_bytes(e)))
        f.write("    };\n\n    inline constexpr RegexCase RegexAllCases[] = {\n")
        for p, fl, t, e in alls:
            f.write("        {%s, \"%s\", %s, %s},\n" % (emit_bytes(p), fl, emit_bytes(t), emit_bytes(e)))
        f.write("    };\n\n    inline constexpr RegexCase RegexSplitCases[] = {\n")
        for p, fl, t, e in splits:
            f.write("        {%s, \"%s\", %s, %s},\n" % (emit_bytes(p), fl, emit_bytes(t), emit_bytes(e)))
        f.write("    };\n}\n")
    print(f"  wektory regex: {len(finds)} find, {len(alls)} all, {len(splits)} split")
    print(f"  wzorce odrzucone, bo re nie odpowiedziało w {REGEX_ORACLE_LIMIT}s: {slow}")
    for p, t in slowest:
        print(f"    {p!r} nad {t!r}")

#------------------------------------------------------------------------------
# txt: IDNA (UTS #46, RFC 5890-5894)
#------------------------------------------------------------------------------
IDNA_STATUS = ["valid", "mapped", "ignored", "disallowed", "deviation"]
JOINING = ["u", "t", "d", "l", "r", "c"]
JOINING_UCD = {"U": "u", "T": "t", "D": "d", "L": "l", "R": "r", "C": "c"}

def write_idna_tables():
    # IdnaMappingTable.txt and IdnaTestV2.txt live in a tree of their own
    # beside the UCD, versioned with it, as the collation files do
    def idna_file(name):
        path = os.path.join(CACHE, "IDNA-" + name)
        if not os.path.exists(path):
            os.makedirs(CACHE, exist_ok=True)
            url = f"https://www.unicode.org/Public/idna/{VERSION}/{name}"
            print(f"  fetching {url}", file=sys.stderr)
            with urllib.request.urlopen(url, timeout=60) as r:
                open(path, "w").write(r.read().decode("utf-8"))
        text = open(path).read()
        assert f"Version: {VERSION}" in "\n".join(text.split("\n")[:12]), name
        return text.split("\n")

    # The status of every code point and, for the ones that are mapped or
    # deviate, what they map to. Unicode 16.0 has no disallowed_STD3_*
    # statuses any more: UseSTD3ASCIIRules is handled in the validity
    # criteria alone (UTS #46 section 4.1.1), so five statuses cover the
    # file. The fourth field (NV8, XV8) says the code point is not valid
    # under IDNA2008 proper; UTS #46 processing does not look at it.
    status, mapping = {}, {}
    for line in idna_file("IdnaMappingTable.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        points = fields[0].split("..")
        lo, hi = int(points[0], 16), int(points[-1], 16)
        value = IDNA_STATUS.index(fields[1])
        to = [int(x, 16) for x in fields[2].split()] if len(fields) > 2 and fields[2] else []
        for c in range(lo, hi + 1):
            status[c] = value
            if to:
                mapping[c] = to
    assert len(status) == 0x110000, len(status)
    # The three deviations are the whole of the transitional difference:
    # sharp s and final sigma map, the two joiners map to nothing
    assert sorted(c for c in status if status[c] == IDNA_STATUS.index("deviation")) \
        == [0x00DF, 0x03C2, 0x200C, 0x200D]

    def status_of(c):
        return status[c]
    table = value_ranges(status_of, 0)
    check_values(table, status_of, 0)

    # The mapping is not written out, and this is why. Of the 6350 code
    # points IdnaMappingTable.txt maps, 6347 are exactly what
    # NFKC_Casefold gives, and identifier.h already works NFKC_Casefold
    # out rather than keeping a table of it (the assertion in
    # write_identifier_tables covers every one of the 1114112 code
    # points). So idna.h computes the mapping the same way and carries
    # three exceptions in a switch, and 58.6 KB of table goes.
    #
    # The three: the capital sharp s, which folds to "ss" for everybody
    # else and which UTS #46 sends to the small sharp s so that it lands
    # on a deviation character; and the two ideographic full stops, which
    # are label separators here and ordinary characters everywhere else.
    nfkc_cf = {}
    for value, rs in derived("DerivedNormalizationProps.txt", "NFKC_CF").items():
        mapped = "".join(chr(int(x, 16)) for x in value.split())
        for lo, hi in rs:
            for c in range(lo, hi + 1):
                nfkc_cf[c] = mapped
    ignorable = set()
    for lo, hi in properties("DerivedCoreProperties.txt",
                             {"Default_Ignorable_Code_Point"})["Default_Ignorable_Code_Point"]:
        ignorable.update(range(lo, hi + 1))
    # The zero width joiners are in this list and are not a difference
    # from NFKC_Casefold: the property drops them for being default
    # ignorable, and the header does not drop anything, so it writes them
    # out. Every other code point it maps must not be default ignorable,
    # which is the assertion below.
    IDNA_MAPPING_EXCEPTIONS = {0x1E9E: [0x00DF], 0x3002: [0x2E], 0xFF61: [0x2E],
                               0x200C: [], 0x200D: []}
    off = []
    asked = 0
    for c, value in status.items():
        if value not in (IDNA_STATUS.index("mapped"), IDNA_STATUS.index("deviation")):
            continue
        asked += 1
        to = mapping.get(c, [])
        want = IDNA_MAPPING_EXCEPTIONS.get(c)
        if want is not None:
            assert to == want, f"the exception for {hex(c)} is not what the table says"
            continue
        assert c not in ignorable, f"a mapped code point is default ignorable: {hex(c)}"
        # the line the header takes for a name typed in capitals: in
        # ASCII the standard maps A to Z and nothing else, each to the
        # letter 32 above it
        assert c >= 0x80 or (0x41 <= c <= 0x5A and to == [c + 32]), \
            f"an ASCII code point is mapped and is not a capital letter: {hex(c)}"
        if "".join(chr(x) for x in to) != nfkc_cf.get(c, chr(c)):
            off.append(c)
    assert not off, ("the idna mapping is not NFKC_Casefold at "
                     + ", ".join(hex(c) for c in off[:8]))
    print(f"  idna: {asked} punktów kodowych odwzorowywanych, "
          f"{asked - len(IDNA_MAPPING_EXCEPTIONS)} z nich to dokładnie NFKC_Casefold, "
          f"reszta ({len(IDNA_MAPPING_EXCEPTIONS)}) jest wyjątkiem w nagłówku")

    # The joining type, which only the ContextJ rule of RFC 5892 reads,
    # and only for a label that has a zero width joiner in it: read once
    # in a while rather than per character, so the sorted ranges stay
    joining = {}
    for line in ucd("extracted/DerivedJoiningType.txt"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 2 or fields[1] not in JOINING_UCD:
            continue
        points = fields[0].split("..")
        for c in range(int(points[0], 16), int(points[-1], 16) + 1):
            joining[c] = JOINING.index(JOINING_UCD[fields[1]])
    joining_table = value_ranges(lambda c: joining.get(c, 0), 0)
    check_values(joining_table, lambda c: joining.get(c, 0), 0)

    sizes = {}
    with open("sgcl/txt/detail/idna_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The status of every code point under UTS #46 and the joining types of
// ArabicShaping, which the ContextJ rule of RFC 5892 asks about the
// characters around a zero width joiner.
//
// What a mapped code point becomes is not here: it is NFKC_Casefold for
// 6347 of the 6350 the standard maps, which identifier.h works out rather
// than tabling, and the three that differ are a switch in idna.h. The
// generator asserts that against IdnaMappingTable.txt before it writes
// this file.

#include "tables.h"

namespace sgcl::txt::detail {{
    // The five statuses of the mapping table. Unicode 16.0 dropped
    // disallowed_STD3_valid and disallowed_STD3_mapped: UseSTD3ASCIIRules
    // is a question the validity criteria ask about ASCII, not a status
    // of its own (UTS #46 section 4.1.1).
    enum class idna_status : uint8_t {{
""")
        for name in IDNA_STATUS:
            f.write(f"        {name},\n")
        f.write("""    };

    // Joining_Type: non-joining, transparent, dual, left, right and
    // join-causing, in the order the rule reads them
    enum class joining : uint8_t {
""")
        for name in JOINING:
            f.write(f"        {name},\n")
        f.write("""    };

    namespace idna_tables {
""")
        sizes["IdnaStatus"] = emit_trie(f, "IdnaStatus", table,
                                        f"the status of every code point: {len(table)} ranges")
        f.write("\n")
        sizes["JoiningType"] = emit_split(f, "ValueRange", "JoiningType", joining_table,
                                          f"Joining_Type, non-joining being the default: {len(joining_table)} ranges")
        f.write("    }\n}\n")
    print(f"  idna: {len(table)} zakresów statusu, {len(joining_table)} zakresów typu łączenia")

    # IdnaTestV2.txt, the conformance file. The escapes are resolved here
    # and the columns' "blank means the same as" rules applied, so that
    # the test reads four strings and three flags and nothing else. Two
    # lines carry an unpaired surrogate, which no UTF-8 string can hold,
    # and are left out.
    def unescape_test(s):
        out, i = [], 0
        while i < len(s):
            if s.startswith("\\u", i):
                out.append(chr(int(s[i + 2:i + 6], 16)))
                i += 6
            elif s.startswith("\\x{", i):
                end = s.index("}", i)
                out.append(chr(int(s[i + 3:end], 16)))
                i = end + 1
            else:
                out.append(s[i])
                i += 1
        return "".join(out)

    def literal(s):
        # Octal escapes rather than hex: a hex escape in C has no bound
        # and would swallow the digit of the byte after it
        out = []
        for b in s.encode("utf-8"):
            if b == 0x22 or b == 0x5C:
                out.append("\\" + chr(b))
            elif 0x20 <= b < 0x7F:
                out.append(chr(b))
            else:
                out.append(f"\\{b:03o}")
        return '"' + "".join(out) + '"'

    cases, skipped = [], 0
    for number, line in enumerate(idna_file("IdnaTestV2.txt"), 1):
        line = line.split("#")[0].strip()
        if not line or line.startswith("@"):
            continue
        fields = [f.strip() for f in line.split(";")]
        if len(fields) < 7:
            continue
        def column(i, fallback):
            v = fields[i]
            return "" if v == '""' else (fallback if v == "" else unescape_test(v))
        def errors(i, fallback):
            v = fields[i]
            return fallback if v == "" else (v != "[]")
        source = column(0, "")
        to_unicode = column(1, source)
        unicode_bad = errors(2, False)
        to_ascii_n = column(3, to_unicode)
        ascii_n_bad = errors(4, unicode_bad)
        to_ascii_t = column(5, to_ascii_n)
        ascii_t_bad = errors(6, ascii_n_bad)
        if any(0xD800 <= ord(c) <= 0xDFFF for c in source + to_unicode + to_ascii_n + to_ascii_t):
            skipped += 1
            continue
        cases.append((number, source, to_unicode, unicode_bad,
                      to_ascii_n, ascii_n_bad, to_ascii_t, ascii_t_bad))

    with open("tests/txt/idna_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// IdnaTestV2.txt, {len(cases)} of its {len(cases) + skipped} cases, with the
// file's escapes resolved and its "a blank column means the same as" rules
// applied. The {skipped} left out carry an unpaired surrogate, which no UTF-8
// string can hold. The statuses are kept only as whether there was an
// error: the file itself says an implementation need not reproduce the
// codes. They are the ones for every flag on — CheckHyphens, CheckBidi,
// CheckJoiners, UseSTD3ASCIIRules and VerifyDnsLength.

namespace ucd {{
    struct IdnaCase {{
        int line;               // where it is in IdnaTestV2.txt
        const char* source;
        const char* to_unicode;
        bool unicode_error;
        const char* to_ascii;           // nontransitional
        bool ascii_error;
        const char* to_ascii_transitional;
        bool ascii_transitional_error;
    }};

    inline constexpr IdnaCase IdnaTests[] = {{
""")
        for number, s, u, ub, a, ab, t, tb in cases:
            f.write(f"        {{{number}, {literal(s)}, {literal(u)}, {'true' if ub else 'false'}, "
                    f"{literal(a)}, {'true' if ab else 'false'}, "
                    f"{literal(t)}, {'true' if tb else 'false'}}},\n")
        f.write("    };\n}\n")
    print(f"  wektory IdnaTestV2: {len(cases)} z {len(cases) + skipped}")
    return sizes

#------------------------------------------------------------------------------
# txt: identifiers (UAX #31) and the security mechanisms (UTS #39)
#------------------------------------------------------------------------------
# Eleven of the twelve Identifier_Type values of UTS #39 as the bits of one
# word: a code point carries a set of them rather than one, and the set is
# what says why a character is not allowed in an identifier. The twelfth,
# Not_Character, is the file's @missing value and so is the empty set.
IDENTIFIER_TYPES = ["deprecated", "default_ignorable", "not_nfkc", "not_xid", "exclusion",
                    "obsolete", "technical", "uncommon_use", "limited_use", "inclusion",
                    "recommended"]
JOINING_TYPES = ["u", "c", "d", "l", "r", "t"]

def write_identifier_tables():
    sizes = {}

    # The files of UTS #39 live beside the UCD in a tree of their own,
    # versioned with it and naming the version in the same header
    def security(name):
        path = os.path.join(CACHE, "security-" + name)
        if not os.path.exists(path):
            os.makedirs(CACHE, exist_ok=True)
            url = f"https://www.unicode.org/Public/security/{VERSION}/{name}"
            print(f"  fetching {url}", file=sys.stderr)
            with urllib.request.urlopen(url, timeout=60) as r:
                open(path, "w").write(r.read().decode("utf-8"))
        text = open(path).read()
        assert f"Version: {VERSION}" in text, f"{name} does not declare Unicode {VERSION}"
        return text.split("\n")

    def rows(lines):
        for line in lines:
            line = line.split("#")[0].strip()
            if line:
                yield [f.strip() for f in line.split(";")]

    def bounds(field):
        parts = field.split("..")
        return int(parts[0], 16), int(parts[-1], 16)

    def as_ranges(codes):
        out = []
        for c in sorted(codes):
            if out and out[-1][1] == c - 1:
                out[-1] = (out[-1][0], c)
            else:
                out.append((c, c))
        return out

    #--- UAX #31: XID_Start and XID_Continue -------------------------------
    # The X forms rather than ID_Start and ID_Continue: those two are not
    # closed under normalization — an identifier written with them can
    # stop being one when it is put into NFKC — and the X forms are the
    # fix. Python's own tables are the second source: str.isidentifier()
    # is XID_Start with the underscore added, and "a" + c is XID_Continue.
    xid = properties("DerivedCoreProperties.txt", {"XID_Start", "XID_Continue"})
    python_start, python_continue = set(), set()
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        if chr(c).isidentifier():
            python_start.add(c)
        if ("a" + chr(c)).isidentifier():
            python_continue.add(c)
    python_start.discard(ord("_"))
    assert as_ranges(python_start) == xid["XID_Start"], "XID_Start: the UCD and Python disagree"
    assert as_ranges(python_continue) == xid["XID_Continue"], "XID_Continue: the UCD and Python disagree"

    #--- the joining types, for rule UAX31-R1a -----------------------------
    # A zero width non-joiner belongs inside an identifier where it does
    # the work of a joining control: between a letter that joins to the
    # left and one that joins to the right, the transparent marks aside
    joining = {}
    for f in rows(ucd("extracted/DerivedJoiningType.txt")):
        lo, hi = bounds(f[0])
        for c in range(lo, hi + 1):
            joining[c] = f[1]
    assert set(joining.values()) <= set("CDLRTU"), sorted(set(joining.values()))
    joining_of = lambda c: JOINING_TYPES.index(joining.get(c, "U").lower())
    joining_table = value_ranges(joining_of, 0)
    check_values(joining_table, joining_of, 0)

    #--- NFKC_Casefold -----------------------------------------------------
    # The mapping itself is not written out. NFKC_CF(c) is NFC(fold(NFKD(c)))
    # with the default ignorable code points dropped — for every one of the
    # 1114112, which is what the loop below asserts — so the module works
    # it out from tables it already carries and keeps only the set of code
    # points the mapping changes, for the quick check, and the set it drops.
    nfkc_cf = {}
    for value, rs in derived("DerivedNormalizationProps.txt", "NFKC_CF").items():
        mapped = "".join(chr(int(x, 16)) for x in value.split())
        for lo, hi in rs:
            for c in range(lo, hi + 1):
                nfkc_cf[c] = mapped
    ignorable = set()
    for lo, hi in properties("DerivedCoreProperties.txt",
                             {"Default_Ignorable_Code_Point"})["Default_Ignorable_Code_Point"]:
        ignorable.update(range(lo, hi + 1))
    assert {c for c, v in nfkc_cf.items() if v == ""} == ignorable, \
        "what NFKC_CF drops is not exactly the default ignorable code points"
    for c in range(0x110000):
        if 0xD800 <= c <= 0xDFFF:
            continue
        decomposed = unicodedata.normalize("NFKD", chr(c))
        folded = "".join(ch.casefold() for ch in decomposed)
        assert "".join(ch for ch in unicodedata.normalize("NFC", folded)
                       if ord(ch) not in ignorable) == nfkc_cf.get(c, chr(c)), \
            f"NFKC_CF is not derivable at {hex(c)}"
        # the two things the header leans on so that it can fold in one
        # pass: what comes out of folding a decomposed code point is
        # decomposed already, and neither step brings in a code point
        # that the mapping would have to drop
        assert unicodedata.normalize("NFKD", folded) == folded, f"a second round is needed at {hex(c)}"
        assert c in ignorable or not any(ord(ch) in ignorable for ch in folded), \
            f"an ignorable code point appears inside the folding of {hex(c)}"
    changes = as_ranges(c for c, v in nfkc_cf.items() if v != chr(c))

    #--- UTS #39: the confusables and the two identifier properties --------
    confusable = {}
    for f in rows(security("confusables.txt")):
        source = f[0].split()
        assert len(source) == 1, f          # every source is one code point in 16.0
        confusable[int(source[0], 16)] = [int(x, 16) for x in f[1].split()]

    allowed = set()
    for f in rows(security("IdentifierStatus.txt")):
        assert f[1] == "Allowed", f         # Restricted is the file's @missing value
        lo, hi = bounds(f[0])
        allowed.update(range(lo, hi + 1))

    type_of = {}
    for f in rows(security("IdentifierType.txt")):
        lo, hi = bounds(f[0])
        mask = 0
        for name in f[1].split():
            assert name != "Not_Character", f   # the @missing value, never written out
            mask |= 1 << IDENTIFIER_TYPES.index(name.lower())
        for c in range(lo, hi + 1):
            type_of[c] = mask
    recommended = 1 << IDENTIFIER_TYPES.index("recommended")
    inclusion = 1 << IDENTIFIER_TYPES.index("inclusion")
    # Table 1 of UTS #39: Allowed is exactly Recommended or Inclusion
    assert {c for c, m in type_of.items() if m & (recommended | inclusion)} == allowed, \
        "Identifier_Status and Identifier_Type disagree"
    types = value_ranges(lambda c: type_of.get(c, 0), 0)

    with open("sgcl/txt/detail/identifier_tables.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// What UAX #31 and UTS #39 need — XID_Start and XID_Continue, the joining
// types rule R1a is written with, the confusable prototypes and the two
// identifier properties of the security mechanisms. The NFKC_Casefold
// mapping is not here: it is NFC(fold(NFKD(c))) without the default
// ignorable code points, which the module has the tables for already, so
// only the set of code points it changes is kept.

#include "tables.h"

namespace sgcl::txt::detail {{
    // Eleven of the twelve Identifier_Type values of UTS #39 as the bits
    // of one word: a code point carries a set of them rather than one.
    // The twelfth, Not_Character, is the empty set — the value of every
    // code point the file does not name.
    enum class identifier_type : uint16_t {{
        not_character = 0,
""")
        for i, name in enumerate(IDENTIFIER_TYPES):
            f.write(f"        {name}{' ' * max(1, 18 - len(name))}= 1 << {i},\n")
        f.write("""    };

    constexpr identifier_type operator|(identifier_type a, identifier_type b) noexcept {
        return identifier_type(uint16_t(a) | uint16_t(b));
    }

    constexpr identifier_type operator&(identifier_type a, identifier_type b) noexcept {
        return identifier_type(uint16_t(a) & uint16_t(b));
    }

    // The joining type of ArabicShaping.txt, for the one rule that asks
    // about it: u (non joining) is the default and carries no range
    enum class joining_type : uint8_t {
""")
        for name in JOINING_TYPES:
            f.write(f"        {name},\n")
        f.write("    };\n\n    namespace identifier_tables {\n")
        sizes["XidStart"] = emit_bits(f, "XidStart", xid["XID_Start"],
                                      f"XID_Start: {len(python_start)} code points in {len(xid['XID_Start'])} ranges")
        f.write("\n")
        sizes["XidContinue"] = emit_bits(f, "XidContinue", xid["XID_Continue"],
                                         f"XID_Continue: {len(python_continue)} code points in {len(xid['XID_Continue'])} ranges")
        f.write("\n")
        sizes["DefaultIgnorable"] = emit_bits(f, "DefaultIgnorable", as_ranges(ignorable),
                                              f"Default_Ignorable_Code_Point: the {len(ignorable)} code points NFKC_Casefold drops")
        f.write("\n")
        sizes["ChangesWhenNfkcCasefolded"] = emit_bits(
            f, "ChangesWhenNfkcCasefolded", changes,
            f"the {sum(hi - lo + 1 for lo, hi in changes)} code points NFKC_Casefold changes: the quick check")
        f.write("\n")
        # Sorted ranges and not a two-stage table: the joining type is
        # asked about the two characters around a zero width non-joiner
        # and about nothing else, so the search is not in any loop —
        # 4.2 KB against the 8.1 a trie of the same property came to
        sizes["JoiningType"] = emit_split(f, "ValueRange", "JoiningType", joining_table,
                                          f"the joining types: {len(joining_table)} ranges")
        f.write("\n")
        sizes["IdentifierAllowed"] = emit_bits(f, "IdentifierAllowed", as_ranges(allowed),
                                               f"Identifier_Status=Allowed: {len(allowed)} code points")
        f.write("\n")
        sizes["IdentifierType"] = emit_split(f, "ValueRange", "IdentifierType", types,
                                             f"Identifier_Type as a set of bits: {len(types)} ranges")
        f.write("\n")
        sizes["Confusables"] = emit_decomp(f, "Confusables", confusable, "the confusable prototypes")
        f.write("    }\n}\n")

    with open("tests/txt/identifier_tests.h", "w") as f:
        f.write(HEADER + f"""
// Generated by tools/unicode_tables.py from Unicode {VERSION}: do not edit.
// The oracles. XID_Start and XID_Continue are Python's own tables rather
// than the UCD's — str.isidentifier() is the first with the underscore
// added, and "a" + c the second — and the generator asserts that the two
// sources agree before writing them here. NFKC_CF is the whole of
// DerivedNormalizationProps.txt; the confusables, Identifier_Status and
// Identifier_Type are the whole of the three files of UTS #39.

#include <cstdint>

namespace ucd {{
    struct Range {{
        char32_t lo;
        char32_t hi;
    }};

    struct Mapping {{
        char32_t cp;
        const char32_t* to;
    }};

    struct TypeRange {{
        char32_t lo;
        char32_t hi;
        uint16_t types;
    }};

""")
        for name, rs in (("XidStart", as_ranges(python_start)),
                         ("XidContinue", as_ranges(python_continue)),
                         ("DefaultIgnorable", as_ranges(ignorable)),
                         ("IdentifierAllowed", as_ranges(allowed))):
            f.write(f"    inline constexpr Range {name}[] = {{\n")
            for lo, hi in rs:
                f.write(f"        {{0x{lo:04X}, 0x{hi:04X}}},\n")
            f.write("    };\n\n")
        for name, m in (("NfkcCasefold", nfkc_cf),
                        ("Confusables", {c: "".join(chr(x) for x in v) for c, v in confusable.items()})):
            f.write(f"    inline constexpr Mapping {name}[] = {{\n")
            for c in sorted(m):
                escaped = "".join(f"\\U{ord(ch):08X}" for ch in m[c])
                f.write(f'        {{0x{c:04X}, U"{escaped}"}},\n')
            f.write("    };\n\n")
        f.write("    inline constexpr TypeRange IdentifierTypes[] = {\n")
        for lo, hi, mask in types:
            f.write(f"        {{0x{lo:04X}, 0x{hi:04X}, {mask}}},\n")
        f.write("    };\n}\n")
    print(f"  identyfikatory: XID_Start {len(python_start)}, XID_Continue {len(python_continue)}, "
          f"NFKC_CF {len(nfkc_cf)}, confusables {len(confusable)}, Allowed {len(allowed)}")
    return sizes

#------------------------------------------------------------------------------
if __name__ == "__main__":
    assert os.path.isdir("sgcl/core"), "run from the root of the repository"
    os.makedirs("sgcl/txt/detail", exist_ok=True)
    sizes = {}
    sizes.update(write_property_tables())
    write_regex_tables()
    sizes.update(write_segment_tables())
    sizes.update(write_normalize_tables())
    sizes.update(write_case_tables())
    sizes.update(write_encoding_tables())
    write_encoding_tests()
    write_regex_tests()
    sizes.update(write_bidi_tables())
    sizes.update(write_collate_tables())
    sizes.update(write_idna_tables())
    sizes.update(write_identifier_tables())
    write_collate_tests(1 if "--all-collate" in sys.argv else 3)
    # the tailorings have no conformance file of their own, so their
    # oracle is ICU and its answers are committed rather than worked out
    # here: --icu rewrites them and needs PyICU, an ordinary run does not
    if "--icu" in sys.argv:
        write_collate_locale_tests()
        write_collate_option_tests()
    # every third case: the whole file is 91707 of them and 7.8 MB of
    # header, which is four times everything else the tests carry. The
    # full set is run by hand before a change of this header lands —
    # python3 tools/unicode_tables.py --all-bidi writes it whole.
    write_bidi_tests(1 if "--all-bidi" in sys.argv else 3)
    print(f"Unicode {VERSION}")
    for name, n in sizes.items():
        if n:
            print(f"  {name:<22} {n / 1024:6.1f} KB")
    print(f"  {'total':<22} {sum(sizes.values()) / 1024:6.1f} KB")
