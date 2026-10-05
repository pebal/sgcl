//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// /etc/resolv.conf as net::dns reads it (sgcl/net/detail/resolv_conf.h) on
// any text, without an oracle. The input's first line is the host's name,
// the rest the file. What must hold:
//   - the limits: one to three servers (or the two local ones), at most six
//     search domains each ending with a dot, ndots 0-15, timeout 1-30,
//     attempts 1-5;
//   - the configuration written out again as a resolv.conf and read, is
//     the same configuration;
//   - the order of the names a lookup tries, for a name of the input's,
//     holds the name itself once and every search domain once.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/resolv_conf_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/detail/dns_message.h"
#include "sgcl/net/detail/resolv_conf.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void sound(const nd::ResolvConf& c) {
        check(!c.servers.empty());
        check(c.default_servers ? c.servers.size() == 2 : c.servers.size() <= nd::ResolvConf::MaxServers);
        check(c.search.size() <= nd::ResolvConf::MaxSearch);
        for (auto& s : c.search) {
            check(!s.empty() && s.back() == '.');
        }
        check(c.ndots >= 0 && c.ndots <= nd::ResolvConf::MaxNdots);
        check(c.timeout >= 1 && c.timeout <= nd::ResolvConf::MaxTimeout);
        check(c.attempts >= 1 && c.attempts <= nd::ResolvConf::MaxAttempts);
        uint16_t port = c.servers.front().port();
        for (auto& e : c.servers) {
            check(e.port() == port && port != 0);
        }
    }

    std::string written(const nd::ResolvConf& c) {
        std::string t;
        t += "port " + std::to_string(c.servers.front().port()) + "\n";
        if (!c.default_servers) {
            for (auto& e : c.servers) {
                t += "nameserver " + std::string(e.address().to_string().view()) + "\n";
            }
        }
        t += "search";
        for (auto& s : c.search) {
            t += " " + s;
        }
        t += "\noptions ndots:" + std::to_string(c.ndots) + " timeout:" + std::to_string(c.timeout) + " attempts:" + std::to_string(c.attempts);
        if (c.rotate) {
            t += " rotate";
        }
        if (c.tcp) {
            t += " use-vc";
        }
        return t + "\n";
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 65536) {
        return 0;
    }
    std::string_view all(reinterpret_cast<const char*>(data), size);
    size_t eol = all.find('\n');
    std::string_view host = all.substr(0, eol);
    std::string_view text = eol == std::string_view::npos ? std::string_view() : all.substr(eol + 1);
    nd::ResolvConf c = nd::parse_resolv_conf(text, host);
    sound(c);
    nd::ResolvConf again = nd::parse_resolv_conf(written(c), "");
    sound(again);
    check(again.servers.size() == c.servers.size());
    for (size_t i = 0; i < c.servers.size(); ++i) {
        check(again.servers[i] == c.servers[i]);
    }
    check(again.search == c.search);
    check(again.ndots == c.ndots && again.timeout == c.timeout && again.attempts == c.attempts);
    check(again.rotate == c.rotate && again.tcp == c.tcp && again.default_servers == c.default_servers);
    // the names a lookup of the host's name would try
    nd::DnsName name;
    nd::DnsNameText info;
    if (nd::dns_name_from_text(host, name, &info)) {
        auto order = nd::dns_search_order(info, c);
        size_t given = 0;
        for (size_t i = 0; i < order.count; ++i) {
            given += order.order[i] < 0;
            if (order.order[i] >= 0) {
                check(size_t(order.order[i]) < c.search.size());
                nd::DnsName domain, joined;
                if (nd::dns_name_from_text(c.search[size_t(order.order[i])], domain)) {
                    (void)nd::dns_name_join(name, domain, joined);
                }
            }
        }
        check(given == 1);
        check(order.count == (info.rooted ? 1 : 1 + c.search.size()));
    }
    return 0;
}
