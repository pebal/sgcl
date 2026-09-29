//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt against ICU on any text: normalization, the full case mappings,
// IDNA (UTS #46) and the collator, each asked the same question of the
// same code points. The first byte picks the area and its options; the
// rest is made UTF-8 by ICU (an ill-formed sequence is U+FFFD), and a text
// with a code point that Unicode 17 assigned is left out (the library's
// tables are Unicode 16, ICU 78's are 17: the two disagree there by
// design). What must hold:
//   - normalize to each of the four forms gives ICU's text, is_normalized
//     ICU's answer, and nfc∘nfd = nfc, nfd∘nfc = nfd, nfkc∘nfc = nfkc;
//   - to_upper_full, to_lower_full, to_title and fold_case give ICU's text
//     (root, Turkish and Lithuanian; the title with ICU's words adjusted
//     to the first cased letter, as the library adjusts);
//   - to_ascii and to_unicode refuse what ICU refuses and give what it
//     gives, the standard and the transitional processing, with and
//     without the STD3 rules;
//   - compare of two texts (the second after a NUL) has ICU's sign, for
//     the root and for a language picked by a byte, under the strengths
//     and the settings; and the keys compare as the texts do.
// Where the two differ by design or by version — UCA 16 against 17 (Han by
// radical and stroke, properties Unicode 17 changed), the DUCET against
// CLDR's root (U+FFFE/FFFF, the variable groups, some marks), UAX #29
// against ICU's dictionaries, a language's [reorder] the library does not
// honour, ICU's own bounds — the case is left out, each named where it is;
// the two findings once held for the auditor (a language's contraction
// past a mark, the case of each element of a letter that expands) are
// fixed and asked again (DESIGN 248 and the note after it).
// Built with libFuzzer and ICU:
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/icu4c/include -L/opt/homebrew/opt/icu4c/lib -licuuc -licui18n" \
//       tests/fuzz/run.sh tests/txt/fuzz/txt_icu_fuzz.cpp 300
#include "sgcl/txt/txt.h"

