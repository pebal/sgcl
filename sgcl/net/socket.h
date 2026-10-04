//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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

        SGCL_INLINE_HOT expected<Target, io::error> parse_target(const string& address, const char* op) noexcept {
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
        inline vector<endpoint> interleave(const vector<ip_address>& addresses, uint16_t port) noexcept {
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

        SGCL_INLINE_HOT int family_of(const endpoint& e) noexcept {
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

        inline expected<Connecting, io::error> start_connect(const endpoint& to, const char* op) noexcept {
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

        inline expected<connection, io::error> finish_connect(Connecting& c, int e, const char* op, const endpoint& to) noexcept {
            if (e != 0) {
                (void)c.conn->close();
                return fail(system_error(e, op, to.to_string()));
            }
            c.conn->set_local(local_of(c.conn->fd()));
            if (to.address().has_zone() || to.address().is_v4_mapped()) {   // the peer as the system names it, as an accepted connection has it: the zone by its name ("%1" is "%lo0"), the IPv4 address unmapped
                if (auto peer = remote_of(c.conn->fd()); peer.is_valid()) {
                    c.conn->set_remote(peer);
                }
            }
            tune_tcp(c.conn->fd());
            return ConnectionAccess::make(tracked_ptr<ConnImpl>(c.conn));
        }

        // One TCP connection to the endpoint, on this thread
        SGCL_INLINE_HOT expected<connection, io::error> dial_tcp(const endpoint& to) {
            auto c = start_connect(to, "dial tcp");
            if (!c) {
                return fail(c);
            }
            return finish_connect(*c, c->pending ? c->conn->connected() : 0, "dial tcp", to);
        }

        // The same from a task; the stop ends it with ECANCELED and
        // closes the socket
        inline async::task<expected<connection, io::error>> _co_dial_tcp(endpoint to, async::stop_token stop) noexcept {
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
            SGCL_INLINE_HOT explicit Race(size_t n) noexcept
            : results(n) {
            }

            async::detail::ChannelState<RaceAttempt> results;   // room for every attempt: a send never waits
            std::mutex m;                   // `over` and the sends: none after the winner's drain
            bool over = false;
        };

        inline async::task<void> race_attempt(tracked_ptr<Race> race, size_t index, endpoint to, async::stop_token stop, DialOne dial) noexcept {
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

        // One attempt's failure as a part of the race's error: where it went
        // and why, without the operation ("[::1]:80: Connection refused")
        inline std::string attempt_failure(const io::error& e) noexcept {
            std::string s(e.path().view());
            if (!s.empty()) {
                s += ": ";
            }
            s += e.code().message();
            return s;
        }

        // The error of a race every attempt of which failed: the earliest
        // attempt's code (as Go reports it), and every attempt in the
        // text, in the order they were made, so that a name that gave one
        // address is told from one whose every address failed:
        //   dial tcp localhost:80 (every address failed: [::1]:80: Connection refused; 127.0.0.1:80: Connection refused): Connection refused
        //   dial tcp localhost:80 (its only address: [::1]:80: Connection refused): Connection refused
        inline io::error every_attempt_failed(const vector<optional<io::error>>& failures, const string& what) noexcept {
            std::string path(what.view());
            path += failures.size() == 1 ? " (its only address: " : " (every address failed: ";
            error_code code;
            bool first = true;
            for (auto& f : failures) {
                if (!f) {
                    continue;
                }
                if (first) {
                    code = f->code();
                } else {
                    path += "; ";
                }
                path += attempt_failure(*f);
                first = false;
            }
            path += ')';
            return io::error(code, string("dial tcp"), string(path));
        }

        inline async::task<expected<connection, io::error>> dial_race(vector<endpoint> targets, async::stop_token stop, time_point deadline, std::chrono::nanoseconds delay, DialOne dial, string what) noexcept {
            deadline = no_deadline_at_max(deadline);
            size_t n = targets.size();
            if (n == 0) {
                co_return fail(net_error(errc::no_suitable_address, "dial tcp", what));
            }
            if (stop.stop_requested()) {
                co_return fail(system_error(ECANCELED, "dial tcp", what));
            }
            if (deadline != time_point() && sgcl::clock::now() >= deadline) {   // passed before the start: no attempt, as no read starts past its deadline
                co_return fail(system_error(ETIMEDOUT, "dial tcp", what));
            }
            if (n == 1 && !stop.stop_possible() && deadline == time_point()) {
                auto only = co_await dial(targets[0], stop);   // nothing to race
                if (!only) {
                    vector<optional<io::error>> failures;
                    failures.push_back(only.error());
                    co_return fail(every_attempt_failed(failures, what));
                }
                co_return only;
            }
            tracked_ptr<Race> race = make_tracked<Race>(n);
            async::stop_source attempts(stop);   // a child of the caller's token: its stop reaches every attempt
            async::stop_token token = attempts.token();
            size_t started = 0, failed = 0;
            vector<optional<io::error>> failures;   // each attempt's, by its index: the race's error when every one fails
            failures.resize(n);
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
                    failures[got->index] = got->result.error();
                    if (failed == n) {
                        end_race(*race, attempts);
                        co_return fail(every_attempt_failed(failures, what));
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
        inline async::task<expected<connection, io::error>> dial(string address, async::stop_token stop, time_point deadline) noexcept {
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
        inline expected<endpoint, io::error> pick_local(const Target& t, const vector<ip_address>& found) noexcept {
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
        inline expected<int, io::error> bind_socket(const endpoint& at, int type, bool reuse_port, const char* op, const string& what) noexcept {
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

        SGCL_INLINE_HOT expected<listener, io::error> listen_tcp(const string& address, const vector<ip_address>& found, const Target& t, bool reuse_port) noexcept {
            auto at = pick_local(t, found);
            if (!at) {
                return fail(at);
            }
            auto s = bind_socket(*at, SOCK_STREAM, reuse_port, "listen tcp", address);
            if (!s) {
                return fail(s);
            }
            return ListenerAccess::make(make_tracked<ListenerImpl>(*s, true, local_of(*s), string(), false));
        }

        // The addresses of a target's host for a listener or a UDP
        // socket: none needed for an empty host, the host itself when
        // it is numeric, the resolver's otherwise
        SGCL_INLINE_HOT expected<vector<ip_address>, io::error> local_addresses(const Target& t) noexcept {
            if (t.host.empty()) {
                return vector<ip_address>();
            }
            return dns::lookup(t.host);
        }

        inline async::task<expected<vector<ip_address>, io::error>> _co_local_addresses(Target t) noexcept {
            if (t.host.empty()) {
                co_return vector<ip_address>();
            }
            co_return co_await dns::async_lookup(t.host);
        }

        // A unix socket at the path: dialed, or bound and listening
        inline expected<listener, io::error> listen_unix(const string& path) noexcept {
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
            return ListenerAccess::make(make_tracked<ListenerImpl>(s, false, endpoint(), path, true));
        }

        inline expected<Connecting, io::error> start_connect_unix(const string& path) noexcept {
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

        SGCL_INLINE_HOT expected<connection, io::error> finish_connect_unix(Connecting& c, int e, const string& path) noexcept {
            if (e != 0) {
                (void)c.conn->close();
                return fail(system_error(e, "dial unix", path));
            }
            return ConnectionAccess::make(tracked_ptr<ConnImpl>(c.conn));
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
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& address) {
            return _block_connect(address);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(const string& address) noexcept {
            return _co_connect(address);
        }


        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& address, async::stop_token stop) {
            return _block_connect(address, stop);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(const string& address, async::stop_token stop) noexcept {
            return _co_connect(address, stop);
        }

        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& address, duration timeout) {
            return _block_connect(address, timeout);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(const string& address, duration timeout) noexcept {
            return _co_connect(address, timeout);
        }

        // To the endpoint as it is: no lookup, no race
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const net::endpoint& to) {
            return _block_connect(to);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(const net::endpoint& to) noexcept {
            return _co_connect(to);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        SGCL_INLINE_HOT static expected<net::listener, io::error> listen(const string& address) noexcept {
            return _block_listen(address);
        }

        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> async_listen(const string& address) noexcept {
            return _co_listen(address);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        SGCL_INLINE_HOT static expected<net::listener, io::error> listen(const string& address, net::reuse_port_t flag) noexcept {
            return _block_listen(address, flag);
        }

        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> async_listen(const string& address, net::reuse_port_t flag) noexcept {
            return _co_listen(address, flag);
        }

    private:
        SGCL_INLINE_HOT static expected<net::listener, io::error> _listen(const string& address, bool reuse_port) noexcept {
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

        static async::task<expected<net::listener, io::error>> _async_listen(string address, bool reuse_port) noexcept {
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
        SGCL_INLINE_HOT static expected<net::connection, io::error> _block_connect(const string& address)  {
            return _co_connect(address).wait();
        }

        SGCL_INLINE_HOT static expected<net::connection, io::error> _block_connect(const string& address, async::stop_token stop)  {
            return _co_connect(address, std::move(stop)).wait();
        }

        SGCL_INLINE_HOT static expected<net::connection, io::error> _block_connect(const string& address, duration timeout)  {
            return _co_connect(address, timeout).wait();
        }

        SGCL_INLINE_HOT static expected<net::connection, io::error> _block_connect(const net::endpoint& to)  {
            return net::detail::dial_tcp(to);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_connect(const string& address) noexcept {
            return net::detail::dial(address, async::stop_token(), time_point());
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_connect(const string& address, async::stop_token stop) noexcept {
            return net::detail::dial(address, std::move(stop), time_point());
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_connect(const string& address, duration timeout) noexcept {
            return net::detail::dial(address, async::stop_token(), sgcl::clock::now() + timeout);   // saturates: duration::max() is time_point::max(), no deadline
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_connect(const net::endpoint& to) noexcept {
            return net::detail::_co_dial_tcp(to, async::stop_token());
        }

        SGCL_INLINE_HOT static expected<net::listener, io::error> _block_listen(const string& address) noexcept {
            return _listen(address, false);
        }

        SGCL_INLINE_HOT static expected<net::listener, io::error> _block_listen(const string& address, net::reuse_port_t) noexcept {
            return _listen(address, true);
        }

        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> _co_listen(const string& address) noexcept {
            return _async_listen(address, false);
        }

        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> _co_listen(const string& address, net::reuse_port_t) noexcept {
            return _async_listen(address, true);
        }
    };

    namespace detail {
        class UdpImpl;

        // The module's way to make a udp::socket over its object, never a
        // public constructor
        struct UdpAccess;
    }

    // UDP: bind takes an address to receive on (":5353" every address of
    // both families, as tcp::listen); connect fixes the peer, so that send
    // and receive need no address and datagrams from anyone else are
    // dropped by the system. Neither waits on the network: the async forms
    // differ only in the lookup of a name.
    struct udp {
        // A datagram received: how many bytes the buffer got, from whom,
        // and whether the datagram was longer than the buffer and cut
        // (MSG_TRUNC)
        struct datagram {
            size_t size = 0;
            endpoint from;
            bool truncated = false;
        };

        // A UDP socket (udp::bind, udp::connect): datagrams to and from
        // any address, or, connected, to and from one. A datagram is sent
        // whole or not at all; one longer than the buffer is cut, and says
        // so. A handle of one word, as connection is: copies are the same
        // socket. (Its members that reach the socket's object are defined
        // below, after the object.)
        class socket {
        public:
            socket() noexcept = default;   // no socket; an operation on it is a contract violation

            // The next datagram into the buffer: its size, its sender, and
            // whether it was cut to fit
            // `receive_from(...)` on this thread, `co_await async_receive_from(...)` in a task
            expected<datagram, io::error> receive_from(const slice<byte>& buffer) const;
            async::task<expected<datagram, io::error>> async_receive_from(const slice<byte>& buffer) const noexcept;

            // `send_to(...)` on this thread, `co_await async_send_to(...)` in a task
            expected<size_t, io::error> send_to(const slice<const byte>& data, const endpoint& to) const;
            async::task<expected<size_t, io::error>> async_send_to(const slice<const byte>& data, const endpoint& to) const noexcept;

            // A socket from udp::connect: to and from its one peer
            // `receive(...)` on this thread, `co_await async_receive(...)` in a task
            expected<size_t, io::error> receive(const slice<byte>& buffer) const;
            async::task<expected<size_t, io::error>> async_receive(const slice<byte>& buffer) const noexcept;

            // `send(...)` on this thread, `co_await async_send(...)` in a task
            expected<size_t, io::error> send(const slice<const byte>& data) const;
            async::task<expected<size_t, io::error>> async_send(const slice<const byte>& data) const noexcept;

            expected<void, io::error> close() const noexcept;
            bool is_closed() const noexcept;
            endpoint local_endpoint() const noexcept;

            // The peer of a connected socket, empty otherwise
            endpoint remote_endpoint() const noexcept;

            void set_deadline(time_point t) const noexcept;
            void set_read_deadline(time_point t) const noexcept;
            void set_write_deadline(time_point t) const noexcept;
            time_point read_deadline() const noexcept;
            time_point write_deadline() const noexcept;

            SGCL_INLINE_HOT explicit operator bool() const noexcept {
                return (bool)_impl;
            }

            SGCL_INLINE_HOT friend bool operator==(const socket& a, const socket& b) noexcept {
                return a._impl == b._impl;
            }

        private:
            friend struct detail::UdpAccess;

            SGCL_INLINE_HOT explicit socket(const tracked_ptr<detail::UdpImpl>& impl) noexcept
            : _impl(impl) {
            }

            // The handle's word, for the atomics (core/detail/handle_word.h)
            friend struct sgcl::detail::HandleWord;

            SGCL_INLINE_HOT socket(sgcl::detail::FromWord, const tracked_ptr<detail::UdpImpl>& w) noexcept
            : _impl(w) {
            }

            SGCL_INLINE_HOT tracked_ptr<detail::UdpImpl>& _handle_word() noexcept {
                return _impl;
            }

            SGCL_INLINE_HOT const tracked_ptr<detail::UdpImpl>& _handle_word() const noexcept {
                return _impl;
            }

            detail::UdpImpl& _get() const noexcept;
            static async::task<expected<size_t, io::error>> _receive_size(tracked_ptr<detail::UdpImpl> impl, slice<byte> buffer) noexcept;

            tracked_ptr<detail::UdpImpl> _impl;
        };

        // `bind(...)` on this thread, `co_await async_bind(...)` in a task
        static expected<udp::socket, io::error> bind(const string& address) noexcept;
        static async::task<expected<udp::socket, io::error>> async_bind(const string& address) noexcept;

        // `udp::connect(...)` on this thread, `co_await udp::async_connect(...)` in a task
        static expected<udp::socket, io::error> connect(const string& address) noexcept;
        static async::task<expected<udp::socket, io::error>> async_connect(const string& address) noexcept;

    private:
        static async::task<expected<udp::socket, io::error>> _async_bind(string address) noexcept;
        static async::task<expected<udp::socket, io::error>> _async_connect(string address) noexcept;
    };

    namespace detail {
        class UdpImpl {
        public:
            SGCL_INLINE_HOT UdpImpl(int fd, int family, endpoint local, endpoint remote) noexcept
            : _d(fd)
            , _local(local)
            , _remote(remote)
            , _family(family) {
            }

            // One datagram: its size in the buffer and its sender
            // `receive(...)` on this thread, `co_await async_receive(...)` in a task
            SGCL_INLINE_HOT expected<udp::datagram, io::error> receive(const slice<byte>& b) {
                return _block_receive(b);
            }

            SGCL_INLINE_HOT async::task<expected<udp::datagram, io::error>> async_receive(const slice<byte>& b) noexcept {
                return _co_receive(b);
            }

            expected<udp::datagram, io::error> _block_receive(const slice<byte>& b)  {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("read", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "read")) {
                        return fail(*e);
                    }
                    udp::datagram d;
                    int e = _recv(b, d);
                    if (e == 0) {
                        return d;
                    }
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "read", describe()));
                    }
                    auto r = _d.wait(Descriptor::Read);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "read", describe()));
                    }
                }
            }

            async::task<expected<udp::datagram, io::error>> _co_receive(slice<byte> b) noexcept {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("read", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "read")) {
                        co_return fail(*e);
                    }
                    udp::datagram d;
                    int e = _recv(b, d);
                    if (e == 0) {
                        co_return d;
                    }
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        co_return fail(system_error(e, "read", describe()));
                    }
                    auto r = co_await _d.async_wait(Descriptor::Read);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "read", describe()));
                    }
                }
            }

            // One datagram to `to`, or to the connected peer when to is empty
            expected<size_t, io::error> send(const slice<const byte>& data, const endpoint& to) {
                SockAddr sa;
                if (to.is_valid() && !to_sockaddr(to, _family, sa)) {
                    return fail(_unreachable(to));
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Write, "write")) {
                        return fail(*e);
                    }
                    _d.prepare(Descriptor::Write);
                    ssize_t n = to.is_valid() ? ::sendto(_d.fd(), data.data(), data.size(), SendFlags, sa.get(), sa.size) : ::send(_d.fd(), data.data(), data.size(), SendFlags);
                    if (n >= 0) {
                        return size_t(n);
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {   // ENOBUFS an error, as in Go: the socket has room, so a wait for writability would spin
                        return fail(system_error(e, "write", describe()));
                    }
                    auto r = _d.wait(Descriptor::Write);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "write", describe()));
                    }
                }
            }

            async::task<expected<size_t, io::error>> _co_send(slice<const byte> data, endpoint to) noexcept {
                SockAddr sa;
                if (to.is_valid() && !to_sockaddr(to, _family, sa)) {
                    co_return fail(_unreachable(to));
                }
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("write", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Write, "write")) {
                        co_return fail(*e);
                    }
                    _d.prepare(Descriptor::Write);
                    ssize_t n = to.is_valid() ? ::sendto(_d.fd(), data.data(), data.size(), SendFlags, sa.get(), sa.size) : ::send(_d.fd(), data.data(), data.size(), SendFlags);
                    if (n >= 0) {
                        co_return size_t(n);
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {   // ENOBUFS an error, as in Go: the socket has room, so a wait for writability would spin
                        co_return fail(system_error(e, "write", describe()));
                    }
                    auto r = co_await _d.async_wait(Descriptor::Write);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "write", describe()));
                    }
                }
            }

            SGCL_INLINE_HOT expected<void, io::error> close() noexcept {
                int e = _d.close();
                if (e) {
                    return fail(system_error(e, "close", describe()));
                }
                return {};
            }

            SGCL_INLINE_HOT bool is_closed() const noexcept {
                return _d.closing();
            }

            SGCL_INLINE_HOT void set_deadline(int dir, time_point t) noexcept {
                _d.set_deadline(dir, t);
            }

            SGCL_INLINE_HOT time_point deadline(int dir) const noexcept {
                return _d.deadline(dir);
            }

            SGCL_INLINE_HOT endpoint local_endpoint() const noexcept {
                return _local;
            }

            SGCL_INLINE_HOT endpoint remote_endpoint() const noexcept {
                return _remote;
            }

            SGCL_INLINE_HOT string describe() const noexcept {
                return _remote.is_valid() ? string("udp ") + _local.to_string() + "->" + _remote.to_string() : string("udp ") + _local.to_string();
            }

        private:
            // recvmsg, for the flag that says the datagram was cut: 0 or
            // errno. An empty buffer reads into a byte of its own: macOS
            // answers an empty one with 0 and leaves the datagram queued, a
            // datagram of nothing that is not there; so the datagram is
            // taken, its size in the buffer 0, truncated when it had bytes
            int _recv(const slice<byte>& b, udp::datagram& d) noexcept {
                SockAddr from;
                byte spare[1];
                iovec iov;
                iov.iov_base = b.empty() ? spare : b.data();
                iov.iov_len = b.empty() ? 1 : b.size();
                msghdr m = {};
                m.msg_name = &from.storage;
                m.msg_namelen = sizeof(from.storage);
                m.msg_iov = &iov;
                m.msg_iovlen = 1;
                _d.prepare(Descriptor::Read);
                ssize_t n = ::recvmsg(_d.fd(), &m, 0);
                if (n < 0) {
                    return errno;
                }
                d.size = b.empty() ? 0 : size_t(n);
                d.truncated = (m.msg_flags & MSG_TRUNC) != 0 || (b.empty() && n > 0);
                d.from = m.msg_namelen ? from_sockaddr(from.get()) : _remote;
                return 0;
            }

            // An address the socket cannot take: EAFNOSUPPORT for an IPv6
            // one on an IPv4 socket, invalid_address for a zone that names
            // no interface (as a connect's)
            io::error _unreachable(const endpoint& to) const noexcept {
                if (_family == AF_INET && !to.address().unmap().is_v4()) {
                    return system_error(EAFNOSUPPORT, "write", to.to_string());
                }
                return net_error(errc::invalid_address, "write", to.to_string());
            }

            SGCL_INLINE_HOT optional<io::error> _check(int dir, const char* op) const noexcept {
                if (_d.closing()) {
                    return closed_error(op, describe());
                }
                if (_d.expired(dir)) {
                    return system_error(ETIMEDOUT, op, describe());
                }
                return nullopt;
            }

            Descriptor _d;
            endpoint _local;
            endpoint _remote;
            int _family;
        };

        struct UdpAccess {
            SGCL_INLINE_HOT static udp::socket make(const tracked_ptr<UdpImpl>& impl) noexcept {
                return udp::socket(impl);
            }
        };

        inline expected<udp::socket, io::error> bind_udp(const string& address, const vector<ip_address>& found, const Target& t) noexcept {
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
            return UdpAccess::make(make_tracked<UdpImpl>(*s, self.family(), from_sockaddr(self.get()), endpoint()));
        }

        inline expected<udp::socket, io::error> connect_udp(const string& address, const vector<ip_address>& found, uint16_t port) noexcept {
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
            return UdpAccess::make(make_tracked<UdpImpl>(s, family, local_of(s), to));
        }
    }

    // --- udp::socket, over its object ---------------------------------------

    SGCL_INLINE_HOT detail::UdpImpl& udp::socket::_get() const noexcept {
        assert(_impl && "an empty net::udp::socket");
        return *_impl;
    }

    SGCL_INLINE_HOT expected<udp::datagram, io::error> udp::socket::receive_from(const slice<byte>& buffer) const {
        return _get()._block_receive(buffer);
    }

    SGCL_INLINE_HOT async::task<expected<udp::datagram, io::error>> udp::socket::async_receive_from(const slice<byte>& buffer) const noexcept {
        return _get()._co_receive(buffer);
    }

    SGCL_INLINE_HOT expected<size_t, io::error> udp::socket::send_to(const slice<const byte>& data, const endpoint& to) const {
        return _get().send(data, to);
    }

    SGCL_INLINE_HOT async::task<expected<size_t, io::error>> udp::socket::async_send_to(const slice<const byte>& data, const endpoint& to) const noexcept {
        return _get()._co_send(data, to);
    }

    SGCL_INLINE_HOT expected<size_t, io::error> udp::socket::receive(const slice<byte>& buffer) const {
        auto d = _get()._block_receive(buffer);
        if (!d) {
            return detail::fail(d);
        }
        return d->size;
    }

    SGCL_INLINE_HOT async::task<expected<size_t, io::error>> udp::socket::async_receive(const slice<byte>& buffer) const noexcept {
        return _receive_size(_impl, buffer);
    }

    inline async::task<expected<size_t, io::error>> udp::socket::_receive_size(tracked_ptr<detail::UdpImpl> impl, slice<byte> buffer) noexcept {
        auto d = co_await impl->async_receive(buffer);
        if (!d) {
            co_return detail::fail(d);
        }
        co_return d->size;
    }

    SGCL_INLINE_HOT expected<size_t, io::error> udp::socket::send(const slice<const byte>& data) const {
        return _get().send(data, endpoint());
    }

    SGCL_INLINE_HOT async::task<expected<size_t, io::error>> udp::socket::async_send(const slice<const byte>& data) const noexcept {
        return _get()._co_send(data, endpoint());
    }

    SGCL_INLINE_HOT expected<void, io::error> udp::socket::close() const noexcept {
        return _get().close();
    }

    SGCL_INLINE_HOT bool udp::socket::is_closed() const noexcept {
        return _get().is_closed();
    }

    SGCL_INLINE_HOT endpoint udp::socket::local_endpoint() const noexcept {
        return _get().local_endpoint();
    }

    SGCL_INLINE_HOT endpoint udp::socket::remote_endpoint() const noexcept {
        return _get().remote_endpoint();
    }

    SGCL_INLINE_HOT void udp::socket::set_deadline(time_point t) const noexcept {
        _get().set_deadline(detail::Descriptor::Read, t);
        _get().set_deadline(detail::Descriptor::Write, t);
    }

    SGCL_INLINE_HOT void udp::socket::set_read_deadline(time_point t) const noexcept {
        _get().set_deadline(detail::Descriptor::Read, t);
    }

    SGCL_INLINE_HOT void udp::socket::set_write_deadline(time_point t) const noexcept {
        _get().set_deadline(detail::Descriptor::Write, t);
    }

    SGCL_INLINE_HOT time_point udp::socket::read_deadline() const noexcept {
        return _get().deadline(detail::Descriptor::Read);
    }

    SGCL_INLINE_HOT time_point udp::socket::write_deadline() const noexcept {
        return _get().deadline(detail::Descriptor::Write);
    }

    // --- udp's functions ------------------------------------------------------

    SGCL_INLINE_HOT expected<udp::socket, io::error> udp::bind(const string& address) noexcept {
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

    SGCL_INLINE_HOT async::task<expected<udp::socket, io::error>> udp::async_bind(const string& address) noexcept {
        return _async_bind(address);
    }

    SGCL_INLINE_HOT expected<udp::socket, io::error> udp::connect(const string& address) noexcept {
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

    SGCL_INLINE_HOT async::task<expected<udp::socket, io::error>> udp::async_connect(const string& address) noexcept {
        return _async_connect(address);
    }

    inline async::task<expected<udp::socket, io::error>> udp::_async_bind(string address) noexcept {
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

    inline async::task<expected<udp::socket, io::error>> udp::_async_connect(string address) noexcept {
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

    // Stream sockets in the file system (AF_UNIX): connect to the path a
    // listener made; listen creates the socket's file (an error when
    // something is at the path already, as in Go), and the listener's
    // close() removes it. A path is at most 103 bytes on macOS (107 on
    // Linux). `unix_domain` and not `unix`: `unix` is a macro in the GNU
    // modes of GCC and Clang on Linux.
    struct unix_domain {
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& path) {
            return _block_connect(path);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(const string& path) noexcept {
            return _co_connect(path);
        }

        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        SGCL_INLINE_HOT static expected<net::listener, io::error> listen(const string& path) noexcept {
            return _block_listen(path);
        }

        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> async_listen(const string& path) noexcept {
            return _co_listen(path);
        }

    private:
        static async::task<expected<net::connection, io::error>> _async_connect(string path) noexcept {
            auto c = net::detail::start_connect_unix(path);
            if (!c) {
                co_return net::detail::fail(c);
            }
            int e = c->pending ? co_await c->conn->_co_connected(async::stop_token()) : 0;
            co_return net::detail::finish_connect_unix(*c, e, path);
        }

        static async::task<expected<net::listener, io::error>> _async_listen(string path) noexcept {
            co_return net::detail::listen_unix(path);
        }

        // the two halves of the operations above: a thread's and a task's
        SGCL_INLINE_HOT static expected<net::connection, io::error> _block_connect(const string& path)  {
            auto c = net::detail::start_connect_unix(path);
            if (!c) {
                return net::detail::fail(c);
            }
            return net::detail::finish_connect_unix(*c, c->pending ? c->conn->connected() : 0, path);
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_connect(const string& path) noexcept {
            return _async_connect(path);
        }

        SGCL_INLINE_HOT static expected<net::listener, io::error> _block_listen(const string& path) noexcept {
            return net::detail::listen_unix(path);
        }

        // Nothing to wait for: the task form of listen, for symmetry
        SGCL_INLINE_HOT static async::task<expected<net::listener, io::error>> _co_listen(const string& path) noexcept {
            return _async_listen(path);
        }
    };
}
