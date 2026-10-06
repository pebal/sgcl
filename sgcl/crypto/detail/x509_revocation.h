//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "der.h"
#include "x509_cert.h"
#include "x509_name.h"
#include "x509_verify.h"
#include "../hash_id.h"
#include "../sha256.h"
#include "../x509.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../time/datetime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

// The values and the parsers of revocation: OCSP (RFC 6960) — the CertID
// a request and a response name a certificate by, a request written and
// read, a response read — and the CRL (RFC 5280 §5), over the module's one
// DER reader. Strict DER, as for a certificate; the times of RFC 5280's one
// form. The bounds: an OCSP message of at most 1 MiB, 256 single responses,
// 64 extensions to a list; a CRL of at most 4 GiB (its entries indexed by
// 32-bit offsets into its bytes, never copied out).
namespace sgcl::crypto::x509 {
    // Why a certificate was revoked: CRLReason (RFC 5280 §5.3.1), in a CRL's
    // entry and an OCSP response's RevokedInfo. 7 is not used
    enum class revocation_reason : uint8_t {
        unspecified = 0,
        key_compromise = 1,
        ca_compromise = 2,
        affiliation_changed = 3,
        superseded = 4,
        cessation_of_operation = 5,
        certificate_hold = 6,
        remove_from_crl = 8,
        privilege_withdrawn = 9,
        aa_compromise = 10
    };

    // What is known of a certificate: OCSP's CertStatus; a CRL says good
    // or revoked
    enum class revocation_status : uint8_t {
        good,
        revoked,
        unknown
    };

    // OCSPResponseStatus (RFC 6960 §4.2.1): whether the responder answered
    // with a status, or why not. 4 is not used
    enum class ocsp_response_status : uint8_t {
        successful = 0,
        malformed_request = 1,
        internal_error = 2,
        try_later = 3,
        sig_required = 5,
        unauthorized = 6
    };

    // One SingleResponse of an OCSP response: the certificate it is for
    // (its CertID: the hash, the hashes of the issuer's name and key, the
    // serial number), the status, the times
    struct ocsp_single_response {
        hash_id hash = hash_id::sha1;
        vector<byte> issuer_name_hash;
        vector<byte> issuer_key_hash;
        vector<byte> serial_number;
        revocation_status status = revocation_status::unknown;
        time::datetime this_update;
        optional<time::datetime> next_update;
        optional<time::datetime> revocation_time;     // revoked: when
        revocation_reason reason = revocation_reason::unspecified;   // revoked: why, unspecified when not said
    };

    // A certificate a CRL lists: its serial number (the bytes of the CRL,
    // as the certificate's serial_number has them), when it was revoked,
    // why, and since when its key is suspect when the CRL says it
    struct revoked_certificate {
        slice<const byte> serial_number;
        time::datetime revocation_time;
        revocation_reason reason = revocation_reason::unspecified;
        optional<time::datetime> invalidity_date;
    };
}

namespace sgcl::crypto::x509::detail {
    inline constexpr size_t max_ocsp_size = size_t(1) << 20;
    inline constexpr size_t max_single_responses = 256;
    inline constexpr size_t max_revocation_extensions = 64;

    namespace oid {
        inline constexpr unsigned char ocsp_basic[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01};
        inline constexpr unsigned char ocsp_nonce[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x02};
        inline constexpr unsigned char ocsp_no_check[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x05};
    }

    SGCL_INLINE_HOT vector<byte> bytes_vector(const unsigned char* p, size_t n) noexcept {
        const byte* b = reinterpret_cast<const byte*>(p);
        return vector<byte>(b, b + n);
    }

    SGCL_INLINE_HOT bool same_bytes(const vector<byte>& a, const unsigned char* p, size_t n) noexcept {
        return a.size() == n && (n == 0 || std::memcmp(a.data(), p, n) == 0);
    }

    SGCL_INLINE_HOT bool same_slice(const slice<const byte>& a, const slice<const byte>& b) noexcept {
        return a.size() == b.size() && (a.size() == 0 || std::memcmp(a.data(), b.data(), a.size()) == 0);
    }

    SGCL_INLINE_HOT time::datetime utc(int64_t seconds) noexcept {
        return time::datetime::from_unix(seconds, time::zone::utc());
    }

    // The hash of a CertID's AlgorithmIdentifier: SHA-1 or SHA-2, with NULL
    // or no parameters; false for another
    inline bool read_hash_algorithm(DerReader& r, hash_id& id) noexcept {
        DerReader alg, o;
        if (!r.read(der::sequence, alg) || !alg.read_oid(o)) {
            return false;
        }
        static constexpr unsigned char nothing[1] = {};
        if (!alg.empty() && !(alg.read_exact(der::null, nothing, 0) && alg.empty())) {
            return false;
        }
        if (oid::is(o, oid::sha1)) {
            id = hash_id::sha1;
        } else if (oid::is(o, oid::sha256)) {
            id = hash_id::sha256;
        } else if (oid::is(o, oid::sha384)) {
            id = hash_id::sha384;
        } else if (oid::is(o, oid::sha512)) {
            id = hash_id::sha512;
        } else {
            return false;
        }
        return true;
    }

