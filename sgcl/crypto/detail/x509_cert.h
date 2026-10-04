//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "der.h"
#include "x509_hostname.h"
#include "x509_name.h"
#include "../ed25519.h"
#include "../error.h"
#include "../hash_id.h"
#include "../p256.h"
#include "../p384.h"
#include "../rsa.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/detail/bytes.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/variant.h"
#include "../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

// The values a certificate holds and the parser that reads them (RFC 5280
// §4.1 and §4.2), over the module's one DER reader. What the parser takes
// is what Go's x509.ParseCertificate takes, but stricter where DER is:
// nothing after an element that ends a structure (a TBSCertificate, an
// extension, a name's attribute), the times in their one form, a version
// 1 or 2 certificate without extensions. What a certificate may hold is
// bounded (§11 A7): its size, its extensions, the names of its SAN and the
// subtrees of its name constraints, the attributes of a name; the depth of
// nesting is the reader's. A public key of an algorithm the module has no
// type for, or one its type refuses (an RSA key under 1024 bits, a point
// off the curve), leaves the certificate readable and its key none, as Go
// leaves a key of an unknown algorithm; a key whose DER is broken is a
// certificate that cannot be read.
namespace sgcl::crypto::x509 {
    // An IP address of a certificate (an iPAddress of the SAN, RFC 5280
    // §4.2.1.6): the bytes in network order, size of them used, 4 or 16.
    // As bytes and never as text, as Go keeps it; net::ip_address converts
    // from and to it through its bytes
    struct ip_address {
        array<byte, 16> bytes{};
        uint8_t size = 0;

        SGCL_INLINE_HOT friend bool operator==(const ip_address& a, const ip_address& b) noexcept {
            return a.size == b.size && std::memcmp(a.bytes.data(), b.bytes.data(), a.size) == 0;
        }
    };

    // An IP range of name constraints: an address and a mask of the same
    // size, the mask ones then zeros (10.0.0.0/255.0.0.0)
    struct ip_range {
        ip_address address;
        ip_address mask;

        // Whether the address is in the range: of the same size, and
        // equal to the range's address under the mask
        bool contains(const ip_address& a) const noexcept {
            if (a.size != address.size) {
                return false;
            }
            for (size_t i = 0; i < a.size; ++i) {
                if (((unsigned(a.bytes[i]) ^ unsigned(address.bytes[i])) & unsigned(mask.bytes[i])) != 0) {
                    return false;
                }
            }
            return true;
        }
    };

    // The bits of keyUsage (RFC 5280 §4.2.1.3), as flags
    enum class key_usage : uint16_t {
        digital_signature = 1 << 0,
        content_commitment = 1 << 1,
        key_encipherment = 1 << 2,
        data_encipherment = 1 << 3,
        key_agreement = 1 << 4,
        cert_sign = 1 << 5,
        crl_sign = 1 << 6,
        encipher_only = 1 << 7,
        decipher_only = 1 << 8
    };

    SGCL_INLINE_HOT constexpr key_usage operator|(key_usage a, key_usage b) noexcept {
        return key_usage(uint16_t(a) | uint16_t(b));
    }

    SGCL_INLINE_HOT constexpr key_usage operator&(key_usage a, key_usage b) noexcept {
        return key_usage(uint16_t(a) & uint16_t(b));
    }

    // The purposes of extKeyUsage (RFC 5280 §4.2.1.12) the module knows by
    // name, Go's ExtKeyUsage; any other is kept as its OID
    enum class ext_key_usage : uint8_t {
        any = 1,
        server_auth,
        client_auth,
        code_signing,
        email_protection,
        ipsec_end_system,
        ipsec_tunnel,
        ipsec_user,
        time_stamping,
        ocsp_signing,
        microsoft_server_gated_crypto,
        netscape_server_gated_crypto,
        microsoft_commercial_code_signing,
        microsoft_kernel_code_signing
    };

    // The algorithm a certificate is signed with, Go's SignatureAlgorithm.
    // Those over MD2, MD5 and SHA-1 are named so that they can be refused
    // by name; the module verifies the RSA, RSA-PSS and ECDSA ones over
    // SHA-256, SHA-384 and SHA-512, and Ed25519
    enum class signature_algorithm : uint8_t {
        unknown = 0,
        md2_with_rsa,
        md5_with_rsa,
        sha1_with_rsa,
        sha256_with_rsa,
        sha384_with_rsa,
        sha512_with_rsa,
        sha256_with_rsa_pss,
        sha384_with_rsa_pss,
        sha512_with_rsa_pss,
        ecdsa_with_sha1,
        ecdsa_with_sha256,
        ecdsa_with_sha384,
        ecdsa_with_sha512,
        ed25519
    };

    // What a certificate's public key is: none for an algorithm the module
    // has no type for (DSA, X25519, P-521, ML-DSA) or a key its type
    // refuses
    enum class key_kind : uint8_t {
        none = 0,
        rsa,
        p256,
        p384,
        ed25519
    };

    namespace detail {
        struct CertParser;
    }

    // The public key of a certificate, one of the module's key types or
    // none: kind() says which, and rsa(), p256(), p384(), ed25519() give it
    // (std::logic_error for another kind); algorithm() is the OID of the
    // SubjectPublicKeyInfo whatever the kind
    class public_key {
    public:
        using value_type = variant<monostate, crypto::rsa::public_key, crypto::p256::public_key, crypto::p384::public_key, crypto::ed25519::public_key>;

        public_key() = default;

        SGCL_INLINE_HOT key_kind kind() const noexcept {
            return key_kind(_key.index());
        }

