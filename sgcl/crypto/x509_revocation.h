//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/x509_revocation.h"
#include "random.h"
#include "x509.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../encoding/base64.h"
#include "../encoding/pem.h"
#include "../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

// Revocation (§11 of the design's net 1.0.0 plan): OCSP (RFC 6960) and
// CRLs (RFC 5280 §5), what Go has in golang.org/x/crypto/ocsp and in
// crypto/x509's RevocationList. Everything here is offline: a request
// written, a response or a CRL read and verified against the certificate
// and its issuer. Fetching them is net::tls's (its config's revocation
// setting, the OCSP staple of a server's identity), or the program's over
// an http::client.
//
//   auto req = crypto::x509::ocsp_request::make(cert, issuer);        // POST req->raw() or GET req->url(responder)
//   auto resp = crypto::x509::ocsp_response::parse(body);
//   auto single = resp->verify(cert, issuer);                         // the signer, the CertID, the times
//   if (single && single->status == crypto::x509::revocation_status::good) { ... }
//
//   auto crl = crypto::x509::revocation_list::parse(der);
//   auto status = crl->status_of(cert, issuer);                       // the signature, the scope, the times
//
// README: docs/sgcl/crypto/x509-ocsp_response/README.md,
// docs/sgcl/crypto/x509-revocation_list/README.md
namespace sgcl::crypto::x509 {
    // How an OCSP request is made: the hash of its CertID (SHA-1, what
    // responders are required to take; SHA-256, SHA-384, SHA-512) and
    // whether it carries a nonce of 16 random bytes (RFC 8954), which the
    // response is then asked to echo
    struct ocsp_request_options {
        hash_id hash = hash_id::sha1;
        bool nonce = false;
    };

    // An OCSP request (RFC 6960 §4.1) of one certificate: a value that
    // costs a pointer to copy. raw() is the body of a POST
    // (application/ocsp-request), url() the GET of Appendix A
    class ocsp_request {
    public:
        // The request of the certificate's status from its issuer's
        // responder. errc::verification (unknown_authority) when issuer is
        // not the certificate's issuer by name; std::invalid_argument for a
        // hash other than SHA-1 and SHA-2
        [[nodiscard]] static expected<ocsp_request, error> make(const certificate& cert, const certificate& issuer, const ocsp_request_options& o = {}) {
            const auto& c = detail::PoolAccess::data(cert);
            const auto& i = detail::PoolAccess::data(issuer);
            if (!detail::same_slice(cert.raw_issuer(), issuer.raw_subject())) {
                return unexpected<error>(detail::reject(reason::unknown_authority, "the issuer given, " + detail::describe(i) + ", is not the issuer of " + detail::describe(c)));
            }
            const unsigned char* kp;
            size_t kn;
            if (!detail::key_bits(issuer.raw_subject_public_key_info(), kp, kn)) {
                return unexpected<error>(error(errc::malformed, string("sgcl::crypto::x509: the issuer's public key cannot be read")));
            }
            const unsigned char* op;
            size_t on;
            detail::hash_oid(o.hash, op, on);
            auto d = make_tracked<detail::OcspRequestData>();
            d->hash = o.hash;
            d->name_hash = digest(o.hash, issuer.raw_subject());
            d->key_hash = digest(o.hash, slice<const byte>(reinterpret_cast<const byte*>(kp), kn));
            d->serial = c.serial;
            if (o.nonce) {
                d->nonce = random::bytes(16);
            }
            d->raw = detail::write_ocsp_request(d->hash, d->name_hash, d->key_hash, d->serial, d->nonce);
            return ocsp_request(std::move(d));
        }

