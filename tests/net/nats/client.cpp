//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::nats's client against the test server (server.h), and its protocol
// pieces on their own: subjects, the header block, nkeys.
#include "tests/types.h"
#include "server.h"

#include <set>

using namespace nats_test;

namespace {
    // A user's nkey seed made of 32 bytes, as `nk -gen user` writes one
    std::string seed_text(uint8_t fill) {
        uint8_t raw[36];
        raw[0] = nd::NkeySeedPrefix | (nd::NkeyUserPrefix >> 5);
        raw[1] = uint8_t((nd::NkeyUserPrefix & 31) << 3);
        for (int i = 0; i < 32; ++i) {
            raw[2 + i] = uint8_t(fill + i);
        }
        uint16_t crc = nd::nats_crc16(raw, 34);
        raw[34] = uint8_t(crc);
        raw[35] = uint8_t(crc >> 8);
        return str(nd::NkeyBase32.encode(slice<const byte>(reinterpret_cast<const byte*>(raw), 36)));
    }
}

TEST(NatsProtocol, SubjectsHeadersNkeys) {
    EXPECT_TRUE(nd::nats_subject_valid("orders.new", false));
    EXPECT_FALSE(nd::nats_subject_valid("orders.*", false));
    EXPECT_TRUE(nd::nats_subject_valid("orders.*", true));
    EXPECT_TRUE(nd::nats_subject_valid("orders.>", true));
    EXPECT_FALSE(nd::nats_subject_valid("orders.>.x", true));
    EXPECT_FALSE(nd::nats_subject_valid("orders..new", true));
    EXPECT_FALSE(nd::nats_subject_valid("a b", true));
    EXPECT_FALSE(nd::nats_subject_valid("", true));
    EXPECT_FALSE(nd::nats_subject_valid("a.b*", true));
    EXPECT_TRUE(nd::nats_match("a.*.c", "a.b.c"));
    EXPECT_FALSE(nd::nats_match("a.*.c", "a.b.c.d"));
    EXPECT_TRUE(nd::nats_match("a.>", "a.b.c.d"));
    EXPECT_FALSE(nd::nats_match("a.>", "a"));
    EXPECT_TRUE(nd::nats_match(">", "a"));
    EXPECT_FALSE(nd::nats_match("a.b", "a.b.c"));
    // the header block, both ways
    nats::message m;
    m.subject = "s";
    m.headers = {{"Content-Type", "text/plain"}, {"X-Id", "1"}, {"X-Id", "2"}};
    std::string out;
    nd::nats_write_pub(out, m);
    EXPECT_EQ(out, "HPUB s 56 56\r\nNATS/1.0\r\nContent-Type: text/plain\r\nX-Id: 1\r\nX-Id: 2\r\n\r\n\r\n");
    nats::message back;
    ASSERT_TRUE(nd::nats_read_headers("NATS/1.0\r\nContent-Type: text/plain\r\nX-Id: 1\r\nX-Id: 2\r\n\r\n", back));
    EXPECT_TRUE(back.headers == m.headers);
    EXPECT_EQ(str(back.header("x-id")), "1");
    nats::message status;
    ASSERT_TRUE(nd::nats_read_headers("NATS/1.0 503 No Responders\r\n\r\n", status));
    EXPECT_EQ(status.status, 503);
    EXPECT_EQ(str(status.description), "No Responders");
    EXPECT_FALSE(nd::nats_read_headers("HTTP/1.1\r\n\r\n", status));
    EXPECT_FALSE(nd::nats_read_headers("NATS/1.0\r\nbad line\r\n\r\n", status));
    // nkeys: a seed read back, its public key a 'U', the CRC checked
    std::string seed = seed_text(7);
    EXPECT_EQ(seed.substr(0, 2), "SU");
    array<byte, 32> raw;
    uint8_t kind = 0;
    ASSERT_TRUE(nd::nats_seed(seed, raw, kind));
    EXPECT_EQ(kind, nd::NkeyUserPrefix);
    EXPECT_EQ(uint8_t(raw[0]), 7);
    std::string broken = seed;
    broken[10] = broken[10] == 'A' ? 'B' : 'A';
    EXPECT_FALSE(nd::nats_seed(broken, raw, kind));
    sgcl::string pub, sig;
    ASSERT_TRUE(nd::nats_sign_nonce(seed, "nonce", pub, sig));
    EXPECT_EQ(str(pub)[0], 'U');
    EXPECT_EQ(pub.size(), 56u);
    EXPECT_EQ(sig.size(), 86u);   // 64 bytes in base64url without padding
}

