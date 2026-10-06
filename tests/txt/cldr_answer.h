//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The library's answer to a question of tools/cldr_oracle.cpp (fields
// separated by tabs), shared by the tests' vectors and tools/cldr_driver.cpp,
// which writes them. Numbers take the fields of txt::number_options:
//   num  locale  style  currency  display  sign  mode  min-frac  max-frac  min-sig  max-sig  grouping  decimal
#pragma once

#include "sgcl/txt.h"

#include <string>
#include <vector>

namespace cldr_answer {
    using namespace sgcl;

    inline std::vector<std::string> fields(const std::string& line) {
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

    inline txt::locale locale_of(const std::string& tag) {
        return txt::locale(string(tag.c_str()));
    }

    // The answer of the txt module, or "?" for a question it does not take
    inline std::string text_answer(const std::vector<std::string>& f) {
        const std::string& kind = f[0];
        if (kind == "num") {
            txt::number_options o;
            o.style = txt::number_style(std::stoi(f[2]));
            if (!f[3].empty()) {
                o.currency = txt::currency(string(f[3].c_str()));
            }
            o.display = txt::currency_display(std::stoi(f[4]));
            o.sign = txt::sign_display(std::stoi(f[5]));
            o.mode = sgcl::rounding(std::stoi(f[6]));
            o.min_fraction = std::stoi(f[7]);
            o.max_fraction = std::stoi(f[8]);
            o.min_significant = std::stoi(f[9]);
            o.max_significant = std::stoi(f[10]);
            o.grouping = f[11] == "1";
            auto r = txt::number_format(locale_of(f[1]), o).format(string(f[12].c_str()));
            return r ? std::string(r->c_str()) : "NULLOPT";
        }
        if (kind == "plural") {
            txt::locale l = locale_of(f[1]);
            txt::plural p = f[2] == "ord" ? txt::ordinal_of(std::stoll(f[3]), l) : txt::plural_of(string(f[3].c_str()), l);
            static const char* Names[] = {"zero", "one", "two", "few", "many", "other"};
            return Names[int(p)];
        }
        if (kind == "list") {
            vector<string> items;
            for (size_t i = 4; i < f.size(); ++i) {
                items.push_back(string(f[i].c_str()));
            }
            return txt::format_list(items, locale_of(f[1]), txt::list_type(std::stoi(f[2])), txt::width(std::stoi(f[3])))
                .c_str();
        }
        if (kind == "relative") {
            txt::relative_options o;
            o.width = txt::width(std::stoi(f[2]));
            o.numeric = f[3] == "1";
            return txt::format_relative(std::stod(f[5]), txt::time_unit(std::stoi(f[4])), locale_of(f[1]), o).c_str();
        }
        if (kind == "maximize") {
            return locale_of(f[1]).maximize().to_string().c_str();
        }
        if (kind == "minimize") {
            return locale_of(f[1]).minimize().to_string().c_str();
        }
        if (kind == "match") {
            vector<txt::locale> supported;
            std::string cur;
            for (char c : f[2] + ",") {
                if (c == ',') {
                    supported.push_back(locale_of(cur));
                    cur.clear();
                } else {
                    cur += c;
                }
            }
            vector<txt::locale> desired;
            for (char c : f[1] + ",") {
                if (c == ',') {
                    desired.push_back(locale_of(cur));
                    cur.clear();
                } else {
                    cur += c;
                }
            }
            return txt::best_match(desired, supported).to_string().c_str();
        }
        if (kind == "dn-lang") {
            return locale_of(f[2]).display_name(locale_of(f[1])).c_str();
        }
        if (kind == "dn-region") {
            return txt::region(string(f[2].c_str())).display_name(locale_of(f[1])).c_str();
        }
        if (kind == "dn-script") {
            return txt::script_code(string(f[2].c_str())).display_name(locale_of(f[1])).c_str();
        }
        if (kind == "dn-cur") {
            txt::currency c(string(f[2].c_str()));
            if (f[3] == "-1") {
                return c.display_name(locale_of(f[1])).c_str();
            }
            static const char* Names[] = {"zero", "one", "two", "few", "many", "other"};
            for (int k = 0; k < 6; ++k) {
                if (f[3] == Names[k]) {
                    return c.display_name(locale_of(f[1]), txt::plural(k)).c_str();
                }
            }
        }
        return "?";
    }
}
