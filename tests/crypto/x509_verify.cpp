//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X.509, verification: chains made by OpenSSL in the test (RSA PKCS #1 v1.5
// and PSS, ECDSA on P-256, P-384 and P-521, Ed25519, with intermediates) verified
// here and by X509_verify_cert, the verdict and the reason compared — valid
// chains of every algorithm, expired and not yet valid certificates at each
// level, broken signatures, SHA-1 and MD5, intermediates that are no CA or
// lack keyCertSign, path lengths, extended key usages, unknown critical
// extensions, name constraints of every kind, wildcards and IP addresses,
// the limit of ten intermediates, loops of cross-signatures, a missing root;
// then every case of Go's verify_test.go and name_constraints_test.go
// (tests/crypto/data/x509), checked against the verdict Go gave.
#include "x509_common.h"

#include <functional>

using namespace x509_test;
using sgcl::crypto::errc;

namespace {
    // Ours and OpenSSL's verdicts: both the one expected; where OpenSSL's
    // rule differs from Go's (which this module follows), its own expected
    // reason is given apart
    void expect_verdict(const chain_case& c, reason want, const char* what, int openssl_want = -1, size_t want_length = 0) {
        std::string message;
        size_t length = 0;
        reason ours = our_reason(c, &message, &length);
        EXPECT_EQ(reason_name(ours), std::string(reason_name(want))) << what << ": " << message;
        int e = openssl_verify(c);
        reason theirs = openssl_reason(e);
        reason expected_theirs = openssl_want < 0 ? want : reason(openssl_want);
        EXPECT_EQ(reason_name(theirs), std::string(reason_name(expected_theirs))) << what << " (OpenSSL: " << X509_verify_cert_error_string(e) << ")";
        if (want == reason::none) {
            EXPECT_EQ(length, want_length ? want_length : c.intermediates.size() + 2) << what;
        }
    }

    // root -> intermediate -> leaf of one key type, the leaf's SANs given
    chain_case simple(key_type k, sig s, const std::string& san = "DNS:www.example.com") {
        chain_case c;
        spec r = root_spec(k);
        r.algorithm = s;
        c.roots.push_back(make_cert(r));
        spec i = intermediate_spec(k);
        i.algorithm = s;
        c.intermediates.push_back(make_cert(i, &c.roots[0]));
        spec l = leaf_spec(k, san);
        l.algorithm = s;
        c.leaf = make_cert(l, &c.intermediates[0]);
        c.dns = "www.example.com";
        return c;
    }
}

// Valid chains of every algorithm, and the chain returned leaf to root
TEST(Crypto_X509_Verify, EveryAlgorithm) {
    struct alg {
        key_type key;
        sig s;
    };
    for (auto a : {alg{key_type::rsa2048, sig::sha256}, alg{key_type::rsa2048, sig::sha384}, alg{key_type::rsa2048, sig::sha512},
                   alg{key_type::rsa2048, sig::pss256}, alg{key_type::rsa2048, sig::pss384}, alg{key_type::rsa2048, sig::pss512},
                   alg{key_type::rsa1024, sig::sha256}, alg{key_type::p256, sig::sha256}, alg{key_type::p256, sig::sha384},
                   alg{key_type::p256, sig::sha512}, alg{key_type::p384, sig::sha256}, alg{key_type::p384, sig::sha384},
                   alg{key_type::p384, sig::sha512}, alg{key_type::ed25519, sig::sha256}, alg{key_type::p521, sig::sha512},
                   alg{key_type::p521, sig::sha256}}) {
        SCOPED_TRACE(std::string(key_name(a.key)) + " " + std::to_string(int(a.s)));
        auto c = simple(a.key, a.s);
        expect_verdict(c, reason::none, "valid");
        // the chain, leaf first
        x509::certificate_pool roots;
        roots.add(parse_or_fail(c.roots[0].der));
        x509::verify_options o;
        o.roots = roots;
        o.intermediates.add(parse_or_fail(c.intermediates[0].der));
        o.dns_name = "www.example.com";
        o.time = sgcl::time::datetime::from_unix(now, sgcl::time::zone::utc());
        auto chain = parse_or_fail(c.leaf.der).verify(o);
        ASSERT_TRUE(chain);
        ASSERT_EQ(chain->size(), 3u);
        EXPECT_EQ(to_bytes((*chain)[0].raw()), c.leaf.der);
        EXPECT_EQ(to_bytes((*chain)[1].raw()), c.intermediates[0].der);
        EXPECT_EQ(to_bytes((*chain)[2].raw()), c.roots[0].der);
        // a signature broken at each level
        for (int level = 0; level < 2; ++level) {
            auto b = c;
            if (level == 0) {
                spec l = leaf_spec(a.key, "DNS:www.example.com");
                l.algorithm = a.s;
                l.break_signature = true;
                b.leaf = make_cert(l, &b.intermediates[0]);
            } else {
                spec i = intermediate_spec(a.key);
                i.algorithm = a.s;
                i.break_signature = true;
                i.use_key = c.intermediates[0].key;
                b.intermediates[0] = make_cert(i, &b.roots[0]);
            }
            expect_verdict(b, reason::invalid_signature, level == 0 ? "the leaf's signature broken" : "the intermediate's signature broken");
        }
    }
}

