//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/protocol.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
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
#include "../../crypto/random.h"
#include "../../encoding/json.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

// The client of NATS: publish, subscribe with wildcards and queue groups,
// request and reply over one inbox, headers, flush, drain; user and
// password, a token, an nkey (with a JWT), TLS
namespace sgcl::net::nats {
    class client;
    class subscription;

    namespace detail {
        using NatsBlock = array<std::byte, 32768>;

        struct NatsSubState {
            int64_t sid = 0;
            string subject;
            string queue;
            async::channel<message> incoming;
            uint64_t delivered = 0;
            uint64_t max = 0;                        // unsubscribed after this many; 0: no limit
            std::atomic<uint64_t> dropped = {0};     // messages past the queue's limit
            optional<io::error> end;

            explicit NatsSubState(size_t queue)
            : incoming(queue ? queue : 1) {
            }
        };

        struct NatsRequest {
            async::promise<bool> done;
            message reply;
        };

        struct NatsState {
            net::connection conn;
            tracked_ptr<NatsBlock> block = make_tracked<NatsBlock>();
            std::mutex lock;
            map<int64_t, tracked_ptr<NatsSubState>> subs;
            int64_t next_sid = 0;
            string inbox;                            // "_INBOX.<random>.": the requests' replies under it
            int64_t inbox_sid = 0;
            map<string, tracked_ptr<NatsRequest>> requests;
            uint64_t next_token = 0;
            vector<tracked_ptr<async::promise<bool>>> pongs;   // the PINGs waiting, in their order
            int pings_out = 0;
            optional<io::error> end;
            std::atomic<bool> closed = {false};
            size_t max_payload = 1 << 20;
            bool headers = false;
            string server_id;
            string server;
            duration timeout = std::chrono::seconds(30);
            duration ping_interval = std::chrono::minutes(2);
            int max_pings_out = 2;
            size_t queue = 65536;
            async::channel<bool> stopped{1};
            // what is written: appended here in the order of the calls, written by the flusher
            std::string outbox;
            bool flushing = false;                   // the flusher was rung and has not emptied the outbox yet
            async::channel<bool> doorbell{1};
            vector<tracked_ptr<async::promise<bool>>> room;   // writers waiting while the outbox is past its limit
            std::mutex flush_lock;                   // a PING's promise and the PING in the same order
        };

        inline constexpr size_t NatsOutboxLimit = size_t(4) << 20;

        inline io::error nats_ended(NatsState& s) {
            std::lock_guard g(s.lock);
            return s.end ? *s.end : io::error(io::errc::closed, "nats", s.server);
        }

        inline void nats_end(NatsState& s, const io::error& e) {
            map<int64_t, tracked_ptr<NatsSubState>> subs;
            map<string, tracked_ptr<NatsRequest>> requests;
            vector<tracked_ptr<async::promise<bool>>> pongs;
            {
                std::lock_guard g(s.lock);
                if (!s.end) {
                    s.end = e;
                }
                subs = std::move(s.subs);
                s.subs = map<int64_t, tracked_ptr<NatsSubState>>();
                requests = std::move(s.requests);
                s.requests = map<string, tracked_ptr<NatsRequest>>();
                pongs = std::move(s.pongs);
                s.pongs = vector<tracked_ptr<async::promise<bool>>>();
            }
            if (s.closed.exchange(true)) {
                return;
            }
            for (auto& [sid, sub] : subs) {
                if (!sub->end) {
                    sub->end = e;
                }
                sub->incoming.close();   // what came before stays receivable
            }
            for (auto& [t, r] : requests) {
                r->done.set_value(false);
            }
            for (auto& p : pongs) {
                p->set_value(false);
            }
            vector<tracked_ptr<async::promise<bool>>> room;
            {
                std::lock_guard g(s.lock);
                room = std::move(s.room);
                s.room = vector<tracked_ptr<async::promise<bool>>>();
            }
            for (auto& p : room) {
                p->set_value(false);
            }
            s.stopped.close();
            s.doorbell.close();
            (void)s.conn.close();
        }

        // The rest of a write the caller began, then what came meanwhile
        inline async::task<> nats_finish(tracked_ptr<NatsState> s, async::task<expected<size_t, io::error>> rest) noexcept {
            auto w = co_await std::move(rest);
            if (!w) {
                nats_end(*s, net::detail::fail(w).error());
                co_return;
            }
            bool more;
            {
                std::lock_guard g(s->lock);
                more = !s->outbox.empty();
                if (!more) {
                    s->flushing = false;
                }
            }
            if (more) {
                (void)s->doorbell.try_send(true);
            }
        }

