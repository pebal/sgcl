//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client_connection.h"
#include "stream.h"
#include "../parser.h"
#include "../wire.h"
#include "../../response.h"
#include "../../../connection.h"
#include "../../../../async/channel.h"
#include "../../../../async/coroutine.h"
#include "../../../../async/event.h"
#include "../../../../async/timer.h"
#include "../../../../core/clock.h"
#include "../../../../core/duration.h"
#include "../../../../core/function.h"
#include "../../../../core/map.h"
#include "../../../../core/weak_ptr.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <system_error>

// The client's HTTP/2 (RFC 9113): one connection to an origin shared by the
// requests to it, a stream each (the client's pool holds it, client.h). The
// connection's task reads frames into the machine (client_connection.h)
// under the connection's lock; a request opens a stream (its field block
// encoded under the same lock), sends its body as DATA within the windows,
// and waits for its response's head; the response's body is a Body over
// the stream (h2::StreamState), its window given back as it is read. One
// task writes: whatever the machine has to send is taken under the lock
// and written by it, in order. A request's deadlines (the whole exchange,
// the head) are timers on its stream: past one, the stream is reset
// (CANCEL) and its reader gets ETIMEDOUT; the connection lives.
namespace sgcl::net::http::detail::h2 {
    inline int64_t client_clock_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(sgcl::clock::now().time_since_epoch()).count();
    }

    inline io::error timed_out_error(const char* op) {
        return io::error(error_code(ETIMEDOUT, std::system_category()), op, "HTTP/2 stream");
    }

    // A request's stream: what the connection's events leave for the
    // request's task (under the connection's lock, then read by the task
    // after `headed` signals)
    struct ClientStream final : StreamState {
        enum class Fate : uint8_t {
            open,
            headed,        // the final head came
            refused,       // REFUSED_STREAM: never processed, retried whatever the method (§8.7)
            unprocessed,   // above the last of the server's GOAWAY: never processed (§6.8)
            reset,         // reset by the server or by the machine
            lost,          // the connection ended before the head
            too_large,     // the head past max_header_list_size
            malformed,     // no :status, a pseudo-field unknown, a bad length
            timed_out
        };

        async::channel<void> headed;               // the fate settled (a signal, one held)
        tracked_ptr<ResponseImpl> head;            // the final response, once headed
        Fate fate = Fate::open;
        bool head_request = false;                 // HEAD: no body whatever the length says
        bool sent_all = false;                     // END_STREAM sent
        bool remote_ended = false;                 // END_STREAM received
        std::atomic<bool> expired = {false};       // a deadline passed (the timer's thread)
        tracked_ptr<async::detail::Timer> deadline;
        tracked_ptr<async::detail::Timer> head_deadline;

        explicit ClientStream(tracked_ptr<StreamOwner> owner)
        : StreamState(0, std::move(owner)), headed(1) {
        }

        void cancel_timers() {
            for (auto* t : {&deadline, &head_deadline}) {
                if (auto& x = *t) {
                    x->cancelled.store(true, std::memory_order_release);
                    async::detail::timer_cancelled(*x);
                    x = nullptr;
                }
            }
        }
    };

    // The connection's settings the client gives
    struct TransportSettings {
        ClientSettings machine;
        duration idle_timeout;      // a connection without streams this long: GOAWAY and closed
        uint64_t max_body_bytes = 0;
    };

    class ClientH2 final : public StreamOwner {
    public:
        ClientH2(net::connection c, const TransportSettings& s)
        : _c(std::move(c)), _idle_timeout(s.idle_timeout), _m(*this, s.machine), _wake(1) {
            _last_active = client_clock_ns();
        }

        // Called once, when the connection leaves service (its pool forgets it)
        void set_on_closed(function<void()> f) {
            _on_closed = std::move(f);
        }

        // --- the pool's side ------------------------------------------------------

        // A place for one request (a stream it will open); false when the
        // connection takes no more (going away, closed, its streams full)
        bool reserve() {
            std::lock_guard<std::mutex> g(_lock);
            if (_closed || !_m.can_open() || _m.open_streams() + _reserved >= _m.stream_limit()) {
                return false;
            }
            ++_reserved;
            return true;
        }

        void unreserve() {
            std::lock_guard<std::mutex> g(_lock);
            if (_reserved) {
                --_reserved;
            }
        }

        // The server's limit of streams, once its SETTINGS came (0 before):
        // a second connection to the origin starts from it, not from 100
        uint32_t learned_limit() {
            std::lock_guard<std::mutex> g(_lock);
            return _m.peer_settings_received() ? _m.stream_limit() : 0;
        }

        // Whether the connection may take requests at all (not going away)
        bool usable() {
            std::lock_guard<std::mutex> g(_lock);
            return !_closed && !_m.goaway_received() && !_m.failed();
        }

        // The request's stream opened with its field block, the reservation
        // used: false when it could not open (the connection went away or
        // its limit fell meanwhile: the caller goes to another)
        bool open(const tracked_ptr<ClientStream>& st, const FieldBlock& block, bool end_stream) {
            uint32_t id = 0;
            {
                std::lock_guard<std::mutex> g(_lock);
                if (_reserved) {
                    --_reserved;
                }
                // encoded only when it will go: the encoder's table is the
                // server's decoder's (RFC 7541 §2.2)
                if (!_closed && _m.can_open()) {
                    _block.clear();
                    _m.encoder().begin_block(_block);
                    block.encode(_m.encoder(), _block);
                    id = _m.open_stream(reinterpret_cast<const uint8_t*>(_block.data()), _block.size(), end_stream);
                }
                if (id) {
                    st->id = id;
                    st->sent_all = end_stream;
                    _streams.insert_or_assign(id, st);
                    _last_active = client_clock_ns();
                }
            }
            _kick();
            return id != 0;
        }

        // What became of a request's stream (open while nothing yet)
        ClientStream::Fate fate_of(const tracked_ptr<ClientStream>& st) {
            std::lock_guard<std::mutex> g(_lock);
            return st->fate;
        }

        // A request's deadlines as timers on its stream: past one the stream
        // is reset and its reader and the request's task learn ETIMEDOUT.
        // The timers hold the stream weakly (as a descriptor's deadline
        // does): a cancelled one waits in the timers' heap until it is
        // swept, and one that held the stream would keep it, and through
        // it the connection, alive until then
        void arm(const tracked_ptr<ClientStream>& st, time_point deadline, time_point head_deadline) {
            weak_ptr<void> target = tracked_ptr<void>(st);
            if (deadline != time_point()) {
                st->deadline = async::detail::add_weak_timer(deadline, target, &ClientH2::_expire);
            }
            if (head_deadline != time_point()) {
                st->head_deadline = async::detail::add_weak_timer(head_deadline, target, &ClientH2::_expire_head);
            }
        }

        // --- the machine's events (under the lock) ------------------------------

        ErrorCode on_response(uint32_t id, Block&& b, bool end_stream, bool informational) {
            if (informational) {
                return ErrorCode::no_error;   // 100 Continue, 103 Early Hints: the final one follows
            }
            auto st = _find(id);
            if (!st) {
                return ErrorCode::cancel;
            }
            if (b.truncated) {
                st->fate = ClientStream::Fate::too_large;
                return ErrorCode::cancel;
            }
            // §8.3.2: :status alone among the pseudo-fields, first; the
            // rest lower case, none of HTTP/1.1's connection fields
            int status = 0;
            size_t regulars = 0;
            bool bad = false;
            bool regular = false;
            for (auto& f : HeadersAccess::fields(b.fields)) {
                auto n = f.first.view();
                if (n.empty()) {
                    bad = true;
                    break;
                }
                if (n[0] == ':') {
                    if (regular || n != ":status" || status) {
                        bad = true;
                        break;
                    }
                    auto v = f.second.view();
                    if (v.size() != 3 || v[0] < '2' || v[0] > '9' || v[1] < '0' || v[1] > '9' || v[2] < '0' || v[2] > '9') {
                        bad = true;
                        break;
                    }
                    status = (v[0] - '0') * 100 + (v[1] - '0') * 10 + (v[2] - '0');
                    continue;
                }
                regular = true;
                ++regulars;
                for (char c : n) {
                    if (c >= 'A' && c <= 'Z') {
                        bad = true;
                    }
                }
                if (n == "connection" || n == "keep-alive" || n == "proxy-connection" || n == "transfer-encoding" || n == "upgrade") {
                    bad = true;
                }
            }
            tracked_ptr<ResponseImpl> impl;
            if (!bad && status) {
                impl = make_tracked<ResponseImpl>();
                impl->status = status;
                impl->h2 = true;
                impl->head = b.bytes;
                auto& out = HeadersAccess::fields(impl->fields);
                out.reserve(regulars);
                for (auto& f : HeadersAccess::fields(b.fields)) {
                    if (f.first.view()[0] != ':') {
                        out.push_back(f);
                    }
                }
                if (HeadersAccess::find(impl->fields, "content-length")) {
                    optional<uint64_t> n;
                    if (!content_length(impl->fields, n)) {
                        bad = true;
                    } else {
                        impl->content_length = n;
                    }
                }
            }
            if (bad || !status) {
                st->fate = ClientStream::Fate::malformed;
                return ErrorCode::protocol_error;
            }
            impl->body = make_tracked<Body>(tracked_ptr<StreamState>(st), 0, true);
            st->head = impl;
            st->fate = ClientStream::Fate::headed;
            if (end_stream) {
                _remote_end(st);
            }
            st->headed.try_send();
            return ErrorCode::no_error;
        }

        void on_trailers(uint32_t id, Block&& b) {
            if (auto st = _find(id)) {
                http::headers t;
                if (!b.truncated) {
                    for (auto& f : HeadersAccess::fields(b.fields)) {
                        HeadersAccess::add(t, f.first, f.second);
                    }
                }
                st->end_with(std::move(t));
                st->remote_ended = true;
                _forget_if_done(st);
            }
        }

        void on_data(uint32_t id, const uint8_t* p, size_t n, bool end_stream) {
            if (auto st = _find(id)) {
                if (st->head_request) {
                    // HEAD: a body's bytes are not the response's (§8.1), given back
                    _m_consumed_later += n;
                    n = 0;
                }
                st->add(p, n, end_stream);
                if (end_stream) {
                    st->remote_ended = true;
                    _forget_if_done(st);
                }
            }
        }

        void on_reset(uint32_t id, ErrorCode code) {
            if (auto st = _find(id)) {
                _streams.erase(id);
                if (code == ErrorCode::no_error && st->remote_ended) {
                    // the whole response came: the server needs no more
                    // of the request (§8.1); the body stays readable
                    st->window_opened();
                    return;
                }
                if (st->fate == ClientStream::Fate::open) {
                    st->fate = code == ErrorCode::refused_stream ? ClientStream::Fate::refused : ClientStream::Fate::reset;
                }
                if (st->expired.load()) {
                    st->fate = ClientStream::Fate::timed_out;
                    st->reset_by(code, timed_out_error("read"));
                } else {
                    st->reset_by(code);
                }
                st->headed.try_send();
            }
        }

        void on_unprocessed(uint32_t id) {
            if (auto st = _find(id)) {
                _streams.erase(id);
                st->fate = ClientStream::Fate::unprocessed;
                st->reset_by(ErrorCode::refused_stream);
                st->headed.try_send();
            }
        }

        void on_window(uint32_t id) {
            if (id == 0) {
                for (auto& [k, st] : _streams) {
                    st->window_opened();
                }
            } else if (auto st = _find(id)) {
                st->window_opened();
            }
        }

        void on_goaway(uint32_t, ErrorCode) {
            _going_away = true;
        }

        void on_ping_ack(const uint8_t*) {
        }

        // --- StreamOwner ---------------------------------------------------------

        expected<void, io::error> send_headers(uint32_t id, const FieldBlock& block, bool end_stream) override {
            bool sent = false;
            {
                std::lock_guard<std::mutex> g(_lock);
                if (!_m.failed() && _m.sendable(id)) {
                    _block.clear();
                    _m.encoder().begin_block(_block);
                    block.encode(_m.encoder(), _block);
                    sent = _m.send_headers(id, reinterpret_cast<const uint8_t*>(_block.data()), _block.size(), end_stream);
                    if (sent && end_stream) {
                        _local_end(id);
                    }
                }
            }
            _kick();
            if (!sent) {
                return unexpected(stream_reset_error("write", ErrorCode::cancel));
            }
            return {};
        }

        async::task<expected<void, io::error>> send_data(uint32_t id, slice<const byte> data, bool end_stream) override {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
            size_t at = 0;
            for (;;) {
                tracked_ptr<ClientStream> st;
                bool done = false;
                bool dead = false;
                {
                    std::lock_guard<std::mutex> g(_lock);
                    st = _find(id);
                    if (!st || st->was_reset() || _m.failed() || !_m.sendable(id)) {
                        dead = true;
                    } else {
                        at += _m.send_data(id, p + at, data.size() - at, end_stream);
                        done = at == data.size() && (!end_stream || !_m.sendable(id));
                        if (done && end_stream) {
                            _local_end(id);
                        }
                        _last_active = client_clock_ns();
                    }
                }
                _kick();
                if (dead) {
                    co_return unexpected(st && st->expired.load() ? timed_out_error("write") : stream_reset_error("write", ErrorCode::cancel));
                }
                if (done) {
                    co_return expected<void, io::error>();
                }
                co_await st->writable.receive();
            }
        }

        void consumed(uint32_t id, size_t n) override {
            {
                std::lock_guard<std::mutex> g(_lock);
                _m.consumed(id, n);   // a stream gone: its bytes still go back to the connection's window
            }
            _kick();
        }

        void reset(uint32_t id, ErrorCode code) override {
            {
                std::lock_guard<std::mutex> g(_lock);
                if (auto st = _find(id)) {
                    _streams.erase(id);
                    st->reset_by(code, st->expired.load() ? optional<io::error>(timed_out_error("read")) : nullopt);
                    if (st->fate == ClientStream::Fate::open) {
                        st->fate = st->expired.load() ? ClientStream::Fate::timed_out : ClientStream::Fate::reset;
                    }
                    st->headed.try_send();
                }
                _m.reset(id, code);
            }
            _kick();
        }

        // --- the connection's tasks ------------------------------------------------

        // The machine started, the reader, the writer and the clock begun
        static void start(const tracked_ptr<ClientH2>& h) {
            {
                std::lock_guard<std::mutex> g(h->_lock);
                h->_m.start(client_clock_ns());
            }
            h->_kick();
            async::go(_pump(h));
            async::go(_ticker(weak_ptr<ClientH2>(h), _tick_period(h->_idle_timeout)));
            async::go(_read(h));
        }

        // The connection given up (the client's close_idle, the pool's end):
        // GOAWAY when no stream is open, and closed
        void close_if_idle() {
            bool close = false;
            {
                std::lock_guard<std::mutex> g(_lock);
                if (!_closed && _streams.empty() && _reserved == 0) {
                    _m.goaway(ErrorCode::no_error);
                    _closed = true;   // before the pump sees the GOAWAY: it closes the writing side after it
                    close = true;
                }
            }
            if (close) {
                _kick();
                _leave(false);
            }
        }

        size_t streams() {
            std::lock_guard<std::mutex> g(_lock);
            return _streams.size();
        }

    private:
        net::connection _c;
        duration _idle_timeout;
        std::mutex _lock;
        ClientConnection<ClientH2> _m;
        map<uint32_t, tracked_ptr<ClientStream>> _streams;   // the streams the machine has, as it has them
        size_t _reserved = 0;
        std::string _block;                  // a field block being encoded (under the lock)
        std::string _send;                   // the writer's copy of the output
        async::channel<void> _wake;          // something to send (a signal, one held)
        async::event _pump_done;
        function<void()> _on_closed;
        int64_t _last_active = 0;
        size_t _m_consumed_later = 0;        // HEAD's DATA, given back after the feed
        bool _closed = false;
        bool _left = false;
        bool _going_away = false;
        bool _write_closed = false;          // the pump's

        tracked_ptr<ClientStream> _find(uint32_t id) {
            auto it = _streams.find(id);
            return it == _streams.end() ? tracked_ptr<ClientStream>() : it->second;
        }

        void _kick() {
            _wake.try_send();
        }

        // Both sides ended: the machine has let the stream go, so does the
        // table (the Body keeps the state for its reader)
        void _forget_if_done(const tracked_ptr<ClientStream>& st) {
            if (st->remote_ended && st->sent_all) {
                _streams.erase(st->id);
            }
        }

        void _remote_end(const tracked_ptr<ClientStream>& st) {
            st->add(nullptr, 0, true);
            st->remote_ended = true;
            _forget_if_done(st);
        }

        void _local_end(uint32_t id) {
            if (auto st = _find(id)) {
                st->sent_all = true;
                _forget_if_done(st);
            }
        }

        // The timer's thread: the whole exchange's deadline passed
        static void _expire(void* p) {
            auto st = static_cast<ClientStream*>(p);
            st->expired.store(true);
            st->owner->reset(st->id, ErrorCode::cancel);
        }

        // The head's deadline passed: only while the head has not come
        static void _expire_head(void* p) {
            auto st = static_cast<ClientStream*>(p);
            auto* h = static_cast<ClientH2*>(st->owner.get());
            {
                std::lock_guard<std::mutex> g(h->_lock);
                if (st->fate != ClientStream::Fate::open) {
                    return;
                }
            }
            st->expired.store(true);
            st->owner->reset(st->id, ErrorCode::cancel);
        }

        // Out of service: the pool forgets it; `lost`: every stream still
        // open ends with the connection's loss
        void _leave(bool lost) {
            function<void()> f;
            {
                std::lock_guard<std::mutex> g(_lock);
                _closed = true;
                if (lost) {
                    for (auto& [k, st] : _streams) {
                        if (st->fate == ClientStream::Fate::open) {
                            st->fate = ClientStream::Fate::lost;
                        }
                        st->reset_by(ErrorCode::cancel, io::error(std::make_error_code(std::errc::connection_reset), "read", "HTTP/2 connection"));
                        st->headed.try_send();
                    }
                    _streams.clear();
                }
                if (!_left) {
                    _left = true;
                    f = std::move(_on_closed);
                    _on_closed = {};
                }
            }
            if (f) {
                f();
            }
        }

        // The reader: frames fed to the machine until the end
        static async::task<> _read(tracked_ptr<ClientH2> h) {
            tracked_ptr wire = make_tracked<Wire>(h->_c);
            wire->reserve(size_t(h->_m.limits().max_frame_size) + FrameHeaderSize);
            for (;;) {
                auto r = co_await wire->fill();
                if (!r || *r == 0) {
                    break;
                }
                expected<size_t, Error> fed = size_t(0);
                size_t later = 0;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    auto v = wire->view();
                    fed = h->_m.feed(reinterpret_cast<const uint8_t*>(v.data()), v.size(), client_clock_ns());
                    h->_last_active = client_clock_ns();
                    later = h->_m_consumed_later;
                    h->_m_consumed_later = 0;
                }
                if (fed) {
                    wire->consume(*fed);
                }
                if (later) {
                    std::lock_guard<std::mutex> g(h->_lock);
                    h->_m.consumed(0, later);
                }
                bool going = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    going = h->_going_away && !h->_left;
                }
                if (going) {
                    // no new stream here: the pool forgets it, its streams go on
                    function<void()> f;
                    {
                        std::lock_guard<std::mutex> g(h->_lock);
                        h->_left = true;
                        f = std::move(h->_on_closed);
                        h->_on_closed = {};
                    }
                    if (f) {
                        f();
                    }
                }
                h->_kick();
                if (!fed) {
                    break;
                }
                bool finished = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    finished = h->_going_away && h->_streams.empty();
                }
                if (finished) {
                    break;
                }
            }
            h->_leave(true);
            h->_wake.close();
            co_await h->_pump_done;
            (void)h->_c.close();
        }

        // The one writer: the machine's output, taken under the lock, sent in order
        static async::task<> _pump(tracked_ptr<ClientH2> h) {
            bool open = true;
            while (open) {
                open = co_await h->_wake.receive();
                for (;;) {
                    bool closing = false;
                    {
                        std::lock_guard<std::mutex> g(h->_lock);
                        auto o = h->_m.output();
                        h->_send.assign(reinterpret_cast<const char*>(o.data()), o.size());
                        h->_m.written(o.size());
                        closing = h->_closed && h->_streams.empty();
                    }
                    if (h->_send.empty()) {
                        if (closing && !h->_write_closed) {
                            h->_write_closed = true;
                            (void)h->_c.close_write();   // after our GOAWAY: the server closes, the reader ends
                        }
                        break;
                    }
                    auto r = co_await h->_c.async_write(slice<const byte>(reinterpret_cast<const byte*>(h->_send.data()), h->_send.size()));
                    if (!r) {
                        (void)h->_c.close();   // the reader wakes and ends the streams
                        open = false;
                        break;
                    }
                }
            }
            h->_pump_done.set();
        }

        // The time for the machine (SETTINGS unanswered, streams starved of
        // window) and the idle connection closed: once a second, or a
        // quarter of idle_timeout when that is shorter (10 ms at least).
        //
        // The ticker holds the connection weakly, as the server's does
        // (serve.h): asleep, it keeps nothing of it alive, and a connection
        // that has ended is garbage at once, not a tick later (with its
        // TLS state, its buffers, the pool it points to)
        static int64_t _tick_period(duration idle_timeout) {
            int64_t period = 1'000'000'000;
            if (idle_timeout > duration::zero()) {
                period = std::min(period, std::max<int64_t>(10'000'000, idle_timeout.nanoseconds() / 4));
            }
            return period;
        }

        static async::task<> _ticker(weak_ptr<ClientH2> weak, int64_t period) {
            for (;;) {
                co_await async::after(std::chrono::nanoseconds(period));
                tracked_ptr<ClientH2> h = weak.lock();
                if (!h || h->_pump_done.is_set()) {
                    break;
                }
                bool failed = false;
                bool idle = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    if (h->_closed) {
                        break;
                    }
                    failed = !h->_m.tick(client_clock_ns());
                    idle = h->_idle_timeout > duration::zero() && h->_streams.empty() && h->_reserved == 0
                        && client_clock_ns() - h->_last_active > std::chrono::nanoseconds(h->_idle_timeout).count();
                }
                if (idle) {
                    h->close_if_idle();
                    break;
                }
                h->_kick();
                if (failed) {
                    break;   // GOAWAY is in the output; the server's close ends the reader
                }
            }
        }
    };
}
