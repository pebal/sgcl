//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../url.h"
#include "../../../core/aliases.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/detail/small_vector.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// The patterns of the server's routes, in the syntax of Go 1.22's ServeMux
// (written from its documentation): "[METHOD ][HOST]/PATH", where a path
// segment is a literal, {name} (one segment, not empty), {name...} (the
// rest of the path, last), or {$} (the end of a path that ends in a '/',
// last), and a path ending in '/' matches everything under it.
//
// Precedence as Go documents it: of the patterns that match a request the
// most specific wins, P1 being more specific than P2 when P1 matches a
// strict subset of P2's requests; two patterns of which neither is more
// specific and which match a request in common conflict, which is found
// when the second is registered (invalid_argument: a broken program, as a
// panic in Go). One exception, Go's too: of two that would conflict, the
// one with a host wins over the one without. GET matches HEAD as well.
namespace sgcl::net::http::detail {
    struct RouteSegment {
        enum Kind : uint8_t {
            literal,    // text, unescaped
            wild,       // {name}: one segment, not empty
            multi,      // {name...} or a trailing '/': one segment or more, the last possibly empty
            end         // {$}: the empty last segment of a path ending in '/'
        };
        Kind kind = literal;
        std::string text;   // the literal, or the name ("" for a trailing '/')
        uint32_t name = 0;  // a wildcard's: its name in RouteTable's names, made once
    };

    struct RoutePattern {
        std::string text;
        std::string method;     // "" for any
        std::string host;       // "" for any
        std::vector<RouteSegment> segments;

        bool trailing_slash() const noexcept {
            return !segments.empty() && segments.back().kind == RouteSegment::multi && segments.back().text.empty();
        }
    };

    enum class Relation : uint8_t {
        equivalent, more_specific, more_general, overlaps, disjoint
    };

    inline Relation combine(Relation a, Relation b) noexcept {
        if (a == Relation::disjoint || b == Relation::disjoint) {
            return Relation::disjoint;
        }
        if (a == Relation::equivalent) {
            return b;
        }
        if (b == Relation::equivalent) {
            return a;
        }
        if (a == Relation::overlaps || b == Relation::overlaps) {
            return Relation::overlaps;
        }
        return a == b ? a : Relation::overlaps;
    }

    inline bool route_identifier(std::string_view s) noexcept {
        if (s.empty() || (s[0] >= '0' && s[0] <= '9')) {
            return false;
        }
        for (char c : s) {
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
            if (!ok) {
                return false;
            }
        }
        return true;
    }