        // The bytes written in the order of the calls. A publication
        // (buffered) is appended to the outbox and the flusher rung, which
        // writes everything appended meanwhile in one piece: a stream of
        // publications costs an append each, as NATS's clients buffer. The
        // rest (a request, a reply, a subscription, a PING) is written by
        // the caller itself when no write is in progress, with no hop to
        // the flusher, and appended behind the writes in progress otherwise.
        // Past the outbox's limit, a promise the caller waits on until the
        // flusher made room.
        inline expected<tracked_ptr<async::promise<bool>>, io::error> nats_send(const tracked_ptr<NatsState>& sp, std::string_view bytes, bool buffered = false) {
            NatsState& s = *sp;
            tracked_ptr<async::promise<bool>> wait;
            bool ring = false;
            {
                std::lock_guard g(s.lock);
                if (s.end) {
                    return unexpected(*s.end);
                }
                if (s.flushing || buffered) {
                    s.outbox.append(bytes.data(), bytes.size());
                    ring = !s.flushing;
                    s.flushing = true;
                    if (s.outbox.size() > NatsOutboxLimit) {
                        wait = make_tracked<async::promise<bool>>();
                        s.room.push_back(wait);
                    }
                    if (!ring) {
                        return wait;   // behind the write in progress
                    }
                } else {
                    s.flushing = true;   // this caller writes
                }
            }
            if (ring) {
                (void)s.doorbell.try_send(true);
                return wait;
            }
            if (s.timeout > duration::zero()) {
                s.conn.set_write_deadline(sgcl::clock::now() + s.timeout);
            }
            auto st = net::detail::ConnectionAccess::impl(s.conn).start_write(slice<const byte>(reinterpret_cast<const byte*>(bytes.data()), bytes.size()));
            if (st.rest) {
                async::go(nats_finish(sp, std::move(*st.rest)));   // the socket took a part: the rest by a task
                return wait;
            }
            if (!st.done) {
                auto e = net::detail::fail(st.done).error();
                nats_end(s, e);
                return unexpected(e);
            }
            bool more;
            {
                std::lock_guard g(s.lock);
                more = !s.outbox.empty();
                if (!more) {
                    s.flushing = false;
                }
            }
            if (more) {
                (void)s.doorbell.try_send(true);   // what others appended meanwhile: the flusher's
            }
            return wait;
        }

        inline async::task<expected<void, io::error>> nats_write(tracked_ptr<NatsState> s, std::string bytes) noexcept {
            auto r = nats_send(s, bytes);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (*r && !co_await **r) {
                co_return unexpected(nats_ended(*s));
            }
            co_return expected<void, io::error>();
        }

        // The flusher: the outbox written, in one write each time it is
        // taken, until it is empty
        inline async::task<> nats_flusher(tracked_ptr<NatsState> s) noexcept {
            std::string out;
            for (;;) {
                auto rung = co_await s->doorbell.receive();
                if (!rung) {
                    co_return;
                }
                for (;;) {
                    vector<tracked_ptr<async::promise<bool>>> room;
                    {
                        std::lock_guard g(s->lock);
                        out.clear();
                        out.swap(s->outbox);
                        if (out.empty()) {
                            s->flushing = false;
                        }
                        room = std::move(s->room);
                        s->room = vector<tracked_ptr<async::promise<bool>>>();
                    }
                    for (auto& p : room) {
                        p->set_value(true);
                    }
                    if (out.empty()) {
                        break;
                    }
                    if (s->timeout > duration::zero()) {
                        s->conn.set_write_deadline(sgcl::clock::now() + s->timeout);
                    }
                    auto st = net::detail::ConnectionAccess::impl(s->conn).start_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
                    expected<size_t, io::error> w = std::move(st.done);
                    if (st.rest) {
                        w = co_await std::move(*st.rest);
                    }
                    if (!w) {
                        nats_end(*s, net::detail::fail(w).error());
                        co_return;
                    }
                    if (out.capacity() > (size_t(1) << 20)) {
                        out = std::string();   // a burst's buffer not kept
                    }
                }
            }
        }

        // The text of an -ERR as its error: the kinds the server names
        inline io::error nats_server_error(std::string_view text) {
            if (text.size() >= 2 && text.front() == '\'' && text.back() == '\'') {
                text = text.substr(1, text.size() - 2);
            }
            auto has = [&](std::string_view w) {
                for (size_t i = 0; i + w.size() <= text.size(); ++i) {
                    bool same = true;
                    for (size_t k = 0; k < w.size() && same; ++k) {
                        char a = text[i + k], b = w[k];
                        same = (a | 0x20) == (b | 0x20);
                    }
                    if (same) {
                        return true;
                    }
                }
                return false;
            };
            errc e = errc::server_error;
            if (has("authorization violation") || has("authentication timeout") || has("authentication expired") || has("authentication revoked")) {
                e = errc::authorization_violation;
            } else if (has("permissions violation")) {
                e = errc::permissions_violation;
            } else if (has("maximum payload")) {
                e = errc::max_payload;
            } else if (has("slow consumer")) {
                e = errc::slow_consumer;
            } else if (has("invalid subject")) {
                e = errc::invalid_subject;
            }
            return nats_error(e, "nats", string(text));
        }

