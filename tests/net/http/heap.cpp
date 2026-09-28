//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: what the head, the body and the connection leave to the collector
// (the audit of the managed heap, 2026-09-26). The fields of a head in one
// list of their number, set and erase in place; a request without a body
// has no Body; the wire's buffer is unmanaged, a body is discarded where it
// lies in it, a chunked body's decoder is a member, a body read whole is
// one vector of its size; a lingering connection is read through unmanaged
// scratch; a stream body, whose reads may run on the pool, is sent through
// a managed block its slices hold. Each case counts what it no longer
// makes, while the operation runs or while its object lives; the discard's
// rewrite is checked for what it does too.
#include "tests/types.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/http/detail/parser.h"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <typeinfo>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    namespace http = sgcl::net::http;
    namespace hd = sgcl::net::http::detail;

    size_t live_of(const std::type_info& type) {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (!s.buffers && *s.type == type) {
                n += s.live_objects;
            }
        }
        return n;
    }

    size_t io_blocks() {
        return live_of(typeid(sgcl::io::detail::IoBlock));
    }

    using ManagedCopyBlock = sgcl::array<std::byte, 32768>;

    // The blocks of 8 and 32 KB a wire grew its buffer in
    size_t managed_blocks() {
        return io_blocks() + live_of(typeid(ManagedCopyBlock));
    }

    string head_of(const std::string& s) {
        return string(std::string_view(s));
    }

    struct Serving {
        http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Serving(http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(server.async_serve(listener));
        }

        ~Serving() {
            server.close();
            (void)serving.wait();
        }

        string url(const std::string& path) const {
            return string("http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + path);
        }
    };

    // Bytes into a Wire's connection, each write taken by the reader
    // before it returns (connection::in_memory is a rendezvous)
    void put(const net::connection& c, const std::string& s) {
        ASSERT_TRUE(c.write(string(std::string_view(s))));
    }
}

// Five fields: one list of five, not the 1, 2, 4, 8 of a growth
TEST(HttpHeap_Tests, AHeadsFieldsAreOneListOfTheirNumber) {
    auto head = head_of("GET / HTTP/1.1\r\nHost: x\r\nA: 1\r\nB: 2\r\nC: 3\r\nD: 4\r\n\r\n");
    hd::RequestLine line;
    http::headers h;
    hd::BodyFraming framing;
    ASSERT_EQ(hd::check_request_head(head, line, h, framing), 0);
    auto& fields = hd::HeadersAccess::fields(h);
    EXPECT_EQ(fields.size(), 5u);
    EXPECT_LT(fields.capacity(), 8u);   // a size class may round it up a little (7 with the page-anchored classes); the growth made 8
    EXPECT_EQ(h.get("c"), "3");
}

TEST(HttpHeap_Tests, SetAndEraseWorkInPlace) {
    http::headers h;
    h.add("A", "1").add("B", "2").add("b", "3").add("C", "4");
    auto& fields = hd::HeadersAccess::fields(h);
    auto* list = fields.data();
    h.set("B", "5");                                   // the first B's place, the second gone
    EXPECT_EQ(fields.data(), list);
    ASSERT_EQ(h.size(), 3u);
    std::string all;
    for (auto [name, value] : h) {
        all += std::string(name.view()) + "=" + std::string(value.view()) + ";";
    }
    EXPECT_EQ(all, "A=1;B=5;C=4;");
    h.erase("a");
    EXPECT_EQ(fields.data(), list);
    ASSERT_EQ(h.size(), 2u);
    EXPECT_EQ(h.get("B"), "5");
    EXPECT_EQ(h.get("C"), "4");
    h.set("D", "6");                                   // none of the name: at the end
    EXPECT_EQ(h.size(), 3u);
    EXPECT_EQ(h.get("d"), "6");
}

TEST(HttpHeap_Tests, ARequestWithoutABodyMakesNoBody) {
    http::request r("GET", "http://example.com/");
    auto body = r.body();
    std::byte b[8];
    auto n = body.read(slice<std::byte>(b, sizeof(b)));
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 0u);
    EXPECT_FALSE(hd::RequestAccess::impl(r)->body);
    EXPECT_EQ(r.trailers().size(), 0u);
}

TEST(HttpHeap_Tests, AServedRequestWithoutABodyMakesNoBody) {
    std::atomic<int> had_body = -1;
    std::atomic<int> read = -1;
    http::server s;
    s.route("GET /", [&](http::request req, http::response_writer w) -> async::task<> {
        had_body = bool(hd::RequestAccess::impl(req)->body);
        auto t = co_await req.async_text();
        std::byte b[8];
        auto n = co_await req.body().async_read(slice<std::byte>(b, sizeof(b)));
        read = t && n ? int(t->size() + *n) : -2;
        w.write("ok");
    });
    Serving r(s);
    http::client c;
    auto res = c.get(r.url("/"));
    ASSERT_TRUE(res);
    EXPECT_EQ(*res->text(), "ok");
    EXPECT_EQ(had_body.load(), 0);
    EXPECT_EQ(read.load(), 0);
}

