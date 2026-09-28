//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X.509, reading: every certificate Go's x509 reads compared field by field
// with what Go read (tests/crypto/data/x509/go_certs.txt, written by a Go
// program over Go's test tables, its testdata and the system's bundle: the
// names as pkix.Name.String() writes them, the times, the algorithms, the
// extensions); the system's bundle read whole; certificates made by OpenSSL
// with every extension read back and compared with OpenSSL's view of them;
// the keys of every kind; DER refused as the strict reader refuses it (BER,
// lengths, times, strings, OIDs, the bounds of §11 A7, the depth of the
// reader) and the forms that must still pass; the names' text with its
// escapes; RFC 6125 host names.
#include "x509_common.h"

#include <algorithm>
#include <set>

using namespace x509_test;
using sgcl::crypto::errc;

namespace {
    std::string s(const sgcl::string& v) {
        return std::string(v.view());
    }

    std::string join(const sgcl::vector<sgcl::string>& v, const char* sep = ",") {
        std::string r;
        for (auto& x : v) {
            if (!r.empty()) {
                r += sep;
            }
            r += s(x);
        }
        return r;
    }

    std::string sha256_hex(const bytes_t& der) {
        auto d = sgcl::crypto::sha256::of(view(der));
        return hex(to_bytes(d));
    }

    std::vector<std::string> split(const std::string& line, char sep) {
        std::vector<std::string> out;
        size_t i = 0;
        while (true) {
            size_t j = line.find(sep, i);
            out.push_back(line.substr(i, j == std::string::npos ? std::string::npos : j - i));
            if (j == std::string::npos) {
                break;
            }
            i = j + 1;
        }
        return out;
    }

    std::string unescape(const std::string& v) {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size() && (v[i + 1] == 't' || v[i + 1] == 'n')) {
                r += v[i + 1] == 't' ? '\t' : '\n';
                ++i;
            } else {
                r += v[i];
            }
        }
        return r;
    }

    // Every certificate DER of the files: the data files, Go's testdata,
    // the system's bundle
    std::vector<bytes_t> all_certificates(size_t* system_count = nullptr) {
        std::vector<std::string> files = {data_dir() + "/go_verify.txt", data_dir() + "/go_name_constraints.txt"};
        std::string go = go_root();
        if (!go.empty()) {
            for (auto name : {"policy_intermediate.pem", "policy_intermediate_any.pem", "policy_intermediate_duplicate.pem", "policy_intermediate_invalid.pem",
                              "policy_intermediate_mapped.pem", "policy_intermediate_mapped_any.pem", "policy_intermediate_mapped_oid3.pem",
                              "policy_intermediate_require.pem", "policy_intermediate_require1.pem", "policy_intermediate_require2.pem",
                              "policy_intermediate_require_duplicate.pem", "policy_intermediate_require_no_policies.pem", "policy_leaf.pem",
                              "policy_leaf_any.pem", "policy_leaf_duplicate.pem", "policy_leaf_invalid.pem", "policy_leaf_none.pem",
                              "policy_leaf_oid1.pem", "policy_leaf_oid2.pem", "policy_leaf_oid3.pem", "policy_leaf_oid4.pem", "policy_leaf_oid5.pem",
                              "policy_leaf_require.pem", "policy_leaf_require1.pem", "policy_root.pem", "policy_root2.pem",
                              "policy_root_cross_inhibit_mapping.pem"}) {
                files.push_back(go + "/src/crypto/x509/testdata/" + name);
            }
        }
        std::vector<bytes_t> out;
        for (auto& f : files) {
            for (auto& [type, der] : pem_blocks(read_file(f))) {
                if (type.size() >= 11 && type.compare(type.size() - 11, 11, "CERTIFICATE") == 0) {
                    out.push_back(der);
                }
            }
        }
        size_t before = out.size();
        for (auto& [type, der] : pem_blocks(read_file("/etc/ssl/cert.pem"))) {
            if (type == "CERTIFICATE") {
                out.push_back(der);
            }
        }
        if (system_count) {
            *system_count = out.size() - before;
        }
        return out;
    }

    int go_signature_algorithm(x509::signature_algorithm a) {
        using sa = x509::signature_algorithm;
        switch (a) {
            case sa::unknown: return 0;
            case sa::md2_with_rsa: return 1;
            case sa::md5_with_rsa: return 2;
            case sa::sha1_with_rsa: return 3;
            case sa::sha256_with_rsa: return 4;
            case sa::sha384_with_rsa: return 5;
            case sa::sha512_with_rsa: return 6;
            case sa::ecdsa_with_sha1: return 9;
            case sa::ecdsa_with_sha256: return 10;
            case sa::ecdsa_with_sha384: return 11;
            case sa::ecdsa_with_sha512: return 12;
            case sa::sha256_with_rsa_pss: return 13;
            case sa::sha384_with_rsa_pss: return 14;
            case sa::sha512_with_rsa_pss: return 15;
            case sa::ed25519: return 16;
        }
        return -1;
    }

    // datetime's range: what not_before()/not_after() give for a time
    // beyond it is the end of the range
    int64_t clamp_unix(int64_t t) {
        return sgcl::time::datetime::from_unix(t, sgcl::time::zone::utc()).unix();
    }
}

