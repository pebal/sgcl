//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::amqp's client against the test broker (server.h) and its codec on
// its own: the frames and field tables of the specification by hand.
#include "tests/types.h"
#include "server.h"

#include <set>

using namespace amqp_test;

namespace {
    amqp::channel open(const amqp::client& c) {
        return c.open_channel().value();
    }

    std::string hex(std::string_view b) {
        static constexpr char d[] = "0123456789abcdef";
        std::string out;
        for (char x : b) {
            out += d[uint8_t(x) >> 4];
            out += d[uint8_t(x) & 15];
        }
        return out;
    }
}

TEST(AmqpCodec, FramesAndTables) {
    // queue.declare of "q" on channel 1, as the specification lays it out (§4.2.3, §1.9)
    std::string out;
    size_t at = ad::amqp_method_begin(out, 1, ad::m::queue_declare);
    ad::AmqpWriter w(out);
    w.u16(0);
    w.shortstr("q");
    w.bit(false);   // passive
    w.bit(true);    // durable
    w.bit(false);   // exclusive
    w.bit(true);    // auto-delete
    w.bit(false);   // no-wait
    w.table({});
    ad::amqp_frame_end(out, at);
    EXPECT_EQ(hex(out), "01" "0001" "0000000d" "0032000a" "0000" "0171" "0a" "00000000" "ce");
    // every type of a field table, written and read back
    amqp::table nested{{"n", amqp::field(int32_t(7))}};
    amqp::table t{{"none", amqp::field()},
                  {"bool", amqp::field(true)},
                  {"i8", amqp::field::int8(-5)},
                  {"u8", amqp::field::uint8(250)},
                  {"i16", amqp::field::int16(-30000)},
                  {"u16", amqp::field::uint16(60000)},
                  {"i32", amqp::field(int32_t(-2000000000))},
                  {"u32", amqp::field::uint32(4000000000u)},
                  {"i64", amqp::field(int64_t(-9000000000000000000))},
                  {"f", amqp::field::float32(1.5f)},
                  {"d", amqp::field(2.25)},
                  {"dec", amqp::field::decimal(2, 12345)},
                  {"s", amqp::field("text")},
                  {"x", amqp::field::bytes(sgcl::string(std::string("\0\1\2", 3)))},
                  {"t", amqp::field::timestamp(time::datetime::from_unix(1700000000, time::zone::utc()))},
                  {"a", amqp::field(vector<amqp::field>{amqp::field(int32_t(1)), amqp::field("two"), amqp::field(nested)})},
                  {"tbl", amqp::field(nested)}};
    std::string bytes;
    ad::AmqpWriter tw(bytes);
    tw.table(t);
    ad::AmqpReader tr(bytes);
    auto back = tr.table();
    ASSERT_TRUE(tr.ok);
    EXPECT_TRUE(tr.done());
    EXPECT_TRUE(back == t);
    EXPECT_EQ(*amqp::find(back, "dec")->as_double(), 123.45);
    EXPECT_EQ(amqp::find(back, "t")->as_timestamp()->unix(), 1700000000);
    EXPECT_EQ(*amqp::find(back, "u32")->as_int(), 4000000000);
    EXPECT_FALSE(amqp::find(back, "missing"));
    // the bytes of a table as RabbitMQ writes one: {"a": 'I' 1}
    ad::AmqpReader rr(std::string_view("\x00\x00\x00\x07\x01" "a" "I\x00\x00\x00\x01", 12));
    auto one = rr.table();
    ASSERT_TRUE(rr.ok);
    EXPECT_EQ(*amqp::find(one, "a")->as_int(), 1);
    // properties, every one of them
    amqp::properties p;
    p.content_type = "application/json";
    p.content_encoding = "gzip";
    p.headers = {{"k", amqp::field("v")}};
    p.delivery_mode = amqp::delivery_mode::persistent;
    p.priority = 5;
    p.correlation_id = "c1";
    p.reply_to = "r1";
    p.expiration = "60000";
    p.message_id = "m1";
    p.timestamp = time::datetime::from_unix(1700000001, time::zone::utc());
    p.type = "t1";
    p.user_id = "guest";
    p.app_id = "a1";
    std::string pb;
    ad::AmqpWriter pw(pb);
    pw.properties(p);
    ad::AmqpReader pr(pb);
    auto q = pr.properties();
    EXPECT_TRUE(pr.ok && pr.done());
    EXPECT_TRUE(q == p);
    // short and broken input reads as not ok, never past its end
    for (size_t n = 0; n < bytes.size(); ++n) {
        ad::AmqpReader cut(std::string_view(bytes).substr(0, n));
        (void)cut.table();
        EXPECT_FALSE(cut.ok);
    }
    amqp::field f = amqp::field(amqp::table{});
    for (int i = 0; i < 40; ++i) {
        f = amqp::field(amqp::table{{"a", f}});
    }
    std::string deep;
    ad::AmqpWriter dw(deep);
    dw.table({{"a", f}});
    ad::AmqpReader dr(deep);
    (void)dr.table();
    EXPECT_FALSE(dr.ok);   // past 32 tables in tables
}

