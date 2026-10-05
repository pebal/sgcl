//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "response.h"
#include "response_writer.h"
#include "status.h"
#include "../../async/coroutine.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timeout.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"

#include <charconv>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

// Server-Sent Events, the event stream of the WHATWG HTML Standard (§9.2):
// a server's stream of events over a response (event_stream: text/event-
// stream, each event flushed as it is sent, over HTTP/1.1 and HTTP/2), the
// stream read on a client (event_reader, over any body), and a client that
// reconnects as a browser's EventSource does (event_source: the stream's
// retry, Last-Event-ID).
namespace sgcl::net::http {
    // An event: its type (the "event" field; "message" when a stream gives
    // none), its data (lines joined by LF), its id (the stream's last event
    // id when it was dispatched) and a retry, the reconnection time a
    // server sets
    struct event {
        string type;
        string data;
        string id;
        optional<duration> retry;
    };

    namespace detail {
        using namespace sgcl::detail;

        struct ParsedEvent {
            std::string type;
            std::string data;
            std::string id;
            optional<int64_t> retry;
        };

        // The event stream's interpretation (WHATWG HTML §9.2.6): bytes in,
        // events out, as the bytes come. Lines end in CRLF, LF or CR alone
        // (a CR at a piece's end waits to see whether an LF follows); a
        // byte order mark at the stream's start is dropped; a line that
        // starts with ':' is a comment; a field without a colon has an
        // empty value; one space after the colon is dropped. The fields:
        // event, data (appended with an LF), id (unless it holds NUL; kept
        // across events, as the last event id), retry (ASCII digits alone).
        // An empty line dispatches the event, unless its data is empty. An
        // event or a line past `limit` bytes is too large
        class EventParser {
        public:
            SGCL_INLINE_HOT explicit EventParser(size_t limit) noexcept
            : _limit(limit) {
            }

            // false when an event or a line passed the limit
            bool feed(const char* p, size_t n, std::deque<ParsedEvent>& out) noexcept {
                size_t i = 0;
                if (!_started) {
                    // the BOM, which may come cut over pieces
                    while (i < n && _bom < 3) {
                        static constexpr char bom[3] = {char(0xEF), char(0xBB), char(0xBF)};
                        if (p[i] != bom[_bom]) {
                            break;
                        }
                        ++_bom;
                        ++i;
                    }
                    if (_bom == 3 || (i < n)) {
                        if (_bom != 3 && _bom > 0) {
                            // a partial BOM: the bytes were the stream's own
                            _line.assign("\xEF\xBB\xBF", _bom);
                        }
                        _started = true;
                    } else {
                        return true;   // all of it a prefix of the BOM: wait
                    }
                }
                while (i < n) {
                    if (_after_cr) {
                        _after_cr = false;
                        if (p[i] == '\n') {
                            ++i;
                            continue;
                        }
                    }
                    // the bytes up to the next line end, at once
                    size_t j = i;
                    while (j < n && p[j] != '\n' && p[j] != '\r') {
                        ++j;
                    }
                    if (_line.size() + (j - i) > _limit) {
                        return false;
                    }
                    _line.append(p + i, j - i);
                    if (j == n) {
                        break;
                    }
                    _after_cr = p[j] == '\r';
                    i = j + 1;
                    if (!_process(out)) {
                        return false;
                    }
                    _line.clear();
                }
                return true;
            }

            // The stream's end: an event without its empty line is dropped
            SGCL_INLINE_HOT void finish() noexcept {
                _line.clear();
                _data.clear();
                _type.clear();
                _retry.reset();
            }

            SGCL_INLINE_HOT const std::string& last_id() const noexcept {
                return _id;
            }

            SGCL_INLINE_HOT void set_last_id(std::string_view id) noexcept {
                _id.assign(id);
            }

