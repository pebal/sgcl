//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::mqtt over its other transports and against bytes written apart: the
// broker behind an http::server's WebSocket route (subprotocol "mqtt") with
// the client over ws://, mqtts:// with the tree's test certificates, and a
// Python script that speaks MQTT packet by packet (written from the OASIS
// specifications, not from the module) and checks the broker's answers to
// both versions, QoS 2's four packets, a ping, a malformed packet, an
// unsupported level, a silent client past its keep alive.
#include "helpers.h"

using namespace mqtt_test;

TEST(MqttInterop, WebSocket) {
    mqtt::broker b;
    net::http::server srv;
    srv.route("/mqtt", [b](net::http::request r, net::http::response_writer w) -> async::task<> {
        (void)co_await b.async_accept(r, w);
    });
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto serving = async::spawn(srv.async_serve(*l));
    std::string url = "ws://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/mqtt";
    auto c = mqtt::client::connect(sgcl::string(url));
    ASSERT_TRUE(c) << str(c.error().message());
    ASSERT_TRUE(c->subscribe("ws/#", mqtt::qos::exactly_once));
    for (int q = 0; q < 3; ++q) {
        ASSERT_TRUE(c->publish("ws/t", sgcl::string(std::string(70000, char('a' + q))), mqtt::qos(q)));   // past one frame's 64 KB
        auto m = c->receive();
        ASSERT_TRUE(m);
        EXPECT_EQ(m->payload.size(), 70000u);
        EXPECT_EQ(m->qos, mqtt::qos(q));
    }
    // a TCP client of the same broker sees the WebSocket one's messages
    auto l2 = net::tcp::listen("127.0.0.1:0");
    auto tcp_serving = async::spawn(b.async_serve(*l2));
    auto t = mqtt::client::connect(sgcl::string("mqtt://127.0.0.1:" + std::to_string(l2->local_endpoint().port())));
    ASSERT_TRUE(t);
    t->subscribe("mixed");
    c->publish("mixed", "from ws");
    EXPECT_EQ(str(t->receive()->text()), "from ws");
    c->disconnect();
    t->disconnect();
    b.close();
    srv.close();
    serving.wait();
    tcp_serving.wait();
}

TEST(MqttInterop, Tls) {
    Broker b(nullptr, true);
    mqtt::client::options o;
    o.tls = client_tls();
    auto c = mqtt::client::connect(b.url("mqtts"), o);
    ASSERT_TRUE(c) << str(c.error().message());
    c->subscribe("secure", mqtt::qos::at_least_once);
    c->publish("secure", "over tls", mqtt::qos::at_least_once);
    EXPECT_EQ(str(c->receive()->text()), "over tls");
    // the system's roots do not know the test CA
    auto refused = mqtt::client::connect(b.url("mqtts"));
    EXPECT_FALSE(refused);
}

TEST(MqttInterop, PythonPacketLevel) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    Broker b([](mqtt::broker& x) { x.max_keep_alive = 0s; });
    std::string script = (source_root() / "tests/net/mqtt/python/conformance.py").string();
    int status = 0;
    std::string out = run_command("python3 " + script + " " + std::to_string(b.port()) + " 2>&1", &status);
    EXPECT_EQ(status, 0) << out;
    EXPECT_EQ(out,
              "v5 connack ok\n"
              "v5 suback ok\n"
              "v5 qos2 ok\n"
              "v5 delivery ok\n"
              "ping ok\n"
              "v311 connack ok\n"
              "v311 retained ok\n"
              "malformed ok\n"
              "level ok\n"
              "keepalive ok\n"
              "first packet ok\n")
        << out;
}