        // A message for the subscription of its sid, or the request of its
        // inbox token
        inline void nats_deliver(NatsState& s, int64_t sid, message m) {
            tracked_ptr<NatsRequest> req;
            tracked_ptr<NatsSubState> sub;
            bool last = false;
            {
                std::lock_guard g(s.lock);
                if (sid == s.inbox_sid) {
                    std::string_view subj = m.subject.view();
                    if (subj.size() > s.inbox.size()) {
                        auto it = s.requests.find(string(subj.substr(s.inbox.size())));
                        if (it != s.requests.end()) {
                            req = it->second;
                            s.requests.erase(it);
                        }
                    }
                } else {
                    auto it = s.subs.find(sid);
                    if (it != s.subs.end()) {
                        sub = it->second;
                        ++sub->delivered;
                        if (sub->max && sub->delivered >= sub->max) {
                            s.subs.erase(it);
                            last = true;
                        }
                    }
                }
            }
            if (req) {
                req->reply = std::move(m);
                req->done.set_value(true);
                return;
            }
            if (sub) {
                if (!sub->incoming.try_send(std::move(m))) {
                    sub->dropped.fetch_add(1, std::memory_order_relaxed);   // a slow consumer: the message dropped, as NATS's clients do
                }
                if (last) {
                    sub->end = io::error(io::errc::closed, "nats subscription", sub->subject);
                    sub->incoming.close();
                }
            }
        }

        // The reader: the server's units off the connection (nats_parse)
        inline async::task<> nats_read_loop(tracked_ptr<NatsState> s, std::string buf) noexcept {
            io::error end(io::errc::closed, "nats", s->server);
            auto& impl = net::detail::ConnectionAccess::impl(s->conn);
            size_t at = 0;
            NatsFrame f;
            for (;;) {
                size_t limit;
                {
                    std::lock_guard g(s->lock);
                    limit = s->max_payload;
                }
                const char* why = nullptr;
                long used = nats_parse(std::string_view(buf).substr(at), limit, f, why);
                if (used < 0) {
                    end = nats_error(errc::malformed, "nats", string(why));
                    break;
                }
                if (used == 0) {
                    if (at) {
                        buf.erase(0, at);
                        at = 0;
                    }
                    expected<size_t, io::error> r = size_t(0);
                    for (;;) {
                        bool slow = false;
                        auto t = impl.try_read(slice<byte>(s->block->data(), s->block->size()), slow);
                        if (slow) {
                            r = co_await s->conn.async_read(slice<byte>(s->block->data(), s->block->size()));
                            break;
                        }
                        if (!t) {
                            r = net::detail::fail(t);
                            break;
                        }
                        if (*t) {
                            r = **t;
                            break;
                        }
                        if (auto ready = co_await impl.raw_readable(); !ready) {
                            r = net::detail::fail(ready);
                            break;
                        }
                    }
                    if (!r) {
                        end = r.error();
                        break;
                    }
                    if (*r == 0) {
                        end = io::error(io::errc::unexpected_eof, "nats", s->server);
                        break;
                    }
                    buf.append(reinterpret_cast<const char*>(s->block->data()), *r);
                    continue;
                }
                at += size_t(used);
                switch (f.k) {
                    case NatsFrame::kind::msg:
                        nats_deliver(*s, int64_t(f.sid), std::move(f.m));
                        continue;
                    case NatsFrame::kind::ping:
                        (void)co_await nats_write(s, std::string("PONG\r\n"));
                        continue;
                    case NatsFrame::kind::pong: {
                        tracked_ptr<async::promise<bool>> p;
                        {
                            std::lock_guard g(s->lock);
                            s->pings_out = 0;
                            if (!s->pongs.empty()) {
                                p = s->pongs[0];
                                s->pongs.erase(s->pongs.begin());
                            }
                        }
                        if (p) {
                            p->set_value(true);
                        }
                        continue;
                    }
                    case NatsFrame::kind::ok:
                        continue;
                    case NatsFrame::kind::info: {
                        auto info = encoding::json::parse(string(f.text));
                        if (info) {
                            if (auto mp = (*info)["max_payload"].as_int(); mp && *mp > 0) {
                                std::lock_guard g(s->lock);
                                s->max_payload = size_t(*mp);
                            }
                        }
                        continue;
                    }
                    case NatsFrame::kind::err:
                        break;
                }
                io::error e = nats_server_error(f.text);
                if (e.code() == errc::permissions_violation) {
                    // a subscription refused: its own end; a publication refused: nothing to tell
                    std::string_view text = e.path().view();
                    size_t q = text.find("Subscription to \"");
                    if (q != std::string_view::npos) {
                        std::string_view subj = text.substr(q + 17);
                        subj = subj.substr(0, subj.find('"'));
                        vector<tracked_ptr<NatsSubState>> refused;
                        {
                            std::lock_guard g(s->lock);
                            for (auto it = s->subs.begin(); it != s->subs.end();) {
                                if (it->second->subject.view() == subj) {
                                    refused.push_back(it->second);
                                    it = s->subs.erase(it);
                                } else {
                                    ++it;
                                }
                            }
                        }
                        for (auto& sub : refused) {
                            sub->end = e;
                            sub->incoming.close();
                        }
                    }
                    continue;
                }
                end = e;   // the server closes the connection after any other -ERR
                break;
            }
            nats_end(*s, end);
        }

