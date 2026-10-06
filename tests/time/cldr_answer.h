//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The time module's answer to a question of tools/cldr_oracle.cpp: date,
// style, skeleton, interval (tests/txt/cldr_answer.h has the rest)
#pragma once

#include "sgcl/time.h"
#include "tests/txt/cldr_answer.h"

namespace cldr_answer {
    inline time::zone zone_of(const std::string& name) {
        auto z = time::zone::load(string(name.c_str()));
        return z ? *z : time::zone::utc();
    }

    inline time::style style_of(const std::string& s) {
        return s == "-1" ? time::style::none : time::style(4 - std::stoi(s));
    }

    inline std::string date_answer(const std::vector<std::string>& f) {
        const std::string& kind = f[0];
        if (kind == "date") {
            auto t = time::datetime::from_unix_milli(std::stoll(f[3]), zone_of(f[4]));
            return time::date_format::from_pattern(locale_of(f[1]), string(f[2].c_str())).format(t).c_str();
        }
        if (kind == "style") {
            auto t = time::datetime::from_unix_milli(std::stoll(f[4]), zone_of(f[5]));
            return t.format(locale_of(f[1]), style_of(f[2]), style_of(f[3])).c_str();
        }
        if (kind == "skeleton") {
            return time::date_format::from_skeleton(locale_of(f[1]), string(f[2].c_str())).pattern().c_str();
        }
        if (kind == "interval") {
            time::zone z = zone_of(f[5]);
            auto a = time::datetime::from_unix_milli(std::stoll(f[3]), z);
            auto b = time::datetime::from_unix_milli(std::stoll(f[4]), z);
            return a.format_interval(b, locale_of(f[1]), string(f[2].c_str())).c_str();
        }
        return "?";
    }
}
