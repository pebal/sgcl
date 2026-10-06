// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// Documents the writer of sgcl/encoding/toml.h writes of 400 random values,
// in the corpus form ("#%%% written N" before each) that tools/toml_oracle.py
// reads (its header says how the two are run): so that tomllib reads what the
// writer writes as the value written.
#include "sgcl/encoding/toml.h"

#include <cstdio>
#include <random>
#include <string>

using sgcl::encoding::toml;

namespace {
    const char* strings[] = {"plain", "two words", "", " lead", "trail ", "a = b", "a #b", "#c", "[x]", "[[y]]", "{z}", "a.b",
                             "true", "12", "1.5", "0x1F", "inf", "nan", "multi\nline", "end\n", "\ttab", "quote\"s", "it's",
                             "'''", "\"\"\"", "zażółć", "日本", "😀", "back\\slash", "\x01", "\x7f", "\x1f", "\r\n", "\b\f",
                             "1979-05-27", "07:32:00", "-", "_", "ʞ"};

    sgcl::string pick(std::mt19937_64& r) {
        return sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]);
    }

    toml random_value(std::mt19937_64& r, int depth, bool table_only) {
        int k = table_only ? 9 : int(r() % (depth > 3 ? 8 : 11));
        switch (k) {
            case 0: return toml(bool(r() & 1));
            case 1: return toml(int64_t(r()) >> (r() % 64));
            case 2: {
                static const double specials[] = {0.0, -0.0, 1e300, -1e-300, 5e-324, 0.1, 1.0 / 3.0, 1e16, 123456789.0};
                return r() % 3 ? toml(double(int64_t(r() % 2000001) - 1000000) / 64.0) : toml(specials[r() % 9]);
            }
            case 3:
            case 4: return toml(pick(r));
            case 5: {
                int64_t s = int64_t(r() % 8000000000ull) - 2000000000;
                int64_t offset = (int64_t(r() % 97) - 48) * 15 * 60;
                auto z = sgcl::time::zone::fixed(sgcl::duration(std::chrono::seconds(offset)));
                return toml(sgcl::time::datetime::from_unix_nano(s * 1000000000 + int64_t(r() % 4 ? 0 : r() % 1000000) * 1000, z));
            }
            case 6: return toml(sgcl::time::date(int(1 + r() % 9999), int(1 + r() % 12), int(1 + r() % 28)));
            case 7: {
                auto t = sgcl::duration(std::chrono::microseconds(int64_t(r() % 86400000000ull)));
                return r() & 1 ? toml::local_time(t) : toml::local_datetime(sgcl::time::date(int(1 + r() % 9999), int(1 + r() % 12), int(1 + r() % 28)), t);
            }
            case 8: {
                sgcl::vector<toml> items;
                bool tables = r() % 3 == 0;
                for (int i = int(r() % 4); i > 0; --i) {
                    items.push_back(random_value(r, depth + 1, tables));
                }
                return toml::array(items);
            }
            default: {
                sgcl::vector<toml::member> ms;
                for (int i = int(r() % 5); i > 0; --i) {
                    sgcl::string key = pick(r);
                    bool dup = false;
                    for (auto& m : ms) {
                        dup = dup || m.key == key;
                    }
                    if (!dup) {
                        ms.push_back(toml::member{key, random_value(r, depth + 1, false)});
                    }
                }
                return toml::table(ms);
            }
        }
    }
}

int main() {
    std::mt19937_64 r(20261006);
    for (int i = 0; i < 400; ++i) {
        toml v = random_value(r, 0, true);
        std::printf("#%%%%%% written %d\n%s", i, std::string(v.to_string().view()).c_str());
    }
    std::printf("#%%%%%% end\n");
}