// Every certificate of Go's tables, Go's testdata and the system's bundle,
// read here and compared with what Go read from it
TEST(Crypto_X509, ParsesAsGoDoes) {
    std::map<std::string, std::vector<std::string>> go;
    std::istringstream in(read_file(data_dir() + "/go_certs.txt"));
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') {
            auto f = split(line, '\t');
            go[f[0]] = f;
        }
    }
    ASSERT_GT(go.size(), 400u);
    size_t compared = 0, refused = 0;
    std::set<std::string> done;
    for (auto& der : all_certificates()) {
        std::string key = sha256_hex(der);
        if (!done.insert(key).second) {
            continue;
        }
        auto it = go.find(key);
        if (it == go.end()) {
            continue;   // a system bundle other than the one the data was made from
        }
        auto& f = it->second;
        auto c = x509::certificate::parse(view(der));
        if (f[1] == "error") {
            // Go refuses a negative serial number since 1.23; the rest it refuses, so does this parser
            if (c && c->serial_number().size() > 0 && (unsigned(c->serial_number()[0]) & 0x80) != 0) {
                continue;
            }
            EXPECT_FALSE(c) << key;
            ++refused;
            continue;
        }
        ASSERT_TRUE(c) << key << ": " << s(c.error().message());
        SCOPED_TRACE(f[2]);
        EXPECT_EQ(s(c->subject().to_string()), unescape(f[2]));
        EXPECT_EQ(s(c->issuer().to_string()), unescape(f[3]));
        EXPECT_EQ(c->version(), std::stoi(f[4]));
        EXPECT_EQ(c->not_before().unix(), clamp_unix(std::stoll(f[5])));
        EXPECT_EQ(c->not_after().unix(), clamp_unix(std::stoll(f[6])));
        int go_sig = std::stoi(f[7]);
        EXPECT_EQ(go_signature_algorithm(c->signature_algorithm()), go_sig == 7 || go_sig == 8 ? 0 : go_sig);
        switch (std::stoi(f[8])) {
            case 1: EXPECT_TRUE(c->public_key().kind() == x509::key_kind::rsa || !c->public_key().has_value()); break;
            case 3: EXPECT_TRUE(c->public_key().kind() == x509::key_kind::p256 || c->public_key().kind() == x509::key_kind::p384 || !c->public_key().has_value()); break;
            case 4: EXPECT_EQ(c->public_key().kind(), x509::key_kind::ed25519); break;
            default: EXPECT_FALSE(c->public_key().has_value()); break;
        }
        EXPECT_EQ(c->has_basic_constraints(), f[9] == "true");
        EXPECT_EQ(c->is_ca(), f[10] == "true");
        if (c->has_basic_constraints()) {
            int go_len = std::stoi(f[11]);
            if (go_len < 0) {
                EXPECT_FALSE(c->max_path_length());
            } else {
                ASSERT_TRUE(c->max_path_length());
                EXPECT_EQ(*c->max_path_length(), go_len);
            }
        }
        EXPECT_EQ(int(c->key_usage()), std::stoi(f[12]));
        std::string ekus;
        for (auto u : c->ext_key_usages()) {
            ekus += (ekus.empty() ? "" : ",") + std::to_string(int(u) - 1);
        }
        EXPECT_EQ(ekus, f[13]);
        EXPECT_EQ(c->unknown_ext_key_usages().size(), std::stoul(f[14]));
        EXPECT_EQ(join(c->dns_names()), f[15]);
        EXPECT_EQ(join(c->email_addresses()), f[16]);
        std::string ips;
        for (auto& ip : c->ip_addresses()) {
            ips += (ips.empty() ? "" : ",") + hex(bytes_t(reinterpret_cast<const unsigned char*>(ip.bytes.data()), reinterpret_cast<const unsigned char*>(ip.bytes.data()) + ip.size));
        }
        EXPECT_EQ(ips, f[17]);
        EXPECT_EQ(join(c->uris(), " "), f[18]);
        EXPECT_EQ(hex(to_bytes(c->subject_key_id())), f[19]);
        EXPECT_EQ(hex(to_bytes(c->authority_key_id())), f[20]);
        EXPECT_EQ(join(c->policies()), f[21]);
        // the policy extensions Go validates and this module does not are unhandled here where critical
        size_t policy_critical = 0;
        for (auto& e : c->extensions()) {
            if (e.critical && (e.oid == "2.5.29.33" || e.oid == "2.5.29.36" || e.oid == "2.5.29.54")) {
                ++policy_critical;
            }
        }
        EXPECT_EQ(c->unhandled_critical_extensions().size(), std::stoul(f[22]) + policy_critical);
        EXPECT_EQ(join(c->permitted_dns_domains()), f[23]);
        EXPECT_EQ(join(c->excluded_dns_domains()), f[24]);
        EXPECT_EQ(s(c->subject().common_name()), unescape(f[25]));
        bytes_t serial = to_bytes(c->serial_number());
        while (serial.size() > 1 && serial[0] == 0) {
            serial.erase(serial.begin());
        }
        EXPECT_EQ(serial == bytes_t{0} ? std::string() : hex(serial), f[26]);
        ++compared;
    }
    std::printf("  %zu certificates compared with Go, %zu refused by both\n", compared, refused);
    EXPECT_GT(compared, 300u);
}

// Every certificate of the system's bundle reads, and the pool holds them all
TEST(Crypto_X509, TheSystemBundleReadsWhole) {
    std::set<std::string> distinct;
    size_t n = 0;
    for (auto& [type, der] : pem_blocks(read_file("/etc/ssl/cert.pem"))) {
        if (type != "CERTIFICATE") {
            continue;
        }
        auto c = x509::certificate::parse(view(der));
        EXPECT_TRUE(c) << (c ? "" : s(c.error().message()));
        if (c) {
            EXPECT_TRUE(c->unhandled_critical_extensions().empty());
            EXPECT_TRUE(c->public_key().has_value()) << s(c->subject().to_string());
        }
        distinct.insert(sha256_hex(der));
        ++n;
    }
    if (n == 0) {
        GTEST_SKIP() << "no /etc/ssl/cert.pem";
    }
    auto pool = x509::certificate_pool::system();
    ASSERT_TRUE(pool);
    EXPECT_EQ(pool->size(), distinct.size());
    // a clone each call: a change to one is not seen by the next
    pool->add(parse_or_fail(make_cert(root_spec(key_type::p256)).der));
    auto again = x509::certificate_pool::system();
    ASSERT_TRUE(again);
    EXPECT_EQ(again->size(), distinct.size());
    // the same from a task, the files read on the blocking pool
    auto from_task = sgcl::async::spawn(x509::certificate_pool::async_system()).wait();
    ASSERT_TRUE(from_task);
    EXPECT_EQ(from_task->size(), distinct.size());
    std::printf("  %zu certificates in /etc/ssl/cert.pem\n", n);
}

