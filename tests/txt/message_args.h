//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The arguments of a message vector as tools/message_oracle.cpp reads them:
// name=type:value separated by ';' (i an integer, d a double, s a text, m a
// date in milliseconds), with \\ \t \n \; \= escaped. Shared by the tests
// and tools/message_driver.cpp.
#pragma once

#include "sgcl/txt.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace message_args {
    inline std::string unescape(const std::string& s) {
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

    inline std::vector<std::string> split(const std::string& s, char sep) {
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

    inline sgcl::txt::value parse(const std::string& spec) {
        sgcl::txt::object o;
        if (spec.empty()) {
            return o;
        }
        for (auto& a : split(spec, ';')) {
            auto kv = split(a, '=');
            std::string name = unescape(kv[0]), v = kv.size() > 1 ? kv[1] : "";
            char type = v.empty() ? 's' : v[0];
            std::string value = unescape(v.size() > 2 ? v.substr(2) : "");
            sgcl::string key{std::string_view(name)};
            if (type == 'i') {
                o.set(key, (long long)std::strtoll(value.c_str(), nullptr, 10));
            } else if (type == 'd' || type == 'm') {
                o.set(key, std::strtod(value.c_str(), nullptr));
            } else {
                o.set(key, sgcl::string(std::string_view(value)));
            }
        }
        return o;
    }
}
