//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// Included by x509.h after certificate and certificate_request: not a
// header of its own

#include "../random.h"
#include "../sha1.h"
#include "../sha256.h"
#include "../sha512.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Certificates and certificate requests made (RFC 5280 §4.1, RFC 2986 §4),
// Go's x509.CreateCertificate and x509.CreateCertificateRequest: a template
// of the fields, the public key of the subject, the issuer and the key that
// signs. What is written is DER, the extensions in the order RFC 5280 lists
// them; the result is read back by the module's own parser, so a template
// that cannot make a certificate the parser takes is refused before the
// certificate is handed out.
namespace sgcl::crypto::x509 {
    class signing_key;

    namespace detail {
        struct SignerAccess;
    }

    // A private key a certificate or a request is signed with: a view of
    // one of the module's five kinds, made from the key itself, which must
    // outlive it (as a string_view its string). ECDSA signs over SHA-256 for
    // P-256, SHA-384 for P-384 and SHA-512 for P-521, RSA with PKCS #1 v1.5
    // over SHA-256, as Go and OpenSSL sign by default
    class signing_key {
    public:
        SGCL_INLINE_HOT signing_key(const p256::private_key& k) noexcept
        : _kind(key_kind::p256), _key(&k) {
        }

        SGCL_INLINE_HOT signing_key(const p384::private_key& k) noexcept
        : _kind(key_kind::p384), _key(&k) {
        }

        SGCL_INLINE_HOT signing_key(const p521::private_key& k) noexcept
        : _kind(key_kind::p521), _key(&k) {
        }

        SGCL_INLINE_HOT signing_key(const ed25519::private_key& k) noexcept
        : _kind(key_kind::ed25519), _key(&k) {
        }

        SGCL_INLINE_HOT signing_key(const rsa::private_key& k) noexcept
        : _kind(key_kind::rsa), _key(&k) {
        }

        SGCL_INLINE_HOT key_kind kind() const noexcept {
            return _kind;
        }

        // The SubjectPublicKeyInfo of the key's public half, in DER
        vector<byte> public_key_der() const {
            switch (_kind) {
                case key_kind::p256: return static_cast<const p256::private_key*>(_key)->public_key().to_pkix_der();
                case key_kind::p384: return static_cast<const p384::private_key*>(_key)->public_key().to_pkix_der();
                case key_kind::p521: return static_cast<const p521::private_key*>(_key)->public_key().to_pkix_der();
                case key_kind::ed25519: return static_cast<const ed25519::private_key*>(_key)->public_key().to_pkix_der();
                case key_kind::rsa: return static_cast<const rsa::private_key*>(_key)->public_key().to_pkix_der();
                case key_kind::none: break;
            }
            return {};
        }

    private:
        friend struct detail::SignerAccess;

        // The AlgorithmIdentifier of the signature (DER) and the signature
        // of the bytes
        std::vector<unsigned char> _algorithm_der() const;
        vector<byte> _sign(const slice<const byte>& data) const;
        secret_bytes _pkcs8_der() const;

        key_kind _kind;
        const void* _key;
    };

    namespace detail {
        struct SignerAccess {
            SGCL_INLINE_HOT static std::vector<unsigned char> algorithm(const signing_key& k) {
                return k._algorithm_der();
            }

            SGCL_INLINE_HOT static vector<byte> sign(const signing_key& k, const slice<const byte>& data) {
                return k._sign(data);
            }

            // The key's PKCS #8 PrivateKeyInfo (pkcs12, cms), in plain memory
            SGCL_INLINE_HOT static secret_bytes pkcs8(const signing_key& k) {
                return k._pkcs8_der();
            }

            SGCL_INLINE_HOT static const void* key(const signing_key& k) noexcept {
                return k._key;
            }
        };
    }

    // The fields of a certificate to make (Go's x509.Certificate as a
    // template). Defaults: a serial of 128 random bits, valid from now for a
    // year, keyUsage digitalSignature (and keyEncipherment for an RSA key)
    // for a leaf, keyCertSign, cRLSign and digitalSignature for a CA
    struct certificate_template {
        vector<byte> serial_number;                 // big-endian, positive; empty: 16 random bytes
        string common_name;                         // the subject's CN; empty: none
        vector<string> organization;                // the subject's O
        optional<time::datetime> not_before;        // none: now
        optional<time::datetime> not_after;         // none: a year after not_before
        vector<string> dns_names;                   // subjectAltName
        vector<x509::ip_address> ip_addresses;
        vector<string> email_addresses;
        vector<string> uris;
        bool is_ca = false;                         // basicConstraints cA
        optional<int64_t> max_path_length;          // a CA's pathLenConstraint; none: no limit
        optional<x509::key_usage> key_usage;        // none: the defaults above
        vector<ext_key_usage> ext_key_usages;       // extKeyUsage; empty: none written
        vector<extension> extensions;               // more, written after the module's own (an OID of these replaces the module's)
    };

