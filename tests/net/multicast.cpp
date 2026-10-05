//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// UDP multicast (net::udp::listen_multicast and the multicast members of
// udp::socket) and net::interfaces, on the loopback interface alone, so
// that nothing leaves the machine: IPv4 groups of 239.0.0.0/8 (RFC 2365,
// organization-local) and IPv6 groups of ff01:: and ff02:: on the
// loopback's index; two receivers on one port; leave stops the delivery;
// a source-specific membership takes its source alone; the TTL, the hop
// limit and the loopback read back as set, the loopback off keeps a
// datagram from the machine's own members; the boundaries (a group that
// is not one, a group of the other family, a zone that names nothing, a
// closed socket, twice joined, never joined); and Go's
// net.ListenMulticastUDP (tools/multicast_oracle.go) receiving what we
// send and sending what we receive.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/io/exec.h"
#include "sgcl/net/net.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
    using namespace std::chrono_literals;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    net::network_interface loopback() {
        auto all = net::interfaces();
        EXPECT_TRUE(all.has_value());
        for (auto& i : *all) {
            if (i.loopback && i.up && i.multicast) {
                return i;
            }
        }
        ADD_FAILURE() << "no loopback interface able to multicast";
        return {};
    }

    // A group of this run's own, so that two runs at once never share one
    std::string group_v4(int k) {
        return "239.255." + std::to_string((::getpid() >> 8) & 0xFF) + "." + std::to_string((::getpid() + k) & 0xFF);
    }

    std::string group_v6(const char* scope, int k) {
        char text[64];
        std::snprintf(text, sizeof(text), "%s::5:%x:%x", scope, unsigned(::getpid() & 0xFFFF), unsigned(k));
        return text;
    }

    std::string with_port(const std::string& group, uint16_t port) {
        return group.find(':') != std::string::npos ? "[" + group + "]:" + std::to_string(port) : group + ":" + std::to_string(port);
    }

    // The next datagram's text, or the error's message, within `wait`
    std::string receive(const net::udp::socket& s, std::chrono::milliseconds wait = 2000ms) {
        sgcl::vector<byte> room(256);
        s.set_read_deadline(clock::now() + wait);
        auto d = s.receive_from(room);
        if (!d) {
            return "error: " + str(d.error().message());
        }
        return std::string(reinterpret_cast<const char*>(room.data()), d->size);
    }

    struct Sender {
        net::udp::socket s;
        net::endpoint to;

        Sender(const std::string& group, uint16_t port, const net::network_interface& ifi) {
            bool v4 = group.find(':') == std::string::npos;
            s = net::udp::bind(v4 ? "127.0.0.1:0" : "[::1]:0").value();
            EXPECT_TRUE(s.set_multicast_interface(ifi).has_value());
            std::string zoned = v4 ? group : group + "%" + str(ifi.name);
            to = net::endpoint(net::ip_address(sgcl::string(zoned)), port);
        }

        void send(const char* text) {
            EXPECT_TRUE(s.send_to(std::string_view(text), to).has_value());
        }
    };
}

TEST(NetInterfaces_Tests, TheMachinesInterfaces) {
    auto all = net::interfaces();
    ASSERT_TRUE(all.has_value()) << str(all.error().message());
    ASSERT_FALSE(all->empty());
    std::set<std::string> names;
    std::set<uint32_t> indexes;
    bool loop = false;
    for (auto& i : *all) {
        EXPECT_FALSE(i.name.empty());
        EXPECT_TRUE(names.insert(str(i.name)).second) << str(i.name);   // each once
        EXPECT_GT(i.index, 0u) << str(i.name);
        EXPECT_TRUE(indexes.insert(i.index).second);
        for (auto& a : i.addresses) {
            EXPECT_TRUE(a.is_valid());
            EXPECT_GE(a.bits(), 0);
            EXPECT_LE(a.bits(), a.address().is_v4() ? 32 : 128);
            EXPECT_FALSE(a.address().has_zone());
        }
        if (i.loopback) {
            loop = true;
            bool has127 = false;
            for (auto& a : i.addresses) {
                has127 = has127 || a.address() == net::ip_address::loopback_v4() || a.address() == net::ip_address::loopback_v6();
            }
            EXPECT_TRUE(has127) << str(i.name);
            EXPECT_TRUE(i.up);
        }
    }
    EXPECT_TRUE(loop);
    auto lo = loopback();
    auto by_name = net::interface_by_name(lo.name);
    ASSERT_TRUE(by_name.has_value());
    EXPECT_EQ(by_name->index, lo.index);
    EXPECT_EQ(by_name->addresses.size(), lo.addresses.size());
    auto by_index = net::interface_by_index(lo.index);
    ASSERT_TRUE(by_index.has_value());
    EXPECT_EQ(by_index->name, lo.name);
    auto none = net::interface_by_name("no-such-interface0");
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), std::errc::no_such_device_or_address);
    EXPECT_FALSE(net::interface_by_name("").has_value());
    EXPECT_FALSE(net::interface_by_index(0).has_value());
    EXPECT_FALSE(net::interface_by_index(0xFFFFFFFFu).has_value());
    // the prefix length of a netmask
    const uint8_t m24[] = {255, 255, 255, 0}, m0[] = {0, 0, 0, 0}, m32[] = {255, 255, 255, 255}, m20[] = {255, 255, 240, 0};
    EXPECT_EQ(net::detail::mask_bits(m24, 4), 24);
    EXPECT_EQ(net::detail::mask_bits(m0, 4), 0);
    EXPECT_EQ(net::detail::mask_bits(m32, 4), 32);
    EXPECT_EQ(net::detail::mask_bits(m20, 4), 20);
    net::network_interface empty;
    EXPECT_TRUE(empty.name.empty());
    EXPECT_EQ(empty.index, 0u);
    EXPECT_FALSE(empty.up || empty.loopback || empty.multicast);
}

