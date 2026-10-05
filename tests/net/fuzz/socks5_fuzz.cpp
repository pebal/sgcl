//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::socks5's client handshake (detail::Socks5Handshake) on any bytes from
// the proxy, and the CONNECT request of any target text. The first byte
// picks the credentials (bit 0) and whether the rest is a target to encode
// (bit 1) or the proxy's answer. What must hold:
//   - the handshake never asks for more than a reply can hold (257 bytes)
//     nor for nothing while it goes on, and takes the answer exactly as
//     asked: it never reads past the reply;
//   - it is done only for an answer RFC 1928 allows: VER 5 and a method it
//     offered, RFC 1929's status 0 after username/password, VER 5 and REP 0,
//     an ATYP of 1, 3 or 4 with its address's length; what it sends after
//     the method is the request (or the authentication, then the request);
//   - a failure has an error, one of the codes the module documents;
//   - a target encoded is a CONNECT whose fields read back to the target's
//     host and port.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/socks5_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/socks5.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using sgcl::net::detail::Socks5Handshake;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool documented(error_code e) {
        return e == net::errc::malformed_proxy_response || e == net::errc::proxy_auth_required || e == net::errc::proxy_failure
               || e == net::errc::proxy_refused || e == net::errc::proxy_unsupported || e == std::errc::network_unreachable
               || e == std::errc::host_unreachable || e == std::errc::connection_refused || e == std::errc::timed_out;
    }

    void handshake(bool with_credentials, const uint8_t* p, size_t n) {
        const std::string request = *net::detail::socks5_request("example.com:443");
        const std::string user = "user", pass = "pass";
        Socks5Handshake h(request, with_credentials ? std::string_view(user) : std::string_view(), with_credentials ? std::string_view(pass) : std::string_view());
        std::string greeting = h.greeting();
        check(greeting.size() == (with_credentials ? 4u : 3u) && greeting[0] == 5);
        size_t at = 0;
        std::string out;
        int sent_requests = 0;
        bool sent_auth = false;
        for (;;) {
            size_t want = h.want();
            check(want > 0 && want <= 257);
            if (n - at < want) {
                return;   // the connection would wait: nothing more to say
            }
            auto s = h.feed(p + at, want, out);
            at += want;
            if (s == Socks5Handshake::step::failed) {
                check(documented(h.error()));
                check(h.want() == 0);
                return;
            }
            if (!out.empty()) {
                if (out == request) {
                    ++sent_requests;
                } else {
                    check(with_credentials && !sent_auth && sent_requests == 0);
                    check(out == std::string("\1\4user\4pass", 11));
                    sent_auth = true;
                }
            }
            if (s == Socks5Handshake::step::done) {
                break;
            }
        }
        // done: the answer read back by hand
        check(sent_requests == 1);
        check(p[0] == 5 && (p[1] == 0 || (p[1] == 2 && with_credentials)));
        size_t i = 2;
        if (p[1] == 2) {
            check(sent_auth && p[3] == 0);
            i = 4;
        }
        check(p[i] == 5 && p[i + 1] == 0);
        uint8_t atyp = p[i + 3];
        size_t len = atyp == 1 ? 4 : atyp == 4 ? 16 : atyp == 3 ? size_t(1) + p[i + 4] : 0;
        check(len != 0);
        check(at == i + 4 + len + 2);
        check((bool)h.bound() == (atyp != 3));
        if (h.bound()) {
            check(h.bound()->port() == uint16_t((p[at - 2] << 8) | p[at - 1]));
        }
    }

    void target(std::string_view t) {
        auto r = net::detail::socks5_request(t);
        if (!r) {
            return;
        }
        const std::string& b = *r;
        check(b.size() >= 7 && b[0] == 5 && b[1] == 1 && b[2] == 0);
        size_t colon = t.rfind(':');
        check(colon != std::string_view::npos);
        std::string_view host = t.substr(0, colon);
        unsigned port = 0;
        for (char c : t.substr(colon + 1)) {
            check(c >= '0' && c <= '9');
            port = port * 10 + unsigned(c - '0');
        }
        check(port == ((uint8_t(b[b.size() - 2]) << 8) | uint8_t(b[b.size() - 1])));
        uint8_t atyp = uint8_t(b[3]);
        if (atyp == 3) {
            size_t len = uint8_t(b[4]);
            check(len >= 1 && b.size() == 5 + len + 2);
            check(std::string_view(b).substr(5, len) == host);
        } else if (atyp == 1) {
            check(b.size() == 4 + 4 + 2);
            auto a = net::ip_address::parse(string(host));
            check(a.has_value() && a->is_v4());
        } else {
            check(atyp == 4 && b.size() == 4 + 16 + 2);
            check(host.size() >= 2 && host.front() == '[' && host.back() == ']');
            auto a = net::ip_address::parse(string(host.substr(1, host.size() - 2)));
            check(a.has_value() && a->is_v6() && !a->has_zone());
            auto bytes = a->bytes();
            for (size_t k = 0; k < 16; ++k) {
                check(uint8_t(b[4 + k]) == bytes[k]);
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    uint8_t mode = data[0];
    if (mode & 2) {
        target(std::string_view(reinterpret_cast<const char*>(data + 1), size - 1));
    } else {
        handshake((mode & 1) != 0, data + 1, size - 1);
    }
    return 0;
}
