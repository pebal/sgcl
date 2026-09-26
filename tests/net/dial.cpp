//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: the race of RFC 8305 (happy eyeballs) with the dial of one address
// replaced by a script and the time by the manual clock, so that every
// step of it is seen: the families in turns (§4), the next attempt 250 ms
// after the last or at once when it fails (§5), the first connection the
// winner and a later one closed, the error of the first attempt, the
// deadline, the stop. Then the real thing over the loopback, and dns: a
// name, a number without the pool, a name that does not exist (RFC 6761
// .invalid), a wait stopped by its token or its deadline.
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    enum class Script {
        black_hole,   // never answers: waits for the stop
        at_once,      // connects at once
        after_10ms,   // connects after 10 ms of the clock
        after_300ms,
        refused,      // fails at once
        unreachable
    };

    // What the fake dial saw and made, in a managed object the dial holds
    struct Dialer {
        std::mutex m;
        std::vector<net::endpoint> started;                   // in the order the race started them
        std::vector<net::endpoint> stopped;                   // ended by the race's stop
        std::map<ip_address, Script> scripts;
        vector<net::connection> made;                               // every connection it returned

        std::vector<net::endpoint> starts() {
            std::lock_guard lock(m);
            return started;
        }
    };

    net::endpoint ep(const char* address) {
        return net::endpoint(*ip_address::parse(address), 443);
    }

    net::detail::DialOne fake(tracked_ptr<Dialer> d) {
        return [d](net::endpoint to, stop_token stop) -> task<expected<net::connection, io::error>> {
            Script s;
            {
                std::lock_guard lock(d->m);
                d->started.push_back(to);
                s = d->scripts.at(to.address());
            }
            if (s == Script::refused || s == Script::unreachable) {
                co_return io::detail::fail(io::error(std::make_error_code(s == Script::refused ? std::errc::connection_refused : std::errc::host_unreachable), "dial tcp", to.to_string()));
            }
            if (s == Script::black_hole) {
                co_await stop.stopped();
                std::lock_guard lock(d->m);
                d->stopped.push_back(to);
                co_return io::detail::fail(io::error(std::make_error_code(std::errc::operation_canceled), "dial tcp", to.to_string()));
            }
            if (s == Script::after_10ms) {
                co_await sgcl::async::sleep(10ms);
            } else if (s == Script::after_300ms) {
                co_await sgcl::async::sleep(300ms);
            }
            auto pair = net::connection::in_memory();
            {
                std::lock_guard lock(d->m);
                d->made.push_back(pair.first);
            }
            co_return pair.first;
        };
    }

    task<expected<net::connection, io::error>> race(vector<net::endpoint> targets, tracked_ptr<Dialer> d, stop_token stop = {}, time_point deadline = {}) {
        co_return co_await net::detail::dial_race(std::move(targets), std::move(stop), deadline, net::detail::AttemptDelay, fake(d), "test");
    }

    vector<net::endpoint> targets_of(std::initializer_list<const char*> addresses) {
        vector<net::endpoint> out;
        for (auto a : addresses) {
            out.push_back(ep(a));
        }
        return out;
    }

    // The workers idle and the timers settled: what a step of the clock waits for
    void settle(sgcl::async::manual_clock& clock) {
        clock.advance(0ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        clock.advance(0ms);
    }
}

TEST(NetDial_Tests, FamiliesInTurns) {
    vector<ip_address> found;
    for (auto a : {"2001:db8::1", "2001:db8::2", "192.0.2.1", "2001:db8::3", "192.0.2.2", "192.0.2.3"}) {
        found.push_back(*ip_address::parse(a));
    }
    auto order = net::detail::interleave(found, 80);
    std::vector<std::string> got;
    for (auto& e : order) {
        got.push_back(std::string(e.to_string().data(), e.to_string().size()));
    }
    std::vector<std::string> expected = {"[2001:db8::1]:80", "192.0.2.1:80", "[2001:db8::2]:80", "192.0.2.2:80", "[2001:db8::3]:80", "192.0.2.3:80"};
    EXPECT_EQ(got, expected);   // the first family the first address's, then in turns, each in its order
    vector<ip_address> one_family;
    one_family.push_back(ip_address::loopback_v4());
    one_family.push_back(ip_address::v4(10, 0, 0, 1));
    auto same = net::detail::interleave(one_family, 1);
    ASSERT_EQ(same.size(), 2u);
    EXPECT_EQ(same[1].address(), ip_address::v4(10, 0, 0, 1));
}

