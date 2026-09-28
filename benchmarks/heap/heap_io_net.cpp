//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The managed heap an operation of io, hash, compress and net uses: the
// bytes and the objects a call leaves to the collector. Each case runs n
// times between two full cycles; the program is built with
// SGCL_LOG_PRINT_LEVEL=2 (benchmarks/CMakeLists.txt), so every cycle prints
// its "objects created", and heap_parse.py sums them between a case's BEGIN
// and END lines:
//
//   bench_heap_io_net | python3 benchmarks/heap/heap_parse.py /dev/stdin
//
// A line per case: the managed bytes in use grown by the loop (live_bytes,
// pages of the allocators, garbage not yet swept included), per call; the
// cycles that ran during it; the time per call; the objects created per call.
// The time of a case is its own warm run: each case runs a tenth of n first.
// Arguments: n (1000 by default), and a part of a case's name to run only
// the cases that hold it.
#include "sgcl/core/collector.h"
#include "sgcl/core/make_tracked.h"
#include "sgcl/io/io.h"
#include "sgcl/crypto/sha256.h"
#include "sgcl/compress/compress.h"
#include "sgcl/net/net.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/http/detail/parser.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

using namespace sgcl;

namespace {
    const char* only = nullptr;   // the cases whose names hold it, when given

    // A reader over plain memory: no write_to, so io::copy goes through its
    // block; no async_read, so an async form runs it on the pool
    struct MemReader {
        const unsigned char* p;
        size_t n;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t k = std::min(out.size(), n - at);
            std::memcpy(out.data(), p + at, k);
            at += k;
            return k;
        }
    };

    // The same with an async_read of its own, which never waits
    struct AsyncMemReader {
        const unsigned char* p;
        size_t n;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t k = std::min(out.size(), n - at);
            std::memcpy(out.data(), p + at, k);
            at += k;
            return k;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }
    };

    // A writer that drops what it is given, with an async_write that never waits
    struct AsyncDiscard {
        expected<size_t, io::error> write(const slice<const byte>& d) {
            return d.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> d) {
            co_return d.size();
        }
    };

    // g(k) makes k calls
    template<class G>
    void measure_runs(const char* name, size_t reps, G g) {
        if (only && !std::strstr(name, only)) {
            return;
        }
        g(reps / 10 + 1);
        collector::force_collect(true);
        std::printf("BEGIN %s\n", name);
        auto s0 = collector::get_statistics();
        auto t0 = std::chrono::steady_clock::now();
        g(reps);
        auto t1 = std::chrono::steady_clock::now();
        auto s1 = collector::get_statistics();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(reps);
        long delta = long(s1.live_bytes) - long(s0.live_bytes);
        collector::force_collect(true);   // the log line: the objects created since the last cycle
        std::printf("END %-52s live_bytes %+10ld over %5zu calls = %7ld B/op  cycles during loop: %2zu  (%9.0f ns/op) reps=%zu\n", name, delta, reps, delta / long(reps), s1.cycles - s0.cycles, ns, reps);
    }

    template<class F>
    void measure(const char* name, size_t reps, F f) {
        measure_runs(name, reps, [&](size_t k) {
            for (size_t i = 0; i < k; ++i) {
                f();
            }
        });
    }

    // A task body run k times in one task
    template<class F>
    void measure_task(const char* name, size_t reps, F f) {
        measure_runs(name, reps, [&](size_t k) {
            auto run = [&](size_t k) -> async::task<> {
                for (size_t i = 0; i < k; ++i) {
                    co_await f();
                }
            };
            async::spawn(run(k)).wait();
        });
    }

    std::string temp_path(const char* name) {
        const char* dir = std::getenv("TMPDIR");
        std::string p = dir && *dir ? dir : "/tmp";
        if (p.back() != '/') {
            p += '/';
        }
        return p + name + std::to_string(::getpid());
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const size_t N = argc > 1 ? size_t(std::atol(argv[1])) : 1000;
    only = argc > 2 ? argv[2] : nullptr;
    std::vector<unsigned char> mb(1 << 20, 'x');
    const slice<const byte> mb_bytes(reinterpret_cast<const byte*>(mb.data()), mb.size());

    string path_100k(temp_path("sgcl_heap_100k_"));
    string path_small(temp_path("sgcl_heap_small_"));
    (void)io::write_file(path_100k, mb_bytes.first(100 * 1024));
    (void)io::write_file(path_small, mb_bytes.first(10));

    measure("nothing", N, [] {});

    // io
    measure("io::copy(discard, MemReader 1 MB)", N, [&] {
        MemReader r{mb.data(), mb.size()};
        auto n = io::copy(io::discard, r);
        if (!n || *n != mb.size()) std::printf("copy failed\n");
    });
    measure("io::async_copy(AsyncDiscard, AsyncMemReader 1 MB)", N, [&] {
        auto run = [&]() -> async::task<size_t> {
            AsyncMemReader r{mb.data(), mb.size()};
            AsyncDiscard d;
            auto n = co_await io::async_copy(d, r);
            co_return n ? *n : 0;
        };
        if (async::spawn(run()).wait() != mb.size()) std::printf("async_copy failed\n");
    });
    measure("io::read_all(MemReader 100 KB)", N, [&] {
        MemReader r{mb.data(), 100 * 1024};
        auto v = io::read_all(r);
        if (!v || v->size() != 100 * 1024) std::printf("read_all failed\n");
    });
    measure("io::read_all(MemReader 1 KB)", N, [&] {
        MemReader r{mb.data(), 1024};
        auto v = io::read_all(r);
        if (!v || v->size() != 1024) std::printf("read_all failed\n");
    });
    measure("io::async_read_all(AsyncMemReader 100 KB)", N, [&] {
        auto run = [&]() -> async::task<size_t> {
            AsyncMemReader r{mb.data(), 100 * 1024};
            auto v = co_await io::async_read_all(r);
            co_return v ? v->size() : 0;
        };
        if (async::spawn(run()).wait() != 100 * 1024) std::printf("async_read_all failed\n");
    });
    measure("io::read_file (100 KB file)", N, [&] {
        auto r = io::read_file(path_100k);
        if (!r || r->size() != 100 * 1024) std::printf("read_file failed\n");
    });
    measure("io::read_file (10 B file)", N, [&] {
        auto r = io::read_file(path_small);
        if (!r || r->size() != 10) std::printf("read_file failed\n");
    });
    measure("io::buffered_writer: made, 100 B written, flushed", N, [&] {
        io::buffered_writer w(io::discard);
        (void)w.write(mb_bytes.first(100));
        (void)w.flush();
    });

    std::vector<unsigned char> lines;
    for (int i = 0; i < 1000; ++i) {
        lines.insert(lines.end(), 63, 'l');
        lines.push_back('\n');
    }
    measure("io::buffered_reader: 1000 read_line of 64 B", N, [&] {
        MemReader r{lines.data(), lines.size()};
        io::buffered_reader br{io::reader(r)};
        size_t n = 0;
        while (auto l = br.read_line()) {
            if (!*l) {
                break;
            }
            ++n;
        }
        if (n != 1000) std::printf("read_line failed %zu\n", n);
    });

    // hash
    measure("sha256 copy_from(MemReader 1 MB)", N, [&] {
        MemReader r{mb.data(), mb.size()};
        crypto::sha256 h;
        auto n = h.copy_from(io::reader(r));
        if (!n || *n != mb.size()) std::printf("copy_from failed\n");
    });
    measure("sha256 async_copy_from(AsyncMemReader 1 MB)", N, [&] {
        auto run = [&]() -> async::task<size_t> {
            AsyncMemReader r{mb.data(), mb.size()};
            crypto::sha256 h;
            auto n = co_await h.async_copy_from(io::reader(r));
            co_return n ? *n : 0;
        };
        if (async::spawn(run()).wait() != mb.size()) std::printf("async_copy_from failed\n");
    });

    // compress: a zip of one deflated entry, written and read back
    auto zip_bytes = [&] {
        io::buffer sink;
        compress::zip::writer w(sink);
        (void)w.add("a.txt", mb_bytes.first(4096));
        (void)w.close();
        return vector<byte>(sink.data().begin(), sink.data().end());
    }();
    measure("zip::writer: one deflated entry of 4 KB", N / 4, [&] {
        io::buffer sink;
        compress::zip::writer w(sink);
        (void)w.add("a.txt", mb_bytes.first(4096));
        (void)w.close();
    });
    auto archive = compress::zip::archive::from(zip_bytes.as_slice());
    measure("zip::archive::read: one deflated entry of 4 KB", N, [&] {
        auto v = archive->read(archive->entries()[0]);
        if (!v || v->size() != 4096) std::printf("zip read failed\n");
    });

    // net
    measure("connection::in_memory + one read_line", N, [&] {
        auto [a, b] = net::connection::in_memory();
        auto send = [](net::connection c) -> async::task<> {
            (void)co_await c.async_write(string("hello\n"));
        };
        async::go(send(a));
        auto line = b.read_line();
        if (!line || !*line || **line != "hello") std::printf("read_line failed\n");
        (void)a.close();
        (void)b.close();
    });

    // http
    static const char head_text[] =
        "GET /index.html?q=1 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: probe/1.0\r\n"
        "Accept: text/html,application/xhtml+xml\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Connection: keep-alive\r\n"
        "Cookie: a=1; b=2\r\n"
        "\r\n";
    string shared_head{std::string_view(head_text)};
    measure("http check_request_head (8 fields)", N, [&] {
        net::http::detail::RequestLine line;
        net::http::headers h;
        net::http::detail::BodyFraming framing;
        int refused = net::http::detail::check_request_head(shared_head, line, h, framing);
        if (refused) std::printf("parse refused %d\n", refused);
    });
    net::http::headers eight;
    (void)net::http::detail::check_request_head(shared_head, *std::make_unique<net::http::detail::RequestLine>(), eight, *std::make_unique<net::http::detail::BodyFraming>());
    string accept("Accept"), html("text/html");
    measure("http headers::set over 8 fields", N, [&] {
        eight.set(accept, html);
    });
    measure("http request::body() of a request without one", N, [&] {
        net::http::request r("GET", "http://example.com/");
        auto b = r.body();
        (void)b;
    });

    {
        net::http::server server;
        server.route("GET /hello", [](net::http::request, net::http::response_writer w) { w.write("hello, world\n"); });
        server.route("GET /chunked", [](net::http::request, net::http::response_writer w) -> async::task<> {
            w.write("hello, ");
            (void)co_await w.async_flush();
            w.write("world\n");
        });
        server.route("POST /ignore", [](net::http::request, net::http::response_writer w) { w.write("ok\n"); });
        auto l = net::tcp::listen("127.0.0.1:0");
        auto serving = async::spawn(server.async_serve(*l));
        net::http::client client;
        std::string base = "http://127.0.0.1:" + std::to_string(l->local_endpoint().port());
        string hello(base + "/hello"), chunked(base + "/chunked"), ignore(base + "/ignore");
        string body(std::string(4096, 'b'));
        measure_task("http GET, kept connection (client + server)", N, [&]() -> async::task<> {
            auto res = co_await client.async_get(hello);
            if (!res || (co_await res->async_text())->size() != 13) std::printf("GET failed\n");
        });
        measure_task("http GET, chunked response (client + server)", N, [&]() -> async::task<> {
            auto res = co_await client.async_get(chunked);
            if (!res || (co_await res->async_text())->size() != 13) std::printf("GET chunked failed\n");
        });
        measure_task("http POST 4 KB the handler leaves (client + server)", N, [&]() -> async::task<> {
            auto res = co_await client.async_post(ignore, "text/plain", body);
            if (!res || (co_await res->async_text())->size() != 3) std::printf("POST failed\n");
        });
        measure_task("http GET, a new connection each (client + server)", N / 4, [&]() -> async::task<> {
            net::http::client fresh;
            auto res = co_await fresh.async_get(hello);
            if (!res || (co_await res->async_text())->size() != 13) std::printf("GET fresh failed\n");
        });
        server.close();
        (void)serving.wait();
    }

    (void)io::remove(path_100k);
    (void)io::remove(path_small);
    async::scheduler::stop();
    return 0;
}