TEST(AmqpClient, ConnectAndRefusals) {
    Broker b;
    auto c = amqp::client::connect(b.url());
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(*c);
    EXPECT_FALSE(c->blocked());
    auto wrong = amqp::client::connect(b.url("guest:nope@"));
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), amqp::errc::access_refused);
    auto vhost = amqp::client::connect(sgcl::string("amqp://guest:guest@127.0.0.1:" + std::to_string(b.port()) + "/other"));
    ASSERT_FALSE(vhost);
    EXPECT_EQ(vhost.error().code(), amqp::errc::not_allowed);
    EXPECT_EQ(amqp::client::connect("http://127.0.0.1/").error().code(), net::errc::unsupported_scheme);
    EXPECT_FALSE(amqp::client::connect("amqp://127.0.0.1:1/"));
    // a peer that is no AMQP broker: an HTTP server's answer
    auto l = net::tcp::listen("127.0.0.1:0").value();
    auto other = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            char buf[64];
            (void)co_await c->async_read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
            std::string v = std::string("AMQP\x00\x00\x09\x00", 8);
            (void)co_await c->async_write(slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size()));
            (void)c->close();
        }
    }(l));
    auto old = amqp::client::connect(sgcl::string("amqp://" + str(l.local_endpoint().to_string()) + "/"));
    ASSERT_FALSE(old);
    EXPECT_EQ(old.error().code(), amqp::errc::not_implemented);
    other.wait();
    EXPECT_TRUE(c->close());
    EXPECT_FALSE(c->open_channel());
    amqp::client none;
    EXPECT_FALSE(none);
}

TEST(AmqpClient, PublishAndGet) {
    Broker b;
    auto c = amqp::client::connect(b.url()).value();
    auto ch = open(c);
    auto q = ch.declare_queue("tasks").value();
    EXPECT_EQ(str(q.name), "tasks");
    EXPECT_EQ(q.messages, 0u);
    amqp::properties p;
    p.content_type = "text/plain";
    p.message_id = "id-1";
    p.headers = {{"attempt", amqp::field(int32_t(3))}};
    p.delivery_mode = amqp::delivery_mode::persistent;
    ASSERT_TRUE(ch.publish("", "tasks", "hello", p));
    ASSERT_TRUE(ch.publish("", "tasks", ""));
    // a body of several frames
    std::string big(300000, 'x');
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = char('a' + i % 26);
    }
    ASSERT_TRUE(ch.publish("", "tasks", sgcl::string(big)));
    EXPECT_EQ(ch.declare_queue("tasks", {.passive = true})->messages, 3u);
    auto d = ch.get("tasks").value();
    ASSERT_TRUE(d);
    EXPECT_EQ(str(d->body), "hello");
    EXPECT_TRUE(d->properties == p);
    EXPECT_EQ(str(d->routing_key), "tasks");
    EXPECT_EQ(d->message_count, 2u);
    EXPECT_FALSE(d->redelivered);
    ASSERT_TRUE(ch.ack(d->delivery_tag));
    auto empty = ch.get("tasks", true).value();
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->body.size(), 0u);
    auto large = ch.get("tasks", true).value();
    ASSERT_TRUE(large);
    EXPECT_EQ(str(large->body), big);
    EXPECT_FALSE(ch.get("tasks").value());   // get-empty
    // a server-named queue
    auto named = ch.declare_queue("", {.exclusive = true}).value();
    EXPECT_EQ(str(named.name).rfind("amq.gen-", 0), 0u);
    EXPECT_EQ(ch.purge_queue("tasks").value(), 0u);
    EXPECT_TRUE(ch.publish("", "tasks", "x"));
    EXPECT_EQ(ch.delete_queue("tasks").value(), 1u);
    EXPECT_TRUE(ch.close());
    EXPECT_FALSE(ch.publish("", "tasks", "x"));
    EXPECT_TRUE(c.close());
}