TEST(HttpHeap_Tests, TheWiresBufferIsNotManaged) {
    size_t before = managed_blocks();
    auto [a, b] = net::connection::in_memory();
    tracked_ptr wire = make_tracked<hd::Wire>(b);
    auto reading = async::spawn(wire->read_head(64 * 1024));
    put(a, "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    auto head = reading.wait();
    ASSERT_TRUE(head && *head);
    EXPECT_EQ((*head)->size(), 27u);
    EXPECT_EQ(managed_blocks(), before);   // the wire alive, its buffer of 8 KB
    reading = async::spawn(wire->read_head(64 * 1024));
    put(a, "GET / HTTP/1.1\r\nHost: " + std::string(20000, 'h') + "\r\n\r\n");
    head = reading.wait();
    ASSERT_TRUE(head && *head);
    EXPECT_EQ((*head)->size(), 20026u);
    EXPECT_EQ(managed_blocks(), before);   // grown to 32 KB for a head of 20 KB
    (void)a.close();
    (void)b.close();
}

TEST(HttpHeap_Tests, AChunkedBodysDecoderIsAMember) {
    auto [a, b] = net::connection::in_memory();
    tracked_ptr wire = make_tracked<hd::Wire>(b);
    hd::BodyFraming f;
    f.kind = hd::Framing::chunked;
    tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, false);
    std::byte got[16];
    auto reading = async::spawn(body->async_read(slice<std::byte>(got, sizeof(got))));
    put(a, "5\r\nhello\r\n");
    auto n = reading.wait();
    ASSERT_TRUE(n);
    ASSERT_EQ(*n, 5u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(got), 5), "hello");
    EXPECT_EQ(live_of(typeid(hd::ChunkedDecoder)), 0u);   // the body alive, its decoder made
    auto rest = async::spawn(body->read_everything());
    put(a, "3\r\nabc\r\n0\r\nX-T: 1\r\n\r\n");
    auto all = rest.wait();
    ASSERT_TRUE(all);
    EXPECT_EQ(all->size(), 3u);
    EXPECT_TRUE(body->done());
    EXPECT_EQ(body->trailers().get("x-t"), "1");
    (void)a.close();
    (void)b.close();
}

// discard drops the bytes where they lie, in the wire's buffer: no block
// of its own while it waits for the rest
TEST(HttpHeap_Tests, DiscardUsesTheWiresBuffer) {
    auto [a, b] = net::connection::in_memory();
    tracked_ptr wire = make_tracked<hd::Wire>(b);
    hd::BodyFraming f;
    f.kind = hd::Framing::length;
    f.length = 100000;
    tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, false);
    size_t before = io_blocks();
    auto discarding = async::spawn(body->discard(1 << 20));
    put(a, std::string(1000, 'b'));
    EXPECT_EQ(io_blocks(), before);   // the discard waiting for the rest
    put(a, std::string(99000, 'b'));
    EXPECT_TRUE(discarding.wait());
    EXPECT_TRUE(body->done());
    EXPECT_FALSE(body->failed());
    EXPECT_EQ(body->read_total(), 100000u);
    (void)a.close();
    (void)b.close();
}

// What discard does, framing by framing: a chunked body to its end with
// its trailers, one past `most` left unfinished, one past the limit a
// 413, one cut short a failure
TEST(HttpHeap_Tests, DiscardReadsTheFraming) {
    {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        hd::BodyFraming f;
        f.kind = hd::Framing::chunked;
        tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, false);
        bool ended = false;
        body->set_on_end([&](bool clean) { ended = clean; });
        auto discarding = async::spawn(body->discard(1 << 20));
        put(a, "4\r\nabcd\r\n");
        put(a, "a\r\n0123456789\r\n0\r\n");
        put(a, "T: x\r\n\r\nGET");
        EXPECT_TRUE(discarding.wait());
        EXPECT_TRUE(ended);
        EXPECT_EQ(body->read_total(), 14u);
        EXPECT_EQ(body->trailers().get("t"), "x");
        EXPECT_EQ(wire->view(), "GET");
        (void)a.close();
    }
    {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        hd::BodyFraming f;
        f.kind = hd::Framing::length;
        f.length = 50000;
        tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, false);
        auto discarding = async::spawn(body->discard(1000));
        put(a, std::string(3000, 'b'));
        EXPECT_FALSE(discarding.wait());   // past `most`: left, the connection to be closed
        EXPECT_FALSE(body->done());
        EXPECT_FALSE(body->failed());
        (void)a.close();
    }
    {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        hd::BodyFraming f;
        f.kind = hd::Framing::length;
        f.length = 5000;
        tracked_ptr body = make_tracked<hd::Body>(wire, f, 100, false);
        auto discarding = async::spawn(body->discard(1 << 20));
        put(a, std::string(5000, 'b'));
        EXPECT_FALSE(discarding.wait());
        EXPECT_TRUE(body->failed());
        EXPECT_EQ(body->error_status(), 413);
        (void)a.close();
    }
    {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        hd::BodyFraming f;
        f.kind = hd::Framing::length;
        f.length = 5000;
        tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, false);
        auto discarding = async::spawn(body->discard(1 << 20));
        put(a, std::string(100, 'b'));
        (void)a.close();
        EXPECT_FALSE(discarding.wait());
        EXPECT_TRUE(body->failed());
    }
}

