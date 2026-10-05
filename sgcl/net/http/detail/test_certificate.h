//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/random.h"
#include "../../../crypto/secret.h"
#include "../../../crypto/sha256.h"
#include "../../../encoding/pem.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

// The certificates of a test server (test.h): a test CA and a leaf it
// signed, made when the server starts, each with a P-256 key of its own
// (RFC 5280: version 3, ecdsa-with-SHA256, the CA's basicConstraints and
// keyUsage critical, the leaf's names in subjectAltName — localhost,
// 127.0.0.1 and ::1 — and its extKeyUsage serverAuth and clientAuth, the
// key identifiers). Valid from an hour ago for a year. DER written forward
// here: the certificates are public, and the leaf's key leaves through
// the key's own PEM (a secret_bytes), never through these strings.
namespace sgcl::net::http::detail {
    struct TestCertificates {
        string ca_pem;          // the CA's certificate: what a client trusts
        string leaf_pem;        // the leaf's, the server's chain (the leaf, then the CA)
        crypto::secret_bytes key_pem;   // the leaf's private key, PKCS #8 in PEM
    };

    // A TLV of DER: the tag, the length in its shortest form, the content
    inline std::string der(unsigned char tag, std::string_view content) {
        std::string out;
        out.reserve(content.size() + 6);
        out += char(tag);
        const size_t n = content.size();
        if (n < 0x80) {
            out += char(n);
        } else {
            unsigned char k = 0;
            for (size_t l = n; l; l >>= 8) {
                ++k;
            }
            out += char(0x80 | k);
            for (int i = k - 1; i >= 0; --i) {
                out += char((n >> (8 * i)) & 0xFF);
            }
        }
        out += content;
        return out;
    }

    SGCL_INLINE_HOT std::string der_oid(std::string_view encoded) {
        return der(0x06, encoded);
    }

    // A Name of one RDN: the common name as a UTF8String
    inline std::string der_name(std::string_view common_name) {
        std::string atv = der_oid("\x55\x04\x03") + der(0x0c, common_name);
        return der(0x30, der(0x31, der(0x30, atv)));
    }

    // UTCTime YYMMDDHHMMSSZ of seconds since 1970 (RFC 5280 §4.1.2.5.1:
    // the years to 2049)
    inline std::string der_time(int64_t unix_seconds) {
        std::time_t t = std::time_t(unix_seconds);
        std::tm tm{};
        gmtime_r(&t, &tm);
        char text[16];
        std::snprintf(text, sizeof text, "%02d%02d%02d%02d%02d%02dZ", tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return der(0x17, std::string_view(text, 13));
    }

    inline std::string der_extension(std::string_view oid, bool critical, std::string_view value) {
        std::string body = der_oid(oid);
        if (critical) {
            body += der(0x01, std::string_view("\xFF", 1));
        }
        body += der(0x04, value);
        return der(0x30, body);
    }

    // A key's identifier (RFC 5280 §4.2.1.2): 20 bytes of the SHA-256 of
    // its SubjectPublicKeyInfo
    inline std::string key_id(const vector<byte>& spki) {
        auto d = crypto::sha256::of(spki.as_slice());
        return std::string(reinterpret_cast<const char*>(d.data()), 20);
    }

    inline std::string as_text(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // The certificate of `subject_key`, signed by `issuer_key`: the
    // TBSCertificate, the algorithm, the signature
    inline std::string make_certificate(std::string_view subject, std::string_view issuer, const crypto::p256::private_key& subject_key,
                                        const crypto::p256::private_key& issuer_key, bool ca, int64_t now) {
        static constexpr std::string_view EcdsaSha256 = "\x2A\x86\x48\xCE\x3D\x04\x03\x02";
        const std::string algorithm = der(0x30, der_oid(EcdsaSha256));
        auto serial_bytes = crypto::random::bytes(16);
        std::string serial = as_text(serial_bytes);
        serial[0] = char((uint8_t(serial[0]) & 0x7F) | 0x40);   // positive, sixteen bytes in its shortest form
        const auto spki = subject_key.public_key().to_pkix_der();
        const auto issuer_spki = issuer_key.public_key().to_pkix_der();
        std::string extensions;
        if (ca) {
            extensions += der_extension("\x55\x1D\x13", true, der(0x30, der(0x01, std::string_view("\xFF", 1))));    // basicConstraints: CA
            extensions += der_extension("\x55\x1D\x0F", true, der(0x03, std::string_view("\x01\x86", 2)));           // keyUsage: digitalSignature, keyCertSign, cRLSign
            extensions += der_extension("\x55\x1D\x0E", false, der(0x04, key_id(spki)));                             // subjectKeyIdentifier
        } else {
            extensions += der_extension("\x55\x1D\x13", true, der(0x30, ""));                                         // basicConstraints: not a CA
            extensions += der_extension("\x55\x1D\x0F", true, der(0x03, std::string_view("\x07\x80", 2)));           // keyUsage: digitalSignature
            extensions += der_extension("\x55\x1D\x25", false,
                                        der(0x30, der_oid("\x2B\x06\x01\x05\x05\x07\x03\x01") + der_oid("\x2B\x06\x01\x05\x05\x07\x03\x02")));   // serverAuth, clientAuth
            std::string names = der(0x82, "localhost") + der(0x87, std::string_view("\x7F\x00\x00\x01", 4))
                                + der(0x87, std::string_view("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01", 16));
            extensions += der_extension("\x55\x1D\x11", false, der(0x30, names));                                     // subjectAltName
            extensions += der_extension("\x55\x1D\x0E", false, der(0x04, key_id(spki)));
            extensions += der_extension("\x55\x1D\x23", false, der(0x30, der(0x80, key_id(issuer_spki))));            // authorityKeyIdentifier
        }
        std::string tbs = der(0xA0, der(0x02, std::string_view("\x02", 1)));   // version 3
        tbs += der(0x02, serial);
        tbs += algorithm;
        tbs += der_name(issuer);
        tbs += der(0x30, der_time(now - 3600) + der_time(now + 365 * 86400));
        tbs += der_name(subject);
        tbs += as_text(spki);
        tbs += der(0xA3, der(0x30, extensions));
        tbs = der(0x30, tbs);
        auto digest = crypto::sha256::of(slice<const byte>(reinterpret_cast<const byte*>(tbs.data()), tbs.size()));
        auto signature = issuer_key.sign_digest(slice<const byte>(digest.data(), digest.size()));
        std::string bits(1, '\0');
        bits += as_text(signature);
        return der(0x30, tbs + algorithm + der(0x03, bits));
    }

    inline string certificate_pem(const std::string& certificate) {
        vector<byte> bytes(certificate.size());
        sgcl::detail::copy_bytes(bytes.data(), certificate.data(), certificate.size());
        return encoding::pem(string("CERTIFICATE"), std::move(bytes)).to_string();
    }

    // A fresh CA and leaf
    inline TestCertificates make_test_certificates(int64_t now) {
        auto ca_key = crypto::p256::private_key::generate();
        auto leaf_key = crypto::p256::private_key::generate();
        TestCertificates out;
        out.ca_pem = certificate_pem(make_certificate("sgcl test CA", "sgcl test CA", ca_key, ca_key, true, now));
        out.leaf_pem = certificate_pem(make_certificate("sgcl test server", "sgcl test CA", leaf_key, ca_key, false, now));
        out.key_pem = leaf_key.to_pem();
        return out;
    }
}
