//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's keys on any bytes: the first byte picks a private key file
// (OpenSSH's format or PEM, with an empty passphrase or "pw"), a public key
// line, or a key blob and its certificate. What must hold:
//   - a private key read has a public half whose blob parses, signs data
//     that verifies under it, and writes itself back in OpenSSH's format to
//     a file that reads again to the same public key;
//   - a public key line read writes back as a line that reads to the same
//     key; its fingerprint is "SHA256:" and 43 characters;
//   - a blob read is a key; its certificate, when it has one that verifies,
//     certifies a plain key and names a plain signature key.
// The key file's KDF rounds are bounded by the reader itself (no more than
// 2^16), so an input cannot make a run take long: a bcrypt round of a large
// count is refused before it is made.
#include "sgcl/net/ssh/keys.h"

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
    if (size < 1) {
        return 0;
    }
    const uint8_t mode = data[0];
    const slice<const byte> rest(reinterpret_cast<const byte*>(data + 1), size - 1);
    switch (mode % 3) {
        case 0: {
            auto k = net::ssh::private_key::parse(rest, (mode & 4) ? string("pw") : string());
            if (!k) {
                return 0;
            }
            auto pub = k->public_key();
            check((bool)pub && !pub.is_certificate());
            auto sig = k->sign(slice<const byte>("fuzz"));
            check(pub.verify(slice<const byte>("fuzz"), sig.as_slice()));
            auto again = net::ssh::private_key::parse(k->to_openssh().as_slice());
            check(again && again->public_key() == pub);
            return 0;
        }
        case 1: {
            auto k = net::ssh::public_key::parse(string(std::string_view(reinterpret_cast<const char*>(data + 1), size - 1)));
            if (!k) {
                return 0;
            }
            auto back = net::ssh::public_key::parse(k->to_string());
            check(back && *back == *k);
            auto fp = k->fingerprint();
            check(fp.size() == 50 && fp.view().substr(0, 7) == "SHA256:");
            return 0;
        }
        default: {
            auto k = net::ssh::public_key::from_bytes(rest);
            if (!k) {
                return 0;
            }
            check(k->bytes().size() == rest.size());
            if (auto c = k->certificate()) {
                check(k->is_certificate());
                check(!c->key.is_certificate() && !c->signature_key.is_certificate());
                check(c->type == net::ssh::certificate_type::user || c->type == net::ssh::certificate_type::host);
            }
            return 0;
        }
    }
}
