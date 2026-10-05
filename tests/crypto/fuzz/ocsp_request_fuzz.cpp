//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The OCSP request (RFC 6960 §4.1): a request read from any bytes, and the
// builder's round trip. The first byte chooses:
//
//   - even: the rest is a request in DER (the seeds are `openssl ocsp
//     -reqout`'s): what ocsp_request::parse takes, OpenSSL's
//     d2i_OCSP_REQUEST takes whole, with one request in it; read twice the
//     same; matches() never throws; url() of the rest's first bytes as a
//     responder gives a URL whose part after it is the base64 of the DER,
//     escaped;
//   - odd: the rest chooses a hash (of SHA-1 and SHA-2) and a nonce; the
//     request made for a fixture and its issuer reads back with the same
//     CertID and nonce, OpenSSL reads it, and it matches that certificate
//     and no other.
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/ocsp_request_fuzz.cpp 300
#include "revocation_fuzz_common.h"

#include <openssl/ocsp.h>

using namespace revocation_fuzz;

namespace {
    void openssl_reads(const slice<const byte>& der, int requests) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(der.data());
        OCSP_REQUEST* theirs = d2i_OCSP_REQUEST(nullptr, &p, long(der.size()));
        check(theirs != nullptr && p == reinterpret_cast<const unsigned char*>(der.data()) + der.size(), "a request OpenSSL does not read whole");
        check(OCSP_request_onereq_count(theirs) == requests, "another number of requests than OpenSSL's");
        OCSP_REQUEST_free(theirs);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    static const rooted<x509::certificate> good(std::in_place, fixture("good"));
    static const rooted<x509::certificate> revoked(std::in_place, fixture("revoked"));
    static const rooted<x509::certificate> issuer(std::in_place, fixture("int"));
    if (data[0] % 2 == 0) {
        auto r = x509::ocsp_request::parse(view(data + 1, size - 1));
        if (!r) {
            check(r.error().code() == crypto::errc::malformed, "a parse error that is not malformed");
            return 0;
        }
        openssl_reads(r->raw(), 1);
        auto again = x509::ocsp_request::parse(view(data + 1, size - 1));
        check(again.has_value() && again->serial_number() == r->serial_number() && again->nonce() == r->nonce(), "read twice, not the same");
        (void)r->matches(*good, *issuer);
        size_t n = std::min<size_t>(size - 1, 40);
        std::string responder(reinterpret_cast<const char*>(data + 1), n);
        string u = r->url(string(responder));
        check(u.size() >= responder.size(), "a URL shorter than its responder");
        return 0;
    }
    static constexpr crypto::hash_id hashes[] = {crypto::hash_id::sha1, crypto::hash_id::sha256, crypto::hash_id::sha384, crypto::hash_id::sha512};
    uint8_t choice = size > 1 ? data[1] : 0;
    x509::ocsp_request_options o;
    o.hash = hashes[choice % 4];
    o.nonce = (choice & 4) != 0;
    const x509::certificate& cert = (choice & 8) ? *revoked : *good;
    auto r = x509::ocsp_request::make(cert, *issuer, o);
    check(r.has_value(), "a request of a fixture not made");
    openssl_reads(r->raw(), 1);
    auto back = x509::ocsp_request::parse(r->raw());
    check(back.has_value(), "a request made that does not read back");
    check(back->hash() == o.hash && back->serial_number() == cert.serial_number() && back->issuer_name_hash() == r->issuer_name_hash()
              && back->issuer_key_hash() == r->issuer_key_hash() && back->nonce() == r->nonce() && back->nonce().size() == (o.nonce ? 16u : 0u),
          "a request made that reads back otherwise");
    check(back->matches(cert, *issuer) && !back->matches((choice & 8) ? *good : *revoked, *issuer), "a request that matches another certificate");
    return 0;
}