TEST(NetMulticast_Tests, IPv4OnTheLoopbackToTwoReceivers) {
    auto lo = loopback();
    std::string g = group_v4(1);
    auto first = net::udp::listen_multicast(sgcl::string(g + ":0"), lo);
    ASSERT_TRUE(first.has_value()) << str(first.error().message());
    uint16_t port = first->local_endpoint().port();
    EXPECT_NE(port, 0);
    EXPECT_EQ(first->local_endpoint().address(), net::ip_address::any_v4());
    auto second = net::udp::listen_multicast(sgcl::string(with_port(g, port)), lo);   // the same port: SO_REUSEPORT
    ASSERT_TRUE(second.has_value()) << str(second.error().message());
    Sender out(g, port, lo);
    out.send("hello");
    EXPECT_EQ(receive(*first), "hello");
    EXPECT_EQ(receive(*second), "hello");
    // the sender as the receiver sees it
    out.send("from");
    sgcl::vector<byte> room(64);
    first->set_read_deadline(clock::now() + 2s);
    auto d = first->receive_from(room);
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->from, out.s.local_endpoint());
    EXPECT_EQ(receive(*second), "from");
}

TEST(NetMulticast_Tests, IPv6OnTheLoopbackIndex) {
    auto lo = loopback();
    for (const char* scope : {"ff02", "ff01"}) {
        SCOPED_TRACE(scope);
        std::string g = group_v6(scope, 1);
        auto first = net::udp::listen_multicast(sgcl::string(with_port(g, 0)), lo);
        if (!first && first.error().code() == std::errc::address_family_not_supported) {
            GTEST_SKIP() << "no IPv6";
        }
        ASSERT_TRUE(first.has_value()) << str(first.error().message());
        uint16_t port = first->local_endpoint().port();
        // the second by the group's zone, not by the interface given
        auto second = net::udp::listen_multicast(sgcl::string(with_port(g + "%" + str(lo.name), port)));
        ASSERT_TRUE(second.has_value()) << str(second.error().message());
        Sender out(g, port, lo);
        out.send("six");
        EXPECT_EQ(receive(*first), "six");
        EXPECT_EQ(receive(*second), "six");
    }
}

TEST(NetMulticast_Tests, LeaveStopsTheDelivery) {
    auto lo = loopback();
    for (bool v6 : {false, true}) {
        SCOPED_TRACE(v6 ? "IPv6" : "IPv4");
        std::string g = v6 ? group_v6("ff02", 2) : group_v4(2);
        auto first = net::udp::listen_multicast(sgcl::string(with_port(g, 0)), lo);
        ASSERT_TRUE(first.has_value()) << str(first.error().message());
        uint16_t port = first->local_endpoint().port();
        auto second = net::udp::listen_multicast(sgcl::string(with_port(g, port)), lo);
        ASSERT_TRUE(second.has_value());
        Sender out(g, port, lo);
        net::ip_address group{sgcl::string(g)};
        ASSERT_TRUE(first->leave_group(group, lo).has_value());
        out.send("after");
        EXPECT_EQ(receive(*second), "after");
        EXPECT_EQ(receive(*first, 300ms).rfind("error: ", 0), 0u);   // nothing for the one that left
        // joined again: delivered again; left twice: the system's error
        ASSERT_TRUE(first->join_group(group, lo).has_value());
        out.send("again");
        EXPECT_EQ(receive(*first), "again");
        EXPECT_EQ(receive(*second), "again");
        auto twice = first->join_group(group, lo);
        ASSERT_FALSE(twice.has_value());
        EXPECT_EQ(str(twice.error().op()), "join");
        ASSERT_TRUE(first->leave_group(group, lo).has_value());
        auto gone = first->leave_group(group, lo);
        ASSERT_FALSE(gone.has_value());
        EXPECT_EQ(str(gone.error().op()), "leave");
        EXPECT_EQ(str(gone.error().path()), g);
    }
}

