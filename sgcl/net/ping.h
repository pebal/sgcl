//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns.h"
#include "error.h"
#include "ip.h"
#include "socket.h"
#include "detail/fd.h"
#include "detail/sockaddr.h"
#include "../async/coroutine.h"
#include "../async/promise.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../core/aliases.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../crypto/random.h"
#include "../io/error.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// ICMP echo (ping, RFC 792 and RFC 4443): an echo request and its reply,
// over an unprivileged datagram socket of ICMP where the system has one
// (macOS; Linux within net.ipv4.ping_group_range), else a raw socket
// (root, or CAP_NET_RAW on Linux)
namespace sgcl::net {
    // How an echo is asked for
    struct ping_options {
        duration timeout = std::chrono::seconds(2);   // the reply waited for at most; zero: none
        size_t size = 56;                             // the payload's bytes (56: ping's own, 64 with the ICMP head); 8 at least
        int ttl = 0;                                  // the request's TTL (hop limit); 0: the system's
        async::stop_token stop;                       // the wait ended with ECANCELED
    };

    // An echo's reply: who answered, after how long, with how much, and
    // the TTL its packet arrived with (-1 where the socket does not tell)
    struct ping_reply {
        ip_address from;
        duration rtt;
        size_t bytes = 0;          // the ICMP message's size, head included
        int ttl = -1;
        uint16_t sequence = 0;
    };

    namespace detail {
        inline uint16_t icmp_checksum(const uint8_t* p, size_t n) noexcept {
            uint32_t sum = 0;
            for (size_t i = 0; i + 1 < n; i += 2) {
                sum += uint32_t(p[i]) << 8 | p[i + 1];
            }
            if (n & 1) {
                sum += uint32_t(p[n - 1]) << 8;
            }
            while (sum >> 16) {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }
            return uint16_t(~sum);
        }

        // What a datagram of an ICMP socket is to an echo of ours: its
        // reply, an error about it (the original's head inside, RFC 792 /
        // RFC 4443), or another's message
        struct PingSeen {
            enum class kind : uint8_t { other, reply, unreachable, exceeded } kind = kind::other;
            size_t bytes = 0;   // the ICMP message's size
            int ttl = -1;       // from the IPv4 head when the datagram has one
        };

        inline PingSeen ping_parse(const uint8_t* p, size_t n, bool v4, bool raw, uint16_t id, uint16_t seq, const uint8_t* cookie) noexcept {
            PingSeen r;
            if (v4 && n >= 20 && (p[0] >> 4) == 4) {   // the IP head before the ICMP message (a raw socket, macOS's datagram one)
                size_t ihl = size_t(p[0] & 15) * 4;
                if (ihl < 20 || n < ihl) {
                    return r;
                }
                r.ttl = p[8];
                p += ihl;
                n -= ihl;
            }
            if (n < 8) {
                return r;
            }
            uint8_t type = p[0];
            bool echo_reply = v4 ? type == 0 : type == 129;
            if (!echo_reply) {
                bool unreachable = v4 ? type == 3 : type == 1;
                bool exceeded = v4 ? type == 11 : type == 3;
                if (!unreachable && !exceeded) {
                    return r;
                }
                const uint8_t* orig = p + 8;
                size_t on = n - 8;
                if (v4 && on >= 20 && (orig[0] >> 4) == 4) {
                    size_t ihl = size_t(orig[0] & 15) * 4;
                    if (ihl < 20 || on < ihl) {
                        return r;
                    }
                    orig += ihl;
                    on -= ihl;
                } else if (!v4) {
                    if (on < 40) {
                        return r;
                    }
                    orig += 40;
                    on -= 40;
                }
                if (on >= 8 && uint16_t(orig[6] << 8 | orig[7]) == seq) {
                    r.kind = unreachable ? PingSeen::kind::unreachable : PingSeen::kind::exceeded;
                    r.bytes = n;
                }
                return r;
            }
            if (n < 16) {
                return r;
            }
            uint16_t rseq = uint16_t(p[6] << 8 | p[7]);
            uint16_t rid = uint16_t(p[4] << 8 | p[5]);
            bool same = true;
            for (size_t i = 0; i < 8; ++i) {
                same &= p[8 + i] == cookie[i];
            }
            if (rseq != seq || !same || (raw && rid != id)) {
                return r;   // another echo's reply (a datagram socket of Linux has its own identifier)
            }
            r.kind = PingSeen::kind::reply;
            r.bytes = n;
            return r;
        }