// RSA-PSS: the salt as long as the parameters say (Go's
// PSSSaltLengthEqualsHash for certificates, OpenSSL's check of the
// parameters): a TBS signed with a salt of 20 under parameters of 32 does not
// verify, at the leaf or at the intermediate
TEST(Crypto_X509_Verify, PssSaltAsTheParametersSay) {
    for (auto s : {sig::pss256, sig::pss384, sig::pss512}) {
        auto c = simple(key_type::rsa2048, s);
        for (int salt : {20, 0, 32 + 32}) {
            auto b = c;
            spec l = leaf_spec(key_type::rsa2048, "DNS:www.example.com");
            l.algorithm = s;
            l.pss_salt = salt;
            b.leaf = make_cert(l, &b.intermediates[0]);
            // a salt of 64 is right for SHA-512
            bool right = salt == 64 && s == sig::pss512;
            expect_verdict(b, right ? reason::none : reason::invalid_signature, "the leaf signed with another salt");
        }
        auto b = c;
        spec i = intermediate_spec(key_type::rsa2048);
        i.algorithm = s;
        i.pss_salt = 20;
        i.use_key = c.intermediates[0].key;
        b.intermediates[0] = make_cert(i, &b.roots[0]);
        expect_verdict(b, reason::invalid_signature, "the intermediate signed with a salt of 20");
    }
}

// The validity at each level
TEST(Crypto_X509_Verify, Validity) {
    auto c = simple(key_type::p256, sig::sha256);
    for (int level = 0; level < 3; ++level) {
        for (bool future : {false, true}) {
            chain_case b;
            spec r = root_spec(key_type::p256);
            spec i = intermediate_spec(key_type::p256);
            spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
            spec& s = level == 0 ? l : level == 1 ? i : r;
            if (future) {
                s.not_before = now + 60;
            } else {
                s.not_after = now - 60;
            }
            b.roots.push_back(make_cert(r));
            b.intermediates.push_back(make_cert(i, &b.roots[0]));
            b.leaf = make_cert(l, &b.intermediates[0]);
            b.dns = "www.example.com";
            expect_verdict(b, future ? reason::not_yet_valid : reason::expired, future ? "not yet valid" : "expired");
        }
    }
    // the edges: valid at not_before and at not_after, inclusive
    chain_case e = c;
    spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
    l.not_before = now;
    l.not_after = now + 10;
    e.leaf = make_cert(l, &e.intermediates[0]);
    expect_verdict(e, reason::none, "at not_before");
    e.time = now + 10;
    // RFC 5280 §4.1.2.5 and Go: the period includes not_after; OpenSSL ends it there
    expect_verdict(e, reason::none, "at not_after", int(reason::expired));
    e.time = now + 11;
    expect_verdict(e, reason::expired, "a second after not_after");
    // a time past 2050 (GeneralizedTime) and far past datetime's range
    chain_case f = c;
    spec far = leaf_spec(key_type::p256, "DNS:www.example.com");
    far.not_after = 253402300799;   // 9999-12-31T23:59:59Z
    f.leaf = make_cert(far, &f.intermediates[0]);
    expect_verdict(f, reason::none, "valid until 9999");
}

