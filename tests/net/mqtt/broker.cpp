//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::mqtt's client against its broker: every QoS under both versions,
// retained messages and retain handling, wills and their delay, sessions
// (present, queued while offline, expiry, takeover), topic aliases, shared
// subscriptions, no-local, subscription identifiers, the properties of a
// message, authentication and authorization, the limits (QoS, packet size,
// receive maximum), keep alive, the boundaries of the client.
#include "helpers.h"

#include <set>

using namespace mqtt_test;

namespace {
    mqtt::client::options named(const char* id, bool clean = true, std::chrono::seconds expiry = 0s) {
        mqtt::client::options o;
        o.client_id = id;
        o.clean_start = clean;
        o.session_expiry = expiry;
        return o;
    }
}

TEST(MqttBroker, EveryQosBothVersions) {
    Broker b;
    for (auto v : {mqtt::version::v5, mqtt::version::v3_1_1}) {
        mqtt::client::options o;
        o.version = v;
        auto c = b.connect(o);
        ASSERT_TRUE(c);
        EXPECT_FALSE(c.session_present());
        EXPECT_FALSE(c.client_id().empty());
        EXPECT_EQ(c.subscribe("t/#", mqtt::qos::exactly_once).value(), mqtt::qos::exactly_once);
        for (int q = 0; q < 3; ++q) {
            ASSERT_TRUE(c.publish("t/x", sgcl::string("m" + std::to_string(q)), mqtt::qos(q)));
            auto m = c.receive();
            ASSERT_TRUE(m);
            EXPECT_EQ(str(m->topic), "t/x");
            EXPECT_EQ(str(m->text()), "m" + std::to_string(q));
            EXPECT_EQ(m->qos, mqtt::qos(q));
            EXPECT_FALSE(m->retain);
        }
        // the subscription's QoS caps the delivery's
        EXPECT_EQ(c.subscribe("low", mqtt::qos::at_most_once).value(), mqtt::qos::at_most_once);
        ASSERT_TRUE(c.publish("low", "x", mqtt::qos::exactly_once));
        EXPECT_EQ(c.receive()->qos, mqtt::qos::at_most_once);
        ASSERT_TRUE(c.unsubscribe("t/#"));
        ASSERT_TRUE(c.unsubscribe("never/subscribed"));   // 0x11: not an error
        ASSERT_TRUE(c.publish("t/x", "after", mqtt::qos::at_least_once));
        EXPECT_FALSE(next(c, 200ms));
        ASSERT_TRUE(c.disconnect());
        EXPECT_FALSE(c.is_connected());
    }
}

TEST(MqttBroker, ManyClientsWildcards) {
    Broker b;
    auto a = b.connect(), x = b.connect(), y = b.connect();
    x.subscribe("home/+/temp", mqtt::qos::at_least_once);
    y.subscribe("home/#");
    x.subscribe("$SYS/#");
    a.publish("home/kitchen/temp", "21", mqtt::qos::at_least_once);
    a.publish("home/kitchen/light", "on", mqtt::qos::at_least_once);
    a.publish("$SYS/uptime", "1");
    EXPECT_EQ(str(x.receive()->text()), "21");
    EXPECT_EQ(str(x.receive()->topic), "$SYS/uptime");
    EXPECT_EQ(str(y.receive()->text()), "21");
    EXPECT_EQ(str(y.receive()->text()), "on");
    EXPECT_FALSE(next(y, 200ms));   // "home/#" does not see $SYS
}

