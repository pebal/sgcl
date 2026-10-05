//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's version exchange (detail::scan_version) on any bytes, as a
// client (lines before the version passed over) and as a server, the bytes
// given whole or in pieces of the sizes the input's first byte names. What
// must hold:
//   - the result is the same however the bytes are cut;
//   - a version taken starts with "SSH-2.0-" or "SSH-1.99-", is printable
//     ASCII, at most 255 bytes, and what was taken ends with its LF;
//   - nothing is read past the bytes given.
#include "sgcl/net/ssh/detail/conn.h"

#include <cstdint>
#include <string>

namespace {
    using namespace sgcl::net::ssh::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Outcome {
        VersionScan r = VersionScan::more;
        std::string version;
        size_t used = 0;
    };

    Outcome whole(const uint8_t* p, size_t n, bool client) {
        Outcome o;
        size_t lines = 0;
        const char* why = nullptr;
        o.r = scan_version(p, n, client, lines, o.used, o.version, why);
        if (o.r == VersionScan::failed) {
            check(why != nullptr);
        }
        return o;
    }

    Outcome pieces(const uint8_t* p, size_t n, bool client, size_t step) {
        Outcome o;
        size_t lines = 0, have = 0, taken = 0;
        while (true) {
            have = std::min(n, have + step);
            size_t used = 0;
            const char* why = nullptr;
            o.r = scan_version(p + taken, have - taken, client, lines, used, o.version, why);
            taken += used;
            if (o.r != VersionScan::more || have == n) {
                break;
            }
        }
        o.used = taken;
        return o;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const bool client = data[0] & 1;
    const size_t step = 1 + (data[0] >> 1);
    const uint8_t* p = data + 1;
    const size_t n = size - 1;
    Outcome a = whole(p, n, client);
    Outcome b = pieces(p, n, client, step);
    // a line past 255 bytes with no LF in the first piece may fail sooner
    // whole than in pieces only by its length: both fail or both agree
    if (a.r == VersionScan::done) {
        check(b.r == VersionScan::done && b.version == a.version && b.used == a.used);
        check(a.version.rfind("SSH-2.0-", 0) == 0 || a.version.rfind("SSH-1.99-", 0) == 0);
        check(a.version.size() <= 255);
        for (char c : a.version) {
            check(uint8_t(c) >= 0x20 && uint8_t(c) <= 0x7E);
        }
        check(a.used >= 1 && a.used <= n && p[a.used - 1] == '\n');
    }
    if (b.r == VersionScan::done) {
        check(a.r == VersionScan::done);
    }
    return 0;
}
