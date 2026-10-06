//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// Included by x509.h after certificate: not a header of its own

#include <string>
#include <string_view>

// A certificate request read (PKCS #10, RFC 2986 §4): Go's
// x509.ParseCertificateRequest. The subject, the public key and the
// extensions asked for (an extensionRequest attribute, PKCS #9 §5.4.2, as
// every CA reads them; other attributes are passed over), read by the
// certificate's parser over the same rules and bounds; the signature checked
// by check_signature, never by the parse.
namespace sgcl::crypto::x509 {
    namespace detail {
        inline constexpr unsigned char oid_extension_request[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x0e};   // 1.2.840.113549.1.9.14
    }

    class certificate_request {
    public:
        // A request in DER: errc::malformed with the offset for anything
        // that is not one, or is not strict DER, or passes the bounds of a
        // certificate
        [[nodiscard]] static expected<certificate_request, error> parse(const slice<const byte>& der) noexcept {
            if (der.size() > detail::max_certificate_size) {
                return unexpected<error>(error(errc::malformed, uint64_t(0), string("sgcl::crypto::x509: a certificate request larger than 128 KiB")));
            }
            auto d = make_tracked<detail::CertData>();
            if (auto r = _run(*d, der); !r) {
                return unexpected<error>(r.error());
            }
            // copied after the parse: the parse reads the caller's bytes, which a fuzzer gives at their exact size
            const byte* p = der.data();
            d->raw = vector<byte>(p, p + der.size());
            return certificate_request(tracked_ptr<const detail::CertData>(std::move(d)));
        }

        // The first CERTIFICATE REQUEST block of a PEM text (or NEW
        // CERTIFICATE REQUEST, as old tools write it), the others passed over
        [[nodiscard]] static expected<certificate_request, error> from_pem(const string& text) noexcept {
            optional<encoding::pem> found;
            detail::walk_pem(text, [&](expected<encoding::pem, encoding::error>& block) noexcept {
                if (!block || (block->type() != "CERTIFICATE REQUEST" && block->type() != "NEW CERTIFICATE REQUEST")) {
                    return false;
                }
                found = std::move(*block);
                return true;
            });
            if (!found) {
                return unexpected<error>(error(errc::malformed, string("sgcl::crypto::x509: no CERTIFICATE REQUEST block in the PEM text")));
            }
            return parse(found->bytes().as_slice());
        }

        // The whole request, and its CertificationRequestInfo (what the
        // signature covers), subject and SubjectPublicKeyInfo as bytes
        SGCL_INLINE_HOT slice<const byte> raw() const noexcept {
            return _d->raw.as_slice();
        }

        SGCL_INLINE_HOT slice<const byte> raw_tbs() const noexcept {
            return _d->range(_d->tbs_at, _d->tbs_size);
        }

        SGCL_INLINE_HOT slice<const byte> raw_subject() const noexcept {
            return _d->range(_d->subject_at, _d->subject_size);
        }

        SGCL_INLINE_HOT slice<const byte> raw_subject_public_key_info() const noexcept {
            return _d->range(_d->spki_at, _d->spki_size);
        }

        SGCL_INLINE_HOT const name& subject() const noexcept {
            return _d->subject;
        }

        SGCL_INLINE_HOT const x509::public_key& public_key() const noexcept {
            return _d->key;
        }

        SGCL_INLINE_HOT x509::signature_algorithm signature_algorithm() const noexcept {
            return _d->sig_algorithm;
        }

        SGCL_INLINE_HOT const vector<byte>& signature() const noexcept {
            return _d->signature;
        }

        // The extensions asked for, in their order
        SGCL_INLINE_HOT const vector<extension>& extensions() const noexcept {
            return _d->extensions;
        }

        // The names of the subjectAltName asked for, by kind
        SGCL_INLINE_HOT const vector<string>& dns_names() const noexcept {
            return _d->dns_names;
        }

        SGCL_INLINE_HOT const vector<x509::ip_address>& ip_addresses() const noexcept {
            return _d->ip_addresses;
        }

        SGCL_INLINE_HOT const vector<string>& email_addresses() const noexcept {
            return _d->email_addresses;
        }

        SGCL_INLINE_HOT const vector<string>& uris() const noexcept {
            return _d->uris;
        }

        // Whether the request is signed by the private key of its own
        // public key: errc::verification with insecure_algorithm,
        // unsupported_algorithm or invalid_signature. Go's CheckSignature
        [[nodiscard]] expected<void, error> check_signature() const noexcept {
            const detail::CertData& c = *_d;
            hash_id id = hash_id::sha256;
            if (auto h = detail::signature_hash(c, id); !h) {
                return h;
            }
            const x509::public_key& key = c.key;
            if (!key.has_value()) {
                return unexpected<error>(error(reason::unsupported_algorithm, string("sgcl::crypto::x509: the public key of the certificate request is not one verified here")));
            }
            slice<const byte> tbs = c.range(c.tbs_at, c.tbs_size);
            slice<const byte> sig = c.signature.as_slice();
            bool ok = false;
            bool matched = true;
            switch (c.sig_algorithm) {
                case signature_algorithm::sha256_with_rsa:
                case signature_algorithm::sha384_with_rsa:
                case signature_algorithm::sha512_with_rsa:
                    if ((matched = key.kind() == key_kind::rsa)) {
                        ok = key.rsa().verify_digest(id, crypto::digest(id, tbs), sig);
                    }
                    break;
                case signature_algorithm::sha256_with_rsa_pss:
                case signature_algorithm::sha384_with_rsa_pss:
                case signature_algorithm::sha512_with_rsa_pss:
                    if ((matched = key.kind() == key_kind::rsa)) {
                        ok = key.rsa().verify_digest_pss(id, crypto::digest(id, tbs), sig, digest_size(id));
                    }
                    break;
                case signature_algorithm::ecdsa_with_sha256:
                case signature_algorithm::ecdsa_with_sha384:
                case signature_algorithm::ecdsa_with_sha512:
                    if (key.kind() == key_kind::p256) {
                        ok = key.p256().verify_digest(crypto::digest(id, tbs), sig);
                    } else if (key.kind() == key_kind::p384) {
                        ok = key.p384().verify_digest(crypto::digest(id, tbs), sig);
                    } else if (key.kind() == key_kind::p521) {
                        ok = key.p521().verify_digest(crypto::digest(id, tbs), sig);
                    } else {
                        matched = false;
                    }
                    break;
                case signature_algorithm::ed25519:
                    if ((matched = key.kind() == key_kind::ed25519)) {
                        ok = key.ed25519().verify(tbs, sig);
                    }
                    break;
                default:
                    matched = false;
                    break;
            }
            if (!matched || !ok || !c.signature_whole) {
                return unexpected<error>(error(reason::invalid_signature, string("sgcl::crypto::x509: the signature of the certificate request does not verify under its public key")));
            }
            return {};
        }

