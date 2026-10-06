//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// CMS (RFC 5652, 5083, 5753, 8419) and S/MIME 4.0 (RFC 8551) against
// OpenSSL 3.6's `cms` and `smime`, both ways, for every kind of key the
// module has: SignedData made here verified by OpenSSL and OpenSSL's
// verified here (embedded and detached, RSA, ECDSA on the three curves,
// Ed25519, RSASSA-PSS and signatures without signed attributes from
// OpenSSL); EnvelopedData and AuthEnvelopedData made here decrypted by
// OpenSSL and OpenSSL's decrypted here (RSAES-OAEP, ECDH on the three
// curves, AES-GCM and AES-CBC of every key size); S/MIME messages signed
// and encrypted both ways; and the contract at its boundaries (a changed
// byte, the wrong recipient, a chain that does not verify, PKCS #1 v1.5
// key transport refused).
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/crypto.h"
#include "sgcl/crypto/smime.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace sgcl;
namespace x509 = sgcl::crypto::x509;
namespace cms = sgcl::crypto::cms;

namespace {
    std::string openssl() {
        if (const char* e = std::getenv("SGCL_OPENSSL")) {
            return e;
        }
        const char* p = "/opt/homebrew/opt/openssl@3/bin/openssl";
        return ::access(p, X_OK) == 0 ? p : "";
    }

    struct Dir {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("sgcl_cms_" + std::to_string(::getpid()));

        Dir() {
            std::filesystem::create_directories(path);
        }

        ~Dir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::string operator/(const std::string& name) const {
            return (path / name).string();
        }
    };

    int sh(const std::string& cmd) {
        return std::system((cmd + " > /dev/null 2>&1").c_str());
    }

    std::string read(const std::string& file) {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream s;
        s << in.rdbuf();
        return s.str();
    }

    void write(const std::string& file, const slice<const byte>& b) {
        std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    }