// A certificate with every extension read here, made by OpenSSL, read back
TEST(Crypto_X509, TheFieldsOfACertificateOpenSslMade) {
    auto root = make_cert(root_spec(key_type::p384));
    spec ls = leaf_spec(key_type::rsa2048, "DNS:www.example.com,DNS:*.example.org,email:someone@example.com,IP:192.0.2.7,IP:2001:db8::1,URI:https://example.com/path");
    ls.cn = "www.example.com";
    ls.o = "Example, Inc.";
    ls.serial = 0x123456789;
    ls.ext_key_usage = "serverAuth,clientAuth,1.2.3.4.5";
    ls.policies = {"2.23.140.1.2.1", "1.3.6.1.4.1.44947.1.1.1"};
    ls.not_before = 1700000000;
    ls.not_after = 1800000000;
    ls.algorithm = sig::sha384;
    auto leaf = make_cert(ls, &root);
    auto c = parse_or_fail(leaf.der);
    EXPECT_EQ(c.version(), 3);
    EXPECT_EQ(hex(to_bytes(c.serial_number())), "0123456789");
    EXPECT_EQ(s(c.subject().to_string()), "CN=www.example.com,O=Example\\, Inc.");
    EXPECT_EQ(s(c.subject().common_name()), "www.example.com");
    ASSERT_EQ(c.subject().organization().size(), 1u);
    EXPECT_EQ(s(c.subject().organization()[0]), "Example, Inc.");
    EXPECT_EQ(s(c.issuer().to_string()), "CN=Test Root,O=SGCL Tests");
    EXPECT_EQ(c.not_before().unix(), 1700000000);
    EXPECT_EQ(c.not_after().unix(), 1800000000);
    EXPECT_EQ(c.signature_algorithm(), x509::signature_algorithm::ecdsa_with_sha384);
    EXPECT_EQ(s(c.signature_algorithm_oid()), "1.2.840.10045.4.3.3");
    ASSERT_EQ(c.public_key().kind(), x509::key_kind::rsa);
    EXPECT_EQ(c.public_key().rsa().bits(), 2048u);
    EXPECT_EQ(s(c.public_key().algorithm()), "1.2.840.113549.1.1.1");
    EXPECT_THROW((void)c.public_key().p256(), std::logic_error);
    EXPECT_TRUE(c.has_basic_constraints());
    EXPECT_FALSE(c.is_ca());
    EXPECT_FALSE(c.max_path_length());
    EXPECT_TRUE(c.has_key_usage());
    EXPECT_EQ(c.key_usage(), x509::key_usage::digital_signature);
    EXPECT_TRUE(c.allows(x509::key_usage::digital_signature));
    EXPECT_FALSE(c.allows(x509::key_usage::cert_sign));
    ASSERT_EQ(c.ext_key_usages().size(), 2u);
    EXPECT_EQ(c.ext_key_usages()[0], x509::ext_key_usage::server_auth);
    EXPECT_EQ(c.ext_key_usages()[1], x509::ext_key_usage::client_auth);
    ASSERT_EQ(c.unknown_ext_key_usages().size(), 1u);
    EXPECT_EQ(s(c.unknown_ext_key_usages()[0]), "1.2.3.4.5");
    EXPECT_EQ(join(c.dns_names()), "www.example.com,*.example.org");
    EXPECT_EQ(join(c.email_addresses()), "someone@example.com");
    EXPECT_EQ(join(c.uris()), "https://example.com/path");
    ASSERT_EQ(c.ip_addresses().size(), 2u);
    EXPECT_EQ(c.ip_addresses()[0].size, 4);
    EXPECT_EQ(unsigned(c.ip_addresses()[0].bytes[3]), 7u);
    EXPECT_EQ(c.ip_addresses()[1].size, 16);
    EXPECT_EQ(unsigned(c.ip_addresses()[1].bytes[0]), 0x20u);
    EXPECT_EQ(join(c.policies()), "2.23.140.1.2.1,1.3.6.1.4.1.44947.1.1.1");
    EXPECT_TRUE(c.unhandled_critical_extensions().empty());
    // the key ids are OpenSSL's
    auto r = parse_or_fail(root.der);
    EXPECT_FALSE(r.subject_key_id().empty());
    EXPECT_TRUE(to_bytes(c.authority_key_id()) == to_bytes(r.subject_key_id()));
    const ASN1_OCTET_STRING* skid = X509_get0_subject_key_id(leaf.cert.get());
    ASSERT_NE(skid, nullptr);
    EXPECT_EQ(hex(to_bytes(c.subject_key_id())), hex(bytes_t(skid->data, skid->data + skid->length)));
    // the raw parts are the bytes OpenSSL writes for them
    auto i2d = [](auto f, auto* x) {
        unsigned char* p = nullptr;
        int n = f(x, &p);
        bytes_t v(p, p + n);
        OPENSSL_free(p);
        return v;
    };
    EXPECT_EQ(to_bytes(c.raw()), leaf.der);
    EXPECT_EQ(to_bytes(c.raw_subject()), i2d(i2d_X509_NAME, X509_get_subject_name(leaf.cert.get())));
    EXPECT_EQ(to_bytes(c.raw_issuer()), i2d(i2d_X509_NAME, X509_get_issuer_name(leaf.cert.get())));
    EXPECT_EQ(to_bytes(c.raw_subject_public_key_info()), i2d(i2d_PUBKEY, X509_get0_pubkey(leaf.cert.get())));
    EXPECT_EQ(to_bytes(c.raw_tbs()), i2d(i2d_re_X509_tbs, leaf.cert.get()));
    EXPECT_EQ(to_bytes(c.public_key().rsa().to_pkix_der()), to_bytes(c.raw_subject_public_key_info()));
    // the signature is the one OpenSSL made, and the root's key checks it
    const ASN1_BIT_STRING* sig = nullptr;
    X509_get0_signature(&sig, nullptr, leaf.cert.get());
    EXPECT_EQ(to_bytes(c.signature()), bytes_t(sig->data, sig->data + sig->length));
    EXPECT_TRUE(c.check_signature_from(r));
    auto self = c.check_signature_from(c);
    ASSERT_FALSE(self);
    EXPECT_EQ(self.error().reason(), reason::not_a_ca);
    // a copy is the same certificate, the same bytes
    x509::certificate copy = c;
    EXPECT_TRUE(copy == c);
    EXPECT_FALSE(copy == r);
}

// The keys: each kind the module has, and those it has not (the certificate
// still read, its key none)
TEST(Crypto_X509, KeysOfEveryKind) {
    for (auto k : {key_type::rsa2048, key_type::rsa1024, key_type::p256, key_type::p384, key_type::ed25519}) {
        SCOPED_TRACE(key_name(k));
        auto m = make_cert(root_spec(k));
        auto c = parse_or_fail(m.der);
        switch (k) {
            case key_type::rsa2048: case key_type::rsa1024:
                ASSERT_EQ(c.public_key().kind(), x509::key_kind::rsa);
                EXPECT_EQ(to_bytes(c.public_key().rsa().to_pkix_der()), to_bytes(c.raw_subject_public_key_info()));
                break;
            case key_type::p256:
                ASSERT_EQ(c.public_key().kind(), x509::key_kind::p256);
                EXPECT_EQ(to_bytes(c.public_key().p256().to_pkix_der()), to_bytes(c.raw_subject_public_key_info()));
                break;
            case key_type::p384:
                ASSERT_EQ(c.public_key().kind(), x509::key_kind::p384);
                EXPECT_EQ(to_bytes(c.public_key().p384().to_pkix_der()), to_bytes(c.raw_subject_public_key_info()));
                break;
            case key_type::ed25519:
                ASSERT_EQ(c.public_key().kind(), x509::key_kind::ed25519);
                EXPECT_EQ(to_bytes(c.public_key().ed25519().to_pkix_der()), to_bytes(c.raw_subject_public_key_info()));
                break;
        }
        EXPECT_TRUE(c.check_signature_from(c));   // self-signed, a CA
    }
    // P-521 and X25519: read, the key none, its algorithm named
    struct other {
        const char* type;
        const char* curve;
        const char* oid;
    };
    for (auto o : {other{"EC", "P-521", "1.2.840.10045.2.1"}, other{"X25519", nullptr, "1.3.101.110"}, other{"RSA", nullptr, "1.2.840.113549.1.1.1"}}) {
        SCOPED_TRACE(o.type);
        pkey_ptr key(o.curve ? EVP_PKEY_Q_keygen(nullptr, nullptr, o.type, o.curve) : std::string(o.type) == "RSA" ? EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(768))
                                                                                                                       : EVP_PKEY_Q_keygen(nullptr, nullptr, o.type), EVP_PKEY_free);
        auto root = make_cert(root_spec(key_type::p256));
        spec ls = leaf_spec(key_type::p256);
        ls.use_key = key;
        ls.key_usage = "critical,keyAgreement";
        auto m = make_cert(ls, &root);
        auto c = parse_or_fail(m.der);
        EXPECT_FALSE(c.public_key().has_value());
        EXPECT_EQ(s(c.public_key().algorithm()), o.oid);
        // what it signs cannot be verified: its key is none
        if (std::string(o.type) == "EC") {
            spec child = leaf_spec(key_type::p256);
            spec ca = root_spec(key_type::p256, "P-521 CA");
            ca.use_key = key;
            auto ca_cert = make_cert(ca, &root);
            auto leaf = make_cert(child, &ca_cert);
            auto r = parse_or_fail(leaf.der).check_signature_from(parse_or_fail(ca_cert.der));
            ASSERT_FALSE(r);
            EXPECT_EQ(r.error().reason(), reason::unsupported_algorithm);
        }
    }
}