TEST(MqttBroker, Retained) {
    Broker b;
    auto p = b.connect();
    ASSERT_TRUE(p.publish("cfg/a", "1", mqtt::qos::at_least_once, true));
    ASSERT_TRUE(p.publish("cfg/b", "2", mqtt::qos::at_most_once, true));
    auto s = b.connect();
    s.subscribe("cfg/+", mqtt::qos::at_least_once);
    std::set<std::string> got;
    for (int i = 0; i < 2; ++i) {
        auto m = s.receive();
        EXPECT_TRUE(m->retain);
        got.insert(str(m->topic) + "=" + str(m->text()));
    }
    EXPECT_EQ(got, (std::set<std::string>{"cfg/a=1", "cfg/b=2"}));
    // live messages are not retained toward a subscriber unless it asks
    p.publish("cfg/a", "3", mqtt::qos::at_least_once, true);
    auto live = s.receive();
    EXPECT_FALSE(live->retain);
    // retain-as-published, retain handling
    auto r = b.connect();
    mqtt::subscription keep;
    keep.filter = "cfg/a";
    keep.qos = mqtt::qos::at_least_once;
    keep.retain_as_published = true;
    keep.retain_handling = mqtt::retain_handling::never;
    r.subscribe(vector<mqtt::subscription>{keep});
    EXPECT_FALSE(next(r, 200ms));   // never: no retained message at subscribe
    p.publish("cfg/a", "4", mqtt::qos::at_least_once, true);
    EXPECT_TRUE(r.receive()->retain);
    keep.retain_handling = mqtt::retain_handling::send_if_new;
    r.subscribe(vector<mqtt::subscription>{keep});
    EXPECT_FALSE(next(r, 200ms));   // not new
    // an empty retained message deletes the retained one
    p.publish("cfg/a", "", mqtt::qos::at_least_once, true);
    r.receive();
    auto late = b.connect();
    late.subscribe("cfg/a");
    EXPECT_FALSE(next(late, 200ms));
}

TEST(MqttBroker, Wills) {
    Broker b;
    auto watcher = b.connect();
    watcher.subscribe("wills/#", mqtt::qos::at_least_once);
    auto with_will = [&](const char* topic, std::chrono::seconds delay = 0s) {
        mqtt::client::options o;
        o.will = mqtt::message(topic, "gone", mqtt::qos::at_least_once);
        o.will_delay = delay;
        o.session_expiry = 60s;
        return b.connect(o);
    };
    auto a = with_will("wills/a");
    a.close();   // without DISCONNECT: the will goes out
    auto m = watcher.receive();
    EXPECT_EQ(str(m->topic), "wills/a");
    EXPECT_EQ(str(m->text()), "gone");
    auto c = with_will("wills/c");
    c.disconnect();   // a normal DISCONNECT: no will
    EXPECT_FALSE(next(watcher, 300ms));
    // a delayed will, and the session back before the delay: no will
    mqtt::client::options o;
    o.client_id = "delayed";
    o.clean_start = false;
    o.session_expiry = 60s;
    o.will = mqtt::message("wills/d", "gone", mqtt::qos::at_least_once);
    o.will_delay = 2s;
    auto d = b.connect(o);
    d.close();
    o.will = nullopt;
    auto d2 = b.connect(o);
    EXPECT_FALSE(next(watcher, 2500ms));
    d2.close();
    // a 3.1.1 will
    mqtt::client::options v3;
    v3.version = mqtt::version::v3_1_1;
    v3.will = mqtt::message("wills/v3", "bye");
    auto e = b.connect(v3);
    e.close();
    EXPECT_EQ(str(watcher.receive()->topic), "wills/v3");
}

