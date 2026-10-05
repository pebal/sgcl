//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X.509 revocation: OCSP requests and responses (RFC 6960) and CRLs (RFC 5280
// §5) against what OpenSSL made — the fixtures of tests/crypto/data/revocation
// (make.sh: `openssl ca`, `openssl ocsp`), and responses and lists signed by
// libcrypto in the test where a case needs a time, a signer or an extension
// the fixtures do not have. The request builder is held byte for byte to
// `openssl ocsp -reqout`; every response and list is read by OpenSSL too.
#include "x509_common.h"
#include "sgcl/crypto/x509_revocation.h"

#include <openssl/ocsp.h>

using namespace x509_test;
using sgcl::crypto::errc;
using sgcl::crypto::hash_id;
using x509::revocation_reason;
using x509::revocation_status;

namespace {
    std::string rdir() {
        return (source_root() / "tests/crypto/data/revocation").string();
    }

    bytes_t file_bytes(const std::string& name) {
        std::string s = read_file(rdir() + "/" + name);
        return bytes_t(s.begin(), s.end());
    }

    x509::certificate cert(const std::string& name) {
        return x509::certificate::from_pem(sgcl::string(read_file(rdir() + "/" + name + ".pem"))).value();
    }

    sgcl::slice<const sgcl::byte> sv(const bytes_t& b) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(b.data()), b.size());
    }

    bytes_t vec(const sgcl::vector<sgcl::byte>& v) {
        return bytes_t(reinterpret_cast<const unsigned char*>(v.data()), reinterpret_cast<const unsigned char*>(v.data()) + v.size());
    }

    bytes_t vec(const sgcl::slice<const sgcl::byte>& v) {
        return bytes_t(reinterpret_cast<const unsigned char*>(v.data()), reinterpret_cast<const unsigned char*>(v.data()) + v.size());
    }

    sgcl::time::datetime at(int64_t unix) {
        return sgcl::time::datetime::from_unix(unix, sgcl::time::zone::utc());
    }

    // OpenSSL's objects of the fixtures
    X509* ossl_cert(const std::string& name) {
        std::string pem = read_file(rdir() + "/" + name + ".pem");
        BIO* b = BIO_new_mem_buf(pem.data(), int(pem.size()));
        X509* x = PEM_read_bio_X509(b, nullptr, nullptr, nullptr);
        BIO_free(b);
        return x;
    }

    EVP_PKEY* ossl_key(const std::string& name) {
        std::string pem = read_file(rdir() + "/" + name + ".key");
        BIO* b = BIO_new_mem_buf(pem.data(), int(pem.size()));
        EVP_PKEY* k = PEM_read_bio_PrivateKey(b, nullptr, nullptr, nullptr);
        BIO_free(b);
        return k;
    }

    // An OCSP response signed by libcrypto: the status of `subject` (an
    // issuer `issuer`), signed by `signer`, the times given (0: no
    // nextUpdate), the signer's certificate included when asked
    struct response_spec {
        std::string subject = "good";
        std::string issuer = "int";
        std::string signer = "responder";
        int status = V_OCSP_CERTSTATUS_GOOD;
        int64_t this_update = 1790000000;
        int64_t next_update = 1890000000;
        bool include_signer = true;
        bool by_key = false;
        bool sha256_id = false;
        bytes_t nonce;
        bool critical_extension = false;
    };

    bytes_t make_response(const response_spec& s) {
        X509* subject = ossl_cert(s.subject);
        X509* issuer = ossl_cert(s.issuer);
        X509* signer = ossl_cert(s.signer);
        EVP_PKEY* key = ossl_key(s.signer);
        OCSP_CERTID* id = OCSP_cert_to_id(s.sha256_id ? EVP_sha256() : EVP_sha1(), subject, issuer);
        OCSP_BASICRESP* basic = OCSP_BASICRESP_new();
        ASN1_TIME* tu = ASN1_TIME_set(nullptr, time_t(s.this_update));
        ASN1_TIME* nu = s.next_update ? ASN1_TIME_set(nullptr, time_t(s.next_update)) : nullptr;
        ASN1_TIME* rt = s.status == V_OCSP_CERTSTATUS_REVOKED ? ASN1_TIME_set(nullptr, time_t(s.this_update - 100)) : nullptr;
        OCSP_basic_add1_status(basic, id, s.status, s.status == V_OCSP_CERTSTATUS_REVOKED ? OCSP_REVOKED_STATUS_SUPERSEDED : -1, rt, tu, nu);
        if (!s.nonce.empty()) {
            OCSP_basic_add1_nonce(basic, const_cast<unsigned char*>(s.nonce.data()), int(s.nonce.size()));
        }
        if (s.critical_extension) {
            ASN1_OBJECT* o = OBJ_txt2obj("1.2.3.4.5", 1);
            ASN1_OCTET_STRING* v = ASN1_OCTET_STRING_new();
            ASN1_OCTET_STRING_set(v, (const unsigned char*)"\x05\x00", 2);
            X509_EXTENSION* e = X509_EXTENSION_create_by_OBJ(nullptr, o, 1, v);
            OCSP_BASICRESP_add_ext(basic, e, -1);
            X509_EXTENSION_free(e);
            ASN1_OCTET_STRING_free(v);
            ASN1_OBJECT_free(o);
        }
        unsigned long flags = (s.include_signer ? 0 : OCSP_NOCERTS) | (s.by_key ? OCSP_RESPID_KEY : 0);
        OCSP_basic_sign(basic, signer, key, EVP_sha256(), nullptr, flags);
        OCSP_RESPONSE* resp = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, basic);
        unsigned char* out = nullptr;
        int n = i2d_OCSP_RESPONSE(resp, &out);
        bytes_t r(out, out + n);
        OPENSSL_free(out);
        OCSP_RESPONSE_free(resp);
        OCSP_BASICRESP_free(basic);
        ASN1_TIME_free(tu);
        ASN1_TIME_free(nu);
        ASN1_TIME_free(rt);
        OCSP_CERTID_free(id);
        EVP_PKEY_free(key);
        X509_free(subject);
        X509_free(issuer);
        X509_free(signer);
        return r;
    }

    // A CRL signed by libcrypto: `count` entries of serials 1..count (the
    // last of them `last_serial` when given), the extensions given as DER
    // values by OID
    struct crl_spec {
        std::string issuer = "int";
        size_t count = 0;
        int64_t this_update = 1790000000;
        int64_t next_update = 1890000000;
        std::vector<std::pair<std::string, bytes_t>> extensions;   // OID, DER value
        std::vector<bool> critical;
    };

    bytes_t make_crl(const crl_spec& s) {
        X509* issuer = ossl_cert(s.issuer);
        EVP_PKEY* key = ossl_key(s.issuer);
        X509_CRL* crl = X509_CRL_new();
        X509_CRL_set_version(crl, X509_CRL_VERSION_2);
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(issuer));
        ASN1_TIME* tu = ASN1_TIME_set(nullptr, time_t(s.this_update));
        ASN1_TIME* nu = ASN1_TIME_set(nullptr, time_t(s.next_update));
        X509_CRL_set1_lastUpdate(crl, tu);
        X509_CRL_set1_nextUpdate(crl, nu);
        for (size_t i = 0; i < s.count; ++i) {
            X509_REVOKED* r = X509_REVOKED_new();
            ASN1_INTEGER* serial = ASN1_INTEGER_new();
            ASN1_INTEGER_set_uint64(serial, uint64_t(i + 1) * 7919u);
            X509_REVOKED_set_serialNumber(r, serial);
            X509_REVOKED_set_revocationDate(r, tu);
            X509_CRL_add0_revoked(crl, r);
            ASN1_INTEGER_free(serial);
        }
        for (size_t i = 0; i < s.extensions.size(); ++i) {
            ASN1_OBJECT* o = OBJ_txt2obj(s.extensions[i].first.c_str(), 1);
            ASN1_OCTET_STRING* v = ASN1_OCTET_STRING_new();
            ASN1_OCTET_STRING_set(v, s.extensions[i].second.data(), int(s.extensions[i].second.size()));
            X509_EXTENSION* e = X509_EXTENSION_create_by_OBJ(nullptr, o, i < s.critical.size() && s.critical[i] ? 1 : 0, v);
            X509_CRL_add_ext(crl, e, -1);
            X509_EXTENSION_free(e);
            ASN1_OCTET_STRING_free(v);
            ASN1_OBJECT_free(o);
        }
        X509_CRL_sort(crl);
        X509_CRL_sign(crl, key, EVP_sha256());
        unsigned char* out = nullptr;
        int n = i2d_X509_CRL(crl, &out);
        bytes_t r(out, out + n);
        OPENSSL_free(out);
        X509_CRL_free(crl);
        ASN1_TIME_free(tu);
        ASN1_TIME_free(nu);
        EVP_PKEY_free(key);
        X509_free(issuer);
        return r;
    }

    // The fixtures' responses were made 2026-10-05 with a century ahead:
    // a time inside every one of them
    const int64_t fixture_now = 1800000000;   // 2027-01-15
}