#include <unicode/ucol.h>
#include <unicode/uchar.h>
#include <unicode/uidna.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/ucasemap.h>
#include <unicode/uscript.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The two texts, printed in code points before the trap when
    // SGCL_FUZZ_EXPLAIN is set (a crash replayed: which side said what)
    void same_text(const std::string& ours, const std::string& theirs, const char* what) {
        if (ours == theirs) {
            return;
        }
        if (std::getenv("SGCL_FUZZ_EXPLAIN")) {
            auto print = [](const char* side, const std::string& s) {
                std::fprintf(stderr, "%s:", side);
                for (size_t i = 0; i < s.size();) {
                    auto [c, n] = utf8::decode(std::string_view(s), i);
                    std::fprintf(stderr, " %04X", unsigned(c));
                    i += n;
                }
                std::fprintf(stderr, "\n");
            };
            std::fprintf(stderr, "%s differs\n", what);
            print("ours", ours);
            print("icu ", theirs);
        }
        __builtin_trap();
    }

    // UTF-8 made from any bytes, as ICU makes it: U+FFFD for what is ill-formed
    std::u16string to16(std::string_view bytes) {
        std::u16string out(bytes.size() + 1, u'\0');
        int32_t n = 0;
        UErrorCode e = U_ZERO_ERROR;
        u_strFromUTF8WithSub(out.data(), int32_t(out.size()), &n, bytes.data(), int32_t(bytes.size()), 0xFFFD, nullptr, &e);
        check(U_SUCCESS(e));
        out.resize(size_t(n));
        return out;
    }

    std::string to8(const std::u16string& s) {
        std::string out(s.size() * 3 + 1, '\0');
        int32_t n = 0;
        UErrorCode e = U_ZERO_ERROR;
        u_strToUTF8(out.data(), int32_t(out.size()), &n, s.data(), int32_t(s.size()), &e);
        check(U_SUCCESS(e));
        out.resize(size_t(n));
        return out;
    }

    // Whether every code point was there in Unicode 16
    bool unicode16(const std::u16string& s) {
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            UVersionInfo age;
            u_charAge(c, age);
            if (age[0] > 16) {
                return false;
            }
        }
        return true;
    }

    bool han(const std::u16string& text) {
        // what the text decomposes to: 🈒 is a square around 双
        UErrorCode ne = U_ZERO_ERROR;
        const UNormalizer2* nfkd = unorm2_getNFKDInstance(&ne);
        std::u16string s(text.size() * 18 + 8, u'\0');
        int32_t made = unorm2_normalize(nfkd, text.data(), int32_t(text.size()), s.data(), int32_t(s.size()), &ne);
        check(U_SUCCESS(ne));
        s.resize(size_t(made));
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            UErrorCode e = U_ZERO_ERROR;
            if (uscript_getScript(c, &e) == USCRIPT_HAN) {
                return true;
            }
        }
        return false;
    }

    bool modifier_letters(const std::string& s) {
        for (size_t i = 0; i < s.size();) {
            auto [c, n] = utf8::decode(std::string_view(s), i);
            i += n;
            auto type = u_charType(UChar32(c));
            // and the controls: ICU answers "_" < "_\x03\u0300" while
            // "_" == "_\u0300" == "_\x03\u0300" (a completely ignorable
            // between a variable and its mark), not an order at all
            if (type == U_MODIFIER_LETTER || type == U_CONTROL_CHAR || type == U_FORMAT_CHAR) {
                return true;
            }
        }
        return false;
    }

    // Marks written out of their canonical order (the text is not FCD):
    // ICU with its normalization off, as a language's collator has it,
    // reads them as they stand, where the library puts them in order
    // first; the two part on a contraction reached past a mark
    bool not_fcd(const std::string& s) {
        UErrorCode e = U_ZERO_ERROR;
        const UNormalizer2* fcd = unorm2_getInstance(nullptr, "nfc", UNORM2_FCD, &e);
        check(U_SUCCESS(e));
        auto s16 = to16(s);
        bool yes = unorm2_isNormalized(fcd, s16.data(), int32_t(s16.size()), &e);
        check(U_SUCCESS(e));
        return !yes;
    }

    bool other_digits(const std::string& s) {
        for (size_t i = 0; i < s.size();) {
            auto [c, n] = utf8::decode(std::string_view(s), i);
            i += n;
            if (u_getIntPropertyValue(UChar32(c), UCHAR_NUMERIC_TYPE) == U_NT_DIGIT) {
                return true;
            }
        }
        return false;
    }

    bool odd_marks(const std::u16string& s) {
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            if ((U_GET_GC_MASK(c) & U_GC_M_MASK) && !(c >= 0x0300 && c <= 0x036F)) {
                return true;
            }
        }
        return false;
    }

    bool dictionary_words(const std::u16string& s) {
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            if (u_getIntPropertyValue(c, UCHAR_LINE_BREAK) == U_LB_COMPLEX_CONTEXT) {
                return true;   // Thai, Lao, Khmer, Myanmar, New Tai Lue...: ICU keeps a run together
            }
            UErrorCode e = U_ZERO_ERROR;
            switch (uscript_getScript(c, &e)) {
                case USCRIPT_HAN: case USCRIPT_HIRAGANA: case USCRIPT_KATAKANA: case USCRIPT_HANGUL:
                    return true;
                default:
                    break;
            }
        }
        return false;
    }

    template<class F>
    std::u16string icu16(const std::u16string& in, F f) {
        std::u16string out(in.size() * 4 + 16, u'\0');
        UErrorCode e = U_ZERO_ERROR;
        int32_t n = f(out.data(), int32_t(out.size()), &e);
        if (e == U_BUFFER_OVERFLOW_ERROR) {
            out.assign(size_t(n) + 1, u'\0');
            e = U_ZERO_ERROR;
            n = f(out.data(), int32_t(out.size()), &e);
        }
        check(U_SUCCESS(e));
        out.resize(size_t(n));
        return out;
    }

    std::string text(const string& s) {
        return std::string(s.view());
    }

    void normalization(const std::u16string& in16, const std::string& in) {
        UErrorCode e = U_ZERO_ERROR;
        const UNormalizer2* forms[4] = {unorm2_getNFCInstance(&e), unorm2_getNFDInstance(&e), unorm2_getNFKCInstance(&e), unorm2_getNFKDInstance(&e)};
        check(U_SUCCESS(e));
        string s(in);
        string ours[4] = {txt::normalize(s, txt::nfc), txt::normalize(s, txt::nfd), txt::normalize(s, txt::nfkc), txt::normalize(s, txt::nfkd)};
        bool normal[4] = {txt::is_normalized(s, txt::nfc), txt::is_normalized(s, txt::nfd), txt::is_normalized(s, txt::nfkc), txt::is_normalized(s, txt::nfkd)};
        for (int k = 0; k < 4; ++k) {
            auto theirs = icu16(in16, [&](UChar* d, int32_t cap, UErrorCode* err) {
                return unorm2_normalize(forms[k], in16.data(), int32_t(in16.size()), d, cap, err);
            });
            same_text(text(ours[k]), to8(theirs), "normalize");
            UErrorCode e2 = U_ZERO_ERROR;
            bool is = unorm2_isNormalized(forms[k], in16.data(), int32_t(in16.size()), &e2);
            check(U_SUCCESS(e2));
            check(normal[k] == is);
        }
        check(txt::normalize(ours[1], txt::nfc) == ours[0]);
        check(txt::normalize(ours[0], txt::nfd) == ours[1]);
        check(txt::normalize(ours[0], txt::nfkc) == ours[2]);
        check(txt::normalize(ours[0], txt::nfc) == ours[0]);
    }

    // Whether ICU's properties of the code points are the library's: a
    // letter Unicode 17 changed (ʕ is Ll and cased in 16, not in 17) is
    // a difference of the versions, left out
    bool same_case_properties(const std::u16string& s) {
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            if (bool(u_hasBinaryProperty(c, UCHAR_CASED)) != txt::detail::is_cased(char32_t(c))
                || char32_t(u_toupper(c)) != unicode::to_upper(char32_t(c))
                || char32_t(u_tolower(c)) != unicode::to_lower(char32_t(c))) {
                return false;
            }
        }
        return true;
    }

    // Whether ICU's Word_Break of the code points is the library's: a
    // character Unicode 17 moved to another class cuts the words, and the
    // title case with them, differently by design — U+00B8 CEDILLA is
    // Other in 16 (a break on each side: "G¸G" is two words, "G¸G" in
    // title case) and ALetter in 17 (one word, "G¸g")
    bool same_word_break(const std::u16string& s) {
        using txt::detail::wb;
        auto icu_of = [](wb x) -> int32_t {
            switch (x) {
                case wb::other: return U_WB_OTHER;
                case wb::cr: return U_WB_CR;
                case wb::lf: return U_WB_LF;
                case wb::newline: return U_WB_NEWLINE;
                case wb::extend: return U_WB_EXTEND;
                case wb::zwj: return U_WB_ZWJ;
                case wb::regional_indicator: return U_WB_REGIONAL_INDICATOR;
                case wb::format: return U_WB_FORMAT;
                case wb::katakana: return U_WB_KATAKANA;
                case wb::aletter: return U_WB_ALETTER;
                case wb::hebrew_letter: return U_WB_HEBREW_LETTER;
                case wb::mid_letter: return U_WB_MIDLETTER;
                case wb::mid_num: return U_WB_MIDNUM;
                case wb::mid_num_let: return U_WB_MIDNUMLET;
                case wb::single_quote: return U_WB_SINGLE_QUOTE;
                case wb::double_quote: return U_WB_DOUBLE_QUOTE;
                case wb::numeric: return U_WB_NUMERIC;
                case wb::extend_num_let: return U_WB_EXTENDNUMLET;
                case wb::wseg_space: return U_WB_WSEGSPACE;
            }
            return -1;
        };
        for (int32_t i = 0; i < int32_t(s.size());) {
            UChar32 c;
            U16_NEXT(s.data(), i, int32_t(s.size()), c);
            if (u_getIntPropertyValue(c, UCHAR_WORD_BREAK) != icu_of(txt::detail::wb_of(char32_t(c)))) {
                return false;
            }
        }
        return true;
    }

    void casing(uint8_t mode, const std::u16string& in16, const std::string& in) {
        if (!same_case_properties(in16)) {
            return;
        }
        const char* tags[3] = {"", "tr", "lt"};
        const char* tag = tags[mode % 3];
        txt::locale where = mode % 3 == 0 ? txt::locale() : txt::locale(string(tag));
        string s(in);
        auto upper = icu16(in16, [&](UChar* d, int32_t cap, UErrorCode* err) {
            return u_strToUpper(d, cap, in16.data(), int32_t(in16.size()), tag, err);
        });
        same_text(text(txt::to_upper_full(s, where)), to8(upper), "upper");
        auto lower = icu16(in16, [&](UChar* d, int32_t cap, UErrorCode* err) {
            return u_strToLower(d, cap, in16.data(), int32_t(in16.size()), tag, err);
        });
        same_text(text(txt::to_lower_full(s, where)), to8(lower), "lower");
        auto fold = icu16(in16, [&](UChar* d, int32_t cap, UErrorCode* err) {
            return u_strFoldCase(d, cap, in16.data(), int32_t(in16.size()), U_FOLD_CASE_DEFAULT, err);
        });
        same_text(text(txt::fold_case(s)), to8(fold), "fold");
        if (((mode >> 2) & 1) && !dictionary_words(in16) && same_word_break(in16)) {
            // the title case, the words ICU's: UAX #29 but for the scripts
            // ICU breaks by a dictionary or keeps in runs (Han, kana,
            // Hangul, and Line_Break=SA: Thai, Lao, Khmer, Myanmar, New Tai
            // Lue...), where it cuts a word the rules keep whole
            // ("PPP핀PPP" is one word of UAX #29) or keeps one they cut
            // ("ⅱᦛΣ" is three) — those left out
            UErrorCode e = U_ZERO_ERROR;
            UCaseMap* map = ucasemap_open(tag, U_TITLECASE_ADJUST_TO_CASED, &e);
            check(U_SUCCESS(e));
            std::string out(in.size() * 4 + 16, '\0');
            int32_t n = ucasemap_utf8ToTitle(map, out.data(), int32_t(out.size()), in.data(), int32_t(in.size()), &e);
            ucasemap_close(map);
            check(U_SUCCESS(e));
            out.resize(size_t(n));
            same_text(text(txt::to_title(s, where)), out, "title");
        }
    }

    // A label of the name, as the library maps it, with a hyphen in its
    // first four code points and one past the BMP in front of it: there
    // the UTF-16 positions are not the code points' (see idna below)
    bool hyphens_past_the_bmp(const std::string& in) {
        auto mapped = txt::idna::to_unicode(string(in), txt::idna::options::whatwg());
        std::string name = mapped ? std::string(text(*mapped)) : in;
        std::u32string label;
        auto look = [&] {
            for (size_t i = 1; i < 4 && i < label.size(); ++i) {
                if (label[i] == U'-') {
                    for (size_t j = 0; j < i; ++j) {
                        if (label[j] > 0xFFFF) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        for (size_t i = 0; i < name.size();) {
            auto [c, n] = utf8::decode(std::string_view(name), i);
            i += n;
            if (c == U'.') {
                if (look()) {
                    return true;
                }
                label.clear();
            } else {
                label += c;
            }
        }
        return look();
    }

    void idna(uint8_t mode, const std::string& in) {
        bool transitional = mode & 1;
        bool std3 = (mode >> 1) & 1;
        uint32_t flags = UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ;
        if (!transitional) {
            flags |= UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE;
        }
        if (std3) {
            flags |= UIDNA_USE_STD3_RULES;
        }
        UErrorCode e = U_ZERO_ERROR;
        UIDNA* uts46 = uidna_openUTS46(flags, &e);
        check(U_SUCCESS(e));
        txt::idna::options o;
        o.transitional = transitional;
        o.use_std3_ascii_rules = std3;
        string s(in);
        for (int ascii = 0; ascii < 2; ++ascii) {
            UIDNAInfo info = UIDNA_INFO_INITIALIZER;
            std::string out(in.size() * 4 + 64, '\0');
            e = U_ZERO_ERROR;
            int32_t n = ascii ? uidna_nameToASCII_UTF8(uts46, in.data(), int32_t(in.size()), out.data(), int32_t(out.size()), &info, &e)
                              : uidna_nameToUnicodeUTF8(uts46, in.data(), int32_t(in.size()), out.data(), int32_t(out.size()), &info, &e);
            if (e == U_BUFFER_OVERFLOW_ERROR) {
                out.assign(size_t(n) + 1, '\0');
                e = U_ZERO_ERROR;
                info = UIDNA_INFO_INITIALIZER;
                n = ascii ? uidna_nameToASCII_UTF8(uts46, in.data(), int32_t(in.size()), out.data(), int32_t(out.size()), &info, &e)
                          : uidna_nameToUnicodeUTF8(uts46, in.data(), int32_t(in.size()), out.data(), int32_t(out.size()), &info, &e);
            }
            if (e == U_INPUT_TOO_LONG_ERROR) {
                break;   // ICU's own bound on what it takes (the library has none but DNS's in to_ascii)
            }
            check(U_SUCCESS(e));
            out.resize(size_t(n));
            if (!ascii && !unicode16(to16(out))) {
                break;   // punycode that decodes to a code point of Unicode 17
            }
            auto ours = ascii ? txt::idna::to_ascii(s, o) : txt::idna::to_unicode(s, o);
            // ICU checks the DNS lengths in to_unicode too, where UTS #46
            // (and the library) check them in to_ascii alone; an empty
            // label both refuse in both (X4_2)
            uint32_t errors = info.errors;
            if (!ascii) {
                errors &= ~uint32_t(UIDNA_ERROR_LABEL_TOO_LONG | UIDNA_ERROR_DOMAIN_NAME_TOO_LONG);
            }
            // ICU takes the root's empty label after a final dot in
            // to_ascii ("a.b."), which IdnaTestV2 refuses (A4_2) and the
            // library with it: a known difference of ICU's
            if (ascii && !ours && errors == 0 && ours.error().rule == txt::idna::error::empty_label && !out.empty() && out.back() == '.') {
                continue;
            }
            // transitional processing deletes a joiner, and a label of one
            // ("0.\u200D") is empty after it: ICU refuses it in to_unicode
            // as an empty label, the library takes it as the root's after
            // the final dot (transitional processing is deprecated and
            // IdnaTestV2 no longer tests it)
            if (!ascii && transitional && ours && errors == UIDNA_ERROR_EMPTY_LABEL && !out.empty() && out.back() == '.') {
                continue;
            }
            // ICU counts the third and fourth position of criterion 2
            // (UTS #46 4.1, the hyphens) in UTF-16 units, the standard and
            // the library in code points: a label with a code point past
            // the BMP in front of its hyphens parts them either way
            // (tests/txt/idna.cpp, TheThirdAndFourthPositionsAreCodePoints),
            // a difference of ICU's
            if (hyphens_past_the_bmp(in)
                && ((!ours && ours.error().rule == txt::idna::error::hyphen && !(errors & UIDNA_ERROR_HYPHEN_3_4))
                    || (ours && errors == UIDNA_ERROR_HYPHEN_3_4))) {
                continue;
            }
            check(ours.has_value() == (errors == 0));
            if (ours) {
                same_text(text(*ours), out, ascii ? "to_ascii" : "to_unicode");
            }
        }
        uidna_close(uts46);
    }

    UCollator* icu_collator(const char* tag) {
        UErrorCode e = U_ZERO_ERROR;
        UCollator* c = ucol_open(tag, &e);
        check(U_SUCCESS(e));
        return c;
    }

    int sign(int x) {
        return (x > 0) - (x < 0);
    }

    void collation(uint8_t mode, uint8_t pick, const std::string& a, const std::string& b) {
        // the languages whose rules reorder the scripts ([reorder], read
        // and not honoured by the library: collate.md) are not among them
        static const char* tags[] = {"", "de", "sv", "da", "tr", "pl", "cs", "es", "fi", "ro", "lt", "vi", "sq", "lv", "ln", "sk", "hu", "is", "mt", "tk", "cy", "fil", "et", "se"};
        const char* tag = tags[pick % (sizeof(tags) / sizeof(tags[0]))];
        txt::options how;
        how.strength = txt::strength(mode & 3);
        UCollator* icu = icu_collator(tag);
        UErrorCode e = U_ZERO_ERROR;
        static const UColAttributeValue strengths[4] = {UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_QUATERNARY};
        // the DUCET's variable elements, which the library shifts (asked,
        // or by a language's rules, Thai's): the spaces, the punctuation
        // and the symbols, where CLDR's root stops at the punctuation
        ucol_setMaxVariable(icu, UCOL_REORDER_CODE_SYMBOL, &e);
        ucol_setAttribute(icu, UCOL_STRENGTH, strengths[mode & 3], &e);
        if ((mode >> 2) & 1) {
            how.punctuation = txt::punctuation::shifted;
            ucol_setAttribute(icu, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &e);
        }
        if ((mode >> 3) & 1) {
            how.numeric = true;
            ucol_setAttribute(icu, UCOL_NUMERIC_COLLATION, UCOL_ON, &e);
        }
        if ((mode >> 4) & 1) {
            how.case_order = txt::case_order::upper_first;
            ucol_setAttribute(icu, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &e);
        }
        if ((mode >> 5) & 1) {
            how.case_level = true;
            ucol_setAttribute(icu, UCOL_CASE_LEVEL, UCOL_ON, &e);
        }
        if ((mode >> 6) & 1) {
            how.backwards = true;
            ucol_setAttribute(icu, UCOL_FRENCH_COLLATION, UCOL_ON, &e);
        }
        check(U_SUCCESS(e));
        // the half-width voiced marks weigh only at the second level, and
        // ICU gives them the case of a normal kana there, where the library
        // gives an element of no first weight none (collate.md): a known
        // difference, under the case settings alone
        if ((how.case_level || how.case_order) && (a.find("\xEF\xBE\x9E") != std::string::npos || a.find("\xEF\xBE\x9F") != std::string::npos
                                                   || b.find("\xEF\xBE\x9E") != std::string::npos || b.find("\xEF\xBE\x9F") != std::string::npos)) {
            ucol_close(icu);
            return;
        }
        // marks out of their canonical order: the library puts them in
        // order before it weighs them, ICU with its normalization off (the
        // default of the root and of these languages) reads them as they
        // stand — a contraction reached past a mark (UCA S2.1) and the
        // accents themselves part there (root, "s U+0341 U+0327" against
        // "S U+0327" at the second level: ICU -1, and 1 with its
        // normalization on, as the library): such texts left out
        if (not_fcd(a) || not_fcd(b)) {
            ucol_close(icu);
            return;
        }
        // numeric, a number weighs from the first weight of its digit zero
        // on in the library and below it in ICU, which parts only against
        // the digits that are no decimal digits (₀, ¹, ①: Numeric_Type
        // Digit), whose first weight is a digit's: those left out
        if (how.numeric && (other_digits(a) || other_digits(b))) {
            ucol_close(icu);
            return;
        }
        // shifted, the variable elements are the DUCET's for the library
        // and CLDR's groups up to the symbols for ICU, which part on the
        // modifier letters (ー and ｰ are symbols to CLDR, letters to the
        // DUCET), and ICU's answers around a control are no order (see
        // modifier_letters): those left out
        if (how.punctuation && (modifier_letters(a) || modifier_letters(b))) {
            ucol_close(icu);
            return;
        }
        txt::collator ours(tag[0] ? txt::locale(string(tag)) : txt::locale(), how);
        string sa(a), sb(b);
        int mine = sign(ours.compare(sa, sb));
        int theirs = sign(ucol_strcollUTF8(icu, a.data(), int32_t(a.size()), b.data(), int32_t(b.size()), &e));
        check(U_SUCCESS(e));
        ucol_close(icu);
        if (mine != theirs && std::getenv("SGCL_FUZZ_EXPLAIN")) {
            std::fprintf(stderr, "compare differs: locale '%s' options %02X: ours %d, icu %d\n", tag, mode, mine, theirs);
            same_text(a, b, "the texts (ours: a, icu: b)");
        }
        check(mine == theirs);
        check(sign(ours.compare(sb, sa)) == -mine);
        auto ka = ours.key(sa);
        auto kb = ours.key(sb);
        size_t n = std::min(ka.size(), kb.size());
        int bytes = sign(std::memcmp(ka.data(), kb.data(), n));
        if (bytes == 0) {
            bytes = sign(int(ka.size()) - int(kb.size()));
        }
        check(bytes == mine);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2 || size > 2048) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode & 3) {
        case 0:
        case 1:
        case 2: {
            auto in16 = to16(rest);
            if (!unicode16(in16)) {
                return 0;
            }
            std::string in = to8(in16);
            if ((mode & 3) == 0) {
                normalization(in16, in);
            } else if ((mode & 3) == 1) {
                casing(mode >> 2, in16, in);
            } else {
                idna(mode >> 2, in);
            }
            break;
        }
        case 3: {
            if (rest.size() < 2) {
                return 0;
            }
            uint8_t options = uint8_t(rest[0]);
            uint8_t pick = uint8_t(mode >> 2);
            rest.remove_prefix(1);
            size_t nul = rest.find('\0');
            auto a16 = to16(rest.substr(0, nul));
            auto b16 = to16(nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1));
            if (!unicode16(a16) || !unicode16(b16)) {
                return 0;
            }
            // Han: UCA 17 (ICU 78) weighs the ideographs by radical and
            // stroke where UCA 16 (the library's DUCET) weighs them by
            // block and code point, U+6914 before U+2346D there and after
            // it here — a difference of the versions, not of the code
            if (han(a16) && han(b16)) {
                return 0;
            }
            // the marks outside the common diacritics (U+0300 to U+036F):
            // CLDR's root weighs some of them at the second level in an
            // order of its own (U+05B3 before U+0334, the DUCET the other
            // way round), a difference of the two roots, not of the code
            if (odd_marks(a16) || odd_marks(b16)) {
                return 0;
            }
            // U+FFFE and U+FFFF: CLDR's root gives them the lowest and the
            // highest first weight, the DUCET (the library's root) an
            // implicit one as any noncharacter
            if (a16.find_first_of(u"\uFFFE\uFFFF") != std::u16string::npos || b16.find_first_of(u"\uFFFE\uFFFF") != std::u16string::npos) {
                return 0;
            }
            collation(options, pick, to8(a16), to8(b16));
            break;
        }
    }
    return 0;
}
