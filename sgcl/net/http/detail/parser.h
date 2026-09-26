//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../url.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

// The parser of HTTP/1.1 (RFC 9112), pure: bytes in, a head or "not yet"
// or the status to refuse it with out. It knows nothing of sockets, so the
// rules are tested by vectors and a mutator, without a network.
//
// The rules against request smuggling, each where it is applied:
//
//   - the start line: one SP between the parts, a method that is a token,
//     a target without whitespace or controls in one of the four forms,
//     the version exactly HTTP/1.0 or HTTP/1.1 (another HTTP/x.y is 505,
//     anything else 400);
//   - the fields: a bare CR anywhere is 400; a bare LF ends a line (RFC
//     9112 §2.2 allows it, as Go and nginx do); whitespace between a name
//     and its colon, whitespace before the first field and obs-fold are
//     400; a name is a token; a value has no NUL, CR, LF or control but
//     HTAB (bytes 0x80 to 0xFF, obs-text, are let through);
//   - the framing of a request: Transfer-Encoding with Content-Length is
//     400 (RFC 9112 §6.3 lets a server take TE, a proxy in front may have
//     taken CL); a Transfer-Encoding other than exactly "chunked" is 501,
//     "chunked" twice 400, TE in an HTTP/1.0 request 400; a Content-Length
//     is 1*DIGIT with no sign or space, at most 2^63 - 1, and several of
//     them (fields or list items) must agree; none of the two is a body
//     of 0;
//   - chunked: a size of hex digits only, bounded; extensions by the
//     grammar, the line at most 4 KB; strictly CRLF after the size line
//     and after the data; trailers parsed as fields, with no framing field
//     (Content-Length, Transfer-Encoding, Host, Trailer) among them;
//   - Host: an HTTP/1.1 request without one, or with two, is 400, and the
//     value must be an authority (host and port).
//
// After any error of framing the connection is closed: where the next
// request would begin is no longer known.
namespace sgcl::net::http::detail {
    // The framing of a body. A word wide, as is BodyFraming whole: a
    // struct copied into a managed object carries its padding along, and
    // stale bytes of the stack there read as a pointer to the collector's
    // debug check of rule 2
    enum class Framing : uint64_t {
        none,           // no body: a request without CL and TE, a response to HEAD, 1xx, 204, 304
        length,         // Content-Length
        chunked,        // Transfer-Encoding: chunked
        until_close     // a response with neither: the body ends with the connection
    };

    struct BodyFraming {
        Framing kind = Framing::none;
        uint64_t length = 0;
    };

    // A number that may be absent, set into a managed object: emplace and
    // reset, never the assignment of an optional made on the stack, whose
    // payload (uninitialized when empty) the trivial copy would carry in
    inline void set_optional(optional<uint64_t>& to, const optional<uint64_t>& from) noexcept {
        if (from) {
            to.emplace(*from);
        } else {
            to.reset();
        }
    }

    // The bytes of the start line and the fields, as offsets into the head
    struct RequestLine {
        size_t method_at = 0, method_size = 0;
        size_t target_at = 0, target_size = 0;
        int minor = 1;              // HTTP/1.x
    };

    struct StatusLine {
        int minor = 1;
        int status = 0;
        size_t reason_at = 0, reason_size = 0;
    };

    // Where the head ends in [p, p + n): the offset just past the empty
    // line that ends it, 0 when the bytes hold no whole head yet. `from`
    // is where the last search stopped (a byte before, since a line end
    // may be cut between two reads), so a head read in pieces is scanned
    // once.
    inline size_t find_head_end(const char* p, size_t n, size_t from = 0) noexcept {
        for (size_t i = from; i < n; ++i) {
            if (p[i] != '\n') {
                continue;
            }
            if (i + 1 < n && p[i + 1] == '\n') {
                return i + 2;
            }
            if (i + 2 < n && p[i + 1] == '\r' && p[i + 2] == '\n') {
                return i + 3;
            }
        }
        return 0;
    }

