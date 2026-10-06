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
#include "../http/client.h"
#include "../http/websocket.h"
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/promise.h"
#include "../../async/select.h"
#include "../../async/semaphore.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/random.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

// The client of MQTT (5.0 and 3.1.1): a session with a broker over TCP,
// TLS or a WebSocket, publications of every QoS, subscriptions, the
// messages that come as a stream to receive()
namespace sgcl::net::mqtt {
    class client;

    namespace detail {
        // What came for an operation waiting on a packet identifier, or
        // the connection's end
        struct MqttAck {
            uint8_t type = 0;
            uint8_t reason = 0;
            std::vector<uint8_t> reasons;
            std::string reason_string;
            optional<io::error> error;
        };

        // The permits of QoS 1 and 2 publications in flight
        struct MqttPermits {
            async::semaphore sem;

            explicit MqttPermits(size_t n)
            : sem(n) {
            }
        };

        struct MqttClientState {
            tracked_ptr<MqttLink> link;
            async::mutex write_lock;
            std::mutex lock;   // what follows, never held across a wait
            map<uint16_t, async::promise<MqttAck>> pending;
            uint16_t next_id = 0;
            std::set<uint16_t> qos2_in;                     // PUBREC sent, PUBREL awaited
            std::map<uint16_t, std::string> aliases_in;     // the broker's topic aliases
            std::map<std::string, uint16_t> aliases_out;    // ours
            optional<io::error> end;
            async::channel<message> incoming;
            async::channel<bool> stopped;                   // closed at the end: the keep-alive's wake
            std::atomic<bool> closed = {false};
            std::atomic<int64_t> last_sent = {0};           // the clock's nanoseconds
            uint8_t level = 5;
            duration timeout = 30 * second;
            duration keep_alive = 60 * second;
            uint16_t alias_max_in = 0;                      // what we let the broker use
            uint16_t alias_max_out = 0;                     // what the broker lets us use
            bool use_aliases = true;
            uint16_t server_receive_max = 65535;
            uint8_t server_max_qos = 2;
            bool server_retain = true;
            uint32_t server_max_packet = 0;
            uint32_t max_packet = 0;                        // ours
            string client_id;
            bool session_present = false;
            tracked_ptr<MqttPermits> inflight;              // made with the broker's receive maximum (at most 1024)

            explicit MqttClientState(size_t queue)
            : incoming(queue ? queue : 1) {
            }
        };

        inline time_point mqtt_deadline(const MqttClientState& s) noexcept {
            return s.timeout > duration::zero() ? net::detail::no_deadline_at_max(sgcl::clock::now() + s.timeout) : time_point();
        }

        // Writes packets whole, in turn with every other writer of the session
        inline async::task<expected<void, io::error>> mqtt_send(tracked_ptr<MqttClientState> s, std::string data) noexcept {
            if (s->closed.load()) {
                std::lock_guard g(s->lock);
                co_return unexpected(s->end ? *s->end : io::error(io::errc::closed, "mqtt", string()));
            }
            auto guard = co_await s->write_lock.scoped_lock();
            auto w = co_await s->link->send(data);
            s->last_sent.store(sgcl::clock::now().time_since_epoch().count());
            co_return w;
        }

        // A PUBLISH written: its topic alias decided under the write lock,
        // so that the packet that sets an alias goes out before any that
        // uses it; a packet past the broker's maximum size refused
        inline async::task<expected<void, io::error>> mqtt_send_publish(tracked_ptr<MqttClientState> s, MqttPacket p) noexcept {
            if (s->closed.load()) {
                std::lock_guard g(s->lock);
                co_return unexpected(s->end ? *s->end : io::error(io::errc::closed, "mqtt", string()));
            }
            auto guard = co_await s->write_lock.scoped_lock();
            uint16_t added = 0;
            if (s->level >= 5 && s->use_aliases && s->alias_max_out) {
                std::lock_guard g(s->lock);
                auto it = s->aliases_out.find(p.topic);
                if (it != s->aliases_out.end()) {
                    p.props.topic_alias = it->second;
                    p.topic.clear();
                } else if (s->aliases_out.size() < s->alias_max_out) {
                    added = uint16_t(s->aliases_out.size() + 1);
                    p.props.topic_alias = added;
                }
            }
            std::string bytes = mqtt_encode(p, s->level);
            if (s->server_max_packet && bytes.size() > s->server_max_packet) {
                co_return unexpected(mqtt_error(errc::packet_too_large, "mqtt publish", 0x95));
            }
            auto w = co_await s->link->send(bytes);
            if (w && added) {
                std::lock_guard g(s->lock);
                s->aliases_out.emplace(p.topic, added);
            }
            s->last_sent.store(sgcl::clock::now().time_since_epoch().count());
            co_return w;
        }

