//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../core/utf8.h"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <ranges>
#include <string>
#include <string_view>
#include <unistd.h>

namespace sgcl::io::path {
    // Paths as strings (path/filepath): lexical operations on the text
    // of a path in the platform's form ("/" on POSIX), touching the file
    // system only where the name says so (abs, glob). Nothing here
    // allocates a path type: a string in, a string out; a literal is a
    // string.

    inline constexpr char separator = '/';
    inline constexpr char list_separator = ':';

    // The shortest equivalent: "." and ".." resolved, repeated
    // separators and a trailing one dropped, "" being "."
    inline string clean(const string& path) {
        std::string_view p(path);
        if (p.empty()) {
            return string(".");
        }
        bool rooted = p.front() == separator;
        std::string out;
        out.reserve(p.size());
        size_t n = p.size();
        size_t r = 0;          // the next byte of p to read
        size_t dotdot = 0;     // out may not shrink past here (a leading ".." or the root)
        if (rooted) {
            out += separator;
            r = 1;
            dotdot = 1;
        }
        while (r < n) {
            if (p[r] == separator) {
                ++r;
            } else if (p[r] == '.' && (r + 1 == n || p[r + 1] == separator)) {
                ++r;
            } else if (p[r] == '.' && p[r + 1] == '.' && (r + 2 == n || p[r + 2] == separator)) {
                r += 2;
                if (out.size() > dotdot) {
                    // back up to the previous separator
                    size_t w = out.size() - 1;
                    while (w > dotdot && out[w] != separator) {
                        --w;
                    }
                    out.resize(w);
                } else if (!rooted) {
                    if (!out.empty()) {
                        out += separator;
                    }
                    out += "..";
                    dotdot = out.size();
                }
            } else {
                if ((rooted && out.size() != 1) || (!rooted && !out.empty())) {
                    out += separator;
                }
                while (r < n && p[r] != separator) {
                    out += p[r++];
                }
            }
        }
        if (out.empty()) {
            return string(".");
        }
        return string(out);
    }

    // The elements joined with the separator and cleaned; empty
    // elements skipped; a range of strings, or an initializer list
    template<class R>
    requires std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, const string&>
    string join(const R& elements) {
        std::string out;
        for (const string& e : elements) {
            if (e.empty()) {
                continue;
            }
            if (!out.empty()) {
                out += separator;
            }
            out.append(e.data(), e.size());
        }
        if (out.empty()) {
            return string();
        }
        return clean(string(out));
    }

    inline string join(std::initializer_list<string> elements) {
        return join<std::initializer_list<string>>(elements);
    }

    template<class... S>
    string join(const string& first, const S&... rest) {
        return join({first, string(rest)...});
    }

    // The last element ("" and "/" give themselves); everything but the
    // last element, cleaned; the extension from the last dot ("" when
    // none); the name without it
    inline string base(const string& path) {
        std::string_view p(path);
        if (p.empty()) {
            return string();
        }
        while (p.size() > 1 && p.back() == separator) {
            p.remove_suffix(1);
        }
        if (p == "/") {
            return string("/");
        }
        auto i = p.rfind(separator);
        if (i != std::string_view::npos) {
            p.remove_prefix(i + 1);
        }
        return string(p);
    }

    inline string dir(const string& path) {
        std::string_view p(path);
        auto i = p.rfind(separator);
        if (i == std::string_view::npos) {
            return string(".");
        }
        return clean(string(p.substr(0, i + 1)));
    }

    inline string ext(const string& path) {
        std::string_view p(path);
        for (size_t i = p.size(); i-- > 0 && p[i] != separator;) {
            if (p[i] == '.') {
                return string(p.substr(i));
            }
        }
        return string();
    }

    inline string stem(const string& path) {
        auto b = base(path);
        std::string_view v(b);
        for (size_t i = v.size(); i-- > 0;) {
            if (v[i] == '.') {
                return string(v.substr(0, i));
            }
        }
        return b;
    }

    // dir and base, as a pair: the directory with its trailing separator
    // as written, the file after it (Go's Split)
    inline pair<string, string> split(const string& path) {
        std::string_view p(path);
        auto i = p.rfind(separator);
        if (i == std::string_view::npos) {
            return {string(), string(p)};
        }
        return {string(p.substr(0, i + 1)), string(p.substr(i + 1))};
    }

    // The elements of a PATH-like list, empty ones skipped
    inline vector<string> split_list(const string& path_list) {
        std::string_view list(path_list);
        vector<string> out;
        size_t pos = 0;
        while (pos <= list.size()) {
            size_t next = list.find(list_separator, pos);
            if (next == std::string_view::npos) {
                next = list.size();
            }
            if (next > pos) {
                out.push_back(string(list.substr(pos, next - pos)));
            }
            pos = next + 1;
        }
        return out;
    }

    inline bool is_abs(const string& p) noexcept {
        return !p.empty() && p.front() == separator;
    }