// The extensions a certificate points at its revocation with
TEST(Crypto_X509_Revocation, CertificateLocations) {
    auto good = cert("good");
    ASSERT_EQ(good.ocsp_servers().size(), 1u);
    EXPECT_EQ(good.ocsp_servers()[0], "http://127.0.0.1:47811");
    ASSERT_EQ(good.issuing_certificate_urls().size(), 1u);
    EXPECT_EQ(good.issuing_certificate_urls()[0], "http://127.0.0.1:47812/int.cer");
    ASSERT_EQ(good.crl_distribution_points().size(), 1u);
    EXPECT_EQ(good.crl_distribution_points()[0], "http://127.0.0.1:47812/int.crl");
    EXPECT_FALSE(good.must_staple());
    EXPECT_TRUE(cert("staple").must_staple());
    auto crlonly = cert("crlonly");
    EXPECT_TRUE(crlonly.ocsp_servers().empty());
    EXPECT_EQ(crlonly.crl_distribution_points().size(), 1u);
    auto root = cert("root");
    EXPECT_TRUE(root.ocsp_servers().empty());
    EXPECT_TRUE(root.crl_distribution_points().empty());
    EXPECT_EQ(cert("int").crl_distribution_points()[0], "http://127.0.0.1:47812/root.crl");
    // the chain still verifies with them (none is critical)
    x509::verify_options o;
    o.roots = x509::certificate_pool::from_pem(sgcl::string(read_file(rdir() + "/root.pem")));
    o.intermediates.add(cert("int"));
    o.dns_name = "localhost";
    o.time = at(fixture_now);
    EXPECT_TRUE(cert("staple").verify(o).has_value());
}