// Weak digests: SHA-1 and MD5 never
TEST(Crypto_X509_Verify, InsecureAlgorithms) {
    for (sig s : {sig::sha1, sig::md5}) {
        auto c = simple(key_type::rsa2048, sig::sha256);
        spec l = leaf_spec(key_type::rsa2048, "DNS:www.example.com");
        l.algorithm = s;
        c.leaf = make_cert(l, &c.intermediates[0]);
        expect_verdict(c, reason::insecure_algorithm, s == sig::sha1 ? "SHA-1" : "MD5");
        auto r = parse_or_fail(c.leaf.der).check_signature_from(parse_or_fail(c.intermediates[0].der));
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().reason(), reason::insecure_algorithm);
    }
    // a root signed with SHA-1 is a trust anchor, whose own signature is never checked
    chain_case c;
    spec r = root_spec(key_type::rsa2048);
    r.algorithm = sig::sha1;
    c.roots.push_back(make_cert(r));
    c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.roots[0]);
    c.dns = "www.example.com";
    expect_verdict(c, reason::none, "a SHA-1 root");
}

// CA constraints: basicConstraints, keyUsage keyCertSign, path lengths
TEST(Crypto_X509_Verify, CaConstraints) {
    auto with_intermediate = [](const std::function<void(spec&)>& change) {
        chain_case c;
        c.roots.push_back(make_cert(root_spec(key_type::p256)));
        spec i = intermediate_spec(key_type::p256);
        change(i);
        c.intermediates.push_back(make_cert(i, &c.roots[0]));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[0]);
        c.dns = "www.example.com";
        return c;
    };
    expect_verdict(with_intermediate([](spec& s) { s.basic_constraints = "critical,CA:FALSE"; }), reason::not_a_ca, "an intermediate with cA false");
    expect_verdict(with_intermediate([](spec& s) { s.basic_constraints = ""; }), reason::not_a_ca, "an intermediate without basicConstraints");
    // OpenSSL calls a CA without keyCertSign an invalid CA
    expect_verdict(with_intermediate([](spec& s) { s.key_usage = "critical,digitalSignature"; }), reason::missing_cert_sign, "an intermediate without keyCertSign",
                   int(reason::not_a_ca));
    expect_verdict(with_intermediate([](spec& s) { s.key_usage = ""; }), reason::none, "an intermediate without keyUsage");
    expect_verdict(with_intermediate([](spec& s) { s.basic_constraints = "critical,CA:TRUE,pathlen:0"; }), reason::none, "pathlen 0 over a leaf");

    // pathlen 0 at the root over one intermediate; at the first of two
    {
        chain_case c;
        spec r = root_spec(key_type::p256);
        r.basic_constraints = "critical,CA:TRUE,pathlen:0";
        c.roots.push_back(make_cert(r));
        c.intermediates.push_back(make_cert(intermediate_spec(key_type::p256), &c.roots[0]));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[0]);
        c.dns = "www.example.com";
        expect_verdict(c, reason::path_length, "pathlen 0 at the root, one intermediate");
    }
    {
        chain_case c;
        c.roots.push_back(make_cert(root_spec(key_type::p256)));
        spec i1 = intermediate_spec(key_type::p256, "Intermediate 1");
        i1.basic_constraints = "critical,CA:TRUE,pathlen:0";
        c.intermediates.push_back(make_cert(i1, &c.roots[0]));
        c.intermediates.push_back(make_cert(intermediate_spec(key_type::p256, "Intermediate 2"), &c.intermediates[0]));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[1]);
        c.dns = "www.example.com";
        expect_verdict(c, reason::path_length, "pathlen 0 over another intermediate");
        spec ok = intermediate_spec(key_type::p256, "Intermediate 1");
        ok.basic_constraints = "critical,CA:TRUE,pathlen:1";
        ok.use_key = c.intermediates[0].key;
        c.intermediates[0] = make_cert(ok, &c.roots[0]);
        expect_verdict(c, reason::none, "pathlen 1 over another intermediate");
    }
    // a leaf that is a CA verifies as a leaf (Go and OpenSSL)
    {
        chain_case c = with_intermediate([](spec&) {});
        spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
        l.basic_constraints = "critical,CA:TRUE";
        l.key_usage = "critical,digitalSignature,keyCertSign";
        c.leaf = make_cert(l, &c.intermediates[0]);
        expect_verdict(c, reason::none, "a CA as the leaf");
    }
    // a version 1 root (no extensions) is a trust anchor; a version 1 intermediate is no CA
    {
        chain_case c;
        spec r = root_spec(key_type::p256);
        r.version = 1;
        c.roots.push_back(make_cert(r));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.roots[0]);
        c.dns = "www.example.com";
        expect_verdict(c, reason::none, "a version 1 root");
        spec i = intermediate_spec(key_type::p256);
        i.version = 1;
        c.intermediates.push_back(make_cert(i, &c.roots[0]));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[0]);
        expect_verdict(c, reason::not_a_ca, "a version 1 intermediate");
    }
}

