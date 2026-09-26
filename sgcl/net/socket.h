//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"
#include "detail/fd.h"
#include "detail/sockaddr.h"
#include "dns.h"
#include "error.h"
#include "ip.h"
#include "../async/channel.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/function.h"
#include "../core/make_tracked.h"
#include "../core/string.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <mutex>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }
    // The flag of a listener shared with other processes (SO_REUSEPORT):
    // tcp::listen(":8080", net::reuse_port) in each, and the kernel
    // spreads the connections among them
    struct reuse_port_t {
        explicit reuse_port_t() = default;
    };

    inline constexpr reuse_port_t reuse_port{};

    namespace detail {
        // RFC 8305 §5: the Connection Attempt Delay, 250 ms recommended
        inline constexpr std::chrono::milliseconds AttemptDelay{250};

        // One attempt to connect: what the race runs for each address,
        // replaceable in a test
        using DialOne = function<async::task<expected<connection, io::error>>(endpoint, async::stop_token)>;

        // "host:port" with a numeric port; the host may be empty (":80")
        struct Target {
            string host;
            uint16_t port = 0;
        };

        inline expected<Target, io::error> parse_target(const string& address, const char* op) {
            auto hp = split_host_port(std::string_view(address.data(), address.size()));
            if (!hp) {
                return fail(net_error(errc::invalid_address, op, address));
            }
            auto port = parse_port(hp->port);
            if (!port) {
                return fail(net_error(errc::invalid_address, op, address));
            }
            return Target{string(hp->host), *port};
        }

        // RFC 8305 §4: the addresses of the two families taken in turns,
        // the first family the first address's, each family in the
        // resolver's order
        inline vector<endpoint> interleave(const vector<ip_address>& addresses, uint16_t port) {
            vector<endpoint> first, second, out;
            for (auto& a : addresses) {
                bool same = a.unmap().is_v4() == addresses.front().unmap().is_v4();
                (same ? first : second).push_back(endpoint(a, port));
            }
            for (size_t i = 0; i < std::max(first.size(), second.size()); ++i) {
                if (i < first.size()) {
                    out.push_back(first[i]);
                }
                if (i < second.size()) {
                    out.push_back(second[i]);
                }
            }
            return out;
        }

        inline int family_of(const endpoint& e) noexcept {
            return e.address().unmap().is_v4() ? AF_INET : AF_INET6;
        }

        // A socket of the family, ready for the module: non-blocking,
        // close-on-exec, no SIGPIPE; -1 and errno on failure
        inline int open_socket(int family, int type) noexcept {
            int s = ::socket(family, type, 0);
            if (s < 0) {
                return -1;
            }
            if (!prepare_socket(s)) {
                int e = errno;
                ::close(s);
                errno = e;
                return -1;
            }
            return s;
        }

        // The first step of a TCP connect: the socket, its connection
        // object (which owns the descriptor from here on) and connect();
        // `pending` when the kernel answered EINPROGRESS
        struct Connecting {
            tracked_ptr<SocketConn> conn;
            bool pending = false;
        };

        inline expected<Connecting, io::error> start_connect(const endpoint& to, const char* op) {
            int family = family_of(to);
            SockAddr sa;
            if (!to_sockaddr(to, family, sa)) {
                return fail(net_error(errc::invalid_address, op, to.to_string()));
            }
            int s = open_socket(family, SOCK_STREAM);
            if (s < 0) {
                return fail(system_error(errno, op, to.to_string()));
            }
            Connecting c;
            c.conn = make_tracked<SocketConn>(s, true, endpoint(), to, string());
            if (::connect(s, sa.get(), sa.size) != 0) {   // EINTR: the connect goes on in the kernel, as after EINPROGRESS
                int e = errno;
                if (e != EINPROGRESS && e != EINTR) {
                    (void)c.conn->close();
                    return fail(system_error(e, op, to.to_string()));
                }
                c.pending = true;
            }
            return c;
        }

        inline expected<connection, io::error> finish_connect(Connecting& c, int e, const char* op, const endpoint& to) {
            if (e != 0) {
                (void)c.conn->close();
                return fail(system_error(e, op, to.to_string()));
            }
            c.conn->set_local(local_of(c.conn->fd()));
            tune_tcp(c.conn->fd());
            return connection(tracked_ptr<ConnImpl>(c.conn));
        }

        // One TCP connection to the endpoint, on this thread
        inline expected<connection, io::error> dial_tcp(const endpoint& to) {
            auto c = start_connect(to, "dial tcp");
            if (!c) {
                return fail(c);
            }
            return finish_connect(*c, c->pending ? c->conn->connected() : 0, "dial tcp", to);
        }

        // The same from a task; the stop ends it with ECANCELED and
        // closes the socket
        inline async::task<expected<connection, io::error>> _co_dial_tcp(endpoint to, async::stop_token stop)  {
            auto c = start_connect(to, "dial tcp");
            if (!c) {
                co_return fail(c);
            }
            int e = c->pending ? co_await c->conn->_co_connected(stop) : 0;
            co_return finish_connect(*c, e, "dial tcp", to);
        }

        // The race of RFC 8305 §5: an attempt started for the first
        // address, the next one `delay` later or as soon as one fails,
        // the first to connect the winner, the others stopped (their
        // token, a child of the caller's) and a connection that comes
        // after the winner's closed. The error of the earliest attempt
        // when every one fails, as Go reports; ECANCELED for the
        // caller's stop, ETIMEDOUT for the deadline.
        struct RaceAttempt {
            size_t index = 0;
            expected<connection, io::error> result;
        };

        struct Race {
            explicit Race(size_t n)
            : results(n) {
            }

            async::channel<RaceAttempt> results;   // room for every attempt: a send never waits
            std::mutex m;                   // `over` and the sends: none after the winner's drain
            bool over = false;
        };

        inline async::task<void> race_attempt(tracked_ptr<Race> race, size_t index, endpoint to, async::stop_token stop, DialOne dial) {
            auto r = co_await dial(to, stop);
            connection late = r ? *r : connection();
            {
                std::lock_guard lock(race->m);
                if (!race->over) {
                    race->results.try_send(RaceAttempt{index, std::move(r)});
                    co_return;
                }
            }
            if (late) {
                (void)late.close();   // the race was won by another
            }
        }

        // The race over: the attempts still running stopped, the
        // connections that came meanwhile closed
        inline void end_race(Race& race, async::stop_source& attempts) {
            attempts.request_stop();
            {
                std::lock_guard lock(race.m);
                race.over = true;
            }
            while (auto a = race.results.try_receive()) {
                if (a->result) {
                    (void)a->result->close();
                }
            }
        }

        inline async::task<expected<connection, io::error>> dial_race(vector<endpoint> targets, async::stop_token stop, time_point deadline, std::chrono::nanoseconds delay, DialOne dial, string what) {
            deadline = no_deadline_at_max(deadline);
            size_t n = targets.size();
            if (n == 0) {
                co_return fail(net_error(errc::no_suitable_address, "dial tcp", what));
            }
            if (stop.stop_requested()) {
                co_return fail(system_error(ECANCELED, "dial tcp", what));
            }
            if (n == 1 && !stop.stop_possible() && deadline == time_point()) {
                co_return co_await dial(targets[0], stop);   // nothing to race
            }
            tracked_ptr<Race> race = make_tracked<Race>(n);
            async::stop_source attempts(stop);   // a child of the caller's token: its stop reaches every attempt
            async::stop_token token = attempts.token();
            size_t started = 0, failed = 0, first_failed = n;
            optional<io::error> first_error;
            time_point last_start;
            auto start_next = [&] {
                async::go(race_attempt(race, started, targets[started], token, dial));
                last_start = sgcl::clock::now();
                ++started;
            };
            start_next();
            for (;;) {
                bool more = started < n;
                time_point next = more ? last_start + delay : time_point::max();
                time_point wake = deadline == time_point() ? next : std::min(next, deadline);
                optional<RaceAttempt> got;
                bool stopped = false;
                if (wake == time_point::max()) {
                    co_await sgcl::async::select(race->results.on_receive([&](optional<RaceAttempt> a) { got = std::move(a); }),
                                                token.on_stop([&] { stopped = true; }));
                } else {
                    co_await sgcl::async::select(race->results.on_receive([&](optional<RaceAttempt> a) { got = std::move(a); }),
                                                token.on_stop([&] { stopped = true; }),
                                                sgcl::async::timeout(wake, [] {}));
                }
                if (stopped || token.stop_requested()) {   // the caller's stop wins over whatever came with it
                    if (got && got->result) {
                        (void)got->result->close();
                    }
                    end_race(*race, attempts);
                    co_return fail(system_error(ECANCELED, "dial tcp", what));
                }
                if (got) {
                    if (got->result) {
                        connection winner = *got->result;
                        end_race(*race, attempts);
                        co_return winner;
                    }
                    ++failed;
                    if (got->index < first_failed) {
                        first_failed = got->index;
                        first_error = got->result.error();
                    }
                    if (failed == n) {
                        end_race(*race, attempts);
                        co_return fail(*first_error);
                    }
                    if (started < n) {
                        start_next();   // a failure starts the next at once
                    }
                    continue;
                }
                auto now = sgcl::clock::now();
                if (deadline != time_point() && now >= deadline) {
                    end_race(*race, attempts);
                    co_return fail(system_error(ETIMEDOUT, "dial tcp", what));
                }
                if (started < n && now >= next) {
                    start_next();
                }
            }
        }

        // "host:port" dialed: the name looked up (the stop and the
        // deadline apply to the lookup too), the addresses raced; an
        // empty host is this machine
        inline async::task<expected<connection, io::error>> dial(string address, async::stop_token stop, time_point deadline) {
            auto t = parse_target(address, "dial tcp");
            if (!t) {
                co_return fail(t);
            }
            vector<ip_address> addresses;
            if (t->host.empty()) {
                addresses.push_back(ip_address::loopback_v4());
                addresses.push_back(ip_address::loopback_v6());
            } else {
                auto found = co_await lookup_until(t->host, stop, deadline);
                if (!found) {
                    co_return fail(found);
                }
                addresses = std::move(*found);
            }
            co_return co_await dial_race(interleave(addresses, t->port), stop, deadline, AttemptDelay, DialOne(_co_dial_tcp), address);
        }

        // The address a listener or a bound socket takes: an empty host
        // is every address of both families (the IPv6 wildcard with
        // IPV6_V6ONLY off: dual stack, as Go); a name, its first IPv4
        // address, else its first
        inline expected<endpoint, io::error> pick_local(const Target& t, const vector<ip_address>& found) {
            if (t.host.empty()) {
                return endpoint(ip_address::any_v6(), t.port);
            }
            for (auto& a : found) {
                if (a.is_v4()) {
                    return endpoint(a, t.port);
                }
            }
            return endpoint(found.front(), t.port);
        }

        // A socket bound to the endpoint (and listening, for a stream);
        // the IPv6 wildcard falls back to IPv4 on a system without IPv6
        inline expected<int, io::error> bind_socket(const endpoint& at, int type, bool reuse_port, const char* op, const string& what) {
            int family = family_of(at);
            endpoint e = at;
            int s = open_socket(family, type);
            if (s < 0 && family == AF_INET6 && errno == EAFNOSUPPORT && at.address() == ip_address::any_v6()) {
                family = AF_INET;
                e = endpoint(ip_address::any_v4(), at.port());
                s = open_socket(family, type);
            }
            if (s < 0) {
                return fail(system_error(errno, op, what));
            }
            int one = 1, zero = 0;
            if (type == SOCK_STREAM) {
                ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            }
            if (reuse_port) {
                ::setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
            }
            if (family == AF_INET6) {
                ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
            }
            SockAddr sa;
            if (!to_sockaddr(e, family, sa)) {
                ::close(s);
                return fail(net_error(errc::invalid_address, op, what));
            }
            if (::bind(s, sa.get(), sa.size) != 0 || (type == SOCK_STREAM && ::listen(s, SOMAXCONN) != 0)) {
                int err = errno;
                ::close(s);
                return fail(system_error(err, op, what));
            }
            return s;
        }

        inline expected<listener, io::error> listen_tcp(const string& address, const vector<ip_address>& found, const Target& t, bool reuse_port) {
            auto at = pick_local(t, found);
            if (!at) {
                return fail(at);
            }
            auto s = bind_socket(*at, SOCK_STREAM, reuse_port, "listen tcp", address);
            if (!s) {
                return fail(s);
            }
            return listener(make_tracked<ListenerImpl>(*s, true, local_of(*s), string(), false));
        }

        inline expected<UdpSocket, io::error> bind_udp(const string& address, const vector<ip_address>& found, const Target& t) {
            auto at = pick_local(t, found);
            if (!at) {
                return fail(at);
            }
            auto s = bind_socket(*at, SOCK_DGRAM, false, "bind udp", address);
            if (!s) {
                return fail(s);
            }
            SockAddr self;
            ::getsockname(*s, self.get(), &self.size);
            return UdpSocket(make_tracked<UdpImpl>(*s, self.family(), from_sockaddr(self.get()), endpoint()));
        }

        inline expected<UdpSocket, io::error> connect_udp(const string& address, const vector<ip_address>& found, uint16_t port) {
            endpoint to(found.front(), port);
            int family = family_of(to);
            SockAddr sa;
            if (!to_sockaddr(to, family, sa)) {
                return fail(net_error(errc::invalid_address, "dial udp", address));
            }
            int s = open_socket(family, SOCK_DGRAM);
            if (s < 0) {
                return fail(system_error(errno, "dial udp", address));
            }
            if (::connect(s, sa.get(), sa.size) != 0) {
                int e = errno;
                ::close(s);
                return fail(system_error(e, "dial udp", address));
            }
            return UdpSocket(make_tracked<UdpImpl>(s, family, local_of(s), to));
        }

        // The addresses of a target's host for a listener or a UDP
        // socket: none needed for an empty host, the host itself when
        // it is numeric, the resolver's otherwise
        inline expected<vector<ip_address>, io::error> local_addresses(const Target& t) {
            if (t.host.empty()) {
                return vector<ip_address>();
            }
            return dns::lookup(t.host);
        }

        inline async::task<expected<vector<ip_address>, io::error>> _co_local_addresses(Target t)  {
            if (t.host.empty()) {
                co_return vector<ip_address>();
            }
            co_return co_await dns::async_lookup(t.host);
        }

        // A unix socket at the path: dialed, or bound and listening
        inline expected<listener, io::error> listen_unix(const string& path) {
            SockAddr sa;
            if (!unix_sockaddr(path, sa)) {
                return fail(net_error(errc::invalid_address, "listen unix", path));
            }
            int s = open_socket(AF_UNIX, SOCK_STREAM);
            if (s < 0) {
                return fail(system_error(errno, "listen unix", path));
            }
            if (::bind(s, sa.get(), sa.size) != 0 || ::listen(s, SOMAXCONN) != 0) {
                int e = errno;
                ::close(s);
                return fail(system_error(e, "listen unix", path));
            }
            return listener(make_tracked<ListenerImpl>(s, false, endpoint(), path, true));
        }

        inline expected<Connecting, io::error> start_connect_unix(const string& path) {
            SockAddr sa;
            if (!unix_sockaddr(path, sa)) {
                return fail(net_error(errc::invalid_address, "dial unix", path));
            }
            int s = open_socket(AF_UNIX, SOCK_STREAM);
            if (s < 0) {
                return fail(system_error(errno, "dial unix", path));
            }
            Connecting c;
            c.conn = make_tracked<SocketConn>(s, false, endpoint(), endpoint(), path);
            if (::connect(s, sa.get(), sa.size) != 0) {
                int e = errno;
                if (e != EINPROGRESS && e != EINTR) {
                    (void)c.conn->close();
                    return fail(system_error(e, "dial unix", path));
                }
                c.pending = true;
            }
            return c;
        }

        inline expected<connection, io::error> finish_connect_unix(Connecting& c, int e, const string& path) {
            if (e != 0) {
                (void)c.conn->close();
                return fail(system_error(e, "dial unix", path));
            }
            return connection(tracked_ptr<ConnImpl>(c.conn));
        }
    }

    // TCP: connections out (connect) and in (listen). An address is
    // "host:port": a name ("example.com:80"), an IPv4 address
    // ("10.0.0.1:80"), an IPv6 address in brackets ("[::1]:80"), or no host
    // (":80": this machine to connect, every address to listen). The port
    // is a number; service names are not looked up.
    //
    // connect looks the name up (dns) and races the addresses as RFC 8305
    // has it (happy eyeballs): the families in turns, the next attempt 250
    // ms after the last or at once when it fails, the first connection the
    // one returned, the others stopped. A timeout bounds the whole of it
    // (ETIMEDOUT), a stop token ends it (ECANCELED). A connection has
    // Nagle's algorithm off and keep-alive probes after fifteen seconds,
    // as in Go. The blocking forms of a connect by name run the race on the
    // scheduler and wait for it: from a thread, as task::join is (a task
    // co_awaits async_connect); connect(endpoint) is the one blocking call
    // with no race, and serves anywhere.
    //
    // listen binds the address with SO_REUSEADDR, listens with the
    // system's backlog, and for no host takes the IPv6 wildcard with
    // IPV6_V6ONLY off, one socket for both families, as Go; the
    // reuse_port flag lets other processes listen on the same port.
    struct tcp {
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<net::connection, io::error> connect(const string& address) {
            return _block_connect(address);
        }

        static async::task<expected<net::connection, io::error>> async_connect(const string& address) {
            return _co_connect(address);
        }


        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<net::connection, io::error> connect(const string& address, async::stop_token stop) {
            return _block_connect(address, stop);
        }

        static async::task<expected<net::connection, io::error>> async_connect(const string& address, async::stop_token stop) {
            return _co_connect(address, stop);
        }

        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<net::connection, io::error> connect(const string& address, duration timeout) {
            return _block_connect(address, timeout);
        }

        static async::task<expected<net::connection, io::error>> async_connect(const string& address, duration timeout) {
            return _co_connect(address, timeout);
        }

        // To the endpoint as it is: no lookup, no race
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<net::connection, io::error> connect(const net::endpoint& to) {
            return _block_connect(to);
        }

        static async::task<expected<net::connection, io::error>> async_connect(const net::endpoint& to) {
            return _co_connect(to);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        static expected<net::listener, io::error> listen(const string& address) {
            return _block_listen(address);
        }

        static async::task<expected<net::listener, io::error>> async_listen(const string& address) {
            return _co_listen(address);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        static expected<net::listener, io::error> listen(const string& address, net::reuse_port_t flag) {
            return _block_listen(address, flag);
        }

        static async::task<expected<net::listener, io::error>> async_listen(const string& address, net::reuse_port_t flag) {
            return _co_listen(address, flag);
        }

    private:
        static expected<net::listener, io::error> _listen(const string& address, bool reuse_port) {
            auto t = net::detail::parse_target(address, "listen tcp");
            if (!t) {
                return net::detail::fail(t);
            }
            auto found = net::detail::local_addresses(*t);
            if (!found) {
                return net::detail::fail(found);
            }
            return net::detail::listen_tcp(address, *found, *t, reuse_port);
        }

        static async::task<expected<net::listener, io::error>> _async_listen(string address, bool reuse_port) {
            auto t = net::detail::parse_target(address, "listen tcp");
            if (!t) {
                co_return net::detail::fail(t);
            }
            auto found = co_await net::detail::_co_local_addresses(*t);
            if (!found) {
                co_return net::detail::fail(found);
            }
            co_return net::detail::listen_tcp(address, *found, *t, reuse_port);
        }

        // the two halves of the operations above: a thread's and a task's
        static expected<net::connection, io::error> _block_connect(const string& address)  {
            return _co_connect(address).wait();
        }

        static expected<net::connection, io::error> _block_connect(const string& address, async::stop_token stop)  {
            return _co_connect(address, std::move(stop)).wait();
        }

        static expected<net::connection, io::error> _block_connect(const string& address, duration timeout)  {
            return _co_connect(address, timeout).wait();
        }

        static expected<net::connection, io::error> _block_connect(const net::endpoint& to)  {
            return net::detail::dial_tcp(to);
        }

        static async::task<expected<net::connection, io::error>> _co_connect(const string& address)  {
            return net::detail::dial(address, async::stop_token(), time_point());
        }

        static async::task<expected<net::connection, io::error>> _co_connect(const string& address, async::stop_token stop)  {
            return net::detail::dial(address, std::move(stop), time_point());
        }

        static async::task<expected<net::connection, io::error>> _co_connect(const string& address, duration timeout)  {
            return net::detail::dial(address, async::stop_token(), sgcl::clock::now() + timeout);   // saturates: duration::max() is time_point::max(), no deadline
        }

        static async::task<expected<net::connection, io::error>> _co_connect(const net::endpoint& to)  {
            return net::detail::_co_dial_tcp(to, async::stop_token());
        }

        static expected<net::listener, io::error> _block_listen(const string& address)  {
            return _listen(address, false);
        }

        static expected<net::listener, io::error> _block_listen(const string& address, net::reuse_port_t)  {
            return _listen(address, true);
        }

        static async::task<expected<net::listener, io::error>> _co_listen(const string& address)  {
            return _async_listen(address, false);
        }

        static async::task<expected<net::listener, io::error>> _co_listen(const string& address, net::reuse_port_t)  {
            return _async_listen(address, true);
        }
    };

    // UDP: bind takes an address to receive on (":5353" every address of
    // both families, as tcp::listen); connect fixes the peer, so that send
    // and receive need no address and datagrams from anyone else are
    // dropped by the system. Neither waits on the network: the async forms
    // differ only in the lookup of a name.
    struct udp {
        // The socket bind and connect give, and what one receive gives
        using socket = detail::UdpSocket;
        using datagram = detail::Datagram;

        // `bind(...)` on this thread, `co_await async_bind(...)` in a task
        static expected<udp::socket, io::error> bind(const string& address) {
            return _block_bind(address);
        }

        static async::task<expected<udp::socket, io::error>> async_bind(const string& address) {
            return _co_bind(address);
        }

        // `udp::connect(...)` on this thread, `co_await udp::async_connect(...)` in a task
        static expected<udp::socket, io::error> connect(const string& address) {
            return _block_connect(address);
        }

        static async::task<expected<udp::socket, io::error>> async_connect(const string& address) {
            return _co_connect(address);
        }

    private:
        static async::task<expected<udp::socket, io::error>> _async_bind(string address) {
            auto t = net::detail::parse_target(address, "bind udp");
            if (!t) {
                co_return net::detail::fail(t);
            }
            auto found = co_await net::detail::_co_local_addresses(*t);
            if (!found) {
                co_return net::detail::fail(found);
            }
            co_return net::detail::bind_udp(address, *found, *t);
        }

        static async::task<expected<udp::socket, io::error>> _async_connect(string address) {
            auto t = net::detail::parse_target(address, "dial udp");
            if (!t) {
                co_return net::detail::fail(t);
            }
            expected<vector<ip_address>, io::error> found = vector<ip_address>{ip_address::loopback_v4()};
            if (!t->host.empty()) {
                found = co_await dns::async_lookup(t->host);
            }
            if (!found) {
                co_return net::detail::fail(found);
            }
            co_return net::detail::connect_udp(address, *found, t->port);
        }

        // the two halves of the operations above: a thread's and a task's
        static expected<udp::socket, io::error> _block_bind(const string& address)  {
            auto t = net::detail::parse_target(address, "bind udp");
            if (!t) {
                return net::detail::fail(t);
            }
            auto found = net::detail::local_addresses(*t);
            if (!found) {
                return net::detail::fail(found);
            }
            return net::detail::bind_udp(address, *found, *t);
        }

        static async::task<expected<udp::socket, io::error>> _co_bind(const string& address)  {
            return _async_bind(address);
        }

        static expected<udp::socket, io::error> _block_connect(const string& address) {
            auto t = net::detail::parse_target(address, "dial udp");
            if (!t) {
                return net::detail::fail(t);
            }
            auto found = t->host.empty() ? expected<vector<ip_address>, io::error>(vector<ip_address>{ip_address::loopback_v4()}) : dns::lookup(t->host);
            if (!found) {
                return net::detail::fail(found);
            }
            return net::detail::connect_udp(address, *found, t->port);
        }

        static async::task<expected<udp::socket, io::error>> _co_connect(const string& address)  {
            return _async_connect(address);
        }
    };

    // Stream sockets in the file system (AF_UNIX): connect to the path a
    // listener made; listen creates the socket's file (an error when
    // something is at the path already, as in Go), and the listener's
    // close() removes it. A path is at most 103 bytes on macOS (107 on
    // Linux). `unix_domain` and not `unix`: `unix` is a macro in the GNU
    // modes of GCC and Clang on Linux.
    struct unix_domain {
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<net::connection, io::error> connect(const string& path) {
            return _block_connect(path);
        }

        static async::task<expected<net::connection, io::error>> async_connect(const string& path) {
            return _co_connect(path);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        static expected<net::listener, io::error> listen(const string& path) {
            return _block_listen(path);
        }

        static async::task<expected<net::listener, io::error>> async_listen(const string& path) {
            return _co_listen(path);
        }

    private:
        static async::task<expected<net::connection, io::error>> _async_connect(string path) {
            auto c = net::detail::start_connect_unix(path);
            if (!c) {
                co_return net::detail::fail(c);
            }
            int e = c->pending ? co_await c->conn->_co_connected(async::stop_token()) : 0;
            co_return net::detail::finish_connect_unix(*c, e, path);
        }

        static async::task<expected<net::listener, io::error>> _async_listen(string path) {
            co_return net::detail::listen_unix(path);
        }

        // the two halves of the operations above: a thread's and a task's
        static expected<net::connection, io::error> _block_connect(const string& path)  {
            auto c = net::detail::start_connect_unix(path);
            if (!c) {
                return net::detail::fail(c);
            }
            return net::detail::finish_connect_unix(*c, c->pending ? c->conn->connected() : 0, path);
        }

        static async::task<expected<net::connection, io::error>> _co_connect(const string& path)  {
            return _async_connect(path);
        }

        static expected<net::listener, io::error> _block_listen(const string& path)  {
            return net::detail::listen_unix(path);
        }

        // Nothing to wait for: the task form of listen, for symmetry
        static async::task<expected<net::listener, io::error>> _co_listen(const string& path)  {
            return _async_listen(path);
        }
    };
}
