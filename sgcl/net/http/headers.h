//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace sgcl::net::http {
    namespace detail {
        using namespace sgcl::detail;

        // tchar of RFC 9110 §5.6.2: the characters of a token (a method,
        // a field name, a coding)
        inline constexpr bool token_char(uint8_t c) noexcept {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                return true;
            }
            switch (c) {
                case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
                case '-': case '.': case '^': case '_': case '`': case '|': case '~':
                    return true;
                default:
                    return false;
            }
        }

        inline constexpr bool is_token(std::string_view s) noexcept {
            if (s.empty()) {
                return false;
            }
            for (char c : s) {
                if (!token_char(uint8_t(c))) {
                    return false;
                }
            }
            return true;
        }

        inline constexpr char ascii_lower(char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
        }

        inline constexpr bool iequal(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (ascii_lower(a[i]) != ascii_lower(b[i])) {
                    return false;
                }
            }
            return true;
        }

        // OWS (SP and HTAB) off both ends
        inline constexpr std::string_view trim_ows(std::string_view s) noexcept {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
                s.remove_prefix(1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
                s.remove_suffix(1);
            }
            return s;
        }

        // A value as a program gives it, made safe to write: CR, LF and
        // NUL become spaces (a header of a user's text must not end the
        // head and start another: response splitting), as Go does
        inline string field_value(const string& value) {
            auto v = value.view();
            bool clean = true;
            for (char c : v) {
                if (c == '\r' || c == '\n' || c == '\0') {
                    clean = false;
                    break;
                }
            }
            if (clean) {
                return value;
            }
            std::string s(v);
            for (auto& c : s) {
                if (c == '\r' || c == '\n' || c == '\0') {
                    c = ' ';
                }
            }
            return string(std::string_view(s));
        }

        struct HeadersAccess;
    }

    // The fields of a head: a list of (name, value) in the order of the
    // wire, a name as often as it comes (Set-Cookie). Names are compared
    // without regard to ASCII case and kept as they were written: no
    // canonical form ("content-type" is not turned into "Content-Type",
    // which would cost a string a field, and HTTP/2 writes them lower case
    // anyway). A lookup walks the list; a head is a handful of fields and
    // its size is bounded by the server's limit, so there is no hash to be
    // steered by whoever sends it.
    //
    // The fields of a parsed head are slices of the one string the head
    // was copied into: nothing is allocated a field. get() makes the
    // string it returns.
    //
    // A name the program gives must be a token of RFC 9110 (a broken
    // contract otherwise: invalid_argument); a value has CR, LF and NUL
    // turned into spaces, since values often come from users and an
    // exception in the path of a request would be worse than the change.
    class headers {
    public:
        headers() = default;

        // The first value of the name, or "" when there is none (has()
        // tells the two apart)
        string get(const string& name) const {
            for (auto& f : _fields) {
                if (detail::iequal(f.first.view(), name.view())) {
                    return string(f.second);
                }
            }
            return string();
        }

        // Every value of the name, in their order (Set-Cookie)
        vector<string> get_all(const string& name) const {
            vector<string> out;
            for (auto& f : _fields) {
                if (detail::iequal(f.first.view(), name.view())) {
                    out.push_back(string(f.second));
                }
            }
            return out;
        }

        bool contains(const string& name) const {
            for (auto& f : _fields) {
                if (detail::iequal(f.first.view(), name.view())) {
                    return true;
                }
            }
            return false;
        }

        // The value in the place of the first field of the name, the
        // others of the name gone; a field at the end when there was none
        headers& set(const string& name, const string& value) {
            _check(name);
            auto v = detail::field_value(value);
            bool found = false;
            vector<Field> kept;
            kept.reserve(_fields.size() + 1);
            for (auto& f : _fields) {
                if (!detail::iequal(f.first.view(), name.view())) {
                    kept.push_back(f);
                } else if (!found) {
                    found = true;
                    kept.push_back(Field(f.first, v.as_slice()));
                }
            }
            if (!found) {
                kept.push_back(Field(name.as_slice(), v.as_slice()));
            }
            _fields = std::move(kept);
            return *this;
        }

        // A field at the end
        headers& add(const string& name, const string& value) {
            _check(name);
            auto v = detail::field_value(value);
            _fields.push_back(Field(name.as_slice(), v.as_slice()));
            return *this;
        }

        // Every field of the name
        headers& erase(const string& name) {
            vector<Field> kept;
            kept.reserve(_fields.size());
            for (auto& f : _fields) {
                if (!detail::iequal(f.first.view(), name.view())) {
                    kept.push_back(f);
                }
            }
            _fields = std::move(kept);
            return *this;
        }

        // The first value of the name as a date of HTTP (RFC 9110 §5.6.7:
        // IMF-fixdate, and the obsolete RFC 850 and asctime forms a
        // recipient must take), in UTC; nullopt when there is none or it is
        // not a date (Last-Modified, If-Modified-Since, Expires, Date)
        optional<time::datetime> date(const string& name) const {
            for (auto& f : _fields) {
                if (detail::iequal(f.first.view(), name.view())) {
                    auto t = time::datetime::parse(string(f.second), time::http);
                    if (!t) {
                        return nullopt;
                    }
                    return *t;
                }
            }
            return nullopt;
        }

        // The instant as IMF-fixdate, always GMT: "Sun, 06 Nov 1994 08:49:37 GMT"
        headers& set_date(const string& name, const time::datetime& t) {
            return set(name, t.format(time::http));
        }

        size_t size() const noexcept {
            return _fields.size();
        }

        bool empty() const noexcept {
            return _fields.empty();
        }

        // The fields in their order, each a pair<string, string> made as
        // it is reached
        class iterator {
        public:
            using value_type = pair<string, string>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::input_iterator_tag;

            iterator() = default;

            value_type operator*() const {
                return value_type(string(_at->first), string(_at->second));
            }

            iterator& operator++() {
                ++_at;
                return *this;
            }

            iterator operator++(int) {
                auto was = *this;
                ++_at;
                return was;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._at == b._at;
            }

        private:
            friend class headers;
            using Base = const pair<slice<const char>, slice<const char>>*;

            explicit iterator(Base at) noexcept
            : _at(at) {
            }

            Base _at = nullptr;
        };

        iterator begin() const noexcept {
            return iterator(_fields.data());
        }

        iterator end() const noexcept {
            return iterator(_fields.data() + _fields.size());
        }

    private:
        friend struct detail::HeadersAccess;
        using Field = pair<slice<const char>, slice<const char>>;

        static void _check(const string& name) {
            if (!detail::is_token(name.view())) {
                throw invalid_argument("http::headers: a field name must be a token of RFC 9110");
            }
        }

        vector<Field> _fields;
    };

    namespace detail {
        // What the library reads a head by without making a string a field
        struct HeadersAccess {
            using Field = pair<slice<const char>, slice<const char>>;

            static vector<Field>& fields(headers& h) noexcept {
                return h._fields;
            }

            static const vector<Field>& fields(const headers& h) noexcept {
                return h._fields;
            }

            // The first value of the name as a view (valid while h is)
            static optional<std::string_view> find(const headers& h, std::string_view name) noexcept {
                for (auto& f : h._fields) {
                    if (iequal(f.first.view(), name)) {
                        return f.second.view();
                    }
                }
                return nullopt;
            }

            static size_t count(const headers& h, std::string_view name) noexcept {
                size_t n = 0;
                for (auto& f : h._fields) {
                    n += iequal(f.first.view(), name) ? 1 : 0;
                }
                return n;
            }

            // Whether a list-valued field (Connection, Transfer-Encoding)
            // holds the token, in any of its fields, ASCII case aside
            static bool has_token(const headers& h, std::string_view name, std::string_view token) noexcept {
                for (auto& f : h._fields) {
                    if (!iequal(f.first.view(), name)) {
                        continue;
                    }
                    std::string_view v = f.second.view();
                    while (!v.empty()) {
                        auto comma = v.find(',');
                        auto item = trim_ows(v.substr(0, comma));
                        if (iequal(item, token)) {
                            return true;
                        }
                        if (comma == std::string_view::npos) {
                            break;
                        }
                        v.remove_prefix(comma + 1);
                    }
                }
                return false;
            }

            // A field the library itself adds, its name trusted
            static void add(headers& h, const slice<const char>& name, const slice<const char>& value) {
                h._fields.push_back(Field(name, value));
            }
        };
    }
}