        // The same bytes
        SGCL_INLINE_HOT friend bool operator==(const certificate_request& a, const certificate_request& b) noexcept {
            return a._d == b._d || a._d->raw == b._d->raw;
        }

    private:
        tracked_ptr<const detail::CertData> _d;

        SGCL_INLINE_HOT explicit certificate_request(tracked_ptr<const detail::CertData> d) noexcept
        : _d(std::move(d)) {
        }

        // CertificationRequest ::= SEQUENCE { certificationRequestInfo,
        // signatureAlgorithm, signature BIT STRING }, the info SEQUENCE {
        // version 0, subject, subjectPKInfo, attributes [0] IMPLICIT SET OF
        // Attribute }
        static expected<void, error> _run(detail::CertData& c, const slice<const byte>& der) noexcept {
            using namespace detail;
            CertParser p{c, der};
            DerReader in(reinterpret_cast<const unsigned char*>(der.data()), der.size());
            DerReader req, info_el, info, alg;
            if (!in.read(der::sequence, req) || !in.empty()) {
                return unexpected<error>(CertParser::fail(in.offset(), "not a CertificationRequest SEQUENCE, or data after it"));
            }
            if (!req.read_element(der::sequence, info_el)) {
                return unexpected<error>(CertParser::fail(req.offset(), "not a CertificationRequestInfo SEQUENCE"));
            }
            c.tbs_at = p.at_of(info_el);
            c.tbs_size = info_el.size();
            info_el.read(der::sequence, info);
            uint64_t v;
            size_t ver_at = info.offset();
            if (!info.read_small_unsigned(v) || v != 0) {
                return unexpected<error>(CertParser::fail(ver_at, "a certificate request of a version other than 1"));
            }
            c.version = 1;
            DerReader subject_el;
            size_t subject_at = info.offset();
            if (!info.read_element(der::sequence, subject_el)) {
                return unexpected<error>(CertParser::fail(subject_at, "a malformed subject name"));
            }
            c.subject_at = subject_at;
            c.subject_size = subject_el.size();
            if (!parse_name(subject_el, c.subject) || !subject_el.empty()) {
                return unexpected<error>(CertParser::fail(subject_at, "a malformed subject name"));
            }
            size_t spki_at = info.offset();
            DerReader spki_el;
            if (!info.read_element(der::sequence, spki_el)) {
                return unexpected<error>(CertParser::fail(spki_at, "a malformed SubjectPublicKeyInfo"));
            }
            c.spki_at = spki_at;
            c.spki_size = spki_el.size();
            if (auto k = p.public_key_of(spki_el, spki_at); !k) {
                return unexpected<error>(k.error());
            }
            DerReader attrs;
            size_t attrs_at = info.offset();
            if (!info.read(der::context0, attrs) || !info.empty()) {
                return unexpected<error>(CertParser::fail(attrs_at, "malformed attributes"));
            }
            bool seen = false;
            while (!attrs.empty()) {
                size_t at = attrs.offset();
                DerReader attr, o, values;
                if (!attrs.read(der::sequence, attr) || !attr.read_oid(o) || !attr.read(der::set, values) || !attr.empty()) {
                    return unexpected<error>(CertParser::fail(at, "a malformed attribute"));
                }
                if (!oid::is(o, oid_extension_request)) {
                    continue;
                }
                DerReader list;
                if (seen || !values.read(der::sequence, list) || !values.empty()) {
                    return unexpected<error>(CertParser::fail(at, "a malformed extensionRequest, or two of them"));
                }
                seen = true;
                if (auto e = p.extensions_of(list); !e) {
                    return unexpected<error>(e.error());
                }
            }
            size_t alg_at = req.offset();
            if (!req.read(der::sequence, alg)) {
                return unexpected<error>(CertParser::fail(alg_at, "a malformed signature algorithm"));
            }
            if (auto a = p.signature_algorithm_of(alg); !a) {
                return unexpected<error>(a.error());
            }
            const unsigned char* sig;
            size_t sig_n;
            unsigned unused;
            size_t sig_at = req.offset();
            if (!req.read_bit_string(sig, sig_n, unused) || !req.empty()) {
                return unexpected<error>(CertParser::fail(sig_at, "a malformed signature"));
            }
            c.signature = CertParser::copy(sig, sig_n);
            c.signature_whole = unused == 0;
            return {};
        }
    };
}
