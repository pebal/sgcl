//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The HTTP client's proxies on any bytes: what a proxy answered a CONNECT
// (detail::read_connect_answer, fed in pieces as a connection gives them),
// a proxy's URL (detail::parse_proxy_url) and NO_PROXY's list against a
// host (detail::no_proxy_matches). The first byte picks the path (its low
// two bits) and, for the answer, how the bytes are cut. What must hold:
//   - an answer is "more" exactly until a whole head is there (or the
//     limit is reached), and the same whichever pieces it came in; a tunnel
//     is up only for a 2xx whose head is all of the bytes; a 407 is
//     credentials wanted, another status refused; a status is 100 to 999;
//   - a proxy's URL read is of a known kind, with a host and a port not 0;
//     its key and its shown form parse as URLs again to the same host and
//     port, and the shown form holds no credentials;
//   - NO_PROXY: "*" matches every host; a list matches a host when one of
//     its entries does alone; a list never matches the empty host by a
//     name; adding an entry never takes a match away.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/http_proxy_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/proxy.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void answer(uint8_t mode, std::string_view bytes) {
        const size_t limit = 64 + (mode >> 2) * 16;
        auto whole = read_connect_answer(bytes, limit);
        // fed as a connection gives it: pieces of 1 + (mode >> 4) bytes,
        // looked at after each, stopped at the first answer
        size_t step = 1 + (mode >> 4);
        ConnectAnswer last;
        size_t fed = 0;
        while (fed < bytes.size()) {
            fed = std::min(bytes.size(), fed + step);
            last = read_connect_answer(bytes.substr(0, fed), limit);
            if (last.result != ConnectAnswer::kind::more) {
                break;
            }
        }
        if (bytes.empty()) {
            check(whole.result == ConnectAnswer::kind::more);
            return;
        }
        size_t end = find_head_end(bytes.data(), bytes.size());
        if (whole.result == ConnectAnswer::kind::more) {
            check(end == 0 && bytes.size() < limit);
            check(last.result == ConnectAnswer::kind::more);
            return;
        }
        if (last.result != ConnectAnswer::kind::more && last.head) {
            check(last.head <= fed);
            // what was decided on a prefix is the head of the whole as well
            check(last.head == end);
        }
        switch (whole.result) {
            case ConnectAnswer::kind::established:
                check(whole.status >= 200 && whole.status < 300);
                check(whole.head == bytes.size() && end == bytes.size());
                break;
            case ConnectAnswer::kind::auth_required:
                check(whole.status == 407 && whole.head == end);
                break;
            case ConnectAnswer::kind::refused:
                check(whole.status >= 300 && whole.status <= 999 && whole.status != 407);
                break;
            case ConnectAnswer::kind::malformed:
                check(whole.status == 0 || (whole.status >= 100 && whole.status <= 999));
                break;
            default:
                check(false);
        }
    }

    void proxy_url(std::string_view text) {
        auto p = parse_proxy_url(string(text));
        if (!p) {
            check(p.error().code() == net::errc::invalid_url || p.error().code() == net::errc::unsupported_scheme);
            return;
        }
        check(p->port != 0 && !p->host.empty());
        check(p->kind == ProxyKind::http || p->kind == ProxyKind::https || p->kind == ProxyKind::socks5 || p->kind == ProxyKind::socks5h);
        auto again = parse_proxy_url(p->key);
        check(again.has_value());
        check(again->kind == p->kind && again->host == p->host && again->port == p->port);
        check(again->username == p->username && again->password == p->password);
        auto shown = parse_proxy_url(p->shown);
        check(shown.has_value() && !shown->has_credentials());
        check(shown->host == p->host && shown->port == p->port);
    }

    void no_proxy(std::string_view input) {
        // the list, then a NUL, then the host; the port from the host's length
        size_t nul = input.find('\0');
        std::string_view list = input.substr(0, nul);
        std::string_view host = nul == std::string_view::npos ? std::string_view("example.com") : input.substr(nul + 1);
        uint16_t port = uint16_t(80 + host.size() % 3);
        bool all = no_proxy_matches(list, host, port);
        check(no_proxy_matches("*", host, port));
        // one entry matching alone is a match of the list, and the list
        // matches only when one does
        bool any = false;
        size_t i = 0;
        while (i < list.size()) {
            while (i < list.size() && (list[i] == ',' || list[i] == ' ' || list[i] == '\t')) {
                ++i;
            }
            size_t j = i;
            while (j < list.size() && list[j] != ',' && list[j] != ' ' && list[j] != '\t') {
                ++j;
            }
            if (j > i && no_proxy_matches(list.substr(i, j - i), host, port)) {
                any = true;
            }
            i = j;
        }
        check(all == any);
        // an entry more never takes a match away
        std::string more(list);
        more += ",unmatched.invalid";
        check(!all || no_proxy_matches(more, host, port));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode & 3) {
        case 0:
        case 1:
            answer(mode, rest);
            break;
        case 2:
            proxy_url(rest);
            break;
        default:
            no_proxy(rest);
            break;
    }
    return 0;
}
