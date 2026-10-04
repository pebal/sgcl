//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"
#include "request_check.h"
#include "stream.h"
#include "../server_state.h"
#include "../../../../async/channel.h"
#include "../../../../async/event.h"
#include "../../../../async/timer.h"
#include "../../../../async/wait_group.h"
#include "../../../../core/vector.h"
#include "../../../../core/weak_ptr.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// The server's HTTP/2 (RFC 9113): one connection, after ALPN chose "h2" or
// the client's preface came on a plain connection (h2c by prior knowledge).
// The connection's task reads frames into the machine (connection.h) under
// the connection's lock; a request that opens a stream runs the server's
// handler as in HTTP/1.1, in a task of its own, its body read from the
// stream (Body over h2::StreamState), its response written through a
// WriterImpl on the stream (HEADERS, DATA within the windows, a flush as
// DATA without END_STREAM). One task writes: whatever the machine has to
// send is taken under the lock and written by it, in order.
namespace sgcl::net::http::detail::h2 {
    SGCL_INLINE_HOT int64_t now_ns() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(sgcl::clock::now().time_since_epoch()).count();
    }

    // A stream of the server's: the request it carries
    struct ServerStream final : StreamState {
        tracked_ptr<RequestImpl> req;
        bool started = false;
        bool small_body = false;       // a body of at most SmallBody bytes declared, no Expect: its handler starts with its first bytes
        bool deferred = false;         // its turn came before them: it starts when they come
        bool body_came = false;        // DATA with bytes, or the end, came
        BodyCount count;               // the DATA against the content-length (§8.1.1)
        int64_t read_by = 0;           // the request's body whole by then (ns; 0: no limit, or it came)
        int64_t write_by = 0;          // the response whole by then (ns; 0: no limit)

        SGCL_INLINE_HOT ServerStream(uint32_t id, tracked_ptr<StreamOwner> owner) noexcept
        : StreamState(id, std::move(owner)) {
        }
    };

    // A request whose declared body is at most this many bytes, and which
    // does not ask Expect: 100-continue, has its handler started with the
    // body's first bytes (or its end), not with its HEADERS: it does not
    // start only to wait for a DATA that comes in the next record (the TLS
    // POST profile: a park and a wake of the handler per request). Go starts
    // the handler with the HEADERS; what the handler sees is the same
    inline constexpr uint64_t SmallBody = 16 * 1024;

    // The streams of a connection whose handlers run or wait, by their
    // identifiers: open addressing in one managed array (it holds the
    // streams' words), deletion by backward shift as the machine's
    // StreamTable does (no tombstones gather). 64 slots at first (1 KB),
    // twice as many past half full, never smaller while the connection
    // lives: a connection has a few streams at once, and only a queue of
    // requests waiting for a handler grows it, bounded by the machine's
    // limit against rapid reset (4 × max_concurrent_streams waiting, then
    // GOAWAY). Nothing is allocated for a stream (sgcl::map took a node).
    // A range of its slots in use: `for (auto& [id, st] : streams)`
    inline std::atomic<size_t> stream_table_high{0};   // the largest table a connection grew to (the tests look)

    class ServerStreams {
    public:
        struct Slot {
            uint32_t id = 0;   // 0: empty
            tracked_ptr<ServerStream> st;
        };

        class iterator {
        public:
            SGCL_INLINE_HOT iterator(Slot* p, Slot* end) noexcept
            : _p(p), _end(end) {
                _skip();
            }

            SGCL_INLINE_HOT Slot& operator*() const noexcept {
                return *_p;
            }

            SGCL_INLINE_HOT iterator& operator++() noexcept {
                ++_p;
                _skip();
                return *this;
            }

            SGCL_INLINE_HOT bool operator!=(const iterator& o) const noexcept {
                return _p != o._p;
            }

        private:
            Slot* _p;
            Slot* _end;

            void _skip() noexcept {
                while (_p != _end && _p->id == 0) {
                    ++_p;
                }
            }
        };

        static constexpr size_t Initial = 64;

        SGCL_INLINE_HOT ServerStreams() noexcept
        : _slots(Initial) {
        }

        tracked_ptr<ServerStream> find(uint32_t id) const noexcept {
            const Slot* s = _slots.data();
            for (size_t i = _home(id);; i = (i + 1) & _mask()) {
                if (s[i].id == id) {
                    return s[i].st;
                }
                if (s[i].id == 0) {
                    return tracked_ptr<ServerStream>();
                }
            }
        }

        SGCL_INLINE_HOT void insert_or_assign(uint32_t id, tracked_ptr<ServerStream> st) noexcept {
            if (Slot* s = _at(id)) {
                s->st = std::move(st);
                return;
            }
            if (2 * (_size + 1) > _slots.size()) {
                _grow();
            }
            _place(id, std::move(st));
            ++_size;
        }

        void erase(uint32_t id) noexcept {
            Slot* s = _at(id);
            if (!s) {
                return;
            }
            Slot* d = _slots.data();
            size_t i = size_t(s - d);
            d[i] = Slot();
            --_size;
            for (size_t j = (i + 1) & _mask(); d[j].id != 0; j = (j + 1) & _mask()) {
                const size_t h = _home(d[j].id);
                // j's entry may move to i when its home is not in (i, j]
                const bool stays = i <= j ? (i < h && h <= j) : (i < h || h <= j);
                if (!stays) {
                    d[i] = std::move(d[j]);
                    d[j] = Slot();
                    i = j;
                }
            }
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _size == 0;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT size_t capacity() const noexcept {
            return _slots.size();
        }

        SGCL_INLINE_HOT iterator begin() noexcept {
            return iterator(_slots.data(), _slots.data() + _slots.size());
        }

        SGCL_INLINE_HOT iterator end() noexcept {
            return iterator(_slots.data() + _slots.size(), _slots.data() + _slots.size());
        }

    private:
        vector<Slot> _slots;
        size_t _size = 0;

        SGCL_INLINE_HOT size_t _mask() const noexcept {
            return _slots.size() - 1;
        }

        SGCL_INLINE_HOT size_t _home(uint32_t id) const noexcept {
            return size_t(uint64_t(id) * 0x9E3779B97F4A7C15ull >> 32) & _mask();
        }

        Slot* _at(uint32_t id) noexcept {
            Slot* s = _slots.data();
            for (size_t i = _home(id);; i = (i + 1) & _mask()) {
                if (s[i].id == id) {
                    return &s[i];
                }
                if (s[i].id == 0) {
                    return nullptr;
                }
            }
        }

        void _place(uint32_t id, tracked_ptr<ServerStream> st) noexcept {
            Slot* s = _slots.data();
            size_t i = _home(id);
            while (s[i].id != 0) {
                i = (i + 1) & _mask();
            }
            s[i].id = id;
            s[i].st = std::move(st);
        }

        void _grow() noexcept {
            vector<Slot> old = std::move(_slots);
            _slots = vector<Slot>(old.size() * 2);
            for (auto& x : old) {
                if (x.id) {
                    _place(x.id, std::move(x.st));
                }
            }
            size_t high = stream_table_high.load(std::memory_order_relaxed);
            while (_slots.size() > high && !stream_table_high.compare_exchange_weak(high, _slots.size(), std::memory_order_relaxed)) {
            }
        }
    };

    class ServerH2 final : public StreamOwner {
    public:
        SGCL_INLINE_HOT ServerH2(net::connection c, tracked_ptr<ServerImpl> s, tracked_ptr<detail::ServerSettings> cfg, tracked_ptr<ServerConn> node) noexcept
        : _c(std::move(c))
        , _s(std::move(s))
        , _cfg(std::move(cfg))
        , _node(std::move(node))
        , _m(*this, _machine_settings(*_cfg))
        , _wake(1) {
            _remote = _c.remote_endpoint();
        }

        // --- the machine's events (under the lock) ----------------------------

        ErrorCode on_request(uint32_t id, Block&& b, bool end_stream, bool start) noexcept {
            // §8.3.1, §8.2, §8.1.1: a malformed request is the stream's
            // PROTOCOL_ERROR (request_check.h)
            RequestHead head;
            if (auto e = check_request(b.fields, end_stream, head); e != ErrorCode::no_error) {
                return e;
            }
            const RequestField* method = head.method;
            const RequestField* authority = head.authority;
            const RequestField* path = head.path;
            tracked_ptr req = make_tracked<RequestImpl>();
            req->h2 = true;
            req->head = b.bytes;
            req->method = method_name(method->second.view());
            if (path) {
                req->target = path->second;
            }
            if (authority) {
                req->host = authority->second;
            }
            // the block's fields become the request's: the pseudo-fields,
            // first of them all (check_request, §8.3), moved out in place,
            // no second list (method, path and authority read above: the
            // pointers into the list are not used past here)
            auto& fields = HeadersAccess::fields(b.fields);
            fields.erase(fields.begin(), fields.begin() + (fields.size() - head.regulars));
            req->fields = std::move(b.fields);
            auto& out = HeadersAccess::fields(req->fields);
            if (!authority) {
                for (auto& f : out) {
                    if (f.first.view() == "host") {
                        req->host = f.second;
                        break;
                    }
                }
            }
            req->content_length = head.content_length;
            req->remote = _remote;
            req->stop = _node->stop.token();
            tracked_ptr st = make_tracked<ServerStream>(id, tracked_ptr<StreamOwner>(this));
            st->req = req;
            st->count.declared = req->content_length;
            if (!end_stream) {
                req->body = make_tracked<Body>(tracked_ptr<StreamState>(st), _cfg->max_body_bytes, false, req->content_length);
                st->small_body = req->content_length && *req->content_length <= SmallBody && !HeadersAccess::count(req->fields, "expect");
            }
            // the server's read_timeout and write_timeout, per stream, from
            // its head (Go's ReadTimeout and WriteTimeout in HTTP/2)
            const int64_t now = now_ns();
            if (!end_stream && _cfg->read_timeout > duration::zero()) {
                st->read_by = now + _cfg->read_timeout.nanoseconds();
            }
            if (_cfg->write_timeout > duration::zero()) {
                st->write_by = now + _cfg->write_timeout.nanoseconds();
            }
            _streams.insert_or_assign(id, st);
            if (start) {
                _turn(*st);
            }
            return ErrorCode::no_error;
        }

        SGCL_INLINE_HOT void on_start(uint32_t id) noexcept {
            if (auto st = _find(id)) {
                _turn(*st);
            } else {
                _to_start.push_back(id);   // gone: _start_waiting gives its place back
            }
        }

        // A request's turn to run: now, or with its body's first bytes
        // (SmallBody)
        SGCL_INLINE_HOT void _turn(ServerStream& st) noexcept {
            if (st.small_body && !st.body_came) {
                st.deferred = true;
                return;
            }
            _to_start.push_back(st.id);
        }

        // Its body's first bytes (or its end) came: a deferred start is due
        SGCL_INLINE_HOT void _body_came(ServerStream& st) noexcept {
            st.body_came = true;
            if (st.deferred) {
                st.deferred = false;
                _to_start.push_back(st.id);
            }
        }

        void on_trailers(uint32_t id, Block&& b) {
            if (auto st = _find(id)) {
                if (!st->count.ended() || check_trailers(b.fields) != ErrorCode::no_error) {
                    _malformed.push_back(id);
                    return;
                }
                http::headers t;
                for (auto& f : HeadersAccess::fields(b.fields)) {
                    if (f.first.view()[0] != ':') {
                        HeadersAccess::add(t, f.first, f.second);
                    }
                }
                st->read_by = 0;
                st->end_with(std::move(t));
                _body_came(*st);
            }
        }

        void on_data(uint32_t id, const uint8_t* p, size_t n, bool end_stream) {
            if (auto st = _find(id)) {
                // §8.1.1: DATA past the content-length, or an end short of
                // it, is a malformed request: the stream's PROTOCOL_ERROR,
                // given after feed returns (an event does not call the machine)
                if (!st->count.data(n, end_stream)) {
                    _malformed.push_back(id);
                    return;
                }
                if (end_stream) {
                    st->read_by = 0;
                }
                st->add(p, n, end_stream);
                if (n || end_stream) {
                    _body_came(*st);
                }
            }
        }

        SGCL_INLINE_HOT void on_reset(uint32_t id, ErrorCode code) {
            if (auto st = _find(id)) {
                st->reset_by(code);
                if (!st->started) {
                    const bool held = st->deferred;
                    _streams.erase(id);   // waited its turn: it never starts (connection.h)
                    if (held) {
                        _to_start.push_back(id);   // its turn had come: _start_waiting gives the place back
                    }
                }
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

        SGCL_INLINE_HOT void on_goaway(uint32_t, ErrorCode) noexcept {
        }

        SGCL_INLINE_HOT void on_ping_ack(const uint8_t*) noexcept {
        }

        // --- StreamOwner ---------------------------------------------------------

        expected<void, io::error> send_headers(uint32_t id, const FieldBlock& block, bool end_stream) override {
            bool sent = false;
            {
                std::lock_guard<std::mutex> g(_lock);
                // encoded only when it will go: the encoder's table is the
                // peer's decoder's (RFC 7541 §2.2)
                if (!_m.failed() && _m.sendable(id)) {
                    _block.clear();
                    _m.encoder().begin_block(_block);
                    block.encode(_m.encoder(), _block);
                    sent = _m.send_headers(id, reinterpret_cast<const uint8_t*>(_block.data()), _block.size(), end_stream);
                }
            }
            _kick();
            if (!sent) {
                return unexpected(stream_reset_error("write", ErrorCode::cancel));
            }
            return {};
        }

        expected<size_t, io::error> send_now(uint32_t id, const FieldBlock* block, bool headers_end, const BodyBuffer* data, bool end_stream) override {
            size_t taken = 0;
            bool dead = false;
            {
                std::lock_guard<std::mutex> g(_lock);
                if (_m.failed() || !_m.sendable(id)) {
                    dead = true;
                } else {
                    if (block) {
                        // encoded only when it will go (RFC 7541 §2.2)
                        _block.clear();
                        _m.encoder().begin_block(_block);
                        block->encode(_m.encoder(), _block);
                        dead = !_m.send_headers(id, reinterpret_cast<const uint8_t*>(_block.data()), _block.size(), headers_end);
                    }
                    if (!dead && !(block && headers_end)) {
                        const size_t total = data ? data->size() : 0;
                        bool full = true;
                        if (data) {
                            // the bytes inside the buffer copied (they are
                            // the writer's), its blocks' in place (retire)
                            auto small = data->small();
                            if (!small.empty()) {
                                const bool last = small.size() == total;
                                taken = _m.send_data(id, reinterpret_cast<const uint8_t*>(small.data()), small.size(), end_stream && last);
                                full = taken == small.size();
                            }
                            data->chunks().each([&](const slice<const byte>& s) {
                                if (!full) {
                                    return;
                                }
                                const bool last = taken + s.size() == total;
                                const size_t k = _send_block_now(id, s, end_stream && last);
                                taken += k;
                                full = k == s.size();
                            });
                        }
                        if (total == 0 && end_stream) {
                            _m.send_data(id, nullptr, 0, true);
                        }
                    }
                }
            }
            _kick();
            if (dead) {
                return unexpected(stream_reset_error("write", ErrorCode::cancel));
            }
            return taken;
        }

        async::task<expected<void, io::error>> send_data(uint32_t id, slice<const byte> data, bool end_stream) noexcept override {
            return _send_data(tracked_ptr<ServerH2>(this), id, std::move(data), end_stream, false);
        }

        async::task<expected<void, io::error>> send_block(uint32_t id, slice<const byte> data, bool end_stream) noexcept override {
            return _send_data(tracked_ptr<ServerH2>(this), id, std::move(data), end_stream, true);
        }

        // The blocks kept until the write that sends them in place is done
        // (the pump gives them back after its next write)
        void retire(BodyBuffer& body) override {
            tracked_ptr<ByteChunk> first;
            tracked_ptr<ByteChunk> last;
            body.detach(first, last);
            if (!first) {
                return;
            }
            {
                std::lock_guard<std::mutex> g(_lock);
                if (_retired_last) {
                    _retired_last->next = std::move(first);
                } else {
                    _retired = std::move(first);
                }
                _retired_last = std::move(last);
            }
            _kick();
        }

        // Under the lock: a block's piece as DATA in place when it is worth
        // a piece of the write of its own, else copied
        SGCL_INLINE_HOT size_t _send_block_now(uint32_t id, const slice<const byte>& s, bool end_stream) noexcept {
            if (s.size() < InPlaceMin) {
                return _m.send_data(id, reinterpret_cast<const uint8_t*>(s.data()), s.size(), end_stream);
            }
            return _m.send_data_in_place(id, s, end_stream);
        }

        static async::task<expected<void, io::error>> _send_data(tracked_ptr<ServerH2> h, uint32_t id, slice<const byte> data, bool end_stream, bool in_place) noexcept {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
            size_t at = 0;
            for (;;) {
                tracked_ptr<ServerStream> st;
                bool done = false;
                bool dead = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    st = h->_find(id);
                    if (!st || st->was_reset() || h->_m.failed() || !h->_m.sendable(id)) {
                        dead = true;
                    } else {
                        if (in_place) {
                            at += h->_send_block_now(id, data.last(data.size() - at), end_stream);   // a slice of the block's owner
                        } else {
                            at += h->_m.send_data(id, p + at, data.size() - at, end_stream);
                        }
                        done = at == data.size() && (!end_stream || !h->_m.sendable(id));
                    }
                }
                h->_kick();
                if (dead) {
                    co_return unexpected(stream_reset_error("write", ErrorCode::cancel));
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
                auto st = _find(id);
                if (!st || st->was_reset()) {
                    return;   // reset: the machine gave its bytes back
                }
                _m.consumed(id, n);
            }
            _kick();
        }

        void reset(uint32_t id, ErrorCode code) override {
            {
                std::lock_guard<std::mutex> g(_lock);
                if (auto st = _find(id)) {
                    st->reset_by(code);
                }
                _m.reset(id, code);
            }
            _kick();
        }

        // --- the connection's tasks -----------------------------------------------

        // The connection's reader: frames fed to the machine until the end
        static async::task<> serve(tracked_ptr<ServerH2> h, tracked_ptr<Wire> wire) noexcept {
            auto& c = h->_c;
            {
                std::lock_guard<std::mutex> g(h->_lock);
                h->_m.start(now_ns());
            }
            h->_kick();
            async::go(_pump(h));
            async::go(_ticker(weak_ptr<ServerH2>(h), _tick_period(*h->_cfg)));
            wire->reserve(size_t(h->_m.limits().max_frame_size) + FrameHeaderSize);
            for (;;) {
                if (wire->buffered()) {
                    expected<size_t, Error> r = size_t(0);
                    {
                        std::lock_guard<std::mutex> g(h->_lock);
                        auto v = wire->view();
                        r = h->_m.feed(reinterpret_cast<const uint8_t*>(v.data()), v.size(), now_ns());
                        for (uint32_t id : h->_malformed) {
                            if (auto st = h->_find(id)) {
                                st->reset_by(ErrorCode::protocol_error);
                                h->_body_came(*st);   // a deferred handler started: it sees the reset and ends
                            }
                            h->_m.reset(id, ErrorCode::protocol_error);
                        }
                        h->_malformed.clear();
                    }
                    if (r) {
                        wire->consume(*r);
                    }
                    h->_start_waiting();
                    h->_kick();
                    if (!r) {
                        break;
                    }
                }
                bool idle = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    if (h->_m.finished()) {
                        break;
                    }
                    idle = h->_streams.empty();
                    if (idle && h->_s->shutting_down.load()) {
                        h->_m.goaway(ErrorCode::no_error);
                    }
                }
                if (idle && h->_s->shutting_down.load()) {
                    h->_kick();
                    break;
                }
                if (idle) {
                    h->_node->state.store(ServerConn::idle);
                    c.set_read_deadline(deadline_after(h->_cfg->idle_timeout));
                } else {
                    c.set_read_deadline(time_point());
                }
                // wire->fill() in this frame (Wire::try_fill, as HTTP/1.1's
                // loop): a TLS connection gives its record block back to the
                // worker that took it before the wait, not to the one that
                // resumes (DESIGN 277: no system allocation per request)
                expected<size_t, io::error> r = size_t(0);
                for (;;) {
                    auto step = wire->try_fill();
                    if (step.done) {
                        r = std::move(step.result);
                        break;
                    }
                    if (step.slow) {
                        r = co_await wire->fill();
                        break;
                    }
                    if (auto ready = co_await step.ready; !ready) {
                        r = fail(ready);
                        break;
                    }
                }
                if (!r || *r == 0) {
                    break;
                }
                // bytes: active again, unless shutdown took the connection
                // while idle (its streams all released while the reader waited)
                int expected = ServerConn::idle;
                if (!h->_node->state.compare_exchange_strong(expected, ServerConn::active) && expected == ServerConn::closed) {
                    break;
                }
            }
            // the end: every stream still open reset for its handler, the
            // handlers awaited, what is left to send sent
            {
                std::lock_guard<std::mutex> g(h->_lock);
                h->_closing = true;
                for (auto& [k, st] : h->_streams) {
                    st->reset_by(ErrorCode::cancel);
                }
            }
            co_await h->_handlers;
            h->_wake.close();
            co_await h->_pump_done;
        }

    private:
        net::connection _c;
        tracked_ptr<ServerImpl> _s;
        tracked_ptr<detail::ServerSettings> _cfg;
        tracked_ptr<ServerConn> _node;
        std::mutex _lock;
        ServerConnection<ServerH2> _m;
        ServerStreams _streams;
        std::vector<uint32_t> _to_start;     // requests to start, gathered under the lock, started after it
        std::vector<uint32_t> _malformed;    // streams to reset PROTOCOL_ERROR once feed returns
        std::string _block;                  // a field block being encoded (under the lock)
        std::string _send;                   // the output's own bytes, taken by the writer (swapped with the machine's)
        vector<ServerConnection<ServerH2>::OutPiece> _send_pieces;   // the DATA payloads in place among them, slices of their blocks
        vector<slice<const byte>> _parts;    // the write's pieces in order (the writer's, kept for its room)
        tracked_ptr<ByteChunk> _retired;     // body blocks queued in place, back to the pool after the next write (under the lock)
        tracked_ptr<ByteChunk> _retired_last;

        // A block's piece shorter than this is copied into the output, not
        // sent as a piece of its own
        static constexpr size_t InPlaceMin = 512;
        async::channel<void> _wake;          // something to send (a signal, one held)
        async::event _pump_done;
        async::wait_group _handlers;
        net::endpoint _remote;
        bool _closing = false;

        SGCL_INLINE_HOT static h2::ServerSettings _machine_settings(const detail::ServerSettings& cfg) noexcept {
            h2::ServerSettings m;
            m.max_concurrent_streams = cfg.max_concurrent_streams;
            m.max_header_list_size = uint32_t(std::min<size_t>(cfg.max_header_bytes, 0xFFFFFFFFu));
            return m;
        }

        SGCL_INLINE_HOT tracked_ptr<ServerStream> _find(uint32_t id) noexcept {
            return _streams.find(id);
        }

        SGCL_INLINE_HOT void _kick() {
            _wake.try_send();
        }

        // The pieces of the write in order: the output's own bytes up to
        // each payload in place, the payload, and the bytes after the last
        void _gather() noexcept {
            const byte* bytes = reinterpret_cast<const byte*>(_send.data());
            size_t at = 0;
            for (auto& q : _send_pieces) {
                if (q.at > at) {
                    _parts.push_back(slice<const byte>(bytes + at, q.at - at));
                }
                _parts.push_back(q.piece);
                at = q.at;
            }
            if (_send.size() > at) {
                _parts.push_back(slice<const byte>(bytes + at, _send.size() - at));
            }
        }

        // The requests whose turn came, started (after the lock: a handler
        // may call the connection at once)
        void _start_waiting() {
            for (;;) {
                tracked_ptr<ServerStream> st;
                {
                    std::lock_guard<std::mutex> g(_lock);
                    while (!_to_start.empty() && !st) {
                        const uint32_t id = _to_start.front();
                        _to_start.erase(_to_start.begin());
                        st = _find(id);
                        if (!st || _closing) {
                            // reset before its handler started, or the end:
                            // the place the machine gave it goes back
                            st = tracked_ptr<ServerStream>();
                            _streams.erase(id);
                            _m.release(id);
                        }
                    }
                    if (!st) {
                        return;
                    }
                    st->started = true;
                }
                _handlers.add();
                async::go(_run(tracked_ptr<ServerH2>(this), st));
            }
        }

        // A handler's end: its place to the next request, its stream
        // forgotten; at shutdown, the connection's GOAWAY
        void _release(uint32_t id) {
            {
                std::lock_guard<std::mutex> g(_lock);
                _streams.erase(id);
                _m.release(id);
                if (_streams.empty()) {
                    // no stream: idle, for shutdown() to close it as it
                    // closes an idle connection of HTTP/1.1 (the reader,
                    // waiting for bytes, takes it back when they come)
                    _node->state.store(ServerConn::idle);
                }
                if (_s->shutting_down.load()) {
                    _m.goaway(ErrorCode::no_error);
                }
            }
            _start_waiting();
            _kick();
        }

        // One request: routed, its handler run, its response finished
        static async::task<> _run(tracked_ptr<ServerH2> h, tracked_ptr<ServerStream> st) noexcept {
            const auto start = sgcl::clock::now();
            tracked_ptr<RequestImpl> req = st->req;
            tracked_ptr w = make_tracked<WriterImpl>();
            w->h2 = tracked_ptr<StreamState>(st);
            std::string_view method = req->method.view();
            w->head_request = method == "HEAD";
            auto r = RequestAccess::make(req);
            auto writer = WriterAccess::make(w);
            std::string_view host_text = req->host.empty() ? std::string_view() : req->host.view();
            std::string_view target = req->target.empty() ? std::string_view() : req->target.view();
            string route_path;
            std::string_view path;
            bool refused = false;
            if (method == "CONNECT") {
                writer.error(status::method_not_allowed);   // not in v1 (sketch-http2.md, 12)
                refused = true;
            } else if (auto fast = net::detail::origin_form_path(host_text.empty() ? std::string_view("localhost") : host_text, target)) {
                path = *fast;
                req->url_later = true;
            } else if (!target.empty() && (target.front() == '/' || target == "*")) {
                if (auto u = net::url::parse(string::concat("http://", host_text.empty() ? std::string_view("localhost") : host_text,
                                                            target == "*" ? std::string_view("/") : target))) {
                    req->url = std::move(*u);
                    route_path = req->url->path();
                    path = route_path.view();
                } else {
                    writer.error(status::bad_request);
                    refused = true;
                }
            } else {
                writer.error(status::bad_request);
                refused = true;
            }
            if (!refused && req->content_length && h->_cfg->max_body_bytes && *req->content_length > h->_cfg->max_body_bytes) {
                writer.error(status::content_too_large);
                refused = true;
            }
            if (!refused) {
                try {
                    if (auto t = dispatch(*h->_s, req, r, writer, method, host_text, path)) {
                        co_await *t;
                    }
                } catch (const std::exception& e) {
                    handler_threw(*h->_cfg, *req, *w, writer, e.what());
                } catch (...) {
                    handler_threw(*h->_cfg, *req, *w, writer, nullptr);
                }
            }
            auto& body = req->body;
            if (body && body->failed() && !w->head_sent && !w->touched) {
                writer.error(body->error_status() == 413 ? status::content_too_large : status::bad_request);
            }
            if (!w->head_sent) {
                if (auto e = invalid_field(w->fields)) {
                    h->_cfg->report(string("a handler of ") + req->method + " " + string(req->target) + " wrote " + *e);
                    w->failed.reset();
                    w->fields = http::headers();
                    writer.error(status::internal_server_error);
                } else if (w->failed) {
                    // a file of the body that could not be read, nothing sent yet
                    h->_cfg->report(string("a handler of ") + req->method + " " + string(req->target) + " wrote a file that failed: " + w->failed->message());
                    w->failed.reset();
                    w->fields = http::headers();
                    writer.error(status::internal_server_error);
                }
            }
            // the rest of the response: at once when the windows take it
            // (one step of the connection, no frame), a task only for what
            // waits for window (as HTTP/1.1's finish_start)
            const uint64_t body_bytes = w->body_bytes();   // before the finish gives the blocks back
            expected<void, io::error> sent;
            if (auto rest = w->finish_start(sent)) {
                sent = co_await *rest;
            }
            log_access(*h->_cfg, *req, *w, body_bytes, path, "HTTP/2.0", start);
            // a request whose body did not end: RST_STREAM NO_ERROR after a
            // whole response (§8.1), CANCEL after a broken one
            if (body && !body->done()) {
                h->reset(st->id, sent ? ErrorCode::no_error : ErrorCode::cancel);
            } else if (!sent) {
                h->reset(st->id, ErrorCode::cancel);
            }
            h->_release(st->id);
            h->_handlers.done();
        }

        // The one writer: the machine's output, taken under the lock, sent
        // in order; after the last GOAWAY with every stream closed, the
        // connection closed (the reader waiting on it wakes). The output is
        // taken whole, its own bytes swapped out (no copy) and the DATA
        // payloads in place as pieces among them: one write of the pieces
        // (a sendmsg; over TLS sealed from where they lie). The body
        // blocks retired before the take go back to the pool once that
        // write is done: every piece of them was in it or in one before
        static async::task<> _pump(tracked_ptr<ServerH2> h) noexcept {
            bool open = true;
            while (open) {
                open = co_await h->_wake.receive();
                for (;;) {
                    bool finished = false;
                    tracked_ptr<ByteChunk> giving;
                    {
                        std::lock_guard<std::mutex> g(h->_lock);
                        h->_m.take_output(h->_send, h->_send_pieces);
                        giving = std::move(h->_retired);
                        h->_retired = tracked_ptr<ByteChunk>();
                        h->_retired_last = tracked_ptr<ByteChunk>();
                        finished = h->_m.finished();
                    }
                    if (!h->_send.empty()) {
                        expected<size_t, io::error> r;
                        if (h->_send_pieces.empty()) {
                            r = co_await h->_c.async_write(slice<const byte>(reinterpret_cast<const byte*>(h->_send.data()), h->_send.size()));
                        } else {
                            h->_gather();
                            auto s = net::detail::ConnectionAccess::impl(h->_c).start_write_parts(h->_parts);
                            if (s.rest) {
                                r = co_await std::move(*s.rest);
                            } else {
                                r = std::move(s.done);
                            }
                            h->_parts.clear();
                            h->_send_pieces.clear();
                        }
                        ByteChunks::give_back(std::move(giving));
                        if (!r) {
                            (void)h->_c.close();
                            open = false;
                            break;
                        }
                        continue;
                    }
                    ByteChunks::give_back(std::move(giving));
                    if (finished) {
                        (void)h->_c.close_write();
                    }
                    break;
                }
            }
            h->_pump_done.set();
        }

        // The time for the machine (SETTINGS unanswered, streams starved of
        // window: connection.h) and for the streams' read_timeout and
        // write_timeout: a request whose body has not come whole by its
        // read_timeout is reset CANCEL (its handler's reads fail), a
        // response not whole by its write_timeout is reset INTERNAL_ERROR
        // (as Go's onWriteTimeout); the connection lives. Once a second, or
        // a quarter of the shorter timeout (10 ms at least)
        //
        // The ticker holds the connection weakly: asleep, it keeps nothing
        // of it alive, and a connection that has ended is garbage at once,
        // not a tick later (with its server, its buffers, its TLS state)
        static int64_t _tick_period(const detail::ServerSettings& cfg) noexcept {
            int64_t period = 1'000'000'000;
            for (auto t : {cfg.read_timeout, cfg.write_timeout}) {
                if (t > duration::zero()) {
                    period = std::min(period, std::max<int64_t>(10'000'000, t.nanoseconds() / 4));
                }
            }
            return period;
        }

        static async::task<> _ticker(weak_ptr<ServerH2> weak, int64_t period) noexcept {
            for (;;) {
                co_await async::after(std::chrono::nanoseconds(period));
                tracked_ptr<ServerH2> h = weak.lock();
                if (!h || h->_pump_done.is_set()) {
                    break;
                }
                bool failed = false;
                {
                    std::lock_guard<std::mutex> g(h->_lock);
                    if (h->_closing) {
                        break;
                    }
                    const int64_t now = now_ns();
                    failed = !h->_m.tick(now);
                    uint32_t late[32];
                    ErrorCode why[32];
                    size_t k = 0;
                    for (auto& [id, st] : h->_streams) {
                        if (k == std::size(late)) {
                            break;   // the rest on the next tick
                        }
                        if (st->read_by && now > st->read_by) {
                            st->read_by = 0;
                            late[k] = id;
                            why[k++] = ErrorCode::cancel;
                        } else if (st->write_by && now > st->write_by) {
                            st->write_by = 0;
                            late[k] = id;
                            why[k++] = ErrorCode::internal_error;
                        }
                    }
                    for (size_t i = 0; i < k && !failed; ++i) {
                        if (auto st = h->_find(late[i])) {
                            st->reset_by(why[i]);
                            h->_body_came(*st);   // a deferred handler started: it sees the reset and ends
                        }
                        h->_m.reset(late[i], why[i]);
                    }
                }
                h->_start_waiting();
                h->_kick();
                if (failed) {
                    break;   // GOAWAY is in the output; the peer's close ends the reader
                }
            }
        }
    };

    // The connection's HTTP/2, from its first byte (the wire holds what
    // was read of it already)
    inline async::task<> serve(tracked_ptr<ServerImpl> s, tracked_ptr<detail::ServerSettings> cfg, tracked_ptr<ServerConn> node, tracked_ptr<Wire> wire) noexcept {
        tracked_ptr h = make_tracked<ServerH2>(node->c, s, cfg, node);
        co_await ServerH2::serve(h, wire);
    }
}
