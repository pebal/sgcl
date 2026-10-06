//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::amqp: the module's client against a minimal broker of this program's
// own. One case per run; prints one line, ns per operation, for compare.sh
// (CASES=amqp). Go's standard library has no AMQP: the Go side
// (benchmarks/go/amqp) is a minimal client by hand from the specification,
// against the same broker.
//
//   amqp server               the broker on 127.0.0.1: prints "port N", serves until killed
//   amqp <case> sgcl ADDR [n] the module's client against the broker at ADDR:
//     amqp_publish            publications of 64 B, then a declaration as the point they all arrived: per message
//     amqp_confirm            publications of 64 B with publisher confirms, each waited for: per message
//     amqp_consume            deliveries of 64 B the broker streams to a consumer, each acked: per delivery
//     amqp_get                basic.get and its message, in turn: per get
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/amqp.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace ad = sgcl::net::amqp::detail;
    namespace amqp = sgcl::net::amqp;

    void report(const char* what, double wall, double ops) {
        std::printf("amqp %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    void method(std::string& out, uint16_t ch, uint32_t m, void (*args)(ad::AmqpWriter&, const void*) = nullptr, const void* ctx = nullptr) {
        size_t at = ad::amqp_method_begin(out, ch, m);
        ad::AmqpWriter w(out);
        if (args) {
            args(w, ctx);
        }
        w.finish();
        ad::amqp_frame_end(out, at);
    }

    const std::string Body(64, 'm');

    // What the broker writes, by a task of its own: reading never waits for
    // a write (a consumer's acks keep coming while deliveries go out)
    async::task<> writer(net::connection c, async::channel<std::string> out) {
        for (;;) {
            auto b = co_await out.receive();
            if (!b) {
                co_return;
            }
            std::string all = std::move(*b);
            while (all.size() < (1 << 20)) {
                auto more = out.try_receive();
                if (!more) {
                    break;
                }
                all += *more;
            }
            if (!co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(all.data()), all.size()))) {
                out.close();
                co_return;
            }
        }
    }

    // n deliveries of 64 B to the consumer "ctag" of the channel
    async::task<> stream(async::channel<std::string> outq, uint16_t ch, long n, uint64_t first) {
        std::string out;
        for (long i = 0; i < n; ++i) {
            uint64_t t = first + uint64_t(i) + 1;
            method(out, ch, ad::m::basic_deliver, [](ad::AmqpWriter& w, const void* p) {
                w.shortstr("ctag");
                w.u64(*static_cast<const uint64_t*>(p));
                w.bit(false);
                w.shortstr("");
                w.shortstr("bench");
            }, &t);
            ad::amqp_content(out, ch, amqp::properties(), Body, 131072);
            if (out.size() > (1 << 16) || i + 1 == n) {
                if (!co_await outq.send(std::move(out))) {
                    co_return;
                }
                out = std::string();
            }
        }
    }

    // The broker: the handshake, channels, declarations answered, every
    // publication counted (and acked with confirms), a consumer of
    // "stream:N" sent N deliveries, a get answered with a message
    async::task<> serve(net::connection c) {
        async::channel<std::string> outq(256);
        async::go(writer(c, outq));
        std::string buf, out;
        std::vector<char> block(65536);
        auto read = [&]() -> async::task<bool> {
            auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
            if (!r || *r == 0) {
                co_return false;
            }
            buf.append(block.data(), *r);
            co_return true;
        };
        while (buf.size() < 8) {
            if (!co_await read()) {
                co_return;
            }
        }
        buf.erase(0, 8);
        method(out, 0, ad::m::connection_start, [](ad::AmqpWriter& w, const void*) {
            w.u8(0);
            w.u8(9);
            w.table({});
            w.longstr("PLAIN");
            w.longstr("en_US");
        });
        uint64_t published = 0, seq = 0, tag = 0;
        bool confirms = false;
        int stage = 0;   // 1 header due, 2 body due
        uint64_t body_left = 0;
        for (;;) {
            if (!out.empty()) {
                if (!co_await outq.send(std::move(out))) {
                    co_return;
                }
                out = std::string();
            }
            size_t at = 0;
            for (;;) {
                uint8_t type = 0;
                uint16_t ch = 0;
                uint32_t size = 0;
                if (!ad::amqp_frame_head(std::string_view(buf).substr(at), type, ch, size) || buf.size() - at < size_t(size) + 8) {
                    break;
                }
                std::string_view payload(buf.data() + at + 7, size);
                at += size_t(size) + 8;
                if (type == ad::FrameHeader) {
                    ad::AmqpReader r(payload);
                    (void)r.u32();
                    body_left = r.u64();
                    stage = 2;
                    if (body_left == 0) {
                        stage = 0;
                        ++published;
                        if (confirms) {
                            uint64_t s = ++seq;
                            method(out, ch, ad::m::basic_ack, [](ad::AmqpWriter& w, const void* p) {
                                w.u64(*static_cast<const uint64_t*>(p));
                                w.bit(false);
                            }, &s);
                        }
                    }
                    continue;
                }
                if (type == ad::FrameBody) {
                    body_left -= std::min<uint64_t>(body_left, payload.size());
                    if (body_left == 0) {
                        stage = 0;
                        ++published;
                        if (confirms) {
                            uint64_t s = ++seq;
                            method(out, ch, ad::m::basic_ack, [](ad::AmqpWriter& w, const void* p) {
                                w.u64(*static_cast<const uint64_t*>(p));
                                w.bit(false);
                            }, &s);
                        }
                    }
                    continue;
                }
                if (type != ad::FrameMethod || payload.size() < 4) {
                    continue;
                }
                uint32_t m = uint32_t(uint8_t(payload[0])) << 24 | uint32_t(uint8_t(payload[1])) << 16 | uint32_t(uint8_t(payload[2])) << 8 | uint8_t(payload[3]);
                ad::AmqpReader r(payload.substr(4));
                switch (m) {
                    case ad::m::connection_start_ok:
                        method(out, 0, ad::m::connection_tune, [](ad::AmqpWriter& w, const void*) {
                            w.u16(2047);
                            w.u32(131072);
                            w.u16(0);
                        });
                        break;
                    case ad::m::connection_open:
                        method(out, 0, ad::m::connection_open_ok, [](ad::AmqpWriter& w, const void*) { w.shortstr(""); });
                        break;
                    case ad::m::connection_close:
                        method(out, 0, ad::m::connection_close_ok);
                        (void)co_await outq.send(std::move(out));
                        outq.close();
                        co_return;
                    case ad::m::channel_open:
                        method(out, ch, ad::m::channel_open_ok, [](ad::AmqpWriter& w, const void*) { w.longstr(""); });
                        break;
                    case ad::m::channel_close:
                        method(out, ch, ad::m::channel_close_ok);
                        break;
                    case ad::m::confirm_select:
                        confirms = true;
                        method(out, ch, ad::m::confirm_select_ok);
                        break;
                    case ad::m::queue_declare: {
                        (void)r.u16();
                        std::string name(r.shortstr());
                        struct A {
                            std::string name;
                            uint32_t n;
                        } a{name, uint32_t(published)};
                        method(out, ch, ad::m::queue_declare_ok, [](ad::AmqpWriter& w, const void* p) {
                            auto& a = *static_cast<const A*>(p);
                            w.shortstr(a.name);
                            w.u32(a.n);
                            w.u32(0);
                        }, &a);
                        break;
                    }
                    case ad::m::basic_qos:
                        method(out, ch, ad::m::basic_qos_ok);
                        break;
                    case ad::m::basic_publish:
                        stage = 1;
                        break;
                    case ad::m::basic_consume: {
                        (void)r.u16();
                        std::string queue(r.shortstr());
                        method(out, ch, ad::m::basic_consume_ok, [](ad::AmqpWriter& w, const void*) { w.shortstr("ctag"); });
                        long n = queue.rfind("stream:", 0) == 0 ? std::atol(queue.c_str() + 7) : 0;
                        // the deliveries by a task of their own: this one goes on reading the acks
                        async::go(stream(outq, ch, n, tag));
                        tag += uint64_t(n);
                        break;
                    }
                    case ad::m::basic_get: {
                        uint64_t t = ++tag;
                        method(out, ch, ad::m::basic_get_ok, [](ad::AmqpWriter& w, const void* p) {
                            w.u64(*static_cast<const uint64_t*>(p));
                            w.bit(false);
                            w.shortstr("");
                            w.shortstr("bench");
                            w.u32(1000);
                        }, &t);
                        ad::amqp_content(out, ch, amqp::properties(), Body, 131072);
                        break;
                    }
                    default:
                        break;   // acks and the rest: nothing to answer
                }
            }
            buf.erase(0, at);
            if (!out.empty()) {
                continue;
            }
            if (!co_await read()) {
                outq.close();
                co_return;
            }
        }
        (void)stage;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: amqp server | amqp <amqp_publish|amqp_confirm|amqp_consume|amqp_get> sgcl ADDR [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        auto l = net::tcp::listen("127.0.0.1:0").value();
        std::printf("port %u\n", unsigned(l.local_endpoint().port()));
        std::fflush(stdout);
        async::spawn([](net::listener l) -> async::task<> {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                async::go(serve(*c));
            }
        }(l)).wait();
        return 0;
    }
    if (argc < 4) {
        return 2;
    }
    string url = string::concat("amqp://guest:guest@", string(argv[3]), "/");
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    auto c = amqp::client::connect(url).value();
    auto ch = c.open_channel().value();
    string body(Body);
    bool ok = true;
    auto run = [&]() -> async::task<> {
        if (what == "amqp_publish") {
            n = n ? n : 1000000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await ch.async_publish(string(), string("bench"), body));
            }
            auto q = co_await ch.async_declare_queue(string("bench"));
            ok &= q && q->messages == uint32_t(n);
            report("amqp_publish", bench::seconds_since(t0), double(n));
        } else if (what == "amqp_confirm") {
            n = n ? n : 50000;
            ok &= bool(co_await ch.async_confirm());
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await ch.async_publish(string(), string("bench"), body));
            }
            report("amqp_confirm", bench::seconds_since(t0), double(n));
        } else if (what == "amqp_consume") {
            n = n ? n : 1000000;
            auto t0 = bench::Clock::now();
            auto in = co_await ch.async_consume(string::concat("stream:", string(std::to_string(n))));
            ok &= bool(in);
            for (long i = 0; ok && i < n; ++i) {
                auto d = co_await in->async_receive();
                ok &= d && d->body.size() == 64;
                if (d) {
                    ok &= bool(co_await ch.async_ack(d->delivery_tag));
                }
            }
            report("amqp_consume", bench::seconds_since(t0), double(n));
        } else if (what == "amqp_get") {
            n = n ? n : 50000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto d = co_await ch.async_get(string("bench"));
                ok &= d && *d && (*d)->body.size() == 64;
            }
            report("amqp_get", bench::seconds_since(t0), double(n));
        } else {
            ok = false;
        }
        (void)co_await c.async_close();
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