// Unknown critical extensions, at each level; unknown non-critical ones pass
TEST(Crypto_X509_Verify, CriticalExtensions) {
    for (int level = 0; level < 3; ++level) {
        chain_case c;
        spec r = root_spec(key_type::p256);
        spec i = intermediate_spec(key_type::p256);
        spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
        (level == 0 ? l : level == 1 ? i : r).unknown_critical = true;
        c.roots.push_back(make_cert(r));
        c.intermediates.push_back(make_cert(i, &c.roots[0]));
        c.leaf = make_cert(l, &c.intermediates[0]);
        c.dns = "www.example.com";
        expect_verdict(c, reason::unhandled_critical_extension, "an unknown critical extension");
    }
}

// Extended key usage, nested down the chain
TEST(Crypto_X509_Verify, ExtendedKeyUsage) {
    auto make = [](const std::string& root_eku, const std::string& inter_eku, const std::string& leaf_eku) {
        chain_case c;
        spec r = root_spec(key_type::p256);
        r.ext_key_usage = root_eku;
        spec i = intermediate_spec(key_type::p256);
        i.ext_key_usage = inter_eku;
        spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
        l.ext_key_usage = leaf_eku;
        c.roots.push_back(make_cert(r));
        c.intermediates.push_back(make_cert(i, &c.roots[0]));
        c.leaf = make_cert(l, &c.intermediates[0]);
        c.dns = "www.example.com";
        return c;
    };
    expect_verdict(make("", "", "serverAuth"), reason::none, "serverAuth");
    expect_verdict(make("", "", ""), reason::none, "no EKU at all");
    expect_verdict(make("", "", "clientAuth"), reason::incompatible_usage, "a leaf for clients only");
    expect_verdict(make("", "clientAuth", "serverAuth"), reason::incompatible_usage, "an intermediate for clients only");
    expect_verdict(make("", "serverAuth,clientAuth", "serverAuth"), reason::none, "an intermediate for both");
    // Go takes anyExtendedKeyUsage in a CA as every usage; OpenSSL's ssl_server purpose wants serverAuth there
    expect_verdict(make("", "anyExtendedKeyUsage", "serverAuth"), reason::none, "an intermediate for any usage", int(reason::incompatible_usage));
    auto client = make("", "", "clientAuth");
    client.ekus = {x509::ext_key_usage::client_auth};
    expect_verdict(client, reason::none, "clientAuth asked");
    // any usage asked: no check (OpenSSL checks its default purpose, none)
    auto any = make("", "", "codeSigning");
    any.ekus = {x509::ext_key_usage::any};
    EXPECT_EQ(our_reason(any), reason::none);
}