TEST(NetDial_Tests, TheNextAttemptAfterTheDelay) {
    sgcl::async::manual_clock clock;
    clock.install();
    tracked_ptr<Dialer> d = make_tracked<Dialer>();
    d->scripts[ep("2001:db8::1").address()] = Script::black_hole;
    d->scripts[ep("192.0.2.1").address()] = Script::at_once;
    d->scripts[ep("2001:db8::2").address()] = Script::black_hole;
    auto t = spawn(race(targets_of({"2001:db8::1", "192.0.2.1", "2001:db8::2"}), d));
    settle(clock);
    EXPECT_EQ(d->starts().size(), 1u);          // the first alone
    clock.advance(249ms);
    settle(clock);
    EXPECT_EQ(d->starts().size(), 1u);          // not before 250 ms
    clock.advance(1ms);
    auto r = t.wait();
    ASSERT_TRUE(r) << r.error().message();
    auto starts = d->starts();
    ASSERT_EQ(starts.size(), 2u);               // the third never started: the second won
    EXPECT_EQ(starts[1], ep("192.0.2.1"));
    EXPECT_EQ(*r, d->made[0]);
    settle(clock);
    std::lock_guard lock(d->m);
    ASSERT_EQ(d->stopped.size(), 1u);           // the loser stopped
    EXPECT_EQ(d->stopped[0], ep("2001:db8::1"));
    clock.uninstall();
    sgcl::async::scheduler::stop();
}

TEST(NetDial_Tests, AFailureStartsTheNextAtOnce) {
    sgcl::async::manual_clock clock;
    clock.install();
    tracked_ptr<Dialer> d = make_tracked<Dialer>();
    d->scripts[ep("192.0.2.1").address()] = Script::refused;
    d->scripts[ep("2001:db8::1").address()] = Script::refused;
    d->scripts[ep("192.0.2.2").address()] = Script::at_once;
    auto r = spawn(race(targets_of({"192.0.2.1", "2001:db8::1", "192.0.2.2"}), d)).wait();   // no time passes
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(d->starts().size(), 3u);
    clock.uninstall();
    sgcl::async::scheduler::stop();
}

TEST(NetDial_Tests, EveryAttemptFailsTheFirstErrorReported) {
    tracked_ptr<Dialer> d = make_tracked<Dialer>();
    d->scripts[ep("192.0.2.1").address()] = Script::refused;
    d->scripts[ep("2001:db8::1").address()] = Script::unreachable;
    auto r = spawn(race(targets_of({"192.0.2.1", "2001:db8::1"}), d)).wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::connection_refused);
    auto none = spawn(race(vector<net::endpoint>(), d)).wait();
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::no_suitable_address);
}

TEST(NetDial_Tests, ALaterConnectionIsClosed) {
    sgcl::async::manual_clock clock;
    clock.install();
    tracked_ptr<Dialer> d = make_tracked<Dialer>();
    d->scripts[ep("192.0.2.1").address()] = Script::after_300ms;   // started at 0, done at 300
    d->scripts[ep("2001:db8::1").address()] = Script::after_10ms;  // started at 250, done at 260: the winner
    auto t = spawn(race(targets_of({"192.0.2.1", "2001:db8::1"}), d));
    settle(clock);
    clock.advance(250ms);
    settle(clock);
    clock.advance(10ms);
    auto r = t.wait();
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(d->starts().size(), 2u);
    clock.advance(40ms);   // the first attempt connects now, after the race is over
    settle(clock);
    {
        std::lock_guard lock(d->m);
        ASSERT_EQ(d->made.size(), 2u);
        EXPECT_EQ(d->made[0], *r);
        EXPECT_FALSE(d->made[0].is_closed());
        EXPECT_TRUE(d->made[1].is_closed());   // the late one closed by the race
    }
    clock.uninstall();
    sgcl::async::scheduler::stop();
}