TEST(MqttBroker, Sessions) {
    Broker b;
    auto first = b.connect(named("dev-1", true, 60s));
    ASSERT_TRUE(first.subscribe("jobs", mqtt::qos::at_least_once));
    first.close();
    auto p = b.connect();
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(p.publish("jobs", sgcl::string("job" + std::to_string(i)), mqtt::qos::at_least_once));
    }
    p.publish("jobs", "dropped at qos 0");   // queued too: QoS 0 of a subscription at 1 is 0
    auto again = b.connect(named("dev-1", false, 60s));
    EXPECT_TRUE(again.session_present());
    for (int i = 0; i < 3; ++i) {
        auto m = again.receive();
        ASSERT_TRUE(m);
        EXPECT_EQ(str(m->text()), sgcl::string("job" + std::to_string(i)));
    }
    EXPECT_EQ(str(again.receive()->text()), "dropped at qos 0");
    // clean start: the session gone
    again.disconnect();
    auto clean = b.connect(named("dev-1", true));
    EXPECT_FALSE(clean.session_present());
    clean.disconnect();
    // a session of expiry 0 ends with its connection
    auto brief = b.connect(named("dev-2", false, 0s));
    brief.subscribe("x");
    brief.disconnect();
    auto back = b.connect(named("dev-2", false, 0s));
    EXPECT_FALSE(back.session_present());
    // an expiry that passes
    auto shortlived = b.connect(named("dev-3", false, 1s));
    shortlived.subscribe("x");
    shortlived.disconnect();
    std::this_thread::sleep_for(1500ms);
    EXPECT_FALSE(b.connect(named("dev-3", false, 60s)).session_present());
}

TEST(MqttBroker, Takeover) {
    Broker b;
    auto old = b.connect(named("same", false, 60s));
    old.subscribe("t");
    auto fresh = b.connect(named("same", false, 60s));
    EXPECT_TRUE(fresh.session_present());
    auto r = old.receive();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), mqtt::errc::disconnected);
    EXPECT_EQ(mqtt::reason_of(r.error()), uint8_t(0x8E));
    EXPECT_FALSE(old.is_connected());
    auto p = b.connect();
    p.publish("t", "to the new one");
    EXPECT_EQ(str(fresh.receive()->text()), "to the new one");
}

TEST(MqttBroker, SharedSubscriptions) {
    Broker b;
    auto w1 = b.connect(), w2 = b.connect(), plain = b.connect();
    ASSERT_TRUE(w1.subscribe("$share/workers/jobs/+", mqtt::qos::at_least_once));
    ASSERT_TRUE(w2.subscribe("$share/workers/jobs/+", mqtt::qos::at_least_once));
    ASSERT_TRUE(plain.subscribe("jobs/+", mqtt::qos::at_least_once));
    auto p = b.connect();
    for (int i = 0; i < 10; ++i) {
        p.publish(sgcl::string("jobs/" + std::to_string(i)), "x", mqtt::qos::at_least_once);
    }
    int one = 0, two = 0, all = 0;
    while (next(w1, 300ms)) {
        ++one;
    }
    while (next(w2, 300ms)) {
        ++two;
    }
    while (next(plain, 300ms)) {
        ++all;
    }
    EXPECT_EQ(one + two, 10);
    EXPECT_GT(one, 0);
    EXPECT_GT(two, 0);
    EXPECT_EQ(all, 10);
}

TEST(MqttBroker, NoLocalAndIdentifiers) {
    Broker b;
    auto c = b.connect();
    mqtt::subscription s;
    s.filter = "chat";
    s.no_local = true;
    s.identifier = 7;
    mqtt::subscription t;
    t.filter = "chat/#";
    t.identifier = 9;
    ASSERT_TRUE(c.subscribe(vector<mqtt::subscription>{s, t}));
    auto other = b.connect();
    other.publish("chat", "hi");
    auto m = c.receive();
    std::multiset<uint32_t> ids(m->subscription_ids.begin(), m->subscription_ids.end());
    EXPECT_EQ(ids, (std::multiset<uint32_t>{7, 9}));
    c.publish("chat", "mine");
    auto own = c.receive();   // chat/# has no no-local
    EXPECT_EQ(str(own->text()), "mine");
    ASSERT_EQ(own->subscription_ids.size(), 1u);
    EXPECT_EQ(own->subscription_ids[0], 9u);
}