        private:
            bool _process(std::deque<ParsedEvent>& out) noexcept {
                std::string_view line = _line;
                if (line.empty()) {
                    // dispatch
                    if (_data.empty()) {
                        _type.clear();
                        _retry.reset();
                        return true;
                    }
                    ParsedEvent e;
                    _data.pop_back();   // the last LF
                    e.data = std::move(_data);
                    e.type = _type.empty() ? std::string("message") : std::move(_type);
                    e.id = _id;
                    e.retry = _retry;
                    out.push_back(std::move(e));
                    _data.clear();
                    _type.clear();
                    _retry.reset();
                    return true;
                }
                if (line.front() == ':') {
                    return true;   // a comment
                }
                std::string_view field = line, value;
                auto colon = line.find(':');
                if (colon != std::string_view::npos) {
                    field = line.substr(0, colon);
                    value = line.substr(colon + 1);
                    if (!value.empty() && value.front() == ' ') {
                        value.remove_prefix(1);
                    }
                }
                if (field == "data") {
                    if (_data.size() + value.size() + 1 > _limit) {
                        return false;
                    }
                    _data.append(value);
                    _data += '\n';
                } else if (field == "event") {
                    _type.assign(value);
                } else if (field == "id") {
                    if (value.find('\0') == std::string_view::npos) {
                        _id.assign(value);
                    }
                } else if (field == "retry") {
                    bool digits = !value.empty();
                    for (char c : value) {
                        digits &= c >= '0' && c <= '9';
                    }
                    if (digits) {
                        int64_t ms = 0;
                        auto r = std::from_chars(value.data(), value.data() + value.size(), ms);
                        _retry = r.ec == std::errc() ? ms : INT64_MAX / 1000000;   // past int64's milliseconds: as long as a duration can be
                    }
                }
                return true;
            }

            size_t _limit;
            bool _started = false;
            uint8_t _bom = 0;
            bool _after_cr = false;
            std::string _line;
            std::string _data;
            std::string _type;
            std::string _id;
            optional<int64_t> _retry;
        };

        // An event's text (WHATWG HTML §9.2.6 read backwards): "event:",
        // "id:", "retry:" when set, a "data:" line for each line of the
        // data (CRLF, LF and CR all ending one), the empty line; nullopt
        // for a type or an id that would end its line, or an id with NUL
        inline optional<std::string> event_text(const event& e) noexcept {
            auto one_line = [](std::string_view v) { return v.find_first_of(std::string_view("\r\n", 2)) == std::string_view::npos; };
            if (!one_line(e.type.view()) || !one_line(e.id.view()) || e.id.view().find('\0') != std::string_view::npos) {
                return nullopt;
            }
            std::string s;
            s.reserve(e.data.size() + e.type.size() + e.id.size() + 32);
            if (!e.type.empty()) {
                s += "event: ";
                s += e.type.view();
                s += '\n';
            }
            if (!e.id.empty()) {
                s += "id: ";
                s += e.id.view();
                s += '\n';
            }
            if (e.retry) {
                int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds(*e.retry)).count();
                s += "retry: ";
                s += std::to_string(ms < 0 ? 0 : ms);
                s += '\n';
            }
            std::string_view d = e.data.view();
            for (;;) {
                size_t end = d.find_first_of(std::string_view("\r\n", 2));
                s += "data: ";
                s += d.substr(0, end);
                s += '\n';
                if (end == std::string_view::npos) {
                    break;
                }
                d.remove_prefix(end + (d[end] == '\r' && end + 1 < d.size() && d[end + 1] == '\n' ? 2 : 1));
            }
            s += '\n';
            return s;
        }