        // The session's end: every waiter told, the stream of messages closed
        inline void mqtt_end(MqttClientState& s, io::error e) {
            map<uint16_t, async::promise<MqttAck>> waiting;
            {
                std::lock_guard g(s.lock);
                if (!s.end) {
                    s.end = e;
                }
                waiting = std::move(s.pending);
                s.pending = map<uint16_t, async::promise<MqttAck>>();
            }
            s.closed.store(true);
            for (auto& [id, p] : waiting) {
                MqttAck a;
                a.error = e;
                p.set_value(std::move(a));
            }
            s.incoming.close();
            s.stopped.close();
            (void)s.link->close();
        }

        inline async::task<expected<void, io::error>> mqtt_ack(tracked_ptr<MqttClientState> s, uint8_t type, uint16_t id) noexcept {
            MqttPacket p;
            p.type = type;
            p.id = id;
            co_return co_await mqtt_send(s, mqtt_encode(p, s->level));
        }

        // The reader: packets taken off the link and dispatched until the end
        inline async::task<> mqtt_read_loop(tracked_ptr<MqttClientState> s, std::string buf) noexcept {
            io::error end(io::errc::closed, "mqtt", string());
            for (;;) {
                size_t header = 0, total = 0;
                int f = mqtt_frame(buf, header, total);
                if (f < 0 || (f > 0 && s->max_packet && total > s->max_packet)) {
                    end = mqtt_error(f < 0 ? errc::malformed_packet : errc::packet_too_large, "mqtt", f < 0 ? 0x81 : 0x95);
                    break;
                }
                if (f == 0 || buf.size() < total) {
                    if (s->keep_alive > duration::zero()) {
                        s->link->read_deadline(sgcl::clock::now() + s->keep_alive * 2);
                    }
                    auto r = co_await s->link->fill(buf);
                    if (!r) {
                        end = r.error();
                        break;
                    }
                    if (*r == 0) {
                        end = io::error(io::errc::unexpected_eof, "mqtt", string());
                        break;
                    }
                    continue;
                }
                MqttPacket p;
                uint8_t reason = 0;
                bool ok = mqtt_decode(uint8_t(buf[0]), std::string_view(buf).substr(header, total - header), s->level, p, reason);
                buf.erase(0, total);
                if (!ok) {
                    end = mqtt_error(errc::malformed_packet, "mqtt", reason);
                    break;
                }
                if (p.type == packet::publish) {
                    std::string topic = p.topic;
                    if (p.props.topic_alias) {
                        uint16_t a = *p.props.topic_alias;
                        std::lock_guard g(s->lock);
                        if (a > s->alias_max_in) {
                            end = mqtt_error(errc::protocol_error, "mqtt", 0x94);
                            ok = false;
                        } else if (topic.empty()) {
                            auto it = s->aliases_in.find(a);
                            if (it == s->aliases_in.end()) {
                                end = mqtt_error(errc::protocol_error, "mqtt", 0x82);
                                ok = false;
                            } else {
                                topic = it->second;
                            }
                        } else {
                            s->aliases_in[a] = topic;
                        }
                    }
                    if (!ok) {
                        break;
                    }
                    bool deliver = true;
                    if (p.qos == 2) {
                        std::lock_guard g(s->lock);
                        deliver = s->qos2_in.insert(p.id).second;   // a duplicate of one held: not again
                    }
                    if (deliver) {
                        message m = mqtt_message_of(topic, p.payload, p.qos, p.retain, p.props);
                        if (!co_await s->incoming.send(std::move(m))) {
                            break;   // the stream was closed by the program's close()
                        }
                    }
                    if (p.qos == 1) {
                        (void)co_await mqtt_ack(s, packet::puback, p.id);
                    } else if (p.qos == 2) {
                        (void)co_await mqtt_ack(s, packet::pubrec, p.id);
                    }
                    continue;
                }
                if (p.type == packet::pubrel) {
                    {
                        std::lock_guard g(s->lock);
                        s->qos2_in.erase(p.id);
                    }
                    (void)co_await mqtt_ack(s, packet::pubcomp, p.id);
                    continue;
                }
                if (p.type == packet::puback || p.type == packet::pubrec || p.type == packet::pubcomp || p.type == packet::suback || p.type == packet::unsuback) {
                    optional<async::promise<MqttAck>> waiter;
                    {
                        std::lock_guard g(s->lock);
                        auto it = s->pending.find(p.id);
                        if (it != s->pending.end()) {
                            waiter = it->second;
                            s->pending.erase(it);
                        }
                    }
                    if (waiter) {
                        MqttAck a;
                        a.type = p.type;
                        a.reason = p.reason;
                        a.reasons = std::move(p.reasons);
                        a.reason_string = std::move(p.props.reason_string);
                        waiter->set_value(std::move(a));
                    } else if (p.type == packet::pubrec) {
                        // a PUBREC of nothing we sent: PUBREL so the broker lets go (§4.3.3)
                        MqttPacket rel;
                        rel.type = packet::pubrel;
                        rel.id = p.id;
                        rel.reason = 0x92;
                        (void)co_await mqtt_send(s, mqtt_encode(rel, s->level));
                    }
                    continue;
                }
                if (p.type == packet::pingresp) {
                    continue;
                }
                if (p.type == packet::disconnect) {
                    end = mqtt_error(errc::disconnected, "mqtt", p.reason, p.props.reason_string);
                    break;
                }
                end = mqtt_error(errc::protocol_error, "mqtt", 0x82);   // CONNACK again, AUTH, a packet of a server's
                break;
            }
            mqtt_end(*s, end);
        }

