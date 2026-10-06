//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Certificates and certificate requests made (x509::create_certificate,
// create_certificate_request) and requests read (certificate_request), held
// to OpenSSL's libcrypto: what is made parses there, its signature verifies
// there under the issuer's key (every kind: P-256, P-384, P-521, Ed25519, RSA), its
// extensions read there as written, a chain of it verifies with
// X509_verify_cert; a request OpenSSL makes reads here and its signature
// verifies; the templates that cannot be written refused, requests that are
// not ones refused, a signature altered refused.
#include "tests/types.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace sgcl;
namespace x509 = sgcl::crypto::x509;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    struct X509Free {
        void operator()(X509* x) const {
            X509_free(x);
        }
    };
    struct ReqFree {
        void operator()(X509_REQ* x) const {
            X509_REQ_free(x);
        }
    };
    struct KeyFree {
        void operator()(EVP_PKEY* k) const {
            EVP_PKEY_free(k);
        }
    };
    using X509Ptr = std::unique_ptr<X509, X509Free>;
    using ReqPtr = std::unique_ptr<X509_REQ, ReqFree>;
    using KeyPtr = std::unique_ptr<EVP_PKEY, KeyFree>;

    X509Ptr openssl_cert(const slice<const byte>& der) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(der.data());
        return X509Ptr(d2i_X509(nullptr, &p, long(der.size())));
    }

    ReqPtr openssl_req(const slice<const byte>& der) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(der.data());
        return ReqPtr(d2i_X509_REQ(nullptr, &p, long(der.size())));
    }

    // The five kinds of key, each with its PKCS #8 for OpenSSL
    struct Keys {
        crypto::p256::private_key p256 = crypto::p256::private_key::generate();
        crypto::p384::private_key p384 = crypto::p384::private_key::generate();
        crypto::p521::private_key p521 = crypto::p521::private_key::generate();
        crypto::ed25519::private_key ed25519 = crypto::ed25519::private_key::generate();
        crypto::rsa::private_key rsa = crypto::rsa::private_key::generate(2048);
    };

    template<class K>
    KeyPtr openssl_public(const K& k) {
        auto der = k.public_key().to_pkix_der();
        const unsigned char* p = reinterpret_cast<const unsigned char*>(der.data());
        return KeyPtr(d2i_PUBKEY(nullptr, &p, long(der.size())));
    }

    std::string openssl_ext(X509* x, int nid) {
        int i = X509_get_ext_by_NID(x, nid, -1);
        if (i < 0) {
            return "";
        }
        X509_EXTENSION* e = X509_get_ext(x, i);
        BIO* b = BIO_new(BIO_s_mem());
        X509V3_EXT_print(b, e, 0, 0);
        char* data = nullptr;
        long n = BIO_get_mem_data(b, &data);
        std::string out(data, size_t(n));
        BIO_free(b);
        return out;
    }

    x509::ip_address v4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        x509::ip_address ip;
        ip.size = 4;
        ip.bytes[0] = byte(a);
        ip.bytes[1] = byte(b);
        ip.bytes[2] = byte(c);
        ip.bytes[3] = byte(d);
        return ip;
    }

    // A CA and a leaf it issued for one kind of issuer key, checked by OpenSSL
    template<class K>
    void chain_of(const K& ca_key, const x509::signing_key& leaf_signer, const vector<byte>& leaf_spki) {
        x509::certificate_template ct;
        ct.common_name = "sgcl test CA";
        ct.organization = {"SGCL", "Tests & Co"};
        ct.is_ca = true;
        ct.max_path_length = 1;
        auto ca = x509::create_certificate(ct, ca_key);
        EXPECT_TRUE(ca.is_ca());
        EXPECT_EQ(ca.max_path_length(), 1);
        EXPECT_EQ(text(ca.subject().common_name()), "sgcl test CA");
        EXPECT_EQ(ca.subject().organization().size(), 2u);
        EXPECT_TRUE(ca.allows(x509::key_usage::cert_sign));
        EXPECT_FALSE(ca.subject_key_id().empty());
        EXPECT_TRUE(ca.check_signature_from(ca).has_value());
        x509::certificate_template lt;
        lt.common_name = "example.test";
        lt.dns_names = {"example.test", "*.example.test"};
        lt.ip_addresses = {v4(192, 0, 2, 1)};
        lt.email_addresses = {"admin@example.test"};
        lt.uris = {"https://example.test/"};
        lt.ext_key_usages = {x509::ext_key_usage::server_auth, x509::ext_key_usage::client_auth};
        auto leaf = x509::create_certificate(lt, leaf_spki.as_slice(), ca, ca_key);
        EXPECT_TRUE(leaf.check_signature_from(ca).has_value());
        EXPECT_EQ(leaf.authority_key_id(), ca.subject_key_id());
        EXPECT_EQ(leaf.dns_names().size(), 2u);
        EXPECT_EQ(leaf.ip_addresses().size(), 1u);
        EXPECT_EQ(leaf.email_addresses().size(), 1u);
        EXPECT_EQ(leaf.uris().size(), 1u);
        EXPECT_FALSE(leaf.is_ca());
        EXPECT_TRUE(leaf.has_basic_constraints());
        // OpenSSL: both parse, the leaf verifies under the CA's key, the chain verifies
        auto oca = openssl_cert(ca.raw());
        auto oleaf = openssl_cert(leaf.raw());
        ASSERT_TRUE(oca && oleaf);
        KeyPtr pub = openssl_public(ca_key);
        ASSERT_TRUE(pub);
        EXPECT_EQ(X509_verify(oleaf.get(), pub.get()), 1);
        EXPECT_EQ(X509_verify(oca.get(), pub.get()), 1);
        EXPECT_EQ(X509_check_host(oleaf.get(), "a.example.test", 0, 0, nullptr), 1);
        EXPECT_EQ(X509_check_ip_asc(oleaf.get(), "192.0.2.1", 0), 1);
        EXPECT_EQ(X509_check_ca(oca.get()), 1);
        EXPECT_NE(openssl_ext(oleaf.get(), NID_ext_key_usage).find("TLS Web Server Authentication"), std::string::npos);
        EXPECT_NE(openssl_ext(oca.get(), NID_basic_constraints).find("CA:TRUE"), std::string::npos);
        EXPECT_NE(openssl_ext(oca.get(), NID_basic_constraints).find("pathlen:1"), std::string::npos);
        X509_STORE* store = X509_STORE_new();
        X509_STORE_add_cert(store, oca.get());
        X509_STORE_CTX* ctx = X509_STORE_CTX_new();
        X509_STORE_CTX_init(ctx, store, oleaf.get(), nullptr);
        EXPECT_EQ(X509_verify_cert(ctx), 1) << X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx));
        X509_STORE_CTX_free(ctx);
        X509_STORE_free(store);
        // and the module's own verification
        x509::certificate_pool pool;
        pool.add(ca);
        x509::verify_options vo;
        vo.roots = pool;
        vo.dns_name = "www.example.test";
        auto v = leaf.verify(vo);
        EXPECT_TRUE(v.has_value()) << (v ? "" : text(v.error().message()));
        (void)leaf_signer;
    }
}

