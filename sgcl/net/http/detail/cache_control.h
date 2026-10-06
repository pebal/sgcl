//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../../core/aliases.h"

#include <cstdint>
#include <string>
#include <string_view>

// What a private cache reads of a message (RFC 9111): the directives of
// Cache-Control (§5.2) of a request and of a response, the age (§4.2.3),
// the freshness lifetime (§4.2.1, the heuristic of §4.2.2), whether a
// response may be stored (§3) and whether a stored one may be served (§4.2,
// §4.2.4, RFC 5861). Pure functions of the fields and the times, in
// seconds; cache.h keeps the entries.
namespace sgcl::net::http::detail {
    // The directives of a Cache-Control field (a request's or a response's),
    // the delta-seconds of each that has one, -1 for none
    struct CacheControl {
        bool no_store = false;
        bool no_cache = false;            // response: no-cache without fields names; request: no-cache
        bool no_transform = false;
        bool must_revalidate = false;
        bool proxy_revalidate = false;
        bool is_public = false;
        bool is_private = false;
        bool only_if_cached = false;
        bool immutable = false;
        int64_t max_age = -1;
        int64_t s_maxage = -1;
        int64_t max_stale = -1;           // request: max-stale; a max-stale without a value is any staleness (INT64_MAX)
        int64_t min_fresh = -1;
        int64_t stale_while_revalidate = -1;
        int64_t stale_if_error = -1;
    };

