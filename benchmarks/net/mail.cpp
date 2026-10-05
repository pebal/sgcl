//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Mail: encoding::email and net::smtp. One case per run; prints one line,
// ns per operation, for compare.sh (CASES=mail; benchmarks/go/mail/main.go
// has the Go side, the same cases).
//
//   mail mime_build sgcl [n]     a message of a text of 2 KB and an HTML of 4 KB (both with
//                                non-ASCII, quoted-printable), an attachment of 1 MB (base64),
//                                a subject and a name in encoded words, written to bytes: per message
//   mail mime_parse sgcl [n]     the same message parsed, its text and HTML decoded to UTF-8, the
//                                attachment's bytes decoded: per message
//   mail smtp_client sgcl ADDR [n]  net::smtp::client sends messages of 4 KB over one session to the
//                                server at ADDR (the Go side's minimal server, `mail smtp_serve`):
//                                per message
//   mail smtp_serve sgcl         net::smtp::server on the loopback, a handler that takes each
//                                message: prints "port N", serves until killed; the Go client
//                                (`mail smtp_send ADDR n`) feeds it, as it feeds the Go server
#include "../common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/net.h"
#include "sgcl/net/smtp.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;

    void report(const char* what, double wall, double ops) {
        std::printf("mail %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    // The texts both sides build: the same bytes
    std::string body_text() {
        std::string t;
        while (t.size() < 2048) {
            t += "Zażółć gęślą jaźń: the quick brown fox jumps over the lazy dog, line after line.\n";
        }
        return t;
    }

    std::string body_html() {
        std::string t = "<html><body>\n";
        while (t.size() < 4096) {
            t += "<p>Zażółć <b>gęślą</b> jaźń — the quick brown fox jumps over the lazy dog.</p>\n";
        }
        return t + "</body></html>\n";
    }

    vector<byte> attachment() {
        vector<byte> v(1 << 20);
        uint64_t s = 0x9E3779B97F4A7C15ull;
        for (auto& b : v) {
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            b = byte(uint8_t(s));
        }
        return v;
    }

    async::task<bool> send_all(net::smtp::client c, net::smtp::envelope e, string msg, long n) {
        for (long i = 0; i < n; ++i) {
            if (!co_await c.async_send(e, msg)) {
                co_return false;
            }
        }
        co_return true;
    }

    string build(const string& text, const string& html, const vector<byte>& file) {
        encoding::email m("Łucja Żółć <lucja@example.pl>", "Bob Example <bob@example.com>", "Raport kwartalny — zażółć gęślą jaźń", text);
        m.set_html(html);
        m.attach("raport.bin", file);
        m.set_date(time::datetime::from_unix(1791203400, time::zone::utc()));
        m.set_message_id("bench@example.pl");
        return m.to_string();
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: mail <mime_build|mime_parse|smtp_client ADDR|smtp_serve> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    bool ok = true;
    if (what == "mime_build") {
        long n = argc > 3 ? std::atol(argv[3]) : 2000;
        string text(body_text()), html(body_html());
        auto file = attachment();
        size_t total = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            total += build(text, html, file).size();
        }
        report("mime_build", bench::seconds_since(t0), double(n));
        ok = total > size_t(n) * (1 << 20);
    } else if (what == "mime_parse") {
        long n = argc > 3 ? std::atol(argv[3]) : 2000;
        string wire = build(string(body_text()), string(body_html()), attachment());
        size_t total = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto m = encoding::email::parse(wire);
            if (!m) {
                return 1;
            }
            total += m->text().size() + m->html().size();
            for (auto& a : m->attachments()) {
                total += a.content().size();
            }
        }
        report("mime_parse", bench::seconds_since(t0), double(n));
        ok = total > size_t(n) * (1 << 20);
    } else if (what == "smtp_client") {
        if (argc < 4) {
            return 2;
        }
        string addr(argv[3]);
        long n = argc > 4 ? std::atol(argv[4]) : 20000;
        auto c = net::smtp::client::connect(string::concat("smtp://", addr));
        if (!c) {
            std::fprintf(stderr, "%s\n", c.error().message().c_str());
            return 1;
        }
        net::smtp::envelope e;
        e.from = "a@example.com";
        e.to.push_back(string("b@example.com"));
        string msg = string::concat("From: a@example.com\r\nTo: b@example.com\r\nSubject: bench\r\n\r\n", std::string(4096, 'x'), "\r\n");
        auto t0 = bench::Clock::now();
        // one task sends them all, as the net cases run their loops
        ok = async::spawn(send_all(*c, e, msg, n)).wait();
        report("smtp_client", bench::seconds_since(t0), double(n));
        (void)c->quit();
    } else if (what == "smtp_serve") {
        net::smtp::server srv;
        srv.hostname = "bench.test";
        srv.handle([](net::smtp::message) {});
        auto l = net::tcp::listen("127.0.0.1:0");
        std::printf("port %d\n", int(l->local_endpoint().port()));
        std::fflush(stdout);
        (void)srv.serve(*l);
    } else {
        return 2;
    }
    return ok ? 0 : 1;
}
