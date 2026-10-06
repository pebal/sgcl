//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "methods.h"
#include "../connection.h"
#include "../error.h"
#include "../http/websocket.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/promise.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/json.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

// A peer of JSON-RPC 2.0 over a byte stream (LSP's Content-Length framing,
// or one JSON a line) or a WebSocket: calls go out, requests come in to the
// methods, both at once
namespace sgcl::net::jsonrpc {
    class peer;

    // How messages are cut out of a byte stream: LSP's head of
    // "Content-Length: N" before each, or one JSON a line
    enum class framing : uint8_t { content_length, line };

    // A call's waits: the reply waited for at most timeout; the stop token
    // cancels it (the other side told, ECANCELED at once)
    struct call_options {
        duration timeout = std::chrono::seconds(30);
        async::stop_token stop;
    };

    // One of a batch: a call, or a notification (no answer)
    struct batch_entry {
        string method;
        json params;            // null: none sent
        bool notification = false;
    };

    namespace detail {
        // The messages of one transport: read whole, written whole
        class RpcLink {
        public:
            virtual ~RpcLink() = default;
            virtual async::task<expected<optional<string>, io::error>> read() noexcept = 0;   // none: the end
            virtual async::task<expected<void, io::error>> write(string text) noexcept = 0;
            virtual expected<void, io::error> close() noexcept = 0;
        };

