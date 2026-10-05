//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "headers.h"
#include "detail/parser.h"
#include "../error.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/detail/bytes.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

// A multipart body read as it comes (RFC 2046 §5.1, multipart/form-data of
// RFC 7578): its parts one after the other, each part's head parsed and
// its content read as a stream, nothing of the body held but a buffer of
// 32 KB, as Go's multipart.Reader.
namespace sgcl::net::http {
    namespace detail {
        using namespace sgcl::detail;

        // The window over the body: two to a page
        inline constexpr size_t MultipartWindow = 32768;
        using MultipartBlock = array<byte, MultipartWindow>;

        // A transport padding after a boundary longer than this is not one
        inline constexpr size_t MultipartMaxPadding = 1024;

        // The value of a parameter: a token, or a quoted-string with its
        // backslashes taken out (RFC 9110 §5.6.4); `at` past it
        inline bool param_value(std::string_view v, size_t& at, std::string& out) noexcept {
            out.clear();
            if (at < v.size() && v[at] == '"') {
                for (++at; at < v.size(); ++at) {
                    char c = v[at];
                    if (c == '"') {
                        ++at;
                        return true;
                    }
                    if (c == '\\' && at + 1 < v.size()) {
                        c = v[++at];
                    }
                    out += c;
                }
                return false;   // no closing quote
            }
            while (at < v.size() && v[at] != ';' && v[at] != ' ' && v[at] != '\t') {
                out += v[at++];
            }
            return true;
        }

        // RFC 8187's ext-value, UTF-8''%e2%82%ac.txt: the percent-decoded
        // text of a value in UTF-8 or US-ASCII; nullopt for another charset
        inline optional<std::string> ext_value(std::string_view v) noexcept {
            auto q1 = v.find('\'');
            if (q1 == std::string_view::npos) {
                return nullopt;
            }
            auto q2 = v.find('\'', q1 + 1);
            if (q2 == std::string_view::npos) {
                return nullopt;
            }
            auto charset = v.substr(0, q1);
            if (!iequal(charset, "utf-8") && !iequal(charset, "us-ascii")) {
                return nullopt;
            }
            return net::detail::url_unescape(v.substr(q2 + 1));
        }

        // The last element of a file's name, as Go's Part.FileName: the
        // part after the last '/' or '\', none for "." and ".."
        inline std::string_view base_name(std::string_view f) noexcept {
            auto slash = f.find_last_of("/\\");
            if (slash != std::string_view::npos) {
                f.remove_prefix(slash + 1);
            }
            return f == "." || f == ".." ? std::string_view() : f;
        }

        // Content-Disposition's name and filename (filename* preferred):
        // `form-data; name="field"; filename="a.txt"`. Parameters past one
        // that cannot be read are not looked at, as Go's reader gives no
        // name for a field it cannot parse
        inline void parse_disposition(std::string_view v, string& name, string& filename) noexcept {
            size_t at = v.find(';');
            std::string value;
            optional<std::string> plain_filename, ext_filename;
            optional<std::string> got_name;
            while (at != std::string_view::npos && at < v.size()) {
                ++at;   // past ';'
                while (at < v.size() && (v[at] == ' ' || v[at] == '\t')) {
                    ++at;
                }
                size_t eq = v.find('=', at);
                if (eq == std::string_view::npos) {
                    break;
                }
                std::string_view key = trim_ows(v.substr(at, eq - at));
                at = eq + 1;
                while (at < v.size() && (v[at] == ' ' || v[at] == '\t')) {
                    ++at;
                }
                if (!param_value(v, at, value)) {
                    break;
                }
                if (iequal(key, "name") && !got_name) {
                    got_name = value;
                } else if (iequal(key, "filename") && !plain_filename) {
                    plain_filename = value;
                } else if (iequal(key, "filename*") && !ext_filename) {
                    ext_filename = ext_value(value);
                }
                while (at < v.size() && v[at] != ';') {
                    ++at;
                }
            }
            name = got_name ? string(std::string_view(*got_name)) : string();
            const optional<std::string>& f = ext_filename ? ext_filename : plain_filename;
            filename = f ? string(base_name(*f)) : string();
        }

