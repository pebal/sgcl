//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/codec.h"
#include "detail/link.h"
#include "detail/messages.h"
#include "../connection.h"
#include "../error.h"
#include "../http/request.h"
#include "../http/response_writer.h"
#include "../http/websocket.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/select.h"
#include "../../async/timer.h"
#include "../../async/wait_group.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/random.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The broker of MQTT (5.0 and 3.1.1): sessions kept in memory, the
// messages routed from publishers to subscribers by their filters
namespace sgcl::net::mqtt {
    namespace detail {
        struct BrokerSettings {
            function<bool(const string&, const string&, const string&)> authenticate;
            function<bool(const string&, const string&, const string&, bool)> authorize;
            uint8_t maximum_qos = 2;
            bool retain_available = true;
            uint32_t maximum_packet_size = 1 << 20;
            uint16_t receive_maximum = 1024;
            uint16_t topic_alias_maximum = 64;
            uint32_t max_session_expiry = 86400;
            uint16_t max_keep_alive = 0;
            size_t max_queued = 1000;
            size_t max_connections = 0;
            duration connect_timeout = 10 * second;
            function<void(const string&)> on_error;

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "mqtt: " << what.view() << std::endl;
                }
            }
        };

        // A subscription a session holds
        struct BrokerSub {
            std::string filter;   // as given, "$share/g/a/+" included
            std::string match;    // what topics are matched against: the filter without its share
            std::string group;    // the share's group; "" for none
            uint8_t qos = 0;
            bool no_local = false;
            bool retain_as_published = false;
            uint32_t id = 0;
        };

        struct BrokerQueued {
            message msg;
            time_point at;
        };

        struct BrokerConn;

        // A session (MQTT 5 §4.1): its subscriptions, what waits to go to its
        // client, what is in flight both ways; online while it has a connection
        struct BrokerSession {
            std::string client_id;
            std::string user;
            std::mutex lock;
            std::vector<BrokerSub> subs;
            vector<BrokerQueued> queue;   // not sent yet, from head on
            size_t head = 0;
            map<uint16_t, message> inflight;   // sent at QoS 1 or 2, PUBACK or PUBREC awaited
            std::set<uint16_t> released;       // PUBREL sent, PUBCOMP awaited
            std::set<uint16_t> qos2_in;        // PUBREC sent, PUBREL awaited
            uint16_t next_id = 0;
            tracked_ptr<BrokerConn> conn;
            uint64_t generation = 0;           // which connection the session is in (the delayed will's and the expiry's check)
            uint32_t expiry = 0;               // seconds kept after the connection ends
            bool gone = false;                 // removed from the broker
            uint8_t level = 5;
            uint16_t receive_max = 65535;      // what its client takes in flight
            uint32_t max_packet = 0;           // its client's largest packet; 0: none
            optional<message> will;
            uint32_t will_delay = 0;

            size_t queued() const noexcept {
                return queue.size() - head;
            }
        };

        struct BrokerConn {
            enum : int { active = 0, idle = 1, closed = 2 };
            tracked_ptr<MqttLink> link;
            async::mutex write_lock;
            async::channel<bool> wake;      // the writer's: something to send
            std::atomic<int> state = {active};
            std::atomic<bool> ended = {false};
            tracked_ptr<BrokerConn> prev;
            tracked_ptr<BrokerConn> next;
            uint8_t level = 5;

            explicit BrokerConn(tracked_ptr<MqttLink> l) noexcept
            : link(std::move(l))
            , wake(1) {
            }
        };

        struct BrokerImpl {
            std::mutex lock;
            map<string, tracked_ptr<BrokerSession>> sessions;
            map<string, message> retained;
            std::map<std::string, size_t> share_next;   // a share's group and filter, the next of its members
            tracked_ptr<BrokerConn> connections;
            vector<net::listener> listeners;
            async::detail::WaitGroupState running;
            std::atomic<size_t> active = {0};
            std::atomic<bool> shutting_down = {false};
            std::atomic<bool> closed = {false};

            void link(const tracked_ptr<BrokerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                n->next = connections;
                if (connections) {
                    connections->prev = n;
                }
                connections = n;
            }

            void unlink(const tracked_ptr<BrokerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                if (n->prev) {
                    n->prev->next = n->next;
                } else if (connections == n) {
                    connections = n->next;
                }
                if (n->next) {
                    n->next->prev = n->prev;
                }
                n->prev = tracked_ptr<BrokerConn>();
                n->next = tracked_ptr<BrokerConn>();
            }

            vector<tracked_ptr<BrokerConn>> snapshot() noexcept {
                std::lock_guard<std::mutex> g(lock);
                vector<tracked_ptr<BrokerConn>> all;
                for (auto n = connections; n; n = n->next) {
                    all.push_back(n);
                }
                return all;
            }

            void close_listeners() {
                vector<net::listener> ls;
                {
                    std::lock_guard<std::mutex> g(lock);
                    ls = listeners;
                }
                for (auto& l : ls) {
                    (void)l.close();
                }
            }
        };

        inline async::task<expected<void, io::error>> broker_write(tracked_ptr<BrokerConn> c, std::string data) noexcept {
            if (c->ended.load()) {
                co_return unexpected(io::error(io::errc::closed, "mqtt", string()));
            }
            auto guard = co_await c->write_lock.scoped_lock();
            co_return co_await c->link->send(data);
        }

        // A message put in a session's queue (its connection's writer woken);
        // past max_queued dropped, the oldest QoS 0 one first
        inline void broker_enqueue(BrokerSession& s, message m, const BrokerSettings& cfg) {
            tracked_ptr<BrokerConn> wake;
            {
                std::lock_guard g(s.lock);
                if (s.gone) {
                    return;
                }
                if (s.queued() >= cfg.max_queued) {
                    bool dropped = false;
                    for (size_t i = s.head; i < s.queue.size(); ++i) {
                        if (s.queue[i].msg.qos == qos::at_most_once) {
                            s.queue.erase(s.queue.begin() + ptrdiff_t(i));
                            dropped = true;
                            break;
                        }
                    }
                    if (!dropped) {
                        return;   // full of QoS 1 and 2: the new one dropped
                    }
                }
                s.queue.push_back(BrokerQueued{std::move(m), sgcl::clock::now()});
                if (s.head > 1024 && s.head * 2 > s.queue.size()) {
                    s.queue.erase(s.queue.begin(), s.queue.begin() + ptrdiff_t(s.head));
                    s.head = 0;
                }
                wake = s.conn;
            }
            if (wake) {
                (void)wake->wake.try_send(true);
            }
        }

        // A message from a client (or the broker) to every session that
        // subscribed to its topic; a shared subscription's group gets it
        // once, in turn (§4.8.2)
        inline void broker_route(BrokerImpl& b, const BrokerSettings& cfg, const message& m, std::string_view sender) {
            std::string_view topic = m.topic.view();
            if (m.retain && cfg.retain_available) {
                std::lock_guard g(b.lock);
                if (m.payload.empty()) {
                    b.retained.erase(m.topic);
                } else {
                    message kept = m;
                    kept.subscription_ids.clear();
                    b.retained[m.topic] = kept;
                }
            }
            vector<tracked_ptr<BrokerSession>> all;
            {
                std::lock_guard g(b.lock);
                for (auto& [id, s] : b.sessions) {
                    all.push_back(s);
                }
            }
            // a share's members: group + filter, the sessions and their subscriptions
            struct Member {
                tracked_ptr<BrokerSession> s;
                uint8_t qos;
                bool rap;
                uint32_t id;
            };
            std::map<std::string, std::vector<size_t>> shares;
            vector<Member> members;
            for (auto& s : all) {
                int best = -1;
                bool rap = false;
                std::vector<uint32_t> ids;
                {
                    std::lock_guard g(s->lock);
                    for (auto& sub : s->subs) {
                        if (!mqtt_match(sub.match, topic)) {
                            continue;
                        }
                        if (!sub.group.empty()) {
                            shares[sub.group + '\n' + sub.match].push_back(members.size());
                            members.push_back(Member{s, sub.qos, sub.retain_as_published, sub.id});
                            continue;
                        }
                        if (sub.no_local && s->client_id == sender) {
                            continue;
                        }
                        best = std::max(best, int(sub.qos));
                        rap |= sub.retain_as_published;
                        if (sub.id) {
                            ids.push_back(sub.id);
                        }
                    }
                }
                if (best < 0) {
                    continue;
                }
                message copy = m;
                copy.qos = qos(std::min(int(m.qos), best));
                copy.retain = rap && m.retain;
                copy.subscription_ids.clear();
                for (uint32_t id : ids) {
                    copy.subscription_ids.push_back(id);
                }
                broker_enqueue(*s, std::move(copy), cfg);
            }
            for (auto& [key, list] : shares) {
                size_t pick;
                {
                    std::lock_guard g(b.lock);
                    pick = b.share_next[key]++ % list.size();
                }
                // an online member first, from the one in turn on
                for (size_t k = 0; k < list.size(); ++k) {
                    Member& mm = members[list[(pick + k) % list.size()]];
                    bool online;
                    {
                        std::lock_guard g(mm.s->lock);
                        online = bool(mm.s->conn);
                    }
                    if (online || k + 1 == list.size()) {
                        message copy = m;
                        copy.qos = qos(std::min(int(m.qos), int(mm.qos)));
                        copy.retain = mm.rap && m.retain;
                        copy.subscription_ids.clear();
                        if (mm.id) {
                            copy.subscription_ids.push_back(mm.id);
                        }
                        broker_enqueue(*mm.s, std::move(copy), cfg);
                        break;
                    }
                }
            }
        }

        // The PUBLISH of a message to a session: an identifier for QoS 1 and
        // 2, the message kept in flight; "" when it expired or is past the
        // client's maximum packet size
        inline std::string broker_publish_bytes(BrokerSession& s, BrokerQueued& q, bool dup, uint16_t id) {
            message& m = q.msg;
            MqttPacket p = mqtt_publish_of(m);
            p.dup = dup;
            p.id = id;
            if (m.expiry > duration::zero()) {
                duration left = m.expiry - duration(sgcl::clock::now() - q.at);
                if (left <= duration::zero()) {
                    return std::string();
                }
                p.props.message_expiry = std::max<uint32_t>(1, mqtt_seconds(left));
            }
            for (uint32_t sid : m.subscription_ids) {
                p.props.subscription_ids.push_back(sid);
            }
            if (s.level < 5) {
                p.props = MqttProps();
            }
            std::string bytes = mqtt_encode(p, s.level);
            if (s.max_packet && bytes.size() > s.max_packet) {
                return std::string();
            }
            return bytes;
        }

        // The writer of a connection: what is in flight sent again (after a
        // reconnect), then the queue, as many in flight as the client takes
        inline async::task<> broker_writer(tracked_ptr<BrokerConn> c, tracked_ptr<BrokerSession> s, bool resume) noexcept {
            if (resume) {
                std::string again;
                {
                    std::lock_guard g(s->lock);
                    for (auto& [id, m] : s->inflight) {
                        BrokerQueued q{m, sgcl::clock::now()};
                        again += broker_publish_bytes(*s, q, true, id);
                    }
                    for (uint16_t id : s->released) {
                        MqttPacket rel;
                        rel.type = packet::pubrel;
                        rel.id = id;
                        again += mqtt_encode(rel, s->level);
                    }
                }
                if (!again.empty() && !co_await broker_write(c, std::move(again))) {
                    co_return;
                }
            }
            for (;;) {
                std::string out;
                {
                    std::lock_guard g(s->lock);
                    if (s->conn != c) {
                        co_return;   // taken over
                    }
                    while (s->head < s->queue.size() && out.size() < 262144) {
                        BrokerQueued& q = s->queue[s->head];
                        uint16_t id = 0;
                        if (q.msg.qos != qos::at_most_once) {
                            if (s->inflight.size() + s->released.size() >= s->receive_max) {
                                break;   // the client's receive maximum: the rest after an acknowledgement
                            }
                            do {
                                if (++s->next_id == 0) {
                                    s->next_id = 1;
                                }
                            } while (s->inflight.find(s->next_id) != s->inflight.end() || s->released.count(s->next_id));
                            id = s->next_id;
                        }
                        std::string bytes = broker_publish_bytes(*s, q, false, id);
                        if (!bytes.empty()) {
                            out += bytes;
                            if (id) {
                                s->inflight.insert({id, q.msg});
                            }
                        }
                        ++s->head;
                    }
                }
                if (!out.empty()) {
                    if (!co_await broker_write(c, std::move(out))) {
                        co_return;
                    }
                    continue;
                }
                auto w = co_await c->wake.receive();
                if (!w || c->ended.load()) {
                    co_return;
                }
            }
        }

        inline async::task<> broker_delayed_will(tracked_ptr<BrokerImpl> b, tracked_ptr<BrokerSettings> cfg, tracked_ptr<BrokerSession> s, uint64_t generation, message will,
                                                 duration delay) noexcept {
            co_await async::sleep_until(sgcl::clock::now() + delay);
            {
                std::lock_guard g(s->lock);
                if (s->generation != generation || s->conn) {
                    co_return;   // the session came back first: no will (§3.1.3.2.2)
                }
            }
            broker_route(*b, *cfg, will, std::string_view());
        }

        inline async::task<> broker_expire(tracked_ptr<BrokerImpl> b, tracked_ptr<BrokerSession> s, uint64_t generation, duration after) noexcept {
            co_await async::sleep_until(sgcl::clock::now() + after);
            {
                std::lock_guard g(s->lock);
                if (s->generation != generation || s->conn) {
                    co_return;
                }
                s->gone = true;
            }
            std::lock_guard g(b->lock);
            auto it = b->sessions.find(string(s->client_id));
            if (it != b->sessions.end() && it->second == s) {
                b->sessions.erase(it);
            }
        }

        // One connection served: CONNECT and CONNACK, then the packets of
        // the session until DISCONNECT or the end
        inline async::task<> broker_serve(tracked_ptr<BrokerImpl> b, tracked_ptr<BrokerSettings> cfg, tracked_ptr<MqttLink> link) noexcept {
            tracked_ptr c = make_tracked<BrokerConn>(link);
            b->link(c);
            const size_t active = b->active.fetch_add(1) + 1;
            std::string buf;
            tracked_ptr<BrokerSession> s;
            optional<message> will;
            uint32_t will_delay = 0;
            bool publish_will = false;
            uint64_t generation = 0;
            // ---- CONNECT
            MqttPacket p;
            uint8_t level = 5;
            {
                link->read_deadline(sgcl::clock::now() + cfg->connect_timeout);
                size_t header = 0, total = 0;
                bool got = false;
                for (;;) {
                    int f = mqtt_frame(buf, header, total);
                    if (f < 0 || (f > 0 && total > cfg->maximum_packet_size)) {
                        break;
                    }
                    if (f > 0 && buf.size() >= total) {
                        got = true;
                        break;
                    }
                    auto r = co_await link->fill(buf);
                    if (!r || *r == 0) {
                        break;
                    }
                }
                uint8_t reason = 0;
                bool ok = got && mqtt_decode(uint8_t(buf[0]), std::string_view(buf).substr(header, total - header), 5, p, reason) && p.type == packet::connect;
                if (got && (buf.empty() || (uint8_t(buf[0]) >> 4) != packet::connect)) {
                    ok = false;
                    reason = 0;   // not a CONNECT first: closed without a word (§3.1)
                }
                if (ok) {
                    level = p.level >= 5 ? 5 : 4;
                } else if (got && p.level >= 3 && p.level <= 5) {
                    level = p.level >= 5 ? 5 : 4;
                }
                auto refuse = [&](uint8_t why) -> std::string {
                    MqttPacket a;
                    a.type = packet::connack;
                    a.reason = level >= 5 ? why : mqtt_v3_code(why);
                    return mqtt_encode(a, level);
                };
                std::string refusal;
                if (!ok) {
                    if (got && reason) {
                        refusal = refuse(reason == 0x84 ? 0x84 : reason);
                    }
                } else if (level < 5 && p.client_id.empty() && !p.clean_start) {
                    refusal = refuse(0x85);
                } else if (p.props.has_auth_method) {
                    refusal = refuse(0x8C);
                } else if (cfg->max_connections && active > cfg->max_connections) {
                    refusal = refuse(level >= 5 ? 0x9F : 0x88);
                } else if (b->shutting_down.load()) {
                    refusal = refuse(0x88);
                } else if (cfg->authenticate && !cfg->authenticate(string(p.client_id), string(p.user), string(p.password))) {
                    refusal = refuse(level >= 5 ? 0x86 : 0x86);
                } else if (p.has_will && uint8_t(p.will_qos) > cfg->maximum_qos && level >= 5) {
                    refusal = refuse(0x9B);
                } else if (p.has_will && p.will_retain && !cfg->retain_available && level >= 5) {
                    refusal = refuse(0x9A);
                }
                if (!ok || !refusal.empty()) {
                    if (!refusal.empty()) {
                        (void)co_await link->send(refusal);
                    }
                    (void)link->close();
                    b->active.fetch_sub(1);
                    b->unlink(c);
                    b->running.done();
                    co_return;
                }
                buf.erase(0, total);
            }
            c->level = level;
            // ---- the session: taken over, resumed or new
            bool assigned = p.client_id.empty();
            if (assigned) {
                uint8_t r[8];
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(r), sizeof r));
                static constexpr char hex[] = "0123456789abcdef";
                p.client_id = "sgcl-";
                for (uint8_t x : r) {
                    p.client_id += hex[x >> 4];
                    p.client_id += hex[x & 15];
                }
            }
            uint32_t expiry = level >= 5 && p.props.session_expiry ? std::min(*p.props.session_expiry, cfg->max_session_expiry) : 0;
            if (level < 5 && !p.clean_start) {
                expiry = cfg->max_session_expiry;   // 3.1.1: a session kept until a clean one replaces it
            }
            bool present = false;
            tracked_ptr<BrokerConn> old;
            {
                std::lock_guard g(b->lock);
                auto it = b->sessions.find(string(p.client_id));
                if (it != b->sessions.end() && !p.clean_start) {
                    s = it->second;
                    present = true;
                } else {
                    if (it != b->sessions.end()) {
                        std::lock_guard sg(it->second->lock);
                        it->second->gone = true;
                        old = it->second->conn;
                        it->second->conn = tracked_ptr<BrokerConn>();
                        b->sessions.erase(it);
                    }
                    s = make_tracked<BrokerSession>();
                    s->client_id = p.client_id;
                    b->sessions[string(p.client_id)] = s;
                }
                std::lock_guard sg(s->lock);
                if (s->conn && s->conn != c) {
                    old = s->conn;
                }
                s->conn = c;
                generation = ++s->generation;
                s->expiry = expiry;
                s->level = level;
                s->user = p.user;
                s->receive_max = level >= 5 && p.props.receive_maximum ? *p.props.receive_maximum : 65535;
                s->max_packet = level >= 5 && p.props.maximum_packet_size ? *p.props.maximum_packet_size : 0;
                s->will = nullopt;
            }
            if (old) {
                // session taken over (§3.1.4): the old connection told and closed
                if (old->level >= 5) {
                    MqttPacket d;
                    d.type = packet::disconnect;
                    d.reason = 0x8E;
                    (void)co_await broker_write(old, mqtt_encode(d, 5));
                }
                old->ended.store(true);
                (void)old->link->close();
                (void)old->wake.try_send(true);
            }
            if (p.has_will) {
                will = mqtt_message_of(p.will_topic, p.will_payload, p.will_qos, p.will_retain && cfg->retain_available, p.will_props);
                will_delay = p.will_props.will_delay ? *p.will_props.will_delay : 0;
                publish_will = true;
            }
            uint16_t keep_alive = p.keep_alive;
            MqttPacket ack;
            ack.type = packet::connack;
            ack.session_present = present;
            if (level >= 5) {
                if (p.props.session_expiry && expiry != *p.props.session_expiry) {
                    ack.props.session_expiry = expiry;
                }
                if (cfg->receive_maximum != 65535) {
                    ack.props.receive_maximum = cfg->receive_maximum;
                }
                if (cfg->maximum_qos < 2) {
                    ack.props.maximum_qos = cfg->maximum_qos;
                }
                if (!cfg->retain_available) {
                    ack.props.retain_available = 0;
                }
                ack.props.maximum_packet_size = cfg->maximum_packet_size;
                if (cfg->topic_alias_maximum) {
                    ack.props.topic_alias_maximum = cfg->topic_alias_maximum;
                }
                if (assigned) {
                    ack.props.assigned_client_id = p.client_id;
                    ack.props.has_assigned_client_id = true;
                }
                if (cfg->max_keep_alive && (keep_alive == 0 || keep_alive > cfg->max_keep_alive)) {
                    keep_alive = cfg->max_keep_alive;
                    ack.props.server_keep_alive = keep_alive;
                }
            }
            if (!co_await broker_write(c, mqtt_encode(ack, level))) {
                publish_will = true;
            } else {
                async::go(broker_writer(c, s, present));
                // ---- the session's packets
                std::map<uint16_t, std::string> aliases;
                io::error end(io::errc::closed, "mqtt", string());
                uint8_t disconnect_reason = 0;   // 0: none to send
                for (;;) {
                    size_t header = 0, total = 0;
                    int f = mqtt_frame(buf, header, total);
                    if (f < 0) {
                        disconnect_reason = 0x81;
                        break;
                    }
                    if (f > 0 && total > cfg->maximum_packet_size) {
                        disconnect_reason = 0x95;
                        break;
                    }
                    if (f == 0 || buf.size() < total) {
                        if (b->shutting_down.load()) {
                            disconnect_reason = 0x8B;
                            break;
                        }
                        c->state.store(BrokerConn::idle);
                        if (b->shutting_down.load()) {
                            disconnect_reason = 0x8B;
                            break;
                        }
                        link->read_deadline(keep_alive ? sgcl::clock::now() + std::chrono::milliseconds(uint64_t(keep_alive) * 1500) : time_point());
                        auto r = co_await link->fill(buf);
                        int idle = BrokerConn::idle;
                        if (!c->state.compare_exchange_strong(idle, BrokerConn::active)) {
                            break;   // the shutdown took it
                        }
                        if (!r || *r == 0) {
                            if (!r && r.error().is_timeout()) {
                                disconnect_reason = 0x8D;   // keep alive timeout
                            }
                            break;
                        }
                        continue;
                    }
                    MqttPacket q;
                    uint8_t reason = 0;
                    bool ok = mqtt_decode(uint8_t(buf[0]), std::string_view(buf).substr(header, total - header), level, q, reason);
                    buf.erase(0, total);
                    if (!ok) {
                        disconnect_reason = reason;
                        break;
                    }
                    if (q.type == packet::publish) {
                        if (q.props.topic_alias) {
                            uint16_t a = *q.props.topic_alias;
                            if (a > cfg->topic_alias_maximum) {
                                disconnect_reason = 0x94;
                                break;
                            }
                            if (q.topic.empty()) {
                                auto it = aliases.find(a);
                                if (it == aliases.end()) {
                                    disconnect_reason = 0x82;
                                    break;
                                }
                                q.topic = it->second;
                            } else {
                                aliases[a] = q.topic;
                            }
                        }
                        if (q.qos > cfg->maximum_qos) {
                            disconnect_reason = 0x9B;
                            break;
                        }
                        if (q.retain && !cfg->retain_available) {
                            disconnect_reason = 0x9A;
                            break;
                        }
                        bool allowed = !cfg->authorize || cfg->authorize(string(p.client_id), string(p.user), string(q.topic), false);
                        bool fresh = true;
                        if (q.qos == 2) {
                            std::lock_guard g(s->lock);
                            if (s->qos2_in.size() >= cfg->receive_maximum && !s->qos2_in.count(q.id)) {
                                disconnect_reason = 0x93;
                                break;
                            }
                            fresh = s->qos2_in.insert(q.id).second;
                        }
                        if (allowed && fresh) {
                            q.props.topic_alias = nullopt;
                            message m = mqtt_message_of(q.topic, q.payload, q.qos, q.retain, q.props);
                            broker_route(*b, *cfg, m, p.client_id);
                        }
                        if (q.qos > 0) {
                            MqttPacket a;
                            a.type = q.qos == 1 ? packet::puback : packet::pubrec;
                            a.id = q.id;
                            a.reason = allowed ? 0 : 0x87;
                            if (q.qos == 2 && !allowed) {
                                std::lock_guard g(s->lock);
                                s->qos2_in.erase(q.id);
                            }
                            if (!co_await broker_write(c, mqtt_encode(a, level))) {
                                break;
                            }
                        }
                        continue;
                    }
                    if (q.type == packet::pubrel) {
                        bool had;
                        {
                            std::lock_guard g(s->lock);
                            had = s->qos2_in.erase(q.id) > 0;
                        }
                        MqttPacket a;
                        a.type = packet::pubcomp;
                        a.id = q.id;
                        a.reason = had ? 0 : 0x92;
                        if (!co_await broker_write(c, mqtt_encode(a, level))) {
                            break;
                        }
                        continue;
                    }
                    if (q.type == packet::puback || q.type == packet::pubrec || q.type == packet::pubcomp) {
                        bool send_rel = false;
                        {
                            std::lock_guard g(s->lock);
                            if (q.type == packet::pubcomp) {
                                s->released.erase(q.id);
                            } else if (s->inflight.erase(q.id) && q.type == packet::pubrec && q.reason < 0x80) {
                                s->released.insert(q.id);
                                send_rel = true;
                            }
                        }
                        if (send_rel) {
                            MqttPacket rel;
                            rel.type = packet::pubrel;
                            rel.id = q.id;
                            if (!co_await broker_write(c, mqtt_encode(rel, level))) {
                                break;
                            }
                        }
                        (void)c->wake.try_send(true);
                        continue;
                    }
                    if (q.type == packet::subscribe) {
                        MqttPacket a;
                        a.type = packet::suback;
                        a.id = q.id;
                        uint32_t sid = q.props.subscription_ids.empty() ? 0 : q.props.subscription_ids[0];
                        vector<message> to_send;
                        for (auto& sub : q.subs) {
                            uint8_t want = sub.options & 3;
                            bool no_local = sub.options & 0x04;
                            bool rap = sub.options & 0x08;
                            uint8_t rh = uint8_t((sub.options >> 4) & 3);
                            std::string_view group;
                            std::string match(mqtt_share(sub.filter, &group));
                            if (!mqtt_filter_valid(sub.filter)) {
                                a.reasons.push_back(level >= 5 ? 0x8F : 0x80);
                                continue;
                            }
                            if (!group.empty() && no_local) {
                                disconnect_reason = 0x82;   // §3.8.3.1
                                break;
                            }
                            if (cfg->authorize && !cfg->authorize(string(p.client_id), string(p.user), string(sub.filter), true)) {
                                a.reasons.push_back(level >= 5 ? 0x87 : 0x80);
                                continue;
                            }
                            uint8_t granted = std::min(want, cfg->maximum_qos);
                            bool is_new = true;
                            {
                                std::lock_guard g(s->lock);
                                for (auto& old_sub : s->subs) {
                                    if (old_sub.filter == sub.filter) {
                                        old_sub = BrokerSub{sub.filter, match, std::string(group), granted, no_local, rap, sid};
                                        is_new = false;
                                    }
                                }
                                if (is_new) {
                                    s->subs.push_back(BrokerSub{sub.filter, match, std::string(group), granted, no_local, rap, sid});
                                }
                            }
                            a.reasons.push_back(granted);
                            // the retained messages of the filter (§3.3.1.3), never to a share
                            if (group.empty() && (rh == 0 || (rh == 1 && is_new))) {
                                std::lock_guard g(b->lock);
                                for (auto& [t, m] : b->retained) {
                                    if (mqtt_match(match, t.view())) {
                                        message copy = m;
                                        copy.qos = qos(std::min(uint8_t(m.qos), granted));
                                        copy.retain = true;
                                        if (sid) {
                                            copy.subscription_ids.push_back(sid);
                                        }
                                        to_send.push_back(copy);
                                    }
                                }
                            }
                        }
                        if (disconnect_reason) {
                            break;
                        }
                        if (!co_await broker_write(c, mqtt_encode(a, level))) {
                            break;
                        }
                        for (auto& m : to_send) {
                            broker_enqueue(*s, m, *cfg);
                        }
                        continue;
                    }
                    if (q.type == packet::unsubscribe) {
                        MqttPacket a;
                        a.type = packet::unsuback;
                        a.id = q.id;
                        {
                            std::lock_guard g(s->lock);
                            for (auto& f : q.filters) {
                                auto it = std::find_if(s->subs.begin(), s->subs.end(), [&](const BrokerSub& x) { return x.filter == f; });
                                if (it != s->subs.end()) {
                                    s->subs.erase(it);
                                    a.reasons.push_back(0x00);
                                } else {
                                    a.reasons.push_back(0x11);
                                }
                            }
                        }
                        if (!co_await broker_write(c, mqtt_encode(a, level))) {
                            break;
                        }
                        continue;
                    }
                    if (q.type == packet::pingreq) {
                        MqttPacket a;
                        a.type = packet::pingresp;
                        if (!co_await broker_write(c, mqtt_encode(a, level))) {
                            break;
                        }
                        continue;
                    }
                    if (q.type == packet::disconnect) {
                        publish_will = q.reason == 0x04;
                        if (level >= 5 && q.props.session_expiry) {
                            if (expiry == 0 && *q.props.session_expiry != 0) {
                                disconnect_reason = 0x82;   // §3.14.2.2.2
                                publish_will = true;
                                break;
                            }
                            expiry = std::min(*q.props.session_expiry, cfg->max_session_expiry);
                            std::lock_guard g(s->lock);
                            s->expiry = expiry;
                        }
                        break;
                    }
                    disconnect_reason = 0x82;   // CONNECT again, AUTH, a server's packet
                    break;
                }
                if (disconnect_reason && level >= 5 && !c->ended.load()) {
                    MqttPacket d;
                    d.type = packet::disconnect;
                    d.reason = disconnect_reason;
                    (void)co_await broker_write(c, mqtt_encode(d, level));
                }
                (void)end;
            }
            // ---- the end of the connection
            c->ended.store(true);
            c->wake.close();
            (void)link->close();
            bool taken = false;
            {
                std::lock_guard g(s->lock);
                taken = s->conn != c;
                if (!taken) {
                    s->conn = tracked_ptr<BrokerConn>();
                }
            }
            if (!taken) {
                if (publish_will && will) {
                    duration delay = std::chrono::seconds(std::min(will_delay, expiry));
                    if (delay <= duration::zero()) {
                        broker_route(*b, *cfg, *will, std::string_view());
                    } else {
                        async::go(broker_delayed_will(b, cfg, s, generation, *will, delay));
                    }
                }
                if (expiry == 0) {
                    {
                        std::lock_guard g(s->lock);
                        s->gone = true;
                    }
                    std::lock_guard g(b->lock);
                    auto it = b->sessions.find(string(s->client_id));
                    if (it != b->sessions.end() && it->second == s) {
                        b->sessions.erase(it);
                    }
                } else if (expiry != UINT32_MAX) {
                    async::go(broker_expire(b, s, generation, std::chrono::seconds(expiry)));
                }
            }
            b->active.fetch_sub(1);
            b->unlink(c);
            b->running.done();
            co_return;
        }
    }

    // An MQTT broker (5.0 and 3.1.1): clients connect, subscribe and
    // publish; each message goes to the sessions whose filters match its
    // topic, at the lower of its QoS and the subscription's. Sessions
    // live in memory with their expiry, retained messages too; wills are
    // published when a connection ends without DISCONNECT (after their
    // delay); shared subscriptions get each message once per group. A
    // handle of one word: copies share the sessions. The fields are read
    // when serve() is called.
    //
    //     net::mqtt::broker b;
    //     b.serve(":1883");
    class broker {
    public:
        broker() noexcept
        : _impl(make_tracked<detail::BrokerImpl>()) {
        }

        broker(const broker&) = default;
        broker& operator=(const broker&) = default;

        // Listens on the address (":1883") and serves until shutdown() or
        // close(): then net::errc::server_closed
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // Listens over TLS from the first byte (port 8883)
        expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const {
            return async_serve_tls(address, c).wait();
        }

        async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const noexcept {
            return _co_serve_tls(_impl, _settings(), address, c);
        }

        // The connections of a listener the program made
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return _co_serve(_impl, _settings(), l);
        }

        // A WebSocket upgrade of an http::server's request (subprotocol
        // "mqtt", MQTT 5 §6), served until the connection ends; a request
        // that is no upgrade is answered and its error returned
        // `accept(...)` on this thread, `co_await async_accept(...)` in a task
        expected<void, io::error> accept(const http::request& r, const http::response_writer& w) const {
            return async_accept(r, w).wait();
        }

        async::task<expected<void, io::error>> async_accept(http::request r, http::response_writer w) const noexcept {
            return _co_accept(_impl, _settings(), std::move(r), std::move(w));
        }

        // A message to the subscribers of its topic, as from a client
        // (retained when it says so); errc::topic_invalid for a topic that
        // is none
        expected<void, io::error> publish(const message& m) const {
            if (!detail::mqtt_topic_valid(m.topic.view())) {
                return unexpected(detail::mqtt_error(errc::topic_invalid, "mqtt publish", m.topic));
            }
            detail::broker_route(*_impl, *_settings(), m, std::string_view());
            return {};
        }

        // Gracefully: the listeners closed, every connection told DISCONNECT
        // (0x8B, server shutting down) and closed; returns when all have ended
        void shutdown() const {
            async_shutdown().wait();
        }

        async::task<> async_shutdown() const noexcept {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed
        void close() const {
            _impl->shutting_down.store(true);
            _impl->closed.store(true);
            _impl->close_listeners();
            for (auto& n : _impl->snapshot()) {
                n->state.store(detail::BrokerConn::closed);
                (void)n->link->close();
            }
        }

        // The connections being served
        size_t connections() const noexcept {
            return _impl->active.load();
        }

        function<bool(const string& client_id, const string& user, const string& password)> authenticate;   // empty: everyone
        function<bool(const string& client_id, const string& user, const string& topic, bool subscribe)> authorize;   // empty: everything
        mqtt::qos maximum_qos = mqtt::qos::exactly_once;   // the most QoS taken and granted
        bool retain_available = true;                      // retained messages kept
        uint32_t maximum_packet_size = 1 << 20;            // the largest packet taken
        uint16_t receive_maximum = 1024;                   // QoS 2 publications a client may have in flight to the broker
        uint16_t topic_alias_maximum = 64;                 // aliases a client may use
        duration max_session_expiry = 24 * hour;           // the longest a session is kept after its connection
        duration max_keep_alive = {};                      // the longest keep alive taken; zero: the client's
        size_t max_queued = 1000;                          // messages queued per session while it is offline or slow
        size_t max_connections = 0;                        // zero: no limit
        function<void(const string&)> on_error;            // an accept's failure; a line on stderr by default

    private:
        tracked_ptr<detail::BrokerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::BrokerSettings>();
            cfg->authenticate = authenticate;
            cfg->authorize = authorize;
            cfg->maximum_qos = uint8_t(maximum_qos);
            cfg->retain_available = retain_available;
            cfg->maximum_packet_size = maximum_packet_size ? maximum_packet_size : (1 << 20);
            cfg->receive_maximum = receive_maximum ? receive_maximum : 1;
            cfg->topic_alias_maximum = topic_alias_maximum;
            cfg->max_session_expiry = detail::mqtt_seconds(max_session_expiry);
            cfg->max_keep_alive = uint16_t(std::min<uint32_t>(detail::mqtt_seconds(max_keep_alive), 65535));
            cfg->max_queued = max_queued ? max_queued : 1;
            cfg->max_connections = max_connections;
            cfg->on_error = on_error;
            return cfg;
        }

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::BrokerImpl> impl, tracked_ptr<detail::BrokerSettings> cfg, net::listener l) noexcept {
            if (impl->shutting_down.load()) {
                (void)l.close();
                co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", l.local_endpoint().to_string()));
            }
            {
                std::lock_guard<std::mutex> g(impl->lock);
                impl->listeners.push_back(l);
            }
            if (impl->shutting_down.load()) {
                (void)l.close();
            }
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    if (impl->shutting_down.load()) {
                        co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", l.local_endpoint().to_string()));
                    }
                    if (!async::detail::runtime_exiting()) {
                        cfg->report(string("accept: ") + c.error().message());
                    }
                    co_return io::detail::fail(c);
                }
                impl->running.add();
                async::go(detail::broker_serve(impl, cfg, tracked_ptr<detail::MqttLink>(make_tracked<detail::MqttStreamLink>(*c))));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::BrokerImpl> impl, tracked_ptr<detail::BrokerSettings> cfg, string address) noexcept {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<expected<void, io::error>> _co_serve_tls(tracked_ptr<detail::BrokerImpl> impl, tracked_ptr<detail::BrokerSettings> cfg, string address,
                                                                    net::tls::config c) noexcept {
            auto l = co_await net::tls::async_listen(address, c);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<expected<void, io::error>> _co_accept(tracked_ptr<detail::BrokerImpl> impl, tracked_ptr<detail::BrokerSettings> cfg, http::request r,
                                                                 http::response_writer w) noexcept {
            if (impl->shutting_down.load()) {
                co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", string()));
            }
            http::websocket::options o;
            o.subprotocols.push_back(string("mqtt"));
            o.origins.push_back(string("*"));
            o.max_message_bytes = cfg->maximum_packet_size + 8;
            auto ws = co_await http::websocket::async_accept(r, w, o);
            if (!ws) {
                co_return io::detail::fail(ws);
            }
            impl->running.add();
            co_await detail::broker_serve(impl, cfg, tracked_ptr<detail::MqttLink>(make_tracked<detail::MqttWsLink>(*ws)));
            co_return expected<void, io::error>();
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::BrokerImpl> impl) noexcept {
            impl->shutting_down.store(true);
            impl->close_listeners();
            for (auto& n : impl->snapshot()) {
                int idle = detail::BrokerConn::idle;
                if (n->state.compare_exchange_strong(idle, detail::BrokerConn::closed)) {
                    if (n->level >= 5) {
                        detail::MqttPacket d;
                        d.type = detail::packet::disconnect;
                        d.reason = 0x8B;
                        (void)co_await detail::broker_write(n, detail::mqtt_encode(d, 5));
                    }
                    n->ended.store(true);
                    (void)n->link->close();
                }
            }
            co_await impl->running;
        }

        tracked_ptr<detail::BrokerImpl> _impl;
    };
}