TEST(NetMulticast_Tests, SourceSpecificMembership) {
    auto lo = loopback();
    std::string g = group_v4(3);
    net::ip_address group{sgcl::string(g)};
    net::udp::socket r = net::udp::bind("0.0.0.0:0").value();
    uint16_t port = r.local_endpoint().port();
    auto j = r.join_source_group(group, net::ip_address::loopback_v4(), lo);
    ASSERT_TRUE(j.has_value()) << str(j.error().message());
    Sender out(g, port, lo);
    out.send("right source");
    EXPECT_EQ(receive(r), "right source");
    ASSERT_TRUE(r.leave_source_group(group, net::ip_address::loopback_v4(), lo).has_value());
    ASSERT_TRUE(r.join_source_group(group, net::ip_address("192.0.2.1"), lo).has_value());
    out.send("wrong source");
    EXPECT_EQ(receive(r, 300ms).rfind("error: ", 0), 0u);
    ASSERT_TRUE(r.leave_source_group(group, net::ip_address("192.0.2.1"), lo).has_value());
    // the source refused before the system sees it
    for (const char* bad : {"239.1.1.1", "0.0.0.0", "::1"}) {
        auto e = r.join_source_group(group, net::ip_address(bad), lo);
        ASSERT_FALSE(e.has_value()) << bad;
        EXPECT_EQ(e.error().code(), net::errc::invalid_address);
    }
    EXPECT_EQ(str(r.join_source_group(group, net::ip_address("::1"), lo).error().path()), g + " from ::1");
    EXPECT_FALSE(r.join_source_group(group, net::ip_address(), lo).has_value());
}

TEST(NetMulticast_Tests, TheSendersSettingsReadBack) {
    for (const char* address : {"127.0.0.1:0", "[::1]:0", ":0"}) {
        SCOPED_TRACE(address);
        net::udp::socket s = net::udp::bind(address).value();
        EXPECT_EQ(s.multicast_ttl().value(), 1);   // RFC 1112 §6.1, RFC 3493 §5.2: 1 by default
        EXPECT_TRUE(s.multicast_loopback().value());
        for (int ttl : {0, 5, 64, 255}) {
            ASSERT_TRUE(s.set_multicast_ttl(ttl).has_value()) << ttl;
            EXPECT_EQ(s.multicast_ttl().value(), ttl);
        }
        for (int bad : {-1, 256, 100000}) {
            auto e = s.set_multicast_ttl(bad);
            ASSERT_FALSE(e.has_value());
            EXPECT_EQ(e.error().code(), std::errc::invalid_argument);
            EXPECT_EQ(str(e.error().op()), "multicast ttl");
        }
        EXPECT_EQ(s.multicast_ttl().value(), 255);   // unchanged by the refusals
        ASSERT_TRUE(s.set_multicast_loopback(false).has_value());
        EXPECT_FALSE(s.multicast_loopback().value());
        ASSERT_TRUE(s.set_multicast_loopback(true).has_value());
        EXPECT_TRUE(s.multicast_loopback().value());
        // the interface of index 0: the system's choice again for IPv4; IPv6
        // leaves it to the system, and macOS refuses it (RFC 3493 §5.2 has
        // 0 as the default)
        auto none = s.set_multicast_interface(net::network_interface());
        if (std::string(address) == "127.0.0.1:0") {
            EXPECT_TRUE(none.has_value());
        } else {
            EXPECT_TRUE(none.has_value() || none.error().code() == std::errc::invalid_argument);
        }
    }
}

