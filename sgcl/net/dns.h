//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/sockaddr.h"
#include "error.h"
#include "ip.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/string.h"

#include <algorithm>
#include <cerrno>
#include <netdb.h>
#include <string_view>
#include <sys/socket.h>

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // A deadline at the clock's end, where a limit of duration::max()
        // saturates (clock::now() + limit), is no deadline: the timers
        // never fire at it, and nothing is armed for it
        inline time_point no_deadline_at_max(time_point t) noexcept {
            return t == time_point::max() ? time_point() : t;
        }

        // getaddrinfo, on the calling thread: the addresses in the
        // resolver's order (RFC 6724 on the systems that sort), each once
        inline expected<vector<ip_address>, io::error> resolve(const string& host) {
            if (std::string_view(host.data(), host.size()).find('\0') != std::string_view::npos) {
                return io::detail::fail(net_error(errc::host_not_found, "lookup", host));   // c_str() would cut the name short and resolve another
            }
            addrinfo hints = {};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;   // one entry per address, not one per socket type
            addrinfo* list = nullptr;
            int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &list);
            int saved = errno;
            if (rc != 0) {
                return io::detail::fail(lookup_error(rc, saved, "lookup", host));
            }
            vector<ip_address> out;
            for (addrinfo* p = list; p; p = p->ai_next) {
                if (p->ai_family != AF_INET && p->ai_family != AF_INET6) {
                    continue;
                }
                auto a = from_sockaddr(p->ai_addr).address();
                if (std::find(out.begin(), out.end(), a) == out.end()) {
                    out.push_back(a);
                }
            }
            ::freeaddrinfo(list);
            if (out.empty()) {
                return io::detail::fail(net_error(errc::host_not_found, "lookup", host));
            }
            return out;
        }

        // What the pool runs: nothing when the stop came or the deadline
        // passed while the job waited for a thread, so that a burst of
        // lookups given up does not hold the pool in getaddrinfo (a call
        // already inside it cannot be stopped, and finishes on its own)
        inline optional<io::error> given_up(const async::stop_token& stop, time_point deadline, const string& what) {
            if (stop.stop_requested()) {
                return system_error(ECANCELED, "lookup", what);
            }
            if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                return system_error(ETIMEDOUT, "lookup", what);
            }
            return nullopt;
        }

        inline expected<vector<ip_address>, io::error> resolve_unless_given_up(const string& host, const async::stop_token& stop, time_point deadline) {
            if (auto e = given_up(stop, deadline, host)) {
                return io::detail::fail(*e);
            }
            return resolve(host);
        }

        // getnameinfo: the name of the address, as the system has it
        inline expected<vector<string>, io::error> resolve_name(const ip_address& address) {
            SockAddr sa;
            if (!to_sockaddr(endpoint(address, 0), address.is_v4() ? AF_INET : AF_INET6, sa)) {
                return io::detail::fail(net_error(errc::invalid_address, "lookup", address.to_string()));
            }
            char host[NI_MAXHOST];
            int rc = ::getnameinfo(sa.get(), sa.size, host, sizeof(host), nullptr, 0, NI_NAMEREQD);
            int saved = errno;
            if (rc != 0) {
                return io::detail::fail(lookup_error(rc, saved, "lookup", address.to_string()));
            }
            vector<string> out;
            out.push_back(string(host));
            return out;
        }

        // A job of the blocking pool awaited until it is done, the token
        // is stopped or the deadline passes, whichever comes first: the
        // job's result, ECANCELED or ETIMEDOUT. The job itself runs on to
        // its end (a thread in getaddrinfo cannot be stopped), and its
        // result is dropped.
        template<class T>
        async::task<expected<T, io::error>> await_job(async::blocking_task<expected<T, io::error>> job, async::stop_token stop, time_point deadline, string what) {
            deadline = no_deadline_at_max(deadline);
            if (!stop.stop_possible() && deadline == time_point()) {
                co_return co_await job;
            }
            async::stop_source never;   // a token for the select when the caller has none
            async::stop_token token = stop.stop_possible() ? stop : never.token();
            bool stopped = false, timed = false;
            if (deadline == time_point()) {
                co_await sgcl::async::select(job.on_done([] {}), token.on_stop([&] { stopped = true; }));
            } else {
                co_await sgcl::async::select(job.on_done([] {}), token.on_stop([&] { stopped = true; }), sgcl::async::timeout(deadline, [&] { timed = true; }));
            }
            if (stopped || stop.stop_requested()) {   // the stop wins over a result that came with it
                co_return io::detail::fail(system_error(ECANCELED, "lookup", what));
            }
            if (!job.done()) {
                co_return io::detail::fail(system_error(ETIMEDOUT, "lookup", what));
            }
            (void)timed;
            co_return co_await job;
        }

        // The addresses of a host, the stop and the deadline of a caller
        // (a dial) applied; a numeric host answered at once, without the pool
        inline async::task<expected<vector<ip_address>, io::error>> lookup_until(string host, async::stop_token stop, time_point deadline) {
            if (auto a = detail::IpText::parse(detail::IpText::view(host))) {   // a number: no error made for a name
                vector<ip_address> one;
                one.push_back(*a);
                co_return one;
            }
            if (host.empty()) {
                co_return io::detail::fail(net_error(errc::host_not_found, "lookup", host));
            }
            deadline = no_deadline_at_max(deadline);
            if (auto e = given_up(stop, deadline, host)) {
                co_return io::detail::fail(*e);
            }
            co_return co_await await_job(sgcl::async::spawn_blocking([host, stop, deadline] { return resolve_unless_given_up(host, stop, deadline); }), stop, deadline, host);
        }
    }

    // Names to addresses and back, through the system's resolver
    // (getaddrinfo, getnameinfo): /etc/hosts, the search domains, mDNS, the
    // resolvers a VPN or a profile installs, and the system's cache, which
    // on macOS nothing else answers correctly. The call blocks in the C
    // library, so the async forms run it on the blocking pool
    // (spawn_blocking), bounded by config::blocking_threads; a stop of the
    // token ends the wait with ECANCELED, and the thread finishes the call
    // on its own. A numeric address is answered at once, with no pool and
    // no resolver. A name outside ASCII is converted first
    // (txt::idna::to_ascii): lookup does not guess the profile.
    struct dns {
        // The addresses of the host in the resolver's order; a host with
        // none is net::errc::host_not_found, a failure of the resolver an
        // EAI_* code in net::lookup_category()
        // `dns::lookup(...)` on this thread, `co_await dns::async_lookup(...)` in a task;
        // the stop ends a task's wait. The thread's form takes none: the
        // system's resolver cannot be interrupted, so the call goes on to
        // its end
        static expected<vector<ip_address>, io::error> lookup(const string& host) {
            return _block_lookup(host);
        }

        static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, async::stop_token stop = {}) {
            return _co_lookup(host, stop);
        }

        // The names of the address (getnameinfo: one, the system's); an
        // address with none is net::errc::host_not_found
        // `dns::reverse_lookup(...)` on this thread, `co_await dns::async_reverse_lookup(...)` in a task;
        // the stop ends a task's wait (the thread's form takes none, as lookup)
        static expected<vector<string>, io::error> reverse_lookup(const ip_address& address) {
            return _block_reverse_lookup(address);
        }

        static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address, async::stop_token stop = {}) {
            return _co_reverse_lookup(address, stop);
        }

    private:
        // the two halves of the operations above: a thread's and a task's
        static expected<vector<ip_address>, io::error> _block_lookup(const string& host) {
            if (auto a = detail::IpText::parse(detail::IpText::view(host))) {   // a number: no error made for a name
                vector<ip_address> one;
                one.push_back(*a);
                return one;
            }
            if (host.empty()) {
                return io::detail::fail(net::detail::net_error(net::errc::host_not_found, "lookup", host));
            }
            return net::detail::resolve(host);
        }

        static async::task<expected<vector<ip_address>, io::error>> _co_lookup(const string& host, async::stop_token stop)  {
            return net::detail::lookup_until(host, std::move(stop), time_point());
        }

        static expected<vector<string>, io::error> _block_reverse_lookup(const ip_address& address) {
            return net::detail::resolve_name(address);
        }

        static async::task<expected<vector<string>, io::error>> _co_reverse_lookup(const ip_address& address, async::stop_token stop)  {
            return net::detail::await_job(sgcl::async::spawn_blocking([address, stop]() -> expected<vector<string>, io::error> {
                if (auto e = net::detail::given_up(stop, time_point(), address.to_string())) {
                    return io::detail::fail(*e);
                }
                return net::detail::resolve_name(address);
            }), stop, time_point(), address.to_string());
        }
    };
}