// Name constraints of each kind, at the root and at the intermediate
TEST(Crypto_X509_Verify, NameConstraints) {
    struct test {
        const char* constraints;
        const char* san;
        reason want;
        int openssl = -1;
    };
    std::vector<test> tests = {
        {"critical,permitted;DNS:example.com", "DNS:www.example.com", reason::none},
        {"critical,permitted;DNS:example.com", "DNS:example.com", reason::none},
        {"critical,permitted;DNS:.example.com", "DNS:www.example.com", reason::none},
        {"critical,permitted;DNS:example.com", "DNS:www.example.org", reason::name_constraints},
        {"critical,permitted;DNS:example.com", "DNS:www.badexample.com", reason::name_constraints},
        {"critical,permitted;DNS:example.com", "DNS:*.example.com", reason::none},
        {"critical,excluded;DNS:bad.example.com", "DNS:www.example.com", reason::none},
        {"critical,excluded;DNS:bad.example.com", "DNS:x.bad.example.com", reason::name_constraints},
        {"critical,excluded;DNS:bad.example.com", "DNS:BAD.Example.COM", reason::name_constraints},
        // a wildcard that would match an excluded name: Go refuses it, OpenSSL compares the text
        {"critical,excluded;DNS:bad.example.com", "DNS:*.example.com", reason::name_constraints, int(reason::none)},
        {"critical,permitted;IP:10.0.0.0/255.0.0.0", "DNS:www.example.com,IP:10.1.2.3", reason::none},
        {"critical,permitted;IP:10.0.0.0/255.0.0.0", "DNS:www.example.com,IP:11.1.2.3", reason::name_constraints},
        {"critical,permitted;IP:10.0.0.0/255.0.0.0", "DNS:www.example.com,IP:2001:db8::1", reason::name_constraints},
        {"critical,excluded;IP:192.168.0.0/255.255.0.0", "DNS:www.example.com,IP:192.168.4.4", reason::name_constraints},
        {"critical,excluded;IP:2001:db8::/ffff:ffff::", "DNS:www.example.com,IP:2001:db8::7", reason::name_constraints},
        {"critical,permitted;IP:2001:db8::/ffff:ffff::", "DNS:www.example.com,IP:2001:db8::7", reason::none},
        {"critical,permitted;email:example.com", "DNS:www.example.com,email:a@example.com", reason::none},
        {"critical,permitted;email:example.com", "DNS:www.example.com,email:a@example.org", reason::name_constraints},
        {"critical,permitted;email:a@example.com", "DNS:www.example.com,email:b@example.com", reason::name_constraints},
        {"critical,excluded;email:.example.com", "DNS:www.example.com,email:a@sub.example.com", reason::name_constraints},
        {"critical,permitted;URI:.example.com", "DNS:www.example.com,URI:https://www.example.com/x", reason::none},
        {"critical,permitted;URI:.example.com", "DNS:www.example.com,URI:https://www.example.org/x", reason::name_constraints},
        // a kind not constrained passes
        {"critical,permitted;DNS:example.com", "DNS:www.example.com,IP:1.2.3.4", reason::none},
        {"critical,permitted;DNS:example.com,permitted;IP:10.0.0.0/255.0.0.0", "DNS:www.example.com,IP:1.2.3.4", reason::name_constraints},
    };
    for (auto& t : tests) {
        for (int at = 0; at < 2; ++at) {
            SCOPED_TRACE(std::string(t.constraints) + " / " + t.san + (at == 0 ? " at the root" : " at the intermediate"));
            chain_case c;
            spec r = root_spec(key_type::p256);
            spec i = intermediate_spec(key_type::p256);
            (at == 0 ? r : i).name_constraints = t.constraints;
            c.roots.push_back(make_cert(r));
            c.intermediates.push_back(make_cert(i, &c.roots[0]));
            c.leaf = make_cert(leaf_spec(key_type::p256, t.san), &c.intermediates[0]);
            c.ekus = {x509::ext_key_usage::server_auth};
            expect_verdict(c, t.want, "name constraints", t.openssl);
        }
    }
    // an intermediate's own names are checked against the root's constraints
    chain_case c;
    spec r = root_spec(key_type::p256);
    r.name_constraints = "critical,permitted;DNS:example.com";
    spec i = intermediate_spec(key_type::p256);
    i.san = "DNS:ca.example.org";
    c.roots.push_back(make_cert(r));
    c.intermediates.push_back(make_cert(i, &c.roots[0]));
    c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[0]);
    expect_verdict(c, reason::name_constraints, "an intermediate's SAN outside the root's constraints");
}

// Names asked of the leaf: DNS names with wildcards, IP addresses
TEST(Crypto_X509_Verify, HostNameAndIp) {
    auto c = simple(key_type::p256, sig::sha256, "DNS:www.example.com,DNS:*.example.org,IP:192.0.2.7,IP:2001:db8::5");
    expect_verdict(c, reason::none, "the name");
    c.dns = "a.example.org";
    expect_verdict(c, reason::none, "a wildcard");
    c.dns = "a.b.example.org";
    expect_verdict(c, reason::hostname_mismatch, "a wildcard over two labels");
    c.dns = "example.org";
    expect_verdict(c, reason::hostname_mismatch, "a wildcard over none");
    c.dns = "www.example.net";
    expect_verdict(c, reason::hostname_mismatch, "another name");
    c.dns.clear();
    c.ip = {192, 0, 2, 7};
    expect_verdict(c, reason::none, "an IPv4 address");
    c.ip = {192, 0, 2, 8};
    expect_verdict(c, reason::hostname_mismatch, "another IPv4 address");
    c.ip = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
    expect_verdict(c, reason::none, "an IPv6 address");
}

