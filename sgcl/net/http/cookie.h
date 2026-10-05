//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <charconv>

#include "headers.h"
#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../time/date.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::net::http {
    // A cookie as a server sets it (Set-Cookie, RFC 6265 §4.1) and as a
    // client reads one back (§5.2): a value type with its attributes as
    // fields, Go's http.Cookie. Which cookies go to which request is the
    // jar's (cookie_jar.h), which a client takes in its `jar`.
    //
    // to_string writes a Set-Cookie value; a name that is not a token is
    // a broken contract (invalid_argument), a byte of the value no cookie
    // may hold is dropped, and a value with a space or a comma is quoted,
    // as Go does. parse reads one leniently, as a browser does: an
    // attribute it does not understand is ignored, and nullopt comes back
    // only for a first pair without a '=' or with an empty name.
    class cookie {
    public:
        string name;
        string value;
        string path;                 // "/" and below; "" for the default
        string domain;               // "example.com" (and its subdomains); "" for the host alone
        optional<time::datetime> expires;   // Expires; Max-Age wins over it where both are given
        optional<duration> max_age;  // Max-Age in seconds: zero or less deletes the cookie now
        bool secure = false;
        bool http_only = false;
        bool partitioned = false;
        string same_site;            // "Strict", "Lax", "None", or "" for none

        cookie() noexcept = default;

        SGCL_INLINE_HOT cookie(const string& name, const string& value) noexcept
        : name(name), value(value) {
        }

        // The cookie a Set-Cookie literal in the program spells: parse's
        // value or its bad_expected_access<io::error> (DESIGN 234)
        SGCL_INLINE_HOT explicit cookie(const string& field)
        : cookie(parse(field).value()) {
        }

        // The value of a Set-Cookie field
        string to_string() const;

        // A Set-Cookie value; net::errc::invalid_cookie when it holds no cookie
        static expected<cookie, io::error> parse(const string& field) noexcept;
    };

    namespace detail {
        // cookie::parse without the error, for a reading that may find none
        optional<cookie> parse_cookie(const string& field) noexcept;
    }

    namespace detail {
        // cookie-octet of RFC 6265 §4.1.1
        SGCL_INLINE_HOT constexpr bool cookie_octet(uint8_t c) noexcept {
            return c == 0x21 || (c >= 0x23 && c <= 0x2B) || (c >= 0x2D && c <= 0x3A) || (c >= 0x3C && c <= 0x5B) || (c >= 0x5D && c <= 0x7E);
        }

        // The cookie-date of RFC 6265 §5.1.1, which takes what browsers
        // take ("Wed, 09 Jun 2021 10:18:14 GMT", "Wednesday, 09-Jun-21
        // 10:18:14 GMT", "Wed Jun  9 10:18:14 2021", "09 Jun 2021 10:18:14"):
        // the text cut at its delimiters, and of the tokens the first that
        // is a time, a day of the month, a month and a year, each once. A
        // year of two digits is 1970 to 2069; before 1601 is no date. In
        // UTC; a date past 2262 is the end of datetime's range
        inline optional<time::datetime> cookie_date(std::string_view v) noexcept {
            auto delimiter = [](uint8_t c) {
                return c == 0x09 || (c >= 0x20 && c <= 0x2F) || (c >= 0x3B && c <= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E);
            };
            auto digit = [](char c) { return c >= '0' && c <= '9'; };
            // 1*max DIGIT at the front, and then nothing or a non-digit
            auto digits = [&](std::string_view t, size_t min, size_t max, int& out) {
                size_t n = 0;
                int value = 0;
                while (n < t.size() && digit(t[n])) {
                    value = value * 10 + (t[n] - '0');
                    ++n;
                    if (n > max) {
                        return false;
                    }
                }
                if (n < min) {
                    return false;
                }
                out = value;
                return true;
            };
            bool found_time = false, found_day = false, found_month = false, found_year = false;
            int hour = 0, minute = 0, second = 0, day = 0, month = 0, year = 0;
            size_t i = 0;
            while (i < v.size()) {
                while (i < v.size() && delimiter(uint8_t(v[i]))) {
                    ++i;
                }
                size_t from = i;
                while (i < v.size() && !delimiter(uint8_t(v[i]))) {
                    ++i;
                }
                auto token = v.substr(from, i - from);
                if (token.empty()) {
                    continue;
                }
                if (!found_time) {
                    // time-field ":" time-field ":" time-field, then nothing or a non-digit
                    int h, m, s;
                    auto c1 = token.find(':');
                    auto c2 = c1 == std::string_view::npos ? c1 : token.find(':', c1 + 1);
                    auto whole = [&](std::string_view t, int& out) {
                        return !t.empty() && t.size() <= 2 && std::all_of(t.begin(), t.end(), digit) && digits(t, 1, 2, out);
                    };
                    if (c2 != std::string_view::npos && whole(token.substr(0, c1), h) && whole(token.substr(c1 + 1, c2 - c1 - 1), m)
                        && digits(token.substr(c2 + 1), 1, 2, s)) {
                        found_time = true;
                        hour = h;
                        minute = m;
                        second = s;
                        continue;
                    }
                }
                int n;
                if (!found_day && digits(token, 1, 2, n)) {
                    found_day = true;
                    day = n;
                    continue;
                }
                if (!found_month && token.size() >= 3) {
                    static constexpr const char* Months[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
                    int found = 0;
                    for (int k = 0; k < 12 && !found; ++k) {
                        if (iequal(token.substr(0, 3), Months[k])) {
                            found = k + 1;
                        }
                    }
                    if (found) {
                        found_month = true;
                        month = found;
                        continue;
                    }
                }
                if (!found_year && digits(token, 2, 4, n)) {
                    found_year = true;
                    year = n;
                    continue;
                }
            }
            if (year >= 70 && year <= 99) {
                year += 1900;
            } else if (year <= 69) {
                year += 2000;
            }
            if (!found_time || !found_day || !found_month || !found_year || day < 1 || day > 31 || year < 1601 || hour > 23 || minute > 59 || second > 59) {
                return nullopt;
            }
            if (!time::date::is_valid(year, month, day)) {
                return nullopt;
            }
            auto days = static_cast<std::chrono::sys_days>(time::date(year, month, day)).time_since_epoch().count();
            int64_t secs = int64_t(days) * 86400 + hour * 3600 + minute * 60 + second;
            return time::datetime::from_unix(secs, time::zone::utc());
        }

        SGCL_INLINE_HOT std::string_view cookie_unquote(std::string_view v) noexcept {
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                v.remove_prefix(1);
                v.remove_suffix(1);
            }
            return v;
        }

        // The value of the first cookie of the name in the Cookie fields
        // of a request ("a=1; b=2"), "" when there is none
        inline optional<string> request_cookie(const headers& h, std::string_view name) noexcept {
            for (auto& f : HeadersAccess::fields(h)) {
                if (!iequal(f.first.view(), "cookie")) {
                    continue;
                }
                std::string_view v = f.second.view();
                while (!v.empty()) {
                    auto semi = v.find(';');
                    auto part = trim_ows(v.substr(0, semi));
                    auto eq = part.find('=');
                    if (eq != std::string_view::npos) {
                        auto n = trim_ows(part.substr(0, eq));
                        if (n == name && is_token(n)) {
                            auto value = cookie_unquote(trim_ows(part.substr(eq + 1)));
                            bool ok = true;
                            for (char c : value) {
                                ok = ok && (cookie_octet(uint8_t(c)) || c == ' ' || c == ',');
                            }
                            if (ok) {
                                return string(value);
                            }
                        }
                    }
                    if (semi == std::string_view::npos) {
                        break;
                    }
                    v.remove_prefix(semi + 1);
                }
            }
            return nullopt;
        }
    }

    // The Set-Cookie value, written twice over the same steps: a pass that
    // counts, then one into the string of that size (a string each for the
    // name, the value and the whole before, and a copy between them)
    inline string cookie::to_string() const {
        if (!detail::is_token(name.view())) {
            throw invalid_argument("http::cookie: a name must be a token of RFC 6265");
        }
        string when = expires ? expires->format(time::http) : string();
        char age[24];
        size_t age_n = 0;
        if (max_age) {
            int64_t secs = max_age->nanoseconds() / 1000000000;
            age_n = size_t(std::to_chars(age, age + sizeof age, secs > 0 ? secs : int64_t(0)).ptr - age);
        }
        auto write = [&](auto&& put) {
            put(name.view());
            put(std::string_view("="));
            bool quote = false;
            for (char c : value.view()) {
                quote = quote || (!detail::cookie_octet(uint8_t(c)) && (c == ' ' || c == ','));
            }
            if (quote) {
                put(std::string_view("\""));
            }
            for (char c : value.view()) {
                if (detail::cookie_octet(uint8_t(c)) || c == ' ' || c == ',') {
                    put(std::string_view(&c, 1));
                }
            }
            if (quote) {
                put(std::string_view("\""));
            }
            if (!path.empty()) {
                put(std::string_view("; Path="));
                for (char c : path.view()) {
                    if (uint8_t(c) >= 0x20 && uint8_t(c) < 0x7F && c != ';') {
                        put(std::string_view(&c, 1));
                    }
                }
            }
            if (!domain.empty()) {
                auto d = domain.view();
                if (d.front() == '.') {
                    d.remove_prefix(1);
                }
                bool ok = !d.empty() && d.front() != '.';   // "..x" is no host name, and a reader would take one dot off again
                for (char c : d) {
                    ok = ok && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_');
                }
                if (ok) {
                    put(std::string_view("; Domain="));
                    put(d);
                }
            }
            if (expires) {
                put(std::string_view("; Expires="));
                put(when.view());
            }
            if (max_age) {
                put(std::string_view("; Max-Age="));
                put(std::string_view(age, age_n));
            }
            if (http_only) {
                put(std::string_view("; HttpOnly"));
            }
            if (secure) {
                put(std::string_view("; Secure"));
            }
            if (detail::iequal(same_site.view(), "strict")) {
                put(std::string_view("; SameSite=Strict"));
            } else if (detail::iequal(same_site.view(), "lax")) {
                put(std::string_view("; SameSite=Lax"));
            } else if (detail::iequal(same_site.view(), "none")) {
                put(std::string_view("; SameSite=None"));
            }
            if (partitioned) {
                put(std::string_view("; Partitioned"));
            }
        };
        size_t n = 0;
        write([&](std::string_view piece) {
            n += piece.size();
        });
        return sgcl::detail::StringAccess::filled<string>(n, [&](char* at) {
            write([&](std::string_view piece) {
                sgcl::detail::copy_bytes(at, piece.data(), piece.size());
                at += piece.size();
            });
        });
    }

    SGCL_INLINE_HOT expected<cookie, io::error> cookie::parse(const string& field) noexcept {
        if (auto c = detail::parse_cookie(field)) {
            return std::move(*c);
        }
        return unexpected(net::detail::net_error(net::errc::invalid_cookie, "parse cookie", field));
    }

    inline optional<cookie> detail::parse_cookie(const string& field) noexcept {
        std::string_view v = field.view();
        auto semi = v.find(';');
        auto pair = detail::trim_ows(v.substr(0, semi));
        auto eq = pair.find('=');
        if (eq == std::string_view::npos) {
            return nullopt;
        }
        auto n = detail::trim_ows(pair.substr(0, eq));
        if (!detail::is_token(n)) {
            return nullopt;
        }
        cookie c;
        c.name = string(n);
        c.value = string(detail::cookie_unquote(detail::trim_ows(pair.substr(eq + 1))));
        v = semi == std::string_view::npos ? std::string_view() : v.substr(semi + 1);
        while (!v.empty()) {
            semi = v.find(';');
            auto attr = detail::trim_ows(v.substr(0, semi));
            v = semi == std::string_view::npos ? std::string_view() : v.substr(semi + 1);
            auto aeq = attr.find('=');
            auto key = detail::trim_ows(attr.substr(0, aeq));
            auto val = aeq == std::string_view::npos ? std::string_view() : detail::trim_ows(attr.substr(aeq + 1));
            if (detail::iequal(key, "secure")) {
                c.secure = true;
            } else if (detail::iequal(key, "httponly")) {
                c.http_only = true;
            } else if (detail::iequal(key, "partitioned")) {
                c.partitioned = true;
            } else if (detail::iequal(key, "path")) {
                if (!val.empty() && val.front() == '/') {
                    c.path = string(val);
                }
            } else if (detail::iequal(key, "domain")) {
                if (!val.empty() && val.front() == '.') {
                    val.remove_prefix(1);
                }
                std::string d(val);
                for (auto& ch : d) {
                    ch = detail::ascii_lower(ch);
                }
                if (!d.empty()) {
                    c.domain = string(std::string_view(d));
                }
            } else if (detail::iequal(key, "expires")) {
                if (auto t = detail::cookie_date(val)) {
                    c.expires = *t;
                }
            } else if (detail::iequal(key, "max-age")) {
                // §5.2.2: an optional '-' and digits, else the attribute is
                // ignored; a number of any length, leading zeros among it,
                // and one past 18 digits saturates (it is past duration's
                // range anyway) without the sum overflowing
                std::string_view digits = val;
                bool negative = !digits.empty() && digits.front() == '-';
                if (negative) {
                    digits.remove_prefix(1);
                }
                bool ok = !digits.empty();
                int64_t secs = 0;
                size_t significant = 0;
                for (size_t i = 0; ok && i < digits.size(); ++i) {
                    ok = digits[i] >= '0' && digits[i] <= '9';
                    significant += significant || digits[i] != '0';
                    if (significant <= 18) {
                        secs = secs * 10 + (digits[i] - '0');
                    }
                }
                if (ok) {
                    c.max_age = negative || secs == 0 ? duration::zero()
                              : significant > 18      ? duration::max()
                                                      : duration(std::chrono::seconds(secs));
                }
            } else if (detail::iequal(key, "samesite")) {
                if (detail::iequal(val, "strict")) {
                    c.same_site = "Strict";
                } else if (detail::iequal(val, "lax")) {
                    c.same_site = "Lax";
                } else if (detail::iequal(val, "none")) {
                    c.same_site = "None";
                }
            }
        }
        return c;
    }
}