TEST(NetMulticast_Tests, TheLoopbackOffKeepsItFromThisMachine) {
    auto lo = loopback();
    for (bool v6 : {false, true}) {
        SCOPED_TRACE(v6 ? "IPv6" : "IPv4");
        std::string g = v6 ? group_v6("ff02", 4) : group_v4(4);
        auto r = net::udp::listen_multicast(sgcl::string(with_port(g, 0)), lo);
        ASSERT_TRUE(r.has_value());
        Sender out(g, r->local_endpoint().port(), lo);
        ASSERT_TRUE(out.s.set_multicast_loopback(false).has_value());
        out.send("kept");
        EXPECT_EQ(receive(*r, 300ms).rfind("error: ", 0), 0u);
        ASSERT_TRUE(out.s.set_multicast_loopback(true).has_value());
        out.send("looped");
        EXPECT_EQ(receive(*r), "looped");
    }
}

TEST(NetMulticast_Tests, ATaskReceives) {
    auto lo = loopback();
    std::string g = group_v4(5);
    net::udp::socket r = net::udp::listen_multicast(sgcl::string(g + ":0"), lo).value();
    Sender out(g, r.local_endpoint().port(), lo);
    auto got = [](net::udp::socket r) -> async::task<std::string> {
        tracked_ptr<array<byte, 64>> room = make_tracked<array<byte, 64>>();
        r.set_read_deadline(clock::now() + 2s);
        auto d = co_await r.async_receive_from(slice<byte>(room, room->data(), room->size()));
        co_return d ? std::string(reinterpret_cast<const char*>(room->data()), d->size) : std::string("error");
    }(r);
    auto running = async::spawn(std::move(got));
    std::this_thread::sleep_for(50ms);
    out.send("task");
    EXPECT_EQ(running.wait(), "task");
}

TEST(NetMulticast_Tests, TheBoundaries) {
    auto lo = loopback();
    for (const char* bad : {"10.0.0.1:5000", "239.1.2.3", "239.1.2.3:70000", "nonsense", "", "[2001:db8::1]:5000",
                            "[ff02::1%no-such-zone0]:5000", "224.0.0.1:x", "example.com:5000"}) {
        SCOPED_TRACE(bad);
        auto r = net::udp::listen_multicast(bad);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), net::errc::invalid_address);
        EXPECT_EQ(str(r.error().op()), "listen udp");
        EXPECT_EQ(str(r.error().path()), bad);
    }
    // an interface without an IPv4 address cannot send IPv4 multicast
    net::udp::socket v4 = net::udp::bind("127.0.0.1:0").value();
    net::network_interface fake;
    fake.name = "fake0";
    fake.index = 9999;
    auto e = v4.set_multicast_interface(fake);
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), std::errc::address_not_available);
    // groups: not one, of the other family, of a zone that names nothing
    net::udp::socket v6 = net::udp::bind("[::1]:0").value();
    net::udp::socket both = net::udp::bind(":0").value();
    EXPECT_EQ(v4.join_group(net::ip_address("10.1.2.3")).error().code(), net::errc::invalid_address);
    EXPECT_EQ(v4.join_group(net::ip_address()).error().code(), net::errc::invalid_address);
    EXPECT_EQ(v4.join_group(net::ip_address("ff02::1")).error().code(), std::errc::address_family_not_supported);
    EXPECT_EQ(v6.join_group(net::ip_address("239.1.1.1")).error().code(), std::errc::address_family_not_supported);
    EXPECT_EQ(both.join_group(net::ip_address("239.1.1.1")).error().code(), std::errc::address_family_not_supported);
    EXPECT_EQ(v6.join_group(net::ip_address("ff02::1%no-such-zone0")).error().code(), net::errc::invalid_address);
    EXPECT_EQ(v4.leave_group(net::ip_address("1.2.3.4")).error().code(), net::errc::invalid_address);
    EXPECT_EQ(str(v4.join_group(net::ip_address("10.1.2.3")).error().message()), "join 10.1.2.3: invalid address");
    // an IPv4-mapped group is the IPv4 one
    std::string g = group_v4(6);
    net::udp::socket r = net::udp::bind("0.0.0.0:0").value();
    ASSERT_TRUE(r.join_group(net::ip_address(sgcl::string("::ffff:" + g)), lo).has_value());
    ASSERT_TRUE(r.leave_group(net::ip_address(sgcl::string(g)), lo).has_value());
    // a closed socket: io's closed for every one of them
    net::udp::socket closed = net::udp::listen_multicast(sgcl::string(group_v4(7) + ":0"), lo).value();
    ASSERT_TRUE(closed.close().has_value());
    net::ip_address group{sgcl::string(group_v4(7))};
    EXPECT_TRUE(closed.join_group(group).error().is_closed());
    EXPECT_TRUE(closed.leave_group(group).error().is_closed());
    EXPECT_TRUE(closed.join_source_group(group, net::ip_address::loopback_v4()).error().is_closed());
    EXPECT_TRUE(closed.leave_source_group(group, net::ip_address::loopback_v4()).error().is_closed());
    EXPECT_TRUE(closed.set_multicast_interface(lo).error().is_closed());
    EXPECT_TRUE(closed.set_multicast_ttl(3).error().is_closed());
    EXPECT_TRUE(closed.multicast_ttl().error().is_closed());
    EXPECT_TRUE(closed.set_multicast_loopback(true).error().is_closed());
    EXPECT_TRUE(closed.multicast_loopback().error().is_closed());
    // a copy of a socket is the same socket: its membership is the copy's
    net::udp::socket copy = r;
    ASSERT_TRUE(copy.join_group(net::ip_address(sgcl::string(g)), lo).has_value());
    EXPECT_FALSE(r.join_group(net::ip_address(sgcl::string(g)), lo).has_value());
}