// The request builder against `openssl ocsp -reqout`, byte for byte
TEST(Crypto_X509_Revocation, RequestMatchesOpenSsl) {
    auto good = cert("good");
    auto issuer = cert("int");
    auto r1 = x509::ocsp_request::make(good, issuer);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(vec(r1->raw()), file_bytes("ocsp_req_good_sha1.der"));
    EXPECT_EQ(r1->hash(), hash_id::sha1);
    EXPECT_TRUE(r1->nonce().empty());
    auto r256 = x509::ocsp_request::make(good, issuer, {.hash = hash_id::sha256});
    ASSERT_TRUE(r256.has_value());
    EXPECT_EQ(vec(r256->raw()), file_bytes("ocsp_req_good_sha256.der"));
    EXPECT_EQ(r256->issuer_name_hash().size(), 32u);
    for (auto h : {hash_id::sha384, hash_id::sha512}) {
        auto r = x509::ocsp_request::make(good, issuer, {.hash = h});
        ASSERT_TRUE(r.has_value());
        EXPECT_TRUE(r->matches(good, issuer));
        auto back = x509::ocsp_request::parse(r->raw());
        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(back->hash(), h);
    }
    EXPECT_THROW((void)x509::ocsp_request::make(good, issuer, {.hash = hash_id::sha3_256}), std::invalid_argument);
    EXPECT_TRUE(r1->matches(good, issuer));
    EXPECT_FALSE(r1->matches(cert("revoked"), issuer));
    EXPECT_FALSE(r1->matches(good, cert("root")));
    // a request with a nonce: 16 random bytes, read back here and by OpenSSL
    auto rn = x509::ocsp_request::make(good, issuer, {.nonce = true});
    ASSERT_TRUE(rn.has_value());
    EXPECT_EQ(rn->nonce().size(), 16u);
    auto rn2 = x509::ocsp_request::make(good, issuer, {.nonce = true});
    EXPECT_NE(vec(rn->nonce()), vec(rn2->nonce()));
    bytes_t der = vec(rn->raw());
    const unsigned char* p = der.data();
    OCSP_REQUEST* req = d2i_OCSP_REQUEST(nullptr, &p, long(der.size()));
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(OCSP_request_onereq_count(req), 1);
    EXPECT_GE(OCSP_REQUEST_get_ext_by_NID(req, NID_id_pkix_OCSP_Nonce, -1), 0);
    OCSP_REQUEST_free(req);
    auto back = x509::ocsp_request::parse(rn->raw());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(vec(back->nonce()), vec(rn->nonce()));
    EXPECT_EQ(vec(back->serial_number()), vec(good.serial_number()));
    // OpenSSL's request with its nonce
    auto theirs = x509::ocsp_request::parse(sv(file_bytes("ocsp_req_good_nonce.der")));
    ASSERT_TRUE(theirs.has_value());
    EXPECT_FALSE(theirs->nonce().empty());
    EXPECT_TRUE(theirs->matches(good, issuer));
}

TEST(Crypto_X509_Revocation, RequestBoundaries) {
    auto good = cert("good");
    // the issuer by name: another certificate is refused
    auto wrong = x509::ocsp_request::make(good, cert("root"));
    ASSERT_FALSE(wrong.has_value());
    EXPECT_EQ(wrong.error().reason(), reason::unknown_authority);
    // the GET of Appendix A: '/' joined once, base64 escaped
    auto r = x509::ocsp_request::make(good, cert("int")).value();
    sgcl::string u = r.url("http://ocsp.example.com");
    EXPECT_EQ(u.view().substr(0, 24), "http://ocsp.example.com/");
    EXPECT_EQ(r.url("http://ocsp.example.com/"), u);
    EXPECT_EQ(u.view().find('+'), std::string_view::npos);
    EXPECT_EQ(u.view().find('=', 7), std::string_view::npos);
    EXPECT_EQ(u.view().find('/', 24), std::string_view::npos);
    EXPECT_EQ(r.url("").view()[0], '/');
    // malformed: empty, every cut, a byte after, two requests
    EXPECT_FALSE(x509::ocsp_request::parse(sgcl::slice<const sgcl::byte>()).has_value());
    bytes_t der = vec(r.raw());
    for (size_t n = 0; n < der.size(); ++n) {
        bytes_t cut(der.begin(), der.begin() + std::ptrdiff_t(n));
        EXPECT_FALSE(x509::ocsp_request::parse(sv(cut)).has_value()) << n;
    }
    bytes_t longer = der;
    longer.push_back(0);
    EXPECT_FALSE(x509::ocsp_request::parse(sv(longer)).has_value());
    X509* subject = ossl_cert("good");
    X509* issuer = ossl_cert("int");
    OCSP_REQUEST* two = OCSP_REQUEST_new();
    OCSP_request_add0_id(two, OCSP_cert_to_id(nullptr, subject, issuer));
    OCSP_request_add0_id(two, OCSP_cert_to_id(nullptr, subject, issuer));
    unsigned char* out = nullptr;
    int n = i2d_OCSP_REQUEST(two, &out);
    bytes_t twice(out, out + n);
    OPENSSL_free(out);
    OCSP_REQUEST_free(two);
    X509_free(subject);
    X509_free(issuer);
    auto t = x509::ocsp_request::parse(sv(twice));
    ASSERT_FALSE(t.has_value());
    EXPECT_EQ(t.error().code(), errc::malformed);
    // past the bound
    bytes_t huge((1 << 20) + 1, 0x30);
    EXPECT_FALSE(x509::ocsp_request::parse(sv(huge)).has_value());
    // a copy is the same request
    auto copy = r;
    EXPECT_EQ(vec(copy.raw()), der);
}

