//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/go_text.h"
#include "error.h"
#include "os.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <algorithm>
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sgcl::io {
    class flags;

    namespace detail {
        // A flag's text into its variable: nothing for a value taken, else
        // the reason as Go's flag package gives it ("parse error", "value
        // out of range", or the message of the type's own parse)
        using FlagSet = std::string (*)(void* target, std::string_view text);

        // One flag of a description: a node of a list shared by the
        // descriptions made from one another, the newest first
        struct FlagNode {
            tracked_ptr<FlagNode> prev;
            string name;
            string help;
            string type_name;      // UnquoteUsage's: "int", "string", "duration"...; empty for a bool
            string default_text;   // the variable's value when the flag was added
            string zero_text;      // the zero value's, for "(default ...)" left out
            bool boolean = false;
            bool quoted = false;   // a string's default, quoted
            void* target = nullptr;
            FlagSet set = nullptr;
        };

        template<class T>
        concept FlagParsable = requires(const string& s) {
            { T::parse(s) };
        };

        SGCL_INLINE_HOT const char* go_number_reason(GoNumber r) noexcept {
            return r == GoNumber::range ? "value out of range" : "parse error";
        }

        // A variable of the types the flags read themselves: their text
        // read and written without a throw (a type's own parse and
        // to_string are the program's)
        template<class T>
        inline constexpr bool BuiltinFlag = std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, duration>;

        template<class T>
        std::string flag_set(void* target, std::string_view text) noexcept(BuiltinFlag<T>) {
            T& v = *static_cast<T*>(target);
            if constexpr (std::is_same_v<T, bool>) {
                bool b;
                if (go_parse_bool(text, b) != GoNumber::ok) {
                    return "parse error";
                }
                v = b;
            } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
                int64_t n;
                auto r = go_parse_int(text, unsigned(sizeof(T) * 8), n);
                if (r != GoNumber::ok) {
                    return go_number_reason(r);
                }
                v = T(n);
            } else if constexpr (std::is_integral_v<T>) {
                uint64_t n;
                auto r = go_parse_uint(text, unsigned(sizeof(T) * 8), n);
                if (r != GoNumber::ok) {
                    return go_number_reason(r);
                }
                v = T(n);
            } else if constexpr (std::is_floating_point_v<T>) {
                double d;
                auto r = go_parse_float(text, d);
                if (r == GoNumber::ok && std::is_same_v<T, float> && std::isfinite(d) && std::abs(d) > double(std::numeric_limits<float>::max())) {
                    r = GoNumber::range;
                }
                if (r != GoNumber::ok) {
                    return go_number_reason(r);
                }
                v = T(d);
            } else if constexpr (std::is_same_v<T, string>) {
                v = string(text);
            } else if constexpr (std::is_same_v<T, duration>) {
                auto d = duration::parse(string(text));
                if (!d) {
                    return "parse error";   // Go's durationValue: errParse whatever ParseDuration said
                }
                v = *d;
            } else {
                auto p = T::parse(string(text));
                if (!p) {
                    auto m = p.error().message();
                    return std::string(m.data(), m.size());
                }
                v = T(*p);
            }
            return {};
        }

        template<class T>
        std::string flag_text(const T& v) noexcept(BuiltinFlag<T>) {
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_integral_v<T>) {
                return std::to_string(v);
            } else if constexpr (std::is_floating_point_v<T>) {
                return go_format_float(double(v));
            } else if constexpr (std::is_same_v<T, string>) {
                return std::string(v.data(), v.size());
            } else if constexpr (requires { v.to_string(); }) {
                auto s = v.to_string();
                return std::string(s.data(), s.size());
            } else if constexpr (requires { to_string(v); }) {
                auto s = to_string(v);
                return std::string(s.data(), s.size());
            } else {
                return {};
            }
        }

        template<class T>
        const char* flag_type_name() noexcept {
            if constexpr (std::is_same_v<T, bool>) {
                return "";
            } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
                return "int";
            } else if constexpr (std::is_integral_v<T>) {
                return "uint";
            } else if constexpr (std::is_floating_point_v<T>) {
                return "float";
            } else if constexpr (std::is_same_v<T, string>) {
                return "string";
            } else if constexpr (std::is_same_v<T, duration>) {
                return "duration";
            } else {
                return "value";
            }
        }

        template<class T>
        concept FlagValue = std::is_same_v<T, bool> || std::is_integral_v<T> || std::is_floating_point_v<T>
            || std::is_same_v<T, string> || std::is_same_v<T, duration> || FlagParsable<T>;

        // What a parse came to: taken, help asked for, or refused with
        // Go's text
        struct FlagOutcome {
            enum class kind : uint8_t { ok, help, failed } what = kind::ok;
            std::string message;
        };
    }

    // The command line of a program, as Go's flag package takes it: the
    // flags, each tied to a variable of the program by add, and parse,
    // which reads the arguments into them.
    //
    //   int port = 8080;
    //   bool verbose = false;
    //   io::flags f("serves the files of a directory");
    //   f.add("port", port, "the port to listen on");
    //   f.add("v", verbose, "log every request");
    //   f.parse(argc, argv);
    //
    // The syntax is Go's: -name, --name, -name=value, -name value (not for
    // a bool, whose -name is true and -name=false false); "--" ends the
    // flags, and so does the first argument that is not one ("-" alone
    // included); what follows goes to the positional list, or is an error
    // when the description has none. -h and -help, when no flag is
    // called so, print the usage. The values are read as Go's strconv
    // reads them (0x, 0o, 0b and underscores in integers; 1, t, T, TRUE
    // and the like for a bool; Go's durations), and the messages and the
    // usage are Go's to the byte. It holds the variables by address, and
    // they must outlive its parse; a copy has the flags added so far, so a
    // common set is copied and extended per command.
    class flags {
    public:
        flags() noexcept = default;

        SGCL_INLINE_HOT explicit flags(const string& description) noexcept
        : _description(description) {
        }

        // A flag tied to a variable: bool, an integer, a floating-point
        // number, string, duration, or a type with T::parse(const
        // string&). The variable's value now is the default the usage shows
        // A name that is empty, begins with - or holds = is
        // std::invalid_argument, and so is a name added twice (Go panics)
        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT void add(const string& name, T& target, const string& help) {
            _add(name, target, help);
        }

        // The arguments after the flags, into the vector, in order; the
        // usage lists them last, as "name..."
        SGCL_INLINE_HOT void positional(const string& name, vector<string>& target, const string& help) noexcept {
            _positional(name, target, help);
        }

        // The program's command line: the variables set. -h prints the usage
        // on the standard error and ends the process with 0; a command line
        // the flags do not take prints Go's message and the usage there and
        // ends it with 2 (flag.ExitOnError)
        void parse(int argc, char** argv) const {
            std::vector<std::string_view> args;
            for (int i = 1; i < argc; ++i) {
                args.push_back(argv[i]);
            }
            std::string program = argc > 0 && argv[0] ? argv[0] : "program";
            auto r = _parse(args);
            if (r.what == detail::FlagOutcome::kind::ok) {
                return;
            }
            std::string text;
            if (r.what == detail::FlagOutcome::kind::failed) {
                text = r.message + "\n";
            }
            text += _usage(program);
            (void)io::stderr.write(string(text));
            io::exit(r.what == detail::FlagOutcome::kind::help ? 0 : 2);
        }

        // The same with the arguments given (without the program's name),
        // nothing printed and the process left running: io::errc::
        // help_requested for -h, io::errc::invalid_argument with Go's
        // message as the error's path for a command line refused
        expected<void, error> parse(const vector<string>& args) const {
            std::vector<std::string_view> list;
            for (auto& a : args) {
                list.push_back(a.view());
            }
            auto r = _parse(list);
            if (r.what == detail::FlagOutcome::kind::help) {
                return unexpected(error(errc::help_requested, "flag"));
            }
            if (r.what == detail::FlagOutcome::kind::failed) {
                return unexpected(error(errc::invalid_argument, "flag", string(r.message)));
            }
            return {};
        }

        // The usage parse prints: "Usage of <program>:", the description,
        // then the flags as Go's PrintDefaults writes them, and the
        // positional arguments last
        SGCL_INLINE_HOT string usage() const noexcept {
            auto a = io::args();
            return string(_usage(a.empty() ? std::string("program") : std::string(a[0].data(), a[0].size())));
        }

    private:
        template<class T>
        void _add(const string& name, T& target, const string& help) {
            std::string_view n = name.view();
            if (n.empty() || n[0] == '-' || n.find('=') != std::string_view::npos) {
                throw std::invalid_argument("sgcl::io::flags: a flag's name may be neither empty nor begin with - nor hold =: " + std::string(n));
            }
            if (_find(n)) {
                throw std::invalid_argument("sgcl::io::flags: flag redefined: " + std::string(n));
            }
            tracked_ptr<detail::FlagNode> node = make_tracked<detail::FlagNode>();
            node->prev = _last;
            node->name = name;
            node->help = help;
            node->type_name = string(detail::flag_type_name<T>());
            node->default_text = string(detail::flag_text(target));
            if constexpr (std::is_default_constructible_v<T>) {
                node->zero_text = string(detail::flag_text(T{}));
            }
            node->boolean = std::is_same_v<T, bool>;
            node->quoted = std::is_same_v<T, string>;
            node->target = static_cast<void*>(&target);
            node->set = &detail::flag_set<T>;
            _last = node;
        }

        SGCL_INLINE_HOT void _positional(const string& name, vector<string>& target, const string& help) noexcept {
            _pos_name = name;
            _pos_help = help;
            _pos_target = &target;
        }

        const detail::FlagNode* _find(std::string_view name) const noexcept {
            for (auto n = _last; n; n = n->prev) {
                if (n->name.view() == name) {
                    return n.get();
                }
            }
            return nullptr;
        }

        // Go's FlagSet.Parse with parseOne, the arguments after the flags
        // into the positional list
        detail::FlagOutcome _parse(const std::vector<std::string_view>& args) const {
            using kind = detail::FlagOutcome::kind;
            auto fail = [](std::string m) { return detail::FlagOutcome{kind::failed, std::move(m)}; };
            size_t i = 0;
            while (i < args.size()) {
                std::string_view s = args[i];
                if (s.size() < 2 || s[0] != '-') {
                    break;
                }
                size_t minuses = 1;
                if (s[1] == '-') {
                    minuses = 2;
                    if (s.size() == 2) {
                        ++i;
                        break;
                    }
                }
                std::string_view name = s.substr(minuses);
                if (name.empty() || name[0] == '-' || name[0] == '=') {
                    return fail("bad flag syntax: " + std::string(s));
                }
                ++i;
                bool has_value = false;
                std::string_view value;
                for (size_t k = 1; k < name.size(); ++k) {
                    if (name[k] == '=') {
                        value = name.substr(k + 1);
                        has_value = true;
                        name = name.substr(0, k);
                        break;
                    }
                }
                const detail::FlagNode* f = _find(name);
                if (!f) {
                    if (name == "help" || name == "h") {
                        return detail::FlagOutcome{kind::help, {}};
                    }
                    return fail("flag provided but not defined: -" + std::string(name));
                }
                if (f->boolean) {
                    if (has_value) {
                        if (auto why = f->set(f->target, value); !why.empty()) {
                            std::string m = "invalid boolean value ";
                            detail::go_quote(m, value);
                            return fail(m + " for -" + std::string(name) + ": " + why);
                        }
                    } else {
                        (void)f->set(f->target, "true");
                    }
                    continue;
                }
                if (!has_value && i < args.size()) {
                    has_value = true;
                    value = args[i++];
                }
                if (!has_value) {
                    return fail("flag needs an argument: -" + std::string(name));
                }
                if (auto why = f->set(f->target, value); !why.empty()) {
                    std::string m = "invalid value ";
                    detail::go_quote(m, value);
                    return fail(m + " for flag -" + std::string(name) + ": " + why);
                }
            }
            if (_pos_target) {
                _pos_target->clear();
                for (; i < args.size(); ++i) {
                    _pos_target->push_back(string(args[i]));
                }
            } else if (i < args.size()) {
                return fail("unexpected argument: " + std::string(args[i]));
            }
            return {};
        }

        // "Usage of <program>:", the description, Go's PrintDefaults (the
        // flags by name; "  -name type", the help on the same line after a
        // tab for a one-letter bool, else on the next after four spaces and
        // a tab; a name in backquotes in the help as the type; the default
        // unless it is the type's zero, a string's quoted), the positional
        std::string _usage(const std::string& program) const noexcept {
            std::string out = "Usage of " + program + ":\n";
            if (!_description.empty()) {
                out.append(_description.data(), _description.size());
                out += '\n';
            }
            std::vector<const detail::FlagNode*> all;
            for (auto n = _last; n; n = n->prev) {
                all.push_back(n.get());
            }
            std::sort(all.begin(), all.end(), [](auto a, auto b) { return a->name.view() < b->name.view(); });
            for (auto f : all) {
                std::string line = "  -";
                line.append(f->name.data(), f->name.size());
                std::string help(f->help.data(), f->help.size());
                std::string type(f->type_name.data(), f->type_name.size());
                if (auto open = help.find('`'); open != std::string::npos) {   // UnquoteUsage
                    if (auto close = help.find('`', open + 1); close != std::string::npos) {
                        type = help.substr(open + 1, close - open - 1);
                        help = help.substr(0, open) + type + help.substr(close + 1);
                    }
                }
                if (!type.empty()) {
                    line += ' ';
                    line += type;
                }
                line += line.size() <= 4 ? "\t" : "\n    \t";
                for (char c : help) {
                    line += c;
                    if (c == '\n') {
                        line += "    \t";
                    }
                }
                if (f->default_text.view() != f->zero_text.view()) {
                    if (f->quoted) {
                        line += " (default ";
                        detail::go_quote(line, f->default_text.view());
                        line += ')';
                    } else {
                        line += " (default ";
                        line.append(f->default_text.data(), f->default_text.size());
                        line += ')';
                    }
                }
                out += line;
                out += '\n';
            }
            if (_pos_target) {
                out += "  ";
                out.append(_pos_name.data(), _pos_name.size());
                out += "...\n    \t";
                out.append(_pos_help.data(), _pos_help.size());
                out += '\n';
            }
            return out;
        }

        string _description;
        tracked_ptr<detail::FlagNode> _last;
        string _pos_name;
        string _pos_help;
        vector<string>* _pos_target = nullptr;
    };
}
