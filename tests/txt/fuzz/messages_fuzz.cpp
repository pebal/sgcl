//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The message catalogs and MessageFormat on any input. The first byte picks
// the path (and a locale of a short list); the rest is the input. What must
// hold:
//   - catalog::parse_po of any text refuses it or reads a catalog whose .mo
//     (to_mo) reads back to the same answers for every message it holds, and
//     writes the same bytes again;
//   - catalog::parse_mo of any bytes refuses them or reads a catalog that
//     goes through to_mo and back to the same answers;
//   - a Plural-Forms of any text gives every n a form below nplurals;
//   - message_format::parse of any text refuses it or reads a message that
//     writes, with any arguments, valid UTF-8 from valid UTF-8, never
//     throwing.
// Every reading is of libFuzzer's own buffer or of a malloc'd copy of exactly
// its size (detail entries the public calls wrap), never of a copy on the
// managed heap, where ASan does not see a read past the end.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/messages_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/time.h"
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const char* const Locales[] = {"en", "pl", "ar", "ru", "ja", "de-CH", "cy", "hi"};
    const uint64_t Ns[] = {0, 1, 2, 3, 5, 11, 21, 101, 1000000, 18446744073709551615ull};

    // every message of a and b answers alike
    void same_answers(const txt::catalog& a, const txt::catalog& b) {
        const auto* d = txt::detail::CatalogReader::data(a);
        check(a.size() == b.size());
        if (!d) {
            return;
        }
        for (size_t i = 0; i < d->ids.size(); ++i) {
            const string& id = d->ids[i];
            const string& plural = d->plurals[i];
            if (d->has_context[i]) {
                const string& ctx = d->contexts[i];
                check(a.translate(id, ctx) == b.translate(id, ctx));
                for (uint64_t n : Ns) {
                    check(a.translate(id, plural, n, ctx) == b.translate(id, plural, n, ctx));
                }
            } else {
                check(a.translate(id) == b.translate(id));
                for (uint64_t n : Ns) {
                    check(a.translate(id, plural, n) == b.translate(id, plural, n));
                }
            }
        }
        for (uint64_t n : Ns) {
            check(a.plural_index(n) == b.plural_index(n));
        }
    }

    void po_path(std::string_view in) {
        auto c = txt::detail::parse_po_text(in);
        if (!c) {
            check(!c.error().message().empty());
            return;
        }
        auto mo = c->to_mo();
        auto back = txt::catalog::parse_mo(slice<const byte>(mo.data(), mo.size()));
        check(back.has_value());
        same_answers(*c, *back);
        auto again = back->to_mo();
        check(again.size() == mo.size());
        for (size_t i = 0; i < mo.size(); ++i) {
            check(again[i] == mo[i]);
        }
    }

    void mo_path(std::string_view in) {
        auto c = txt::catalog::parse_mo(slice<const byte>(reinterpret_cast<const byte*>(in.data()), in.size()));
        if (!c) {
            return;
        }
        auto mo = c->to_mo();
        auto back = txt::catalog::parse_mo(slice<const byte>(mo.data(), mo.size()));
        check(back.has_value());
        same_answers(*c, *back);
    }

    void rule_path(std::string_view in) {
        std::string po = "msgid \"\"\nmsgstr \"Plural-Forms: nplurals=4; plural=";
        for (char ch : in) {
            if (ch == '"' || ch == '\\' || ch == '\n' || ch == '\r' || ch == '\0') {
                break;
            }
            po.push_back(ch);
        }
        po += ";\\n\"\n";
        char* exact = static_cast<char*>(std::malloc(po.size()));
        std::copy(po.begin(), po.end(), exact);
        auto c = txt::detail::parse_po_text(std::string_view(exact, po.size()));
        std::free(exact);
        if (!c) {
            return;
        }
        for (uint64_t n : Ns) {
            check(c->plural_index(n) < c->plural_forms() || c->plural_index(n) == 0);
        }
    }

    void message_path(std::string_view in, const txt::locale& l, uint8_t mode) {
        auto m = txt::detail::MessageParser::make(in, string(in), l);
        if (!m) {
            check(!m.error().message().empty());
            return;
        }
        bool utf = utf8::valid(in);
        txt::value args;
        switch (mode % 4) {
            case 0: args = txt::object{{"n", int64_t(mode) - 100}, {"s", "x"}, {"g", "female"}, {"d", 1700000000000ll}}; break;
            case 1: args = txt::object{{"n", double(mode) / 7.0}, {"s", string(in.substr(0, 8))}, {"d", "2026-10-06T14:05:00Z"}}; break;
            case 2: args = txt::list{1, 2.5, "a", true}; break;
            default: args = txt::object{}; break;
        }
        string out = m->format(args);
        check(!utf || utf8::valid(out.view()));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 8192) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    txt::locale l{string(Locales[(mode >> 3) % (sizeof Locales / sizeof Locales[0])])};
    switch (mode % 5) {
        case 0: po_path(rest); break;
        case 1: mo_path(rest); break;
        case 2: rule_path(rest); break;
        default: message_path(rest, l, mode); break;
    }
    return 0;
}
