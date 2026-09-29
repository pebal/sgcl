//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A gzip stream written straight to a connection: for every byte a client
// sends, 256 KB of text compressed by compress::gzip::writer (the default
// level) with co_await async_write and async_close into the connection
// itself, one gzip member a response, the writer reset for the next. Over
// TCP, or TLS 1.3 with a certificate and its key. The workers are the
// scheduler's (SGCL_WORKERS). go/main.go is Go's side (gzip.NewWriter on a
// net.Conn or a tls.Conn, the same text) and the load for both.
//
//   SGCL_WORKERS=N bench_gzip_stream [address=:18200 [certificate.pem key.pem]]
#include "sgcl/compress/gzip.h"
#include "sgcl/io/file.h"
#include "sgcl/io/print.h"
#include "sgcl/net/net.h"
#include "sgcl/net/tls.h"

#include <cstdint>
#include <string>

using namespace sgcl;

namespace {
    // 256 KB of words from a small vocabulary, picked by the same generator
    // as Go's side: text that compresses about three to one
    const std::string& text() {
        static const std::string t = [] {
            static const char* words[] = {"the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog", "and", "runs", "far",
                                          "away", "from", "home", "while", "birds", "sing", "in", "trees", "under", "blue", "sky", "today"};
            std::string s;
            uint32_t x = 2463534242u;
            while (s.size() < (256u << 10)) {
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                s += words[x % 24];
                s += (x >> 8) % 11 == 0 ? ".\n" : " ";
            }
            s.resize(256u << 10);
            return s;
        }();
        return t;
    }

    async::task<> serve(net::connection c) {
        const std::string& t = text();
        const slice<const byte> data(reinterpret_cast<const byte*>(t.data()), t.size());
        tracked_ptr ask = make_tracked<array<byte, 64>>();
        compress::gzip::writer gz{io::writer(c)};
        for (;;) {
            auto n = co_await c.async_read(slice<byte>(ask, ask->data(), ask->size()));
            if (!n || *n == 0) {
                break;
            }
            for (size_t k = 0; k < *n; ++k) {   // a response for every byte asked
                gz.reset(io::writer(c));
                if (!co_await gz.async_write(data) || !co_await gz.async_close()) {
                    (void)co_await c.async_close();
                    co_return;
                }
            }
        }
        (void)co_await c.async_close();
    }

    async::task<> accept_all(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            async::go(serve(*c));
        }
    }
}

int main(int argc, char** argv) {
    string address = argc > 1 ? argv[1] : ":18200";
    expected<net::listener, io::error> l;
    if (argc > 3) {
        net::tls::config tls;
        tls.identities = {net::tls::identity(io::read_text(argv[2]), io::read_text(argv[3]))};
        l = net::tls::listen(address, tls);
    } else {
        l = net::tcp::listen(address);
    }
    if (!l) {
        println("listen: {}", l.error().message());
        return 1;
    }
    (void)text();
    async::spawn(accept_all(*l)).wait();
}
