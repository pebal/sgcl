//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's known_hosts and authorized_keys on any text. What must hold:
//   - known_hosts read gives a set whose text (to_string) reads again to a
//     set of as many lines that answers check() the same for every key it
//     holds, under the hosts the input's first line names;
//   - authorized_keys read finds every key it holds (a cert-authority
//     line's key excepted), and allows() never throws.
#include "sgcl/net/ssh/authorized_keys.h"
#include "sgcl/net/ssh/known_hosts.h"

#include <cstdint>
#include <string>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view text(reinterpret_cast<const char*>(data), size);
    auto kh = net::ssh::known_hosts::parse(string(text));
    auto again = net::ssh::known_hosts::parse(kh.to_string());
    check(again.size() == kh.size());
    std::string_view first = text.substr(0, std::min<size_t>(text.find('\n'), 64));
    auto keys = net::ssh::authorized_keys::parse(string(text));
    // every key of the set, checked under a host of the input and a fixed one
    auto lines = kh.to_string();
    std::string_view lv = lines.view();
    size_t from = 0;
    while (from < lv.size()) {
        size_t nl = lv.find('\n', from);
        std::string_view line = lv.substr(from, nl - from);
        size_t key_at = line.find(" ssh-") != std::string_view::npos ? line.find(" ssh-") : line.find(" ecdsa-");
        if (key_at != std::string_view::npos) {
            auto k = net::ssh::public_key::parse(string(line.substr(key_at + 1)));
            if (k) {
                for (auto host : {std::string(first), std::string("example.com:22")}) {
                    auto a = kh.check(string(host), *k);
                    auto b = again.check(string(host), *k);
                    check(a.has_value() == b.has_value());
                    if (!a) {
                        check(a.error().code() == b.error().code());
                    }
                }
                (void)keys.allows(string("user"), *k);
                (void)keys.find(*k);
            }
        }
        from = nl == std::string_view::npos ? lv.size() : nl + 1;
    }
    return 0;
}