    // A pattern read, or invalid_argument with the reason
    inline RoutePattern parse_route(std::string_view text) {
        auto bad = [&](const char* why) {
            return invalid_argument(std::string("http::server: the pattern \"") + std::string(text) + "\" " + why);
        };
        RoutePattern p;
        p.text = std::string(text);
        std::string_view rest = text;
        auto slash = rest.find('/');
        auto space = rest.find_first_of(" \t");
        if (space != std::string_view::npos && (slash == std::string_view::npos || space < slash)) {
            p.method = std::string(rest.substr(0, space));
            if (!is_token(p.method)) {
                throw bad("has a method that is not a token");
            }
            rest.remove_prefix(space);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                rest.remove_prefix(1);
            }
            slash = rest.find('/');
        }
        if (slash == std::string_view::npos) {
            throw bad("has no path (a pattern is \"[METHOD ][HOST]/PATH\")");
        }
        for (char c : rest.substr(0, slash)) {
            p.host.push_back(ascii_lower(c));
        }
        std::string_view path = rest.substr(slash + 1);
        std::vector<std::string> names;
        if (path.empty()) {
            p.segments.push_back({RouteSegment::multi, ""});
            return p;
        }
        if (path == "{$}") {
            p.segments.push_back({RouteSegment::end, ""});
            return p;
        }
        size_t from = 0;
        for (;;) {
            auto next = path.find('/', from);
            bool last = next == std::string_view::npos;
            std::string_view part = path.substr(from, last ? std::string_view::npos : next - from);
            if (part.empty()) {
                if (!last) {
                    throw bad("has an empty segment");
                }
                break;   // unreachable: a trailing '/' is handled below
            }
            if (!p.segments.empty() && (p.segments.back().kind == RouteSegment::multi || p.segments.back().kind == RouteSegment::end)) {
                throw bad("has a segment after {...} or {$}");
            }
            if (part.front() == '{') {
                if (part.back() != '}' || part.size() < 3) {
                    throw bad("has a wildcard that is not a whole segment");
                }
                auto inner = part.substr(1, part.size() - 2);
                if (inner == "$") {
                    throw bad("has {$} other than as the last segment after a '/'");
                }
                RouteSegment s;
                if (inner.size() > 3 && inner.substr(inner.size() - 3) == "...") {
                    if (!last) {
                        throw bad("has {name...} before the end");
                    }
                    s.kind = RouteSegment::multi;
                    inner.remove_suffix(3);
                } else {
                    s.kind = RouteSegment::wild;
                }
                if (!route_identifier(inner)) {
                    throw bad("has a wildcard whose name is not an identifier");
                }
                for (auto& n : names) {
                    if (n == inner) {
                        throw bad("names a wildcard twice");
                    }
                }
                names.push_back(std::string(inner));
                s.text = std::string(inner);
                p.segments.push_back(std::move(s));
            } else {
                if (part.find('{') != std::string_view::npos || part.find('}') != std::string_view::npos) {
                    throw bad("has a wildcard that is not a whole segment");
                }
                p.segments.push_back({RouteSegment::literal, net::detail::url_unescape(part)});
            }
            if (last) {
                break;
            }
            from = next + 1;
            if (from == path.size()) {
                // a trailing '/': everything under it
                p.segments.push_back({RouteSegment::multi, ""});
                break;
            }
            if (path.substr(from) == "{$}") {
                p.segments.push_back({RouteSegment::end, ""});
                break;
            }
        }
        return p;
    }

    inline Relation compare_methods(const std::string& a, const std::string& b) noexcept {
        if (a == b) {
            return Relation::equivalent;
        }
        if (a.empty()) {
            return Relation::more_general;
        }
        if (b.empty()) {
            return Relation::more_specific;
        }
        if (a == "GET" && b == "HEAD") {
            return Relation::more_general;
        }
        if (a == "HEAD" && b == "GET") {
            return Relation::more_specific;
        }
        return Relation::disjoint;
    }

    inline Relation compare_segments(const RouteSegment& a, const RouteSegment& b) noexcept {
        using K = RouteSegment::Kind;
        if (a.kind == K::end || b.kind == K::end) {
            return a.kind == b.kind ? Relation::equivalent : Relation::disjoint;
        }
        if (a.kind == K::wild && b.kind == K::wild) {
            return Relation::equivalent;
        }
        if (a.kind == K::wild) {
            return Relation::more_general;
        }
        if (b.kind == K::wild) {
            return Relation::more_specific;
        }
        return a.text == b.text ? Relation::equivalent : Relation::disjoint;
    }

    inline Relation compare_paths(const RoutePattern& p, const RoutePattern& q) noexcept {
        Relation r = Relation::equivalent;
        size_t n = std::min(p.segments.size(), q.segments.size());
        for (size_t i = 0; i < n; ++i) {
            auto& a = p.segments[i];
            auto& b = q.segments[i];
            bool am = a.kind == RouteSegment::multi, bm = b.kind == RouteSegment::multi;
            if (am && bm) {
                return combine(r, Relation::equivalent);
            }
            if (am) {
                return combine(r, Relation::more_general);
            }
            if (bm) {
                return combine(r, Relation::more_specific);
            }
            r = combine(r, compare_segments(a, b));
            if (r == Relation::disjoint) {
                return r;
            }
        }
        if (p.segments.size() != q.segments.size()) {
            return Relation::disjoint;
        }
        return r;
    }

    inline Relation compare_routes(const RoutePattern& p, const RoutePattern& q) noexcept {
        auto m = compare_methods(p.method, q.method);
        if (m == Relation::disjoint) {
            return m;
        }
        return combine(m, compare_paths(p, q));
    }

    // The segments of an escaped path, each unescaped: "/" is [""], "/a/"
    // is ["a", ""], as views, with no allocation of the system's: a
    // segment without '%' is a view of the path itself, one with '%' is
    // unescaped into a buffer of the object (512 bytes; past them into one
    // managed block the size of the path); 32 segments are held in place,
    // more in one block of plain memory (core/detail/small_vector.h: the
    // views keep nothing alive, the destructor frees it). The path
    // outlives the object (a view of the
    // request's head); the object is not copied or moved (its views may be
    // of its own buffer). with_empty_end() is the same path with one empty
    // segment more (the path with a '/' added), a view of this one.
    class PathSegments {
    public:
        static constexpr size_t Inline = 32;
        static constexpr size_t Buffer = 512;

        explicit PathSegments(std::string_view path) noexcept {
            size_t n = 1;
            if (!path.empty() && path.front() == '/') {
                n = size_t(std::count(path.begin() + 1, path.end(), '/')) + 1;
            }
            _views.resize(n + 1);   // one more for with_empty_end; no growth after it, so the views keep their place
            if (path.empty() || path.front() != '/') {
                _views[_n++] = _segment(path, path.size());
                return;
            }
            path.remove_prefix(1);
            for (;;) {
                auto slash = path.find('/');
                _views[_n++] = _segment(path.substr(0, slash), path.size());
                if (slash == std::string_view::npos) {
                    break;
                }
                path.remove_prefix(slash + 1);
            }
        }

        PathSegments(const PathSegments&) = delete;
        PathSegments& operator=(const PathSegments&) = delete;

        size_t size() const noexcept {
            return _n;
        }

        std::string_view operator[](size_t i) const noexcept {
            return _views[i];
        }

        // The segments with one empty segment more at the end
        struct List {
            const std::string_view* views;
            size_t n;

            size_t size() const noexcept {
                return n;
            }

            std::string_view operator[](size_t i) const noexcept {
                return i < n ? views[i] : std::string_view();
            }
        };

        List list() const noexcept {
            return List{_views.data(), _n};
        }

        List with_empty_end() noexcept {
            _views[_n] = std::string_view();   // room kept for it (n + 1)
            return List{_views.data(), _n + 1};
        }

    private:
        // One segment: itself when nothing in it is escaped, else unescaped
        // (the standard's percent-decode, as url_unescape) into the buffer
        std::string_view _segment(std::string_view s, size_t rest) noexcept {
            if (s.find('%') == std::string_view::npos) {
                return s;
            }
            char* out;
            if (_used + s.size() <= Buffer) {
                out = _buffer + _used;
            } else {
                if (_spill.empty()) {
                    _spill.resize(rest + 1);   // what is left of the path: nothing unescaped is longer
                }
                out = _spill.data() + _spilled;
            }
            size_t k = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '%' && i + 2 < s.size()) {
                    int hi = net::detail::url_hex(uint8_t(s[i + 1]));
                    int lo = net::detail::url_hex(uint8_t(s[i + 2]));
                    if (hi >= 0 && lo >= 0) {
                        out[k++] = char(hi * 16 + lo);
                        i += 2;
                        continue;
                    }
                }
                out[k++] = s[i];
            }
            if (out == _buffer + _used) {
                _used += k;
            } else {
                _spilled += k;
            }
            return std::string_view(out, k);
        }

        sgcl::detail::SmallVector<std::string_view, Inline + 1> _views;   // n + 1 of them, the last for with_empty_end
        size_t _n = 0;
        char _buffer[Buffer];
        size_t _used = 0;
        vector<char> _spill;             // managed: unescaped bytes past the buffer
        size_t _spilled = 0;
    };

    // Whether the path matches the pattern's, and the values of its wildcards
    inline bool match_path(const RoutePattern& p, const PathSegments::List& segs, vector<pair<string, string>>* values, const vector<string>* names = nullptr) noexcept {
        // a wildcard's name: the table's string, a word copied (one made
        // per value per request before)
        auto name_of = [&](const RouteSegment& s) {
            return names ? (*names)[s.name] : string(std::string_view(s.text));
        };
        for (size_t i = 0; i < p.segments.size(); ++i) {
            auto& s = p.segments[i];
            if (s.kind == RouteSegment::multi) {
                if (i >= segs.size()) {
                    return false;
                }
                if (values && !s.text.empty()) {
                    // the rest of the path joined, on the stack (a longer
                    // one in a managed buffer), then the value's string
                    size_t length = 0;
                    for (size_t k = i; k < segs.size(); ++k) {
                        length += segs[k].size() + (k > i);
                    }
                    char joined[512];
                    vector<char> long_join;
                    char* out = joined;
                    if (length > sizeof joined) {
                        long_join.resize(length);
                        out = long_join.data();
                    }
                    size_t at = 0;
                    for (size_t k = i; k < segs.size(); ++k) {
                        if (k > i) {
                            out[at++] = '/';
                        }
                        sgcl::detail::copy_bytes(out + at, segs[k].data(), segs[k].size());
                        at += segs[k].size();
                    }
                    values->push_back(pair<string, string>(name_of(s), string(std::string_view(out, at))));
                }
                return true;
            }
            if (i >= segs.size()) {
                return false;
            }
            switch (s.kind) {
                case RouteSegment::literal:
                    if (segs[i] != s.text) {
                        return false;
                    }
                    break;
                case RouteSegment::wild:
                    if (segs[i].empty()) {
                        return false;
                    }
                    if (values) {
                        values->push_back(pair<string, string>(name_of(s), string(segs[i])));
                    }
                    break;
                case RouteSegment::end:
                    if (!segs[i].empty() || i + 1 != segs.size()) {
                        return false;
                    }
                    break;
                case RouteSegment::multi:
                    break;
            }
        }
        return p.segments.size() == segs.size();
    }

    inline bool match_method(const std::string& pattern, std::string_view method) noexcept {
        return pattern.empty() || pattern == method || (pattern == "GET" && method == "HEAD");
    }

    // The routes without their handlers: the patterns, the conflicts, the choice
    class RouteTable {
    public:
        // The pattern added, or invalid_argument for a conflict with one
        // already there; its index
        size_t add(std::string_view text) {
            auto p = parse_route(text);
            for (auto& q : _patterns) {
                if (p.host != q.host) {
                    continue;   // two hosts: disjoint; a host and none: the host wins
                }
                auto r = compare_routes(p, q);
                if (r == Relation::equivalent || r == Relation::overlaps) {
                    throw invalid_argument("http::server: the pattern \"" + p.text + "\" conflicts with \"" + q.text + "\"");
                }
            }
            for (auto& s : p.segments) {
                if ((s.kind == RouteSegment::wild || s.kind == RouteSegment::multi) && !s.text.empty()) {
                    s.name = uint32_t(_names.size());
                    _names.push_back(string(std::string_view(s.text)));
                }
            }
            _patterns.push_back(std::move(p));
            return _patterns.size() - 1;
        }

        struct Found {
            enum Kind : uint8_t { route, not_found, method_not_allowed, redirect } kind = not_found;
            size_t index = 0;
            vector<pair<string, string>> values;   // managed: the request's path values as they are
            std::string allow;          // 405: the methods that would do
            std::string location;       // 307: the path to go to
        };

        // The route of a request, as ServeMux finds it: a path with an
        // empty segment inside is redirected to the path without it (the
        // URL parser has resolved "." and ".." already); the most specific
        // pattern that matches serves, unless it matched through a multi
        // wildcard and the path with a '/' added matches exactly, which is a
        // redirect there (the subtree named without its slash); failing a
        // match, the methods the path would match with make a 405
        Found find(std::string_view method, std::string_view host, std::string_view path) const noexcept {
            Found f;
            PathSegments parts(path);
            auto segs = parts.list();
            for (size_t i = 0; i + 1 < segs.size(); ++i) {
                if (segs[i].empty()) {
                    std::string clean;
                    for (size_t k = 0; k < path.size(); ++k) {
                        if (path[k] == '/' && !clean.empty() && clean.back() == '/') {
                            continue;
                        }
                        clean += path[k];
                    }
                    f.kind = Found::redirect;
                    f.location = clean;
                    return f;
                }
            }
            // the host lower-cased without its port, on the stack (a longer
            // one than the buffer, which no DNS name is, in a string)
            char lowered[256];
            std::string long_host;
            std::string_view h;
            if (host.size() <= sizeof lowered) {
                for (size_t k = 0; k < host.size(); ++k) {
                    lowered[k] = ascii_lower(host[k]);
                }
                h = std::string_view(lowered, host.size());
            } else {
                for (char c : host) {
                    long_host.push_back(ascii_lower(c));
                }
                h = long_host;
            }
            if (!h.empty() && h.front() != '[') {
                auto colon = h.rfind(':');
                if (colon != std::string_view::npos) {
                    h = h.substr(0, colon);
                }
            } else if (!h.empty()) {
                auto close = h.find(']');
                if (close != std::string_view::npos) {
                    h = h.substr(0, close + 1);
                }
            }
            bool slashless = !path.empty() && path.back() != '/';
            auto more = parts.with_empty_end();
            auto best = _best(method, h, segs);
            if (slashless && (!best || !_exact(_patterns[*best], segs))) {
                auto sub = _best(method, h, more);
                if (sub && _exact(_patterns[*sub], more)) {
                    f.kind = Found::redirect;
                    f.location = std::string(path) + "/";
                    return f;
                }
            }
            if (best) {
                f.kind = Found::route;
                f.index = *best;
                match_path(_patterns[*best], segs, &f.values, &_names);
                return f;
            }
            std::vector<std::string> methods;
            auto add = [&](const std::string& m) {
                for (auto& x : methods) {
                    if (x == m) {
                        return;
                    }
                }
                methods.push_back(m);
            };
            for (auto& p : _patterns) {
                if (!p.host.empty() && p.host != h) {
                    continue;
                }
                if (match_path(p, segs, nullptr) || (slashless && match_path(p, more, nullptr) && _exact(p, more))) {
                    if (p.method.empty()) {
                        continue;
                    }
                    add(p.method);
                    if (p.method == "GET") {
                        add("HEAD");
                    }
                }
            }
            if (!methods.empty()) {
                std::sort(methods.begin(), methods.end());
                f.kind = Found::method_not_allowed;
                for (auto& m : methods) {
                    if (!f.allow.empty()) {
                        f.allow += ", ";
                    }
                    f.allow += m;
                }
            }
            return f;
        }

        const RoutePattern& pattern(size_t i) const noexcept {
            return _patterns[i];
        }

        size_t size() const noexcept {
            return _patterns.size();
        }

    private:
        // A match without a multi wildcard, or with one that took only the
        // empty last segment
        static bool _exact(const RoutePattern& p, const PathSegments::List& segs) noexcept {
            if (p.segments.empty() || p.segments.back().kind != RouteSegment::multi) {
                return true;
            }
            size_t k = p.segments.size() - 1;
            return segs.size() == k + 1 && segs[k].empty();
        }

        optional<size_t> _best(std::string_view method, std::string_view host, const PathSegments::List& segs) const noexcept {
            optional<size_t> best;
            bool best_host = false;
            for (size_t i = 0; i < _patterns.size(); ++i) {
                auto& p = _patterns[i];
                if (!p.host.empty() && p.host != host) {
                    continue;
                }
                if (!match_method(p.method, method)) {
                    continue;
                }
                if (!match_path(p, segs, nullptr)) {
                    continue;
                }
                bool with_host = !p.host.empty();
                if (!best || (with_host && !best_host)) {
                    best = i;
                    best_host = with_host;
                    continue;
                }
                if (with_host != best_host) {
                    continue;
                }
                if (compare_routes(p, _patterns[*best]) == Relation::more_specific) {
                    best = i;
                }
            }
            return best;
        }

        std::vector<RoutePattern> _patterns;
        vector<string> _names;   // the wildcards' names as strings, made once when a route is added (managed: the table lives in the server's state)
    };
}