// A stream body goes out through unmanaged scratch: no managed block of
// 32 KB while the stream is read
TEST(HttpHeap_Tests, AStreamBodyHandsThePoolAnOwnedBlock) {
    struct Looking {   // no async_read: the client's reads of it run on the pool
        size_t left;
        std::atomic<bool> owned = true;

        expected<size_t, io::error> read(const slice<std::byte>& out) {
            if (!out.owner()) {
                owned = false;
            }
            size_t k = std::min(out.size(), left);
            std::memset(out.data(), 's', k);
            left -= k;
            return k;
        }
    };
    std::atomic<size_t> received = 0;
    http::server s;
    s.route("POST /", [&](http::request req, http::response_writer w) -> async::task<> {
        auto b = co_await req.async_bytes();
        received = b ? b->size() : 0;
        w.write("ok");
    });
    Serving r(s);
    Looking stream{100000};
    http::request req("POST", r.url("/"));
    req.set_body(io::reader(stream));   // chunked
    http::client c;
    auto res = c.send(req);
    ASSERT_TRUE(res);
    EXPECT_EQ(*res->text(), "ok");
    EXPECT_EQ(received.load(), 100000u);
    EXPECT_TRUE(stream.owned.load());
}

// A connection closed with a body left unread lingers, reading and
// dropping what comes for a while: through unmanaged scratch, beside the
// wire's unmanaged buffer
TEST(HttpHeap_Tests, ALingeringConnectionHoldsNoManagedBlock) {
    http::server s;
    s.route("POST /", [](http::request, http::response_writer w) { w.write("ok"); });
    Serving r(s);
    size_t before = io_blocks();
    auto c = *net::tcp::connect(r.listener.local_endpoint());
    ASSERT_TRUE(c.write(string("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 10000000\r\n\r\n" + std::string(1000, 'b'))));
    std::string got;
    std::byte buf[4096];
    while (got.size() < 2 || got.substr(got.size() - 2) != "ok") {
        auto n = c.read(slice<std::byte>(buf, sizeof(buf)));
        ASSERT_TRUE(n && *n);
        got.append(reinterpret_cast<const char*>(buf), *n);
    }
    EXPECT_NE(got.find("Connection: close"), std::string::npos);
    std::this_thread::sleep_for(50ms);   // the server in its linger, half a second of it
    EXPECT_EQ(io_blocks(), before);
    (void)c.close();
}

// A body read whole is one vector of its size: 13 bytes declared by
// Content-Length, or chunked, and not a vector of 8 KB grown a read at a
// time. The managed bytes of the byte buffers while the read waits for the
// body, after a full cycle: the vector it reads into, and nothing else
TEST(HttpHeap_Tests, ABodyReadWholeIsOneVectorOfItsSize) {
    auto buffer_bytes = [] {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (s.buffers && *s.type == typeid(std::byte[])) {
                n += s.live_bytes;
            }
        }
        return n;
    };
    for (auto kind : {hd::Framing::length, hd::Framing::chunked}) {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        hd::BodyFraming f;
        f.kind = kind;
        f.length = 13;
        tracked_ptr body = make_tracked<hd::Body>(wire, f, 0, true);
        collector::force_collect(true);   // the garbage of earlier tests gone before the base, not inside the window
        size_t before = buffer_bytes();
        auto reading = async::spawn(body->read_everything());
        std::this_thread::sleep_for(20ms);   // the read waiting for the body
        size_t during = buffer_bytes() - before;
        put(a, kind == hd::Framing::length ? "hello, world\n" : "d\r\nhello, world\n\r\n0\r\n\r\n");
        auto all = reading.wait();
        ASSERT_TRUE(all);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(all->data()), all->size()), "hello, world\n");
        EXPECT_TRUE(body->done());
        EXPECT_LT(during, 1024u) << "a body of 13 bytes read into " << during << " managed bytes";   // 8 KB before
        (void)a.close();
        (void)b.close();
    }
}