    // The OID content of a hash a CertID may name
    inline void hash_oid(hash_id id, const unsigned char*& p, size_t& n) {
        switch (id) {
            case hash_id::sha1: p = oid::sha1; n = sizeof oid::sha1; return;
            case hash_id::sha256: p = oid::sha256; n = sizeof oid::sha256; return;
            case hash_id::sha384: p = oid::sha384; n = sizeof oid::sha384; return;
            case hash_id::sha512: p = oid::sha512; n = sizeof oid::sha512; return;
            default: break;
        }
        throw invalid_argument("sgcl::crypto::x509: an OCSP CertID hashes with SHA-1, SHA-256, SHA-384 or SHA-512");
    }

    // The bytes of a key's BIT STRING in a SubjectPublicKeyInfo (without
    // the count of unused bits): what issuerKeyHash and a responder's
    // byKey are hashes of
    inline bool key_bits(const slice<const byte>& spki, const unsigned char*& p, size_t& n) noexcept {
        DerReader in(reinterpret_cast<const unsigned char*>(spki.data()), spki.size());
        DerReader seq, alg;
        unsigned unused;
        return in.read(der::sequence, seq) && seq.read(der::sequence, alg) && seq.read_bit_string(p, n, unused);
    }

    // The delegated OCSP responders whose certificates were found signed by
    // their issuer, by the SHA-256 of the responder's DER and of the
    // issuer's key: a responder answers for its CA for days, so the
    // signature of its certificate is checked once per process rather than
    // with every response (the response's own signature is checked every
    // time). A memo of successes only, of a pure function of the bytes: 32
    // entries, the oldest replaced, under a lock of its own. Plain bytes, no
    // tracked word: a static of the process
    class ResponderMemo {
    public:
        using Key = std::array<unsigned char, 32>;

        static Key key(const CertData& responder, const CertData& issuer) noexcept {
            sha256 h;
            h.update(responder.raw.as_slice());
            h.update(issuer.range(issuer.spki_at, issuer.spki_size));
            auto d = h.value();
            Key k;
            sgcl::detail::copy_bytes(k.data(), d.data(), k.size());
            return k;
        }

        static bool known(const Key& k) noexcept {
            auto& m = _memo();
            std::lock_guard<std::mutex> g(m.lock);
            for (size_t i = 0; i < m.size; ++i) {
                if (m.keys[i] == k) {
                    return true;
                }
            }
            return false;
        }

        static void add(const Key& k) noexcept {
            auto& m = _memo();
            std::lock_guard<std::mutex> g(m.lock);
            m.keys[m.next] = k;
            m.next = (m.next + 1) % m.keys.size();
            m.size = m.size < m.keys.size() ? m.size + 1 : m.size;
        }

    private:
        struct Memo {
            std::mutex lock;
            std::array<Key, 32> keys{};
            size_t next = 0;
            size_t size = 0;
        };

        static Memo& _memo() noexcept {
            static Memo* m = new Memo();   // never destroyed: nothing of it runs at exit
            return *m;
        }
    };

    // A CertID (RFC 6960 §4.1.1) read
    struct CertId {
        hash_id hash = hash_id::sha1;
        DerReader name_hash, key_hash, serial;
    };

    inline bool read_cert_id(DerReader& r, CertId& id) noexcept {
        DerReader seq;
        if (!r.read(der::sequence, seq) || !read_hash_algorithm(seq, id.hash) || !seq.read(der::octet_string, id.name_hash) || !seq.read(der::octet_string, id.key_hash)
            || !seq.read(der::integer, id.serial) || !seq.empty()) {
            return false;
        }
        size_t n = digest_size(id.hash);
        return id.name_hash.size() == n && id.key_hash.size() == n && id.serial.size() != 0;
    }

    // Whether a CertID names the certificate whose serial and issuer are
    // these: the serial's bytes, the hashes of the issuer's name and key
    inline bool cert_id_matches(hash_id hash, const vector<byte>& name_hash, const vector<byte>& key_hash, const vector<byte>& serial, const CertData& cert, const CertData& issuer) noexcept {
        if (serial != cert.serial) {
            return false;
        }
        auto name = digest(hash, issuer.range(issuer.subject_at, issuer.subject_size));
        if (name != name_hash) {
            return false;
        }
        const unsigned char* kp;
        size_t kn;
        if (!key_bits(issuer.range(issuer.spki_at, issuer.spki_size), kp, kn)) {
            return false;
        }
        return digest(hash, slice<const byte>(reinterpret_cast<const byte*>(kp), kn)) == key_hash;
    }

    // Everything read from an OCSP request (one Request, as Go's ParseRequest)
    struct OcspRequestData {
        vector<byte> raw;
        hash_id hash = hash_id::sha1;
        vector<byte> name_hash, key_hash, serial;
        vector<byte> nonce;
    };

    inline error ocsp_fail(size_t at, const char* what) noexcept {
        return error(errc::malformed, uint64_t(at), string(std::string("sgcl::crypto::x509: ") + what));
    }

    // Extensions: SEQUENCE OF Extension, at most 64, each once; f(oid,
    // critical, value) for each, false to refuse the list (the offset of
    // the extension in `at`)
    template<class F>
    bool read_extensions(DerReader list, vector<extension>& out, size_t& at, F&& f) noexcept {
        size_t count = 0;
        while (!list.empty()) {
            at = list.offset();
            DerReader ext, o, value;
            bool critical = false;
            if (++count > max_revocation_extensions || !list.read(der::sequence, ext) || !ext.read_oid(o)) {
                return false;
            }
            if (ext.peek(der::boolean) && !ext.read_bool(critical)) {
                return false;
            }
            if (!ext.read(der::octet_string, value) || !ext.empty()) {
                return false;
            }
            extension e;
            e.oid = string(oid_text(o.data(), o.size()));
            for (auto& seen : out) {
                if (seen.oid == e.oid) {
                    return false;
                }
            }
            e.critical = critical;
            e.value = bytes_vector(value.data(), value.size());
            out.push_back(std::move(e));
            if (!f(o, critical, value)) {
                return false;
            }
        }
        return true;
    }