// Chains that end nowhere: a missing root, a missing intermediate, a loop
// of cross-signatures, the leaf alone
TEST(Crypto_X509_Verify, UnknownAuthority) {
    auto c = simple(key_type::p256, sig::sha256);
    auto b = c;
    b.roots.clear();
    b.roots.push_back(make_cert(root_spec(key_type::p256, "Another Root")));
    expect_verdict(b, reason::unknown_authority, "no root of the pool");
    b = c;
    b.intermediates.clear();
    expect_verdict(b, reason::unknown_authority, "the intermediate missing");
    // the same name, another key: the signature is what decides
    b = c;
    b.roots[0] = make_cert(root_spec(key_type::p256));
    expect_verdict(b, reason::invalid_signature, "a root of the same name and another key", int(reason::unknown_authority));
    // A and B sign each other, neither is a root
    {
        chain_case l;
        l.roots.push_back(make_cert(root_spec(key_type::p256, "Unrelated Root")));
        auto ka = new_key(key_type::p256), kb = new_key(key_type::p256);
        spec a = intermediate_spec(key_type::p256, "Loop A");
        a.use_key = ka;
        spec bs = intermediate_spec(key_type::p256, "Loop B");
        bs.use_key = kb;
        made self_a = make_cert(a);
        made b_by_a = make_cert(bs, &self_a);
        made a_by_b = make_cert(a, &b_by_a);
        l.intermediates = {b_by_a, a_by_b};
        l.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &b_by_a);
        l.dns = "www.example.com";
        expect_verdict(l, reason::unknown_authority, "a loop of two cross-signed CAs");
    }
    // a self-signed leaf alone, and the same leaf in the roots (Go takes it, OpenSSL
    // takes a self-signed trust anchor too)
    {
        chain_case s;
        spec l = leaf_spec(key_type::p256, "DNS:www.example.com");
        l.basic_constraints = "critical,CA:FALSE";
        s.leaf = make_cert(l);
        s.roots.push_back(make_cert(root_spec(key_type::p256)));
        s.dns = "www.example.com";
        expect_verdict(s, reason::unknown_authority, "a self-signed leaf");
        s.roots.push_back(s.leaf);
        EXPECT_EQ(our_reason(s), reason::none);
    }
}

// At most ten intermediates
TEST(Crypto_X509_Verify, TenIntermediatesAtMost) {
    for (size_t n : {10u, 11u}) {
        chain_case c;
        c.roots.push_back(make_cert(root_spec(key_type::p256)));
        const made* parent = &c.roots[0];
        c.intermediates.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            c.intermediates.push_back(make_cert(intermediate_spec(key_type::p256, "Intermediate " + std::to_string(i)), parent));
            parent = &c.intermediates.back();
        }
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), parent);
        c.dns = "www.example.com";
        expect_verdict(c, n == 10 ? reason::none : reason::too_many_intermediates, n == 10 ? "ten intermediates" : "eleven intermediates");
    }
}

// Two paths, one broken: the other is found (a cross-signed intermediate
// whose first candidate root has expired)
TEST(Crypto_X509_Verify, AnotherPath) {
    chain_case c;
    spec old_root = root_spec(key_type::p256, "Old Root");
    old_root.not_after = now - 10;
    c.roots.push_back(make_cert(old_root));
    c.roots.push_back(make_cert(root_spec(key_type::p256, "New Root")));
    auto key = new_key(key_type::p256);
    spec i = intermediate_spec(key_type::p256);
    i.use_key = key;
    c.intermediates.push_back(make_cert(i, &c.roots[0]));   // by the old root
    c.intermediates.push_back(make_cert(i, &c.roots[1]));   // the same name and key, by the new one
    c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[0]);
    c.dns = "www.example.com";
    // OpenSSL takes the first issuer it finds and does not try the other
    expect_verdict(c, reason::none, "a second path past an expired root", int(reason::expired), 3);
}