TEST(NetDial_Tests, DeadlineAndStop) {
    sgcl::async::manual_clock clock;
    clock.install();
    tracked_ptr<Dialer> d = make_tracked<Dialer>();
    d->scripts[ep("2001:db8::1").address()] = Script::black_hole;
    d->scripts[ep("192.0.2.1").address()] = Script::black_hole;
    auto t = spawn(race(targets_of({"2001:db8::1", "192.0.2.1"}), d, stop_token(), clock.now() + 1s));
    settle(clock);
    clock.advance(999ms);
    settle(clock);
    EXPECT_FALSE(t.done());
    clock.advance(1ms);
    auto r = t.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout());
    settle(clock);
    {
        std::lock_guard lock(d->m);
        EXPECT_EQ(d->stopped.size(), 2u);     // every attempt stopped
    }
    stop_source src;
    auto u = spawn(race(targets_of({"2001:db8::1", "192.0.2.1"}), d, src.token()));
    settle(clock);
    src.request_stop();
    auto s = u.wait();
    ASSERT_FALSE(s);
    EXPECT_EQ(s.error().code(), std::errc::operation_canceled);
    clock.uninstall();
    sgcl::async::scheduler::stop();
}

TEST(NetDial_Tests, OverTheLoopback) {
    auto l = tcp::listen("127.0.0.1:0");   // IPv4 only: localhost's ::1, first on macOS, is refused, then 127.0.0.1 answers
    ASSERT_TRUE(l);
    auto port = sgcl::to_string(l->local_endpoint().port());
    auto accepting = spawn([](net::listener l) -> task<size_t> {
        size_t n = 0;
        for (int i = 0; i < 3; ++i) {
            auto c = co_await l.async_accept();
            n += (bool)c;
            if (c) {
                (void)c->close();
            }
        }
        co_return n;
    }(*l));
    auto a = tcp::connect(sgcl::string("localhost:") + port);
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(a->remote_endpoint().address(), ip_address::loopback_v4());
    auto b = tcp::connect(sgcl::string("localhost:") + port, 5s);
    ASSERT_TRUE(b) << b.error().message();
    auto c = spawn(tcp::async_connect(sgcl::string("localhost:") + port, stop_token())).wait();
    ASSERT_TRUE(c) << c.error().message();
    EXPECT_EQ(accepting.wait(), 3u);
    stop_source stopped;
    stopped.request_stop();
    auto cancelled = tcp::connect(sgcl::string("localhost:") + port, stopped.token());
    ASSERT_FALSE(cancelled);
    EXPECT_EQ(cancelled.error().code(), std::errc::operation_canceled);
    a->close();
    b->close();
    c->close();
    l->close();
}

TEST(NetDns_Tests, Names) {
    auto local = dns::lookup("localhost");
    ASSERT_TRUE(local) << local.error().message();
    bool loopback = false;
    for (auto& a : *local) {
        loopback |= a.is_loopback();
    }
    EXPECT_TRUE(loopback);
    auto async_local = spawn(dns::async_lookup("localhost")).wait();
    ASSERT_TRUE(async_local);
    EXPECT_EQ(async_local->size(), local->size());
    for (auto name : {"nothing.invalid", "a.b.c.invalid", ""}) {   // RFC 6761: .invalid never resolves
        auto r = dns::lookup(name);
        ASSERT_FALSE(r) << name;
        EXPECT_EQ(r.error().code(), net::errc::host_not_found) << name;
        EXPECT_EQ(r.error().op(), "lookup");
        auto ar = spawn(dns::async_lookup(name)).wait();
        ASSERT_FALSE(ar) << name;
        EXPECT_EQ(ar.error().code(), net::errc::host_not_found) << name;
    }
    auto cut = dns::lookup(sgcl::string(std::string_view("localhost\0.example", 18)));   // not "localhost": the name is not cut at the NUL
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), net::errc::host_not_found);
    auto names = dns::reverse_lookup(ip_address::loopback_v4());
    ASSERT_TRUE(names) << names.error().message();
    ASSERT_FALSE(names->empty());
    EXPECT_FALSE((*names)[0].empty());
    auto async_names = spawn(dns::async_reverse_lookup(ip_address::loopback_v4())).wait();
    ASSERT_TRUE(async_names);
    EXPECT_EQ((*async_names)[0], (*names)[0]);
    io::error e(error_code(EAI_AGAIN, net::lookup_category()), "lookup", "x");
    EXPECT_EQ(std::string_view(e.message()), std::string("lookup x: ") + ::gai_strerror(EAI_AGAIN));
}