        // The reader's state: the window over the body, where the parse
        // is, the delimiter once the first boundary told the line ends
        struct MultipartState {
            io::reader source;
            std::string boundary;
            size_t max_parts = 1000;
            size_t max_header_bytes = 16384;
            tracked_ptr<MultipartBlock> block;
            size_t at = 0, end = 0;          // the window's unread bytes
            bool eof = false;                 // the source ended
            char before = '\n';              // the byte before the window's first (the body's start is a line's)
            enum class Phase : uint8_t { start, delimiter, head, content, done, failed };
            Phase phase = Phase::start;
            std::string delimiter;            // CRLF "--" boundary, or LF "--" boundary
            size_t match = 0;                 // the boundary's length where `at` stands at one (phase delimiter)
            size_t parts = 0;
            optional<io::error> error;

            SGCL_INLINE_HOT const char* data() const noexcept {
                return reinterpret_cast<const char*>(block->data());
            }

            SGCL_INLINE_HOT std::string_view window() const noexcept {
                return std::string_view(data() + at, end - at);
            }
        };

        // What a step of the parse came to: more bytes wanted, or done
        // (its value or the state's error)
        enum class MultipartStep : uint8_t { more, done };

        SGCL_INLINE_HOT io::error multipart_error(errc e) noexcept {
            return net::detail::net_error(e, "multipart");
        }

        // A failure kept: every later step gives it
        SGCL_INLINE_HOT MultipartStep multipart_fail(MultipartState& s, const io::error& e) noexcept {
            s.phase = MultipartState::Phase::failed;
            s.error = e;
            return MultipartStep::done;
        }

        // What follows a boundary at w[from]: 2 the close delimiter ("--"),
        // 1 a boundary line (transport padding, then CRLF or LF), 0 not a
        // boundary (data, or a preamble's line), -1 not known yet; `line`
        // the bytes from `from` to the line's end
        inline int after_boundary(std::string_view w, size_t from, size_t& line, bool eof) noexcept {
            if (w.size() - from < 2) {
                if (w.size() - from == 1 && w[from] != '-' && w[from] != '\r' && w[from] != '\n' && w[from] != ' ' && w[from] != '\t') {
                    return 0;
                }
                return eof ? 0 : -1;
            }
            if (w[from] == '-' && w[from + 1] == '-') {
                line = 2;
                return 2;
            }
            size_t i = from;
            while (i < w.size() && (w[i] == ' ' || w[i] == '\t') && i - from <= MultipartMaxPadding) {
                ++i;
            }
            if (i - from > MultipartMaxPadding) {
                return 0;
            }
            if (i == w.size()) {
                return eof ? 0 : -1;
            }
            if (w[i] == '\n') {
                line = i + 1 - from;
                return 1;
            }
            if (w[i] == '\r') {
                if (i + 1 == w.size()) {
                    return eof ? 0 : -1;
                }
                if (w[i + 1] == '\n') {
                    line = i + 2 - from;
                    return 1;
                }
            }
            return 0;
        }

        // The window's bytes moved to its front, room made for a read
        SGCL_INLINE_HOT void multipart_compact(MultipartState& s) noexcept {
            if (s.at == 0) {
                return;
            }
            size_t n = s.end - s.at;
            if (n) {
                sgcl::detail::move_bytes(s.block->data(), s.block->data() + s.at, n);
            }
            s.at = 0;
            s.end = n;
        }