    // The nonce extension's value (RFC 8954 §2.1): an OCTET STRING of 1 to
    // 128 bytes inside the extnValue; RFC 6960's older responders put the
    // bytes bare, which is taken too
    inline void read_nonce(DerReader value, vector<byte>& out) noexcept {
        DerReader v = value, inner;
        if (v.read(der::octet_string, inner) && v.empty()) {
            out = bytes_vector(inner.data(), inner.size());
        } else {
            out = bytes_vector(value.data(), value.size());
        }
    }

    // der is the caller's: the parse reads it in place, raw is copied after
    inline expected<void, error> parse_ocsp_request(OcspRequestData& d, const slice<const byte>& der) noexcept {
        if (der.size() > max_ocsp_size) {
            return unexpected<error>(ocsp_fail(0, "an OCSP request larger than 1 MiB"));
        }
        DerReader in(reinterpret_cast<const unsigned char*>(der.data()), der.size());
        DerReader req, tbs, list;
        if (!in.read(der::sequence, req) || !in.empty()) {
            return unexpected<error>(ocsp_fail(in.offset(), "not an OCSPRequest SEQUENCE"));
        }
        if (!req.read(der::sequence, tbs)) {
            return unexpected<error>(ocsp_fail(req.offset(), "not a TBSRequest SEQUENCE"));
        }
        DerReader opt;
        bool present;
        if (!tbs.read_optional(der::context0, opt, present)) {
            return unexpected<error>(ocsp_fail(tbs.offset(), "a malformed version"));
        }
        if (present) {
            uint64_t v;
            if (!opt.read_small_unsigned(v) || !opt.empty() || v != 0) {
                return unexpected<error>(ocsp_fail(opt.offset(), "an OCSP request of a version other than 1"));
            }
        }
        if (!tbs.read_optional(der::context1, opt, present)) {   // requestorName
            return unexpected<error>(ocsp_fail(tbs.offset(), "a malformed requestorName"));
        }
        size_t list_at = tbs.offset();
        if (!tbs.read(der::sequence, list)) {
            return unexpected<error>(ocsp_fail(list_at, "a malformed requestList"));
        }
        DerReader one;
        CertId id;
        if (!list.read(der::sequence, one) || !read_cert_id(one, id)) {
            return unexpected<error>(ocsp_fail(list_at, "a requestList without a Request of a CertID"));
        }
        if (!list.empty()) {
            return unexpected<error>(ocsp_fail(list_at, "an OCSP request of more than one certificate"));
        }
        if (!one.read_optional(der::context0, opt, present) || !one.empty()) {   // singleRequestExtensions
            return unexpected<error>(ocsp_fail(list_at, "a malformed Request"));
        }
        d.hash = id.hash;
        d.name_hash = bytes_vector(id.name_hash.data(), id.name_hash.size());
        d.key_hash = bytes_vector(id.key_hash.data(), id.key_hash.size());
        d.serial = bytes_vector(id.serial.data(), id.serial.size());
        if (tbs.peek(0xa2)) {
            DerReader wrap, exts;
            size_t at = tbs.offset();
            if (!tbs.read(0xa2, wrap) || !wrap.read(der::sequence, exts) || !wrap.empty()) {
                return unexpected<error>(ocsp_fail(at, "malformed requestExtensions"));
            }
            vector<extension> seen;
            if (!read_extensions(exts, seen, at, [&](const DerReader& o, bool, DerReader value) noexcept {
                    if (oid::is(o, oid::ocsp_nonce)) {
                        read_nonce(value, d.nonce);
                    }
                    return true;
                })) {
                return unexpected<error>(ocsp_fail(at, "malformed requestExtensions"));
            }
        }
        if (!tbs.empty()) {
            return unexpected<error>(ocsp_fail(tbs.offset(), "data after the TBSRequest's fields"));
        }
        if (req.peek(der::context0)) {   // optionalSignature: read for its syntax
            DerReader sig;
            if (!req.read(der::context0, sig)) {
                return unexpected<error>(ocsp_fail(req.offset(), "a malformed optionalSignature"));
            }
        }
        if (!req.empty()) {
            return unexpected<error>(ocsp_fail(req.offset(), "data after the OCSPRequest's fields"));
        }
        return {};
    }