    slice<const byte> bytes(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    std::string text(const slice<const byte>& b) {
        return std::string(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // A CA of P-256 and, for each kind, a leaf of S/MIME (emailProtection)
    // signed by it, with its key read here
    struct Pki {
        Dir d;
        x509::certificate_pool roots;

        Pki() {
            const std::string o = openssl();
            sh(o + " genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out " + d / "ca.key");
            std::ofstream(d / "ca.ext") << "basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\nsubjectKeyIdentifier=hash\n";
            sh(o + " req -x509 -new -key " + d / "ca.key" + " -subj /CN=cms-ca -days 30 -extensions v3_ca -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign,cRLSign -out " +
               d / "ca.pem");
            roots = x509::certificate_pool::from_pem(string(read(d / "ca.pem")));
        }

        // The leaf of a kind ("rsa", "ed25519", "P-256"…): its PEM file and key file
        void leaf(const std::string& k) {
            if (std::filesystem::exists(d / (k + ".pem"))) {
                return;
            }
            const std::string o = openssl();
            const std::string gen = k == "rsa" ? "-algorithm RSA -pkeyopt rsa_keygen_bits:2048" : k == "ed25519" ? "-algorithm ED25519" : "-algorithm EC -pkeyopt ec_paramgen_curve:" + k;
            const std::string usage = k == "rsa" ? "digitalSignature,keyEncipherment" : k == "ed25519" ? "digitalSignature" : "digitalSignature,keyAgreement";
            std::ofstream(d / (k + ".ext")) << "basicConstraints=CA:FALSE\nkeyUsage=critical," << usage << "\nextendedKeyUsage=emailProtection\nsubjectKeyIdentifier=hash\nsubjectAltName=email:" << k
                                            << "@example.test\n";
            sh(o + " genpkey " + gen + " -out " + d / (k + ".key"));
            sh(o + " req -new -key " + d / (k + ".key") + " -subj /CN=" + k + " -out " + d / (k + ".csr"));
            sh(o + " x509 -req -in " + d / (k + ".csr") + " -CA " + d / "ca.pem" + " -CAkey " + d / "ca.key" + " -days 30 -extfile " + d / (k + ".ext") + " -out " + d / (k + ".pem"));
        }

        x509::certificate certificate(const std::string& k) {
            leaf(k);
            return x509::certificate::from_pem(string(read(d / (k + ".pem")))).value();
        }

        // The key as a PKCS #12 file read gives it: every kind through one type
        crypto::pkcs12 key(const std::string& k) {
            leaf(k);
            const std::string p12 = d / (k + ".p12");
            sh(openssl() + " pkcs12 -export -in " + d / (k + ".pem") + " -inkey " + d / (k + ".key") + " -out " + p12 + " -passout pass:x");
            return crypto::pkcs12::parse(bytes(read(p12)), "x").value();
        }

        cms::verify_options options() const {
            cms::verify_options o;
            o.chain.roots = roots;
            return o;
        }
    };

    const char* const Kinds[] = {"rsa", "P-256", "P-384", "P-521", "ed25519"};
}

TEST(Cms_OpenSsl, SignaturesBothWays) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Pki pki;
    const std::string content = "The content signed,\r\nof two lines.\r\n";
    std::ofstream(pki.d / "content.txt", std::ios::binary) << content;
    for (const char* k : Kinds) {
        auto cert = pki.certificate(k);
        auto key = pki.key(k);
        for (bool detached : {false, true}) {
            const std::string what = std::string(k) + (detached ? " detached" : " embedded");
            // ours, verified by OpenSSL
            auto sd = cms::sign(bytes(content), cert, key.signing_key(), {.detached = detached});
            write(pki.d / "ours.der", sd);
            const std::string verify = openssl() + " cms -verify -binary -inform DER -in " + pki.d / "ours.der" + " -CAfile " + pki.d / "ca.pem" + " -purpose smimesign -out " + pki.d / "out.txt" +
                                       (detached ? " -content " + pki.d / "content.txt" : "");
            EXPECT_EQ(sh(verify), 0) << what << ": OpenSSL does not verify ours";
            EXPECT_EQ(read(pki.d / "out.txt"), content) << what;
            // and here
            auto v = detached ? cms::verify_detached(sd, bytes(content), pki.options()) : cms::verify(sd, pki.options());
            ASSERT_TRUE(v) << what << ": " << v.error().message().view();
            EXPECT_EQ(text(v->content), content) << what;
            ASSERT_EQ(v->signer.size(), 2u) << what;
            EXPECT_TRUE(v->signing_time.has_value()) << what;
            // OpenSSL's, verified here
            const std::string sign = openssl() + " cms -sign -binary -outform DER -in " + pki.d / "content.txt" + " -signer " + pki.d / (std::string(k) + ".pem") + " -inkey " +
                                     pki.d / (std::string(k) + ".key") + " -out " + pki.d / "theirs.der" + (detached ? "" : " -nodetach") +
                                     (std::string(k) == "ed25519" ? " -md sha512" : "");
            ASSERT_EQ(sh(sign), 0) << what;
            auto theirs = read(pki.d / "theirs.der");
            auto t = detached ? cms::verify_detached(bytes(theirs), bytes(content), pki.options()) : cms::verify(bytes(theirs), pki.options());
            ASSERT_TRUE(t) << what << ": " << t.error().message().view();
            EXPECT_EQ(text(t->content), content) << what;
        }
    }
    // OpenSSL's RSASSA-PSS, and a signature without signed attributes
    struct Extra {
        const char* what;
        const char* options;
    };
    for (Extra e : {Extra{"RSASSA-PSS", "-keyopt rsa_padding_mode:pss"}, Extra{"no signed attributes", "-noattr"}}) {
        ASSERT_EQ(sh(openssl() + " cms -sign -binary -nodetach -outform DER -in " + pki.d / "content.txt" + " -signer " + pki.d / "rsa.pem" + " -inkey " + pki.d / "rsa.key" + " " + e.options +
                     " -out " + pki.d / "x.der"),
                  0)
            << e.what;
        auto v = cms::verify(bytes(read(pki.d / "x.der")), pki.options());
        ASSERT_TRUE(v) << e.what << ": " << v.error().message().view();
        EXPECT_EQ(text(v->content), content) << e.what;
    }
}

TEST(Cms_OpenSsl, EncryptionBothWays) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Pki pki;
    const std::string content(1000, 'c');
    std::ofstream(pki.d / "content.txt", std::ios::binary) << content;
    for (const char* k : {"rsa", "P-256", "P-384", "P-521"}) {
        auto cert = pki.certificate(k);
        auto key = pki.key(k);
        for (auto cipher : {cms::content_cipher::aes256_gcm, cms::content_cipher::aes256_cbc}) {
            const std::string what = std::string(k) + (cipher == cms::content_cipher::aes256_gcm ? " GCM" : " CBC");
            // ours, decrypted by OpenSSL
            x509::chain to;
            to.push_back(cert);
            auto env = cms::encrypt(bytes(content), to, {.cipher = cipher});
            write(pki.d / "ours.der", env);
            EXPECT_EQ(sh(openssl() + " cms -decrypt -binary -inform DER -in " + pki.d / "ours.der" + " -recip " + pki.d / (std::string(k) + ".pem") + " -inkey " + pki.d / (std::string(k) + ".key") +
                         " -out " + pki.d / "out.txt"),
                      0)
                << what << ": OpenSSL does not decrypt ours";
            EXPECT_EQ(read(pki.d / "out.txt"), content) << what;
            auto mine = cms::decrypt(env, cert, key.signing_key());
            ASSERT_TRUE(mine) << what << ": " << mine.error().message().view();
            EXPECT_EQ(text(*mine), content) << what;
        }
        // OpenSSL's, of every AES size, decrypted here
        for (const char* c : {"-aes-128-gcm", "-aes-256-gcm", "-aes-128-cbc", "-aes-192-cbc", "-aes-256-cbc"}) {
            const std::string what = std::string(k) + " " + c;
            const std::string rsa_opts = std::string(k) == "rsa" ? " -keyopt rsa_padding_mode:oaep -keyopt rsa_oaep_md:sha256" : "";
            ASSERT_EQ(sh(openssl() + " cms -encrypt -binary -outform DER " + c + " -in " + pki.d / "content.txt" + " -recip " + pki.d / (std::string(k) + ".pem") + rsa_opts + " -out " +
                         pki.d / "theirs.der"),
                      0)
                << what;
            auto mine = cms::decrypt(bytes(read(pki.d / "theirs.der")), cert, key.signing_key());
            ASSERT_TRUE(mine) << what << ": " << mine.error().message().view();
            EXPECT_EQ(text(*mine), content) << what;
        }
    }
    // OpenSSL's default RSA key transport, PKCS #1 v1.5: refused by name
    auto cert = pki.certificate("rsa");
    auto key = pki.key("rsa");
    ASSERT_EQ(sh(openssl() + " cms -encrypt -binary -outform DER -aes-256-cbc -in " + pki.d / "content.txt" + " -recip " + pki.d / "rsa.pem" + " -out " + pki.d / "v15.der"), 0);
    auto refused = cms::decrypt(bytes(read(pki.d / "v15.der")), cert, key.signing_key());
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), crypto::errc::unsupported);
    // two recipients: each decrypts
    x509::chain both;
    both.push_back(pki.certificate("rsa"));
    both.push_back(pki.certificate("P-384"));
    auto env = cms::encrypt(bytes(content), both);
    EXPECT_TRUE(cms::decrypt(env, pki.certificate("rsa"), pki.key("rsa").signing_key()));
    EXPECT_TRUE(cms::decrypt(env, pki.certificate("P-384"), pki.key("P-384").signing_key()));
    auto none = cms::decrypt(env, pki.certificate("P-256"), pki.key("P-256").signing_key());
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), crypto::errc::invalid_key);
}

