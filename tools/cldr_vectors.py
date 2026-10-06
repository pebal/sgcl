#!/usr/bin/env python3
# Writes the vectors of the locale formatting tests from ICU: the same
# questions asked of tools/cldr_oracle.cpp (ICU 78, CLDR 48) and of
# tools/cldr_driver.cpp (sgcl, CLDR 46), and the cases where both answer
# alike written out as the expected answers:
#
#   tests/txt/cldr_vectors.h        numbers, plurals, lists, relative time,
#                                   likely subtags, matching
#   tests/time/cldr_date_vectors.h  patterns, styles, skeletons, intervals
#
# Run from the root of the repository with both programs built (their
# comments say how):
#
#   python3 tools/cldr_vectors.py /tmp/cldr_oracle /tmp/cldr_driver
#
# A case the two answer differently is left out and counted by kind and
# locale in what this prints: ICU's data is two versions of CLDR later than
# the library's, and every such case looked at was a change of the data
# (de-CH's group separator, Turkish currency patterns, new compact forms,
# zone names, which are display names and not here), not of the code. The
# answers of the library's own CLDR 46 data are held by hand-written tests
# beside these.
import collections
import random
import subprocess
import sys

ORACLE, DRIVER = sys.argv[1], sys.argv[2]
NAMES_DRIVER = sys.argv[3] if len(sys.argv) > 3 else None   # built with -DSGCL_CLDR_NAMES
random.seed(20261006)

NUMBER_LOCALES = ["en", "en-GB", "en-IN", "pl", "de", "de-AT", "fr", "fr-CA", "es", "es-MX", "it", "pt", "pt-PT",
                  "nl", "sv", "da", "nb", "fi", "cs", "sk", "hu", "ro", "bg", "ru", "uk", "el", "tr", "ar", "ar-EG",
                  "he", "fa", "hi", "bn", "ta", "te", "mr", "th", "vi", "id", "ms", "ja", "ko", "zh", "zh-Hant",
                  "zh-HK", "sw", "am", "lt", "lv", "et", "sl", "hr", "sr", "sr-Latn", "ca", "eu", "gl", "is", "ga",
                  "cy", "ka", "hy", "az", "kk", "uz", "my", "km", "lo", "si", "ne", "ur", "af", "zu", "fil"]
# the numbers, the relative times and the dates are asked of a sample, every
# fourth locale of the list and the first ten (the cases grow as styles ×
# values)
SAMPLE = sorted(set(NUMBER_LOCALES[:10] + NUMBER_LOCALES[::4]), key=NUMBER_LOCALES.index)
DATE_LOCALES = SAMPLE

def run(binary, questions):
    out = subprocess.run([binary], input="\n".join("\t".join(q) for q in questions) + "\n",
                         capture_output=True, text=True, check=True).stdout.split("\n")
    return out[:len(questions)]

def c_string(s):
    # a u8 literal of the text as it stands (the file is UTF-8 with a byte
    # order mark, which MSVC reads as UTF-8); controls and the invisible
    # characters escaped
    out = []
    for ch in s:
        c = ord(ch)
        if ch in '"\\':
            out.append("\\" + ch)
        elif c == 9:
            out.append("\\t")
        elif c < 0x20 or c == 0x7F:
            out.append("\\%03o" % c)
        elif c in (0x00A0, 0x00AD, 0x061C, 0x2007, 0x2009, 0x200A, 0x2060, 0x202F, 0xFEFF) or 0x200B <= c <= 0x200F \
                or 0x202A <= c <= 0x202E or 0x2066 <= c <= 0x2069 or 0x80 <= c < 0xA0:
            out.append("\\u%04X" % c)
        else:
            out.append(ch)
    return 'u8"' + "".join(out) + '"'