    // An OCSP request of one certificate written (RFC 6960 §4.1.1): the
    // CertID of `hash`, a nonce extension when one is given
    inline vector<byte> write_ocsp_request(hash_id hash, const vector<byte>& name_hash, const vector<byte>& key_hash, const vector<byte>& serial, const vector<byte>& nonce) {
        const unsigned char* op;
        size_t on;
        hash_oid(hash, op, on);
        DerWriter<1024> w;
        size_t all = w.size();
        if (!nonce.empty()) {
            size_t e2 = w.size();
            size_t list = w.size();
            size_t ext = w.size();
            size_t v = w.size();
            w.put(reinterpret_cast<const unsigned char*>(nonce.data()), nonce.size());
            w.wrap(der::octet_string, v);
            w.wrap(der::octet_string, v);
            size_t o = w.size();
            w.put(oid::ocsp_nonce, sizeof oid::ocsp_nonce);
            w.wrap(der::object_identifier, o);
            w.wrap(der::sequence, ext);
            w.wrap(der::sequence, list);
            w.wrap(0xa2, e2);
        }
        size_t request_list = w.size();
        size_t request = w.size();
        size_t cert_id = w.size();
        size_t m = w.size();
        w.put(reinterpret_cast<const unsigned char*>(serial.data()), serial.size());
        w.wrap(der::integer, m);
        m = w.size();
        w.put(reinterpret_cast<const unsigned char*>(key_hash.data()), key_hash.size());
        w.wrap(der::octet_string, m);
        m = w.size();
        w.put(reinterpret_cast<const unsigned char*>(name_hash.data()), name_hash.size());
        w.wrap(der::octet_string, m);
        size_t alg = w.size();
        w.put((unsigned char)0x00);
        w.put(der::null);
        m = w.size();
        w.put(op, on);
        w.wrap(der::object_identifier, m);
        w.wrap(der::sequence, alg);
        w.wrap(der::sequence, cert_id);
        w.wrap(der::sequence, request);
        w.wrap(der::sequence, request_list);
        w.wrap(der::sequence, all);   // TBSRequest
        w.wrap(der::sequence, all);   // OCSPRequest
        return bytes_vector(w.data(), w.size());
    }

    // Everything read from an OCSP response, held by the response object
    // through a tracked_ptr and never changed after the parse
    struct OcspData {
        vector<byte> raw;
        ocsp_response_status status = ocsp_response_status::successful;
        size_t tbs_at = 0, tbs_size = 0;
        vector<byte> responder_name;          // byName: the Name's DER; empty when byKey
        vector<byte> responder_key_hash;      // byKey: SHA-1 of the responder's key; empty when byName
        int64_t produced_at = 0;
        vector<ocsp_single_response> responses;
        vector<extension> extensions;         // the responseExtensions
        vector<byte> nonce;
        bool unhandled_critical = false;      // a critical extension not known here, of the response or of a single response
        signature_algorithm sig_algorithm = signature_algorithm::unknown;
        string sig_algorithm_oid;
        vector<byte> signature;
        bool signature_whole = true;
        vector<certificate> certificates;     // the certs the responder sent

        SGCL_INLINE_HOT const unsigned char* bytes_at(size_t at) const noexcept {
            return reinterpret_cast<const unsigned char*>(raw.data()) + at;
        }
    };