        // PINGREQ when the session has been quiet for most of its keep alive
        inline async::task<> mqtt_keep_alive(tracked_ptr<MqttClientState> s) noexcept {
            const duration every = s->keep_alive * 3 / 4;
            for (;;) {
                bool stop = false;
                co_await async::select(s->stopped.on_receive([&](optional<bool>) { stop = true; }), async::timeout(every, [] {}));
                if (stop || s->closed.load()) {
                    co_return;
                }
                int64_t quiet = sgcl::clock::now().time_since_epoch().count() - s->last_sent.load();
                if (quiet >= std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::nanoseconds(every)).count()) {
                    MqttPacket p;
                    p.type = packet::pingreq;
                    if (!co_await mqtt_send(s, mqtt_encode(p, s->level))) {
                        co_return;
                    }
                }
            }
        }

        // A packet identifier free now, and a promise waiting on it
        inline uint16_t mqtt_register(MqttClientState& s, const async::promise<MqttAck>& p) {
            std::lock_guard g(s.lock);
            if (s.end) {
                MqttAck a;   // the session ended: the wait answered at once
                a.error = *s.end;
                p.set_value(std::move(a));
                return 1;
            }
            for (;;) {
                if (++s.next_id == 0) {
                    s.next_id = 1;
                }
                if (s.pending.find(s.next_id) == s.pending.end()) {
                    s.pending.insert({s.next_id, p});
                    return s.next_id;
                }
            }
        }

