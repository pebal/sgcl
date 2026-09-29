//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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

        // A byte a field value may hold (RFC 9110 §5.5): a visible
        // character, a space, a tab or obs-text; never CR, LF, NUL or
        // another control, so that a value cannot end its line and start
        // another (request and response splitting, obs-fold)
        inline constexpr bool field_value_char(uint8_t c) noexcept {
            return c == '\t' || (c >= 0x20 && c != 0x7F);
        }

        // The name and the value as a program gave them, the bytes that
        // are not printable shown as \xHH: a message naming a field must
        // not carry the line break that made it wrong into a log
        inline std::string printable(std::string_view s) {
            std::string out;
            for (unsigned char c : s) {
                if (c >= 0x20 && c < 0x7F) {
                    out += char(c);
                } else {
                    static const char digits[] = "0123456789ABCDEF";
                    out += "\\x";
                    out += digits[c >> 4];
                    out += digits[c & 15];
                }
            }
            return out;
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
    // A name and a value are kept as the program gives them. What is
    // written is checked where it is written: a name that is not a token
    // of RFC 9110, or a value with CR, LF, NUL or another control, makes
    // the client's send an error (std::errc::invalid_argument, the field
    // named) before a byte is sent, and a handler's response a 500 (see
    // response_writer), since values often come from users and neither a
    // split message nor an exception in the path of a request will do.
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
        // (in place: the fields kept moved down over the ones dropped, no
        // list made)
        headers& set(const string& name, const string& value) {
            const string& v = value;
            bool found = false;
            size_t kept = 0;
            for (size_t i = 0; i < _fields.size(); ++i) {
                if (detail::iequal(_fields[i].first.view(), name.view())) {
                    if (found) {
                        continue;
                    }
                    found = true;
                    _fields[i].second = v.as_slice();
                }
                if (kept != i) {
                    _fields[kept] = _fields[i];
                }
                ++kept;
            }
            if (found) {
                _fields.resize(kept);
            } else {
                _push(Field(name.as_slice(), v.as_slice()));
            }
            return *this;
        }

        // A field at the end
        headers& add(const string& name, const string& value) {
            _push(Field(name.as_slice(), value.as_slice()));
            return *this;
        }

        // Every field of the name
        headers& erase(const string& name) {
            size_t kept = 0;
            for (size_t i = 0; i < _fields.size(); ++i) {
                if (detail::iequal(_fields[i].first.view(), name.view())) {
                    continue;
                }
                if (kept != i) {
                    _fields[kept] = _fields[i];
                }
                ++kept;
            }
            _fields.resize(kept);
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

        // A field appended: room for eight at the first (a response's
        // fields, set one at a time, grew by 1, 2, 4, 8)
        void _push(Field f) {
            if (_fields.capacity() == 0) {
                _fields.reserve(8);
            }
            _fields.push_back(std::move(f));
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

        // What makes the fields unfit to be written, "invalid header name:
        // Bad Name" or "invalid header value: X-Foo"; nullopt when every
        // name is a token and no value holds a byte a field may not
        inline optional<string> invalid_field(const headers& h) {
            for (auto& f : HeadersAccess::fields(h)) {
                if (!is_token(f.first.view())) {
                    return string("invalid header name: " + printable(f.first.view()));
                }
                for (unsigned char c : f.second.view()) {
                    if (!field_value_char(c)) {
                        return string("invalid header value: " + printable(f.first.view()));
                    }
                }
            }
            return nullopt;
        }
    }
}