#------------------------------------------------------------------------------
# numbers: the options of txt::number_options and the skeleton of ICU that
# asks the same
#------------------------------------------------------------------------------
def skeleton(style, cur, disp, sign, mode, minf, maxf, mins, maxs, grp):
    sk = []
    default_digits = minf < 0 and maxf < 0 and maxs == 0
    if style == 0 and default_digits:
        sk.append(".###")
    if style == 1:
        sk += ["percent", "scale/100"] + (["precision-integer"] if default_digits else [])
    if style == 2:
        sk += ["permille", "scale/1000"] + (["precision-integer"] if default_digits else [])
    if style == 3:
        sk += ["scientific"] + ([".###"] if default_digits else [])
    if style == 4:
        sk.append("compact-short")
    if style == 5:
        sk.append("compact-long")
    if style in (6, 7, 8):
        sk.append("currency/" + cur)
    if style == 7:
        sk.append("sign-accounting" + {0: "", 1: "-always", 3: "-except-zero", 4: "-negative"}[sign])
    if style == 8:
        sk.append("compact-short")
    if style in (6, 7, 8) and disp:
        sk.append(["", "unit-width-narrow", "unit-width-iso-code"][disp])
    if style != 7 and sign:
        sk.append(["", "sign-always", "sign-never", "sign-except-zero", "sign-negative"][sign])
    if mode:
        sk.append(["", "rounding-mode-half-up", "rounding-mode-half-down", "rounding-mode-up", "rounding-mode-down",
                   "rounding-mode-ceiling", "rounding-mode-floor"][mode])
    if maxs > 0:
        sk.append("@" * max(mins, 1) + "#" * (maxs - max(mins, 1)))
    elif minf >= 0 and maxf >= 0:
        lo, hi = minf, maxf
        sk.append(("." + "0" * lo + "#" * (hi - lo)) if hi > 0 else "precision-integer")
    if not grp:
        sk.append("group-off")
    return " ".join(sk)

NUMBER_VALUES = ["0", "1", "-1", "1.5", "2.5", "3.5", "-2.675", "2.675", "12", "123", "1234", "12345", "123456",
                 "1234567", "12345678", "999999", "999950", "0.0001", "0.000123456", "1234.5678", "-1234.5678",
                 "98765432.123", "1e21", "0.5", "-0.5", "100", "1000000", "1500000", "2000000", "0.995", "9.9995",
                 "123456789012345", "-0"]

def number_cases():
    cases = []
    for loc in SAMPLE:
        for style in range(9):
            for v in random.sample(NUMBER_VALUES, 12):
                cur = random.choice(["USD", "EUR", "PLN", "JPY", "CHF", "GBP", "INR", "KWD", "BRL"])
                disp = random.choice([0, 0, 1, 2]) if style in (6, 7, 8) else 0
                sign = random.choice([0, 0, 0, 1, 3, 4] if style == 7 else [0, 0, 0, 1, 2, 3, 4])
                mode = random.choice([0, 0, 0, 1, 2, 3, 4, 5, 6])
                minf, maxf, mins, maxs = -1, -1, 0, 0
                r = random.random()
                if r < 0.15 and style not in (4, 5, 8):
                    minf, maxf = random.choice([(0, 2), (2, 2), (1, 4), (0, 0)])
                elif r < 0.25 and style != 8:
                    mins, maxs = random.choice([(1, 3), (3, 3), (2, 5)])
                grp = random.random() > 0.1
                ours = ["num", loc, str(style), cur if style in (6, 7, 8) else "", str(disp), str(sign), str(mode),
                        str(minf), str(maxf), str(mins), str(maxs), "1" if grp else "0", v]
                theirs = ["num", loc, skeleton(style, cur, disp, sign, mode, minf, maxf, mins, maxs, grp), v]
                cases.append((ours, theirs))
    return cases

def same_cases(kind_cases, driver=None):
    # (ours, theirs) pairs; the ones both answer alike, and the counts of the rest
    ours = run(driver or DRIVER, [o for o, _ in kind_cases])
    theirs = run(ORACLE, [t for _, t in kind_cases])
    kept, differ = [], collections.Counter()
    for (o, t), a, b in zip(kind_cases, ours, theirs):
        if a == b and not b.startswith("ERROR"):
            kept.append((o, a))
        else:
            differ[o[1]] += 1
    return kept, differ

def report(name, kept, differ):
    total = len(kept) + sum(differ.values())
    print(f"  {name}: {len(kept)} of {total} agree; differ most in " +
          ", ".join(f"{k} {v}" for k, v in differ.most_common(12)))

