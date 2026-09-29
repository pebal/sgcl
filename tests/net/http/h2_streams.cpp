//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the server's table of the streams whose handlers run or wait
// (detail/h2/serve.h, ServerStreams): open addressing in one managed array,
// 64 slots at first, twice as many past half full, deletion by backward
// shift. Against a model (std::map) under inserts and erases in any order;
// a thousand streams one after another on one connection keep it at 64; a
// queue of requests waiting for a handler (their predecessors reset by the
// client while their handlers still ran) grows it, and every request of
// the queue is served.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    namespace h2 = net::http::detail::h2;

    tracked_ptr<h2::ServerStream> stream(uint32_t id) {
        return make_tracked<h2::ServerStream>(id, tracked_ptr<h2::StreamOwner>());
    }

    // The model holds raw pointers: a tracked_ptr lives on the stack or in a
    // managed object (Rule 1), never in a node of std::map; the table keeps
    // the streams alive, the model only names them
    void same(h2::ServerStreams& t, const std::map<uint32_t, h2::ServerStream*>& model) {
        ASSERT_EQ(t.size(), model.size());
        for (auto& [id, st] : model) {
            ASSERT_EQ(t.find(id).get(), st) << id;
        }
        size_t seen = 0;
        for (auto& [id, st] : t) {
            auto it = model.find(id);
            ASSERT_NE(it, model.end()) << id;
            ASSERT_EQ(st.get(), it->second);
            ++seen;
        }
        ASSERT_EQ(seen, model.size());
    }
}

TEST(H2Streams_Tests, AgainstAModel) {
    std::mt19937 rng(7);
    h2::ServerStreams t;
    std::map<uint32_t, h2::ServerStream*> model;
    for (int step = 0; step < 20000; ++step) {
        const uint32_t id = 1 + 2 * uint32_t(rng() % 400);   // odd ids, many collisions of homes
        if (rng() % 3 == 0) {
            t.erase(id);
            model.erase(id);
        } else {
            auto st = stream(id);
            t.insert_or_assign(id, st);
            model[id] = st.get();
        }
        if (step % 97 == 0) {
            same(t, model);
        }
        ASSERT_FALSE(t.find(id * 2).get());   // an even id: never there
    }
    same(t, model);
    EXPECT_LE(t.capacity(), 1024u);   // 400 ids at most: past half of 512, never past 1024
}

TEST(H2Streams_Tests, GrowsAndNeverShrinks) {
    h2::ServerStreams t;
    EXPECT_EQ(t.capacity(), 64u);
    for (uint32_t i = 0; i < 32; ++i) {
        t.insert_or_assign(2 * i + 1, stream(2 * i + 1));
    }
    EXPECT_EQ(t.capacity(), 64u);   // half full
    t.insert_or_assign(65, stream(65));
    EXPECT_EQ(t.capacity(), 128u);
    for (uint32_t i = 0; i < 33; ++i) {
        t.erase(i < 32 ? 2 * i + 1 : 65);
    }
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.capacity(), 128u);
    EXPECT_FALSE(t.find(65).get());
    // one after another: the same few slots, no growth
    h2::ServerStreams one;
    for (uint32_t id = 1; id < 2000; id += 2) {
        one.insert_or_assign(id, stream(id));
        ASSERT_TRUE(one.find(id).get());
        one.erase(id);
    }
    EXPECT_EQ(one.capacity(), 64u);
    EXPECT_TRUE(one.empty());
}

TEST(H2Streams_Tests, AThousandStreamsInTurnKeep64Slots) {
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.write("ok");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    const auto url = sgcl::string("http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/");
    h2::stream_table_high.store(0);
    {
        net::http::client web;
        web.h2c = true;
        web.timeout = 10s;
        for (int i = 0; i < 1000; ++i) {
            auto r = web.get(url);
            ASSERT_TRUE(r) << i << ": " << std::string(r.error().message().view());
            ASSERT_EQ(*r->text(), "ok");
        }
    }
    EXPECT_EQ(h2::stream_table_high.load(), 0u);   // never grew past the first 64
    srv.close();
    (void)serving.wait();
}

TEST(H2Streams_Tests, AQueueOfWaitingRequestsGrowsItAndIsServed) {
    // 40 handlers held; the client gives their requests up (a timeout: RST
    // CANCEL), which frees the machine's places but not the handlers'; 40
    // more requests then wait for a handler: 80 streams in the table
    async::event gate;
    std::atomic<int> started{0};
    net::http::server srv;
    srv.h2c = true;
    srv.max_concurrent_streams = 40;
    srv.route("GET /hold", [gate, &started](net::http::request, net::http::response_writer w) -> async::task<> {
        started.fetch_add(1);
        co_await gate;
        w.write("held");
    });
    srv.route("GET /free", [](net::http::request, net::http::response_writer w) {
        w.write("free");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    const auto base = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
    h2::stream_table_high.store(0);
    net::http::client web;
    web.h2c = true;
    ASSERT_TRUE(web.get(sgcl::string(base + "/free")));   // one connection, the server's SETTINGS known
    web.timeout = 300ms;
    std::vector<async::task<expected<net::http::response, io::error>>> first;
    for (int i = 0; i < 40; ++i) {
        first.push_back(web.async_get(sgcl::string(base + "/hold")));
        first.back().spawn();
    }
    for (auto& t : first) {
        EXPECT_FALSE(t.wait());   // given up: the handlers still hold
    }
    EXPECT_EQ(started.load(), 40);
    web.timeout = 20s;
    std::vector<async::task<expected<net::http::response, io::error>>> second;
    for (int i = 0; i < 40; ++i) {
        second.push_back(web.async_get(sgcl::string(base + "/free")));
        second.back().spawn();
    }
    std::this_thread::sleep_for(300ms);   // they wait for a handler
    EXPECT_GE(h2::stream_table_high.load(), 256u);   // 80 streams: past 64 and 128 half full
    gate.set();
    int ok = 0;
    for (auto& t : second) {
        auto r = t.wait();
        ok += r && r->status() == 200 && *r->text() == "free";
    }
    EXPECT_EQ(ok, 40);
    srv.close();
    (void)serving.wait();
}