// PEM: the first CERTIFICATE block, whatever is around it
TEST(Crypto_X509, FromPem) {
    auto m = make_cert(root_spec(key_type::p256));
    BIO* b = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(b, m.cert.get());
    char* p;
    long n = BIO_get_mem_data(b, &p);
    std::string pem(p, size_t(n));
    BIO_free(b);
    auto c = x509::certificate::from_pem(string("some text before\n-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n" + pem + "after"));
    ASSERT_TRUE(c) << s(c.error().message());
    EXPECT_EQ(to_bytes(c->raw()), m.der);
    EXPECT_FALSE(x509::certificate::from_pem(string("no block here")));
    auto broken = x509::certificate::from_pem(string("-----BEGIN CERTIFICATE-----\n!!!\n-----END CERTIFICATE-----\n"));
    ASSERT_FALSE(broken);
    EXPECT_EQ(broken.error().code(), errc::malformed);
    // a block that cannot be read, before or after the certificate, is passed over; so is a
    // BEGIN with no END of its own, and the block after it is read from its own BEGIN
    for (const std::string& text : {pem + "-----BEGIN X-----\n!!!\n-----END X-----\n",
                                    "-----BEGIN X-----\n!!!\n-----END X-----\n" + pem,
                                    "-----BEGIN CERTIFICATE-----\nAAAA\n" + pem}) {
        auto around = x509::certificate::from_pem(string(text));
        ASSERT_TRUE(around) << s(around.error().message());
        EXPECT_EQ(to_bytes(around->raw()), m.der);
    }
    EXPECT_EQ(x509::certificate_pool::from_pem(string("-----BEGIN CERTIFICATE-----\nAAAA\n" + pem)).size(), 1u);
    EXPECT_EQ(x509::certificate_pool::from_pem(string("-----BEGIN CERTIFICATE-----\nAAAA\n-----BEGIN X-----\n" + pem)).size(), 1u);
    // a pool from PEM passes over what does not parse and what is not a certificate
    auto second = make_cert(root_spec(key_type::ed25519, "Second"));
    BIO* b2 = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(b2, second.cert.get());
    n = BIO_get_mem_data(b2, &p);
    std::string pem2(p, size_t(n));
    BIO_free(b2);
    auto pool = x509::certificate_pool::from_pem(string(pem + "-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----\n-----BEGIN X-----\nAAAA\n-----END X-----\n" + pem2 + pem));
    EXPECT_EQ(pool.size(), 2u);
    EXPECT_TRUE(pool.contains(parse_or_fail(second.der)));
    // a pool is a handle: copies share, clone() does not
    auto copy = pool;
    auto own = pool.clone();
    copy.add(parse_or_fail(make_cert(root_spec(key_type::p256, "Third")).der));
    EXPECT_EQ(pool.size(), 3u);
    EXPECT_EQ(own.size(), 2u);
}

// A pool from a PEM file, on this thread and from a task: the blocks as
// append_pem reads them, an error of the file system as io's error
TEST(Crypto_X509, PoolFromFile) {
    auto pem_of = [](const made& m) {
        BIO* b = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(b, m.cert.get());
        char* p;
        long n = BIO_get_mem_data(b, &p);
        std::string text(p, size_t(n));
        BIO_free(b);
        return text;
    };
    auto first = make_cert(root_spec(key_type::p256, "First"));
    auto second = make_cert(root_spec(key_type::ed25519, "Second"));
    std::string path = std::string(sgcl::io::temp_dir().view()) + "/sgcl_x509_pool_" + std::to_string(::getpid()) + ".pem";
    auto write = [&](const std::string& text) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << text;
    };
    write(pem_of(first) + pem_of(second));
    auto pool = x509::certificate_pool::from_file(string(path));
    ASSERT_TRUE(pool) << s(pool.error().message());
    EXPECT_EQ(pool->size(), 2u);
    EXPECT_TRUE(pool->contains(parse_or_fail(second.der)));
    // a block that does not read is passed over
    write("-----BEGIN CERTIFICATE-----\nAAAA\n" + pem_of(first) + "-----BEGIN CERTIFICATE-----\n!!!\n-----END CERTIFICATE-----\n");
    pool = x509::certificate_pool::from_file(string(path));
    ASSERT_TRUE(pool);
    EXPECT_EQ(pool->size(), 1u);
    EXPECT_TRUE(pool->contains(parse_or_fail(first.der)));
    auto from_task = sgcl::async::spawn(x509::certificate_pool::async_from_file(string(path))).wait();
    ASSERT_TRUE(from_task);
    EXPECT_EQ(from_task->size(), 1u);
    // no file
    std::remove(path.c_str());
    auto none = x509::certificate_pool::from_file(string(path));
    ASSERT_FALSE(none);
    EXPECT_TRUE(none.error().is_not_found());
    auto none_from_task = sgcl::async::spawn(x509::certificate_pool::async_from_file(string(path))).wait();
    ASSERT_FALSE(none_from_task);
    EXPECT_TRUE(none_from_task.error().is_not_found());
}

namespace {
    x509::certificate parse_hand(const hand& h) {
        return parse_or_fail(h.build());
    }

    void expect_malformed(const bytes_t& der, const char* what) {
        auto c = x509::certificate::parse(view(der));
        EXPECT_FALSE(c) << what;
        if (!c) {
            EXPECT_EQ(c.error().code(), errc::malformed) << what;
        }
    }
}