TEST(Cms_OpenSsl, SmimeBothWays) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Pki pki;
    const std::string entity = "Content-Type: text/plain; charset=utf-8\r\n\r\nHello,\r\nsigned and sealed.\r\n";
    std::ofstream(pki.d / "entity.txt", std::ios::binary) << entity;
    for (const char* k : {"rsa", "P-256", "ed25519"}) {
        auto cert = pki.certificate(k);
        auto key = pki.key(k);
        // ours signed, verified by OpenSSL (its cms: its smime speaks PKCS #7, without Ed25519)
        auto signed_ = crypto::smime::sign(bytes(entity), cert, key.signing_key());
        write(pki.d / "signed.eml", signed_);
        EXPECT_EQ(sh(openssl() + " cms -verify -in " + pki.d / "signed.eml" + " -CAfile " + pki.d / "ca.pem" + " -purpose smimesign -out " + pki.d / "out.txt"), 0) << k;
        auto v = crypto::smime::verify(signed_, pki.options());
        ASSERT_TRUE(v) << k << ": " << v.error().message().view();
        EXPECT_EQ(text(v->content), entity) << k;
        // OpenSSL's signed (multipart and opaque), verified here
        for (const char* mode : {"", " -nodetach"}) {
            ASSERT_EQ(sh(openssl() + " cms -sign -in " + pki.d / "entity.txt" + " -signer " + pki.d / (std::string(k) + ".pem") + " -inkey " + pki.d / (std::string(k) + ".key") + mode +
                         (std::string(k) == "ed25519" ? " -md sha512" : "") + " -out " + pki.d / "theirs.eml"),
                      0)
                << k << mode;
            auto t = crypto::smime::verify(bytes(read(pki.d / "theirs.eml")), pki.options());
            ASSERT_TRUE(t) << k << mode << ": " << t.error().message().view();
            EXPECT_EQ(text(t->content), entity) << k << mode;
        }
        // a changed body: refused
        auto bad = signed_;
        const std::string sv = text(bad);
        bad[sv.find("Hello")] = byte('J');
        EXPECT_FALSE(crypto::smime::verify(bad, pki.options())) << k;
    }
    for (const char* k : {"rsa", "P-384"}) {
        auto cert = pki.certificate(k);
        auto key = pki.key(k);
        x509::chain to;
        to.push_back(cert);
        // ours sealed, opened by OpenSSL
        auto sealed = crypto::smime::encrypt(bytes(entity), to);
        write(pki.d / "sealed.eml", sealed);
        EXPECT_EQ(sh(openssl() + " cms -decrypt -in " + pki.d / "sealed.eml" + " -recip " + pki.d / (std::string(k) + ".pem") + " -inkey " + pki.d / (std::string(k) + ".key") + " -out " +
                     pki.d / "out.txt"),
                  0)
            << k;
        EXPECT_EQ(read(pki.d / "out.txt"), entity) << k;
        auto back = crypto::smime::decrypt(sealed, cert, key.signing_key());
        ASSERT_TRUE(back) << k << ": " << back.error().message().view();
        EXPECT_EQ(text(*back), entity) << k;
        // OpenSSL's sealed (GCM), opened here
        const std::string rsa_opts = std::string(k) == "rsa" ? " -keyopt rsa_padding_mode:oaep -keyopt rsa_oaep_md:sha256" : "";
        ASSERT_EQ(sh(openssl() + " cms -encrypt -aes-256-gcm -in " + pki.d / "entity.txt" + " -recip " + pki.d / (std::string(k) + ".pem") + rsa_opts + " -out " + pki.d / "theirs.eml"), 0) << k;
        auto theirs = crypto::smime::decrypt(bytes(read(pki.d / "theirs.eml")), cert, key.signing_key());
        ASSERT_TRUE(theirs) << k << ": " << theirs.error().message().view();
        EXPECT_EQ(text(*theirs), entity) << k;
    }
    // what is neither signed nor sealed
    EXPECT_FALSE(crypto::smime::verify(bytes(entity), pki.options()));
    EXPECT_FALSE(crypto::smime::decrypt(bytes(entity), pki.certificate("rsa"), pki.key("rsa").signing_key()));
    EXPECT_FALSE(crypto::smime::verify(slice<const byte>(), pki.options()));
}

