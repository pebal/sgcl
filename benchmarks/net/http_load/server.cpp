//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server side of the HTTP load test (run.sh here): hello, world on
// GET /, the module's server at its defaults; POST /echo the request's body
// back; POST /resp?size=N the body read and dropped, a response of N bytes
// (at most 1 MB); GET /stream?size=N&parts=K N bytes flushed in K
// pieces; GET /file the file named by HTTP_LOAD_FILE. The number
// of workers is the scheduler's, from the environment (SGCL_WORKERS=4
// bench_http_hello), as Go's is from GOMAXPROCS. gosrv/main.go is the
// Go side, load/main.go the load
// generator for both. HTTP/2 by prior knowledge (h2c) beside HTTP/1.1 on
// the plain port; with a certificate and its key the server speaks https:
// TLS 1.3 by net::tls::listen, ALPN h2 and http/1.1.
//
//   SGCL_WORKERS=N bench_http_hello [address=:18080 [certificate.pem key.pem]]
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"
#include "benchmarks/placement.h"

#include <cstdlib>
#include <string>

using namespace sgcl;

int main(int argc, char** argv) {
    static const std::string filler(size_t(1) << 20, 'r');
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("hello, world\n");
    });
    // a route with a wildcard: the router's segments and a value looked up
    srv.route("GET /items/{id}", [](net::http::request r, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write(r.path_value("id"));
    });
    srv.route("POST /echo", [](net::http::request r, net::http::response_writer w) -> async::task<> {
        auto body = co_await r.async_bytes();
        if (body) {
            w.write(*body);
        }
    });
    srv.route("POST /resp", [](net::http::request r, net::http::response_writer w) -> async::task<> {
        (void)co_await r.async_bytes();
        size_t n = 1024;
        std::string q(r.url().query().view());
        if (auto at = q.find("size="); at != std::string::npos) {
            n = std::min<size_t>(std::strtoull(q.c_str() + at + 5, nullptr, 10), filler.size());
        }
        w.write(slice<const byte>(reinterpret_cast<const byte*>(filler.data()), n));
    });
    // GET /stream?size=N&parts=K: N bytes (64 KB unless given) in K pieces
    // (4), each flushed: a response that goes chunked
    srv.route("GET /stream", [](net::http::request r, net::http::response_writer w) -> async::task<> {
        size_t n = 65536, parts = 4;
        std::string q(r.url().query().view());
        if (auto at = q.find("size="); at != std::string::npos) {
            n = std::min<size_t>(std::strtoull(q.c_str() + at + 5, nullptr, 10), filler.size());
        }
        if (auto at = q.find("parts="); at != std::string::npos) {
            parts = std::max<size_t>(1, std::strtoull(q.c_str() + at + 6, nullptr, 10));
        }
        w.set_header("Content-Type", "application/octet-stream");
        for (size_t k = 0, at = 0; k < parts; ++k) {
            const size_t piece = k + 1 < parts ? n / parts : n - at;
            w.write(slice<const byte>(reinterpret_cast<const byte*>(filler.data()) + at, piece));
            at += piece;
            if (!co_await w.async_flush()) {
                co_return;
            }
        }
    });
    // GET /file: the file named by HTTP_LOAD_FILE, written as the body
    // (sendfile after the head over TCP, its blocks sealed where they lie
    // over TLS); Go's side serves it with http.ServeFile
    static const char* file_path = std::getenv("HTTP_LOAD_FILE");
    srv.route("GET /file", [](net::http::request, net::http::response_writer w) {
        auto f = file_path ? io::open(file_path) : io::open("");
        if (!f) {
            w.error(net::http::status::not_found);
            return;
        }
        w.set_header("Content-Type", "application/octet-stream");
        w.write(*f);
    });
    string address = argc > 1 ? argv[1] : ":18080";
    expected<void, io::error> r;
    if (argc > 3) {
        net::tls::config tls;
        tls.identities = {net::tls::identity(io::read_text(argv[2]), io::read_text(argv[3]))};
        r = srv.serve_tls(address, tls);
    } else {
        r = srv.serve(address);
    }
    if (!r) {
        println("serve: {}", r.error().message());
        return 1;
    }
}