    // delta-seconds (RFC 9111 §1.2.2): 1*DIGIT, saturating at 2^31 (the RFC's
    // "greatest positive integer"); -1 for anything else
    inline int64_t delta_seconds(std::string_view v) noexcept {
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
            v = v.substr(1, v.size() - 2);   // a quoted value, which senders should not but do send
        }
        if (v.empty()) {
            return -1;
        }
        int64_t n = 0;
        for (char c : v) {
            if (c < '0' || c > '9') {
                return -1;
            }
            n = n * 10 + (c - '0');
            if (n > 2147483648) {
                n = 2147483648;
            }
        }
        return n;
    }

    // The directives of one Cache-Control value into cc, names without case;
    // a directive given twice keeps its first value (§4.2.1: a cache "SHOULD
    // use the first")
    inline void cache_control_value(std::string_view list, CacheControl& cc) noexcept {
        while (!list.empty()) {
            // a comma inside a quoted value (no-cache="a, b") is not a separator
            size_t end = 0;
            bool quoted = false;
            while (end < list.size() && (quoted || list[end] != ',')) {
                quoted = list[end] == '"' ? !quoted : quoted;
                ++end;
            }
            std::string_view item = trim_ows(list.substr(0, end));
            list = end < list.size() ? list.substr(end + 1) : std::string_view();
            const size_t eq = item.find('=');
            const std::string_view name = trim_ows(item.substr(0, eq));
            const std::string_view value = eq == std::string_view::npos ? std::string_view() : trim_ows(item.substr(eq + 1));
            auto first = [&](int64_t& slot, int64_t none_value = -1) {
                if (slot == -1) {
                    int64_t n = delta_seconds(value);
                    slot = n >= 0 ? n : none_value;
                }
            };
            if (iequal(name, "no-store")) {
                cc.no_store = true;
            } else if (iequal(name, "no-cache")) {
                cc.no_cache = cc.no_cache || eq == std::string_view::npos;   // no-cache="field" is about fields, not the whole
            } else if (iequal(name, "no-transform")) {
                cc.no_transform = true;
            } else if (iequal(name, "must-revalidate")) {
                cc.must_revalidate = true;
            } else if (iequal(name, "proxy-revalidate")) {
                cc.proxy_revalidate = true;
            } else if (iequal(name, "public")) {
                cc.is_public = true;
            } else if (iequal(name, "private")) {
                cc.is_private = true;
            } else if (iequal(name, "only-if-cached")) {
                cc.only_if_cached = true;
            } else if (iequal(name, "immutable")) {
                cc.immutable = true;
            } else if (iequal(name, "max-age")) {
                first(cc.max_age, 0);   // §4.2.1: an invalid max-age is stale (0)
            } else if (iequal(name, "s-maxage")) {
                first(cc.s_maxage);
            } else if (iequal(name, "max-stale")) {
                if (cc.max_stale == -1) {
                    cc.max_stale = eq == std::string_view::npos ? INT64_MAX : std::max<int64_t>(delta_seconds(value), 0);
                }
            } else if (iequal(name, "min-fresh")) {
                first(cc.min_fresh);
            } else if (iequal(name, "stale-while-revalidate")) {
                first(cc.stale_while_revalidate);
            } else if (iequal(name, "stale-if-error")) {
                first(cc.stale_if_error);
            }
        }
    }

    // Every Cache-Control field of the message, its directives in their order
    inline CacheControl cache_control(const http::headers& h) noexcept {
        CacheControl cc;
        for (auto& f : HeadersAccess::fields(h)) {
            if (iequal(f.first.view(), "cache-control")) {
                cache_control_value(f.second.view(), cc);
            }
        }
        return cc;
    }

    // The statuses a cache may store and reuse by a heuristic (RFC 9110
    // §15.1: heuristically cacheable)
    SGCL_INLINE_HOT constexpr bool heuristically_cacheable(int status) noexcept {
        switch (status) {
            case 200: case 203: case 204: case 300: case 301: case 308: case 404: case 405: case 410: case 414: case 501:
                return true;
            default:
                return false;
        }
    }

    // The times of a stored response, in unix seconds, as the cache saw them
    // (§4.2.3): the request sent, the response received; the response's Date
    // (the response time when it has none or one that does not parse), Age
    struct CacheTimes {
        int64_t request_time = 0;
        int64_t response_time = 0;
        int64_t date = 0;
        int64_t age = 0;
    };

    inline CacheTimes cache_times(const http::headers& response, int64_t request_time, int64_t response_time) noexcept {
        CacheTimes t;
        t.request_time = request_time;
        t.response_time = response_time;
        auto d = response.date("Date");
        t.date = d ? d->unix() : response_time;
        if (auto a = HeadersAccess::find(response, "age")) {
            t.age = std::max<int64_t>(delta_seconds(trim_ows(*a)), 0);
        }
        return t;
    }

    // current_age of §4.2.3
    SGCL_INLINE_HOT int64_t current_age(const CacheTimes& t, int64_t now) noexcept {
        const int64_t apparent_age = std::max<int64_t>(0, t.response_time - t.date);
        const int64_t response_delay = t.response_time - t.request_time;
        const int64_t corrected_age_value = t.age + response_delay;
        const int64_t corrected_initial_age = std::max(apparent_age, corrected_age_value);
        const int64_t resident_time = now - t.response_time;
        return corrected_initial_age + resident_time;
    }

    // The freshness lifetime of §4.2.1 for a private cache: max-age (s-maxage
    // is a shared cache's), else Expires - Date (an Expires that does not
    // parse is in the past), else the heuristic of §4.2.2 — a fraction of
    // the time since Last-Modified, at most heuristic_max — for a status
    // heuristically cacheable; 0 otherwise
    inline int64_t freshness_lifetime(const http::headers& response, const CacheControl& cc, int status, const CacheTimes& t, double heuristic,
                                      int64_t heuristic_max) noexcept {
        if (cc.max_age >= 0) {
            return cc.max_age;
        }
        if (HeadersAccess::count(response, "expires")) {
            auto e = response.date("Expires");
            return e ? std::max<int64_t>(0, e->unix() - t.date) : 0;
        }
        if (heuristically_cacheable(status) || cc.is_public) {
            if (auto lm = response.date("Last-Modified"); lm && lm->unix() < t.date) {
                return std::min<int64_t>(int64_t(double(t.date - lm->unix()) * heuristic), heuristic_max);
            }
        }
        return 0;
    }

    // Whether a response may be stored by a private cache (§3): to a GET; a
    // status it understands that is heuristically cacheable or has explicit
    // freshness (max-age, Expires, public); no no-store on either side; a
    // request with Authorization only when public or must-revalidate says
    // so (§3.5); Vary: * never matches, so it is not stored
    inline bool storable(std::string_view method, int status, const http::headers& request, const CacheControl& req_cc, const http::headers& response,
                         const CacheControl& res_cc) noexcept {
        if (method != "GET" || req_cc.no_store || res_cc.no_store) {
            return false;
        }
        if (status == 206 || status == 304 || (status >= 100 && status < 200)) {
            return false;
        }
        const bool explicit_freshness = res_cc.max_age >= 0 || HeadersAccess::count(response, "expires") || res_cc.is_public;
        if (!heuristically_cacheable(status) && !explicit_freshness) {
            return false;
        }
        if (HeadersAccess::count(request, "authorization") && !res_cc.is_public && !res_cc.must_revalidate && res_cc.s_maxage < 0) {
            return false;
        }
        for (auto& f : HeadersAccess::fields(response)) {
            if (iequal(f.first.view(), "vary")) {
                std::string_view list = f.second.view();
                while (!list.empty()) {
                    const size_t comma = list.find(',');
                    if (trim_ows(list.substr(0, comma)) == "*") {
                        return false;
                    }
                    list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
                }
            }
        }
        return true;
    }

    // How a stored response may be used for a request now (§4.2, §4.2.4,
    // RFC 5861)
    enum class CacheUse : uint8_t {
        fresh,                 // served as it is
        stale_revalidating,    // served, and revalidated in the background (stale-while-revalidate)
        validate,              // asked again with its validators; served stale only on an error (stale-if-error)
    };

    struct CacheVerdict {
        CacheUse use = CacheUse::validate;
        bool stale_on_error = false;   // a failed revalidation may serve it (stale-if-error, within its window)
        int64_t age = 0;
    };

    inline CacheVerdict cache_verdict(const CacheControl& req_cc, const CacheControl& res_cc, int64_t lifetime, int64_t age) noexcept {
        CacheVerdict v;
        v.age = age;
        const int64_t staleness = age - lifetime;   // >= 0 when stale (fresh while the lifetime is above the age)
        const bool may_stale = !res_cc.must_revalidate && !res_cc.no_cache;
        if (staleness >= 0 && res_cc.stale_if_error >= 0 && staleness <= res_cc.stale_if_error && may_stale) {
            v.stale_on_error = true;
        }
        if (req_cc.stale_if_error >= 0 && staleness >= 0 && staleness <= req_cc.stale_if_error && may_stale) {
            v.stale_on_error = true;
        }
        if (req_cc.no_cache || res_cc.no_cache) {
            return v;   // validate
        }
        if (req_cc.max_age >= 0 && age > req_cc.max_age) {
            return v;
        }
        int64_t left = lifetime - age;   // the freshness left
        if (req_cc.min_fresh >= 0 && left < req_cc.min_fresh) {
            return v;
        }
        if (left > 0) {
            v.use = CacheUse::fresh;
            return v;
        }
        if (req_cc.max_stale >= 0 && may_stale && -left <= req_cc.max_stale) {
            v.use = CacheUse::fresh;   // the client takes this much staleness
            return v;
        }
        if (res_cc.stale_while_revalidate >= 0 && may_stale && -left <= res_cc.stale_while_revalidate) {
            v.use = CacheUse::stale_revalidating;
            return v;
        }
        return v;
    }
}
