//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ntp against a server of the test's (its clock offset, Kiss-o'-Death,
// an unsynchronized clock, a stray answer, no answer) and against an SNTP
// server of Python's (python/server.py), and the timestamps' conversions
// of RFC 5905 (the eras of 2036) on their own; net::ping on the loopback.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/net.h"
#include "sgcl/net/ntp.h"
#include "sgcl/net/ping.h"

#include <cstdio>
#include <cstdlib>

namespace {
    namespace nd = sgcl::net::ntp::detail;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    // A server of one behaviour: its clock off by the offset, or a KoD, or unsynchronized, or silent
    struct Ntpd {
        enum class mode { good, kod, unsync, stray_first, silent };
        net::udp::socket sock;
        async::task<> serving;

        Ntpd(mode m, int64_t offset_ms = 0) {
            sock = net::udp::bind(sgcl::string("127.0.0.1:0")).value();
            serving = async::spawn([](net::udp::socket u, mode m, int64_t offset_ms) -> async::task<> {
                uint8_t in[128];
                for (;;) {
                    auto d = co_await u.async_receive_from(slice<byte>(reinterpret_cast<byte*>(in), sizeof in));
                    if (!d) {
                        co_return;
                    }
                    if (m == mode::silent || d->size < 48) {
                        continue;
                    }
                    int64_t now = time::detail::now_nanos() + offset_ms * 1000000;
                    uint8_t out[48] = {};
                    out[0] = (0 << 6) | (4 << 3) | 4;
                    out[1] = 2;
                    out[2] = 6;
                    out[3] = uint8_t(int8_t(-20));
                    out[12] = 10;
                    out[13] = 0;
                    out[14] = 0;
                    out[15] = 1;
                    if (m == mode::kod) {
                        out[1] = 0;
                        out[12] = 'R';
                        out[13] = 'A';
                        out[14] = 'T';
                        out[15] = 'E';
                    }
                    if (m == mode::unsync) {
                        out[0] = (3 << 6) | (4 << 3) | 4;
                    }
                    for (int i = 0; i < 8; ++i) {
                        out[24 + i] = in[40 + i];   // originate: the client's transmit
                    }
                    nd::ntp_put64(out + 16, nd::ntp_from_unix_nanos(now - int64_t(60) * 1000000000));
                    nd::ntp_put64(out + 32, nd::ntp_from_unix_nanos(now));
                    nd::ntp_put64(out + 40, nd::ntp_from_unix_nanos(now));
                    if (m == mode::stray_first) {
                        uint8_t stray[48];
                        std::copy(out, out + 48, stray);
                        stray[24] ^= 0xFF;   // an answer to another query: passed over
                        (void)co_await u.async_send_to(slice<const byte>(reinterpret_cast<const byte*>(stray), 48), d->from);
                    }
                    (void)co_await u.async_send_to(slice<const byte>(reinterpret_cast<const byte*>(out), 48), d->from);
                }
            }(sock, m, offset_ms));
        }

        ~Ntpd() {
            (void)sock.close();
            serving.wait();
        }

        sgcl::string address() const {
            return sock.local_endpoint().to_string();
        }
    };
}

TEST(Ntp, Timestamps) {
    // 1970 is 2208988800 s after NTP's epoch; the fraction is 2^-32 of a second
    int64_t t = int64_t(1700000000) * 1000000000 + 500000000;
    uint64_t ts = nd::ntp_from_unix_nanos(t);
    EXPECT_EQ(ts >> 32, uint64_t(1700000000 + 2208988800));
    EXPECT_EQ(ts & 0xFFFFFFFF, uint64_t(1) << 31);
    EXPECT_EQ(nd::ntp_to_unix_nanos(ts, t), t);
    // 2036's era: a timestamp of a small number of seconds, read near 2036-02-07, is the next era's
    int64_t after2036 = (int64_t(4294967296) - 2208988800 + 100) * 1000000000;
    uint64_t wrapped = nd::ntp_from_unix_nanos(after2036);
    EXPECT_EQ(wrapped >> 32, 100u);
    EXPECT_EQ(nd::ntp_to_unix_nanos(wrapped, after2036), after2036);
    // short format: 16.16 seconds
    EXPECT_EQ(nd::ntp_short(0x00018000).nanoseconds(), 1500000000);
}

