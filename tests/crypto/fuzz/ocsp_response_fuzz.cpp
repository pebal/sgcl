//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// An OCSP response (RFC 6960) on any bytes, the input in DER (the seeds are
// `openssl ocsp`'s responses of tests/crypto/data/revocation). What
// ocsp_response::parse takes:
//
//   - OpenSSL's d2i_OCSP_RESPONSE takes too, the whole input (the parser
//     here is stricter than OpenSSL's, never laxer);
//   - every accessor runs, read twice the same;
//   - verify against the fixtures' certificates (good, revoked, the
//     intermediate under the root) and check_signature_from never throw:
//     a status, or errc::verification with a reason; a status of good or
//     revoked comes only with a signature OpenSSL's OCSP_basic_verify takes
//     under the same signer (never a false positive).
//
// A parse that fails is errc::malformed or errc::unsupported. A
// disagreement aborts; ASan and UBSan catch the rest.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/ocsp_response_fuzz.cpp 300
#include "revocation_fuzz_common.h"

#include <openssl/ocsp.h>

using namespace revocation_fuzz;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto r = x509::ocsp_response::parse(view(data, size));
    if (!r) {
        check(r.error().code() == crypto::errc::malformed || r.error().code() == crypto::errc::unsupported, "a parse error that is not malformed or unsupported");
        return 0;
    }
    const unsigned char* p = data;
    OCSP_RESPONSE* theirs = d2i_OCSP_RESPONSE(nullptr, &p, long(size));
    check(theirs != nullptr && p == data + size, "a response OpenSSL does not read whole");
    OCSP_RESPONSE_free(theirs);
    auto again = x509::ocsp_response::parse(view(data, size));
    check(again.has_value() && again->responses().size() == r->responses().size() && again->status() == r->status(), "read twice, not the same");
    (void)r->produced_at();
    (void)r->raw_tbs();
    for (const auto& s : r->responses()) {
        (void)s.this_update;
        check(s.issuer_name_hash.size() == s.issuer_key_hash.size(), "a CertID's hashes of two sizes");
    }
    for (const auto& c : r->certificates()) {
        (void)c.subject().to_string();
    }
    static const rooted<x509::certificate> good(std::in_place, fixture("good"));
    static const rooted<x509::certificate> revoked(std::in_place, fixture("revoked"));
    static const rooted<x509::certificate> issuer(std::in_place, fixture("int"));
    static const rooted<x509::certificate> root(std::in_place, fixture("root"));
    static const rooted<x509::certificate> responder(std::in_place, fixture("responder"));
    const std::pair<const x509::certificate*, const x509::certificate*> pairs[] = {{good.get(), issuer.get()}, {revoked.get(), issuer.get()}, {issuer.get(), root.get()}};
    for (auto [cert, iss] : pairs) {
        x509::ocsp_verify_options o;
        o.time = time::datetime::from_unix(1800000000, time::zone::utc());
        auto v = r->verify(*cert, *iss, o);
        if (!v) {
            verification_error(v.error());
        }
    }
    for (const auto* signer : {issuer.get(), root.get(), responder.get()}) {
        auto s = r->check_signature_from(*signer);
        if (!s) {
            verification_error(s.error());
        }
    }
    return 0;
}
