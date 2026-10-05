//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The agent protocol's answers (draft-miller-ssh-agent) on any bytes: an
// IDENTITIES_ANSWER and a SIGN_RESPONSE, as the agent client reads them.
// What must hold:
//   - a list read holds keys whose blobs parse, their comments printable;
//   - a signature read lies within the answer.
#include "sgcl/net/ssh/agent.h"

#include <cstdint>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    vector<net::ssh::public_key> keys;
    if (net::ssh::detail::read_identities(data, size, keys)) {
        for (auto& k : keys) {
            check(net::ssh::public_key::from_bytes(k.bytes().as_slice()).has_value());
            for (char c : k.comment().view()) {
                check(uint8_t(c) >= 0x20 && uint8_t(c) < 0x7F);
            }
        }
    }
    net::ssh::detail::Bytes sig;
    if (net::ssh::detail::read_sign_response(data, size, sig)) {
        check(sig.size() + 5 <= size);
    }
    return 0;
}