        // The acknowledgement of an identifier, or the end, or the timeout
        inline async::task<MqttAck> mqtt_wait(tracked_ptr<MqttClientState> s, async::promise<MqttAck> p, uint16_t id) noexcept {
            if (s->timeout <= duration::zero()) {
                co_return co_await p;
            }
            bool late = false;
            co_await async::select(p.on_done([] {}), async::timeout(s->timeout, [&] { late = true; }));
            if (late && !p.done()) {
                {
                    std::lock_guard g(s->lock);
                    s->pending.erase(id);
                }
                MqttAck a;
                a.error = io::error(error_code(ETIMEDOUT, std::system_category()), "mqtt", string("no acknowledgement"));
                co_return a;
            }
            co_return p.result();
        }
    }

    // A session with an MQTT broker (5.0 or 3.1.1): connect makes it, then
    // publish, subscribe, unsubscribe, and receive takes the messages of the
    // subscriptions in the order they came. A handle of one word: a copy is
    // the same session; safe from many tasks and threads at once — every
    // call waits only for its own acknowledgement.
    //
    //     auto c = net::mqtt::client::connect("mqtt://broker.example.com");
    //     c->subscribe("sensors/+/temp", net::mqtt::qos::at_least_once);
    //     c->publish("sensors/kitchen/temp", "21.5");
    //     auto m = c->receive();
    //     c->disconnect();
    //
    // A reader task takes every packet off the connection: the messages to
    // a queue receive() reads (max_received of them; past it the reader
    // waits, and the broker's flow control holds the rest), the
    // acknowledgements to the calls waiting on them; a keep-alive task
    // sends PINGREQ when the session is quiet. QoS 1 and 2 publications
    // wait for their PUBACK, or PUBREC and PUBCOMP; a message received at
    // QoS 2 is given to receive() once. There is no reconnect: a session
    // that ends is connected again by the program (clean_start false and
    // a session_expiry keep its subscriptions and queued messages at the
    // broker).
    class client {
    public:
        // How a client connects and what it asks of the broker
        struct options {
            string client_id;                               // empty: the broker assigns one (v5), one made here (3.1.1)
            string user;                                    // from the URL's user name when it has one
            string password;
            mqtt::version version = mqtt::version::v5;
            bool clean_start = true;
            duration session_expiry = {};                   // v5: how long the broker keeps the session after the connection; zero: none
            duration keep_alive = 60 * second;              // zero: none
            optional<message> will;                         // published by the broker when the connection ends without disconnect()
            duration will_delay = {};                       // v5
            uint16_t receive_maximum = 65535;               // QoS 1 and 2 messages the broker may have in flight to us
            uint32_t maximum_packet_size = 0;               // the largest packet taken; 0: no limit
            uint16_t topic_alias_maximum = 16;              // aliases the broker may use toward us
            bool topic_aliases = true;                      // aliases used toward the broker, as many as it allows
            size_t max_received = 10000;                    // messages kept until receive(); past it the reader waits
            net::tls::config tls;                           // mqtts:// and wss://; the server's name the URL's host when none is set
            duration timeout = 30 * second;                 // the connection and CONNACK; then each acknowledgement's wait; zero: none
            async::stop_token stop;                         // the connect cancelled
        };

        client() noexcept = default;   // no session; an operation on it is a contract violation

        // A session with the broker of the URL: mqtt://host[:1883],
        // mqtts://host[:8883], ws://host[:80]/path or wss://host[:443]/path
        // (subprotocol "mqtt"), its user and password the credentials when
        // the options have none
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

        // The same over a connection there is (a tunnel, a pipe in memory)
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            return _co_connect(string(), std::move(transport), std::move(o));
        }

        // A message to the topic: QoS 0 written and gone, QoS 1 waited for
        // until PUBACK, QoS 2 until PUBCOMP. A topic with a wildcard or
        // none is errc::topic_invalid; a QoS or a retain the broker does not
        // take is errc::publish_refused (0x9B, 0x9A) without a packet sent
        // `publish(...)` on this thread, `co_await async_publish(...)` in a task
        expected<void, io::error> publish(const string& topic, const string& payload, mqtt::qos q = mqtt::qos::at_most_once, bool retain = false) const {
            return async_publish(message(topic, payload, q, retain)).wait();
        }

        async::task<expected<void, io::error>> async_publish(const string& topic, const string& payload, mqtt::qos q = mqtt::qos::at_most_once,
                                                             bool retain = false) const noexcept {
            return _co_publish(_s, message(topic, payload, q, retain));
        }

        expected<void, io::error> publish(const message& m) const {
            return async_publish(m).wait();
        }

        async::task<expected<void, io::error>> async_publish(const message& m) const noexcept {
            return _co_publish(_s, m);
        }

        // A subscription, and the QoS the broker granted; several in one
        // SUBSCRIBE (one per identifier when they differ), the granted QoS
        // of each; a filter refused is errc::subscribe_refused (its reason
        // code), the others staying subscribed
        // `subscribe(...)` on this thread, `co_await async_subscribe(...)` in a task
        expected<mqtt::qos, io::error> subscribe(const string& filter, mqtt::qos q = mqtt::qos::at_most_once) const {
            return async_subscribe(filter, q).wait();
        }

        async::task<expected<mqtt::qos, io::error>> async_subscribe(const string& filter, mqtt::qos q = mqtt::qos::at_most_once) const noexcept {
            return _co_subscribe_one(_s, filter, q);
        }

        expected<vector<mqtt::qos>, io::error> subscribe(const vector<subscription>& s) const {
            return async_subscribe(s).wait();
        }

        async::task<expected<vector<mqtt::qos>, io::error>> async_subscribe(vector<subscription> s) const noexcept {
            return _co_subscribe(_s, std::move(s));
        }

        // The subscriptions of the filters ended
        // `unsubscribe(...)` on this thread, `co_await async_unsubscribe(...)` in a task
        expected<void, io::error> unsubscribe(const string& filter) const {
            return async_unsubscribe(filter).wait();
        }

        async::task<expected<void, io::error>> async_unsubscribe(const string& filter) const noexcept {
            vector<string> one;
            one.push_back(filter);
            return _co_unsubscribe(_s, std::move(one));
        }

        expected<void, io::error> unsubscribe(const vector<string>& filters) const {
            return async_unsubscribe(filters).wait();
        }

        async::task<expected<void, io::error>> async_unsubscribe(vector<string> filters) const noexcept {
            return _co_unsubscribe(_s, std::move(filters));
        }

        // The next message of the subscriptions, waited for; the session's
        // end (its error: errc::disconnected with the broker's reason, the
        // connection's) once the messages before it are taken
        // `receive()` on this thread, `co_await async_receive()` in a task
        expected<message, io::error> receive() const {
            return async_receive().wait();
        }

        async::task<expected<message, io::error>> async_receive() const noexcept {
            return _co_receive(_s);
        }

        // The next message when one is there, at once
        optional<message> try_receive() const {
            return _s->incoming.try_receive();
        }

        // DISCONNECT (normal: the will dropped), then the connection closed
        // `disconnect()` on this thread, `co_await async_disconnect()` in a task
        expected<void, io::error> disconnect() const {
            return async_disconnect().wait();
        }

        async::task<expected<void, io::error>> async_disconnect() const noexcept {
            return _co_disconnect(_s);
        }

        // The connection closed without DISCONNECT: the broker publishes the will
        expected<void, io::error> close() const noexcept {
            auto r = _s->link->close();
            detail::mqtt_end(*_s, io::error(io::errc::closed, "mqtt", string()));
            return r;
        }

        // Whether the broker had a session of the client id (clean_start false)
        bool session_present() const noexcept {
            return _s->session_present;
        }

        // The client id: the options', or the one the broker assigned
        string client_id() const noexcept {
            return _s->client_id;
        }

        // Whether the session goes on: not disconnected, closed or ended
        bool is_connected() const noexcept {
            return !_s->closed.load();
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        explicit client(tracked_ptr<detail::MqttClientState> s) noexcept
        : _s(std::move(s)) {
        }

        static io::error _ended(const tracked_ptr<detail::MqttClientState>& s) {
            std::lock_guard g(s->lock);
            return s->end ? *s->end : io::error(io::errc::closed, "mqtt", string());
        }

        static async::task<expected<client, io::error>> _co_connect(string url, net::connection transport, options o) noexcept {
            using namespace detail;
            tracked_ptr<MqttLink> link;
            const time_point deadline = o.timeout > duration::zero() ? sgcl::clock::now() + o.timeout : time_point();
            if (!transport) {
                auto u = net::url::parse(url);
                if (!u || u->hostname().empty()) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_url, "mqtt", url));
                }
                std::string scheme(u->scheme().view());
                bool tls = scheme == "mqtts" || scheme == "wss";
                bool ws = scheme == "ws" || scheme == "wss";
                if (!tls && !ws && scheme != "mqtt") {
                    co_return unexpected(net::detail::net_error(net::errc::unsupported_scheme, "mqtt", url));
                }
                if (o.user.empty() && !u->username().empty()) {
                    o.user = string(net::detail::url_unescape(u->username().view()));
                }
                if (o.password.empty() && !u->password().empty()) {
                    o.password = string(net::detail::url_unescape(u->password().view()));
                }
                std::string host(u->hostname().view());
                net::tls::config tc = o.tls;
                if (tc.server_name.empty()) {
                    tc.server_name = string(host);
                }
                if (deadline != time_point()) {
                    tc.handshake_timeout = o.timeout;
                }
                if (ws) {
                    http::client hc;
                    hc.tls = tc;
                    http::websocket::options wo;
                    wo.subprotocols.push_back(string("mqtt"));
                    if (o.timeout > duration::zero()) {
                        wo.handshake_timeout = o.timeout;
                    }
                    wo.stop = o.stop;
                    std::string target = std::string(tls ? "wss" : "ws") + std::string(url.view()).substr(scheme.size());
                    auto w = co_await hc.async_websocket(string(target), wo);
                    if (!w) {
                        co_return unexpected(w.error());
                    }
                    link = make_tracked<MqttWsLink>(*w);
                } else {
                    if (host.find(':') != std::string::npos) {
                        host = "[" + host + "]";
                    }
                    std::string address = host + ":" + std::to_string(u->port() ? *u->port() : tls ? 8883 : 1883);
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
            }
            if (!link) {
                link = make_tracked<MqttStreamLink>(transport);
            }
            const uint8_t level = uint8_t(o.version);
            tracked_ptr s = make_tracked<MqttClientState>(o.max_received);
            s->link = link;
            s->level = level;
            s->timeout = o.timeout;
            s->keep_alive = o.keep_alive;
            s->alias_max_in = level >= 5 ? o.topic_alias_maximum : 0;
            s->use_aliases = o.topic_aliases;
            s->max_packet = o.maximum_packet_size;
            s->client_id = o.client_id;
            // CONNECT
            MqttPacket c;
            c.type = packet::connect;
            c.level = level;
            c.clean_start = o.clean_start;
            c.keep_alive = uint16_t(std::min<int64_t>(o.keep_alive.milliseconds() / 1000, 65535));
            std::string id(o.client_id.view());
            if (id.empty() && level < 5) {
                // 3.1.1 asks the client for an id: one of its own, random
                uint8_t r[8];
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(r), sizeof r));
                static constexpr char hex[] = "0123456789abcdef";
                id = "sgcl-";
                for (uint8_t b : r) {
                    id += hex[b >> 4];
                    id += hex[b & 15];
                }
                s->client_id = string(id);
            }
            c.client_id = id;
            if (level >= 5) {
                if (uint32_t e = mqtt_seconds(o.session_expiry)) {
                    c.props.session_expiry = e;
                }
                if (o.receive_maximum != 65535 && o.receive_maximum) {
                    c.props.receive_maximum = o.receive_maximum;
                }
                if (o.maximum_packet_size) {
                    c.props.maximum_packet_size = o.maximum_packet_size;
                }
                if (o.topic_alias_maximum) {
                    c.props.topic_alias_maximum = o.topic_alias_maximum;
                }
            }
            if (o.will) {
                const message& w = *o.will;
                if (!mqtt_topic_valid(w.topic.view())) {
                    (void)link->close();
                    co_return unexpected(mqtt_error(errc::topic_invalid, "mqtt connect", w.topic));
                }
                c.has_will = true;
                c.will_qos = uint8_t(w.qos);
                c.will_retain = w.retain;
                c.will_topic = std::string(w.topic.view());
                c.will_payload.assign(reinterpret_cast<const char*>(w.payload.data()), w.payload.size());
                mqtt_props_of(w, c.will_props);
                if (uint32_t d = mqtt_seconds(o.will_delay); d && level >= 5) {
                    c.will_props.will_delay = d;
                }
            }
            if (!o.user.empty()) {
                c.has_user = true;
                c.user = std::string(o.user.view());
            }
            if (!o.password.empty()) {
                c.has_password = true;
                c.password = std::string(o.password.view());
            }
            if (auto w = co_await link->send(mqtt_encode(c, level)); !w) {
                (void)link->close();
                co_return unexpected(w.error());
            }
            s->last_sent.store(sgcl::clock::now().time_since_epoch().count());
            // CONNACK
            std::string buf;
            if (deadline != time_point()) {
                link->read_deadline(deadline);
            }
            size_t header = 0, total = 0;
            for (;;) {
                int f = mqtt_frame(buf, header, total);
                if (f < 0) {
                    (void)link->close();
                    co_return unexpected(mqtt_error(errc::malformed_packet, "mqtt connect", 0x81));
                }
                if (f > 0 && buf.size() >= total) {
                    break;
                }
                auto r = co_await link->fill(buf);
                if (!r || *r == 0) {
                    (void)link->close();
                    co_return unexpected(r ? io::error(io::errc::unexpected_eof, "mqtt connect", string()) : r.error());
                }
            }
            MqttPacket a;
            uint8_t reason = 0;
            if (!mqtt_decode(uint8_t(buf[0]), std::string_view(buf).substr(header, total - header), level, a, reason) || a.type != packet::connack) {
                (void)link->close();
                if (a.type == packet::disconnect && level >= 5) {
                    co_return unexpected(mqtt_error(errc::refused, "mqtt connect", a.reason, a.props.reason_string));
                }
                co_return unexpected(mqtt_error(errc::protocol_error, "mqtt connect", 0x82));
            }
            buf.erase(0, total);
            uint8_t code = level >= 5 ? a.reason : mqtt_v3_reason(a.reason);
            if (code >= 0x80) {
                (void)link->close();
                co_return unexpected(mqtt_error(errc::refused, "mqtt connect", code, a.props.reason_string));
            }
            s->session_present = a.session_present;
            if (a.props.has_assigned_client_id) {
                s->client_id = string(a.props.assigned_client_id);
            }
            if (a.props.server_keep_alive) {
                s->keep_alive = std::chrono::seconds(*a.props.server_keep_alive);
            }
            if (a.props.receive_maximum) {
                s->server_receive_max = *a.props.receive_maximum;
            }
            if (a.props.maximum_qos) {
                s->server_max_qos = *a.props.maximum_qos;
            }
            if (a.props.retain_available) {
                s->server_retain = *a.props.retain_available != 0;
            }
            if (a.props.topic_alias_maximum) {
                s->alias_max_out = *a.props.topic_alias_maximum;
            }
            if (a.props.maximum_packet_size) {
                s->server_max_packet = *a.props.maximum_packet_size;
            }
            s->inflight = make_tracked<MqttPermits>(std::min<size_t>(s->server_receive_max, 1024));
            link->read_deadline(time_point());
            async::go(mqtt_read_loop(s, std::move(buf)));
            if (s->keep_alive > duration::zero()) {
                async::go(mqtt_keep_alive(s));
            }
            co_return client(s);
        }

        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            auto t = to.token();
            co_await async::select(from.on_stop([&] { to.request_stop(); }), t.on_stop([] {}));
        }

        static async::task<expected<void, io::error>> _co_publish(tracked_ptr<detail::MqttClientState> s, message m) noexcept {
            using namespace detail;
            if (!mqtt_topic_valid(m.topic.view())) {
                co_return unexpected(mqtt_error(errc::topic_invalid, "mqtt publish", m.topic));
            }
            if (uint8_t(m.qos) > s->server_max_qos || uint8_t(m.qos) > 2) {
                co_return unexpected(mqtt_error(errc::publish_refused, "mqtt publish", 0x9B));
            }
            if (m.retain && !s->server_retain) {
                co_return unexpected(mqtt_error(errc::publish_refused, "mqtt publish", 0x9A));
            }
            if (s->closed.load()) {
                co_return unexpected(_ended(s));
            }
            MqttPacket p = mqtt_publish_of(m);
            if (s->level < 5) {
                p.props = MqttProps();
            }
            if (m.qos == mqtt::qos::at_most_once) {
                co_return co_await mqtt_send_publish(s, std::move(p));
            }
            // QoS 1 and 2: a permit of the broker's receive maximum while in flight
            tracked_ptr<detail::MqttPermits> permits = s->inflight;
            (void)co_await permits->sem.acquire();
            async::promise<MqttAck> done;
            p.id = mqtt_register(*s, done);
            const uint16_t pid = p.id;
            expected<void, io::error> result;
            if (auto w = co_await mqtt_send_publish(s, std::move(p)); !w) {
                {
                    std::lock_guard g(s->lock);
                    s->pending.erase(pid);
                }
                result = unexpected(w.error());
            } else {
                MqttAck a = co_await mqtt_wait(s, done, pid);
                if (a.error) {
                    result = unexpected(*a.error);
                } else if (a.reason >= 0x80) {
                    result = unexpected(mqtt_error(errc::publish_refused, "mqtt publish", a.reason, a.reason_string));
                } else if (m.qos == mqtt::qos::exactly_once) {
                    if (a.type != packet::pubrec) {
                        result = unexpected(mqtt_error(errc::protocol_error, "mqtt publish", 0x82));
                    } else {
                        async::promise<MqttAck> complete;
                        {
                            std::lock_guard g(s->lock);
                            s->pending.insert({pid, complete});
                        }
                        MqttPacket rel;
                        rel.type = packet::pubrel;
                        rel.id = pid;
                        if (auto w2 = co_await mqtt_send(s, mqtt_encode(rel, s->level)); !w2) {
                            result = unexpected(w2.error());
                        } else {
                            MqttAck c = co_await mqtt_wait(s, complete, pid);
                            if (c.error) {
                                result = unexpected(*c.error);
                            } else if (c.reason >= 0x80) {
                                result = unexpected(mqtt_error(errc::publish_refused, "mqtt publish", c.reason, c.reason_string));
                            }
                        }
                    }
                } else if (a.type != packet::puback) {
                    result = unexpected(mqtt_error(errc::protocol_error, "mqtt publish", 0x82));
                }
            }
            permits->sem.release();
            co_return result;
        }

        static async::task<expected<mqtt::qos, io::error>> _co_subscribe_one(tracked_ptr<detail::MqttClientState> s, string filter, mqtt::qos q) noexcept {
            vector<subscription> one;
            subscription sub;
            sub.filter = filter;
            sub.qos = q;
            one.push_back(sub);
            auto r = co_await _co_subscribe(s, std::move(one));
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return (*r)[0];
        }

        static async::task<expected<vector<mqtt::qos>, io::error>> _co_subscribe(tracked_ptr<detail::MqttClientState> s, vector<subscription> subs) noexcept {
            using namespace detail;
            if (subs.empty()) {
                co_return vector<mqtt::qos>();
            }
            for (auto& x : subs) {
                if (!mqtt_filter_valid(x.filter.view())) {
                    co_return unexpected(mqtt_error(errc::topic_invalid, "mqtt subscribe", x.filter));
                }
            }
            // one SUBSCRIBE per identifier
            vector<mqtt::qos> granted(subs.size(), mqtt::qos::at_most_once);
            optional<io::error> refused;
            size_t done = 0;
            vector<bool> sent(subs.size(), false);
            while (done < subs.size()) {
                uint32_t ident = 0;
                bool first = true;
                MqttPacket p;
                p.type = packet::subscribe;
                vector<size_t> which;
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (sent[i]) {
                        continue;
                    }
                    if (first) {
                        ident = subs[i].identifier;
                        first = false;
                    } else if (subs[i].identifier != ident) {
                        continue;
                    }
                    sent[i] = true;
                    which.push_back(i);
                    const subscription& x = subs[i];
                    uint8_t o = uint8_t(x.qos) | (x.no_local ? 0x04 : 0) | (x.retain_as_published ? 0x08 : 0) | uint8_t(uint8_t(x.retain_handling) << 4);
                    p.subs.push_back(MqttSub{std::string(x.filter.view()), o});
                }
                if (ident && s->level >= 5) {
                    p.props.subscription_ids.push_back(ident);
                }
                done += which.size();
                async::promise<MqttAck> ack;
                p.id = mqtt_register(*s, ack);
                if (auto w = co_await mqtt_send(s, mqtt_encode(p, s->level)); !w) {
                    co_return unexpected(w.error());
                }
                MqttAck a = co_await mqtt_wait(s, ack, p.id);
                if (a.error) {
                    co_return unexpected(*a.error);
                }
                if (a.type != packet::suback || a.reasons.size() != which.size()) {
                    co_return unexpected(mqtt_error(errc::protocol_error, "mqtt subscribe", 0x82));
                }
                for (size_t k = 0; k < which.size(); ++k) {
                    uint8_t r = a.reasons[k];
                    if (r >= 0x80) {
                        if (!refused) {
                            refused = mqtt_error(errc::subscribe_refused, "mqtt subscribe", r, a.reason_string);
                        }
                    } else {
                        granted[which[k]] = mqtt::qos(r > 2 ? 2 : r);
                    }
                }
            }
            if (refused) {
                co_return unexpected(*refused);
            }
            co_return granted;
        }

        static async::task<expected<void, io::error>> _co_unsubscribe(tracked_ptr<detail::MqttClientState> s, vector<string> filters) noexcept {
            using namespace detail;
            if (filters.empty()) {
                co_return expected<void, io::error>();
            }
            MqttPacket p;
            p.type = packet::unsubscribe;
            for (auto& f : filters) {
                if (!mqtt_filter_valid(f.view())) {
                    co_return unexpected(mqtt_error(errc::topic_invalid, "mqtt unsubscribe", f));
                }
                p.filters.push_back(std::string(f.view()));
            }
            async::promise<MqttAck> ack;
            p.id = mqtt_register(*s, ack);
            if (auto w = co_await mqtt_send(s, mqtt_encode(p, s->level)); !w) {
                co_return unexpected(w.error());
            }
            MqttAck a = co_await mqtt_wait(s, ack, p.id);
            if (a.error) {
                co_return unexpected(*a.error);
            }
            if (a.type != packet::unsuback) {
                co_return unexpected(mqtt_error(errc::protocol_error, "mqtt unsubscribe", 0x82));
            }
            for (uint8_t r : a.reasons) {
                if (r >= 0x80) {
                    co_return unexpected(mqtt_error(errc::subscribe_refused, "mqtt unsubscribe", r, a.reason_string));
                }
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<message, io::error>> _co_receive(tracked_ptr<detail::MqttClientState> s) noexcept {
            auto m = co_await s->incoming.receive();
            if (!m) {
                co_return unexpected(_ended(s));
            }
            co_return std::move(*m);
        }

        static async::task<expected<void, io::error>> _co_disconnect(tracked_ptr<detail::MqttClientState> s) noexcept {
            using namespace detail;
            if (s->closed.load()) {
                co_return unexpected(_ended(s));
            }
            MqttPacket p;
            p.type = packet::disconnect;
            auto w = co_await mqtt_send(s, mqtt_encode(p, s->level));
            mqtt_end(*s, io::error(io::errc::closed, "mqtt", string()));
            co_return w;
        }

        tracked_ptr<detail::MqttClientState> _s;
    };
}