TEST(X509Create, ChainsOfEveryKindOfIssuerKey) {
    Keys k;
    auto leaf_spki = k.p256.public_key().to_pkix_der();
    chain_of(k.p256, k.p256, leaf_spki);
    chain_of(k.p384, k.p256, leaf_spki);
    chain_of(k.p521, k.p256, leaf_spki);
    chain_of(k.ed25519, k.p256, leaf_spki);
    chain_of(k.rsa, k.p256, leaf_spki);
    // a leaf of each kind of subject key
    for (auto spki : {k.p384.public_key().to_pkix_der(), k.p521.public_key().to_pkix_der(), k.ed25519.public_key().to_pkix_der(),
                      k.rsa.public_key().to_pkix_der()}) {
        chain_of(k.p256, k.p256, spki);
    }
}

TEST(X509Create, TheFieldsAsWritten) {
    auto key = crypto::p256::private_key::generate();
    x509::certificate_template t;
    t.serial_number = {byte(0x01), byte(0x02), byte(0x03)};
    t.not_before = time::datetime::from_unix(1800000000, time::zone::utc());
    t.not_after = time::datetime::from_unix(4102444800 + 86400, time::zone::utc());   // 2100-01-02: GeneralizedTime
    t.key_usage = x509::key_usage::digital_signature | x509::key_usage::key_agreement;
    t.ext_key_usages = {x509::ext_key_usage::any, x509::ext_key_usage::server_auth, x509::ext_key_usage::client_auth, x509::ext_key_usage::code_signing,
                        x509::ext_key_usage::email_protection, x509::ext_key_usage::ipsec_end_system, x509::ext_key_usage::ipsec_tunnel,
                        x509::ext_key_usage::ipsec_user, x509::ext_key_usage::time_stamping, x509::ext_key_usage::ocsp_signing,
                        x509::ext_key_usage::microsoft_server_gated_crypto, x509::ext_key_usage::netscape_server_gated_crypto,
                        x509::ext_key_usage::microsoft_commercial_code_signing, x509::ext_key_usage::microsoft_kernel_code_signing};
    x509::extension custom;
    custom.oid = "1.3.6.1.4.1.99999.1";
    custom.critical = false;
    custom.value = {byte(0x05), byte(0x00)};
    t.extensions = {custom};
    auto c = x509::create_certificate(t, key);
    EXPECT_EQ(c.version(), 3);
    ASSERT_EQ(c.serial_number().size(), 3u);
    EXPECT_EQ(c.serial_number()[2], byte(0x03));
    EXPECT_EQ(c.not_before().unix(), 1800000000);
    EXPECT_EQ(c.not_after().unix(), 4102444800 + 86400);
    EXPECT_TRUE(c.allows(x509::key_usage::key_agreement));
    EXPECT_FALSE(c.allows(x509::key_usage::cert_sign));
    EXPECT_EQ(c.ext_key_usages().size(), t.ext_key_usages.size());
    for (size_t i = 0; i < t.ext_key_usages.size(); ++i) {
        EXPECT_EQ(c.ext_key_usages()[i], t.ext_key_usages[i]);
    }
    bool found = false;
    for (auto& e : c.extensions()) {
        if (text(e.oid) == "1.3.6.1.4.1.99999.1") {
            found = true;
            EXPECT_EQ(e.value.size(), 2u);
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(c.subject().empty());
    auto o = openssl_cert(c.raw());
    ASSERT_TRUE(o);
    ASN1_TIME* na = X509_get_notAfter(o.get());
    EXPECT_EQ(ASN1_STRING_type(na), V_ASN1_GENERALIZEDTIME);
    ASN1_TIME* nb = X509_get_notBefore(o.get());
    EXPECT_EQ(ASN1_STRING_type(nb), V_ASN1_UTCTIME);
    // the defaults: a random positive serial of 16 bytes, a year from now
    auto d = x509::create_certificate(x509::certificate_template{}, key);
    EXPECT_EQ(d.serial_number().size(), 16u);
    EXPECT_EQ((d.serial_number()[0] & byte(0x80)), byte(0));
    EXPECT_NEAR(double(d.not_after().unix() - d.not_before().unix()), 365.0 * 86400, 2);
    EXPECT_TRUE(d.allows(x509::key_usage::digital_signature));
    // an RSA leaf gets keyEncipherment too
    auto rsa = crypto::rsa::private_key::generate(2048);
    auto r = x509::create_certificate(x509::certificate_template{}, rsa);
    EXPECT_TRUE(r.allows(x509::key_usage::key_encipherment));
    // an extension of the template replaces the module's own of its OID
    x509::certificate_template san;
    san.dns_names = {"ours.test"};
    x509::extension theirs;
    theirs.oid = "2.5.29.17";
    theirs.value = {byte(0x30), byte(0x0b), byte(0x82), byte(0x09), byte('t'), byte('h'), byte('e'), byte('i'), byte('r'), byte('s'), byte('.'), byte('t'), byte('t')};
    san.extensions = {theirs};
    auto s = x509::create_certificate(san, key);
    ASSERT_EQ(s.dns_names().size(), 1u);
    EXPECT_EQ(text(s.dns_names()[0]), "theirs.tt");
}

TEST(X509Create, TemplatesThatCannotBeWritten) {
    auto key = crypto::p256::private_key::generate();
    auto throws = [&](const x509::certificate_template& t) {
        EXPECT_THROW((void)x509::create_certificate(t, key), std::invalid_argument);
    };
    x509::certificate_template backwards;
    backwards.not_before = time::now();
    backwards.not_after = time::now() - std::chrono::hours(1);
    throws(backwards);
    x509::certificate_template long_serial;
    long_serial.serial_number = vector<byte>(21, byte(1));
    throws(long_serial);
    x509::certificate_template negative;
    negative.serial_number = {byte(0x80)};
    throws(negative);
    x509::certificate_template idn;
    idn.dns_names = {"żółw.test"};
    throws(idn);
    x509::certificate_template empty_name;
    empty_name.dns_names = {""};
    throws(empty_name);
    x509::certificate_template bad_ip;
    bad_ip.ip_addresses = {x509::ip_address{}};
    throws(bad_ip);
    x509::certificate_template bad_oid;
    bad_oid.extensions = {x509::extension{"1", false, {}}};
    throws(bad_oid);
    x509::certificate_template bad_oid2;
    bad_oid2.extensions = {x509::extension{"3.1.2", false, {}}};
    throws(bad_oid2);
    x509::certificate_template twice;
    twice.extensions = {x509::extension{"1.2.3", false, {byte(5), byte(0)}}, x509::extension{"1.2.3", false, {byte(5), byte(0)}}};
    throws(twice);
    x509::certificate_template path;
    path.is_ca = true;
    path.max_path_length = -1;
    throws(path);
    x509::certificate_template bad_eku;
    bad_eku.ext_key_usages = {x509::ext_key_usage(0)};
    throws(bad_eku);
    x509::certificate_request_template rq;
    rq.email_addresses = {"ünicode@example.test"};
    EXPECT_THROW((void)x509::create_certificate_request(rq, key), std::invalid_argument);
}

TEST(X509Create, RequestsOfEveryKindHeldToOpenSsl) {
    Keys k;
    x509::certificate_request_template t;
    t.common_name = "example.test";
    t.organization = {"SGCL"};
    t.dns_names = {"example.test", "www.example.test"};
    t.ip_addresses = {v4(127, 0, 0, 1)};
    t.email_addresses = {"admin@example.test"};
    t.uris = {"urn:example"};
    auto check = [&](const x509::certificate_request& r) {
        EXPECT_TRUE(r.check_signature().has_value());
        EXPECT_EQ(text(r.subject().common_name()), "example.test");
        EXPECT_EQ(r.dns_names().size(), 2u);
        EXPECT_EQ(r.ip_addresses().size(), 1u);
        EXPECT_EQ(r.email_addresses().size(), 1u);
        EXPECT_EQ(r.uris().size(), 1u);
        auto o = openssl_req(r.raw());
        ASSERT_TRUE(o);
        KeyPtr pub(X509_REQ_get_pubkey(o.get()));
        EXPECT_EQ(X509_REQ_verify(o.get(), pub.get()), 1);
        STACK_OF(X509_EXTENSION)* exts = X509_REQ_get_extensions(o.get());
        ASSERT_NE(exts, nullptr);
        EXPECT_EQ(sk_X509_EXTENSION_num(exts), 1);
        sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
        // the same request again from its own bytes, and from PEM
        auto back = x509::certificate_request::parse(r.raw());
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == r);
        auto pem = encoding::pem("CERTIFICATE REQUEST", vector<byte>(r.raw().begin(), r.raw().end())).to_string();
        auto from_pem = x509::certificate_request::from_pem(string::concat("junk\n", pem));
        ASSERT_TRUE(from_pem.has_value());
        EXPECT_TRUE(*from_pem == r);
        auto old_label = encoding::pem("NEW CERTIFICATE REQUEST", vector<byte>(r.raw().begin(), r.raw().end())).to_string();
        EXPECT_TRUE(x509::certificate_request::from_pem(old_label).has_value());
    };
    check(x509::create_certificate_request(t, k.p256));
    check(x509::create_certificate_request(t, k.p384));
    check(x509::create_certificate_request(t, k.p521));
    check(x509::create_certificate_request(t, k.ed25519));
    check(x509::create_certificate_request(t, k.rsa));
    // a request with nothing but a key: no attribute
    auto bare = x509::create_certificate_request(x509::certificate_request_template{}, k.p256);
    EXPECT_TRUE(bare.check_signature().has_value());
    EXPECT_TRUE(bare.subject().empty());
    EXPECT_TRUE(bare.extensions().empty());
    auto ob = openssl_req(bare.raw());
    ASSERT_TRUE(ob);
    KeyPtr pub(X509_REQ_get_pubkey(ob.get()));
    EXPECT_EQ(X509_REQ_verify(ob.get(), pub.get()), 1);
}

TEST(X509Create, ARequestOpenSslMadeReadsHere) {
    EVP_PKEY* key = EVP_EC_gen("P-256");
    ASSERT_NE(key, nullptr);
    X509_REQ* req = X509_REQ_new();
    X509_REQ_set_version(req, 0);
    X509_NAME* name = X509_REQ_get_subject_name(req);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("openssl.test"), -1, -1, 0);
    X509_REQ_set_pubkey(req, key);
    STACK_OF(X509_EXTENSION)* exts = sk_X509_EXTENSION_new_null();
    sk_X509_EXTENSION_push(exts, X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, "DNS:openssl.test, IP:10.0.0.1"));
    X509_REQ_add_extensions(req, exts);
    sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
    ASSERT_GT(X509_REQ_sign(req, key, EVP_sha256()), 0);
    unsigned char* der = nullptr;
    int n = i2d_X509_REQ(req, &der);
    ASSERT_GT(n, 0);
    auto r = x509::certificate_request::parse(slice<const byte>(reinterpret_cast<const byte*>(der), size_t(n)));
    ASSERT_TRUE(r.has_value()) << text(r.error().message());
    EXPECT_TRUE(r->check_signature().has_value());
    EXPECT_EQ(text(r->subject().common_name()), "openssl.test");
    ASSERT_EQ(r->dns_names().size(), 1u);
    EXPECT_EQ(text(r->dns_names()[0]), "openssl.test");
    ASSERT_EQ(r->ip_addresses().size(), 1u);
    EXPECT_EQ(r->ip_addresses()[0].bytes[0], byte(10));
    EXPECT_EQ(r->public_key().kind(), x509::key_kind::p256);
    EXPECT_EQ(r->signature_algorithm(), x509::signature_algorithm::ecdsa_with_sha256);
    OPENSSL_free(der);
    X509_REQ_free(req);
    EVP_PKEY_free(key);
}

