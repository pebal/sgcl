//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::stencil over a source from outside, without an oracle: Go's
// text/template is one only over the subset tools/stencil_oracle.go asks,
// and the fuzzer's sources are not that. The first byte picks the data the
// page is rendered over; the rest is the source. What must hold:
//   - parse answers a template or an error, never anything else, and
//     parses says which; the error's place is in the source, its line one
//     more than the line feeds before it and its column one more than the
//     bytes since the last of them, and it has a reason;
//   - a template keeps its source, and one with no action in it is a
//     single step (none when empty) that renders as itself;
//   - a render is total over the data (it throws nothing, the six
//     functions throwing nothing), and the same twice; render_to reports
//     the size render writes, with no room, with a little and with all of
//     it, and writes the page's first bytes into what it is lent;
//   - a page of a source and data that are UTF-8 is UTF-8 (a field cut on
//     a cluster, {:c} of a number no character holds a dropped letter).
// The pages are kept small: the lists of the data are short and a source
// with more than four ranges or a number of four digits (a width of
// thousands of columns) is parsed and not rendered, so that the time goes
// into sources and not into writing out the same page many times.
// Built with libFuzzer:
//   tests/fuzz/run.sh tests/txt/fuzz/stencil_fuzz.cpp 300
#include "sgcl/txt/txt.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The shapes a value takes, every one of them, with names a source
    // may guess: text that needs escaping, a number of each kind, the
    // truths, nothing, lists empty and not, mappings nested, and a
    // mapping that holds itself
    txt::value data_of(uint8_t pick) {
        txt::object self;
        self.set("name", txt::value("loop"));
        self.set("self", self);
        txt::value rich = txt::object{
            {"a", "<b>&\"'</b>"},
            {"s", "żółć 日本 \U0001F600"},
            {"n", 42},
            {"neg", -7},
            {"r", -1.5},
            {"big", 1e300},
            {"t", true},
            {"f", false},
            {"z", nullptr},
            {"e", txt::list{}},
            {"o", txt::object{}},
            {"xs", txt::list{1, "two", 3.5, txt::list{4}}},
            {"m", txt::object{{"key", "v"}, {"n", 7}, {"in", txt::object{{"deep", "x"}}}}},
            {"cp", 0x1F600},
            {"bad", 0xD800},
            {"loop", self},
        };
        switch (pick % 4) {
            case 0: return rich;
            case 1: return txt::value();
            case 2: return txt::list{rich, "x", 0};
            default: return txt::value("text alone");
        }
    }

    // Whether a render of the source could be long for nothing: many
    // ranges multiply the page, a width of four digits pads it
    bool too_long_to_render(std::string_view source) {
        size_t ranges = 0;
        for (size_t at = source.find("range"); at != std::string_view::npos; at = source.find("range", at + 1)) {
            ++ranges;
        }
        if (ranges > 4) {
            return true;
        }
        size_t digits = 0;
        for (char c : source) {
            digits = c >= '0' && c <= '9' ? digits + 1 : 0;
            if (digits >= 4) {
                return true;
            }
        }
        return false;
    }

    void rendering(const txt::stencil& t, std::string_view source, const txt::value& data) {
        string page = t.render(data);
        check(t.render(data) == page);
        check(t.render_to(slice<char>(), data) == page.size());
        char little[16];
        size_t size = t.render_to(slice<char>(tracked_ptr<const void>(), little, sizeof little), data);
        check(size == page.size());
        size_t shown = std::min(size, sizeof little);
        check(std::string_view(little, shown) == page.view().substr(0, shown));
        std::string all(page.size(), '\0');
        size = t.render_to(slice<char>(tracked_ptr<const void>(), all.data(), all.size()), data);
        check(size == page.size() && std::string_view(all) == page.view());
        if (utf8::valid(source)) {
            check(utf8::valid(page.view()));
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 2048) {
        return 0;
    }
    std::string_view source(reinterpret_cast<const char*>(data + 1), size - 1);
    string src(source);
    auto t = txt::stencil::parse(src);
    check(t.has_value() == txt::stencil::parses(src));
    if (!t) {
        const auto& e = t.error();
        check(e.offset() <= source.size());
        check(!e.message().empty());
        size_t line = 1, column = 1;
        for (size_t i = 0; i < e.offset(); ++i) {
            if (source[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        check(e.line() == line && e.column() == column);
        return 0;
    }
    check(t->source().view() == source);
    if (source.find("{{") == std::string_view::npos) {
        check(t->steps() == (source.empty() ? 0u : 1u));
        check(t->render(txt::value()).view() == source);
    }
    if (too_long_to_render(source)) {
        return 0;
    }
    rendering(*t, source, data_of(data[0]));
    return 0;
}
