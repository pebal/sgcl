//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// /etc/resolv.conf as the stub resolver reads it (sgcl/net/detail/
// resolv_conf.h), a table of texts and what each gives, against
// resolv.conf(5): the servers (three at most, a zone, a port of macOS's
// resolver(5)), the search list (six at most, the last of domain and search
// winning, the host's domain without one), the options and their caps,
// comments, unknown keywords; the file read from a path and read again
// when it changes; the order of the names a lookup tries (ndots).
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;

    std::vector<std::string> servers(const nd::ResolvConf& c) {
        std::vector<std::string> out;
        for (auto& e : c.servers) {
            out.push_back(std::string(e.to_string().view()));
        }
        return out;
    }

    struct Case {
        const char* text;
        const char* hostname;
        std::vector<std::string> servers;
        std::vector<std::string> search;
        int ndots = 1;
        int timeout = 5;
        int attempts = 2;
        bool rotate = false;
        bool tcp = false;
    };

    const std::vector<std::string> local = {"127.0.0.1:53", "[::1]:53"};
}

TEST(NetDnsResolvConf_Tests, ATableOfTexts) {
    const Case cases[] = {
        {"", "", local, {}},
        {"# nothing\n; nothing\n\n   \n", "", local, {}},
        {"nameserver 10.0.0.1\n", "", {"10.0.0.1:53"}, {}},
        {"nameserver 10.0.0.1\nnameserver 2001:db8::53\nnameserver fe80::1%lo0\n", "", {"10.0.0.1:53", "[2001:db8::53]:53", "[fe80::1%lo0]:53"}, {}},
        {"nameserver 1.1.1.1\nnameserver 2.2.2.2\nnameserver 3.3.3.3\nnameserver 4.4.4.4\n", "", {"1.1.1.1:53", "2.2.2.2:53", "3.3.3.3:53"}, {}},
        {"nameserver not-an-address\nnameserver 10.0.0.2 trailing words\n", "", {"10.0.0.2:53"}, {}},
        {"nameserver\n", "", local, {}},
        {"nameserver 10.0.0.1:53\n", "", local, {}},   // a port is not part of the address
        {"#nameserver 10.0.0.9\n nameserver 10.0.0.1\n", "", {"10.0.0.1:53"}, {}},   // indented: still a line
        {"\tnameserver\t10.0.0.1\r\n", "", {"10.0.0.1:53"}, {}},
        {"port 5353\nnameserver 127.0.0.1\n", "", {"127.0.0.1:5353"}, {}},
        {"nameserver 127.0.0.1\nport 5353\n", "", {"127.0.0.1:5353"}, {}},
        {"port 0\nnameserver 127.0.0.1\n", "", {"127.0.0.1:53"}, {}},
        {"port 70000\n", "", local, {}},
        {"port 5300\n", "", {"127.0.0.1:5300", "[::1]:5300"}, {}},
        {"search corp.example example.com\n", "", local, {"corp.example.", "example.com."}},
        {"search corp.example. . x\n", "", local, {"corp.example.", "x."}},
        {"search a b c d e f g h\n", "", local, {"a.", "b.", "c.", "d.", "e.", "f."}},
        {"domain one.example\nsearch two.example\n", "", local, {"two.example."}},
        {"search two.example\ndomain one.example\n", "", local, {"one.example."}},
        {"domain\n", "host.lan.example", local, {"lan.example."}},
        {"search\n", "host.lan.example", local, {}},   // an empty search line is a search list of none
        {"", "host.lan.example", local, {"lan.example."}},
        {"", "host", local, {}},
        {"", "host.", local, {}},
        {"options ndots:2 timeout:3 attempts:4 rotate use-vc\n", "", local, {}, 2, 3, 4, true, true},
        {"options ndots:0\n", "", local, {}, 0},
        {"options ndots:99 timeout:999 attempts:99\n", "", local, {}, 15, 30, 5},
        {"options timeout:0 attempts:0\n", "", local, {}, 1, 1, 1},
        {"options ndots:x timeout:x attempts:-3\n", "", local, {}, 0, 1, 1},
        {"options tcp\noptions usevc\n", "", local, {}, 1, 5, 2, false, true},
        {"options edns0 trust-ad single-request inet6 debug\n", "", local, {}},
        {"options ndots:3\noptions ndots:1\n", "", local, {}, 1},
        {"sortlist 130.155.160.0/255.255.240.0\nlookup file bind\nfoo bar\n", "", local, {}},
        // macOS's /etc/resolv.conf as scutil writes it
        {"#\n# macOS Notice\n#\nsearch home\nnameserver 192.168.0.1\nnameserver fe80::1%en0\n", "mac.local", {"192.168.0.1:53", "[fe80::1%en0]:53"}, {"home."}},
    };
    for (auto& c : cases) {
        SCOPED_TRACE(c.text);
        nd::ResolvConf r = nd::parse_resolv_conf(c.text, c.hostname);
        EXPECT_EQ(servers(r), c.servers);
        EXPECT_EQ(r.search, c.search);
        EXPECT_EQ(r.ndots, c.ndots);
        EXPECT_EQ(r.timeout, c.timeout);
        EXPECT_EQ(r.attempts, c.attempts);
        EXPECT_EQ(r.rotate, c.rotate);
        EXPECT_EQ(r.tcp, c.tcp);
    }
}