// The fixtures of `openssl ocsp`: good, revoked, unknown, by the delegated
// responder, by the issuer, by a signer the issuer never named
TEST(Crypto_X509_Revocation, ResponsesOfOpenSsl) {
    auto issuer = cert("int");
    x509::ocsp_verify_options o;
    o.time = at(fixture_now);
    auto good = x509::ocsp_response::parse(sv(file_bytes("ocsp_good.der")));
    ASSERT_TRUE(good.has_value());
    EXPECT_EQ(good->status(), x509::ocsp_response_status::successful);
    ASSERT_EQ(good->responses().size(), 1u);
    ASSERT_EQ(good->certificates().size(), 1u);
    EXPECT_EQ(good->certificates()[0], cert("responder"));
    EXPECT_FALSE(good->responder_name().empty());
    EXPECT_TRUE(good->responder_key_hash().empty());
    EXPECT_EQ(good->signature_algorithm(), x509::signature_algorithm::ecdsa_with_sha256);
    auto s = good->verify(cert("good"), issuer, o);
    ASSERT_TRUE(s.has_value()) << s.error().message().view();
    EXPECT_EQ(s->status, revocation_status::good);
    EXPECT_EQ(s->hash, hash_id::sha1);
    EXPECT_EQ(vec(s->serial_number), vec(cert("good").serial_number()));
    ASSERT_TRUE(s->next_update.has_value());
    EXPECT_GT(s->next_update->unix(), s->this_update.unix());
    EXPECT_FALSE(s->revocation_time.has_value());
    EXPECT_TRUE(good->check_signature_from(cert("responder")).has_value());
    EXPECT_EQ(good->check_signature_from(issuer).error().reason(), reason::invalid_signature);
    // the same response for another certificate: no status of it
    auto other = good->verify(cert("revoked"), issuer, o);
    ASSERT_FALSE(other.has_value());
    EXPECT_EQ(other.error().reason(), reason::revocation_unknown);

    auto revoked = x509::ocsp_response::parse(sv(file_bytes("ocsp_revoked.der")))->verify(cert("revoked"), issuer, o);
    ASSERT_TRUE(revoked.has_value());
    EXPECT_EQ(revoked->status, revocation_status::revoked);
    ASSERT_TRUE(revoked->revocation_time.has_value());
    EXPECT_EQ(revoked->reason, revocation_reason::key_compromise);

    auto unknown = x509::ocsp_response::parse(sv(file_bytes("ocsp_unknown.der")))->verify(cert("unknown"), issuer, o);
    ASSERT_TRUE(unknown.has_value());
    EXPECT_EQ(unknown->status, revocation_status::unknown);

    auto by_issuer = x509::ocsp_response::parse(sv(file_bytes("ocsp_good_issuer.der")))->verify(cert("good"), issuer, o);
    ASSERT_TRUE(by_issuer.has_value());
    EXPECT_EQ(by_issuer->status, revocation_status::good);

    auto rogue = x509::ocsp_response::parse(sv(file_bytes("ocsp_good_rogue.der")))->verify(cert("good"), issuer, o);
    ASSERT_FALSE(rogue.has_value());
    EXPECT_EQ(rogue.error().reason(), reason::unknown_authority);

    auto int_root = x509::ocsp_response::parse(sv(file_bytes("ocsp_int_root.der")))->verify(issuer, cert("root"), o);
    ASSERT_TRUE(int_root.has_value());
    EXPECT_EQ(int_root->status, revocation_status::good);

    auto staple = x509::ocsp_response::parse(sv(file_bytes("ocsp_staple.der")))->verify(cert("staple"), issuer, o);
    ASSERT_TRUE(staple.has_value());

    // the nonce: carried, echoed, refused when it differs
    auto nonced = x509::ocsp_response::parse(sv(file_bytes("ocsp_good_nonce.der")));
    ASSERT_TRUE(nonced.has_value());
    ASSERT_FALSE(nonced->nonce().empty());
    x509::ocsp_verify_options on = o;
    on.nonce = nonced->nonce();
    EXPECT_TRUE(nonced->verify(cert("good"), issuer, on).has_value());
    on.nonce = sgcl::vector<sgcl::byte>(16, sgcl::byte(7));
    auto bad_nonce = nonced->verify(cert("good"), issuer, on);
    ASSERT_FALSE(bad_nonce.has_value());
    EXPECT_EQ(bad_nonce.error().reason(), reason::revocation_unknown);
    // a nonce asked for of a response without one
    EXPECT_FALSE(good->verify(cert("good"), issuer, on).has_value());
}

