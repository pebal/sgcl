//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The library's side of tools/distance_vectors.py: a line of two texts in hex
// and a flag (b: bytes, else code points) answered with levenshtein, the
// optimal string alignment, Damerau's distance, the longest common
// subsequence, Jaro and Jaro-Winkler, space-separated.
//
//   clang++ -std=c++20 -O1 -I. tools/distance_driver.cpp -o distance_driver
#include "sgcl/txt/distance.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

using namespace sgcl;

namespace {
    std::string unhex(const std::string& h) {
        std::string out;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            out.push_back(char(std::stoi(h.substr(i, 2), nullptr, 16)));
        }
        return out;
    }
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string ha, hb, flag;
        in >> ha >> hb >> flag;
        string a{std::string_view(unhex(ha == "-" ? "" : ha))}, b{std::string_view(unhex(hb == "-" ? "" : hb))};
        bool bytes = flag == "b";
        txt::distance_options o{.bytes = bytes};
        txt::jaro_options j{.bytes = bytes};
        std::printf("%zu %zu %zu %zu %.12f %.12f\n", txt::levenshtein(a, b, o), txt::osa_distance(a, b, o),
                    txt::damerau_levenshtein(a, b, o), txt::lcs_length(a, b, o), txt::jaro(a, b, j),
                    txt::jaro_winkler(a, b, j));
    }
}
