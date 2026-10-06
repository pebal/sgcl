// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of txt::message_format (sgcl/txt/message_format.h): ICU 78's
// icu::MessageFormat, asked the same questions. Only in this tool, never in
// the library; tools/message_vectors.py writes the tests' vectors from it.
// From the root of the tree, with Homebrew's icu4c:
//
//	clang++ -std=c++20 -O1 -I/opt/homebrew/opt/icu4c/include tools/message_oracle.cpp \
//	    -L/opt/homebrew/opt/icu4c/lib -licuuc -licui18n -o /tmp/message_oracle
//
// It reads questions from stdin, one a line: locale TAB pattern TAB
// arguments, the arguments name=type:value separated by ';' (types i an
// integer, d a double, s a text, m a date in milliseconds since 1970), every
// field with \\ \t \n \; \= escaped; the default zone is UTC. It writes one
// answer a line, \\ \t \n escaped, or ERROR <code> when ICU refuses.
#include <unicode/fmtable.h>
#include <unicode/msgfmt.h>
#include <unicode/timezone.h>
#include <unicode/unistr.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
    std::string unescape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                out.push_back(c == 't' ? '\t' : c == 'n' ? '\n' : c);
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }

    // split at an unescaped separator
    std::vector<std::string> split(const std::string& s, char sep) {
        std::vector<std::string> out(1);
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                out.back() += s.substr(i, 2);
                ++i;
            } else if (s[i] == sep) {
                out.emplace_back();
            } else {
                out.back().push_back(s[i]);
            }
        }
        return out;
    }

    void print(const std::string& s) {
        for (char c : s) {
            if (c == '\\') std::fputs("\\\\", stdout);
            else if (c == '\t') std::fputs("\\t", stdout);
            else if (c == '\n') std::fputs("\\n", stdout);
            else std::putchar(c);
        }
        std::putchar('\n');
    }
}

int main() {
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("UTC"));
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = split(line, '\t');
        while (f.size() < 3) f.emplace_back();
        std::string locale = unescape(f[0]), pattern = unescape(f[1]);
        std::vector<icu::UnicodeString> names;
        std::vector<icu::Formattable> args;
        if (!f[2].empty()) {
            for (auto& a : split(f[2], ';')) {
                auto kv = split(a, '=');
                std::string name = unescape(kv[0]), v = kv.size() > 1 ? kv[1] : "";
                char type = v.empty() ? 's' : v[0];
                std::string value = unescape(v.size() > 2 ? v.substr(2) : "");
                names.push_back(icu::UnicodeString::fromUTF8(name));
                if (type == 'i') args.emplace_back(icu::Formattable(int64_t(std::strtoll(value.c_str(), 0, 10))));
                else if (type == 'd') args.emplace_back(icu::Formattable(std::strtod(value.c_str(), 0)));
                else if (type == 'm') args.emplace_back(icu::Formattable(std::strtod(value.c_str(), 0), icu::Formattable::kIsDate));
                else args.emplace_back(icu::Formattable(icu::UnicodeString::fromUTF8(value)));
            }
        }
        UErrorCode e = U_ZERO_ERROR;
        UParseError pe;
        icu::MessageFormat mf(icu::UnicodeString::fromUTF8(pattern), icu::Locale(locale.c_str()), pe, e);
        if (U_FAILURE(e)) {
            std::printf("ERROR %s\n", u_errorName(e));
            continue;
        }
        icu::UnicodeString out;
        mf.format(names.data(), args.data(), int32_t(args.size()), out, e);
        if (U_FAILURE(e)) {
            std::printf("ERROR %s\n", u_errorName(e));
            continue;
        }
        std::string s;
        out.toUTF8String(s);
        print(s);
    }
    return 0;
}