TEST(NetDns_Tests, NumbersWithoutThePool) {
    sgcl::async::blocking_pool::stop();
    ASSERT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);
    auto v4 = dns::lookup("192.0.2.7");
    ASSERT_TRUE(v4);
    ASSERT_EQ(v4->size(), 1u);
    EXPECT_EQ((*v4)[0], ip_address::v4(192, 0, 2, 7));
    auto v6 = spawn(dns::async_lookup("fe80::1%lo0")).wait();
    ASSERT_TRUE(v6);
    EXPECT_EQ((*v6)[0].to_string(), "fe80::1%lo0");
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);   // no thread was started for them
}

TEST(NetDns_Tests, AWaitEndedByItsTokenOrItsDeadline) {
    std::mutex gate;
    gate.lock();   // the job blocks until the test lets it go, as a slow resolver would
    auto slow = [&] {
        return sgcl::async::spawn_blocking([&gate]() -> expected<vector<ip_address>, io::error> {
            std::lock_guard lock(gate);
            return vector<ip_address>();
        });
    };
    stop_source src;
    auto t = spawn(net::detail::await_job(slow(), src.token(), time_point(), "slow"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(t.done());
    src.request_stop();
    auto r = t.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::operation_canceled);
    sgcl::async::manual_clock clock;
    clock.install();
    auto u = spawn(net::detail::await_job(slow(), stop_token(), clock.now() + 3s, "slow"));
    settle(clock);
    clock.advance(3s);
    auto timed = u.wait();
    ASSERT_FALSE(timed);
    EXPECT_TRUE(timed.error().is_timeout());
    gate.unlock();   // the jobs finish on their own; nobody reads them
    sgcl::async::blocking_pool::wait_idle();
    clock.uninstall();
    sgcl::async::scheduler::stop();
}

// A lookup given up (the stop, or the deadline) before a thread of the pool
// takes it never reaches getaddrinfo: a name under .local costs the resolver
// seconds, a job that looks first costs nothing
TEST(NetDns_Tests, AGivenUpLookupDoesNotReachTheResolver) {
    auto t0 = std::chrono::steady_clock::now();
    stop_source src;
    src.request_stop();
    auto stopped = net::detail::resolve_unless_given_up("sgcl-nothing-here.local", src.token(), time_point());
    ASSERT_FALSE(stopped);
    EXPECT_EQ(stopped.error().code(), std::errc::operation_canceled);
    auto late = net::detail::resolve_unless_given_up("sgcl-nothing-here.local", stop_token(), sgcl::clock::now() - 1s);
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 1s);
    sgcl::async::blocking_pool::stop();
    auto past = spawn(net::detail::lookup_until("sgcl-nothing-here.local", stop_token(), sgcl::clock::now() - 1s)).wait();
    ASSERT_FALSE(past);
    EXPECT_TRUE(past.error().is_timeout());
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 0u);   // not even queued
    auto far = spawn(net::detail::lookup_until("localhost", stop_token(), sgcl::clock::now() + duration::max())).wait();
    ASSERT_TRUE(far) << far.error().message();   // a deadline at the clock's end is none
}
