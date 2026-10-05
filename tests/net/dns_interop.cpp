//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dns's records against Go's own stub resolver (tools/dns_oracle.go,
// built here with `go build`: net.Resolver with PreferGo, its Dial pointed
// at the server of tests/net/dns_server.h): the same queries to the same
// server give the same records, MX, TXT, SRV, NS and CNAME, over UDP and,
// for the answers the server truncates, over TCP, with the names
// compressed and not; the same failures (NXDOMAIN and NODATA "not found",
// SERVFAIL another). The orders that are random by the RFCs (MX of one
// preference, SRV of one priority) are compared as sets. Skipped when
// there is no go on the machine.
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/dns_server.h"

#include "sgcl/io/exec.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
    using namespace dns_test;
    using sgcl::net::dns;

    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    // tools/dns_oracle.go built once
    std::string oracle() {
        static std::string built = [] {
            if (go_path().empty()) {
                return std::string();
            }
            auto d = std::filesystem::temp_directory_path() / ("sgcl_dns_oracle_" + std::to_string(::getpid()));
            std::filesystem::create_directories(d);
            std::string bin = (d / "dns_oracle").string();
            std::string src = (source_root() / "tools/dns_oracle.go").string();
            io::command b(sgcl::string(go_path()), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin), sgcl::string(src));
            auto r = b.combined_output();
            return r.has_value() ? bin : std::string();
        }();
        return built;
    }

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    // Go's %q for printable ASCII
    std::string quoted(const std::string& s) {
        std::string q = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') {
                q += '\\';
            }
            q += c;
        }
        return q + "\"";
    }

    std::vector<std::string> split_fields(const std::string& q) {
        std::istringstream in(q);
        std::vector<std::string> f;
        std::string w;
        while (in >> w) {
            f.push_back(w);
        }
        return f;
    }

    // The lines Go prints for one query, from our lookups
    std::vector<std::string> ours(const std::string& q, const dns::options& o) {
        auto f = split_fields(q);
        std::vector<std::string> lines;
        auto fail = [&](const io::error& e) {
            if (e.code() == net::errc::host_not_found || e.code() == net::errc::no_data) {
                lines.push_back("ERR notfound");
            } else if (e.is_timeout()) {
                lines.push_back("ERR timeout");
            } else {
                lines.push_back("ERR other");
            }
        };
        sgcl::string name(f.back());
        if (f[0] == "mx") {
            auto r = dns::lookup_mx(name, o);
            if (!r) {
                fail(r.error());
            } else {
                for (auto& m : *r) {
                    lines.push_back("MX " + std::to_string(m.preference) + " " + str(m.host));
                }
            }
        } else if (f[0] == "txt") {
            auto r = dns::lookup_txt(name, o);
            if (!r) {
                fail(r.error());
            } else {
                for (auto& t : *r) {
                    lines.push_back("TXT " + quoted(str(t)));
                }
            }
        } else if (f[0] == "srv") {
            auto r = dns::lookup_srv(sgcl::string(f[1]), sgcl::string(f[2]), name, o);
            if (!r) {
                fail(r.error());
            } else {
                for (auto& s : *r) {
                    lines.push_back("SRV " + std::to_string(s.priority) + " " + std::to_string(s.weight) + " " + std::to_string(s.port) + " " + str(s.target));
                }
            }
        } else if (f[0] == "ns") {
            auto r = dns::lookup_ns(name, o);
            if (!r) {
                fail(r.error());
            } else {
                for (auto& n : *r) {
                    lines.push_back("NS " + str(n));
                }
            }
        } else if (f[0] == "cname") {
            auto r = dns::lookup_cname(name, o);
            if (!r) {
                fail(r.error());
            } else {
                lines.push_back("CNAME " + str(*r));
            }
        }
        for (auto& l : lines) {
            l = q + " | " + l;
        }
        return lines;
    }

    std::vector<Rr> zone() {
        return {
            rr_mx("example.test.", 20, "mx2.example.test."),
            rr_mx("example.test.", 10, "mx1.example.test."),
            rr_mx("example.test.", 20, "mx3.example.test."),
            rr_mx("example.test.", 30, "MiXeD.Example.Test."),
            rr_txt("example.test.", {"v=spf1 ", "include:_spf.example.test ", "-all"}),
            rr_txt("example.test.", {"quote\" and \\ back"}),
            rr_txt("empty.example.test.", {""}),
            rr_txt("big.example.test.", {std::string(255, 'a'), std::string(255, 'b'), std::string(255, 'c'), std::string(255, 'd'),
                                         std::string(255, 'e'), std::string(255, 'f'), std::string(255, 'g'), std::string(255, 'h')}),
            rr_named("example.test.", type::ns, "ns1.example.test."),
            rr_named("example.test.", type::ns, "ns2.example.test."),
            rr_srv("_sip._tcp.example.test.", 20, 0, 5060, "backup.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 60, 5060, "sip1.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 40, 5061, "sip2.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 0, 5062, "sip3.example.test."),
            // one CNAME: of a longer chain Go's own resolver gives the first
            // target, where its documentation, RFC 1034 §3.6.2 and
            // dns::lookup_cname give the chain's end (dns_records.cpp has that)
            rr_named("www.example.test.", type::cname, "host.example.test."),
            rr_a("host.example.test.", 192, 0, 2, 1),
            rr_aaaa("host.example.test."),
            rr_aaaa("v6only.example.test."),
            rr_named("alias.example.test.", type::cname, "example.test."),
            rr_a("example.test.", 192, 0, 2, 7),
        };
    }

    const char* const Queries[] = {
        "mx example.test.", "mx alias.example.test.", "mx nothing.example.test.", "mx host.example.test.", "mx servfail.example.test.",
        "txt example.test.", "txt big.example.test.", "txt empty.example.test.", "txt nothing.example.test.",
        "srv sip tcp example.test.", "srv xmpp tcp example.test.",
        "ns example.test.", "ns host.example.test.",
        "cname www.example.test.", "cname host.example.test.", "cname v6only.example.test.", "cname nothing.example.test.",
    };

    std::vector<std::string> lines_of(const std::string& text) {
        std::vector<std::string> v;
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l)) {
            if (!l.empty()) {
                v.push_back(l);
            }
        }
        std::sort(v.begin(), v.end());
        return v;
    }

    void compare(Behaviour& b) {
        Server s(b);
        ASSERT_TRUE(s.ok());
        sgcl::vector<sgcl::string> args;
        args.push_back(sgcl::string("-server"));
        args.push_back(sgcl::string(s.address()));
        for (const char* q : Queries) {
            args.push_back(sgcl::string(q));
        }
        io::command go(sgcl::string(oracle()), std::move(args));
        auto out = go.output();
        ASSERT_TRUE(out.has_value()) << str(out.error().message());
        auto theirs = lines_of(str(*out));
        dns::options o = s.options(std::chrono::milliseconds(2000), 2);
        std::string mine_text;
        for (const char* q : Queries) {
            for (auto& l : ours(q, o)) {
                mine_text += l + "\n";
            }
        }
        auto mine = lines_of(mine_text);
        EXPECT_EQ(mine, theirs);
        EXPECT_GE(theirs.size(), 25u);
    }
}

TEST(NetDnsInterop, GoResolverGivesTheSameRecords) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's resolver is the oracle)";
    }
    ASSERT_FALSE(oracle().empty()) << "go build tools/dns_oracle.go failed";
    Behaviour b;
    b.zone = zone();
    b.rcode["servfail.example.test."] = 2;
    b.truncate.insert("big.example.test.");   // 2 KB: TC, as a server sets it past the payload
    compare(b);
    EXPECT_GT(b.udp_queries.load(), 0);
}

TEST(NetDnsInterop, OverTCPAndWithoutCompression) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's resolver is the oracle)";
    }
    ASSERT_FALSE(oracle().empty());
    Behaviour b;
    b.zone = zone();
    b.rcode["servfail.example.test."] = 2;
    b.compress = false;
    for (const char* n : {"example.test.", "alias.example.test.", "big.example.test.", "www.example.test.", "_sip._tcp.example.test."}) {
        b.truncate.insert(n);
    }
    compare(b);
    EXPECT_GT(b.tcp_queries.load(), 0);
}
