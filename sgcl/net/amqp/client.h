//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/codec.h"
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

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

// The client of AMQP 0-9-1 (RabbitMQ's protocol): a connection, its
// channels, the declarations of exchanges and queues, publications with
// publisher confirms, consumers, get, acknowledgements
namespace sgcl::net::amqp {
    class client;
    class channel;
    class consumer;

    namespace detail {
        using AmqpBlock = array<std::byte, 16384>;

        struct AmqpConnState;

        // A synchronous method waited for: the replies it takes, what came
        struct AmqpRpc {
            uint32_t want = 0;                       // the reply's method
            uint32_t also = 0;                       // a second reply it may be (get-empty beside get-ok)
            uint32_t got = 0;
            std::string args;                        // the reply's arguments
            amqp::delivery content;                  // get-ok's message
            async::promise<bool> done;               // true: the reply came; false: the channel or the connection ended
            tracked_ptr<struct AmqpConsumerState> consumer;   // consume: registered by the reader at consume-ok
        };

        struct AmqpConsumerState {
            async::channel<amqp::delivery> incoming;
            string tag;
            optional<io::error> end;                 // why the deliveries ended: a cancel, the channel's end

            explicit AmqpConsumerState(size_t queue)
            : incoming(queue ? queue : 1) {
            }
        };

        struct AmqpChannelState {
            uint16_t id = 0;
            tracked_ptr<AmqpConnState> conn;
            async::mutex rpc_lock;                   // one synchronous method at a time (0-9-1 §2.3.5.1)
            std::mutex lock;
            tracked_ptr<AmqpRpc> rpc;
            optional<io::error> end;
            map<string, tracked_ptr<AmqpConsumerState>> consumers;
            // the content being read after deliver, get-ok or return
            int content = 0;                         // 0 none, 1 deliver, 2 get-ok, 3 return
            amqp::delivery current;
            amqp::returned current_return;
            uint64_t body_size = 0;
            std::string body;                        // the body's frames until its size
            // publisher confirms
            bool confirms = false;
            uint64_t next_seq = 1;
            map<uint64_t, tracked_ptr<async::promise<bool>>> unconfirmed;
            async::channel<amqp::returned> returns{1024};
            size_t queue = 1000;
        };

        struct AmqpConnState {
            net::connection conn;
            tracked_ptr<AmqpBlock> block = make_tracked<AmqpBlock>();
            std::mutex lock;
            map<uint16_t, tracked_ptr<AmqpChannelState>> channels;
            uint16_t next_channel = 0;
            uint16_t channel_max = 2047;
            uint32_t frame_max = 131072;
            uint16_t heartbeat = 0;
            optional<io::error> end;
            std::atomic<bool> closed = {false};
            std::atomic<bool> blocked = {false};
            std::atomic<bool> wrote = {false};       // something written since the heartbeat last looked
            tracked_ptr<AmqpRpc> close_rpc;          // our connection.close waiting for close-ok
            async::channel<bool> stopped{1};         // closed at the end: the heartbeat's wake
            string server;
            duration timeout = std::chrono::seconds(30);
            size_t queue = 1000;
            // what is written: appended in the order of the calls, written by the flusher (amqp_send)
            std::string outbox;
            bool flushing = false;
            async::channel<bool> doorbell{1};
            vector<tracked_ptr<async::promise<bool>>> room;
        };

        inline constexpr size_t AmqpOutboxLimit = size_t(4) << 20;

        inline io::error amqp_ended(AmqpConnState& s) {
            std::lock_guard g(s.lock);
            return s.end ? *s.end : io::error(io::errc::closed, "amqp", s.server);
        }

        // A channel's end: its waiting method, its consumers, its unconfirmed
        // publications told
        inline void amqp_channel_end(AmqpChannelState& ch, const io::error& e) {
            tracked_ptr<AmqpRpc> rpc;
            map<string, tracked_ptr<AmqpConsumerState>> consumers;
            map<uint64_t, tracked_ptr<async::promise<bool>>> unconfirmed;
            {
                std::lock_guard g(ch.lock);
                if (!ch.end) {
                    ch.end = e;
                }
                rpc = std::move(ch.rpc);
                ch.rpc = tracked_ptr<AmqpRpc>();
                consumers = std::move(ch.consumers);
                ch.consumers = map<string, tracked_ptr<AmqpConsumerState>>();
                unconfirmed = std::move(ch.unconfirmed);
                ch.unconfirmed = map<uint64_t, tracked_ptr<async::promise<bool>>>();
            }
            if (rpc) {
                rpc->done.set_value(false);
            }
            for (auto& [tag, c] : consumers) {
                c->end = e;
                c->incoming.close();
            }
            for (auto& [seq, p] : unconfirmed) {
                p->set_value(false);
            }
            ch.returns.close();
        }