    // The fields of a certificate request to make (PKCS #10): the subject
    // and the names the certificate is asked for, as Go's
    // x509.CertificateRequest
    struct certificate_request_template {
        string common_name;
        vector<string> organization;
        vector<string> dns_names;
        vector<x509::ip_address> ip_addresses;
        vector<string> email_addresses;
        vector<string> uris;
        vector<extension> extensions;               // more extensions asked for (extensionRequest)
    };

    namespace detail {
        using DerBytes = std::vector<unsigned char>;

        SGCL_INLINE_HOT void der_append(DerBytes& out, const unsigned char* p, size_t n) {
            size_t at = out.size();
            out.resize(at + n);
            if (n) {
                sgcl::detail::copy_bytes(out.data() + at, p, n);
            }
        }

        SGCL_INLINE_HOT void der_append(DerBytes& out, const DerBytes& b) {
            der_append(out, b.data(), b.size());
        }

        inline void der_header(DerBytes& out, unsigned char tag, size_t len) {
            out.push_back(tag);
            if (len < 0x80) {
                out.push_back(static_cast<unsigned char>(len));
                return;
            }
            unsigned char k = 0;
            for (size_t l = len; l; l >>= 8) {
                ++k;
            }
            out.push_back(static_cast<unsigned char>(0x80 | k));
            for (int i = k - 1; i >= 0; --i) {
                out.push_back(static_cast<unsigned char>(len >> (8 * i)));
            }
        }

        inline DerBytes der_tlv(unsigned char tag, const unsigned char* p, size_t n) {
            DerBytes out;
            out.reserve(n + 6);
            der_header(out, tag, n);
            der_append(out, p, n);
            return out;
        }

        SGCL_INLINE_HOT DerBytes der_tlv(unsigned char tag, const DerBytes& content) {
            return der_tlv(tag, content.data(), content.size());
        }

        SGCL_INLINE_HOT DerBytes der_tlv(unsigned char tag, std::string_view text) {
            return der_tlv(tag, reinterpret_cast<const unsigned char*>(text.data()), text.size());
        }

        // An INTEGER of a big-endian magnitude, in its shortest form
        inline DerBytes der_unsigned(const unsigned char* p, size_t n) {
            while (n > 1 && p[0] == 0) {
                ++p;
                --n;
            }
            DerBytes c;
            if (n == 0) {
                c.push_back(0);
            } else {
                if (p[0] & 0x80) {
                    c.push_back(0);
                }
                der_append(c, p, n);
            }
            return der_tlv(der::integer, c);
        }

        inline DerBytes der_small(uint64_t v) {
            unsigned char b[8];
            for (int i = 0; i < 8; ++i) {
                b[i] = static_cast<unsigned char>(v >> (56 - 8 * i));
            }
            return der_unsigned(b, 8);
        }

        // The content of an OBJECT IDENTIFIER of a dotted text: false for a
        // text that is not one (fewer than two arcs, a first arc past 2, a
        // second past 39 under 0 and 1, an arc that is not digits)
        inline bool oid_der(std::string_view dotted, DerBytes& out) {
            std::vector<uint64_t> arcs;
            size_t at = 0;
            while (at <= dotted.size()) {
                size_t dot = dotted.find('.', at);
                if (dot == std::string_view::npos) {
                    dot = dotted.size();
                }
                if (dot == at || dot - at > 19) {
                    return false;
                }
                uint64_t v = 0;
                for (size_t i = at; i < dot; ++i) {
                    if (dotted[i] < '0' || dotted[i] > '9') {
                        return false;
                    }
                    v = v * 10 + uint64_t(dotted[i] - '0');
                }
                arcs.push_back(v);
                at = dot + 1;
            }
            if (arcs.size() < 2 || arcs[0] > 2 || (arcs[0] < 2 && arcs[1] > 39) || arcs[1] > UINT64_MAX - 80) {
                return false;
            }
            auto put = [&](uint64_t v) {
                unsigned char tmp[10];
                int k = 0;
                do {
                    tmp[k++] = static_cast<unsigned char>(v & 0x7f);
                    v >>= 7;
                } while (v);
                while (k > 1) {
                    out.push_back(static_cast<unsigned char>(tmp[--k] | 0x80));
                }
                out.push_back(tmp[0]);
            };
            put(arcs[0] * 40 + arcs[1]);
            for (size_t i = 2; i < arcs.size(); ++i) {
                put(arcs[i]);
            }
            return true;
        }

