//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "stream.h"
#include "../txt/format.h"

#include <type_traits>

// Text written in one call: println("{} items", n) is
// io::stdout.write(txt::format("{} items\n", n)), print the same without
// the new line, eprint and eprintln the same on io::stderr, and each with a
// writer first (a file, a connection, a buffer) for any other stream.
// The pattern is txt::format's, read by the compiler (a brace left open,
// a value that does not take its field: an error of the build); a pattern
// that arrives while the program runs goes through txt::runtime(), and a
// pattern that does not fit its values then prints nothing and says so.
// One value needs no pattern: println(n), println(name); a literal is
// always the pattern.
//
// Printing is not checked: a line on a terminal that could not be written
// has nobody to tell, and a result every call would have to drop is noise.
// A program that must know (a pipe closed under it) writes with
// io::stdout.write(...), which answers an expected. The streams stay what
// they are — bytes, io::copy, a buffered writer, async_write in a task —
// and print is only the short way to put text on them.
//
// The functions are io's and are also names of sgcl itself, so that
// `using namespace sgcl;` is all a program needs for println.
namespace sgcl::io {
    namespace detail {
        inline void print_text(const io::writer& w, const string& text) {
            (void)w.write(io::detail::bytes_of(text));
        }

        // The text of the pattern and a new line, one string made once: the
        // first pass on the stack says the size, and the text is copied from
        // there (or, when it did not fit, written again straight into the
        // string) with the '\n' after it (format(...) + "\n" made two)
        template<class... A>
        string line_of(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
            char room[256];
            const size_t n = txt::format_to(slice<char>(room, sizeof room), pattern, args...);
            return sgcl::detail::StringAccess::filled<string>(n + 1, [&](char* chars) {
                if (n <= sizeof room) {
                    sgcl::detail::copy_bytes(chars, room, n);
                } else {
                    (void)txt::format_to(slice<char>(chars, n), pattern, args...);
                }
                chars[n] = '\n';
            });
        }

        // The same for a pattern read while the program runs: nullopt when
        // it does not fit its values
        template<class... A>
        optional<string> line_of(const txt::runtime_pattern& pattern, const A&... args) {
            char room[256];
            auto n = txt::format_to(slice<char>(room, sizeof room), pattern, args...);
            if (!n) {
                return nullopt;
            }
            const size_t k = *n;
            return sgcl::detail::StringAccess::filled<string>(k + 1, [&](char* chars) {
                if (k <= sizeof room) {
                    sgcl::detail::copy_bytes(chars, room, k);
                } else {
                    (void)txt::format_to(slice<char>(chars, k), pattern, args...);
                }
                chars[k] = '\n';
            });
        }

        // A value println takes alone: anything "{}" formats, a string or
        // a text slice included, but not a literal, which is always the
        // pattern (println("done") reads it as one, braces and all)
        template<class T>
        concept Printable = requires(const T& v) { txt::format("{}", v); }
                         && (!std::is_array_v<T>)
                         && (!std::is_convertible_v<const T&, io::writer>);
    }

    // The text of the pattern and its values on io::stdout
    template<class... A>
    void print(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(io::stdout, txt::format(pattern, args...));
    }

    // The same and a new line
    template<class... A>
    void println(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(io::stdout, detail::line_of(pattern, args...));
    }

    // A new line alone
    inline void println() {
        detail::print_text(io::stdout, "\n");
    }

    // One value, as "{}" formats it: println(n), print(when)
    template<detail::Printable T>
    void print(const T& value) {
        print("{}", value);
    }

    template<detail::Printable T>
    void println(const T& value) {
        println("{}", value);
    }

    // On io::stderr
    template<class... A>
    void eprint(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(io::stderr, txt::format(pattern, args...));
    }

    template<class... A>
    void eprintln(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(io::stderr, detail::line_of(pattern, args...));
    }

    inline void eprintln() {
        detail::print_text(io::stderr, "\n");
    }

    template<detail::Printable T>
    void eprint(const T& value) {
        eprint("{}", value);
    }

    template<detail::Printable T>
    void eprintln(const T& value) {
        eprintln("{}", value);
    }

    // On any stream: a file, a connection, a buffer, a buffered writer
    template<class... A>
    void print(const io::writer& to, const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(to, txt::format(pattern, args...));
    }

    template<class... A>
    void println(const io::writer& to, const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        detail::print_text(to, detail::line_of(pattern, args...));
    }

    // A pattern read while the program runs (a translation): printed when
    // it fits its values, nothing when it does not; which one, the result
    // says
    template<class... A>
    bool print(const txt::runtime_pattern& pattern, const A&... args) {
        auto text = txt::format(pattern, args...);
        if (text) {
            detail::print_text(io::stdout, *text);
        }
        return text.has_value();
    }

    template<class... A>
    bool println(const txt::runtime_pattern& pattern, const A&... args) {
        auto text = detail::line_of(pattern, args...);
        if (text) {
            detail::print_text(io::stdout, *text);
        }
        return text.has_value();
    }
}

namespace sgcl {
    using io::eprint;
    using io::eprintln;
    using io::print;
    using io::println;
}