    // Where the search is to go on after a failed one over n bytes
    inline size_t head_search_resume(size_t n) noexcept {
        return n >= 2 ? n - 2 : 0;
    }

    // Empty lines before a request line, which RFC 9112 §2.2 says a
    // server should skip (a client that ended a body with an extra CRLF):
    // how many bytes of them there are at the front
    inline size_t leading_empty_lines(const char* p, size_t n) noexcept {
        size_t i = 0;
        for (;;) {
            if (i < n && p[i] == '\n') {
                ++i;
            } else if (i + 1 < n && p[i] == '\r' && p[i + 1] == '\n') {
                i += 2;
            } else {
                return i;
            }
        }
    }

    // The next line of [at, end): its end without the terminator, and
    // where the one after it begins; false for a bare CR in it (or, when
    // strict, a line ending in a bare LF) or no terminator
    inline bool next_line(std::string_view s, size_t at, size_t& line_end, size_t& next, bool strict) noexcept {
        auto nl = s.find('\n', at);
        if (nl == std::string_view::npos) {
            return false;
        }
        size_t e = nl;
        if (e > at && s[e - 1] == '\r') {
            --e;
        } else if (strict) {
            return false;
        }
        for (size_t i = at; i < e; ++i) {
            if (s[i] == '\r') {
                return false;
            }
        }
        line_end = e;
        next = nl + 1;
        return true;
    }

    inline bool field_value_char(uint8_t c) noexcept {
        return c == '\t' || (c >= 0x20 && c != 0x7F);
    }

    // The field lines of s from `at` to the empty line that ends them,
    // added to h as slices of s: 0, or the status to refuse them with
    inline int parse_fields(const string& s, size_t at, headers& h, bool strict, size_t& end) {
        auto v = s.view();
        for (;;) {
            size_t e, next;
            if (!next_line(v, at, e, next, strict)) {
                return 400;
            }
            if (e == at) {
                end = next;
                return 0;
            }
            if (v[at] == ' ' || v[at] == '\t') {
                return 400;   // obs-fold, or whitespace before the first field
            }
            auto colon = v.find(':', at);
            if (colon == std::string_view::npos || colon >= e) {
                return 400;
            }
            if (!is_token(v.substr(at, colon - at))) {
                return 400;   // whitespace before the colon among it
            }
            size_t vb = colon + 1, ve = e;
            while (vb < ve && (v[vb] == ' ' || v[vb] == '\t')) {
                ++vb;
            }
            while (ve > vb && (v[ve - 1] == ' ' || v[ve - 1] == '\t')) {
                --ve;
            }
            for (size_t i = vb; i < ve; ++i) {
                if (!field_value_char(uint8_t(v[i]))) {
                    return 400;
                }
            }
            HeadersAccess::add(h, s.as_slice(at, colon - at), s.as_slice(vb, ve - vb));
            at = next;
        }
    }

    // "HTTP/1.0", "HTTP/1.1": the minor; -1 for another HTTP/x.y (505),
    // -2 for anything else (400)
    inline int parse_version(std::string_view v) noexcept {
        if (v.size() != 8 || v.substr(0, 5) != "HTTP/" || v[6] != '.' || v[5] < '0' || v[5] > '9' || v[7] < '0' || v[7] > '9') {
            return -2;
        }
        if (v[5] == '1' && (v[7] == '0' || v[7] == '1')) {
            return v[7] - '0';
        }
        return -1;
    }

