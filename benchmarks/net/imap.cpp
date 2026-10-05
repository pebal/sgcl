//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap: the module's server and its client. One case per run; prints
// one line, ns per operation, for compare.sh (CASES=imap). Go's standard
// library has no IMAP (and go-imap is not taken): the Go side
// (benchmarks/go/net/imap.go) is a minimal client written by hand from RFC
// 9051 with the standard library alone, against the same server, so that
// the pairs compare the clients over one server; the server's own rate is
// imap_pipeline's (and benchmarks/net/imap_load.py's, Python's imaplib as
// a load generator).
//
//   imap server               the module's server on 127.0.0.1 over a memory_backend: user
//                             "bench"/"bench", INBOX of 1000 messages of about 1 KB (subjects,
//                             senders and flags of a few kinds), "Big" of 100 messages of 10 KB;
//                             prints "port N" and serves until killed
//   imap <case> sgcl [n]      the module's client against the server at $SGCL_IMAP_SERVER (one of
//                             its own in this process when unset), one connection:
//     imap_noop               NOOP and its answer: per command
//     imap_fetch_flags        UID FETCH 1:* (UID FLAGS) of INBOX, typed: per message
//     imap_fetch_body         UID FETCH 1:* (UID BODY.PEEK[]) of Big: per message, and MB/s
//     imap_fetch_envelope     UID FETCH 1:* (UID FLAGS ENVELOPE) of INBOX: per message
//     imap_search             UID SEARCH FROM "carol" of INBOX (the headers read): per message searched
//     imap_append             APPEND of a 2 KB message to "Drop": per message
//     imap_pipeline           1000 NOOPs written at once, their answers read: per command (the server's rate)
//     imap_rawnoop            diagnostic: imap_noop over a raw connection read by hand, no client of
//                             the module's (what a round trip costs the scheduler and the socket alone)
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/net/net.h"
#include "sgcl/net/imap.h"
#include "sgcl/io.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace imap = sgcl::net::imap;

    void report(const char* what, double wall, double ops, const char* extra = "") {
        std::printf("imap %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds(), extra);
    }

    std::string small_message(int i) {
        static const char* const from[] = {"Alice <alice@example.com>", "Bob <bob@example.org>", "Carol <carol@example.net>", "=?UTF-8?Q?Pawe=C5=82?= <pawel@example.pl>"};
        std::string m = "From: " + std::string(from[i % 4]) + "\r\nTo: bench@example.com\r\nSubject: message number " + std::to_string(i) +
                        "\r\nDate: Mon, 5 Oct 2026 10:00:00 +0200\r\nMessage-ID: <" + std::to_string(i) + "@bench>\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
        while (m.size() < 1000) {
            m += "The quick brown fox jumps over the lazy dog, again and again.\r\n";
        }
        return m;
    }

    std::string big_message(int i) {
        std::string m = "From: bulk@example.com\r\nSubject: big " + std::to_string(i) + "\r\n\r\n";
        while (m.size() < 10240) {
            m += "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789abcdefghijklmnop\r\n";
        }
        return m;
    }

    imap::memory_backend populated() {
        imap::memory_backend mail;
        mail.add_user("bench", "bench");
        for (int i = 0; i < 1000; ++i) {
            vector<string> flags;
            if (i % 3 == 0) {
                flags.push_back(string("\\Seen"));
            }
            if (i % 7 == 0) {
                flags.push_back(string("\\Flagged"));
            }
            mail.append("bench", "INBOX", string(small_message(i)), flags);
        }
        mail.create("bench", "Big", "");
        for (int i = 0; i < 100; ++i) {
            mail.append("bench", "Big", string(big_message(i)));
        }
        mail.create("bench", "Drop", "");
        return mail;
    }

    struct Local {
        imap::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
    };

    imap::client connect(const std::string& address) {
        imap::client::options o;
        o.security = imap::security::none;
        o.user = "bench";
        o.password = "bench";
        if (std::getenv("IMAP_NO_TIMEOUT")) {
            o.timeout = duration::zero();
        }
        auto c = imap::client::connect(string(address), o);
        if (!c) {
            std::fprintf(stderr, "connect: %s\n", std::string(c.error().message().view()).c_str());
            std::exit(1);
        }
        return *c;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: imap server | imap <imap_noop|imap_fetch_flags|imap_fetch_body|imap_fetch_envelope|imap_search|imap_append|imap_pipeline> sgcl [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        imap::server srv;
        srv.backend = populated();
        srv.max_literal_bytes = size_t(64) << 20;
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
    if (const char* env = std::getenv("SGCL_IMAP_SERVER")) {
        address = env;
    } else {
        local.srv.backend = populated();
        local.listener = *net::tcp::listen("127.0.0.1:0");
        local.serving = async::spawn(local.srv.async_serve(local.listener));
        address = "127.0.0.1:" + std::to_string(local.listener.local_endpoint().port());
    }
    imap::client c = connect(address);
    bool ok = true;
    // every loop in a task, the client's async forms (as the other net cases)
    auto run = [&]() -> async::task<> {
        if (what == "imap_noop") {
            n = n ? n : 50000;
            for (int i = 0; i < 1000; ++i) {
                ok &= bool(co_await c.async_noop());
            }
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await c.async_noop());
            }
            report("imap_noop", bench::seconds_since(t0), double(n));
        } else if (what == "imap_fetch_flags" || what == "imap_fetch_envelope") {
            n = n ? n : 100;
            ok &= bool(co_await c.async_select("INBOX"));
            imap::fetch_options o;
            o.envelope = what == "imap_fetch_envelope";
            size_t messages = 0;
            (void)co_await c.async_fetch(imap::sequence_set::all(), o);
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_fetch(imap::sequence_set::all(), o);
                ok &= r && r->size() == 1000;
                messages += r ? r->size() : 0;
            }
            report(what.c_str(), bench::seconds_since(t0), double(messages));
        } else if (what == "imap_fetch_body") {
            n = n ? n : 50;
            ok &= bool(co_await c.async_select("Big"));
            imap::fetch_options o;
            o.flags = false;
            o.sections = {string()};
            size_t messages = 0, bytes = 0;
            (void)co_await c.async_fetch(imap::sequence_set::all(), o);
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_fetch(imap::sequence_set::all(), o);
                ok &= r && r->size() == 100;
                if (r) {
                    messages += r->size();
                    for (const auto& m : *r) {
                        bytes += m.text().size();
                    }
                }
            }
            const double wall = bench::seconds_since(t0);
            char extra[64];
            std::snprintf(extra, sizeof(extra), " MB/s=%.1f", double(bytes) / wall / 1e6);
            report("imap_fetch_body", wall, double(messages), extra);
        } else if (what == "imap_search") {
            n = n ? n : 50;
            ok &= bool(co_await c.async_select("INBOX"));
            size_t searched = 0;
            (void)co_await c.async_search(imap::criteria::from("carol"));
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_search(imap::criteria::from("carol"));
                ok &= r && r->size() == 250;
                searched += 1000;
            }
            report("imap_search", bench::seconds_since(t0), double(searched));
        } else if (what == "imap_append") {
            n = n ? n : 5000;
            std::string body = "From: a@b\r\nSubject: dropped\r\n\r\n";
            while (body.size() < 2048) {
                body += "padding padding padding padding padding padding padding padding\r\n";
            }
            const string msg(body);
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await c.async_append("Drop", msg));
            }
            report("imap_append", bench::seconds_since(t0), double(n));
        } else if (what == "imap_rawnoop") {
            // diagnostic: NOOP over a raw connection, the reading by hand
            n = n ? n : 20000;
            auto raw = co_await net::tcp::async_connect(string(address));
            std::string buf(1 << 16, '\0');
            auto until = [&](const char* tag) -> async::task<bool> {
                std::string acc;
                for (;;) {
                    auto r = co_await raw->async_read(slice<byte>(reinterpret_cast<byte*>(buf.data()), buf.size()));
                    if (!r || *r == 0) {
                        co_return false;
                    }
                    acc.append(buf.data(), *r);
                    if (acc.find(tag) != std::string::npos) {
                        co_return true;
                    }
                }
            };
            (void)co_await raw->async_write(string("a LOGIN bench bench\r\n"));
            ok &= co_await until("a OK");
            const string cmd("x NOOP\r\n");
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                (void)co_await raw->async_write(cmd);
                ok &= co_await until("x OK");
            }
            report("imap_rawnoop", bench::seconds_since(t0), double(n));
        } else if (what == "imap_pipeline") {
            n = n ? n : 200;
            // the server's rate: a raw connection, 1000 NOOPs written at once
            auto raw = co_await net::tcp::async_connect(string(address));
            if (!raw) {
                ok = false;
                co_return;
            }
            (void)co_await raw->async_write(string("a LOGIN bench bench\r\n"));
            std::string batch;
            for (int i = 0; i < 1000; ++i) {
                batch += "n" + std::to_string(i) + " NOOP\r\n";
            }
            const string batch_text(batch);
            std::string buf(1 << 16, '\0');
            std::string tail;
            auto drain = [&](const char* last) -> async::task<bool> {
                tail.clear();
                for (;;) {
                    auto r = co_await raw->async_read(slice<byte>(reinterpret_cast<byte*>(buf.data()), buf.size()));
                    if (!r || *r == 0) {
                        co_return false;
                    }
                    tail.append(buf.data(), *r);
                    if (tail.find(last) != std::string::npos) {
                        co_return true;
                    }
                    if (tail.size() > 4096) {
                        tail.erase(0, tail.size() - 64);
                    }
                }
            };
            ok &= co_await drain("a OK");
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                (void)co_await raw->async_write(batch_text);
                ok &= co_await drain("n999 OK");
            }
            report("imap_pipeline", bench::seconds_since(t0), double(n) * 1000);
        } else {
            std::fprintf(stderr, "unknown case %s\n", what.c_str());
            ok = false;
        }
    };
    async::spawn(run()).wait();
    (void)c.logout();
    if (local.listener) {
        local.srv.close();
    }
    return ok ? 0 : 1;
}