        // A request in DER of exactly one certificate, as Go's ParseRequest
        // takes; errc::malformed with the offset for anything else
        [[nodiscard]] static expected<ocsp_request, error> parse(const slice<const byte>& der) noexcept {
            if (der.size() > detail::max_ocsp_size) {
                return unexpected<error>(detail::ocsp_fail(0, "an OCSP request larger than 1 MiB"));
            }
            auto d = make_tracked<detail::OcspRequestData>();
            // parsed from the caller's bytes, copied after: a fuzzer's exact buffer shows an overread
            if (auto r = detail::parse_ocsp_request(*d, der); !r) {
                return unexpected<error>(r.error());
            }
            d->raw = vector<byte>(der.data(), der.data() + der.size());
            return ocsp_request(std::move(d));
        }

        // The DER: the body of a POST
        SGCL_INLINE_HOT slice<const byte> raw() const noexcept {
            return _d->raw.as_slice();
        }

        // The GET of RFC 6960 Appendix A.1: the responder's URL, a '/', and
        // the base64 of the DER with '+', '/' and '=' escaped
        string url(const string& responder) const {
            std::string u(responder.view());
            if (u.empty() || u.back() != '/') {
                u += '/';
            }
            for (char ch : encoding::base64::standard.encode(raw()).view()) {
                switch (ch) {
                    case '+': u += "%2B"; break;
                    case '/': u += "%2F"; break;
                    case '=': u += "%3D"; break;
                    default: u += ch; break;
                }
            }
            return string(u);
        }

        // The CertID: the hash, the hashes of the issuer's name and key,
        // the certificate's serial number
        SGCL_INLINE_HOT hash_id hash() const noexcept {
            return _d->hash;
        }

        SGCL_INLINE_HOT const vector<byte>& issuer_name_hash() const noexcept {
            return _d->name_hash;
        }

        SGCL_INLINE_HOT const vector<byte>& issuer_key_hash() const noexcept {
            return _d->key_hash;
        }

        SGCL_INLINE_HOT const vector<byte>& serial_number() const noexcept {
            return _d->serial;
        }

        // The nonce the request carries; empty for none
        SGCL_INLINE_HOT const vector<byte>& nonce() const noexcept {
            return _d->nonce;
        }

        // Whether the request names this certificate of this issuer
        [[nodiscard]] bool matches(const certificate& cert, const certificate& issuer) const noexcept {
            return detail::cert_id_matches(_d->hash, _d->name_hash, _d->key_hash, _d->serial, detail::PoolAccess::data(cert), detail::PoolAccess::data(issuer));
        }

    private:
        tracked_ptr<const detail::OcspRequestData> _d;

        SGCL_INLINE_HOT explicit ocsp_request(tracked_ptr<const detail::OcspRequestData> d) noexcept
        : _d(std::move(d)) {
        }
    };

    // What ocsp_response::verify checks the times and the nonce against:
    //
    //   time     the instant the response must be current at; nullopt: now
    //   skew     how far the clocks may differ (thisUpdate ahead, nextUpdate
    //            behind); 5 minutes
    //   max_age  how long after its thisUpdate a response without a
    //            nextUpdate is current; 7 days
    //   nonce    the request's nonce the response must carry; empty: not
    //            asked for
    struct ocsp_verify_options {
        optional<time::datetime> time;
        duration skew = 5 * minute;
        duration max_age = 7 * 24 * hour;
        vector<byte> nonce;
    };

    // An OCSP response (RFC 6960 §4.2): read once, never changed, a value
    // that costs a pointer to copy. One that is not successful (try_later,
    // unauthorized...) parses too and holds nothing but its status
    class ocsp_response {
    public:
        // A response in DER (a BasicOCSPResponse inside): errc::malformed
        // with the offset for anything that is not one, or is not strict
        // DER, or passes the bounds (1 MiB, 256 single responses, 16
        // certificates); errc::unsupported for a response of another type
        [[nodiscard]] static expected<ocsp_response, error> parse(const slice<const byte>& der) noexcept {
            if (der.size() > detail::max_ocsp_size) {
                return unexpected<error>(detail::ocsp_fail(0, "an OCSP response larger than 1 MiB"));
            }
            auto d = make_tracked<detail::OcspData>();
            // parsed from the caller's bytes, copied after: a fuzzer's exact buffer shows an overread
            if (auto r = detail::parse_ocsp_response(*d, der); !r) {
                return unexpected<error>(r.error());
            }
            d->raw = vector<byte>(der.data(), der.data() + der.size());
            return ocsp_response(std::move(d));
        }

