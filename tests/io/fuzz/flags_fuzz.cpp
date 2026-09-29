//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::flags against a model: the bytes split at NUL into a command line,
// parsed by a fixed set of flags (an int, an int64, a uint64, a bool, a
// string, a double, a duration, the positional list), and what must hold:
//   - the outcome is the model's: which token ends the flags, which is
//     refused and why (bad syntax, not defined, needs an argument, help,
//     a value refused), the positional list the arguments after the flags;
//   - a value taken is one the converter reads back to itself (its Go text
//     read again gives the same value), and a plain decimal integer or
//     number gives what std::from_chars gives;
//   - the usage is made and holds every flag's name.
// Go's own answers are the oracle of tests/io/flags.cpp; this is the model
// of the control flow and the converters' consistency, on any bytes.
// Built with libFuzzer (tests/fuzz/run.sh tests/io/fuzz/flags_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/io/flags.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace io = sgcl::io;
    namespace d = sgcl::io::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const std::vector<std::string_view> Names = {"i", "n", "u", "b", "s", "f", "t"};

    bool plain_decimal(std::string_view s) {
        if (s.empty() || s.size() > 18) {
            return false;
        }
        size_t at = s[0] == '-' ? 1 : 0;
        if (at == s.size() || (s[at] == '0' && s.size() > at + 1)) {
            return false;
        }
        for (size_t k = at; k < s.size(); ++k) {
            if (s[k] < '0' || s[k] > '9') {
                return false;
            }
        }
        return true;
    }

    // Whether the converter of the flag takes the text (the converters are
    // checked on their own below: read back, and against from_chars)
    bool accepted(std::string_view name, std::string_view text) {
        if (name == "i") {
            int v;
            return d::flag_set<int>(&v, text).empty();
        }
        if (name == "n") {
            int64_t v;
            return d::flag_set<int64_t>(&v, text).empty();
        }
        if (name == "u") {
            uint64_t v;
            return d::flag_set<uint64_t>(&v, text).empty();
        }
        if (name == "b") {
            bool v;
            return d::flag_set<bool>(&v, text).empty();
        }
        if (name == "f") {
            double v;
            return d::flag_set<double>(&v, text).empty();
        }
        if (name == "t") {
            duration v;
            return d::flag_set<duration>(&v, text).empty();
        }
        return true;   // a string takes anything
    }

    // The model's reading of the command line: the kind of the outcome
    // and, for a value, the flag and its text; the index where the flags end
    struct Expect {
        enum kind { ok, syntax, undefined, needs, help, value } what = ok;
        std::string_view flag;
        std::string_view text;
        bool boolean = false;
        size_t end = 0;
    };

    Expect model(const std::vector<std::string_view>& args) {
        Expect e;
        size_t i = 0;
        for (; i < args.size(); ++i) {
            std::string_view s = args[i];
            if (s.size() < 2 || s[0] != '-') {
                break;
            }
            std::string_view body = s[1] == '-' ? s.substr(2) : s.substr(1);
            if (s == "--") {
                ++i;
                break;
            }
            if (body.empty() || body[0] == '-' || body[0] == '=') {
                e.what = Expect::syntax;
                return e;
            }
            auto eq = body.find('=', 1);
            std::string_view name = eq == std::string_view::npos ? body : body.substr(0, eq);
            bool known = false;
            for (auto n : Names) {
                known = known || n == name;
            }
            if (!known) {
                e.what = name == "h" || name == "help" ? Expect::help : Expect::undefined;
                return e;
            }
            e.flag = name;
            e.boolean = name == "b";
            if (eq != std::string_view::npos) {
                e.text = body.substr(eq + 1);
            } else if (e.boolean) {
                e.text = "true";
            } else if (i + 1 < args.size()) {
                e.text = args[++i];
            } else {
                e.what = Expect::needs;
                return e;
            }
            if (!accepted(name, e.text)) {
                e.what = Expect::value;
                return e;
            }
        }
        e.end = i;
        return e;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in(reinterpret_cast<const char*>(data), size);
    std::vector<std::string_view> args;
    size_t at = 0;
    while (at <= in.size() && args.size() < 64) {
        size_t end = in.find('\0', at);
        if (end == std::string_view::npos) {
            end = in.size();
        }
        args.push_back(in.substr(at, end - at));
        at = end + 1;
    }
    if (!in.empty() && in.back() == '\0') {
        args.pop_back();
    }
    if (in.empty()) {
        args.clear();
    }
    int i = 7;
    int64_t n = 0;
    uint64_t u = 0;
    bool b = false;
    string s = "s";
    double f = 0.5;
    duration t = 5 * second;
    vector<string> rest;
    io::flags fl("the model's flags");
    fl.add("i", i, "an int");
    fl.add("n", n, "an int64");
    fl.add("u", u, "a uint64");
    fl.add("b", b, "a bool");
    fl.add("s", s, "a string");
    fl.add("f", f, "a `number`");
    fl.add("t", t, "a duration");
    fl.positional("rest", rest, "the rest");
    vector<string> list;
    for (auto a : args) {
        list.push_back(string(a));
    }
    auto r = fl.parse(list);
    Expect e = model(args);
    if (r) {
        check(e.what == Expect::ok);
        check(rest.size() == args.size() - e.end);
        for (size_t k = 0; k < rest.size(); ++k) {
            check(rest[k].view() == args[e.end + k]);
        }
        // every value taken reads back to itself
        int64_t n2;
        check(d::go_parse_int(d::flag_text(n), 64, n2) == d::GoNumber::ok && n2 == n);
        uint64_t u2;
        check(d::go_parse_uint(d::flag_text(u), 64, u2) == d::GoNumber::ok && u2 == u);
        double f2;
        check(d::go_parse_float(d::flag_text(f), f2) == d::GoNumber::ok);
        check((std::isnan(f) && std::isnan(f2)) || f2 == f);
        auto t2 = duration::parse(string(d::flag_text(t)));
        check(t2 && *t2 == t);
    } else if (r.error().code() == io::errc::help_requested) {
        check(e.what == Expect::help);
    } else {
        std::string_view m = r.error().path().view();
        switch (e.what) {
            case Expect::syntax: check(m.starts_with("bad flag syntax: ")); break;
            case Expect::undefined: check(m.starts_with("flag provided but not defined: -")); break;
            case Expect::needs: check(m.starts_with("flag needs an argument: -")); break;
            case Expect::value:
                check(m.starts_with(e.boolean ? "invalid boolean value \"" : "invalid value \""));
                check(m.ends_with(": parse error") || m.ends_with(": value out of range"));
                break;
            case Expect::ok:
                check(rest.empty() && false);   // a command line the model takes, refused
                break;
            default:
                check(false);
        }
    }
    // the converters against std::from_chars on plain decimals
    for (auto a : args) {
        if (plain_decimal(a)) {
            int64_t x;
            check(d::go_parse_int(a, 64, x) == d::GoNumber::ok);
            int64_t y = 0;
            std::from_chars(a.data(), a.data() + a.size(), y);
            check(x == y);
            double g;
            check(d::go_parse_float(a, g) == d::GoNumber::ok);
            check(g == double(y));
        }
        std::string q;
        d::go_quote(q, a);
        check(q.size() >= 2 && q.front() == '"' && q.back() == '"');
    }
    string usage = fl.usage();
    for (auto name : Names) {
        check(usage.view().find(std::string("  -") + std::string(name)) != std::string_view::npos);
    }
    return 0;
}