        // Phase start: the first boundary, "--" boundary at the body's
        // start or after a line break of the preamble; what it ends with
        // decides the delimiter's line break
        inline MultipartStep multipart_start(MultipartState& s) noexcept {
            const std::string dash = "--" + s.boundary;
            std::string_view w = s.window();
            size_t from = 0;
            for (;;) {
                size_t p = w.find(dash, from);
                if (p == std::string_view::npos) {
                    break;
                }
                if ((p ? w[p - 1] : s.before) != '\n') {
                    from = p + 1;   // inside a line of the preamble
                    continue;
                }
                size_t line = 0;
                int k = after_boundary(w, p + dash.size(), line, s.eof);
                if (k < 0) {
                    if (p) {
                        s.before = w[p - 1];
                        s.at += p;   // kept from the boundary
                    }
                    return MultipartStep::more;
                }
                if (k == 0) {
                    from = p + 1;
                    continue;
                }
                bool crlf = k == 2 || (line >= 2 && w[p + dash.size() + line - 2] == '\r');
                s.delimiter = (crlf ? "\r\n--" : "\n--") + s.boundary;
                s.at += p;
                s.match = dash.size();
                s.phase = MultipartState::Phase::delimiter;
                return MultipartStep::done;
            }
            if (s.eof) {
                return multipart_fail(s, multipart_error(errc::malformed_multipart));   // no boundary
            }
            // none here: the preamble dropped, the tail where a boundary may
            // begin kept, and the byte before it noted
            size_t keep = std::min(w.size(), dash.size());
            if (w.size() > keep) {
                s.before = w[w.size() - keep - 1];
                s.at = s.end - keep;
            }
            return MultipartStep::more;
        }

        // The next valid delimiter in the window from `from`: its place,
        // npos for none (`safe` then the bytes that are content for sure),
        // or `undecided` when one may be there and the bytes after it are
        // not in yet
        inline size_t find_delimiter(const MultipartState& s, std::string_view w, size_t& safe, bool& undecided, size_t& line, int& kind) noexcept {
            undecided = false;
            const size_t dl = s.delimiter.size();
            size_t from = 0;
            for (;;) {
                size_t p = w.find(s.delimiter, from);
                if (p == std::string_view::npos) {
                    // a delimiter cut at the window's end is kept back
                    size_t tail = std::min(w.size(), dl - 1);
                    size_t q = w.size() - tail;
                    for (; q < w.size(); ++q) {
                        if (w[q] == s.delimiter[0] && s.delimiter.compare(0, w.size() - q, w.substr(q)) == 0) {
                            break;
                        }
                    }
                    safe = s.eof ? w.size() : q;
                    return std::string_view::npos;
                }
                int k = after_boundary(w, p + dl, line, s.eof);
                if (k < 0) {
                    safe = p;
                    undecided = true;
                    return std::string_view::npos;
                }
                if (k > 0) {
                    kind = k;
                    safe = p;
                    return p;
                }
                from = p + 1;   // the boundary's text inside the content
            }
        }

        // Phase content, the content's bytes into out (0 at its end, the
        // delimiter then the next to read); or more wanted
        inline MultipartStep multipart_read(MultipartState& s, const slice<byte>& out, size_t& got) noexcept {
            got = 0;
            std::string_view w = s.window();
            size_t safe = 0, line = 0;
            bool undecided = false;
            int kind = 0;
            size_t p = find_delimiter(s, w, safe, undecided, line, kind);
            if (p == 0) {
                s.match = s.delimiter.size();
                s.phase = MultipartState::Phase::delimiter;
                return MultipartStep::done;   // the content's end
            }
            if (safe == 0) {
                if (s.eof) {
                    return multipart_fail(s, io::error(io::errc::unexpected_eof, "multipart"));   // no close delimiter
                }
                return MultipartStep::more;
            }
            got = std::min(safe, out.size());
            sgcl::detail::copy_bytes(out.data(), w.data(), got);
            s.at += got;
            return MultipartStep::done;
        }

