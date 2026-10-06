//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ntp and net::ping on the loopback. One case per run; prints one line,
// ns per operation, for compare.sh (CASES=ntp). Go's standard library has
// neither: the Go side (benchmarks/go/ntp) is a minimal SNTP client by hand
// over net.UDPConn against this program's server; ping has no Go side (Go
// needs x/net/icmp), its reference is the round trip of a UDP datagram.
//
//   ntp server               an SNTP server on 127.0.0.1: prints "port N", serves until killed
//   ntp ntp_query sgcl ADDR [n]   a query and its answer, in turn: per query
//   ntp ping_loopback sgcl [n]    an ICMP echo to 127.0.0.1 and its reply, in turn: per echo
//   ntp udp_roundtrip sgcl ADDR [n]  the reference: a datagram of 48 bytes to the server and its answer over one socket: per round trip
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/ntp.h"
#include "sgcl/net/ping.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::ntp::detail;

    void report(const char* what, double wall, double ops) {
        std::printf("ntp %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: ntp server | ntp <ntp_query|udp_roundtrip> sgcl ADDR [n] | ntp ping_loopback sgcl [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        auto u = net::udp::bind(string("127.0.0.1:0")).value();
        std::printf("port %u\n", unsigned(u.local_endpoint().port()));
        std::fflush(stdout);
        async::spawn([](net::udp::socket u) -> async::task<> {
            uint8_t in[128];
            for (;;) {
                auto d = co_await u.async_receive_from(slice<byte>(reinterpret_cast<byte*>(in), sizeof in));
                if (!d) {
                    co_return;
                }
                if (d->size < 48) {
                    continue;
                }
                int64_t now = time::detail::now_nanos();
                uint8_t out[48] = {};
                out[0] = (4 << 3) | 4;
                out[1] = 2;
                out[2] = 6;
                out[3] = uint8_t(int8_t(-20));
                out[15] = 1;
                for (int i = 0; i < 8; ++i) {
                    out[24 + i] = in[40 + i];
                }
                nd::ntp_put64(out + 16, nd::ntp_from_unix_nanos(now));
                nd::ntp_put64(out + 32, nd::ntp_from_unix_nanos(now));
                nd::ntp_put64(out + 40, nd::ntp_from_unix_nanos(now));
                (void)co_await u.async_send_to(slice<const byte>(reinterpret_cast<const byte*>(out), 48), d->from);
            }
        }(u)).wait();
        return 0;
    }
    bool ok = true;
    if (what == "ping_loopback") {
        long n = argc > 3 ? std::atol(argv[3]) : 20000;
        auto run = [&]() -> async::task<> {
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await net::async_ping(string("127.0.0.1")));
            }
            report("ping_loopback", bench::seconds_since(t0), double(n));
        };
        async::spawn(run()).wait();
        return ok ? 0 : 1;
    }
    if (argc < 4) {
        return 2;
    }
    string addr(argv[3]);
    long n = argc > 4 ? std::atol(argv[4]) : 30000;
    auto run = [&]() -> async::task<> {
        if (what == "ntp_query") {
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await net::ntp::async_query(addr));
            }
            report("ntp_query", bench::seconds_since(t0), double(n));
        } else if (what == "udp_roundtrip") {
            auto u = (co_await net::udp::async_connect(addr)).value();
            uint8_t out[48] = {(4 << 3) | 3};
            uint8_t in[128];
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await u.async_send(slice<const byte>(reinterpret_cast<const byte*>(out), 48)));
                ok &= bool(co_await u.async_receive(slice<byte>(reinterpret_cast<byte*>(in), sizeof in)));
            }
            report("udp_roundtrip", bench::seconds_since(t0), double(n));
        } else {
            ok = false;
        }
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