        SGCL_INLINE_HOT bool has_value() const noexcept {
            return _key.index() != 0;
        }

        SGCL_INLINE_HOT const crypto::rsa::public_key& rsa() const {
            return _get<crypto::rsa::public_key, 1>("an RSA key");
        }

        SGCL_INLINE_HOT const crypto::p256::public_key& p256() const {
            return _get<crypto::p256::public_key, 2>("a P-256 key");
        }

        SGCL_INLINE_HOT const crypto::p384::public_key& p384() const {
            return _get<crypto::p384::public_key, 3>("a P-384 key");
        }

        SGCL_INLINE_HOT const crypto::ed25519::public_key& ed25519() const {
            return _get<crypto::ed25519::public_key, 4>("an Ed25519 key");
        }

        // The key as the variant, for visit
        SGCL_INLINE_HOT const value_type& value() const noexcept {
            return _key;
        }

        // "1.2.840.113549.1.1.1" (rsaEncryption), "1.2.840.10045.2.1"
        // (id-ecPublicKey), "1.3.101.112" (Ed25519): the SPKI's algorithm
        SGCL_INLINE_HOT const string& algorithm() const noexcept {
            return _algorithm;
        }

    private:
        friend struct detail::CertParser;

        value_type _key;
        string _algorithm;

        template<class T, size_t I>
        SGCL_INLINE_HOT const T& _get(const char* what) const {
            if (_key.index() != I) {
                throw logic_error(std::string("sgcl::crypto::x509: the public key is not ") + what);
            }
            return get<I>(_key);
        }
    };

    // An extension as it is in the certificate: its OID, whether it is
    // critical, the bytes of its extnValue
    struct extension {
        string oid;
        bool critical = false;
        vector<byte> value;
    };
}

namespace sgcl::crypto::x509::detail {
    // The bounds of §11 A7: what no real certificate comes near, and what
    // keeps the parse and the checks of a chain small whatever the input
    inline constexpr size_t max_certificate_size = 128 * 1024;
    inline constexpr size_t max_extensions = 64;
    inline constexpr size_t max_san_names = 1024;
    inline constexpr size_t max_constraint_subtrees = 256;
    inline constexpr size_t max_policies = 64;
    inline constexpr size_t max_ext_key_usages = 64;