// The times: current from thisUpdate to nextUpdate, by the skew either side;
// without nextUpdate, max_age
TEST(Crypto_X509_Revocation, ResponseTimes) {
    auto issuer = cert("int");
    auto good = cert("good");
    response_spec s;
    s.this_update = 1800000000;
    s.next_update = 1800086400;
    auto r = x509::ocsp_response::parse(sv(make_response(s))).value();
    x509::ocsp_verify_options o;
    o.time = at(1800000000 - 299);
    EXPECT_TRUE(r.verify(good, issuer, o).has_value());
    o.time = at(1800000000 - 301);
    EXPECT_EQ(r.verify(good, issuer, o).error().reason(), reason::not_yet_valid);
    o.time = at(1800086400 + 299);
    EXPECT_TRUE(r.verify(good, issuer, o).has_value());
    o.time = at(1800086400 + 301);
    EXPECT_EQ(r.verify(good, issuer, o).error().reason(), reason::expired);
    o.skew = sgcl::duration();
    o.time = at(1800086400 + 1);
    EXPECT_EQ(r.verify(good, issuer, o).error().reason(), reason::expired);
    o.time = at(1800000000 - 1);
    EXPECT_EQ(r.verify(good, issuer, o).error().reason(), reason::not_yet_valid);
    // no nextUpdate: max_age from thisUpdate
    s.next_update = 0;
    auto open = x509::ocsp_response::parse(sv(make_response(s))).value();
    EXPECT_FALSE(open.responses()[0].next_update.has_value());
    x509::ocsp_verify_options m;
    m.time = at(1800000000 + 7 * 86400 + 200);
    EXPECT_TRUE(open.verify(good, issuer, m).has_value());
    m.max_age = sgcl::hour;
    EXPECT_EQ(open.verify(good, issuer, m).error().reason(), reason::expired);
    // the time now by default: the fixtures are current for a century
    EXPECT_TRUE(x509::ocsp_response::parse(sv(file_bytes("ocsp_good.der")))->verify(good, issuer).has_value());
}

// The signer: a responder without id-kp-OCSPSigning, one not sent, by key,
// SHA-256 CertIDs, a critical extension unknown
TEST(Crypto_X509_Revocation, ResponseSigners) {
    auto issuer = cert("int");
    auto good = cert("good");
    x509::ocsp_verify_options o;
    o.time = at(1800000000);
    response_spec s;
    s.signer = "revoked";   // a leaf of int: no OCSPSigning
    auto r = x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().reason(), reason::incompatible_usage);
    s = response_spec();
    s.include_signer = false;   // the responder named, its certificate not sent
    r = x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().reason(), reason::unknown_authority);
    s = response_spec();
    s.by_key = true;
    auto byk = x509::ocsp_response::parse(sv(make_response(s)));
    ASSERT_TRUE(byk.has_value());
    EXPECT_EQ(byk->responder_key_hash().size(), 20u);
    EXPECT_TRUE(byk->verify(good, issuer, o).has_value());
    s.signer = "int";
    s.include_signer = false;
    EXPECT_TRUE(x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o).has_value());
    s = response_spec();
    s.sha256_id = true;
    auto h = x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o);
    ASSERT_TRUE(h.has_value());
    EXPECT_EQ(h->hash, hash_id::sha256);
    s = response_spec();
    s.critical_extension = true;
    r = x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().reason(), reason::unhandled_critical_extension);
    s = response_spec();
    s.status = V_OCSP_CERTSTATUS_REVOKED;
    auto rv = x509::ocsp_response::parse(sv(make_response(s)))->verify(good, issuer, o);
    ASSERT_TRUE(rv.has_value());
    EXPECT_EQ(rv->status, revocation_status::revoked);
    EXPECT_EQ(rv->reason, revocation_reason::superseded);
    EXPECT_EQ(rv->revocation_time->unix(), 1790000000 - 100);
    // a signature broken
    bytes_t der = make_response(response_spec());
    auto sig_at = x509::ocsp_response::parse(sv(der))->signature().size();
    (void)sig_at;
    bytes_t broken = der;
    // the last byte of the signature lies before the certs: flip a byte of
    // the ResponseData instead (the producedAt's last digit), which keeps it DER
    auto parsed = x509::ocsp_response::parse(sv(der)).value();
    size_t tbs_off = size_t(parsed.raw_tbs().data() - parsed.raw().data());
    for (size_t i = tbs_off; i + 15 < tbs_off + parsed.raw_tbs().size(); ++i) {
        if (broken[i] == 0x18 && broken[i + 1] == 15) {   // the GeneralizedTime producedAt
            broken[i + 15] = broken[i + 15] == '0' ? '1' : '0';
            break;
        }
    }
    auto b = x509::ocsp_response::parse(sv(broken));
    ASSERT_TRUE(b.has_value());
    auto bv = b->verify(good, issuer, o);
    ASSERT_FALSE(bv.has_value());
    EXPECT_EQ(bv.error().reason(), reason::invalid_signature);
}