        // Whether the responder answered with a status
        SGCL_INLINE_HOT ocsp_response_status status() const noexcept {
            return _d->status;
        }

        // The whole response, and its ResponseData (what the signature
        // covers); empty for a response that is not successful
        SGCL_INLINE_HOT slice<const byte> raw() const noexcept {
            return _d->raw.as_slice();
        }

        SGCL_INLINE_HOT slice<const byte> raw_tbs() const noexcept {
            return _d->raw.as_slice().subslice(_d->tbs_at, _d->tbs_size);
        }

        // The responder: its Name's DER (byName) or the SHA-1 of its key
        // (byKey), the other empty
        SGCL_INLINE_HOT const vector<byte>& responder_name() const noexcept {
            return _d->responder_name;
        }

        SGCL_INLINE_HOT const vector<byte>& responder_key_hash() const noexcept {
            return _d->responder_key_hash;
        }

        // When the responder signed it, in UTC
        SGCL_INLINE_HOT time::datetime produced_at() const noexcept {
            return detail::utc(_d->produced_at);
        }

        // Every SingleResponse, in order
        SGCL_INLINE_HOT const vector<ocsp_single_response>& responses() const noexcept {
            return _d->responses;
        }

        // The certificates the responder sent (a delegated responder's)
        SGCL_INLINE_HOT const vector<certificate>& certificates() const noexcept {
            return _d->certificates;
        }

        // The responseExtensions, and the nonce among them (empty: none)
        SGCL_INLINE_HOT const vector<extension>& extensions() const noexcept {
            return _d->extensions;
        }

        SGCL_INLINE_HOT const vector<byte>& nonce() const noexcept {
            return _d->nonce;
        }

        SGCL_INLINE_HOT x509::signature_algorithm signature_algorithm() const noexcept {
            return _d->sig_algorithm;
        }

        SGCL_INLINE_HOT const string& signature_algorithm_oid() const noexcept {
            return _d->sig_algorithm_oid;
        }

        SGCL_INLINE_HOT const vector<byte>& signature() const noexcept {
            return _d->signature;
        }

        // Whether the key of `signer` signed the response (the algorithm
        // one verified here); errc::verification with the reason, as
        // certificate::check_signature_from
        [[nodiscard]] expected<void, error> check_signature_from(const certificate& signer) const noexcept {
            if (_d->status != ocsp_response_status::successful) {
                return unexpected<error>(detail::reject(reason::revocation_unknown, "an OCSP response that is not successful has no signature"));
            }
            const auto& s = detail::PoolAccess::data(signer);
            return detail::check_signed(_d->sig_algorithm, _d->sig_algorithm_oid, raw_tbs(), _d->signature.as_slice(), _d->signature_whole, s.key, "the OCSP response",
                                        detail::describe(s), s.key_error);
        }