    // der is the caller's: the parse reads it in place, raw is copied after
    inline expected<void, error> parse_ocsp_response(OcspData& d, const slice<const byte>& der) noexcept {
        if (der.size() > max_ocsp_size) {
            return unexpected<error>(ocsp_fail(0, "an OCSP response larger than 1 MiB"));
        }
        const auto* base = reinterpret_cast<const unsigned char*>(der.data());
        DerReader in(base, der.size());
        DerReader resp, bytes, type, octets;
        if (!in.read(der::sequence, resp) || !in.empty()) {
            return unexpected<error>(ocsp_fail(in.offset(), "not an OCSPResponse SEQUENCE"));
        }
        DerReader status;
        size_t status_at = resp.offset();
        if (!resp.read(0x0a, status) || status.size() != 1) {
            return unexpected<error>(ocsp_fail(status_at, "a malformed responseStatus"));
        }
        uint8_t st = uint8_t(status.data()[0]);
        if (st > 6 || st == 4) {
            return unexpected<error>(ocsp_fail(status_at, "a responseStatus of no value of RFC 6960"));
        }
        d.status = ocsp_response_status(st);
        if (d.status != ocsp_response_status::successful) {
            if (!resp.empty()) {
                return unexpected<error>(ocsp_fail(resp.offset(), "responseBytes in a response that is not successful"));
            }
            return {};
        }
        size_t rb_at = resp.offset();
        if (!resp.read(der::context0, bytes) || !resp.empty() || !bytes.read(der::sequence, type) || !bytes.empty()) {
            return unexpected<error>(ocsp_fail(rb_at, "a successful response without its responseBytes"));
        }
        DerReader o;
        if (!type.read_oid(o) || !type.read(der::octet_string, octets) || !type.empty()) {
            return unexpected<error>(ocsp_fail(rb_at, "malformed responseBytes"));
        }
        if (!oid::is(o, oid::ocsp_basic)) {
            return unexpected<error>(error(errc::unsupported, uint64_t(rb_at), string("sgcl::crypto::x509: an OCSP response of a type other than id-pkix-ocsp-basic")));
        }
        // BasicOCSPResponse
        DerReader basic, tbs_el, tbs;
        if (!octets.read(der::sequence, basic) || !octets.empty()) {
            return unexpected<error>(ocsp_fail(octets.offset(), "not a BasicOCSPResponse SEQUENCE"));
        }
        if (!basic.read_element(der::sequence, tbs_el)) {
            return unexpected<error>(ocsp_fail(basic.offset(), "not a ResponseData SEQUENCE"));
        }
        d.tbs_at = size_t(tbs_el.data() - base);
        d.tbs_size = tbs_el.size();
        tbs_el.read(der::sequence, tbs);
        DerReader opt;
        bool present;
        if (!tbs.read_optional(der::context0, opt, present)) {
            return unexpected<error>(ocsp_fail(tbs.offset(), "a malformed version"));
        }
        if (present) {
            uint64_t v;
            if (!opt.read_small_unsigned(v) || !opt.empty() || v != 0) {
                return unexpected<error>(ocsp_fail(opt.offset(), "an OCSP response of a version other than 1"));
            }
        }
        size_t rid_at = tbs.offset();
        if (tbs.peek(der::context1)) {
            DerReader by, name_el;
            if (!tbs.read(der::context1, by) || !by.read_element(der::sequence, name_el) || !by.empty()) {
                return unexpected<error>(ocsp_fail(rid_at, "a malformed responderID"));
            }
            DerReader check = name_el;
            name parsed;
            if (!parse_name(check, parsed)) {
                return unexpected<error>(ocsp_fail(rid_at, "a malformed responderID name"));
            }
            d.responder_name = bytes_vector(name_el.data(), name_el.size());
        } else {
            DerReader by, hash;
            if (!tbs.read(0xa2, by) || !by.read(der::octet_string, hash) || !by.empty() || hash.size() != 20) {
                return unexpected<error>(ocsp_fail(rid_at, "a malformed responderID"));
            }
            d.responder_key_hash = bytes_vector(hash.data(), hash.size());
        }
        size_t produced_at = tbs.offset();
        if (!tbs.read_time(d.produced_at)) {
            return unexpected<error>(ocsp_fail(produced_at, "a malformed producedAt"));
        }
        DerReader list;
        size_t list_at = tbs.offset();
        if (!tbs.read(der::sequence, list)) {
            return unexpected<error>(ocsp_fail(list_at, "malformed responses"));
        }
        while (!list.empty()) {
            size_t at = list.offset();
            DerReader single;
            CertId id;
            if (d.responses.size() == max_single_responses) {
                return unexpected<error>(ocsp_fail(at, "more than 256 single responses"));
            }
            if (!list.read(der::sequence, single) || !read_cert_id(single, id)) {
                return unexpected<error>(ocsp_fail(at, "a malformed SingleResponse"));
            }
            ocsp_single_response r;
            r.hash = id.hash;
            r.issuer_name_hash = bytes_vector(id.name_hash.data(), id.name_hash.size());
            r.issuer_key_hash = bytes_vector(id.key_hash.data(), id.key_hash.size());
            r.serial_number = bytes_vector(id.serial.data(), id.serial.size());
            DerReader cs;
            unsigned char tag;
            size_t cs_at = single.offset();
            if (!single.read_any(tag, cs)) {
                return unexpected<error>(ocsp_fail(cs_at, "a malformed certStatus"));
            }
            if (tag == 0x80 && cs.empty()) {
                r.status = revocation_status::good;
            } else if (tag == 0x82 && cs.empty()) {
                r.status = revocation_status::unknown;
            } else if (tag == 0xa1) {
                int64_t when;
                if (!cs.read_time(when)) {
                    return unexpected<error>(ocsp_fail(cs_at, "a malformed revocationTime"));
                }
                r.status = revocation_status::revoked;
                r.revocation_time = utc(when);
                if (cs.peek(der::context0)) {
                    DerReader wrap, reason;
                    if (!cs.read(der::context0, wrap) || !wrap.read(0x0a, reason) || !wrap.empty() || reason.size() != 1 || reason.data()[0] > 10 || reason.data()[0] == 7) {
                        return unexpected<error>(ocsp_fail(cs_at, "a malformed revocationReason"));
                    }
                    r.reason = revocation_reason(reason.data()[0]);
                }
                if (!cs.empty()) {
                    return unexpected<error>(ocsp_fail(cs_at, "a malformed RevokedInfo"));
                }
            } else {
                return unexpected<error>(ocsp_fail(cs_at, "a certStatus that is not good, revoked or unknown"));
            }
            int64_t t;
            size_t t_at = single.offset();
            if (!single.read_time(t)) {
                return unexpected<error>(ocsp_fail(t_at, "a malformed thisUpdate"));
            }
            r.this_update = utc(t);
            if (single.peek(der::context0)) {
                DerReader wrap;
                if (!single.read(der::context0, wrap) || !wrap.read_time(t) || !wrap.empty()) {
                    return unexpected<error>(ocsp_fail(t_at, "a malformed nextUpdate"));
                }
                r.next_update = utc(t);
            }
            if (single.peek(der::context1)) {
                DerReader wrap, exts;
                size_t eat = single.offset();
                if (!single.read(der::context1, wrap) || !wrap.read(der::sequence, exts) || !wrap.empty()) {
                    return unexpected<error>(ocsp_fail(eat, "malformed singleExtensions"));
                }
                vector<extension> seen;
                if (!read_extensions(exts, seen, eat, [&](const DerReader&, bool critical, DerReader) noexcept {
                        d.unhandled_critical |= critical;
                        return true;
                    })) {
                    return unexpected<error>(ocsp_fail(eat, "malformed singleExtensions"));
                }
            }
            if (!single.empty()) {
                return unexpected<error>(ocsp_fail(single.offset(), "data after a SingleResponse's fields"));
            }
            d.responses.push_back(std::move(r));
        }
        if (tbs.peek(der::context1)) {
            DerReader wrap, exts;
            size_t eat = tbs.offset();
            if (!tbs.read(der::context1, wrap) || !wrap.read(der::sequence, exts) || !wrap.empty()) {
                return unexpected<error>(ocsp_fail(eat, "malformed responseExtensions"));
            }
            if (!read_extensions(exts, d.extensions, eat, [&](const DerReader& o, bool critical, DerReader value) noexcept {
                    if (oid::is(o, oid::ocsp_nonce)) {
                        read_nonce(value, d.nonce);
                    } else {
                        d.unhandled_critical |= critical;
                    }
                    return true;
                })) {
                return unexpected<error>(ocsp_fail(eat, "malformed responseExtensions"));
            }
        }
        if (!tbs.empty()) {
            return unexpected<error>(ocsp_fail(tbs.offset(), "data after the ResponseData's fields"));
        }
        DerReader alg;
        size_t alg_at = basic.offset();
        if (!basic.read(der::sequence, alg)) {
            return unexpected<error>(ocsp_fail(alg_at, "a malformed signature algorithm"));
        }
        if (auto a = CertParser::read_signature_algorithm(alg, d.sig_algorithm, d.sig_algorithm_oid); !a) {
            return a;
        }
        const unsigned char* sig;
        size_t sig_n;
        unsigned unused;
        size_t sig_at = basic.offset();
        if (!basic.read_bit_string(sig, sig_n, unused)) {
            return unexpected<error>(ocsp_fail(sig_at, "a malformed signature"));
        }
        d.signature = bytes_vector(sig, sig_n);
        d.signature_whole = unused == 0;
        if (basic.peek(der::context0)) {
            DerReader wrap, certs;
            size_t cat = basic.offset();
            if (!basic.read(der::context0, wrap) || !wrap.read(der::sequence, certs) || !wrap.empty()) {
                return unexpected<error>(ocsp_fail(cat, "malformed certs"));
            }
            while (!certs.empty()) {
                DerReader el;
                size_t at = certs.offset();
                if (d.certificates.size() == 16 || !certs.read_element(der::sequence, el)) {
                    return unexpected<error>(ocsp_fail(at, "malformed certs (at most 16)"));
                }
                auto c = certificate::parse(slice<const byte>(reinterpret_cast<const byte*>(el.data()), el.size()));
                if (!c) {
                    return unexpected<error>(error(errc::malformed, uint64_t(at), string("sgcl::crypto::x509: a certificate of the OCSP response does not parse: ") + c.error().message()));
                }
                d.certificates.push_back(std::move(*c));
            }
        }
        if (!basic.empty()) {
            return unexpected<error>(ocsp_fail(basic.offset(), "data after the BasicOCSPResponse's fields"));
        }
        return {};
    }