        // PING every interval; past max_pings_out unanswered, the connection
        // is stale and ends
        inline async::task<> nats_pinger(tracked_ptr<NatsState> s) noexcept {
            for (;;) {
                bool ended = false;
                co_await async::select(s->stopped.on_receive([&](optional<bool>) { ended = true; }), async::timeout(s->ping_interval, [] {}));
                if (ended || s->closed.load()) {
                    co_return;
                }
                bool stale = false;
                {
                    std::lock_guard g(s->lock);
                    stale = ++s->pings_out > s->max_pings_out;
                }
                if (stale) {
                    nats_end(*s, nats_error(errc::server_error, "nats", string("stale connection")));
                    co_return;
                }
                if (!co_await nats_write(s, std::string("PING\r\n"))) {
                    co_return;
                }
            }
        }
    }

    // A subscription's messages, in the order the server sent them;
    // receive waits for the next. A handle of one word.
    class subscription {
    public:
        subscription() noexcept = default;

        // `receive()` on this thread, `co_await async_receive()` in a task
        expected<message, io::error> receive() const {
            return async_receive().wait();
        }

        async::task<expected<message, io::error>> async_receive() const noexcept {
            return _co_receive(_s);
        }

        optional<message> try_receive() const {
            return _s->incoming.try_receive();
        }

        string subject() const {
            return _s->subject;
        }

        string queue_group() const {
            return _s->queue;
        }

        // The messages dropped because the queue was full (a slow consumer)
        uint64_t dropped() const noexcept {
            return _s->dropped.load(std::memory_order_relaxed);
        }

        // UNSUB: no more messages, or after max more of them; those
        // received already stay to be read
        // `unsubscribe(...)` on this thread, `co_await async_unsubscribe(...)` in a task
        expected<void, io::error> unsubscribe(uint64_t max = 0) const {
            return async_unsubscribe(max).wait();
        }

        async::task<expected<void, io::error>> async_unsubscribe(uint64_t max = 0) const noexcept {
            return _co_unsubscribe(_c, _s, max);
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const subscription& a, const subscription& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend class client;

        subscription(tracked_ptr<detail::NatsSubState> s, tracked_ptr<detail::NatsState> c) noexcept
        : _s(std::move(s)), _c(std::move(c)) {
        }

        static async::task<expected<message, io::error>> _co_receive(tracked_ptr<detail::NatsSubState> s) noexcept {
            auto m = co_await s->incoming.receive();
            if (!m) {
                co_return unexpected(s->end ? *s->end : io::error(io::errc::closed, "nats subscription", s->subject));
            }
            co_return std::move(*m);
        }

        static async::task<expected<void, io::error>> _co_unsubscribe(tracked_ptr<detail::NatsState> c, tracked_ptr<detail::NatsSubState> s, uint64_t max) noexcept {
            using namespace detail;
            bool gone = false;
            {
                std::lock_guard g(c->lock);
                auto it = c->subs.find(s->sid);
                if (it == c->subs.end()) {
                    gone = true;
                } else if (max == 0 || s->delivered >= max) {
                    c->subs.erase(it);
                    max = 0;
                } else {
                    s->max = max;
                }
            }
            if (gone) {
                co_return unexpected(s->end ? *s->end : io::error(io::errc::closed, "nats unsubscribe", s->subject));
            }
            std::string out = "UNSUB " + std::to_string(s->sid);
            if (max) {
                out += ' ';
                out += std::to_string(max);
            }
            out += "\r\n";
            auto w = co_await nats_write(c, std::move(out));
            if (max == 0) {
                s->end = io::error(io::errc::closed, "nats subscription", s->subject);
                s->incoming.close();
            }
            co_return w;
        }

        tracked_ptr<detail::NatsSubState> _s;
        tracked_ptr<detail::NatsState> _c;
    };