// The hand-made certificate reads, and each change below breaks it
TEST(Crypto_X509, DerIsStrict) {
    hand h;
    auto good = parse_hand(h);
    EXPECT_EQ(s(good.subject().common_name()), "Hand Leaf");
    bytes_t base = h.build();

    // the encodings of lengths
    {
        bytes_t v = base;
        v[1] = 0x80;   // indefinite length
        expect_malformed(v, "indefinite length");
        // the length in one byte more than it needs: a leading zero byte
        size_t k = base[1] & 0x7f;
        ASSERT_TRUE(base[1] & 0x80);
        bytes_t longer = {0x30, (unsigned char)(0x80 | (k + 1)), 0x00};
        longer.insert(longer.end(), base.begin() + 2, base.end());
        expect_malformed(longer, "a length with a leading zero byte");
        // a length of 1 to 127 written in the long form
        bytes_t small = {0x30, 0x81, 0x05, 0x02, 0x01, 0x01, 0x05, 0x00};
        expect_malformed(small, "a short length in the long form");
        bytes_t trailing = base;
        trailing.push_back(0);
        expect_malformed(trailing, "data after the certificate");
        expect_malformed(bytes_t(base.begin(), base.end() - 1), "cut short");
        expect_malformed({}, "empty");
        expect_malformed({0x30, 0x00}, "an empty SEQUENCE");
    }
    // the TBSCertificate's fields
    {
        hand x = h;
        x.version = der(0xa0, der(0x02, {0x03}));
        expect_malformed(x.build(), "version 4");
        x = h;
        x.version = der(0xa0, der(0x02, {0x00, 0x02}));
        expect_malformed(x.build(), "a padded version");
        x = h;
        x.version = {};
        x.set_extensions({extension("2.5.29.19", true, seq({der(0x01, {0xff})}))});
        expect_malformed(x.build(), "extensions in a version 1 certificate");
        x = h;
        x.serial = der(0x02, {0x00, 0x01});
        expect_malformed(x.build(), "a padded serial");
        x = h;
        x.serial = der(0x02, {});
        expect_malformed(x.build(), "an empty serial");
        x = h;
        x.serial = {0x02, 0x81, 0x01, 0x01};
        expect_malformed(x.build(), "a length of one written in the long form");
        x = h;
        x.trailing_tbs = der(0x05, {});
        expect_malformed(x.build(), "data after the TBSCertificate's fields");
        x = h;
        bytes_t outer = seq({oid("1.2.840.10045.4.3.3")});
        auto tbs = x.tbs();
        expect_malformed(seq({tbs, outer, x.signature}), "inner and outer algorithms differ");
        x = h;
        x.signature = der(0x03, {0x08, 0x00});
        expect_malformed(x.build(), "a BIT STRING with 8 unused bits");
        x = h;
        x.signature = der(0x03, {0x01, 0x01});
        expect_malformed(x.build(), "a BIT STRING whose unused bit is set");
        x = h;
        x.issuer = der(0x30, der(0x31, {}));
        expect_malformed(x.build(), "an empty RDN");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x13, text("a@b"))}});
        expect_malformed(x.build(), "'@' in a PrintableString");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x0c, {0xc0, 0x80})}});
        expect_malformed(x.build(), "an overlong UTF-8 sequence");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x1e, {0x00, 0x41, 0x00})}});
        expect_malformed(x.build(), "a BMPString of an odd length");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x1e, {0xd8, 0x00})}});
        expect_malformed(x.build(), "a surrogate in a BMPString");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x16, {0x80})}});
        expect_malformed(x.build(), "a byte above 7F in an IA5String");
        x = h;
        x.issuer = der(0x30, der(0x31, seq({der(0x06, {0x55, 0x80, 0x03}), utf8_der("x")})));
        expect_malformed(x.build(), "an OID arc with a leading 80");
        x = h;
        x.issuer = der(0x30, der(0x31, seq({oid("2.5.4.3"), utf8_der("x"), utf8_der("y")})));
        expect_malformed(x.build(), "an attribute of three elements");
        x = h;
        x.issuer = der(0x30, der(0x31, seq({oid("2.5.4.3"), bytes_t{0x1f, 0x81, 0x00, 0x00}})));
        expect_malformed(x.build(), "a high-tag-number form");
        // what the fuzzer found OpenSSL refuses and this parser took: each
        // is not DER, or not a value a name may hold
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x00, text("abc"))}});
        expect_malformed(x.build(), "the universal tag 0 (end-of-contents)");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x33, der(0x13, text("abc")))}});
        expect_malformed(x.build(), "a PrintableString in the constructed form");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x80, text("abc"))}});
        expect_malformed(x.build(), "a context-specific value in a name");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x02, {0x05})}});
        expect_malformed(x.build(), "an INTEGER value in a name");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x1c, {0x00, 0x11, 0x00, 0x00})}});
        expect_malformed(x.build(), "a UniversalString above U+10FFFF");
        x = h;
        x.issuer = name_of({{"2.5.4.3", der(0x1c, {0x00, 0x00, 0x00, 0x41})}});
        EXPECT_EQ(s(parse_hand(x).issuer().to_string()), "") << "a valid UniversalString is kept as a value, as Go keeps it";
        x = h;
        x.spki = der(0x30, cat({seq({oid("1.2.3.4"), der(0x17, text("garbage"))}), der(0x03, {0x00, 0x01})}));
        expect_malformed(x.build(), "a time of another form in an algorithm's parameters");
        x = h;
        x.spki = der(0x30, cat({der(0x10, cat({oid("1.2.840.10045.2.1"), oid("1.2.840.10045.3.1.7")})), der(0x03, {0x00, 0x01})}));
        expect_malformed(x.build(), "a SEQUENCE in the primitive form");
        x = h;
        x.algorithm = seq({oid("1.2.3.4"), der(0x05, {}), der(0x05, {})});
        expect_malformed(x.build(), "two parameters of the signature algorithm");
        x = h;
        x.spki = der(0x30, cat({seq({oid("1.2.840.10045.2.1"), der(0x06, {0x2a, 0x80, 0x48})}), der(0x03, {0x00, 0x01})}));
        expect_malformed(x.build(), "an OID with a leading 80 in an algorithm's parameters");
        x = h;
        x.trailing_tbs = der(0xa1, der(0x03, {0x00}));
        x.extensions.clear();
        expect_malformed(x.build(), "an issuerUniqueID in the constructed form");
        x = h;
        x.extensions.clear();
        x.trailing_tbs = der(0x81, {0x00, 0xff});
        EXPECT_TRUE(x509::certificate::parse(view(x.build()))) << "an issuerUniqueID";
        // an algorithm of an unknown OID: the certificate reads, the key none
        x = h;
        x.spki = der(0x30, cat({seq({oid("1.2.3.4"), der(0x05, {})}), der(0x03, {0x00, 0x01, 0x02})}));
        auto unknown = x509::certificate::parse(view(x.build()));
        ASSERT_TRUE(unknown);
        EXPECT_FALSE(unknown->public_key().has_value());
        EXPECT_EQ(s(unknown->public_key().algorithm()), "1.2.3.4");
    }
    // the times: exactly YYMMDDHHMMSSZ and YYYYMMDDHHMMSSZ, every field in range
    for (const char* bad : {"250101000000", "2501010000Z", "250101000000+0100", "251301000000Z", "250132000000Z", "250229000000Z", "250101240000Z",
                            "250101006000Z", "250101000060Z", "25010100000aZ"}) {
        hand x = h;
        x.validity = seq({der(0x17, text(bad)), der(0x17, text("300101000000Z"))});
        expect_malformed(x.build(), bad);
    }
    for (const char* bad : {"20250101000000.5Z", "20250101000000", "202501010000Z", "20250101000000+0000", "20251301000000Z"}) {
        hand x = h;
        x.validity = seq({der(0x18, text(bad)), der(0x17, text("300101000000Z"))});
        expect_malformed(x.build(), bad);
    }
    {
        hand x = h;
        x.validity = seq({der(0x17, text("240229120000Z")), der(0x18, text("99991231235959Z"))});
        auto c = parse_hand(x);
        EXPECT_EQ(c.not_before().unix(), 1709208000);
        EXPECT_EQ(c.not_after(), sgcl::time::datetime::from_unix(253402300799, sgcl::time::zone::utc()));   // datetime's end
        x.validity = seq({der(0x17, text("500101000000Z")), der(0x17, text("491231235959Z"))});
        c = parse_hand(x);
        EXPECT_EQ(c.not_before().unix(), -631152000);   // 1950
        EXPECT_EQ(c.not_after().unix(), 2524607999);    // 2049
        x.validity = seq({der(0x18, text("19000101000000Z")), der(0x18, text("21000301000000Z"))});
        c = parse_hand(x);
        EXPECT_EQ(c.not_after().unix(), 4107542400);
    }
    // extensions
    {
        auto bc = extension("2.5.29.19", true, seq({der(0x01, {0xff})}));
        hand x = h;
        x.set_extensions({bc, bc});
        expect_malformed(x.build(), "an extension given twice");
        x.set_extensions({});
        auto c = x509::certificate::parse(view(x.build()));
        EXPECT_TRUE(c) << "an empty list of extensions, as Go takes it";
        x.set_extensions({seq({oid("2.5.29.19"), der(0x01, {0x01}), der(0x04, seq({}))})});
        expect_malformed(x.build(), "a BOOLEAN of 01");
        x.set_extensions({seq({oid("2.5.29.19"), der(0x04, seq({})), der(0x05, {})})});
        expect_malformed(x.build(), "an extension of four elements");
        x.set_extensions({extension("2.5.29.35", true, seq({der(0x80, {1, 2, 3})}))});
        expect_malformed(x.build(), "an authority key id marked critical");
        x.set_extensions({extension("2.5.29.14", true, der(0x04, {1, 2, 3}))});
        expect_malformed(x.build(), "a subject key id marked critical");
        x.set_extensions({extension("1.3.6.1.5.5.7.1.1", true, seq({}))});
        expect_malformed(x.build(), "authority information access marked critical");
        x.set_extensions({extension("2.5.29.17", false, seq({der(0x87, {1, 2, 3, 4, 5})}))});
        expect_malformed(x.build(), "an IP address of 5 bytes");
        // an IPv4-mapped IPv6 address is read as its IPv4 address, as Go and OpenSSL read it
        x.set_extensions({extension("2.5.29.17", false, seq({der(0x87, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 2, 3, 4})}))});
        {
            auto mapped = x509::certificate::parse(view(x.build()));
            ASSERT_TRUE(mapped) << "an IPv4-mapped IPv6 address: " << s(mapped.error().message());
            ASSERT_EQ(mapped->ip_addresses().size(), 1u);
            EXPECT_EQ(mapped->ip_addresses()[0].size, 4u);
            EXPECT_TRUE(mapped->verify_ip(view(bytes_t{1, 2, 3, 4})));
            EXPECT_TRUE(mapped->verify_ip(view(bytes_t{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 2, 3, 4})));
        }
        x.set_extensions({extension("2.5.29.17", false, seq({der(0x82, {'a', 0xc3, 0xa9})}))});
        expect_malformed(x.build(), "a dNSName that is not IA5");
        x.set_extensions({extension("2.5.29.30", true, seq({}))});
        expect_malformed(x.build(), "empty name constraints");
        x.set_extensions({extension("2.5.29.30", true, seq({der(0xa0, seq({der(0x87, {10, 0, 0, 0, 255, 0, 255, 0})}))}))});
        expect_malformed(x.build(), "an IP constraint whose mask is not ones then zeros");
        x.set_extensions({extension("2.5.29.30", true, seq({der(0xa0, seq({der(0x82, text("example.com."))}))}))});
        expect_malformed(x.build(), "a DNS constraint with a trailing dot");
        x.set_extensions({extension("2.5.29.30", true, seq({der(0xa0, seq({der(0x86, text("1.2.3.4"))}))}))});
        expect_malformed(x.build(), "a URI constraint that is an IP address");
        x.set_extensions({extension("2.5.29.30", true, seq({der(0xa0, seq({der(0x86, text("ffff::1"))}))}))});
        expect_malformed(x.build(), "a URI constraint that is an IPv6 address");
        x.set_extensions({extension("2.5.29.30", true, seq({der(0xa0, seq({der(0x81, text("abc@foo.com."))}))}))});
        expect_malformed(x.build(), "an email constraint with a trailing dot");
        x.set_extensions({extension("2.5.29.32", false, seq({seq({oid("1.2.3")}), seq({oid("1.2.3")})}))});
        expect_malformed(x.build(), "a policy given twice");
        x.set_extensions({extension("2.5.29.15", true, der(0x03, {0x07, 0x80}))});
        auto ku = x509::certificate::parse(view(x.build()));
        ASSERT_TRUE(ku);
        EXPECT_EQ(ku->key_usage(), x509::key_usage::digital_signature);
        x.set_extensions({extension("2.5.29.17", false, seq({der(0x86, text("https://exa mple.com/"))}))});
        expect_malformed(x.build(), "a URI whose host is not a domain");
        // a SAN of only an otherName: read, and unhandled where it is critical (Go)
        x.set_extensions({extension("2.5.29.17", true, seq({der(0xa0, cat({oid("1.2.3"), der(0xa0, utf8_der("x"))}))}))});
        auto other = x509::certificate::parse(view(x.build()));
        ASSERT_TRUE(other);
        EXPECT_EQ(other->unhandled_critical_extensions().size(), 1u);
        // policy constraints: read, not enforced, so unhandled where critical
        x.set_extensions({extension("2.5.29.36", true, seq({der(0x80, {0x00})}))});
        auto pc = x509::certificate::parse(view(x.build()));
        ASSERT_TRUE(pc);
        ASSERT_EQ(pc->unhandled_critical_extensions().size(), 1u);
        EXPECT_EQ(s(pc->unhandled_critical_extensions()[0]), "2.5.29.36");
        // an unknown extension: read; unhandled only where critical
        x.set_extensions({extension("1.2.3.4", false, {0x05, 0x00}), extension("1.2.3.5", true, {0x05, 0x00})});
        auto u = x509::certificate::parse(view(x.build()));
        ASSERT_TRUE(u);
        ASSERT_EQ(u->unhandled_critical_extensions().size(), 1u);
        EXPECT_EQ(s(u->unhandled_critical_extensions()[0]), "1.2.3.5");
        ASSERT_EQ(u->extensions().size(), 2u);
        EXPECT_FALSE(u->extensions()[0].critical);
        EXPECT_EQ(to_bytes(u->extensions()[1].value), (bytes_t{0x05, 0x00}));
    }
}