        template<size_t N>
        SGCL_INLINE_HOT DerBytes der_oid(const unsigned char (&o)[N]) {
            return der_tlv(der::object_identifier, o, N);
        }

        inline DerBytes der_oid_text(std::string_view dotted, const char* what) {
            DerBytes c;
            if (!oid_der(dotted, c)) {
                throw std::invalid_argument(std::string("sgcl::crypto::x509: ") + what + " is not an OID: " + std::string(dotted));
            }
            return der_tlv(der::object_identifier, c);
        }

        // PrintableString when every character is of its set, else
        // UTF8String (RFC 5280 §4.1.2.4, as Go chooses)
        inline DerBytes der_directory_string(std::string_view s) {
            bool printable = true;
            for (unsigned char ch : s) {
                bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '\'' || ch == '(' || ch == ')'
                    || ch == '+' || ch == ',' || ch == '-' || ch == '.' || ch == '/' || ch == ':' || ch == '=' || ch == '?';
                printable &= ok;
            }
            return der_tlv(printable ? der::printable_string : der::utf8_string, s);
        }

        inline constexpr unsigned char oid_cn[] = {0x55, 0x04, 0x03};
        inline constexpr unsigned char oid_o[] = {0x55, 0x04, 0x0a};
        inline constexpr unsigned char oid_ski[] = {0x55, 0x1d, 0x0e};
        inline constexpr unsigned char oid_key_usage[] = {0x55, 0x1d, 0x0f};
        inline constexpr unsigned char oid_san[] = {0x55, 0x1d, 0x11};
        inline constexpr unsigned char oid_basic_constraints[] = {0x55, 0x1d, 0x13};
        inline constexpr unsigned char oid_aki[] = {0x55, 0x1d, 0x23};
        inline constexpr unsigned char oid_ext_key_usage[] = {0x55, 0x1d, 0x25};

        // A Name of a CN and the Os, each its own RDN, the Os first
        inline DerBytes der_name(const string& common_name, const vector<string>& organization) {
            DerBytes rdns;
            auto rdn = [&](const DerBytes& type, std::string_view value) {
                DerBytes atv = type;
                der_append(atv, der_directory_string(value));
                der_append(rdns, der_tlv(der::set, der_tlv(der::sequence, atv)));
            };
            for (auto& o : organization) {
                rdn(der_oid(oid_o), o.view());
            }
            if (!common_name.empty()) {
                rdn(der_oid(oid_cn), common_name.view());
            }
            return der_tlv(der::sequence, rdns);
        }

        // UTCTime for the years 1950 to 2049, GeneralizedTime outside (§4.1.2.5)
        inline DerBytes der_time(const time::datetime& t) {
            const auto u = time::datetime::from_unix(t.unix(), time::zone::utc());
            const int y = u.year();
            char b[16];
            auto two = [](char* p, int v) {
                p[0] = char('0' + v / 10);
                p[1] = char('0' + v % 10);
            };
            size_t n = 0;
            if (y >= 1950 && y < 2050) {
                two(b, y % 100);
                n = 2;
            } else {
                two(b, y / 100);
                two(b + 2, y % 100);
                n = 4;
            }
            two(b + n, int(u.month()));
            two(b + n + 2, u.day());
            two(b + n + 4, u.hour());
            two(b + n + 6, u.minute());
            two(b + n + 8, u.second());
            b[n + 10] = 'Z';
            return der_tlv(y >= 1950 && y < 2050 ? der::utc_time : der::generalized_time, std::string_view(b, n + 11));
        }

        inline void require_ia5(std::string_view s, const char* what) {
            for (unsigned char ch : s) {
                if (ch >= 0x80) {
                    throw std::invalid_argument(std::string("sgcl::crypto::x509: ") + what + " is not ASCII (an IDN goes as its A-label): " + std::string(s));
                }
            }
            if (s.empty()) {
                throw std::invalid_argument(std::string("sgcl::crypto::x509: an empty ") + what);
            }
        }