    // A connection to a NATS server: publish, subscribe, request. A handle
    // of one word: a copy is the same connection; every call may come from
    // any task at once.
    //
    //     auto nc = net::nats::client::connect("nats://localhost:4222").value();
    //     auto sub = nc.subscribe("orders.>").value();
    //     nc.publish("orders.new", "{\"id\":1}");
    //     auto m = sub.receive().value();
    class client {
    public:
        struct options {
            string user;                                    // with password; empty: the URL's
            string password;
            string token;                                   // auth_token; empty: the URL's user alone when it has no password
            string nkey_seed;                               // a user's seed "SU...": the server's nonce signed with it
            string jwt;                                     // a user JWT beside the nkey (decentralized authentication)
            string name;                                    // the connection's name the server shows
            net::tls::config tls;                           // tls:// and a server that requires TLS; the server's name the URL's host when none is set
            duration ping_interval = std::chrono::minutes(2);
            int max_pings_out = 2;                          // PINGs unanswered before the connection counts as stale
            duration timeout = std::chrono::seconds(30);    // the dial, TLS and CONNECT, then flush's wait
            size_t queue = 65536;                           // messages kept per subscription until received; past it they are dropped
            async::stop_token stop;                         // the connect cancelled
        };

        client() noexcept = default;

        // A connection to the server of the URL: nats://[user:password@]host[:4222],
        // nats://token@host, tls://host[:4222], or host[:port]
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& url) {
            return async_connect(url, options()).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string url) noexcept {
            return _co_connect(std::move(url), net::connection(), options());
        }

