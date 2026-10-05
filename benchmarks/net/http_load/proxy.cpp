//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The proxy of the reverse proxy load test (run_proxy.sh here): the
// module's http::reverse_proxy at its defaults on every path, in front of
// one backend (bench_http_hello, whose routes the load asks for).
// goproxy/main.go is the Go side (httputil.NewSingleHostReverseProxy in
// front of gosrv). The number of workers is the scheduler's, from the
// environment (SGCL_WORKERS=4 bench_http_proxy), as Go's is from GOMAXPROCS.
//
//   SGCL_WORKERS=N bench_http_proxy [address=:18080 [backend=http://127.0.0.1:18081]]
#include "sgcl/net/http/http.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main(int argc, char** argv) {
    string address = argc > 1 ? argv[1] : ":18080";
    string backend = argc > 2 ? argv[2] : "http://127.0.0.1:18081";
    net::http::server srv;
    srv.max_body_bytes = 0;
    srv.route("/", net::http::reverse_proxy(backend));
    auto r = srv.serve(address);
    if (!r) {
        println("serve: {}", r.error().message());
        return 1;
    }
}