def main():
    out = []
    head = ["\ufeff//------------------------------------------------------------------------------",
            "// SGCL: a C++20 application platform",
            "// Copyright (c) 2022-2026 Sebastian Nibisz",
            "// SPDX-License-Identifier: Apache-2.0",
            "//------------------------------------------------------------------------------",
            "// Written by tools/cldr_vectors.py: do not edit. The answers of ICU 78 that the library",
            "// gives too, for its tests; a question is the fields of tools/cldr_driver.cpp, tab-separated.",
            "#pragma once", ""]
    sections = {}
    kept, differ = same_cases(number_cases())
    report("numbers", kept, differ)
    sections["NumberVectors"] = kept
    # plurals: integers and decimals as text, cardinal and ordinal
    cases = []
    for loc in SAMPLE + ["ga", "cy", "br", "mt", "gd", "lv", "fr-CA", "pt-PT", "pt-AO", "ar", "sl", "he"]:
        for v in ["0", "1", "2", "3", "4", "5", "6", "7", "11", "12", "14", "21", "22", "25", "100", "101", "102",
                  "111", "1000000", "0.5", "1.5", "2.1", "1.0", "0.0"]:
            cases.append((["plural", loc, "card", v], ["plural", loc, "card", v]))
        for v in ["1", "2", "3", "4", "5", "11", "12", "13", "21", "22", "23", "101", "111", "1000"]:
            cases.append((["plural", loc, "ord", v], ["plural", loc, "ord", v]))
    kept, differ = same_cases(cases)
    report("plurals", kept, differ)
    sections["PluralVectors"] = kept
    # lists
    words = ["Ala", "Ola", "Ela", "Iga", "Hiro", "otro", "8", "11", "Irene", "hielo", "English", "יוסי"]
    cases = []
    for loc in NUMBER_LOCALES:
        for t in range(3):
            for w in range(3):
                for n in random.sample((1, 2, 3, 4, 5), 2):
                    items = random.sample(words, n)
                    q = ["list", loc, str(t), str(w)] + items
                    cases.append((q, q))
    kept, differ = same_cases(cases)
    report("lists", kept, differ)
    sections["ListVectors"] = kept
    # relative time
    cases = []
    for loc in SAMPLE:
        for unit in range(8):
            for w in range(3):
                for numeric in ("0", "1"):
                    for v in random.sample(["-2", "-1", "-0.5", "0", "0.5", "1", "2", "3", "1.5", "10", "100", "1234.5",
                                            "-3", "-25", "5"], 2):
                        q = ["relative", loc, str(w), numeric, str(unit), v]
                        cases.append((q, q))
    kept, differ = same_cases(cases)
    report("relative", kept, differ)
    sections["RelativeVectors"] = kept
    # likely subtags and matching
    tags = ["pl", "de-CH", "zh-TW", "zh-Hant", "sr", "sr-ME", "und-PL", "en-GB", "es-MX", "pt-AO", "und", "und-Cyrl",
            "und-RU", "ar", "fa-AF", "pa-PK", "uz-AF", "az-IR", "ku", "ha-CM", "mn-CN", "yue", "yue-CN", "hy",
            "und-Latn-RS", "ja", "ko-KP", "en-US", "en-001", "es-419", "zh-SG", "zh-Hans-TW", "sh", "iw", "in",
            "tl", "mo", "nb", "no", "ms-CC", "kk-CN", "tg-PK", "ky-TR", "sd-IN", "ug-KZ", "xx", "und-Zzzz"]
    cases = [(["maximize", t], ["maximize", t]) for t in tags] + [(["minimize", t], ["minimize", t]) for t in tags]
    supported = ["en,de,en-GB,pt-BR,pt-PT,sr-Latn,zh-TW,fr,es,es-419,ja,no,nb,da,pl",
                 "en,en-US,en-GB,en-AU", "de,fr,it", "es,es-MX,es-419,pt", "zh-Hans,zh-Hant,en", "sr,sr-Latn,hr,bs"]
    desired = ["de-CH", "en-AU", "pl", "pt-AO", "sr", "zh-HK", "fr", "en-US", "nb", "da", "es-AR", "es-ES", "it",
               "ja-JP", "zh-SG", "hr", "bs", "sh", "en-IN", "en-CA", "pt-MO", "gsw", "lb", "af", "fr-CA", "ko",
               "de,fr", "ko,ja", "xx,de"]
    for s in supported:
        for d in desired:
            cases.append((["match", d, s], ["match", d, s]))
    kept, differ = same_cases(cases)
    report("likely and matching", kept, differ)
    sections["LocaleVectors"] = kept
    lines = list(head) + ["struct CldrVector {", "    const char8_t* question;", "    const char8_t* answer;", "};", ""]
    for name, rows in sections.items():
        lines.append(f"inline constexpr CldrVector {name}[] = {{")
        for q, a in rows:
            lines.append(f"    {{{c_string(chr(9).join(q))}, {c_string(a)}}},")
        lines += ["};", ""]
    with open("tests/txt/cldr_vectors.h", "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    # dates
    zones = ["UTC", "Europe/Warsaw", "America/New_York", "Asia/Kolkata", "Australia/Adelaide", "America/Sao_Paulo"]
    times = [1790258700000, 0, -2208988800000, 951782400000, 1735689599999, 1719835200123, 1700000000000,
             1609459200000 + 12 * 3600 * 1000, 1719835200000 + 3600 * 1000 * 7]
    patterns = ["G GGGG GGGGG", "y yy yyy yyyy u", "Q QQ QQQ QQQQ QQQQQ q qqq qqqq", "M MM MMM MMMM MMMMM",
                "L LL LLL LLLL LLLLL", "d dd D DDD F", "E EEEE EEEEE EEEEEE", "e ee eee eeee c ccc cccc ccccc",
                "a aaaa aaaaa", "b bbbb bbbbb", "B BBBB BBBBB", "h hh H HH K k", "m mm s ss S SS SSS", "A", "O OOOO",
                "Z ZZZZ ZZZZZ", "X XX XXX x xx xxx", "VV", "w ww W Y YY", "'quoted '' text' d", "g"]
    skels = ["yMd", "yMMMd", "yMMMMd", "yMMMMEEEEd", "MMMd", "MMMMd", "Md", "MEd", "MMMEd", "yM", "yMMM", "yMMMM",
             "y", "M", "MMM", "d", "Ed", "Hm", "hm", "jm", "Hms", "hms", "jms", "Bhm", "yQQQ", "yQQQQ", "Gy",
             "GyMMMd", "yMMMdjm", "yMMMMEEEEdjms", "yMdHm", "MMMdhm", "ms", "jmsSSS", "EEEEd", "yMMMEd", "H", "h",
             "j", "GyMMMEd"]
    cases = []
    for loc in DATE_LOCALES:
        for p in patterns:
            q = ["date", loc, p, str(random.choice(times)), random.choice(zones)]
            cases.append((q, q))
        for d in (-1, 0, 1, 2, 3):
            for t in (-1, 2, 3):
                if d == -1 and t == -1:
                    continue
                q = ["style", loc, str(d), str(t), str(random.choice(times)), random.choice(zones)]
                cases.append((q, q))
        for s in skels:
            q = ["skeleton", loc, s]
            cases.append((q, q))
        base = 1790258700000
        for sk in ["yMMMd", "yMMMMd", "MMMd", "yMd", "Md", "yMMM", "yMMMEd", "MMMEd", "hm", "Hm", "jm", "yMMMdjm",
                   "y", "MMM", "d", "Hms", "Bhm", "yMMMMEEEEd", "GyMMMd"]:
            for dlt in random.sample([0, 30 * 1000, 20 * 60 * 1000, 3 * 3600 * 1000, 9 * 3600 * 1000, 2 * 86400 * 1000,
                                      40 * 86400 * 1000, 400 * 86400 * 1000], 4):
                q = ["interval", loc, sk, str(base), str(base + dlt), random.choice(zones[:4])]
                cases.append((q, q))
    kept, differ = same_cases(cases)
    report("dates", kept, differ)
    lines = list(head) + ["struct CldrDateVector {", "    const char8_t* question;", "    const char8_t* answer;", "};", "",
                          "inline constexpr CldrDateVector DateVectors[] = {"]
    for q, a in kept:
        lines.append(f"    {{{c_string(chr(9).join(q))}, {c_string(a)}}},")
    lines += ["};", ""]
    with open("tests/time/cldr_date_vectors.h", "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def names_main():
    # the display names of the locales tests/txt/cldr_names_set.h includes
    locales = ["ar", "de", "de-CH", "en", "en-GB", "es", "es-MX", "fr", "hi", "ja", "pl", "pt", "ru", "sr-Latn", "zh-Hant"]
    tags = ["de", "de-CH", "en-GB", "sr-Latn-ME", "zh-Hant-TW", "pt-BR", "es-419", "fr-CA", "ja", "ar-EG", "und",
            "nb", "fil", "haw", "zh", "en-US", "pl-PL", "qaa", "uz-Arab", "ku-Latn-TR"]
    regions = ["PL", "DE", "CH", "US", "GB", "JP", "CN", "TW", "HK", "MO", "419", "001", "150", "BA", "CI", "KP", "KR",
               "MK", "PS", "EU", "UN", "ZZ", "AQ", "IO", "XK"]
    scripts = ["Latn", "Cyrl", "Arab", "Hans", "Hant", "Jpan", "Deva", "Grek", "Zzzz", "Zyyy", "Brai"]
    currencies = ["PLN", "EUR", "USD", "JPY", "CHF", "GBP", "CNY", "RUB", "INR", "BRL", "XAU", "KWD"]
    cases = []
    for loc in locales:
        for t in tags:
            cases.append((["dn-lang", loc, t], ["dn-lang", loc, t]))
        for r in regions:
            cases.append((["dn-region", loc, r], ["dn-region", loc, r]))
        for sc in scripts:
            cases.append((["dn-script", loc, sc], ["dn-script", loc, sc]))
        for c in currencies:
            for form in ["-1", "one", "few", "many", "other"]:
                cases.append((["dn-cur", loc, c, form], ["dn-cur", loc, c, form]))
            for v in ["1", "2", "5", "12.5", "-3", "1234567.891"]:
                ours = ["num", loc, "6", c, "3", "0", "0", "-1", "-1", "0", "0", "1", v]
                theirs = ["num", loc, "currency/" + c + " unit-width-full-name", v]
                cases.append((ours, theirs))
        for z in ["Europe/Warsaw", "America/New_York", "Asia/Tokyo", "Asia/Kolkata", "Europe/London", "UTC",
                  "Australia/Sydney", "America/Sao_Paulo"]:
            for ms in ["1790258700000", "1767225600000"]:
                for pat in ["z", "zzzz", "v", "vvvv", "VVV", "VVVV", "O"]:
                    q = ["date", loc, pat, ms, z]
                    cases.append((q, q))
                q = ["style", loc, "-1", "0", ms, z]
                cases.append((q, q))
    kept, differ = same_cases(cases, NAMES_DRIVER)
    report("display names", kept, differ)
    lines = ["\ufeff//------------------------------------------------------------------------------",
             "// SGCL: a C++20 application platform", "// Copyright (c) 2022-2026 Sebastian Nibisz",
             "// SPDX-License-Identifier: Apache-2.0",
             "//------------------------------------------------------------------------------",
             "// Written by tools/cldr_vectors.py: do not edit. The display names ICU 78 gives that the library gives",
             "// too with the headers of tests/txt/cldr_names_set.h included.",
             "#pragma once", "", "struct CldrNamesVector {", "    const char8_t* question;", "    const char8_t* answer;", "};", "",
             "inline constexpr CldrNamesVector NamesVectors[] = {"]
    for q, a in kept:
        lines.append(f"    {{{c_string(chr(9).join(q))}, {c_string(a)}}},")
    lines += ["};", ""]
    with open("tests/txt/cldr_names_vectors.h", "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

if NAMES_DRIVER:
    names_main()
else:
    main()
