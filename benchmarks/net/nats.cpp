//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::nats: the module's client against a minimal server of this program's
// own (exact subjects and a last "*" token, which the requests' inbox
// takes: the routing is not what is measured).
// One case per run; prints one line, ns per operation, for compare.sh
// (CASES=nats). Go's standard library has no NATS: the Go side
// (benchmarks/go/nats) is a minimal client by hand from the protocol's
// documentation, against the same server.
//
//   nats server               the server on 127.0.0.1: prints "port N", serves until killed
//   nats <case> sgcl ADDR [n] the module's client against the server at ADDR:
//     nats_publish            publications of 64 B, then a flush (PING, PONG): per message
//     nats_pubsub             64 B from one connection to a subscriber on another: per message received
//     nats_request            a request and its reply through a responder on another connection: per request
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/nats.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace nats = sgcl::net::nats;

    void report(const char* what, double wall, double ops) {
        std::printf("nats %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    struct Peer {
        async::channel<std::string> out{1024};
    };

    struct Server {
        std::mutex mu;
        int next = 0;
        map<int, tracked_ptr<Peer>> peers;
        std::multimap<std::string, std::pair<int, std::string>> subs;   // subject: peer, sid
    };

    async::task<> writer(net::connection c, tracked_ptr<Peer> p) {
        for (;;) {
            auto b = co_await p->out.receive();
            if (!b) {
                co_return;
            }
            std::string all = std::move(*b);
            while (all.size() < (1 << 20)) {
                auto more = p->out.try_receive();
                if (!more) {
                    break;
                }
                all += *more;
            }
            if (!co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(all.data()), all.size()))) {
                co_return;
            }
        }
    }

    async::task<> serve(Server* srv, net::connection c) {
        tracked_ptr p = make_tracked<Peer>();
        int id;
        {
            std::lock_guard g(srv->mu);
            id = ++srv->next;
            srv->peers[id] = p;
        }
        async::go(writer(c, p));
        (void)co_await p->out.send(std::string("INFO {\"server_id\":\"BENCH\",\"max_payload\":1048576,\"headers\":true}\r\n"));
        std::string buf;
        std::vector<char> block(65536);
        for (;;) {
            size_t at = 0;
            std::string out;
            std::vector<std::pair<int, std::string>> routed;   // peer, bytes
            for (;;) {
                size_t eol = buf.find("\r\n", at);
                if (eol == std::string::npos) {
                    break;
                }
                std::string_view line(buf.data() + at, eol - at);
                if (line.rfind("PUB ", 0) == 0) {
                    size_t sp1 = line.find(' ', 4);
                    std::string subject(line.substr(4, sp1 - 4));
                    size_t last = line.rfind(' ');
                    std::string reply = last > sp1 ? std::string(line.substr(sp1 + 1, last - sp1 - 1)) : std::string();
                    size_t n = std::stoul(std::string(line.substr(last + 1)));
                    if (buf.size() < eol + 2 + n + 2) {
                        break;
                    }
                    std::string_view data(buf.data() + eol + 2, n);
                    {
                        std::lock_guard g(srv->mu);
                        for (auto it = srv->subs.begin(); it != srv->subs.end(); ++it) {
                            const std::string& f = it->first;
                            bool match = f == subject;
                            if (!match && f.size() >= 2 && f.compare(f.size() - 2, 2, ".*") == 0) {
                                std::string_view prefix(f.data(), f.size() - 1);
                                match = subject.size() > prefix.size() && subject.compare(0, prefix.size(), prefix) == 0 &&
                                        subject.find('.', prefix.size()) == std::string::npos;
                            }
                            if (!match) {
                                continue;
                            }
                            std::string msg = "MSG " + subject + " " + it->second.second + (reply.empty() ? "" : " " + reply) + " " + std::to_string(n) + "\r\n";
                            msg.append(data);
                            msg += "\r\n";
                            routed.push_back({it->second.first, std::move(msg)});
                        }
                    }
                    at = eol + 2 + n + 2;
                    continue;
                }
                if (line.rfind("SUB ", 0) == 0) {
                    size_t sp = line.find(' ', 4);
                    std::string subject(line.substr(4, sp - 4));
                    std::string sid(line.substr(line.rfind(' ') + 1));
                    std::lock_guard g(srv->mu);
                    srv->subs.insert({subject, {id, sid}});
                } else if (line == "PING") {
                    out += "PONG\r\n";
                }
                at = eol + 2;
            }
            buf.erase(0, at);
            if (!out.empty()) {
                (void)co_await p->out.send(std::move(out));
            }
            for (auto& [peer, msg] : routed) {
                tracked_ptr<Peer> to;
                {
                    std::lock_guard g(srv->mu);
                    auto it = srv->peers.find(peer);
                    if (it != srv->peers.end()) {
                        to = it->second;
                    }
                }
                if (to) {
                    (void)co_await to->out.send(std::move(msg));
                }
            }
            auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
            if (!r || *r == 0) {
                break;
            }
            buf.append(block.data(), *r);
        }
        {
            std::lock_guard g(srv->mu);
            srv->peers.erase(id);
            for (auto it = srv->subs.begin(); it != srv->subs.end();) {
                it = it->second.first == id ? srv->subs.erase(it) : std::next(it);
            }
        }
        p->out.close();
        (void)c.close();
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: nats server | nats <nats_publish|nats_pubsub|nats_request> sgcl ADDR [n]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "server") {
        auto l = net::tcp::listen("127.0.0.1:0").value();
        std::printf("port %u\n", unsigned(l.local_endpoint().port()));
        std::fflush(stdout);
        tracked_ptr srv = make_tracked<Server>();   // managed (it holds a map of handles), kept by this frame
        async::spawn([](net::listener l, tracked_ptr<Server> srv) -> async::task<> {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                async::go(serve(srv.get(), *c));
            }
        }(l, srv)).wait();
        return 0;
    }
    if (argc < 4) {
        return 2;
    }
    string addr(argv[3]);
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    auto nc = nats::client::connect(addr).value();
    string data(std::string(64, 'd'));
    bool ok = true;
    auto run = [&]() -> async::task<> {
        if (what == "nats_publish") {
            n = n ? n : 1000000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await nc.async_publish(string("bench"), data));
            }
            ok &= bool(co_await nc.async_flush());
            report("nats_publish", bench::seconds_since(t0), double(n));
        } else if (what == "nats_pubsub") {
            n = n ? n : 500000;
            auto sc = (co_await nats::client::async_connect(addr)).value();
            auto sub = (co_await sc.async_subscribe(string("bench.sub"))).value();
            ok &= bool(co_await sc.async_flush());
            auto t0 = bench::Clock::now();
            auto pub = async::spawn([](nats::client nc, string data, long n) -> async::task<bool> {
                bool ok = true;
                for (long i = 0; i < n; ++i) {
                    ok &= bool(co_await nc.async_publish(string("bench.sub"), data));
                }
                co_return ok;
            }(nc, data, n));
            for (long i = 0; ok && i < n; ++i) {
                auto m = co_await sub.async_receive();
                ok &= m && m->data.size() == 64;
            }
            ok &= co_await pub;
            report("nats_pubsub", bench::seconds_since(t0), double(n));
            sc.close();
        } else if (what == "nats_request") {
            n = n ? n : 50000;
            auto rc = (co_await nats::client::async_connect(addr)).value();
            auto svc = (co_await rc.async_subscribe(string("bench.svc"))).value();
            ok &= bool(co_await rc.async_flush());
            auto responder = async::spawn([](nats::client rc, nats::subscription svc) -> async::task<> {
                for (;;) {
                    auto m = co_await svc.async_receive();
                    if (!m) {
                        co_return;
                    }
                    (void)co_await rc.async_respond(*m, m->data);
                }
            }(rc, svc));
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await nc.async_request(string("bench.svc"), data);
                ok &= r && r->data.size() == 64;
            }
            report("nats_request", bench::seconds_since(t0), double(n));
            rc.close();
            co_await responder;
        } else {
            ok = false;
        }
        nc.close();
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
