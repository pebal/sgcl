//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "json.h"
#include "detail/json_number.h"
#include "detail/files.h"
#include "detail/utf8_check.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/maker.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/utf8.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"
#include "../time/date.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;
        struct TomlAccess;
        class TomlParser;
        class TomlWriter;
    }

    // One TOML value (TOML v1.0.0), immutable, as json is one JSON value: a
    // pointer, a word and a kind, 24 bytes, shared by copying. A document is
    // a table; a number, a boolean and a date or time keep their text as
    // written, the string its characters. Read by parse, written by to_string.
    class toml {
    public:
        using error = encoding::error;

        // The kinds of TOML
        enum class kind : uint8_t {
            table = 0,
            string,
            integer,
            floating,
            boolean,
            offset_datetime,   // 1979-05-27T07:32:00-08:00: an instant
            local_datetime,    // 1979-05-27T07:32:00: a date and a time, no zone
            local_date,        // 1979-05-27
            local_time,        // 07:32:00
            array
        };

        // A member of a table: a key and its value
        struct member;

        // What a parse accepts
        struct options {
            uint32_t max_depth = 512;   // tables and arrays inside one another
        };

        // --- making one ---

        // An empty table
        toml() noexcept = default;

        template<class B>
        requires std::same_as<B, bool>
        toml(B b) noexcept
        : toml(kind::boolean, string(b ? "true" : "false")) {
        }

        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>) && (!std::same_as<I, char>) && (!std::same_as<I, wchar_t>)
              && (!std::same_as<I, char8_t>) && (!std::same_as<I, char16_t>) && (!std::same_as<I, char32_t>)
        toml(I v) noexcept
        : toml(kind::integer, string(std::to_string(v))) {
        }

        // A float: its shortest digits, inf, -inf, nan
        toml(double d) noexcept;

        SGCL_INLINE_HOT toml(const string& s) noexcept
        : toml(kind::string, s) {
        }

        SGCL_INLINE_HOT toml(const char* s) noexcept
        : toml(kind::string, string(s)) {
        }

        // An offset date-time: the instant in the offset of its zone at it
        toml(const time::datetime& t) noexcept;

        // A local date
        toml(const time::date& d) noexcept;

        // A local time, the time since midnight; and a local date-time.
        // invalid_argument for a time outside a day
        static toml local_time(duration since_midnight);
        static toml local_datetime(const time::date& d, duration since_midnight);

        static toml array(std::initializer_list<toml> elements) noexcept;
        static toml array(const vector<toml>& elements) noexcept;
        static toml table(std::initializer_list<member> members) noexcept;
        static toml table(const vector<member>& members) noexcept;

        // A JSON value as TOML: null has no TOML and is left out of a table
        // and an array (a null alone, an empty table)
        static toml from_json(const json& j) noexcept;

        // --- reading and writing ---

        static expected<toml, error> parse(const string& text) noexcept;
        static expected<toml, error> parse(const string& text, const options& o) noexcept;
        static expected<toml, error> parse(const io::reader& in);
        static expected<toml, error> parse(const io::reader& in, const options& o);
        static async::task<expected<toml, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<toml, error>> async_parse(io::reader in, options o) noexcept;

        // A document of a table: its values, then its tables as [a.b] and
        // its arrays of tables as [[a.b]]. invalid_argument for a value that
        // is no table, or one deeper than 512 levels
        string to_string() const;

        // --- files: one line each ---

        // A document of a file, read as parse reads it: toml::load("app.toml"); a
        // file that does not open is errc::io, io_error() saying why
        static expected<toml, error> load(const string& path);
        static async::task<expected<toml, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // As JSON: a date or a time its text
        json to_json() const noexcept;

        // --- what it is ---

        SGCL_INLINE_HOT kind type() const noexcept {
            return _kind;
        }

        SGCL_INLINE_HOT bool is_table() const noexcept {
            return _kind == kind::table;
        }

        SGCL_INLINE_HOT bool is_array() const noexcept {
            return _kind == kind::array;
        }

        SGCL_INLINE_HOT bool is_string() const noexcept {
            return _kind == kind::string;
        }

        SGCL_INLINE_HOT bool is_integer() const noexcept {
            return _kind == kind::integer;
        }

        SGCL_INLINE_HOT bool is_floating() const noexcept {
            return _kind == kind::floating;
        }

        SGCL_INLINE_HOT bool is_number() const noexcept {
            return _kind == kind::integer || _kind == kind::floating;
        }

        SGCL_INLINE_HOT bool is_bool() const noexcept {
            return _kind == kind::boolean;
        }

        // Any of the four dates and times
        SGCL_INLINE_HOT bool is_datetime() const noexcept {
            return _kind == kind::offset_datetime || _kind == kind::local_datetime || _kind == kind::local_date || _kind == kind::local_time;
        }

        optional<bool> as_bool() const noexcept;
        optional<int64_t> as_int() const noexcept;
        optional<double> as_double() const noexcept;   // a float, or an integer rounded

        optional<string> as_string() const noexcept {
            if (_kind != kind::string) {
                return nullopt;
            }
            return _text();
        }

        SGCL_INLINE_HOT bool as_bool(bool fallback) const noexcept {
            return as_bool().value_or(fallback);
        }

        SGCL_INLINE_HOT int64_t as_int(int64_t fallback) const noexcept {
            return as_int().value_or(fallback);
        }

        SGCL_INLINE_HOT double as_double(double fallback) const noexcept {
            return as_double().value_or(fallback);
        }

        SGCL_INLINE_HOT string as_string(const string& fallback) const noexcept {
            auto s = as_string();
            return s ? *s : fallback;
        }

        // An offset date-time's instant, in the fixed zone of its offset
        optional<time::datetime> as_datetime() const noexcept;

        // A local date-time, or a local date at midnight, as the instant of
        // that clock in the zone; an offset date-time's instant in the zone
        optional<time::datetime> as_datetime(const time::zone& z) const noexcept;

        // The date of a date or a date-time
        optional<time::date> as_date() const noexcept;

        // The time since midnight of a time or a date-time
        optional<duration> as_time() const noexcept;

        // A number, a boolean, a date or a time as written; a string's
        // characters; empty for a table and an array
        string text() const noexcept;

        // --- inside it ---

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _kind == kind::table || _kind == kind::array ? size_t(_bits) : 0;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        // An array's element at the index; an empty table past the end
        SGCL_INLINE_HOT toml operator[](size_t index) const noexcept {
            return _kind == kind::array && index < _bits ? _elements()[index] : toml();
        }

        // A table's value of the key; an empty table when there is none
        toml operator[](const string& key) const noexcept;

        template<size_t N>
        toml operator[](const char (&key)[N]) const noexcept {
            return (*this)[string(key)];
        }

        bool contains(const string& key) const noexcept;

        slice<const toml> elements() const noexcept;
        slice<const member> members() const noexcept;

        // New versions: a table with the key set, without it; an array with
        // the element at the index set (one past the end appending), an
        // array with the value appended
        toml set(const string& key, const toml& value) const noexcept;
        toml set(size_t index, const toml& value) const noexcept;
        toml erase(const string& key) const noexcept;
        toml push_back(const toml& value) const noexcept;

        // Deep: scalars by their kind and the value they read (0xFF is 255,
        // 1e2 is 100.0, every nan one value, an offset date-time by its
        // instant), tables as sets of members
        friend bool operator==(const toml& a, const toml& b) noexcept {
            return a._equals(b);
        }

        size_t hash() const noexcept;

    private:
        friend struct detail::TomlAccess;
        friend class detail::TomlParser;
        friend class detail::TomlWriter;

        tracked_ptr<const void> _ptr;   // a scalar's text, a collection's buffer
        uint64_t _bits = 0;             // a collection's count
        kind _kind = kind::table;

        SGCL_INLINE_HOT toml(kind k, const string& text) noexcept
        : _ptr(text.as_slice().owner()), _kind(k) {
        }

        SGCL_INLINE_HOT string _text() const noexcept {
            return sgcl::detail::StringAccess::over<string>(_ptr);
        }

        SGCL_INLINE_HOT const toml* _elements() const noexcept {
            return static_cast<const toml*>(_ptr.get());
        }

        const member* _members() const noexcept;
        size_t _find(std::string_view key) const noexcept;
        bool _equals(const toml& o) const noexcept;
    };

    static_assert(sizeof(toml) == 24, "a toml is a pointer, a word and a kind");

    struct toml::member {
        string key;
        toml value;
    };

    namespace detail {
        struct TomlAccess {
            template<class T>
            static tracked_ptr<const void> buffer(const T* first, size_t n) noexcept {
                tracked_ptr<const void> owner;
                if (n) {
                    T* p = json_buffer<T>(n, owner);
                    for (size_t i = 0; i < n; ++i) {
                        Maker<T>::construct(p + i, first[i]);
                    }
                }
                return owner;
            }

            static toml scalar(toml::kind k, const string& text) noexcept {
                return toml(k, text);
            }

            static toml collection(toml::kind k, tracked_ptr<const void> owner, size_t n) noexcept {
                toml t;
                t._kind = k;
                t._bits = n;
                t._ptr = std::move(owner);
                return t;
            }
        };

        SGCL_INLINE_HOT bool toml_digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        // The fields of a date, a time, or a date-time with its offset, read
        // from its text (known good: the parse checked it)
        struct TomlMoment {
            bool has_date = false, has_time = false, has_offset = false;
            int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
            uint32_t ns = 0;
            int offset = 0;   // seconds east of UTC
        };

        // The text of a date, a time or a date-time checked and read: false
        // for what TOML does not take (a field out of its range, a day the
        // month has not, a missing second)
        inline bool toml_moment(std::string_view s, TomlMoment& m) noexcept {
            size_t i = 0;
            auto two = [&](size_t at) { return (s[at] - '0') * 10 + (s[at + 1] - '0'); };
            auto digits = [&](size_t at, size_t n) {
                if (at + n > s.size()) {
                    return false;
                }
                for (size_t k = 0; k < n; ++k) {
                    if (!toml_digit(s[at + k])) {
                        return false;
                    }
                }
                return true;
            };
            if (digits(0, 4) && s.size() >= 10 && s[4] == '-' && digits(5, 2) && s[7] == '-' && digits(8, 2)) {
                m.has_date = true;
                m.year = two(0) * 100 + two(2);
                m.month = two(5);
                m.day = two(8);
                static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                bool leap = (m.year % 4 == 0 && m.year % 100 != 0) || m.year % 400 == 0;
                if (m.month < 1 || m.month > 12 || m.day < 1 || m.day > days[m.month - 1] + (m.month == 2 && leap)) {
                    return false;
                }
                i = 10;
                if (i == s.size()) {
                    return true;
                }
                if (s[i] != 'T' && s[i] != 't' && s[i] != ' ') {
                    return false;
                }
                ++i;
            }
            if (!(digits(i, 2) && i + 8 <= s.size() && s[i + 2] == ':' && digits(i + 3, 2) && s[i + 5] == ':' && digits(i + 6, 2))) {
                return false;
            }
            m.has_time = true;
            m.hour = two(i);
            m.minute = two(i + 3);
            m.second = two(i + 6);
            if (m.hour > 23 || m.minute > 59 || m.second > 60) {
                return false;
            }
            i += 8;
            if (i < s.size() && s[i] == '.') {
                ++i;
                size_t first = i;
                uint32_t scale = 100000000;
                while (i < s.size() && toml_digit(s[i])) {
                    m.ns += uint32_t(s[i] - '0') * scale;
                    scale /= 10;
                    ++i;
                }
                if (i == first) {
                    return false;
                }
            }
            if (i == s.size()) {
                return true;
            }
            if (!m.has_date) {
                return false;   // a local time has no offset
            }
            if (s[i] == 'Z' || s[i] == 'z') {
                m.has_offset = true;
                return i + 1 == s.size();
            }
            if ((s[i] == '+' || s[i] == '-') && digits(i + 1, 2) && i + 6 == s.size() && s[i + 3] == ':' && digits(i + 4, 2)) {
                int h = two(i + 1), mi = two(i + 4);
                if (h > 23 || mi > 59) {
                    return false;
                }
                m.has_offset = true;
                m.offset = (s[i] == '-' ? -1 : 1) * (h * 3600 + mi * 60);
                return true;
            }
            return false;
        }

        // A number's text: its value, the underscores and the prefixes read
        inline bool toml_integer(std::string_view s, int64_t& out) noexcept {
            bool negative = false;
            size_t i = 0;
            int base = 10;
            if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
                negative = s[0] == '-';
                i = 1;
            } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'o' || s[1] == 'b')) {
                base = s[1] == 'x' ? 16 : s[1] == 'o' ? 8 : 2;
                i = 2;
            }
            unsigned __int128 v = 0;
            for (; i < s.size(); ++i) {
                char c = s[i];
                if (c == '_') {
                    continue;
                }
                int d = toml_digit(c) ? c - '0' : (c | 32) - 'a' + 10;
                v = v * unsigned(base) + unsigned(d);
                if (v > (unsigned __int128)INT64_MAX + 1) {
                    return false;
                }
            }
            if (!negative && v > (unsigned __int128)INT64_MAX) {
                return false;
            }
            out = negative ? int64_t(0 - uint64_t(v)) : int64_t(v);
            return true;
        }

        inline optional<double> toml_float(std::string_view s) noexcept {
            std::string t;
            bool negative = false;
            size_t i = 0;
            if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
                negative = s[0] == '-';
                i = 1;
            }
            std::string_view r = s.substr(i);
            if (r == "inf") {
                return negative ? -INFINITY : INFINITY;
            }
            if (r == "nan") {
                return negative ? -NAN : NAN;
            }
            for (char c : r) {
                if (c != '_') {
                    t += c;
                }
            }
            double d = 0;
            auto res = std::from_chars(t.data(), t.data() + t.size(), d);
            if (res.ec == std::errc::result_out_of_range) {
                auto e = t.find_first_of("eE");
                d = e == std::string::npos || t[e + 1] != '-' ? INFINITY : 0.0;
            } else if (res.ec != std::errc()) {
                return nullopt;
            }
            return negative ? -d : d;
        }

        inline std::string toml_float_text(double d) noexcept {
            if (std::isnan(d)) {
                return "nan";
            }
            if (std::isinf(d)) {
                return d < 0 ? "-inf" : "inf";
            }
            char room[NumberTextSize];
            std::string t(room, number_text(room, d));
            if (t.find_first_of(".e") == std::string::npos) {
                t += ".0";
            }
            return t;
        }

        inline std::string toml_two(int v) {
            return std::string(1, char('0' + v / 10)) + char('0' + v % 10);
        }

        inline std::string toml_time_text(int64_t ns_of_day) {
            int64_t s = ns_of_day / 1000000000;
            uint32_t ns = uint32_t(ns_of_day % 1000000000);
            std::string t = toml_two(int(s / 3600)) + ":" + toml_two(int(s / 60 % 60)) + ":" + toml_two(int(s % 60));
            if (ns) {
                std::string f = std::to_string(ns + 1000000000).substr(1);
                while (f.back() == '0') {
                    f.pop_back();
                }
                t += "." + f;
            }
            return t;
        }

        inline std::string toml_date_text(const time::date& d) {
            int y = d.year();
            std::string t = std::to_string(y + 10000).substr(1);
            return t + "-" + toml_two(int(d.month())) + "-" + toml_two(d.day());
        }
    }

    // --- making ---

    inline toml::toml(double d) noexcept
    : toml(kind::floating, string(detail::toml_float_text(d))) {
    }

    inline toml::toml(const time::datetime& t) noexcept
    : toml(kind::offset_datetime, string([&] {
          int64_t offset = t.offset().nanoseconds() / 1000000000;
          std::string s = detail::toml_date_text(t.date()) + "T" + detail::toml_time_text(int64_t(t.hour()) * 3600000000000 + int64_t(t.minute()) * 60000000000 + int64_t(t.second()) * 1000000000 + t.nanosecond());
          if (offset == 0) {
              s += 'Z';
          } else {
              int64_t a = offset < 0 ? -offset : offset;
              s += offset < 0 ? '-' : '+';
              s += detail::toml_two(int(a / 3600)) + ":" + detail::toml_two(int(a / 60 % 60));
          }
          return s;
      }())) {
    }

    inline toml::toml(const time::date& d) noexcept
    : toml(kind::local_date, string(detail::toml_date_text(d))) {
    }

    inline toml toml::local_time(duration since_midnight) {
        int64_t ns = since_midnight.nanoseconds();
        if (ns < 0 || ns >= int64_t(86400) * 1000000000) {
            throw invalid_argument("sgcl::encoding::toml::local_time: a time outside a day");
        }
        return toml(kind::local_time, string(detail::toml_time_text(ns)));
    }

    inline toml toml::local_datetime(const time::date& d, duration since_midnight) {
        int64_t ns = since_midnight.nanoseconds();
        if (ns < 0 || ns >= int64_t(86400) * 1000000000) {
            throw invalid_argument("sgcl::encoding::toml::local_datetime: a time outside a day");
        }
        return toml(kind::local_datetime, string(detail::toml_date_text(d) + "T" + detail::toml_time_text(ns)));
    }

    inline toml toml::array(std::initializer_list<toml> elements) noexcept {
        return detail::TomlAccess::collection(kind::array, detail::TomlAccess::buffer(elements.begin(), elements.size()), elements.size());
    }

    inline toml toml::array(const vector<toml>& elements) noexcept {
        return detail::TomlAccess::collection(kind::array, detail::TomlAccess::buffer(elements.data(), elements.size()), elements.size());
    }

    inline toml toml::table(std::initializer_list<member> members) noexcept {
        return detail::TomlAccess::collection(kind::table, detail::TomlAccess::buffer(members.begin(), members.size()), members.size());
    }

    inline toml toml::table(const vector<member>& members) noexcept {
        return detail::TomlAccess::collection(kind::table, detail::TomlAccess::buffer(members.data(), members.size()), members.size());
    }

    inline const toml::member* toml::_members() const noexcept {
        return static_cast<const member*>(_ptr.get());
    }

    inline slice<const toml> toml::elements() const noexcept {
        if (_kind != kind::array || _bits == 0) {
            return slice<const toml>();
        }
        return slice<const toml>(_ptr, _elements(), size_t(_bits));
    }

    inline slice<const toml::member> toml::members() const noexcept {
        if (_kind != kind::table || _bits == 0) {
            return slice<const member>();
        }
        return slice<const member>(_ptr, _members(), size_t(_bits));
    }

    inline size_t toml::_find(std::string_view key) const noexcept {
        if (_kind != kind::table) {
            return size_t(-1);
        }
        const member* m = _members();
        for (size_t i = 0; i < _bits; ++i) {
            if (m[i].key.view() == key) {
                return i;
            }
        }
        return size_t(-1);
    }

    inline toml toml::operator[](const string& key) const noexcept {
        size_t i = _find(key.view());
        return i == size_t(-1) ? toml() : _members()[i].value;
    }

    inline bool toml::contains(const string& key) const noexcept {
        return _find(key.view()) != size_t(-1);
    }

    inline toml toml::set(const string& key, const toml& value) const noexcept {
        if (_kind != kind::table) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        size_t i = _find(key.view());
        if (i == size_t(-1)) {
            v.push_back(member{key, value});
        } else {
            v[i].value = value;
        }
        return table(v);
    }

    inline toml toml::set(size_t index, const toml& value) const noexcept {
        if (_kind != kind::array || index > _bits) {
            return *this;
        }
        vector<toml> v(elements().begin(), elements().end());
        if (index == _bits) {
            v.push_back(value);
        } else {
            v[index] = value;
        }
        return array(v);
    }

    inline toml toml::erase(const string& key) const noexcept {
        size_t i = _find(key.view());
        if (i == size_t(-1)) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        v.erase(v.begin() + ptrdiff_t(i));
        return table(v);
    }

    inline toml toml::push_back(const toml& value) const noexcept {
        if (_kind != kind::array) {
            return *this;
        }
        vector<toml> v(elements().begin(), elements().end());
        v.push_back(value);
        return array(v);
    }

    // --- values ---

    inline string toml::text() const noexcept {
        if (_kind == kind::table || _kind == kind::array) {
            return string();
        }
        return _text();
    }

    inline optional<bool> toml::as_bool() const noexcept {
        if (_kind != kind::boolean) {
            return nullopt;
        }
        return _text().view() == "true";
    }

    inline optional<int64_t> toml::as_int() const noexcept {
        if (_kind != kind::integer) {
            return nullopt;
        }
        int64_t v;
        if (!detail::toml_integer(_text().view(), v)) {
            return nullopt;
        }
        return v;
    }

    inline optional<double> toml::as_double() const noexcept {
        if (_kind == kind::floating) {
            return detail::toml_float(_text().view());
        }
        if (_kind == kind::integer) {
            if (auto i = as_int()) {
                return double(*i);
            }
        }
        return nullopt;
    }

    namespace detail {
        // The seconds since 1970 of the moment's clock (its UTC seconds once
        // the offset is taken off); a leap second as the second before
        inline int64_t toml_wall_seconds(const TomlMoment& m) noexcept {
            int64_t days = std::chrono::sys_days(time::date(m.year, m.month, m.day)).time_since_epoch().count();
            return days * 86400 + m.hour * 3600 + m.minute * 60 + std::min(m.second, 59);
        }

        // Whether the seconds are inside time::datetime's range (the years
        // 1677 to 2262), a day kept off either end for a zone's offset
        SGCL_INLINE_HOT bool toml_in_datetime_range(int64_t seconds) noexcept {
            return seconds > INT64_MIN / 1000000000 + 86400 && seconds < INT64_MAX / 1000000000 - 86400;
        }
    }

    inline optional<time::datetime> toml::as_datetime() const noexcept {
        if (_kind != kind::offset_datetime) {
            return nullopt;
        }
        detail::TomlMoment m;
        if (!detail::toml_moment(_text().view(), m)) {
            return nullopt;
        }
        int64_t seconds = detail::toml_wall_seconds(m) - m.offset;
        if (!detail::toml_in_datetime_range(seconds)) {
            return nullopt;
        }
        // the offset is less than a day (the parse checked it): fixed does not throw
        auto zone = time::zone::fixed(duration(std::chrono::seconds(m.offset)));
        return time::datetime::from_unix_nano(seconds * 1000000000 + m.ns, zone);
    }

    inline optional<time::datetime> toml::as_datetime(const time::zone& z) const noexcept {
        if (_kind == kind::offset_datetime) {
            auto t = as_datetime();
            return t ? optional<time::datetime>(t->in(z)) : nullopt;
        }
        if (_kind != kind::local_datetime && _kind != kind::local_date) {
            return nullopt;
        }
        detail::TomlMoment m;
        if (!detail::toml_moment(_text().view(), m) || !detail::toml_in_datetime_range(detail::toml_wall_seconds(m))) {
            return nullopt;
        }
        // the wall clock in the zone (a time in a gap or an overlap as date::at takes it)
        return time::date(m.year, m.month, m.day).at(m.hour, m.minute, std::min(m.second, 59), z) + duration(std::chrono::nanoseconds(m.ns));
    }

    inline optional<time::date> toml::as_date() const noexcept {
        if (_kind != kind::offset_datetime && _kind != kind::local_datetime && _kind != kind::local_date) {
            return nullopt;
        }
        detail::TomlMoment m;
        if (!detail::toml_moment(_text().view(), m)) {
            return nullopt;
        }
        return time::date(m.year, m.month, m.day);
    }

    inline optional<duration> toml::as_time() const noexcept {
        if (_kind != kind::offset_datetime && _kind != kind::local_datetime && _kind != kind::local_time) {
            return nullopt;
        }
        detail::TomlMoment m;
        if (!detail::toml_moment(_text().view(), m)) {
            return nullopt;
        }
        return duration(std::chrono::nanoseconds((int64_t(m.hour) * 3600 + m.minute * 60 + m.second) * 1000000000 + m.ns));
    }

    // --- equality and hash ---

    namespace detail {
        // A scalar's value as two words that are equal when the values are
        inline pair<uint64_t, uint64_t> toml_key(const toml& t) noexcept {
            switch (t.type()) {
                case toml::kind::integer:
                    return {uint64_t(t.as_int(0)), 0};
                case toml::kind::floating: {
                    double d = t.as_double(0);
                    return {std::isnan(d) ? 0x7FF8000000000000ull : std::bit_cast<uint64_t>(d == 0 ? 0.0 : d), 1};
                }
                case toml::kind::boolean:
                    return {uint64_t(t.as_bool(false)), 2};
                case toml::kind::offset_datetime: {
                    TomlMoment m;
                    toml_moment(t.text().view(), m);
                    return {uint64_t(toml_wall_seconds(m) - m.offset), uint64_t(m.ns) << 8 | 3};
                }
                case toml::kind::local_datetime:
                case toml::kind::local_date:
                case toml::kind::local_time: {
                    TomlMoment m;
                    toml_moment(t.text().view(), m);
                    uint64_t a = uint64_t(m.year) << 32 | uint64_t(m.month) << 16 | uint64_t(m.day);
                    uint64_t b = (uint64_t(m.hour) * 3600 + uint64_t(m.minute) * 60 + uint64_t(m.second)) * 1000000000 + m.ns;
                    return {a ^ (b << 1), b};
                }
                default:
                    return {t.text().hash(), 4};
            }
        }
    }

    inline bool toml::_equals(const toml& top) const noexcept {
        std::vector<pair<const toml*, const toml*>> todo{{this, &top}};
        while (!todo.empty()) {
            auto [a, b] = todo.back();
            todo.pop_back();
            if (a->_kind != b->_kind) {
                return false;
            }
            if (a->_kind == kind::array) {
                if (a->_bits != b->_bits) {
                    return false;
                }
                for (size_t i = 0; i < a->_bits; ++i) {
                    todo.push_back({a->_elements() + i, b->_elements() + i});
                }
            } else if (a->_kind == kind::table) {
                if (a->_bits != b->_bits) {
                    return false;
                }
                for (size_t i = 0; i < a->_bits; ++i) {
                    const member& m = a->_members()[i];
                    size_t j = b->_members()[i].key == m.key ? i : b->_find(m.key.view());
                    if (j == size_t(-1)) {
                        return false;
                    }
                    todo.push_back({&m.value, &b->_members()[j].value});
                }
            } else if (a->_kind == kind::string) {
                if (a->_text() != b->_text()) {
                    return false;
                }
            } else if (detail::toml_key(*a) != detail::toml_key(*b)) {
                return false;
            } else if (a->_kind == kind::local_date || a->_kind == kind::local_time || a->_kind == kind::local_datetime) {
                detail::TomlMoment x, y;
                detail::toml_moment(a->_text().view(), x);
                detail::toml_moment(b->_text().view(), y);
                if (x.year != y.year || x.month != y.month || x.day != y.day || x.hour != y.hour || x.minute != y.minute || x.second != y.second || x.ns != y.ns) {
                    return false;
                }
            }
        }
        return true;
    }

    inline size_t toml::hash() const noexcept {
        uint64_t h = 0;
        std::vector<const toml*> todo{this};
        while (!todo.empty()) {
            const toml* t = todo.back();
            todo.pop_back();
            uint64_t x = uint64_t(t->_kind) * 0x9E3779B97F4A7C15ull;
            if (t->_kind == kind::array) {
                x ^= t->_bits;
                for (size_t i = 0; i < t->_bits; ++i) {
                    todo.push_back(t->_elements() + i);
                }
            } else if (t->_kind == kind::table) {
                uint64_t sum = 0;
                for (size_t i = 0; i < t->_bits; ++i) {
                    const member& m = t->_members()[i];
                    sum += m.key.hash() * 31 + m.value.hash();
                }
                x ^= sum + t->_bits;
            } else {
                auto k = detail::toml_key(*t);
                x ^= k.first * 0xff51afd7ed558ccdull + k.second;
            }
            h = detail::mix(h ^ x);
        }
        return size_t(h);
    }

    // --- the parse ---

    namespace detail {
        class TomlParser {
        public:
            TomlParser(std::string_view text, const toml::options& o) noexcept
            : _p(text.data()), _n(text.size()), _o(o) {
                _new(0);   // the root
                _nodes[0].defined = true;
            }

            expected<toml, error> run(const string& text) noexcept {
                size_t current = 0;
                if (!_document(current)) {
                    error e = _e;
                    return unexpected<error>(std::move(e.locate(text)));
                }
                return _freeze(0);
            }

        private:
            // A member is a table still open to keys (a node: of a header,
            // a dotted key, an array of tables, an inline table being read),
            // or a value read whole (in _values): Value marks the second
            static constexpr size_t Value = size_t(1) << 63;

            struct Node {
                std::vector<pair<std::string_view, size_t>> members;   // keys in _keys
                std::unordered_map<std::string_view, size_t> index;     // past 8 members
                std::vector<size_t> elements;   // an array of tables' tables
                bool defined = false;        // by a [header] (or the root)
                bool by_dotted = false;      // made by dotted keys
                bool table_array = false;    // an array of [[tables]]
                size_t depth = 0;
            };

            const char* _p;
            size_t _n;
            size_t _at = 0;
            const toml::options& _o;
            std::vector<Node> _nodes;
            size_t _used = 0;   // the nodes in use: past them, room kept
            std::deque<std::string> _keys;   // the keys, where they stay
            vector<toml> _values;            // the values read whole
            error _e;
            bool _failed = false;

            bool _fail(errc code, size_t at, const char* text) noexcept {
                if (!_failed) {
                    _failed = true;
                    _e = error(code, at, string(text));
                }
                return false;
            }

            bool _fail(errc code, size_t at, const std::string& text) noexcept {
                return _fail(code, at, text.c_str());
            }

            SGCL_INLINE_HOT char _c(size_t k = 0) const noexcept {
                return _at + k < _n ? _p[_at + k] : '\0';
            }

            SGCL_INLINE_HOT bool _end() const noexcept {
                return _at >= _n;
            }

            void _spaces() noexcept {
                while (_c() == ' ' || _c() == '\t') {
                    ++_at;
                }
            }

            // A comment, if one, then the end of the line
            bool _line_end() noexcept {
                _spaces();
                if (_c() == '#') {
                    if (!_comment()) {
                        return false;
                    }
                }
                if (_end()) {
                    return true;
                }
                if (_c() == '\n') {
                    ++_at;
                    return true;
                }
                if (_c() == '\r' && _c(1) == '\n') {
                    _at += 2;
                    return true;
                }
                return _fail(errc::syntax, _at, "more on the line than its key and value");
            }

            bool _comment() noexcept {
                while (!_end() && _c() != '\n') {
                    auto b = uint8_t(_c());
                    if ((b < 0x20 && b != '\t' && !(b == '\r' && _c(1) == '\n')) || b == 0x7F) {
                        return _fail(errc::invalid_character, _at, "a control character in a comment");
                    }
                    ++_at;
                }
                return true;
            }

            // Blank lines, comments, and the newlines inside an array
            bool _skip_blank_lines() noexcept {
                for (;;) {
                    _spaces();
                    if (_c() == '#') {
                        if (!_comment()) {
                            return false;
                        }
                    }
                    if (_c() == '\n') {
                        ++_at;
                    } else if (_c() == '\r' && _c(1) == '\n') {
                        _at += 2;
                    } else {
                        return true;
                    }
                }
            }

            bool _document(size_t& current) noexcept {
                for (;;) {
                    if (!_skip_blank_lines()) {
                        return false;
                    }
                    if (_end()) {
                        return true;
                    }
                    if (_c() == '[') {
                        if (!_header(current)) {
                            return false;
                        }
                    } else {
                        if (!_key_value(current, false) || !_line_end()) {
                            return false;
                        }
                    }
                }
            }

            static bool _bare(char c) noexcept {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            }

            // The parts of the keys being read, one stack for every key (a
            // value's inline table reads its keys above its key's), and
            // where each began
            std::vector<std::string_view> _parts;
            std::vector<size_t> _places;

            // The parts of one key, taken off the stack when it is done
            struct Key {
                TomlParser& p;
                size_t base;
                explicit Key(TomlParser& parser) noexcept
                : p(parser), base(parser._parts.size()) {
                }
                ~Key() {
                    p._parts.resize(base);
                    p._places.resize(base);
                }
                size_t size() const noexcept {
                    return p._parts.size() - base;
                }
                std::string_view operator[](size_t i) const noexcept {
                    return p._parts[base + i];
                }
                size_t place(size_t i) const noexcept {
                    return p._places[base + i];
                }
                std::string name(size_t n) const {
                    std::string s;
                    for (size_t i = 0; i < n; ++i) {
                        if (i) {
                            s += '.';
                        }
                        s += (*this)[i];
                    }
                    return s;
                }
            };

            // A key: its parts, bare or quoted, joined by dots; a part that
            // stands as written is a view of the text, one with escapes kept
            // in _keys
            bool _key() noexcept {
                for (;;) {
                    _spaces();
                    size_t from = _at;
                    _places.push_back(from);
                    char q = _c();
                    if (q == '"' || q == '\'') {
                        size_t k = _at + 1;
                        while (k < _n) {
                            auto b = uint8_t(_p[k]);
                            if (b == uint8_t(q) || b == '\\' || (b < 0x20 && b != '\t') || b == 0x7F) {
                                break;
                            }
                            ++k;
                        }
                        if (_c(1) != q && k < _n && _p[k] == q) {
                            _parts.push_back(std::string_view(_p + from + 1, k - from - 1));
                            _at = k + 1;
                        } else {
                            std::string part;
                            bool multi = false;
                            if (!_string(part, multi)) {
                                return false;
                            }
                            if (multi) {
                                return _fail(errc::syntax, from, "a multi-line string as a key");
                            }
                            _parts.push_back(_keys.emplace_back(std::move(part)));
                        }
                    } else {
                        while (_bare(_c())) {
                            ++_at;
                        }
                        if (_at == from) {
                            return _fail(errc::syntax, _at, "a key expected");
                        }
                        _parts.push_back(std::string_view(_p + from, _at - from));
                    }
                    _spaces();
                    if (_c() != '.') {
                        return true;
                    }
                    ++_at;
                }
            }

            // A node: one an inline table made whole left behind, its room
            // kept, or a new one
            size_t _new(size_t depth) {
                if (_used == _nodes.size()) {
                    _nodes.push_back(Node{});
                } else {
                    Node& n = _nodes[_used];
                    n.members.clear();
                    n.index.clear();
                    n.elements.clear();
                    n.defined = n.by_dotted = n.table_array = false;
                }
                _nodes[_used].depth = depth;
                return _used++;
            }

            size_t _find(size_t table, std::string_view key) const noexcept {
                const Node& n = _nodes[table];
                if (!n.index.empty()) {
                    auto i = n.index.find(key);
                    return i == n.index.end() ? size_t(-1) : i->second;
                }
                for (auto& m : n.members) {
                    if (m.first == key) {
                        return m.second;
                    }
                }
                return size_t(-1);
            }

            void _add(size_t table, std::string_view k, size_t value) {
                Node& n = _nodes[table];
                n.members.push_back({k, value});
                if (n.members.size() > 8) {
                    if (n.index.empty()) {
                        for (auto& m : n.members) {
                            n.index.emplace(m.first, m.second);
                        }
                    } else {
                        n.index.emplace(k, value);
                    }
                }
            }

            bool _depth(size_t depth, size_t at) noexcept {
                if (depth > _o.max_depth) {
                    return _fail(errc::depth_limit, at, "tables and arrays nested deeper than max_depth");
                }
                return true;
            }

            bool _header(size_t& current) noexcept {
                size_t start = _at;
                bool array = _c(1) == '[';
                _at += array ? 2 : 1;
                Key parts(*this);
                if (!_key()) {
                    return false;
                }
                _spaces();
                if (_c() != ']' || (array && _c(1) != ']')) {
                    return _fail(errc::syntax, _at, array ? "no ']]' after the key of an array of tables" : "no ']' after the key of a table");
                }
                _at += array ? 2 : 1;
                if (!_line_end()) {
                    return false;
                }
                size_t t = 0;
                for (size_t i = 0; i + 1 < parts.size(); ++i) {
                    size_t next = _find(t, parts[i]);
                    if (next == size_t(-1)) {
                        if (!_depth(_nodes[t].depth + 1, parts.place(i))) {
                            return false;
                        }
                        next = _new(_nodes[t].depth + 1);
                        _add(t, parts[i], next);
                    } else if (next & Value) {
                        return _fail(errc::syntax, parts.place(i), "a table header through " + parts.name(i + 1) + ", which is no table");
                    } else if (_nodes[next].table_array) {
                        next = _nodes[next].elements.back();
                    }
                    t = next;
                }
                std::string_view last = parts[parts.size() - 1];
                size_t node = _find(t, last);
                if (array) {
                    if (node == size_t(-1)) {
                        if (!_depth(_nodes[t].depth + 2, parts.place(parts.size() - 1))) {
                            return false;
                        }
                        node = _new(_nodes[t].depth + 1);
                        _nodes[node].table_array = true;
                        _add(t, last, node);
                    } else if ((node & Value) || !_nodes[node].table_array) {
                        return _fail(errc::syntax, start, "[[" + parts.name(parts.size()) + "]] where a value of another kind is");
                    }
                    size_t element = _new(_nodes[node].depth + 1);
                    _nodes[element].defined = true;
                    _nodes[node].elements.push_back(element);
                    current = element;
                    return true;
                }
                if (node == size_t(-1)) {
                    if (!_depth(_nodes[t].depth + 1, parts.place(parts.size() - 1))) {
                        return false;
                    }
                    node = _new(_nodes[t].depth + 1);
                    _add(t, last, node);
                } else if ((node & Value) || _nodes[node].table_array || _nodes[node].defined || _nodes[node].by_dotted) {
                    return _fail(errc::duplicate_key, start, "the table " + parts.name(parts.size()) + " defined twice");
                }
                _nodes[node].defined = true;
                current = node;
                return true;
            }

            // A key and its value into the table
            bool _key_value(size_t table, bool in_inline) noexcept {
                Key parts(*this);
                if (!_key()) {
                    return false;
                }
                _spaces();
                if (_c() != '=') {
                    return _fail(errc::syntax, _at, "no '=' after a key");
                }
                ++_at;
                _spaces();
                size_t t = table;
                for (size_t i = 0; i + 1 < parts.size(); ++i) {
                    size_t next = _find(t, parts[i]);
                    if (next == size_t(-1)) {
                        if (!_depth(_nodes[t].depth + 1, parts.place(i))) {
                            return false;
                        }
                        next = _new(_nodes[t].depth + 1);
                        _nodes[next].by_dotted = true;
                        _add(t, parts[i], next);
                    } else if ((next & Value) || _nodes[next].table_array || _nodes[next].defined) {
                        return _fail(errc::duplicate_key, parts.place(i), "a dotted key through " + parts.name(i + 1) + ", defined already");
                    } else {
                        _nodes[next].by_dotted = true;   // a table made by a header's path, defined now by dotted keys
                    }
                    t = next;
                }
                if (_find(t, parts[parts.size() - 1]) != size_t(-1)) {
                    return _fail(errc::duplicate_key, parts.place(parts.size() - 1), "the key " + parts.name(parts.size()) + " defined twice");
                }
                toml value;
                if (!_value(value, _nodes[t].depth + 1)) {
                    return false;
                }
                _values.push_back(value);
                _add(t, parts[parts.size() - 1], Value | (_values.size() - 1));
                (void)in_inline;
                return true;
            }

            // The string at _at: basic, literal, and their multi-line forms
            bool _string(std::string& out, bool& multi) noexcept {
                size_t start = _at;
                char q = _c();
                multi = _c(1) == q && _c(2) == q;
                _at += multi ? 3 : 1;
                if (multi) {
                    // a newline right after the opening is trimmed
                    if (_c() == '\n') {
                        ++_at;
                    } else if (_c() == '\r' && _c(1) == '\n') {
                        _at += 2;
                    }
                }
                for (;;) {
                    // a run of plain characters at once
                    size_t run = _at;
                    while (run < _n) {
                        auto b = uint8_t(_p[run]);
                        if (b == uint8_t(q) || b == '\\' || b < 0x20 || b == 0x7F) {
                            break;
                        }
                        ++run;
                    }
                    if (run > _at) {
                        out.append(_p + _at, run - _at);
                        _at = run;
                    }
                    if (_end()) {
                        return _fail(errc::unexpected_end, start, "a string without its closing quote");
                    }
                    char c = _c();
                    if (c == q) {
                        if (!multi) {
                            ++_at;
                            return true;
                        }
                        if (_c(1) == q && _c(2) == q) {
                            // up to two more quotes belong to the content
                            size_t k = 3;
                            while (k < 5 && _c(k) == q) {
                                ++k;
                            }
                            out.append(k - 3, q);
                            _at += k;
                            return true;
                        }
                        out += c;
                        ++_at;
                        continue;
                    }
                    if (c == '\n' || (c == '\r' && _c(1) == '\n')) {
                        if (!multi) {
                            return _fail(errc::syntax, _at, "a newline in a one-line string");
                        }
                        if (c == '\r') {
                            ++_at;
                        }
                        out += '\n';
                        ++_at;
                        continue;
                    }
                    auto b = uint8_t(c);
                    if ((b < 0x20 && c != '\t') || b == 0x7F) {
                        return _fail(errc::invalid_character, _at, "a control character in a string");
                    }
                    if (c == '\\' && q == '"') {
                        char e = _c(1);
                        if (multi && (e == ' ' || e == '\t' || e == '\n' || e == '\r')) {
                            // a line-ending backslash: the whitespace up to the next content trimmed
                            size_t k = _at + 1;
                            while (k < _n && (_p[k] == ' ' || _p[k] == '\t')) {
                                ++k;
                            }
                            if (k < _n && (_p[k] == '\n' || (_p[k] == '\r' && k + 1 < _n && _p[k + 1] == '\n'))) {
                                _at = k;
                                while (!_end() && (_c() == ' ' || _c() == '\t' || _c() == '\n' || (_c() == '\r' && _c(1) == '\n'))) {
                                    _at += _c() == '\r' ? 2 : 1;
                                }
                                continue;
                            }
                            return _fail(errc::invalid_escape, _at, "a backslash before white space that is not the line's end");
                        }
                        _at += 2;
                        switch (e) {
                            case 'b': out += '\b'; break;
                            case 't': out += '\t'; break;
                            case 'n': out += '\n'; break;
                            case 'f': out += '\f'; break;
                            case 'r': out += '\r'; break;
                            case '"': out += '"'; break;
                            case '\\': out += '\\'; break;
                            case 'u':
                            case 'U': {
                                size_t digits = e == 'u' ? 4 : 8;
                                uint32_t v = 0;
                                for (size_t i = 0; i < digits; ++i) {
                                    char h = _c(i);
                                    int d = toml_digit(h) ? h - '0' : (h | 32) >= 'a' && (h | 32) <= 'f' ? (h | 32) - 'a' + 10 : -1;
                                    if (d < 0 || _end()) {
                                        return _fail(errc::invalid_escape, _at - 2, "an escape without its hex digits");
                                    }
                                    v = v * 16 + uint32_t(d);
                                }
                                if (!sgcl::utf8::valid(char32_t(v))) {
                                    return _fail(errc::invalid_escape, _at - 2, "an escape of no Unicode scalar value");
                                }
                                char b4[4];
                                out.append(b4, sgcl::utf8::encode(char32_t(v), b4));
                                _at += digits;
                                break;
                            }
                            default:
                                return _fail(errc::invalid_escape, _at - 2, "an unknown escape in a string");
                        }
                        continue;
                    }
                    out += c;
                    ++_at;
                }
            }

            static bool _delimiter(char c) noexcept {
                return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#' || c == ',' || c == ']' || c == '}';
            }

            // digits with single underscores between them
            static bool _digits(std::string_view s, bool (*ok)(char)) noexcept {
                if (s.empty() || s.front() == '_' || s.back() == '_') {
                    return false;
                }
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == '_') {
                        if (s[i + 1] == '_') {
                            return false;
                        }
                    } else if (!ok(s[i])) {
                        return false;
                    }
                }
                return true;
            }

            // A number, a boolean, a date or a time: the kind its text is of
            bool _scalar_kind(std::string_view s, toml::kind& k) noexcept {
                if (s == "true" || s == "false") {
                    k = toml::kind::boolean;
                    return true;
                }
                std::string_view r = s;
                if (!r.empty() && (r[0] == '+' || r[0] == '-')) {
                    r.remove_prefix(1);
                }
                if (r == "inf" || r == "nan") {
                    k = toml::kind::floating;
                    return true;
                }
                TomlMoment m;
                if (s.size() >= 8 && (s[2] == ':' || (s.size() >= 10 && s[4] == '-')) && toml_moment(s, m)) {
                    k = m.has_date ? (m.has_time ? (m.has_offset ? toml::kind::offset_datetime : toml::kind::local_datetime) : toml::kind::local_date) : toml::kind::local_time;
                    return true;
                }
                auto dec = [](char c) { return toml_digit(c); };
                if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'o' || s[1] == 'b')) {
                    std::string_view d = s.substr(2);
                    bool ok = s[1] == 'x' ? _digits(d, [](char c) { return toml_digit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f'); })
                            : s[1] == 'o' ? _digits(d, [](char c) { return c >= '0' && c <= '7'; })
                                          : _digits(d, [](char c) { return c == '0' || c == '1'; });
                    if (!ok) {
                        return false;
                    }
                    k = toml::kind::integer;
                    return true;
                }
                // [+-]? int (frac)? (exp)?
                size_t e = r.find_first_of("eE");
                std::string_view mant = r.substr(0, e);
                std::string_view exp = e == std::string_view::npos ? std::string_view() : r.substr(e + 1);
                size_t dot = mant.find('.');
                std::string_view ip = mant.substr(0, dot);
                std::string_view fp = dot == std::string_view::npos ? std::string_view() : mant.substr(dot + 1);
                if (!_digits(ip, dec) || (ip.size() > 1 && ip[0] == '0')) {
                    return false;
                }
                if (dot != std::string_view::npos && !_digits(fp, dec)) {
                    return false;
                }
                if (e != std::string_view::npos) {
                    if (!exp.empty() && (exp[0] == '+' || exp[0] == '-')) {
                        exp.remove_prefix(1);
                    }
                    if (!_digits(exp, dec)) {
                        return false;
                    }
                }
                k = dot == std::string_view::npos && e == std::string_view::npos ? toml::kind::integer : toml::kind::floating;
                return true;
            }

            bool _value(toml& out, size_t depth) noexcept {
                size_t start = _at;
                char c = _c();
                if (c == '"' || c == '\'') {
                    // a one-line string of plain characters: taken as it stands
                    if (_c(1) != c) {
                        size_t k = _at + 1;
                        while (k < _n) {
                            auto b = uint8_t(_p[k]);
                            if (b == uint8_t(c) || b == '\\' || (b < 0x20 && b != '\t') || b == 0x7F) {
                                break;
                            }
                            ++k;
                        }
                        if (k < _n && _p[k] == c) {
                            out = TomlAccess::scalar(toml::kind::string, string(std::string_view(_p + _at + 1, k - _at - 1)));
                            _at = k + 1;
                            return true;
                        }
                    }
                    std::string s;
                    bool multi;
                    if (!_string(s, multi)) {
                        return false;
                    }
                    out = TomlAccess::scalar(toml::kind::string, string(s));
                    return true;
                }
                if (c == '[') {
                    return _array(out, depth);
                }
                if (c == '{') {
                    return _inline_table(out, depth);
                }
                // a number, a boolean, a date or a time: to the next
                // delimiter (a date-time's space followed by a time joins)
                size_t from = _at;
                while (!_delimiter(_c())) {
                    ++_at;
                }
                if (_at - from == 10 && _c() == ' ' && toml_digit(_c(1)) && toml_digit(_c(2)) && _c(3) == ':') {
                    ++_at;
                    while (!_delimiter(_c())) {
                        ++_at;
                    }
                }
                std::string_view s(_p + from, _at - from);
                if (s.empty()) {
                    return _fail(errc::syntax, start, "a value expected");
                }
                toml::kind k;
                if (!_scalar_kind(s, k)) {
                    return _fail(errc::syntax, start, "a value that is no string, number, boolean, date or time: " + std::string(s.substr(0, 40)));
                }
                if (k == toml::kind::integer) {
                    int64_t v;
                    if (!toml_integer(s, v)) {
                        return _fail(errc::out_of_range, start, "an integer past 64 bits");
                    }
                }
                out = TomlAccess::scalar(k, string(s));
                return true;
            }

            bool _array(toml& out, size_t depth) noexcept {
                if (!_depth(depth, _at)) {
                    return false;
                }
                ++_at;
                vector<toml> items;
                for (;;) {
                    if (!_skip_blank_lines()) {
                        return false;
                    }
                    if (_c() == ']') {
                        break;
                    }
                    toml v;
                    if (!_value(v, depth + 1)) {
                        return false;
                    }
                    items.push_back(v);
                    if (!_skip_blank_lines()) {
                        return false;
                    }
                    if (_c() == ',') {
                        ++_at;
                        continue;
                    }
                    if (_c() == ']') {
                        break;
                    }
                    return _fail(_end() ? errc::unexpected_end : errc::syntax, _at, "no ',' or ']' after an element of an array");
                }
                ++_at;
                out = TomlAccess::collection(toml::kind::array, TomlAccess::buffer(items.data(), items.size()), items.size());
                return true;
            }

            // An inline table: read into a node (its dotted keys make tables
            // inside it), then made whole, a value no key extends
            bool _inline_table(toml& out, size_t depth) noexcept {
                if (!_depth(depth, _at)) {
                    return false;
                }
                ++_at;
                size_t node = _new(depth);
                _spaces();
                if (_c() != '}') {
                    for (;;) {
                        if (!_key_value(node, true)) {
                            return false;
                        }
                        _spaces();
                        if (_c() == ',') {
                            ++_at;
                            _spaces();
                            if (_c() == '}') {
                                return _fail(errc::syntax, _at, "a trailing ',' in an inline table");
                            }
                            continue;
                        }
                        if (_c() == '}') {
                            break;
                        }
                        return _fail(_end() ? errc::unexpected_end : errc::syntax, _at, "no ',' or '}' after a member of an inline table (one line)");
                    }
                }
                ++_at;
                out = _freeze(node);
                _used = node;   // its nodes and the ones under it, free again
                return true;
            }

            // A node as a value, the nodes under it first (as deep as the
            // depth limit lets them be)
            toml _freeze(size_t node) noexcept {
                size_t n = _nodes[node].table_array ? _nodes[node].elements.size() : _nodes[node].members.size();
                tracked_ptr<const void> owner;
                if (_nodes[node].table_array) {
                    if (n) {
                        toml* p = json_buffer<toml>(n, owner);
                        for (size_t i = 0; i < n; ++i) {
                            Maker<toml>::construct(p + i, _freeze(_nodes[node].elements[i]));
                        }
                    }
                    return TomlAccess::collection(toml::kind::array, std::move(owner), n);
                }
                if (n) {
                    toml::member* p = json_buffer<toml::member>(n, owner);
                    for (size_t i = 0; i < n; ++i) {
                        auto [key, ref] = _nodes[node].members[i];
                        Maker<toml::member>::construct(p + i, toml::member{string(key), ref & Value ? _values[ref & ~Value] : _freeze(ref)});
                    }
                }
                return TomlAccess::collection(toml::kind::table, std::move(owner), n);
            }
        };
    }

    inline expected<toml, toml::error> toml::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<toml, toml::error> toml::parse(const string& text, const options& o) noexcept {
        auto v = text.view();
        if (!detail::utf8_text_valid(v.data(), v.data() + v.size())) {
            size_t at = 0;
            while (at < v.size()) {
                int k = detail::utf8_sequence(v.data() + at, v.data() + v.size());
                if (k <= 0) {
                    break;
                }
                at += size_t(k);
            }
            error e(errc::invalid_utf8, at, string("invalid UTF-8"));
            return unexpected<error>(std::move(e.locate(text)));
        }
        return detail::TomlParser(v, o).run(text);
    }

    inline expected<toml, toml::error> toml::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<toml, toml::error> toml::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<toml, toml::error>> toml::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<toml, toml::error>> toml::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    // --- writing ---

    namespace detail {
        class TomlWriter {
        public:
            std::string out;

            static bool bare(std::string_view k) noexcept {
                if (k.empty()) {
                    return false;
                }
                for (char c : k) {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                        return false;
                    }
                }
                return true;
            }

            void quoted(std::string_view s) {
                static constexpr char digits[] = "0123456789ABCDEF";
                out += '"';
                for (char c : s) {
                    auto b = uint8_t(c);
                    switch (c) {
                        case '"': out += "\\\""; continue;
                        case '\\': out += "\\\\"; continue;
                        case '\n': out += "\\n"; continue;
                        case '\t': out += "\\t"; continue;
                        case '\r': out += "\\r"; continue;
                        case '\b': out += "\\b"; continue;
                        case '\f': out += "\\f"; continue;
                        default: break;
                    }
                    if (b < 0x20 || b == 0x7F) {
                        out += "\\u00";
                        out += digits[b >> 4];
                        out += digits[b & 15];
                    } else {
                        out += c;
                    }
                }
                out += '"';
            }

            void key(std::string_view k) {
                if (bare(k)) {
                    out.append(k);
                } else {
                    quoted(k);
                }
            }

            void path(const std::vector<std::string>& keys) {
                for (size_t i = 0; i < keys.size(); ++i) {
                    if (i) {
                        out += '.';
                    }
                    key(keys[i]);
                }
            }

            // A value inline (an array, an inline table, a scalar)
            void inline_value(const toml& top, size_t depth) {
                struct Frame {
                    const toml* v;
                    size_t at;
                };
                std::vector<Frame> stack;
                auto one = [&](const toml& v) {
                    // a collection at its depth (a scalar has none)
                    if ((v.is_array() || v.is_table()) && stack.size() + depth > 512) {
                        throw invalid_argument("sgcl::encoding::toml::to_string: a value deeper than 512 levels");
                    }
                    if (v.is_array()) {
                        out += '[';
                        stack.push_back({&v, 0});
                    } else if (v.is_table()) {
                        out += '{';
                        stack.push_back({&v, 0});
                    } else if (v.is_string()) {
                        quoted(v.text().view());
                    } else {
                        out.append(v.text().view());
                    }
                };
                one(top);
                while (!stack.empty()) {
                    Frame& f = stack.back();
                    const toml* v = f.v;
                    if (f.at == v->size()) {
                        out += v->is_array() ? "]" : (v->size() ? " }" : "}");
                        stack.pop_back();
                        continue;
                    }
                    size_t i = f.at++;
                    if (v->is_array()) {
                        if (i) {
                            out += ", ";
                        }
                        one(v->elements()[i]);
                    } else {
                        out += i ? ", " : " ";
                        key(v->members()[i].key.view());
                        out += " = ";
                        one(v->members()[i].value);
                    }
                }
            }

            static bool table_array(const toml& v) noexcept {
                if (!v.is_array() || v.empty()) {
                    return false;
                }
                for (auto& e : v.elements()) {
                    if (!e.is_table()) {
                        return false;
                    }
                }
                return true;
            }

            // A table of tables alone needs no header: theirs make it
            static bool only_tables(const toml& t) noexcept {
                if (t.empty()) {
                    return false;
                }
                for (auto& m : t.members()) {
                    if (!m.value.is_table() && !table_array(m.value)) {
                        return false;
                    }
                }
                return true;
            }

            // A table's body under its header: its values, then its tables
            void table(const toml& t, std::vector<std::string>& keys, bool header_written) {
                if (keys.size() > 512) {
                    throw invalid_argument("sgcl::encoding::toml::to_string: a value deeper than 512 levels");
                }
                bool any_value = false;
                for (auto& m : t.members()) {
                    if (m.value.is_table() || table_array(m.value)) {
                        continue;
                    }
                    key(m.key.view());
                    out += " = ";
                    inline_value(m.value, keys.size() + 1);
                    out += '\n';
                    any_value = true;
                }
                (void)any_value;
                (void)header_written;
                for (auto& m : t.members()) {
                    if (m.value.is_table()) {
                        keys.push_back(std::string(m.key.view()));
                        if (!only_tables(m.value)) {
                            if (!out.empty()) {
                                out += '\n';
                            }
                            out += '[';
                            path(keys);
                            out += "]\n";
                        }
                        table(m.value, keys, true);
                        keys.pop_back();
                    } else if (table_array(m.value)) {
                        keys.push_back(std::string(m.key.view()));
                        for (auto& e : m.value.elements()) {
                            if (!out.empty()) {
                                out += '\n';
                            }
                            out += "[[";
                            path(keys);
                            out += "]]\n";
                            table(e, keys, true);
                        }
                        keys.pop_back();
                    }
                }
            }
        };
    }

    inline string toml::to_string() const {
        if (!is_table()) {
            throw invalid_argument("sgcl::encoding::toml::to_string: a document is a table");
        }
        detail::TomlWriter w;
        std::vector<std::string> keys;
        w.table(*this, keys, false);
        return string(w.out);
    }

    // --- JSON ---

    inline toml toml::from_json(const json& top) noexcept {
        struct Frame {
            const json* src;
            size_t next;
            vector<toml> items;
            std::vector<size_t> kept;   // the members of an object kept (null dropped)
        };
        auto scalar = [](const json& j, int& what) -> toml {
            what = 0;
            switch (j.type()) {
                case json::kind::null: what = 2; return toml();
                case json::kind::boolean: return toml(*j.as_bool());
                case json::kind::number:
                    if (auto i = j.as_int()) {
                        return toml(*i);
                    }
                    return toml(j.as_double(0.0));
                case json::kind::string: return toml(*j.as_string());
                default:
                    what = 1;
                    return toml();
            }
        };
        int what;
        toml first = scalar(top, what);
        if (what != 1) {
            return first;
        }
        vector<Frame> stack;
        stack.push_back(Frame{&top, 0, {}, {}});
        toml result;
        while (!stack.empty()) {
            Frame& f = stack.back();
            size_t n = f.src->size();
            if (f.next == n) {
                toml made;
                if (f.src->type() == json::kind::array) {
                    made = array(f.items);
                } else {
                    vector<member> ms;
                    auto src = f.src->members();
                    for (size_t i = 0; i < f.kept.size(); ++i) {
                        ms.push_back(member{src[f.kept[i]].key, f.items[i]});
                    }
                    made = table(ms);
                }
                stack.pop_back();
                if (stack.empty()) {
                    result = made;
                } else {
                    stack.back().items.push_back(made);
                }
                continue;
            }
            size_t index = f.next++;
            const json& child = f.src->type() == json::kind::array ? f.src->elements()[index] : f.src->members()[index].value;
            toml c = scalar(child, what);
            if (what == 2) {
                continue;   // null: no TOML
            }
            if (f.src->type() == json::kind::object) {
                f.kept.push_back(index);
            }
            if (what == 1) {
                stack.push_back(Frame{&child, 0, {}, {}});
            } else {
                f.items.push_back(c);
            }
        }
        return result;
    }

    inline json toml::to_json() const noexcept {
        struct Frame {
            const toml* src;
            size_t next;
            vector<json> items;
        };
        auto scalar = [](const toml& t, bool& container) -> json {
            container = false;
            switch (t.type()) {
                case kind::string: return json(*t.as_string());
                case kind::integer: return json(t.as_int(0));
                case kind::floating: {
                    double d = t.as_double(0);
                    return std::isfinite(d) ? json(d) : json();
                }
                case kind::boolean: return json(t.as_bool(false));
                case kind::table:
                case kind::array:
                    container = true;
                    return json();
                default:
                    return json(t.text());
            }
        };
        bool container;
        json first = scalar(*this, container);
        if (!container) {
            return first;
        }
        vector<Frame> stack;
        stack.push_back(Frame{this, 0, {}});
        json result;
        while (!stack.empty()) {
            Frame& f = stack.back();
            size_t n = f.src->size();
            if (f.next == n) {
                json made;
                if (f.src->is_array()) {
                    made = json::array(f.items);
                } else if (n == 0) {
                    made = json::object({});
                } else {
                    json::builder b;
                    auto ms = f.src->members();
                    for (size_t i = 0; i < n; ++i) {
                        b.set(ms[i].key, f.items[i]);
                    }
                    made = b.build();
                }
                stack.pop_back();
                if (stack.empty()) {
                    result = made;
                } else {
                    stack.back().items.push_back(made);
                }
                continue;
            }
            const toml* child = f.src->is_array() ? f.src->_elements() + f.next : &f.src->_members()[f.next].value;
            ++f.next;
            json j = scalar(*child, container);
            if (container) {
                stack.push_back(Frame{child, 0, {}});
            } else {
                f.items.push_back(j);
            }
        }
        return result;
    }

    namespace detail {
        // A toml's save in a task: the value copied into the frame
        inline async::task<expected<void, toml::error>> toml_save_task(string path, toml value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<toml, toml::error> toml::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return toml::parse(in); });
    }

    inline async::task<expected<toml, toml::error>> toml::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return toml::load(path); });
    }

    inline expected<void, toml::error> toml::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, toml::error>> toml::async_save(string path) const noexcept {
        return detail::toml_save_task(std::move(path), *this);
    }
}

template<>
struct std::hash<sgcl::encoding::toml> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::toml& t) const noexcept {
        return t.hash();
    }
};
