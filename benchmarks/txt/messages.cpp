//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The message catalogs and MessageFormat, one lookup or one message at a
// time, the catalog read and the message parsed once:
//   messages <lookup|plural|parse|format> [mo file]
//   lookup:  catalog::translate of ids of a catalog of 1000 messages (the
//            .mo written by to_mo, or the one given)
//   plural:  catalog::translate(id, plural, n) by the Polish rule
//   parse:   catalog::parse_mo of the 1000 messages, per message
//   format:  message_format::format of a plural, a select and a number
// Prints nanoseconds per operation. The references are GNU libintl
// (dgettext, dngettext over the same .mo) and ICU 78's MessageFormat, C
// programs over the same inputs, run in a scratch directory: neither is in
// the tree.
#include "benchmarks/common.h"
#include "sgcl/txt.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    template<class F>
    double per_op(size_t n, F&& f) {
        size_t sink = 0;
        for (size_t i = 0; i < n / 10; ++i) {
            sink += f(i);
        }
        auto t0 = bench::Clock::now();
        for (size_t i = 0; i < n; ++i) {
            sink += f(i);
        }
        double s = bench::seconds_since(t0);
        if (sink == 42) {
            std::printf(" ");
        }
        return s * 1e9 / double(n);
    }

    // 1000 messages, every tenth a plural, the Polish rule
    std::string catalog_po() {
        std::string po = "msgid \"\"\nmsgstr \"\"\n\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                         "\"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && "
                         "(n%100<10 || n%100>=20) ? 1 : 2);\\n\"\n\n";
        for (int i = 0; i < 1000; ++i) {
            std::string id = "Message number " + std::to_string(i) + " of the program";
            if (i % 10 == 0) {
                po += "msgid \"" + id + "\"\nmsgid_plural \"" + id + "s\"\nmsgstr[0] \"Komunikat " +
                      std::to_string(i) + "\"\nmsgstr[1] \"Komunikaty " + std::to_string(i) +
                      "\"\nmsgstr[2] \"Komunikatów " + std::to_string(i) + "\"\n\n";
            } else {
                po += "msgid \"" + id + "\"\nmsgstr \"Komunikat numer " + std::to_string(i) + " programu\"\n\n";
            }
        }
        return po;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "lookup";
    if (!bench::has_variant(variant, {"lookup", "plural", "parse", "format"})) {
        std::fprintf(stderr, "usage: messages <lookup|plural|parse|format> [write.mo]\n");
        return 2;
    }
    auto c = txt::catalog::parse_po(string(catalog_po())).value();
    auto mo = c.to_mo();
    if (argc > 2) {   // the .mo the references read
        std::ofstream(argv[2], std::ios::binary).write(reinterpret_cast<const char*>(mo.data()), std::streamsize(mo.size()));
    }
    vector<string> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.push_back(string("Message number " + std::to_string(i) + " of the program"));
    }
    double ns = 0;
    constexpr size_t N = 2000000;
    if (!std::strcmp(variant, "lookup")) {
        ns = per_op(N, [&](size_t i) { return c.translate(ids[(i * 7) % 1000]).size(); });
    } else if (!std::strcmp(variant, "plural")) {
        vector<string> plurals;
        for (int i = 0; i < 100; ++i) {
            plurals.push_back(string("Message number " + std::to_string(i * 10) + " of the programs"));
        }
        ns = per_op(N, [&](size_t i) {
            size_t k = (i * 7) % 100;
            return c.translate(ids[k * 10], plurals[k], uint64_t(i % 30)).size();
        });
    } else if (!std::strcmp(variant, "parse")) {
        slice<const byte> bytes(mo.data(), mo.size());
        ns = per_op(200, [&](size_t) { return txt::catalog::parse_mo(bytes)->size(); }) / 1000.0;
    } else {
        txt::message_format m(string("{n, plural, one {# plik} few {# pliki} other {# plików}} w katalogu {dir}, "
                                     "{g, select, female {ostatnio zmieniła} other {ostatnio zmienił}} {who}: "
                                     "{size, number, integer} B"),
                              txt::locale(string("pl")));
        txt::value args[4] = {
            txt::object{{"n", 1}, {"dir", "src"}, {"g", "female"}, {"who", "Ada"}, {"size", 1234.5}},
            txt::object{{"n", 3}, {"dir", "lib"}, {"g", "male"}, {"who", "Jan"}, {"size", 99.0}},
            txt::object{{"n", 25}, {"dir", "doc"}, {"g", "female"}, {"who", "Ewa"}, {"size", 123456.0}},
            txt::object{{"n", 112}, {"dir", "bin"}, {"g", "other"}, {"who", "Kim"}, {"size", 7.0}},
        };
        ns = per_op(N / 4, [&](size_t i) { return m.format(args[i % 4]).size(); });
    }
    std::printf("%s ns/op=%.1f\n", variant, ns);
    return 0;
}
