//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A CRL (RFC 5280 §5) on any bytes, the input in DER (the seeds are `openssl
// ca -gencrl`'s lists of tests/crypto/data/revocation, a complete list and
// its delta). What revocation_list::parse takes:
//
//   - OpenSSL's d2i_X509_CRL takes too, the whole input, with as many
//     entries (stricter here, never laxer);
//   - every entry reads (operator[]), and lookup of each entry's serial
//     finds an entry of that serial;
//   - status_of against the fixtures (good, revoked, the intermediate
//     under the root), with the list as a delta of itself too, and
//     check_signature_from never throw: a status, or errc::verification
//     with a reason; revoked or good only when check_signature_from of the
//     issuer passes.
//
// A parse that fails is errc::malformed. A disagreement aborts; ASan and
// UBSan catch the rest.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/crl_fuzz.cpp 300
#include "revocation_fuzz_common.h"

#include <openssl/x509.h>

using namespace revocation_fuzz;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto r = x509::revocation_list::parse(view(data, size));
    if (!r) {
        check(r.error().code() == crypto::errc::malformed, "a parse error that is not malformed");
        return 0;
    }
    const unsigned char* p = data;
    X509_CRL* theirs = d2i_X509_CRL(nullptr, &p, long(size));
    check(theirs != nullptr && p == data + size, "a CRL OpenSSL does not read whole");
    auto* entries = X509_CRL_get_REVOKED(theirs);
    check(size_t(entries ? sk_X509_REVOKED_num(entries) : 0) == r->size(), "another number of entries than OpenSSL's");
    X509_CRL_free(theirs);
    for (size_t i = 0; i < r->size(); ++i) {
        auto e = (*r)[i];
        auto found = r->lookup(e.serial_number);
        check(found.has_value() && found->serial_number.size() == e.serial_number.size(), "an entry's serial not found");
        (void)e.revocation_time.unix();
    }
    (void)r->issuer().to_string();
    (void)r->next_update();
    static const rooted<x509::certificate> good(std::in_place, fixture("good"));
    static const rooted<x509::certificate> revoked(std::in_place, fixture("revoked"));
    static const rooted<x509::certificate> issuer(std::in_place, fixture("int"));
    static const rooted<x509::certificate> root(std::in_place, fixture("root"));
    const std::pair<const x509::certificate*, const x509::certificate*> pairs[] = {{good.get(), issuer.get()}, {revoked.get(), issuer.get()}, {issuer.get(), root.get()}};
    const auto t = time::datetime::from_unix(1800000000, time::zone::utc());
    for (auto [cert, iss] : pairs) {
        auto s = r->status_of(*cert, *iss, t);
        if (!s) {
            verification_error(s.error());
        } else {
            check(r->check_signature_from(*iss).has_value(), "a status of a list its issuer did not sign");
        }
        auto d = r->status_of(*cert, *iss, *r, t);
        if (!d) {
            verification_error(d.error());
        }
        auto sig = r->check_signature_from(*iss);
        if (!sig) {
            verification_error(sig.error());
        }
    }
    return 0;
}