        // An ICMP socket of the family: a datagram one, else a raw one;
        // raw tells which (a raw IPv4 socket's datagrams carry the IP head,
        // and only a raw socket keeps the identifier as written)
        inline expected<udp::socket, io::error> icmp_socket(int family, int ttl, bool& raw) noexcept {
            int proto = family == AF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6;
            raw = false;
            int s = ::socket(family, SOCK_DGRAM, proto);
            if (s < 0) {
                int first = errno;
                s = ::socket(family, SOCK_RAW, proto);
                if (s < 0) {
                    // neither: on Linux, the user's group outside net.ipv4.ping_group_range and no CAP_NET_RAW
                    return unexpected(system_error(first == EACCES || first == EPERM ? first : errno, "ping",
                                                   string("no ICMP socket: on Linux, net.ipv4.ping_group_range must hold the user's group, or the program "
                                                          "needs CAP_NET_RAW")));
                }
                raw = true;
            }
            if (!prepare_socket(s)) {
                int e = errno;
                ::close(s);
                return unexpected(system_error(e, "ping", string("socket")));
            }
            if (ttl > 0) {
                if (family == AF_INET) {
                    (void)::setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof ttl);
                } else {
                    (void)::setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof ttl);
                }
            }
            return UdpAccess::make(make_tracked<UdpImpl>(s, family, local_of(s), endpoint()));
        }

        inline async::task<expected<ping_reply, io::error>> ping_one(string host, ping_options o) noexcept {
            auto ip = ip_address::parse(host);
            if (!ip) {
                auto found = co_await dns::async_lookup(host, o.stop);
                if (!found) {
                    co_return unexpected(found.error());
                }
                if (found->empty()) {
                    co_return unexpected(net_error(net::errc::host_not_found, "ping", host));
                }
                ip = (*found)[0];
            }
            bool v4 = ip->is_v4() || ip->is_v4_mapped();
            int family = v4 ? AF_INET : AF_INET6;
            bool raw = false;
            auto u = icmp_socket(family, o.ttl, raw);
            if (!u) {
                co_return unexpected(u.error());
            }
            // the request: type, code, checksum, identifier, sequence, then a cookie and the payload's pattern
            size_t payload = o.size < 8 ? 8 : o.size;
            std::vector<uint8_t> req(8 + payload);
            uint8_t rnd[12];
            crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(rnd), sizeof rnd));
            uint16_t id = uint16_t(rnd[0] << 8 | rnd[1]);
            uint16_t seq = uint16_t(rnd[2] << 8 | rnd[3]);
            req[0] = v4 ? 8 : 128;
            req[4] = uint8_t(id >> 8);
            req[5] = uint8_t(id);
            req[6] = uint8_t(seq >> 8);
            req[7] = uint8_t(seq);
            for (size_t i = 0; i < 8; ++i) {
                req[8 + i] = rnd[4 + i];   // the cookie: a reply of another process's echo is not ours
            }
            for (size_t i = 16; i < req.size(); ++i) {
                req[i] = uint8_t(i);
            }
            if (v4) {
                uint16_t c = icmp_checksum(req.data(), req.size());
                req[2] = uint8_t(c >> 8);
                req[3] = uint8_t(c);
            }
            endpoint to(*ip, 0);
            if (o.timeout > duration::zero()) {
                u->set_read_deadline(sgcl::clock::now() + o.timeout);
            }
            // a stop closes the socket, which ends the wait
            tracked_ptr<async::promise<bool>> done = make_tracked<async::promise<bool>>();
            std::atomic<bool> stopped = {false};
            bool watching = o.stop.stop_possible();
            async::task<> closer;
            if (watching) {
                closer = async::spawn([](udp::socket u, async::stop_token stop, tracked_ptr<async::promise<bool>> done, std::atomic<bool>* stopped) -> async::task<> {
                    bool now = false;
                    co_await async::select(stop.on_stop([&] { now = true; }), done->on_done([] {}));
                    if (now) {
                        stopped->store(true);
                        (void)u.close();
                    }
                }(*u, o.stop, done, &stopped));
            }
            auto start = sgcl::clock::now();
            auto sent = co_await u->async_send_to(slice<const byte>(reinterpret_cast<const byte*>(req.data()), req.size()), to);
            expected<ping_reply, io::error> out = unexpected(io::error(io::errc::closed, "ping", host));
            if (!sent) {
                out = unexpected(sent.error());
            } else {
                std::vector<uint8_t> in(65536);   // a datagram whole; reactor I/O, the frame keeps the buffer
                for (;;) {
                    auto d = co_await u->async_receive_from(slice<byte>(reinterpret_cast<byte*>(in.data()), in.size()));
                    auto now = sgcl::clock::now();
                    if (!d) {
                        out = stopped.load() ? unexpected(io::error(error_code(ECANCELED, std::system_category()), "ping", host)) : unexpected(d.error());
                        break;
                    }
                    PingSeen seen = ping_parse(in.data(), d->size, v4, raw, id, seq, rnd + 4);
                    if (seen.kind == PingSeen::kind::other) {
                        continue;
                    }
                    if (seen.kind != PingSeen::kind::reply) {
                        bool unreachable = seen.kind == PingSeen::kind::unreachable;
                        out = unexpected(io::error(error_code(unreachable ? EHOSTUNREACH : ETIMEDOUT, std::system_category()), "ping",
                                                   string::concat(unreachable ? string("unreachable from ") : string("time exceeded at "), d->from.address().to_string())));
                        break;
                    }
                    size_t n = seen.bytes;
                    int ttl = seen.ttl;
                    uint16_t rseq = seq;
                    ping_reply r;
                    r.from = d->from.address();
                    r.rtt = now - start;
                    r.bytes = n;
                    r.ttl = ttl;
                    r.sequence = rseq;
                    out = r;
                    break;
                }
            }
            (void)u->close();
            if (watching) {
                done->set_value(true);
                co_await closer;
            }
            co_return out;
        }
    }

    // An ICMP echo to the host (a name or an address, IPv4 or IPv6) and its
    // reply: who answered and after how long. ETIMEDOUT past the timeout,
    // EHOSTUNREACH for a host or a network an ICMP error says cannot be
    // reached, ETIMEDOUT with "time exceeded" for a TTL that ran out.
    //
    //     auto r = net::ping("example.com");
    //     println("{} ms", r->rtt.milliseconds());
    // `ping(...)` on this thread, `co_await async_ping(...)` in a task
    inline expected<ping_reply, io::error> ping(const string& host, const ping_options& o = {}) {
        return detail::ping_one(host, o).wait();
    }

    inline async::task<expected<ping_reply, io::error>> async_ping(string host, ping_options o = {}) noexcept {
        return detail::ping_one(std::move(host), std::move(o));
    }
}
