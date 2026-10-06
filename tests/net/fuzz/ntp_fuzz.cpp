//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ntp's reader of a server's answer on any bytes (libFuzzer's own, their
// end the input's): the first 8 bytes the transmit timestamp the query sent,
// the rest the answer. What must hold: an answer taken echoes that
// timestamp as its originate, is a server's (mode 4) of a stratum 1 to 15,
// and its delay is not negative; a refusal is one of the module's errors.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/ntp_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/ntp.h"

#include <cstdint>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::ntp::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return 0;
    }
    uint64_t transmit = nd::ntp_u64(data);
    int64_t t1 = int64_t(1700000000) * 1000000000;
    auto r = nd::ntp_read(data + 8, size - 8, transmit, t1, t1 + 1000000, string("fuzz"));
    if (r) {
        check(size - 8 >= 48 && nd::ntp_u64(data + 8 + 24) == transmit);
        check((data[8] & 7) == 4 && r->stratum >= 1 && r->stratum <= 15);
        check(r->delay.nanoseconds() >= 0);
    } else {
        check(r.error().code().category() == net::ntp::category());
    }
    // the conversions both ways near any timestamp
    int64_t back = nd::ntp_to_unix_nanos(nd::ntp_from_unix_nanos(t1), t1);
    check(back == t1);
    return 0;
}