    // One entry of a CRL as an index into its bytes: no copy of a serial,
    // no allocation for an entry
    struct CrlEntry {
        uint32_t serial_at = 0;
        uint32_t serial_size = 0;
        int64_t revoked = 0;
        int64_t invalidity = std::numeric_limits<int64_t>::min();   // min: none
        uint8_t reason = 0;
        bool unhandled_critical = false;     // a critical entry extension not known here (certificateIssuer among them)
    };

    // Everything read from a CRL, held by the list object through a
    // tracked_ptr and never changed after the parse
    struct CrlData {
        vector<byte> raw;
        size_t tbs_at = 0, tbs_size = 0;
        size_t issuer_at = 0, issuer_size = 0;
        int version = 1;
        name issuer;
        int64_t this_update = 0;
        int64_t next_update = 0;
        bool has_next_update = false;
        signature_algorithm sig_algorithm = signature_algorithm::unknown;
        string sig_algorithm_oid;
        vector<byte> signature;
        bool signature_whole = true;
        vector<extension> extensions;
        vector<byte> number;                  // cRLNumber's INTEGER bytes; empty: none
        vector<byte> base_number;             // deltaCRLIndicator's; empty: not a delta
        vector<byte> authority_key_id;
        bool has_idp = false;                 // issuingDistributionPoint
        vector<string> idp_uris;
        bool only_user = false, only_ca = false, only_attribute = false, indirect = false, only_some_reasons = false;
        bool unhandled_critical = false;      // a critical CRL extension not known here
        std::vector<CrlEntry> entries;        // unmanaged: no tracked word in an entry

        SGCL_INLINE_HOT const unsigned char* bytes_at(size_t at) const noexcept {
            return reinterpret_cast<const unsigned char*>(raw.data()) + at;
        }

        SGCL_INLINE_HOT slice<const byte> range(size_t at, size_t n) const noexcept {
            return raw.as_slice().subslice(at, n);
        }
    };

    // INTEGER bytes in their shortest form, sign aside (a serial, a CRL
    // number)
    SGCL_INLINE_HOT bool integer_ok(const DerReader& r) noexcept {
        const unsigned char* p = r.data();
        size_t n = r.size();
        return n != 0 && !(n > 1 && ((p[0] == 0x00 && (p[1] & 0x80) == 0) || (p[0] == 0xff && (p[1] & 0x80) != 0)));
    }