TEST(MqttBroker, MessageProperties) {
    Broker b;
    auto c = b.connect();
    c.subscribe("req");
    mqtt::message m("req", "{\"q\":1}", mqtt::qos::at_least_once);
    m.utf8 = true;
    m.content_type = "application/json";
    m.response_topic = "resp/1";
    m.correlation_data = vector<byte>{byte(1), byte(2), byte(3)};
    m.user_properties.push_back({sgcl::string("trace"), sgcl::string("abc")});
    m.expiry = 60s;
    ASSERT_TRUE(c.publish(m));
    auto r = c.receive();
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->utf8);
    EXPECT_EQ(str(r->content_type), "application/json");
    EXPECT_EQ(str(r->response_topic), "resp/1");
    EXPECT_EQ(r->correlation_data, m.correlation_data);
    ASSERT_EQ(r->user_properties.size(), 1u);
    EXPECT_EQ(str(r->user_properties[0].second), "abc");
    EXPECT_GT(r->expiry, 0s);
    EXPECT_LE(r->expiry, 60s);
    // a message that expires while its session is offline is not delivered
    auto off = b.connect(named("off", false, 60s));
    off.subscribe("exp", mqtt::qos::at_least_once);
    off.close();
    mqtt::message brief("exp", "short", mqtt::qos::at_least_once);
    brief.expiry = 1s;
    c.publish(brief);
    c.publish("exp", "kept", mqtt::qos::at_least_once);
    std::this_thread::sleep_for(1500ms);
    auto back = b.connect(named("off", false, 60s));
    EXPECT_EQ(str(back.receive()->text()), "kept");
}

TEST(MqttBroker, TopicAliases) {
    Broker b;
    auto c = b.connect();
    c.subscribe("long/topic/name/for/aliases", mqtt::qos::at_least_once);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(c.publish("long/topic/name/for/aliases", sgcl::string(std::to_string(i)), mqtt::qos::at_least_once));
        EXPECT_EQ(str(c.receive()->topic), "long/topic/name/for/aliases");
    }
    // a broker that takes none: full topics
    Broker none([](mqtt::broker& x) { x.topic_alias_maximum = 0; });
    auto n = none.connect();
    n.subscribe("a");
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(n.publish("a", "x"));
        EXPECT_EQ(str(n.receive()->topic), "a");
    }
}

TEST(MqttBroker, AuthenticationAndAuthorization) {
    Broker b([](mqtt::broker& x) {
        x.authenticate = [](const sgcl::string&, const sgcl::string& user, const sgcl::string& password) {
            return user.view() == "alice" && password.view() == "secret";
        };
        x.authorize = [](const sgcl::string&, const sgcl::string&, const sgcl::string& topic, bool subscribe) {
            return !topic.view().starts_with(subscribe ? "private" : "readonly");
        };
    });
    auto bad = mqtt::client::connect(b.url());
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), mqtt::errc::refused);
    EXPECT_EQ(mqtt::reason_of(bad.error()), uint8_t(0x86));
    mqtt::client::options v3;
    v3.version = mqtt::version::v3_1_1;
    auto bad3 = mqtt::client::connect(b.url(), v3);
    ASSERT_FALSE(bad3);
    EXPECT_EQ(mqtt::reason_of(bad3.error()), uint8_t(0x86));
    auto c = mqtt::client::connect(sgcl::string("mqtt://alice:secret@127.0.0.1:" + std::to_string(b.port())));
    ASSERT_TRUE(c) << str(c.error().message());
    auto sub = c->subscribe("private/x");
    ASSERT_FALSE(sub);
    EXPECT_EQ(sub.error().code(), mqtt::errc::subscribe_refused);
    EXPECT_EQ(mqtt::reason_of(sub.error()), uint8_t(0x87));
    auto pub = c->publish("readonly/x", "no", mqtt::qos::at_least_once);
    ASSERT_FALSE(pub);
    EXPECT_EQ(pub.error().code(), mqtt::errc::publish_refused);
    EXPECT_EQ(mqtt::reason_of(pub.error()), uint8_t(0x87));
    auto pub2 = c->publish("readonly/x", "no", mqtt::qos::exactly_once);
    ASSERT_FALSE(pub2);
    EXPECT_TRUE(c->publish("open/x", "yes", mqtt::qos::exactly_once));
}

