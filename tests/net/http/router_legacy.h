//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The router's lookup as it was before it read the path in views (router.h
// until 2026-09-27: std::vector<std::string> of the segments, copied for
// the path with a '/' added): the oracle of the differential test
// (router.cpp: TheLookupOfViewsAnswersAsTheOneOfStrings). Kept word for
// word but for the names, over the same RoutePattern.
#pragma once

#include "sgcl/net/http/detail/router.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace router_legacy {
    using sgcl::net::http::detail::Relation;
    using sgcl::net::http::detail::RoutePattern;
    using sgcl::net::http::detail::RouteSegment;

    struct Found {
        enum Kind : uint8_t { route, not_found, method_not_allowed, redirect } kind = not_found;
        size_t index = 0;
        std::vector<std::pair<std::string, std::string>> values;
        std::string allow;
        std::string location;
    };

    inline std::vector<std::string> path_segments(std::string_view path) {
        std::vector<std::string> out;
        if (path.empty() || path.front() != '/') {
            out.push_back(sgcl::net::detail::url_unescape(path));
            return out;
        }
        path.remove_prefix(1);
        for (;;) {
            auto slash = path.find('/');
            out.push_back(sgcl::net::detail::url_unescape(path.substr(0, slash)));
            if (slash == std::string_view::npos) {
                break;
            }
            path.remove_prefix(slash + 1);
        }
        return out;
    }

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

    inline bool exact(const RoutePattern& p, const std::vector<std::string>& segs) noexcept {
        if (p.segments.empty() || p.segments.back().kind != RouteSegment::multi) {
            return true;
        }
        size_t k = p.segments.size() - 1;
        return segs.size() == k + 1 && segs[k].empty();
    }

    inline std::optional<size_t> best(const std::vector<const RoutePattern*>& patterns, std::string_view method, const std::string& host, const std::vector<std::string>& segs) {
        std::optional<size_t> b;
        bool best_host = false;
        for (size_t i = 0; i < patterns.size(); ++i) {
            auto& p = *patterns[i];
            if (!p.host.empty() && p.host != host) {
                continue;
            }
            if (!sgcl::net::http::detail::match_method(p.method, method)) {
                continue;
            }
            if (!match_path(p, segs, nullptr)) {
                continue;
            }
            bool with_host = !p.host.empty();
            if (!b || (with_host && !best_host)) {
                b = i;
                best_host = with_host;
                continue;
            }
            if (with_host != best_host) {
                continue;
            }
            if (sgcl::net::http::detail::compare_routes(p, *patterns[*b]) == Relation::more_specific) {
                b = i;
            }
        }
        return b;
    }

    inline Found find(const std::vector<const RoutePattern*>& patterns, std::string_view method, std::string_view host, std::string_view path) {
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
            h.push_back(sgcl::net::http::detail::ascii_lower(c));
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
        auto b = best(patterns, method, h, segs);
        if (slashless && (!b || !exact(*patterns[*b], segs))) {
            auto sub = best(patterns, method, h, more);
            if (sub && exact(*patterns[*sub], more)) {
                f.kind = Found::redirect;
                f.location = std::string(path) + "/";
                return f;
            }
        }
        if (b) {
            f.kind = Found::route;
            f.index = *b;
            match_path(*patterns[*b], segs, &f.values);
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
        for (auto* pp : patterns) {
            auto& p = *pp;
            if (!p.host.empty() && p.host != h) {
                continue;
            }
            if (match_path(p, segs, nullptr) || (slashless && match_path(p, more, nullptr) && exact(p, more))) {
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
}
