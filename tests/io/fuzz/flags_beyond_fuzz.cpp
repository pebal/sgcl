//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::flags beyond Go against a model of its own: the bytes split at NUL
// into a command line, parsed by a description with every addition (two
// bools for the combined form, an int with a second name, a list of ints
// with a second name, a positional list, and a subcommand with a flag and a
// list of its own), and what must hold:
//   - the outcome is the model's (ok, help, refused), and when the command
//     line is taken, every variable is the model's: the bools set by the
//     combined tokens, the int's last value under either name, the list's
//     values in their order (the default gone at the first), the
//     arguments that went to the positional list, the command chosen and
//     its own flag and arguments;
//   - a refused command line says why in one of the messages the parser has;
//   - the usages are made and name every flag and the command.
// Go has none of these (tests/io/flags_fuzz.cpp holds the parser to Go's
// syntax). Built with libFuzzer (tests/fuzz/run.sh
// tests/io/fuzz/flags_beyond_fuzz.cpp) or replayed by the library's own
// driver (tests/fuzz/driver.cpp).
#include "sgcl/io/flags.h"

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

    struct State {
        bool a = false;
        bool b = false;
        int32_t number = 5;
        std::vector<int32_t> list = {9};
        std::vector<std::string> rest;
        bool sub = false;
        int32_t q = 0;
        std::vector<std::string> subrest;
    };

    enum class Kind { ok, help, refused };

    // One level of the model: the flags of `names`, the arguments after
    // them; the index where the flags end, or the outcome that ended it
    struct Level {
        Kind kind = Kind::ok;
        size_t end = 0;
    };

    bool int32_of(std::string_view text, int32_t& out) {
        int64_t v;
        if (d::go_parse_int(text, 32, v) != d::GoNumber::ok) {
            return false;
        }
        out = int32_t(v);
        return true;
    }

    Level root_flags(const std::vector<std::string_view>& args, State& st) {
        Level l;
        bool list_given = false;
        size_t i = 0;
        while (i < args.size()) {
            std::string_view s = args[i];
            if (s.size() < 2 || s[0] != '-') {
                break;
            }
            if (s == "--") {
                ++i;
                break;
            }
            std::string_view body = s.substr(s[1] == '-' ? 2 : 1);
            if (body.empty() || body[0] == '-' || body[0] == '=') {
                l.kind = Kind::refused;
                return l;
            }
            ++i;
            auto eq = body.find('=', 1);
            bool has_value = eq != std::string_view::npos;
            std::string_view name = has_value ? body.substr(0, eq) : body;
            std::string_view value = has_value ? body.substr(eq + 1) : std::string_view();
            if (name == "a" || name == "b") {
                bool v = true;
                if (has_value && d::go_parse_bool(value, v) != d::GoNumber::ok) {
                    l.kind = Kind::refused;
                    return l;
                }
                (name == "a" ? st.a : st.b) = v;
                continue;
            }
            bool number = name == "number" || name == "n";
            bool list = name == "list" || name == "l";
            if (!number && !list) {
                if (name == "h" || name == "help") {
                    l.kind = Kind::help;
                    return l;
                }
                bool combined = !has_value && name.size() >= 2;
                for (char c : name) {
                    combined = combined && (c == 'a' || c == 'b');
                }
                if (combined) {
                    for (char c : name) {
                        (c == 'a' ? st.a : st.b) = true;
                    }
                    continue;
                }
                l.kind = Kind::refused;
                return l;
            }
            if (!has_value) {
                if (i == args.size()) {
                    l.kind = Kind::refused;
                    return l;
                }
                value = args[i++];
            }
            int32_t v;
            if (!int32_of(value, v)) {
                l.kind = Kind::refused;
                return l;
            }
            if (number) {
                st.number = v;
            } else {
                if (!list_given) {
                    st.list.clear();
                    list_given = true;
                }
                st.list.push_back(v);
            }
        }
        l.end = i;
        return l;
    }

    Kind sub_flags(const std::vector<std::string_view>& args, State& st) {
        size_t i = 0;
        while (i < args.size()) {
            std::string_view s = args[i];
            if (s.size() < 2 || s[0] != '-') {
                break;
            }
            if (s == "--") {
                ++i;
                break;
            }
            std::string_view body = s.substr(s[1] == '-' ? 2 : 1);
            if (body.empty() || body[0] == '-' || body[0] == '=') {
                return Kind::refused;
            }
            ++i;
            auto eq = body.find('=', 1);
            bool has_value = eq != std::string_view::npos;
            std::string_view name = has_value ? body.substr(0, eq) : body;
            std::string_view value = has_value ? body.substr(eq + 1) : std::string_view();
            if (name != "q") {
                return name == "h" || name == "help" ? Kind::help : Kind::refused;
            }
            if (!has_value) {
                if (i == args.size()) {
                    return Kind::refused;
                }
                value = args[i++];
            }
            if (!int32_of(value, st.q)) {
                return Kind::refused;
            }
        }
        for (; i < args.size(); ++i) {
            st.subrest.push_back(std::string(args[i]));
        }
        return Kind::ok;
    }

    Kind model(const std::vector<std::string_view>& args, State& st) {
        Level l = root_flags(args, st);
        if (l.kind != Kind::ok) {
            return l.kind;
        }
        if (l.end < args.size() && args[l.end] == "sub") {
            st.sub = true;
            std::vector<std::string_view> rest(args.begin() + ptrdiff_t(l.end) + 1, args.end());
            return sub_flags(rest, st);
        }
        for (size_t i = l.end; i < args.size(); ++i) {
            st.rest.push_back(std::string(args[i]));
        }
        return Kind::ok;
    }

    const char* const Messages[] = {"bad flag syntax: ", "flag provided but not defined: -", "flag needs an argument: -",
                                    "invalid value \"", "invalid boolean value \"", "unknown command: "};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in(reinterpret_cast<const char*>(data), size);
    std::vector<std::string_view> args;
    if (!in.empty()) {
        size_t at = 0;
        while (at <= in.size() && args.size() < 64) {
            size_t end = in.find('\0', at);
            if (end == std::string_view::npos) {
                end = in.size();
            }
            args.push_back(in.substr(at, end - at));
            at = end + 1;
        }
    }
    bool a = false;
    bool b = false;
    int32_t number = 5;
    vector<int32_t> list = {9};
    vector<string> rest;
    int32_t q = 0;
    vector<string> subrest;
    io::flags root("the root", {{"a", a, "a bool"}, {"b", b, "a bool"},
                                {"number", number, "an int", {.short_name = "n"}},
                                {"list", list, "ints", {.short_name = "l"}}});
    root.positional("rest", rest, "the rest");
    io::flags sub("the command", {{"q", q, "an int"}});
    sub.positional("subrest", subrest, "its rest");
    root.add_command("sub", sub);
    vector<string> line;
    for (auto s : args) {
        line.push_back(string(s));
    }
    auto r = root.parse(line);
    State st;
    Kind want = model(args, st);
    if (r) {
        check(want == Kind::ok);
        check(a == st.a && b == st.b && number == st.number);
        check(list.size() == st.list.size());
        for (size_t k = 0; k < list.size(); ++k) {
            check(list[k] == st.list[k]);
        }
        check(sub.chosen() == st.sub);
        if (st.sub) {
            check(q == st.q);
            check(subrest.size() == st.subrest.size());
            for (size_t k = 0; k < subrest.size(); ++k) {
                check(subrest[k].view() == st.subrest[k]);
            }
        } else {
            check(rest.size() == st.rest.size());
            for (size_t k = 0; k < rest.size(); ++k) {
                check(rest[k].view() == st.rest[k]);
            }
        }
    } else if (r.error().code() == io::errc::help_requested) {
        check(want == Kind::help);
    } else {
        check(want == Kind::refused);
        std::string_view m = r.error().path().view();
        bool known = false;
        for (auto p : Messages) {
            known = known || m.starts_with(p);
        }
        check(known);
    }
    string u = root.usage();
    for (const char* name : {"  -a\t", "  -b\t", "  -n, -number int", "  -l, -list int...", "Commands:\n  sub\n"}) {
        check(u.view().find(name) != std::string_view::npos);
    }
    check(sub.usage().view().find(" sub:\nthe command\n  -q int\n") != std::string_view::npos);
    return 0;
}