// The bounds of §11 A7: at them the certificate reads, past them it does not
TEST(Crypto_X509, TheBoundsOfACertificate) {
    hand h;
    auto exts = [](size_t n) {
        std::vector<bytes_t> v;
        for (size_t i = 0; i < n; ++i) {
            v.push_back(extension("1.2.3." + std::to_string(i + 1), false, {0x05, 0x00}));
        }
        return v;
    };
    h.set_extensions(exts(64));
    EXPECT_TRUE(x509::certificate::parse(view(h.build())));
    h.set_extensions(exts(65));
    expect_malformed(h.build(), "65 extensions");

    auto san = [](size_t n) {
        bytes_t names;
        for (size_t i = 0; i < n; ++i) {
            auto d = der(0x82, text("h" + std::to_string(i) + ".example.com"));
            names.insert(names.end(), d.begin(), d.end());
        }
        return extension("2.5.29.17", false, der(0x30, names));
    };
    h.set_extensions({san(1024)});
    auto big = x509::certificate::parse(view(h.build()));
    ASSERT_TRUE(big);
    EXPECT_EQ(big->dns_names().size(), 1024u);
    h.set_extensions({san(1025)});
    expect_malformed(h.build(), "1025 names");

    auto nc = [](size_t n) {
        bytes_t subtrees;
        for (size_t i = 0; i < n; ++i) {
            auto d = seq({der(0x82, text("d" + std::to_string(i) + ".example.com"))});
            subtrees.insert(subtrees.end(), d.begin(), d.end());
        }
        return extension("2.5.29.30", true, seq({der(0xa0, subtrees)}));
    };
    h.set_extensions({nc(256)});
    EXPECT_TRUE(x509::certificate::parse(view(h.build())));
    h.set_extensions({nc(257)});
    expect_malformed(h.build(), "257 subtrees");

    auto names = [](size_t n) {
        bytes_t rdns;
        for (size_t i = 0; i < n; ++i) {
            auto r = der(0x31, seq({oid("2.5.4.11"), utf8_der("u" + std::to_string(i))}));
            rdns.insert(rdns.end(), r.begin(), r.end());
        }
        return der(0x30, rdns);
    };
    hand n = h;
    n.extensions.clear();
    n.subject = names(64);
    EXPECT_TRUE(x509::certificate::parse(view(n.build())));
    n.subject = names(65);
    expect_malformed(n.build(), "65 attributes in a name");

    // 128 KiB
    hand l;
    auto pad = [](size_t n) {
        return extension("1.2.3.99", false, der(0x04, bytes_t(n, 0x41)));
    };
    size_t want = 128 * 1024, fill = want - 400;
    for (int i = 0; i < 8; ++i) {
        l.set_extensions({pad(fill)});
        fill += want - l.build().size();
    }
    ASSERT_EQ(l.build().size(), want);
    EXPECT_TRUE(x509::certificate::parse(view(l.build())));
    l.set_extensions({pad(fill + 1)});
    ASSERT_EQ(l.build().size(), want + 1);
    expect_malformed(l.build(), "a certificate of 128 KiB and one byte");
}