    // The OIDs read (content bytes)
    namespace oid {
        inline constexpr unsigned char md2_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x02};
        inline constexpr unsigned char md5_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04};
        inline constexpr unsigned char sha1_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05};
        inline constexpr unsigned char rsa_pss[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a};
        inline constexpr unsigned char sha256_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
        inline constexpr unsigned char sha384_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c};
        inline constexpr unsigned char sha512_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d};
        inline constexpr unsigned char mgf1[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x08};
        inline constexpr unsigned char ecdsa_sha1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01};
        inline constexpr unsigned char ecdsa_sha256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};
        inline constexpr unsigned char ecdsa_sha384[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03};
        inline constexpr unsigned char ecdsa_sha512[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04};
        inline constexpr unsigned char ed25519[] = {0x2b, 0x65, 0x70};
        inline constexpr unsigned char sha256[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
        inline constexpr unsigned char sha384[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02};
        inline constexpr unsigned char sha512[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03};
        inline constexpr unsigned char rsa_encryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
        inline constexpr unsigned char ec_public_key[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
        inline constexpr unsigned char prime256v1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
        inline constexpr unsigned char secp384r1[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
        inline constexpr unsigned char authority_info_access[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01};
        inline constexpr unsigned char any_ext_key_usage[] = {0x55, 0x1d, 0x25, 0x00};
        inline constexpr unsigned char kp_prefix[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03};   // 1.3.6.1.5.5.7.3.x
        inline constexpr unsigned char ms_sgc[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0a, 0x03, 0x03};
        inline constexpr unsigned char ns_sgc[] = {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x04, 0x01};
        inline constexpr unsigned char ms_commercial_code_signing[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x16};
        inline constexpr unsigned char ms_kernel_code_signing[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3d, 0x01, 0x01};

        template<size_t N>
        SGCL_INLINE_HOT bool is(const DerReader& r, const unsigned char (&o)[N]) noexcept {
            return r.size() == N && std::memcmp(r.data(), o, N) == 0;
        }
    }

    // Everything read from one certificate, held by the certificate
    // object through a tracked_ptr and never changed after the parse: a
    // copy of a certificate costs one pointer. The byte ranges index raw
    struct CertData {
        vector<byte> raw;
        size_t tbs_at = 0, tbs_size = 0;
        size_t issuer_at = 0, issuer_size = 0;
        size_t subject_at = 0, subject_size = 0;
        size_t spki_at = 0, spki_size = 0;
        size_t san_at = 0, san_size = 0;   // the SAN's extnValue, for the loop check of a chain

        int version = 1;
        vector<byte> serial;
        signature_algorithm sig_algorithm = signature_algorithm::unknown;
        string sig_algorithm_oid;
        vector<byte> signature;
        bool signature_whole = true;       // the BIT STRING had no unused bits
        name issuer, subject;
        int64_t not_before = 0, not_after = 0;
        public_key key;
        string key_error;                  // why the key is none, for a known algorithm

        vector<extension> extensions;
        bool basic_constraints_valid = false;
        bool is_ca = false;
        int64_t max_path_len = -1;         // -1: no pathLenConstraint
        bool has_key_usage = false;
        uint16_t key_usage_bits = 0;
        vector<ext_key_usage> ext_key_usages;
        vector<string> unknown_ext_key_usages;
        bool has_san = false;
        vector<string> dns_names, email_addresses, uris;
        vector<ip_address> ip_addresses;
        vector<byte> authority_key_id, subject_key_id;
        bool has_name_constraints = false;
        vector<string> permitted_dns, excluded_dns, permitted_email, excluded_email, permitted_uri, excluded_uri;
        vector<ip_range> permitted_ip, excluded_ip;
        vector<string> policies;
        vector<string> unhandled_critical;

        SGCL_INLINE_HOT slice<const byte> range(size_t at, size_t n) const noexcept {
            return raw.as_slice().subslice(at, n);
        }

        SGCL_INLINE_HOT const unsigned char* bytes_at(size_t at) const noexcept {
            return reinterpret_cast<const unsigned char*>(raw.data()) + at;
        }
    };

    // The parser of one certificate into a CertData, its raw already set
    struct CertParser {
        CertData& c;

        static error fail(size_t at, const char* what) noexcept {
            return error(errc::malformed, uint64_t(at), string(std::string("sgcl::crypto::x509: ") + what));
        }

        SGCL_INLINE_HOT static string text(const DerReader& r) noexcept {
            return string(std::string(reinterpret_cast<const char*>(r.data()), r.size()));
        }

        SGCL_INLINE_HOT static vector<byte> copy(const unsigned char* p, size_t n) noexcept {
            const byte* b = reinterpret_cast<const byte*>(p);
            return vector<byte>(b, b + n);
        }

        SGCL_INLINE_HOT size_t at_of(const DerReader& r) const noexcept {
            return size_t(r.data() - c.bytes_at(0));
        }

        expected<void, error> run() noexcept {
            if (c.raw.size() > max_certificate_size) {
                return unexpected<error>(fail(0, "a certificate larger than 128 KiB"));
            }
            DerReader in(c.bytes_at(0), c.raw.size());
            DerReader cert, tbs_el, tbs, alg_in, alg_out;
            if (!in.read(der::sequence, cert)) {
                return unexpected<error>(fail(in.offset(), "not a Certificate SEQUENCE"));
            }
            if (!in.empty()) {
                return unexpected<error>(fail(in.offset(), "data after the certificate"));
            }
            if (!cert.read_element(der::sequence, tbs_el)) {
                return unexpected<error>(fail(cert.offset(), "not a TBSCertificate SEQUENCE"));
            }
            c.tbs_at = at_of(tbs_el);
            c.tbs_size = tbs_el.size();
            tbs_el.read(der::sequence, tbs);
            // version [0] EXPLICIT INTEGER DEFAULT v1
            DerReader ver;
            bool present;
            if (!tbs.read_optional(der::context0, ver, present)) {
                return unexpected<error>(fail(tbs.offset(), "a malformed version"));
            }
            if (present) {
                uint64_t v;
                if (!ver.read_small_unsigned(v) || !ver.empty() || v > 2) {
                    return unexpected<error>(fail(ver.offset(), "a version other than 1, 2 or 3"));
                }
                c.version = int(v) + 1;
            }
            // serialNumber: an INTEGER in its shortest form. A negative one
            // is taken, as OpenSSL takes it and Go did before 1.23: a root
            // of the system's bundle (EC-ACC) has one
            DerReader serial;
            size_t serial_at = tbs.offset();
            if (!tbs.read(der::integer, serial) || serial.size() == 0
                || (serial.size() > 1 && ((serial.data()[0] == 0x00 && (serial.data()[1] & 0x80) == 0) || (serial.data()[0] == 0xff && (serial.data()[1] & 0x80) != 0)))) {
                return unexpected<error>(fail(serial_at, "a serial number that is not an INTEGER in its shortest form"));
            }
            c.serial = copy(serial.data(), serial.size());
            // signature: the AlgorithmIdentifier, byte for byte the outer one
            size_t alg_at = tbs.offset();
            if (!tbs.read(der::sequence, alg_in)) {
                return unexpected<error>(fail(alg_at, "a malformed signature algorithm"));
            }
            size_t outer_at = cert.offset();
            if (!cert.read(der::sequence, alg_out)) {
                return unexpected<error>(fail(outer_at, "a malformed signature algorithm"));
            }
            if (alg_in.size() != alg_out.size() || std::memcmp(alg_in.data(), alg_out.data(), alg_in.size()) != 0) {
                return unexpected<error>(fail(outer_at, "the inner and the outer signature algorithms differ"));
            }
            if (auto a = signature_algorithm_of(alg_out); !a) {
                return unexpected<error>(a.error());
            }
            // issuer
            DerReader issuer_el;
            size_t issuer_at = tbs.offset();
            if (!tbs.read_element(der::sequence, issuer_el)) {
                return unexpected<error>(fail(issuer_at, "a malformed issuer name"));
            }
            c.issuer_at = issuer_at;
            c.issuer_size = issuer_el.size();
            if (!parse_name(issuer_el, c.issuer) || !issuer_el.empty()) {
                return unexpected<error>(fail(issuer_at, "a malformed issuer name"));
            }
            // validity
            DerReader validity;
            size_t validity_at = tbs.offset();
            if (!tbs.read(der::sequence, validity) || !validity.read_time(c.not_before) || !validity.read_time(c.not_after) || !validity.empty()) {
                return unexpected<error>(fail(validity_at, "a malformed validity: the times are UTCTime YYMMDDHHMMSSZ or GeneralizedTime YYYYMMDDHHMMSSZ"));
            }
            // subject
            DerReader subject_el;
            size_t subject_at = tbs.offset();
            if (!tbs.read_element(der::sequence, subject_el)) {
                return unexpected<error>(fail(subject_at, "a malformed subject name"));
            }
            c.subject_at = subject_at;
            c.subject_size = subject_el.size();
            if (!parse_name(subject_el, c.subject) || !subject_el.empty()) {
                return unexpected<error>(fail(subject_at, "a malformed subject name"));
            }
            // subjectPublicKeyInfo
            size_t spki_at = tbs.offset();
            DerReader spki_el;
            if (!tbs.read_element(der::sequence, spki_el)) {
                return unexpected<error>(fail(spki_at, "a malformed SubjectPublicKeyInfo"));
            }
            c.spki_at = spki_at;
            c.spki_size = spki_el.size();
            if (auto k = public_key_of(spki_el, spki_at); !k) {
                return unexpected<error>(k.error());
            }
            // issuerUniqueID [1], subjectUniqueID [2]: version 2 and 3 only
            // (IMPLICIT BIT STRING: primitive, its content a BIT STRING's)
            for (unsigned char t : {der::implicit1, der::implicit2}) {
                if (tbs.peek(t)) {
                    DerReader id;
                    size_t id_at = tbs.offset();
                    if (c.version < 2 || !tbs.read(t, id)) {
                        return unexpected<error>(fail(id_at, "a malformed unique identifier"));
                    }
                    const unsigned char* p = id.data();
                    size_t n = id.size();
                    if (n == 0 || p[0] > 7 || (n == 1 && p[0] != 0) || (n > 1 && (p[n - 1] & ((1u << p[0]) - 1)) != 0)) {
                        return unexpected<error>(fail(id_at, "a malformed unique identifier"));
                    }
                }
            }
            // extensions [3]: version 3 only
            if (tbs.peek(der::context3)) {
                size_t ext_at = tbs.offset();
                DerReader wrap, list;
                if (c.version < 3 || !tbs.read(der::context3, wrap) || !wrap.read(der::sequence, list) || !wrap.empty()) {
                    return unexpected<error>(fail(ext_at, "malformed extensions, or extensions in a version 1 or 2 certificate"));
                }
                if (auto e = extensions_of(list); !e) {
                    return unexpected<error>(e.error());
                }
            }
            if (!tbs.empty()) {
                return unexpected<error>(fail(tbs.offset(), "data after the TBSCertificate's fields"));
            }
            // signatureValue
            const unsigned char* sig;
            size_t sig_n;
            unsigned unused;
            size_t sig_at = cert.offset();
            if (!cert.read_bit_string(sig, sig_n, unused) || !cert.empty()) {
                return unexpected<error>(fail(sig_at, "a malformed signature"));
            }
            c.signature = copy(sig, sig_n);
            c.signature_whole = unused == 0;
            return {};
        }

        // The AlgorithmIdentifier of the signature: an algorithm the list
        // names, or unknown (the certificate is read, never verified).
        // RSA PKCS #1 v1.5 and ECDSA with NULL or no parameters, Ed25519
        // with none, RSA-PSS with the parameters Go takes: MGF1 over the
        // same hash, the salt as long as the hash, the trailer 1
        expected<void, error> signature_algorithm_of(DerReader alg) noexcept {
            DerReader o;
            if (!alg.read_oid(o)) {
                return unexpected<error>(fail(alg.offset(), "a malformed signature algorithm OID"));
            }
            c.sig_algorithm_oid = string(oid_text(o.data(), o.size()));
            DerReader params = alg;
            bool no_params = alg.empty();
            if (!no_params) {
                DerReader rest = alg, any;
                unsigned char tag;
                if (!rest.read_any(tag, any) || !rest.empty()) {
                    return unexpected<error>(fail(alg.offset(), "malformed parameters of the signature algorithm"));
                }
            }
            bool null_params = false;
            if (!no_params) {
                static constexpr unsigned char nothing[1] = {};
                DerReader p = alg;
                null_params = p.read_exact(der::null, nothing, 0) && p.empty();
            }
            auto set = [&](signature_algorithm a) {
                c.sig_algorithm = a;
            };
            bool plain = no_params || null_params;
            if (oid::is(o, oid::md2_rsa) && plain) set(signature_algorithm::md2_with_rsa);
            else if (oid::is(o, oid::md5_rsa) && plain) set(signature_algorithm::md5_with_rsa);
            else if (oid::is(o, oid::sha1_rsa) && plain) set(signature_algorithm::sha1_with_rsa);
            else if (oid::is(o, oid::sha256_rsa) && plain) set(signature_algorithm::sha256_with_rsa);
            else if (oid::is(o, oid::sha384_rsa) && plain) set(signature_algorithm::sha384_with_rsa);
            else if (oid::is(o, oid::sha512_rsa) && plain) set(signature_algorithm::sha512_with_rsa);
            else if (oid::is(o, oid::ecdsa_sha1) && plain) set(signature_algorithm::ecdsa_with_sha1);
            else if (oid::is(o, oid::ecdsa_sha256) && plain) set(signature_algorithm::ecdsa_with_sha256);
            else if (oid::is(o, oid::ecdsa_sha384) && plain) set(signature_algorithm::ecdsa_with_sha384);
            else if (oid::is(o, oid::ecdsa_sha512) && plain) set(signature_algorithm::ecdsa_with_sha512);
            else if (oid::is(o, oid::ed25519) && no_params) set(signature_algorithm::ed25519);
            else if (oid::is(o, oid::rsa_pss) && !no_params) set(pss_of(params));
            return {};
        }

        // RSASSA-PSS-params (RFC 4055 §3.1) in one of Go's three buckets
        static signature_algorithm pss_of(DerReader alg) noexcept {
            DerReader seq;
            if (!alg.read(der::sequence, seq) || !alg.empty()) {
                return signature_algorithm::unknown;
            }
            // a hash AlgorithmIdentifier: the OID and NULL or nothing
            auto hash_of = [](DerReader& r, int& which) {
                DerReader h, o;
                if (!r.read(der::sequence, h) || !h.read_oid(o)) {
                    return false;
                }
                static constexpr unsigned char nothing[1] = {};
                if (!h.empty() && !(h.read_exact(der::null, nothing, 0) && h.empty())) {
                    return false;
                }
                which = oid::is(o, oid::sha256) ? 256 : oid::is(o, oid::sha384) ? 384 : oid::is(o, oid::sha512) ? 512 : 0;
                return which != 0;
            };
            int hash = 0, mgf_hash = 0;
            uint64_t salt = 20, trailer = 1;
            DerReader f;
            bool present;
            if (!seq.read_optional(der::context0, f, present) || !present || !hash_of(f, hash) || !f.empty()) {
                return signature_algorithm::unknown;   // SHA-1 by default: not taken
            }
            DerReader mgf, mo;
            if (!seq.read_optional(der::context1, f, present) || !present || !f.read(der::sequence, mgf) || !f.empty() || !mgf.read_oid(mo)
                || !oid::is(mo, oid::mgf1) || !hash_of(mgf, mgf_hash) || !mgf.empty()) {
                return signature_algorithm::unknown;
            }
            if (!seq.read_optional(0xa2, f, present) || (present && (!f.read_small_unsigned(salt) || !f.empty()))) {
                return signature_algorithm::unknown;
            }
            if (!seq.read_optional(der::context3, f, present) || (present && (!f.read_small_unsigned(trailer) || !f.empty()))) {
                return signature_algorithm::unknown;
            }
            if (!seq.empty() || mgf_hash != hash || trailer != 1 || salt != uint64_t(hash / 8)) {
                return signature_algorithm::unknown;
            }
            return hash == 256 ? signature_algorithm::sha256_with_rsa_pss : hash == 384 ? signature_algorithm::sha384_with_rsa_pss : signature_algorithm::sha512_with_rsa_pss;
        }

        // The SubjectPublicKeyInfo: its structure must be DER (an
        // AlgorithmIdentifier and a BIT STRING); the key it holds becomes
        // one of the module's types when the algorithm is one of them and
        // the type takes it, else none, the certificate still read
        expected<void, error> public_key_of(DerReader el, size_t at) noexcept {
            DerReader spki, alg, o;
            const unsigned char* kp;
            size_t kn;
            unsigned unused;
            if (!el.read(der::sequence, spki) || !spki.read(der::sequence, alg) || !alg.read_oid(o) || !spki.read_bit_string(kp, kn, unused) || !spki.empty()) {
                return unexpected<error>(fail(at, "a malformed SubjectPublicKeyInfo"));
            }
            if (!alg.empty()) {
                DerReader rest = alg, params;
                unsigned char tag;
                if (!rest.read_any(tag, params) || !rest.empty()) {
                    return unexpected<error>(fail(at, "malformed parameters of the public key's algorithm"));
                }
            }
            c.key._algorithm = string(oid_text(o.data(), o.size()));
            slice<const byte> der = c.range(at, c.spki_size);
            auto take = [&](auto&& r, auto index) -> expected<void, error> {
                if (r) {
                    c.key._key.template emplace<decltype(index)::value>(std::move(*r));
                    return {};
                }
                if (r.error().code() == errc::malformed) {
                    return unexpected<error>(error(errc::malformed, uint64_t(at), r.error().message()));
                }
                c.key_error = r.error().message();
                return {};
            };
            if (oid::is(o, oid::rsa_encryption)) {
                return take(crypto::rsa::public_key::from_pkix_der(der), std::integral_constant<size_t, 1>());
            }
            if (oid::is(o, oid::ec_public_key)) {
                DerReader a2 = alg, curve;
                if (a2.read_oid(curve) && oid::is(curve, oid::prime256v1)) {
                    return take(crypto::p256::public_key::from_pkix_der(der), std::integral_constant<size_t, 2>());
                }
                if (oid::is(curve, oid::secp384r1)) {
                    return take(crypto::p384::public_key::from_pkix_der(der), std::integral_constant<size_t, 3>());
                }
                c.key_error = string("an elliptic curve the module does not have");
                return {};
            }
            if (oid::is(o, oid::ed25519)) {
                return take(crypto::ed25519::public_key::from_pkix_der(der), std::integral_constant<size_t, 4>());
            }
            return {};
        }

        expected<void, error> extensions_of(DerReader list) noexcept {
            size_t count = 0;
            while (!list.empty()) {
                size_t at = list.offset();
                DerReader ext, o, value;
                bool critical = false;
                if (++count > max_extensions) {
                    return unexpected<error>(fail(at, "more than 64 extensions"));
                }
                if (!list.read(der::sequence, ext) || !ext.read_oid(o)) {
                    return unexpected<error>(fail(at, "a malformed extension"));
                }
                if (ext.peek(der::boolean) && !ext.read_bool(critical)) {
                    return unexpected<error>(fail(ext.offset(), "a malformed extension's critical flag"));
                }
                if (!ext.read(der::octet_string, value) || !ext.empty()) {
                    return unexpected<error>(fail(ext.offset(), "a malformed extension's value"));
                }
                extension e;
                e.oid = string(oid_text(o.data(), o.size()));
                for (auto& seen : c.extensions) {
                    if (seen.oid == e.oid) {
                        return unexpected<error>(fail(at, "an extension given twice"));
                    }
                }
                e.critical = critical;
                e.value = copy(value.data(), value.size());
                c.extensions.push_back(std::move(e));
                bool unhandled = false;
                if (auto r = extension_of(o, critical, value, unhandled); !r) {
                    return r;
                }
                if (critical && unhandled) {
                    c.unhandled_critical.push_back(string(oid_text(o.data(), o.size())));
                }
            }
            return {};
        }

        expected<void, error> extension_of(const DerReader& o, bool critical, DerReader value, bool& unhandled) noexcept {
            size_t at = value.offset();
            static constexpr unsigned char ce[] = {0x55, 0x1d};   // 2.5.29
            if (o.size() == 3 && std::memcmp(o.data(), ce, 2) == 0) {
                switch (o.data()[2]) {
                    case 14: {   // subjectKeyIdentifier
                        DerReader id;
                        if (critical) {
                            return unexpected<error>(fail(at, "a subject key identifier marked critical"));
                        }
                        if (!value.read(der::octet_string, id) || !value.empty()) {
                            return unexpected<error>(fail(at, "a malformed subject key identifier"));
                        }
                        c.subject_key_id = copy(id.data(), id.size());
                        return {};
                    }
                    case 15: {   // keyUsage
                        const unsigned char* p;
                        size_t n;
                        unsigned unused;
                        if (!value.read_bit_string(p, n, unused) || !value.empty()) {
                            return unexpected<error>(fail(at, "a malformed key usage"));
                        }
                        uint16_t bits = 0;
                        for (unsigned i = 0; i < 9; ++i) {
                            if (i / 8 < n && (p[i / 8] >> (7 - i % 8) & 1)) {
                                bits = uint16_t(bits | 1u << i);
                            }
                        }
                        c.has_key_usage = true;
                        c.key_usage_bits = bits;
                        return {};
                    }
                    case 17: {   // subjectAltName
                        if (auto r = san_of(value); !r) {
                            return r;
                        }
                        c.has_san = true;
                        c.san_at = at;
                        c.san_size = value.size();
                        unhandled = c.dns_names.empty() && c.email_addresses.empty() && c.ip_addresses.empty() && c.uris.empty();
                        return {};
                    }
                    case 19: {   // basicConstraints
                        DerReader seq;
                        if (!value.read(der::sequence, seq) || !value.empty()) {
                            return unexpected<error>(fail(at, "malformed basic constraints"));
                        }
                        if (seq.peek(der::boolean) && !seq.read_bool(c.is_ca)) {
                            return unexpected<error>(fail(at, "malformed basic constraints"));
                        }
                        if (seq.peek(der::integer)) {
                            uint64_t n;
                            if (!seq.read_small_unsigned(n)) {
                                return unexpected<error>(fail(at, "a malformed path length constraint"));
                            }
                            c.max_path_len = int64_t(n);
                        }
                        if (!seq.empty()) {
                            return unexpected<error>(fail(at, "malformed basic constraints"));
                        }
                        c.basic_constraints_valid = true;
                        return {};
                    }
                    case 30:     // nameConstraints
                        return name_constraints_of(value, critical, unhandled);
                    case 31:     // cRLDistributionPoints: read, not used
                        return sequence_of_sequences(value, "malformed CRL distribution points");
                    case 32:     // certificatePolicies
                        return policies_of(value);
                    case 33:     // policyMappings: read, not enforced
                    case 36:     // policyConstraints
                    case 54: {   // inhibitAnyPolicy
                        DerReader any;
                        unsigned char tag;
                        if (!value.read_any(tag, any) || !value.empty()) {
                            return unexpected<error>(fail(at, "a malformed policy extension"));
                        }
                        unhandled = true;   // policy validation is not done: refused where critical
                        return {};
                    }
                    case 35: {   // authorityKeyIdentifier
                        if (critical) {
                            return unexpected<error>(fail(at, "an authority key identifier marked critical"));
                        }
                        DerReader seq, id;
                        bool present;
                        if (!value.read(der::sequence, seq) || !value.empty() || !seq.read_optional(der::implicit0, id, present)) {
                            return unexpected<error>(fail(at, "a malformed authority key identifier"));
                        }
                        if (present) {
                            c.authority_key_id = copy(id.data(), id.size());
                        }
                        while (!seq.empty()) {
                            DerReader skip;
                            unsigned char tag;
                            if (!seq.read_any(tag, skip)) {
                                return unexpected<error>(fail(at, "a malformed authority key identifier"));
                            }
                        }
                        return {};
                    }
                    case 37:     // extKeyUsage
                        return ext_key_usage_of(value);
                    default:
                        break;
                }
            } else if (oid::is(o, oid::authority_info_access)) {
                if (critical) {
                    return unexpected<error>(fail(at, "authority information access marked critical"));
                }
                return sequence_of_sequences(value, "malformed authority information access");
            }
            unhandled = true;
            return {};
        }

        // A SEQUENCE of SEQUENCEs, each well formed: what an extension
        // read and not used must be to be taken
        expected<void, error> sequence_of_sequences(DerReader value, const char* what) noexcept {
            size_t at = value.offset();
            DerReader seq;
            if (!value.read(der::sequence, seq) || !value.empty()) {
                return unexpected<error>(fail(at, what));
            }
            while (!seq.empty()) {
                DerReader item;
                if (!seq.read(der::sequence, item)) {
                    return unexpected<error>(fail(at, what));
                }
            }
            return {};
        }

        expected<void, error> san_of(DerReader value) noexcept {
            size_t at = value.offset();
            DerReader seq;
            if (!value.read(der::sequence, seq) || !value.empty()) {
                return unexpected<error>(fail(at, "a malformed subject alternative name"));
            }
            size_t count = 0;
            while (!seq.empty()) {
                DerReader gn;
                unsigned char tag;
                size_t gat = seq.offset();
                if (++count > max_san_names) {
                    return unexpected<error>(fail(gat, "more than 1024 subject alternative names"));
                }
                if (!seq.read_any(tag, gn)) {
                    return unexpected<error>(fail(gat, "a malformed subject alternative name"));
                }
                switch (tag) {
                    case 0x81:   // rfc822Name
                        if (!ia5_valid(gn.data(), gn.size())) {
                            return unexpected<error>(fail(gat, "an rfc822Name that is not IA5"));
                        }
                        c.email_addresses.push_back(text(gn));
                        break;
                    case 0x82:   // dNSName
                        if (!ia5_valid(gn.data(), gn.size())) {
                            return unexpected<error>(fail(gat, "a dNSName that is not IA5"));
                        }
                        c.dns_names.push_back(text(gn));
                        break;
                    case 0x86: { // uniformResourceIdentifier
                        std::string_view s(reinterpret_cast<const char*>(gn.data()), gn.size());
                        std::string_view host;
                        if (!ia5_valid(gn.data(), gn.size()) || !x509_names::uri_host(s, host) || (!host.empty() && !x509_names::domain_valid(host, false))) {
                            return unexpected<error>(fail(gat, "a URI that cannot be read"));
                        }
                        c.uris.push_back(text(gn));
                        break;
                    }
                    case 0x87: { // iPAddress
                        if (gn.size() != 4 && gn.size() != 16) {
                            return unexpected<error>(fail(gat, "an iPAddress that is not 4 or 16 bytes"));
                        }
                        // an IPv4-mapped IPv6 address (::ffff:a.b.c.d) is
                        // its IPv4 address, as Go's net.IP compares it and
                        // as an address asked is taken (address_of)
                        static constexpr unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
                        ip_address a;
                        if (gn.size() == 16 && std::memcmp(gn.data(), mapped, 12) == 0) {
                            std::memcpy(a.bytes.data(), gn.data() + 12, 4);
                            a.size = 4;
                        } else {
                            sgcl::detail::copy_bytes(a.bytes.data(), gn.data(), gn.size());
                            a.size = uint8_t(gn.size());
                        }
                        c.ip_addresses.push_back(a);
                        break;
                    }
                    default:     // otherName, x400Address, directoryName, ediPartyName, registeredID
                        break;
                }
            }
            return {};
        }

        expected<void, error> ext_key_usage_of(DerReader value) noexcept {
            size_t at = value.offset();
            DerReader seq;
            if (!value.read(der::sequence, seq) || !value.empty()) {
                return unexpected<error>(fail(at, "malformed extended key usages"));
            }
            size_t count = 0;
            while (!seq.empty()) {
                DerReader o;
                if (++count > max_ext_key_usages || !seq.read_oid(o)) {
                    return unexpected<error>(fail(at, "malformed extended key usages"));
                }
                ext_key_usage u{};
                if (oid::is(o, oid::any_ext_key_usage)) {
                    u = ext_key_usage::any;
                } else if (o.size() == sizeof oid::kp_prefix + 1 && std::memcmp(o.data(), oid::kp_prefix, sizeof oid::kp_prefix) == 0 && o.data()[7] >= 1 && o.data()[7] <= 9) {
                    static constexpr ext_key_usage kp[] = {ext_key_usage::server_auth, ext_key_usage::client_auth, ext_key_usage::code_signing,
                        ext_key_usage::email_protection, ext_key_usage::ipsec_end_system, ext_key_usage::ipsec_tunnel, ext_key_usage::ipsec_user,
                        ext_key_usage::time_stamping, ext_key_usage::ocsp_signing};
                    u = kp[o.data()[7] - 1];
                } else if (oid::is(o, oid::ms_sgc)) {
                    u = ext_key_usage::microsoft_server_gated_crypto;
                } else if (oid::is(o, oid::ns_sgc)) {
                    u = ext_key_usage::netscape_server_gated_crypto;
                } else if (oid::is(o, oid::ms_commercial_code_signing)) {
                    u = ext_key_usage::microsoft_commercial_code_signing;
                } else if (oid::is(o, oid::ms_kernel_code_signing)) {
                    u = ext_key_usage::microsoft_kernel_code_signing;
                } else {
                    c.unknown_ext_key_usages.push_back(string(oid_text(o.data(), o.size())));
                    continue;
                }
                c.ext_key_usages.push_back(u);
            }
            return {};
        }

        expected<void, error> policies_of(DerReader value) noexcept {
            size_t at = value.offset();
            DerReader seq;
            if (!value.read(der::sequence, seq) || !value.empty()) {
                return unexpected<error>(fail(at, "malformed certificate policies"));
            }
            while (!seq.empty()) {
                DerReader info, o;
                if (c.policies.size() == max_policies || !seq.read(der::sequence, info) || !info.read_oid(o)) {
                    return unexpected<error>(fail(at, "malformed certificate policies"));
                }
                if (!info.empty()) {
                    DerReader qualifiers;
                    if (!info.read(der::sequence, qualifiers) || !info.empty()) {
                        return unexpected<error>(fail(at, "malformed policy qualifiers"));
                    }
                }
                string p(oid_text(o.data(), o.size()));
                for (auto& q : c.policies) {
                    if (q == p) {
                        return unexpected<error>(fail(at, "a certificate policy given twice"));
                    }
                }
                c.policies.push_back(p);
            }
            return {};
        }

        // NameConstraints (RFC 5280 §4.2.1.10): permitted [0] and excluded
        // [1] subtrees of DNS names, IP ranges, mailboxes and URI domains,
        // each checked as Go checks it; a subtree of another kind
        // (directoryName, otherName) makes the extension unhandled, refused
        // where it is critical
        expected<void, error> name_constraints_of(DerReader value, bool critical, bool& unhandled) noexcept {
            (void)critical;
            size_t at = value.offset();
            DerReader seq, permitted, excluded;
            bool has_permitted, has_excluded;
            if (!value.read(der::sequence, seq) || !value.empty() || !seq.read_optional(der::context0, permitted, has_permitted)
                || !seq.read_optional(der::context1, excluded, has_excluded) || !seq.empty()) {
                return unexpected<error>(fail(at, "malformed name constraints"));
            }
            if ((!has_permitted && !has_excluded) || (permitted.empty() && excluded.empty())) {
                return unexpected<error>(fail(at, "empty name constraints"));
            }
            size_t count = 0;
            auto subtrees = [&](DerReader list, vector<string>& dns, vector<ip_range>& ips, vector<string>& emails, vector<string>& uris) -> expected<void, error> {
                while (!list.empty()) {
                    DerReader subtree, base;
                    unsigned char tag;
                    size_t sat = list.offset();
                    if (++count > max_constraint_subtrees) {
                        return unexpected<error>(fail(sat, "more than 256 name constraint subtrees"));
                    }
                    if (!list.read(der::sequence, subtree) || !subtree.read_any(tag, base)) {
                        return unexpected<error>(fail(sat, "a malformed name constraint"));
                    }
                    while (!subtree.empty()) {   // minimum and maximum, which Go and RFC 5280 profiles ignore
                        DerReader skip;
                        unsigned char t;
                        if (!subtree.read_any(t, skip)) {
                            return unexpected<error>(fail(sat, "a malformed name constraint"));
                        }
                    }
                    std::string_view s(reinterpret_cast<const char*>(base.data()), base.size());
                    switch (tag) {
                        case 0x82:   // dNSName
                            if (!ia5_valid(base.data(), base.size()) || !x509_names::domain_valid(s, true)) {
                                return unexpected<error>(fail(sat, "a dNSName constraint that cannot be read"));
                            }
                            dns.push_back(text(base));
                            break;
                        case 0x87: { // iPAddress: an address and a mask
                            if (base.size() != 8 && base.size() != 32) {
                                return unexpected<error>(fail(sat, "an IP constraint that is not 8 or 32 bytes"));
                            }
                            size_t half = base.size() / 2;
                            const unsigned char* p = base.data();
                            bool zero = false;
                            for (size_t i = 0; i < half; ++i) {
                                unsigned char m = p[half + i];
                                if (zero ? m != 0 : !(m == 0xff || m == 0x00 || m == 0x80 || m == 0xc0 || m == 0xe0 || m == 0xf0 || m == 0xf8 || m == 0xfc || m == 0xfe)) {
                                    return unexpected<error>(fail(sat, "an IP constraint with a mask that is not ones then zeros"));
                                }
                                zero = zero || m != 0xff;
                            }
                            static constexpr unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
                            if (half == 16 && std::memcmp(p, mapped, 12) == 0) {
                                return unexpected<error>(fail(sat, "an IP constraint of an IPv4-mapped IPv6 address"));
                            }
                            ip_range r;
                            sgcl::detail::copy_bytes(r.address.bytes.data(), p, half);
                            sgcl::detail::copy_bytes(r.mask.bytes.data(), p + half, half);
                            r.address.size = r.mask.size = uint8_t(half);
                            ips.push_back(r);
                            break;
                        }
                        case 0x81: { // rfc822Name: a mailbox or a domain
                            std::string local, domain;
                            bool ok = ia5_valid(base.data(), base.size())
                                   && (s.find('@') != std::string_view::npos ? x509_names::parse_mailbox(s, local, domain) : x509_names::domain_valid(s, true));
                            if (!ok) {
                                return unexpected<error>(fail(sat, "an rfc822Name constraint that cannot be read"));
                            }
                            emails.push_back(text(base));
                            break;
                        }
                        case 0x86:   // uniformResourceIdentifier: a domain
                            if (!ia5_valid(base.data(), base.size()) || x509_names::looks_like_ipv4(s) || x509_names::looks_like_ipv6(s) || !x509_names::domain_valid(s, true)) {
                                return unexpected<error>(fail(sat, "a URI constraint that cannot be read"));
                            }
                            uris.push_back(text(base));
                            break;
                        default:
                            unhandled = true;
                            break;
                    }
                }
                return {};
            };
            if (auto r = subtrees(permitted, c.permitted_dns, c.permitted_ip, c.permitted_email, c.permitted_uri); !r) {
                return r;
            }
            if (auto r = subtrees(excluded, c.excluded_dns, c.excluded_ip, c.excluded_email, c.excluded_uri); !r) {
                return r;
            }
            c.has_name_constraints = true;
            return {};
        }
    };
}