// A root rolled over to a new key: the old root in the pool, its new key
// cross-signed by it among the intermediates. The old root is tried first
// as the issuer of the intermediate below the new key, and its key does not
// verify that signature; when the path through the cross-signature then
// fails higher, the reason is that path's, not the signature of the first
// parent tried (Go's buildChains keeps the hint of a signature only when no
// parent's signature verified)
TEST(Crypto_X509_Verify, ARolledOverRootsReason) {
    auto rolled = [](const std::function<void(spec& old_root, spec& cross)>& change) {
        chain_case c;
        spec r = root_spec(key_type::p256);
        spec x = root_spec(key_type::p256);   // the same name, a key of its own
        x.not_before = now - 86400 * 365;
        change(r, x);
        c.roots.push_back(make_cert(r));
        c.intermediates.push_back(make_cert(x, &c.roots[0]));
        c.intermediates.push_back(make_cert(intermediate_spec(key_type::p256), &c.intermediates[0]));
        c.leaf = make_cert(leaf_spec(key_type::p256, "DNS:www.example.com"), &c.intermediates[1]);
        c.dns = "www.example.com";
        return c;
    };
    expect_verdict(rolled([](spec&, spec&) {}), reason::none, "the new key through its cross-signature");
    expect_verdict(rolled([](spec&, spec& x) { x.not_after = now - 10; }), reason::expired, "the cross-signature expired");
    expect_verdict(rolled([](spec& r, spec&) { r.basic_constraints = "critical,CA:TRUE,pathlen:0"; }), reason::path_length,
                   "the old root's path length over the cross-signature");
    // the cross-signature is self-issued (the same name above and below): RFC 5280
    // §4.2.1.9 and OpenSSL do not count it against a pathLenConstraint (Go does)
    expect_verdict(rolled([](spec& r, spec&) { r.basic_constraints = "critical,CA:TRUE,pathlen:1"; }), reason::none,
                   "the old root's path length of 1 over the self-issued cross-signature");
    // no parent's signature verifies: the signature is the reason
    auto c = rolled([](spec&, spec&) {});
    c.intermediates.erase(c.intermediates.begin());
    expect_verdict(c, reason::invalid_signature, "the cross-signature missing", int(reason::unknown_authority));
}

namespace {
    // A case of the Go data files
    struct go_case {
        std::string name;
        int64_t time = 0;
        std::string dns;
        std::vector<int> ekus;
        std::string expect;
        std::vector<bytes_t> roots, intermediates;
        bytes_t leaf;
    };

    std::vector<go_case> read_go_cases(const std::string& file) {
        std::vector<go_case> out;
        std::string text = read_file(data_dir() + "/" + file);
        size_t pos = 0;
        while ((pos = text.find("case: ", pos)) != std::string::npos) {
            size_t end = text.find("\ncase: ", pos + 1);
            std::string block = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? text.size() : end + 1;
            go_case c;
            std::istringstream in(block);
            std::string line;
            while (std::getline(in, line) && line.rfind("-----", 0) != 0) {
                auto v = [&](const char* k) { return line.substr(std::strlen(k)); };
                if (line.rfind("case: ", 0) == 0) c.name = v("case: ");
                else if (line.rfind("time: ", 0) == 0) c.time = std::stoll(v("time: "));
                else if (line.rfind("dns: ", 0) == 0) c.dns = v("dns: ");
                else if (line.rfind("eku: ", 0) == 0) {
                    std::string e = v("eku: ");
                    std::istringstream es(e);
                    std::string x;
                    while (std::getline(es, x, ',')) {
                        if (!x.empty()) c.ekus.push_back(std::stoi(x));
                    }
                } else if (line.rfind("expect: ", 0) == 0) c.expect = v("expect: ");
            }
            for (auto& [type, der] : pem_blocks(block)) {
                if (type == "ROOT CERTIFICATE") c.roots.push_back(der);
                else if (type == "INTERMEDIATE CERTIFICATE") c.intermediates.push_back(der);
                else if (type == "LEAF CERTIFICATE") c.leaf = der;
            }
            out.push_back(std::move(c));
        }
        return out;
    }

    // Our verdict on a Go case: "ok" or the reason's name. A root or an
    // intermediate that does not parse here is counted in unparsed (and
    // named in message): left out silently, it would change the case
    std::string ours(const go_case& c, std::string& message, size_t& unparsed) {
        x509::verify_options o;
        x509::certificate_pool roots;
        auto add = [&](x509::certificate_pool& pool, const bytes_t& der, const char* what) {
            auto p = x509::certificate::parse(view(der));
            if (p) {
                pool.add(*p);
            } else {
                ++unparsed;
                message += std::string(what) + " not parsed: " + std::string(p.error().message().view()) + "; ";
            }
        };
        for (auto& r : c.roots) {
            add(roots, r, "a root");
        }
        o.roots = roots;
        for (auto& i : c.intermediates) {
            add(o.intermediates, i, "an intermediate");
        }
        o.dns_name = string(c.dns);
        o.time = sgcl::time::datetime::from_unix(c.time, sgcl::time::zone::utc());
        for (int e : c.ekus) {
            o.key_usages.push_back(x509::ext_key_usage(e + 1));
        }
        auto leaf = x509::certificate::parse(view(c.leaf));
        if (!leaf) {
            message += std::string(leaf.error().message().view());
            return "parse_error";
        }
        auto r = leaf->verify(o);
        if (r) {
            std::string names;
            for (size_t i = 0; i < r->size(); ++i) {
                names += (i == 0 ? "" : " / ") + std::string((*r)[i].subject().common_name().view());
            }
            return "ok " + names;
        }
        message += std::string(r.error().message().view());
        return reason_name(r.error().reason());
    }

