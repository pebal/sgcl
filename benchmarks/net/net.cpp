//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The net module, stage 1a: sockets on the loopback and addresses. One case
// per run; prints one line, ns per operation, for compare.sh (CASES=net;
// benchmarks/go/net/main.go has the Go side, the same cases).
//
//   net pingpong sgcl [n]   64 B there and back over one TCP connection, both ends tasks: per round trip
//   net stream sgcl [mb]    mb megabytes (1024 by default) one way over one connection, 32 KB writes: per byte, and GB/s
//   net connect sgcl [n]    tcp::async_connect and async_accept on the loopback, then both closed: per connection
//   net parse sgcl [n]      ip_address::parse of a mix of IPv4 and IPv6 text: per address
//   net format sgcl [n]     ip_address::to_string of the same mix: per address
//   net url sgcl [n]        url::parse of a mix of absolute URLs: per URL (Go's url.Parse
//                           reads RFC 3986, not the WHATWG standard: a different amount of work)
//   net http_parse sgcl [n] a request head of twelve fields checked by the server's parser: per head
//                           (Go: http.ReadRequest over a bufio.Reader of the same bytes)
//   net http_hello sgcl [n] GET of a 13-byte body, the module's client and server on the loopback,
//                           one kept connection, one request after another: per request
//   net dns_parse sgcl [n]  a DNS response read as the resolver reads it (the id and the question
//                           checked, every section walked, the records of the type decoded into
//                           strings), MX of five, TXT of three, SRV of three in turn: per message
//   net dns_lookup sgcl [n] dns::async_lookup_mx of five records against a server on the loopback
//                           that answers from memory, one lookup after another: per lookup
//   net socks5_connect sgcl [n]  net::socks5::async_connect to an IPv4 target through a minimal SOCKS5
//                           server on the loopback (no authentication; the server dials the target and
//                           replies), then closed: per connection (Go: the same server and a minimal
//                           client by hand, the standard library having no SOCKS dialer of its own)
//   net proxy_get sgcl [n]  GET of a 13-byte body as http_hello, through a minimal HTTP forward proxy on
//                           the loopback (the request in absolute-form rewritten to origin-form, one kept
//                           connection to the origin; the same proxy on both sides): per request
//   net multipart_parse sgcl [n]  a multipart/form-data body of 20 fields and one file part of 1 MB, in
//                           memory, read part by part by http::multipart_reader, the contents read 32 KB
//                           at a time: per body, and MB/s (Go: mime/multipart.Reader)
//   net multipart_write sgcl [n]  the same form written by http::form, its file of 1 MB read from a file in
//                           $TMPDIR, the body read 32 KB at a time and dropped: per body, and MB/s
//                           (Go: mime/multipart.Writer to io.Discard, the file copied in)
//   net udp_multicast sgcl [n] 64 B sent to a group of 239.255/16 on the loopback interface and
//                           received by a socket that joined it (listen_multicast), one after
//                           the other in one task: per datagram
//   net tls_handshake sgcl [n]  a TCP connection and a full TLS 1.3 handshake on the loopback (X25519, the ECDSA P-256
//                           leaf of tests/net/tls_testdata, tickets on, none kept by the client), one byte read, both
//                           closed: per connection (run from the root of the tree: the certificates' paths)
//   net tls_resume sgcl [n] the same, every handshake resuming the session of the one before (the client's cache)
//   net tls_mtls sgcl [n]   tls_handshake with a client certificate (ECDSA P-256), required and verified
//   net tls_stream sgcl [mb]  stream over one TLS 1.3 connection (the configs of tls_handshake, AES-128-GCM): mb
//                           megabytes (1024 by default) from the client to the server, 32 KB writes and reads,
//                           records sealed and opened in the one process: per byte, and GB/s
//   net ws_echo sgcl [n]    a WebSocket text message of 64 B there and back, the module's client and
//                           server over one connection: per round trip (Go: a minimal peer written by
//                           hand from RFC 6455 with the standard library alone, both sides)
//   net ws_throughput sgcl [n]  binary messages of 1 MB from the client to the server, one way over one
//                           connection, masked: per message, and MB/s
//   net sse_events sgcl [n] Server-Sent Events of about 30 B through one stream, the module's event_stream
//                           (each flushed as it is sent) to its event_reader: per event, and events/s
//                           (Go: http.Flusher and a reader of bufio lines written by hand)
//   net ocsp_verify sgcl [n]  ocsp_response::parse and verify of tests/crypto/data/revocation's good response (a
//                           delegated responder: its certificate checked under the issuer, the response under it,
//                           the CertID and the times): per response (Go: x/crypto/ocsp ParseResponseForCert,
//                           benchmarks/go/ocsp, a module of its own)
//   net crl_1k sgcl [n]     revocation_list::parse of $SGCL_CRL_DIR/crl_1000.der (1000 entries, made by Go's
//                           `ocsp crl_make`), check_signature_from the intermediate, lookup of its last serial: per
//                           list (Go: x509.ParseRevocationList, CheckSignatureFrom, the entries scanned)
//   net crl_100k sgcl [n]   the same of crl_100000.der
//   net tls_staple sgcl [n] a TCP connection and a full TLS 1.3 handshake (X25519, the fixtures' good leaf and its
//                           intermediate), the server stapling the good response, the client verifying it
//                           (revocation_mode::staple_only), one byte read, both closed: per connection
//   net tls_nostaple sgcl [n]  the same without a staple and without revocation
//   net tls12_handshake sgcl [n]  a TCP connection and a full TLS 1.2 handshake (the client's default config:
//                           1.3 and 1.2 offered) to Go's `net tls12_server` at $SGCL_TLS12_SERVER (the module's
//                           server speaks 1.3 alone), one byte read, closed: per connection
//   net tls12_resume sgcl [n]  the same, every handshake resuming the session of the one before (the client's
//                           cache: the abbreviated handshake of TLS 1.2 by the ticket of Go's server)
//   net cookie_jar sgcl [n] a response's two Set-Cookie (parsed once, before the loop) put in a jar of 20
//                           cookies of 5 sites, then the Cookie field of the request to the same URL (6
//                           cookies: path, then creation), on one thread: per request (Go: net/http/cookiejar
//                           with a nil PublicSuffixList, x/net/publicsuffix not being in the standard library;
//                           this jar looks every host up in its embedded list)
//   net public_suffix sgcl [n]  http::registrable_domain of a mix of eight hosts (wildcards, exceptions,
//                           the private section, a punycode name): per host. No Go side: x/net/publicsuffix
//                           is not in the module cache
//   net reverse_proxy_get sgcl [n]  GET of a 13-byte body as http_hello, through http::reverse_proxy (a server's
//                           handler) to the backend, all three on the loopback, the client's connection to
//                           the proxy and the proxy's to the backend kept: per request (Go:
//                           httputil.NewSingleHostReverseProxy as the handler of a net/http server)
//   net reverse_proxy_stream sgcl [n]  GET of 1 MB through the same proxy, the backend writing it in 16 flushed
//                           pieces of 64 KB (chunked, no length: copied by the proxy as it comes): per
//                           request, and MB/s
//   net acme_server sgcl    an acme::test_server on the loopback (challenges valid as answered, certificates
//                           issued at once): prints "directory URL", serves until stdin ends or it is killed;
//                           both sides' clients run against this one server
//   net acme_order sgcl [n] a full order of one name against the acme_server at $SGCL_ACME_DIRECTORY, the account
//                           registered once before the loop: newOrder, the authorization, its http-01 challenge
//                           answered, the order polled ready, a P-256 key and its CSR made, finalize, the chain
//                           downloaded: per order (Go: golang.org/x/crypto/acme v0.31.0, benchmarks/go/acme, a
//                           module of its own)
//   net acme_jws sgcl [n]   one request's JWS as the client signs it: the protected header (alg, kid, nonce,
//                           url) as JSON, the payload {"csr":...} of 410 bytes, both base64url, ECDSA P-256 over
//                           SHA-256, the flattened JSON: per request
//   net dot_lookup sgcl [n] dns::async_lookup_mx of five records over DNS over TLS (RFC 7858) against a server
//                           on the loopback answering from memory (dns_lookup's answer, framed by its length),
//                           one kept connection, one lookup after another: per lookup (Go: a client written by
//                           hand with crypto/tls, a reader goroutine handing the answers out by id, the same
//                           padded query, dns_parse's decoder)
//   net doh_lookup sgcl [n] the same over DNS over HTTPS (RFC 8484): POST of application/dns-message to the
//                           module's http::server over TLS, HTTP/2 by ALPN, one kept connection: per lookup
//                           (Go: net/http's client and server, HTTP/2, the same decoder)
//   net mdns_parse sgcl [n] a multicast DNS response (a PTR, its SRV, TXT, A and AAAA, the names compressed) read
//                           whole, the names inside the rdata made canonical: per message (Go: a reader written
//                           by hand from RFC 1035 and RFC 6762, the standard library having no DNS codec)
//   net mdns_build sgcl [n] the same response written from its records, the names compressed: per message
//   net mdns_roundtrip sgcl [n]  a legacy unicast query (RFC 6762 §6.7) of a PTR to the loopback's group, answered
//                           by the module's responder (dns_sd::publish on the loopback) with the PTR, SRV, TXT and
//                           addresses: per round trip (Go: a responder written by hand over
//                           net.ListenMulticastUDP, the same messages)
#include "benchmarks/common.h"
#include "benchmarks/placement.h"
#include "sgcl/sgcl.h"
#include "sgcl/net/net.h"
#include "sgcl/net/mdns.h"
#include "sgcl/net/acme.h"
#include "sgcl/net/tls.h"
#include "sgcl/crypto/x509_revocation.h"
#include "sgcl/net/socks5.h"
#include "sgcl/net/url.h"
#include "sgcl/net/http/http.h"
#include "sgcl/io.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;

    const char* const Addresses[] = {"127.0.0.1", "10.1.2.3", "192.168.100.200", "8.8.8.8", "::1", "2001:db8::1", "fe80::1:2:3:4", "2001:db8:85a3::8a2e:370:7334", "::ffff:192.0.2.128", "2606:4700:4700::1111"};

    const char* const Urls[] = {"http://example.com/", "https://user:pass@example.com:8443/a/b/c?x=1&y=2#top", "http://10.0.0.1/index.html", "https://[2001:db8::1]:443/", "http://example.com/a/../b/./c/d?q=hello%20world", "ftp://ftp.example.org/pub/file.tar.gz", "https://api.example.com/v1/users/12345/orders?limit=50&offset=100", "ws://chat.example.com/socket"};

    // The responses of dns_parse, the same bytes as the Go side's
    const char* const DnsResponses[] = {
        "123481800001000500000001076578616d706c6503636f6d00000f0001c00c000f000100000e10001b00050d676d61696c2d736d74702d696e016c06676f6f676c65c014c00c000f000100000e100009000f04616c7431c02bc00c000f000100000e100009001904616c7432c02bc00c000f000100000e100009002304616c7433c02bc00c000f000100000e100009002d04616c7434c02b00002904d0000000000000",
        "123481800001000300000001076578616d706c6503636f6d0000100001c00c001000010000012c002524763d7370663120696e636c7564653a5f7370662e6578616d706c652e636f6d207e616c6cc00c001000010000012c004544676f6f676c652d736974652d766572696669636174696f6e3d7744384e3769314a544e546b657a4a34397377765757343866385f39787665524556346f422d304866356fc00c001000010000012c002c2b4d533d4534413638423941423242423936373042434531353431324636323931363136344330423230424200002904d0000000000000",
        "1234818000010003000000010c5f786d70702d736572766572045f746370076578616d706c6503636f6d0000210001c00c002100010000012c001900050032149505786d707031076578616d706c6503636f6d00c00c002100010000012c001900050032149505786d707032076578616d706c6503636f6d00c00c002100010000012c001f000a000014950b786d70702d6261636b7570076578616d706c65036e65740000002904d0000000000000"};

    std::string unhex(const char* h) {
        std::string out;
        for (size_t i = 0; h[i] && h[i + 1]; i += 2) {
            auto v = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
            out += char(v(h[i]) * 16 + v(h[i + 1]));
        }
        return out;
    }

    // The server of dns_lookup: each query answered from memory, its header
    // and question echoed, five MX records after them whose owner points to
    // the question's name (the same bytes as the Go side's server)
    task<> dns_serve(net::udp::socket s) {
        tracked_ptr<array<byte, 2048>> in = make_tracked<array<byte, 2048>>();
        tracked_ptr<array<byte, 2048>> out = make_tracked<array<byte, 2048>>();
        static const uint8_t record[] = {0xc0, 0x0c, 0, 15, 0, 1, 0, 0, 0x0e, 0x10, 0, 8, 0, 10, 3, 'm', 'x', '0', 0xc0, 0x0c};
        for (;;) {
            auto d = co_await s.async_receive_from(slice<byte>(in, in->data(), in->size()));
            if (!d) {
                co_return;
            }
            auto* q = reinterpret_cast<uint8_t*>(in->data());
            auto* a = reinterpret_cast<uint8_t*>(out->data());
            size_t end = 12;
            while (end < d->size && q[end] != 0) {
                end += size_t(q[end]) + 1;
            }
            end += 5;   // the root's zero, the type, the class
            if (d->size < end) {
                continue;
            }
            sgcl::detail::copy_bytes(a, q, end);
            a[2] = 0x81;
            a[3] = 0x80;
            a[6] = 0;
            a[7] = 5;
            a[8] = a[9] = a[10] = a[11] = 0;
            size_t n = end;
            for (int i = 0; i < 5; ++i) {
                sgcl::detail::copy_bytes(a + n, record, sizeof(record));
                a[n + 13] = uint8_t(10 * (i + 1));
                a[n + 17] = uint8_t('1' + i);
                n += sizeof(record);
            }
            (void)co_await s.async_send_to(slice<const byte>(out, out->data(), n), d->from);
        }
    }

    task<long> multicast_rounds(net::udp::socket out, net::udp::socket in, net::endpoint to, long n) {
        tracked_ptr<array<byte, 64>> block = make_tracked<array<byte, 64>>();
        tracked_ptr<array<byte, 2048>> room = make_tracked<array<byte, 2048>>();
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto w = co_await out.async_send_to(slice<const byte>(block, block->data(), block->size()), to);
            if (!w) {
                break;
            }
            auto r = co_await in.async_receive_from(slice<byte>(room, room->data(), room->size()));
            if (!r) {
                break;   // a datagram lost: the deadline ends the wait
            }
            ok += r->size == 64;
        }
        co_return ok;
    }

    void report(const char* what, double wall, double ops, const char* extra = "") {
        std::printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds(), extra);
    }

    task<> echo(net::connection c) {
        (void)co_await c.async_copy_to(c);
        (void)c.close();
    }

    task<> serve(net::listener l, long connections) {
        for (long i = 0; i < connections; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(echo(*c));
        }
    }

    task<long> ping(net::connection c, long n) {
        tracked_ptr<array<byte, 64>> block = make_tracked<array<byte, 64>>();
        slice<byte> buf(block, block->data(), block->size());
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto w = co_await c.async_write(buf);
            if (!w) {
                break;
            }
            auto r = co_await c.async_read_full(buf);
            if (!r || *r == 0) {
                break;
            }
            ++ok;
        }
        (void)c.close();
        co_return ok;
    }

    task<size_t> sink(net::listener l) {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return 0;
        }
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t total = 0;
        for (;;) {
            auto r = co_await c->async_read(buf);
            if (!r || *r == 0) {
                break;
            }
            total += *r;
        }
        (void)c->close();
        co_return total;
    }

    task<size_t> source(net::connection c, size_t bytes) {
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t sent = 0;
        while (sent < bytes) {
            auto w = co_await c.async_write(buf);
            if (!w) {
                break;
            }
            sent += *w;
        }
        (void)c.close();
        co_return sent;
    }

    task<long> accept_all(net::listener l, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                break;
            }
            (void)c->close();
            ++ok;
        }
        co_return ok;
    }

    // A minimal SOCKS5 server (RFC 1928): no authentication, CONNECT to an
    // IPv4 address; the target dialed, the reply sent, the client's close
    // awaited, both closed
    task<> socks5_one(net::connection c) {
        tracked_ptr<array<byte, 16>> block = make_tracked<array<byte, 16>>();
        auto b = [&](size_t i) { return uint8_t((*block)[i]); };
        if (!co_await c.async_read_full(slice<byte>(block, block->data(), 3))) {
            (void)c.close();
            co_return;
        }
        (void)co_await c.async_write(sgcl::string(std::string_view("\x05\x00", 2)));
        auto r = co_await c.async_read_full(slice<byte>(block, block->data(), 10));
        if (!r || *r != 10 || b(3) != 1) {
            (void)c.close();
            co_return;
        }
        net::endpoint to(ip_address::v4(b(4), b(5), b(6), b(7)), uint16_t((b(8) << 8) | b(9)));
        auto t = co_await tcp::async_connect(to);
        if (!t) {
            (void)c.close();
            co_return;
        }
        (void)co_await c.async_write(sgcl::string(std::string_view("\x05\x00\x00\x01\x7f\x00\x00\x01\x00\x00", 10)));
        (void)co_await c.async_read(slice<byte>(block, block->data(), 16));   // the client's close
        (void)c.close();
        (void)t->close();
    }

    task<> socks5_server(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(socks5_one(*c));
        }
    }

    task<long> socks5_all(sgcl::string proxy, sgcl::string target, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await net::socks5::async_connect(proxy, target);
            if (!c) {
                break;
            }
            (void)c->close();
            ++ok;
        }
        co_return ok;
    }

    // A minimal HTTP forward proxy: a connection to the origin per client
    // connection, kept; each request head's absolute-form target cut to
    // origin-form and written there, the response read by its
    // Content-Length and written back (GETs without bodies)
    task<> proxy_one(net::connection c, net::endpoint origin) {
        auto up = co_await tcp::async_connect(origin);
        if (!up) {
            (void)c.close();
            co_return;
        }
        tracked_ptr<array<byte, 16384>> block = make_tracked<array<byte, 16384>>();
        slice<byte> buf(block, block->data(), block->size());
        std::string in, out;
        for (;;) {
            size_t e;
            while ((e = in.find("\r\n\r\n")) == std::string::npos) {
                auto n = co_await c.async_read(buf);
                if (!n || *n == 0) {
                    (void)c.close();
                    (void)up->close();
                    co_return;
                }
                in.append(reinterpret_cast<const char*>(block->data()), *n);
            }
            std::string head = in.substr(0, e + 4);
            in.erase(0, e + 4);
            size_t sp = head.find(' ');
            size_t scheme = head.find("://", sp);
            size_t path = head.find('/', scheme + 3);
            head.erase(sp + 1, path - sp - 1);
            if (!co_await up->async_write(sgcl::string(std::string_view(head)))) {
                break;
            }
            while ((e = out.find("\r\n\r\n")) == std::string::npos) {
                auto n = co_await up->async_read(buf);
                if (!n || *n == 0) {
                    (void)c.close();
                    (void)up->close();
                    co_return;
                }
                out.append(reinterpret_cast<const char*>(block->data()), *n);
            }
            size_t length = 0;
            size_t cl = out.find("Content-Length: ");
            if (cl != std::string::npos && cl < e) {
                length = std::strtoul(out.c_str() + cl + 16, nullptr, 10);
            }
            while (out.size() < e + 4 + length) {
                auto n = co_await up->async_read(buf);
                if (!n || *n == 0) {
                    (void)c.close();
                    (void)up->close();
                    co_return;
                }
                out.append(reinterpret_cast<const char*>(block->data()), *n);
            }
            if (!co_await c.async_write(sgcl::string(std::string_view(out.data(), e + 4 + length)))) {
                break;
            }
            out.erase(0, e + 4 + length);
        }
        (void)c.close();
        (void)up->close();
    }

    task<> proxy_server(net::listener l, net::endpoint origin) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(proxy_one(*c, origin));
        }
    }

    // A reader over a text, as Go's bytes.Reader: the text not copied
    struct TextReader : io::mixin::reader<TextReader> {
        sgcl::string text;
        size_t at = 0;

        explicit TextReader(const sgcl::string& t) noexcept
        : text(t) {
        }

        expected<size_t, io::error> read(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), text.size() - at);
            sgcl::detail::copy_bytes(out.data(), text.data() + at, n);
            at += n;
            return n;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            co_return read(out);
        }
    };

    // The multipart cases' file part: 1 MB of bytes from a generator
    std::string multipart_file() {
        std::string s(1 << 20, '\0');
        uint32_t x = 12345;
        for (auto& c : s) {
            x = x * 1103515245 + 12345;
            c = char(x >> 23);
        }
        return s;
    }

    // The same body both sides parse: 20 fields "fieldN" = "value N ..."
    // and the file, the boundary of Go's length
    std::string multipart_body(const std::string& boundary) {
        std::string b;
        for (int i = 0; i < 20; ++i) {
            b += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"field" + std::to_string(i) + "\"\r\n\r\nvalue " + std::to_string(i) +
                 " of the form, some text\r\n";
        }
        b += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"data.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n";
        b += multipart_file();
        b += "\r\n--" + boundary + "--\r\n";
        return b;
    }

    task<long> connect_all(net::endpoint to, long n) {
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await tcp::async_connect(to);
            if (!c) {
                break;
            }
            (void)c->close();
            ++ok;
        }
        co_return ok;
    }
    // The TLS cases: the server writes one byte on each connection (the
    // ticket goes with it) and closes it; the client reads it and closes
    task<long> tls_serve(net::listener l, long n) {
        tracked_ptr<array<byte, 1>> block = make_tracked<array<byte, 1>>();
        slice<byte> buf(block, block->data(), block->size());
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await l.async_accept();
            if (!c) {
                break;
            }
            if (co_await c->async_write(buf)) {
                ++ok;
            }
            (void)c->close();
        }
        co_return ok;
    }

    task<long> tls_dial(sgcl::string address, net::tls::config cfg, long n) {
        tracked_ptr<array<byte, 1>> block = make_tracked<array<byte, 1>>();
        slice<byte> buf(block, block->data(), block->size());
        long ok = 0;
        for (long i = 0; i < n; ++i) {
            auto c = co_await net::tls::async_connect(address, cfg);
            if (!c) {
                std::fprintf(stderr, "%s\n", std::string(c.error().message().view()).c_str());
                break;
            }
            auto r = co_await c->async_read_full(buf);
            if (r && *r == 1) {
                ++ok;
            }
            (void)c->close();
        }
        co_return ok;
    }

    std::string slurp(const char* path) {
        std::FILE* f = std::fopen(path, "rb");
        std::string s;
        if (!f) {
            return s;
        }
        char b[4096];
        size_t n;
        while ((n = std::fread(b, 1, sizeof b, f)) > 0) {
            s.append(b, n);
        }
        std::fclose(f);
        return s;
    }

    net::tls::identity identity_of(const char* name) {
        std::string dir = "tests/net/tls_testdata/";
        return net::tls::identity(sgcl::string(slurp((dir + name + ".pem").c_str())), sgcl::string(slurp((dir + name + ".key").c_str())));
    }

    // The WebSocket cases' server: an echo, or a sink that counts the bytes
    // it receives and answers the last message with its count
    net::http::server ws_server(bool echo) {
        net::http::server s;
        s.route("/ws", [echo](net::http::request req, net::http::response_writer w) -> task<> {
            auto c = co_await net::http::websocket::async_accept(req, w);
            if (!c) {
                co_return;
            }
            uint64_t total = 0;
            for (;;) {
                auto m = co_await c->async_receive();
                if (!m) {
                    co_return;
                }
                if (echo) {
                    if (!co_await c->async_send(m->text())) {
                        co_return;
                    }
                    continue;
                }
                if (!m->binary) {   // the end: the count back
                    (void)co_await c->async_send(sgcl::string(std::to_string(total)));
                    continue;
                }
                total += m->data.size();
            }
        });
        return s;
    }

    // dns_lookup's answer to a query: its header and question echoed, five
    // MX records after them; its size, 0 for a query that does not read
    size_t dns_answer_of(const uint8_t* q, size_t size, uint8_t* a) {
        static const uint8_t record[] = {0xc0, 0x0c, 0, 15, 0, 1, 0, 0, 0x0e, 0x10, 0, 8, 0, 10, 3, 'm', 'x', '0', 0xc0, 0x0c};
        size_t end = 12;
        while (end < size && q[end] != 0) {
            end += size_t(q[end]) + 1;
        }
        end += 5;
        if (size < end) {
            return 0;
        }
        sgcl::detail::copy_bytes(a, q, end);
        a[2] = 0x81;
        a[3] = 0x80;
        a[6] = 0;
        a[7] = 5;
        a[8] = a[9] = a[10] = a[11] = 0;
        size_t n = end;
        for (int i = 0; i < 5; ++i) {
            sgcl::detail::copy_bytes(a + n, record, sizeof(record));
            a[n + 13] = uint8_t(10 * (i + 1));
            a[n + 17] = uint8_t('1' + i);
            n += sizeof(record);
        }
        return n;
    }

    // dot_lookup's server: each connection's queries read by their length,
    // answered one after another
    task<> dot_connection(net::connection c) {
        tracked_ptr<array<byte, 4096>> in = make_tracked<array<byte, 4096>>();
        tracked_ptr<array<byte, 4096>> out = make_tracked<array<byte, 4096>>();
        for (;;) {
            auto r = co_await c.async_read_full(slice<byte>(in, in->data(), 2));
            if (!r || *r != 2) {
                break;
            }
            size_t len = size_t((*in)[0]) << 8 | size_t((*in)[1]);
            r = co_await c.async_read_full(slice<byte>(in, in->data(), len));
            if (!r) {
                break;
            }
            auto* a = reinterpret_cast<uint8_t*>(out->data());
            size_t n = dns_answer_of(reinterpret_cast<const uint8_t*>(in->data()), len, a + 2);
            a[0] = uint8_t(n >> 8);
            a[1] = uint8_t(n);
            if (!co_await c.async_write(slice<const byte>(out, out->data(), n + 2))) {
                break;
            }
        }
        (void)c.close();
    }

    task<> dot_serve(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            go(dot_connection(*c));
        }
    }

    // The mDNS cases' response: a PTR of _bench._tcp.local., its SRV, TXT,
    // A and AAAA (the same records as the Go side's)
    std::vector<sgcl::net::detail::MdnsRecord> mdns_records() {
        namespace nd = sgcl::net::detail;
        auto name = [](const char* t) {
            nd::DnsName n;
            nd::dns_name_from_text(t, n);
            return n;
        };
        std::vector<nd::MdnsRecord> out(5);
        out[0].name = name("_bench._tcp.local.");
        out[0].type = nd::dns_type::ptr;
        out[0].ttl = 4500;
        nd::mdns_append_name(out[0].rdata, name("Bench Service._bench._tcp.local."));
        out[1].name = name("Bench Service._bench._tcp.local.");
        out[1].type = nd::dns_type::srv;
        out[1].unique = true;
        out[1].ttl = 120;
        nd::mdns_append_u16(out[1].rdata, 0);
        nd::mdns_append_u16(out[1].rdata, 0);
        nd::mdns_append_u16(out[1].rdata, 8080);
        nd::mdns_append_name(out[1].rdata, name("benchhost.local."));
        out[2].name = out[1].name;
        out[2].type = nd::dns_type::txt;
        out[2].unique = true;
        out[2].ttl = 4500;
        out[2].rdata = nd::mdns_txt_rdata({"path=/api", "version=1", "secure"});
        out[3].name = name("benchhost.local.");
        out[3].type = nd::dns_type::a;
        out[3].unique = true;
        out[3].ttl = 120;
        out[3].rdata = std::string("\xc0\xa8\x01\x0a", 4);
        out[4].name = out[3].name;
        out[4].type = nd::dns_type::aaaa;
        out[4].unique = true;
        out[4].ttl = 120;
        out[4].rdata = std::string("\xfe\x80\0\0\0\0\0\0\x02\x11\x22\xff\xfe\x33\x44\x55", 16);
        return out;
    }

    size_t mdns_write(const std::vector<sgcl::net::detail::MdnsRecord>& records, uint8_t* buf, size_t cap) {
        namespace nd = sgcl::net::detail;
        nd::MdnsWriter w(buf, cap, 0, nd::DnsFlagResponse | nd::DnsFlagAuthoritative);
        w.record(0, records[0], records[0].ttl, false);
        for (size_t i = 1; i < records.size(); ++i) {
            w.record(2, records[i], records[i].ttl, true);
        }
        return w.finish();
    }

}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: net <pingpong|stream|connect|parse|format|url|http_parse|http_hello|dns_parse|dns_lookup|socks5_connect|proxy_get|multipart_parse|multipart_write|udp_multicast|tls_handshake|tls_resume|tls_mtls|ws_echo|ws_throughput|sse_events|tls12_handshake|tls12_resume|cookie_jar|public_suffix|ocsp_verify|crl_1k|crl_100k|tls_staple|tls_nostaple|reverse_proxy_get|reverse_proxy_stream|acme_server|acme_order|acme_jws|tls_stream|dot_lookup|doh_lookup|mdns_parse|mdns_build|mdns_roundtrip> sgcl [n]\n");
        return 2;
    }
    std::string what = argv[1];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    bool ok = true;
    if (what == "pingpong") {
        n = n ? n : 50000;
        auto l = tcp::listen("127.0.0.1:0");
        auto server = spawn(serve(*l, 1));
        auto c = tcp::connect(l->local_endpoint());
        auto t0 = bench::Clock::now();
        long done = spawn(ping(*c, n)).wait();
        report("pingpong", bench::seconds_since(t0), double(n));
        server.wait();
        l->close();
        ok = done == n;
    } else if (what == "stream") {
        size_t mb = n ? size_t(n) : 1024;
        size_t bytes = mb << 20;
        auto l = tcp::listen("127.0.0.1:0");
        auto receiver = spawn(sink(*l));
        auto c = tcp::connect(l->local_endpoint());
        auto t0 = bench::Clock::now();
        size_t sent = spawn(source(*c, bytes)).wait();
        size_t received = receiver.wait();
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " GB/s=%.2f", double(received) / wall / 1e9);
        report("stream", wall, double(received), extra);
        l->close();
        ok = sent >= bytes && received == sent;
    } else if (what == "connect") {
        n = n ? n : 2000;   // RUNS of both variants inside the 16384 ephemeral ports a TIME_WAIT of 30 s leaves
        auto l = tcp::listen("127.0.0.1:0");
        auto acceptor = spawn(accept_all(*l, n));
        auto t0 = bench::Clock::now();
        long made = spawn(connect_all(l->local_endpoint(), n)).wait();
        if (made < n) {
            l->close();   // a connect failed (the ports ran out): the accepts end rather than wait for connections that will not come
        }
        long taken = acceptor.wait();
        report("connect", bench::seconds_since(t0), double(n));
        l->close();
        ok = made == n && taken == n;
    } else if (what == "parse" || what == "format") {
        n = n ? n : 20000000;
        const size_t k = std::size(Addresses);
        sgcl::string texts[k];
        ip_address parsed[k];
        for (size_t i = 0; i < k; ++i) {
            texts[i] = Addresses[i];
            parsed[i] = *ip_address::parse(texts[i]);
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        if (what == "parse") {
            for (long i = 0; i < n; ++i) {
                auto a = ip_address::parse(texts[size_t(i) % k]);
                check += a->is_v4();
            }
        } else {
            for (long i = 0; i < n; ++i) {
                check += parsed[size_t(i) % k].to_string().size();
            }
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "url") {
        n = n ? n : 2000000;
        const size_t k = std::size(Urls);
        sgcl::string texts[k];
        for (size_t i = 0; i < k; ++i) {
            texts[i] = Urls[i];
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto u = url::parse(texts[size_t(i) % k]);
            check += u->path().size();
        }
        report("url", bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "http_parse") {
        n = n ? n : 2000000;
        const std::string text = "GET /api/v1/users/12345?fields=name,email HTTP/1.1\r\nHost: api.example.com\r\nUser-Agent: bench/1.0\r\n"
                                 "Accept: application/json\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: en-US,en;q=0.9\r\n"
                                 "Connection: keep-alive\r\nCookie: session=abc123; theme=dark\r\nReferer: https://example.com/page\r\n"
                                 "Cache-Control: no-cache\r\nX-Request-Id: 7f3c9a2e-1b4d-4e6f-8a9b-0c1d2e3f4a5b\r\n"
                                 "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.e30.x\r\nX-Forwarded-For: 203.0.113.7\r\n\r\n";
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::string head(text);
            net::http::detail::RequestLine line;
            net::http::headers h;
            net::http::detail::BodyFraming framing;
            check += net::http::detail::check_request_head(head, line, h, framing) == 0 ? h.size() : 0;
        }
        report("http_parse", bench::seconds_since(t0), double(n));
        ok = check == size_t(n) * 12;
    } else if (what == "http_hello") {
        n = n ? n : 50000;
        net::http::server server;
        server.route("GET /hello", [](net::http::request, net::http::response_writer w) { w.write("hello, world\n"); });
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(server.async_serve(*l));
        net::http::client client;
        auto url = sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/hello");
        size_t check = 0;
        auto t0 = bench::Clock::now();
        auto run = [&]() -> task<> {
            for (long i = 0; i < n; ++i) {
                auto res = co_await client.async_get(url);
                if (res) {
                    check += (co_await res->async_text())->size();
                }
            }
        };
        spawn(run()).wait();
        report("http_hello", bench::seconds_since(t0), double(n));
        server.close();
        (void)serving.wait();
        ok = check == size_t(n) * 13;
    } else if (what == "dns_parse") {
        n = n ? n : 5000000;
        namespace nd = sgcl::net::detail;
        std::string messages[3];
        nd::DnsName names[3];
        const uint16_t types[3] = {nd::dns_type::mx, nd::dns_type::txt, nd::dns_type::srv};
        for (size_t i = 0; i < 3; ++i) {
            messages[i] = unhex(DnsResponses[i]);
        }
        nd::dns_name_from_text("example.com.", names[0]);
        nd::dns_name_from_text("example.com.", names[1]);
        nd::dns_name_from_text("_xmpp-server._tcp.example.com.", names[2]);
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            size_t k = size_t(i) % 3;
            nd::DnsAnswer a;
            nd::dns_read_answer(reinterpret_cast<const uint8_t*>(messages[k].data()), messages[k].size(), 0x1234, names[k], types[k], false, a);
            check += a.records.size();
        }
        report("dns_parse", bench::seconds_since(t0), double(n));
        ok = check == size_t(n / 3) * 11 + (n % 3 > 0 ? 5 : 0) + (n % 3 > 1 ? 3 : 0);
    } else if (what == "dns_lookup") {
        n = n ? n : 20000;
        auto s = net::udp::bind("127.0.0.1:0");
        auto serving = spawn(dns_serve(*s));
        net::dns::options o;
        o.servers = {sgcl::string("127.0.0.1:" + std::to_string(s->local_endpoint().port()))};
        o.timeout = std::chrono::seconds(1);
        o.attempts = 1;
        size_t check = 0;
        auto run = [&](long count) -> task<> {
            for (long i = 0; i < count; ++i) {
                auto r = co_await net::dns::async_lookup_mx("example.test.", o);
                if (r) {
                    check += r->size();
                }
            }
        };
        spawn(run(100)).wait();   // warm: /etc/resolv.conf read, the socket paths taken once (Go's side warms the same)
        check = 0;
        auto t0 = bench::Clock::now();
        spawn(run(n)).wait();
        report("dns_lookup", bench::seconds_since(t0), double(n));
        (void)s->close();
        serving.wait();
        ok = check == size_t(n) * 5;
    } else if (what == "proxy_get") {
        n = n ? n : 50000;
        net::http::server server;
        server.route("GET /hello", [](net::http::request, net::http::response_writer w) { w.write("hello, world\n"); });
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(server.async_serve(*l));
        auto p = tcp::listen("127.0.0.1:0");
        auto proxying = spawn(proxy_server(*p, l->local_endpoint()));
        net::http::client client;
        client.proxy = net::http::proxy(sgcl::string("http://" + std::string(p->local_endpoint().to_string().view())));
        auto url = sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/hello");
        size_t check = 0;
        auto t0 = bench::Clock::now();
        auto run = [&]() -> task<> {
            for (long i = 0; i < n; ++i) {
                auto res = co_await client.async_get(url);
                if (res) {
                    check += (co_await res->async_text())->size();
                }
            }
        };
        spawn(run()).wait();
        report("proxy_get", bench::seconds_since(t0), double(n));
        client.close_idle_connections();
        p->close();
        proxying.wait();
        server.close();
        (void)serving.wait();
        ok = check == size_t(n) * 13;
    } else if (what == "multipart_parse") {
        n = n ? n : 2000;
        const std::string boundary = "4f3c2a1b0e9d8c7b6a5f4e3d2c1b0a99887766554433221100ffeeddccbb";
        const sgcl::string body(multipart_body(boundary));
        const sgcl::string b(boundary);
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t check = 0, parts = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            tracked_ptr source = make_tracked<TextReader>(body);   // over the text, as Go's bytes.Reader: nothing copied
            net::http::multipart_reader m(io::reader(source), b);
            while (auto p = m.next()) {
                if (!*p) {
                    break;
                }
                ++parts;
                for (;;) {
                    auto r = m.read(buf);
                    if (!r || *r == 0) {
                        break;
                    }
                    check += *r;
                }
            }
        }
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " MB/s=%.0f", double(body.size()) * double(n) / wall / 1e6);
        report("multipart_parse", wall, double(n), extra);
        ok = parts == size_t(n) * 21 && check > size_t(n) * (1 << 20);
    } else if (what == "multipart_write") {
        n = n ? n : 2000;
        const sgcl::string path = io::path::join(io::temp_dir(), "sgcl_bench_multipart.bin");
        (void)io::write_file(path, sgcl::string(multipart_file()));
        tracked_ptr<array<byte, 32768>> block = make_tracked<array<byte, 32768>>();
        slice<byte> buf(block, block->data(), block->size());
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            net::http::form f;
            for (int k = 0; k < 20; ++k) {
                f.add(sgcl::string("field" + std::to_string(k)), sgcl::string("value " + std::to_string(k) + " of the form, some text"));
            }
            f.add(net::http::form::file("upload", path, "application/octet-stream"));
            auto r = f.reader();
            if (!r) {
                break;
            }
            for (;;) {
                auto got = r->read(buf);
                if (!got || *got == 0) {
                    break;
                }
                check += *got;
            }
        }
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " MB/s=%.0f", double(check) / wall / 1e6);
        report("multipart_write", wall, double(n), extra);
        (void)io::remove(path);
        ok = check > size_t(n) * (1 << 20);
    } else if (what == "socks5_connect") {
        n = n ? n : 1000;   // two connections each: RUNS of both variants inside the ephemeral ports a TIME_WAIT leaves
        auto target = tcp::listen("127.0.0.1:0");
        auto acceptor = spawn(accept_all(*target, n));
        auto proxy = tcp::listen("127.0.0.1:0");
        auto serving = spawn(socks5_server(*proxy));
        auto t0 = bench::Clock::now();
        long made = spawn(socks5_all(proxy->local_endpoint().to_string(), target->local_endpoint().to_string(), n)).wait();
        if (made < n) {
            target->close();
        }
        long taken = acceptor.wait();
        report("socks5_connect", bench::seconds_since(t0), double(n));
        proxy->close();
        serving.wait();
        target->close();
        ok = made == n && taken == n;
    } else if (what == "udp_multicast") {
        n = n ? n : 200000;
        net::network_interface lo;
        auto all = net::interfaces();
        for (auto& i : *all) {
            if (i.loopback && i.multicast) {
                lo = i;
                break;
            }
        }
        auto in = net::udp::listen_multicast("239.255.77.1:0", lo);
        auto out = net::udp::bind("127.0.0.1:0");
        (void)out->set_multicast_interface(lo);
        net::endpoint to(net::ip_address::parse("239.255.77.1").value(), in->local_endpoint().port());
        in->set_read_deadline(clock::now() + std::chrono::seconds(60));
        (void)spawn(multicast_rounds(*out, *in, to, 1000)).wait();   // warm, as the Go side
        auto t0 = bench::Clock::now();
        long got = spawn(multicast_rounds(*out, *in, to, n)).wait();
        report("udp_multicast", bench::seconds_since(t0), double(n));
        (void)in->close();
        (void)out->close();
        ok = got == n;
    } else if (what == "ocsp_verify") {
        n = n ? n : 20000;
        const std::string dir = "tests/crypto/data/revocation/";
        std::string der = slurp((dir + "ocsp_good.der").c_str());
        auto leaf = crypto::x509::certificate::from_pem(sgcl::string(slurp((dir + "good.pem").c_str()))).value();
        auto issuer = crypto::x509::certificate::from_pem(sgcl::string(slurp((dir + "int.pem").c_str()))).value();
        slice<const byte> bytes(reinterpret_cast<const byte*>(der.data()), der.size());
        auto check = [&] {
            auto r = crypto::x509::ocsp_response::parse(bytes);
            if (!r) {
                return false;
            }
            auto s = r->verify(leaf, issuer);
            return s && s->status == crypto::x509::revocation_status::good;
        };
        for (int i = 0; i < 200; ++i) {
            (void)check();
        }
        long good = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            good += check();
        }
        report("ocsp_verify", bench::seconds_since(t0), double(n));
        ok = good == n;
    } else if (what == "crl_1k" || what == "crl_100k") {
        const bool big = what == "crl_100k";
        n = n ? n : big ? 40 : 2000;
        const char* dir_env = std::getenv("SGCL_CRL_DIR");
        std::string file = std::string(dir_env ? dir_env : ".") + (big ? "/crl_100000.der" : "/crl_1000.der");
        std::string der = slurp(file.c_str());
        if (der.empty()) {
            std::fprintf(stderr, "no %s (benchmarks/go/ocsp: ocsp crl_make <dir>, SGCL_CRL_DIR)\n", file.c_str());
            return 1;
        }
        auto issuer = crypto::x509::certificate::from_pem(sgcl::string(slurp("tests/crypto/data/revocation/int.pem"))).value();
        slice<const byte> bytes(reinterpret_cast<const byte*>(der.data()), der.size());
        vector<byte> last;
        auto check = [&] {
            auto rl = crypto::x509::revocation_list::parse(bytes);
            if (!rl || !rl->check_signature_from(issuer)) {
                return false;
            }
            if (last.empty()) {
                auto e = (*rl)[rl->size() - 1].serial_number;
                last = vector<byte>(e.data(), e.data() + e.size());
            }
            return rl->lookup(last.as_slice()).has_value();
        };
        (void)check();
        long good = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            good += check();
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = good == n;
    } else if (what == "tls_staple" || what == "tls_nostaple") {
        n = n ? n : 2000;
        const std::string dir = "tests/crypto/data/revocation/";
        const bool staple = what == "tls_staple";
        net::tls::identity id(sgcl::string(slurp((dir + "good.pem").c_str()) + slurp((dir + "int.pem").c_str())),
                              crypto::read_secret(sgcl::string(dir + "good.key")).value());
        if (staple) {
            std::string der = slurp((dir + "ocsp_good.der").c_str());
            (void)id.set_ocsp_staple(slice<const byte>(reinterpret_cast<const byte*>(der.data()), der.size()));
        }
        net::tls::config server;
        server.identities = {id};
        server.groups = {net::tls::group::x25519};
        net::tls::config client;
        client.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp((dir + "root.pem").c_str())));
        client.server_name = "localhost";
        client.groups = {net::tls::group::x25519};
        if (staple) {
            client.revocation = net::tls::revocation_mode::staple_only;
        }
        auto l = net::tls::listen("127.0.0.1:0", server);
        if (!l) {
            std::fprintf(stderr, "%s\n", std::string(l.error().message().view()).c_str());
            return 1;
        }
        sgcl::string address(std::string_view("127.0.0.1:" + std::to_string(l->local_endpoint().port())));
        auto serving = spawn(tls_serve(*l, n + 1));
        long warm = spawn(tls_dial(address, client, 1)).wait();
        auto t0 = bench::Clock::now();
        long done = spawn(tls_dial(address, client, n)).wait();
        report(what.c_str(), bench::seconds_since(t0), double(n));
        long served = serving.wait();
        l->close();
        ok = warm == 1 && done == n && served == n + 1;
    } else if (what == "tls_handshake" || what == "tls_resume" || what == "tls_mtls") {
        n = n ? n : 2000;   // ports: a TIME_WAIT each, as connect
        net::tls::config server;
        server.identities = {identity_of("ecdsa")};
        server.groups = {net::tls::group::x25519};
        net::tls::config client;
        client.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp("tests/net/tls_testdata/ca.pem")));
        client.server_name = "localhost";
        client.groups = {net::tls::group::x25519};
        if (what == "tls_resume") {
            client.session_cache = net::tls::session_cache();
        }
        if (what == "tls_mtls") {
            server.client_auth = net::tls::client_auth::require;
            server.client_roots = client.roots;
            client.identities = {identity_of("client_ecdsa")};
        }
        auto l = net::tls::listen("127.0.0.1:0", server);
        if (!l) {
            std::fprintf(stderr, "%s\n", std::string(l.error().message().view()).c_str());
            return 1;
        }
        sgcl::string address(std::string_view("127.0.0.1:" + std::to_string(l->local_endpoint().port())));
        auto serving = spawn(tls_serve(*l, n + 1));
        long warm = spawn(tls_dial(address, client, 1)).wait();   // a session for the first resumption
        auto t0 = bench::Clock::now();
        long done = spawn(tls_dial(address, client, n)).wait();
        report(what.c_str(), bench::seconds_since(t0), double(n));
        long served = serving.wait();
        l->close();
        ok = warm == 1 && done == n && served == n + 1;
    } else if (what == "tls_stream") {
        size_t mb = n ? size_t(n) : 1024;
        size_t bytes = mb << 20;
        net::tls::config server;
        server.identities = {identity_of("ecdsa")};
        server.groups = {net::tls::group::x25519};
        net::tls::config client;
        client.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp("tests/net/tls_testdata/ca.pem")));
        client.server_name = "localhost";
        client.groups = {net::tls::group::x25519};
        auto l = net::tls::listen("127.0.0.1:0", server);
        if (!l) {
            std::fprintf(stderr, "%s\n", std::string(l.error().message().view()).c_str());
            return 1;
        }
        auto receiver = spawn(sink(*l));
        auto c = net::tls::connect(sgcl::string(std::string_view("127.0.0.1:" + std::to_string(l->local_endpoint().port()))), client);
        if (!c) {
            std::fprintf(stderr, "%s\n", std::string(c.error().message().view()).c_str());
            return 1;
        }
        auto t0 = bench::Clock::now();
        size_t sent = spawn(source(*c, bytes)).wait();
        size_t received = receiver.wait();
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " GB/s=%.2f", double(received) / wall / 1e9);
        report("tls_stream", wall, double(received), extra);
        l->close();
        ok = sent >= bytes && received == sent;
    } else if (what == "ws_echo" || what == "ws_throughput") {
        const bool echo = what == "ws_echo";
        n = n ? n : echo ? 50000 : 2000;
        net::http::server server = ws_server(echo);
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(server.async_serve(*l));
        net::http::client client;
        client.proxy = net::http::proxy();
        auto url = sgcl::string("ws://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/ws");
        auto c = client.websocket(url);
        if (!c) {
            std::fprintf(stderr, "connect: %s\n", std::string(c.error().message().view()).c_str());
            return 1;
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        if (echo) {
            const sgcl::string message(std::string(64, 'm'));
            auto run = [&]() -> task<> {
                for (long i = 0; i < n; ++i) {
                    if (!co_await c->async_send(message)) {
                        co_return;
                    }
                    auto m = co_await c->async_receive();
                    if (!m) {
                        co_return;
                    }
                    check += m->data.size();
                }
            };
            spawn(run()).wait();
            report("ws_echo", bench::seconds_since(t0), double(n));
            ok = check == size_t(n) * 64;
        } else {
            vector<byte> block(size_t(1) << 20, byte(0x5A));
            auto run = [&]() -> task<> {
                for (long i = 0; i < n; ++i) {
                    if (!co_await c->async_send(block.as_slice())) {
                        co_return;
                    }
                }
                (void)co_await c->async_send("end");
                auto m = co_await c->async_receive();
                if (m) {
                    check = std::stoull(std::string(m->text().view()));
                }
            };
            spawn(run()).wait();
            double wall = bench::seconds_since(t0);
            char extra[64];
            std::snprintf(extra, sizeof(extra), " MB/s=%.0f", double(check) / wall / 1e6);
            report("ws_throughput", wall, double(n), extra);
            ok = check == size_t(n) << 20;
        }
        (void)c->close();
        server.close();
        (void)serving.wait();
    } else if (what == "sse_events") {
        n = n ? n : 200000;
        net::http::server server;
        long count = n;
        server.route("GET /events", [count](net::http::request, net::http::response_writer w) -> task<> {
            net::http::event_stream events(w);
            net::http::event e;
            e.type = "tick";
            for (long i = 0; i < count; ++i) {
                e.data = sgcl::string("event number " + std::to_string(i));
                if (!co_await events.async_send(e)) {
                    co_return;
                }
            }
        });
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(server.async_serve(*l));
        net::http::client client;
        client.proxy = net::http::proxy();
        auto url = sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/events");
        long got = 0;
        auto t0 = bench::Clock::now();
        auto run = [&]() -> task<> {
            auto res = co_await client.async_get(url);
            if (!res) {
                co_return;
            }
            net::http::event_reader r(*res);
            while (auto e = co_await r.async_next()) {
                if (!*e) {
                    break;
                }
                ++got;
            }
        };
        spawn(run()).wait();
        double wall = bench::seconds_since(t0);
        char extra[64];
        std::snprintf(extra, sizeof(extra), " events/s=%.0f", double(got) / wall);
        report("sse_events", wall, double(n), extra);
        server.close();
        (void)serving.wait();
        ok = got == n;
    } else if (what == "tls12_handshake" || what == "tls12_resume") {
        n = n ? n : 2000;
        const char* server = std::getenv("SGCL_TLS12_SERVER");
        if (!server) {
            std::fprintf(stderr, "%s: SGCL_TLS12_SERVER (the address of Go's `net tls12_server`) is not set\n", what.c_str());
            return 2;
        }
        net::tls::config client;
        client.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp("tests/net/tls_testdata/ca.pem")));
        client.server_name = "localhost";
        client.groups = {net::tls::group::x25519};
        if (what == "tls12_resume") {
            client.session_cache = net::tls::session_cache();
        }
        sgcl::string address(server);
        auto probe = net::tls::connect(address, client);   // and the session for the first resumption
        if (!probe || net::tls::state_of(*probe)->version != net::tls::version::tls12) {
            std::fprintf(stderr, "%s: no TLS 1.2 server at %s\n", what.c_str(), server);
            return 1;
        }
        (void)probe->close();
        if (what == "tls12_resume") {
            // the abbreviated handshake, checked once
            auto again = net::tls::connect(address, client);
            if (!again || !net::tls::state_of(*again)->resumed) {
                std::fprintf(stderr, "tls12_resume: the session was not resumed\n");
                return 1;
            }
            (void)again->close();
        }
        auto t0 = bench::Clock::now();
        long done = spawn(tls_dial(address, client, n)).wait();
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = done == n;
    } else if (what == "cookie_jar") {
        n = n ? n : 3000000;
        net::http::cookie_jar jar;
        const char* const sites[] = {"https://www.example.com/", "https://shop.example.co.uk/", "https://a.github.io/",
                                     "https://api.example.org/v1/", "https://example.net/"};
        for (const char* site : sites) {
            for (int k = 0; k < 4; ++k) {
                std::string field = "k" + std::to_string(k) + "=v" + std::to_string(k) + "; Path=/; Max-Age=3600";
                jar.set_cookies(net::url(site), {net::http::cookie(sgcl::string(field))});
            }
        }
        net::url u("https://www.example.com/a/b");
        sgcl::vector<net::http::cookie> response = {net::http::cookie("session=abc123; Path=/; Secure; HttpOnly"),
                                                    net::http::cookie("theme=dark; Path=/a; Max-Age=86400")};
        const std::string want = "theme=dark; k0=v0; k1=v1; k2=v2; k3=v3; session=abc123";
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            jar.set_cookies(u, response);
            check += jar.header(u).size();
        }
        report("cookie_jar", bench::seconds_since(t0), double(n));
        ok = check == size_t(n) * want.size() && jar.header(u).view() == want;
    } else if (what == "public_suffix") {
        n = n ? n : 20000000;
        const char* const hosts[] = {"www.example.com", "a.b.example.co.uk", "foo.github.io", "www.city.kobe.jp",
                                     "a.b.c.mm", "deep.sub.domain.example.org", "xn--85x722f.xn--55qx5d.cn", "api.service.blogspot.com"};
        sgcl::string names[8];
        for (int i = 0; i < 8; ++i) {
            names[i] = hosts[i];
        }
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            check += net::http::registrable_domain(names[i & 7]).size();
        }
        report("public_suffix", bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "reverse_proxy_get" || what == "reverse_proxy_stream") {
        const bool stream = what == "reverse_proxy_stream";
        n = n ? n : (stream ? 2000 : 50000);
        static const std::string filler(size_t(1) << 20, 'r');
        net::http::server backend;
        backend.route("GET /hello", [](net::http::request, net::http::response_writer w) { w.write("hello, world\n"); });
        backend.route("GET /stream", [](net::http::request, net::http::response_writer w) -> task<> {
            for (size_t at = 0; at < filler.size(); at += 65536) {
                w.write(slice<const byte>(reinterpret_cast<const byte*>(filler.data()) + at, 65536));
                if (!co_await w.async_flush()) {
                    co_return;
                }
            }
        });
        auto l = tcp::listen("127.0.0.1:0");
        auto serving = spawn(backend.async_serve(*l));
        net::http::server front;
        front.route("/", net::http::reverse_proxy(sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()))));
        auto p = tcp::listen("127.0.0.1:0");
        auto proxying = spawn(front.async_serve(*p));
        net::http::client client;
        client.proxy = net::http::proxy();
        auto url = sgcl::string("http://127.0.0.1:" + std::to_string(p->local_endpoint().port()) + (stream ? "/stream" : "/hello"));
        size_t check = 0;
        auto t0 = bench::Clock::now();
        auto run = [&]() -> task<> {
            for (long i = 0; i < n; ++i) {
                auto res = co_await client.async_get(url);
                if (res) {
                    check += (co_await res->async_bytes())->size();
                }
            }
        };
        spawn(run()).wait();
        double wall = bench::seconds_since(t0);
        char extra[64] = "";
        if (stream) {
            std::snprintf(extra, sizeof(extra), " MB/s=%.0f", double(filler.size()) * double(n) / wall / 1e6);
        }
        report(what.c_str(), wall, double(n), extra);
        client.close_idle_connections();
        front.close();
        (void)proxying.wait();
        backend.close();
        (void)serving.wait();
        ok = check == size_t(n) * (stream ? filler.size() : 13);
    } else if (what == "acme_server") {
        net::acme::test_server::options so;
        so.skip_validation = true;
        net::acme::test_server ca(so);
        std::printf("directory %s\n", std::string(ca.directory_url().view()).c_str());
        std::fflush(stdout);
        char buf[256];
        while (std::fread(buf, 1, sizeof buf, stdin) > 0) {
        }
        ca.close();
    } else if (what == "acme_order") {
        n = n ? n : 300;
        const char* dir = std::getenv("SGCL_ACME_DIRECTORY");
        if (!dir) {
            std::fprintf(stderr, "acme_order: SGCL_ACME_DIRECTORY (the URL of `net acme_server`) is not set\n");
            return 2;
        }
        net::acme::client::options o;
        o.http.proxy = net::http::proxy();
        net::acme::client c(sgcl::string(dir), net::acme::account_key(), o);
        if (!c.register_account({.terms_agreed = true})) {
            std::fprintf(stderr, "acme_order: no account at %s\n", dir);
            return 1;
        }
        auto order = [](net::acme::client c, long i) -> task<bool> {
            sgcl::string name = sgcl::string::concat("host", sgcl::string(std::to_string(i)), ".example.test");
            auto o = co_await c.async_new_order({name});
            if (!o) {
                co_return false;
            }
            auto az = co_await c.async_authorization(o->authorizations[0]);
            if (!az) {
                co_return false;
            }
            for (auto& ch : az->challenges) {
                if (ch.type == "http-01" && !co_await c.async_accept(ch)) {
                    co_return false;
                }
            }
            auto ready = co_await c.async_wait_order(o->url);
            if (!ready) {
                co_return false;
            }
            auto key = crypto::p256::private_key::generate();
            crypto::x509::certificate_request_template t;
            t.dns_names = {name};
            auto fin = co_await c.async_finalize(*ready, crypto::x509::create_certificate_request(t, key));
            if (!fin) {
                co_return false;
            }
            auto chain = co_await c.async_certificate(fin->certificate);
            co_return chain && chain->certificates.size() == 2;
        };
        auto t0 = bench::Clock::now();
        long done = spawn([](net::acme::client c, long n, decltype(order) order) -> task<long> {
            long good = 0;
            for (long i = 0; i < n; ++i) {
                good += co_await order(c, i);
            }
            co_return good;
        }(c, n, order)).wait();
        report("acme_order", bench::seconds_since(t0), double(n));
        ok = done == n;
    } else if (what == "acme_jws") {
        n = n ? n : 50000;
        net::acme::account_key key;
        const auto& st = net::acme::detail::KeyAccess::state(key);
        sgcl::string kid("https://ca.example.test/acme/account/12345678");
        sgcl::string url("https://ca.example.test/acme/finalize/12345678/87654321");
        sgcl::string nonce("Zm9vYmFyYmF6cXV4cXV1eA");
        std::string csr(400, 'A');
        sgcl::string payload = sgcl::string::concat("{\"csr\":\"", sgcl::string(csr), "\"}");
        size_t check = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            sgcl::string h = net::acme::detail::header(st, kid, nonce, url);
            check += net::acme::detail::jws(*st.key, h, payload).size();
        }
        report("acme_jws", bench::seconds_since(t0), double(n));
        ok = check > 0;
    } else if (what == "dot_lookup" || what == "doh_lookup") {
        n = n ? n : 20000;
        net::tls::config server;
        server.identities = {identity_of("ecdsa")};
        net::dns::options o;
        o.roots_pem = sgcl::string(slurp("tests/net/tls_testdata/ca.pem"));
        o.timeout = std::chrono::seconds(2);
        o.attempts = 1;
        net::http::server doh;
        doh.route("POST /dns-query", [](net::http::request req, net::http::response_writer w) -> task<> {
            auto q = co_await req.async_bytes();
            if (!q) {
                w.error(400);
                co_return;
            }
            sgcl::vector<byte> a(4096);
            size_t len = dns_answer_of(reinterpret_cast<const uint8_t*>(q->data()), q->size(), reinterpret_cast<uint8_t*>(a.data()));
            a.resize(len);
            w.set_header("Content-Type", "application/dns-message");
            w.write(a);
        });
        if (what == "doh_lookup") {
            server.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        }
        auto l = net::tls::listen("127.0.0.1:0", server);
        if (!l) {
            std::fprintf(stderr, "%s\n", std::string(l.error().message().view()).c_str());
            return 1;
        }
        std::string port = std::to_string(l->local_endpoint().port());
        task<expected<void, io::error>> serving_doh;
        task<> serving_dot;
        if (what == "dot_lookup") {
            o.servers = {sgcl::string("tls://127.0.0.1:" + port)};
            serving_dot = spawn(dot_serve(*l));
        } else {
            o.servers = {sgcl::string("https://127.0.0.1:" + port + "/dns-query")};
            serving_doh = spawn(doh.async_serve(*l));
        }
        size_t check = 0;
        auto run = [&](long count) -> task<> {
            for (long i = 0; i < count; ++i) {
                auto r = co_await net::dns::async_lookup_mx("example.test.", o);
                if (r) {
                    check += r->size();
                }
            }
        };
        spawn(run(100)).wait();   // warm: the connection made, as the Go side
        check = 0;
        auto t0 = bench::Clock::now();
        spawn(run(n)).wait();
        report(what.c_str(), bench::seconds_since(t0), double(n));
        if (what == "dot_lookup") {
            (void)l->close();
            serving_dot.wait();
        } else {
            doh.close();
            (void)serving_doh.wait();
        }
        ok = check == size_t(n) * 5;
    } else if (what == "mdns_parse" || what == "mdns_build") {
        namespace nd = sgcl::net::detail;
        n = n ? n : 2000000;
        auto records = mdns_records();
        uint8_t buf[1500];
        size_t len = mdns_write(records, buf, sizeof buf);
        size_t check = 0;
        auto t0 = bench::Clock::now();
        if (what == "mdns_parse") {
            nd::MdnsMessage m;
            for (long i = 0; i < n; ++i) {
                if (nd::mdns_read(buf, len, m)) {
                    check += m.answers.size() + m.additionals.size();
                }
            }
        } else {
            uint8_t out[1500];
            for (long i = 0; i < n; ++i) {
                check += mdns_write(records, out, sizeof out) == len ? 5 : 0;
            }
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        ok = check == size_t(n) * 5;
    } else if (what == "mdns_roundtrip") {
        namespace nd = sgcl::net::detail;
        n = n ? n : 20000;
        net::network_interface lo;
        auto all = net::interfaces();
        for (auto& i : *all) {
            if (i.loopback && i.multicast) {
                lo = i;
            }
        }
        net::mdns::options mo;
        mo.interfaces.push_back(lo);
        mo.host = "benchhost";
        net::dns_sd::service svc;
        svc.name = "Bench Service";
        svc.type = "_bench._tcp";
        svc.port = 8080;
        svc.txt = {{"path", "/api"}, {"version", "1"}};
        svc.txt.set("secure");
        auto responder = net::dns_sd::publish(svc, mo);
        if (!responder) {
            std::fprintf(stderr, "%s\n", std::string(responder.error().message().view()).c_str());
            return 1;
        }
        auto q = net::udp::bind("127.0.0.1:0");
        (void)q->set_multicast_interface(lo);
        net::endpoint group(net::ip_address::v4(224, 0, 0, 251), 5353);
        tracked_ptr<array<byte, 512>> query = make_tracked<array<byte, 512>>();
        tracked_ptr<array<byte, 2048>> room = make_tracked<array<byte, 2048>>();
        nd::DnsName ptr;
        nd::dns_name_from_text("_bench._tcp.local.", ptr);
        auto rounds = [&](long count) -> task<long> {
            long got = 0;
            q->set_read_deadline(clock::now() + std::chrono::seconds(60));
            for (long i = 0; i < count; ++i) {
                uint16_t id = uint16_t(i + 1);
                size_t len = nd::dns_write_query(reinterpret_cast<uint8_t*>(query->data()), query->size(), id, ptr, nd::dns_type::ptr, false);
                (*query)[2] = byte(0);   // no recursion desired: an mDNS query
                if (!co_await q->async_send_to(slice<const byte>(query, query->data(), len), group)) {
                    break;
                }
                for (;;) {
                    auto d = co_await q->async_receive_from(slice<byte>(room, room->data(), room->size()));
                    if (!d) {
                        co_return got;
                    }
                    auto* a = reinterpret_cast<const uint8_t*>(room->data());
                    if (d->size >= 12 && uint16_t(a[0] << 8 | a[1]) == id) {
                        got += d->size > 100;
                        break;
                    }
                }
            }
            co_return got;
        };
        (void)spawn(rounds(200)).wait();   // warm, as the Go side
        auto t0 = bench::Clock::now();
        long got = spawn(rounds(n)).wait();
        report("mdns_roundtrip", bench::seconds_since(t0), double(n));
        (void)q->close();
        responder->close();
        ok = got == n;
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    sgcl::async::scheduler::stop();
    return ok ? 0 : 1;
}