        static expected<client, io::error> connect(const string& url, const options& o) {
            return async_connect(url, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept {
            return _co_connect(std::move(url), net::connection(), std::move(o));
        }

        // The same over a connection there is
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            return _co_connect(string(), std::move(transport), std::move(o));
        }

        // PUB: the data to the subject, written and gone (NATS's core is at
        // most once). errc::invalid_subject for a subject that may not be
        // published to, errc::max_payload past the server's limit.
        // `publish(...)` on this thread, `co_await async_publish(...)` in a task
        expected<void, io::error> publish(const string& subject, const string& data) const {
            return async_publish(message(subject, data)).wait();
        }

        async::task<expected<void, io::error>> async_publish(const string& subject, const string& data) const noexcept {
            return _co_publish(_s, message(subject, data));
        }

        // The same of a whole message: its reply subject and its headers (HPUB)
        expected<void, io::error> publish(const message& m) const {
            return async_publish(m).wait();
        }

        async::task<expected<void, io::error>> async_publish(const message& m) const noexcept {
            return _co_publish(_s, m);
        }

        // SUB: the messages of the subject ("*" a token, ">" the rest); with
        // a queue group, each message to one member of the group
        // `subscribe(...)` on this thread, `co_await async_subscribe(...)` in a task
        expected<subscription, io::error> subscribe(const string& subject, const string& queue_group = {}) const {
            return async_subscribe(subject, queue_group).wait();
        }

        async::task<expected<subscription, io::error>> async_subscribe(string subject, string queue_group = {}) const noexcept {
            return _co_subscribe(_s, std::move(subject), std::move(queue_group));
        }

        // A request and its first reply: the data published with a reply
        // subject under this client's inbox. errc::no_responders when no
        // one subscribes to the subject (servers of headers), ETIMEDOUT
        // past the timeout.
        // `request(...)` on this thread, `co_await async_request(...)` in a task
        expected<message, io::error> request(const string& subject, const string& data, duration timeout = std::chrono::seconds(2)) const {
            return async_request(message(subject, data), timeout).wait();
        }

        async::task<expected<message, io::error>> async_request(const string& subject, const string& data, duration timeout = std::chrono::seconds(2)) const noexcept {
            return _co_request(_s, message(subject, data), timeout);
        }

        expected<message, io::error> request(const message& m, duration timeout = std::chrono::seconds(2)) const {
            return async_request(m, timeout).wait();
        }

        async::task<expected<message, io::error>> async_request(const message& m, duration timeout = std::chrono::seconds(2)) const noexcept {
            return _co_request(_s, m, timeout);
        }

        // A reply to a message received: the data to its reply subject
        // `respond(...)` on this thread, `co_await async_respond(...)` in a task
        expected<void, io::error> respond(const message& to, const string& data) const {
            return async_respond(to, data).wait();
        }

        async::task<expected<void, io::error>> async_respond(const message& to, const string& data) const noexcept {
            if (to.reply.empty()) {
                return _co_fail(detail::nats_error(errc::invalid_subject, "nats respond", string("a message without a reply subject")));
            }
            return _co_publish(_s, message(to.reply, data), false);   // a reply waited for: written at once
        }

        // PING, and its PONG waited for: everything written before it was read
        // by the server
        // `flush()` on this thread, `co_await async_flush()` in a task
        expected<void, io::error> flush() const {
            return async_flush().wait();
        }

        async::task<expected<void, io::error>> async_flush() const noexcept {
            return _co_flush(_s);
        }

        // Every subscription unsubscribed, what the server sent until then
        // left to be received, the connection closed
        // `drain()` on this thread, `co_await async_drain()` in a task
        expected<void, io::error> drain() const {
            return async_drain().wait();
        }

        async::task<expected<void, io::error>> async_drain() const noexcept {
            return _co_drain(_s);
        }

        // The connection closed at once
        expected<void, io::error> close() const noexcept {
            auto r = _s->conn.close();
            detail::nats_end(*_s, io::error(io::errc::closed, "nats", _s->server));
            return r;
        }

        // The server's id and the largest message it takes (INFO)
        string server_id() const {
            std::lock_guard g(_s->lock);
            return _s->server_id;
        }

        size_t max_payload() const noexcept {
            std::lock_guard g(_s->lock);
            return _s->max_payload;
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        explicit client(tracked_ptr<detail::NatsState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<expected<void, io::error>> _co_fail(io::error e) noexcept {
            co_return unexpected(std::move(e));
        }

        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            auto t = to.token();
            co_await async::select(from.on_stop([&] { to.request_stop(); }), t.on_stop([] {}));
        }

        static async::task<expected<void, io::error>> _co_publish(tracked_ptr<detail::NatsState> s, message m, bool buffered = true) noexcept {
            using namespace detail;
            if (!nats_subject_valid(m.subject.view(), false) || (!m.reply.empty() && !nats_subject_valid(m.reply.view(), false))) {
                co_return unexpected(nats_error(errc::invalid_subject, "nats publish", m.subject));
            }
            bool headers = !m.headers.empty() || m.status;
            size_t limit;
            bool server_headers;
            {
                std::lock_guard g(s->lock);
                limit = s->max_payload;
                server_headers = s->headers;
            }
            if (headers && (!server_headers || !nats_header_valid(m))) {
                co_return unexpected(nats_error(server_headers ? errc::malformed : errc::server_error, "nats publish",
                                                string(server_headers ? "a header with CR, LF or a ':' in its name" : "the server takes no headers")));
            }
            std::string out;
            out.reserve(m.data.size() + m.subject.size() + 32);
            nats_write_pub(out, m);
            if (m.data.size() > limit) {
                co_return unexpected(nats_error(errc::max_payload, "nats publish", m.subject));
            }
            auto r = nats_send(s, out, buffered);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (*r && !co_await **r) {   // the outbox past its limit: until the flusher made room
                co_return unexpected(nats_ended(*s));
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<subscription, io::error>> _co_subscribe(tracked_ptr<detail::NatsState> s, string subject, string queue) noexcept {
            using namespace detail;
            if (!nats_subject_valid(subject.view(), true) || (!queue.empty() && !nats_subject_valid(queue.view(), false))) {
                co_return unexpected(nats_error(errc::invalid_subject, "nats subscribe", subject));
            }
            tracked_ptr sub = make_tracked<NatsSubState>(s->queue);
            sub->subject = subject;
            sub->queue = queue;
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                sub->sid = ++s->next_sid;
                s->subs[sub->sid] = sub;
            }
            std::string out = "SUB ";
            out.append(subject.view());
            if (!queue.empty()) {
                out += ' ';
                out.append(queue.view());
            }
            out += ' ';
            out += std::to_string(sub->sid);
            out += "\r\n";
            if (auto w = co_await nats_write(s, std::move(out)); !w) {
                co_return unexpected(w.error());
            }
            co_return subscription(sub, s);
        }

        static async::task<expected<message, io::error>> _co_request(tracked_ptr<detail::NatsState> s, message m, duration timeout) noexcept {
            using namespace detail;
            tracked_ptr req = make_tracked<NatsRequest>();
            string token;
            bool subscribe = false;
            int64_t inbox_sid = 0;
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                if (s->inbox_sid == 0) {   // the one inbox subscription, at the first request
                    s->inbox_sid = ++s->next_sid;
                    subscribe = true;
                }
                inbox_sid = s->inbox_sid;
                token = string(std::to_string(++s->next_token));
                s->requests[token] = req;
            }
            std::string out;
            if (subscribe) {
                out = "SUB ";
                out.append(s->inbox.view());
                out += "* ";
                out += std::to_string(inbox_sid);
                out += "\r\n";
            }
            m.reply = string::concat(s->inbox, token);
            if (!nats_subject_valid(m.subject.view(), false)) {
                std::lock_guard g(s->lock);
                s->requests.erase(token);
                co_return unexpected(nats_error(errc::invalid_subject, "nats request", m.subject));
            }
            nats_write_pub(out, m);
            if (auto w = co_await nats_write(s, std::move(out)); !w) {
                co_return unexpected(w.error());
            }
            bool late = false;
            if (timeout > duration::zero()) {
                co_await async::select(req->done.on_done([] {}), async::timeout(timeout, [&] { late = true; }));
            } else {
                (void)co_await req->done;
            }
            if (late && !req->done.done()) {
                std::lock_guard g(s->lock);
                s->requests.erase(token);
                co_return unexpected(io::error(error_code(ETIMEDOUT, std::system_category()), "nats request", m.subject));
            }
            if (!req->done.result()) {
                co_return unexpected(nats_ended(*s));
            }
            if (req->reply.status == 503 && req->reply.data.empty()) {
                co_return unexpected(nats_error(errc::no_responders, "nats request", m.subject));
            }
            co_return std::move(req->reply);
        }

        static async::task<expected<void, io::error>> _co_flush(tracked_ptr<detail::NatsState> s) noexcept {
            using namespace detail;
            tracked_ptr p = make_tracked<async::promise<bool>>();
            {
                // the promise registered and the PING sent under one lock: the PONGs come in the order of the promises
                std::lock_guard g(s->flush_lock);
                {
                    std::lock_guard g2(s->lock);
                    if (s->end) {
                        co_return unexpected(*s->end);
                    }
                    s->pongs.push_back(p);
                }
                auto r = nats_send(s, "PING\r\n");
                if (!r) {
                    co_return unexpected(r.error());
                }
            }
            bool late = false;
            if (s->timeout > duration::zero()) {
                co_await async::select(p->on_done([] {}), async::timeout(s->timeout, [&] { late = true; }));
            } else {
                (void)co_await *p;
            }
            if (late && !p->done()) {
                co_return unexpected(io::error(error_code(ETIMEDOUT, std::system_category()), "nats flush", s->server));
            }
            if (!p->result()) {
                co_return unexpected(nats_ended(*s));
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_drain(tracked_ptr<detail::NatsState> s) noexcept {
            using namespace detail;
            vector<tracked_ptr<NatsSubState>> subs;
            std::string out;
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                for (auto& [sid, sub] : s->subs) {
                    subs.push_back(sub);
                    out += "UNSUB " + std::to_string(sid) + "\r\n";
                }
            }
            if (!out.empty()) {
                if (auto w = co_await nats_write(s, std::move(out)); !w) {
                    co_return w;
                }
            }
            auto f = co_await _co_flush(s);   // the server read the UNSUBs; what it sent before is here
            {
                std::lock_guard g(s->lock);
                for (auto& sub : subs) {
                    s->subs.erase(sub->sid);
                }
            }
            for (auto& sub : subs) {
                sub->end = io::error(io::errc::closed, "nats subscription", sub->subject);
                sub->incoming.close();
            }
            nats_end(*s, io::error(io::errc::closed, "nats", s->server));
            co_return f;
        }

        // A line of the handshake read whole
        static async::task<expected<std::string, io::error>> _line(net::connection& c, std::string& buf) noexcept {
            char block[4096];
            for (;;) {
                size_t eol = buf.find("\r\n");
                if (eol != std::string::npos) {
                    std::string line = buf.substr(0, eol);
                    buf.erase(0, eol + 2);
                    co_return line;
                }
                if (buf.size() > 64 * 1024) {
                    co_return unexpected(detail::nats_error(errc::malformed, "nats connect", string("a line past its limit")));
                }
                auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block), sizeof block));
                if (!r) {
                    co_return unexpected(r.error());
                }
                if (*r == 0) {
                    co_return unexpected(io::error(io::errc::unexpected_eof, "nats connect", string()));
                }
                buf.append(block, *r);
            }
        }