        inline void amqp_end(AmqpConnState& s, const io::error& e) {
            map<uint16_t, tracked_ptr<AmqpChannelState>> channels;
            tracked_ptr<AmqpRpc> close_rpc;
            {
                std::lock_guard g(s.lock);
                if (!s.end) {
                    s.end = e;
                }
                channels = std::move(s.channels);
                s.channels = map<uint16_t, tracked_ptr<AmqpChannelState>>();
                close_rpc = std::move(s.close_rpc);
                s.close_rpc = tracked_ptr<AmqpRpc>();
            }
            if (s.closed.exchange(true)) {
                return;
            }
            for (auto& [id, ch] : channels) {
                amqp_channel_end(*ch, e);
            }
            if (close_rpc) {
                close_rpc->done.set_value(true);
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
        inline async::task<> amqp_finish(tracked_ptr<AmqpConnState> s, async::task<expected<size_t, io::error>> rest) noexcept {
            auto w = co_await std::move(rest);
            if (!w) {
                amqp_end(*s, net::detail::fail(w).error());
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

        // Frames written whole, in the order of the calls. Publications and
        // acknowledgements (buffered) go to the outbox and the flusher is
        // rung, which writes everything appended meanwhile in one piece; the
        // rest (a method waited for) is written by the caller itself when no
        // write is in progress, and appended behind it otherwise. under_lock
        // runs under the lock that orders the writes (a confirm's number).
        // Past the outbox's limit, a promise the caller waits on until the
        // flusher made room.
        template<class F>
        inline expected<tracked_ptr<async::promise<bool>>, io::error> amqp_send(const tracked_ptr<AmqpConnState>& sp, std::string_view bytes, bool buffered,
                                                                                F&& under_lock) {
            AmqpConnState& s = *sp;
            tracked_ptr<async::promise<bool>> wait;
            bool ring = false;
            {
                std::lock_guard g(s.lock);
                if (s.end) {
                    return unexpected(*s.end);
                }
                if (auto e = under_lock()) {
                    return unexpected(std::move(*e));
                }
                s.wrote.store(true, std::memory_order_relaxed);
                if (s.flushing || buffered) {
                    s.outbox.append(bytes.data(), bytes.size());
                    ring = !s.flushing;
                    s.flushing = true;
                    if (s.outbox.size() > AmqpOutboxLimit) {
                        wait = make_tracked<async::promise<bool>>();
                        s.room.push_back(wait);
                    }
                    if (!ring) {
                        return wait;
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
                async::go(amqp_finish(sp, std::move(*st.rest)));
                return wait;
            }
            if (!st.done) {
                auto e = net::detail::fail(st.done).error();
                amqp_end(s, e);
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
                (void)s.doorbell.try_send(true);
            }
            return wait;
        }

        inline expected<tracked_ptr<async::promise<bool>>, io::error> amqp_send(const tracked_ptr<AmqpConnState>& sp, std::string_view bytes, bool buffered) {
            return amqp_send(sp, bytes, buffered, []() -> optional<io::error> { return nullopt; });
        }

        inline async::task<expected<void, io::error>> amqp_write(tracked_ptr<AmqpConnState> s, std::string bytes) noexcept {
            auto r = amqp_send(s, bytes, false);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (*r && !co_await **r) {
                co_return unexpected(amqp_ended(*s));
            }
            co_return expected<void, io::error>();
        }

        // The flusher: the outbox written, in one write each time it is
        // taken, until it is empty
        inline async::task<> amqp_flusher(tracked_ptr<AmqpConnState> s) noexcept {
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
                        amqp_end(*s, net::detail::fail(w).error());
                        co_return;
                    }
                    if (out.capacity() > (size_t(1) << 20)) {
                        out = std::string();
                    }
                }
            }
        }

        // A reply of the broker's close: its code and text
        inline io::error amqp_close_error(std::string_view args, const char* op) {
            AmqpReader r(args);
            uint16_t code = r.u16();
            std::string_view text = r.shortstr();
            if (!r.ok || code == 0) {
                return amqp_error(errc::malformed, op, string("a close that does not read"));
            }
            return amqp_error(int(code), op, string(text));
        }

        // A content complete: given to its consumer, its get or the returns.
        // A consumer whose queue is full is handed back with its delivery in
        // pending, for the reader to wait on (the only wait of a content)
        inline tracked_ptr<AmqpConsumerState> amqp_content_done(AmqpChannelState& ch, amqp::delivery& pending) {
            int kind = ch.content;
            ch.content = 0;
            tracked_ptr<AmqpConsumerState> wait;
            if (kind == 1) {
                tracked_ptr<AmqpConsumerState> c;
                {
                    std::lock_guard g(ch.lock);
                    auto it = ch.consumers.find(ch.current.consumer_tag);
                    if (it != ch.consumers.end()) {
                        c = it->second;
                    }
                }
                if (c && !c->incoming.try_send(std::move(ch.current)) && !c->incoming.closed()) {
                    pending = std::move(ch.current);
                    wait = c;
                }
            } else if (kind == 2) {
                tracked_ptr<AmqpRpc> rpc;
                {
                    std::lock_guard g(ch.lock);
                    rpc = std::move(ch.rpc);
                    ch.rpc = tracked_ptr<AmqpRpc>();
                }
                if (rpc) {
                    rpc->got = m::basic_get_ok;
                    rpc->content = std::move(ch.current);
                    rpc->done.set_value(true);
                }
            } else if (kind == 3) {
                (void)ch.returns.try_send(std::move(ch.current_return));   // past 1024 unread returns, dropped
            }
            ch.current = amqp::delivery();
            ch.current_return = amqp::returned();
            return wait;
        }

        // A method on a channel other than 0: false for one that does not
        // read or was not asked for; what to answer in reply
        inline bool amqp_channel_method(AmqpConnState& s, const tracked_ptr<AmqpChannelState>& ch, uint32_t method, std::string_view args, std::string& reply) {
            AmqpReader r(args);
            switch (method) {
                case m::channel_close: {
                    size_t at = amqp_method_begin(reply, ch->id, m::channel_close_ok);
                    amqp_frame_end(reply, at);
                    {
                        std::lock_guard g(s.lock);
                        s.channels.erase(ch->id);
                    }
                    amqp_channel_end(*ch, amqp_close_error(args, "amqp channel"));
                    return true;
                }
                case m::channel_flow: {
                    bool active = r.bit();
                    size_t at = amqp_method_begin(reply, ch->id, m::channel_flow_ok);
                    AmqpWriter w(reply);
                    w.bit(active);
                    w.finish();
                    amqp_frame_end(reply, at);
                    return true;
                }
                case m::basic_deliver: {
                    ch->current = amqp::delivery();
                    ch->current.consumer_tag = string(r.shortstr());
                    ch->current.delivery_tag = r.u64();
                    ch->current.redelivered = r.bit();
                    ch->current.exchange = string(r.shortstr());
                    ch->current.routing_key = string(r.shortstr());
                    ch->content = 1;
                    return r.ok;
                }
                case m::basic_get_ok: {
                    ch->current = amqp::delivery();
                    ch->current.delivery_tag = r.u64();
                    ch->current.redelivered = r.bit();
                    ch->current.exchange = string(r.shortstr());
                    ch->current.routing_key = string(r.shortstr());
                    ch->current.message_count = r.u32();
                    ch->content = 2;
                    return r.ok;
                }
                case m::basic_return: {
                    ch->current_return = amqp::returned();
                    ch->current_return.reply_code = r.u16();
                    ch->current_return.reply_text = string(r.shortstr());
                    ch->current_return.exchange = string(r.shortstr());
                    ch->current_return.routing_key = string(r.shortstr());
                    ch->content = 3;
                    return r.ok;
                }
                case m::basic_ack:
                case m::basic_nack: {
                    uint64_t tag = r.u64();
                    bool multiple = r.bit();
                    if (!r.ok) {
                        return false;
                    }
                    vector<tracked_ptr<async::promise<bool>>> settled;
                    {
                        std::lock_guard g(ch->lock);
                        if (multiple) {
                            for (auto it = ch->unconfirmed.begin(); it != ch->unconfirmed.end();) {
                                if (it->first <= tag) {
                                    settled.push_back(it->second);
                                    it = ch->unconfirmed.erase(it);
                                } else {
                                    ++it;
                                }
                            }
                        } else if (auto it = ch->unconfirmed.find(tag); it != ch->unconfirmed.end()) {
                            settled.push_back(it->second);
                            ch->unconfirmed.erase(it);
                        }
                    }
                    for (auto& p : settled) {
                        p->set_value(method == m::basic_ack);
                    }
                    return true;
                }
                case m::basic_cancel: {   // the broker cancelled a consumer (its queue deleted): RabbitMQ's consumer_cancel_notify
                    string tag(r.shortstr());
                    tracked_ptr<AmqpConsumerState> c;
                    {
                        std::lock_guard g(ch->lock);
                        auto it = ch->consumers.find(tag);
                        if (it != ch->consumers.end()) {
                            c = it->second;
                            ch->consumers.erase(it);
                        }
                    }
                    if (c) {
                        c->end = amqp_error(errc::not_found, "amqp consume", string("the broker cancelled the consumer"));
                        c->incoming.close();
                    }
                    return r.ok;
                }
                default: {
                    tracked_ptr<AmqpRpc> rpc;
                    {
                        std::lock_guard g(ch->lock);
                        if (ch->rpc && (ch->rpc->want == method || ch->rpc->also == method)) {
                            rpc = std::move(ch->rpc);
                            ch->rpc = tracked_ptr<AmqpRpc>();
                            if (method == m::basic_consume_ok && rpc->consumer) {
                                AmqpReader t(args);
                                rpc->consumer->tag = string(t.shortstr());
                                ch->consumers[rpc->consumer->tag] = rpc->consumer;
                            } else if (method == m::basic_cancel_ok) {
                                AmqpReader t(args);
                                string tag(t.shortstr());
                                auto it = ch->consumers.find(tag);
                                if (it != ch->consumers.end()) {
                                    it->second->incoming.close();
                                    ch->consumers.erase(it);
                                }
                            }
                        }
                    }
                    if (!rpc) {
                        return false;   // a reply nobody asked for: the broker broke the protocol
                    }
                    rpc->got = method;
                    rpc->args.assign(args.data(), args.size());
                    rpc->done.set_value(true);
                    return true;
                }
            }
        }

        // The reader: frames off the connection, each method given to its
        // channel; the end of the connection when the broker closes it, a
        // frame breaks the protocol or two heartbeats are missed
        inline async::task<> amqp_read_loop(tracked_ptr<AmqpConnState> s, std::string buf) noexcept {
            io::error end(io::errc::closed, "amqp", s->server);
            auto& impl = net::detail::ConnectionAccess::impl(s->conn);
            size_t at = 0;
            amqp::delivery pending;
            duration silence = s->heartbeat ? std::chrono::seconds(s->heartbeat * 2) : duration::zero();
            for (;;) {
                uint8_t type = 0;
                uint16_t chan = 0;
                uint32_t size = 0;
                int f = amqp_frame_head(std::string_view(buf).substr(at), type, chan, size);
                if (f > 0 && size > s->frame_max) {
                    end = amqp_error(errc::frame_error, "amqp", string("a frame past frame_max"));
                    break;
                }
                if (f == 0 || buf.size() - at < size_t(size) + 8) {
                    if (at) {
                        buf.erase(0, at);
                        at = 0;
                    }
                    if (silence > duration::zero()) {
                        s->conn.set_read_deadline(sgcl::clock::now() + silence);
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
                        end = r.error().is_timeout() ? amqp_error(errc::connection_forced, "amqp", string("missed heartbeats")) : r.error();
                        break;
                    }
                    if (*r == 0) {
                        end = io::error(io::errc::unexpected_eof, "amqp", s->server);
                        break;
                    }
                    buf.append(reinterpret_cast<const char*>(s->block->data()), *r);
                    continue;
                }
                if (uint8_t(buf[at + 7 + size]) != FrameEnd) {
                    end = amqp_error(errc::frame_error, "amqp", string("a frame without its end octet"));
                    break;
                }
                std::string_view payload(buf.data() + at + 7, size);
                at += size_t(size) + 8;
                if (type == FrameHeartbeat) {
                    continue;
                }
                if (chan == 0) {
                    if (type != FrameMethod || payload.size() < 4) {
                        end = amqp_error(errc::unexpected_frame, "amqp", string("a content frame on channel 0"));
                        break;
                    }
                    uint32_t method = uint32_t(uint8_t(payload[0])) << 24 | uint32_t(uint8_t(payload[1])) << 16 | uint32_t(uint8_t(payload[2])) << 8 | uint8_t(payload[3]);
                    std::string_view args = payload.substr(4);
                    if (method == m::connection_close) {
                        std::string out;
                        size_t a = amqp_method_begin(out, 0, m::connection_close_ok);
                        amqp_frame_end(out, a);
                        (void)co_await amqp_write(s, std::move(out));
                        end = amqp_close_error(args, "amqp");
                        break;
                    }
                    if (method == m::connection_close_ok) {
                        end = io::error(io::errc::closed, "amqp", s->server);
                        break;
                    }
                    if (method == m::connection_blocked || method == m::connection_unblocked) {
                        s->blocked.store(method == m::connection_blocked);
                        continue;
                    }
                    end = amqp_error(errc::command_invalid, "amqp", string("a method out of place on channel 0"));
                    break;
                }
                tracked_ptr<AmqpChannelState> ch;
                {
                    std::lock_guard g(s->lock);
                    auto it = s->channels.find(chan);
                    if (it != s->channels.end()) {
                        ch = it->second;
                    }
                }
                if (!ch) {
                    continue;   // a channel closed here, its last frames still coming
                }
                if (type == FrameMethod) {
                    if (payload.size() < 4 || ch->content != 0) {
                        end = amqp_error(errc::unexpected_frame, "amqp", string("a method where a content was due"));
                        break;
                    }
                    uint32_t method = uint32_t(uint8_t(payload[0])) << 24 | uint32_t(uint8_t(payload[1])) << 16 | uint32_t(uint8_t(payload[2])) << 8 | uint8_t(payload[3]);
                    std::string reply;
                    if (!amqp_channel_method(*s, ch, method, payload.substr(4), reply)) {
                        end = amqp_error(errc::malformed, "amqp", string("a method that does not read, or a reply nobody asked for"));
                        break;
                    }
                    if (!reply.empty()) {
                        (void)co_await amqp_write(s, std::move(reply));
                    }
                    continue;
                }
                if (type == FrameHeader) {
                    AmqpReader r(payload);
                    uint16_t cls = r.u16();
                    (void)r.u16();
                    uint64_t body = r.u64();
                    amqp::properties p = r.properties();
                    if (!r.ok || cls != 60 || ch->content == 0 || ch->body_size != 0) {
                        end = amqp_error(errc::unexpected_frame, "amqp", string("a content header out of place"));
                        break;
                    }
                    if (body > (uint64_t(1) << 32)) {
                        end = amqp_error(errc::content_too_large, "amqp", string("a body past 4 GB"));
                        break;
                    }
                    ch->body_size = body;
                    ch->body.clear();
                    if (ch->content == 3) {
                        ch->current_return.properties = std::move(p);
                    } else {
                        ch->current.properties = std::move(p);
                    }
                    if (body == 0) {
                        if (auto full = amqp_content_done(*ch, pending)) {
                            (void)co_await full->incoming.send(std::move(pending));   // the consumer's queue full: the reader waits
                        }
                    }
                    continue;
                }
                if (type == FrameBody) {
                    if (ch->content == 0 || ch->body_size == 0) {
                        end = amqp_error(errc::unexpected_frame, "amqp", string("a body frame out of place"));
                        break;
                    }
                    if (ch->body.size() + payload.size() > ch->body_size) {
                        end = amqp_error(errc::frame_error, "amqp", string("a body past its size"));
                        break;
                    }
                    if (ch->body.empty() && payload.size() == ch->body_size) {
                        (ch->content == 3 ? ch->current_return.body : ch->current.body) = string(payload);   // one frame: no copy between
                        ch->body_size = 0;
                        if (auto full = amqp_content_done(*ch, pending)) {
                            (void)co_await full->incoming.send(std::move(pending));   // the consumer's queue full: the reader waits
                        }
                        continue;
                    }
                    ch->body.append(payload.data(), payload.size());
                    if (ch->body.size() == ch->body_size) {
                        (ch->content == 3 ? ch->current_return.body : ch->current.body) = string(ch->body);
                        ch->body.clear();
                        ch->body_size = 0;
                        if (auto full = amqp_content_done(*ch, pending)) {
                            (void)co_await full->incoming.send(std::move(pending));   // the consumer's queue full: the reader waits
                        }
                    }
                    continue;
                }
                end = amqp_error(errc::frame_error, "amqp", string("a frame of an unknown type"));
                break;
            }
            amqp_end(*s, end);
        }

        // Heartbeats (0-9-1 §4.2.7): one when nothing was written for half
        // the interval the two sides agreed
        inline async::task<> amqp_heartbeats(tracked_ptr<AmqpConnState> s) noexcept {
            duration half = std::chrono::milliseconds(int64_t(s->heartbeat) * 500);
            for (;;) {
                bool ended = false;
                co_await async::select(s->stopped.on_receive([&](optional<bool>) { ended = true; }), async::timeout(half, [] {}));
                if (ended || s->closed.load()) {
                    co_return;
                }
                if (!s->wrote.exchange(false, std::memory_order_relaxed)) {
                    std::string hb;
                    amqp_heartbeat(hb);
                    if (!co_await amqp_write(s, std::move(hb))) {
                        co_return;
                    }
                    s->wrote.store(false, std::memory_order_relaxed);
                }
            }
        }
    }

    // A consumer's deliveries (basic.consume), in the order the broker sent
    // them; receive waits for the next. A handle of one word.
    class consumer {
    public:
        consumer() noexcept = default;

        // `receive()` on this thread, `co_await async_receive()` in a task
        expected<delivery, io::error> receive() const {
            return async_receive().wait();
        }

        async::task<expected<delivery, io::error>> async_receive() const noexcept {
            return _co_receive(_s);
        }

        // The next delivery when one is there, at once
        optional<delivery> try_receive() const {
            return _s->incoming.try_receive();
        }

        // The consumer's tag, the broker's when none was asked for
        string tag() const {
            return _s->tag;
        }

        // basic.cancel: no more deliveries; those received and not acked
        // go back to the queue when the channel closes
        // `cancel()` on this thread, `co_await async_cancel()` in a task
        expected<void, io::error> cancel() const {
            return async_cancel().wait();
        }

        async::task<expected<void, io::error>> async_cancel() const noexcept;

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const consumer& a, const consumer& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend class channel;

        consumer(tracked_ptr<detail::AmqpConsumerState> s, tracked_ptr<detail::AmqpChannelState> ch) noexcept
        : _s(std::move(s)), _ch(std::move(ch)) {
        }

        static async::task<expected<delivery, io::error>> _co_receive(tracked_ptr<detail::AmqpConsumerState> s) noexcept {
            auto d = co_await s->incoming.receive();
            if (!d) {
                co_return unexpected(s->end ? *s->end : io::error(io::errc::closed, "amqp consume", s->tag));
            }
            co_return std::move(*d);
        }

        tracked_ptr<detail::AmqpConsumerState> _s;
        tracked_ptr<detail::AmqpChannelState> _ch;
    };

    // A channel of a connection (0-9-1 §2.2.5): the declarations, the
    // publications, the consumers, the acknowledgements. A handle of one
    // word; its synchronous methods are taken one at a time. A refusal of
    // the broker's closes the channel (AMQP's rule): its error is the
    // operation's, and every later operation gets it too.
    class channel {
    public:
        channel() noexcept = default;

        // exchange.declare
        // `declare_exchange(...)` on this thread, `co_await async_declare_exchange(...)` in a task
        expected<void, io::error> declare_exchange(const string& name, const exchange_options& o = {}) const {
            return async_declare_exchange(name, o).wait();
        }

        async::task<expected<void, io::error>> async_declare_exchange(string name, exchange_options o = {}) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::exchange_declare);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(name.view());
            w.shortstr(o.type.view());
            w.bit(o.passive);
            w.bit(o.durable);
            w.bit(o.auto_delete);
            w.bit(o.internal);
            w.bit(false);
            w.table(o.arguments);
            amqp_frame_end(out, at);
            return _co_void(_s, std::move(out), m::exchange_declare_ok, "amqp declare_exchange");
        }

        // exchange.delete
        // `delete_exchange(...)` on this thread, `co_await async_delete_exchange(...)` in a task
        expected<void, io::error> delete_exchange(const string& name, bool if_unused = false) const {
            return async_delete_exchange(name, if_unused).wait();
        }

        async::task<expected<void, io::error>> async_delete_exchange(string name, bool if_unused = false) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::exchange_delete);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(name.view());
            w.bit(if_unused);
            w.bit(false);
            w.finish();
            amqp_frame_end(out, at);
            return _co_void(_s, std::move(out), m::exchange_delete_ok, "amqp delete_exchange");
        }

