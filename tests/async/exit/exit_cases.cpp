//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The end of a program with the async runtime busy, run as a child by
// exit_teardown.cpp (AsyncExit_Tests), one case per argument. Each returns
// from main at once, with something parked in the runtime or about to be:
// - tcp_accept_loop: a task accepting in a loop that tries again on every
//   error but a closed listener (a server's loop);
// - tcp_accept_once: a task accepting once;
// - tls_listen: a TLS listener, whose own task accepts in a loop;
// - tls_listen_wait: the same, main returning 200 ms later, the accept parked;
// - http_serve, https_serve: a server's serve task;
// - timer_after_teardown: a timer armed from the destructor of a static
//   destroyed after the runtime (made before main), on a thread and in a task;
// - blocking_after_teardown: a blocking job from the same place.
// The program must end by itself with 0, nothing on stderr but its own
// lines (the runtime's singletons are statics destroyed at exit; a wait
// woken by their destructors, or one begun after them, locked a destroyed
// mutex: "mutex lock failed", DESIGN 478).
#include "sgcl/async.h"
#include "sgcl/net/http.h"
#include "sgcl/net/tls.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

using namespace sgcl;
namespace tls = sgcl::net::tls;
namespace http = sgcl::net::http;

namespace {
    std::string slurp(const char* name) {
        std::ifstream in(std::string(SGCL_TEST_SOURCE_ROOT) + "/tests/net/tls_testdata/" + name);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    tls::config server_config() {
        tls::config c;
        c.identities = {tls::identity(string(slurp("ecdsa.pem")), string(slurp("ecdsa.key")))};
        return c;
    }

    async::task<void> accept_loop(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c && l.is_closed()) {
                co_return;
            }
        }
    }

    async::task<void> accept_once(net::listener l) {
        auto c = co_await l.async_accept();
        (void)c;
    }

    async::task<void> serve(http::server s) {
        (void)co_await s.async_serve("127.0.0.1:0");
    }

    async::task<void> serve_tls(http::server s, tls::config c) {
        (void)co_await s.async_serve_tls("127.0.0.1:0", c);
    }

    async::task<void> nap() {
        co_await async::sleep(std::chrono::milliseconds(1));
    }

    async::task<void> nap_long() {
        co_await async::sleep(std::chrono::milliseconds(50));
    }

    // Made before main, so destroyed after every static main made: the
    // runtime's singletons are gone when its destructor runs
    struct AfterTeardown {
        int which = 0;   // 1: a timer, 2: a blocking job

        ~AfterTeardown() {
            if (which == 1) {
                async::sleep(std::chrono::milliseconds(5)).wait();   // a thread's sleep: a timer
                auto e = async::after(std::chrono::milliseconds(5));
                (void)e;
                async::go(nap_long());   // a task: never runs, the scheduler is gone
                std::fputs("timer after teardown: done\n", stderr);
            } else if (which == 2) {
                int r = async::spawn_blocking([] { return 7; }).wait();
                async::go_blocking([] {});
                std::fprintf(stderr, "blocking after teardown: %d\n", r);
            }
        }
    };

    AfterTeardown after_teardown;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    const std::string m = argv[1];
    if (m == "tcp_accept_loop" || m == "tcp_accept_once") {
        auto l = net::tcp::listen("127.0.0.1:0");
        if (!l) {
            return 3;
        }
        async::go(m == "tcp_accept_loop" ? accept_loop(*l) : accept_once(*l));
        return 0;
    }
    if (m == "tls_listen" || m == "tls_listen_wait") {
        auto l = tls::listen("127.0.0.1:0", server_config());
        if (!l) {
            return 3;
        }
        if (m == "tls_listen_wait") {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return 0;
    }
    if (m == "http_serve" || m == "https_serve") {
        http::server s;
        s.route("/", [](const http::request&, http::response_writer w) {
            (void)w;
        });
        if (m == "http_serve") {
            async::go(serve(s));
        } else {
            async::go(serve_tls(s, server_config()));
        }
        return 0;
    }
    if (m == "timer_after_teardown") {
        async::run(nap());   // the timers and the scheduler made now, so destroyed before the static
        after_teardown.which = 1;
        return 0;
    }
    if (m == "blocking_after_teardown") {
        if (async::spawn_blocking([] { return 1; }).wait() != 1) {   // the pool made now
            return 3;
        }
        async::go(nap());   // and the scheduler
        after_teardown.which = 2;
        return 0;
    }
    return 2;
}