TEST(Crypto_X509_Revocation, ResponseMalformed) {
    // not successful: only the status
    for (int st : {1, 2, 3, 5, 6}) {
        bytes_t der = {0x30, 0x03, 0x0a, 0x01, (unsigned char)st};
        auto r = x509::ocsp_response::parse(sv(der));
        ASSERT_TRUE(r.has_value()) << st;
        EXPECT_EQ(int(r->status()), st);
        EXPECT_TRUE(r->responses().empty());
        auto v = r->verify(cert("good"), cert("int"));
        ASSERT_FALSE(v.has_value());
        EXPECT_EQ(v.error().reason(), reason::revocation_unknown);
        EXPECT_FALSE(r->check_signature_from(cert("int")).has_value());
    }
    for (int st : {4, 7, 0}) {
        bytes_t der = {0x30, 0x03, 0x0a, 0x01, (unsigned char)st};
        EXPECT_FALSE(x509::ocsp_response::parse(sv(der)).has_value()) << st;   // 0: successful without its bytes
    }
    EXPECT_FALSE(x509::ocsp_response::parse(sgcl::slice<const sgcl::byte>()).has_value());
    bytes_t der = file_bytes("ocsp_good.der");
    for (size_t n = 0; n < der.size(); ++n) {
        bytes_t cut(der.begin(), der.begin() + std::ptrdiff_t(n));
        EXPECT_FALSE(x509::ocsp_response::parse(sv(cut)).has_value()) << n;
    }
    bytes_t longer = der;
    longer.push_back(0);
    EXPECT_FALSE(x509::ocsp_response::parse(sv(longer)).has_value());
    // a response of another type than basic
    bytes_t other = {0x30, 0x14, 0x0a, 0x01, 0x00, 0xa0, 0x0f, 0x30, 0x0d, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x09, 0x04, 0x00};
    other[1] = (unsigned char)(other.size() - 2);
    other[6] = (unsigned char)(other.size() - 7);
    other[8] = (unsigned char)(other.size() - 9);
    auto o = x509::ocsp_response::parse(sv(other));
    ASSERT_FALSE(o.has_value());
    EXPECT_EQ(o.error().code(), errc::unsupported);
    // past the bound
    bytes_t huge((1 << 20) + 1, 0x30);
    EXPECT_FALSE(x509::ocsp_response::parse(sv(huge)).has_value());
    // every byte changed in turn: never a crash, and what parses still
    // verifies only when the bytes are the signer's
    size_t verified = 0;
    for (size_t i = 0; i < der.size(); ++i) {
        bytes_t m = der;
        m[i] ^= 0x01;
        auto r = x509::ocsp_response::parse(sv(m));
        if (r && r->verify(cert("good"), cert("int"), {.time = at(fixture_now)})) {
            ++verified;
        }
    }
    EXPECT_EQ(verified, 0u);
}

