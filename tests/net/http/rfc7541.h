//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// RFC 7541 read from its text (~/Programming/oracles/rfc7541/rfc7541.txt, or
// $SGCL_RFC7541): the Huffman code of Appendix B and the examples of
// Appendix C — the header list to encode, the hex dump of the encoding, the
// dynamic table after it and its size — the oracle of tests/net/http/
// hpack.cpp. The page breaks of the text are skipped wherever they fall.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rfc7541 {
    inline std::string path() {
        if (const char* e = std::getenv("SGCL_RFC7541")) {
            return e;
        }
        const char* home = std::getenv("HOME");
        return std::string(home ? home : "") + "/Programming/oracles/rfc7541/rfc7541.txt";
    }

    // The lines of the RFC without the page breaks (the footer, the form
    // feed, the header of the next page)
    inline std::vector<std::string> lines() {
        std::ifstream in(path());
        std::vector<std::string> out;
        std::string l;
        while (std::getline(in, l)) {
            if (!l.empty() && l.back() == '\r') {
                l.pop_back();
            }
            if (l.find("Peon & Ruellan") == 0 || l.find("RFC 7541 ") == 0 || l.find('\f') != std::string::npos) {
                continue;
            }
            out.push_back(l);
        }
        return out;
    }

    inline std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(' ');
        size_t b = s.find_last_not_of(' ');
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    }

    struct Code {
        int symbol;
        uint32_t code;
        int bits;
    };

    // Appendix B: "(sym)  |bits|...  hex  [len]", with the ASCII form before
    // the symbol for the printable ones and EOS for 256
    inline std::vector<Code> huffman(const std::vector<std::string>& ls) {
        std::vector<Code> out;
        bool in = false;
        for (auto& l : ls) {
            if (l.rfind("Appendix B.", 0) == 0) {
                in = true;
                continue;
            }
            if (l.rfind("Appendix C.", 0) == 0) {
                break;
            }
            if (!in) {
                continue;
            }
            // the symbol's number in "( 65)"; the ASCII form before it may be
            // '(' , ')' or '|' itself, so everything is found after "( n)"
            auto open = l.find("(");
            while (open != std::string::npos && !(open + 1 < l.size() && (l[open + 1] == ' ' || std::isdigit((unsigned char)l[open + 1])) && l.find(')', open) != std::string::npos
                                                  && std::isdigit((unsigned char)l[l.find(')', open) - 1]))) {
                open = l.find('(', open + 1);
            }
            if (open == std::string::npos) {
                continue;
            }
            auto close = l.find(')', open);
            auto bar = l.find('|', close);
            auto lb = l.find('[', close);
            if (close == std::string::npos || bar == std::string::npos || lb == std::string::npos) {
                continue;
            }
            Code c;
            c.symbol = std::atoi(l.c_str() + open + 1);
            std::string bits;
            size_t k = bar;
            for (; k < lb && (l[k] == '0' || l[k] == '1' || l[k] == '|'); ++k) {
                if (l[k] != '|') {
                    bits += l[k];
                }
            }
            std::istringstream rest(l.substr(k, lb - k));
            std::string hex;
            rest >> hex;
            c.code = uint32_t(std::strtoul(hex.c_str(), nullptr, 16));
            c.bits = std::atoi(l.c_str() + lb + 1);
            if (int(bits.size()) != c.bits || std::strtoul(bits.c_str(), nullptr, 2) != c.code) {
                continue;   // not a row of the table
            }
            out.push_back(c);
        }
        return out;
    }

    struct Entry {
        std::string name;
        std::string value;
        size_t size;   // the "(s = n)" of the RFC
    };

    struct Example {
        std::string section;                                    // "C.3.1"
        std::vector<std::pair<std::string, std::string>> fields;   // the header list to encode
        std::vector<uint8_t> encoded;                            // the hex dump
        std::vector<Entry> table;                                // the dynamic table after, newest first
        size_t table_size = 0;
    };

    // "name: value", the name possibly a pseudo-header (":path")
    inline std::pair<std::string, std::string> field(const std::string& l) {
        auto s = trim(l);
        auto colon = s.find(": ", 1);
        if (colon == std::string::npos) {
            return {s.substr(0, s.size() - (s.back() == ':' ? 1 : 0)), ""};
        }
        return {s.substr(0, colon), s.substr(colon + 2)};
    }

    // Appendix C.2 to C.6: every example with a header list
    inline std::vector<Example> examples(const std::vector<std::string>& ls) {
        std::vector<Example> out;
        enum { none, list, hex, table, decoding } state = none;
        Example cur;
        bool in = false;
        auto flush = [&] {
            if (!cur.section.empty() && !cur.encoded.empty()) {
                out.push_back(cur);
            }
            cur = Example();
        };
        for (auto& l : ls) {
            if (l.rfind("Appendix C.", 0) == 0) {
                in = true;
                continue;
            }
            if (!in) {
                continue;
            }
            if (l.rfind("Authors' Addresses", 0) == 0 || l.rfind("Acknowledgments", 0) == 0) {
                break;
            }
            if (l.size() > 3 && l[0] == 'C' && l[1] == '.' && std::isdigit((unsigned char)l[2])) {
                auto sp = l.find(' ');
                std::string sec = l.substr(0, sp);
                if (!sec.empty() && sec.back() == '.') {
                    sec.pop_back();
                }
                if (std::count(sec.begin(), sec.end(), '.') == 2) {   // C.x.y: an example
                    flush();
                    cur.section = sec;
                }
                state = none;
                continue;
            }
            auto t = trim(l);
            if (t == "Header list to encode:") {
                state = list;
                continue;
            }
            if (t == "Hex dump of encoded data:") {
                state = hex;
                continue;
            }
            if (t == "Decoding process:") {
                state = decoding;
                continue;
            }
            if (t == "Dynamic Table (after decoding):") {
                state = table;
                continue;
            }
            if (t == "Decoded header list:") {
                state = none;
                continue;
            }
            if (t.empty()) {
                continue;
            }
            switch (state) {
                case list:
                    cur.fields.push_back(field(l));
                    break;
                case hex: {
                    auto bar = l.find('|');
                    std::istringstream words(l.substr(0, bar));
                    std::string w;
                    while (words >> w) {
                        for (size_t i = 0; i + 1 < w.size(); i += 2) {
                            cur.encoded.push_back(uint8_t(std::strtoul(w.substr(i, 2).c_str(), nullptr, 16)));
                        }
                    }
                    break;
                }
                case table:
                    if (t.rfind("Table size:", 0) == 0) {
                        cur.table_size = size_t(std::atoi(t.c_str() + 11));
                        state = none;
                    } else if (t[0] != '[' && !cur.table.empty()) {
                        cur.table.back().value += " " + t;   // an entry's value wrapped onto the next line
                    } else if (t[0] == '[') {
                        // "[  1] (s =  55) custom-key: custom-header"
                        auto s = t.find("(s =");
                        auto e = t.find(')', s);
                        Entry en;
                        en.size = size_t(std::atoi(t.c_str() + s + 4));
                        auto f = field(t.substr(e + 2));
                        en.name = f.first;
                        en.value = f.second;
                        cur.table.push_back(en);
                    }
                    break;
                default:
                    break;
            }
        }
        flush();
        return out;
    }
}
