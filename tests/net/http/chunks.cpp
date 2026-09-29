//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the bytes of bodies on the managed heap (detail/chunks.h) —
// ByteChunks (blocks of 8 KB through `next`) and BodyBuffer (64 bytes in
// place, then blocks), against a std::string with the same operations,
// under a deterministic generator: appends and takes of every size across
// block edges, the slices in order, the copies whole and cut.
#include "tests/types.h"
#include "sgcl/net/http/detail/chunks.h"
#include "sgcl/net/http/http.h"

#include <random>
#include <string>

using namespace sgcl;
using namespace sgcl::net::http::detail;

namespace {
    std::string pattern(size_t at, size_t n) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char((at + i) * 131 % 251);
        }
        return s;
    }

    std::string slices(const ByteChunks& c) {
        std::string out;
        c.each([&](const slice<const byte>& s) {
            out.append(reinterpret_cast<const char*>(s.data()), s.size());
        });
        return out;
    }
}

TEST(HttpChunks_Tests, OneBlockIsEightKilobytes) {
    EXPECT_EQ(sizeof(ByteChunk), 8192u);
    EXPECT_EQ(ByteChunk::Room, 8192u - sizeof(tracked_ptr<void>) - 8);
}

TEST(HttpChunks_Tests, AppendsAndTakesAsAString) {
    for (unsigned seed = 0; seed < 200; ++seed) {
        std::mt19937 g(seed);
        ByteChunks c;
        std::string model;
        size_t written = 0;
        for (int step = 0; step < 60; ++step) {
            if (g() % 3) {
                size_t n = g() % 3 == 0 ? g() % 20000 : g() % 100;
                auto s = pattern(written, n);
                written += n;
                c.append(s);
                model += s;
            } else {
                size_t n = g() % 12000;
                std::string out(n, '\0');
                size_t k = c.take(out.data(), n);
                ASSERT_EQ(k, std::min(n, model.size())) << seed;
                ASSERT_EQ(out.substr(0, k), model.substr(0, k)) << seed;
                model.erase(0, k);
            }
            ASSERT_EQ(c.size(), model.size()) << seed;
            ASSERT_EQ(c.empty(), model.empty());
        }
        ASSERT_EQ(slices(c), model) << seed;
        std::string copy = "head:";
        c.copy_to(copy);
        ASSERT_EQ(copy, "head:" + model);
        c.clear();
        EXPECT_TRUE(c.empty());
        EXPECT_EQ(slices(c), "");
    }
}

TEST(HttpChunks_Tests, BodyBufferInPlaceThenBlocks) {
    BodyBuffer b;
    b.append("hello, world\n");
    EXPECT_EQ(b.size(), 13u);
    EXPECT_TRUE(b.chunks().empty());   // in place: no block for a short answer
    std::string s;
    b.copy_to(s);
    EXPECT_EQ(s, "hello, world\n");
    b.append(std::string(51, 'x'));    // 64 exactly: still in place
    EXPECT_TRUE(b.chunks().empty());
    b.append("y");                     // past it: all of it into blocks, in order
    EXPECT_EQ(b.small().size(), 0u);
    EXPECT_EQ(b.size(), 65u);
    s.clear();
    b.copy_to(s);
    EXPECT_EQ(s, "hello, world\n" + std::string(51, 'x') + "y");
    for (unsigned seed = 0; seed < 100; ++seed) {
        std::mt19937 g(seed);
        BodyBuffer body;
        std::string model;
        for (int i = 0; i < 20; ++i) {
            auto p = pattern(model.size(), g() % 3 == 0 ? g() % 30000 : g() % 40);
            body.append(p);
            model += p;
        }
        std::string all;
        body.each([&](const slice<const byte>& x) {
            all.append(reinterpret_cast<const char*>(x.data()), x.size());
        });
        ASSERT_EQ(all, model) << seed;
        size_t cut = model.empty() ? 0 : g() % model.size();
        std::string part;
        body.copy_to(part, cut);
        ASSERT_EQ(part, model.substr(0, cut)) << seed;
        BodyBuffer moved = std::move(body);
        std::string again;
        moved.copy_to(again);
        ASSERT_EQ(again, model) << seed;
    }
}

// The blocks go back to the worker's pool once a body is sent: a thousand
// responses of 64 KB over HTTP/1.1 take their blocks from it (after the
// first few), and leave at most workers × Kept blocks alive, plus the ones
// in flight (none when the last response is read)
TEST(HttpChunks_Tests, BlocksComeBackToTheWorkersPool) {
    net::http::server s;
    static const std::string big(65536, 'z');
    s.route("GET /big", [](net::http::request, net::http::response_writer w) {
        w.write(slice<const byte>(reinterpret_cast<const byte*>(big.data()), big.size()));
    });
    net::listener l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(s.async_serve(l));
    net::http::client c;
    const auto url = sgcl::string("http://127.0.0.1:" + std::to_string(l.local_endpoint().port()) + "/big");
    const auto before = ChunkPool::totals();
    for (int i = 0; i < 1000; ++i) {
        auto res = c.get(url);
        ASSERT_TRUE(res);
        ASSERT_EQ(res->text()->size(), big.size());
    }
    const auto after = ChunkPool::totals();
    s.close();
    (void)serving.wait();
    const double takes = double(after.takes - before.takes);
    const double hits = double(after.hits - before.hits);
    EXPECT_GE(takes, 1000.0 * 8);   // 64 KB: 9 blocks of 8176 bytes a response
    EXPECT_GE(hits / takes, 0.9) << hits << " of " << takes;
    collector::force_collect(true);
    collector::force_collect(true);
    size_t live = 0;
    for (auto& t : collector::get_type_statistics()) {
        if (*t.type == typeid(ByteChunk)) {
            live += t.live_objects;
        }
    }
    EXPECT_LE(live, size_t(async::scheduler::workers()) * ChunkPool::Kept + 16);
}