TEST(NatsClient, PublishSubscribe) {
    Server s;
    auto nc = nats::client::connect(s.url());
    ASSERT_TRUE(nc) << str(nc.error().message());
    EXPECT_EQ(str(nc->server_id()), "NTEST");
    EXPECT_EQ(nc->max_payload(), size_t(1) << 20);
    auto all = nc->subscribe("orders.>").value();
    auto one = nc->subscribe("orders.*.paid").value();
    ASSERT_TRUE(nc->flush());
    ASSERT_TRUE(nc->publish("orders.42.paid", "{\"id\":42}"));
    ASSERT_TRUE(nc->publish("orders.43.new", "{\"id\":43}"));
    ASSERT_TRUE(nc->publish("other", "x"));
    auto a1 = all.receive().value();
    EXPECT_EQ(str(a1.subject), "orders.42.paid");
    EXPECT_EQ(str(a1.data), "{\"id\":42}");
    EXPECT_EQ(str(all.receive()->subject), "orders.43.new");
    auto o1 = one.receive().value();
    EXPECT_EQ(str(o1.subject), "orders.42.paid");
    ASSERT_TRUE(nc->flush());
    EXPECT_FALSE(one.try_receive());
    // headers, a reply subject, an empty payload, binary data
    nats::message m;
    m.subject = "orders.1.paid";
    m.reply = "answers";
    m.data = sgcl::string(std::string("\0\r\n\xff", 4));
    m.headers = {{"Trace-Id", "abc"}};
    ASSERT_TRUE(nc->publish(m));
    auto h = one.receive().value();
    EXPECT_EQ(str(h.reply), "answers");
    EXPECT_EQ(h.data.size(), 4u);
    EXPECT_EQ(str(h.header("trace-id")), "abc");
    ASSERT_TRUE(nc->publish("orders.2.paid", ""));
    EXPECT_EQ(one.receive()->data.size(), 0u);
    // refusals of the client's own
    EXPECT_EQ(nc->publish("orders.*", "x").error().code(), nats::errc::invalid_subject);
    EXPECT_EQ(nc->publish("a b", "x").error().code(), nats::errc::invalid_subject);
    EXPECT_EQ(nc->subscribe("a..b").error().code(), nats::errc::invalid_subject);
    EXPECT_EQ(nc->publish("big", sgcl::string(std::string((1 << 20) + 1, 'x'))).error().code(), nats::errc::max_payload);
    // unsubscribe: at once (what came before stays receivable), and after a count of the
    // subscription's messages, as the server counts them
    ASSERT_TRUE(all.unsubscribe());
    EXPECT_EQ(str(all.receive()->subject), "orders.1.paid");
    EXPECT_EQ(str(all.receive()->subject), "orders.2.paid");
    EXPECT_FALSE(all.receive());
    EXPECT_FALSE(all.unsubscribe());
    ASSERT_TRUE(one.unsubscribe());
    one = nc->subscribe("orders.*.paid").value();
    ASSERT_TRUE(one.unsubscribe(2));
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(nc->publish("orders.9.paid", sgcl::string(std::to_string(i))));
    }
    EXPECT_EQ(str(one.receive()->data), "0");
    EXPECT_EQ(str(one.receive()->data), "1");
    EXPECT_FALSE(one.receive());
    ASSERT_TRUE(nc->flush());
    EXPECT_EQ(s.subscriptions(), 0u);
    nc->close();
    EXPECT_FALSE(nc->publish("x", "y"));
}

