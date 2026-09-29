//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON and XML documents on any bytes, written and read again, without an
// oracle. The first byte picks the format and the style. What must hold:
//   - a JSON text that parses is written (compact, pretty, with HTML
//     escaped) to a text that parses to an equal value, and writes again
//     to the same text; the writing of one text twice in a row is the
//     same (the block lent by the thread is emptied between calls);
//   - a typed value read from it (a record of a number, a text, a list,
//     a hashed set and a map) is written compact to the text its pretty
//     writing compacts to: a hashed set sorted and written once (compact)
//     and one sorted and written again (pretty) hold the same elements in
//     the same order;
//   - an XML document that parses is written (compact, pretty) to a text
//     that parses to an equal document, and writes again to the same text.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/documents_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct record {
        int64_t n = 0;
        string s;
        vector<double> list;
        set<string> tags;
        map<string, int64_t> counts;

        void describe(field_list& f) {
            f.add("n", n);
            f.add("s", s);
            f.add("list", list);
            f.add("tags", tags);
            f.add("counts", counts);
        }
    };

    void json_text(uint8_t mode, std::string_view in) {
        auto v = json::parse(string(in));
        if (!v) {
            return;
        }
        json::style styles[3] = {json::compact, json::pretty, json::compact};
        styles[2].escape_html = true;
        const auto& s = styles[mode % 3];
        string once = v->to_string(s);
        check(once == v->to_string(s));
        auto back = json::parse(once);
        check(back.has_value());
        check(*back == *v);
        check(back->to_string(s) == once);
        // typed: sorted once (compact) and again (pretty) agree
        auto r = json::parse<record>(string(in));
        if (r) {
            auto compact = json::stringify(*r);
            auto pretty = json::stringify(*r, json::pretty);
            check(compact.has_value() && pretty.has_value());
            auto p = json::parse(*pretty);
            check(p.has_value());
            check(p->to_string() == *compact);
        }
    }

    void xml_text(uint8_t mode, std::string_view in) {
        auto d = xml::parse(string(in));
        if (!d) {
            return;
        }
        const auto& s = mode & 1 ? xml::pretty : xml::compact;
        string once = d->to_string(s);
        check(once == d->to_string(s));
        auto back = xml::parse(once);
        check(back.has_value());
        if (!(mode & 1)) {
            check(*back == *d);   // pretty adds white space between elements
        }
        check(back->to_string(s) == once);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode & 0x80) {
        xml_text(mode, in);
    } else {
        json_text(mode, in);
    }
    return 0;
}