        // The GeneralNames of a subjectAltName
        inline DerBytes der_san(const vector<string>& dns, const vector<ip_address>& ips, const vector<string>& emails, const vector<string>& uris) {
            DerBytes names;
            for (auto& e : emails) {
                require_ia5(e.view(), "email address");
                der_append(names, der_tlv(0x81, e.view()));
            }
            for (auto& d : dns) {
                require_ia5(d.view(), "DNS name");
                der_append(names, der_tlv(0x82, d.view()));
            }
            for (auto& u : uris) {
                require_ia5(u.view(), "URI");
                der_append(names, der_tlv(0x86, u.view()));
            }
            for (auto& a : ips) {
                if (a.size != 4 && a.size != 16) {
                    throw std::invalid_argument("sgcl::crypto::x509: an IP address of neither 4 nor 16 bytes");
                }
                der_append(names, der_tlv(0x87, reinterpret_cast<const unsigned char*>(a.bytes.data()), a.size));
            }
            return der_tlv(der::sequence, names);
        }

        inline DerBytes der_extension(const DerBytes& oid, bool critical, const DerBytes& value) {
            DerBytes e = oid;
            if (critical) {
                const unsigned char t[] = {der::boolean, 1, 0xff};
                der_append(e, t, 3);
            }
            der_append(e, der_tlv(der::octet_string, value));
            return der_tlv(der::sequence, e);
        }

        // A BIT STRING of the bits of keyUsage, its trailing zero bits
        // unused (DER, X.690 §11.2.2)
        inline DerBytes der_key_usage(uint16_t bits) {
            unsigned char b[3] = {0, 0, 0};
            // bit 0 is the most significant bit of the first byte
            for (int i = 0; i < 9; ++i) {
                if (bits & (1u << i)) {
                    b[1 + i / 8] |= static_cast<unsigned char>(0x80 >> (i % 8));
                }
            }
            size_t n = b[2] ? 2 : b[1] ? 1 : 0;
            unsigned unused = 0;
            if (n) {
                unsigned char last = b[n];
                while (!(last & 1)) {
                    last >>= 1;
                    ++unused;
                }
            }
            b[0] = static_cast<unsigned char>(unused);
            return der_tlv(der::bit_string, b, n + 1);
        }

        inline DerBytes der_ext_key_usage(const vector<ext_key_usage>& usages) {
            DerBytes list;
            for (auto u : usages) {
                switch (u) {
                    case ext_key_usage::any: der_append(list, der_oid(oid::any_ext_key_usage)); break;
                    case ext_key_usage::microsoft_server_gated_crypto: der_append(list, der_oid(oid::ms_sgc)); break;
                    case ext_key_usage::netscape_server_gated_crypto: der_append(list, der_oid(oid::ns_sgc)); break;
                    case ext_key_usage::microsoft_commercial_code_signing: der_append(list, der_oid(oid::ms_commercial_code_signing)); break;
                    case ext_key_usage::microsoft_kernel_code_signing: der_append(list, der_oid(oid::ms_kernel_code_signing)); break;
                    default: {
                        unsigned k = unsigned(u) - unsigned(ext_key_usage::server_auth) + 1;   // 1.3.6.1.5.5.7.3.k
                        if (k < 1 || k > 9) {
                            throw std::invalid_argument("sgcl::crypto::x509: an extended key usage of no value of its enumeration");
                        }
                        DerBytes o(oid::kp_prefix, oid::kp_prefix + sizeof oid::kp_prefix);
                        o.push_back(static_cast<unsigned char>(k));
                        der_append(list, der_tlv(der::object_identifier, o));
                    }
                }
            }
            return der_tlv(der::sequence, list);
        }

        // The subjectPublicKey's bits of a SubjectPublicKeyInfo, for the
        // key identifier (RFC 5280 §4.2.1.2, method 1: SHA-1 of them)
        inline DerBytes key_identifier(const slice<const byte>& spki) {
            DerReader in(reinterpret_cast<const unsigned char*>(spki.data()), spki.size()), seq, alg;
            const unsigned char* bits;
            size_t n;
            unsigned unused;
            if (!in.read(der::sequence, seq) || !seq.read(der::sequence, alg) || !seq.read_bit_string(bits, n, unused)) {
                throw std::invalid_argument("sgcl::crypto::x509: a public key that is not a SubjectPublicKeyInfo in DER");
            }
            auto h = sha1::of(slice<const byte>(reinterpret_cast<const byte*>(bits), n));
            return DerBytes(reinterpret_cast<const unsigned char*>(h.data()), reinterpret_cast<const unsigned char*>(h.data()) + h.size());
        }