        // The single response of the certificate, verified: the response
        // successful; signed by the issuer, or by a responder the issuer
        // certified for it (a certificate of the response the issuer
        // signed, with id-kp-OCSPSigning, valid at the time); the
        // certificate named by a CertID (SHA-1 or SHA-2); current at the
        // time (thisUpdate no later than it, nextUpdate, or thisUpdate and
        // max_age, no earlier, each by the skew); the nonce asked for
        // echoed; no critical extension unknown. errc::verification with
        // the reason: revocation_unknown (not successful, no status of the
        // certificate, a nonce that differs), unknown_authority (a signer
        // the issuer did not authorize), invalid_signature,
        // unsupported_algorithm, insecure_algorithm, incompatible_usage, expired,
        // not_yet_valid, unhandled_critical_extension. The status is the
        // answer's: a revoked certificate verifies, its status revoked
        [[nodiscard]] expected<ocsp_single_response, error> verify(const certificate& cert, const certificate& issuer, const ocsp_verify_options& o = {}) const noexcept {
            using detail::reject;
            const auto& d = *_d;
            const auto& c = detail::PoolAccess::data(cert);
            const auto& i = detail::PoolAccess::data(issuer);
            if (d.status != ocsp_response_status::successful) {
                return unexpected<error>(reject(reason::revocation_unknown, std::string("the OCSP responder answered ") + _status_text(d.status)));
            }
            if (d.unhandled_critical) {
                return unexpected<error>(reject(reason::unhandled_critical_extension, "the OCSP response has a critical extension not handled here"));
            }
            const int64_t now = o.time ? o.time->unix() : time::now().unix();
            const int64_t skew = o.skew.milliseconds() / 1000;
            // the signer: the issuer, or a certificate of the response it
            // certified, by the responderID
            const detail::CertData* signer = nullptr;
            if (_names(i)) {
                signer = &i;
            } else {
                optional<error> why;
                for (const auto& cand : d.certificates) {
                    const auto& r = detail::PoolAccess::data(cand);
                    if (!_names(r)) {
                        continue;
                    }
                    if (auto e = _delegated(r, i, now); !e) {
                        if (!why) {
                            why = e.error();
                        }
                        continue;
                    }
                    signer = &r;
                    break;
                }
                if (!signer) {
                    if (why) {
                        return unexpected<error>(*why);
                    }
                    return unexpected<error>(reject(reason::unknown_authority, "the OCSP response is signed by a responder that neither is " + detail::describe(i) + " nor holds a certificate of it"));
                }
            }
            if (auto s = detail::check_signed(d.sig_algorithm, d.sig_algorithm_oid, raw_tbs(), d.signature.as_slice(), d.signature_whole, signer->key, "the OCSP response",
                                              detail::describe(*signer), signer->key_error); !s) {
                return unexpected<error>(s.error());
            }
            const ocsp_single_response* found = nullptr;
            for (const auto& r : d.responses) {
                if (detail::cert_id_matches(r.hash, r.issuer_name_hash, r.issuer_key_hash, r.serial_number, c, i)) {
                    found = &r;
                    break;
                }
            }
            if (!found) {
                return unexpected<error>(reject(reason::revocation_unknown, "the OCSP response holds no status of " + detail::describe(c)));
            }
            const int64_t this_update = found->this_update.unix();
            if (this_update > now + skew) {
                return unexpected<error>(reject(reason::not_yet_valid, "the OCSP response of " + detail::describe(c) + " is not current yet (thisUpdate ahead)"));
            }
            const int64_t until = found->next_update ? found->next_update->unix() : this_update + o.max_age.milliseconds() / 1000;
            if (until + skew < now) {
                return unexpected<error>(reject(reason::expired, "the OCSP response of " + detail::describe(c) + " is no longer current (past its nextUpdate)"));
            }
            if (!o.nonce.empty() && d.nonce != o.nonce) {
                return unexpected<error>(reject(reason::revocation_unknown, "the OCSP response does not carry the request's nonce"));
            }
            return *found;
        }

    private:
        tracked_ptr<const detail::OcspData> _d;

        SGCL_INLINE_HOT explicit ocsp_response(tracked_ptr<const detail::OcspData> d) noexcept
        : _d(std::move(d)) {
        }

        static const char* _status_text(ocsp_response_status s) noexcept {
            switch (s) {
                case ocsp_response_status::successful: return "successful";
                case ocsp_response_status::malformed_request: return "malformedRequest";
                case ocsp_response_status::internal_error: return "internalError";
                case ocsp_response_status::try_later: return "tryLater";
                case ocsp_response_status::sig_required: return "sigRequired";
                case ocsp_response_status::unauthorized: return "unauthorized";
            }
            return "an unknown status";
        }