        // Phase delimiter: the boundary line read (at the close delimiter
        // the end); then phase head
        inline MultipartStep multipart_delimiter(MultipartState& s, bool& finished) noexcept {
            finished = false;
            std::string_view w = s.window();
            size_t line = 0;
            int k = after_boundary(w, s.match, line, s.eof);
            if (k < 0) {
                if (w.size() >= MultipartMaxPadding + s.match + 2) {
                    return multipart_fail(s, multipart_error(errc::malformed_multipart));
                }
                return s.eof ? multipart_fail(s, io::error(io::errc::unexpected_eof, "multipart")) : MultipartStep::more;
            }
            if (k == 0) {
                return multipart_fail(s, multipart_error(errc::malformed_multipart));
            }
            if (k == 2) {
                s.phase = MultipartState::Phase::done;   // the epilogue is not read
                finished = true;
                return MultipartStep::done;
            }
            s.at += s.match + line;
            s.phase = MultipartState::Phase::head;
            return MultipartStep::done;
        }

        inline MultipartStep multipart_skip(MultipartState& s) noexcept;

        // The values a request's form() keeps together, as Go's
        // ParseMultipartForm keeps 10 MB of them in memory
        inline constexpr size_t FormValuesMax = size_t(10) << 20;

        // The media type of a Content-Type, in lower case, without its
        // parameters: "multipart/form-data"
        inline std::string media_type(std::string_view v) noexcept {
            v = trim_ows(v.substr(0, v.find(';')));
            std::string t(v);
            for (auto& c : t) {
                c = ascii_lower(c);
            }
            return t;
        }

        // The boundary of a multipart Content-Type (RFC 2046 §5.1.1: 1 to
        // 70 characters); not_multipart for another type, malformed_multipart
        // for none
        inline expected<string, errc> multipart_boundary(std::string_view v) noexcept {
            auto type = media_type(v);
            if (type.size() <= 10 || type.compare(0, 10, "multipart/") != 0) {
                return unexpected(errc::not_multipart);
            }
            size_t at = v.find(';');
            std::string value;
            while (at != std::string_view::npos && at < v.size()) {
                ++at;
                while (at < v.size() && (v[at] == ' ' || v[at] == '\t')) {
                    ++at;
                }
                size_t eq = v.find('=', at);
                if (eq == std::string_view::npos) {
                    break;
                }
                std::string_view key = trim_ows(v.substr(at, eq - at));
                at = eq + 1;
                while (at < v.size() && (v[at] == ' ' || v[at] == '\t')) {
                    ++at;
                }
                if (!param_value(v, at, value)) {
                    break;
                }
                if (iequal(key, "boundary")) {
                    if (value.empty() || value.size() > 70) {
                        break;
                    }
                    return string(std::string_view(value));
                }
                while (at < v.size() && v[at] != ';') {
                    ++at;
                }
            }
            return unexpected(errc::malformed_multipart);
        }
    }

    // A multipart body read part by part, as it comes: multipart/form-data
    // of a request (request::multipart), or any multipart body over a
    // stream with its boundary. next() goes to the next part, past what
    // was left of the current one, and gives its head; the reads give that
    // part's content to its end, as a stream (io::copy of it to a file
    // holds 32 KB at a time). Go's multipart.Reader and its Part in one, as
    // compress::tar::reader is. A handle of one word: copies are the same
    // reader. The preamble before the first boundary and the epilogue
    // after the last are dropped; line breaks are CRLF, or LF alone when
    // the first boundary's line ends so (as Go reads them).
    class multipart_reader : public io::mixin::reader<multipart_reader> {
    public:
        // What a part's head says: Content-Disposition's name and filename
        // (its last element; filename* preferred), Content-Type as sent
        // (empty when none: text/plain by RFC 7578), and every field
        struct part {
            string name;
            string filename;
            string content_type;
            http::headers headers;
        };

        // How much a body may hold: parts (Go's default, 1000), and the
        // bytes of one part's head (at most the buffer's 32 KB). The body's
        // whole size is its stream's to bound: a request's is the server's
        // max_body_bytes
        struct limits {
            size_t max_parts = 1000;
            size_t max_header_bytes = 16384;
        };

        // No body; an operation on it is a contract violation
        multipart_reader() noexcept = default;

        // The parts of body, delimited by boundary (the Content-Type's
        // boundary parameter)
        SGCL_INLINE_HOT multipart_reader(const io::reader& body, const string& boundary) noexcept
        : multipart_reader(body, boundary, limits()) {
        }

