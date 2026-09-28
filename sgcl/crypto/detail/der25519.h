//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "der.h"
#include "../error.h"
#include "../secure_zero.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// The DER forms of the keys of X25519 and Ed25519 (RFC 8410): a public key
// in a SubjectPublicKeyInfo (RFC 5280 §4.1.2.7), a private key in a
// PKCS #8 PrivateKeyInfo / OneAsymmetricKey (RFC 5208, RFC 5958), each key
// 32 bytes and the algorithm an OID with no parameters:
//
//   SubjectPublicKeyInfo ::= SEQUENCE { SEQUENCE { OID }, BIT STRING (0 unused bits, key) }
//   OneAsymmetricKey     ::= SEQUENCE { INTEGER 0 or 1, SEQUENCE { OID },
//                                       OCTET STRING { OCTET STRING key },
//                                       [0] attributes OPTIONAL, [1] public key OPTIONAL (version 1 only) }
//
// Written as OpenSSL and Go write them: the private form at version 0 with
// neither of the optional fields. Read strictly as DER: definite, minimal
// lengths, nothing after the outer SEQUENCE, the OID one of the two, no
// parameters; attributes are skipped; a public key beside the private one
// must be the one the private key gives (the caller checks). Read with
// the module's one DER reader (der.h).
namespace sgcl::crypto::detail {
    // the OID's body: 1.3.101.110 (X25519) or 1.3.101.112 (Ed25519)
    inline constexpr unsigned char oid_x25519 = 0x6e;
    inline constexpr unsigned char oid_ed25519 = 0x70;

    inline vector<byte> der_bytes(const unsigned char* p, size_t n) {
        vector<byte> out(n);
        std::memcpy(out.data(), p, n);
        return out;
    }

    inline vector<byte> der_pkix(unsigned char oid, const unsigned char* key) {
        unsigned char d[44] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, oid, 0x03, 0x21, 0x00};
        std::memcpy(d + 12, key, 32);
        return der_bytes(d, sizeof d);
    }

    // the private key's 32 bytes are a secret: the stack copy is zeroed,
    // the vector returned is the caller's to keep or clear
    inline vector<byte> der_pkcs8(unsigned char oid, const unsigned char* key) {
        unsigned char d[48] = {0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b,
                               0x65, oid, 0x04, 0x22, 0x04, 0x20};
        std::memcpy(d + 16, key, 32);
        vector<byte> out = der_bytes(d, sizeof d);
        secure_zero(d, sizeof d);
        return out;
    }

    inline error der_malformed(const DerReader& at, const char* what) {
        return error(errc::malformed, uint64_t(at.offset()), string(what));
    }

    // AlgorithmIdentifier: SEQUENCE { OID 1.3.101.x } with no parameters.
    // Another OID of the same shape (the other curve, Ed448, X448) is
    // unsupported; anything else is malformed or unsupported alike
    inline expected<void, error> der_algorithm(DerReader& r, unsigned char oid) {
        DerReader alg, id;
        if (!r.read(der::sequence, alg)) {
            return unexpected<error>(der_malformed(r, "DER: the algorithm is not a SEQUENCE"));
        }
        if (!alg.read(der::object_identifier, id)) {
            return unexpected<error>(der_malformed(alg, "DER: the algorithm has no OID"));
        }
        const unsigned char* o = id.data();
        if (id.size() != 3 || o[0] != 0x2b || o[1] != 0x65 || o[2] != oid) {
            return unexpected<error>(error(errc::unsupported, uint64_t(id.offset()), string("the key is of another algorithm")));
        }
        if (!alg.empty()) {
            return unexpected<error>(der_malformed(alg, "DER: the algorithm has parameters (RFC 8410 forbids them)"));
        }
        return {};
    }

    // The 32 bytes of the key in a SubjectPublicKeyInfo
    inline expected<void, error> der_read_pkix(const slice<const byte>& der, unsigned char oid, unsigned char* key) {
        DerReader top(reinterpret_cast<const unsigned char*>(der.data()), der.size()), r, bits;
        if (!top.read(der::sequence, r) || !top.empty()) {
            return unexpected<error>(error(errc::malformed, 0, string("DER: not one SEQUENCE")));
        }
        if (auto a = der_algorithm(r, oid); !a) {
            return unexpected<error>(a.error());
        }
        if (!r.read(der::bit_string, bits) || bits.size() != 33 || bits.data()[0] != 0) {
            return unexpected<error>(error(errc::invalid_key, uint64_t(r.offset()), string("the public key is not a BIT STRING of 32 bytes")));
        }
        if (!r.empty()) {
            return unexpected<error>(der_malformed(r, "DER: data after the public key"));
        }
        std::memcpy(key, bits.data() + 1, 32);
        return {};
    }

    // The 32 bytes of the key in a PrivateKeyInfo; has_public says whether
    // a version 1 key carried its public key too, copied into public_key
    inline expected<void, error> der_read_pkcs8(const slice<const byte>& der, unsigned char oid, unsigned char* key,
                                                bool& has_public, unsigned char* public_key) {
        static constexpr unsigned char v0[] = {0}, v1[] = {1};
        has_public = false;
        DerReader top(reinterpret_cast<const unsigned char*>(der.data()), der.size()), r, outer, inner;
        if (!top.read(der::sequence, r) || !top.empty()) {
            return unexpected<error>(error(errc::malformed, 0, string("DER: not one SEQUENCE")));
        }
        bool version1 = false;
        if (!r.read_exact(der::integer, v0, 1)) {
            if (!r.read_exact(der::integer, v1, 1)) {
                return unexpected<error>(der_malformed(r, "DER: the version is not 0 or 1"));
            }
            version1 = true;
        }
        if (auto a = der_algorithm(r, oid); !a) {
            return unexpected<error>(a.error());
        }
        size_t at = r.offset();
        if (!r.read(der::octet_string, outer) || !outer.read(der::octet_string, inner) || inner.size() != 32 || !outer.empty()) {
            return unexpected<error>(error(errc::invalid_key, uint64_t(at), string("the private key is not an OCTET STRING of 32 bytes")));
        }
        std::memcpy(key, inner.data(), 32);
        if (r.peek(der::context0)) {   // [0] attributes: skipped
            DerReader attributes;
            if (!r.read(der::context0, attributes)) {
                secure_zero(key, 32);
                return unexpected<error>(der_malformed(r, "DER: the attributes cannot be read"));
            }
        }
        if (!r.empty()) {
            DerReader bits;
            if (!version1 || !r.read(der::implicit1, bits) || bits.size() != 33 || bits.data()[0] != 0 || !r.empty()) {
                secure_zero(key, 32);
                return unexpected<error>(der_malformed(r, "DER: data after the private key"));
            }
            has_public = true;
            std::memcpy(public_key, bits.data() + 1, 32);
        }
        return {};
    }
}
