//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ping's reader of what an ICMP socket receives, on any bytes
// (libFuzzer's own): the first byte picks IPv4 or IPv6 and a datagram or a
// raw socket, the next two the sequence of the echo sent; its cookie is the
// fixed one below. What must hold: a reply taken has the sequence and the
// cookie, a size within the datagram; nothing is read past the bytes.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/ping_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/ping.h"

#include <cstdint>

namespace {
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const uint8_t Cookie[8] = {1, 2, 3, 4, 5, 6, 7, 8};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    bool v4 = data[0] & 1;
    bool raw = data[0] & 2;
    uint16_t seq = uint16_t(data[1] << 8 | data[2]);
    const uint8_t* p = data + 3;
    size_t n = size - 3;
    auto seen = nd::ping_parse(p, n, v4, raw, 0x1234, seq, Cookie);
    check(seen.bytes <= n);
    if (seen.kind == nd::PingSeen::kind::reply) {
        size_t head = n - seen.bytes;
        const uint8_t* icmp = p + head;
        check(uint16_t(icmp[6] << 8 | icmp[7]) == seq);
        for (int i = 0; i < 8; ++i) {
            check(icmp[8 + i] == Cookie[i]);
        }
    }
    return 0;
}