    // The entries of revokedCertificates: SEQUENCE { serial, revocationDate,
    // crlEntryExtensions OPTIONAL } each. The loop of a CRL of a hundred
    // thousand entries: reads in place, nothing allocated but the index
    inline expected<void, error> parse_crl_entries(CrlData& d, DerReader list, const unsigned char* base) noexcept {
        static constexpr unsigned char ce[] = {0x55, 0x1d};
        d.entries.reserve(list.size() / 40);   // an entry with its reason is about 40 bytes: one growth or none
        while (!list.empty()) {
            size_t at = list.offset();
            DerReader e, serial, exts;
            CrlEntry entry;
            if (!list.read(der::sequence, e) || !e.read(der::integer, serial) || !e.read_time(entry.revoked)) {
                return unexpected<error>(ocsp_fail(at, "a malformed revoked certificate"));
            }
            entry.serial_at = uint32_t(serial.data() - base);
            entry.serial_size = uint32_t(serial.size());
            if (!e.empty()) {
                if (!e.read(der::sequence, exts) || !e.empty()) {
                    return unexpected<error>(ocsp_fail(at, "malformed entry extensions"));
                }
                size_t count = 0;
                while (!exts.empty()) {
                    DerReader ext, o, value;
                    bool critical = false;
                    if (++count > max_revocation_extensions || !exts.read(der::sequence, ext) || !ext.read_oid(o) || (ext.peek(der::boolean) && !ext.read_bool(critical))
                        || !ext.read(der::octet_string, value) || !ext.empty()) {
                        return unexpected<error>(ocsp_fail(at, "a malformed entry extension"));
                    }
                    if (o.size() == 3 && std::memcmp(o.data(), ce, 2) == 0 && o.data()[2] == 21) {   // reasonCode
                        DerReader r;
                        if (!value.read(0x0a, r) || !value.empty() || r.size() != 1 || r.data()[0] > 10 || r.data()[0] == 7) {
                            return unexpected<error>(ocsp_fail(at, "a malformed reasonCode"));
                        }
                        entry.reason = r.data()[0];
                    } else if (o.size() == 3 && std::memcmp(o.data(), ce, 2) == 0 && o.data()[2] == 24) {   // invalidityDate
                        if (!value.read_time(entry.invalidity) || !value.empty()) {
                            return unexpected<error>(ocsp_fail(at, "a malformed invalidityDate"));
                        }
                    } else if (critical) {
                        entry.unhandled_critical = true;   // certificateIssuer (an indirect CRL's) and the unknown
                    }
                }
            }
            d.entries.push_back(entry);
        }
        return {};
    }

    // issuingDistributionPoint (RFC 5280 §5.2.5)
    inline bool parse_idp(CrlData& d, DerReader value) noexcept {
        DerReader seq;
        if (!value.read(der::sequence, seq) || !value.empty()) {
            return false;
        }
        d.has_idp = true;
        DerReader f;
        bool present;
        if (!seq.read_optional(der::context0, f, present)) {
            return false;
        }
        if (present) {
            DerReader full;
            bool has_full;
            if (!f.read_optional(der::context0, full, has_full)) {
                return false;
            }
            if (has_full) {
                while (!full.empty()) {
                    DerReader gn;
                    unsigned char tag;
                    if (!full.read_any(tag, gn)) {
                        return false;
                    }
                    if (tag == 0x86) {
                        if (!ia5_valid(gn.data(), gn.size()) || d.idp_uris.size() == max_access_locations) {
                            return false;
                        }
                        d.idp_uris.push_back(string(std::string(reinterpret_cast<const char*>(gn.data()), gn.size())));
                    }
                }
            } else {
                DerReader rdn;
                if (!f.read(der::context1, rdn)) {
                    return false;
                }
            }
            if (!f.empty()) {
                return false;
            }
        }
        // the BOOLEANs, [IMPLICIT] primitive: 81 onlyContainsUserCerts, 82
        // onlyContainsCACerts, 84 indirectCRL, 85 onlyContainsAttributeCerts;
        // 83 onlySomeReasons a BIT STRING
        auto flag = [&](unsigned char tag, bool& out) noexcept {
            if (!seq.peek(tag)) {
                return true;
            }
            DerReader b;
            if (!seq.read(tag, b) || b.size() != 1 || b.data()[0] != 0xff) {
                return false;   // a DEFAULT FALSE written as FALSE is not DER
            }
            out = true;
            return true;
        };
        if (!flag(0x81, d.only_user) || !flag(0x82, d.only_ca)) {
            return false;
        }
        if (seq.peek(0x83)) {
            DerReader bits;
            if (!seq.read(0x83, bits)) {
                return false;
            }
            d.only_some_reasons = true;
        }
        if (!flag(0x84, d.indirect) || !flag(0x85, d.only_attribute) || !seq.empty()) {
            return false;
        }
        return int(d.only_user) + int(d.only_ca) + int(d.only_attribute) <= 1;
    }