    // The absolute form: the working directory joined when relative,
    // cleaned
    inline expected<string, error> abs(const string& p) {
        if (is_abs(p)) {
            return clean(p);
        }
        char buf[4096];
        if (!::getcwd(buf, sizeof buf)) {
            return io::detail::fail(last_error("getcwd"));
        }
        return join(string(buf), p);
    }

    // The path from base to target with ".." where needed, both cleaned
    // first; an error when it cannot be done lexically (one absolute,
    // the other relative)
    inline expected<string, error> rel(const string& base_path, const string& target) {
        auto b = clean(base_path);
        auto t = clean(target);
        std::string_view bv(b), tv(t);
        if (bv == tv) {
            return string(".");
        }
        if (bv == ".") {
            bv = "";
        }
        if (tv == ".") {
            tv = "";
        }
        if (is_abs(bv) != is_abs(tv)) {
            return io::detail::fail(error(errc::invalid_path, "rel", target));
        }
        // the common prefix of whole elements
        size_t bi = 0, ti = 0;
        for (;;) {
            size_t be = bv.find(separator, bi);
            size_t te = tv.find(separator, ti);
            if (be == std::string_view::npos) be = bv.size();
            if (te == std::string_view::npos) te = tv.size();
            if (bv.substr(bi, be - bi) != tv.substr(ti, te - ti)) {
                break;
            }
            bi = be < bv.size() ? be + 1 : be;
            ti = te < tv.size() ? te + 1 : te;
            if (bi >= bv.size() && ti >= tv.size()) {
                break;
            }
            if (bi >= bv.size() || ti >= tv.size()) {
                break;
            }
        }
        std::string_view brest = bi < bv.size() ? bv.substr(bi) : std::string_view();
        std::string_view trest = ti < tv.size() ? tv.substr(ti) : std::string_view();
        if (brest == "..") {
            return io::detail::fail(error(errc::invalid_path, "rel", target));
        }
        std::string out;
        if (!brest.empty()) {
            size_t ups = 1 + std::count(brest.begin(), brest.end(), separator);
            for (size_t i = 0; i < ups; ++i) {
                if (!out.empty()) {
                    out += separator;
                }
                out += "..";
            }
        }
        if (!trest.empty()) {
            if (!out.empty()) {
                out += separator;
            }
            out.append(trest.data(), trest.size());
        }
        return string(out);
    }

    namespace detail {
        // One element of a pattern against one of a name: the shell's
        // rules ('*' any run without a separator, '?' one character,
        // '[...]' a class with ranges and a leading '^' or '!' negation,
        // '\' escaping the next character), characters being UTF-8 code
        // points; nullopt for a malformed pattern
        inline optional<bool> match_element(std::string_view pat, std::string_view name) {
            size_t p = 0, n = 0;
            size_t star_p = std::string_view::npos, star_n = 0;
            while (n < name.size() || p < pat.size()) {
                if (p < pat.size()) {
                    char c = pat[p];
                    if (c == '*') {
                        star_p = ++p;
                        star_n = n;
                        continue;
                    }
                    if (n < name.size()) {
                        auto [nc, nl] = utf8::decode(name, n);
                        if (c == '?') {
                            ++p;
                            n += nl;
                            continue;
                        }
                        if (c == '[') {
                            size_t q = p + 1;
                            bool negate = false;
                            if (q < pat.size() && (pat[q] == '^' || pat[q] == '!')) {
                                negate = true;
                                ++q;
                            }
                            bool matched = false;
                            bool any = false;
                            for (;;) {
                                if (q >= pat.size()) {
                                    return nullopt;
                                }
                                if (pat[q] == ']' && any) {
                                    break;
                                }
                                auto range_end = [&](size_t& at) -> optional<char32_t> {
                                    if (pat[at] == '\\') {
                                        if (++at >= pat.size()) {
                                            return nullopt;
                                        }
                                    }
                                    auto [cp, len] = utf8::decode(pat, at);
                                    at += len;
                                    return cp;
                                };
                                auto lo = range_end(q);
                                if (!lo) {
                                    return nullopt;
                                }
                                char32_t hi = *lo;
                                if (q + 1 < pat.size() && pat[q] == '-' && pat[q + 1] != ']') {
                                    ++q;
                                    auto h = range_end(q);
                                    if (!h) {
                                        return nullopt;
                                    }
                                    hi = *h;
                                }
                                if (*lo > hi) {
                                    return nullopt;
                                }
                                if (*lo <= nc && nc <= hi) {
                                    matched = true;
                                }
                                any = true;
                            }
                            if (matched != negate) {
                                p = q + 1;
                                n += nl;
                                continue;
                            }
                        } else {
                            if (c == '\\') {
                                if (++p >= pat.size()) {
                                    return nullopt;
                                }
                            }
                            auto [pc, pl] = utf8::decode(pat, p);
                            // An invalid byte decodes as U+FFFD on both sides:
                            // it matches only the same byte, not another one
                            if (pc == nc && pl == nl && (pc != utf8::replacement || pl != 1 || pat[p] == name[n])) {
                                p += pl;
                                n += nl;
                                continue;
                            }
                        }
                    }
                }
                if (star_p != std::string_view::npos && star_n < name.size()) {
                    p = star_p;
                    star_n += utf8::decode(name, star_n).second;
                    n = star_n;
                    continue;
                }
                return false;
            }
            return true;
        }

