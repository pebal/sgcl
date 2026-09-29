//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X.509 on any bytes. The input is a certificate in DER (the seeds are real
// ones: the fuzzer's mutations land in its lengths, tags, times, names and
// extensions). What certificate::parse takes:
//
//   - OpenSSL's d2i_X509 must take too, the whole input, and i2d_X509 must
//     give back the very bytes (the parser here is stricter than OpenSSL's,
//     never laxer);
//   - read twice it gives the same fields, and every accessor runs (the
//     names' text, the key, the extensions), and the certificate verifies
//     or fails with errc::verification and a reason, never an exception,
//     against pools that hold it, a fixed CA of the seeds, or nothing, with
//     a DNS name and an IP address asked;
//   - check_signature_from with itself and with the fixed CA never throws,
//     and is never true unless OpenSSL's X509_verify agrees (a signature
//     this module takes is one OpenSSL takes: never a false positive).
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/x509_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o x509_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./x509_fuzz <seconds> <seed files...>
//
// The seeds: any certificates in DER, one per file (openssl x509 -outform DER).
#include "sgcl/crypto/crypto.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace x509 = crypto::x509;
    using bytes_t = std::vector<unsigned char>;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "x509_fuzz: %s\n", what);
            std::abort();
        }
    }

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    template<class R>
    bytes_t to_bytes(const R& r) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    // Everything a certificate gives, as one text: read twice, the same
    std::string fields(const x509::certificate& c) {
        std::string s;
        auto add = [&](const string& v) {
            s.append(v.data(), v.size());
            s += '|';
        };
        add(c.subject().to_string());
        add(c.issuer().to_string());
        add(c.subject().common_name());
        for (auto& a : c.subject().attributes()) {
            add(a.oid);
            add(a.value);
        }
        s += std::to_string(c.version()) + '|' + std::to_string(c.not_before().unix()) + '|' + std::to_string(c.not_after().unix()) + '|';
        s += std::to_string(int(c.signature_algorithm())) + '|' + std::to_string(int(c.public_key().kind())) + '|';
        add(c.public_key().algorithm());
        s += std::to_string(c.is_ca()) + std::to_string(c.has_basic_constraints()) + std::to_string(c.max_path_length().value_or(-1)) + '|';
        s += std::to_string(int(c.key_usage())) + '|';
        for (auto u : c.ext_key_usages()) {
            s += std::to_string(int(u)) + ',';
        }
        for (auto& v : c.dns_names()) add(v);
        for (auto& v : c.email_addresses()) add(v);
        for (auto& v : c.uris()) add(v);
        for (auto& v : c.policies()) add(v);
        for (auto& v : c.permitted_dns_domains()) add(v);
        for (auto& v : c.excluded_email_addresses()) add(v);
        for (auto& v : c.unhandled_critical_extensions()) add(v);
        s += std::to_string(c.ip_addresses().size()) + std::to_string(c.permitted_ip_ranges().size()) + std::to_string(c.extensions().size());
        s += std::to_string(c.raw_tbs().size()) + std::to_string(c.raw_subject().size()) + std::to_string(c.raw_issuer().size());
        return s;
    }

    // The fixed CA: the first seed that parses as a CA, kept for the run as
    // its bytes (a certificate lives on the stack or in managed memory)
    bytes_t ca_der;

    void verify_all(const x509::certificate& c, const optional<x509::certificate>& ca) {
        const uint8_t v4[] = {192, 0, 2, 7};
        for (int k = 0; k < 3; ++k) {
            x509::verify_options o;
            x509::certificate_pool roots;
            if (k == 1) {
                roots.add(c);
            }
            if (k == 2 && ca) {
                roots.add(*ca);
                o.intermediates.add(c);
            }
            o.roots = roots;
            o.time = time::datetime::from_unix(k == 0 ? c.not_before().unix() : 1700000000, time::zone::utc());
            o.dns_name = k == 1 ? string("www.example.com") : string();
            if (k == 2) {
                o.ip = view(v4, 4);
            }
            try {
                auto r = c.verify(o);
                if (!r) {
                    check(r.error().code() == crypto::errc::verification, "a verification error that is not errc::verification");
                    check(r.error().reason() != x509::reason::none, "a verification error without a reason");
                } else {
                    check(r->size() >= 1 && (*r)[0] == c, "a chain that does not start with the leaf");
                }
            } catch (...) {
                check(false, "verify threw");
            }
        }
        try {
            (void)c.verify_hostname(string("a.example.com"));
            (void)c.verify_ip(view(v4, 4));
        } catch (...) {
            check(false, "a host check threw");
        }
    }

    // Whether OpenSSL takes child's signature under parent's key
    bool openssl_signed_by(X509* child, X509* parent) {
        EVP_PKEY* k = X509_get0_pubkey(parent);
        return k && X509_verify(child, k) == 1;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto c = x509::certificate::parse(view(data, size));
    if (!c) {
        check(c.error().code() == crypto::errc::malformed, "a parse error that is not malformed");
        return 0;
    }
    // OpenSSL takes what this parser takes, all of it, and writes it back as it was
    const unsigned char* p = data;
    X509* o = d2i_X509(nullptr, &p, long(size));
    check(o != nullptr, "a certificate OpenSSL does not read");
    check(size_t(p - data) == size, "OpenSSL reads a certificate of another length");
    unsigned char* out = nullptr;
    int n = i2d_X509(o, &out);
    check(n == int(size) && std::memcmp(out, data, size) == 0, "OpenSSL writes the certificate back otherwise");
    OPENSSL_free(out);

    auto again = x509::certificate::parse(c->raw());
    check(again && fields(*again) == fields(*c), "read twice, two answers");
    check(to_bytes(c->raw()) == bytes_t(data, data + size), "raw() is not the input");

    optional<x509::certificate> ca;
    if (!ca_der.empty()) {
        auto parsed = x509::certificate::parse(view(ca_der.data(), ca_der.size()));
        check(bool(parsed), "the fixed CA no longer parses");
        ca = *parsed;
    }
    verify_all(*c, ca);

    // signatures: never taken where OpenSSL refuses them
    try {
        if (c->check_signature_from(*c)) {
            check(openssl_signed_by(o, o), "a self-signature taken here and refused by OpenSSL");
        }
        if (ca && c->check_signature_from(*ca)) {
            const unsigned char* q = ca_der.data();
            X509* co = d2i_X509(nullptr, &q, long(ca_der.size()));
            check(openssl_signed_by(o, co), "a signature taken here and refused by OpenSSL");
            X509_free(co);
        }
    } catch (...) {
        check(false, "check_signature_from threw");
    }
    if (ca_der.empty() && c->is_ca() && c->public_key().has_value()) {
        ca_der.assign(data, data + size);
    }
    X509_free(o);
    return 0;
}