    // Whether our verdict agrees with Go's: the same, or an unknown
    // authority of Go's whose hint is the reason given here (this module
    // says why the one candidate failed where Go says it found none). With
    // a hint, the hint is wanted: an unknown authority here would let pass
    // a regression that loses the reason
    bool agrees(const std::string& go, const std::string& mine) {
        if (go == mine) {
            return true;
        }
        if (go.rfind("ok", 0) == 0 && mine.rfind("ok", 0) == 0) {
            return go == mine;
        }
        size_t slash = go.find('/');
        if (go.rfind("unknown_authority", 0) == 0 && slash != std::string::npos) {
            std::string hint = go.substr(slash + 1);
            if (hint == "constraint_violation") {
                return mine == "not_a_ca" || mine == "missing_cert_sign";
            }
            return mine == hint;
        }
        return false;
    }
}

TEST(Crypto_X509_Verify, GoVerifyCases) {
    auto cases = read_go_cases("go_verify.txt");
    ASSERT_GT(cases.size(), 20u);
    size_t n = 0;
    for (auto& c : cases) {
        std::string message;
        size_t unparsed = 0;
        std::string mine = ours(c, message, unparsed);
        EXPECT_EQ(unparsed, 0u) << c.name << ": " << message;
        // Go takes the SHA-1 leaf of "SHA1 leaf" no more either; where Go's
        // name says a legacy CN is relied on, both refuse
        EXPECT_TRUE(agrees(c.expect, mine)) << c.name << ": Go " << c.expect << ", here " << mine << " (" << message << ")";
        ++n;
    }
    std::printf("  %zu cases of verify_test.go\n", n);
}

TEST(Crypto_X509_Verify, GoNameConstraintCases) {
    auto cases = read_go_cases("go_name_constraints.txt");
    ASSERT_GT(cases.size(), 100u);
    size_t n = 0;
    for (auto& c : cases) {
        if (c.expect == "generation_failed") {
            continue;
        }
        std::string message;
        size_t unparsed = 0;
        std::string mine = ours(c, message, unparsed);
        EXPECT_EQ(unparsed, 0u) << c.name << ": " << message;
        EXPECT_TRUE(agrees(c.expect, mine.rfind("ok", 0) == 0 ? "ok" : mine) || agrees(c.expect, mine))
            << c.name << ": Go " << c.expect << ", here " << mine << " (" << message << ")";
        ++n;
    }
    std::printf("  %zu cases of name_constraints_test.go\n", n);
}

// verify() with no roots: the system's; a leaf of the tests is unknown to it
TEST(Crypto_X509_Verify, SystemRootsByDefault) {
    int64_t real_now = sgcl::time::now().unix();
    spec rs = root_spec(key_type::p256);
    rs.not_before = real_now - 86400;
    auto root = make_cert(rs);
    spec l = leaf_spec(key_type::p256);
    l.not_before = real_now - 86400;
    auto leaf = parse_or_fail(make_cert(l, &root).der);
    auto r = leaf.verify();
    ASSERT_FALSE(r);
    if (r.error().code() == errc::unsupported) {
        GTEST_SKIP() << "no system roots";
    }
    EXPECT_EQ(r.error().reason(), reason::unknown_authority);
    // a root of the system verifies against the system's pool, as the leaf of a chain of one
    auto pool = x509::certificate_pool::system();
    ASSERT_TRUE(pool);
    ASSERT_GT(pool->size(), 0u);
    size_t verified = 0;
    for (auto& root : pool->certificates()) {
        x509::verify_options o;
        o.key_usages.push_back(x509::ext_key_usage::any);
        o.time = sgcl::time::datetime::from_unix(root.not_before().unix() + 3600, sgcl::time::zone::utc());
        auto v = root.verify(o);
        if (v) {
            EXPECT_EQ(v->size(), 1u);
            ++verified;
        }
    }
    EXPECT_EQ(verified, pool->size());
}