    // A request head: the request line and the fields. 0, or the status
    inline int parse_request_head(const string& s, RequestLine& line, headers& h) {
        auto v = s.view();
        size_t e, next;
        if (!next_line(v, 0, e, next, false)) {
            return 400;
        }
        auto sp1 = v.find(' ');
        if (sp1 == std::string_view::npos || sp1 >= e) {
            return 400;
        }
        auto sp2 = v.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos || sp2 >= e) {
            return 400;
        }
        auto method = v.substr(0, sp1);
        auto target = v.substr(sp1 + 1, sp2 - sp1 - 1);
        auto version = v.substr(sp2 + 1, e - sp2 - 1);
        if (!is_token(method) || target.empty()) {
            return 400;
        }
        for (char c : target) {
            if (uint8_t(c) <= 0x20 || uint8_t(c) >= 0x7F) {
                return 400;   // no whitespace, no controls, ASCII only
            }
        }
        int minor = parse_version(version);
        if (minor == -1) {
            return 505;
        }
        if (minor < 0) {
            return 400;
        }
        // the four forms (RFC 9112 §3.2): origin ("/x"), asterisk (OPTIONS
        // only), authority (CONNECT only), absolute ("http://h/x")
        if (method == "CONNECT") {
            if (target.front() == '/' || target == "*") {
                return 400;
            }
        } else if (target == "*") {
            if (method != "OPTIONS") {
                return 400;
            }
        } else if (target.front() != '/') {
            auto colon = target.find(':');
            if (colon == std::string_view::npos || colon == 0) {
                return 400;
            }
            for (size_t i = 0; i < colon; ++i) {
                char c = target[i];
                bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'));
                if (!ok) {
                    return 400;
                }
            }
        }
        line.method_at = 0;
        line.method_size = sp1;
        line.target_at = sp1 + 1;
        line.target_size = target.size();
        line.minor = minor;
        size_t end;
        return parse_fields(s, next, h, false, end);
    }

    // A response head. 0, or 1 for a head that breaks RFC 9112
    inline int parse_response_head(const string& s, StatusLine& line, headers& h) {
        auto v = s.view();
        size_t e, next;
        if (!next_line(v, 0, e, next, false)) {
            return 1;
        }
        if (e < 12 || v[8] != ' ') {
            return 1;
        }
        int minor = parse_version(v.substr(0, 8));
        if (minor < 0) {
            return 1;
        }
        int code = 0;
        for (size_t i = 9; i < 12; ++i) {
            if (v[i] < '0' || v[i] > '9') {
                return 1;
            }
            code = code * 10 + (v[i] - '0');
        }
        if (code < 100) {
            return 1;
        }
        size_t reason = e;
        if (e > 12) {
            if (v[12] != ' ') {
                return 1;
            }
            reason = 13;
            for (size_t i = 13; i < e; ++i) {
                if (!field_value_char(uint8_t(v[i]))) {
                    return 1;
                }
            }
        }
        line.minor = minor;
        line.status = code;
        line.reason_at = reason;
        line.reason_size = e - reason;
        size_t end;
        return parse_fields(s, next, h, false, end) ? 1 : 0;
    }

    // Every Content-Length of h, fields and list items, must be 1*DIGIT and
    // agree: the value, or nullopt when there is none; false when they break it
    inline bool content_length(const headers& h, optional<uint64_t>& out) noexcept {
        out = nullopt;
        for (auto& f : HeadersAccess::fields(h)) {
            if (!iequal(f.first.view(), "content-length")) {
                continue;
            }
            std::string_view v = f.second.view();
            for (;;) {
                auto comma = v.find(',');
                auto item = trim_ows(v.substr(0, comma));
                if (item.empty() || item.size() > 19) {
                    return false;
                }
                uint64_t n = 0;
                for (char c : item) {
                    if (c < '0' || c > '9') {
                        return false;
                    }
                    n = n * 10 + uint64_t(c - '0');
                }
                if (n > uint64_t(std::numeric_limits<int64_t>::max())) {
                    return false;
                }
                if (out && *out != n) {
                    return false;
                }
                out = n;
                if (comma == std::string_view::npos) {
                    break;
                }
                v.remove_prefix(comma + 1);
            }
        }
        return true;
    }

    // The codings of every Transfer-Encoding of h: 0 when there is none,
    // 1 for exactly "chunked", 400 for chunked twice or no coding at all,
    // 501 for anything else
    inline int transfer_encoding(const headers& h) noexcept {
        bool any = false;
        int chunked = 0, other = 0;
        for (auto& f : HeadersAccess::fields(h)) {
            if (!iequal(f.first.view(), "transfer-encoding")) {
                continue;
            }
            any = true;
            std::string_view v = f.second.view();
            for (;;) {
                auto comma = v.find(',');
                auto item = trim_ows(v.substr(0, comma));
                if (iequal(item, "chunked")) {
                    ++chunked;
                } else if (!item.empty()) {
                    ++other;
                }
                if (comma == std::string_view::npos) {
                    break;
                }
                v.remove_prefix(comma + 1);
            }
        }
        if (!any) {
            return 0;
        }
        if (other) {
            return 501;
        }
        if (chunked != 1) {
            return 400;
        }
        return 1;
    }

    // The framing of a request's body (RFC 9112 §6.3): 0, or the status
    inline int request_framing(const headers& h, int minor, BodyFraming& out) noexcept {
        int te = transfer_encoding(h);
        optional<uint64_t> cl;
        bool cl_ok = content_length(h, cl);
        bool has_cl = HeadersAccess::count(h, "content-length") != 0;
        if (te && has_cl) {
            return 400;
        }
        if (te) {
            if (minor == 0) {
                return 400;
            }
            if (te != 1) {
                return te;
            }
            out = {Framing::chunked, 0};
            return 0;
        }
        if (!cl_ok) {
            return 400;
        }
        if (cl) {
            out = {*cl ? Framing::length : Framing::none, *cl};
            return 0;
        }
        out = {Framing::none, 0};
        return 0;
    }

    // The framing of a response's body (RFC 9112 §6.3), the same rules a
    // request is held to; false for a response that breaks them
    inline bool response_framing(const headers& h, int status, bool head_request, BodyFraming& out) noexcept {
        if (head_request || (status >= 100 && status < 200) || status == 204 || status == 304) {
            out = {Framing::none, 0};
            return true;
        }
        int te = transfer_encoding(h);
        optional<uint64_t> cl;
        bool cl_ok = content_length(h, cl);
        bool has_cl = HeadersAccess::count(h, "content-length") != 0;
        if (te && has_cl) {
            return false;
        }
        if (te) {
            if (te != 1) {
                return false;
            }
            out = {Framing::chunked, 0};
            return true;
        }
        if (!cl_ok) {
            return false;
        }
        if (cl) {
            out = {*cl ? Framing::length : Framing::none, *cl};
            return true;
        }
        out = {Framing::until_close, 0};
        return true;
    }

    // The value of Host (RFC 9110 §7.2): uri-host [":" port], where the
    // host is an IP literal in brackets or a reg-name (unreserved,
    // sub-delims, %XX), possibly empty
    inline bool valid_host(std::string_view v) noexcept {
        std::string_view host = v, port;
        if (!v.empty() && v.front() == '[') {
            auto close = v.find(']');
            if (close == std::string_view::npos) {
                return false;
            }
            host = v.substr(0, close + 1);
            auto rest = v.substr(close + 1);
            if (!rest.empty()) {
                if (rest.front() != ':') {
                    return false;
                }
                port = rest.substr(1);
            }
            uint16_t pieces[8];
            if (host.size() < 3 || !net::detail::url_parse_ipv6(host.substr(1, host.size() - 2), pieces)) {
                return false;
            }
        } else {
            auto colon = v.rfind(':');
            if (colon != std::string_view::npos) {
                host = v.substr(0, colon);
                port = v.substr(colon + 1);
            }
            for (size_t i = 0; i < host.size(); ++i) {
                uint8_t c = uint8_t(host[i]);
                bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
                bool sub = c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' || c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
                if (c == '%') {
                    if (i + 2 >= host.size() || net::detail::url_hex(uint8_t(host[i + 1])) < 0 || net::detail::url_hex(uint8_t(host[i + 2])) < 0) {
                        return false;
                    }
                    i += 2;
                    continue;
                }
                if (!unreserved && !sub) {
                    return false;
                }
            }
        }
        for (char c : port) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }

    // The decoder of chunked (RFC 9112 §7.1) as a state machine over the
    // bytes given it, so that a body cut anywhere, a byte at a time, reads
    // the same. step() takes what it can of `in`: the size lines, the
    // CRLFs and the trailers are consumed and kept in the decoder, the
    // data is handed back as a span of `in` (at most `room` bytes of it).
    class ChunkedDecoder {
    public:
        static constexpr size_t MaxLine = 4096;

        struct Step {
            size_t consumed = 0;        // bytes of in taken
            size_t data_at = 0;         // the data among them
            size_t data_size = 0;
            bool done = false;          // the last chunk and the trailers read
            int error = 0;              // 400: the framing is broken
        };

        explicit ChunkedDecoder(size_t max_trailer = 32 * 1024) noexcept
        : _max_trailer(max_trailer) {
        }

        Step step(std::string_view in, size_t room) {
            Step r;
            size_t i = 0;
            while (i < in.size() && !r.done && !r.error) {
                switch (_state) {
                    case State::size_line: {
                        auto nl = in.find('\n', i);
                        size_t take = (nl == std::string_view::npos ? in.size() : nl + 1) - i;
                        if (_line.size() + take > MaxLine) {
                            r.error = 400;
                            break;
                        }
                        _line.append(in.substr(i, take));
                        i += take;
                        if (nl == std::string_view::npos) {
                            break;
                        }
                        if (!_size_line()) {
                            r.error = 400;
                            break;
                        }
                        _line.clear();
                        _state = _remaining ? State::data : State::trailers;
                        break;
                    }
                    case State::data: {
                        if (r.data_size) {
                            // one span a step: the caller copies it out first
                            r.consumed = i;
                            return r;
                        }
                        size_t n = size_t(std::min<uint64_t>(_remaining, std::min(in.size() - i, room)));
                        if (n == 0) {
                            r.consumed = i;
                            return r;
                        }
                        r.data_at = i;
                        r.data_size = n;
                        i += n;
                        _remaining -= n;
                        if (_remaining == 0) {
                            _state = State::data_cr;
                        }
                        break;
                    }
                    case State::data_cr:
                        if (in[i] != '\r') {
                            r.error = 400;
                            break;
                        }
                        ++i;
                        _state = State::data_lf;
                        break;
                    case State::data_lf:
                        if (in[i] != '\n') {
                            r.error = 400;
                            break;
                        }
                        ++i;
                        _state = State::size_line;
                        break;
                    case State::trailers: {
                        // the trailer section, to the empty line, strictly CRLF
                        size_t was = _trailer.size();
                        auto nl = in.find('\n', i);
                        size_t take = (nl == std::string_view::npos ? in.size() : nl + 1) - i;
                        if (was + take > _max_trailer) {
                            r.error = 400;
                            break;
                        }
                        _trailer.append(in.substr(i, take));
                        i += take;
                        if (nl == std::string_view::npos) {
                            break;
                        }
                        // a whole line came: the empty one ends the section
                        size_t line_start = _trailer.rfind('\n', _trailer.size() - 2);
                        line_start = line_start == std::string::npos ? 0 : line_start + 1;
                        if (_trailer.size() - line_start == 2 && _trailer[line_start] == '\r') {
                            if (!_parse_trailers()) {
                                r.error = 400;
                                break;
                            }
                            _state = State::done;
                            r.done = true;
                        } else if (_trailer.size() - line_start < 2 || _trailer[_trailer.size() - 2] != '\r') {
                            r.error = 400;   // a bare LF
                        }
                        break;
                    }
                    case State::done:
                        r.done = true;
                        break;
                }
            }
            if (_state == State::done) {
                r.done = true;
            }
            r.consumed = i;
            return r;
        }

        bool done() const noexcept {
            return _state == State::done;
        }

        const headers& trailers() const noexcept {
            return _trailers;
        }

    private:
        enum class State : uint8_t {
            size_line, data, data_cr, data_lf, trailers, done
        };

        // chunk-size [ chunk-ext ] CRLF, the line in _line with its CRLF
        bool _size_line() {
            std::string_view v(_line);
            if (v.size() < 2 || v[v.size() - 2] != '\r') {
                return false;
            }
            v.remove_suffix(2);
            size_t i = 0;
            uint64_t size = 0;
            size_t digits = 0;
            while (i < v.size()) {
                int d = net::detail::url_hex(uint8_t(v[i]));
                if (d < 0) {
                    break;
                }
                if (size >> 59) {
                    return false;   // past 2^63
                }
                size = size * 16 + uint64_t(d);
                ++digits;
                ++i;
            }
            if (!digits) {
                return false;
            }
            // chunk-ext = *( BWS ";" BWS ext-name [ BWS "=" BWS ext-val ] )
            auto ows = [&] {
                while (i < v.size() && (v[i] == ' ' || v[i] == '\t')) {
                    ++i;
                }
            };
            auto token = [&] {
                size_t from = i;
                while (i < v.size() && token_char(uint8_t(v[i]))) {
                    ++i;
                }
                return i > from;
            };
            while (i < v.size()) {
                ows();
                if (i >= v.size() || v[i] != ';') {
                    return false;
                }
                ++i;
                ows();
                if (!token()) {
                    return false;
                }
                ows();
                if (i < v.size() && v[i] == '=') {
                    ++i;
                    ows();
                    if (i < v.size() && v[i] == '"') {
                        ++i;
                        bool closed = false;
                        while (i < v.size()) {
                            uint8_t c = uint8_t(v[i]);
                            if (c == '"') {
                                closed = true;
                                ++i;
                                break;
                            }
                            if (c == '\\') {
                                if (i + 1 >= v.size() || !field_value_char(uint8_t(v[i + 1]))) {
                                    return false;
                                }
                                i += 2;
                                continue;
                            }
                            if (!field_value_char(c)) {
                                return false;
                            }
                            ++i;
                        }
                        if (!closed) {
                            return false;
                        }
                    } else if (!token()) {
                        return false;
                    }
                }
            }
            _remaining = size;
            return true;
        }

        bool _parse_trailers() {
            // the section ends in the empty line: fields as a head's, then
            // no framing field among them
            if (_trailer == "\r\n") {
                return true;
            }
            auto s = string(std::string_view(_trailer));
            size_t end;
            if (parse_fields(s, 0, _trailers, true, end)) {
                return false;
            }
            for (auto& f : HeadersAccess::fields(_trailers)) {
                auto n = f.first.view();
                if (iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "host") || iequal(n, "trailer")) {
                    return false;
                }
            }
            return true;
        }

        State _state = State::size_line;
        uint64_t _remaining = 0;
        std::string _line;
        std::string _trailer;
        headers _trailers;
        size_t _max_trailer;
    };

    // Everything a server checks of a request's head before a handler
    // sees it: the lines, the framing, Host. 0, or the status to refuse
    // it with (and close)
    inline int check_request_head(const string& head, RequestLine& line, headers& h, BodyFraming& framing) {
        int refused = parse_request_head(head, line, h);
        if (!refused) {
            refused = request_framing(h, line.minor, framing);
        }
        if (!refused) {
            auto count = HeadersAccess::count(h, "host");
            auto host = HeadersAccess::find(h, "host");
            if (count > 1 || (line.minor == 1 && count == 0) || (host && !valid_host(*host))) {
                refused = 400;
            }
        }
        return refused;
    }
}