        // Whether the responderID names the certificate: its subject
        // (byName) or the SHA-1 of its key (byKey)
        bool _names(const detail::CertData& c) const noexcept {
            const auto& d = *_d;
            if (!d.responder_name.empty()) {
                return detail::same_bytes(d.responder_name, c.bytes_at(c.subject_at), c.subject_size);
            }
            const unsigned char* kp;
            size_t kn;
            if (!detail::key_bits(c.range(c.spki_at, c.spki_size), kp, kn)) {
                return false;
            }
            auto h = sha1::of(slice<const byte>(reinterpret_cast<const byte*>(kp), kn));
            return detail::same_bytes(d.responder_key_hash, reinterpret_cast<const unsigned char*>(h.data()), h.size());
        }

        // A delegated responder (RFC 6960 §4.2.2.2): issued by the issuer
        // itself, id-kp-OCSPSigning among its extended key usages, valid
        // at the time, no critical extension unknown
        static expected<void, error> _delegated(const detail::CertData& r, const detail::CertData& issuer, int64_t now) noexcept {
            using detail::reject;
            // the signature of the responder's certificate, once per process
            // (detail::ResponderMemo)
            const auto key = detail::ResponderMemo::key(r, issuer);
            if (!detail::ResponderMemo::known(key)) {
                if (auto s = detail::check_signature(r, issuer); !s) {
                    return unexpected<error>(reject(reason::unknown_authority, "the OCSP responder " + detail::describe(r) + " is not certified by " + detail::describe(issuer)));
                }
                detail::ResponderMemo::add(key);
            }
            bool signing = false;
            for (auto u : r.ext_key_usages) {
                signing |= u == ext_key_usage::ocsp_signing;
            }
            if (!signing) {
                return unexpected<error>(reject(reason::incompatible_usage, "the OCSP responder " + detail::describe(r) + " lacks id-kp-OCSPSigning"));
            }
            if (auto t = detail::check_time(r, now); !t) {
                return t;
            }
            return detail::check_critical(r);
        }
    };

    // A certificate revocation list (RFC 5280 §5), Go's RevocationList:
    // read once, never changed, a value that costs a pointer to copy. Its
    // entries stay in its bytes, indexed: a list of a hundred thousand is
    // read without an allocation per entry, and operator[] makes the value
    // of one when asked
    class revocation_list {
    public:
        // A CRL in DER: errc::malformed with the offset for anything that
        // is not one or is not strict DER
        [[nodiscard]] static expected<revocation_list, error> parse(const slice<const byte>& der) noexcept {
            auto d = make_tracked<detail::CrlData>();
            // parsed from the caller's bytes, copied after: a fuzzer's exact buffer shows an overread
            if (auto r = detail::parse_crl(*d, der); !r) {
                return unexpected<error>(r.error());
            }
            d->raw = vector<byte>(der.data(), der.data() + der.size());
            return revocation_list(std::move(d));
        }

        // The first X509 CRL block of a PEM text, as certificate::from_pem
        // reads its CERTIFICATE
        [[nodiscard]] static expected<revocation_list, error> from_pem(const string& text) noexcept {
            optional<encoding::pem> found;
            detail::walk_pem(text, [&](expected<encoding::pem, encoding::error>& block) noexcept {
                if (!block || block->type() != "X509 CRL") {
                    return false;
                }
                found = std::move(*block);
                return true;
            });
            if (!found) {
                return unexpected<error>(error(errc::malformed, string("sgcl::crypto::x509: no X509 CRL block in the PEM text")));
            }
            return parse(found->bytes().as_slice());
        }

        // The whole list, its TBSCertList (what the signature covers), its
        // issuer's name, as the bytes of the encoding
        SGCL_INLINE_HOT slice<const byte> raw() const noexcept {
            return _d->raw.as_slice();
        }

        SGCL_INLINE_HOT slice<const byte> raw_tbs() const noexcept {
            return _d->range(_d->tbs_at, _d->tbs_size);
        }

        SGCL_INLINE_HOT slice<const byte> raw_issuer() const noexcept {
            return _d->range(_d->issuer_at, _d->issuer_size);
        }

        // 1 or 2
        SGCL_INLINE_HOT int version() const noexcept {
            return _d->version;
        }

