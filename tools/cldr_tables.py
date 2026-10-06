#!/usr/bin/env python3
# Generates the locale data of the text module from CLDR. Run from the root
# of the repository:  python3 tools/cldr_tables.py
#
#   sgcl/txt/detail/cldr_locales.h     the locales the data has, the likely
#                                       subtags, the language matching data,
#                                       the autonyms
#   sgcl/txt/detail/cldr_plurals.h     the plural rules, cardinal and ordinal
#   sgcl/txt/detail/cldr_numbers.h     symbols, patterns, digits, compact forms
#   sgcl/txt/detail/cldr_currencies.h  the digits of every currency, the
#                                       symbols of every locale
#   sgcl/txt/detail/cldr_lists.h       the patterns of lists
#   sgcl/txt/detail/cldr_relative.h    relative time
#   sgcl/txt/detail/cldr_dates.h       names and patterns of the Gregorian
#                                       calendar
#
# One source: cldr-common-<CLDR>.zip in .ucd/<Unicode version>/ (the
# directory tools/unicode_tables.py keeps its files in, gitignored). The
# file has to be there; this script never downloads anything.
#
# What a locale says is resolved here, once, as the LDML specification
# resolves it (TR35 §4.1): the chain of parents — truncation, and
# parentLocales of supplementalData where it says otherwise — then the
# aliases of root, which send one path to another and start the lookup
# again from the locale asked. Data marked unconfirmed or provisional is
# left out, as ICU leaves it out. The headers hold every locale's resolved
# data, each distinct record once.
import collections
import os
import re
import sys
import unicodedata
import xml.etree.ElementTree as ET
import zipfile

CLDR = "46.0"
CACHE = os.path.join(".ucd", unicodedata.unidata_version)
ZIP = os.path.join(CACHE, f"cldr-common-{CLDR}.zip")
HEADER = """//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once
"""

if not os.path.exists(ZIP):
    sys.exit(f"{ZIP} is missing: put cldr-common-{CLDR}.zip there (this script downloads nothing)")
_zip = zipfile.ZipFile(ZIP)

def read(name):
    return _zip.read(f"common/{name}")

#------------------------------------------------------------------------------
# A locale's file as paths
#------------------------------------------------------------------------------
# A path is a tuple of steps, a step the element's name with its
# distinguishing attributes sorted: "monthWidth[type=wide]". What an
# element says is the text of a leaf; an alias is a leaf of its own,
# ("alias", target).
SKIP_ATTRS = {"draft", "references", "validSubLocales", "standard"}
DRAFT_OUT = {"unconfirmed", "provisional"}

def step(el):
    attrs = sorted((k, v) for k, v in el.attrib.items() if k not in SKIP_ATTRS)
    return el.tag + "".join(f"[{k}={v}]" for k, v in attrs)

def flatten(root):
    out = {}
    def walk(el, path):
        if el.get("draft") in DRAFT_OUT:
            return
        here = path + (step(el),)
        if el.tag == "alias":
            out[path] = ("alias", el.get("path"))
            return
        children = list(el)
        if not children:
            out[here] = (el.text or "")
            return
        for c in children:
            walk(c, here)
    for c in root:
        walk(c, ())
    return out

def parse_step(text):
    # "monthContext[@type='format']" as a step
    m = re.match(r"([A-Za-z0-9_]+)(.*)", text)
    attrs = sorted(re.findall(r"\[@([A-Za-z]+)='([^']*)'\]", m.group(2)))
    return m.group(1) + "".join(f"[{k}={v}]" for k, v in attrs)

LOCALES = {}
def locale_file(name):
    if name not in LOCALES:
        try:
            LOCALES[name] = flatten(ET.fromstring(read(f"main/{name}.xml")))
        except KeyError:
            LOCALES[name] = None
    return LOCALES[name]

AVAILABLE = sorted(n.split("/")[-1][:-4] for n in _zip.namelist()
                   if n.startswith("common/main/") and n.endswith(".xml"))

#------------------------------------------------------------------------------
# supplemental data
#------------------------------------------------------------------------------
SUPP = ET.fromstring(read("supplemental/supplementalData.xml"))
LIKELY_XML = ET.fromstring(read("supplemental/likelySubtags.xml"))
LIKELY = {e.get("from"): e.get("to") for e in LIKELY_XML.iter("likelySubtag")}

PARENTS = {}
NONLIKELY_SCRIPT_TO_ROOT = False
for block in SUPP.iter("parentLocales"):
    if block.get("component"):
        continue
    for pl in block.iter("parentLocale"):
        for loc in pl.get("locales", "").split():
            PARENTS[loc] = pl.get("parent")
        if pl.get("localeRules") == "nonlikelyScript":
            NONLIKELY_SCRIPT_TO_ROOT = True

def split_tag(tag):
    # (language, script, region) of "sr_Latn_RS", "" where absent
    parts = tag.replace("-", "_").split("_")
    lang, script, region = parts[0], "", ""
    for p in parts[1:]:
        if len(p) == 4 and p.isalpha():
            script = p
        elif (len(p) == 2 and p.isalpha()) or (len(p) == 3 and p.isdigit()):
            region = p
    return lang, script, region

def join_tag(lang, script, region):
    return "_".join(x for x in (lang, script, region) if x)

def maximize(tag):
    # TR35 §4.3 Likely Subtags, "Add Likely Subtags"
    lang, script, region = split_tag(tag)
    if lang == "root":
        lang = "und"
    for key in ([join_tag(lang, script, region)] if script and region else []) + \
               ([join_tag(lang, "", region)] if region else []) + \
               ([join_tag(lang, script, "")] if script else []) + \
               [lang] + (["und_" + script] if script else []):
        if key in LIKELY:
            l2, s2, r2 = split_tag(LIKELY[key])
            return (lang if lang != "und" else l2), (script or s2), (region or r2)
    return lang, script, region

def parent(name):
    # The parent of a locale in the lookup chain, None past root
    if name == "root":
        return None
    if name in PARENTS:
        return PARENTS[name]
    lang, script, region = split_tag(name)
    if NONLIKELY_SCRIPT_TO_ROOT and script and not region:
        likely = maximize(lang)[1]
        if script != likely:
            return "root"
    cut = name.rsplit("_", 1)
    return cut[0] if len(cut) == 2 else "root"

def chain(name):
    out = []
    while name is not None:
        if locale_file(name) is not None:
            out.append(name)
        name = parent(name)
    return out

def lookup(name, path, depth=0):
    # The value of a path in a locale, resolved: None when nothing in the
    # chain has it
    assert depth < 20, path
    for loc in chain(name):
        data = locale_file(loc)
        v = data.get(path)
        if v is not None and not isinstance(v, tuple):
            return None if v == "∅∅∅" else v   # CLDR's mark of no value, which is not inherited
        for n in range(len(path), 0, -1):
            a = data.get(path[:n])
            if isinstance(a, tuple):
                # an alias in the element path[:n]: what is below it is
                # elsewhere, and the lookup starts again from the locale asked
                return lookup(name, rewrite_alias(path, n, a[1]), depth + 1)
    return None

def rewrite_alias(path, n, target):
    # an alias stored at key path[:n] (the element holding the alias) applies
    # to everything below that element; the target is relative to it
    base = list(path[:n])
    for s in target.split("/"):
        if s == "..":
            base.pop()
        elif s:
            base.append(parse_step(s))
    return tuple(base) + tuple(path[n:])


#------------------------------------------------------------------------------
# The locales of the data
#------------------------------------------------------------------------------
# Every locale of common/main whose language, or language and script, CLDR
# rates in coverageLevels.txt (modern, moderate or basic), with all its
# regional and script variants: what has data worth formatting with. A
# language outside the set is the root locale, as in ICU.
COVERAGE = {}
for line in read("properties/coverageLevels.txt").decode().splitlines():
    if line and not line.startswith("#") and ";" in line:
        f = [x.strip() for x in line.split(";")]
        COVERAGE[f[0]] = f[1]

def covered(name):
    # the locale, a truncation of it (pa_Arab of pa) or one of its parents
    # (nb's parent is no) is rated
    p = name.split("_")
    return any("_".join(p[:i]) in COVERAGE for i in range(1, len(p) + 1)) \
        or any(n in COVERAGE for n in chain(name) if n != "root")

def plain(name):
    # language, script, region and nothing else: en_US_POSIX is a variant,
    # which the type does not keep
    return join_tag(*split_tag(name)) == name

DATA_LOCALES = [n for n in AVAILABLE if n != "root" and plain(n) and covered(n)]

# The packing of txt::locale (sgcl/txt/locale.h), which the tables are
# keyed by: the language's letters in the high word as subtag() has them;
# in the low word the script's four letters at five bits each from bit 12,
# the region from bit 1 (two letters as 27*a+b, three digits as 729+n)
def pack_language(lang):
    v = 0
    for c in lang:
        v = (v << 8) | ord(c)
    return v

def pack_script(script):
    v = 0
    for c in script.lower():
        v = (v << 5) | (ord(c) - 96)
    return v

def pack_region(region):
    if not region:
        return 0
    if region.isdigit():
        return 729 + int(region)
    return (ord(region[0]) - 64) * 27 + (ord(region[1]) - 64)

def pack(lang, script="", region=""):
    return (pack_language(lang) << 32) | (pack_script(script) << 12) | (pack_region(region) << 1)

#------------------------------------------------------------------------------
# Writing C++
#------------------------------------------------------------------------------
# Bidirectional controls and other invisible characters are written as
# universal character names: clang warns of an unterminated embedding in a
# literal (-Wbidi-chars), and an invisible character is better seen
INVISIBLE = set(range(0x200B, 0x2010)) | set(range(0x202A, 0x202F)) | set(range(0x2066, 0x206A)) \
    | {0x061C, 0xFEFF, 0x00AD, 0x00A0, 0x202F, 0x2007, 0x2009, 0x200A, 0x2060}

def u8_literal(text):
    # The body of a u8"" literal. The headers are UTF-8 with a byte order
    # mark, which MSVC reads as UTF-8 without a flag, and a u8 literal is
    # UTF-8 in the program whatever the execution character set — so the
    # text stands as it is, at a third of the size of \u escapes; a
    # control character is an octal escape (a \x escape would swallow a
    # following hex digit), an invisible one a universal character name
    out = []
    for ch in text:
        c = ord(ch)
        if ch in '"\\':
            out.append("\\" + ch)
        elif c < 0x20 or c == 0x7F:
            out.append("\\%03o" % c)
        elif c in INVISIBLE or 0x80 <= c < 0xA0:
            out.append("\\u%04X" % c)
        else:
            out.append(ch)
    return "".join(out)

