//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::socks5::server's handshake (the readers of detail, without I/O) on
// any bytes from a client: the greeting, RFC 1929's request when the first
// byte's bit 0 asks the server for credentials, then the request; and a
// UDP datagram's head (bit 1). Each reader runs on libFuzzer's own bytes
// (their end the input's). What must hold: a message taken is whole and no
// longer than the bytes; a method chosen is one offered or none (0xFF); a
// target read is "host:port" with a port and a host without white space;
// an address read back to bytes and read again is the same target.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/socks5_server_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/socks5.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void check_target(const std::string& target) {
        auto hp = nd::split_host_port(target);
        check(bool(hp) && !hp->host.empty() && bool(nd::parse_port(hp->port)));
        for (char c : hp->host) {
            check(uint8_t(c) > ' ');
        }
        // an address target written back as the protocol writes it reads the same
        auto e = net::endpoint::parse(string(target));
        if (e) {
            std::string bytes;
            nd::socks5_put_endpoint(bytes, *e);
            std::string again;
            size_t used = 0;
            check(nd::socks5_read_address(bytes, again, used) == 1 && used == bytes.size());
            auto back = net::endpoint::parse(string(again)).value();
            check(back.port() == e->port());
            check(e->address().is_v4_mapped() || back == *e);   // a v4-mapped address goes out as IPv4: ATYP 1
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    bool user_pass = data[0] & 1;
    std::string_view b(reinterpret_cast<const char*>(data + 1), size - 1);
    if (data[0] & 2) {
        std::string target;
        size_t used = 0;
        if (b.size() >= 3 && nd::socks5_read_address(b.substr(3), target, used) == 1) {
            check(used <= b.size() - 3);
            check_target(target);
        }
        return 0;
    }
    size_t used = 0;
    uint8_t method = 0;
    int r = nd::socks5_take_greeting(b, user_pass, used, method);
    if (r <= 0) {
        return 0;
    }
    check(used <= b.size());
    check(method == 0xFF || std::string_view(b.data() + 2, used - 2).find(char(method)) != std::string_view::npos);
    b.remove_prefix(used);
    if (method == 0xFF) {
        return 0;
    }
    if (method == 0x02) {
        std::string user, password;
        r = nd::socks5_take_userpass(b, used, user, password);
        if (r <= 0) {
            return 0;
        }
        check(used <= b.size() && user.size() <= 255 && password.size() <= 255);
        b.remove_prefix(used);
    }
    uint8_t cmd = 0;
    std::string target;
    r = nd::socks5_take_request(b, used, cmd, target);
    if (r == 1) {
        check(used <= b.size());
        check_target(target);
    }
    return 0;
}