        SGCL_INLINE_HOT const name& issuer() const noexcept {
            return _d->issuer;
        }

        // When it was issued, and when the next one is due (nullopt: the
        // list does not say), in UTC
        SGCL_INLINE_HOT time::datetime this_update() const noexcept {
            return detail::utc(_d->this_update);
        }

        SGCL_INLINE_HOT optional<time::datetime> next_update() const noexcept {
            if (!_d->has_next_update) {
                return nullopt;
            }
            return detail::utc(_d->next_update);
        }

        SGCL_INLINE_HOT x509::signature_algorithm signature_algorithm() const noexcept {
            return _d->sig_algorithm;
        }

        SGCL_INLINE_HOT const string& signature_algorithm_oid() const noexcept {
            return _d->sig_algorithm_oid;
        }

        SGCL_INLINE_HOT const vector<byte>& signature() const noexcept {
            return _d->signature;
        }

        // Every extension of the list, in its order
        SGCL_INLINE_HOT const vector<extension>& extensions() const noexcept {
            return _d->extensions;
        }

        // cRLNumber: the INTEGER's bytes, big-endian; empty when absent
        SGCL_INLINE_HOT const vector<byte>& number() const noexcept {
            return _d->number;
        }

        // A delta CRL (deltaCRLIndicator, RFC 5280 §5.2.4): the changes
        // since the complete list of number base_number()
        SGCL_INLINE_HOT bool is_delta() const noexcept {
            return !_d->base_number.empty();
        }

        SGCL_INLINE_HOT const vector<byte>& base_number() const noexcept {
            return _d->base_number;
        }

        // authorityKeyIdentifier's keyIdentifier; empty when absent
        SGCL_INLINE_HOT const vector<byte>& authority_key_id() const noexcept {
            return _d->authority_key_id;
        }

        // issuingDistributionPoint: the URIs of its full name, and what it
        // limits the list to
        SGCL_INLINE_HOT const vector<string>& distribution_point() const noexcept {
            return _d->idp_uris;
        }

        SGCL_INLINE_HOT bool only_user_certificates() const noexcept {
            return _d->only_user;
        }

        SGCL_INLINE_HOT bool only_ca_certificates() const noexcept {
            return _d->only_ca;
        }

        SGCL_INLINE_HOT bool indirect() const noexcept {
            return _d->indirect;
        }

        // The certificates listed
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _d->entries.size();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _d->entries.empty();
        }

        // The entry at index; std::out_of_range past the end
        revoked_certificate operator[](size_t index) const {
            if (index >= _d->entries.size()) {
                throw out_of_range("sgcl::crypto::x509::revocation_list: an index past the end");
            }
            return _entry(_d->entries[index]);
        }

        // The entry of a serial number (its INTEGER bytes, as
        // certificate::serial_number), or of a certificate's; nullopt when
        // the list does not hold it
        [[nodiscard]] optional<revoked_certificate> lookup(const slice<const byte>& serial) const noexcept {
            const auto* e = _find(reinterpret_cast<const unsigned char*>(serial.data()), serial.size());
            if (!e) {
                return nullopt;
            }
            return _entry(*e);
        }

        [[nodiscard]] optional<revoked_certificate> lookup(const certificate& cert) const noexcept {
            return lookup(cert.serial_number().as_slice());
        }