// The DER reader's depth: sixteen levels below the input, never more
TEST(Crypto_X509, TheReadersDepth) {
    using sgcl::crypto::detail::DerReader;
    for (unsigned depth : {15u, 16u, 17u, 40u}) {
        bytes_t v = {0x05, 0x00};
        for (unsigned i = 0; i < depth; ++i) {
            v = der(0x30, v);
        }
        DerReader r(v.data(), v.size());
        unsigned levels = 0;
        while (true) {
            DerReader c;
            if (!r.read(0x30, c)) {
                break;
            }
            r = c;
            ++levels;
        }
        EXPECT_EQ(levels, std::min(depth, 16u)) << depth;
    }
}

// The text of names as Go writes it: the common attributes in their fixed
// order, the rest after them, the escapes of RFC 2253
TEST(Crypto_X509, NamesAsText) {
    struct test {
        bytes_t name;
        const char* text;
    };
    auto rdn = [](std::initializer_list<std::pair<std::string, bytes_t>> attrs) {
        bytes_t set;
        for (auto& [o, v] : attrs) {
            auto a = seq({oid(o), v});
            set.insert(set.end(), a.begin(), a.end());
        }
        return der(0x31, set);
    };
    std::vector<test> tests = {
        {name_of({{"2.5.4.6", der(0x13, text("US"))}, {"2.5.4.10", utf8_der("Org")}, {"2.5.4.3", utf8_der("Name")}}), "CN=Name,O=Org,C=US"},
        {name_of({{"2.5.4.3", utf8_der("Name")}, {"2.5.4.10", utf8_der("Org")}, {"2.5.4.6", der(0x13, text("US"))}}), "CN=Name,O=Org,C=US"},
        {name_of({{"2.5.4.3", utf8_der("a,b+c\"d\\e<f>g;h")}}), "CN=a\\,b\\+c\\\"d\\\\e\\<f\\>g\\;h"},
        {name_of({{"2.5.4.3", utf8_der(" lead and trail ")}}), "CN=\\ lead and trail\\ "},
        {name_of({{"2.5.4.3", utf8_der("#hash#")}}), "CN=\\#hash#"},
        {name_of({{"2.5.4.11", utf8_der("B")}, {"2.5.4.11", utf8_der("A")}, {"2.5.4.10", utf8_der("O1")}}), "OU=B+OU=A,O=O1"},
        {name_of({{"2.5.4.3", utf8_der("first")}, {"2.5.4.3", utf8_der("last")}}), "CN=last"},
        {name_of({{"1.2.840.113549.1.9.1", der(0x16, text("a@b.c"))}, {"2.5.4.3", utf8_der("N")}}), "CN=N,1.2.840.113549.1.9.1=a@b.c"},
        {name_of({{"1.2.3.4", der(0x03, {0x00, 0x05})}, {"1.2.3.5", utf8_der("x")}}), "1.2.3.5=x,1.2.3.4=#03020005"},
        {name_of({{"2.5.4.3", der(0x03, {0x00, 0x05})}}), ""},
        {name_of({{"2.5.4.3", der(0x14, {'c', 0xe9})}}), "CN=c\xc3\xa9"},
        {name_of({{"2.5.4.3", der(0x1e, {0x00, 'A', 0x04, 0x10, 0x00, 0x00})}}), "CN=A\xd0\x90"},
        {name_of({{"2.5.4.5", der(0x13, text("123"))}, {"2.5.4.17", utf8_der("00-950")}, {"2.5.4.9", utf8_der("Street 1")}, {"2.5.4.7", utf8_der("City")}, {"2.5.4.8", utf8_der("State")}}),
         "SERIALNUMBER=123,POSTALCODE=00-950,STREET=Street 1,L=City,ST=State"},
        {der(0x30, cat({rdn({{"2.5.4.3", utf8_der("A")}, {"2.5.4.10", utf8_der("B")}})})), "CN=A,O=B"},
        {der(0x30, {}), ""},
    };
    for (auto& t : tests) {
        hand h;
        h.subject = t.name;
        auto c = parse_hand(h);
        EXPECT_EQ(s(c.subject().to_string()), t.text) << hex(t.name);
    }
    hand h;
    h.subject = name_of({{"2.5.4.3", utf8_der("N")}, {"2.5.4.10", utf8_der("O")}, {"2.5.4.10", utf8_der("P")}, {"1.2.3.4", der(0x03, {0x00, 0x05})}});
    auto c = parse_hand(h);
    auto& a = c.subject().attributes();
    ASSERT_EQ(a.size(), 4u);
    EXPECT_EQ(s(a[0].oid), "2.5.4.3");
    EXPECT_TRUE(a[0].text);
    EXPECT_EQ(s(a[3].value), "#03020005");
    EXPECT_FALSE(a[3].text);
    EXPECT_EQ(c.subject().organization().size(), 2u);
    EXPECT_TRUE(c.subject().country().empty());
}

