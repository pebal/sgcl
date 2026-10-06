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
#include <initializer_list>
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
            string short_name;     // a second name (flag_options)
            string env;            // the variable read for its default at parse
            bool required = false;
            bool list = false;     // a vector: set appends, clear empties it before a parse's first value
            void (*clear)(void* target) = nullptr;
        };

        // A subcommand: its name and its description, held by address
        struct CommandNode {
            tracked_ptr<CommandNode> prev;
            string name;
            flags* set = nullptr;
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

        // A list's element appended: read into one of its own first, so that a
        // text refused leaves the list as it was
        template<class T>
        std::string flag_append(void* target, std::string_view text) noexcept(BuiltinFlag<T> && std::is_nothrow_default_constructible_v<T>) {
            T v{};
            if (auto why = flag_set<T>(&v, text); !why.empty()) {
                return why;
            }
            static_cast<vector<T>*>(target)->push_back(std::move(v));
            return {};
        }

        template<class T>
        void flag_clear(void* target) noexcept {
            static_cast<vector<T>*>(target)->clear();
        }

        // A list as Go's fmt prints a slice: "[a b]"
        template<class T>
        std::string flag_list_text(const vector<T>& v) noexcept(BuiltinFlag<T>) {
            std::string out = "[";
            for (size_t k = 0; k < v.size(); ++k) {
                if (k) {
                    out += ' ';
                }
                out += flag_text(v[k]);
            }
            out += ']';
            return out;
        }

        // The type's part of a flag's node: its name in the usage, its
        // default and zero as text, how its text is set
        template<class T>
        tracked_ptr<FlagNode> flag_node(T& target) {
            tracked_ptr<FlagNode> node = make_tracked<FlagNode>();
            node->type_name = string(flag_type_name<T>());
            node->default_text = string(flag_text(target));
            if constexpr (std::is_default_constructible_v<T>) {
                node->zero_text = string(flag_text(T{}));
            }
            node->boolean = std::is_same_v<T, bool>;
            node->quoted = std::is_same_v<T, string>;
            node->target = static_cast<void*>(&target);
            node->set = &flag_set<T>;
            return node;
        }

        template<class T>
        tracked_ptr<FlagNode> flag_list_node(vector<T>& target) {
            tracked_ptr<FlagNode> node = make_tracked<FlagNode>();
            const char* type = flag_type_name<T>();
            node->type_name = string(*type ? type : "bool");
            node->default_text = string(flag_list_text(target));
            node->zero_text = string("[]");
            node->target = static_cast<void*>(&target);
            node->set = &flag_append<T>;
            node->boolean = std::is_same_v<T, bool>;   // -x alone appends true
            node->list = true;
            node->clear = &flag_clear<T>;
            return node;
        }

        // A node of the type of the variable an io::flag holds
        template<class T>
        tracked_ptr<FlagNode> flag_node_of(void* target) {
            return flag_node(*static_cast<T*>(target));
        }

        template<class T>
        tracked_ptr<FlagNode> flag_list_node_of(void* target) {
            return flag_list_node(*static_cast<vector<T>*>(target));
        }

        // What a parse came to: taken, help asked for, or refused with
        // Go's text
        struct FlagOutcome {
            enum class kind : uint8_t { ok, help, failed } what = kind::ok;
            std::string message;
            const flags* who = nullptr;   // the description whose usage goes with it: a command's, or the one parsed
        };
    }

    // What a flag may have beyond its name, its variable and its help
    struct flag_options {
        string short_name;        // a second name, one letter by convention: "p" beside "port" (-p 80, --p=80)
        string env;               // a variable of the environment whose value is the default when set and not empty
        bool required = false;    // the command line refused when neither it nor env gave the flag
    };

    // One flag as a value, for the forms that take a list of them
    // (io::parse_flags, the flags constructor): its name, the variable it
    // sets (a list: a vector of them), its help and its options
    class flag {
    public:
        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT flag(const string& name, T& target, const string& help, const flag_options& options = {}) noexcept
        : _name(name), _help(help), _options(options), _target(static_cast<void*>(&target)), _make(&detail::flag_node_of<T>) {
        }

        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT flag(const string& name, vector<T>& target, const string& help, const flag_options& options = {}) noexcept
        : _name(name), _help(help), _options(options), _target(static_cast<void*>(&target)), _make(&detail::flag_list_node_of<T>) {
        }

    private:
        friend class flags;

        string _name;
        string _help;
        flag_options _options;
        void* _target;
        tracked_ptr<detail::FlagNode> (*_make)(void* target);
    };

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
    //
    // Beyond Go, each taking effect only where it is used (a command line
    // Go takes means the same): a second name of a flag and the variable
    // of the environment that gives its default, or a flag required
    // (flag_options); combined one-letter bools (-vx, when no flag is
    // called vx); a list, a vector of values that every occurrence appends
    // to; subcommands, `prog [flags] serve [its flags] [its arguments]`,
    // each a description of its own held by address (add_command,
    // chosen); and the one-line forms over io::flag values.
    class flags {
    public:
        flags() noexcept = default;

        SGCL_INLINE_HOT explicit flags(const string& description) noexcept
        : _description(description) {
        }

        // The flags given at once: io::flags({{"port", port, "the port"}, ...})
        flags(std::initializer_list<flag> list) {
            for (const flag& f : list) {
                add(f);
            }
        }

        flags(const string& description, std::initializer_list<flag> list)
        : _description(description) {
            for (const flag& f : list) {
                add(f);
            }
        }

        // A flag tied to a variable: bool, an integer, a floating-point
        // number, string, duration, or a type with T::parse(const
        // string&). The variable's value now is the default the usage shows
        // A name that is empty, begins with - or holds = is
        // std::invalid_argument, and so is a name added twice (Go panics)
        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT void add(const string& name, T& target, const string& help) {
            _link(name, help, flag_options(), detail::flag_node(target));
        }

        // The same with its options: a second name, the variable of the
        // environment that gives its default, required
        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT void add(const string& name, T& target, const string& help, const flag_options& options) {
            _link(name, help, options, detail::flag_node(target));
        }

        // A list: every occurrence of the flag appends a value of T, the
        // first one of a parse replacing what the vector held (its default);
        // the environment's value is split at commas
        template<class T>
            requires detail::FlagValue<T>
        SGCL_INLINE_HOT void add(const string& name, vector<T>& target, const string& help, const flag_options& options = {}) {
            _link(name, help, options, detail::flag_list_node(target));
        }

        // A flag given as a value (io::flag)
        SGCL_INLINE_HOT void add(const flag& f) {
            _link(f._name, f._help, f._options, f._make(f._target));
        }

        // The arguments after the flags, into the vector, in order; the
        // usage lists them last, as "name..."
        SGCL_INLINE_HOT void positional(const string& name, vector<string>& target, const string& help) noexcept {
            _positional(name, target, help);
        }

        // A subcommand: the first argument after these flags that names it
        // hands the rest of the command line to `command`, a description of
        // its own (its flags, its positional list, its own commands), held
        // by address as the variables are; command.chosen() tells it was.
        // Its description is its line in the usage. A name that is empty or
        // begins with - , or one added twice, is std::invalid_argument
        void add_command(const string& name, flags& command) {
            std::string_view n = name.view();
            if (n.empty() || n[0] == '-') {
                throw std::invalid_argument("sgcl::io::flags: a command's name may be neither empty nor begin with -: " + std::string(n));
            }
            if (_find_command(n)) {
                throw std::invalid_argument("sgcl::io::flags: command redefined: " + std::string(n));
            }
            tracked_ptr<detail::CommandNode> node = make_tracked<detail::CommandNode>();
            node->prev = _commands;
            node->name = name;
            node->set = &command;
            _commands = node;
            command._path = string(std::string(_path.view()) + " " + std::string(n));
        }

        // Whether the last parse of the description this one was added to
        // chose it (a -h of it included); false for a description parsed on
        // its own
        SGCL_INLINE_HOT bool chosen() const noexcept {
            return _chosen;
        }

        // The program's command line: the variables set. -h prints the usage
        // on the standard error and ends the process with 0; a command line
        // the flags do not take prints Go's message and the usage there and
        // ends it with 2 (flag.ExitOnError). A command's -h and errors print
        // the command's usage
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
            text += (r.who ? r.who : this)->_usage(program);
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

        // The usage parse prints: "Usage of <program>:" ("Usage of
        // <program> <command>:" for a command), the description, then the
        // flags as Go's PrintDefaults writes them, the positional arguments
        // and the commands last
        SGCL_INLINE_HOT string usage() const noexcept {
            auto a = io::args();
            return string(_usage(a.empty() ? std::string("program") : std::string(a[0].data(), a[0].size())));
        }

    private:
        static void _check_name(std::string_view n) {
            if (n.empty() || n[0] == '-' || n.find('=') != std::string_view::npos) {
                throw std::invalid_argument("sgcl::io::flags: a flag's name may be neither empty nor begin with - nor hold =: " + std::string(n));
            }
        }

        void _link(const string& name, const string& help, const flag_options& options, tracked_ptr<detail::FlagNode> node) {
            _check_name(name.view());
            if (_find(name.view())) {
                throw std::invalid_argument("sgcl::io::flags: flag redefined: " + std::string(name.view()));
            }
            if (!options.short_name.empty()) {
                _check_name(options.short_name.view());
                if (_find(options.short_name.view()) || options.short_name == name) {
                    throw std::invalid_argument("sgcl::io::flags: flag redefined: " + std::string(options.short_name.view()));
                }
            }
            node->prev = _last;
            node->name = name;
            node->help = help;
            node->short_name = options.short_name;
            node->env = options.env;
            node->required = options.required;
            _last = node;
        }

        SGCL_INLINE_HOT void _positional(const string& name, vector<string>& target, const string& help) noexcept {
            _pos_name = name;
            _pos_help = help;
            _pos_target = &target;
        }

        const detail::FlagNode* _find(std::string_view name) const noexcept {
            for (auto n = _last; n; n = n->prev) {
                if (n->name.view() == name || (!n->short_name.empty() && n->short_name.view() == name)) {
                    return n.get();
                }
            }
            return nullptr;
        }

        const detail::CommandNode* _find_command(std::string_view name) const noexcept {
            for (auto c = _commands; c; c = c->prev) {
                if (c->name.view() == name) {
                    return c.get();
                }
            }
            return nullptr;
        }

        // Combined one-letter bools: -vx is -v -x when no flag is called
        // vx and every letter is a one-letter bool (a token Go refuses)
        bool _combined(std::string_view letters) const noexcept {
            if (letters.size() < 2) {
                return false;
            }
            for (char c : letters) {
                const detail::FlagNode* f = _find(std::string_view(&c, 1));
                if (!f || !f->boolean) {
                    return false;
                }
            }
            return true;
        }

        // Go's FlagSet.Parse with parseOne, the arguments after the flags
        // into the positional list or to the command they name; the
        // environment read first, the required flags checked last
        detail::FlagOutcome _parse(const std::vector<std::string_view>& args) const {
            using kind = detail::FlagOutcome::kind;
            auto fail = [this](std::string m) { return detail::FlagOutcome{kind::failed, std::move(m), this}; };
            std::vector<const detail::FlagNode*> given;   // by the environment or the command line
            auto was_given = [&given](const detail::FlagNode* f) {
                return std::find(given.begin(), given.end(), f) != given.end();
            };
            // a list's first value of the parse replaces its default
            auto set = [&](const detail::FlagNode* f, std::string_view value) {
                if (f->list && !was_given(f)) {
                    f->clear(f->target);
                }
                if (!was_given(f)) {
                    given.push_back(f);
                }
                return f->set(f->target, value);
            };
            for (auto n = _last; n; n = n->prev) {
                if (n->env.empty()) {
                    continue;
                }
                const char* v = ::getenv(n->env.c_str());
                if (!v || !*v) {
                    continue;
                }
                std::string_view text(v);
                std::vector<std::string_view> values;
                if (n->list) {
                    for (size_t at = 0;;) {
                        size_t comma = text.find(',', at);
                        values.push_back(text.substr(at, comma == std::string_view::npos ? std::string_view::npos : comma - at));
                        if (comma == std::string_view::npos) {
                            break;
                        }
                        at = comma + 1;
                    }
                } else {
                    values.push_back(text);
                }
                for (auto value : values) {
                    if (auto why = set(n.get(), value); !why.empty()) {
                        std::string m = "invalid value ";
                        detail::go_quote(m, value);
                        return fail(m + " for $" + std::string(n->env.view()) + " (flag -" + std::string(n->name.view()) + "): " + why);
                    }
                }
            }
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
                        return detail::FlagOutcome{kind::help, {}, this};
                    }
                    if (!has_value && _combined(name)) {
                        for (char c : name) {
                            (void)set(_find(std::string_view(&c, 1)), "true");
                        }
                        continue;
                    }
                    return fail("flag provided but not defined: -" + std::string(name));
                }
                if (f->boolean) {
                    if (has_value) {
                        if (auto why = set(f, value); !why.empty()) {
                            std::string m = "invalid boolean value ";
                            detail::go_quote(m, value);
                            return fail(m + " for -" + std::string(name) + ": " + why);
                        }
                    } else {
                        (void)set(f, "true");
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
                if (auto why = set(f, value); !why.empty()) {
                    std::string m = "invalid value ";
                    detail::go_quote(m, value);
                    return fail(m + " for flag -" + std::string(name) + ": " + why);
                }
            }
            for (auto n = _last; n; n = n->prev) {
                if (n->required && !was_given(n.get())) {
                    return fail("flag is required: -" + std::string(n->name.view()));
                }
            }
            for (auto c = _commands; c; c = c->prev) {
                c->set->_chosen = false;
            }
            if (_commands && i < args.size()) {
                if (const detail::CommandNode* c = _find_command(args[i])) {
                    c->set->_chosen = true;
                    std::vector<std::string_view> rest(args.begin() + ptrdiff_t(i) + 1, args.end());
                    auto r = c->set->_parse(rest);
                    if (!r.who) {
                        r.who = c->set;
                    }
                    return r;
                }
                if (!_pos_target) {
                    return fail("unknown command: " + std::string(args[i]));
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
        // unless it is the type's zero, a string's quoted), the positional;
        // beyond Go: "-s, -name" for a second name, "type..." for a list,
        // "[$VAR]" and "(required)" after the help, the commands last
        std::string _usage(const std::string& program) const noexcept {
            std::string out = "Usage of " + program + std::string(_path.view()) + ":\n";
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
                if (!f->short_name.empty()) {
                    line.append(f->short_name.data(), f->short_name.size());
                    line += ", -";
                }
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
                    if (f->list) {
                        line += "...";
                    }
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
                if (!f->env.empty()) {
                    line += " [$";
                    line.append(f->env.data(), f->env.size());
                    line += ']';
                }
                if (f->required) {
                    line += " (required)";
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
            if (_commands) {
                std::vector<const detail::CommandNode*> commands;
                for (auto c = _commands; c; c = c->prev) {
                    commands.push_back(c.get());
                }
                std::sort(commands.begin(), commands.end(), [](auto a, auto b) { return a->name.view() < b->name.view(); });
                out += "Commands:\n";
                for (auto c : commands) {
                    out += "  ";
                    out.append(c->name.data(), c->name.size());
                    out += "\n    \t";
                    for (char ch : c->set->_description.view()) {
                        out += ch;
                        if (ch == '\n') {
                            out += "    \t";
                        }
                    }
                    out += '\n';
                }
            }
            return out;
        }

        string _description;
        tracked_ptr<detail::FlagNode> _last;
        tracked_ptr<detail::CommandNode> _commands;
        string _pos_name;
        string _pos_help;
        vector<string>* _pos_target = nullptr;
        string _path;          // " serve" for a command: the usage's header
        bool _chosen = false;  // the last parse of the parent chose it
    };

    // The one-line form: the program's command line read into the flags
    // given, as flags::parse(argc, argv) reads it (the usage and 0 after
    // -h, Go's message, the usage and 2 for a command line refused):
    //   io::parse_flags(argc, argv, {{"port", port, "the port"}, {"v", verbose, "verbose"}});
    inline void parse_flags(int argc, char** argv, std::initializer_list<flag> list, const string& description = {}) {
        flags f(description, list);
        f.parse(argc, argv);
    }
}
