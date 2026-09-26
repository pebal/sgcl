//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "ip.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "../txt/idna.h"
#include "../txt/percent.h"

#include <algorithm>
#include <charconv>
#include <compare>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }

    // application/x-www-form-urlencoded (the WHATWG URL Standard, §5): a
    // list of name and value pairs in their order, a name as often as it
    // comes, as the query of a URL and the body of an HTML form carry them.
    // Go's url.Values, JavaScript's URLSearchParams.
    //
    // parse never fails: a '+' is a space, an escape that is not %XX stays
    // as it is written, bytes that are not UTF-8 after the unescaping
    // become U+FFFD, all as the standard says. to_string writes a space as
    // '+' and escapes everything but the letters, the digits and * - . _
    // A lookup by name walks the list, as in the standard: a query is a
    // handful of pairs, and there is no hash to be steered by the sender.
    class query_params {
    public:
        query_params() = default;

        // "a=1&b=2&a=3"; a leading '?' is taken off first, as
        // URLSearchParams does
        static query_params parse(const string& text);

        // The first value of the name, or "" when there is none (has()
        // tells the two apart)
        string get(const string& name) const {
            for (auto& p : _pairs) {
                if (p.first == name) {
                    return p.second;
                }
            }
            return string();
        }

        // Every value of the name, in their order
        vector<string> get_all(const string& name) const {
            vector<string> out;
            for (auto& p : _pairs) {
                if (p.first == name) {
                    out.push_back(p.second);
                }
            }
            return out;
        }

        bool contains(const string& name) const {
            for (auto& p : _pairs) {
                if (p.first == name) {
                    return true;
                }
            }
            return false;
        }

        // A pair at the end
        query_params& add(const string& name, const string& value) {
            _pairs.push_back(pair<string, string>(name, value));
            return *this;
        }

        // The first pair of the name takes the value and the others of
        // the name go; a pair at the end when there was none
        query_params& set(const string& name, const string& value) {
            bool found = false;
            vector<pair<string, string>> kept;
            kept.reserve(_pairs.size() + 1);
            for (auto& p : _pairs) {
                if (p.first != name) {
                    kept.push_back(p);
                } else if (!found) {
                    found = true;
                    kept.push_back(pair<string, string>(name, value));
                }
            }
            if (!found) {
                kept.push_back(pair<string, string>(name, value));
            }
            _pairs = std::move(kept);
            return *this;
        }

        // Every pair of the name
        query_params& erase(const string& name) {
            vector<pair<string, string>> kept;
            kept.reserve(_pairs.size());
            for (auto& p : _pairs) {
                if (p.first != name) {
                    kept.push_back(p);
                }
            }
            _pairs = std::move(kept);
            return *this;
        }

        size_t size() const noexcept {
            return _pairs.size();
        }

        bool empty() const noexcept {
            return _pairs.empty();
        }

        // The pairs, (name, value), in their order
        auto begin() const noexcept {
            return _pairs.begin();
        }

        auto end() const noexcept {
            return _pairs.end();
        }

        // "a=1&b=x+y"; "" for no pairs
        string to_string() const;

        bool operator==(const query_params& other) const {
            if (_pairs.size() != other._pairs.size()) {
                return false;
            }
            for (size_t i = 0; i < _pairs.size(); ++i) {
                if (_pairs[i] != other._pairs[i]) {
                    return false;
                }
            }
            return true;
        }

    private:
        vector<pair<string, string>> _pairs;
    };

    class url;

    namespace detail {
        // The URL record of the standard (§4.1), as the parser builds it:
        // plain buffers, which a url is serialized from once. A path that
        // is a list is kept as its serialization ("/a/b", a '/' before
        // each segment), since a segment never holds a '/': the parser
        // splits on it and escapes nothing into one.
        enum class HostKind : uint8_t {
            none,      // null: "mailto:x", "sc:/x"
            empty,     // "file:///x", "sc:///x"
            domain,
            ipv4,
            ipv6,      // written with its brackets
            opaque     // the host of a scheme that is not special
        };

        struct UrlRecord {
            std::string scheme;
            std::string username;
            std::string password;
            std::string host;
            HostKind host_kind = HostKind::none;
            int32_t port = -1;                 // -1: null
            bool opaque_path = false;
            std::string path;
            bool has_query = false;
            std::string query;
            bool has_fragment = false;
            std::string fragment;

            bool includes_credentials() const noexcept {
                return !username.empty() || !password.empty();
            }

            // "cannot have a username/password/port"
            bool cannot_have_credentials() const noexcept {
                return host_kind == HostKind::none || host_kind == HostKind::empty || scheme == "file";
            }

            size_t segments() const noexcept {
                return size_t(std::count(path.begin(), path.end(), '/'));
            }

            std::string_view first_segment() const noexcept {
                std::string_view p(path);
                if (p.empty()) {
                    return p;
                }
                auto end = p.find('/', 1);
                return p.substr(1, end == std::string_view::npos ? std::string_view::npos : end - 1);
            }
        };

        enum class UrlState : uint8_t {
            scheme_start, scheme, no_scheme, special_relative_or_authority,
            path_or_authority, relative, relative_slash, special_authority_slashes,
            special_authority_ignore_slashes, authority, host, hostname, port, file,
            file_slash, file_host, path_start, path, opaque_path, query, fragment
        };

        // How a run of the parser ended: a URL, a failure, or (a setter's
        // state override) a value the standard declines to apply
        enum class UrlStep : uint8_t {
            ok,
            failure,
            refused
        };

        inline bool url_special(std::string_view scheme) noexcept {
            return scheme == "http" || scheme == "https" || scheme == "ws" || scheme == "wss"
                || scheme == "ftp" || scheme == "file";
        }

        // The default port of a special scheme, -1 for none (file, and
        // every scheme that is not special)
        inline int32_t url_default_port(std::string_view scheme) noexcept {
            if (scheme == "http" || scheme == "ws") {
                return 80;
            }
            if (scheme == "https" || scheme == "wss") {
                return 443;
            }
            if (scheme == "ftp") {
                return 21;
            }
            return -1;
        }

        inline bool url_alpha(int c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        inline bool url_digit(int c) noexcept {
            return c >= '0' && c <= '9';
        }

        inline int url_hex(int c) noexcept {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            return -1;
        }

        inline char url_lower(int c) noexcept {
            return char(c >= 'A' && c <= 'Z' ? c + 32 : c);
        }

        // A byte kept as it is when the set holds it, %XX otherwise; the
        // UTF-8 percent-encoding of a code point is that of its bytes,
        // each over 0x7F and so always escaped
        inline void url_escape(std::string& out, char c, const txt::percent_set& keep) {
            if (keep.holds(c)) {
                out.push_back(c);
                return;
            }
            static constexpr char Digits[] = "0123456789ABCDEF";
            out.push_back('%');
            out.push_back(Digits[uint8_t(c) >> 4]);
            out.push_back(Digits[uint8_t(c) & 15]);
        }

        inline void url_escape(std::string& out, std::string_view s, const txt::percent_set& keep) {
            for (char c : s) {
                url_escape(out, c, keep);
            }
        }

        // The standard's percent-decode: %XX to its byte, any other '%'
        // left as it is
        inline std::string url_unescape(std::string_view s) {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '%' && i + 2 < s.size()) {
                    int hi = url_hex(uint8_t(s[i + 1]));
                    int lo = url_hex(uint8_t(s[i + 2]));
                    if (hi >= 0 && lo >= 0) {
                        out.push_back(char(hi * 16 + lo));
                        i += 2;
                        continue;
                    }
                }
                out.push_back(s[i]);
            }
            return out;
        }

        // UTF-8 decode without BOM (the Encoding Standard): a sequence
        // broken off, an overlong form, a surrogate and a value past
        // U+10FFFF each become one U+FFFD for the longest start of a valid
        // sequence (the "maximal subpart"), the byte that broke it read
        // again as the start of the next
        inline void url_utf8(std::string_view s, std::string& out) {
            static constexpr std::string_view Replacement = "\xEF\xBF\xBD";
            size_t i = 0;
            while (i < s.size()) {
                uint8_t b = uint8_t(s[i]);
                if (b < 0x80) {
                    out.push_back(char(b));
                    ++i;
                    continue;
                }
                size_t need = 0;
                uint8_t lower = 0x80, upper = 0xBF;
                if (b >= 0xC2 && b <= 0xDF) {
                    need = 1;
                } else if (b >= 0xE0 && b <= 0xEF) {
                    need = 2;
                    lower = b == 0xE0 ? 0xA0 : 0x80;
                    upper = b == 0xED ? 0x9F : 0xBF;
                } else if (b >= 0xF0 && b <= 0xF4) {
                    need = 3;
                    lower = b == 0xF0 ? 0x90 : 0x80;
                    upper = b == 0xF4 ? 0x8F : 0xBF;
                } else {
                    out += Replacement;
                    ++i;
                    continue;
                }
                size_t j = i + 1, seen = 0;
                while (seen < need && j < s.size() && uint8_t(s[j]) >= lower && uint8_t(s[j]) <= upper) {
                    lower = 0x80;
                    upper = 0xBF;
                    ++j;
                    ++seen;
                }
                if (seen == need) {
                    out.append(s.substr(i, need + 1));
                } else {
                    out += Replacement;
                }
                i = j;
            }
        }

        inline bool url_utf8_valid(std::string_view s) noexcept {
            for (unsigned char c : s) {
                if (c >= 0x80) {
                    return utf8::valid(s);
                }
            }
            return true;
        }

        // A Windows drive letter: "c:" or "c|"; normalized: "c:"
        inline bool url_drive_letter(std::string_view s) noexcept {
            return s.size() == 2 && url_alpha(uint8_t(s[0])) && (s[1] == ':' || s[1] == '|');
        }

        inline bool url_normalized_drive_letter(std::string_view s) noexcept {
            return s.size() == 2 && url_alpha(uint8_t(s[0])) && s[1] == ':';
        }

        inline bool url_starts_with_drive_letter(std::string_view s) noexcept {
            if (s.size() < 2 || !url_drive_letter(s.substr(0, 2))) {
                return false;
            }
            return s.size() == 2 || s[2] == '/' || s[2] == '\\' || s[2] == '?' || s[2] == '#';
        }

        inline bool url_ascii_iequal(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (url_lower(uint8_t(a[i])) != url_lower(uint8_t(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        inline bool url_single_dot(std::string_view s) noexcept {
            return s == "." || url_ascii_iequal(s, "%2e");
        }

        inline bool url_double_dot(std::string_view s) noexcept {
            return s == ".." || url_ascii_iequal(s, ".%2e") || url_ascii_iequal(s, "%2e.")
                || url_ascii_iequal(s, "%2e%2e");
        }

        // The escape sets of the standard (§1.3) as the sets of what is
        // kept, which is how txt/percent.h writes them
        struct UrlSets {
            static constexpr const txt::percent_set& c0 = txt::percent::whatwg::c0;
            static constexpr const txt::percent_set& fragment = txt::percent::whatwg::fragment;
            static constexpr const txt::percent_set& query = txt::percent::whatwg::query;
            static constexpr const txt::percent_set& special_query = txt::percent::whatwg::special_query;
            static constexpr const txt::percent_set& path = txt::percent::whatwg::path;
            static constexpr const txt::percent_set& userinfo = txt::percent::whatwg::userinfo;
            static constexpr const txt::percent_set& form = txt::percent::whatwg::form_urlencoded;
        };

        // A forbidden host code point: NUL, tab, LF, CR, space # / : < > ? @ [ \ ] ^ |;
        // a forbidden domain code point: those, the C0 controls, % and DEL.
        // Tables, so that a loop over a host reads one byte each and has no
        // branch
        struct UrlForbidden {
            bool host[256] = {};
            bool domain[256] = {};

            constexpr UrlForbidden() {
                for (char c : std::string_view("\0\t\n\r #/:<>?@[\\]^|", 17)) {
                    host[uint8_t(c)] = domain[uint8_t(c)] = true;
                }
                for (int c = 0; c <= 0x1F; ++c) {
                    domain[c] = true;
                }
                domain['%'] = domain[0x7F] = true;
            }
        };

        inline constexpr UrlForbidden url_forbidden{};

        // What a byte is to the one-pass parser of url::_parse_common, one
        // load a byte: a letter; the end of an authority or a path segment
        // (/ ? #); a byte it leaves to the parser (\\); '@'; a byte a host
        // name cannot hold as it is (a forbidden domain code point, '%',
        // above ASCII); a byte a path segment, a special query or a
        // fragment keeps as it is and that does not end it; and the byte
        // lowered
        struct UrlBytes {
            enum : uint8_t {
                Letter = 1,
                AuthorityEnd = 2,
                Backslash = 4,
                At = 8,
                NotHost = 16,
                InSegment = 32,
                InQuery = 64,
                InFragment = 128
            };
            uint8_t kind[256] = {};
            char lower[256] = {};

            constexpr UrlBytes() {
                for (int c = 0; c < 256; ++c) {
                    lower[c] = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                        kind[c] |= Letter;
                    }
                    if (UrlForbidden().domain[c] || c >= 0x80) {
                        kind[c] |= NotHost;
                    }
                    if (txt::percent::whatwg::path.holds(char(c)) && c != '/' && c != '\\') {
                        kind[c] |= InSegment;
                    }
                    if (txt::percent::whatwg::special_query.holds(char(c)) && c != '#') {
                        kind[c] |= InQuery;
                    }
                    if (txt::percent::whatwg::fragment.holds(char(c))) {
                        kind[c] |= InFragment;
                    }
                }
                kind['/'] |= AuthorityEnd;
                kind['?'] |= AuthorityEnd;
                kind['#'] |= AuthorityEnd;
                kind['\\'] |= Backslash;
                kind['@'] |= At;
            }
        };

        inline constexpr UrlBytes url_bytes{};

        inline bool url_forbidden_host(uint8_t c) noexcept {
            return url_forbidden.host[c];
        }

        inline bool url_forbidden_domain(uint8_t c) noexcept {
            return url_forbidden.domain[c];
        }

        // §3.5, the IPv4 number parser: the value, or -1 for failure;
        // a value past 2^32 is kept as 2^32 + 1, which every caller
        // refuses as it would the value itself
        inline int64_t url_ipv4_number(std::string_view s) noexcept {
            if (s.empty()) {
                return -1;
            }
            int radix = 10;
            if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                s.remove_prefix(2);
                radix = 16;
            } else if (s.size() >= 2 && s[0] == '0') {
                s.remove_prefix(1);
                radix = 8;
            }
            if (s.empty()) {
                return 0;
            }
            constexpr int64_t Past = (int64_t(1) << 32) + 1;
            int64_t v = 0;
            for (char ch : s) {
                int d = url_hex(uint8_t(ch));
                if (d < 0 || d >= radix) {
                    return -1;
                }
                v = v * radix + d;
                if (v > Past) {
                    v = Past;
                }
            }
            return v;
        }

        // "Ends in a number" (§3.5): the last label (a trailing empty
        // one dropped) is all digits, or is 0x and hex digits
        inline bool url_ends_in_number(std::string_view s) noexcept {
            if (!s.empty() && s.back() == '.') {
                s.remove_suffix(1);
                if (s.empty()) {
                    return false;   // "." alone: one empty part, and that is kept
                }
            }
            auto dot = s.rfind('.');
            std::string_view last = dot == std::string_view::npos ? s : s.substr(dot + 1);
            if (!last.empty() && std::all_of(last.begin(), last.end(), [](char c) { return url_digit(uint8_t(c)); })) {
                return true;
            }
            return url_ipv4_number(last) >= 0;
        }

        // §3.5, the IPv4 parser; false for failure
        inline bool url_parse_ipv4(std::string_view s, uint32_t& out) noexcept {
            std::string_view parts[5];
            size_t count = 0;
            size_t from = 0;
            for (;;) {
                auto dot = s.find('.', from);
                std::string_view part = s.substr(from, dot == std::string_view::npos ? std::string_view::npos : dot - from);
                if (count == 5) {
                    return false;
                }
                parts[count++] = part;
                if (dot == std::string_view::npos) {
                    break;
                }
                from = dot + 1;
            }
            if (parts[count - 1].empty() && count > 1) {
                --count;
            }
            if (count > 4) {
                return false;
            }
            int64_t numbers[4];
            for (size_t i = 0; i < count; ++i) {
                numbers[i] = url_ipv4_number(parts[i]);
                if (numbers[i] < 0) {
                    return false;
                }
            }
            for (size_t i = 0; i + 1 < count; ++i) {
                if (numbers[i] > 255) {
                    return false;
                }
            }
            if (numbers[count - 1] >= (int64_t(1) << (8 * (5 - count)))) {
                return false;
            }
            uint64_t v = uint64_t(numbers[count - 1]);
            for (size_t i = 0; i + 1 < count; ++i) {
                v += uint64_t(numbers[i]) << (8 * (3 - i));
            }
            out = uint32_t(v);
            return true;
        }

        // At most 15 characters; the end of them
        inline char* url_write_ipv4(uint32_t v, char* out) noexcept {
            for (int i = 3; i >= 0; --i) {
                out = std::to_chars(out, out + 3, (v >> (8 * i)) & 0xff).ptr;
                if (i) {
                    *out++ = '.';
                }
            }
            return out;
        }

        inline void url_write_ipv4(uint32_t v, std::string& out) {
            char text[16];
            out.append(text, url_write_ipv4(v, text));
        }

        // §3.5, the IPv6 parser; false for failure
        inline bool url_parse_ipv6(std::string_view s, uint16_t (&address)[8]) noexcept {
            for (auto& piece : address) {
                piece = 0;
            }
            size_t n = s.size();
            size_t p = 0;
            int piece_index = 0;
            int compress = -1;
            auto at = [&](size_t i) -> int { return i < n ? uint8_t(s[i]) : -1; };
            if (at(p) == ':') {
                if (at(p + 1) != ':') {
                    return false;
                }
                p += 2;
                ++piece_index;
                compress = piece_index;
            }
            while (at(p) != -1) {
                if (piece_index == 8) {
                    return false;
                }
                if (at(p) == ':') {
                    if (compress != -1) {
                        return false;
                    }
                    ++p;
                    ++piece_index;
                    compress = piece_index;
                    continue;
                }
                unsigned value = 0;
                size_t length = 0;
                while (length < 4 && url_hex(at(p)) >= 0) {
                    value = value * 16 + unsigned(url_hex(at(p)));
                    ++p;
                    ++length;
                }
                if (at(p) == '.') {
                    if (length == 0) {
                        return false;
                    }
                    p -= length;
                    if (piece_index > 6) {
                        return false;
                    }
                    int numbers_seen = 0;
                    while (at(p) != -1) {
                        int ipv4_piece = -1;
                        if (numbers_seen > 0) {
                            if (at(p) == '.' && numbers_seen < 4) {
                                ++p;
                            } else {
                                return false;
                            }
                        }
                        if (!url_digit(at(p))) {
                            return false;
                        }
                        while (url_digit(at(p))) {
                            int number = at(p) - '0';
                            if (ipv4_piece == -1) {
                                ipv4_piece = number;
                            } else if (ipv4_piece == 0) {
                                return false;
                            } else {
                                ipv4_piece = ipv4_piece * 10 + number;
                            }
                            if (ipv4_piece > 255) {
                                return false;
                            }
                            ++p;
                        }
                        address[piece_index] = uint16_t(address[piece_index] * 0x100 + ipv4_piece);
                        ++numbers_seen;
                        if (numbers_seen == 2 || numbers_seen == 4) {
                            ++piece_index;
                        }
                    }
                    if (numbers_seen != 4) {
                        return false;
                    }
                    break;
                } else if (at(p) == ':') {
                    ++p;
                    if (at(p) == -1) {
                        return false;
                    }
                } else if (at(p) != -1) {
                    return false;
                }
                address[piece_index] = uint16_t(value);
                ++piece_index;
            }
            if (compress != -1) {
                int swaps = piece_index - compress;
                piece_index = 7;
                while (piece_index != 0 && swaps > 0) {
                    std::swap(address[piece_index], address[compress + swaps - 1]);
                    --piece_index;
                    --swaps;
                }
            } else if (piece_index != 8) {
                return false;
            }
            return true;
        }

        // §3.6, the IPv6 serializer, in brackets: the first longest run of
        // two or more zero pieces as "::", lower case, no leading zeros
        // (no dotted tail, unlike RFC 5952: "[::ffff:102:304]")
        inline char* url_write_ipv6(const uint16_t (&address)[8], char* out) noexcept {
            int compress = -1, best = 1;
            for (int i = 0; i < 8;) {
                if (address[i] != 0) {
                    ++i;
                    continue;
                }
                int j = i;
                while (j < 8 && address[j] == 0) {
                    ++j;
                }
                if (j - i > best) {
                    best = j - i;
                    compress = i;
                }
                i = j;
            }
            static constexpr char Digits[] = "0123456789abcdef";
            *out++ = '[';
            bool ignore0 = false;
            for (int i = 0; i < 8; ++i) {
                if (ignore0 && address[i] == 0) {
                    continue;
                }
                ignore0 = false;
                if (compress == i) {
                    if (i == 0) {
                        *out++ = ':';
                    }
                    *out++ = ':';
                    ignore0 = true;
                    continue;
                }
                unsigned v = address[i];
                bool started = false;
                for (int shift = 12; shift >= 0; shift -= 4) {
                    unsigned d = (v >> shift) & 15;
                    if (d || started || shift == 0) {
                        *out++ = Digits[d];
                        started = true;
                    }
                }
                if (i != 7) {
                    *out++ = ':';
                }
            }
            *out++ = ']';
            return out;
        }

        inline void url_write_ipv6(const uint16_t (&address)[8], std::string& out) {
            char text[48];
            out.append(text, url_write_ipv6(address, text));
        }

        // The rest of the host parser over the domain in ASCII: the checks,
        // and an IPv4 address when the domain ends in a number
        inline bool url_domain_to_host(std::string ascii_domain, std::string& out, HostKind& kind) {
            if (ascii_domain.empty()) {
                return false;
            }
            bool forbidden = false;
            for (char c : ascii_domain) {
                forbidden |= url_forbidden_domain(uint8_t(c));
            }
            if (forbidden) {
                return false;
            }
            if (url_ends_in_number(ascii_domain)) {
                uint32_t v;
                if (!url_parse_ipv4(ascii_domain, v)) {
                    return false;
                }
                url_write_ipv4(v, out);
                kind = HostKind::ipv4;
                return true;
            }
            out = std::move(ascii_domain);
            kind = HostKind::domain;
            return true;
        }

        // §3.5, the host parser: the host serialized and its kind, or false
        inline bool url_parse_host(std::string_view input, bool opaque, std::string& out, HostKind& kind) {
            out.clear();
            if (!input.empty() && input.front() == '[') {
                if (input.back() != ']' || input.size() < 2) {
                    return false;
                }
                uint16_t address[8];
                if (!url_parse_ipv6(input.substr(1, input.size() - 2), address)) {
                    return false;
                }
                url_write_ipv6(address, out);
                kind = HostKind::ipv6;
                return true;
            }
            if (opaque) {
                for (char c : input) {
                    if (url_forbidden_host(uint8_t(c))) {
                        return false;
                    }
                }
                url_escape(out, input, UrlSets::c0);
                kind = out.empty() ? HostKind::empty : HostKind::opaque;
                return true;
            }
            uint8_t escaped = 0;
            for (unsigned char c : input) {
                escaped |= uint8_t(c >> 7) | uint8_t(c == '%');
            }
            std::string ascii_domain;
            if (!escaped) {
                ascii_domain.assign(input);
                for (auto& c : ascii_domain) {
                    c = url_lower(uint8_t(c));
                }
                return url_domain_to_host(std::move(ascii_domain), out, kind);
            }
            std::string domain;
            url_utf8(url_unescape(input), domain);
            bool ascii = std::all_of(domain.begin(), domain.end(), [](char c) { return uint8_t(c) < 0x80; });
            if (ascii) {
                ascii_domain = domain;
                for (auto& c : ascii_domain) {
                    c = url_lower(uint8_t(c));
                }
            } else {
                auto r = txt::idna::to_ascii(string(std::string_view(domain)), txt::idna::options::whatwg());
                if (!r) {
                    return false;
                }
                ascii_domain.assign(r->data(), r->size());
            }
            return url_domain_to_host(std::move(ascii_domain), out, kind);
        }

        // The basic URL parser (§4.4), over the bytes of UTF-8 text: a
        // code point above ASCII only ever goes into a buffer or is
        // escaped, byte by byte, and every decision is taken on ASCII, so
        // walking the bytes is walking the code points
        class UrlParser {
        public:
            UrlParser(std::string_view input, const UrlRecord* base, UrlRecord& url) noexcept
            : _in(input), _base(base), _url(url) {
            }

            UrlStep run(UrlState state, bool override_given) {
                _override = override_given;
                _override_state = state;
                std::string buffer;
                bool at_sign_seen = false, inside_brackets = false, password_token_seen = false;
                const ptrdiff_t n = ptrdiff_t(_in.size());
                auto& url = _url;
                bool special = url_special(url.scheme);
                for (ptrdiff_t p = 0;; ++p) {
                    const int c = p < n ? uint8_t(_in[size_t(p)]) : Eof;
                    auto next = [&](ptrdiff_t k = 1) -> int { return p + k < n ? uint8_t(_in[size_t(p + k)]) : Eof; };
                    switch (state) {
                        case UrlState::scheme_start:
                            if (url_alpha(c)) {
                                buffer.push_back(url_lower(c));
                                state = UrlState::scheme;
                            } else if (!_override) {
                                state = UrlState::no_scheme;
                                --p;
                            } else {
                                return UrlStep::failure;
                            }
                            break;

                        case UrlState::scheme:
                            if (url_alpha(c) || url_digit(c) || c == '+' || c == '-' || c == '.') {
                                buffer.push_back(url_lower(c));
                            } else if (c == ':') {
                                const bool then = url_special(buffer);
                                if (_override) {
                                    if (special != then) {
                                        return UrlStep::refused;
                                    }
                                    if ((url.includes_credentials() || url.port >= 0) && buffer == "file") {
                                        return UrlStep::refused;
                                    }
                                    if (url.scheme == "file" && url.host_kind == HostKind::empty) {
                                        return UrlStep::refused;
                                    }
                                }
                                url.scheme = buffer;
                                special = then;
                                if (_override) {
                                    if (url.port == url_default_port(url.scheme)) {
                                        url.port = -1;
                                    }
                                    return UrlStep::ok;
                                }
                                buffer.clear();
                                if (url.scheme == "file") {
                                    state = UrlState::file;
                                } else if (special && _base && _base->scheme == url.scheme) {
                                    state = UrlState::special_relative_or_authority;
                                } else if (special) {
                                    state = UrlState::special_authority_slashes;
                                } else if (next() == '/') {
                                    state = UrlState::path_or_authority;
                                    ++p;
                                } else {
                                    url.opaque_path = true;
                                    url.path.clear();
                                    state = UrlState::opaque_path;
                                }
                            } else if (!_override) {
                                buffer.clear();
                                state = UrlState::no_scheme;
                                p = -1;
                            } else {
                                return UrlStep::failure;
                            }
                            break;

                        case UrlState::no_scheme:
                            if (!_base || (_base->opaque_path && c != '#')) {
                                return UrlStep::failure;
                            } else if (_base->opaque_path && c == '#') {
                                url.scheme = _base->scheme;
                                special = url_special(url.scheme);
                                url.opaque_path = true;
                                url.path = _base->path;
                                url.has_query = _base->has_query;
                                url.query = _base->query;
                                url.has_fragment = true;
                                url.fragment.clear();
                                state = UrlState::fragment;
                            } else if (_base->scheme != "file") {
                                state = UrlState::relative;
                                --p;
                            } else {
                                state = UrlState::file;
                                --p;
                            }
                            break;

                        case UrlState::special_relative_or_authority:
                            if (c == '/' && next() == '/') {
                                state = UrlState::special_authority_ignore_slashes;
                                ++p;
                            } else {
                                state = UrlState::relative;
                                --p;
                            }
                            break;

                        case UrlState::path_or_authority:
                            if (c == '/') {
                                state = UrlState::authority;
                            } else {
                                state = UrlState::path;
                                --p;
                            }
                            break;

                        case UrlState::relative:
                            url.scheme = _base->scheme;
                            special = url_special(url.scheme);
                            if (c == '/' || (special && c == '\\')) {
                                state = UrlState::relative_slash;
                            } else {
                                _take_authority(*_base);
                                url.opaque_path = false;
                                url.path = _base->path;
                                url.has_query = _base->has_query;
                                url.query = _base->query;
                                if (c == '?') {
                                    _begin_query();
                                    state = UrlState::query;
                                } else if (c == '#') {
                                    _begin_fragment();
                                    state = UrlState::fragment;
                                } else if (c != Eof) {
                                    url.has_query = false;
                                    url.query.clear();
                                    _shorten();
                                    state = UrlState::path;
                                    --p;
                                }
                            }
                            break;

                        case UrlState::relative_slash:
                            if (special && (c == '/' || c == '\\')) {
                                state = UrlState::special_authority_ignore_slashes;
                            } else if (c == '/') {
                                state = UrlState::authority;
                            } else {
                                _take_authority(*_base);
                                state = UrlState::path;
                                --p;
                            }
                            break;

                        case UrlState::special_authority_slashes:
                            state = UrlState::special_authority_ignore_slashes;
                            if (c == '/' && next() == '/') {
                                ++p;
                            } else {
                                --p;
                            }
                            break;

                        case UrlState::special_authority_ignore_slashes:
                            if (c != '/' && c != '\\') {
                                state = UrlState::authority;
                                --p;
                            }
                            break;

                        case UrlState::authority:
                            if (c == '@') {
                                if (at_sign_seen) {
                                    buffer.insert(0, "%40");
                                }
                                at_sign_seen = true;
                                for (char b : buffer) {
                                    if (b == ':' && !password_token_seen) {
                                        password_token_seen = true;
                                        continue;
                                    }
                                    url_escape(password_token_seen ? url.password : url.username, b, UrlSets::userinfo);
                                }
                                buffer.clear();
                            } else if (c == Eof || c == '/' || c == '?' || c == '#' || (special && c == '\\')) {
                                if (at_sign_seen && buffer.empty()) {
                                    return UrlStep::failure;
                                }
                                p -= ptrdiff_t(buffer.size()) + 1;
                                buffer.clear();
                                state = UrlState::host;
                            } else {
                                p = _run(p, buffer, [](char b) { return b != '@' && b != '/' && b != '?' && b != '#' && b != '\\'; });
                            }
                            break;

                        case UrlState::host:
                        case UrlState::hostname:
                            if (_override && url.scheme == "file") {
                                --p;
                                state = UrlState::file_host;
                            } else if (c == ':' && !inside_brackets) {
                                if (buffer.empty()) {
                                    return UrlStep::failure;
                                }
                                if (_override && _override_state == UrlState::hostname) {
                                    return UrlStep::failure;
                                }
                                if (!_set_host(buffer, !special)) {
                                    return UrlStep::failure;
                                }
                                buffer.clear();
                                state = UrlState::port;
                            } else if (c == Eof || c == '/' || c == '?' || c == '#' || (special && c == '\\')) {
                                --p;
                                if (special && buffer.empty()) {
                                    return UrlStep::failure;
                                }
                                if (_override && buffer.empty() && (url.includes_credentials() || url.port >= 0)) {
                                    return UrlStep::failure;
                                }
                                if (!_set_host(buffer, !special)) {
                                    return UrlStep::failure;
                                }
                                buffer.clear();
                                state = UrlState::path_start;
                                if (_override) {
                                    return UrlStep::ok;
                                }
                            } else {
                                if (c == '[') {
                                    inside_brackets = true;
                                }
                                if (c == ']') {
                                    inside_brackets = false;
                                }
                                p = _run(p, buffer, [](char b) {
                                    return b != ':' && b != '/' && b != '?' && b != '#' && b != '\\' && b != '[' && b != ']';
                                });
                            }
                            break;

                        case UrlState::port:
                            if (url_digit(c)) {
                                buffer.push_back(char(c));
                            } else if (c == Eof || c == '/' || c == '?' || c == '#' || (special && c == '\\') || _override) {
                                if (!buffer.empty()) {
                                    uint32_t port = 0;
                                    for (char d : buffer) {
                                        port = port * 10 + uint32_t(d - '0');
                                        if (port > 65535) {
                                            return UrlStep::failure;
                                        }
                                    }
                                    url.port = int32_t(port) == url_default_port(url.scheme) ? -1 : int32_t(port);
                                    buffer.clear();
                                    if (_override) {
                                        return UrlStep::ok;
                                    }
                                }
                                if (_override) {
                                    // the host setter's "a:": the host set, the port
                                    // kept (the standard's failure here is one its
                                    // setter drops, the change made standing)
                                    return UrlStep::ok;
                                }
                                state = UrlState::path_start;
                                --p;
                            } else {
                                return UrlStep::failure;
                            }
                            break;

                        case UrlState::file:
                            url.scheme = "file";
                            special = true;
                            url.host.clear();
                            url.host_kind = HostKind::empty;
                            if (c == '/' || c == '\\') {
                                state = UrlState::file_slash;
                            } else if (_base && _base->scheme == "file") {
                                url.host = _base->host;
                                url.host_kind = _base->host_kind;
                                url.opaque_path = false;
                                url.path = _base->path;
                                url.has_query = _base->has_query;
                                url.query = _base->query;
                                if (c == '?') {
                                    _begin_query();
                                    state = UrlState::query;
                                } else if (c == '#') {
                                    _begin_fragment();
                                    state = UrlState::fragment;
                                } else if (c != Eof) {
                                    url.has_query = false;
                                    url.query.clear();
                                    if (!url_starts_with_drive_letter(_in.substr(size_t(p)))) {
                                        _shorten();
                                    } else {
                                        url.path.clear();
                                    }
                                    state = UrlState::path;
                                    --p;
                                }
                            } else {
                                state = UrlState::path;
                                --p;
                            }
                            break;

                        case UrlState::file_slash:
                            if (c == '/' || c == '\\') {
                                state = UrlState::file_host;
                            } else {
                                if (_base && _base->scheme == "file") {
                                    url.host = _base->host;
                                    url.host_kind = _base->host_kind;
                                    if (!url_starts_with_drive_letter(_in.substr(size_t(std::min(p, n))))
                                        && _base->segments() > 0 && url_normalized_drive_letter(_base->first_segment())) {
                                        url.path += '/';
                                        url.path += _base->first_segment();
                                    }
                                }
                                state = UrlState::path;
                                --p;
                            }
                            break;

                        case UrlState::file_host:
                            if (c == Eof || c == '/' || c == '\\' || c == '?' || c == '#') {
                                --p;
                                if (!_override && url_drive_letter(buffer)) {
                                    state = UrlState::path;   // the buffer is the path's first segment
                                } else if (buffer.empty()) {
                                    url.host.clear();
                                    url.host_kind = HostKind::empty;
                                    if (_override) {
                                        return UrlStep::ok;
                                    }
                                    state = UrlState::path_start;
                                } else {
                                    if (!_set_host(buffer, !special)) {
                                        return UrlStep::failure;
                                    }
                                    if (url.host == "localhost") {
                                        url.host.clear();
                                        url.host_kind = HostKind::empty;
                                    }
                                    if (_override) {
                                        return UrlStep::ok;
                                    }
                                    buffer.clear();
                                    state = UrlState::path_start;
                                }
                            } else {
                                buffer.push_back(char(c));
                            }
                            break;

                        case UrlState::path_start:
                            if (special) {
                                state = UrlState::path;
                                if (c != '/' && c != '\\') {
                                    --p;
                                }
                            } else if (!_override && c == '?') {
                                _begin_query();
                                state = UrlState::query;
                            } else if (!_override && c == '#') {
                                _begin_fragment();
                                state = UrlState::fragment;
                            } else if (c != Eof) {
                                state = UrlState::path;
                                if (c != '/') {
                                    --p;
                                }
                            } else if (_override && url.host_kind == HostKind::none) {
                                url.path += '/';
                            }
                            break;

                        case UrlState::path:
                            if (c == Eof || c == '/' || (special && c == '\\') || (!_override && (c == '?' || c == '#'))) {
                                bool slash = c == '/' || (special && c == '\\');
                                if (url_double_dot(buffer)) {
                                    _shorten();
                                    if (!slash) {
                                        url.path += '/';
                                    }
                                } else if (url_single_dot(buffer)) {
                                    if (!slash) {
                                        url.path += '/';
                                    }
                                } else {
                                    if (url.scheme == "file" && url.path.empty() && url_drive_letter(buffer)) {
                                        buffer[1] = ':';
                                    }
                                    url.path += '/';
                                    url.path += buffer;
                                }
                                buffer.clear();
                                if (c == '?') {
                                    _begin_query();
                                    state = UrlState::query;
                                }
                                if (c == '#') {
                                    _begin_fragment();
                                    state = UrlState::fragment;
                                }
                            } else if (UrlSets::path.holds(char(c))) {
                                p = _run(p, buffer, [](char b) { return UrlSets::path.holds(b) && b != '/' && b != '\\'; });
                            } else {
                                url_escape(buffer, char(c), UrlSets::path);
                            }
                            break;

                        case UrlState::opaque_path:
                            if (c == '?') {
                                _begin_query();
                                state = UrlState::query;
                            } else if (c == '#') {
                                _begin_fragment();
                                state = UrlState::fragment;
                            } else if (c == ' ') {
                                int after = next();
                                url.path += after == '?' || after == '#' ? "%20" : " ";
                            } else if (c != Eof && UrlSets::c0.holds(char(c))) {
                                p = _run(p, url.path, [](char b) { return UrlSets::c0.holds(b) && b != '?' && b != '#' && b != ' '; });
                            } else if (c != Eof) {
                                url_escape(url.path, char(c), UrlSets::c0);
                            }
                            break;

                        case UrlState::query:
                            if ((!_override && c == '#') || c == Eof) {
                                if (c == '#') {
                                    _begin_fragment();
                                    state = UrlState::fragment;
                                }
                            } else if (const auto& set = special ? UrlSets::special_query : UrlSets::query; set.holds(char(c))) {
                                p = _run(p, url.query, [&set](char b) { return set.holds(b) && b != '#'; });
                            } else {
                                url_escape(url.query, char(c), set);
                            }
                            break;

                        case UrlState::fragment:
                            if (c != Eof && UrlSets::fragment.holds(char(c))) {
                                p = _run(p, url.fragment, [](char b) { return UrlSets::fragment.holds(b); });
                            } else if (c != Eof) {
                                url_escape(url.fragment, char(c), UrlSets::fragment);
                            }
                            break;
                    }
                    if (p >= n) {
                        break;
                    }
                }
                return UrlStep::ok;
            }

        private:
            static constexpr int Eof = -1;

            // The byte at p, which the state takes as it is, and those after
            // it that the state would take the same way, appended at once;
            // the place of the last, which the loop steps past
            template<class Plain>
            ptrdiff_t _run(ptrdiff_t p, std::string& out, Plain plain) const {
                size_t q = size_t(p) + 1;
                while (q < _in.size() && plain(_in[q])) {
                    ++q;
                }
                out.append(_in.data() + p, q - size_t(p));
                return ptrdiff_t(q) - 1;
            }

            void _take_authority(const UrlRecord& from) {
                _url.username = from.username;
                _url.password = from.password;
                _url.host = from.host;
                _url.host_kind = from.host_kind;
                _url.port = from.port;
            }

            void _begin_query() {
                _url.has_query = true;
                _url.query.clear();
            }

            void _begin_fragment() {
                _url.has_fragment = true;
                _url.fragment.clear();
            }

            // "Shorten url's path": the last segment goes, unless the path is
            // a file URL's drive letter alone
            void _shorten() {
                if (_url.scheme == "file" && _url.segments() == 1 && url_normalized_drive_letter(_url.first_segment())) {
                    return;
                }
                auto slash = _url.path.rfind('/');
                if (slash != std::string::npos) {
                    _url.path.erase(slash);
                }
            }

            bool _set_host(std::string_view buffer, bool opaque) {
                std::string host;
                HostKind kind = HostKind::none;
                if (!url_parse_host(buffer, opaque, host, kind)) {
                    return false;
                }
                _url.host = std::move(host);
                _url.host_kind = kind;
                return true;
            }

            std::string_view _in;
            const UrlRecord* _base;
            UrlRecord& _url;
            bool _override = false;
            UrlState _override_state = UrlState::scheme_start;
        };

        // The text as the parser takes it: the C0 controls and spaces at
        // the ends gone (when a URL is parsed, not a setter's value), every
        // tab and newline gone, and a byte that is not UTF-8 turned into
        // U+FFFD, since the standard's input is a string of code points.
        // A view of s itself when there is nothing to remove or replace
        // (nearly always), of storage otherwise
        inline std::string_view url_input(std::string_view s, bool trim, std::string& storage) {
            if (trim) {
                while (!s.empty() && uint8_t(s.front()) <= 0x20) {
                    s.remove_prefix(1);
                }
                while (!s.empty() && uint8_t(s.back()) <= 0x20) {
                    s.remove_suffix(1);
                }
            }
            uint8_t odd = 0;   // a byte and not a bool: a bool's | keeps the loop scalar, 3 to 12 times slower
            for (unsigned char c : s) {
                odd |= uint8_t(c == '\t') | uint8_t(c == '\n') | uint8_t(c == '\r') | uint8_t(c >> 7);
            }
            if (!odd) {
                return s;
            }
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                if (c != '\t' && c != '\n' && c != '\r') {
                    out.push_back(c);
                }
            }
            if (!url_utf8_valid(out)) {
                url_utf8(out, storage);
            } else {
                storage = std::move(out);
            }
            return storage;
        }

        struct UrlAccess;
    }

    // A URL by the WHATWG URL Standard, the way browsers, curl, Node and
    // Deno read one: an immutable value, as a string is (one string of the
    // serialization and the places of its parts in it), made only by the
    // parser or by a setter of the standard, so that every url is a URL.
    // The standard's test data (urltestdata.json, setters_tests.json,
    // toascii.json of the web platform tests) is the oracle, whole.
    //
    // Unlike Go's net/url, which reads RFC 3986 loosely: a '\' in a special
    // scheme is a '/', tabs and newlines vanish, the host goes through IDNA
    // (bücher.de is xn--bcher-kva.de), an IPv4 host may be written 0x7f.1,
    // the path is resolved ("/a/../b" is "/b"), a space is escaped, and a
    // relative reference is resolved by the same parser, given a base.
    //
    // The parts come back as the standard serializes them, escaped: path()
    // is "/a%20b", not "/a b". host() is the host and the port when one
    // is written ("example.com:8443", "[::1]:8080", "10.0.0.1"), as
    // WHATWG's host and Go's URL.Host; hostname() the host alone, no port
    // and no brackets, as Go's Hostname().
    class url {
    public:
        // A URL: "https://user@example.com:8443/a/b?q=1#top"; for anything
        // else, a relative reference among it, net::errc::invalid_url
        static expected<url, io::error> parse(const string& text) {
            return _parsed(_parse(text, nullptr), text);
        }

        // A URL or a reference relative to base: "../c", "?q=2", "//host/x"
        static expected<url, io::error> parse(const string& text, const url& base) {
            auto b = base._record();
            return _parsed(_parse(text, &b), text);
        }

        // "https", without the ':'
        string scheme() const {
            return _part(0, _scheme_end);
        }

        string username() const {
            return _part(_username_begin(), _username_end);
        }

        string password() const {
            return _password_end > _username_end ? _part(_username_end + 1, _password_end) : string();
        }

        // The host and the port when one is written, as WHATWG's host and
        // Go's URL.Host have it: "example.com:8443", "[::1]:8080",
        // "xn--bcher-kva.de", "10.0.0.1"; "" when there is none (and for
        // the empty host of "file:///x"). The name alone is hostname()
        string host() const {
            if (_port < 0) {
                return _part(_host_begin, _host_end);
            }
            return string(std::string(_part(_host_begin, _host_end).view()) + ':' + std::to_string(_port));
        }

        // The host without the port, and without the brackets of an IPv6
        // address, as Go's Hostname(): "example.com", "::1"
        string hostname() const {
            if (_host_kind == detail::HostKind::ipv6) {
                return _part(_host_begin + 1, _host_end - 1);
            }
            return _part(_host_begin, _host_end);
        }

        bool has_host() const noexcept {
            return _host_kind != detail::HostKind::none;
        }

        // The host when it is an IP address; nullopt for a name, and for
        // the opaque host of a scheme that is not special
        optional<ip_address> host_address() const {
            if (_host_kind != detail::HostKind::ipv4 && _host_kind != detail::HostKind::ipv6) {
                return nullopt;
            }
            return detail::IpText::parse(detail::IpText::view(hostname()));
        }

        // The port written; nullopt when none was, and when the one
        // written is the scheme's default ("http://x:80/" has none)
        optional<uint16_t> port() const noexcept {
            if (_port < 0) {
                return nullopt;
            }
            return uint16_t(_port);
        }

        // The port, or the scheme's default: 80 for http and ws, 443 for
        // https and wss, 21 for ftp; 0 for a scheme without one
        uint16_t effective_port() const noexcept {
            if (_port >= 0) {
                return uint16_t(_port);
            }
            int32_t d = detail::url_default_port(_href.view().substr(0, _scheme_end));
            return d < 0 ? 0 : uint16_t(d);
        }

        // "/a/b%20c", escaped; the whole of what follows the ':' for a URL
        // without a hierarchical path ("mailto:x@example.com": "x@example.com")
        string path() const {
            return _part(_path_begin, _path_end());
        }

        // The query without its '?', escaped; "" when there is none
        string query() const {
            return has_query() ? _part(_query_begin + 1, _fragment_begin == Npos ? _href.size() : _fragment_begin) : string();
        }

        // The fragment without its '#'; "" when there is none
        string fragment() const {
            return has_fragment() ? _part(_fragment_begin + 1, _href.size()) : string();
        }

        bool has_query() const noexcept {
            return _query_begin != Npos;
        }

        bool has_fragment() const noexcept {
            return _fragment_begin != Npos;
        }

        // A URL without a hierarchical path: "mailto:x", "data:,x"
        bool has_opaque_path() const noexcept {
            return _opaque_path;
        }

        // A scheme the standard gives a meaning of its own: http, https,
        // ws, wss, ftp, file
        bool is_special() const {
            return detail::url_special(_href.view().substr(0, _scheme_end));
        }

        // The origin as the standard serializes it: "https://example.com:8443"
        // for http, https, ws, wss and ftp (and a blob: of an http or https
        // URL); "null" for anything else, file included
        string origin() const;

        // What goes into the request line of HTTP: the path, and "?" and
        // the query when there is one
        string request_target() const {
            return _part(_path_begin, has_fragment() ? _fragment_begin : _href.size());
        }

        // The query's pairs (application/x-www-form-urlencoded)
        net::query_params query_params() const {
            return has_query() ? net::query_params::parse(query()) : net::query_params();
        }

        // The reference resolved against this URL: parse(reference, *this)
        expected<url, io::error> resolve(const string& reference) const {
            return parse(reference, *this);
        }

        // The setters of the standard (WHATWG URL §6.1), each a new URL;
        // net::errc::invalid_url where the standard refuses the value or
        // declines it: a scheme that is not
        // one, a special scheme for one that is not (or the other way), a
        // host that does not parse (or, to with_hostname, is given with a
        // port), credentials
        // or a port for a URL without a host or with file's, a host or a
        // path for a URL with an opaque path.
        expected<url, io::error> with_scheme(const string& scheme) const;
        expected<url, io::error> with_username(const string& username) const;
        expected<url, io::error> with_password(const string& password) const;
        // The standard's host setter, the pair of host(): the host and a
        // port when the value has one ("example.com:8080"); with_hostname
        // is its hostname setter, the host alone
        expected<url, io::error> with_host(const string& host) const;
        expected<url, io::error> with_hostname(const string& hostname) const;
        expected<url, io::error> with_port(optional<uint16_t> port) const;   // nullopt removes it
        expected<url, io::error> with_path(const string& path) const;
        // A query ("" removes it, a leading '?' is dropped) and a fragment
        // (likewise with '#'); these always apply
        url with_query(const string& query) const;
        url with_query(const net::query_params& params) const;   // empty: no query
        url with_fragment(const string& fragment) const;
        url without_fragment() const;

        // The serialization, href
        string to_string() const {
            return _href;
        }

        friend bool operator==(const url& a, const url& b) noexcept {
            return a._href == b._href;
        }

        friend std::strong_ordering operator<=>(const url& a, const url& b) noexcept {
            return a._href.view() <=> b._href.view();
        }

    private:
        friend struct detail::UrlAccess;
        friend struct std::hash<url>;
        static constexpr uint32_t Npos = uint32_t(-1);

        url() = default;

        static expected<url, io::error> _parsed(optional<url>&& u, const string& text) {
            if (u) {
                return std::move(*u);
            }
            return unexpected(detail::net_error(errc::invalid_url, "parse URL", text));
        }

        static optional<url> _parse(const string& text, const detail::UrlRecord* base, bool one_pass = true) {
            std::string storage;
            auto input = detail::url_input(text.view(), true, storage);
            if (url u; one_pass && _parse_common(input, u)) {
                return u;
            }
            detail::UrlRecord r;
            if (detail::UrlParser(input, base, r).run(detail::UrlState::scheme_start, false) != detail::UrlStep::ok) {
                return nullopt;
            }
            return _from(r);
        }

        static bool _parse_common(std::string_view in, url& u);

        string _part(size_t from, size_t to) const {
            return to > from ? string(_href.view().substr(from, to - from)) : string();
        }

        uint32_t _username_begin() const noexcept {
            return _host_kind == detail::HostKind::none ? _username_end : _scheme_end + 3;
        }

        uint32_t _path_end() const noexcept {
            return has_query() ? _query_begin : has_fragment() ? _fragment_begin : uint32_t(_href.size());
        }

        // The serializer (§4.5), and where each part landed. Measured
        // first and written into a buffer on the stack (a heap one past
        // 256 bytes), from which the string is made: no std::string to
        // grow or to allocate on the way
        static url _from(const detail::UrlRecord& r) {
            char port[8];
            size_t port_size = 0;
            if (r.host_kind != detail::HostKind::none && r.port >= 0) {
                port[0] = ':';
                port_size = size_t(std::to_chars(port + 1, port + sizeof(port), r.port).ptr - port);
            }
            const bool credentials = r.includes_credentials();
            const bool dot = r.host_kind == detail::HostKind::none && !r.opaque_path && r.segments() > 1
                && r.first_segment().empty();
            size_t size = r.scheme.size() + 1 + r.path.size() + port_size;
            if (r.host_kind != detail::HostKind::none) {
                size += 2 + r.username.size() + (r.password.empty() ? 0 : 1 + r.password.size()) + (credentials ? 1 : 0)
                    + r.host.size();
            }
            size += dot ? 2 : 0;
            size += r.has_query ? 1 + r.query.size() : 0;
            size += r.has_fragment ? 1 + r.fragment.size() : 0;
            char local[256];
            std::string heap;
            char* const begin = size <= sizeof(local) ? local : (heap.resize(size), heap.data());
            char* at = begin;
            auto put = [&at](std::string_view part) {
                std::memcpy(at, part.data(), part.size());
                at += part.size();
            };
            auto here = [&] { return uint32_t(at - begin); };
            url u;
            put(r.scheme);
            u._scheme_end = here();
            put(":");
            if (r.host_kind != detail::HostKind::none) {
                put("//");
                put(r.username);
                u._username_end = here();
                if (!r.password.empty()) {
                    put(":");
                    put(r.password);
                }
                u._password_end = here();
                if (credentials) {
                    put("@");
                }
                u._host_begin = here();
                put(r.host);
                u._host_end = here();
                put(std::string_view(port, port_size));
            } else {
                u._username_end = u._password_end = u._host_begin = u._host_end = here();
                if (dot) {
                    put("/.");
                }
            }
            u._path_begin = here();
            put(r.path);
            if (r.has_query) {
                u._query_begin = here();
                put("?");
                put(r.query);
            }
            if (r.has_fragment) {
                u._fragment_begin = here();
                put("#");
                put(r.fragment);
            }
            u._port = r.port;
            u._host_kind = r.host_kind;
            u._opaque_path = r.opaque_path;
            u._href = string(std::string_view(begin, size));
            return u;
        }

        // The record again, for a setter and for a base
        detail::UrlRecord _record() const {
            detail::UrlRecord r;
            auto v = _href.view();
            r.scheme = std::string(v.substr(0, _scheme_end));
            if (_host_kind != detail::HostKind::none) {
                r.username = std::string(v.substr(_username_begin(), _username_end - _username_begin()));
                if (_password_end > _username_end) {
                    r.password = std::string(v.substr(_username_end + 1, _password_end - _username_end - 1));
                }
                r.host = std::string(v.substr(_host_begin, _host_end - _host_begin));
            }
            r.host_kind = _host_kind;
            r.port = _port;
            r.opaque_path = _opaque_path;
            r.path = std::string(v.substr(_path_begin, _path_end() - _path_begin));
            r.has_query = has_query();
            if (r.has_query) {
                size_t end = has_fragment() ? _fragment_begin : v.size();
                r.query = std::string(v.substr(_query_begin + 1, end - _query_begin - 1));
            }
            r.has_fragment = has_fragment();
            if (r.has_fragment) {
                r.fragment = std::string(v.substr(_fragment_begin + 1));
            }
            return r;
        }

        string _href;
        uint32_t _scheme_end = 0;
        uint32_t _username_end = 0;
        uint32_t _password_end = 0;
        uint32_t _host_begin = 0;
        uint32_t _host_end = 0;
        uint32_t _path_begin = 0;
        uint32_t _query_begin = Npos;
        uint32_t _fragment_begin = Npos;
        int32_t _port = -1;
        detail::HostKind _host_kind = detail::HostKind::none;
        bool _opaque_path = false;
    };

    namespace detail {
        // The setters of the standard (§6.1) over a url: the URL after the
        // setter, and whether the value was taken. A setter may change
        // the URL and still fail (the host setter with "example.com:99999"
        // sets the host and refuses the port), which is why both come back.
        struct UrlSetResult {
            url value;
            bool applied;
        };

        struct UrlAccess {
            // The host as the URL writes it, the brackets of an IPv6
            // address kept and no port: "[::1]" (WHATWG's hostname), what
            // a dial address and an origin put the port after
            static string host_as_written(const url& u) {
                return u._part(u._host_begin, u._host_end);
            }

            // The parser alone, and the one-pass path alone (nullopt where
            // it leaves the text to the parser): for the test that holds
            // the second to the first's answers
            static optional<url> parse_by_parser(const string& text) {
                return url::_parse(text, nullptr, false);
            }

            static optional<url> parse_in_one_pass(const string& text) {
                std::string storage;
                auto input = url_input(text.view(), true, storage);
                if (url u; url::_parse_common(input, u)) {
                    return u;
                }
                return nullopt;
            }

            static UrlRecord record(const url& u) {
                return u._record();
            }

            static url make(const UrlRecord& r) {
                return url::_from(r);
            }

            static UrlSetResult run(const url& u, const string& value, UrlState state) {
                auto r = u._record();
                std::string storage;
                auto input = url_input(value.view(), false, storage);
                auto step = UrlParser(input, nullptr, r).run(state, true);
                return {url::_from(r), step == UrlStep::ok};
            }

            static UrlSetResult unchanged(const url& u) {
                return {u, false};
            }

            static UrlSetResult protocol(const url& u, const string& value) {
                return run(u, value + ":", UrlState::scheme_start);
            }

            static UrlSetResult username(const url& u, const string& value, bool password) {
                auto r = u._record();
                if (r.cannot_have_credentials()) {
                    return unchanged(u);
                }
                std::string escaped;
                url_escape(escaped, value.view(), UrlSets::userinfo);
                (password ? r.password : r.username) = std::move(escaped);
                return {url::_from(r), true};
            }

            static UrlSetResult host(const url& u, const string& value, bool hostname) {
                if (u.has_opaque_path()) {
                    return unchanged(u);
                }
                return run(u, value, hostname ? UrlState::hostname : UrlState::host);
            }

            static UrlSetResult port(const url& u, const string& value) {
                auto r = u._record();
                if (r.cannot_have_credentials()) {
                    return unchanged(u);
                }
                if (value.empty()) {
                    r.port = -1;
                    return {url::_from(r), true};
                }
                return run(u, value, UrlState::port);
            }

            static UrlSetResult pathname(const url& u, const string& value) {
                if (u.has_opaque_path()) {
                    return unchanged(u);
                }
                auto r = u._record();
                r.path.clear();
                std::string storage;
                auto input = url_input(value.view(), false, storage);
                auto step = UrlParser(input, nullptr, r).run(UrlState::path_start, true);
                return {url::_from(r), step == UrlStep::ok};
            }

            static url search(const url& u, const string& value) {
                auto r = u._record();
                if (value.empty()) {
                    r.has_query = false;
                    r.query.clear();
                    return url::_from(r);
                }
                auto v = value.view();
                if (v.front() == '?') {
                    v.remove_prefix(1);
                }
                r.has_query = true;
                r.query.clear();
                std::string storage;
                auto input = url_input(v, false, storage);
                UrlParser(input, nullptr, r).run(UrlState::query, true);
                return url::_from(r);
            }

            static url hash(const url& u, const string& value) {
                auto r = u._record();
                if (value.empty()) {
                    r.has_fragment = false;
                    r.fragment.clear();
                    return url::_from(r);
                }
                auto v = value.view();
                if (v.front() == '#') {
                    v.remove_prefix(1);
                }
                r.has_fragment = true;
                r.fragment.clear();
                std::string storage;
                auto input = url_input(v, false, storage);
                UrlParser(input, nullptr, r).run(UrlState::fragment, true);
                return url::_from(r);
            }
        };

        // A setter's result: the new URL, or invalid_url with what was
        // asked, where the standard leaves the URL as it was
        inline expected<url, io::error> url_applied(const UrlSetResult& r, const char* op, const string& value) {
            if (!r.applied) {
                return unexpected(net_error(errc::invalid_url, op, value));
            }
            return r.value;
        }
    }

    // The URL most text is, in one pass written straight into its
    // serialization: a special scheme other than file, "//", credentials, a
    // host that is an ASCII name without a '%', an IPv4 or an IPv6 address,
    // a port, a path, a query and a fragment. The parser's record, its
    // buffer, the second reading of the authority and the copies into the
    // serialization are what made a parse cost four times Go's. Anything
    // else is left to the parser (false): a scheme that is not special or
    // is file, slashes other than two, a backslash, a host needing
    // percent-decoding or IDNA, and every input that fails, so that the
    // parser alone says why. After "scheme://" a base changes nothing
    // (§4.4: the special authority states read the same with a base and
    // without), so this runs with one as well.
    inline bool url::_parse_common(std::string_view in, url& u) {
        using namespace detail;
        const auto& bytes = url_bytes;
        const size_t n = in.size();
        // The scheme: letters only (the special ones are), lowered
        char scheme[6];
        size_t i = 0;
        while (i < n && i < sizeof(scheme) && (bytes.kind[uint8_t(in[i])] & UrlBytes::Letter)) {
            scheme[i] = bytes.lower[uint8_t(in[i])];
            ++i;
        }
        if (i + 3 > n || in[i] != ':' || in[i + 1] != '/' || in[i + 2] != '/') {
            return false;
        }
        const std::string_view name(scheme, i);
        int32_t default_port;
        if (name == "http" || name == "ws") {
            default_port = 80;
        } else if (name == "https" || name == "wss") {
            default_port = 443;
        } else if (name == "ftp") {
            default_port = 21;
        } else {
            return false;   // file, and every scheme that is not special
        }
        i += 3;   // a third slash or a backslash leaves the host empty, below, and so to the parser

        // The serialization is at most three times the input (a byte
        // escaped) and a few characters of a path's '/' and an address
        const size_t bound = 3 * n + 64;
        char local[1024];
        std::string heap;
        char* const out = bound <= sizeof(local) ? local : (heap.resize(bound), heap.data());
        char* at = out;
        auto here = [&] { return uint32_t(at - out); };
        auto put = [&at](std::string_view part) {
            std::memcpy(at, part.data(), part.size());
            at += part.size();
        };
        auto escape = [&at](uint8_t c, const txt::percent_set& keep) {
            static constexpr char Digits[] = "0123456789ABCDEF";
            if (keep.holds(char(c))) {
                *at++ = char(c);
            } else {
                at[0] = '%';
                at[1] = Digits[c >> 4];
                at[2] = Digits[c & 15];
                at += 3;
            }
        };
        put(name);
        u._scheme_end = here();
        put("://");

        // The authority, to the first of / ? # or the end; a '\' is a '/'
        // here, and is left to the parser
        size_t end = i;
        size_t at_sign = std::string_view::npos;
        for (; end < n; ++end) {
            const uint8_t k = bytes.kind[uint8_t(in[end])];
            if (k & (UrlBytes::AuthorityEnd | UrlBytes::Backslash | UrlBytes::At)) {
                if (k & UrlBytes::AuthorityEnd) {
                    break;
                }
                if (k & UrlBytes::Backslash) {
                    return false;
                }
                at_sign = end;   // the last: what is before it is the credentials, '@'s escaped
            }
        }
        if (at_sign != std::string_view::npos) {
            bool password = false;
            for (size_t k = i; k < at_sign; ++k) {
                if (in[k] == ':' && !password) {
                    password = true;
                    u._username_end = here();
                    *at++ = ':';
                    continue;
                }
                escape(uint8_t(in[k]), UrlSets::userinfo);
            }
            if (!password) {
                u._username_end = here();
            } else if (here() == u._username_end + 1) {
                --at;   // an empty password is not written
            }
            u._password_end = here();
            if (here() > u._scheme_end + 3) {
                *at++ = '@';
            }
            i = at_sign + 1;
        } else {
            u._username_end = u._password_end = here();
        }

        // The host, to a ':' outside brackets
        u._host_begin = here();
        size_t host_end = i;
        if (i < end && in[i] == '[') {
            while (host_end < end && in[host_end] != ']') {
                ++host_end;
            }
            if (host_end == end) {
                return false;
            }
            uint16_t address[8];
            if (!url_parse_ipv6(in.substr(i + 1, host_end - i - 1), address)) {
                return false;
            }
            ++host_end;
            if (host_end < end && in[host_end] != ':') {
                return false;
            }
            at = url_write_ipv6(address, at);
            u._host_kind = HostKind::ipv6;
        } else {
            uint8_t odd = 0;   // a byte and not a bool, as in url_input
            for (; host_end < end && in[host_end] != ':'; ++host_end) {
                const uint8_t c = uint8_t(in[host_end]);
                odd |= bytes.kind[c];
                *at++ = bytes.lower[c];
            }
            if ((odd & UrlBytes::NotHost) || host_end == i) {
                return false;
            }
            const std::string_view host(out + u._host_begin, host_end - i);
            u._host_kind = HostKind::domain;
            // A name ending in a letter past 'f' other than x ("com")
            // cannot end in a number
            const char last = host.size() > 1 && host.back() == '.' ? host[host.size() - 2] : host.back();
            if ((url_hex(uint8_t(last)) >= 0 || last == 'x') && url_ends_in_number(host)) {
                uint32_t v;
                if (!url_parse_ipv4(host, v)) {
                    return false;
                }
                at = url_write_ipv4(v, out + u._host_begin);
                u._host_kind = HostKind::ipv4;
            }
        }
        u._host_end = here();

        // The port: digits only, the scheme's default dropped
        u._port = -1;
        if (host_end < end) {
            uint32_t port = 0;
            for (size_t k = host_end + 1; k < end; ++k) {
                if (!url_digit(uint8_t(in[k]))) {
                    return false;
                }
                port = port * 10 + uint32_t(in[k] - '0');
                if (port > 65535) {
                    return false;
                }
            }
            if (end > host_end + 1 && int32_t(port) != default_port) {
                u._port = int32_t(port);
                *at++ = ':';
                at = std::to_chars(at, at + 5, port).ptr;
            }
        }

        // The path: a segment after each '/', "." and ".." resolved as the
        // path state does, a '/' for a path that is empty
        i = end;
        u._path_begin = here();
        if (i < n && in[i] == '/') {
            ++i;
        }
        for (;;) {
            char* const segment = at;
            *at++ = '/';
            for (; i < n; ++i) {
                const uint8_t c = uint8_t(in[i]);
                const uint8_t k = bytes.kind[c];
                if (k & UrlBytes::InSegment) {
                    *at++ = char(c);
                } else if (k & UrlBytes::AuthorityEnd) {
                    break;
                } else if (k & UrlBytes::Backslash) {
                    return false;
                } else {
                    escape(c, UrlSets::path);
                }
            }
            const bool slash = i < n && in[i] == '/';
            const std::string_view text(segment + 1, size_t(at - segment - 1));
            const bool dots = text.size() <= 6 && (text.starts_with('.') || text.starts_with('%'));
            if (dots && url_double_dot(text)) {
                at = segment;   // and the segment before it, as "shorten" does
                while (at > out + u._path_begin && *--at != '/') {
                }
                if (!slash) {
                    *at++ = '/';
                }
            } else if (dots && url_single_dot(text)) {
                at = segment;
                if (!slash) {
                    *at++ = '/';
                }
            }
            if (!slash) {
                break;
            }
            ++i;
        }

        u._query_begin = Npos;
        if (i < n && in[i] == '?') {
            u._query_begin = here();
            *at++ = '?';
            for (++i; i < n && in[i] != '#'; ++i) {
                const uint8_t c = uint8_t(in[i]);
                if (bytes.kind[c] & UrlBytes::InQuery) {
                    *at++ = char(c);
                } else {
                    escape(c, UrlSets::special_query);
                }
            }
        }
        u._fragment_begin = Npos;
        if (i < n) {
            u._fragment_begin = here();
            *at++ = '#';
            for (++i; i < n; ++i) {
                const uint8_t c = uint8_t(in[i]);
                if (bytes.kind[c] & UrlBytes::InFragment) {
                    *at++ = char(c);
                } else {
                    escape(c, UrlSets::fragment);
                }
            }
        }
        u._opaque_path = false;
        u._href = string(std::string_view(out, here()));
        return true;
    }

    inline expected<url, io::error> url::with_scheme(const string& scheme) const {
        return detail::url_applied(detail::UrlAccess::protocol(*this, scheme), "set URL scheme", scheme);
    }

    inline expected<url, io::error> url::with_username(const string& username) const {
        return detail::url_applied(detail::UrlAccess::username(*this, username, false), "set URL username", username);
    }

    inline expected<url, io::error> url::with_password(const string& password) const {
        return detail::url_applied(detail::UrlAccess::username(*this, password, true), "set URL password", string());
    }

    inline expected<url, io::error> url::with_host(const string& host) const {
        return detail::url_applied(detail::UrlAccess::host(*this, host, false), "set URL host", host);
    }

    inline expected<url, io::error> url::with_hostname(const string& hostname) const {
        return detail::url_applied(detail::UrlAccess::host(*this, hostname, true), "set URL hostname", hostname);
    }

    inline expected<url, io::error> url::with_port(optional<uint16_t> port) const {
        return detail::url_applied(detail::UrlAccess::port(*this, port ? string(std::to_string(*port)) : string()), "set URL port", port ? string(std::to_string(*port)) : string());
    }

    inline expected<url, io::error> url::with_path(const string& path) const {
        return detail::url_applied(detail::UrlAccess::pathname(*this, path), "set URL path", path);
    }

    inline url url::with_query(const string& query) const {
        return detail::UrlAccess::search(*this, query);
    }

    inline url url::with_query(const net::query_params& params) const {
        // the URLSearchParams update steps: the serialization, or no
        // query for an empty one
        auto r = _record();
        auto s = params.to_string();
        r.has_query = !s.empty();
        r.query.assign(s.data(), s.size());
        return _from(r);
    }

    inline url url::with_fragment(const string& fragment) const {
        return detail::UrlAccess::hash(*this, fragment);
    }

    inline url url::without_fragment() const {
        auto r = _record();
        r.has_fragment = false;
        r.fragment.clear();
        return _from(r);
    }

    inline string url::origin() const {
        auto scheme = _href.view().substr(0, _scheme_end);
        if (scheme == "blob") {
            auto inner = parse(path());
            if (inner) {
                auto s = inner->_href.view().substr(0, inner->_scheme_end);
                if (s == "http" || s == "https") {
                    return inner->origin();
                }
            }
            return string("null");
        }
        if (scheme == "http" || scheme == "https" || scheme == "ws" || scheme == "wss" || scheme == "ftp") {
            std::string s(scheme);
            s += "://";
            s += _href.view().substr(_host_begin, _host_end - _host_begin);
            if (_port >= 0) {
                s += ':';
                s += std::to_string(_port);
            }
            return string(std::string_view(s));
        }
        return string("null");
    }

    inline query_params query_params::parse(const string& text) {
        query_params out;
        auto v = text.view();
        if (!v.empty() && v.front() == '?') {
            v.remove_prefix(1);
        }
        std::string name, value;
        size_t from = 0;
        while (from <= v.size()) {
            auto amp = v.find('&', from);
            auto piece = v.substr(from, amp == std::string_view::npos ? std::string_view::npos : amp - from);
            from = amp == std::string_view::npos ? v.size() + 1 : amp + 1;
            if (piece.empty()) {
                continue;
            }
            auto eq = piece.find('=');
            auto n = piece.substr(0, eq);
            auto w = eq == std::string_view::npos ? std::string_view() : piece.substr(eq + 1);
            auto decode = [](std::string_view s, std::string& to) {
                std::string plus(s);
                std::replace(plus.begin(), plus.end(), '+', ' ');
                to.clear();
                detail::url_utf8(detail::url_unescape(plus), to);
            };
            decode(n, name);
            decode(w, value);
            out._pairs.push_back(pair<string, string>(string(std::string_view(name)), string(std::string_view(value))));
        }
        return out;
    }

    inline string query_params::to_string() const {
        std::string s;
        for (auto& p : _pairs) {
            if (!s.empty()) {
                s.push_back('&');
            }
            for (char c : p.first.view()) {
                if (c == ' ') {
                    s.push_back('+');
                } else {
                    detail::url_escape(s, c, detail::UrlSets::form);
                }
            }
            s.push_back('=');
            for (char c : p.second.view()) {
                if (c == ' ') {
                    s.push_back('+');
                } else {
                    detail::url_escape(s, c, detail::UrlSets::form);
                }
            }
        }
        return string(std::string_view(s));
    }
}

template<>
struct std::hash<sgcl::net::url> {
    size_t operator()(const sgcl::net::url& u) const noexcept {
        return std::hash<sgcl::string>()(u._href);
    }
};