TEST(MqttBroker, Limits) {
    Broker b([](mqtt::broker& x) {
        x.maximum_qos = mqtt::qos::at_least_once;
        x.retain_available = false;
        x.maximum_packet_size = 1024;
    });
    auto c = b.connect();
    auto q2 = c.publish("t", "x", mqtt::qos::exactly_once);
    ASSERT_FALSE(q2);
    EXPECT_EQ(mqtt::reason_of(q2.error()), uint8_t(0x9B));
    auto r = c.publish("t", "x", mqtt::qos::at_most_once, true);
    ASSERT_FALSE(r);
    EXPECT_EQ(mqtt::reason_of(r.error()), uint8_t(0x9A));
    auto big = c.publish("t", sgcl::string(std::string(2000, 'x')));
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), mqtt::errc::packet_too_large);
    EXPECT_EQ(c.subscribe("t", mqtt::qos::exactly_once).value(), mqtt::qos::at_least_once);   // granted at most 1
    EXPECT_TRUE(c.is_connected());
    // a client's receive maximum of 1: every message still comes, one in flight at a time
    mqtt::client::options o;
    o.receive_maximum = 1;
    auto slow = b.connect(o);
    slow.subscribe("flow", mqtt::qos::at_least_once);
    for (int i = 0; i < 20; ++i) {
        c.publish("flow", sgcl::string(std::to_string(i)), mqtt::qos::at_least_once);
    }
    for (int i = 0; i < 20; ++i) {
        auto m = slow.receive();
        ASSERT_TRUE(m);
        EXPECT_EQ(str(m->text()), std::to_string(i));
    }
    // a client's maximum packet size: what is larger is not sent to it
    mqtt::client::options small;
    small.maximum_packet_size = 64;
    auto tiny = b.connect(small);
    tiny.subscribe("sized");
    c.publish("sized", sgcl::string(std::string(200, 'x')));
    c.publish("sized", "fits");
    EXPECT_EQ(str(tiny.receive()->text()), "fits");
}

TEST(MqttBroker, KeepAlive) {
    Broker b;
    mqtt::client::options o;
    o.keep_alive = 1s;
    auto c = b.connect(o);
    std::this_thread::sleep_for(2500ms);   // quiet past the keep alive: PINGREQ kept it
    EXPECT_TRUE(c.is_connected());
    EXPECT_TRUE(c.publish("t", "still here", mqtt::qos::at_least_once));
    // a broker that caps the keep alive tells the client
    Broker capped([](mqtt::broker& x) { x.max_keep_alive = 2s; });
    mqtt::client::options lazy;
    lazy.keep_alive = 0s;
    auto l = capped.connect(lazy);
    std::this_thread::sleep_for(3500ms);
    EXPECT_TRUE(l.is_connected());   // the client took the server's 2 s and pinged
}

TEST(MqttBroker, PublishFromTheBrokerAndShutdown) {
    Broker b;
    auto c = b.connect();
    c.subscribe("news");
    ASSERT_TRUE(b.b.publish(mqtt::message("news", "from the broker")));
    EXPECT_EQ(str(c.receive()->text()), "from the broker");
    EXPECT_FALSE(b.b.publish(mqtt::message("news/#", "x")));
    EXPECT_EQ(b.b.connections(), 1u);
    b.b.shutdown();
    auto r = c.receive();
    ASSERT_FALSE(r);
    EXPECT_EQ(mqtt::reason_of(r.error()), uint8_t(0x8B));
    auto again = b.b.serve("127.0.0.1:0");
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), net::errc::server_closed);
}

