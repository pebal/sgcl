//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A minimal AMQP 0-9-1 broker for the client's tests, written from the
// specification and RabbitMQ's documented behaviour on the client's own
// codec (no RabbitMQ here): PLAIN login on the vhost "/", channels, the
// default, direct, fanout, topic and headers exchanges, queues (server
// named, exclusive, auto-delete), bindings, publish with mandatory and
// basic.return, publisher confirms, consumers in turn with prefetch, get,
// ack, nack and reject with requeue, channel errors as channel.close,
// heartbeats, connection.blocked, a broker's connection.close. What the
// client is verified against here is this broker and the specification;
// no other implementation of AMQP is installed on this machine.
#pragma once

#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/amqp.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace amqp_test {
    using namespace sgcl;
    namespace ad = sgcl::net::amqp::detail;
    namespace amqp = sgcl::net::amqp;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    struct Msg {
        std::string body;
        std::string header;        // the content header's payload past the body size: flags and properties
        std::string exchange;
        std::string routing_key;
        bool redelivered = false;
        std::map<std::string, std::string> headers;   // for the headers exchange, as text
    };

    struct Binding {
        std::string queue;
        std::string key;
        std::map<std::string, std::string> args;
    };

    struct Exchange {
        std::string type;
        bool durable = false;
        std::vector<Binding> bindings;
    };

    struct Queue {
        std::deque<Msg> messages;
        bool exclusive = false;
        bool auto_delete = false;
        int owner = 0;             // the connection of an exclusive queue
        bool had_consumer = false;
    };

    struct Unacked {
        std::string queue;
        Msg msg;
    };

    struct ChannelState {
        bool open = false;
        bool closing = false;      // channel.close sent, waiting for close-ok
        bool confirms = false;
        uint64_t publish_seq = 0;
        uint64_t next_tag = 0;
        uint16_t prefetch = 0;
        std::map<uint64_t, Unacked> unacked;
        // a publication being read
        int stage = 0;             // 0 none, 1 header due, 2 body due
        std::string exchange, routing_key;
        bool mandatory = false;
        uint64_t body_size = 0;
        Msg pending;
    };

    struct Conn {
        int id = 0;
        net::connection c;
        async::channel<std::string> out{1 << 16};
        std::map<uint16_t, ChannelState> channels;
        std::string user;
        bool open = false;
    };

    struct Consumer {
        int conn = 0;
        uint16_t channel = 0;
        std::string tag;
        std::string queue;
        bool no_ack = false;
    };

    inline bool topic_match(std::string_view pattern, std::string_view key) {
        auto words = [](std::string_view s) {
            std::vector<std::string_view> w;
            size_t at = 0;
            for (;;) {
                size_t d = s.find('.', at);
                w.push_back(s.substr(at, d == std::string_view::npos ? std::string_view::npos : d - at));
                if (d == std::string_view::npos) {
                    break;
                }
                at = d + 1;
            }
            return w;
        };
        auto p = words(pattern), k = words(key);
        std::function<bool(size_t, size_t)> go = [&](size_t i, size_t j) -> bool {
            if (i == p.size()) {
                return j == k.size();
            }
            if (p[i] == "#") {
                for (size_t n = j; n <= k.size(); ++n) {
                    if (go(i + 1, n)) {
                        return true;
                    }
                }
                return false;
            }
            if (j == k.size()) {
                return false;
            }
            return (p[i] == "*" || p[i] == k[j]) && go(i + 1, j + 1);
        };
        return go(0, 0);
    }

    // A field's value as text, for the headers exchange's comparison
    inline std::string field_text(const amqp::field& f) {
        if (auto s = f.as_string()) {
            return str(*s);
        }
        if (auto i = f.as_int()) {
            return std::to_string(*i);
        }
        if (auto b = f.as_bool()) {
            return *b ? "true" : "false";
        }
        return "?";
    }

    struct Broker {
        net::listener listener;
        async::task<> serving;
        std::mutex mu;
        std::map<std::string, Exchange> exchanges;
        std::map<std::string, Queue> queues;
        std::vector<Consumer> consumers;
        size_t turn = 0;
        map<int, tracked_ptr<Conn>> conns;
        int next_conn = 0;
        int next_queue = 0;
        int next_tag = 0;
        std::string user = "guest", password = "guest";
        uint16_t heartbeat = 0;                  // offered in tune
        uint32_t frame_max = 131072;
        std::atomic<bool> silent{false};         // nothing read or written any more: a dead peer
        std::atomic<bool> block_on_open{false};  // connection.blocked right after open-ok
        std::atomic<bool> nack_publications{false};   // confirms nack instead of ack
        std::atomic<int> connections{0};
        std::atomic<long> published{0};

        Broker() {
            for (auto [name, type] : std::vector<std::pair<std::string, std::string>>{
                     {"", "direct"}, {"amq.direct", "direct"}, {"amq.fanout", "fanout"}, {"amq.topic", "topic"}, {"amq.headers", "headers"}}) {
                exchanges[name].type = type;
            }
            listener = net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(accept_loop(this, listener));
        }

        ~Broker() {
            (void)listener.close();
            serving.wait();
            {
                std::lock_guard g(mu);
                for (auto& [id, c] : conns) {
                    (void)c->c.close();
                    c->out.close();
                }
            }
            for (int i = 0; i < 500 && connections.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const std::string& credentials = "guest:guest@") const {
            return sgcl::string("amqp://" + credentials + "127.0.0.1:" + std::to_string(port()) + "/");
        }

        // A connection.close of the broker's to every client
        void close_all(uint16_t code, const std::string& text) {
            std::lock_guard g(mu);
            for (auto& [id, c] : conns) {
                std::string out;
                size_t at = ad::amqp_method_begin(out, 0, ad::m::connection_close);
                ad::AmqpWriter w(out);
                w.u16(code);
                w.shortstr(text);
                w.u16(0);
                w.u16(0);
                ad::amqp_frame_end(out, at);
                (void)c->out.try_send(std::move(out));
            }
        }

        size_t queue_depth(const std::string& q) {
            std::lock_guard g(mu);
            auto it = queues.find(q);
            return it == queues.end() ? 0 : it->second.messages.size();
        }

        bool has_queue(const std::string& q) {
            std::lock_guard g(mu);
            return queues.count(q) != 0;
        }

        static async::task<> accept_loop(Broker* self, net::listener l) {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                ++self->connections;
                tracked_ptr conn = make_tracked<Conn>();
                conn->c = *c;
                {
                    std::lock_guard g(self->mu);
                    conn->id = ++self->next_conn;
                    self->conns[conn->id] = conn;
                }
                async::go(writer(self, conn));
                async::go(serve(self, conn));
                if (self->heartbeat) {
                    async::go(beats(self, conn));
                }
            }
        }

        // A heartbeat every half of the interval offered
        static async::task<> beats(Broker* self, tracked_ptr<Conn> conn) {
            for (;;) {
                co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(int64_t(self->heartbeat) * 500));
                std::string hb;
                ad::amqp_heartbeat(hb);
                if (conn->out.closed() || !conn->out.try_send(std::move(hb))) {
                    co_return;
                }
            }
        }

        static async::task<> writer(Broker* self, tracked_ptr<Conn> conn) {
            for (;;) {
                auto b = co_await conn->out.receive();
                if (!b) {
                    co_return;
                }
                std::string all = std::move(*b);
                while (auto more = conn->out.try_receive()) {   // what queued meanwhile, in one write
                    all += *more;
                }
                if (self->silent.load()) {
                    continue;
                }
                if (!co_await conn->c.async_write(slice<const byte>(reinterpret_cast<const byte*>(all.data()), all.size()))) {
                    co_return;
                }
            }
        }

        static void method(std::string& out, uint16_t ch, uint32_t m, const std::function<void(ad::AmqpWriter&)>& args = {}) {
            size_t at = ad::amqp_method_begin(out, ch, m);
            ad::AmqpWriter w(out);
            if (args) {
                args(w);
            }
            w.finish();
            ad::amqp_frame_end(out, at);
        }

        // A channel error: channel.close with the code, the channel closing
        void channel_error(Conn& c, uint16_t ch, uint16_t code, const std::string& text, uint32_t cause, std::string& out) {
            method(out, ch, ad::m::channel_close, [&](ad::AmqpWriter& w) {
                w.u16(code);
                w.shortstr(text);
                w.u16(uint16_t(cause >> 16));
                w.u16(uint16_t(cause));
            });
            drop_channel(c, ch);
            c.channels[ch].closing = true;
        }

        // The channel's consumers gone, its unacked messages back to their queues
        void drop_channel(Conn& c, uint16_t ch) {
            auto& st = c.channels[ch];
            std::set<std::string> touched;
            for (auto it = st.unacked.rbegin(); it != st.unacked.rend(); ++it) {
                auto q = queues.find(it->second.queue);
                if (q != queues.end()) {
                    it->second.msg.redelivered = true;
                    q->second.messages.push_front(it->second.msg);
                    touched.insert(it->second.queue);
                }
            }
            st.unacked.clear();
            for (auto it = consumers.begin(); it != consumers.end();) {
                if (it->conn == c.id && it->channel == ch) {
                    touched.insert(it->queue);
                    it = consumers.erase(it);
                } else {
                    ++it;
                }
            }
            st.open = false;
            st.confirms = false;
            st.stage = 0;
            for (auto& q : touched) {
                auto_delete(q);
                dispatch(q);
            }
        }

        void auto_delete(const std::string& q) {
            auto it = queues.find(q);
            if (it == queues.end() || !it->second.auto_delete || !it->second.had_consumer) {
                return;
            }
            for (auto& cs : consumers) {
                if (cs.queue == q) {
                    return;
                }
            }
            delete_queue(q);
        }

        void delete_queue(const std::string& q) {
            queues.erase(q);
            for (auto& [n, x] : exchanges) {
                std::erase_if(x.bindings, [&](const Binding& b) { return b.queue == q; });
            }
            for (auto it = consumers.begin(); it != consumers.end();) {
                if (it->queue == q) {   // RabbitMQ's consumer cancel notification
                    auto c = conns.find(it->conn);
                    if (c != conns.end()) {
                        std::string out;
                        std::string tag = it->tag;
                        method(out, it->channel, ad::m::basic_cancel, [&](ad::AmqpWriter& w) {
                            w.shortstr(tag);
                            w.bit(true);
                        });
                        (void)c->second->out.try_send(std::move(out));
                    }
                    it = consumers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // The ready messages of the queue to its consumers in turn, as far
        // as their prefetch allows
        void dispatch(const std::string& qname) {
            auto q = queues.find(qname);
            if (q == queues.end()) {
                return;
            }
            while (!q->second.messages.empty()) {
                bool sent = false;
                for (size_t n = 0; n < consumers.size() && !sent; ++n) {
                    Consumer& cs = consumers[(turn + n) % consumers.size()];
                    if (cs.queue != qname) {
                        continue;
                    }
                    auto c = conns.find(cs.conn);
                    if (c == conns.end()) {
                        continue;
                    }
                    auto& st = c->second->channels[cs.channel];
                    if (!cs.no_ack && st.prefetch && st.unacked.size() >= st.prefetch) {
                        continue;
                    }
                    Msg m = std::move(q->second.messages.front());
                    q->second.messages.pop_front();
                    uint64_t tag = ++st.next_tag;
                    std::string out;
                    method(out, cs.channel, ad::m::basic_deliver, [&](ad::AmqpWriter& w) {
                        w.shortstr(cs.tag);
                        w.u64(tag);
                        w.bit(m.redelivered);
                        w.shortstr(m.exchange);
                        w.shortstr(m.routing_key);
                    });
                    content(out, cs.channel, m);
                    if (!cs.no_ack) {
                        st.unacked[tag] = Unacked{qname, std::move(m)};
                    }
                    (void)c->second->out.try_send(std::move(out));
                    turn = (turn + n + 1) % (consumers.empty() ? 1 : consumers.size());
                    sent = true;
                }
                if (!sent) {
                    return;
                }
            }
        }

        void content(std::string& out, uint16_t ch, const Msg& m) {
            size_t at = out.size();
            char h[7] = {char(ad::FrameHeader), char(ch >> 8), char(ch), 0, 0, 0, 0};
            out.append(h, 7);
            ad::AmqpWriter w(out);
            w.u16(60);
            w.u16(0);
            w.u64(m.body.size());
            w.finish();
            out += m.header;
            ad::amqp_frame_end(out, at);
            size_t chunk = frame_max - 8;
            for (size_t off = 0; off < m.body.size(); off += chunk) {
                size_t n = std::min(chunk, m.body.size() - off);
                size_t b = out.size();
                char bh[7] = {char(ad::FrameBody), char(ch >> 8), char(ch), 0, 0, 0, 0};
                out.append(bh, 7);
                out.append(m.body, off, n);
                ad::amqp_frame_end(out, b);
            }
        }

        // The queues a publication goes to
        std::vector<std::string> route(const std::string& exchange, const Msg& m) {
            std::vector<std::string> out;
            auto x = exchanges.find(exchange);
            if (x == exchanges.end()) {
                return out;
            }
            if (exchange.empty()) {
                if (queues.count(m.routing_key)) {
                    out.push_back(m.routing_key);
                }
                return out;
            }
            for (auto& b : x->second.bindings) {
                bool match = false;
                if (x->second.type == "direct") {
                    match = b.key == m.routing_key;
                } else if (x->second.type == "fanout") {
                    match = true;
                } else if (x->second.type == "topic") {
                    match = topic_match(b.key, m.routing_key);
                } else if (x->second.type == "headers") {
                    bool any = b.args.count("x-match") && b.args.at("x-match") == "any";
                    int hits = 0, wanted = 0;
                    for (auto& [k, v] : b.args) {
                        if (k.rfind("x-", 0) == 0) {
                            continue;
                        }
                        ++wanted;
                        auto h = m.headers.find(k);
                        hits += h != m.headers.end() && h->second == v;
                    }
                    match = any ? hits > 0 : hits == wanted;
                }
                if (match && std::find(out.begin(), out.end(), b.queue) == out.end()) {
                    out.push_back(b.queue);
                }
            }
            return out;
        }

        static async::task<> serve(Broker* self, tracked_ptr<Conn> conn) {
            std::string buf;
            std::vector<char> block(65536);
            auto read_more = [&]() -> async::task<bool> {
                auto r = co_await conn->c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                if (!r || *r == 0) {
                    co_return false;
                }
                if (!self->silent.load()) {
                    buf.append(block.data(), *r);
                }
                co_return true;
            };
            // the protocol header
            while (buf.size() < 8) {
                if (!co_await read_more()) {
                    goto done;
                }
            }
            if (buf.compare(0, 8, std::string(ad::ProtocolHeader)) != 0) {
                (void)co_await conn->c.async_write(slice<const byte>(reinterpret_cast<const byte*>(ad::ProtocolHeader.data()), 8));
                goto done;
            }
            buf.erase(0, 8);
            {
                std::string out;
                method(out, 0, ad::m::connection_start, [&](ad::AmqpWriter& w) {
                    w.u8(0);
                    w.u8(9);
                    w.table(amqp::table{{"product", amqp::field("sgcl test broker")},
                                        {"capabilities", amqp::field(amqp::table{{"publisher_confirms", amqp::field(true)}, {"basic.nack", amqp::field(true)}})}});
                    w.longstr("PLAIN AMQPLAIN");
                    w.longstr("en_US");
                });
                (void)conn->out.try_send(std::move(out));
            }
            for (;;) {
                uint8_t type = 0;
                uint16_t ch = 0;
                uint32_t size = 0;
                if (!ad::amqp_frame_head(buf, type, ch, size) || buf.size() < size_t(size) + 8) {
                    if (!co_await read_more()) {
                        break;
                    }
                    continue;
                }
                if (uint8_t(buf[7 + size]) != ad::FrameEnd || size > self->frame_max) {
                    break;
                }
                std::string payload = buf.substr(7, size);
                buf.erase(0, size_t(size) + 8);
                if (type == ad::FrameHeartbeat) {
                    continue;
                }
                std::string out;
                bool go_on = true;
                {
                    std::lock_guard g(self->mu);
                    go_on = self->frame(*conn, type, ch, payload, out);
                }
                if (!out.empty()) {
                    (void)conn->out.try_send(std::move(out));
                }
                if (!go_on) {
                    break;
                }
            }
        done:
            {
                std::lock_guard g(self->mu);
                for (auto& [id, st] : conn->channels) {
                    self->drop_channel(*conn, id);
                }
                std::vector<std::string> exclusive;
                for (auto& [n, q] : self->queues) {
                    if (q.exclusive && q.owner == conn->id) {
                        exclusive.push_back(n);
                    }
                }
                for (auto& n : exclusive) {
                    self->delete_queue(n);
                }
                self->conns.erase(conn->id);
            }
            conn->out.close();   // the writer sends what is queued (close-ok), then ends
            co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(20));
            (void)conn->c.close();
            --self->connections;
        }

        // One frame: false ends the connection
        bool frame(Conn& c, uint8_t type, uint16_t ch, const std::string& payload, std::string& out) {
            auto& st = c.channels[ch];
            if (type == ad::FrameHeader || type == ad::FrameBody) {
                if (st.closing || !st.open) {
                    return true;
                }
                if (type == ad::FrameHeader) {
                    if (st.stage != 1) {
                        return connection_close(out, 505, "UNEXPECTED_FRAME");
                    }
                    ad::AmqpReader r(payload);
                    (void)r.u16();
                    (void)r.u16();
                    st.body_size = r.u64();
                    std::string header = payload.substr(12);
                    ad::AmqpReader pr(header);
                    auto props = pr.properties();
                    st.pending.header = header;
                    st.pending.headers.clear();
                    for (auto& [k, v] : props.headers) {
                        st.pending.headers[str(k)] = field_text(v);
                    }
                    st.pending.body.clear();
                    st.stage = 2;
                } else {
                    if (st.stage != 2) {
                        return connection_close(out, 505, "UNEXPECTED_FRAME");
                    }
                    st.pending.body += payload;
                }
                if (st.stage == 2 && st.pending.body.size() >= st.body_size) {
                    st.stage = 0;
                    publish(c, ch, st, out);
                }
                return true;
            }
            if (type != ad::FrameMethod || payload.size() < 4) {
                return connection_close(out, 505, "UNEXPECTED_FRAME");
            }
            uint32_t m = uint32_t(uint8_t(payload[0])) << 24 | uint32_t(uint8_t(payload[1])) << 16 | uint32_t(uint8_t(payload[2])) << 8 | uint8_t(payload[3]);
            ad::AmqpReader r(std::string_view(payload).substr(4));
            if (ch == 0) {
                return connection_method(c, m, r, out);
            }
            if (st.closing) {
                if (m == ad::m::channel_close_ok) {
                    c.channels.erase(ch);
                } else if (m == ad::m::channel_close) {
                    method(out, ch, ad::m::channel_close_ok);
                }
                return true;
            }
            if (m == ad::m::channel_open) {
                if (st.open) {
                    return connection_close(out, 504, "CHANNEL_ERROR - second 'channel.open'");
                }
                st = ChannelState();
                st.open = true;
                method(out, ch, ad::m::channel_open_ok, [](ad::AmqpWriter& w) { w.longstr(""); });
                return true;
            }
            if (!st.open) {
                return connection_close(out, 504, "CHANNEL_ERROR - expected 'channel.open'");
            }
            switch (m) {
                case ad::m::channel_close:
                    drop_channel(c, ch);
                    c.channels.erase(ch);
                    method(out, ch, ad::m::channel_close_ok);
                    return true;
                case ad::m::exchange_declare: {
                    (void)r.u16();
                    std::string name(r.shortstr()), type_(r.shortstr());
                    bool passive = r.bit(), durable = r.bit();
                    (void)r.bit();
                    (void)r.bit();
                    (void)r.bit();
                    (void)r.table();
                    auto x = exchanges.find(name);
                    if (passive) {
                        if (x == exchanges.end()) {
                            channel_error(c, ch, 404, "NOT_FOUND - no exchange '" + name + "' in vhost '/'", m, out);
                            return true;
                        }
                    } else if (x != exchanges.end() && x->second.type != type_) {
                        channel_error(c, ch, 406, "PRECONDITION_FAILED - inequivalent arg 'type' for exchange '" + name + "'", m, out);
                        return true;
                    } else if (name.rfind("amq.", 0) == 0 && x == exchanges.end()) {
                        channel_error(c, ch, 403, "ACCESS_REFUSED - exchange name '" + name + "' contains reserved prefix 'amq.*'", m, out);
                        return true;
                    } else if (type_ != "direct" && type_ != "fanout" && type_ != "topic" && type_ != "headers") {
                        return connection_close(out, 503, "COMMAND_INVALID - unknown exchange type '" + type_ + "'");
                    } else {
                        exchanges[name].type = type_;
                        exchanges[name].durable = durable;
                    }
                    method(out, ch, ad::m::exchange_declare_ok);
                    return true;
                }
                case ad::m::exchange_delete: {
                    (void)r.u16();
                    std::string name(r.shortstr());
                    bool if_unused = r.bit();
                    auto x = exchanges.find(name);
                    if (x == exchanges.end()) {
                        channel_error(c, ch, 404, "NOT_FOUND - no exchange '" + name + "' in vhost '/'", m, out);
                        return true;
                    }
                    if (if_unused && !x->second.bindings.empty()) {
                        channel_error(c, ch, 406, "PRECONDITION_FAILED - exchange '" + name + "' in use", m, out);
                        return true;
                    }
                    exchanges.erase(x);
                    method(out, ch, ad::m::exchange_delete_ok);
                    return true;
                }
                case ad::m::queue_declare: {
                    (void)r.u16();
                    std::string name(r.shortstr());
                    bool passive = r.bit(), durable = r.bit(), exclusive = r.bit(), auto_del = r.bit();
                    (void)durable;
                    (void)r.bit();
                    (void)r.table();
                    if (name.empty()) {
                        name = "amq.gen-" + std::to_string(++next_queue);
                    }
                    auto q = queues.find(name);
                    if (passive && q == queues.end()) {
                        channel_error(c, ch, 404, "NOT_FOUND - no queue '" + name + "' in vhost '/'", m, out);
                        return true;
                    }
                    if (q != queues.end() && q->second.exclusive && q->second.owner != c.id) {
                        channel_error(c, ch, 405, "RESOURCE_LOCKED - cannot obtain exclusive access to locked queue '" + name + "'", m, out);
                        return true;
                    }
                    if (q == queues.end()) {
                        Queue& nq = queues[name];
                        nq.exclusive = exclusive;
                        nq.auto_delete = auto_del;
                        nq.owner = c.id;
                        exchanges[""].bindings.push_back({name, name, {}});
                        q = queues.find(name);
                    }
                    uint32_t consumers_n = 0;
                    for (auto& cs : consumers) {
                        consumers_n += cs.queue == name;
                    }
                    uint32_t msgs = uint32_t(q->second.messages.size());
                    method(out, ch, ad::m::queue_declare_ok, [&](ad::AmqpWriter& w) {
                        w.shortstr(name);
                        w.u32(msgs);
                        w.u32(consumers_n);
                    });
                    return true;
                }
                case ad::m::queue_bind:
                case ad::m::queue_unbind: {
                    (void)r.u16();
                    std::string queue(r.shortstr()), exchange(r.shortstr()), key(r.shortstr());
                    if (m == ad::m::queue_bind) {
                        (void)r.bit();
                    }
                    auto args = r.table();
                    if (!queues.count(queue)) {
                        channel_error(c, ch, 404, "NOT_FOUND - no queue '" + queue + "' in vhost '/'", m, out);
                        return true;
                    }
                    auto x = exchanges.find(exchange);
                    if (x == exchanges.end()) {
                        channel_error(c, ch, 404, "NOT_FOUND - no exchange '" + exchange + "' in vhost '/'", m, out);
                        return true;
                    }
                    if (exchange.empty()) {
                        channel_error(c, ch, 403, "ACCESS_REFUSED - operation not permitted on the default exchange", m, out);
                        return true;
                    }
                    Binding b{queue, key, {}};
                    for (auto& [k, v] : args) {
                        b.args[str(k)] = field_text(v);
                    }
                    auto& bs = x->second.bindings;
                    std::erase_if(bs, [&](const Binding& o) { return o.queue == b.queue && o.key == b.key && o.args == b.args; });
                    if (m == ad::m::queue_bind) {
                        bs.push_back(b);
                    }
                    method(out, ch, m == ad::m::queue_bind ? ad::m::queue_bind_ok : ad::m::queue_unbind_ok);
                    return true;
                }
                case ad::m::queue_purge:
                case ad::m::queue_delete: {
                    (void)r.u16();
                    std::string queue(r.shortstr());
                    bool if_unused = m == ad::m::queue_delete && r.bit();
                    bool if_empty = m == ad::m::queue_delete && r.bit();
                    auto q = queues.find(queue);
                    if (q == queues.end()) {
                        if (m == ad::m::queue_delete) {   // RabbitMQ: deleting a queue that is not there is fine
                            method(out, ch, ad::m::queue_delete_ok, [](ad::AmqpWriter& w) { w.u32(0); });
                            return true;
                        }
                        channel_error(c, ch, 404, "NOT_FOUND - no queue '" + queue + "' in vhost '/'", m, out);
                        return true;
                    }
                    uint32_t n = uint32_t(q->second.messages.size());
                    if (m == ad::m::queue_purge) {
                        q->second.messages.clear();
                        method(out, ch, ad::m::queue_purge_ok, [&](ad::AmqpWriter& w) { w.u32(n); });
                        return true;
                    }
                    bool used = std::any_of(consumers.begin(), consumers.end(), [&](const Consumer& cs) { return cs.queue == queue; });
                    if ((if_unused && used) || (if_empty && n)) {
                        channel_error(c, ch, 406, "PRECONDITION_FAILED - queue '" + queue + "' in use or not empty", m, out);
                        return true;
                    }
                    delete_queue(queue);
                    method(out, ch, ad::m::queue_delete_ok, [&](ad::AmqpWriter& w) { w.u32(n); });
                    return true;
                }
                case ad::m::basic_qos: {
                    (void)r.u32();
                    st.prefetch = r.u16();
                    method(out, ch, ad::m::basic_qos_ok);
                    return true;
                }
                case ad::m::confirm_select: {
                    st.confirms = true;
                    method(out, ch, ad::m::confirm_select_ok);
                    return true;
                }
                case ad::m::basic_publish: {
                    (void)r.u16();
                    st.exchange = std::string(r.shortstr());
                    st.routing_key = std::string(r.shortstr());
                    st.mandatory = r.bit();
                    if (st.confirms) {
                        ++st.publish_seq;
                    }
                    st.stage = 1;
                    return true;
                }
                case ad::m::basic_consume: {
                    (void)r.u16();
                    std::string queue(r.shortstr()), tag(r.shortstr());
                    (void)r.bit();
                    bool no_ack = r.bit(), exclusive = r.bit();
                    (void)exclusive;
                    if (!queues.count(queue)) {
                        channel_error(c, ch, 404, "NOT_FOUND - no queue '" + queue + "' in vhost '/'", m, out);
                        return true;
                    }
                    if (tag.empty()) {
                        tag = "amq.ctag-" + std::to_string(++next_tag);
                    }
                    for (auto& cs : consumers) {
                        if (cs.conn == c.id && cs.channel == ch && cs.tag == tag) {
                            return connection_close(out, 530, "NOT_ALLOWED - attempt to reuse consumer tag '" + tag + "'");
                        }
                    }
                    consumers.push_back({c.id, ch, tag, queue, no_ack});
                    queues[queue].had_consumer = true;
                    method(out, ch, ad::m::basic_consume_ok, [&](ad::AmqpWriter& w) { w.shortstr(tag); });
                    (void)c.out.try_send(std::move(out));
                    out.clear();
                    dispatch(queue);
                    return true;
                }
                case ad::m::basic_cancel: {
                    std::string tag(r.shortstr());
                    std::string queue;
                    for (auto it = consumers.begin(); it != consumers.end(); ++it) {
                        if (it->conn == c.id && it->channel == ch && it->tag == tag) {
                            queue = it->queue;
                            consumers.erase(it);
                            break;
                        }
                    }
                    method(out, ch, ad::m::basic_cancel_ok, [&](ad::AmqpWriter& w) { w.shortstr(tag); });
                    if (!queue.empty()) {
                        auto_delete(queue);
                    }
                    return true;
                }
                case ad::m::basic_get: {
                    (void)r.u16();
                    std::string queue(r.shortstr());
                    bool no_ack = r.bit();
                    auto q = queues.find(queue);
                    if (q == queues.end()) {
                        channel_error(c, ch, 404, "NOT_FOUND - no queue '" + queue + "' in vhost '/'", m, out);
                        return true;
                    }
                    if (q->second.messages.empty()) {
                        method(out, ch, ad::m::basic_get_empty, [](ad::AmqpWriter& w) { w.shortstr(""); });
                        return true;
                    }
                    Msg msg = std::move(q->second.messages.front());
                    q->second.messages.pop_front();
                    uint64_t tag = ++st.next_tag;
                    uint32_t left = uint32_t(q->second.messages.size());
                    method(out, ch, ad::m::basic_get_ok, [&](ad::AmqpWriter& w) {
                        w.u64(tag);
                        w.bit(msg.redelivered);
                        w.shortstr(msg.exchange);
                        w.shortstr(msg.routing_key);
                        w.u32(left);
                    });
                    content(out, ch, msg);
                    if (!no_ack) {
                        st.unacked[tag] = Unacked{queue, std::move(msg)};
                    }
                    return true;
                }
                case ad::m::basic_ack:
                case ad::m::basic_nack:
                case ad::m::basic_reject: {
                    uint64_t tag = r.u64();
                    bool multiple = m != ad::m::basic_reject && r.bit();
                    bool requeue = m != ad::m::basic_ack && r.bit();
                    std::vector<uint64_t> tags;
                    if (multiple) {
                        for (auto& [t, u] : st.unacked) {
                            if (t <= tag) {
                                tags.push_back(t);
                            }
                        }
                    } else if (st.unacked.count(tag)) {
                        tags.push_back(tag);
                    }
                    if (tags.empty() && !(multiple && tag == 0)) {
                        channel_error(c, ch, 406, "PRECONDITION_FAILED - unknown delivery tag " + std::to_string(tag), m, out);
                        return true;
                    }
                    std::set<std::string> touched;
                    for (auto t : tags) {
                        Unacked u = std::move(st.unacked[t]);
                        st.unacked.erase(t);
                        touched.insert(u.queue);
                        if (requeue) {
                            auto q = queues.find(u.queue);
                            if (q != queues.end()) {
                                u.msg.redelivered = true;
                                q->second.messages.push_front(std::move(u.msg));
                            }
                        }
                    }
                    for (auto& q : touched) {
                        dispatch(q);
                    }
                    return true;
                }
                default:
                    return connection_close(out, 540, "NOT_IMPLEMENTED");
            }
        }

        void publish(Conn& c, uint16_t ch, ChannelState& st, std::string& out) {
            ++published;
            Msg msg = std::move(st.pending);
            st.pending = Msg();
            msg.exchange = st.exchange;
            msg.routing_key = st.routing_key;
            if (!exchanges.count(st.exchange)) {
                channel_error(c, ch, 404, "NOT_FOUND - no exchange '" + st.exchange + "' in vhost '/'", ad::m::basic_publish, out);
                return;
            }
            auto targets = route(st.exchange, msg);
            if (targets.empty() && st.mandatory) {
                method(out, ch, ad::m::basic_return, [&](ad::AmqpWriter& w) {
                    w.u16(312);
                    w.shortstr("NO_ROUTE");
                    w.shortstr(msg.exchange);
                    w.shortstr(msg.routing_key);
                });
                content(out, ch, msg);
            }
            if (st.confirms) {
                uint64_t seq = st.publish_seq;
                method(out, ch, nack_publications.load() ? ad::m::basic_nack : ad::m::basic_ack, [&](ad::AmqpWriter& w) {
                    w.u64(seq);
                    w.bit(false);
                    if (nack_publications.load()) {
                        w.bit(false);
                    }
                });
            }
            if (!out.empty()) {
                (void)c.out.try_send(std::move(out));
                out.clear();
            }
            for (size_t i = 0; i < targets.size(); ++i) {
                queues[targets[i]].messages.push_back(i + 1 < targets.size() ? msg : std::move(msg));
                dispatch(targets[i]);
            }
        }

        bool connection_close(std::string& out, uint16_t code, const std::string& text) {
            method(out, 0, ad::m::connection_close, [&](ad::AmqpWriter& w) {
                w.u16(code);
                w.shortstr(text);
                w.u16(0);
                w.u16(0);
            });
            return true;   // the client answers close-ok, which ends it
        }

        bool connection_method(Conn& c, uint32_t m, ad::AmqpReader& r, std::string& out) {
            switch (m) {
                case ad::m::connection_start_ok: {
                    (void)r.table();
                    std::string mech(r.shortstr());
                    std::string response(r.longstr());
                    std::string want = std::string(1, '\0') + user + std::string(1, '\0') + password;
                    if (mech != "PLAIN" || response != want) {
                        connection_close(out, 403, "ACCESS_REFUSED - Login was refused using authentication mechanism PLAIN");
                        return true;
                    }
                    c.user = user;
                    method(out, 0, ad::m::connection_tune, [&](ad::AmqpWriter& w) {
                        w.u16(2047);
                        w.u32(frame_max);
                        w.u16(heartbeat);
                    });
                    return true;
                }
                case ad::m::connection_tune_ok:
                    return true;
                case ad::m::connection_open: {
                    std::string vhost(r.shortstr());
                    if (vhost != "/") {
                        connection_close(out, 530, "NOT_ALLOWED - vhost " + vhost + " not found");
                        return true;
                    }
                    c.open = true;
                    method(out, 0, ad::m::connection_open_ok, [](ad::AmqpWriter& w) { w.shortstr(""); });
                    if (block_on_open.load()) {
                        method(out, 0, ad::m::connection_blocked, [](ad::AmqpWriter& w) { w.shortstr("low on memory"); });
                    }
                    return true;
                }
                case ad::m::connection_close:
                    method(out, 0, ad::m::connection_close_ok);
                    (void)c.out.try_send(std::move(out));
                    out.clear();
                    return false;
                case ad::m::connection_close_ok:
                    return false;
                default:
                    connection_close(out, 503, "COMMAND_INVALID");
                    return true;
            }
        }
    };
}