TEST(Ntp, QueryAndOffset) {
    Ntpd server(Ntpd::mode::good, 2500);
    auto r = net::ntp::query(server.address());
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_NEAR(double(r->offset.milliseconds()), 2500.0, 50.0);
    EXPECT_GE(r->delay.nanoseconds(), 0);
    EXPECT_LT(r->delay.milliseconds(), 1000);
    EXPECT_EQ(r->stratum, 2);
    EXPECT_EQ(r->version, 4);
    EXPECT_EQ(r->poll, 6);
    EXPECT_NEAR(r->precision, 1.0 / (1 << 20), 1e-9);
    EXPECT_EQ(str(r->reference_id), "10.0.0.1");
    EXPECT_NEAR(double(r->time.unix() - time::now().unix()), 2.5, 1.5);
    EXPECT_NEAR(double(r->time.unix() - r->reference_time.unix()), 60.0, 1.0);
    // a stray answer first: passed over, the right one taken
    Ntpd stray(Ntpd::mode::stray_first);
    auto s = net::ntp::query(stray.address());
    ASSERT_TRUE(s) << str(s.error().message());
    EXPECT_NEAR(double(s->offset.milliseconds()), 0.0, 50.0);
}

TEST(Ntp, Refusals) {
    Ntpd kod(Ntpd::mode::kod);
    auto k = net::ntp::query(kod.address());
    ASSERT_FALSE(k);
    EXPECT_EQ(k.error().code(), net::ntp::errc::kiss_of_death);
    EXPECT_EQ(str(k.error().path()), "RATE");
    Ntpd unsync(Ntpd::mode::unsync);
    EXPECT_EQ(net::ntp::query(unsync.address()).error().code(), net::ntp::errc::unsynchronized);
    Ntpd silent(Ntpd::mode::silent);
    net::ntp::options o;
    o.timeout = std::chrono::milliseconds(200);
    auto late = net::ntp::query(silent.address(), o);
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    async::stop_source stop;
    o.timeout = std::chrono::seconds(10);
    o.stop = stop.token();
    auto pending = async::spawn(net::ntp::async_query(silent.address(), o));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.request_stop();
    auto r = pending.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::operation_canceled);
    error_code e = net::ntp::errc::kiss_of_death;
    EXPECT_EQ(std::string(e.category().name()), "ntp");
}

TEST(Ntp, PythonServer) {
    if (std::system("command -v python3 > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no python3";
    }
    std::string script = (source_root() / "tests/net/ntp/python/server.py").string();
    FILE* p = popen(("python3 " + script + " -1.25").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof line, p));
    auto r = net::ntp::query(sgcl::string("127.0.0.1:" + std::string(line, std::strlen(line) - 1)));
    pclose(p);
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_NEAR(double(r->offset.milliseconds()), -1250.0, 50.0);
    EXPECT_EQ(r->stratum, 2);
    EXPECT_EQ(str(r->reference_id), "192.0.2.1");
    EXPECT_EQ(r->root_delay.nanoseconds(), 3906250);       // 0x100 / 65536 s
    EXPECT_EQ(r->root_dispersion.nanoseconds(), 7812500);  // 0x200 / 65536 s
}

TEST(Ping, Loopback) {
    auto r = net::ping("127.0.0.1");
    if (!r && (r.error().code() == std::errc::permission_denied || r.error().code() == std::errc::operation_not_permitted)) {
        GTEST_SKIP() << "no ICMP socket here: " << str(r.error().message());
    }
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_EQ(str(r->from.to_string()), "127.0.0.1");
    EXPECT_LT(r->rtt.milliseconds(), 1000);
    EXPECT_EQ(r->bytes, 64u);
    net::ping_options o;
    o.size = 1000;
    o.ttl = 5;
    auto big = net::ping("localhost", o);
    ASSERT_TRUE(big) << str(big.error().message());
    EXPECT_EQ(big->bytes, 1008u);
    auto v6 = net::ping("::1");
    if (v6) {
        EXPECT_EQ(str(v6->from.to_string()), "::1");
    }
    // many at once, each its own reply
    vector<async::task<bool>> all;
    for (int i = 0; i < 16; ++i) {
        all.push_back(async::spawn([]() -> async::task<bool> {
            auto r = co_await net::async_ping(sgcl::string("127.0.0.1"));
            co_return bool(r);
        }()));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
    // a host whose name does not resolve
    EXPECT_FALSE(net::ping("no-such-host.invalid"));
}