// The CRLs of `openssl ca`: the complete list of int, its delta, root's
TEST(Crypto_X509_Revocation, ListsOfOpenSsl) {
    auto issuer = cert("int");
    auto crl = x509::revocation_list::parse(sv(file_bytes("int.crl")));
    ASSERT_TRUE(crl.has_value()) << crl.error().message().view();
    EXPECT_EQ(crl->version(), 2);
    EXPECT_EQ(crl->issuer().to_string(), "CN=SGCL Revocation Intermediate");
    EXPECT_EQ(vec(crl->number()), (bytes_t{0x02}));
    EXPECT_FALSE(crl->is_delta());
    EXPECT_EQ(vec(crl->authority_key_id()), vec(issuer.subject_key_id()));
    ASSERT_EQ(crl->size(), 2u);
    ASSERT_TRUE(crl->next_update().has_value());
    EXPECT_EQ(crl->signature_algorithm(), x509::signature_algorithm::ecdsa_with_sha256);
    auto e0 = (*crl)[0];
    EXPECT_EQ(vec(e0.serial_number), vec(cert("revoked").serial_number()));
    EXPECT_EQ(e0.reason, revocation_reason::key_compromise);
    EXPECT_EQ((*crl)[1].reason, revocation_reason::certificate_hold);
    EXPECT_THROW((void)(*crl)[2], std::out_of_range);
    EXPECT_TRUE(crl->lookup(cert("revoked")).has_value());
    EXPECT_FALSE(crl->lookup(cert("good")).has_value());
    EXPECT_TRUE(crl->check_signature_from(issuer).has_value());
    EXPECT_EQ(crl->check_signature_from(cert("root")).error().reason(), reason::unknown_authority);
    auto t = at(fixture_now);
    EXPECT_EQ(crl->status_of(cert("good"), issuer, t).value(), revocation_status::good);
    EXPECT_EQ(crl->status_of(cert("revoked"), issuer, t).value(), revocation_status::revoked);
    EXPECT_EQ(crl->status_of(cert("held"), issuer, t).value(), revocation_status::revoked);
    EXPECT_EQ(crl->status_of(cert("good"), issuer).value(), revocation_status::good);   // now
    // another issuer's certificate
    auto wrong = crl->status_of(issuer, cert("root"), t);
    ASSERT_FALSE(wrong.has_value());
    EXPECT_EQ(wrong.error().reason(), reason::revocation_unknown);
    // root's list, for the intermediate
    auto root_crl = x509::revocation_list::parse(sv(file_bytes("root.crl")));
    ASSERT_TRUE(root_crl.has_value());
    EXPECT_TRUE(root_crl->empty());
    EXPECT_EQ(root_crl->status_of(issuer, cert("root"), t).value(), revocation_status::good);
    // the delta: alone it says nothing; over the list it decides
    auto delta = x509::revocation_list::parse(sv(file_bytes("int_delta.crl")));
    ASSERT_TRUE(delta.has_value());
    EXPECT_TRUE(delta->is_delta());
    EXPECT_EQ(vec(delta->base_number()), (bytes_t{0x02}));
    EXPECT_EQ(delta->status_of(cert("good"), issuer, t).error().reason(), reason::revocation_unknown);
    EXPECT_EQ(crl->status_of(cert("held"), issuer, *delta, t).value(), revocation_status::good);
    EXPECT_EQ(crl->status_of(cert("revoked"), issuer, *delta, t).value(), revocation_status::revoked);
    EXPECT_EQ(crl->status_of(cert("good"), issuer, *delta, t).value(), revocation_status::good);
    EXPECT_FALSE(crl->status_of(cert("good"), issuer, *crl, t).has_value());     // not a delta
    EXPECT_FALSE(delta->status_of(cert("good"), issuer, *delta, t).has_value()); // not a complete list
    // OpenSSL reads them alike
    bytes_t d = file_bytes("int.crl");
    const unsigned char* p = d.data();
    X509_CRL* x = d2i_X509_CRL(nullptr, &p, long(d.size()));
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(size_t(sk_X509_REVOKED_num(X509_CRL_get_REVOKED(x))), crl->size());
    X509_CRL_free(x);
}

TEST(Crypto_X509_Revocation, ListTimesAndScope) {
    auto issuer = cert("int");
    auto good = cert("good");
    crl_spec s;
    auto crl = x509::revocation_list::parse(sv(make_crl(s))).value();
    EXPECT_EQ(crl.status_of(good, issuer, at(1800000000)).value(), revocation_status::good);
    EXPECT_EQ(crl.status_of(good, issuer, at(1790000000 - 1)).error().reason(), reason::not_yet_valid);
    EXPECT_EQ(crl.status_of(good, issuer, at(1890000000 + 1)).error().reason(), reason::expired);
    EXPECT_TRUE(crl.number().empty());
    // issuingDistributionPoint: user certificates only, CA certificates only
    s.extensions = {{"2.5.29.28", bytes_t{0x30, 0x03, 0x82, 0x01, 0xff}}};
    s.critical = {true};
    auto ca_only = x509::revocation_list::parse(sv(make_crl(s))).value();
    EXPECT_TRUE(ca_only.only_ca_certificates());
    EXPECT_EQ(ca_only.status_of(good, issuer, at(1800000000)).error().reason(), reason::revocation_unknown);
    s.extensions = {{"2.5.29.28", bytes_t{0x30, 0x03, 0x81, 0x01, 0xff}}};
    auto user_only = x509::revocation_list::parse(sv(make_crl(s))).value();
    EXPECT_TRUE(user_only.only_user_certificates());
    EXPECT_EQ(user_only.status_of(good, issuer, at(1800000000)).value(), revocation_status::good);
    // a point of the certificate's, and one of another
    std::string url = "http://127.0.0.1:47812/int.crl";
    bytes_t uri = der(0x86, bytes_t(url.begin(), url.end()));
    s.extensions = {{"2.5.29.28", der(0x30, der(0xa0, der(0xa0, uri)))}};
    auto mine = x509::revocation_list::parse(sv(make_crl(s))).value();
    ASSERT_EQ(mine.distribution_point().size(), 1u);
    EXPECT_EQ(mine.status_of(good, issuer, at(1800000000)).value(), revocation_status::good);
    std::string other = "http://elsewhere/x.crl";
    s.extensions = {{"2.5.29.28", der(0x30, der(0xa0, der(0xa0, der(0x86, bytes_t(other.begin(), other.end())))))}};
    EXPECT_EQ(x509::revocation_list::parse(sv(make_crl(s)))->status_of(good, issuer, at(1800000000)).error().reason(), reason::revocation_unknown);
    // indirect: refused
    s.extensions = {{"2.5.29.28", bytes_t{0x30, 0x03, 0x84, 0x01, 0xff}}};
    auto indirect = x509::revocation_list::parse(sv(make_crl(s))).value();
    EXPECT_TRUE(indirect.indirect());
    EXPECT_EQ(indirect.status_of(good, issuer, at(1800000000)).error().reason(), reason::revocation_unknown);
    // a DEFAULT FALSE written: not DER
    s.extensions = {{"2.5.29.28", bytes_t{0x30, 0x03, 0x81, 0x01, 0x00}}};
    EXPECT_FALSE(x509::revocation_list::parse(sv(make_crl(s))).has_value());
    // a critical extension unknown
    s.extensions = {{"1.2.3.4", bytes_t{0x05, 0x00}}};
    s.critical = {true};
    EXPECT_EQ(x509::revocation_list::parse(sv(make_crl(s)))->status_of(good, issuer, at(1800000000)).error().reason(), reason::unhandled_critical_extension);
    s.critical = {false};
    EXPECT_TRUE(x509::revocation_list::parse(sv(make_crl(s)))->status_of(good, issuer, at(1800000000)).has_value());
    // a list of root's for a certificate of int
    crl_spec r;
    r.issuer = "root";
    EXPECT_EQ(x509::revocation_list::parse(sv(make_crl(r)))->status_of(good, issuer, at(1800000000)).error().reason(), reason::revocation_unknown);
    // a list of int checked against root's key: the signer is not the issuer
    auto signed_by_int = x509::revocation_list::parse(sv(make_crl(crl_spec()))).value();
    EXPECT_EQ(signed_by_int.check_signature_from(cert("good")).error().reason(), reason::unknown_authority);
}