TEST(Cms, Boundaries) {
    auto ca_key = crypto::p256::private_key::generate();
    x509::certificate_template ct;
    ct.common_name = "ca";
    ct.is_ca = true;
    auto ca = x509::create_certificate(ct, ca_key);
    auto key = crypto::p256::private_key::generate();
    x509::certificate_template lt;
    lt.common_name = "leaf";
    lt.ext_key_usages = {x509::ext_key_usage::email_protection};
    lt.key_usage = x509::key_usage(uint16_t(x509::key_usage::digital_signature) | uint16_t(x509::key_usage::key_agreement));
    auto leaf = x509::create_certificate(lt, key.public_key().to_pkix_der(), ca, ca_key);
    x509::certificate_pool roots;
    roots.add(ca);
    cms::verify_options vo;
    vo.chain.roots = roots;
    // empty content; a megabyte
    for (size_t n : {size_t(0), size_t(1) << 20}) {
        std::string c(n, 'x');
        auto sd = cms::sign(bytes(c), leaf, key);
        auto v = cms::verify(sd, vo);
        ASSERT_TRUE(v) << n << ": " << v.error().message().view();
        EXPECT_EQ(v->content.size(), n);
    }
    // the signing time given is the one read
    auto when = time::datetime::from_unix(1700000000, time::zone::utc());
    auto sd = cms::sign(bytes(std::string("m")), leaf, key, {.signing_time = when});
    auto v = cms::verify(sd, vo);
    ASSERT_TRUE(v) << v.error().message().view();
    EXPECT_EQ(v->signing_time->unix(), 1700000000);
    // a changed byte anywhere: refused, or (in what no signature covers: the
    // digestAlgorithms list, a version, the certificates' order) the same content
    size_t refused = 0;
    for (size_t i = 0; i < sd.size(); i += 3) {
        auto bad = sd;
        bad[i] = byte(uint8_t(bad[i]) ^ 0x01);
        auto r = cms::verify(bad, vo);
        if (r) {
            EXPECT_EQ(text(r->content), "m") << i;
        } else {
            ++refused;
        }
    }
    EXPECT_GT(refused, sd.size() / 3 * 9 / 10);
    // detached: the content needed, the other content refused; the embedded refuses a detached content
    auto det = cms::sign(bytes(std::string("detached")), leaf, key, {.detached = true});
    EXPECT_FALSE(cms::verify(det, vo));
    EXPECT_TRUE(cms::verify_detached(det, bytes(std::string("detached")), vo));
    EXPECT_FALSE(cms::verify_detached(det, bytes(std::string("Detached")), vo));
    EXPECT_FALSE(cms::verify_detached(sd, bytes(std::string("m")), vo));
    // without the certificates: the signer must be given
    auto bare = cms::sign(bytes(std::string("m")), leaf, key, {.include_certificates = false});
    auto nobody = cms::verify(bare, vo);
    ASSERT_FALSE(nobody);
    EXPECT_EQ(nobody.error().code(), crypto::errc::verification);
    cms::verify_options with = vo;
    with.certificates.push_back(leaf);
    EXPECT_TRUE(cms::verify(bare, with));
    // a chain to other roots, a signature of another key's certificate
    EXPECT_FALSE(cms::verify(sd, cms::verify_options{.chain = {.roots = x509::certificate_pool()}}));
    auto other = crypto::p256::private_key::generate();
    EXPECT_THROW((void)cms::sign(bytes(std::string("m")), leaf, other), std::invalid_argument);
    // encryption: no recipient, an Ed25519 recipient; a round trip; the wrong key
    EXPECT_THROW((void)cms::encrypt(bytes(std::string("m")), x509::chain()), std::invalid_argument);
    auto ed = crypto::ed25519::private_key::generate();
    x509::chain eds;
    eds.push_back(x509::create_certificate(lt, ed));
    EXPECT_THROW((void)cms::encrypt(bytes(std::string("m")), eds), std::invalid_argument);
    x509::chain to;
    to.push_back(leaf);
    for (auto cipher : {cms::content_cipher::aes256_gcm, cms::content_cipher::aes256_cbc}) {
        auto env = cms::encrypt(bytes(std::string("secret")), to, {.cipher = cipher});
        auto back = cms::decrypt(env, leaf, key);
        ASSERT_TRUE(back) << back.error().message().view();
        EXPECT_EQ(text(*back), "secret");
        EXPECT_FALSE(cms::decrypt(env, leaf, other));
        for (size_t i = 0; i < env.size(); i += 7) {
            auto bad = env;
            bad[i] = byte(uint8_t(bad[i]) ^ 0x01);
            auto r = cms::decrypt(bad, leaf, key);
            if (r && cipher == cms::content_cipher::aes256_gcm) {
                EXPECT_EQ(text(*r), "secret") << i;   // a change outside what is authenticated (a tag's class)
            }
        }
    }
    // nothing to read
    EXPECT_FALSE(cms::verify(slice<const byte>(), vo));
    EXPECT_FALSE(cms::decrypt(slice<const byte>(), leaf, key));
}
