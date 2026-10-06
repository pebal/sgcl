// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// txt::message_format asked the questions of tools/message_oracle.cpp, the
// answers in the same form, for tools/message_vectors.py to compare. The
// display names it includes are those tests_txt includes (the tests'
// vectors are answered with the names the test binary has):
//
//	clang++ -std=c++20 -O1 -I. tools/message_driver.cpp -o /tmp/message_driver
#include "sgcl/time.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/sr-Cyrl.h"
#include "sgcl/txt/names/zh-Hans.h"
#include "tests/txt/cldr_names_set.h"
#include "tests/txt/message_args.h"

#include <cstdio>
#include <iostream>
#include <string>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = message_args::split(line, '\t');
        while (f.size() < 3) {
            f.emplace_back();
        }
        std::string loc = message_args::unescape(f[0]), pattern = message_args::unescape(f[1]);
        auto r = sgcl::txt::format_message(sgcl::string(std::string_view(pattern)), sgcl::txt::locale(sgcl::string(std::string_view(loc))),
                                           message_args::parse(f[2]));
        if (!r) {
            std::printf("ERROR %zu %s\n", r.error().offset(), r.error().message().c_str());
            continue;
        }
        for (char c : r->view()) {
            if (c == '\\') std::fputs("\\\\", stdout);
            else if (c == '\t') std::fputs("\\t", stdout);
            else if (c == '\n') std::fputs("\\n", stdout);
            else std::putchar(c);
        }
        std::putchar('\n');
    }
    return 0;
}
