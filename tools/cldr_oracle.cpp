// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of the locale data and its formatting (sgcl/txt/locale.h,
// plural.h, number.h, currency.h, list_format.h, relative_time.h and
// sgcl/time/localized.h): ICU, asked the same questions. Only in this tool,
// never in the library. ICU 78 carries CLDR 48 where the library carries
// CLDR 46, so an answer that differs is a difference of the data or of the
// code, and tools/cldr_tables.py --icu, which writes the tests' vectors from
// it, keeps the library's answer where CLDR 46 says otherwise and names the
// case. From the root of the tree, with Homebrew's icu4c:
//
//	clang++ -std=c++20 -O1 -I/opt/homebrew/opt/icu4c/include tools/cldr_oracle.cpp \
//	    -L/opt/homebrew/opt/icu4c/lib -licuuc -licui18n -o /tmp/cldr_oracle
//
// It reads questions from stdin, one a line, fields separated by tabs, and
// writes one answer a line (ERROR <code> when ICU refuses):
//
//	num      locale  skeleton  decimal          unumf, a number skeleton ("currency/PLN compact-short")
//	plural   locale  card|ord  decimal          uplrules_select
//	date     locale  pattern   epoch-ms  zone   udat with the pattern, UDAT_PATTERN
//	style    locale  d t       epoch-ms  zone   udat with the styles (0 full, 1 long, 2 medium, 3 short, -1 none)
//	skeleton locale  skeleton                   udatpg_getBestPattern
//	interval locale  skeleton  from-ms   to-ms  zone   udtitvfmt
//	list     locale  type      width     item...       ulistfmt (type 0 and, 1 or, 2 units; width 0 wide, 1 short, 2 narrow)
//	relative locale  style     numeric   unit  value   ureldatefmt (style 0 long, 1 short, 2 narrow; numeric 1/0)
//	maximize tag  /  minimize tag               uloc_addLikelySubtags, uloc_minimizeSubtags
//	match    desired  supported,supported...    icu::LocaleMatcher, its default settings
//	dn-lang  locale  tag                        uloc_getDisplayName
//	dn-region locale code  /  dn-script locale code    uloc_getDisplayCountry, uloc_getDisplayScript
//	dn-cur   locale  code  form                 ucurr_getName (form -1) or ucurr_getPluralName (a category)
#include <unicode/localematcher.h>
#include <unicode/locid.h>
#include <unicode/udat.h>
#include <unicode/udateintervalformat.h>
#include <unicode/udatpg.h>
#include <unicode/uloc.h>
#include <unicode/ulistformatter.h>
#include <unicode/unumberformatter.h>
#include <unicode/upluralrules.h>
#include <unicode/ureldatefmt.h>
#include <unicode/ucurr.h>
#include <unicode/ustring.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    std::u16string u16(const std::string& s) {
        std::u16string out(s.size() * 2 + 4, u'\0');
        int32_t n = 0;
        UErrorCode e = U_ZERO_ERROR;
        u_strFromUTF8(reinterpret_cast<UChar*>(out.data()), int32_t(out.size()), &n, s.data(), int32_t(s.size()), &e);
        out.resize(size_t(n));
        return out;
    }

    std::string u8(const UChar* s, int32_t n) {
        std::string out(size_t(n) * 4 + 4, '\0');
        int32_t m = 0;
        UErrorCode e = U_ZERO_ERROR;
        u_strToUTF8(out.data(), int32_t(out.size()), &m, s, n, &e);
        out.resize(size_t(m));
        return out;
    }

    std::vector<std::string> fields(const std::string& line) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : line) {
            if (c == '\t') {
                out.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        out.push_back(cur);
        return out;
    }

    std::string error(UErrorCode e) {
        return std::string("ERROR ") + u_errorName(e);
    }

    std::string num(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        auto skel = u16(f[2]);
        UNumberFormatter* nf = unumf_openForSkeletonAndLocale(reinterpret_cast<const UChar*>(skel.data()),
                                                              int32_t(skel.size()), f[1].c_str(), &e);
        UFormattedNumber* r = unumf_openResult(&e);
        unumf_formatDecimal(nf, f[3].data(), int32_t(f[3].size()), r, &e);
        UChar buf[512];
        int32_t n = unumf_resultToString(r, buf, 512, &e);
        std::string out = U_FAILURE(e) ? error(e) : u8(buf, n);
        unumf_closeResult(r);
        unumf_close(nf);
        return out;
    }

    std::string plural(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        UPluralRules* p = uplrules_openForType(f[1].c_str(), f[2] == "ord" ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL, &e);
        UChar buf[32];
        int32_t n = uplrules_select(p, std::strtod(f[3].c_str(), nullptr), buf, 32, &e);
        uplrules_close(p);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    // the locale with the Gregorian calendar, which is sgcl::time's (fa
    // and th default to others)
    std::string gregorian(const std::string& tag) {
        return tag + (tag.find('@') == std::string::npos ? "@calendar=gregorian" : ";calendar=gregorian");
    }

    std::string date(std::vector<std::string> f, bool styles) {
        UErrorCode e = U_ZERO_ERROR;
        f[1] = gregorian(f[1]);
        auto zone = u16(f.size() > 5 ? f[5] : f[4]);
        UDateFormat* df;
        double ms;
        if (styles) {
            df = udat_open(UDateFormatStyle(std::atoi(f[3].c_str())), UDateFormatStyle(std::atoi(f[2].c_str())),
                           f[1].c_str(), reinterpret_cast<const UChar*>(zone.data()), int32_t(zone.size()), nullptr, 0, &e);
            ms = std::strtod(f[4].c_str(), nullptr);
            zone = u16(f[5]);
        } else {
            auto pattern = u16(f[2]);
            df = udat_open(UDAT_PATTERN, UDAT_PATTERN, f[1].c_str(), reinterpret_cast<const UChar*>(zone.data()),
                           int32_t(zone.size()), reinterpret_cast<const UChar*>(pattern.data()), int32_t(pattern.size()), &e);
            ms = std::strtod(f[3].c_str(), nullptr);
        }
        if (U_FAILURE(e)) {
            return error(e);
        }
        // the proleptic Gregorian calendar, as sgcl::time has it
        UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(df));
        ucal_setGregorianChange(cal, -1e300, &e);
        UChar buf[512];
        int32_t n = udat_format(df, ms, buf, 512, nullptr, &e);
        udat_close(df);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string skeleton(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        UDateTimePatternGenerator* g = udatpg_open(gregorian(f[1]).c_str(), &e);
        auto skel = u16(f[2]);
        UChar buf[256];
        int32_t n = udatpg_getBestPattern(g, reinterpret_cast<const UChar*>(skel.data()), int32_t(skel.size()), buf, 256, &e);
        udatpg_close(g);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string interval(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        auto skel = u16(f[2]);
        auto zone = u16(f[5]);
        UDateIntervalFormat* df = udtitvfmt_open(gregorian(f[1]).c_str(), reinterpret_cast<const UChar*>(skel.data()),
                                                 int32_t(skel.size()), reinterpret_cast<const UChar*>(zone.data()),
                                                 int32_t(zone.size()), &e);
        if (U_FAILURE(e)) {
            return error(e);
        }
        UChar buf[512];
        int32_t n = udtitvfmt_format(df, std::strtod(f[3].c_str(), nullptr), std::strtod(f[4].c_str(), nullptr), buf,
                                     512, nullptr, &e);
        udtitvfmt_close(df);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string list(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        UListFormatter* lf = ulistfmt_openForType(f[1].c_str(), UListFormatterType(std::atoi(f[2].c_str())),
                                                  UListFormatterWidth(std::atoi(f[3].c_str())), &e);
        std::vector<std::u16string> items;
        for (size_t i = 4; i < f.size(); ++i) {
            items.push_back(u16(f[i]));
        }
        std::vector<const UChar*> ptrs;
        std::vector<int32_t> lens;
        for (auto& s : items) {
            ptrs.push_back(reinterpret_cast<const UChar*>(s.data()));
            lens.push_back(int32_t(s.size()));
        }
        UChar buf[1024];
        int32_t n = ulistfmt_format(lf, ptrs.data(), lens.data(), int32_t(items.size()), buf, 1024, &e);
        ulistfmt_close(lf);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string relative(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        static const URelativeDateTimeUnit Units[] = {UDAT_REL_UNIT_SECOND, UDAT_REL_UNIT_MINUTE, UDAT_REL_UNIT_HOUR,
                                                      UDAT_REL_UNIT_DAY, UDAT_REL_UNIT_WEEK, UDAT_REL_UNIT_MONTH,
                                                      UDAT_REL_UNIT_QUARTER, UDAT_REL_UNIT_YEAR};
        URelativeDateTimeFormatter* r = ureldatefmt_open(f[1].c_str(), nullptr,
                                                         UDateRelativeDateTimeFormatterStyle(std::atoi(f[2].c_str())),
                                                         UDISPCTX_CAPITALIZATION_NONE, &e);
        UChar buf[256];
        double v = std::strtod(f[5].c_str(), nullptr);
        URelativeDateTimeUnit unit = Units[std::atoi(f[4].c_str())];
        int32_t n = f[3] == "1" ? ureldatefmt_formatNumeric(r, v, unit, buf, 256, &e)
                                : ureldatefmt_format(r, v, unit, buf, 256, &e);
        ureldatefmt_close(r);
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string likely(const std::vector<std::string>& f, bool max) {
        UErrorCode e = U_ZERO_ERROR;
        char id[128], out[128], tag[128];
        uloc_forLanguageTag(f[1].c_str(), id, 128, nullptr, &e);
        if (max) {
            uloc_addLikelySubtags(id, out, 128, &e);
        } else {
            uloc_minimizeSubtags(id, out, 128, &e);
        }
        uloc_toLanguageTag(out, tag, 128, false, &e);
        return U_FAILURE(e) ? error(e) : std::string(tag);
    }

    std::string display(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        UChar buf[512];
        int32_t n = 0;
        char id[128];
        if (f[0] == "dn-lang") {
            uloc_forLanguageTag(f[2].c_str(), id, 128, nullptr, &e);
            n = uloc_getDisplayName(id, f[1].c_str(), buf, 512, &e);
        } else if (f[0] == "dn-region") {
            n = uloc_getDisplayCountry(("und_" + f[2]).c_str(), f[1].c_str(), buf, 512, &e);
        } else if (f[0] == "dn-script") {
            n = uloc_getDisplayScript(("und_" + f[2]).c_str(), f[1].c_str(), buf, 512, &e);
        } else {
            auto code = u16(f[2]);
            UBool choice = false;
            const UChar* r;
            if (f[3] == "-1") {
                r = ucurr_getName(reinterpret_cast<const UChar*>(code.c_str()), f[1].c_str(), UCURR_LONG_NAME, &choice, &n, &e);
            } else {
                r = ucurr_getPluralName(reinterpret_cast<const UChar*>(code.c_str()), f[1].c_str(), &choice, f[3].c_str(), &n, &e);
            }
            if (U_FAILURE(e)) {
                return error(e);
            }
            return u8(r, n);
        }
        return U_FAILURE(e) ? error(e) : u8(buf, n);
    }

    std::string match(const std::vector<std::string>& f) {
        UErrorCode e = U_ZERO_ERROR;
        icu::LocaleMatcher::Builder b;
        std::stringstream ss(f[2]);
        std::string tag;
        std::vector<icu::Locale> supported;
        while (std::getline(ss, tag, ',')) {
            supported.push_back(icu::Locale::forLanguageTag(tag, e));
        }
        for (auto& l : supported) {
            b.addSupportedLocale(l);
        }
        b.setDefaultLocale(&supported[0]);
        icu::LocaleMatcher m = b.build(e);
        std::vector<icu::Locale> desired;
        std::stringstream ds(f[1]);
        while (std::getline(ds, tag, ',')) {
            desired.push_back(icu::Locale::forLanguageTag(tag, e));
        }
        icu::Locale::RangeIterator<std::vector<icu::Locale>::iterator> it(desired.begin(), desired.end());
        const icu::Locale* best = m.getBestMatch(it, e);
        if (U_FAILURE(e) || !best) {
            return error(e);
        }
        return best->toLanguageTag<std::string>(e);
    }
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = fields(line);
        std::string out;
        if (f[0] == "num") {
            out = num(f);
        } else if (f[0] == "plural") {
            out = plural(f);
        } else if (f[0] == "date") {
            out = date(f, false);
        } else if (f[0] == "style") {
            out = date(f, true);
        } else if (f[0] == "skeleton") {
            out = skeleton(f);
        } else if (f[0] == "interval") {
            out = interval(f);
        } else if (f[0] == "list") {
            out = list(f);
        } else if (f[0] == "relative") {
            out = relative(f);
        } else if (f[0] == "maximize" || f[0] == "minimize") {
            out = likely(f, f[0] == "maximize");
        } else if (f[0].rfind("dn-", 0) == 0) {
            out = display(f);
        } else if (f[0] == "match") {
            out = match(f);
        } else {
            out = "ERROR question";
        }
        std::printf("%s\n", out.c_str());
        std::fflush(stdout);
    }
}