        // Whether `issuer` signed the list: its subject the list's issuer,
        // its keyUsage, where it has one, with cRLSign, its key the
        // signature's. errc::verification with the reason (unknown_authority,
        // missing_cert_sign, invalid_signature, unsupported_algorithm,
        // insecure_algorithm). Go's CheckSignatureFrom
        [[nodiscard]] expected<void, error> check_signature_from(const certificate& issuer) const noexcept {
            using detail::reject;
            const auto& i = detail::PoolAccess::data(issuer);
            if (!detail::same_slice(raw_issuer(), issuer.raw_subject())) {
                return unexpected<error>(reject(reason::unknown_authority, "the CRL of \"" + std::string(_d->issuer.to_string().view()) + "\" is not issued by " + detail::describe(i)));
            }
            if (i.has_key_usage && (i.key_usage_bits & uint16_t(key_usage::crl_sign)) == 0) {
                return unexpected<error>(reject(reason::missing_cert_sign, "the key usage of " + detail::describe(i) + " lacks cRLSign"));
            }
            if (!_d->authority_key_id.empty() && !i.subject_key_id.empty() && _d->authority_key_id != i.subject_key_id) {
                return unexpected<error>(reject(reason::unknown_authority, "the CRL names another key of its issuer than " + detail::describe(i) + "'s"));
            }
            return detail::check_signed(_d->sig_algorithm, _d->sig_algorithm_oid, raw_tbs(), _d->signature.as_slice(), _d->signature_whole, i.key, "the CRL", detail::describe(i),
                                        i.key_error);
        }

        // The status of the certificate by this complete list (RFC 5280
        // §6.3): the list signed by `issuer` (check_signature_from), the
        // certificate's issuer, current at the time (thisUpdate no later,
        // nextUpdate no earlier), of a scope that covers the certificate
        // (issuingDistributionPoint: user or CA certificates, its point one
        // of the certificate's, not indirect, every reason), no critical
        // extension unknown. good or revoked (an entry of certificateHold
        // is revoked; removeFromCRL belongs to deltas); errc::verification
        // with the reason when the list cannot say: revocation_unknown (a
        // delta alone, a scope that does not cover it, an indirect list),
        // expired, not_yet_valid, unhandled_critical_extension, or what
        // check_signature_from gives
        [[nodiscard]] expected<revocation_status, error> status_of(const certificate& cert, const certificate& issuer, optional<time::datetime> time = nullopt) const noexcept {
            if (is_delta()) {
                return unexpected<error>(detail::reject(reason::revocation_unknown, "a delta CRL says nothing alone: status_of(cert, issuer, delta) of its complete list"));
            }
            const int64_t now = time ? time->unix() : time::now().unix();
            if (auto r = _applies(cert, issuer, now); !r) {
                return unexpected<error>(r.error());
            }
            const auto& c = detail::PoolAccess::data(cert);
            const auto* e = _find(reinterpret_cast<const unsigned char*>(c.serial.data()), c.serial.size());
            if (e && e->unhandled_critical) {
                return unexpected<error>(detail::reject(reason::unhandled_critical_extension, "the CRL's entry of " + detail::describe(c) + " has a critical extension not handled here"));
            }
            if (e && e->reason != uint8_t(revocation_reason::remove_from_crl)) {
                return revocation_status::revoked;
            }
            if (_d->only_some_reasons) {
                return unexpected<error>(detail::reject(reason::revocation_unknown, "the CRL covers only some reasons, and does not list " + detail::describe(c)));
            }
            return revocation_status::good;
        }

        // The same by this complete list and a delta of it (RFC 5280
        // §5.2.4): both checked as above, the delta of the same issuer and
        // scope and of a base_number no greater than the list's number; an
        // entry of the delta decides (removeFromCRL: good again), else the
        // list's
        [[nodiscard]] expected<revocation_status, error> status_of(const certificate& cert, const certificate& issuer, const revocation_list& delta,
                                                                   optional<time::datetime> time = nullopt) const noexcept {
            using detail::reject;
            if (is_delta() || !delta.is_delta()) {
                return unexpected<error>(reject(reason::revocation_unknown, "status_of(cert, issuer, delta) takes a complete CRL and a delta CRL"));
            }
            if (_d->number.empty() || detail::compare_unsigned(delta._d->base_number, _d->number) > 0) {
                return unexpected<error>(reject(reason::revocation_unknown, "the delta CRL is of a later complete list than this one"));
            }
            if (!detail::same_slice(delta.raw_issuer(), raw_issuer()) || delta._d->only_user != _d->only_user || delta._d->only_ca != _d->only_ca || delta._d->idp_uris != _d->idp_uris) {
                return unexpected<error>(reject(reason::revocation_unknown, "the delta CRL is of another issuer or scope than the list"));
            }
            const int64_t now = time ? time->unix() : time::now().unix();
            if (auto r = delta._applies(cert, issuer, now); !r) {
                return unexpected<error>(r.error());
            }
            const auto& c = detail::PoolAccess::data(cert);
            if (const auto* e = delta._find(reinterpret_cast<const unsigned char*>(c.serial.data()), c.serial.size())) {
                if (e->unhandled_critical) {
                    return unexpected<error>(reject(reason::unhandled_critical_extension, "the delta CRL's entry of " + detail::describe(c) + " has a critical extension not handled here"));
                }
                return e->reason == uint8_t(revocation_reason::remove_from_crl) ? revocation_status::good : revocation_status::revoked;
            }
            return status_of(cert, issuer, time);
        }