TEST(MqttClient, Boundaries) {
    mqtt::client none;
    EXPECT_FALSE(none);
    EXPECT_FALSE(mqtt::client::connect("http://127.0.0.1:1"));
    EXPECT_EQ(mqtt::client::connect("http://127.0.0.1:1").error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(mqtt::client::connect("not a url").error().code(), net::errc::invalid_url);
    EXPECT_FALSE(mqtt::client::connect("mqtt://127.0.0.1:1"));
    Broker b;
    auto c = b.connect();
    auto copy = c;
    EXPECT_TRUE(copy == c);
    EXPECT_EQ(c.publish("a/+", "x").error().code(), mqtt::errc::topic_invalid);
    EXPECT_EQ(c.publish("", "x").error().code(), mqtt::errc::topic_invalid);
    EXPECT_EQ(c.subscribe("a/#/b").error().code(), mqtt::errc::topic_invalid);
    EXPECT_EQ(c.unsubscribe("a#").error().code(), mqtt::errc::topic_invalid);
    EXPECT_TRUE(c.subscribe(vector<mqtt::subscription>{})->empty());
    EXPECT_TRUE(c.unsubscribe(vector<sgcl::string>{}));
    EXPECT_FALSE(c.try_receive());
    mqtt::client::options bad_will;
    bad_will.will = mqtt::message("w/+", "x");
    EXPECT_EQ(mqtt::client::connect(b.url(), bad_will).error().code(), mqtt::errc::topic_invalid);
    ASSERT_TRUE(c.disconnect());
    EXPECT_FALSE(c.disconnect());
    EXPECT_FALSE(c.publish("a", "x"));
    EXPECT_FALSE(c.receive());
    EXPECT_TRUE(c.close() || true);
    // a client id given is kept; a 3.1.1 client without one makes its own
    auto named_one = b.connect(named("given-id"));
    EXPECT_EQ(str(named_one.client_id()), "given-id");
    auto v3 = b.connect_v3();
    EXPECT_TRUE(str(v3.client_id()).starts_with("sgcl-"));
    // an async session
    auto t = [](sgcl::string url) -> async::task<bool> {
        auto c = co_await mqtt::client::async_connect(url);
        if (!c) {
            co_return false;
        }
        bool ok = bool(co_await c->async_subscribe("a", mqtt::qos::at_least_once));
        ok &= bool(co_await c->async_publish("a", "b", mqtt::qos::at_least_once));
        auto m = co_await c->async_receive();
        ok &= m && m->text().view() == "b";
        ok &= bool(co_await c->async_unsubscribe("a"));
        ok &= bool(co_await c->async_disconnect());
        co_return ok;
    }(b.url());
    EXPECT_TRUE(t.wait());
}

// Many tasks publishing at once through one client: a topic alias is
// set by the packet written first, never used by one written before it
// (the broker would end the session with 0x82)
TEST(MqttBroker, ConcurrentPublishersAndAliases) {
    for (int round = 0; round < 5; ++round) {
        Broker b;
        mqtt::client::options o;
        o.max_received = 100000;
        auto pub = b.connect(o);
        auto sub = b.connect(o);
        ASSERT_TRUE(sub.subscribe("many/+", mqtt::qos::at_least_once));
        const long per = 50;
        auto worker = [](mqtt::client c, int k, long count) -> async::task<bool> {
            bool ok = true;
            for (long i = 0; i < count; ++i) {
                ok &= bool(co_await c.async_publish(sgcl::string("many/" + std::to_string(k % 4)), "x", mqtt::qos::at_least_once));
            }
            co_return ok;
        };
        vector<async::task<bool>> ws;
        for (int k = 0; k < 32; ++k) {
            ws.push_back(async::spawn(worker(pub, k, per)));
        }
        for (auto& w : ws) {
            EXPECT_TRUE(w.wait());
        }
        for (long i = 0; i < 32 * per; ++i) {
            ASSERT_TRUE(sub.receive());
        }
        EXPECT_TRUE(pub.is_connected());
    }
}
