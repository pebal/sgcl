//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The fixed key and certificate of the CMS harness (cms_fuzz.cpp) and of
// the program that writes its seeds: a P-256 key of a fixed scalar and its
// self-signed certificate of a fixed serial and validity, so that a seed
// signed or encrypted with them names the same signer and recipient in
// every run.
#pragma once

#include "sgcl/crypto/cms.h"
#include "sgcl/crypto/smime.h"

namespace cms_fuzz {
    using namespace sgcl;
    namespace x509 = sgcl::crypto::x509;

    struct Fixed {
        crypto::p256::private_key key = crypto::p256::private_key::from_bytes(slice<const byte>("a fixed P-256 scalar of 32 bytes")).value();
        x509::certificate cert = make(key);
        x509::certificate_pool roots;

        Fixed() {
            roots.add(cert);
        }

        static x509::certificate make(const crypto::p256::private_key& key) {
            x509::certificate_template t;
            t.common_name = "cms fuzz";
            t.serial_number = vector<byte>{byte(1), byte(2), byte(3)};
            t.not_before = time::datetime::from_unix(1700000000, time::zone::utc());
            t.not_after = time::datetime::from_unix(4000000000, time::zone::utc());
            t.ext_key_usages = {x509::ext_key_usage::email_protection};
            return x509::create_certificate(t, key);
        }

        crypto::cms::verify_options options() const {
            crypto::cms::verify_options o;
            o.chain.roots = roots;
            return o;
        }
    };
}
