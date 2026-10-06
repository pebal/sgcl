//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The configuration formats of the encoding module: dotenv and ini. Go's
// standard library has neither and no C or C++ library of them is on the
// machine: INI's reference is Python's configparser (benchmarks/python/ini.py,
// the same document, the dialect ini.h states); dotenv has none.
//   config sgcl [op=ini_parse] [seconds=2] [dump]
//
//   ini_parse      ini::parse of 1000 sections of 10 keys (some values of
//                  several lines), the sections made each time
//   ini_write      to_string of them parsed once
//   dotenv_parse   dotenv::parse of 10000 entries (plain, quoted, escaped,
//                  some ${NAME} expansions), the entries made each time
//   dotenv_write   to_string of them parsed once
//
// "dump" as the last argument writes the document of the op instead (for
// benchmarks/python/ini.py). Prints nanoseconds per document and megabytes
// of it per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace sgcl;
using encoding::dotenv;
using encoding::ini;

namespace {
    volatile size_t sink = 0;

    std::string ini_text() {
        std::string t = "; generated\nname = bench\n";
        for (int s = 0; s < 1000; ++s) {
            t += "\n[section " + std::to_string(s) + "]\n";
            for (int k = 0; k < 10; ++k) {
                t += "key_" + std::to_string(k) + " = ";
                switch (k % 4) {
                    case 0: t += std::to_string(s * 10 + k); break;
                    case 1: t += "a value of several words, " + std::to_string(k); break;
                    case 2: t += "http://example.com/path?q=" + std::to_string(s); break;
                    default: t += "first line\n    second line\n    third line"; break;
                }
                t += '\n';
            }
        }
        return t;
    }

    std::string dotenv_text() {
        std::string t = "# generated\nBASE=/srv/app\n";
        for (int i = 0; i < 10000; ++i) {
            std::string k = "KEY_" + std::to_string(i);
            switch (i % 4) {
                case 0: t += k + "=" + std::to_string(i) + "\n"; break;
                case 1: t += "export " + k + "='single quoted value " + std::to_string(i) + "'\n"; break;
                case 2: t += k + "=\"double\\tquoted\\nvalue " + std::to_string(i) + "\"\n"; break;
                default: t += k + "=${BASE}/data/" + std::to_string(i) + " # a comment\n"; break;
            }
        }
        return t;
    }

    template<class F>
    double timed(double seconds, long& count, F&& f) {
        for (int i = 0; i < 3; ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        count = 0;
        while (bench::seconds_since(t0) < seconds) {
            f();
            ++count;
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "ini_parse";
    double seconds = argc > 3 ? std::atof(argv[3]) : 2.0;
    bool is_ini = !std::strncmp(op, "ini", 3);
    string text(is_ini ? ini_text() : dotenv_text());
    if (argc > 4 && !std::strcmp(argv[4], "dump")) {
        std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }
    if (std::strcmp(variant, "sgcl")) {
        std::fprintf(stderr, "usage: config sgcl [ini_parse|ini_write|dotenv_parse|dotenv_write] [seconds] [dump]\n");
        return 2;
    }
    long count = 0;
    double ns = 0;
    if (!std::strcmp(op, "ini_parse")) {
        ns = timed(seconds, count, [&] { sink += ini::parse(text)->size(); });
    } else if (!std::strcmp(op, "ini_write")) {
        ini v = ini::parse(text).value();
        ns = timed(seconds, count, [&] { sink += v.to_string().size(); });
    } else if (!std::strcmp(op, "dotenv_parse")) {
        dotenv::options o;
        o.use_environment = false;
        ns = timed(seconds, count, [&] { sink += dotenv::parse(text, o)->size(); });
    } else if (!std::strcmp(op, "dotenv_write")) {
        dotenv v = dotenv::parse(text).value();
        ns = timed(seconds, count, [&] { sink += v.to_string().size(); });
    } else {
        std::fprintf(stderr, "config: no op called %s\n", op);
        return 2;
    }
    std::printf("%s op=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, text.size(), count, ns, double(text.size()) / ns * 1e3);
}