        // queue.declare: a queue of the name, the broker's own name for an
        // empty one (exclusive then, as a reply queue is)
        // `declare_queue(...)` on this thread, `co_await async_declare_queue(...)` in a task
        expected<queue_info, io::error> declare_queue(const string& name = {}, const queue_options& o = {}) const {
            return async_declare_queue(name, o).wait();
        }

        async::task<expected<queue_info, io::error>> async_declare_queue(string name = {}, queue_options o = {}) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::queue_declare);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(name.view());
            w.bit(o.passive);
            w.bit(o.durable);
            w.bit(o.exclusive);
            w.bit(o.auto_delete);
            w.bit(false);
            w.table(o.arguments);
            amqp_frame_end(out, at);
            return _co_declare_queue(_s, std::move(out));
        }

        // queue.bind: the exchange's messages of the routing key (a topic's
        // pattern, a headers exchange's arguments) to the queue
        // `bind_queue(...)` on this thread, `co_await async_bind_queue(...)` in a task
        expected<void, io::error> bind_queue(const string& queue, const string& exchange, const string& routing_key, const table& arguments = {}) const {
            return async_bind_queue(queue, exchange, routing_key, arguments).wait();
        }

        async::task<expected<void, io::error>> async_bind_queue(string queue, string exchange, string routing_key, table arguments = {}) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::queue_bind);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.shortstr(exchange.view());
            w.shortstr(routing_key.view());
            w.bit(false);
            w.table(arguments);
            amqp_frame_end(out, at);
            return _co_void(_s, std::move(out), m::queue_bind_ok, "amqp bind_queue");
        }

        // queue.unbind
        // `unbind_queue(...)` on this thread, `co_await async_unbind_queue(...)` in a task
        expected<void, io::error> unbind_queue(const string& queue, const string& exchange, const string& routing_key, const table& arguments = {}) const {
            return async_unbind_queue(queue, exchange, routing_key, arguments).wait();
        }

        async::task<expected<void, io::error>> async_unbind_queue(string queue, string exchange, string routing_key, table arguments = {}) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::queue_unbind);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.shortstr(exchange.view());
            w.shortstr(routing_key.view());
            w.table(arguments);
            amqp_frame_end(out, at);
            return _co_void(_s, std::move(out), m::queue_unbind_ok, "amqp unbind_queue");
        }

        // queue.purge: the queue's ready messages dropped; how many
        // `purge_queue(...)` on this thread, `co_await async_purge_queue(...)` in a task
        expected<uint32_t, io::error> purge_queue(const string& queue) const {
            return async_purge_queue(queue).wait();
        }

        async::task<expected<uint32_t, io::error>> async_purge_queue(string queue) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::queue_purge);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.bit(false);
            w.finish();
            amqp_frame_end(out, at);
            return _co_count(_s, std::move(out), m::queue_purge_ok, "amqp purge_queue");
        }

        // queue.delete: the queue and its messages; how many it held
        // `delete_queue(...)` on this thread, `co_await async_delete_queue(...)` in a task
        expected<uint32_t, io::error> delete_queue(const string& queue, bool if_unused = false, bool if_empty = false) const {
            return async_delete_queue(queue, if_unused, if_empty).wait();
        }

        async::task<expected<uint32_t, io::error>> async_delete_queue(string queue, bool if_unused = false, bool if_empty = false) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::queue_delete);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.bit(if_unused);
            w.bit(if_empty);
            w.bit(false);
            w.finish();
            amqp_frame_end(out, at);
            return _co_count(_s, std::move(out), m::queue_delete_ok, "amqp delete_queue");
        }

        // basic.qos: how many unacked deliveries the broker sends this
        // channel's consumers (each of them; all together with global)
        // `qos(...)` on this thread, `co_await async_qos(...)` in a task
        expected<void, io::error> qos(uint16_t prefetch_count, bool global = false) const {
            return async_qos(prefetch_count, global).wait();
        }

        async::task<expected<void, io::error>> async_qos(uint16_t prefetch_count, bool global = false) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::basic_qos);
            AmqpWriter w(out);
            w.u32(0);
            w.u16(prefetch_count);
            w.bit(global);
            w.finish();
            amqp_frame_end(out, at);
            return _co_void(_s, std::move(out), m::basic_qos_ok, "amqp qos");
        }

        // confirm.select: from now on every publication waits for the
        // broker's ack (errc::nacked for a nack)
        // `confirm()` on this thread, `co_await async_confirm()` in a task
        expected<void, io::error> confirm() const {
            return async_confirm().wait();
        }

        async::task<expected<void, io::error>> async_confirm() const noexcept {
            return _co_confirm(_s);
        }

        // basic.publish: the message to the exchange with the routing key
        // ("" is the default exchange, whose routing key is a queue's name).
        // Written and gone, or with confirms on, waited for until the broker
        // acked it. A mandatory message no queue takes comes back to
        // receive_returned.
        // `publish(...)` on this thread, `co_await async_publish(...)` in a task
        expected<void, io::error> publish(const string& exchange, const string& routing_key, const string& body, const properties& p = {},
                                          const publish_options& o = {}) const {
            return async_publish(exchange, routing_key, body, p, o).wait();
        }

        async::task<expected<void, io::error>> async_publish(string exchange, string routing_key, string body, properties p = {},
                                                             publish_options o = {}) const noexcept {
            return _co_publish(_s, std::move(exchange), std::move(routing_key), std::move(body), std::move(p), o);
        }

        // basic.consume: the queue's messages, as they come, to a consumer
        // `consume(...)` on this thread, `co_await async_consume(...)` in a task
        expected<consumer, io::error> consume(const string& queue, const consume_options& o = {}) const {
            return async_consume(queue, o).wait();
        }

        async::task<expected<consumer, io::error>> async_consume(string queue, consume_options o = {}) const noexcept {
            return _co_consume(_s, std::move(queue), std::move(o));
        }

        // basic.get: the queue's next message, none when it is empty
        // `get(...)` on this thread, `co_await async_get(...)` in a task
        expected<optional<delivery>, io::error> get(const string& queue, bool no_ack = false) const {
            return async_get(queue, no_ack).wait();
        }

        async::task<expected<optional<delivery>, io::error>> async_get(string queue, bool no_ack = false) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::basic_get);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.bit(no_ack);
            w.finish();
            amqp_frame_end(out, at);
            return _co_get(_s, std::move(out));
        }

        // basic.ack: the delivery (and every one before it, multiple) done
        // `ack(...)` on this thread, `co_await async_ack(...)` in a task
        expected<void, io::error> ack(uint64_t delivery_tag, bool multiple = false) const {
            return async_ack(delivery_tag, multiple).wait();
        }

        async::task<expected<void, io::error>> async_ack(uint64_t delivery_tag, bool multiple = false) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::basic_ack);
            AmqpWriter w(out);
            w.u64(delivery_tag);
            w.bit(multiple);
            w.finish();
            amqp_frame_end(out, at);
            return _co_send(_s, std::move(out));
        }

        // basic.nack (RabbitMQ): the delivery refused, back to the queue
        // or dropped (dead-lettered where the queue says)
        // `nack(...)` on this thread, `co_await async_nack(...)` in a task
        expected<void, io::error> nack(uint64_t delivery_tag, bool multiple = false, bool requeue = true) const {
            return async_nack(delivery_tag, multiple, requeue).wait();
        }

        async::task<expected<void, io::error>> async_nack(uint64_t delivery_tag, bool multiple = false, bool requeue = true) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::basic_nack);
            AmqpWriter w(out);
            w.u64(delivery_tag);
            w.bit(multiple);
            w.bit(requeue);
            w.finish();
            amqp_frame_end(out, at);
            return _co_send(_s, std::move(out));
        }

        // basic.reject: one delivery refused
        // `reject(...)` on this thread, `co_await async_reject(...)` in a task
        expected<void, io::error> reject(uint64_t delivery_tag, bool requeue = true) const {
            return async_reject(delivery_tag, requeue).wait();
        }

        async::task<expected<void, io::error>> async_reject(uint64_t delivery_tag, bool requeue = true) const noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, _s->id, m::basic_reject);
            AmqpWriter w(out);
            w.u64(delivery_tag);
            w.bit(requeue);
            w.finish();
            amqp_frame_end(out, at);
            return _co_send(_s, std::move(out));
        }

        // The next publication the broker gave back (basic.return)
        // `receive_returned()` on this thread, `co_await async_receive_returned()` in a task
        expected<returned, io::error> receive_returned() const {
            return async_receive_returned().wait();
        }

        async::task<expected<returned, io::error>> async_receive_returned() const noexcept {
            return _co_returned(_s);
        }

        optional<returned> try_receive_returned() const {
            return _s->returns.try_receive();
        }

        // channel.close: the channel ended, its unacked deliveries back to
        // their queues
        // `close()` on this thread, `co_await async_close()` in a task
        expected<void, io::error> close() const {
            return async_close().wait();
        }

        async::task<expected<void, io::error>> async_close() const noexcept {
            return _co_close(_s);
        }

        // The channel's number on its connection
        uint16_t id() const noexcept {
            return _s->id;
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const channel& a, const channel& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend class client;
        friend class consumer;

        explicit channel(tracked_ptr<detail::AmqpChannelState> s) noexcept
        : _s(std::move(s)) {
        }

        static io::error _ended(detail::AmqpChannelState& ch) {
            {
                std::lock_guard g(ch.lock);
                if (ch.end) {
                    return *ch.end;
                }
            }
            return detail::amqp_ended(*ch.conn);
        }

        // A synchronous method: written, its reply waited for
        static async::task<expected<tracked_ptr<detail::AmqpRpc>, io::error>> _rpc(tracked_ptr<detail::AmqpChannelState> ch, std::string out, uint32_t want,
                                                                                   uint32_t also = 0,
                                                                                   tracked_ptr<detail::AmqpConsumerState> consumer = {}) noexcept {
            using namespace detail;
            auto guard = co_await ch->rpc_lock.scoped_lock();
            tracked_ptr rpc = make_tracked<AmqpRpc>();
            rpc->want = want;
            rpc->also = also;
            rpc->consumer = consumer;
            {
                std::lock_guard g(ch->lock);
                if (ch->end) {
                    co_return unexpected(*ch->end);
                }
                ch->rpc = rpc;
            }
            if (auto w = co_await amqp_write(ch->conn, std::move(out)); !w) {
                co_return unexpected(w.error());
            }
            bool late = false;
            duration timeout = ch->conn->timeout;
            if (timeout > duration::zero()) {
                co_await async::select(rpc->done.on_done([] {}), async::timeout(timeout, [&] { late = true; }));
            } else {
                (void)co_await rpc->done;
            }
            if (late && !rpc->done.done()) {
                // the reply may still come: the channel cannot tell it from the next one's, so it ends
                auto e = io::error(error_code(ETIMEDOUT, std::system_category()), "amqp", ch->conn->server);
                amqp_channel_end(*ch, e);
                co_return unexpected(e);
            }
            if (!rpc->done.result()) {
                co_return unexpected(_ended(*ch));
            }
            co_return rpc;
        }

        static async::task<expected<void, io::error>> _co_void(tracked_ptr<detail::AmqpChannelState> ch, std::string out, uint32_t want, const char*) noexcept {
            auto r = co_await _rpc(ch, std::move(out), want);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<uint32_t, io::error>> _co_count(tracked_ptr<detail::AmqpChannelState> ch, std::string out, uint32_t want, const char*) noexcept {
            auto r = co_await _rpc(ch, std::move(out), want);
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::AmqpReader rd((*r)->args);
            uint32_t n = rd.u32();
            if (!rd.ok) {
                co_return unexpected(detail::amqp_error(errc::malformed, "amqp", string("a reply that does not read")));
            }
            co_return n;
        }

        static async::task<expected<queue_info, io::error>> _co_declare_queue(tracked_ptr<detail::AmqpChannelState> ch, std::string out) noexcept {
            auto r = co_await _rpc(ch, std::move(out), detail::m::queue_declare_ok);
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::AmqpReader rd((*r)->args);
            queue_info q;
            q.name = string(rd.shortstr());
            q.messages = rd.u32();
            q.consumers = rd.u32();
            if (!rd.ok) {
                co_return unexpected(detail::amqp_error(errc::malformed, "amqp declare_queue", string("a reply that does not read")));
            }
            co_return q;
        }

        static async::task<expected<void, io::error>> _co_confirm(tracked_ptr<detail::AmqpChannelState> ch) noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, ch->id, m::confirm_select);
            AmqpWriter w(out);
            w.bit(false);
            w.finish();
            amqp_frame_end(out, at);
            auto r = co_await _rpc(ch, std::move(out), m::confirm_select_ok);
            if (!r) {
                co_return unexpected(r.error());
            }
            std::lock_guard g(ch->lock);
            ch->confirms = true;
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_send(tracked_ptr<detail::AmqpChannelState> ch, std::string out) noexcept {
            {
                std::lock_guard g(ch->lock);
                if (ch->end) {
                    co_return unexpected(*ch->end);
                }
            }
            auto r = detail::amqp_send(ch->conn, out, true);   // an acknowledgement: buffered beside the next ones
            if (!r) {
                co_return unexpected(r.error());
            }
            if (*r && !co_await **r) {
                co_return unexpected(detail::amqp_ended(*ch->conn));
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_publish(tracked_ptr<detail::AmqpChannelState> ch, string exchange, string routing_key, string body,
                                                                  properties p, publish_options o) noexcept {
            using namespace detail;
            if (exchange.size() > 255 || routing_key.size() > 255) {
                co_return unexpected(amqp_error(errc::syntax_error, "amqp publish", string("an exchange or a routing key past 255 bytes")));
            }
            std::string out;
            size_t at = amqp_method_begin(out, ch->id, m::basic_publish);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(exchange.view());
            w.shortstr(routing_key.view());
            w.bit(o.mandatory);
            w.bit(false);
            w.finish();
            amqp_frame_end(out, at);
            amqp_content(out, ch->id, p, body.view(), ch->conn->frame_max);
            tracked_ptr<async::promise<bool>> confirm;
            auto& s = ch->conn;
            bool confirms;
            {
                std::lock_guard g(ch->lock);
                confirms = ch->confirms;
            }
            // the confirm's number given under the lock that orders the writes: numbers in the order of the
            // frames; a publication waited for is written at once, the others beside the next ones
            auto r = amqp_send(s, out, !confirms, [&]() -> optional<io::error> {
                std::lock_guard g(ch->lock);
                if (ch->end) {
                    return *ch->end;
                }
                if (ch->confirms) {
                    confirm = make_tracked<async::promise<bool>>();
                    ch->unconfirmed[ch->next_seq++] = confirm;
                }
                return nullopt;
            });
            if (!r) {
                co_return unexpected(r.error());
            }
            if (*r && !co_await **r) {   // the outbox past its limit: until the flusher made room
                co_return unexpected(amqp_ended(*s));
            }
            if (!confirm) {
                co_return expected<void, io::error>();
            }
            bool late = false;
            if (s->timeout > duration::zero()) {
                co_await async::select(confirm->on_done([] {}), async::timeout(s->timeout, [&] { late = true; }));
            } else {
                (void)co_await *confirm;
            }
            if (late && !confirm->done()) {
                co_return unexpected(io::error(error_code(ETIMEDOUT, std::system_category()), "amqp publish", s->server));
            }
            if (!confirm->result()) {
                {
                    std::lock_guard g(ch->lock);
                    if (ch->end) {
                        co_return unexpected(*ch->end);
                    }
                }
                co_return unexpected(amqp_error(errc::nacked, "amqp publish", routing_key));
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<consumer, io::error>> _co_consume(tracked_ptr<detail::AmqpChannelState> ch, string queue, consume_options o) noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, ch->id, m::basic_consume);
            AmqpWriter w(out);
            w.u16(0);
            w.shortstr(queue.view());
            w.shortstr(o.tag.view());
            w.bit(false);
            w.bit(o.no_ack);
            w.bit(o.exclusive);
            w.bit(false);
            w.table(o.arguments);
            amqp_frame_end(out, at);
            tracked_ptr c = make_tracked<AmqpConsumerState>(ch->queue);
            auto r = co_await _rpc(ch, std::move(out), m::basic_consume_ok, 0, c);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return consumer(c, ch);
        }

        static async::task<expected<optional<delivery>, io::error>> _co_get(tracked_ptr<detail::AmqpChannelState> ch, std::string out) noexcept {
            auto r = co_await _rpc(ch, std::move(out), detail::m::basic_get_ok, detail::m::basic_get_empty);
            if (!r) {
                co_return unexpected(r.error());
            }
            if ((*r)->got == detail::m::basic_get_empty) {
                co_return optional<delivery>();
            }
            co_return optional<delivery>(std::move((*r)->content));
        }

        static async::task<expected<returned, io::error>> _co_returned(tracked_ptr<detail::AmqpChannelState> ch) noexcept {
            auto r = co_await ch->returns.receive();
            if (!r) {
                co_return unexpected(_ended(*ch));
            }
            co_return std::move(*r);
        }

        static async::task<expected<void, io::error>> _co_close(tracked_ptr<detail::AmqpChannelState> ch) noexcept {
            using namespace detail;
            std::string out;
            size_t at = amqp_method_begin(out, ch->id, m::channel_close);
            AmqpWriter w(out);
            w.u16(200);
            w.shortstr("");
            w.u16(0);
            w.u16(0);
            amqp_frame_end(out, at);
            auto r = co_await _rpc(ch, std::move(out), m::channel_close_ok);
            {
                std::lock_guard g(ch->conn->lock);
                ch->conn->channels.erase(ch->id);
            }
            amqp_channel_end(*ch, io::error(io::errc::closed, "amqp channel", ch->conn->server));
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        tracked_ptr<detail::AmqpChannelState> _s;
    };

    inline async::task<expected<void, io::error>> consumer::async_cancel() const noexcept {
        using namespace detail;
        std::string out;
        size_t at = amqp_method_begin(out, _ch->id, m::basic_cancel);
        AmqpWriter w(out);
        w.shortstr(_s->tag.view());
        w.bit(false);
        w.finish();
        amqp_frame_end(out, at);
        return channel::_co_void(_ch, std::move(out), m::basic_cancel_ok, "amqp cancel");
    }

    // A connection to an AMQP 0-9-1 broker (RabbitMQ, LavinMQ, Qpid):
    // connect makes it, open_channel opens its channels. A handle of one
    // word: a copy is the same connection.
    //
    //     auto c = net::amqp::client::connect("amqp://guest:guest@localhost/").value();
    //     auto ch = c.open_channel().value();
    //     ch.declare_queue("tasks");
    //     ch.publish("", "tasks", "hello");
    class client {
    public:
        struct options {
            net::tls::config tls;                               // amqps://; the server's name the URL's host when none is set
            duration heartbeat = std::chrono::seconds(60);      // asked for; the lower of both sides'; zero: none
            uint32_t frame_max = 131072;                        // the largest frame asked for; the lower of both sides'
            uint16_t channel_max = 2047;                        // channels at most; the lower of both sides'
            duration timeout = std::chrono::seconds(30);        // the dial, TLS and the handshake, then each method's wait; zero: none
            string name;                                        // the connection's name the broker shows
            size_t queue = 1000;                                // deliveries kept per consumer until received; past it the reader waits
            async::stop_token stop;                             // the connect cancelled
        };

        client() noexcept = default;

        // A connection to the broker of the URL: amqp://user:password@host[:5672]/vhost
        // or amqps://...:5671; guest:guest and the vhost "/" when the URL has none
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

        // The same over a connection there is: the URL then gives only the
        // credentials and the vhost
        static expected<client, io::error> connect(const net::connection& transport, const string& url, const options& o) {
            return async_connect(transport, url, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, string url, options o) noexcept {
            return _co_connect(std::move(url), std::move(transport), std::move(o));
        }

        // channel.open: a channel of its own number
        // `open_channel()` on this thread, `co_await async_open_channel()` in a task
        expected<amqp::channel, io::error> open_channel() const {
            return async_open_channel().wait();
        }

        async::task<expected<amqp::channel, io::error>> async_open_channel() const noexcept {
            return _co_open_channel(_s);
        }

        // Whether the broker said it stops reading publications
        // (connection.blocked, RabbitMQ's alarm of memory or disk)
        bool blocked() const noexcept {
            return _s->blocked.load();
        }

        // connection.close: every channel ended, the connection closed
        // `close()` on this thread, `co_await async_close()` in a task
        expected<void, io::error> close() const {
            return async_close().wait();
        }

        async::task<expected<void, io::error>> async_close() const noexcept {
            return _co_close(_s);
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        explicit client(tracked_ptr<detail::AmqpConnState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            auto t = to.token();
            co_await async::select(from.on_stop([&] { to.request_stop(); }), t.on_stop([] {}));
        }

        // One frame of the handshake read whole: its type, channel and payload
        static async::task<expected<std::string, io::error>> _frame(net::connection& c, std::string& buf, uint8_t& type) noexcept {
            using namespace detail;
            char block[4096];
            for (;;) {
                uint16_t chan = 0;
                uint32_t size = 0;
                if (buf.size() >= 4 && buf.compare(0, 4, "AMQP") == 0) {   // a broker of another version answers with its own header
                    co_return unexpected(amqp_error(errc::not_implemented, "amqp connect", string("the broker speaks another version of AMQP")));
                }
                if (amqp_frame_head(buf, type, chan, size) && size > (uint32_t(1) << 24)) {
                    co_return unexpected(amqp_error(errc::frame_error, "amqp connect", string("a frame past its limit")));
                }
                if (amqp_frame_head(buf, type, chan, size) && buf.size() >= size_t(size) + 8) {
                    if (uint8_t(buf[7 + size]) != FrameEnd) {
                        co_return unexpected(amqp_error(errc::frame_error, "amqp connect", string("a frame without its end octet")));
                    }
                    std::string payload = buf.substr(7, size);
                    buf.erase(0, size_t(size) + 8);
                    if (type == FrameHeartbeat) {
                        continue;
                    }
                    co_return payload;
                }
                auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block), sizeof block));
                if (!r) {
                    co_return unexpected(r.error());
                }
                if (*r == 0) {
                    co_return unexpected(io::error(io::errc::unexpected_eof, "amqp connect", string()));
                }
                buf.append(block, *r);
            }
        }

        static async::task<expected<std::string, io::error>> _method(net::connection& c, std::string& buf, uint32_t want) noexcept {
            using namespace detail;
            uint8_t type = 0;
            auto p = co_await _frame(c, buf, type);
            if (!p) {
                co_return unexpected(p.error());
            }
            if (type != FrameMethod || p->size() < 4) {
                co_return unexpected(amqp_error(errc::unexpected_frame, "amqp connect", string("a frame out of place")));
            }
            uint32_t method = uint32_t(uint8_t((*p)[0])) << 24 | uint32_t(uint8_t((*p)[1])) << 16 | uint32_t(uint8_t((*p)[2])) << 8 | uint8_t((*p)[3]);
            if (method == m::connection_close) {
                co_return unexpected(amqp_close_error(std::string_view(*p).substr(4), "amqp connect"));
            }
            if (method != want) {
                co_return unexpected(amqp_error(errc::command_invalid, "amqp connect", string("a method out of place")));
            }
            co_return p->substr(4);
        }

        static async::task<expected<client, io::error>> _co_connect(string url, net::connection transport, options o) noexcept {
            using namespace detail;
            auto u = net::url::parse(url.empty() ? string("amqp://localhost/") : url);
            if (!u) {
                co_return unexpected(net::detail::net_error(net::errc::invalid_url, "amqp", url));
            }
            std::string scheme(u->scheme().view());
            bool tls = scheme == "amqps";
            if (!tls && scheme != "amqp") {
                co_return unexpected(net::detail::net_error(net::errc::unsupported_scheme, "amqp", url));
            }
            std::string user = u->username().empty() ? std::string("guest") : net::detail::url_unescape(u->username().view());
            std::string password = u->username().empty() ? std::string("guest") : net::detail::url_unescape(u->password().view());
            std::string vhost = u->path().view().size() > 1 ? net::detail::url_unescape(u->path().view().substr(1)) : std::string("/");
            std::string host(u->hostname().view());
            net::tls::config tc = o.tls;
            if (tc.server_name.empty()) {
                tc.server_name = string(host);
            }
            if (o.timeout > duration::zero()) {
                tc.handshake_timeout = o.timeout;
            }
            if (!transport) {
                if (host.empty()) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_url, "amqp", url));
                }
                if (host.find(':') != std::string::npos) {
                    host = "[" + host + "]";
                }
                std::string address = host + ":" + std::to_string(u->port() ? *u->port() : tls ? 5671 : 5672);
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
                if (tls) {
                    auto tt = co_await net::tls::async_client(transport, tc);
                    if (!tt) {
                        (void)transport.close();
                        co_return unexpected(tt.error());
                    }
                    transport = *tt;
                }
            }
            if (o.timeout > duration::zero()) {
                transport.set_deadline(sgcl::clock::now() + o.timeout);
            }
            auto fail = [&](io::error e) -> expected<client, io::error> {
                (void)transport.close();
                return unexpected(std::move(e));
            };
            std::string buf;
            if (auto w = co_await transport.async_write(slice<const byte>(reinterpret_cast<const byte*>(ProtocolHeader.data()), ProtocolHeader.size())); !w) {
                co_return fail(w.error());
            }
            // connection.start: the version and the mechanisms
            auto start = co_await _method(transport, buf, m::connection_start);
            if (!start) {
                co_return fail(start.error());
            }
            {
                AmqpReader r(*start);
                uint8_t major = r.u8();
                uint8_t minor = r.u8();
                (void)r.table();
                std::string_view mechanisms = r.longstr();
                if (!r.ok || major != 0 || minor != 9) {
                    co_return fail(amqp_error(errc::not_implemented, "amqp connect", string("a broker of another version")));
                }
                bool plain = false;
                for (size_t a = 0; a < mechanisms.size();) {
                    size_t sp = mechanisms.find(' ', a);
                    std::string_view one = mechanisms.substr(a, sp == std::string_view::npos ? std::string_view::npos : sp - a);
                    plain |= one == "PLAIN";
                    a = sp == std::string_view::npos ? mechanisms.size() : sp + 1;
                }
                if (!plain) {
                    co_return fail(amqp_error(errc::access_refused, "amqp connect", string("the broker offers no PLAIN")));
                }
            }
            {
                table capabilities{{"publisher_confirms", field(true)}, {"consumer_cancel_notify", field(true)}, {"basic.nack", field(true)},
                                   {"connection.blocked", field(true)}, {"authentication_failure_close", field(true)}};
                table props{{"product", field("sgcl")}, {"platform", field("C++")}, {"capabilities", field(capabilities)}};
                if (!o.name.empty()) {
                    props.push_back({string("connection_name"), field(o.name)});
                }
                std::string out;
                size_t at = amqp_method_begin(out, 0, m::connection_start_ok);
                AmqpWriter w(out);
                w.table(props);
                w.shortstr("PLAIN");
                std::string response;
                response += '\0';
                response += user;
                response += '\0';
                response += password;
                w.longstr(response);
                w.shortstr("en_US");
                amqp_frame_end(out, at);
                if (auto wr = co_await transport.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size())); !wr) {
                    co_return fail(wr.error());
                }
            }
            auto tune = co_await _method(transport, buf, m::connection_tune);
            if (!tune) {
                // a broker without authentication_failure_close closes the socket on bad credentials
                if (tune.error().code() == io::errc::unexpected_eof) {
                    co_return fail(amqp_error(errc::access_refused, "amqp connect", string("the broker closed the connection at the login")));
                }
                co_return fail(tune.error());
            }
            tracked_ptr s = make_tracked<AmqpConnState>();
            {
                AmqpReader r(*tune);
                uint16_t channel_max = r.u16();
                uint32_t frame_max = r.u32();
                uint16_t heartbeat = r.u16();
                if (!r.ok) {
                    co_return fail(amqp_error(errc::malformed, "amqp connect", string("a tune that does not read")));
                }
                auto lower = [](uint32_t a, uint32_t b) { return a == 0 ? b : b == 0 ? a : a < b ? a : b; };
                s->channel_max = uint16_t(lower(channel_max, o.channel_max));
                s->frame_max = lower(frame_max, o.frame_max < FrameMin ? FrameMin : o.frame_max);
                uint32_t want = uint32_t(o.heartbeat.milliseconds() / 1000);
                s->heartbeat = o.heartbeat == duration::zero() ? 0 : uint16_t(lower(heartbeat, want > 65535 ? 65535 : want));
                std::string out;
                size_t at = amqp_method_begin(out, 0, m::connection_tune_ok);
                AmqpWriter w(out);
                w.u16(s->channel_max);
                w.u32(s->frame_max);
                w.u16(s->heartbeat);
                amqp_frame_end(out, at);
                at = amqp_method_begin(out, 0, m::connection_open);
                AmqpWriter w2(out);
                w2.shortstr(vhost);
                w2.shortstr("");
                w2.bit(false);
                w2.finish();
                amqp_frame_end(out, at);
                if (auto wr = co_await transport.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size())); !wr) {
                    co_return fail(wr.error());
                }
            }
            auto open = co_await _method(transport, buf, m::connection_open_ok);
            if (!open) {
                co_return fail(open.error());
            }
            transport.set_deadline(time_point());
            s->conn = transport;
            s->server = transport.remote_endpoint().to_string();
            s->timeout = o.timeout;
            s->queue = o.queue;
            async::go(amqp_read_loop(s, std::move(buf)));
            async::go(amqp_flusher(s));
            if (s->heartbeat) {
                async::go(amqp_heartbeats(s));
            }
            co_return client(s);
        }

        static async::task<expected<amqp::channel, io::error>> _co_open_channel(tracked_ptr<detail::AmqpConnState> s) noexcept {
            using namespace detail;
            tracked_ptr ch = make_tracked<AmqpChannelState>();
            ch->conn = s;
            ch->queue = s->queue;
            {
                std::lock_guard g(s->lock);
                if (s->end) {
                    co_return unexpected(*s->end);
                }
                uint16_t limit = s->channel_max ? s->channel_max : 65535;
                uint16_t id = 0;
                for (uint32_t tries = 0; tries < limit; ++tries) {
                    s->next_channel = uint16_t(s->next_channel % limit + 1);
                    if (s->channels.find(s->next_channel) == s->channels.end()) {
                        id = s->next_channel;
                        break;
                    }
                }
                if (id == 0) {
                    co_return unexpected(amqp_error(errc::channel_error, "amqp open_channel", string("every channel is open")));
                }
                ch->id = id;
                s->channels[id] = ch;
            }
            std::string out;
            size_t at = amqp_method_begin(out, ch->id, m::channel_open);
            AmqpWriter w(out);
            w.shortstr("");
            amqp_frame_end(out, at);
            auto r = co_await channel::_rpc(ch, std::move(out), m::channel_open_ok);
            if (!r) {
                std::lock_guard g(s->lock);
                s->channels.erase(ch->id);
                co_return unexpected(r.error());
            }
            co_return amqp::channel(ch);
        }

        static async::task<expected<void, io::error>> _co_close(tracked_ptr<detail::AmqpConnState> s) noexcept {
            using namespace detail;
            if (s->closed.load()) {
                co_return unexpected(amqp_ended(*s));
            }
            tracked_ptr rpc = make_tracked<AmqpRpc>();
            {
                std::lock_guard g(s->lock);
                s->close_rpc = rpc;
            }
            std::string out;
            size_t at = amqp_method_begin(out, 0, m::connection_close);
            AmqpWriter w(out);
            w.u16(200);
            w.shortstr("");
            w.u16(0);
            w.u16(0);
            amqp_frame_end(out, at);
            if (auto wr = co_await amqp_write(s, std::move(out)); wr) {
                if (s->timeout > duration::zero()) {
                    co_await async::select(rpc->done.on_done([] {}), async::timeout(s->timeout, [] {}));
                } else {
                    (void)co_await rpc->done;
                }
            }
            amqp_end(*s, io::error(io::errc::closed, "amqp", s->server));
            co_return expected<void, io::error>();
        }

        tracked_ptr<detail::AmqpConnState> _s;
    };
}
