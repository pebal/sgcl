//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The data of an html_stencil vector as tools/html_stencil_oracle.go reads
// it: name=type:value separated by ';' (i, d, s, b a truth, n nothing), with
// \\ \t \n \; \= \0 escaped, and always "list" (["a", "<b>", "c\"d"]) and "obj"
// ({"k": "v<"}). Shared by the tests and tools/html_stencil_driver.cpp.
#pragma once

#include "sgcl/txt.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace html_stencil_data {
    inline std::string unescape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                out.push_back(c == 't' ? '\t' : c == 'n' ? '\n' : c == '0' ? '\0' : c);
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
        using namespace sgcl;
        txt::object o;
        o.set(string("list"), txt::list{"a", "<b>", "c\"d"});
        o.set(string("obj"), txt::object{{"k", "v<"}});
        if (spec.empty()) {
            return o;
        }
        for (auto& a : split(spec, ';')) {
            auto kv = split(a, '=');
            std::string name = unescape(kv[0]), v = kv.size() > 1 ? kv[1] : "";
            char type = v.empty() ? 's' : v[0];
            std::string text = unescape(v.size() > 2 ? v.substr(2) : "");
            string key{std::string_view(name)};
            switch (type) {
                case 'i': o.set(key, (long long)std::strtoll(text.c_str(), nullptr, 10)); break;
                case 'd': o.set(key, std::strtod(text.c_str(), nullptr)); break;
                case 'b': o.set(key, text == "1"); break;
                case 'n': o.set(key, txt::value()); break;
                default: o.set(key, string(std::string_view(text))); break;
            }
        }
        return o;
    }
}
