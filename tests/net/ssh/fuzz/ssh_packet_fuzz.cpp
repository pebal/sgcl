//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's binary packets (detail::PacketKeys) on any bytes, under each
// cipher and MAC: the first byte picks them (and whether the rest is a
// stream to open or a payload to seal), the keys are fixed. What must hold:
//   - a stream of any bytes is taken apart packet by packet, each length
//     refused or within MaxPacketLength and the block, each packet opened
//     or refused (a tag, a MAC, a padding), never read past;
//   - a payload sealed opens to itself under the same keys and sequence
//     number, and its length is aligned as RFC 4253 §6 asks.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/ssh/fuzz/ssh_packet_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/ssh/detail/cipher.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl::net::ssh::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void keys_for(PacketKeys& k, int cipher, int mac) {
        uint8_t key[64], iv[16], mk[64];
        for (int i = 0; i < 64; ++i) {
            key[i] = uint8_t(i + 1);
            mk[i] = uint8_t(i * 5);
        }
        for (int i = 0; i < 16; ++i) {
            iv[i] = uint8_t(i * 9);
        }
        if (cipher < 0) {
            return;   // none
        }
        k.set(cipher_table[cipher].kind, key, iv, mac_table[mac].kind, mk);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const int ncipher = int(std::size(cipher_table)) + 1;
    const int cipher = int(data[0] % ncipher) - 1;
    const int mac = (data[0] / ncipher) % int(std::size(mac_table));
    const bool seal = (data[0] & 0x80) != 0;
    const uint8_t* p = data + 1;
    size_t n = size - 1;
    if (seal) {
        PacketKeys a, b;
        keys_for(a, cipher, mac);
        keys_for(b, cipher, mac);
        uint32_t seq = n ? p[0] : 0;
        Bytes out;
        a.seal(seq, p, n, out);
        uint32_t len;
        size_t total;
        check(b.length(seq, out.data(), len, total) == OpenError::none);
        check(total == out.size());
        size_t at, m;
        check(b.open(seq, out.data(), len, at, m) == OpenError::none);
        check(m == n && (n == 0 || std::memcmp(out.data() + at, p, n) == 0));
        const size_t aligned = b.length_aligned() ? 4 + len : len;
        check(aligned % b.block() == 0);
        return 0;
    }
    PacketKeys k;
    keys_for(k, cipher, mac);
    std::vector<uint8_t> buf(p, p + n);
    size_t at = 0;
    for (uint32_t seq = 0; at < buf.size(); ++seq) {
        const size_t head = k.head_size();
        if (buf.size() - at < head) {
            break;
        }
        uint32_t len;
        size_t total;
        if (k.length(seq, buf.data() + at, len, total) != OpenError::none) {
            break;
        }
        check(len <= MaxPacketLength && total == 4 + size_t(len) + k.tag_size());
        if (buf.size() - at < total) {
            break;
        }
        size_t pa, pn;
        if (k.open(seq, buf.data() + at, len, pa, pn) != OpenError::none) {
            break;
        }
        check(pa == 5 && pn + 1 + 4 <= size_t(len) + 1);
        at += total;
    }
    return 0;
}