namespace {
    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    std::string scratch() {
        static int n = 0;
        auto d = std::filesystem::temp_directory_path() / ("sgcl_mc_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(d);
        return d.string();
    }

    std::string oracle() {
        static std::string built = [] {
            if (go_path().empty()) {
                return std::string();
            }
            std::string bin = scratch() + "/multicast_oracle";
            io::command b(sgcl::string(go_path()), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin),
                          sgcl::string((source_root() / "tools/multicast_oracle.go").string()));
            return b.combined_output().has_value() ? bin : std::string();
        }();
        return built;
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // The oracle as a child, its output in a file read as it grows
    struct Go {
        io::command cmd;
        std::string log;

        explicit Go(sgcl::vector<sgcl::string> args)
        : cmd(sgcl::string(oracle()), std::move(args)) {
            log = scratch() + "/out.log";
            auto f = io::create(sgcl::string(log));
            cmd.out = *f;
            cmd.err = *f;
            EXPECT_TRUE(cmd.start().has_value());
            (void)f->close();
        }

        std::string wait_for(const std::string& what, int ms = 10000) {
            for (int i = 0; i < ms / 10; ++i) {
                std::string s = slurp(log);
                if (s.find(what) != std::string::npos) {
                    return s;
                }
                std::this_thread::sleep_for(10ms);
            }
            return slurp(log);
        }

        ~Go() {
            (void)cmd.process.kill();
            (void)cmd.wait();
        }
    };
}

TEST(NetMulticastInterop, GoReceivesWhatWeSendAndSendsWhatWeReceive) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's net.ListenMulticastUDP is the oracle)";
    }
    ASSERT_FALSE(oracle().empty()) << "go build tools/multicast_oracle.go failed";
    auto lo = loopback();
    for (bool v6 : {false, true}) {
        SCOPED_TRACE(v6 ? "IPv6" : "IPv4");
        std::string g = v6 ? group_v6("ff02", 8) : group_v4(8);
        // a free port of the group's: ours bound, its port lent to Go
        auto probe = net::udp::listen_multicast(sgcl::string(with_port(g, 0)), lo);
        ASSERT_TRUE(probe.has_value());
        uint16_t port = probe->local_endpoint().port();
        ASSERT_TRUE(probe->close().has_value());
        std::string at = with_port(g, port);
        {
            Go go({sgcl::string("-listen"), sgcl::string(at), sgcl::string("-if"), lo.name, sgcl::string("-n"), sgcl::string("3")});
            ASSERT_NE(go.wait_for("LISTEN").find("LISTEN"), std::string::npos) << slurp(go.log);
            Sender out(g, port, lo);
            for (const char* m : {"one", "two", "three"}) {
                out.send(m);
                std::this_thread::sleep_for(10ms);
            }
            std::string got = go.wait_for("GOT three");
            EXPECT_NE(got.find("GOT one\nGOT two\nGOT three"), std::string::npos) << got;
        }
        {
            net::udp::socket r = net::udp::listen_multicast(sgcl::string(at), lo).value();
            Go go({sgcl::string("-send"), sgcl::string(at), sgcl::string("-if"), lo.name, sgcl::string("-n"), sgcl::string("3"), sgcl::string("-msg"), sgcl::string("go")});
            EXPECT_EQ(receive(r, 5000ms), "go 0");
            EXPECT_EQ(receive(r, 5000ms), "go 1");
            EXPECT_EQ(receive(r, 5000ms), "go 2");
            EXPECT_NE(go.wait_for("SENT").find("SENT"), std::string::npos);
        }
    }
}