        // Whether the pattern is well formed: every '\\' escapes a
        // character, every class closes and its ranges are ordered
        inline bool valid_pattern(std::string_view pat) noexcept {
            for (size_t p = 0; p < pat.size();) {
                if (pat[p] == '\\') {
                    if (++p >= pat.size()) {
                        return false;
                    }
                    ++p;
                } else if (pat[p] == '[') {
                    size_t q = p + 1;
                    if (q < pat.size() && (pat[q] == '^' || pat[q] == '!')) {
                        ++q;
                    }
                    bool any = false;
                    for (;;) {
                        if (q >= pat.size()) {
                            return false;
                        }
                        if (pat[q] == ']' && any) {
                            break;
                        }
                        auto one = [&](size_t& at) -> optional<char32_t> {
                            if (pat[at] == '\\' && ++at >= pat.size()) {
                                return nullopt;
                            }
                            auto [cp, len] = utf8::decode(pat, at);
                            at += len;
                            return cp;
                        };
                        auto lo = one(q);
                        if (!lo) {
                            return false;
                        }
                        if (q + 1 < pat.size() && pat[q] == '-' && pat[q + 1] != ']') {
                            ++q;
                            auto hi = one(q);
                            if (!hi || *lo > *hi) {
                                return false;
                            }
                        }
                        any = true;
                    }
                    p = q + 1;
                } else {
                    ++p;
                }
            }
            return true;
        }

        inline bool has_meta(std::string_view s) noexcept {
            return s.find_first_of("*?[\\") != std::string_view::npos;
        }

        inline void glob_in(const string& dir, const string& pattern, vector<string>& out);

        // The paths matching pattern, whose directory part may hold
        // patterns itself
        inline void glob_walk(const string& pattern, vector<string>& out) {
            auto [d, file] = split(pattern);
            std::string_view dv(d);
            while (dv.size() > 1 && dv.back() == separator) {
                dv.remove_suffix(1);
            }
            if (dv.empty()) {
                glob_in(string("."), file, out);
                return;
            }
            if (!has_meta(dv)) {
                glob_in(string(dv), file, out);
                return;
            }
            vector<string> dirs;
            glob_walk(string(dv), dirs);
            for (auto& sub : dirs) {
                glob_in(sub, file, out);
            }
        }

        inline void glob_in(const string& dir, const string& pattern, vector<string>& out) {
            namespace fs = std::filesystem;
            if (!has_meta(pattern)) {
                auto p = dir == "." && pattern.find(separator) == string::npos && !pattern.empty() ? pattern : join(dir, pattern);
                error_code ec;
                if (fs::exists(fs::path(p.str()), ec)) {
                    out.push_back(std::move(p));
                }
                return;
            }
            error_code ec;
            fs::directory_iterator it(fs::path(dir.str()), ec);
            if (ec) {
                return;
            }
            vector<string> names;
            for (; it != fs::directory_iterator(); it.increment(ec)) {
                if (ec) {
                    return;
                }
                string name(it->path().filename().native());
                if (match_element(pattern, name).value_or(false)) {
                    names.push_back(dir == "." ? name : join(dir, name));
                }
            }
            std::sort(names.begin(), names.end());
            for (auto& n : names) {
                out.push_back(std::move(n));
            }
        }
    }

    // Shell pattern matching on the whole name, element by element ('*'
    // and '?' never match a separator): '*' any run, '?' one character,
    // '[a-z]' a class, '[^a-z]' its negation, '\' an escape;
    // errc::invalid_pattern for a malformed pattern
    inline expected<bool, error> match(const string& pattern_text, const string& name_text) {
        std::string_view pattern(pattern_text), name(name_text);
        if (!detail::valid_pattern(pattern)) {
            return io::detail::fail(error(errc::invalid_pattern, "match", pattern_text));
        }
        for (;;) {
            size_t pe = pattern.find(separator);
            size_t ne = name.find(separator);
            auto pel = pattern.substr(0, pe);
            auto nel = name.substr(0, ne);
            auto m = detail::match_element(pel, nel);
            if (!m) {
                return io::detail::fail(error(errc::invalid_pattern, "match", pattern));
            }
            if (!*m) {
                return false;
            }
            if (pe == std::string_view::npos || ne == std::string_view::npos) {
                return pe == ne;
            }
            pattern.remove_prefix(pe + 1);
            name.remove_prefix(ne + 1);
        }
    }

    // The paths that match the pattern, sorted within each directory;
    // a directory that cannot be read is skipped; a pattern without
    // meta characters names the file if it exists
    inline expected<vector<string>, error> glob(const string& pattern) {
        auto m = match(pattern, string());
        if (!m) {
            return io::detail::fail(m);
        }
        vector<string> out;
        detail::glob_walk(pattern, out);
        return out;
    }

    // Separators converted to and from "/", the form of URLs and archives
    inline string from_slash(const string& p) {
        return p;
    }

    inline string to_slash(const string& p) {
        return p;
    }
}
