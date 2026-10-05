//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The objects of RFC 8555 read from any JSON (acme/detail/parse.h): the
// first byte picks the reader (directory, account, order, authorization,
// challenge, renewal information), the rest is the text. What must hold:
// no input crashes or hangs a reader; what a reader takes has what the RFC
// requires (a status of its set, the required URLs, a token for the
// challenges of §8, a window whose start is not after its end), and the
// same text read again gives the same object.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/acme/fuzz/acme_objects_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/acme/detail/parse.h"

#include <cstdint>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::acme::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool known(net::acme::status s) noexcept {
        return uint8_t(s) <= uint8_t(net::acme::status::expired);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    std::string_view input(reinterpret_cast<const char*>(data + 1), size - 1);
    auto j = encoding::json::parse(string(input));
    if (!j) {
        return 0;
    }
    const string url("https://ca.test/acme/x/1");
    switch (data[0] % 6) {
        case 0: {
            auto r = d::parse_directory(*j);
            if (r) {
                check(!r->new_nonce.empty() || (*j)["newNonce"].as_string().has_value());
                auto again = d::parse_directory(*j);
                check(again && again->new_order == r->new_order && again->profiles.size() == r->profiles.size());
            }
            break;
        }
        case 1: {
            auto r = d::parse_account(*j, url);
            if (r) {
                check(known(r->status) && r->url == url);
            }
            break;
        }
        case 2: {
            auto r = d::parse_order(*j, url);
            if (r) {
                check(known(r->status) && (*j)["finalize"].as_string().has_value());
                if (r->error) {
                    check(!r->error->type.empty());
                    (void)r->error->code();
                }
                auto again = d::parse_order(*j, url);
                check(again && again->authorizations.size() == r->authorizations.size() && again->identifiers.size() == r->identifiers.size());
            }
            break;
        }
        case 3: {
            auto r = d::parse_authorization(*j, url);
            if (r) {
                check(known(r->status));
                for (auto& c : r->challenges) {
                    check(known(c.status) && !c.type.empty() && !c.url.empty());
                    if (c.type == "http-01" || c.type == "dns-01" || c.type == "tls-alpn-01") {
                        check(!c.token.empty());
                    }
                }
            }
            break;
        }
        case 4: {
            auto r = d::parse_challenge(*j);
            if (r) {
                check(known(r->status));
            }
            break;
        }
        default: {
            auto r = d::parse_renewal_info(*j);
            if (r) {
                check(r->start.unix_nano() <= r->end.unix_nano());
            }
            break;
        }
    }
    return 0;
}
