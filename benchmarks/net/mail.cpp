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
//   mail dkim_sign_rsa sgcl [n]  a message of 4 KB signed by net::dkim, relaxed/relaxed, RSA 2048:
//                                per message (benchmarks/go/mailauth, a minimal DKIM by hand: Go
//                                has none)
//   mail dkim_sign_ed25519 sgcl [n]  the same with Ed25519
//   mail dkim_verify_rsa sgcl [n]    its signature checked without the key's DNS lookup: the field
//                                parsed, the body and the head hashed again, the signature verified
//   mail dkim_verify_ed25519 sgcl [n]  the same with Ed25519
//   mail dkim_body sgcl [n]      a body of 1 MB canonicalized (relaxed) into SHA-256: per body
#include "../common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/net.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net/dkim.h"

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

namespace {
    // The message both sides sign: the same bytes (benchmarks/go/mailauth)
    std::string dkim_message() {
        std::string b = "From: Alice <alice@example.com>\r\nTo: Bob <bob@example.org>\r\nSubject: Quarterly  report\r\n"
                        "Date: Tue, 06 Oct 2026 12:00:00 +0000\r\nMessage-ID: <bench@example.com>\r\n"
                        "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
        while (b.size() < 4096) {
            b += "The quick brown fox  jumps over\tthe lazy dog, line after line.  \r\n";
        }
        return b + "\r\n\r\n";
    }

    std::string dkim_big_body() {
        std::string b;
        while (b.size() < (1u << 20)) {
            b += "The quick brown fox  jumps over\tthe lazy dog, line after line.  \r\n";
        }
        return b;
    }

    net::dkim::signer dkim_signer(bool ed) {
        if (ed) {
            std::string seed(32, '\x2a');
            auto k = crypto::ed25519::private_key::from_seed(slice<const byte>(reinterpret_cast<const byte*>(seed.data()), seed.size()));
            return net::dkim::signer("example.com", "s1", k->to_pem());
        }
        return net::dkim::signer::generate("example.com", "s1");
    }

    // net::dkim's verification without its lookup: the key given
    bool dkim_check(const string& m, const net::dkim::signer& s) {
        namespace dk = sgcl::net::dkim::detail;
        dk::DkimText text(m.view());
        auto split = net::detail::mail_split(text.view);
        dk::DkimSignature sig;
        net::dkim::result r;
        if (!dk::dkim_parse_signature(split.fields[0].raw, 0, sig, r)) {
            return false;
        }
        dk::DkimPublic key;
        const char* why = nullptr;
        if (dk::dkim_parse_key(s.record().view(), sig, key, why) != net::dkim::status::pass) {
            return false;
        }
        uint64_t total = 0;
        auto bh = dk::dkim_body_hash(split.body, sig.body, sig.length, total);
        if (!std::equal(bh.begin(), bh.end(), sig.bh.begin(), sig.bh.end())) {
            return false;
        }
        auto hh = dk::dkim_head_hash(split.fields, sig.names, split.fields[0].raw, sig.head);
        slice<const byte> digest(hh.data(), hh.size());
        return key.rsa ? key.rsa->verify_digest(crypto::hash_id::sha256, digest, sig.b) : key.ed->verify(digest, sig.b);
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: mail <mime_build|mime_parse|smtp_client ADDR|smtp_serve|dkim_sign_rsa|dkim_sign_ed25519|dkim_verify_rsa|dkim_verify_ed25519|dkim_body> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    bool ok = true;
    if (what == "dkim_sign_rsa" || what == "dkim_sign_ed25519") {
        bool ed = what == "dkim_sign_ed25519";
        long n = argc > 3 ? std::atol(argv[3]) : (ed ? 50000 : 2000);
        auto s = dkim_signer(ed);
        string m(dkim_message());
        net::dkim::sign_options o;
        o.time = time::datetime::from_unix(1791246000, time::zone::utc());
        size_t total = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto r = s.sign(m, o);
            if (!r) {
                return 1;
            }
            total += r->size();
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = total > size_t(n) * m.size();
    } else if (what == "dkim_verify_rsa" || what == "dkim_verify_ed25519") {
        bool ed = what == "dkim_verify_ed25519";
        long n = argc > 3 ? std::atol(argv[3]) : 50000;
        auto s = dkim_signer(ed);
        auto m = s.sign(string(dkim_message()));
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            if (!dkim_check(*m, s)) {
                std::fprintf(stderr, "does not verify\n");
                return 1;
            }
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
    } else if (what == "dkim_body") {
        long n = argc > 3 ? std::atol(argv[3]) : 500;
        std::string body = dkim_big_body();
        unsigned sum = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            uint64_t total = 0;
            auto h = sgcl::net::dkim::detail::dkim_body_hash(body, net::dkim::canonicalization::relaxed, UINT64_MAX, total);
            sum ^= unsigned(h[0]);
        }
        report("dkim_body", bench::seconds_since(t0), double(n));
        ok = sum != 1000;
    } else if (what == "mime_build") {
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