        SGCL_INLINE_HOT multipart_reader(const io::reader& body, const string& boundary, const limits& l) noexcept
        : _s(make_tracked<detail::MultipartState>()) {
            _s->source = body;
            _s->boundary = std::string(boundary.view());
            _s->max_parts = l.max_parts;
            _s->max_header_bytes = std::min(l.max_header_bytes, detail::MultipartWindow - 1);
            _s->block = make_tracked<detail::MultipartBlock>();
            if (_s->boundary.empty() || _s->boundary.size() > 70) {
                (void)detail::multipart_fail(*_s, detail::multipart_error(errc::malformed_multipart));   // RFC 2046: 1 to 70 characters
            }
        }

        // The next part's head, nullopt after the last; what was left of
        // the current part's content is read past
        // `next(...)` on this thread, `co_await async_next(...)` in a task
        SGCL_INLINE_HOT expected<optional<part>, io::error> next() const {
            detail::MultipartState& s = *_s;
            for (;;) {
                optional<part> p;
                if (_step_next(s, p) == detail::MultipartStep::done) {
                    return _result(s, std::move(p));
                }
                if (auto e = _fill(s)) {
                    return unexpected(*e);
                }
            }
        }

        SGCL_INLINE_HOT async::task<expected<optional<part>, io::error>> async_next() const noexcept {
            return _co_next(_s);
        }