TEST(AmqpClient, ConsumeAckNackReject) {
    Broker b;
    auto c = amqp::client::connect(b.url()).value();
    auto ch = open(c);
    ch.declare_queue("jobs").value();
    ASSERT_TRUE(ch.qos(2));
    auto in = ch.consume("jobs").value();
    EXPECT_EQ(str(in.tag()).rfind("amq.ctag-", 0), 0u);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(ch.publish("", "jobs", sgcl::string("job " + std::to_string(i))));
    }
    auto d0 = in.receive().value();
    auto d1 = in.receive().value();
    EXPECT_EQ(str(d0.body), "job 0");
    EXPECT_EQ(str(d1.body), "job 1");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(in.try_receive());   // prefetch 2: no third until one is acked
    ASSERT_TRUE(ch.ack(d0.delivery_tag));
    auto d2 = in.receive().value();
    EXPECT_EQ(str(d2.body), "job 2");
    ASSERT_TRUE(ch.nack(d1.delivery_tag));   // back to the queue, redelivered
    auto again = in.receive().value();
    EXPECT_EQ(str(again.body), "job 1");
    EXPECT_TRUE(again.redelivered);
    ASSERT_TRUE(ch.reject(d2.delivery_tag, false));   // dropped
    ASSERT_TRUE(ch.ack(again.delivery_tag));
    std::set<std::string> rest;
    for (int i = 0; i < 2; ++i) {
        auto d = in.receive().value();
        rest.insert(str(d.body));
        ASSERT_TRUE(ch.ack(d.delivery_tag, true));
    }
    EXPECT_EQ(rest, (std::set<std::string>{"job 3", "job 4"}));
    ASSERT_TRUE(in.cancel());
    EXPECT_FALSE(in.receive());
    // an unknown delivery tag: the broker closes the channel (406), the connection goes on
    auto bad = open(c);
    ASSERT_TRUE(bad.ack(999));
    auto after = bad.declare_queue("jobs");
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), amqp::errc::precondition_failed);
    EXPECT_TRUE(open(c).declare_queue("jobs"));
}

TEST(AmqpClient, Exchanges) {
    Broker b;
    auto c = amqp::client::connect(b.url()).value();
    auto ch = open(c);
    for (auto q : {"a", "b", "c"}) {
        ch.declare_queue(q).value();
    }
    ASSERT_TRUE(ch.declare_exchange("logs", {.type = "fanout"}));
    ASSERT_TRUE(ch.bind_queue("a", "logs", ""));
    ASSERT_TRUE(ch.bind_queue("b", "logs", ""));
    ASSERT_TRUE(ch.publish("logs", "", "to all"));
    ASSERT_TRUE(ch.declare_exchange("events", {.type = "topic"}));
    ASSERT_TRUE(ch.bind_queue("a", "events", "order.*"));
    ASSERT_TRUE(ch.bind_queue("c", "events", "#.error"));
    ASSERT_TRUE(ch.publish("events", "order.created", "o1"));
    ASSERT_TRUE(ch.publish("events", "payment.card.error", "e1"));
    ASSERT_TRUE(ch.publish("events", "order.created.late", "none"));
    ASSERT_TRUE(ch.declare_exchange("match", {.type = "headers"}));
    ASSERT_TRUE(ch.bind_queue("b", "match", "", {{"x-match", amqp::field("all")}, {"format", amqp::field("pdf")}, {"type", amqp::field("report")}}));
    amqp::properties pdf;
    pdf.headers = {{"format", amqp::field("pdf")}, {"type", amqp::field("report")}};
    ASSERT_TRUE(ch.publish("match", "", "report.pdf", pdf));
    pdf.headers = {{"format", amqp::field("pdf")}};
    ASSERT_TRUE(ch.publish("match", "", "only-format", pdf));
    auto drain = [&](const char* q) {
        std::vector<std::string> out;
        while (auto d = ch.get(q, true).value()) {
            out.push_back(str(d->body));
        }
        return out;
    };
    EXPECT_EQ(drain("a"), (std::vector<std::string>{"to all", "o1"}));
    EXPECT_EQ(drain("b"), (std::vector<std::string>{"to all", "report.pdf"}));
    EXPECT_EQ(drain("c"), (std::vector<std::string>{"e1"}));
    ASSERT_TRUE(ch.unbind_queue("a", "logs", ""));
    ASSERT_TRUE(ch.publish("logs", "", "after unbind"));
    EXPECT_TRUE(drain("a").empty());
    // redeclared with another type: 406, the channel closed
    auto wrong = ch.declare_exchange("logs", {.type = "direct"});
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), amqp::errc::precondition_failed);
    EXPECT_FALSE(ch.declare_queue("a"));   // the channel is gone
    auto ch2 = open(c);
    auto missing = ch2.declare_exchange("nowhere", {.passive = true});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), amqp::errc::not_found);
    EXPECT_NE(str(missing.error().path()).find("nowhere"), std::string::npos);
    auto ch3 = open(c);
    ASSERT_TRUE(ch3.delete_exchange("logs"));
    // publication to an exchange that is not there: the channel closes
    ASSERT_TRUE(ch3.publish("logs", "", "x"));
    EXPECT_EQ(ch3.declare_queue("a").error().code(), amqp::errc::not_found);
}