        static async::task<expected<client, io::error>> _co_connect(string url, net::connection transport, options o) noexcept {
            using namespace detail;
            std::string host;
            uint16_t port = 4222;
            bool tls_scheme = false;
            if (!transport) {
                std::string text(url.view());
                if (text.find("://") == std::string::npos) {
                    text = "nats://" + text;
                }
                auto u = net::url::parse(string(text));
                if (!u || u->hostname().empty()) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_url, "nats", url));
                }
                std::string scheme(u->scheme().view());
                tls_scheme = scheme == "tls";
                if (!tls_scheme && scheme != "nats") {
                    co_return unexpected(net::detail::net_error(net::errc::unsupported_scheme, "nats", url));
                }
                std::string user = net::detail::url_unescape(u->username().view());
                std::string password = net::detail::url_unescape(u->password().view());
                if (o.user.empty() && o.token.empty() && !user.empty()) {
                    if (password.empty()) {
                        o.token = string(user);
                    } else {
                        o.user = string(user);
                        o.password = string(password);
                    }
                }
                host = std::string(u->hostname().view());
                port = u->port() ? *u->port() : 4222;
                std::string address = (host.find(':') != std::string::npos ? "[" + host + "]" : host) + ":" + std::to_string(port);
                async::stop_source dial_stop;
                if (o.timeout > duration::zero()) {
                    dial_stop.stop_after(o.timeout);
                }
                if (o.stop.stop_possible()) {
                    async::go(_link_stop(o.stop, dial_stop));
                }
                auto t = co_await net::tcp::async_connect(string(address), dial_stop.token());
                dial_stop.request_stop();
                if (!t) {
                    co_return unexpected(t.error());
                }
                transport = *t;
            }
            if (o.timeout > duration::zero()) {
                transport.set_deadline(sgcl::clock::now() + o.timeout);
            }
            auto fail = [&](io::error e) -> expected<client, io::error> {
                (void)transport.close();
                return unexpected(std::move(e));
            };
            std::string buf;
            auto first = co_await _line(transport, buf);
            if (!first) {
                co_return fail(first.error());
            }
            if (first->size() < 5 || first->compare(0, 5, "INFO ") != 0) {
                co_return fail(nats_error(errc::malformed, "nats connect", string("a server that sends no INFO")));
            }
            auto info = encoding::json::parse(string(std::string_view(*first).substr(5)));
            if (!info || !info->is_object()) {
                co_return fail(nats_error(errc::malformed, "nats connect", string("an INFO that does not read")));
            }
            tracked_ptr s = make_tracked<NatsState>();
            s->server_id = (*info)["server_id"].as_string(string());
            if (auto mp = (*info)["max_payload"].as_int(); mp && *mp > 0) {
                s->max_payload = size_t(*mp);
            }
            s->headers = (*info)["headers"].as_bool().value_or(false);
            bool tls_required = (*info)["tls_required"].as_bool().value_or(false);
            if (tls_scheme || tls_required) {
                if (!buf.empty()) {
                    co_return fail(nats_error(errc::malformed, "nats connect", string("data after INFO before TLS")));
                }
                net::tls::config tc = o.tls;
                if (tc.server_name.empty()) {
                    tc.server_name = string(host);
                }
                if (o.timeout > duration::zero()) {
                    tc.handshake_timeout = o.timeout;
                }
                transport.set_deadline(time_point());
                auto tt = co_await net::tls::async_client(transport, tc);
                if (!tt) {
                    co_return fail(tt.error());
                }
                transport = *tt;
                if (o.timeout > duration::zero()) {
                    transport.set_deadline(sgcl::clock::now() + o.timeout);
                }
            }
            // CONNECT
            using encoding::json;
            json connect = json::object({{string("verbose"), json(false)},
                                         {string("pedantic"), json(false)},
                                         {string("tls_required"), json(tls_scheme || tls_required)},
                                         {string("lang"), json("cpp")},
                                         {string("version"), json("sgcl")},
                                         {string("protocol"), json(1.0)},
                                         {string("headers"), json(true)},
                                         {string("no_responders"), json(s->headers)}});
            if (!o.name.empty()) {
                connect = connect.set(string("name"), json(o.name));
            }
            if (!o.user.empty()) {
                connect = connect.set(string("user"), json(o.user)).set(string("pass"), json(o.password));
            }
            if (!o.token.empty()) {
                connect = connect.set(string("auth_token"), json(o.token));
            }
            if (!o.jwt.empty()) {
                connect = connect.set(string("jwt"), json(o.jwt));
            }
            string nonce = (*info)["nonce"].as_string(string());
            if (!o.nkey_seed.empty()) {
                string pub, sig;
                if (!nats_sign_nonce(o.nkey_seed.view(), nonce.view(), pub, sig)) {
                    co_return fail(nats_error(errc::authorization_violation, "nats connect", string("an nkey seed that does not read")));
                }
                connect = connect.set(string("nkey"), json(pub)).set(string("sig"), json(sig));
            }
            std::string out = "CONNECT ";
            out.append(connect.to_string().view());
            out += "\r\nPING\r\n";
            if (auto w = co_await transport.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size())); !w) {
                co_return fail(w.error());
            }
            for (;;) {
                auto line = co_await _line(transport, buf);
                if (!line) {
                    co_return fail(line.error());
                }
                std::string_view l = *line;
                if (l == "PONG") {
                    break;
                }
                if (l.rfind("-ERR", 0) == 0) {
                    co_return fail(nats_server_error(l.substr(l.size() > 5 ? 5 : l.size())));
                }
                if (l == "+OK" || l.rfind("INFO ", 0) == 0 || l == "PING") {
                    continue;
                }
                co_return fail(nats_error(errc::malformed, "nats connect", string("an unexpected line")));
            }
            transport.set_deadline(time_point());
            s->conn = transport;
            s->server = transport.remote_endpoint().to_string();
            s->timeout = o.timeout;
            s->ping_interval = o.ping_interval;
            s->max_pings_out = o.max_pings_out;
            s->queue = o.queue;
            {
                uint8_t r[12];
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(r), sizeof r));
                static constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
                std::string inbox = "_INBOX.";
                for (uint8_t b : r) {
                    inbox += alphabet[b % 62];
                }
                inbox += '.';
                s->inbox = string(inbox);
            }
            async::go(nats_read_loop(s, std::move(buf)));
            async::go(nats_flusher(s));
            if (s->ping_interval > duration::zero()) {
                async::go(nats_pinger(s));
            }
            co_return client(s);
        }

        tracked_ptr<detail::NatsState> _s;
    };
}