        // A comment's lines: ": text" each
        inline std::string comment_text(std::string_view text) noexcept {
            std::string s;
            for (;;) {
                size_t end = text.find_first_of(std::string_view("\r\n", 2));
                s += ':';
                if (!text.empty()) {
                    s += ' ';
                }
                s += text.substr(0, end);
                s += '\n';
                if (end == std::string_view::npos) {
                    break;
                }
                text.remove_prefix(end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1));
            }
            s += '\n';
            return s;
        }

        inline event to_event(ParsedEvent&& p) noexcept {
            event e;
            e.type = string(std::string_view(p.type));
            e.data = string(std::string_view(p.data));
            e.id = string(std::string_view(p.id));
            if (p.retry) {
                e.retry = duration(std::chrono::milliseconds(*p.retry));
            }
            return e;
        }

        struct EventSourceAccess;

        struct EventReaderState {
            io::reader body;
            EventParser parser;
            std::deque<ParsedEvent> ready;
            tracked_ptr<array<byte, 16384>> block = make_tracked<array<byte, 16384>>();
            bool ended = false;
            optional<io::error> failure;
            optional<duration> retry;   // the last retry the stream set

            SGCL_INLINE_HOT EventReaderState(const io::reader& b, size_t limit) noexcept
            : body(b)
            , parser(limit) {
            }

            // the bytes read handed to the parser: false at the end or a failure
            bool took(const expected<size_t, io::error>& n) noexcept {
                if (!n) {
                    failure = n.error();
                    return false;
                }
                if (*n == 0) {
                    parser.finish();
                    ended = true;
                    return false;
                }
                if (!parser.feed(reinterpret_cast<const char*>(block->data()), *n, ready)) {
                    failure = net::detail::net_error(net::errc::body_too_large, "events", "an event");
                    return false;
                }
                return true;
            }

            SGCL_INLINE_HOT optional<event> pop() noexcept {
                if (ready.empty()) {
                    return nullopt;
                }
                ParsedEvent p = std::move(ready.front());
                ready.pop_front();
                if (p.retry) {
                    retry = duration(std::chrono::milliseconds(*p.retry));
                }
                return to_event(std::move(p));
            }
        };
    }

    // A server's event stream over a response (WHATWG HTML §9.2): the
    // fields of text/event-stream set on the writer, each event written
    // and flushed as it is sent, over HTTP/1.1 (chunked) and HTTP/2. A
    // handle of one word over the writer: copies are the same stream.
    // Sent from a handler that is a task, which keeps the response open
    // for as long as it sends
    class event_stream {
    public:
        // Content-Type: text/event-stream, Cache-Control: no-cache,
        // X-Accel-Buffering: no (a proxy of nginx's kind holds nothing
        // back); nothing sent yet
        SGCL_INLINE_HOT explicit event_stream(const response_writer& w) noexcept
        : _w(w) {
            _w.set_header("Content-Type", "text/event-stream");
            _w.set_header("Cache-Control", "no-cache");
            _w.set_header("X-Accel-Buffering", "no");
        }

        // An event, written and flushed; EINVAL for a type or an id with a
        // line break (or an id with NUL), nothing written
        // `send(...)` on this thread, `co_await async_send(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> send(const event& e) const {
            return async_send(e).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send(const event& e) const noexcept {
            auto text = detail::event_text(e);
            if (!text) {
                return _co_error(io::error(std::make_error_code(std::errc::invalid_argument), "events", "an event's type or id"));
            }
            return _co_write(_w, std::move(*text));
        }

        // An event of data alone (the type "message")
        SGCL_INLINE_HOT expected<void, io::error> send(const string& data) const {
            return async_send(data).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send(const string& data) const noexcept {
            event e;
            e.data = data;
            return async_send(e);
        }

        // A comment, which a reader drops: a heartbeat that keeps the
        // connection and the proxies on its way from closing it idle
        // `comment(...)` on this thread, `co_await async_comment(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> comment(const string& text) const {
            return async_comment(text).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_comment(const string& text) const noexcept {
            return _co_write(_w, detail::comment_text(text.view()));
        }

        // The head sent now, before any event: a client sees the stream
        // open
        // `flush(...)` on this thread, `co_await async_flush(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> flush() const {
            return _w.flush();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_flush() const noexcept {
            return _w.async_flush();
        }

    private:
        // The text written and flushed in this one frame: the flush begun
        // without one of its own (a task only for the rest of a write that
        // would wait), the text in the frame, no managed copy of it
        static async::task<expected<void, io::error>> _co_write(response_writer w, std::string text) noexcept {
            w.write(std::string_view(text));
            expected<void, io::error> now;
            if (auto rest = detail::WriterAccess::flush_start(w, now)) {
                co_return co_await *rest;
            }
            co_return now;
        }

        static async::task<expected<void, io::error>> _co_error(io::error e) noexcept {
            co_return unexpected(e);
        }

        response_writer _w;
    };

    // The events of a stream, read as they come (WHATWG HTML §9.2.6): over
    // a response's body, or any reader. next() gives the next event, nullopt
    // at the stream's end (an event cut by it is dropped). A handle of one
    // word: copies are the same reader. An event or a line past
    // max_event_bytes (1 MB by default) is net::errc::body_too_large
    class event_reader {
    public:
        static constexpr size_t default_max_event_bytes = size_t(1) << 20;

        event_reader() noexcept = default;

        SGCL_INLINE_HOT explicit event_reader(const io::reader& body, size_t max_event_bytes = default_max_event_bytes) noexcept
        : _s(make_tracked<detail::EventReaderState>(body, max_event_bytes)) {
        }

        SGCL_INLINE_HOT explicit event_reader(const response& r, size_t max_event_bytes = default_max_event_bytes) noexcept
        : event_reader(r.body(), max_event_bytes) {
        }

        // `next(...)` on this thread, `co_await async_next(...)` in a task
        expected<optional<event>, io::error> next() const {
            detail::EventReaderState& s = *_s;
            for (;;) {
                if (auto e = s.pop()) {
                    return e;
                }
                if (s.failure) {
                    return unexpected(*s.failure);
                }
                if (s.ended) {
                    return optional<event>();
                }
                (void)s.took(s.body.read(slice<byte>(s.block, s.block->data(), s.block->size())));
            }
        }

        SGCL_INLINE_HOT async::task<expected<optional<event>, io::error>> async_next() const noexcept {
            return _co_next(_s);
        }

        // The stream's last event id: the last id field, kept across events
        SGCL_INLINE_HOT string last_event_id() const noexcept {
            return string(std::string_view(_s->parser.last_id()));
        }

        // The last reconnection time the stream set (a retry field), none
        // when it set none
        SGCL_INLINE_HOT optional<duration> retry() const noexcept {
            return _s->retry;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        friend struct detail::EventSourceAccess;

        static async::task<expected<optional<event>, io::error>> _co_next(tracked_ptr<detail::EventReaderState> s) noexcept {
            for (;;) {
                if (auto e = s->pop()) {
                    co_return e;
                }
                if (s->failure) {
                    co_return unexpected(*s->failure);
                }
                if (s->ended) {
                    co_return optional<event>();
                }
                (void)s->took(co_await s->body.async_read(slice<byte>(s->block, s->block->data(), s->block->size())));
            }
        }

        tracked_ptr<detail::EventReaderState> _s;
    };

    namespace detail {
        struct EventSourceAccess {
            // A reconnection's reader starts from the last event id there was
            SGCL_INLINE_HOT static void seed(const event_reader& r, const string& id) noexcept {
                r._s->parser.set_last_id(id.view());
            }
        };

        struct EventSourceState {
            http::client client;
            string url;
            http::headers headers;
            duration retry;
            int max_reconnects = -1;
            size_t max_event_bytes = 0;
            async::stop_token stop;
            optional<event_reader> reader;
            optional<response> current;
            int reconnects = 0;
            bool connected_once = false;
            string last_id;
            optional<io::error> done;   // given up: every next gives it
        };
    }

    // A client of an event stream that reconnects, as a browser's
    // EventSource does (WHATWG HTML §9.2.2-§9.2.3): a GET with Accept:
    // text/event-stream, and when the stream ends or the connection fails
    // another one after the reconnection time (the stream's retry field, or
    // options::retry), with Last-Event-ID the last id it gave. It gives up
    // for good, as EventSource fails the connection, on a status other than
    // 200 (204 is the server's way to stop it), a Content-Type other than
    // text/event-stream, max_reconnects reconnections in a row with no
    // event, or the stop. A handle of one word: copies are the same source
    class event_source {
    public:
        struct options {
            http::headers headers;                     // the request's fields
            duration retry = 3 * second;               // the reconnection time until the stream sets one
            int max_reconnects = -1;                   // reconnections in a row with no event; below zero: no end
            size_t max_event_bytes = event_reader::default_max_event_bytes;
            async::stop_token stop;
        };

        event_source() noexcept = default;

        // The stream at url, through a client of default settings (the
        // environment's proxy), or through a client given (its proxy, TLS,
        // timeouts); nothing is sent before the first next
        SGCL_INLINE_HOT explicit event_source(const string& url) noexcept
        : event_source(http::client(), url, options()) {
        }

        SGCL_INLINE_HOT event_source(const string& url, const options& o) noexcept
        : event_source(http::client(), url, o) {
        }

        event_source(const http::client& via, const string& url, const options& o) noexcept
        : _s(make_tracked<detail::EventSourceState>()) {
            _s->client = via;
            _s->url = url;
            _s->headers = o.headers;
            _s->retry = o.retry;
            _s->max_reconnects = o.max_reconnects;
            _s->max_event_bytes = o.max_event_bytes;
            _s->stop = o.stop;
        }

        // The next event, connecting and reconnecting as needed; the error
        // it gave up with
        // `next(...)` on this thread, `co_await async_next(...)` in a task
        SGCL_INLINE_HOT expected<event, io::error> next() const {
            return async_next().wait();
        }

        SGCL_INLINE_HOT async::task<expected<event, io::error>> async_next() const noexcept {
            return _co_next(_s);
        }

        // The last event id the stream gave: Last-Event-ID of a reconnection
        SGCL_INLINE_HOT string last_event_id() const noexcept {
            return _s->last_id;
        }

        // The reconnection time now: the stream's retry, or options::retry
        SGCL_INLINE_HOT duration retry() const noexcept {
            return _s->retry;
        }

        // The connection closed, no more reconnections: next gives
        // io::errc::closed
        SGCL_INLINE_HOT void close() const noexcept {
            _s->done = io::error(io::errc::closed, "events", _s->url);
            if (_s->current) {
                _s->current->close();
            }
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        // The stream (re)opened: its status and type checked
        static async::task<optional<io::error>> _open(tracked_ptr<detail::EventSourceState> s) noexcept {
            request req("GET", s->url);
            for (auto f : s->headers) {
                req.add_header(f.first, f.second);
            }
            req.set_header("Accept", "text/event-stream");
            req.set_header("Cache-Control", "no-cache");
            if (!s->last_id.empty()) {
                req.set_header("Last-Event-ID", s->last_id);
            }
            auto r = co_await s->client.async_send(req);
            if (!r) {
                co_return r.error();   // a network failure: another try
            }
            if (r->status() != 200) {
                int st = r->status();
                r->close();
                s->done = net::detail::net_error(net::errc::http_status, "events", string::concat(s->url, " (", std::to_string(st), ')'));
                co_return s->done;
            }
            auto type = detail::media_type(r->header("Content-Type").view());
            if (type != "text/event-stream") {
                r->close();
                s->done = net::detail::net_error(net::errc::malformed_response, "events", string::concat(s->url, " (Content-Type ", r->header("Content-Type"), ')'));
                co_return s->done;
            }
            s->current = *r;
            event_reader reader(*r, s->max_event_bytes);
            if (!s->last_id.empty()) {
                detail::EventSourceAccess::seed(reader, s->last_id);
            }
            s->reader = reader;
            co_return nullopt;
        }

        static async::task<expected<event, io::error>> _co_next(tracked_ptr<detail::EventSourceState> s) noexcept {
            for (;;) {
                if (s->done) {
                    co_return unexpected(*s->done);
                }
                if (s->stop.stop_requested()) {
                    s->done = io::error(error_code(ECANCELED, std::system_category()), "events", s->url);
                    continue;
                }
                if (!s->reader) {
                    if (s->connected_once) {
                        // the reconnection time, the stop heeded
                        if (s->max_reconnects >= 0 && s->reconnects >= s->max_reconnects) {
                            s->done = io::error(io::errc::unexpected_eof, "events", string::concat(s->url, " (no more reconnections)"));
                            continue;
                        }
                        ++s->reconnects;
                        bool stopped = false;
                        if (s->stop.stop_possible()) {
                            co_await sgcl::async::select(s->stop.on_stop([&] { stopped = true; }), sgcl::async::timeout(sgcl::clock::now() + s->retry, [] {}));
                        } else {
                            co_await sgcl::async::select(sgcl::async::timeout(sgcl::clock::now() + s->retry, [] {}));
                        }
                        if (stopped) {
                            continue;
                        }
                    }
                    s->connected_once = true;
                    auto failed = co_await _open(s);
                    if (failed) {
                        continue;   // given up (done set), or another try
                    }
                }
                auto e = co_await s->reader->async_next();
                if (auto r = s->reader->retry()) {
                    s->retry = *r;
                }
                if (e && *e) {
                    s->reconnects = 0;
                    s->last_id = (*e)->id;
                    co_return std::move(**e);
                }
                // the stream ended, or failed: reconnect
                s->last_id = s->reader->last_event_id();
                if (s->current) {
                    s->current->close();
                }
                s->reader.reset();
                s->current.reset();
                if (!e && e.error().code() == net::errc::body_too_large) {
                    s->done = e.error();
                }
            }
        }

        tracked_ptr<detail::EventSourceState> _s;
    };
}