def chunk_lines(name, pieces, ctype="char8_t", prefix="u8"):
    # A literal of the pieces in lines of about 100 columns, never cutting
    # an escape or a character
    out = [f"    inline constexpr {ctype} {name}[] ="]
    line = ""
    for piece in pieces:
        k = 0
        while k < len(piece):
            # take up to the next escape boundary
            if piece[k] == "\\":
                width = 4 if piece[k + 1] in "01234567" else 6 if piece[k + 1] == "u" else 2
                tok = piece[k:k + width]
            else:
                tok = piece[k]
            k += len(tok)
            if len(line) + len(tok) > 100:
                out.append(f'        {prefix}"{line}"')
                line = ""
            line += tok
    out.append(f'        {prefix}"{line}";')
    return out

B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

class Packed:
    # Unsigned values of a fixed number of base-64 digits each (3: 18 bits,
    # 6: 36), the first digit the highest, 8192 to a literal (MSVC refuses
    # one past 65535 characters); read with detail::cldr::Packed. Half the text of the
    # numbers written out, and a literal compiles faster than an array.
    SHIFT = 13   # 8192 values a chunk: under 60000 characters for six digits, and a shift to find one
    def __init__(self, name, values, digits):
        self.name, self.values, self.digits = name, list(values), digits
        assert all(0 <= v < (1 << (6 * digits)) for v in self.values), name
    def emit(self):
        per = 1 << self.SHIFT
        out = []
        chunks = max(1, (len(self.values) + per - 1) // per)
        for k in range(chunks):
            text = "".join("".join(B64[(v >> (6 * (self.digits - 1 - d))) & 63] for d in range(self.digits))
                           for v in self.values[k * per:(k + 1) * per])
            out += chunk_lines(f"{self.name}Chunk{k}", [text], "char", "")
        out.append(f"    inline constexpr const char* {self.name}Chunks[] = {{" +
                   ", ".join(f"{self.name}Chunk{k}" for k in range(chunks)) + "};")
        out.append(f"    inline constexpr Packed {self.name}{{{self.name}Chunks, {self.SHIFT}, {self.digits}}};")
        return "\n".join(out)

class Pool:
    # Each distinct text once, in chunks of at most CHUNK bytes of UTF-8
    # (MSVC refuses a literal past 65535); a text is a reference of 22
    # bits: the chunk (6) and the offset in it (16).
    CHUNK = 60000
    def __init__(self, name):
        self.name = name
        self.index = {"": 0}
        self.texts = [""]
    def add(self, text):
        if text is None:
            text = ""
        i = self.index.get(text)
        if i is None:
            i = self.index[text] = len(self.texts)
            self.texts.append(text)
        return i
    def emit(self):
        # a reference is the chunk << 16 | the offset, and one more marks
        # the end of the last text: a text runs to the next one's offset
        # (to the end of its chunk where the next is in another)
        chunks = [[]]
        sizes = [0]
        refs = []
        for t in self.texts:
            n = len(t.encode())
            if sizes[-1] + n > self.CHUNK:
                chunks.append([])
                sizes.append(0)
            refs.append(((len(chunks) - 1) << 16) | sizes[-1])
            chunks[-1].append(t)
            sizes[-1] += n
        refs.append(((len(chunks) - 1) << 16) | sizes[-1])
        assert len(chunks) <= 64 and len(self.texts) < 65536
        out = []
        for k, chunk in enumerate(chunks):
            out += chunk_lines(f"{self.name}Chunk{k}", [u8_literal(t) for t in chunk])
        out.append(f"    inline constexpr const char8_t* {self.name}Chunks[] = {{" +
                   ", ".join(f"{self.name}Chunk{k}" for k in range(len(chunks))) + "};")
        out.append(f"    inline constexpr uint16_t {self.name}ChunkSizes[] = {{" + ", ".join(map(str, sizes)) + "};")
        out.append(Packed(f"{self.name}Refs", refs, 4).emit())
        out.append(f"    inline constexpr Text {self.name}Texts{{{self.name}Chunks, {self.name}ChunkSizes, {self.name}Refs}};")
        return "\n".join(out)

def wrap_numbers(values, hexa=False, indent=8, width=116):
    out = []
    line = " " * indent
    for v in values:
        piece = (hex(v) if hexa else str(v)) + ","
        if len(line) + len(piece) + 1 > width:
            out.append(line.rstrip())
            line = " " * indent
        line += piece + " "
    if line.strip():
        out.append(line.rstrip())
    return out

class Rows:
    # Records of a fixed width, each distinct one once, packed: the value
    # k of row i is Name[i * width + k]
    def __init__(self, name, width, ctype="uint16_t"):
        self.name = name
        self.width = width
        self.digits = 3 if ctype == "uint16_t" else 6
        self.index = {}
        self.rows = []
    def add(self, row):
        row = tuple(row)
        assert len(row) == self.width, (self.name, len(row), self.width)
        i = self.index.get(row)
        if i is None:
            i = self.index[row] = len(self.rows)
            self.rows.append(row)
        return i
    def emit(self):
        flat = [v for r in self.rows for v in r]
        return f"    inline constexpr uint32_t {self.name}Width = {self.width};\n" + \
            Packed(self.name, flat, self.digits).emit()

class SparseRows:
    # Records of a fixed width most of whose values are 0, each distinct
    # one once: a record is a mask of its values that are not 0 followed
    # by those values, and NameStart gives where record i begins; read with
    # detail::cldr::sparse (value k: 0 when its bit is clear, else the
    # value after as many as the mask has set bits below k)
    def __init__(self, name, width):
        assert width <= 18
        self.name, self.width = name, width
        self.index = {}
        self.flat = []
        self.starts = []
    def add(self, row):
        row = tuple(row)
        assert len(row) == self.width, (self.name, len(row), self.width)
        i = self.index.get(row)
        if i is None:
            i = self.index[row] = len(self.starts)
            self.starts.append(len(self.flat))
            mask = 0
            for k, v in enumerate(row):
                if v:
                    mask |= 1 << k
            self.flat.append(mask)
            self.flat += [v for v in row if v]
        return i
    def emit(self):
        return Packed(self.name, self.flat, 3).emit() + "\n" + Packed(self.name + "Start", self.starts, 3).emit()

def emit_array(name, values, ctype, hexa=False):
    return "\n".join([f"    inline constexpr {ctype} {name}[] = {{"] + wrap_numbers(values, hexa) + ["    };"])

def write_header(path, comment, body, includes=()):
    text = "\ufeff" + HEADER
    for inc in includes:
        text += f'#include "{inc}"\n'
    text += '#include "' + os.path.relpath("sgcl/txt/detail/cldr.h", os.path.dirname(path)) + '"\n\n'
    text += "#include <cstdint>\n\n"
    text += "".join(f"// {line}\n".replace("// \n", "//\n") for line in comment.strip().split("\n"))
    text += "namespace sgcl::txt::detail::cldr {\n" + body.rstrip() + "\n}\n"
    if not (os.path.exists(path) and open(path, encoding="utf-8").read() == text):
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    SIZES.append((path, len(text.encode())))

SIZES = []

#------------------------------------------------------------------------------
# cldr_locales.h: which record a locale reads
#------------------------------------------------------------------------------
# Index 0 is root, 1.. the data locales. A key is a file's name with the
# language's default script dropped (zh_Hans_SG is zh-SG), so the lookup
# at run time is: fill a missing script from the region where the
# language's script depends on it (zh-TW is zh-Hant-TW), drop the
# default script, then the key with the region, the key without it, root.
INDEX = {"root": 0}
for i, name in enumerate(DATA_LOCALES):
    INDEX[name] = i + 1
DATA_LANGUAGES = sorted({split_tag(n)[0] for n in DATA_LOCALES})
DEFAULT_SCRIPT = {lang: maximize(lang)[1] for lang in DATA_LANGUAGES}
REGION_SCRIPT = {}
for frm, to in LIKELY.items():
    lang, script, region = split_tag(frm)
    if lang in DEFAULT_SCRIPT and region and not script:
        s2 = split_tag(to)[1]
        if s2 != DEFAULT_SCRIPT[lang]:
            REGION_SCRIPT[(lang, region)] = s2

def key_of(name):
    lang, script, region = split_tag(name)
    if script == DEFAULT_SCRIPT.get(lang):
        script = ""
    return (lang, script, region)

KEYS = {}
for name in sorted(DATA_LOCALES, key=len):   # the longer name wins a key it shares (zh_Hans over zh)
    KEYS[key_of(name)] = INDEX[name]

def find(lang, script="", region=""):
    # what the C++ lookup does, for the checks below
    if lang not in DEFAULT_SCRIPT:
        return 0
    if not script and region:
        script = REGION_SCRIPT.get((lang, region), "")
    if script == DEFAULT_SCRIPT[lang]:
        script = ""
    for k in ((lang, script, region), (lang, script, "")):
        if k in KEYS:
            return KEYS[k]
    return 0

for name in DATA_LOCALES:
    got = find(*split_tag(name))
    assert DATA_LOCALES[got - 1] == name or chain(DATA_LOCALES[got - 1])[1:2] == [name] \
        or (got and key_of(DATA_LOCALES[got - 1]) == key_of(name)), (name, DATA_LOCALES[got - 1])
assert DATA_LOCALES[find("zh", "", "TW") - 1] == "zh_Hant_TW"
assert DATA_LOCALES[find("sr", "Latn", "RS") - 1] == "sr_Latn_RS"
assert DATA_LOCALES[find("de", "", "CH") - 1] == "de_CH"
assert DATA_LOCALES[find("pa", "", "PK") - 1] == "pa_Arab_PK"

def gen_locales():
    keys = sorted((pack(*k), i) for k, i in KEYS.items())
    body = []
    body.append(f"    // {len(DATA_LOCALES)} locales of CLDR {CLDR} and root; a key is txt::locale's packing")
    body.append(f"    inline constexpr uint16_t LocaleCount = {len(DATA_LOCALES) + 1};")
    body.append(emit_array("LocaleKeys", [k for k, _ in keys], "uint64_t", hexa=True))
    body.append(emit_array("LocaleKeyIndex", [i for _, i in keys], "uint16_t"))
    body.append("    // the languages with data and their default script, sorted by language")
    langs = sorted(DATA_LANGUAGES, key=pack_language)
    body.append(emit_array("DataLanguages", [pack_language(l) for l in langs], "uint32_t", hexa=True))
    body.append(emit_array("DefaultScripts", [pack_script(DEFAULT_SCRIPT[l]) for l in langs], "uint32_t", hexa=True))
    body.append("    // a language whose script follows its region: (language << 32 | region) and the script")
    rs = sorted(((pack_language(l) << 32) | pack_region(r), pack_script(s)) for (l, r), s in REGION_SCRIPT.items())
    body.append(emit_array("RegionScriptKeys", [k for k, _ in rs], "uint64_t", hexa=True))
    body.append(emit_array("RegionScripts", [s for _, s in rs], "uint32_t", hexa=True))
    body.append("    // the names of the locales by index, for tests and diagnostics")
    names = Pool("LocaleName")
    idx = [names.add("root")] + [names.add(n.replace("_", "-")) for n in DATA_LOCALES]
    body.append(names.emit())
    body.append(emit_array("LocaleNames", idx, "uint16_t"))
    write_header("sgcl/txt/detail/cldr_locales.h", f"""
The locales of the CLDR data (generated by tools/cldr_tables.py from CLDR
{CLDR}: do not edit). Every group of data — numbers, dates, lists — has a
record per locale index; this header says which index a locale reads.
""", "\n\n".join(body))

#------------------------------------------------------------------------------
# cldr_plurals.h: the plural rules as C++
#------------------------------------------------------------------------------
# Each distinct rule set is a function of the operands of TR35 §5.1 (n i v
# w f t c/e), compiled from the rule text; a language (pt_PT: a language
# and a region) names one for cardinals and one for ordinals. The rules'
# own samples (@integer, @decimal) are written out for the tests.
CATEGORIES = ["zero", "one", "two", "few", "many", "other"]

def compile_condition(text):
    # "n % 10 = 3..4,9 and n % 100 != 10..19 or ..." as a C++ expression
    toks = re.findall(r"\.\.|!=|=|%|,|[a-z]+|\d+", text)
    pos = [0]
    def peek():
        return toks[pos[0]] if pos[0] < len(toks) else None
    def take(expect=None):
        t = toks[pos[0]]
        assert expect is None or t == expect, (text, t, expect)
        pos[0] += 1
        return t
    def relation():
        op = take()
        assert op in "niftvwce", (text, op)
        mod = None
        if peek() == "%":
            take()
            mod = int(take())
        rel = take()
        assert rel in ("=", "!="), (text, rel)
        ranges = []
        while True:
            lo = int(take())
            hi = lo
            if peek() == "..":
                take()
                hi = int(take())
            ranges.append((lo, hi))
            if peek() != ",":
                break
            take()
        var = {"n": "o.i", "i": "o.i", "f": "o.f", "t": "o.t", "v": "o.v", "w": "o.w", "c": "o.e", "e": "o.e"}[op]
        x = f"{var} % {mod}" if mod else var
        parts = []
        for lo, hi in ranges:
            if lo == hi:
                parts.append(f"{x} == {lo}")
            elif lo == 0:
                parts.append(f"{x} <= {hi}")
            else:
                parts.append(f"({x} >= {lo} && {x} <= {hi})")
        test = parts[0] if len(parts) == 1 else "(" + " || ".join(parts) + ")"
        if op == "n":
            # n with a fraction is no integer of any range
            return f"(!o.fraction && {test})" if rel == "=" else f"(o.fraction || !({test}))"
        return test if rel == "=" else f"!({test})" if len(parts) == 1 else f"!{test}"
    def conj():
        out = [relation()]
        while peek() == "and":
            take()
            out.append(relation())
        return " && ".join(out) if len(out) == 1 else "(" + " && ".join(out) + ")"
    out = [conj()]
    while peek() == "or":
        take()
        out.append(conj())
    assert pos[0] == len(toks), text
    return " || ".join(out)

def read_rules(file):
    root = ET.fromstring(read(f"supplemental/{file}"))
    sets = []      # [(locales, [(category, condition, integer samples, decimal samples)])]
    for block in root.iter("pluralRules"):
        rules = []
        for r in block.iter("pluralRule"):
            text = r.text or ""
            cond, _, samples = text.partition("@")
            samples = "@" + samples if samples else ""
            ints = re.search(r"@integer([^@]*)", samples)
            decs = re.search(r"@decimal([^@]*)", samples)
            rules.append((r.get("count"), cond.strip(), ints.group(1).strip() if ints else "",
                          decs.group(1).strip() if decs else ""))
        sets.append((block.get("locales").split(), rules))
    return sets

def expand_samples(text):
    # "0, 2~16, 100, 1000, …" as numbers written as text, a range of
    # integers or of decimals with the same number of fraction digits
    out = []
    for item in text.replace("…", "").split(","):
        item = item.strip()
        if not item:
            continue
        if "~" in item:
            lo, hi = item.split("~")
            if "." in lo and "c" not in lo and "e" not in lo:
                digits = len(lo.split(".")[1])
                a = int(lo.replace(".", ""))
                b = int(hi.replace(".", ""))
                for v in range(a, b + 1):
                    s = str(v).rjust(digits + 1, "0")
                    out.append(s[:-digits] + "." + s[-digits:])
            elif "c" in lo or "e" in lo:
                out += [lo, hi]
            else:
                out += [str(v) for v in range(int(lo), int(hi) + 1)]
        else:
            out.append(item)
    return out

def gen_plurals():
    body = []
    funcs = {}
    lang_keys = {}
    samples = []
    for kind, file in (("Cardinal", "plurals.xml"), ("Ordinal", "ordinals.xml")):
        for locales, rules in read_rules(file):
            conds = tuple((c, cond) for c, cond, _, _ in rules if c != "other" and cond)
            fid = funcs.get((kind, conds))
            if fid is None:
                fid = funcs[(kind, conds)] = sum(1 for k in funcs if k[0] == kind)
            for loc in locales:
                lang, script, region = split_tag(loc)
                if lang == "root":
                    lang = ""
                lang_keys.setdefault((lang, region), {})[kind] = fid
            for c, cond, ints, decs in rules:
                for s in expand_samples(ints) + expand_samples(decs):
                    samples.append((kind, " ".join(locales), s, c))
    for kind in ("Cardinal", "Ordinal"):
        ids = sorted((fid, conds) for (k, conds), fid in funcs.items() if k == kind)
        for fid, conds in ids:
            lines = [f"    inline txt::plural {kind}{fid}(const PluralOperands& o) noexcept {{"]
            if not conds:
                lines.append("        (void)o;")
            for c, cond in conds:
                lines.append(f"        if ({compile_condition(cond)}) {{")
                lines.append(f"            return txt::plural::{c};")
                lines.append("        }")
            lines.append("        return txt::plural::other;")
            lines.append("    }")
            body.append("\n".join(lines))
        body.append(f"    inline constexpr PluralRule {kind}s[] = {{\n" +
                    "\n".join(wrap_numbers([f"&{kind}{fid}" for fid, _ in ids])).replace("'", "") + "\n    };")
    # the languages: a key as txt::locale packs language and region
    keys = sorted(((pack(l, "", r) if l else 0), v.get("Cardinal", 0), v.get("Ordinal", 0))
                  for (l, r), v in lang_keys.items())
    body.append("    // a language (a language and a region) and its rule sets, cardinal and ordinal")
    body.append(emit_array("PluralKeys", [k for k, _, _ in keys], "uint64_t", hexa=True))
    body.append(emit_array("PluralSets", [(c << 8) | o for _, c, o in keys], "uint16_t"))
    write_header("sgcl/txt/detail/cldr_plurals.h", f"""
The plural rules of CLDR {CLDR} (supplemental/plurals.xml and ordinals.xml),
compiled into C++ by tools/cldr_tables.py: do not edit. Every distinct set
of rules is one function of the operands of TR35 §5.1. Included by
sgcl/txt/plural.h after txt::plural, PluralOperands and PluralRule.
""", "\n\n".join(body))
    return samples

#------------------------------------------------------------------------------
# cldr_numbers.h
#------------------------------------------------------------------------------
# A pattern of LDML §3.2 is read here into its affixes and its digits. In
# an affix the symbols stand as bytes the formatter replaces: 1 minus,
# 2 plus, 3 percent, 4 per mille, 5 the currency; the quotes are gone.
AFFIX_SYMBOLS = {"-": "\x01", "+": "\x02", "%": "\x03", "‰": "\x04", "¤": "\x05"}

def split_pattern(p):
    # the positive and the negative subpattern, at a ';' outside quotes
    q = False
    for i, ch in enumerate(p):
        if ch == "'":
            q = not q
        elif ch == ";" and not q:
            return p[:i], p[i + 1:]
    return p, None

def read_subpattern(p):
    prefix, number, suffix = [], [], []
    i, q, state = 0, False, 0
    while i < len(p):
        ch = p[i]
        if ch == "'":
            if i + 1 < len(p) and p[i + 1] == "'":
                (prefix if state == 0 else suffix).append("'")
                i += 2
                continue
            q = not q
            i += 1
            continue
        if not q and state < 2 and ch in "#0123456789@,.E" and (state == 1 or ch != "E"):
            state = 1
            number.append(ch)
            if ch == "E" and i + 1 < len(p) and p[i + 1] == "+":
                number.append("+")
                i += 1
            i += 1
            continue
        if state == 1:
            state = 2
        out = prefix if state == 0 else suffix
        if not q and ch in AFFIX_SYMBOLS:
            if ch == "¤" and out and out[-1] == "\x05":
                i += 1
                continue
            out.append(AFFIX_SYMBOLS[ch])
        else:
            out.append(ch)
        i += 1
    return "".join(prefix), "".join(number), "".join(suffix)

def read_pattern(p):
    pos, neg = split_pattern(p)
    pp, num, ps = read_subpattern(pos)
    assert "@" not in num, p
    mantissa, _, exponent = num.partition("E")
    integer, _, fraction = mantissa.partition(".")
    groups = integer.split(",")
    g1 = len(groups[-1]) if len(groups) > 1 else 0
    g2 = len(groups[-2]) if len(groups) > 2 else g1
    digits = integer.replace(",", "")
    row = dict(pp=pp, ps=ps, min_int=digits.count("0"), max_int=len(digits) if exponent else 0,
               min_frac=fraction.count("0"), max_frac=len(fraction), g1=g1, g2=g2,
               exp=exponent.count("0") | (16 if exponent.startswith("+") else 0) if exponent else 0)
    if neg is not None:
        row["np"], _, row["ns"] = read_subpattern(neg)
    else:
        row["np"], row["ns"] = "\x01" + pp, ps
    return row

NUMBERING = {e.get("id"): e for e in ET.fromstring(read("supplemental/numberingSystems.xml")).iter("numberingSystem")}
SYMBOLS = ["decimal", "group", "percentSign", "plusSign", "minusSign", "exponential", "perMille", "infinity", "nan",
           "currencyDecimal", "currencyGroup", "approximatelySign"]
MAGNITUDES = list(range(3, 20))

def gen_numbers():
    pool = Pool("Number")
    patterns = Rows("NumberPatterns", 11)
    forms = SparseRows("CompactForms", 6)
    compacts = Rows("CompactRows", 17)
    systems = Rows("NumberSystems", 21)
    locales = Rows("NumberLocales", 3)
    N = ("numbers",)
    def pattern_row(text):
        if not text:
            return 0
        r = read_pattern(text)
        return patterns.add([pool.add(r["pp"]), pool.add(r["ps"]), pool.add(r["np"]), pool.add(r["ns"]),
                             r["min_int"], r["max_int"], r["min_frac"], r["max_frac"], r["g1"], r["g2"], r["exp"]])
    patterns.add([0] * 11)
    forms.add([0] * 6)
    compacts.add([0] * 17)
    def compact_row(loc, base):
        row = []
        for m in MAGNITUDES:
            f = []
            zeros = set()
            for c in CATEGORIES:
                text = lookup(loc, base + (f"pattern[count={c}][type={10 ** m}]",))
                if not text or text == "0":
                    f.append(0)
                    continue
                pos, neg = split_pattern(text)
                pre, num, suf = read_subpattern(pos)
                if num:
                    zeros.add(num.count("0"))
                    form = pre + "\x07" + suf
                else:
                    form = pre   # "mille": a word and no number
                if neg is not None:
                    # a negative form of its own (Swahili "elfu -0"): after 8
                    npre, _, nsuf = read_subpattern(neg)
                    form += "\x08" + npre + "\x07" + nsuf
                f.append(pool.add(form))
            assert len(zeros) <= 1, (loc, m)
            f = [x if x != f[-1] or k == 5 else 0 for k, x in enumerate(f)]   # other's own text: 0
            i = forms.add(f)
            assert i < 4096
            row.append(i | ((zeros.pop() if zeros else 0) << 12))
        return compacts.add(row)
    def system_row(loc, ns):
        digits = NUMBERING[ns].get("digits")
        assert digits, (loc, ns)
        sym = [lookup(loc, N + (f"symbols[numberSystem={ns}]", s)) for s in SYMBOLS]
        def pat(kind, length="", alt=""):
            group = {"decimal": "decimalFormats", "percent": "percentFormats", "scientific": "scientificFormats",
                     "currency": "currencyFormats"}[kind]
            if kind == "currency":
                path = N + (f"currencyFormats[numberSystem={ns}]", "currencyFormatLength", f"currencyFormat[type={length}]",
                            f"pattern[alt={alt}]" if alt else "pattern")
            else:
                path = N + (f"{group}[numberSystem={ns}]", f"{kind}FormatLength", f"{kind}Format", "pattern")
            return lookup(loc, path)
        row = [pool.add(digits)] + [pool.add(s) for s in sym]
        row += [pattern_row(pat("decimal")), pattern_row(pat("percent")), pattern_row(pat("scientific")),
                pattern_row(pat("currency", "standard")), pattern_row(pat("currency", "accounting"))]
        row.append(compact_row(loc, N + (f"decimalFormats[numberSystem={ns}]", "decimalFormatLength[type=short]", "decimalFormat")))
        row.append(compact_row(loc, N + (f"decimalFormats[numberSystem={ns}]", "decimalFormatLength[type=long]", "decimalFormat")))
        row.append(compact_row(loc, N + (f"currencyFormats[numberSystem={ns}]", "currencyFormatLength[type=short]",
                                          "currencyFormat[type=standard]")))
        return systems.add(row)
    index = []
    for name in ["root"] + DATA_LOCALES:
        ns = lookup(name, N + ("defaultNumberingSystem",))
        grouping = int(lookup(name, N + ("minimumGroupingDigits",)))
        index.append(locales.add([system_row(name, ns), system_row(name, "latn"), grouping]))
    body = [pool.emit(),
            "    // a pattern: positive prefix, suffix, negative prefix, suffix (texts), the least integer digits,\n"
            "    // the most (scientific), the least and the most fraction digits, the primary and the secondary\n"
            "    // grouping, the exponent's least digits (| 16: a plus sign)",
            patterns.emit(),
            "    // the compact forms of one magnitude: zero one two few many other (0: other's; a text is the\n"
            "    // prefix, 7, the suffix; without the 7, a word that stands for the number: Italian mille; after an 8,\n"
            "    // the negative form, its sign inside: Swahili elfu -0)",
            forms.emit(),
            "    // the compact forms of 10^3 to 10^19: a row of CompactForms | the count of the pattern's zeros << 12\n"
            "    // (0: the magnitude is not compacted)",
            compacts.emit(),
            "    // a numbering system in a locale: the digits, " + ", ".join(SYMBOLS) + ";\n"
            "    // the patterns decimal, percent, scientific, currency, accounting; the compact rows short, long\n"
            "    // and currency (the alphaNextToNumber patterns are left out: ICU 78 does not use them either, and\n"
            "    // currencySpacing gives the same text where they agree)",
            systems.emit(),
            "    // a locale: its numbering system, latn, the minimum grouping digits",
            locales.emit(),
            emit_array("NumberLocale", index, "uint16_t")]
    write_header("sgcl/txt/detail/cldr_numbers.h", f"""
The symbols and patterns of numbers of CLDR {CLDR} for every locale of
cldr_locales.h (generated by tools/cldr_tables.py: do not edit).
""", "\n\n".join(body))

#------------------------------------------------------------------------------
# cldr_currencies.h
#------------------------------------------------------------------------------
# A currency is its code packed in 15 bits; the tables are its digits and
# cash rounding (supplementalData's currencyData), the narrow symbols of
# root, every region's currency of the day, and per locale the symbols
# that are not the code itself. The spacing of currencySpacing — a no-break
# space between a digit and a symbol whose letter faces it — is the same in
# every locale of CLDR (asserted below); whether a symbol's first and last
# characters are letters ([[:^S:]&[:^Z:]]) is worked out here, two bits.
def pack_currency(code):
    v = 0
    for c in code:
        v = (v << 5) | (ord(c) - 64)
    return v

def spacing_bits(symbol):
    def nonsymbol(ch):
        cat = unicodedata.category(ch)
        return not cat.startswith("S") and not cat.startswith("Z")
    if not symbol:
        return 0
    return (1 if nonsymbol(symbol[-1]) else 0) | (2 if nonsymbol(symbol[0]) else 0)

def gen_currencies():
    supp = read("supplemental/supplementalData.xml").decode()
    N = ("numbers",)
    codes = set()
    info = {}
    for e in SUPP.find("currencyData").find("fractions").iter("info"):
        c = e.get("iso4217")
        digits = int(e.get("digits", 2))
        info[c] = (digits, int(e.get("cashDigits", digits)), int(e.get("cashRounding", e.get("rounding", 0))))
    default = info.pop("DEFAULT")
    region_currency = {}
    for region in SUPP.find("currencyData").iter("region"):
        for cur in region.iter("currency"):
            if cur.get("to") is None and cur.get("tender") != "false":
                region_currency[region.get("iso3166")] = cur.get("iso4217")
                break
    for k in locale_file("root"):
        if k[:2] == ("numbers", "currencies"):
            codes.add(k[2][len("currency[type="):-1])
    for loc in DATA_LOCALES:
        for k in locale_file(loc):
            if k[:2] == ("numbers", "currencies"):
                codes.add(k[2][len("currency[type="):-1])
    codes |= set(info) | set(region_currency.values())
    codes = sorted(c for c in codes if re.fullmatch(r"[A-Z]{3}", c))
    cindex = {c: i for i, c in enumerate(codes)}
    pool = Pool("Currency")
    narrow_root = []
    for c in codes:
        s = lookup("root", N + ("currencies", f"currency[type={c}]", "symbol[alt=narrow]"))
        narrow_root.append(pool.add(s) | (spacing_bits(s or c) << 16) if s and s != c else 0)
    def packed_info(c):
        d, cd, cr = info.get(c, default)
        return d | (cd << 4) | (cr << 8)
    lists = {}
    flat = []
    def add_list(entries):
        entries = tuple(entries)
        if entries not in lists:
            lists[entries] = (len(flat), len(entries))
            flat.extend(entries)
        return lists[entries]
    add_list(())
    sym_index = []
    nar_index = []
    base_index = []
    resolved = {}
    def symbols_of(name):
        if name not in resolved:
            sym, nar = {}, {}
            for c in codes:
                path = N + ("currencies", f"currency[type={c}]")
                s = lookup(name, path + ("symbol",))
                n = lookup(name, path + ("symbol[alt=narrow]",))
                if s and s != c:
                    sym[c] = s
                if n and n != lookup("root", path + ("symbol[alt=narrow]",)):
                    nar[c] = n
            resolved[name] = (sym, nar)
        return resolved[name]
    def entry(c, s):
        # a text of 0 is the code itself: an entry that takes back the base's symbol
        return cindex[c] | (spacing_bits(s or c) << 9) | ((pool.add(s) if s else 0) << 16)
    for name in ["root"] + DATA_LOCALES:
        # a regional or script variant keeps only what differs from its
        # base, the first locale of its chain below root
        ch = chain(name)
        base = ch[-2] if len(ch) > 2 else None
        sym, nar = symbols_of(name)
        bsym, bnar = symbols_of(base) if base else ({}, {})
        own = [entry(c, sym.get(c)) for c in codes if sym.get(c) != bsym.get(c)]
        ownn = [entry(c, nar.get(c)) for c in codes if nar.get(c) != bnar.get(c)]
        sym_index.append(add_list(own))
        nar_index.append(add_list(ownn))
        base_index.append(INDEX[base] if base else 0)
        for side in ("beforeCurrency", "afterCurrency"):
            ns = lookup(name, N + ("defaultNumberingSystem",))
            got = tuple(lookup(name, N + (f"currencyFormats[numberSystem={ns}]", "currencySpacing", side, x))
                        for x in ("currencyMatch", "surroundingMatch", "insertBetween"))
            assert got == ("[[:^S:]&[:^Z:]]", "[:digit:]", " "), (name, got)
    assert len(flat) < 65536
    regions = sorted((pack_region(r), cindex[c]) for r, c in region_currency.items() if re.fullmatch(r"[A-Z]{2}|\d{3}", r))
    body = [pool.emit(),
            f"    // {len(codes)} codes, each packed as five bits a letter (A = 1), sorted",
            emit_array("CurrencyCodes", [pack_currency(c) for c in codes], "uint16_t"),
            "    // digits | cash digits << 4 | cash rounding << 8",
            emit_array("CurrencyInfo", [packed_info(c) for c in codes], "uint16_t"),
            "    // root's narrow symbol: the text | its spacing bits << 16 (bit 0: its last character is a\n"
            "    // letter, bit 1: its first), 0 when it is the code",
            emit_array("CurrencyNarrow", narrow_root, "uint32_t", hexa=True),
            "    // a region (packed as txt::locale packs one) and its currency today",
            emit_array("RegionCurrencyKeys", [r for r, _ in regions], "uint16_t"),
            emit_array("RegionCurrencies", [c for _, c in regions], "uint16_t"),
            "    // the symbols of the locales: the code's index | spacing bits << 9 | the text << 16, sorted by\n"
            "    // code; a locale's symbols and its narrow symbols that are not root's are each a run of these",
            Packed("CurrencySymbols", flat, 6).emit(),
            "    // a locale's runs: start << 16 | length; its symbols, its narrow symbols, and the locale whose\n"
            "    // runs are read after its own (a variant's base; 0: none)",
            emit_array("CurrencySymbolRuns", [(o << 16) | n for o, n in sym_index], "uint32_t", hexa=True),
            emit_array("CurrencyNarrowRuns", [(o << 16) | n for o, n in nar_index], "uint32_t", hexa=True),
            emit_array("CurrencyBase", base_index, "uint16_t")]
    write_header("sgcl/txt/detail/cldr_currencies.h", f"""
The currencies of CLDR {CLDR} (generated by tools/cldr_tables.py: do not
edit): every code's digits and cash rounding, the symbols of every locale
of cldr_locales.h, and the currency of every region.
""", "\n\n".join(body))
    return codes

#------------------------------------------------------------------------------
# cldr_likely.h: likely subtags, parents, matching
#------------------------------------------------------------------------------
# The likely subtags of every language CLDR knows (TR35 §4.3): a language
# alone is one word — its letters at five bits each (15), the script's
# index (8), the region's index (8), sorted by language; the rest (a
# language with a script or a region, und with either) are pairs of packed
# locales. The parents CLDR names (parentLocales) and the matching rules of
# languageInfo.xml with their variables, expanded over the containment of
# regions.
def pack15(lang):
    v = 0
    for c in lang:
        v = (v << 5) | (ord(c) - 96)
    return v

def contained(region, seen=None):
    out = {region}
    for g in SUPP.find("territoryContainment").iter("group"):
        if g.get("type") == region and g.get("status") != "deprecated" and region not in g.get("contains").split():
            for r in g.get("contains").split():
                out |= contained(r)
    return out

def gen_likely():
    scripts = sorted({split_tag(to)[1] for to in LIKELY.values()})
    regions = sorted({split_tag(to)[2] for to in LIKELY.values()})
    sidx = {x: i for i, x in enumerate(scripts)}
    ridx = {x: i for i, x in enumerate(regions)}
    assert len(scripts) <= 256 and len(regions) <= 256
    words, pairs = [], []
    for frm, to in LIKELY.items():
        l, s, r = split_tag(frm)
        l2, s2, r2 = split_tag(to)
        if l != "und" and not s and not r and re.fullmatch(r"[a-z]{2,3}", l):
            assert l2 == l
            words.append((pack15(l) << 17) | (sidx[s2] << 8) | ridx[r2])
        else:
            pairs.append((pack("" if l == "und" else l, s, r), pack(l2, s2, r2)))
    words.sort()
    pairs.sort()
    parents = []
    for child, par in PARENTS.items():
        cl, cs, cr = split_tag(child)
        pl, ps, pr = split_tag(par) if par != "root" else ("", "", "")
        parents.append((pack(cl, cs, cr), pack(pl, ps, pr)))
    parents.sort()
    info = ET.fromstring(read("supplemental/languageInfo.xml"))
    matches = info.find("languageMatching").find("languageMatches")
    variables = {}
    for v in matches.iter("matchVariable"):
        regs = set()
        for r in v.get("value").split("+"):
            regs |= contained(r)
        variables[v.get("id")[1:]] = sorted(regs)
    vnames = sorted(variables)
    def field(level, value):
        if value == "*":
            return 0
        if level == 0:
            return pack_language(value)
        if level == 1:
            return pack_script(value)
        if value.startswith("$!"):
            return 0xC000 | vnames.index(value[2:])
        if value.startswith("$"):
            return 0x8000 | vnames.index(value[1:])
        return pack_region(value)
    rules = []
    for m in matches.iter("languageMatch"):
        d = m.get("desired").split("_")
        sp = m.get("supported").split("_")
        assert len(d) == len(sp)
        row = [len(d)]
        for side in (d, sp):
            row += [field(k, side[k]) if k < len(side) else 0 for k in range(3)]
        row.append(int(m.get("distance")) | (256 if m.get("oneway") == "true" else 0))
        rules.append(row)
    paradigm = sorted(pack(*maximize(x)) for x in matches.find("paradigmLocales").get("locales").split())
    var_rows = []
    var_start = []
    for name in vnames:
        var_start.append(len(var_rows))
        var_rows += sorted(pack_region(r) for r in variables[name])
    var_start.append(len(var_rows))
    body = [
        f"    // {len(scripts)} scripts and {len(regions)} regions the likely subtags name, as txt::locale packs them",
        emit_array("LikelyScripts", [pack_script(x) for x in scripts], "uint32_t", hexa=True),
        emit_array("LikelyRegions", [pack_region(x) for x in regions], "uint16_t"),
        "    // a language alone: its letters at five bits each << 17 | the script's index << 8 | the region's",
        Packed("LikelyLanguages", words, 6).emit(),
        f"    inline constexpr uint32_t LikelyLanguageCount = {len(words)};",
        "    // the others: a packed locale (und: language 0) and what it stands for, sorted",
        emit_array("LikelyKeys", [a for a, _ in pairs], "uint64_t", hexa=True),
        emit_array("LikelyValues", [b for _, b in pairs], "uint64_t", hexa=True),
        "    // the parents of parentLocales: a packed locale and its parent (0: root), sorted",
        emit_array("ParentKeys", [a for a, _ in parents], "uint64_t", hexa=True),
        emit_array("ParentValues", [b for _, b in parents], "uint64_t", hexa=True),
        "    // the matching rules of languageInfo.xml in their order: the fields compared (1 language, 2 and\n"
        "    // script, 3 and region), the desired language, script, region, the supported ones (0: any; a\n"
        "    // region | 0x8000: in the variable, | 0xC000: not in it), the distance | 256 when one way",
        "    inline constexpr uint32_t MatchRules[][8] = {\n" + "\n".join("        {" + ", ".join(map(str, r)) + "}," for r in rules) + "\n    };",
        "    // the variables' regions (" + ", ".join(vnames) + "), sorted, each from its start to the next",
        emit_array("MatchVariableStart", var_start, "uint16_t"),
        emit_array("MatchVariableRegions", var_rows, "uint16_t"),
        "    // the paradigm locales, maximized",
        emit_array("ParadigmLocales", paradigm, "uint64_t", hexa=True),
    ]
    write_header("sgcl/txt/detail/cldr_likely.h", f"""
The likely subtags, the parent locales and the language matching data of
CLDR {CLDR} (supplemental likelySubtags.xml, supplementalData.xml and
languageInfo.xml; generated by tools/cldr_tables.py: do not edit).
""", "\n\n".join(body))
    return rules

#------------------------------------------------------------------------------
# cldr_autonyms.h: every locale's name in itself
#------------------------------------------------------------------------------
def autonym(name):
    lang, script, region = split_tag(name)
    for code in [join_tag(lang, script, region), join_tag(lang, "", region), join_tag(lang, script, ""), lang]:
        v = lookup(name, ("localeDisplayNames", "languages", f"language[type={code}]"))
        if v:
            return v
    return ""

def gen_autonyms():
    pool = Pool("Autonym")
    idx = [0] + [pool.add(autonym(n)) for n in DATA_LOCALES]
    body = [pool.emit(), "    // by the index of cldr_locales.h", emit_array("Autonyms", idx, "uint16_t")]
    write_header("sgcl/txt/detail/cldr_autonyms.h", f"""
The name of every locale of cldr_locales.h in its own language, CLDR {CLDR}
(generated by tools/cldr_tables.py: do not edit): "polski", "Deutsch",
"British English".
""", "\n\n".join(body))

#------------------------------------------------------------------------------
# cldr_lists.h, cldr_relative.h
#------------------------------------------------------------------------------
LIST_TYPES = ["standard", "standard-short", "standard-narrow", "or", "or-short", "or-narrow",
              "unit", "unit-short", "unit-narrow"]
LIST_PARTS = ["start", "middle", "end", "2", "3"]

def gen_lists():
    pool = Pool("List")
    rows = SparseRows("ListPatterns", len(LIST_PARTS))
    locales = Rows("ListLocales", len(LIST_TYPES))
    index = []
    for name in ["root"] + DATA_LOCALES:
        row = []
        for t in LIST_TYPES:
            step_ = "listPattern" if t == "standard" else f"listPattern[type={t}]"
            row.append(rows.add([pool.add(lookup(name, ("listPatterns", step_, f"listPatternPart[type={part}]")))
                                 for part in LIST_PARTS]))
        index.append(locales.add(row))
    body = [pool.emit(),
            "    // the parts of a pattern: start, middle, end, the pattern of two, of three (0: none)",
            rows.emit(),
            "    // a locale: " + ", ".join(LIST_TYPES),
            locales.emit(),
            emit_array("ListLocale", index, "uint16_t")]
    write_header("sgcl/txt/detail/cldr_lists.h", f"""
The patterns of lists of CLDR {CLDR} for every locale of cldr_locales.h
(generated by tools/cldr_tables.py: do not edit).
""", "\n\n".join(body))

RELATIVE_UNITS = ["second", "minute", "hour", "day", "week", "month", "quarter", "year"]
RELATIVE_WIDTHS = ["", "-short", "-narrow"]

def gen_relative():
    pool = Pool("Relative")
    blocks = SparseRows("RelativeBlocks", 5 + 12)
    locales = Rows("RelativeLocales", len(RELATIVE_UNITS) * len(RELATIVE_WIDTHS))
    index = []
    for name in ["root"] + DATA_LOCALES:
        row = []
        for width in RELATIVE_WIDTHS:
            for unit in RELATIVE_UNITS:
                base = ("dates", "fields", f"field[type={unit}{width}]")
                b = [pool.add(lookup(name, base + (f"relative[type={k}]",))) for k in (-2, -1, 0, 1, 2)]
                for when in ("future", "past"):
                    for c in CATEGORIES:
                        b.append(pool.add(lookup(name, base + (f"relativeTime[type={when}]",
                                                               f"relativeTimePattern[count={c}]"))))
                row.append(blocks.add(b))
        index.append(locales.add(row))
    body = [pool.emit(),
            "    // a unit in a width: the words of -2, -1, 0, 1, 2 (yesterday, today...), then the patterns of the\n"
            "    // future and of the past in the forms zero one two few many other (0: none)",
            blocks.emit(),
            "    // a locale: the units " + ", ".join(RELATIVE_UNITS) + " wide, then abbreviated, then narrow",
            locales.emit(),
            emit_array("RelativeLocale", index, "uint16_t")]
    write_header("sgcl/txt/detail/cldr_relative.h", f"""
Relative time of CLDR {CLDR} ("in 3 days", "yesterday") for every locale
of cldr_locales.h (generated by tools/cldr_tables.py: do not edit).
""", "\n\n".join(body))

#------------------------------------------------------------------------------
# cldr_dates.h (sgcl/time/detail): the Gregorian calendar
#------------------------------------------------------------------------------
GREG = ("dates", "calendars", "calendar[type=gregorian]")
NAME_WIDTHS = ["wide", "abbreviated", "narrow"]
DAY_KEYS = ["mon", "tue", "wed", "thu", "fri", "sat", "sun"]
PERIODS = ["am", "pm", "midnight", "noon", "morning1", "morning2", "afternoon1", "afternoon2",
           "evening1", "evening2", "night1", "night2"]
LENGTHS = ["full", "long", "medium", "short"]
GREATEST = ["G", "y", "M", "d", "a", "B", "h", "H", "m"]

def calendar_keys(name, sub):
    # the ids a locale's chain gives under availableFormats or intervalFormats
    out = set()
    for loc in chain(name):
        for k in locale_file(loc):
            if k[:len(GREG) + 2] == GREG + ("dateTimeFormats", sub) and len(k) > len(GREG) + 2:
                out.add(k[len(GREG) + 2])
    return out

# appendItems of availableFormats and the field whose display name is {2}
APPEND = [("Era", "era"), ("Year", "year"), ("Quarter", "quarter"), ("Month", "month"), ("Week", "week"),
          ("Day-Of-Week", "weekday"), ("Day", "day"), ("Hour", "hour"), ("Minute", "minute"), ("Second", "second"),
          ("Timezone", "zone")]

def gen_dates():
    pool = Pool("Date")
    tables = {k: Rows(n, w) for k, (n, w) in {"m": ("MonthNames", 12), "d": ("DayNames", 7), "q": ("QuarterNames", 4),
                                                "e": ("EraNames", 6), "p": ("PeriodNames", 12)}.items()}
    skeletons = Pool("Skeleton")
    zone_rows = Rows("ZoneFormats", 4)
    append_rows = Rows("AppendItems", 2 * len(APPEND))
    items, item_runs = [], {}
    intervals, interval_runs = [], {}
    def run_of(store, runs, entries):
        entries = tuple(entries)
        if entries not in runs:
            runs[entries] = (len(store), len(entries))
            store.extend(entries)
        start, n = runs[entries]
        assert n < 4096 and start < (1 << 24)
        return (start << 12) | n
    resolved = {}
    def formats_of(name):
        # the resolved availableFormats and intervalFormats of a locale
        if name in resolved:
            return resolved[name]
        avail = {}
        for k in calendar_keys(name, "availableFormats"):
            attrs = dict(re.findall(r"\[(\w+)=([^\]]*)\]", k))
            if "alt" in attrs or attrs.get("count", "other") != "other":
                continue
            v = lookup(name, GREG + ("dateTimeFormats", "availableFormats", k))
            if v:
                avail[attrs["id"]] = v
        inter = {}
        for k in calendar_keys(name, "intervalFormats"):
            attrs = dict(re.findall(r"\[(\w+)=([^\]]*)\]", k))
            if "alt" in attrs or "id" not in attrs:
                continue
            for g in GREATEST:
                v = lookup(name, GREG + ("dateTimeFormats", "intervalFormats", k, f"greatestDifference[id={g}]"))
                if v:
                    inter[(attrs["id"], g)] = v
        resolved[name] = (avail, inter)
        return resolved[name]
    records = []
    item_index, interval_index, base_index = [], [], []
    for name in ["root"] + DATA_LOCALES:
        rec = []
        for ctx in ("format", "stand-alone"):
            for w in NAME_WIDTHS:
                rec.append(tables["m"].add([pool.add(lookup(name, GREG + ("months", f"monthContext[type={ctx}]",
                                            f"monthWidth[type={w}]", f"month[type={m}]"))) for m in range(1, 13)]))
        for ctx in ("format", "stand-alone"):
            for w in ["wide", "abbreviated", "short", "narrow"]:
                rec.append(tables["d"].add([pool.add(lookup(name, GREG + ("days", f"dayContext[type={ctx}]",
                                            f"dayWidth[type={w}]", f"day[type={d}]"))) for d in DAY_KEYS]))
        for ctx in ("format", "stand-alone"):
            for w in NAME_WIDTHS:
                rec.append(tables["q"].add([pool.add(lookup(name, GREG + ("quarters", f"quarterContext[type={ctx}]",
                                            f"quarterWidth[type={w}]", f"quarter[type={q}]"))) for q in range(1, 5)]))
        rec.append(tables["e"].add([pool.add(lookup(name, GREG + ("eras", el, f"era[type={e}]")))
                                    for el in ("eraNames", "eraAbbr", "eraNarrow") for e in (0, 1)]))
        for ctx in ("format", "stand-alone"):
            for w in NAME_WIDTHS:
                rec.append(tables["p"].add([pool.add(lookup(name, GREG + ("dayPeriods", f"dayPeriodContext[type={ctx}]",
                                            f"dayPeriodWidth[type={w}]", f"dayPeriod[type={p}]"))) for p in PERIODS]))
        for kind in ("date", "time"):
            for l in LENGTHS:
                rec.append(pool.add(lookup(name, GREG + (f"{kind}Formats", f"{kind}FormatLength[type={l}]",
                                                         f"{kind}Format", "pattern"))))
        for l in LENGTHS:
            rec.append(pool.add(lookup(name, GREG + ("dateTimeFormats", f"dateTimeFormatLength[type={l}]",
                                                     "dateTimeFormat", "pattern"))))
        for l in LENGTHS:
            rec.append(pool.add(lookup(name, GREG + ("dateTimeFormats", f"dateTimeFormatLength[type={l}]",
                                                     "dateTimeFormat[type=atTime]", "pattern"))))
        # the formats of skeletons and of intervals: a variant keeps what
        # differs from its base, the first locale of its chain below root
        ch = chain(name)
        base = ch[-2] if len(ch) > 2 else None
        avail, inter = formats_of(name)
        bavail, binter = formats_of(base) if base else ({}, {})
        own = sorted((skeletons.add(k), pool.add(v)) for k, v in avail.items() if bavail.get(k) != v)
        item_index.append(run_of(items, item_runs, [(a_ << 16) | b_ for a_, b_ in own]))
        own = sorted((skeletons.add(k), GREATEST.index(g), pool.add(v)) for (k, g), v in inter.items()
                     if binter.get((k, g)) != v)
        interval_index.append(run_of(intervals, interval_runs, [(a_ << 20) | (g_ << 16) | b_ for a_, g_, b_ in own]))
        base_index.append(INDEX[base] if base else 0)
        rec.append(pool.add(lookup(name, GREG + ("dateTimeFormats", "intervalFormats", "intervalFormatFallback"))))
        tz = ("dates", "timeZoneNames")
        rec.append(zone_rows.add([pool.add(lookup(name, tz + (x,))) for x in
                                  ("hourFormat", "gmtFormat", "gmtZeroFormat", "fallbackFormat")]))
        ns = lookup(name, ("numbers", "defaultNumberingSystem"))
        rec.append(pool.add(NUMBERING[ns].get("digits")))
        rec.append(pool.add(lookup(name, ("numbers", f"symbols[numberSystem={ns}]", "decimal"))))
        rec.append(append_rows.add([pool.add(lookup(name, GREG + ("dateTimeFormats", "appendItems",
                                                                  f"appendItem[request={r}]"))) for r, _ in APPEND]
                                   + [pool.add(lookup(name, ("dates", "fields", f"field[type={f}]", "displayName")))
                                      for _, f in APPEND]))
        records.append(rec)
    width = len(records[0])
    cal = Rows("DateLocales", width)
    index = [cal.add(rec) for rec in records]
    assert len(skeletons.texts) < 4096
    # the supplemental data: the week, the hours, the periods of the day
    first_day = {}
    min_days = {}
    week = SUPP.find("weekData")
    days = {"sun": 7, "mon": 1, "tue": 2, "wed": 3, "thu": 4, "fri": 5, "sat": 6}
    for e in week.iter("firstDay"):
        if e.get("alt"):
            continue
        for t in e.get("territories").split():
            first_day[t] = days[e.get("day")]
    for e in week.iter("minDays"):
        if e.get("alt"):
            continue
        for t in e.get("territories").split():
            min_days[t] = int(e.get("count"))
    regions = sorted(set(first_day) | set(min_days), key=pack_region)
    week_rows = [(pack_region(r), first_day.get(r, first_day["001"]) | (min_days.get(r, min_days["001"]) << 4))
                 for r in regions]
    hours = []
    for e in SUPP.find("timeData").iter("hours"):
        pref = e.get("preferred")
        allowed = e.get("allowed").split()[0]
        code = ord(pref) | (ord(allowed[0]) << 8) | ((ord(allowed[1]) if len(allowed) > 1 else 0) << 16)
        for r in e.get("regions").split():
            if "_" in r:
                l, rg = r.split("_")
                hours.append((pack(l, "", rg), code))
            else:
                hours.append((pack("", "", r), code))
    hours.sort()
    periods = ET.fromstring(read("supplemental/dayPeriods.xml"))
    period_keys = []
    period_rows = []
    for block in periods.iter("dayPeriodRuleSet"):
        if block.get("type"):
            continue   # the selection rules, not the format's
        for rules in block.iter("dayPeriodRules"):
            row = []
            for r in rules.iter("dayPeriodRule"):
                def minutes(t):
                    h, m = t.split(":")
                    return int(h) * 60 + int(m)
                k = PERIODS.index(r.get("type"))
                if r.get("at"):
                    a_ = minutes(r.get("at"))
                    row.append((k << 24) | (a_ << 12) | a_ | (1 << 23))
                else:
                    row.append((k << 24) | (minutes(r.get("from")) << 12) | minutes(r.get("before")))
            for loc in rules.get("locales").split():
                lang, _, region = split_tag(loc)
                period_keys.append((pack(lang if lang != "root" else "", "", region), len(period_rows)))
            period_rows.append(row)
    period_keys.sort()
    pflat, pstart = [], []
    for row in period_rows:
        pstart.append(len(pflat))
        pflat += row
    pstart.append(len(pflat))
    body = [pool.emit(), skeletons.emit(),
            "    // names in one context and width: the months, the days (Monday first), the quarters, the eras\n"
            "    // (wide BC AD, abbreviated BC AD, narrow BC AD), the periods of the day (" + " ".join(PERIODS) + ")",
            tables["m"].emit(), tables["d"].emit(), tables["q"].emit(), tables["e"].emit(), tables["p"].emit(),
            "    // a locale's calendar: the rows of the months (format wide, abbreviated, narrow, stand-alone the\n"
            "    // same), of the days (format wide, abbreviated, short, narrow, stand-alone the same), of the\n"
            "    // quarters (as the months), of the eras, of the periods of the day (as the months); the texts of\n"
            "    // the date patterns full long medium short, the time patterns, the date-time patterns, their\n"
            "    // atTime forms; the interval fallback, the row of ZoneFormats, the digits of the locale's\n"
            "    // numbering system and its decimal separator (for fractions of a second), the row of AppendItems",
            cal.emit(),
            emit_array("DateLocale", index, "uint16_t"),
            "    // by locale index: the runs of DateItems and of DateIntervals (start << 12 | length), and the\n"
            "    // locale whose runs are read after its own (a variant's base; 0: none)",
            Packed("DateItemRuns", item_index, 5).emit(),
            Packed("DateIntervalRuns", interval_index, 5).emit(),
            emit_array("DateBase", base_index, "uint16_t"),
            "    // a skeleton's pattern: the skeleton's index << 16 | the text, sorted by skeleton within a run",
            Packed("DateItems", items, 5).emit(),
            "    // an interval's pattern: the skeleton << 20 | the greatest difference (" + " ".join(GREATEST) +
            ") << 16 | the text",
            Packed("DateIntervals", intervals, 5).emit(),
            "    // the zone formats: hourFormat, gmtFormat, gmtZeroFormat, fallbackFormat",
            zone_rows.emit(),
            "    // appendItems (" + ", ".join(r for r, _ in APPEND) + "), then the display names of those fields",
            append_rows.emit(),
            "    // the week of a region: the first day (ISO, Monday 1) | the least days of the first week << 4",
            emit_array("WeekKeys", [k for k, _ in week_rows], "uint16_t"),
            emit_array("WeekRules", [v for _, v in week_rows], "uint8_t"),
            "    // the hours of a region (or a language and region, as txt::locale packs them): the preferred\n"
            "    // letter | the first allowed form's letters << 8 (h, H, K, k; hB, hb)",
            emit_array("HourKeys", [k for k, _ in hours], "uint64_t", hexa=True),
            emit_array("HourRules", [v for _, v in hours], "uint32_t", hexa=True),
            "    // the rules of the periods of the day by language: the period << 24 | an exact time << 23 |\n"
            "    // from << 12 | before, in minutes of the day; a language's rules run from its start to the next",
            emit_array("PeriodKeys", [k for k, _ in period_keys], "uint64_t", hexa=True),
            emit_array("PeriodSets", [v for _, v in period_keys], "uint16_t"),
            emit_array("PeriodStart", pstart, "uint16_t"),
            emit_array("PeriodRules", pflat, "uint32_t", hexa=True)]
    write_header("sgcl/time/detail/cldr_dates.h", f"""
The Gregorian calendar of CLDR {CLDR} for every locale of
txt/detail/cldr_locales.h: names, patterns, the patterns of skeletons and
intervals, the zone formats, and the supplemental week, hour and day period
rules (generated by tools/cldr_tables.py: do not edit).
""", "\n\n".join(body))

#------------------------------------------------------------------------------
# sgcl/txt/names/<locale>.h: display names, one optional header a locale
#------------------------------------------------------------------------------
# DESIGN 493/494: the names of languages, regions, scripts, currencies (with
# their plural forms) and time zones in one language a header, for every
# locale CLDR rates modern or moderate and their variants. A variant holds what
# differs from its parent and includes the parent's header; a header registers
# its table when included (detail/registry.h). The zones' identifiers and
# their metazones are shared (names/detail/zones.h).
NAMES_DIR = "sgcl/txt/names"

def names_locales():
    out = []
    for n in DATA_LOCALES:
        p = n.split("_")
        levels = [COVERAGE.get("_".join(p[:i])) for i in range(1, len(p) + 1)] + [COVERAGE.get(c) for c in chain(n)]
        if any(l in ("modern", "moderate") for l in levels):
            out.append(n)
    return out

def tag_of(name):
    return name.replace("_", "-")

def ident_of(name):
    return "l_" + name.replace("-", "_")

def zone_data():
    # the canonical CLDR identifiers, every alias of each (bcp47/timezone.xml),
    # and the metazone each uses today (metaZones.xml)
    tz = ET.fromstring(read("bcp47/timezone.xml"))
    canon, alias = [], {}
    for t in tz.iter("type"):
        names = (t.get("alias") or "").split()
        if not names:
            continue
        canon.append(names[0])
        for a in names:
            alias[a] = names[0]
        if t.get("iana"):
            alias[t.get("iana")] = names[0]
    canon = sorted(set(canon))
    mz = ET.fromstring(read("supplemental/metaZones.xml"))
    uses = {}
    metas = set()
    for z in mz.find("metaZones").find("metazoneInfo").iter("timezone"):
        for u in z.iter("usesMetazone"):
            metas.add(u.get("mzone"))
            if u.get("to") is None:
                uses[z.get("type")] = u.get("mzone")
    return canon, alias, sorted(metas), uses

def split_tag_key(k):
    # (language letters, script, region) of a packed locale
    lang = k >> 32
    letters = "".join(chr((lang >> sh) & 0xFF) for sh in (16, 8, 0) if (lang >> sh) & 0xFF)
    return letters, (k >> 12) & 0xFFFFF, (k >> 1) & 0x7FF

def is_plain(k):
    _, sc, r = split_tag_key(k)
    return sc == 0 and r == 0

def pk(name_, rows):
    return name_ if rows else "Packed{}"

def gen_names():
    os.makedirs(NAMES_DIR + "/detail", exist_ok=True)
    canon, alias, metas, uses = zone_data()
    cidx = {z: i for i, z in enumerate(canon)}
    midx = {m: i for i, m in enumerate(metas)}
    # the shared zones header
    pool = Pool("Zone")
    aliases = sorted(alias.items())
    body = [pool.emit() if False else ""]
    zp = Pool("ZoneId")
    ids = [zp.add(z) for z in canon]
    mp = Pool("Metazone")
    mids = [mp.add(m) for m in metas]
    ap = Pool("ZoneAlias")
    alias_rows = [(ap.add(a), cidx[c]) for a, c in aliases if c in cidx]
    zone_meta = [midx[uses[z]] + 1 if z in uses else 0 for z in canon]
    # the country of a zone (windowsZones' territories), and whether the
    # location format names the country rather than the city: the zone is
    # its country's only one, or its primary one (metaZones' primaryZones)
    wz = ET.fromstring(read("supplemental/windowsZones.xml"))
    country, per_country = {}, collections.Counter()
    for m in wz.iter("mapZone"):
        t = m.get("territory")
        if t in ("001", "ZZ") or not re.fullmatch(r"[A-Z]{2}", t):
            continue
        for z in m.get("type").split():
            z = alias.get(z, z)
            if z in cidx and z not in country:
                country[z] = t
                per_country[t] += 1
    primary = {p_.text: p_.get("iso3166") for p_ in ET.fromstring(read("supplemental/metaZones.xml")).iter("primaryZone")}
    zone_region = []
    for z in canon:
        t = country.get(z)
        if not t:
            zone_region.append(0)
            continue
        named = per_country[t] == 1 or primary.get(z) == t
        zone_region.append(pack_region(t) | (0x8000 if named else 0))
    body = [zp.emit(), "    // the canonical zones, sorted: their texts", emit_array("ZoneIds", ids, "uint16_t"),
            mp.emit(), "    // the metazones, sorted", emit_array("MetazoneIds", mids, "uint16_t"),
            "    // a zone's metazone today, + 1 (0: none)", emit_array("ZoneMetazones", zone_meta, "uint16_t"),
            "    // a zone's country, packed as txt::locale packs a region | 0x8000 when the location format names",
            "    // the country (its only zone, or its primary one); 0: none",
            emit_array("ZoneRegions", zone_region, "uint16_t"),
            ap.emit(), "    // every name of a zone (the IANA ones and CLDR's old ones), sorted, and its canonical zone",
            emit_array("ZoneAliasNames", [a for a, _ in alias_rows], "uint16_t"),
            emit_array("ZoneAliasZones", [c for _, c in alias_rows], "uint16_t")]
    write_header(NAMES_DIR + "/detail/zones.h", f"""
The time zones of CLDR {CLDR} the display names speak of (bcp47/timezone.xml,
supplemental/metaZones.xml; generated by tools/cldr_tables.py: do not edit):
their identifiers, their aliases and the metazone each uses today.
""", "\n\n".join(body))
    locales = names_locales()
    lset = set(locales)
    # the currencies a region uses today: the names of the others (the Polish
    # złoty of 1950–1995) are history, and half the text of a header
    tenders = set()
    for region in SUPP.find("currencyData").iter("region"):
        for c in region.iter("currency"):
            if c.get("to") is None and c.get("tender") != "false":
                tenders.add(c.get("iso4217"))
    files = {}
    def resolved(name):
        if name in files:
            return files[name]
        d = {"lang": {}, "region": {}, "script": {}, "alone": {}, "cur": {}, "zone": {}, "meta": {}, "fmt": {}}
        for loc in chain(name):
            for k in locale_file(loc):
                if k[0] == "localeDisplayNames" and len(k) == 3 and k[1] in ("languages", "territories", "scripts"):
                    attrs = dict(re.findall(r"\[(\w+)=([^\]]*)\]", k[2]))
                    kind = {"languages": "lang", "territories": "region", "scripts": "script"}[k[1]]
                    # a script's stand-alone name where it has one ("Simplified Han"), as ICU
                    # names a script alone; no other alternative
                    if kind == "script" and attrs.get("alt") == "stand-alone":
                        v = lookup(name, k)
                        if v and attrs["type"] not in d["alone"]:
                            d["alone"][attrs["type"]] = v
                        continue
                    if "alt" in attrs or "menu" in attrs:
                        continue
                    code = attrs["type"]
                    if code not in d[kind]:
                        v = lookup(name, k)
                        if v:
                            d[kind][code] = v
                elif k[:2] == ("numbers", "currencies") and len(k) == 4:
                    code = k[2][len("currency[type="):-1]
                    if (code not in tenders and not code.startswith("X")) or code in d["cur"]:
                        continue
                    base = ("numbers", "currencies", f"currency[type={code}]")
                    names = [lookup(name, base + ("displayName",))] + \
                            [lookup(name, base + (f"displayName[count={c}]",)) for c in CATEGORIES]
                    if any(names):
                        d["cur"][code] = names
                elif k[:2] == ("dates", "timeZoneNames") and len(k) >= 4 and k[2].startswith(("zone[", "metazone[")):
                    t = k[2].split("=", 1)[1][:-1]
                    if k[2].startswith("zone["):
                        if t not in cidx or t in d["zone"]:
                            continue
                        base = ("dates", "timeZoneNames", k[2])
                        d["zone"][t] = [lookup(name, base + ("exemplarCity",))] + \
                            [lookup(name, base + (w, x)) for w in ("long", "short") for x in ("generic", "standard", "daylight")]
                    else:
                        if t not in midx or t in d["meta"]:
                            continue
                        base = ("dates", "timeZoneNames", k[2])
                        d["meta"][t] = [lookup(name, base + (w, x)) for w in ("long", "short") for x in ("generic", "standard", "daylight")]
        ld = ("localeDisplayNames", "localeDisplayPattern")
        tzn = ("dates", "timeZoneNames")
        ns = lookup(name, ("numbers", "defaultNumberingSystem"))
        d["fmt"] = {"pattern": lookup(name, ld + ("localePattern",)), "separator": lookup(name, ld + ("localeSeparator",)),
                    "keytype": lookup(name, ld + ("localeKeyTypePattern",)),
                    "region": lookup(name, tzn + ("regionFormat",)),
                    "region_std": lookup(name, tzn + ("regionFormat[type=standard]",)),
                    "region_dst": lookup(name, tzn + ("regionFormat[type=daylight]",)),
                    "fallback": lookup(name, tzn + ("fallbackFormat",))}
        for c in CATEGORIES:
            d["fmt"]["unit_" + c] = lookup(name, ("numbers", f"currencyFormats[numberSystem={ns}]", f"unitPattern[count={c}]"))
        files[name] = d
        return d
    sizes = []
    for name in locales:
        ch = chain(name)
        par = next((c for c in ch[1:] if c in lset), None)
        mine = resolved(name)
        base = resolved(par) if par else {"lang": {}, "region": {}, "script": {}, "alone": {}, "cur": {}, "zone": {},
                                          "meta": {}, "fmt": {}}
        pool = Pool("Name")
        def diff(kind):
            return {k: v for k, v in mine[kind].items() if base[kind].get(k) != v}
        def key_lang(code):
            l, sc, r = split_tag(code)
            return pack(l, sc, r)
        lang = sorted((key_lang(c), pool.add(v)) for c, v in diff("lang").items())
        region = sorted((pack_region(c), pool.add(v)) for c, v in diff("region").items() if re.fullmatch(r"[A-Z]{2}|\d{3}", c))
        script = sorted((pack_script(c), pool.add(v)) for c, v in diff("script").items() if re.fullmatch(r"[A-Z][a-z]{3}", c))
        alone = sorted((pack_script(c), pool.add(v)) for c, v in diff("alone").items() if re.fullmatch(r"[A-Z][a-z]{3}", c))
        cur = sorted((pack_currency(c), [pool.add(x) for x in v]) for c, v in diff("cur").items())
        zone = sorted((cidx[z], [pool.add(x) for x in v]) for z, v in diff("zone").items())
        meta = sorted((midx[m], [pool.add(x) for x in v]) for m, v in diff("meta").items())
        fmt = mine["fmt"]
        fmt_keys = ["pattern", "separator", "region", "region_std", "region_dst", "fallback"] + ["unit_" + c for c in CATEGORIES]
        fmt_row = [pool.add(fmt[k]) for k in fmt_keys]
        nm = ident_of(name)
        if par and pack(*key_of(name)) == pack(*key_of(par)):
            # a tag whose script is its language's default (zh_Hans, sr_Cyrl): the
            # same key as its parent's, so no table of its own, which would shadow
            # the parent's in the registry; its header includes the parent's
            if lang or region or script or alone or cur or zone or meta or \
                    any(fmt[k] != base["fmt"].get(k) for k in fmt_keys):
                print("names: %s differs from %s under one key" % (name, par), file=sys.stderr)
            path = f"{NAMES_DIR}/{tag_of(name)}.h"
            text = "\ufeff" + HEADER + f'#include "{tag_of(par)}.h"\n\n'
            comment = f"""The display names of the locale {tag_of(name)}: those of {tag_of(par)}, its script being
its language's default (one key, one table). Generated by tools/cldr_tables.py:
do not edit."""
            text += "".join(f"// {line}\n" for line in comment.split("\n"))
            if not (os.path.exists(path) and open(path, encoding="utf-8").read() == text):
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
            sizes.append((tag_of(name), len(text.encode())))
            continue
        def arr(name_, values, ctype, digits=3):
            if not values:
                return f"    inline constexpr const {ctype}* {name_} = nullptr;"
            if digits:
                return Packed(name_, values, digits).emit()
            return emit_array(name_, values, ctype, hexa=True)
        # a language alone is its letters at five bits each; a language with
        # a script or a region (en_GB, zh_Hant) a packed locale, apart
        # the names of tags (en_GB "British English") are left out: a
        # locale's name is composed of its parts, as ICU's standard names are
        plain = [(pack15(split_tag_key(k)[0]), v) for k, v in lang if is_plain(k)]
        tagged = []
        plain.sort()
        body = [pool.emit(),
                arr("LanguageKeys", [a for a, _ in plain], "uint16_t"),
                arr("LanguageNames", [b for _, b in plain], "uint16_t"),
                arr("TaggedKeys", [a for a, _ in tagged], "uint64_t", 0),
                arr("TaggedNames", [b for _, b in tagged], "uint16_t"),
                arr("RegionKeys", [a for a, _ in region], "uint16_t"),
                arr("RegionNames", [b for _, b in region], "uint16_t"),
                arr("ScriptKeys", [a for a, _ in script], "uint32_t", 4),
                arr("ScriptNames", [b for _, b in script], "uint16_t"),
                arr("AloneKeys", [a for a, _ in alone], "uint32_t", 4),
                arr("AloneNames", [b for _, b in alone], "uint16_t"),
                arr("CurrencyKeys", [a for a, _ in cur], "uint16_t"),
                arr("CurrencyNames", [x for _, v in cur for x in v], "uint16_t"),
                arr("ZoneKeys", [a for a, _ in zone], "uint16_t"),
                arr("ZoneNames", [x for _, v in zone for x in v], "uint16_t"),
                arr("MetaKeys", [a for a, _ in meta], "uint16_t"),
                arr("MetaNames", [x for _, v in meta for x in v], "uint16_t"),
                emit_array("Formats", fmt_row, "uint16_t"),
                f"""    inline constexpr Table table{{
        {hex(pack(*key_of(name)))}, {hex(pack(*key_of(par))) if par else 0},
        NameTexts,
        {pk("LanguageKeys", plain)}, {pk("LanguageNames", plain)}, {len(plain)},
        {"TaggedKeys" if tagged else "nullptr"}, {pk("TaggedNames", tagged)}, {len(tagged)},
        {pk("RegionKeys", region)}, {pk("RegionNames", region)}, {len(region)},
        {pk("ScriptKeys", script)}, {pk("ScriptNames", script)}, {len(script)},
        {pk("AloneKeys", alone)}, {pk("AloneNames", alone)}, {len(alone)},
        {pk("CurrencyKeys", cur)}, {pk("CurrencyNames", cur)}, {len(cur)},
        {pk("ZoneKeys", zone)}, {pk("ZoneNames", zone)}, {len(zone)},
        {pk("MetaKeys", meta)}, {pk("MetaNames", meta)}, {len(meta)},
        Formats,
    }};

    // the table registered when the header is included, once a program
    inline Registration registration{{&table}};"""]
        inc = ["detail/registry.h"] + ([tag_of(par) + ".h"] if par else [])
        path = f"{NAMES_DIR}/{tag_of(name)}.h"
        text = "﻿" + HEADER
        for x in inc:
            text += f'#include "{x}"\n'
        text += "\n#include <cstdint>\n\n"
        lang_name = autonym(name) or tag_of(name)
        comment = f"""The display names of the locale {tag_of(name)} ({lang_name}), CLDR {CLDR}: the names of languages,
regions, scripts, currencies and time zones in it{', what differs from ' + tag_of(par) + ' (included)' if par else ''}.
Including this header is all a program does: txt::locale::display_name,
region::display_name, script::display_name and currency::display_name then
answer in this language. Generated by tools/cldr_tables.py: do not edit."""
        text += "".join(f"// {line}\n" for line in comment.split("\n"))
        text += f"namespace sgcl::txt::detail::names::{nm} {{\n" + "\n\n".join(b for b in body if b).rstrip() + "\n}\n"
        if not (os.path.exists(path) and open(path, encoding="utf-8").read() == text):
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        sizes.append((tag_of(name), len(text.encode())))
    # all.h
    lines = ["﻿" + HEADER.rstrip(), "// Every locale's display names (DESIGN 494): what a server answering any",
             "// Accept-Language includes. Generated by tools/cldr_tables.py: do not edit.", ""]
    lines += [f'#include "{t}.h"' for t, _ in sizes]
    with open(f"{NAMES_DIR}/all.h", "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    total = sum(n for _, n in sizes)
    print(f"  {NAMES_DIR}: {len(sizes)} headers, {total / 1024 / 1024:.2f} MB, "
          f"largest {max(sizes, key=lambda x: x[1])}, median {sorted(n for _, n in sizes)[len(sizes) // 2] / 1024:.1f} KB")
    return sizes

def write_plural_samples(samples):
    # The samples of the rules (@integer, @decimal), every one written out
    # and asked of every locale of its rule set
    sets = []
    for kind, locs, _, _ in samples:
        if (kind, locs) not in sets:
            sets.append((kind, locs))
    lines = [HEADER.replace("#pragma once\n", "").rstrip("\n"),
             f"// The samples of the plural rules of CLDR {CLDR}, written out by tools/cldr_tables.py: do not edit.",
             "// A set: ordinal, its locales; a sample: its set, the number as text, the category.",
             "#pragma once", "", "struct PluralSampleSet {", "    bool ordinal;", "    const char* locales;", "};", "",
             "struct PluralSample {", "    int set;", "    const char* number;", "    int category;", "};", "",
             "inline constexpr PluralSampleSet PluralSampleSets[] = {"]
    for kind, locs in sets:
        lines.append(f'    {{{"true" if kind == "Ordinal" else "false"}, "{locs}"}},')
    lines += ["};", "", "inline constexpr PluralSample PluralSamples[] = {"]
    for kind, locs, number, cat in samples:
        lines.append(f'    {{{sets.index((kind, locs))}, "{number}", {CATEGORIES.index(cat)}}},')
    lines.append("};")
    with open("tests/txt/cldr_plural_samples.h", "w") as f:
        f.write("\n".join(lines) + "\n")

if __name__ == "__main__":
    gen_locales()
    plural_samples = gen_plurals()
    gen_numbers()
    gen_currencies()
    gen_likely()
    gen_autonyms()
    gen_lists()
    gen_relative()
    gen_dates()
    if "--names" in sys.argv or "--all" in sys.argv:
        gen_names()
    write_plural_samples(plural_samples)
    for path, size in SIZES:
        print(f"  {path}: {size / 1024:.1f} KB")