        // The message at the buffer's front: 1 with where its text starts,
        // its length and the bytes it takes; 0 when it is not whole yet; -1
        // for bytes that break the framing (why says how). LSP's head is
        // header lines ("Content-Length: N", any others passed over) and an
        // empty line; a line's message ends at LF, a CR before it dropped.
        inline int rpc_frame(std::string_view b, framing f, size_t max, size_t& from, size_t& length, size_t& used, const char*& why) noexcept {
            if (f == framing::line) {
                size_t nl = b.find('\n');
                if (nl == std::string_view::npos) {
                    if (b.size() > max) {
                        why = "a message past max_message";
                        return -1;
                    }
                    return 0;
                }
                from = 0;
                length = nl > 0 && b[nl - 1] == '\r' ? nl - 1 : nl;
                used = nl + 1;
                return 1;
            }
            size_t head = b.find("\r\n\r\n");
            if (head == std::string_view::npos) {
                if (b.size() > 64 * 1024) {
                    why = "a head past its limit";
                    return -1;
                }
                return 0;
            }
            bool have = false;
            size_t n = 0;
            std::string_view h = b.substr(0, head);
            size_t at = 0;
            while (at <= h.size()) {
                size_t e = h.find("\r\n", at);
                std::string_view line = h.substr(at, e == std::string_view::npos ? std::string_view::npos : e - at);
                constexpr std::string_view Name = "content-length";
                if (line.size() > Name.size() && line[Name.size()] == ':') {
                    bool is = true;
                    for (size_t i = 0; i < Name.size() && is; ++i) {
                        is = char(line[i] | 0x20) == Name[i];
                    }
                    if (is) {
                        std::string_view v = line.substr(Name.size() + 1);
                        while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) {
                            v.remove_prefix(1);
                        }
                        while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) {
                            v.remove_suffix(1);
                        }
                        auto r = std::from_chars(v.data(), v.data() + v.size(), n);
                        if (v.empty() || r.ec != std::errc() || r.ptr != v.data() + v.size()) {
                            why = "a Content-Length that does not read";
                            return -1;
                        }
                        have = true;
                    }
                }
                if (e == std::string_view::npos) {
                    break;
                }
                at = e + 2;
            }
            if (!have) {
                why = "a message without Content-Length";
                return -1;
            }
            if (n > max) {
                why = "a message past max_message";
                return -1;
            }
            size_t body = head + 4;
            if (b.size() - body < n) {
                return 0;
            }
            from = body;
            length = n;
            used = body + n;
            return 1;
        }

        // A byte stream cut by the framing
        class RpcStreamLink final : public RpcLink {
        public:
            RpcStreamLink(const net::connection& c, framing f, size_t max) noexcept
            : _c(c), _framing(f), _max(max) {
            }

            async::task<expected<optional<string>, io::error>> read() noexcept override {
                for (;;) {
                    size_t from = 0, length = 0, used = 0;
                    const char* why = nullptr;
                    int f = rpc_frame(std::string_view(_buf).substr(_at), _framing, _max, from, length, used, why);
                    if (f < 0) {
                        co_return unexpected(rpc_error(errc::malformed, "jsonrpc", string(why)));
                    }
                    if (f > 0) {
                        string text(std::string_view(_buf).substr(_at + from, length));
                        _at += used;
                        if (text.empty()) {
                            continue;   // an empty line between messages
                        }
                        co_return optional<string>(std::move(text));
                    }
                    if (_at) {
                        _buf.erase(0, _at);
                        _at = 0;
                    }
                    auto r = co_await _c.async_read(slice<byte>(_block->data(), _block->size()));
                    if (!r) {
                        if (r.error().code() == io::errc::closed || r.error().code() == io::errc::unexpected_eof) {
                            co_return optional<string>();
                        }
                        co_return unexpected(r.error());
                    }
                    if (*r == 0) {
                        co_return optional<string>();
                    }
                    _buf.append(reinterpret_cast<const char*>(_block->data()), *r);
                }
            }

            async::task<expected<void, io::error>> write(string text) noexcept override {
                std::string out;
                if (_framing == framing::content_length) {
                    out = "Content-Length: " + std::to_string(text.size()) + "\r\n\r\n";
                    out.append(text.view());
                } else {
                    out.reserve(text.size() + 1);
                    out.append(text.view());
                    out += '\n';
                }
                auto guard = co_await _write_lock.scoped_lock();
                auto w = co_await _c.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
                if (!w) {
                    co_return unexpected(w.error());
                }
                co_return expected<void, io::error>();
            }

            expected<void, io::error> close() noexcept override {
                return _c.close();
            }

        private:
            net::connection _c;
            framing _framing;
            size_t _max;
            std::string _buf;
            size_t _at = 0;
            tracked_ptr<array<std::byte, 16384>> _block = make_tracked<array<std::byte, 16384>>();
            async::mutex _write_lock;
        };

        // A WebSocket: a message of text a JSON-RPC message
        class RpcWsLink final : public RpcLink {
        public:
            explicit RpcWsLink(const http::websocket& ws) noexcept
            : _ws(ws) {
            }

            async::task<expected<optional<string>, io::error>> read() noexcept override {
                auto m = co_await _ws.async_receive();
                if (!m) {
                    if (m.error().code() == io::errc::closed || m.error().code() == io::errc::unexpected_eof) {
                        co_return optional<string>();
                    }
                    co_return unexpected(m.error());
                }
                co_return optional<string>(m->text());
            }

            async::task<expected<void, io::error>> write(string text) noexcept override {
                co_return co_await _ws.async_send(text);
            }

            expected<void, io::error> close() noexcept override {
                async::go(_close(_ws));   // the close handshake by a task: this may run on a worker, which must not wait
                return {};
            }

        private:
            static async::task<> _close(http::websocket ws) noexcept {
                (void)co_await ws.async_close();
            }

            http::websocket _ws;
        };

        struct RpcPending {
            async::promise<bool> done;
            json response;
        };

        struct RpcPeerState {
            tracked_ptr<RpcLink> link;
            tracked_ptr<RpcMethodsState> methods;
            string cancel_method;
            std::mutex lock;
            int64_t next_id = 0;
            map<int64_t, tracked_ptr<RpcPending>> pending;
            map<string, async::stop_source> running;   // the requests that came in, by their id's JSON: their cancellation
            optional<io::error> end;
            std::atomic<bool> closed = {false};
            async::promise<bool> ended;
        };

        inline io::error rpc_ended(RpcPeerState& s) {
            std::lock_guard g(s.lock);
            return s.end ? *s.end : io::error(io::errc::closed, "jsonrpc", string());
        }

        inline void rpc_end(RpcPeerState& s, const io::error& e) {
            map<int64_t, tracked_ptr<RpcPending>> pending;
            map<string, async::stop_source> running;
            {
                std::lock_guard g(s.lock);
                if (!s.end) {
                    s.end = e;
                }
                pending = std::move(s.pending);
                s.pending = map<int64_t, tracked_ptr<RpcPending>>();
                running = std::move(s.running);
                s.running = map<string, async::stop_source>();
            }
            if (s.closed.exchange(true)) {
                return;
            }
            for (auto& [id, p] : pending) {
                p->done.set_value(false);
            }
            for (auto& [id, stop] : running) {
                stop.request_stop();
            }
            (void)s.link->close();
            s.ended.set_value(true);
        }

        // A response that came: to the call of its id
        inline void rpc_response_in(RpcPeerState& s, const json& msg) {
            auto id = msg["id"].as_int();
            if (!id) {
                return;   // an id this side never sends: not ours
            }
            tracked_ptr<RpcPending> p;
            {
                std::lock_guard g(s.lock);
                auto it = s.pending.find(*id);
                if (it != s.pending.end()) {
                    p = it->second;
                    s.pending.erase(it);
                }
            }
            if (p) {
                p->response = msg;
                p->done.set_value(true);
            }
        }

        // A request that came: dispatched under a stop source of its own,
        // its response written when there is one
        inline async::task<> rpc_serve_one(tracked_ptr<RpcPeerState> s, json msg) noexcept {
            async::stop_source stop;
            string key;
            bool has_id = msg.is_object() && msg.contains(string("id"));
            if (has_id) {
                key = msg["id"].to_string();
                std::lock_guard g(s->lock);
                s->running[key] = stop;
            }
            auto r = co_await rpc_dispatch(s->methods, msg, stop.token());
            if (has_id) {
                std::lock_guard g(s->lock);
                s->running.erase(key);
            }
            if (r) {
                (void)co_await s->link->write(r->to_string());
            }
        }

        inline async::task<> rpc_serve_batch(tracked_ptr<RpcPeerState> s, string text) noexcept {
            auto r = co_await rpc_handle_text(s->methods, std::move(text), async::stop_token());
            if (r) {
                (void)co_await s->link->write(*r);
            }
        }

        // The cancel notification of a request that came: its stop requested
        inline bool rpc_cancel_in(RpcPeerState& s, const json& msg) {
            if (s.cancel_method.empty() || !msg.is_object() || msg.contains(string("id")) || msg["method"].as_string(string()) != s.cancel_method) {
                return false;
            }
            string key = msg["params"]["id"].to_string();
            async::stop_source stop;
            bool found = false;
            {
                std::lock_guard g(s.lock);
                auto it = s.running.find(key);
                if (it != s.running.end()) {
                    stop = it->second;
                    found = true;
                }
            }
            if (found) {
                stop.request_stop();
            }
            return true;
        }

        // The reader: each message a response to a call of this side, or a
        // request (a batch) to serve in a task of its own
        inline async::task<> rpc_read_loop(tracked_ptr<RpcPeerState> s) noexcept {
            io::error end(io::errc::closed, "jsonrpc", string());
            for (;;) {
                auto text = co_await s->link->read();
                if (!text) {
                    end = text.error();
                    break;
                }
                if (!*text) {
                    break;
                }
                auto msg = json::parse(**text);
                if (!msg) {
                    // a message that is no JSON: the specification's parse error, the connection goes on
                    (void)co_await s->link->write(rpc_response_error(json(nullptr), error(errc::parse_error, string("Parse error"))).to_string());
                    continue;
                }
                if (msg->is_array()) {
                    bool responses = !msg->empty() && (*msg)[0].is_object() && !(*msg)[0].contains(string("method"));
                    if (responses) {
                        for (auto& e : msg->elements()) {
                            rpc_response_in(*s, e);
                        }
                    } else {
                        async::go(rpc_serve_batch(s, std::move(**text)));
                    }
                    continue;
                }
                if (msg->is_object() && !msg->contains(string("method")) && (msg->contains(string("result")) || msg->contains(string("error")))) {
                    rpc_response_in(*s, *msg);
                    continue;
                }
                if (rpc_cancel_in(*s, *msg)) {
                    continue;
                }
                {
                    // a method that answers at once runs here, its answer written in turn; one that waits
                    // (add_task) in a task of its own
                    RpcCall c = rpc_check(*s->methods, *msg);
                    if (c.done) {
                        if (c.answer) {
                            (void)co_await s->link->write(c.answer->to_string());
                        }
                        continue;
                    }
                }
                async::go(rpc_serve_one(s, std::move(*msg)));
            }
            rpc_end(*s, end);
        }

        inline json rpc_request(int64_t id, const string& method, const json& params, bool notification) {
            json r = json::object({{string("jsonrpc"), json("2.0")}, {string("method"), json(method)}});
            if (!params.is_null()) {
                r = r.set(string("params"), params);
            }
            if (!notification) {
                r = r.set(string("id"), json(id));
            }
            return r;
        }

        // A response's outcome: its result, or its error object as an error
        inline expected<json, io::error> rpc_outcome(const json& response, const string& method) {
            if (response.contains(string("error"))) {
                const json& e = response["error"];
                auto code = e["code"].as_int();
                if (!e.is_object() || !code || !e["message"].is_string()) {
                    return unexpected(rpc_error(errc::malformed, string::concat("jsonrpc ", method), string("an error object that does not read")));
                }
                error err(int(*code), *e["message"].as_string());
                if (e.contains(string("data"))) {
                    err.data = e["data"];
                }
                return unexpected(rpc_error(err, string::concat("jsonrpc ", method)));
            }
            if (!response.contains(string("result"))) {
                return unexpected(rpc_error(errc::malformed, string::concat("jsonrpc ", method), string("a response without result or error")));
            }
            return response["result"];
        }
    }

    // A connection of JSON-RPC 2.0 over a byte stream or a WebSocket, both
    // ways at once, as LSP's: this side's calls go out and wait for their
    // responses, the other side's requests go to the methods, each in a
    // task of its own, their responses written as they finish. A handle of
    // one word: a copy is the same connection.
    //
    //     auto p = net::jsonrpc::peer::connect(c, {.framing = net::jsonrpc::framing::content_length});
    //     auto r = p.call("initialize", json::object({{"processId", json(nullptr)}}));
    class peer {
    public:
        struct options {
            jsonrpc::framing framing = jsonrpc::framing::content_length;   // over a stream
            jsonrpc::methods methods;                                       // what the other side may call; an empty table by default
            string cancel_method = string("$/cancelRequest");               // sent with {"id"} when a call is cancelled; empty: none
            size_t max_message = size_t(64) << 20;                          // a message past it ends the connection
        };

        peer() noexcept = default;

        // JSON-RPC over the connection, framed as the options say: read from
        // now on (the requests that come served by the options' methods)
        static peer connect(const net::connection& c) {
            return connect(c, options());
        }

        static peer connect(const net::connection& c, const options& o) {
            return _start(make_tracked<detail::RpcStreamLink>(c, o.framing, o.max_message), o);
        }

        // JSON-RPC over the WebSocket: a message of text each
        static peer connect(const http::websocket& ws) {
            return connect(ws, options());
        }

        static peer connect(const http::websocket& ws, const options& o) {
            return _start(make_tracked<detail::RpcWsLink>(ws), o);
        }

        // A call and its result: the error object of a refusal as an error
        // of the category "jsonrpc" (error_of reads it back); ETIMEDOUT past
        // the timeout; ECANCELED when the stop token is stopped (the other
        // side then sent the cancel notification)
        // `call(...)` on this thread, `co_await async_call(...)` in a task
        expected<json, io::error> call(const string& method, const json& params = json(), const call_options& o = {}) const {
            return async_call(method, params, o).wait();
        }

        async::task<expected<json, io::error>> async_call(string method, json params = json(), call_options o = {}) const noexcept {
            return _co_call(_s, std::move(method), std::move(params), std::move(o));
        }

        // A notification: written, nothing answered
        // `notify(...)` on this thread, `co_await async_notify(...)` in a task
        expected<void, io::error> notify(const string& method, const json& params = json()) const {
            return async_notify(method, params).wait();
        }

        async::task<expected<void, io::error>> async_notify(string method, json params = json()) const noexcept {
            return _co_notify(_s, std::move(method), std::move(params));
        }

        // A batch (§6): its calls' outcomes in the order of the calls (the
        // notifications have none); the batch's own failure (the connection,
        // the timeout) as the error
        // `batch(...)` on this thread, `co_await async_batch(...)` in a task
        expected<vector<expected<json, io::error>>, io::error> batch(const vector<batch_entry>& entries, const call_options& o = {}) const {
            return async_batch(entries, o).wait();
        }

        async::task<expected<vector<expected<json, io::error>>, io::error>> async_batch(vector<batch_entry> entries, call_options o = {}) const noexcept {
            return _co_batch(_s, std::move(entries), std::move(o));
        }

        // Until the connection ends: the error that ended it, nothing for a
        // close of either side
        // `wait()` on this thread, `co_await async_wait()` in a task
        expected<void, io::error> wait() const {
            return async_wait().wait();
        }

        async::task<expected<void, io::error>> async_wait() const noexcept {
            return _co_wait(_s);
        }

        // The connection closed: the calls waiting fail, the requests running
        // are stopped
        expected<void, io::error> close() const noexcept {
            detail::rpc_end(*_s, io::error(io::errc::closed, "jsonrpc", string()));
            return {};
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const peer& a, const peer& b) noexcept {
            return a._s == b._s;
        }

    private:
        explicit peer(tracked_ptr<detail::RpcPeerState> s) noexcept
        : _s(std::move(s)) {
        }

        static peer _start(tracked_ptr<detail::RpcLink> link, const options& o) {
            tracked_ptr s = make_tracked<detail::RpcPeerState>();
            s->link = std::move(link);
            s->methods = detail::RpcMethodsAccess::state(o.methods);
            s->cancel_method = o.cancel_method;
            async::go(detail::rpc_read_loop(s));
            return peer(s);
        }

        static async::task<expected<json, io::error>> _co_call(tracked_ptr<detail::RpcPeerState> s, string method, json params, call_options o) noexcept {
            using namespace detail;
            tracked_ptr p = make_tracked<RpcPending>();
            int64_t id;
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                id = ++s->next_id;
                s->pending[id] = p;
            }
            if (auto w = co_await s->link->write(rpc_request(id, method, params, false).to_string()); !w) {
                rpc_end(*s, w.error());
                co_return unexpected(w.error());
            }
            bool late = false, cancelled = false;
            auto stop = o.stop;
            bool can_stop = stop.stop_possible();   // an empty token has no case to wait on
            if (o.timeout > duration::zero() && can_stop) {
                co_await async::select(p->done.on_done([] {}), async::timeout(o.timeout, [&] { late = true; }), stop.on_stop([&] { cancelled = true; }));
            } else if (o.timeout > duration::zero()) {
                co_await async::select(p->done.on_done([] {}), async::timeout(o.timeout, [&] { late = true; }));
            } else if (can_stop) {
                co_await async::select(p->done.on_done([] {}), stop.on_stop([&] { cancelled = true; }));
            } else {
                (void)co_await p->done;
            }
            if ((late || cancelled) && !p->done.done()) {
                {
                    std::lock_guard g(s->lock);
                    s->pending.erase(id);
                }
                if (cancelled && !s->cancel_method.empty()) {
                    (void)co_await s->link->write(rpc_request(0, s->cancel_method, json::object({{string("id"), json(id)}}), true).to_string());
                }
                co_return unexpected(io::error(error_code(cancelled ? ECANCELED : ETIMEDOUT, std::system_category()), string::concat("jsonrpc ", method), string()));
            }
            if (!p->done.result()) {
                co_return unexpected(rpc_ended(*s));
            }
            co_return rpc_outcome(p->response, method);
        }

        static async::task<expected<void, io::error>> _co_notify(tracked_ptr<detail::RpcPeerState> s, string method, json params) noexcept {
            if (s->closed.load()) {
                co_return unexpected(detail::rpc_ended(*s));
            }
            co_return co_await s->link->write(detail::rpc_request(0, method, params, true).to_string());
        }

        static async::task<expected<vector<expected<json, io::error>>, io::error>> _co_batch(tracked_ptr<detail::RpcPeerState> s, vector<batch_entry> entries,
                                                                                              call_options o) noexcept {
            using namespace detail;
            vector<int64_t> ids;
            vector<tracked_ptr<RpcPending>> waits;
            json all = json::array({});
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                for (auto& e : entries) {
                    int64_t id = 0;
                    if (!e.notification) {
                        id = ++s->next_id;
                        tracked_ptr p = make_tracked<RpcPending>();
                        s->pending[id] = p;
                        waits.push_back(p);
                    }
                    ids.push_back(id);
                    all = all.push_back(rpc_request(id, e.method, e.params, e.notification));
                }
            }
            if (entries.empty()) {
                co_return vector<expected<json, io::error>>();
            }
            if (auto w = co_await s->link->write(all.to_string()); !w) {
                rpc_end(*s, w.error());
                co_return unexpected(w.error());
            }
            time_point deadline = o.timeout > duration::zero() ? sgcl::clock::now() + o.timeout : time_point();
            for (auto& p : waits) {
                bool late = false;
                if (deadline != time_point()) {
                    co_await async::select(p->done.on_done([] {}), async::timeout(deadline - sgcl::clock::now(), [&] { late = true; }));
                } else {
                    (void)co_await p->done;
                }
                if (late && !p->done.done()) {
                    std::lock_guard g(s->lock);
                    for (int64_t id : ids) {
                        s->pending.erase(id);
                    }
                    co_return unexpected(io::error(error_code(ETIMEDOUT, std::system_category()), "jsonrpc batch", string()));
                }
                if (!p->done.result()) {
                    co_return unexpected(rpc_ended(*s));
                }
            }
            vector<expected<json, io::error>> out;
            size_t k = 0;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].notification) {
                    continue;
                }
                out.push_back(rpc_outcome(waits[k++]->response, entries[i].method));
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_wait(tracked_ptr<detail::RpcPeerState> s) noexcept {
            (void)co_await s->ended;
            auto e = detail::rpc_ended(*s);
            if (e.code() == io::errc::closed) {
                co_return expected<void, io::error>();
            }
            co_return unexpected(e);
        }

        tracked_ptr<detail::RpcPeerState> _s;
    };
}