// A large list: read without an entry's allocation, looked up against
// OpenSSL's own lookup
TEST(Crypto_X509_Revocation, LargeList) {
    crl_spec s;
    s.count = 20000;
    bytes_t der_bytes = make_crl(s);
    auto crl = x509::revocation_list::parse(sv(der_bytes));
    ASSERT_TRUE(crl.has_value());
    EXPECT_EQ(crl->size(), 20000u);
    const unsigned char* p = der_bytes.data();
    X509_CRL* x = d2i_X509_CRL(nullptr, &p, long(der_bytes.size()));
    ASSERT_NE(x, nullptr);
    for (uint64_t k : {uint64_t(1), uint64_t(777), uint64_t(20000), uint64_t(20001), uint64_t(0)}) {
        ASN1_INTEGER* serial = ASN1_INTEGER_new();
        ASN1_INTEGER_set_uint64(serial, k * 7919u);
        X509_REVOKED* found = nullptr;
        int theirs = X509_CRL_get0_by_serial(x, &found, serial);
        unsigned char* buf = nullptr;
        int n = i2d_ASN1_INTEGER(serial, &buf);
        // the content bytes, after the tag and the short length
        bytes_t content(buf + 2, buf + n);
        OPENSSL_free(buf);
        ASN1_INTEGER_free(serial);
        EXPECT_EQ(crl->lookup(sv(content)).has_value(), theirs == 1) << k;
    }
    X509_CRL_free(x);
    EXPECT_TRUE(crl->check_signature_from(cert("int")).has_value());
}

TEST(Crypto_X509_Revocation, ListMalformed) {
    EXPECT_FALSE(x509::revocation_list::parse(sgcl::slice<const sgcl::byte>()).has_value());
    bytes_t der_bytes = file_bytes("int.crl");
    for (size_t n = 0; n < der_bytes.size(); ++n) {
        bytes_t cut(der_bytes.begin(), der_bytes.begin() + std::ptrdiff_t(n));
        EXPECT_FALSE(x509::revocation_list::parse(sv(cut)).has_value()) << n;
    }
    bytes_t longer = der_bytes;
    longer.push_back(0);
    EXPECT_FALSE(x509::revocation_list::parse(sv(longer)).has_value());
    for (size_t i = 0; i < der_bytes.size(); ++i) {
        bytes_t m = der_bytes;
        m[i] ^= 0x80;
        auto r = x509::revocation_list::parse(sv(m));
        if (r) {
            (void)r->status_of(cert("good"), cert("int"), at(fixture_now));
        }
    }
    // PEM: the X509 CRL block, whatever is around it
    X509_CRL* x = nullptr;
    const unsigned char* p = der_bytes.data();
    x = d2i_X509_CRL(nullptr, &p, long(der_bytes.size()));
    BIO* b = BIO_new(BIO_s_mem());
    BIO_puts(b, "text before\n");
    PEM_write_bio_X509_CRL(b, x);
    char* data;
    long n = BIO_get_mem_data(b, &data);
    std::string pem(data, size_t(n));
    BIO_free(b);
    X509_CRL_free(x);
    auto fp = x509::revocation_list::from_pem(sgcl::string(pem));
    ASSERT_TRUE(fp.has_value());
    EXPECT_EQ(vec(fp->raw()), der_bytes);
    EXPECT_FALSE(x509::revocation_list::from_pem(sgcl::string(read_file(rdir() + "/good.pem"))).has_value());
    EXPECT_FALSE(x509::revocation_list::from_pem(sgcl::string()).has_value());
}