TEST(X509Create, RequestsThatAreNotOnes) {
    auto key = crypto::p256::private_key::generate();
    x509::certificate_request_template t;
    t.dns_names = {"example.test"};
    auto r = x509::create_certificate_request(t, key);
    std::vector<uint8_t> der(reinterpret_cast<const uint8_t*>(r.raw().data()), reinterpret_cast<const uint8_t*>(r.raw().data()) + r.raw().size());
    auto parse = [](const std::vector<uint8_t>& v) {
        return x509::certificate_request::parse(slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size()));
    };
    // empty, cut short, with a byte after it
    EXPECT_FALSE(parse({}).has_value());
    for (size_t cut : {size_t(1), size_t(10), der.size() / 2, der.size() - 1}) {
        auto shorter = std::vector<uint8_t>(der.begin(), der.begin() + long(cut));
        auto p = parse(shorter);
        ASSERT_FALSE(p.has_value());
        EXPECT_EQ(p.error().code(), crypto::errc::malformed);
    }
    auto longer = der;
    longer.push_back(0);
    EXPECT_FALSE(parse(longer).has_value());
    // a signature altered: parses, does not verify
    auto altered = der;
    altered[der.size() - 3] ^= 1;
    auto a = parse(altered);
    ASSERT_TRUE(a.has_value());
    auto chk = a->check_signature();
    ASSERT_FALSE(chk.has_value());
    EXPECT_EQ(chk.error().reason(), x509::reason::invalid_signature);
    // a certificate is not a request; a PEM text without one
    auto cert = x509::create_certificate(x509::certificate_template{}, key);
    EXPECT_FALSE(x509::certificate_request::parse(cert.raw()).has_value());
    EXPECT_FALSE(x509::certificate_request::from_pem("no PEM here").has_value());
    // past the size of a certificate
    std::vector<uint8_t> huge(200 * 1024, 0x30);
    EXPECT_FALSE(parse(huge).has_value());
}