// Without a nameserver line the servers are the local machine's, and say so
TEST(NetDnsResolvConf_Tests, TheDefaultServers) {
    EXPECT_TRUE(nd::parse_resolv_conf("", "").default_servers);
    EXPECT_TRUE(nd::parse_resolv_conf("nameserver nonsense\n", "").default_servers);
    EXPECT_TRUE(nd::parse_resolv_conf("port 5300\n", "").default_servers);
    EXPECT_FALSE(nd::parse_resolv_conf("nameserver 127.0.0.1\n", "").default_servers);
    EXPECT_FALSE(nd::parse_resolv_conf("nameserver ::1\n", "").default_servers);
}

TEST(NetDnsResolvConf_Tests, LongLinesAndManyFields) {
    std::string text = "search";
    for (int i = 0; i < 40; ++i) {
        text += " d" + std::to_string(i);
    }
    text += "\nnameserver " + std::string(100000, '1') + "\n";
    text += std::string(70000, 'x') + "\n";
    text += "nameserver 10.9.8.7";   // no line break at the end
    nd::ResolvConf r = nd::parse_resolv_conf(text, "");
    EXPECT_EQ(r.search.size(), 6u);
    EXPECT_EQ(servers(r), std::vector<std::string>{"10.9.8.7:53"});
    static const char with_nuls[] = "nameserver 10.0.0.1\0garbage\nsearch a\0b\n";
    std::string nul(with_nuls, sizeof with_nuls - 1);
    r = nd::parse_resolv_conf(nul, "");
    EXPECT_TRUE(r.default_servers);   // "10.0.0.1\0garbage" is no address
}

namespace {
    std::string scratch_file(const std::string& name) {
        auto d = std::filesystem::temp_directory_path() / ("sgcl_resolv_" + std::to_string(::getpid()));
        std::filesystem::create_directories(d);
        return (d / name).string();
    }

    void write_file(const std::string& path, const std::string& text) {
        std::ofstream(path, std::ios::trunc) << text;
    }
}

TEST(NetDnsResolvConf_Tests, TheFileReadAndReadAgainWhenItChanges) {
    nd::ResolvConfCache cache;
    std::string path = scratch_file("resolv.conf");
    write_file(path, "nameserver 10.1.1.1\nsearch first.example\n");
    cache.set_path(path);
    EXPECT_EQ(cache.path(), path);
    auto c = cache.get();
    EXPECT_EQ(servers(*c), std::vector<std::string>{"10.1.1.1:53"});
    EXPECT_EQ(c->search, std::vector<std::string>{"first.example."});
    EXPECT_EQ(cache.get().get(), c.get());   // within five seconds: the same, not looked at
    write_file(path, "nameserver 10.2.2.2\nnameserver 10.3.3.3\n");
    EXPECT_EQ(cache.get().get(), c.get());
    cache.set_path(path);   // what a change of the path does: read at the next get
    auto d = cache.get();
    EXPECT_EQ(servers(*d), (std::vector<std::string>{"10.2.2.2:53", "10.3.3.3:53"}));
    // a file that is not there: the defaults
    cache.set_path(scratch_file("missing.conf"));
    auto e = cache.get();
    EXPECT_TRUE(e->default_servers);
    EXPECT_EQ(e->ndots, 1);
    // the system's file reads without error, whatever it holds
    nd::ResolvConfCache system;
    EXPECT_EQ(system.path(), "/etc/resolv.conf");
    EXPECT_FALSE(system.get()->servers.empty());
    std::filesystem::remove_all(std::filesystem::path(path).parent_path());
}

TEST(NetDnsResolvConf_Tests, TheOrderOfTheNamesTried) {
    nd::ResolvConf c = nd::parse_resolv_conf("search a.example b.example\noptions ndots:2\n", "");
    auto order = [&](const std::string& name) {
        nd::DnsName n;
        nd::DnsNameText info;
        EXPECT_TRUE(nd::dns_name_from_text(name, n, &info));
        auto o = nd::dns_search_order(info, c);
        return std::vector<int>(o.order, o.order + o.count);
    };
    EXPECT_EQ(order("host"), (std::vector<int>{0, 1, -1}));          // fewer dots than ndots: the list first
    EXPECT_EQ(order("host.sub"), (std::vector<int>{0, 1, -1}));
    EXPECT_EQ(order("host.sub.example"), (std::vector<int>{-1, 0, 1}));   // ndots reached: the name first
    EXPECT_EQ(order("host."), (std::vector<int>{-1}));              // absolute: alone
    EXPECT_EQ(order("."), (std::vector<int>{-1}));
    c.search.clear();
    EXPECT_EQ(order("host"), (std::vector<int>{-1}));
    c.ndots = 0;
    c.search = {"x."};
    EXPECT_EQ(order("host"), (std::vector<int>{-1, 0}));
}