        // The current part's content, at most buffer.size() bytes; 0 at its
        // end, and before the first part and after the last
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> read(const slice<byte>& buffer) const {
            detail::MultipartState& s = *_s;
            for (;;) {
                size_t got = 0;
                if (_step_read(s, buffer, got) == detail::MultipartStep::done) {
                    if (s.phase == detail::MultipartState::Phase::failed) {
                        return unexpected(*s.error);
                    }
                    return got;
                }
                if (auto e = _fill(s)) {
                    return unexpected(*e);
                }
            }
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept {
            return _co_read(_s, buffer);
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        using Phase = detail::MultipartState::Phase;
        using Step = detail::MultipartStep;

        static async::task<expected<optional<part>, io::error>> _co_next(tracked_ptr<detail::MultipartState> s) noexcept {
            for (;;) {
                optional<part> p;
                if (_step_next(*s, p) == Step::done) {
                    co_return _result(*s, std::move(p));
                }
                if (auto e = co_await _async_fill(s)) {
                    co_return unexpected(*e);
                }
            }
        }

        static async::task<expected<size_t, io::error>> _co_read(tracked_ptr<detail::MultipartState> s, slice<byte> buffer) noexcept {
            for (;;) {
                size_t got = 0;
                if (_step_read(*s, buffer, got) == Step::done) {
                    if (s->phase == Phase::failed) {
                        co_return unexpected(*s->error);
                    }
                    co_return got;
                }
                if (auto e = co_await _async_fill(s)) {
                    co_return unexpected(*e);
                }
            }
        }

        // A step of next: the current part's rest read past, the boundary's
        // line, the head parsed
        static Step _step_next(detail::MultipartState& s, optional<part>& out) noexcept {
            for (;;) {
                switch (s.phase) {
                    case Phase::failed:
                    case Phase::done:
                        return Step::done;
                    case Phase::start:
                        if (detail::multipart_start(s) == Step::more) {
                            return Step::more;
                        }
                        break;
                    case Phase::content:
                        if (detail::multipart_skip(s) == Step::more) {
                            return Step::more;
                        }
                        break;
                    case Phase::delimiter: {
                        bool finished = false;
                        if (detail::multipart_delimiter(s, finished) == Step::more) {
                            return Step::more;
                        }
                        break;
                    }
                    case Phase::head:
                        return _head(s, out);
                }
            }
        }

        // The head of a part: its fields up to the empty line, within the
        // limit of its bytes, through the HTTP field parser (no obs-fold)
        static Step _head(detail::MultipartState& s, optional<part>& out) noexcept {
            std::string_view w = s.window();
            size_t end = 0;
            if (!w.empty() && w[0] == '\n') {
                end = 1;
            } else if (w.size() >= 2 && w[0] == '\r' && w[1] == '\n') {
                end = 2;
            } else {
                end = detail::find_head_end(w.data(), std::min(w.size(), s.max_header_bytes + 1));
            }
            if (end == 0) {
                if (w.size() > s.max_header_bytes) {
                    return detail::multipart_fail(s, net::detail::net_error(errc::header_too_large, "multipart"));
                }
                if (s.eof) {
                    return detail::multipart_fail(s, io::error(io::errc::unexpected_eof, "multipart"));
                }
                return Step::more;
            }
            if (end > s.max_header_bytes) {
                return detail::multipart_fail(s, net::detail::net_error(errc::header_too_large, "multipart"));
            }
            if (++s.parts > s.max_parts) {
                return detail::multipart_fail(s, detail::multipart_error(errc::too_many_parts));
            }
            part p;
            string head(w.substr(0, end));
            size_t fields_end = 0;
            if (detail::parse_fields(head, 0, p.headers, false, fields_end)) {
                return detail::multipart_fail(s, detail::multipart_error(errc::malformed_multipart));
            }
            s.at += end;
            if (auto cd = detail::HeadersAccess::find(p.headers, "content-disposition")) {
                detail::parse_disposition(*cd, p.name, p.filename);
            }
            if (auto ct = detail::HeadersAccess::find(p.headers, "content-type")) {
                p.content_type = string(*ct);
            }
            s.phase = Phase::content;
            out.emplace(std::move(p));
            return Step::done;
        }

        static Step _step_read(detail::MultipartState& s, const slice<byte>& buffer, size_t& got) noexcept {
            got = 0;
            if (s.phase != Phase::content || buffer.empty()) {
                return Step::done;
            }
            return detail::multipart_read(s, buffer, got);
        }

        static expected<optional<part>, io::error> _result(const detail::MultipartState& s, optional<part> p) noexcept {
            if (s.phase == Phase::failed) {
                return unexpected(*s.error);
            }
            return p;
        }

        // More bytes into the window: the source's read; its end noted.
        // A step keeps far less than the window (a delimiter cut at its
        // end, a head within its limit), so there is always room
        static optional<io::error> _fill(detail::MultipartState& s) {
            detail::multipart_compact(s);
            auto n = s.source.read(slice<byte>(s.block, s.block->data() + s.end, detail::MultipartWindow - s.end));
            return _filled(s, n);
        }

        static async::task<optional<io::error>> _async_fill(tracked_ptr<detail::MultipartState> s) noexcept {
            detail::multipart_compact(*s);
            auto n = co_await s->source.async_read(slice<byte>(s->block, s->block->data() + s->end, detail::MultipartWindow - s->end));
            co_return _filled(*s, n);
        }

        static optional<io::error> _filled(detail::MultipartState& s, const expected<size_t, io::error>& n) noexcept {
            if (!n) {
                (void)detail::multipart_fail(s, n.error());
                return n.error();
            }
            if (*n == 0) {
                s.eof = true;
            }
            s.end += *n;
            return nullopt;
        }

        tracked_ptr<detail::MultipartState> _s;
    };

    namespace detail {
        // Phase content, read past to the delimiter: what is safe dropped
        inline MultipartStep multipart_skip(MultipartState& s) noexcept {
            for (;;) {
                std::string_view w = s.window();
                size_t safe = 0, line = 0;
                bool undecided = false;
                int kind = 0;
                size_t p = find_delimiter(s, w, safe, undecided, line, kind);
                if (p != std::string_view::npos) {
                    s.at += p;
                    s.match = s.delimiter.size();
                    s.phase = MultipartState::Phase::delimiter;
                    return MultipartStep::done;
                }
                s.at += safe;
                if (s.eof) {
                    return multipart_fail(s, io::error(io::errc::unexpected_eof, "multipart"));
                }
                return MultipartStep::more;
            }
        }
    }
}
