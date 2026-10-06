//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ldap: the module's client against a minimal server of this program's
// own (a bind answered, a search answered with 100 entries of 8 attributes). One case per run; prints one line, ns per operation, for
// compare.sh (CASES=ldap). Go's standard library has no LDAP: the Go side
// (benchmarks/go/ldap) is a minimal client by hand, its messages read by
// encoding/asn1, against the same server.
//
//   ldap server               the server on 127.0.0.1: prints "port N", serves until killed
//   ldap <case> sgcl ADDR [n] the module's client against the server at ADDR:
//     ldap_bind               a simple bind and its answer: per bind
//     ldap_search             a search of 100 entries, each read into an entry: per entry
//     ldap_filter             RFC 4515's filter of 5 terms read into its BER (no network): per filter
//     ldap_rawbind            diagnostic: ldap_bind's bytes written and its answer read by hand over a raw
//                             connection, no client of the module's (what a round trip costs the scheduler,
//                             the socket and this server alone)
#include "benchmarks/common.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/ldap.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using encoding::asn1;
    namespace ld = sgcl::net::ldap::detail;
    namespace ldap = sgcl::net::ldap;

    void report(const char* what, double wall, double ops) {
        std::printf("ldap %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    async::task<> serve(net::connection c) {
        std::string buf;
        std::vector<char> block(65536);
        for (;;) {
            size_t total = 0;
            int f = ld::ldap_frame(buf, total);
            if (f < 0) {
                break;
            }
            if (f == 0 || buf.size() < total) {
                auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                if (!r || *r == 0) {
                    break;
                }
                buf.append(block.data(), *r);
                continue;
            }
            vector<byte> bytes(reinterpret_cast<const byte*>(buf.data()), reinterpret_cast<const byte*>(buf.data()) + total);
            buf.erase(0, total);
            auto m = asn1::parse(bytes, asn1::ber);
            if (!m) {
                break;
            }
            int32_t id = int32_t((*m)[0].as_int().value_or(0));
            asn1 op = (*m)[1];
            std::string out;
            auto done = [&](uint32_t tag) {
                auto b = ld::ldap_message(id, ld::ldap_app(tag, vector<asn1>{asn1::enumerated(0), ld::ldap_str(""), ld::ldap_str("")}));
                out.append(reinterpret_cast<const char*>(b.data()), b.size());
            };
            if (op.tag() == ld::op::unbind_request) {
                break;
            }
            if (op.tag() == ld::op::bind_request) {
                done(ld::op::bind_response);
            } else if (op.tag() == ld::op::search_request) {
                for (int i = 0; i < 100; ++i) {
                    std::string n = std::to_string(i);
                    vector<ldap::attribute> attrs = {{"objectClass", {"person", "inetOrgPerson"}}, {"uid", {sgcl::string("user" + n)}}, {"cn", {sgcl::string("User Number " + n)}},
                                                     {"sn", {"Number"}}, {"givenName", {"User"}}, {"mail", {sgcl::string("user" + n + "@example.com")}},
                                                     {"telephoneNumber", {sgcl::string("+48 555 0" + n)}},
                                                     {"description", {sgcl::string("A user of the benchmark, the entry number " + n + " of a hundred.")}}};
                    auto b = ld::ldap_message(id, ld::ldap_app(ld::op::search_entry, vector<asn1>{ld::ldap_str("uid=user" + n + ",ou=people,dc=example,dc=com"), ld::ldap_attributes(attrs)}));
                    out.append(reinterpret_cast<const char*>(b.data()), b.size());
                }
                done(ld::op::search_done);
            } else {
                done(ld::op::extended_response);
            }
            if (!co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()))) {
                break;
            }
        }
        (void)c.close();
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: ldap server | ldap <ldap_bind|ldap_search|ldap_filter> sgcl [ADDR] [n]\n");
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
    if (what == "ldap_filter") {
        long n = argc > 3 ? std::atol(argv[3]) : 500000;
        size_t total = 0;
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            total += ld::ldap_filter("(&(objectClass=person)(|(sn=Smith)(cn=Jo*n*))(!(mail=*@spam.example))(age>=30))").bytes().size();
        }
        report("ldap_filter", bench::seconds_since(t0), double(n));
        return total ? 0 : 1;
    }
    if (argc < 4) {
        return 2;
    }
    string addr(argv[3]);
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    if (what == "ldap_rawbind") {
        n = n ? n : 50000;
        net::connection raw = net::tcp::connect(addr).value();
        auto bytes = ld::ldap_message(1, ld::ldap_app(ld::op::bind_request, vector<asn1>{asn1::integer(3), ld::ldap_str("cn=admin,dc=example,dc=com"),
                                                                                           ld::ldap_prim(asn1::tag_class::context_specific, 0, "secret")}));
        auto t = [&]() -> async::task<bool> {
            char buf[512];
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                (void)co_await raw.async_write(slice<const byte>(bytes.data(), bytes.size()));
                size_t got = 0, total = 0;
                while (got < 2 || ld::ldap_frame(std::string_view(buf, got), total) != 1 || got < total) {
                    auto r = co_await raw.async_read(slice<byte>(reinterpret_cast<byte*>(buf) + got, sizeof buf - got));
                    if (!r || *r == 0) {
                        co_return false;
                    }
                    got += *r;
                }
            }
            report("ldap_rawbind", bench::seconds_since(t0), double(n));
            co_return true;
        };
        bool done = async::spawn(t()).wait();
        (void)raw.close();
        return done ? 0 : 1;
    }
    ldap::client::options o;
    o.security = ldap::security::none;
    auto c = ldap::client::connect(addr, o).value();
    bool ok = true;
    auto run = [&]() -> async::task<> {
        if (what == "ldap_bind") {
            n = n ? n : 50000;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                ok &= bool(co_await c.async_bind("cn=admin,dc=example,dc=com", "secret"));
            }
            report("ldap_bind", bench::seconds_since(t0), double(n));
        } else if (what == "ldap_search") {
            n = n ? n : 2000;
            size_t entries = 0;
            auto t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) {
                auto r = co_await c.async_search("ou=people,dc=example,dc=com", "(objectClass=person)");
                ok &= r && r->entries.size() == 100;
                entries += r ? r->entries.size() : 0;
            }
            report("ldap_search", bench::seconds_since(t0), double(entries));
        } else {
            ok = false;
        }
        (void)co_await c.async_unbind();
    };
    async::spawn(run()).wait();
    return ok ? 0 : 1;
}