// Host names (RFC 6125 §6.4, as Go matches them): a wildcard only as the
// whole leftmost label and for one label, never a partial one, never the
// domain itself; case folded in ASCII; a trailing dot of the name ignored;
// the common name never; an IP address never against DNS names
TEST(Crypto_X509, HostNames) {
    struct test {
        const char* sans;
        const char* cn;
        const char* host;
        bool ok;
    };
    std::vector<test> tests = {
        {"DNS:www.example.com", "", "www.example.com", true},
        {"DNS:www.example.com", "", "WWW.Example.COM", true},
        {"DNS:WWW.EXAMPLE.COM", "", "www.example.com", true},
        {"DNS:www.example.com", "", "www.example.com.", true},
        {"DNS:www.example.com", "", "example.com", false},
        {"DNS:www.example.com", "", "ww.example.com", false},
        {"DNS:www.example.com", "", "www.example.com.evil.com", false},   // the pattern a prefix of the name
        {"DNS:*.example.com", "", "a.example.com.evil.com", false},
        {"DNS:www.example.com", "", "www.example", false},
        {"DNS:www.example.com", "", "www.example.co", false},
        {"DNS:*.example.com", "", "a.example.com", true},
        {"DNS:*.example.com", "", "A.EXAMPLE.com", true},
        {"DNS:*.example.com", "", "example.com", false},
        {"DNS:*.example.com", "", "a.b.example.com", false},
        {"DNS:*.example.com", "", ".example.com", false},
        {"DNS:*.example.com", "", "*.example.com", true},    // an input with a star is not a valid host name: compared exactly
        {"DNS:f*.example.com", "", "foo.example.com", false},
        {"DNS:*o.example.com", "", "foo.example.com", false},
        {"DNS:f*.example.com", "", "f*.example.com", true},  // exactly
        {"DNS:a.*.example.com", "", "a.b.example.com", false},
        {"DNS:*.*.example.com", "", "a.b.example.com", false},
        {"DNS:*", "", "localhost", false},
        {"DNS:*.com", "", "example.com", true},            // Go takes it; the CA/B Forum forbids issuing it
        {"DNS:example.com.", "", "example.com", false},     // a pattern's trailing dot is not dropped
        {"DNS:xn--bcher-kva.example", "", "XN--BCHER-KVA.example", true},
        {"DNS:under_score.example.com", "", "under_score.example.com", true},
        {"DNS:a.example.com,DNS:b.example.com", "", "b.example.com", true},
        {"DNS:other.example.com", "www.example.com", "www.example.com", false},   // the CN never
        {"email:www.example.com", "www.example.com", "www.example.com", false},
        {"IP:192.0.2.1", "", "192.0.2.1", false},
        {"DNS:192.0.2.1", "", "192.0.2.1", false},          // written as an address: never a DNS name
        {"DNS:www.example.com", "", "[::1]", false},
        {"DNS:www.example.com", "", "", false},
        {"DNS:www.example.com", "", ".", false},
    };
    auto root = make_cert(root_spec(key_type::p256));
    for (auto& t : tests) {
        spec ls = leaf_spec(key_type::p256, t.sans);
        if (*t.cn) {
            ls.cn = t.cn;
        }
        auto c = parse_or_fail(make_cert(ls, &root).der);
        auto r = c.verify_hostname(string(t.host));
        EXPECT_EQ(bool(r), t.ok) << t.sans << " / " << t.host;
        if (!r) {
            EXPECT_EQ(r.error().reason(), reason::hostname_mismatch);
            EXPECT_EQ(r.error().code(), errc::verification);
        }
        // OpenSSL agrees where both have a rule (no partial wildcards, no CN, IP apart)
        if (*t.host && t.host[0] != '[' && t.host[0] != '.' && std::string(t.host) != "192.0.2.1" && std::string(t.host) != "." && std::string(t.host).find('*') == std::string::npos
            && std::string(t.sans) != "DNS:*.com" && std::string(t.sans) != "DNS:example.com." && std::string(t.host) != "www.example.com.") {
            auto m = make_cert(ls, &root);
            int o = X509_check_host(m.cert.get(), t.host, 0, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT, nullptr);
            EXPECT_EQ(o == 1, t.ok) << "OpenSSL: " << t.sans << " / " << t.host;
        }
    }
    // an address is refused by what it looks like, not by a ':' alone: a name with a port
    // is a name that does not match
    {
        auto named = parse_or_fail(make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &root).der);
        auto message = [&](const char* host) { return s(error_of(named.verify_hostname(string(host))).message()); };
        EXPECT_NE(message("[::1]").find("is an IP address"), std::string::npos);
        EXPECT_NE(message("2001:db8::1").find("is an IP address"), std::string::npos);
        EXPECT_NE(message("192.0.2.1").find("is an IP address"), std::string::npos);
        EXPECT_NE(message("www.example.com:443").find("is valid for www.example.com, not"), std::string::npos);
    }
    // IP addresses, as bytes
    auto c = parse_or_fail(make_cert(leaf_spec(key_type::p256, "IP:192.0.2.1,IP:2001:db8::1,DNS:192.0.2.9"), &root).der);
    unsigned char v4[] = {192, 0, 2, 1};
    unsigned char v4_other[] = {192, 0, 2, 9};
    unsigned char mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 0, 2, 1};
    unsigned char v6[] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    auto sl = [](unsigned char* p, size_t n) { return sgcl::slice<const byte>(reinterpret_cast<const byte*>(p), n); };
    EXPECT_TRUE(c.verify_ip(sl(v4, 4)));
    EXPECT_TRUE(c.verify_ip(sl(mapped, 16)));
    EXPECT_TRUE(c.verify_ip(sl(v6, 16)));
    EXPECT_FALSE(c.verify_ip(sl(v4_other, 4)));   // the DNS name "192.0.2.9" is not an address
    EXPECT_FALSE(c.verify_ip(sl(v6, 4)));
    EXPECT_THROW((void)c.verify_ip(sl(v4, 5)), std::invalid_argument);
    EXPECT_THROW((void)c.verify_ip(sl(v4, 0)), std::invalid_argument);
}

namespace {
    template<class A>
    constexpr bool verify_ip_takes = requires(const crypto::x509::certificate& c, A a) { c.verify_ip(a); };
}

// Bytes take a text as its characters; an address is not a text, so the
// forms of a text do not compile (four characters would pass for IPv4)
TEST(Crypto_X509, VerifyIpTakesNoText) {
    static_assert(verify_ip_takes<const unsigned char (&)[4]>);
    static_assert(verify_ip_takes<const std::array<uint8_t, 16>&>);
    static_assert(verify_ip_takes<const sgcl::vector<byte>&>);
    static_assert(!verify_ip_takes<const char (&)[5]>);
    static_assert(!verify_ip_takes<const sgcl::string&>);
    static_assert(!verify_ip_takes<const sgcl::slice<const char>&>);
    static_assert(!verify_ip_takes<std::string_view>);
    SUCCEED();
}
