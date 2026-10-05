//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Certificate requests (PKCS #10) both ways on any bytes: the input read as
// a request's DER (certificate_request::parse), and the input made the
// fields of a request (a common name, DNS names, addresses, emails, URIs, an
// extension of its own) that create_certificate_request writes, signed by a
// key the first byte picks. What must hold: nothing crashes or hangs; a
// request read gives its bytes back and reads again the same; a request
// written either is refused whole (std::invalid_argument: a name that is
// not ASCII, an OID that is not one) or reads back with every field as
// given and a signature that verifies under its own key.
// Built with libFuzzer (tests/fuzz/run.sh tests/crypto/fuzz/x509_request_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/crypto/x509.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace x509 = sgcl::crypto::x509;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The pieces of the input split at NULs
    vector<string> pieces(std::string_view v) {
        vector<string> out;
        size_t at = 0;
        while (at <= v.size() && out.size() < 16) {
            size_t z = v.find('\0', at);
            out.push_back(string(v.substr(at, z == std::string_view::npos ? std::string_view::npos : z - at)));
            if (z == std::string_view::npos) {
                break;
            }
            at = z + 1;
        }
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // read
    auto r = x509::certificate_request::parse(slice<const byte>(reinterpret_cast<const byte*>(data), size));
    if (r) {
        check(r->raw().size() == size);
        auto again = x509::certificate_request::parse(r->raw());
        check(again && *again == *r && again->dns_names().size() == r->dns_names().size());
        (void)r->check_signature();
    }
    if (size < 2) {
        return 0;
    }
    // written
    auto p = pieces(std::string_view(reinterpret_cast<const char*>(data + 1), size - 1));
    x509::certificate_request_template t;
    t.common_name = p[0];
    for (size_t i = 1; i < p.size(); ++i) {
        switch (i % 5) {
            case 0: t.dns_names.push_back(p[i]); break;
            case 1: t.email_addresses.push_back(p[i]); break;
            case 2: t.uris.push_back(p[i]); break;
            case 3: {
                x509::ip_address a;
                a.size = p[i].size() >= 16 ? 16 : 4;
                for (size_t k = 0; k < a.size && k < p[i].size(); ++k) {
                    a.bytes[k] = byte(p[i].data()[k]);
                }
                t.ip_addresses.push_back(a);
                break;
            }
            default: {
                x509::extension e;
                e.oid = p[i];
                e.value = {byte(0x05), byte(0x00)};
                t.extensions.push_back(e);
                break;
            }
        }
    }
    try {
        optional<x509::certificate_request> made;
        if (data[0] % 2) {
            auto k = crypto::ed25519::private_key::generate();
            made = x509::create_certificate_request(t, k);
        } else {
            auto k = crypto::p256::private_key::generate();
            made = x509::create_certificate_request(t, k);
        }
        check(made->check_signature().has_value());
        check(made->dns_names().size() == t.dns_names.size() && made->email_addresses().size() == t.email_addresses.size());
        check(made->uris().size() == t.uris.size() && made->ip_addresses().size() == t.ip_addresses.size());
        for (size_t i = 0; i < t.dns_names.size(); ++i) {
            check(made->dns_names()[i] == t.dns_names[i]);
        }
        check(made->subject().common_name() == t.common_name);
        auto back = x509::certificate_request::parse(made->raw());
        check(back && *back == *made);
    } catch (const std::invalid_argument&) {
        // a template that cannot be written: refused whole
    }
    return 0;
}