    // der is the caller's: the parse reads it in place, raw is copied after
    inline expected<void, error> parse_crl(CrlData& d, const slice<const byte>& der) noexcept {
        if (der.size() > std::numeric_limits<uint32_t>::max()) {
            return unexpected<error>(ocsp_fail(0, "a CRL of 4 GiB or more"));
        }
        const auto* base = reinterpret_cast<const unsigned char*>(der.data());
        DerReader in(base, der.size());
        DerReader list, tbs_el, tbs, alg_in, alg_out;
        if (!in.read(der::sequence, list) || !in.empty()) {
            return unexpected<error>(ocsp_fail(in.offset(), "not a CertificateList SEQUENCE"));
        }
        if (!list.read_element(der::sequence, tbs_el)) {
            return unexpected<error>(ocsp_fail(list.offset(), "not a TBSCertList SEQUENCE"));
        }
        d.tbs_at = size_t(tbs_el.data() - base);
        d.tbs_size = tbs_el.size();
        tbs_el.read(der::sequence, tbs);
        if (tbs.peek(der::integer)) {
            uint64_t v;
            if (!tbs.read_small_unsigned(v) || v != 1) {
                return unexpected<error>(ocsp_fail(tbs.offset(), "a CRL of a version other than 1 or 2"));
            }
            d.version = 2;
        }
        size_t alg_at = tbs.offset();
        if (!tbs.read(der::sequence, alg_in)) {
            return unexpected<error>(ocsp_fail(alg_at, "a malformed signature algorithm"));
        }
        size_t outer_at = list.offset();
        if (!list.read(der::sequence, alg_out)) {
            return unexpected<error>(ocsp_fail(outer_at, "a malformed signature algorithm"));
        }
        if (alg_in.size() != alg_out.size() || std::memcmp(alg_in.data(), alg_out.data(), alg_in.size()) != 0) {
            return unexpected<error>(ocsp_fail(outer_at, "the inner and the outer signature algorithms differ"));
        }
        if (auto a = CertParser::read_signature_algorithm(alg_out, d.sig_algorithm, d.sig_algorithm_oid); !a) {
            return a;
        }
        DerReader issuer_el;
        size_t issuer_at = tbs.offset();
        if (!tbs.read_element(der::sequence, issuer_el)) {
            return unexpected<error>(ocsp_fail(issuer_at, "a malformed issuer name"));
        }
        d.issuer_at = issuer_at;
        d.issuer_size = issuer_el.size();
        if (!parse_name(issuer_el, d.issuer) || !issuer_el.empty()) {
            return unexpected<error>(ocsp_fail(issuer_at, "a malformed issuer name"));
        }
        size_t t_at = tbs.offset();
        if (!tbs.read_time(d.this_update)) {
            return unexpected<error>(ocsp_fail(t_at, "a malformed thisUpdate"));
        }
        if (tbs.peek(der::utc_time) || tbs.peek(der::generalized_time)) {
            t_at = tbs.offset();
            if (!tbs.read_time(d.next_update)) {
                return unexpected<error>(ocsp_fail(t_at, "a malformed nextUpdate"));
            }
            d.has_next_update = true;
        }
        if (tbs.peek(der::sequence)) {
            DerReader entries;
            tbs.read(der::sequence, entries);
            if (auto r = parse_crl_entries(d, entries, base); !r) {
                return r;
            }
        }
        if (tbs.peek(der::context0)) {
            size_t at = tbs.offset();
            DerReader wrap, exts;
            if (d.version < 2 || !tbs.read(der::context0, wrap) || !wrap.read(der::sequence, exts) || !wrap.empty()) {
                return unexpected<error>(ocsp_fail(at, "malformed crlExtensions, or extensions in a version 1 CRL"));
            }
            bool bad_value = false;
            if (!read_extensions(exts, d.extensions, at, [&](const DerReader& o, bool critical, DerReader value) noexcept {
                    static constexpr unsigned char ce[] = {0x55, 0x1d};
                    if (o.size() != 3 || std::memcmp(o.data(), ce, 2) != 0) {
                        d.unhandled_critical |= critical;
                        return true;
                    }
                    switch (o.data()[2]) {
                        case 20:     // cRLNumber
                        case 27: {   // deltaCRLIndicator
                            DerReader n;
                            if (!value.read(der::integer, n) || !value.empty() || !integer_ok(n) || (n.data()[0] & 0x80) != 0 || n.size() > 21) {
                                bad_value = true;
                                return false;
                            }
                            (o.data()[2] == 20 ? d.number : d.base_number) = bytes_vector(n.data(), n.size());
                            return true;
                        }
                        case 35: {   // authorityKeyIdentifier
                            DerReader seq, id;
                            bool present;
                            if (!value.read(der::sequence, seq) || !value.empty() || !seq.read_optional(der::implicit0, id, present)) {
                                bad_value = true;
                                return false;
                            }
                            if (present) {
                                d.authority_key_id = bytes_vector(id.data(), id.size());
                            }
                            return true;
                        }
                        case 28:     // issuingDistributionPoint
                            if (!parse_idp(d, value)) {
                                bad_value = true;
                                return false;
                            }
                            return true;
                        case 18:     // issuerAltName
                        case 46:     // freshestCRL
                            return true;
                        default:
                            d.unhandled_critical |= critical;
                            return true;
                    }
                })) {
                return unexpected<error>(ocsp_fail(at, bad_value ? "a malformed CRL extension" : "malformed crlExtensions"));
            }
        }
        if (!tbs.empty()) {
            return unexpected<error>(ocsp_fail(tbs.offset(), "data after the TBSCertList's fields"));
        }
        const unsigned char* sig;
        size_t sig_n;
        unsigned unused;
        size_t sig_at = list.offset();
        if (!list.read_bit_string(sig, sig_n, unused) || !list.empty()) {
            return unexpected<error>(ocsp_fail(sig_at, "a malformed signature"));
        }
        d.signature = bytes_vector(sig, sig_n);
        d.signature_whole = unused == 0;
        return {};
    }

    // Whether two INTEGERs' bytes in their shortest form are the same
    // number (they are equal exactly then)
    SGCL_INLINE_HOT bool same_integer(const unsigned char* a, size_t an, const unsigned char* b, size_t bn) noexcept {
        return an == bn && std::memcmp(a, b, an) == 0;
    }

    // a <=> b for non-negative INTEGERs in their shortest form
    inline int compare_unsigned(const vector<byte>& a, const vector<byte>& b) noexcept {
        if (a.size() != b.size()) {
            return a.size() < b.size() ? -1 : 1;
        }
        int c = a.empty() ? 0 : std::memcmp(a.data(), b.data(), a.size());
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }
}