TEST(AmqpClient, ReturnsAndConfirms) {
    Broker b;
    auto c = amqp::client::connect(b.url()).value();
    auto ch = open(c);
    ch.declare_queue("q").value();
    ASSERT_TRUE(ch.confirm());
    ASSERT_TRUE(ch.publish("", "q", "confirmed"));
    ASSERT_TRUE(ch.publish("", "nowhere", "lost", {}, {.mandatory = true}));   // acked, and given back
    auto r = ch.receive_returned().value();
    EXPECT_EQ(r.reply_code, 312);
    EXPECT_EQ(str(r.reply_text), "NO_ROUTE");
    EXPECT_EQ(str(r.routing_key), "nowhere");
    EXPECT_EQ(str(r.body), "lost");
    EXPECT_FALSE(ch.try_receive_returned());
    b.nack_publications = true;
    auto nacked = ch.publish("", "q", "refused");
    ASSERT_FALSE(nacked);
    EXPECT_EQ(nacked.error().code(), amqp::errc::nacked);
    b.nack_publications = false;
    // many at once from several tasks over one channel: each its own ack
    vector<async::task<bool>> all;
    for (int i = 0; i < 100; ++i) {
        all.push_back(async::spawn([](amqp::channel ch, int i) -> async::task<bool> {
            co_return bool(co_await ch.async_publish("", "q", sgcl::string(std::to_string(i))));
        }(ch, i)));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
    EXPECT_EQ(b.queue_depth("q"), 102u);
}

TEST(AmqpClient, ManyChannelsAndConsumers) {
    Broker b;
    auto c = amqp::client::connect(b.url()).value();
    auto setup = open(c);
    setup.declare_queue("work").value();
    constexpr int Consumers = 4, Messages = 2000;
    vector<async::task<int>> workers;
    std::atomic<int> got{0};
    for (int i = 0; i < Consumers; ++i) {
        auto ch = open(c);
        ch.qos(10).value();
        auto in = ch.consume("work").value();
        workers.push_back(async::spawn([](amqp::channel ch, amqp::consumer in, std::atomic<int>* got) -> async::task<int> {
            int n = 0;
            for (;;) {
                auto d = co_await in.async_receive();
                if (!d) {
                    co_return n;
                }
                ++n;
                (void)co_await ch.async_ack(d->delivery_tag);
                if (++*got == Messages) {
                    co_return n;
                }
            }
        }(ch, in, &got)));
    }
    vector<async::task<bool>> publishers;
    for (int p = 0; p < 4; ++p) {
        publishers.push_back(async::spawn([](amqp::channel ch) -> async::task<bool> {
            for (int i = 0; i < Messages / 4; ++i) {
                if (!co_await ch.async_publish("", "work", sgcl::string("m"))) {
                    co_return false;
                }
            }
            co_return true;
        }(open(c))));
    }
    for (auto& p : publishers) {
        EXPECT_TRUE(p.wait());
    }
    for (int i = 0; i < 400 && got.load() < Messages; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(got.load(), Messages);
    EXPECT_TRUE(c.close());   // ends the consumers waiting
    int total = 0;
    for (auto& w : workers) {
        total += w.wait();
    }
    EXPECT_GE(total, Messages);
}

TEST(AmqpClient, BrokerEvents) {
    Broker b;
    b.block_on_open = true;
    auto c = amqp::client::connect(b.url()).value();
    for (int i = 0; i < 100 && !c.blocked(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(c.blocked());
    // a queue deleted under its consumer: RabbitMQ's cancel notification
    auto ch = open(c);
    ch.declare_queue("gone").value();
    auto in = ch.consume("gone").value();
    open(c).delete_queue("gone").value();
    auto r = in.receive();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), amqp::errc::not_found);
    // the broker closes the connection: every channel's next call has its reason
    b.close_all(320, "CONNECTION_FORCED - broker forced connection closure with reason 'shutdown'");
    for (int i = 0; i < 100 && ch.declare_queue("x"); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto e = ch.declare_queue("x");
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), amqp::errc::connection_forced);
}

TEST(AmqpClient, Heartbeats) {
    Broker b;
    b.heartbeat = 1;
    amqp::client::options o;
    o.heartbeat = std::chrono::seconds(1);
    auto c = amqp::client::connect(b.url(), o).value();
    auto ch = open(c);
    std::this_thread::sleep_for(std::chrono::milliseconds(2600));   // idle past two intervals: the heartbeats keep it
    EXPECT_TRUE(ch.declare_queue("alive"));
    b.silent = true;   // a peer that stops: two intervals without a frame end the connection
    auto start = sgcl::clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2600));
    auto r = ch.declare_queue("dead");
    ASSERT_FALSE(r);
    EXPECT_LT(sgcl::clock::now() - start, std::chrono::seconds(10));
}
