// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The library's side of tools/cldr_oracle.cpp: the same questions on stdin,
// one answer a line, from sgcl::txt and sgcl::time (tests/txt/cldr_answer.h
// and tests/time/cldr_answer.h, which the tests' vectors are checked with).
// tools/cldr_vectors.py runs both programs and writes the cases they agree
// on. From the root of the tree:
//
//	clang++ -std=c++20 -O1 -I. tools/cldr_driver.cpp -o /tmp/cldr_driver
//	clang++ -std=c++20 -O1 -I. -DSGCL_CLDR_NAMES tools/cldr_driver.cpp -o /tmp/cldr_names_driver
#include "tests/time/cldr_answer.h"
#include "tests/txt/cldr_answer.h"
#ifdef SGCL_CLDR_NAMES
#include "tests/txt/cldr_names_set.h"   // the display names of the tests: a driver of its own (-DSGCL_CLDR_NAMES)
#endif

#include <cstdio>
#include <iostream>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = cldr_answer::fields(line);
        std::string a = cldr_answer::text_answer(f);
        if (a == "?") {
            a = cldr_answer::date_answer(f);
        }
        std::printf("%s\n", a.c_str());
        std::fflush(stdout);
    }
}
