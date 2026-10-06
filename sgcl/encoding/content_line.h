//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "recurrence.h"
#include "detail/utf8_check.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../time/date.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::encoding {
    namespace detail {
        struct ContentLineData;
        struct ContentLineAccess;
    }

    // One content line of iCalendar (RFC 5545 §3.1) and vCard (RFC 6350
    // §3.3): a name, its parameters and a value, NAME;PARAM=a,b:value, and in
    // a vCard a group before the name (item1.EMAIL). Immutable, one word
    // shared by copying; what icalendar::property and vcard::property name.
    // The value is kept as written: text() reads its escapes, the as_*
    // functions its other types.
    class content_line {
    public:
        using error = encoding::error;

        // A parameter: its name, upper-cased, and its values (a list
        // separated by commas, quotes taken off, RFC 6868's ^n ^^ ^' read)
        struct parameter {
            string name;
            vector<string> values;
        };

        // No name, no value
        content_line() noexcept;

        // The value as it is written in a file: escapes kept (a TEXT made
        // of any characters goes through text())
        content_line(const string& name, const string& value) noexcept;
        content_line(const string& name, const vector<parameter>& params, const string& value) noexcept;

        // A line of a TEXT value: the text with \ ; , and line breaks escaped
        static content_line text(const string& name, const string& text) noexcept;

        // One line, unfolded: [group.]NAME[;PARAM=...]:value
        static expected<content_line, error> parse(const string& line) noexcept;

        string group() const noexcept;
        string name() const noexcept;
        string value() const noexcept;
        slice<const parameter> params() const noexcept;

        // A parameter's first value (its name in any case)
        optional<string> param(const string& name) const noexcept;
        string param(const string& name, const string& fallback) const noexcept;

        // New lines: the parameter set to one value (in its place or at the
        // end), the group set
        content_line with_param(const string& name, const string& value) const noexcept;
        content_line with_group(const string& group) const noexcept;

        // The value as TEXT: \n and \N a line break, \\ \; \, the character
        // (a backslash before another character dropped)
        string text() const noexcept;
        // A list of TEXT: split at the commas no backslash escapes
        vector<string> list() const noexcept;
        // A structured value (N, ADR, REQUEST-STATUS): split at the
        // semicolons no backslash escapes, each read as TEXT
        vector<string> components() const noexcept;

        optional<int64_t> as_int() const noexcept;
        optional<bool> as_bool() const noexcept;          // TRUE or FALSE, any case
        // A DATE (19970714), or the date of a DATE-TIME as written
        optional<time::date> as_date() const noexcept;
        // A DATE-TIME in UTC (a Z), or of a TZID the system knows (an IANA
        // name); nullopt for a floating one, a DATE, or another TZID (a
        // calendar's own VTIMEZONE: icalendar::datetime_of reads it)
        optional<time::datetime> as_datetime() const noexcept;
        // The same, a floating DATE-TIME as that clock in the zone given
        optional<time::datetime> as_datetime(const time::zone& floating) const noexcept;
        // A DURATION (P1W, -PT15M, P1DT2H)
        optional<duration> as_duration() const noexcept;
        // A UTC-OFFSET (+0530, -080000)
        optional<duration> as_utc_offset() const noexcept;
        // An RRULE's value
        optional<recurrence> as_recurrence() const noexcept;

        // The line as a file holds it: folded at 75 octets (never inside a
        // UTF-8 sequence), each part ended by CRLF; a parameter value with
        // ':', ';' or ',' in quotes, a line break, '^' and '"' as RFC
        // 6868's escapes. invalid_argument for a name that is none
        string to_string() const;

        // The same group, name, parameters and value (names in any case)
        friend bool operator==(const content_line& a, const content_line& b) noexcept;

    private:
        friend struct detail::ContentLineAccess;

        tracked_ptr<const detail::ContentLineData> _data;

        const detail::ContentLineData& _d() const noexcept;
    };

    namespace detail {
        struct ContentLineData {
            string group;
            string name;
            vector<content_line::parameter> params;
            string value;
        };

        SGCL_INLINE_HOT bool cl_name_char(char c) noexcept {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        }

        inline std::string cl_upper(std::string_view s) {
            std::string u(s);
            for (char& c : u) {
                c = c >= 'a' && c <= 'z' ? char(c - 32) : c;
            }
            return u;
        }

        inline bool cl_valid_name(std::string_view s) noexcept {
            if (s.empty()) {
                return false;
            }
            for (char c : s) {
                if (!cl_name_char(c)) {
                    return false;
                }
            }
            return true;
        }

        // A character a value may hold: any but the controls (tab allowed)
        SGCL_INLINE_HOT bool cl_value_char(unsigned char c) noexcept {
            return c >= 0x20 ? c != 0x7F : c == '\t';
        }

        struct ContentLineAccess {
            static content_line make(string group, string name, vector<content_line::parameter> params, string value) noexcept {
                auto d = make_tracked<ContentLineData>();
                d->group = std::move(group);
                d->name = std::move(name);
                d->params = std::move(params);
                d->value = std::move(value);
                content_line l;
                l._data = std::move(d);
                return l;
            }

            static const ContentLineData& data(const content_line& l) noexcept {
                return l._d();
            }
        };

        // One unfolded line at text[0..): its parts, or the place and why
        // not. vcard: a group before the name, and vCard 3.0's parameter
        // without '=' (TEL;HOME:) a TYPE
        struct ContentLineParse {
            size_t error_at = 0;
            const char* why = nullptr;
            errc code = errc::syntax;
        };

        inline bool cl_parse(std::string_view s, bool vcard, string& group, string& name, vector<content_line::parameter>& params, string& value,
                             ContentLineParse& e) noexcept {
            auto fail = [&](size_t at, const char* why, errc code = errc::syntax) {
                e.error_at = at;
                e.why = why;
                e.code = code;
                return false;
            };
            size_t i = 0;
            while (i < s.size() && cl_name_char(s[i])) {
                ++i;
            }
            if (i == 0) {
                return fail(0, "a line that does not start with a name");
            }
            if (i < s.size() && s[i] == '.') {
                if (!vcard) {
                    return fail(i, "a '.' in a name (a group is vCard's)");
                }
                group = string(std::string(s.substr(0, i)));
                size_t from = ++i;
                while (i < s.size() && cl_name_char(s[i])) {
                    ++i;
                }
                if (i == from) {
                    return fail(i, "a group without its name");
                }
                name = string(cl_upper(s.substr(from, i - from)));
            } else {
                name = string(cl_upper(s.substr(0, i)));
            }
            while (i < s.size() && s[i] == ';') {
                size_t from = ++i;
                while (i < s.size() && cl_name_char(s[i])) {
                    ++i;
                }
                if (i == from) {
                    return fail(i, "a parameter without its name");
                }
                content_line::parameter p;
                p.name = string(cl_upper(s.substr(from, i - from)));
                if (i >= s.size() || s[i] != '=') {
                    if (vcard && i < s.size() && (s[i] == ';' || s[i] == ':')) {
                        // vCard 3.0 and 2.1: a bare value is a TYPE
                        p.values.push_back(p.name);
                        p.name = string("TYPE");
                        params.push_back(std::move(p));
                        continue;
                    }
                    return fail(i, "no '=' after a parameter's name");
                }
                ++i;
                for (;;) {
                    std::string v;
                    if (i < s.size() && s[i] == '"') {
                        size_t q = ++i;
                        while (i < s.size() && s[i] != '"') {
                            if (!cl_value_char(uint8_t(s[i]))) {
                                return fail(i, "a control character in a parameter", errc::invalid_character);
                            }
                            ++i;
                        }
                        if (i >= s.size()) {
                            return fail(q - 1, "a quoted parameter value without its closing quote", errc::unexpected_end);
                        }
                        v.assign(s.substr(q, i - q));
                        ++i;
                    } else {
                        size_t q = i;
                        while (i < s.size() && s[i] != ';' && s[i] != ':' && s[i] != ',' && s[i] != '"') {
                            if (!cl_value_char(uint8_t(s[i]))) {
                                return fail(i, "a control character in a parameter", errc::invalid_character);
                            }
                            ++i;
                        }
                        if (i < s.size() && s[i] == '"') {
                            return fail(i, "a '\"' inside a parameter value");
                        }
                        v.assign(s.substr(q, i - q));
                    }
                    // RFC 6868: ^n a line break, ^^ a caret, ^' a quote
                    std::string r;
                    for (size_t k = 0; k < v.size(); ++k) {
                        if (v[k] == '^' && k + 1 < v.size() && (v[k + 1] == 'n' || v[k + 1] == '^' || v[k + 1] == '\'')) {
                            r += v[k + 1] == 'n' ? '\n' : v[k + 1] == '^' ? '^' : '"';
                            ++k;
                        } else {
                            r += v[k];
                        }
                    }
                    p.values.push_back(string(r));
                    if (i < s.size() && s[i] == ',') {
                        ++i;
                        continue;
                    }
                    break;
                }
                params.push_back(std::move(p));
            }
            if (i >= s.size() || s[i] != ':') {
                return fail(i, "no ':' before the value");
            }
            ++i;
            for (size_t k = i; k < s.size(); ++k) {
                if (!cl_value_char(uint8_t(s[k]))) {
                    return fail(k, "a control character in a value", errc::invalid_character);
                }
            }
            value = string(std::string(s.substr(i)));
            return true;
        }

        // TEXT's escapes read
        inline std::string cl_unescape(std::string_view s) {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    char c = s[++i];
                    out += c == 'n' || c == 'N' ? '\n' : c;
                } else {
                    out += s[i];
                }
            }
            return out;
        }

        inline std::string cl_escape(std::string_view s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '\\': out += "\\\\"; break;
                    case ';': out += "\\;"; break;
                    case ',': out += "\\,"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': break;   // CRLF in a text: one line break
                    default: out += c;
                }
            }
            return out;
        }

        // Split at the separators no backslash escapes, each unescaped
        inline vector<string> cl_split(std::string_view s, char sep) {
            vector<string> out;
            size_t from = 0;
            for (size_t i = 0; i <= s.size(); ++i) {
                if (i == s.size() || s[i] == sep) {
                    out.push_back(string(cl_unescape(s.substr(from, i - from))));
                    from = i + 1;
                } else if (s[i] == '\\') {
                    ++i;
                }
            }
            return out;
        }

        // RFC 5545's DURATION: [+-]P(nW | nD[T..] | T..), T: nH[nM[nS]] | nM[nS] | nS
        inline optional<duration> cl_duration(std::string_view s) noexcept {
            size_t i = 0;
            bool neg = false;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
                neg = s[i] == '-';
                ++i;
            }
            if (i >= s.size() || s[i] != 'P') {
                return nullopt;
            }
            ++i;
            auto number = [&](int64_t& v) {
                size_t from = i;
                v = 0;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9' && i - from < 12) {
                    v = v * 10 + (s[i] - '0');
                    ++i;
                }
                return i > from && !(i < s.size() && s[i] >= '0' && s[i] <= '9');
            };
            int64_t seconds = 0, v;
            if (i < s.size() && s[i] != 'T') {
                if (!number(v) || i >= s.size()) {
                    return nullopt;
                }
                if (s[i] == 'W') {
                    ++i;
                    if (i != s.size()) {
                        return nullopt;
                    }
                    seconds = v * 7 * 86400;
                    return duration(std::chrono::seconds(neg ? -seconds : seconds));
                }
                if (s[i] != 'D') {
                    return nullopt;
                }
                ++i;
                seconds = v * 86400;
                if (i == s.size()) {
                    return duration(std::chrono::seconds(neg ? -seconds : seconds));
                }
            }
            if (i >= s.size() || s[i] != 'T') {
                return nullopt;
            }
            ++i;
            static constexpr char units[] = {'H', 'M', 'S'};
            static constexpr int64_t scale[] = {3600, 60, 1};
            int next = 0;
            bool any = false;
            while (i < s.size()) {
                if (!number(v) || i >= s.size()) {
                    return nullopt;
                }
                int u = s[i] == 'H' ? 0 : s[i] == 'M' ? 1 : s[i] == 'S' ? 2 : -1;
                // H then M then S, none skipped between the first and the last
                if (u < next || (any && u != next)) {
                    return nullopt;
                }
                seconds += v * scale[u];
                next = u + 1;
                any = true;
                ++i;
            }
            if (!any) {
                return nullopt;
            }
            return duration(std::chrono::seconds(neg ? -seconds : seconds));
        }

        inline optional<duration> cl_utc_offset(std::string_view s) noexcept {
            if ((s.size() != 5 && s.size() != 7) || (s[0] != '+' && s[0] != '-')) {
                return nullopt;
            }
            int f[3] = {0, 0, 0};
            for (size_t k = 0; k + 1 < s.size(); k += 2) {
                char a = s[k + 1], b = s[k + 2];
                if (a < '0' || a > '9' || b < '0' || b > '9') {
                    return nullopt;
                }
                f[k / 2] = (a - '0') * 10 + (b - '0');
            }
            if (f[0] > 23 || f[1] > 59 || f[2] > 59) {
                return nullopt;
            }
            int64_t sec = f[0] * 3600 + f[1] * 60 + f[2];
            if (s[0] == '-' && sec == 0) {
                return nullopt;   // "-0000" is not allowed
            }
            return duration(std::chrono::seconds(s[0] == '-' ? -sec : sec));
        }

        // A wall clock in a zone of sgcl::time, RFC 5545 §3.3.5's way
        inline time::datetime cl_at(int64_t wall, const time::zone& z) noexcept {
            int64_t day = rec_floor_div(wall, 86400);
            int64_t sod = wall - day * 86400;
            return time::date(std::chrono::sys_days(std::chrono::days(day))).at(int(sod / 3600), int(sod / 60 % 60), int(sod % 60), z);
        }

        // Whether a wall clock is inside time::datetime's years
        SGCL_INLINE_HOT bool cl_in_range(int64_t wall) noexcept {
            return wall > INT64_MIN / 1000000000 + 2 * 86400 && wall < INT64_MAX / 1000000000 - 2 * 86400;
        }

        // Folding of RFC 5545 §3.1: at most 75 octets a physical line, a
        // continuation starting with a space, never inside a UTF-8 sequence
        inline void cl_fold(std::string_view line, std::string& out) {
            size_t at = 0;
            size_t room = 75;
            while (line.size() - at > room) {
                size_t cut = at + room;
                while (cut > at && (uint8_t(line[cut]) & 0xC0) == 0x80) {
                    --cut;
                }
                out.append(line.substr(at, cut - at));
                out += "\r\n ";
                at = cut;
                room = 74;
            }
            out.append(line.substr(at));
            out += "\r\n";
        }
    }

    inline content_line::content_line() noexcept
    : _data(make_tracked<detail::ContentLineData>()) {
    }

    inline content_line::content_line(const string& name, const string& value) noexcept
    : content_line(detail::ContentLineAccess::make(string(), string(detail::cl_upper(name.view())), {}, value)) {
    }

    inline content_line::content_line(const string& name, const vector<parameter>& params, const string& value) noexcept
    : content_line(detail::ContentLineAccess::make(string(), string(detail::cl_upper(name.view())), [&] {
          vector<parameter> v;
          for (const parameter& p : params) {
              v.push_back(parameter{string(detail::cl_upper(p.name.view())), p.values});
          }
          return v;
      }(), value)) {
    }

    inline content_line content_line::text(const string& name, const string& text) noexcept {
        return content_line(name, string(detail::cl_escape(text.view())));
    }

    inline const detail::ContentLineData& content_line::_d() const noexcept {
        return *_data;
    }

    inline string content_line::group() const noexcept {
        return _d().group;
    }

    inline string content_line::name() const noexcept {
        return _d().name;
    }

    inline string content_line::value() const noexcept {
        return _d().value;
    }

    inline slice<const content_line::parameter> content_line::params() const noexcept {
        return _d().params.as_slice();
    }

    inline optional<string> content_line::param(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        for (const parameter& p : _d().params) {
            if (p.name.view() == u && !p.values.empty()) {
                return p.values[0];
            }
        }
        return nullopt;
    }

    inline string content_line::param(const string& name, const string& fallback) const noexcept {
        auto p = param(name);
        return p ? *p : fallback;
    }

    inline content_line content_line::with_param(const string& name, const string& value) const noexcept {
        const auto& d = _d();
        string u(detail::cl_upper(name.view()));
        vector<parameter> ps(d.params.begin(), d.params.end());
        vector<string> vs;
        vs.push_back(value);
        bool found = false;
        for (parameter& p : ps) {
            if (p.name == u) {
                p.values = vs;
                found = true;
                break;
            }
        }
        if (!found) {
            ps.push_back(parameter{u, vs});
        }
        return detail::ContentLineAccess::make(d.group, d.name, ps, d.value);
    }

    inline content_line content_line::with_group(const string& group) const noexcept {
        const auto& d = _d();
        return detail::ContentLineAccess::make(group, d.name, d.params, d.value);
    }

    inline expected<content_line, content_line::error> content_line::parse(const string& line) noexcept {
        string group, name, value;
        vector<parameter> params;
        detail::ContentLineParse e;
        auto v = line.view();
        if (!detail::utf8_text_valid(v.data(), v.data() + v.size())) {
            error err(errc::invalid_utf8, 0, string("invalid UTF-8"));
            return unexpected<error>(std::move(err));
        }
        if (!detail::cl_parse(v, true, group, name, params, value, e)) {
            error err(e.code, e.error_at, string(e.why));
            return unexpected<error>(std::move(err.locate(line)));
        }
        return detail::ContentLineAccess::make(group, name, params, value);
    }

    inline string content_line::text() const noexcept {
        return string(detail::cl_unescape(_d().value.view()));
    }

    inline vector<string> content_line::list() const noexcept {
        return detail::cl_split(_d().value.view(), ',');
    }

    inline vector<string> content_line::components() const noexcept {
        return detail::cl_split(_d().value.view(), ';');
    }

    inline optional<int64_t> content_line::as_int() const noexcept {
        std::string_view s = _d().value.view();
        size_t i = !s.empty() && (s[0] == '+' || s[0] == '-') ? 1 : 0;
        if (i == s.size() || s.size() - i > 18) {
            return nullopt;
        }
        int64_t v = 0;
        for (size_t k = i; k < s.size(); ++k) {
            if (s[k] < '0' || s[k] > '9') {
                return nullopt;
            }
            v = v * 10 + (s[k] - '0');
        }
        return s[0] == '-' ? -v : v;
    }

    inline optional<bool> content_line::as_bool() const noexcept {
        std::string u = detail::cl_upper(_d().value.view());
        if (u == "TRUE") {
            return true;
        }
        if (u == "FALSE") {
            return false;
        }
        return nullopt;
    }

    inline optional<time::date> content_line::as_date() const noexcept {
        detail::RecMoment m;
        if (!detail::rec_moment(_d().value.view(), m)) {
            return nullopt;
        }
        return time::date(std::chrono::sys_days(std::chrono::days(detail::rec_floor_div(m.wall, 86400))));
    }

    inline optional<time::datetime> content_line::as_datetime() const noexcept {
        detail::RecMoment m;
        if (!detail::rec_moment(_d().value.view(), m) || m.date || !detail::cl_in_range(m.wall)) {
            return nullopt;
        }
        if (m.utc) {
            return time::datetime::from_unix(m.wall, time::zone::utc());
        }
        auto tzid = param("TZID");
        if (!tzid) {
            return nullopt;
        }
        auto z = time::zone::load(*tzid);
        if (!z) {
            return nullopt;
        }
        return detail::cl_at(m.wall, *z);
    }

    inline optional<time::datetime> content_line::as_datetime(const time::zone& floating) const noexcept {
        detail::RecMoment m;
        if (!detail::rec_moment(_d().value.view(), m) || m.date || !detail::cl_in_range(m.wall)) {
            return nullopt;
        }
        if (m.utc || param("TZID")) {
            return as_datetime();
        }
        return detail::cl_at(m.wall, floating);
    }

    inline optional<duration> content_line::as_duration() const noexcept {
        return detail::cl_duration(_d().value.view());
    }

    inline optional<duration> content_line::as_utc_offset() const noexcept {
        return detail::cl_utc_offset(_d().value.view());
    }

    inline optional<recurrence> content_line::as_recurrence() const noexcept {
        auto r = recurrence::parse(_d().value);
        if (!r) {
            return nullopt;
        }
        return *r;
    }

    inline string content_line::to_string() const {
        const auto& d = _d();
        if (!detail::cl_valid_name(d.name.view()) || (!d.group.empty() && !detail::cl_valid_name(d.group.view()))) {
            throw invalid_argument("sgcl::encoding::content_line::to_string: a name or a group that is none");
        }
        std::string line;
        if (!d.group.empty()) {
            line.append(d.group.view());
            line += '.';
        }
        line.append(d.name.view());
        for (const parameter& p : d.params) {
            if (!detail::cl_valid_name(p.name.view())) {
                throw invalid_argument("sgcl::encoding::content_line::to_string: a parameter's name that is none");
            }
            line += ';';
            line.append(p.name.view());
            line += '=';
            for (size_t i = 0; i < p.values.size(); ++i) {
                if (i) {
                    line += ',';
                }
                std::string v;
                bool quote = false;
                for (char c : p.values[i].view()) {
                    switch (c) {
                        case '\n': v += "^n"; break;
                        case '^': v += "^^"; break;
                        case '"': v += "^'"; break;
                        case '\r': break;
                        default:
                            quote = quote || c == ':' || c == ';' || c == ',';
                            v += uint8_t(c) < 0x20 && c != '\t' ? ' ' : c;
                    }
                }
                if (quote) {
                    line += '"';
                    line += v;
                    line += '"';
                } else {
                    line += v;
                }
            }
        }
        line += ':';
        for (char c : d.value.view()) {
            line += detail::cl_value_char(uint8_t(c)) ? c : ' ';   // a control character has no place in a value
        }
        std::string out;
        detail::cl_fold(line, out);
        return string(out);
    }

    inline bool operator==(const content_line& a, const content_line& b) noexcept {
        const auto& x = a._d();
        const auto& y = b._d();
        if (x.group != y.group || x.name != y.name || x.value != y.value || x.params.size() != y.params.size()) {
            return false;
        }
        for (size_t i = 0; i < x.params.size(); ++i) {
            const auto& p = x.params[i];
            const auto& q = y.params[i];
            if (p.name != q.name || p.values.size() != q.values.size() || !std::equal(p.values.begin(), p.values.end(), q.values.begin())) {
                return false;
            }
        }
        return true;
    }

}