        SGCL_INLINE_HOT slice<const byte> as_bytes(const DerBytes& b) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(b.data()), b.size());
        }

        // The signed structure: SEQUENCE { tbs, algorithm, BIT STRING signature }
        inline DerBytes der_signed(const DerBytes& tbs, const signing_key& key) {
            auto sig = SignerAccess::sign(key, as_bytes(tbs));
            DerBytes body = tbs;
            der_append(body, SignerAccess::algorithm(key));
            DerBytes bits;
            bits.push_back(0);
            der_append(bits, reinterpret_cast<const unsigned char*>(sig.data()), sig.size());
            der_append(body, der_tlv(der::bit_string, bits));
            return der_tlv(der::sequence, body);
        }

        // The extensions of the template the module does not write itself,
        // and whether one replaces the module's of its OID
        inline bool overridden(const vector<extension>& extra, const unsigned char* o, size_t n) {
            const std::string text = oid_text(o, n);
            for (auto& e : extra) {
                if (e.oid.view() == text) {
                    return true;
                }
            }
            return false;
        }

        inline void der_extra(DerBytes& list, const vector<extension>& extra) {
            for (size_t i = 0; i < extra.size(); ++i) {
                for (size_t j = 0; j < i; ++j) {
                    if (extra[j].oid == extra[i].oid) {
                        throw std::invalid_argument("sgcl::crypto::x509: an extension given twice: " + std::string(extra[i].oid.view()));
                    }
                }
                const auto& e = extra[i];
                DerBytes value(reinterpret_cast<const unsigned char*>(e.value.data()), reinterpret_cast<const unsigned char*>(e.value.data()) + e.value.size());
                der_append(list, der_extension(der_oid_text(e.oid.view(), "an extension's OID"), e.critical, value));
            }
        }
    }

    inline std::vector<unsigned char> signing_key::_algorithm_der() const {
        using namespace detail;
        switch (_kind) {
            case key_kind::p256: return der_tlv(der::sequence, der_oid(oid::ecdsa_sha256));
            case key_kind::p384: return der_tlv(der::sequence, der_oid(oid::ecdsa_sha384));
            case key_kind::p521: return der_tlv(der::sequence, der_oid(oid::ecdsa_sha512));
            case key_kind::ed25519: return der_tlv(der::sequence, der_oid(oid::ed25519));
            case key_kind::rsa: {
                DerBytes a = der_oid(oid::sha256_rsa);
                const unsigned char null[] = {der::null, 0};
                der_append(a, null, 2);
                return der_tlv(der::sequence, a);
            }
            case key_kind::none: break;
        }
        return {};
    }

    inline secret_bytes signing_key::_pkcs8_der() const {
        switch (_kind) {
            case key_kind::p256: return static_cast<const p256::private_key*>(_key)->to_pkcs8_der();
            case key_kind::p384: return static_cast<const p384::private_key*>(_key)->to_pkcs8_der();
            case key_kind::p521: return static_cast<const p521::private_key*>(_key)->to_pkcs8_der();
            case key_kind::ed25519: return static_cast<const ed25519::private_key*>(_key)->to_pkcs8_der();
            case key_kind::rsa: return static_cast<const rsa::private_key*>(_key)->to_pkcs8_der();
            case key_kind::none: break;
        }
        return {};
    }

    inline vector<byte> signing_key::_sign(const slice<const byte>& data) const {
        switch (_kind) {
            case key_kind::p256: {
                auto d = sha256::of(data);
                return static_cast<const p256::private_key*>(_key)->sign_digest(slice<const byte>(d.data(), d.size()));
            }
            case key_kind::p384: {
                auto d = sha384::of(data);
                return static_cast<const p384::private_key*>(_key)->sign_digest(slice<const byte>(d.data(), d.size()));
            }
            case key_kind::p521: {
                auto d = sha512::of(data);
                return static_cast<const p521::private_key*>(_key)->sign_digest(slice<const byte>(d.data(), d.size()));
            }
            case key_kind::ed25519: {
                auto s = static_cast<const ed25519::private_key*>(_key)->sign(data);
                return vector<byte>(s.data(), s.data() + s.size());
            }
            case key_kind::rsa: {
                auto d = sha256::of(data);
                return static_cast<const rsa::private_key*>(_key)->sign_digest(hash_id::sha256, slice<const byte>(d.data(), d.size()));
            }
            case key_kind::none: break;
        }
        return {};
    }

    namespace detail {
        inline certificate make_certificate(const certificate_template& t, const slice<const byte>& subject_key, const certificate* issuer, const signing_key& key) {
            DerBytes tbs;
            // version v3
            der_append(tbs, der_tlv(der::context0, der_small(2)));
            // serialNumber
            if (t.serial_number.empty()) {
                unsigned char r[16];
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(r), 16));
                r[0] &= 0x7f;
                r[0] |= 0x40;   // 16 bytes, positive, never shortened
                der_append(tbs, der_unsigned(r, 16));
            } else {
                if (t.serial_number.size() > 20 || (t.serial_number[0] & byte(0x80)) != byte(0)) {
                    throw std::invalid_argument("sgcl::crypto::x509: a serial number past 20 bytes or negative (RFC 5280 §4.1.2.2)");
                }
                der_append(tbs, der_unsigned(reinterpret_cast<const unsigned char*>(t.serial_number.data()), t.serial_number.size()));
            }
            // signature
            der_append(tbs, SignerAccess::algorithm(key));
            // issuer: the issuer's subject as its certificate has it
            DerBytes subject = der_name(t.common_name, t.organization);
            if (issuer) {
                auto raw = issuer->raw_subject();
                der_append(tbs, reinterpret_cast<const unsigned char*>(raw.data()), raw.size());
            } else {
                der_append(tbs, subject);
            }
            // validity
            const time::datetime from = t.not_before ? *t.not_before : time::now();
            const time::datetime to = t.not_after ? *t.not_after : from + 365 * 24 * hour;
            if (to.unix() < from.unix()) {
                throw std::invalid_argument("sgcl::crypto::x509: not_after before not_before");
            }
            {
                DerBytes v = der_time(from);
                der_append(v, der_time(to));
                der_append(tbs, der_tlv(der::sequence, v));
            }
            der_append(tbs, subject);
            der_append(tbs, reinterpret_cast<const unsigned char*>(subject_key.data()), subject_key.size());
            // extensions
            DerBytes list;
            const DerBytes ski = key_identifier(subject_key);
            if (!overridden(t.extensions, oid_ski, sizeof oid_ski)) {
                der_append(list, der_extension(der_oid(oid_ski), false, der_tlv(der::octet_string, ski)));
            }
            if (issuer && !issuer->subject_key_id().empty() && !overridden(t.extensions, oid_aki, sizeof oid_aki)) {
                const auto& id = issuer->subject_key_id();
                DerBytes aki = der_tlv(0x80, reinterpret_cast<const unsigned char*>(id.data()), id.size());
                der_append(list, der_extension(der_oid(oid_aki), false, der_tlv(der::sequence, aki)));
            }
            if (!overridden(t.extensions, oid_key_usage, sizeof oid_key_usage)) {
                uint16_t bits;
                if (t.key_usage) {
                    bits = uint16_t(*t.key_usage);
                } else if (t.is_ca) {
                    bits = uint16_t(key_usage::cert_sign | key_usage::crl_sign | key_usage::digital_signature);
                } else {
                    bits = uint16_t(key_usage::digital_signature);
                    DerReader in(reinterpret_cast<const unsigned char*>(subject_key.data()), subject_key.size()), seq, alg, o;
                    if (in.read(der::sequence, seq) && seq.read(der::sequence, alg) && alg.read_oid(o) && oid::is(o, oid::rsa_encryption)) {
                        bits |= uint16_t(key_usage::key_encipherment);
                    }
                }
                if (bits) {
                    der_append(list, der_extension(der_oid(oid_key_usage), true, der_key_usage(bits)));
                }
            }
            if (!t.ext_key_usages.empty() && !overridden(t.extensions, oid_ext_key_usage, sizeof oid_ext_key_usage)) {
                der_append(list, der_extension(der_oid(oid_ext_key_usage), false, der_ext_key_usage(t.ext_key_usages)));
            }
            if (!overridden(t.extensions, oid_basic_constraints, sizeof oid_basic_constraints)) {
                DerBytes bc;
                if (t.is_ca) {
                    const unsigned char yes[] = {der::boolean, 1, 0xff};
                    der_append(bc, yes, 3);
                    if (t.max_path_length) {
                        if (*t.max_path_length < 0) {
                            throw std::invalid_argument("sgcl::crypto::x509: a negative max_path_length");
                        }
                        der_append(bc, der_small(uint64_t(*t.max_path_length)));
                    }
                }
                der_append(list, der_extension(der_oid(oid_basic_constraints), true, der_tlv(der::sequence, bc)));
            }
            const bool names = !t.dns_names.empty() || !t.ip_addresses.empty() || !t.email_addresses.empty() || !t.uris.empty();
            if (names && !overridden(t.extensions, oid_san, sizeof oid_san)) {
                // critical when the subject is empty (§4.2.1.6)
                const bool empty_subject = t.common_name.empty() && t.organization.empty();
                der_append(list, der_extension(der_oid(oid_san), empty_subject, der_san(t.dns_names, t.ip_addresses, t.email_addresses, t.uris)));
            }
            der_extra(list, t.extensions);
            der_append(tbs, der_tlv(der::context3, der_tlv(der::sequence, list)));
            DerBytes der = der_signed(der_tlv(der::sequence, tbs), key);
            auto c = certificate::parse(as_bytes(der));
            if (!c) {
                throw std::invalid_argument("sgcl::crypto::x509: the template makes a certificate the parser refuses: " + std::string(c.error().message().view()));
            }
            return std::move(*c);
        }
    }

    // A certificate of the template for the subject's public key (a
    // SubjectPublicKeyInfo in DER: to_pkix_der() of a public key), issued by
    // the CA `issuer` and signed with its key: Go's x509.CreateCertificate.
    // The issuer's subject becomes the certificate's issuer, its
    // subjectKeyIdentifier the authorityKeyIdentifier. The key is not
    // checked against the issuer's certificate: a certificate signed by
    // another key does not verify. std::invalid_argument for a template that
    // cannot be written (not_after before not_before, a serial past 20
    // bytes, a name that is not ASCII, an OID that is not one)
    inline certificate create_certificate(const certificate_template& t, const slice<const byte>& public_key_der, const certificate& issuer, const signing_key& issuer_key) {
        return detail::make_certificate(t, public_key_der, &issuer, issuer_key);
    }

    // A self-signed certificate of the template: its subject is its issuer,
    // its key the key's public half
    inline certificate create_certificate(const certificate_template& t, const signing_key& key) {
        auto spki = key.public_key_der();
        return detail::make_certificate(t, spki.as_slice(), nullptr, key);
    }

    // A certificate request (PKCS #10, RFC 2986) of the template, for the
    // key's public half and signed with it: Go's
    // x509.CreateCertificateRequest. The names go into an extensionRequest
    // of a subjectAltName, as CAs (ACME's finalize among them) read them.
    // std::invalid_argument for a template that cannot be written
    inline certificate_request create_certificate_request(const certificate_request_template& t, const signing_key& key) {
        using namespace detail;
        DerBytes info = der_small(0);
        der_append(info, der_name(t.common_name, t.organization));
        auto spki = key.public_key_der();
        der_append(info, reinterpret_cast<const unsigned char*>(spki.data()), spki.size());
        DerBytes list;
        const bool names = !t.dns_names.empty() || !t.ip_addresses.empty() || !t.email_addresses.empty() || !t.uris.empty();
        if (names && !overridden(t.extensions, oid_san, sizeof oid_san)) {
            der_append(list, der_extension(der_oid(oid_san), false, der_san(t.dns_names, t.ip_addresses, t.email_addresses, t.uris)));
        }
        der_extra(list, t.extensions);
        DerBytes attributes;
        if (!list.empty()) {
            DerBytes attr = der_oid(oid_extension_request);
            der_append(attr, der_tlv(der::set, der_tlv(der::sequence, list)));
            attributes = der_tlv(der::sequence, attr);
        }
        der_append(info, der_tlv(der::context0, attributes));
        DerBytes der = der_signed(der_tlv(der::sequence, info), key);
        auto r = certificate_request::parse(as_bytes(der));
        if (!r) {
            throw std::invalid_argument("sgcl::crypto::x509: the template makes a request the parser refuses: " + std::string(r.error().message().view()));
        }
        return std::move(*r);
    }
}
