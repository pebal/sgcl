//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The public suffix list's lookup on any bytes as a host
// (http::public_suffix, registrable_domain, is_public_suffix), both rule
// sets. What must hold:
//   - the suffix is "" or the end of the host as compared (lower case,
//     A-labels, no dot at the end) at a label boundary, of whole labels;
//   - the registrable domain is "" or the suffix and one label before it,
//     and its own registrable domain is itself;
//   - is_public_suffix says the suffix is the whole host;
//   - a host and the same in upper case, or with a dot at its end, have
//     the same answers.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/public_suffix_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/public_suffix.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void one(std::string_view host, net::http::suffix_rules rules) {
        string h(host);
        string s = net::http::public_suffix(h, rules);
        string r = net::http::registrable_domain(h, rules);
        bool p = net::http::is_public_suffix(h, rules);
        auto norm = net::http::detail::psl_host(host);
        if (!norm) {
            check(s.empty() && r.empty() && !p);
            return;
        }
        std::string_view n = *norm;
        check(!s.empty());
        check(s.size() <= n.size() && n.substr(n.size() - s.size()) == s.view());
        check(s.size() == n.size() || n[n.size() - s.size() - 1] == '.');
        check(s.view().front() != '.' && s.view().back() != '.');
        check(p == (s.size() == n.size()));
        if (r.empty()) {
            check(p);
        } else {
            check(!p);
            check(r.size() > s.size() + 1 && r.view().substr(r.size() - s.size()) == s.view() && r.view()[r.size() - s.size() - 1] == '.');
            auto label = r.view().substr(0, r.size() - s.size() - 1);
            check(!label.empty() && label.find('.') == std::string_view::npos);
            check(n.size() >= r.size() && n.substr(n.size() - r.size()) == r.view());
            check(net::http::registrable_domain(r, rules) == r);
        }
        if (host.size() < 4096) {
            std::string upper(host);
            for (auto& c : upper) {
                c = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
            }
            check(net::http::public_suffix(string(upper), rules) == s);
            if (host.empty() || host.back() != '.') {
                std::string dotted = std::string(host) + ".";
                check(net::http::registrable_domain(string(dotted), rules) == r);
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view host(reinterpret_cast<const char*>(data), size);
    one(host, net::http::suffix_rules::all);
    one(host, net::http::suffix_rules::icann);
    return 0;
}