    private:
        tracked_ptr<const detail::CrlData> _d;

        SGCL_INLINE_HOT explicit revocation_list(tracked_ptr<const detail::CrlData> d) noexcept
        : _d(std::move(d)) {
        }

        revoked_certificate _entry(const detail::CrlEntry& e) const noexcept {
            revoked_certificate r;
            r.serial_number = _d->range(e.serial_at, e.serial_size);
            r.revocation_time = detail::utc(e.revoked);
            r.reason = revocation_reason(e.reason);
            if (e.invalidity != std::numeric_limits<int64_t>::min()) {
                r.invalidity_date = detail::utc(e.invalidity);
            }
            return r;
        }

        // The entry of a serial: a scan of the index, the serials compared
        // in place (no allocation; a list is looked up once or twice a
        // chain)
        const detail::CrlEntry* _find(const unsigned char* serial, size_t n) const noexcept {
            const unsigned char* base = _d->bytes_at(0);
            for (const auto& e : _d->entries) {
                if (e.serial_size == n && std::memcmp(base + e.serial_at, serial, n) == 0) {
                    return &e;
                }
            }
            return nullptr;
        }

        // Whether the list speaks of the certificate at the time
        expected<void, error> _applies(const certificate& cert, const certificate& issuer, int64_t now) const noexcept {
            using detail::reject;
            const auto& d = *_d;
            const auto& c = detail::PoolAccess::data(cert);
            if (!detail::same_slice(cert.raw_issuer(), raw_issuer())) {
                return unexpected<error>(reject(reason::revocation_unknown, "the CRL is of another issuer than that of " + detail::describe(c)));
            }
            if (auto s = check_signature_from(issuer); !s) {
                return s;
            }
            if (d.unhandled_critical) {
                return unexpected<error>(reject(reason::unhandled_critical_extension, "the CRL has a critical extension not handled here"));
            }
            if (d.this_update > now) {
                return unexpected<error>(reject(reason::not_yet_valid, "the CRL is not current yet (thisUpdate ahead)"));
            }
            if (d.has_next_update && d.next_update < now) {
                return unexpected<error>(reject(reason::expired, "the CRL is no longer current (past its nextUpdate)"));
            }
            if (d.indirect) {
                return unexpected<error>(reject(reason::revocation_unknown, "an indirect CRL is not taken here"));
            }
            const bool ca = c.basic_constraints_valid && c.is_ca;
            if ((d.only_user && ca) || (d.only_ca && !ca) || d.only_attribute) {
                return unexpected<error>(reject(reason::revocation_unknown, "the CRL's scope does not cover " + detail::describe(c)));
            }
            if (!d.idp_uris.empty() && !c.crl_distribution_points.empty()) {
                bool shared = false;
                for (const auto& u : d.idp_uris) {
                    for (const auto& p : c.crl_distribution_points) {
                        shared |= u == p;
                    }
                }
                if (!shared) {
                    return unexpected<error>(reject(reason::revocation_unknown, "the CRL is of a distribution point other than those of " + detail::describe(c)));
                }
            }
            return {};
        }
    };
}
