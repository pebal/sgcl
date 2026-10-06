//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::mqtt: the module's client through the module's broker to another of
// its clients, on the loopback. One case per run; prints one line, ns per
// message, for compare.sh (CASES=mqtt). Go's standard library has no MQTT
// and a comparison with a broker of another implementation (mosquitto is not
// installed here) would measure that broker: the numbers are compared with
// this program's own earlier runs.
//
//   mqtt <case> sgcl [n]
//     mqtt_qos0      n messages of 64 bytes at QoS 0, publisher to subscriber: per message
//     mqtt_qos1      the same at QoS 1, each publication waiting for its PUBACK: per message
//     mqtt_qos2      the same at QoS 2 (PUBREC, PUBREL, PUBCOMP): per message
//     mqtt_qos1_par  QoS 1 with 64 publications in flight (64 tasks): per message
//     mqtt_4k        n messages of 4 KB at QoS 0: per message, and MB/s
//     mqtt_fanout    QoS 0 messages to 16 subscribers: per delivery
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/mqtt.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace mqtt = sgcl::net::mqtt;

    void report(const char* what, double wall, double ops, double bytes = 0) {
        if (bytes > 0) {
            std::printf("mqtt %s ns/op=%.1f ops/s=%.0f MB/s=%.1f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, bytes / wall / 1e6, wall, bench::cpu_seconds());
        } else {
            std::printf("mqtt %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: mqtt <mqtt_qos0|mqtt_qos1|mqtt_qos2|mqtt_qos1_par|mqtt_4k|mqtt_fanout> sgcl [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    mqtt::broker b;
    b.max_queued = 1 << 22;   // no message dropped for a subscriber behind the publisher: every one counted
    auto l = net::tcp::listen("127.0.0.1:0");
    if (!l) {
        return 1;
    }
    auto serving = async::spawn(b.async_serve(*l));
    string url = string::concat("mqtt://", l->local_endpoint().to_string());
    mqtt::client::options o;
    o.max_received = 100000;
    auto pub = mqtt::client::connect(url, o).value();
    bool ok = true;
    auto run = [&]() -> async::task<> {
        if (what == "mqtt_fanout") {
            n = n ? n : 20000;
            vector<mqtt::client> subs;
            for (int i = 0; i < 16; ++i) {
                auto s = co_await mqtt::client::async_connect(url, o);
                ok &= bool(s);
                (void)co_await s->async_subscribe("fan");
                subs.push_back(*s);
            }
            auto t0 = bench::Clock::now();
            auto drain = [&](mqtt::client s) -> async::task<long> {
                long got = 0;
                while (got < n) {
                    if (!co_await s.async_receive()) {
                        break;
                    }
                    ++got;
                }
                co_return got;
            };
            vector<async::task<long>> readers;
            for (auto& s : subs) {
                readers.push_back(async::spawn(drain(s)));
            }
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await pub.async_publish("fan", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
            }
            long total = 0;
            for (auto& r : readers) {
                total += co_await std::move(r);
            }
            report("mqtt_fanout", bench::seconds_since(t0), double(total));
            ok &= total == n * 16;
            co_return;
        }
        auto sub = (co_await mqtt::client::async_connect(url, o)).value();
        (void)co_await sub.async_subscribe("bench", mqtt::qos::exactly_once);
        mqtt::qos q = what == "mqtt_qos2" ? mqtt::qos::exactly_once : (what == "mqtt_qos1" || what == "mqtt_qos1_par") ? mqtt::qos::at_least_once : mqtt::qos::at_most_once;
        bool big = what == "mqtt_4k";
        n = n ? n : (q == mqtt::qos::at_most_once ? 200000 : what == "mqtt_qos1_par" ? 100000 : 20000);
        if (big) {
            n = argc > 3 ? n : 50000;
        }
        if (what == "mqtt_qos1_par") {
            n = (n / 64) * 64;
        }
        string payload(big ? std::string(4096, 'x') : std::string(64, 'x'));
        auto drain = [&]() -> async::task<long> {
            long got = 0;
            while (got < n) {
                if (!co_await sub.async_receive()) {
                    break;
                }
                ++got;
            }
            co_return got;
        };
        auto t0 = bench::Clock::now();
        auto reader = async::spawn(drain());
        if (what == "mqtt_qos1_par") {
            auto worker = [&](long count) -> async::task<bool> {
                bool good = true;
                for (long i = 0; i < count; ++i) {
                    good &= bool(co_await pub.async_publish("bench", payload, q));
                }
                co_return good;
            };
            vector<async::task<bool>> ws;
            for (int k = 0; k < 64; ++k) {
                ws.push_back(async::spawn(worker(n / 64)));
            }
            for (auto& w : ws) {
                ok &= co_await std::move(w);
            }
        } else {
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await pub.async_publish("bench", payload, q));
            }
        }
        long got = co_await std::move(reader);
        ok &= got == n;
        report(what.c_str(), bench::seconds_since(t0), double(n), big ? double(n) * 4096 : 0);
    };
    async::spawn(run()).wait();
    b.close();
    serving.wait();
    return ok ? 0 : 1;
}
