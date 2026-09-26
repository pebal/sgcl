//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../url.h"
#include "../../../core/aliases.h"

#include <algorithm>
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

    // The segments of an escaped path, each unescaped: "/" is [""], "/a/" is ["a", ""]
    inline std::vector<std::string> path_segments(std::string_view path) {
        std::vector<std::string> out;
        if (path.empty() || path.front() != '/') {
            out.push_back(net::detail::url_unescape(path));
            return out;
        }
        path.remove_prefix(1);
        for (;;) {
            auto slash = path.find('/');
            out.push_back(net::detail::url_unescape(path.substr(0, slash)));
            if (slash == std::string_view::npos) {
                break;
            }
            path.remove_prefix(slash + 1);
        }
        return out;
    }

    // Whether the path matches the pattern's, and the values of its wildcards
    inline bool match_path(const RoutePattern& p, const std::vector<std::string>& segs, std::vector<std::pair<std::string, std::string>>* values) {
        for (size_t i = 0; i < p.segments.size(); ++i) {
            auto& s = p.segments[i];
            if (s.kind == RouteSegment::multi) {
                if (i >= segs.size()) {
                    return false;
                }
                if (values && !s.text.empty()) {
                    std::string rest;
                    for (size_t k = i; k < segs.size(); ++k) {
                        if (k > i) {
                            rest += '/';
                        }
                        rest += segs[k];
                    }
                    values->emplace_back(s.text, std::move(rest));
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
                        values->emplace_back(s.text, segs[i]);
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
            _patterns.push_back(std::move(p));
            return _patterns.size() - 1;
        }

        struct Found {
            enum Kind : uint8_t { route, not_found, method_not_allowed, redirect } kind = not_found;
            size_t index = 0;
            std::vector<std::pair<std::string, std::string>> values;
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
        Found find(std::string_view method, std::string_view host, std::string_view path) const {
            Found f;
            auto segs = path_segments(path);
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
            std::string h;
            for (char c : host) {
                h.push_back(ascii_lower(c));
            }
            if (!h.empty() && h.front() != '[') {
                auto colon = h.rfind(':');
                if (colon != std::string::npos) {
                    h.erase(colon);
                }
            } else if (!h.empty()) {
                auto close = h.find(']');
                if (close != std::string::npos) {
                    h.erase(close + 1);
                }
            }
            bool slashless = !path.empty() && path.back() != '/';
            auto more = segs;
            more.push_back("");
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
                match_path(_patterns[*best], segs, &f.values);
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
        static bool _exact(const RoutePattern& p, const std::vector<std::string>& segs) noexcept {
            if (p.segments.empty() || p.segments.back().kind != RouteSegment::multi) {
                return true;
            }
            size_t k = p.segments.size() - 1;
            return segs.size() == k + 1 && segs[k].empty();
        }

        optional<size_t> _best(std::string_view method, const std::string& host, const std::vector<std::string>& segs) const {
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
    };
}