TEST(NatsClient, QueueGroupsAndRequests) {
    Server s;
    auto nc = nats::client::connect(s.url()).value();
    auto w1 = nc.subscribe("jobs", "workers").value();
    auto w2 = nc.subscribe("jobs", "workers").value();
    ASSERT_TRUE(nc.flush());
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(nc.publish("jobs", sgcl::string(std::to_string(i))));
    }
    ASSERT_TRUE(nc.flush());
    int n1 = 0, n2 = 0;
    while (w1.try_receive()) {
        ++n1;
    }
    while (w2.try_receive()) {
        ++n2;
    }
    EXPECT_EQ(n1 + n2, 10);   // each message to one member
    EXPECT_GT(n1, 0);
    EXPECT_GT(n2, 0);
    // request and reply: a responder on its own connection
    auto rc = nats::client::connect(s.url()).value();
    auto service = rc.subscribe("time.now").value();
    ASSERT_TRUE(rc.flush());
    auto responder = async::spawn([](nats::client rc, nats::subscription service) -> async::task<int> {
        int n = 0;
        for (;;) {
            auto m = co_await service.async_receive();
            if (!m) {
                co_return n;
            }
            ++n;
            (void)co_await rc.async_respond(*m, sgcl::string("re: " + str(m->data)));
        }
    }(rc, service));
    auto r = nc.request("time.now", "ping").value();
    EXPECT_EQ(str(r.data), "re: ping");
    // many at once over the one inbox
    vector<async::task<bool>> all;
    for (int i = 0; i < 50; ++i) {
        all.push_back(async::spawn([](nats::client nc, int i) -> async::task<bool> {
            auto r = co_await nc.async_request(sgcl::string("time.now"), sgcl::string(std::to_string(i)));
            co_return r && str(r->data) == "re: " + std::to_string(i);
        }(nc, i)));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
    // no one listening: no responders at once, not a timeout
    auto start = sgcl::clock::now();
    auto none = nc.request("nobody.home", "x", std::chrono::seconds(5));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), nats::errc::no_responders);
    EXPECT_LT(sgcl::clock::now() - start, std::chrono::seconds(2));
    // a responder that never answers: the timeout
    auto mute = rc.subscribe("mute").value();
    ASSERT_TRUE(rc.flush());
    auto late = nc.request("mute", "x", std::chrono::milliseconds(200));
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    EXPECT_FALSE(nc.respond(nats::message("s", "d"), "x"));   // no reply subject
    // drain: what came before stays receivable, then the end
    ASSERT_TRUE(rc.drain());
    EXPECT_EQ(responder.wait(), 51);
    EXPECT_FALSE(rc.publish("x", "y"));
}

TEST(NatsClient, Authentication) {
    {
        Server s;
        s.user = "alice";
        s.password = "s3cret";
        EXPECT_TRUE(nats::client::connect(s.url("alice:s3cret@")));
        auto bad = nats::client::connect(s.url("alice:wrong@"));
        ASSERT_FALSE(bad);
        EXPECT_EQ(bad.error().code(), nats::errc::authorization_violation);
        nats::client::options o;
        o.user = "alice";
        o.password = "s3cret";
        o.name = "tester";
        EXPECT_TRUE(nats::client::connect(s.url(), o));
        std::lock_guard g(s.mu);
        EXPECT_NE(s.last_connect.find("\"name\":\"tester\""), std::string::npos);
    }
    {
        Server s;
        s.token = "t0ken";
        EXPECT_TRUE(nats::client::connect(s.url("t0ken@")));
        EXPECT_FALSE(nats::client::connect(s.url()));
    }
    {
        Server s;
        std::string seed = seed_text(42);
        sgcl::string pub, sig;
        ASSERT_TRUE(nd::nats_sign_nonce(seed, "x", pub, sig));
        s.nkey = str(pub);
        nats::client::options o;
        o.nkey_seed = sgcl::string(seed);
        EXPECT_TRUE(nats::client::connect(s.url(), o));
        o.nkey_seed = sgcl::string(seed_text(43));   // another key
        auto other = nats::client::connect(s.url(), o);
        ASSERT_FALSE(other);
        EXPECT_EQ(other.error().code(), nats::errc::authorization_violation);
        o.nkey_seed = "SUBROKEN";
        EXPECT_FALSE(nats::client::connect(s.url(), o));
    }
}

