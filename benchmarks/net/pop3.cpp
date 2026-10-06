//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::pop3: the module's server and its client. One case per run; prints
// one line, ns per operation, for compare.sh (CASES=pop3). Go's standard
// library has no POP3: the Go side (benchmarks/go/pop3) is a minimal client
// and a minimal server written by hand from RFC 1939 with the standard
// library alone; the client cases compare the clients over the module's
// server, pop3_server compares the servers under Go's client.
//
//   pop3 server               the module's server on 127.0.0.1 over a memory_backend: user
//                             "bench"/"bench", INBOX of 1000 messages of 4 KB; prints "port N"
//                             and serves until killed
//   pop3 <case> sgcl [n]      the module's client against the server at $SGCL_POP3_SERVER (one of
//                             its own in this process when unset), one session:
//     pop3_noop               NOOP and its answer: per command
//     pop3_list               LIST and UIDL of the 1000 messages: per message
//     pop3_retr               RETR of each message in turn: per message
//     pop3_retr_batch         RETR of all of them pipelined (retrieve of a list): per message
//     pop3_rawnoop            diagnostic: pop3_noop over a raw connection read by hand, no client of the
//                             module's (what a round trip costs the scheduler and the socket alone)
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace pop3 = sgcl::net::pop3;

    void report(const char* what, double wall, double ops) {
        std::printf("pop3 %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    // The messages both sides read: 4 KB each, the same bytes as the Go server's
    std::string message(int i) {
        std::string m = "From: Sender " + std::to_string(i) + " <s" + std::to_string(i) + "@example.com>\r\nSubject: Message " + std::to_string(i) + "\r\n\r\n";
        while (m.size() < 4096) {
            m += "The quick brown fox jumps over the lazy dog, line after line of it.\r\n";
        }
        return m;
    }

    net::imap::memory_backend populated() {
        net::imap::memory_backend mail;
        mail.add_user("bench", "bench");
        for (int i = 0; i < 1000; ++i) {
            (void)mail.append("bench", "INBOX", string(message(i)));
        }
        return mail;
    }

    struct Local {
        pop3::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
    };
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: pop3 server | pop3 <pop3_noop|pop3_list|pop3_retr|pop3_retr_batch> sgcl [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        pop3::server srv;
        srv.backend = populated();
        auto l = net::tcp::listen("127.0.0.1:0");
        if (!l) {
            return 1;
        }
        std::printf("port %u\n", unsigned(l->local_endpoint().port()));
        std::fflush(stdout);
        (void)srv.serve(*l);
        return 0;
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    std::string address;
    Local local;
    if (const char* env = std::getenv("SGCL_POP3_SERVER")) {
        address = env;
    } else {
        local.srv.backend = populated();
        local.listener = *net::tcp::listen("127.0.0.1:0");
        local.serving = async::spawn(local.srv.async_serve(local.listener));
        address = "127.0.0.1:" + std::to_string(local.listener.local_endpoint().port());
    }
    if (what == "pop3_rawnoop") {
        n = n ? n : 50000;
        auto conn = net::tcp::connect(string(address));
        if (!conn) {
            return 1;
        }
        net::connection raw = *conn;
        auto t = [&]() -> async::task<bool> {
            char buf[512];
            auto line = [&]() -> async::task<bool> {
                size_t got = 0;
                for (;;) {
                    auto r = co_await raw.async_read(slice<byte>(reinterpret_cast<byte*>(buf) + got, sizeof buf - got));
                    if (!r || *r == 0) {
                        co_return false;
                    }
                    got += *r;
                    if (got >= 2 && buf[got - 1] == '\n') {
                        co_return true;
                    }
                }
            };
            std::string login = "USER bench\r\nPASS bench\r\n";
            if (!co_await line()) {
                co_return false;
            }
            (void)co_await raw.async_write(slice<const byte>(reinterpret_cast<const byte*>(login.data()), login.size()));
            (void)co_await line();
            std::string noop = "NOOP\r\n";
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                (void)co_await raw.async_write(slice<const byte>(reinterpret_cast<const byte*>(noop.data()), noop.size()));
                if (!co_await line()) {
                    co_return false;
                }
            }
            report("pop3_rawnoop", bench::seconds_since(t0), double(n));
            co_return true;
        };
        bool good = async::spawn(t()).wait();
        if (local.listener) {
            local.srv.close();
            local.serving.wait();
        }
        return good ? 0 : 1;
    }
    pop3::client::options o;
    o.user = "bench";
    o.password = "bench";
    o.security = pop3::security::none;
    auto connected = pop3::client::connect(string(address), o);
    if (!connected) {
        std::fprintf(stderr, "connect: %s\n", std::string(connected.error().message().view()).c_str());
        return 1;
    }
    pop3::client c = *connected;
    bool ok = true;
    auto run = [&]() -> async::task<> {
        if (what == "pop3_noop") {
            n = n ? n : 50000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await c.async_noop());
            }
            report("pop3_noop", bench::seconds_since(t0), double(n));
        } else if (what == "pop3_list") {
            long rounds = n ? n : 200;
            size_t total = 0;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < rounds; ++i) {
                auto l = co_await c.async_list();
                ok &= bool(l);
                total += l ? l->size() : 0;
            }
            report("pop3_list", bench::seconds_since(t0), double(total));
        } else if (what == "pop3_retr") {
            long rounds = n ? n : 20;
            size_t total = 0;
            auto t0 = bench::Clock::now();
            for (long r = 0; r < rounds; ++r) {
                for (uint32_t i = 1; i <= 1000; ++i) {
                    auto m = co_await c.async_retrieve(i);
                    ok &= m && m->size() >= 4096;
                    ++total;
                }
            }
            report("pop3_retr", bench::seconds_since(t0), double(total));
        } else if (what == "pop3_retr_batch") {
            long rounds = n ? n : 20;
            vector<uint32_t> all;
            for (uint32_t i = 1; i <= 1000; ++i) {
                all.push_back(i);
            }
            size_t total = 0;
            auto t0 = bench::Clock::now();
            for (long r = 0; r < rounds; ++r) {
                auto m = co_await c.async_retrieve(all);
                ok &= m && m->size() == 1000;
                total += 1000;
            }
            report("pop3_retr_batch", bench::seconds_since(t0), double(total));
        } else {
            ok = false;
        }
        (void)co_await c.async_quit();
    };
    async::spawn(run()).wait();
    if (local.listener) {
        local.srv.close();
        local.serving.wait();
    }
    return ok ? 0 : 1;
}