TEST(NatsClient, ServerErrorsAndLiveness) {
    Server s;
    s.deny_subscribe = "secret.stuff";
    auto nc = nats::client::connect(s.url()).value();
    auto denied = nc.subscribe("secret.stuff").value();
    auto r = denied.receive();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), nats::errc::permissions_violation);
    EXPECT_TRUE(nc.flush());   // the connection goes on
    // a payload past the server's limit, which the client did not know: the server ends the connection
    s.max_payload = 1 << 20;
    // a server that stops answering PINGs: stale after max_pings_out
    nats::client::options o;
    o.ping_interval = std::chrono::milliseconds(100);
    o.max_pings_out = 2;
    auto live = nats::client::connect(s.url(), o).value();
    auto sub = live.subscribe("x").value();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_TRUE(live.flush());   // the PINGs answered: alive
    s.silent = true;
    auto end = sub.receive();   // the subscription ends with the connection
    ASSERT_FALSE(end);
    EXPECT_NE(str(end.error().path()).find("stale"), std::string::npos);
    // not a NATS server
    auto l = net::tcp::listen("127.0.0.1:0").value();
    auto other = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            std::string hello = "SSH-2.0-OpenSSH_9.0\r\n";
            (void)co_await c->async_write(slice<const byte>(reinterpret_cast<const byte*>(hello.data()), hello.size()));
            co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(100));
            (void)c->close();
        }
    }(l));
    auto bad = nats::client::connect(l.local_endpoint().to_string());
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), nats::errc::malformed);
    other.wait();
    EXPECT_EQ(nats::client::connect("http://127.0.0.1:1").error().code(), net::errc::unsupported_scheme);
    EXPECT_FALSE(nats::client::connect("127.0.0.1:1"));
    nats::client none;
    EXPECT_FALSE(none);
}

TEST(NatsClient, SlowConsumerAndManyPublishers) {
    Server s;
    nats::client::options o;
    o.queue = 10;
    auto nc = nats::client::connect(s.url(), o).value();
    auto sub = nc.subscribe("flood").value();
    ASSERT_TRUE(nc.flush());
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(nc.publish("flood", "x"));
    }
    ASSERT_TRUE(nc.flush());
    EXPECT_EQ(sub.dropped(), 90u);   // past the queue of 10, dropped as NATS's clients drop
    int kept = 0;
    while (sub.try_receive()) {
        ++kept;
    }
    EXPECT_EQ(kept, 10);
    // publishers in many tasks, one subscriber: every message whole
    auto many = nats::client::connect(s.url()).value();
    auto in = many.subscribe("m.*").value();
    ASSERT_TRUE(many.flush());
    vector<async::task<bool>> pubs;
    for (int p = 0; p < 8; ++p) {
        pubs.push_back(async::spawn([](nats::client c, int p) -> async::task<bool> {
            for (int i = 0; i < 500; ++i) {
                if (!co_await c.async_publish(sgcl::string("m." + std::to_string(p)), sgcl::string(std::string(100, char('a' + p))))) {
                    co_return false;
                }
            }
            co_return true;
        }(many, p)));
    }
    for (auto& p : pubs) {
        EXPECT_TRUE(p.wait());
    }
    ASSERT_TRUE(many.flush());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int got = 0;
    bool whole = true;
    while (auto m = in.try_receive()) {
        ++got;
        char c = str(m->subject).back() - '0' + 'a';
        whole &= m->data.size() == 100 && str(m->data) == std::string(100, c);
    }
    EXPECT_EQ(got, 4000);
    EXPECT_TRUE(whole);
}
