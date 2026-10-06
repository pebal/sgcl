//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PKCS #12 (RFC 7292) against OpenSSL 3.6, both ways: files OpenSSL writes
// for every kind of key the module has (P-256, P-384, P-521, Ed25519, RSA),
// with a chain, under its default PBES2 and MAC and under the others it
// offers (AES-128/192-CBC, MACs of SHA-1 and SHA-512, PBMAC1, no MAC, no
// encryption, a UTF-8 password, an empty one), read here to the same key
// and certificates; files written here read by OpenSSL to the same key and
// chain; the legacy PBEs refused by name; a tls::identity made of a file;
// and the contract at its boundaries.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/crypto.h"
#include "sgcl/net/tls.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace sgcl;
namespace x509 = sgcl::crypto::x509;

namespace {
    std::string openssl() {
        if (const char* e = std::getenv("SGCL_OPENSSL")) {
            return e;
        }
        const char* p = "/opt/homebrew/opt/openssl@3/bin/openssl";
        return ::access(p, X_OK) == 0 ? p : "";
    }

    struct Dir {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("sgcl_p12_" + std::to_string(::getpid()));

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

    slice<const byte> bytes(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    std::string text(const slice<const byte>& b) {
        return std::string(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // A key of the kind, a CA of P-256 and a leaf of the key signed by it
    struct Material {
        Dir& d;
        std::string kind;

        Material(Dir& dir, const std::string& k) : d(dir), kind(k) {
            const std::string o = openssl();
            const std::string gen = k == "rsa" ? "-algorithm RSA -pkeyopt rsa_keygen_bits:2048"
                                  : k == "ed25519" ? "-algorithm ED25519"
                                                   : "-algorithm EC -pkeyopt ec_paramgen_curve:" + k;
            sh(o + " genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out " + d / "ca.key");
            sh(o + " req -x509 -new -key " + d / "ca.key" + " -subj /CN=p12-ca -days 30 -out " + d / "ca.pem");
            sh(o + " genpkey " + gen + " -out " + d / (k + ".key"));
            sh(o + " req -new -key " + d / (k + ".key") + " -subj /CN=p12-leaf -out " + d / (k + ".csr"));
            sh(o + " x509 -req -in " + d / (k + ".csr") + " -CA " + d / "ca.pem" + " -CAkey " + d / "ca.key" + " -days 30 -out " + d / (k + ".pem"));
            sh(o + " pkey -in " + d / (k + ".key") + " -pubout -outform DER -out " + d / (k + ".pub"));
            sh(o + " x509 -in " + d / (k + ".pem") + " -outform DER -out " + d / (k + ".der"));
            sh(o + " x509 -in " + d / "ca.pem" + " -outform DER -out " + d / "ca.der");
        }

        std::string key() const {
            return d / (kind + ".key");
        }

        std::string leaf() const {
            return d / (kind + ".pem");
        }
    };

    // The file read here is the key and the chain OpenSSL was given
    void check_contents(const crypto::pkcs12& p, const Material& m, const std::string& what) {
        EXPECT_NE(p.key_kind(), x509::key_kind::none) << what;
        auto pub = p.signing_key().public_key_der();
        EXPECT_EQ(text(pub), read(m.d / (m.kind + ".pub"))) << what;
        ASSERT_EQ(p.certificates().size(), 2u) << what;
        EXPECT_EQ(text(p.certificates()[0].raw()), read(m.d / (m.kind + ".der"))) << what << ": the leaf first";
        EXPECT_EQ(text(p.certificates()[1].raw()), read(m.d / "ca.der")) << what;
    }
}

TEST(Pkcs12_OpenSsl, FilesOfEveryKindRead) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Dir d;
    for (const char* kind : {"P-256", "P-384", "P-521", "ed25519", "rsa"}) {
        Material m(d, kind);
        const std::string out = d / (std::string(kind) + ".p12");
        ASSERT_EQ(sh(openssl() + " pkcs12 -export -in " + m.leaf() + " -inkey " + m.key() + " -certfile " + d / "ca.pem" + " -name leaf-name -out " + out +
                     " -passout pass:secret"),
                  0)
            << kind;
        auto p = crypto::pkcs12::parse(bytes(read(out)), "secret");
        ASSERT_TRUE(p) << kind << ": " << p.error().message().view();
        check_contents(*p, m, kind);
        EXPECT_EQ(std::string(p->friendly_name().view()), "leaf-name") << kind;
        // the typed key in one line, and the same key as the file's PKCS #8
        auto pkcs8 = p->key_pkcs8();
        EXPECT_TRUE(crypto::jose::jwk::from_pem(crypto::detail::write_key_pem("PRIVATE KEY", pkcs8))) << kind;
        // a wrong password: the MAC refuses it
        auto wrong = crypto::pkcs12::parse(bytes(read(out)), "Secret");
        ASSERT_FALSE(wrong) << kind;
        EXPECT_EQ(wrong.error().code(), crypto::errc::authentication) << kind;
    }
}

TEST(Pkcs12_OpenSsl, TheOtherAlgorithmsOpenSslWrites) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Dir d;
    Material m(d, "P-256");
    struct Case {
        const char* what;
        const char* options;
        const char* password;
    };
    const Case cases[] = {
        {"AES-128 key, AES-192 certificates", "-keypbe AES-128-CBC -certpbe AES-192-CBC", "pw"},
        {"a MAC of SHA-1", "-macalg sha1", "pw"},
        {"a MAC of SHA-512, 10000 iterations", "-macalg sha512 -iter 10000 -maciter", "pw"},
        {"PBMAC1 (RFC 9579)", "-pbmac1_pbkdf2", "pw"},
        {"no MAC", "-nomac", "pw"},
        {"no encryption", "-keypbe NONE -certpbe NONE", "pw"},
        {"a UTF-8 password", "", "za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 \xf0\x9f\x94\x91"},
        {"an empty password", "", ""},
    };
    for (const auto& c : cases) {
        const std::string out = d / "o.p12";
        std::filesystem::remove(out);
        const int rc = sh(openssl() + " pkcs12 -export " + c.options + " -in " + m.leaf() + " -inkey " + m.key() + " -certfile " + d / "ca.pem" + " -out " +
                          out + " -passout 'pass:" + c.password + "'");
        if (rc != 0) {
            std::printf("[ pkcs12 ] OpenSSL does not write: %s\n", c.what);
            continue;
        }
        auto p = crypto::pkcs12::parse(bytes(read(out)), bytes(c.password));
        ASSERT_TRUE(p) << c.what << ": " << p.error().message().view();
        check_contents(*p, m, c.what);
    }
    // the legacy PBEs (3DES, RC2): refused by name
    const std::string legacy = d / "legacy.p12";
    if (sh(openssl() + " pkcs12 -export -legacy -in " + m.leaf() + " -inkey " + m.key() + " -out " + legacy + " -passout pass:pw") == 0) {
        auto p = crypto::pkcs12::parse(bytes(read(legacy)), "pw");
        ASSERT_FALSE(p);
        EXPECT_EQ(p.error().code(), crypto::errc::unsupported);
        EXPECT_NE(std::string(p.error().message().view()).find("legacy"), std::string::npos) << p.error().message().view();
    }
    // certificates alone: no key
    const std::string certs = d / "certs.p12";
    ASSERT_EQ(sh(openssl() + " pkcs12 -export -nokeys -in " + m.leaf() + " -certfile " + d / "ca.pem" + " -out " + certs + " -passout pass:pw"), 0);
    auto p = crypto::pkcs12::parse(bytes(read(certs)), "pw");
    ASSERT_TRUE(p) << p.error().message().view();
    EXPECT_EQ(p->key_kind(), x509::key_kind::none);
    EXPECT_EQ(p->certificates().size(), 2u);
    EXPECT_THROW((void)p->signing_key(), std::logic_error);
    EXPECT_THROW((void)p->key_pkcs8(), std::logic_error);
    EXPECT_FALSE(net::tls::identity::from_pkcs12(*p));
}

TEST(Pkcs12_OpenSsl, OursReadByOpenSsl) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Dir d;
    for (const char* kind : {"P-256", "P-384", "P-521", "ed25519", "rsa"}) {
        Material m(d, kind);
        const std::string theirs = d / "t.p12";
        ASSERT_EQ(sh(openssl() + " pkcs12 -export -in " + m.leaf() + " -inkey " + m.key() + " -certfile " + d / "ca.pem" + " -out " + theirs + " -passout pass:a"), 0);
        auto read_back = crypto::pkcs12::parse(bytes(read(theirs)), "a");
        ASSERT_TRUE(read_back) << kind;
        // ours, of the key and the chain, under another password and a name
        auto ours = crypto::pkcs12::encode(read_back->signing_key(), read_back->certificates(), "an other one", {.friendly_name = "zażółć"});
        const std::string file = d / "ours.p12";
        std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(ours.data()), std::streamsize(ours.size()));
        ASSERT_EQ(sh(openssl() + " pkcs12 -in " + file + " -passin 'pass:an other one' -nocerts -nodes -out " + d / "k.pem"), 0) << kind;
        ASSERT_EQ(sh(openssl() + " pkey -in " + d / "k.pem" + " -pubout -outform DER -out " + d / "k.pub"), 0) << kind;
        EXPECT_EQ(read(d / "k.pub"), read(d / (std::string(kind) + ".pub"))) << kind;
        ASSERT_EQ(sh(openssl() + " pkcs12 -in " + file + " -passin 'pass:an other one' -nokeys -clcerts -out " + d / "leaf.pem"), 0) << kind;
        ASSERT_EQ(sh(openssl() + " x509 -in " + d / "leaf.pem" + " -outform DER -out " + d / "leaf.der"), 0) << kind;
        EXPECT_EQ(read(d / "leaf.der"), read(d / (std::string(kind) + ".der"))) << kind << ": the leaf, tied to the key";
        ASSERT_EQ(sh(openssl() + " pkcs12 -in " + file + " -passin 'pass:an other one' -nokeys -cacerts -out " + d / "ca2.pem"), 0) << kind;
        ASSERT_EQ(sh(openssl() + " x509 -in " + d / "ca2.pem" + " -outform DER -out " + d / "ca2.der"), 0) << kind;
        EXPECT_EQ(read(d / "ca2.der"), read(d / "ca.der")) << kind;
        // the wrong password: OpenSSL's MAC check refuses it too
        EXPECT_NE(sh(openssl() + " pkcs12 -in " + file + " -passin pass:wrong -nokeys -out " + d / "x.pem"), 0) << kind;
        // and ours read here
        auto again = crypto::pkcs12::parse(ours, "an other one");
        ASSERT_TRUE(again) << kind;
        check_contents(*again, m, kind);
        EXPECT_EQ(std::string(again->friendly_name().view()), "zażółć");
    }
}

TEST(Pkcs12, ATlsIdentityOfAFile) {
    if (openssl().empty()) {
        GTEST_SKIP() << "no OpenSSL";
    }
    Dir d;
    Material m(d, "P-256");
    const std::string file = d / "id.p12";
    ASSERT_EQ(sh(openssl() + " pkcs12 -export -in " + m.leaf() + " -inkey " + m.key() + " -certfile " + d / "ca.pem" + " -out " + file + " -passout pass:pw"), 0);
    auto id = net::tls::identity::from_pkcs12(bytes(read(file)), "pw");
    ASSERT_TRUE(id) << id.error().message().view();
    EXPECT_EQ(id->certificates().size(), 2u);
    auto wrong = net::tls::identity::from_pkcs12(bytes(read(file)), "nope");
    ASSERT_FALSE(wrong);
    EXPECT_TRUE(wrong.error().code() == crypto::errc::authentication);
    // the throwing constructor of a file read
    auto p = crypto::pkcs12::parse(bytes(read(file)), "pw");
    net::tls::identity same(*p);
    EXPECT_EQ(same.certificates().size(), 2u);
}

TEST(Pkcs12, Boundaries) {
    auto key = crypto::p256::private_key::generate();
    auto other = crypto::ed25519::private_key::generate();
    x509::certificate_template t;
    t.common_name = "leaf";
    auto leaf = x509::create_certificate(t, key);
    x509::chain chain;
    chain.push_back(leaf);
    // a round trip: the key, the chain, the name, the kind
    auto file = crypto::pkcs12::encode(key, chain, "pw", {.friendly_name = "n", .iterations = 100});
    auto p = crypto::pkcs12::parse(file, "pw");
    ASSERT_TRUE(p) << p.error().message().view();
    EXPECT_EQ(p->key_kind(), x509::key_kind::p256);
    EXPECT_EQ(std::string(p->friendly_name().view()), "n");
    EXPECT_TRUE(crypto::p256::private_key::from_pkcs8_der(p->key_pkcs8())->public_key() == key.public_key());
    // a copy shares the key; the view of the key lives with the handle
    auto copy = *p;
    EXPECT_EQ(copy.signing_key().public_key_der().size(), p->signing_key().public_key_der().size());
    // no chain: the key alone; an empty password
    auto bare = crypto::pkcs12::encode(other, x509::chain(), "");
    auto b = crypto::pkcs12::parse(bare, "");
    ASSERT_TRUE(b) << b.error().message().view();
    EXPECT_EQ(b->key_kind(), x509::key_kind::ed25519);
    EXPECT_TRUE(b->certificates().empty());
    EXPECT_FALSE(net::tls::identity::from_pkcs12(*b));
    // a leaf of another key, iterations of 0: the program's mistakes
    EXPECT_THROW((void)crypto::pkcs12::encode(other, chain, "pw"), std::invalid_argument);
    EXPECT_THROW((void)crypto::pkcs12::encode(key, chain, "pw", {.iterations = 0}), std::invalid_argument);
    // max_iterations: a file past it is refused before any derivation
    auto many = crypto::pkcs12::encode(key, chain, "pw", {.iterations = 5000});
    auto refused = crypto::pkcs12::parse(many, "pw", {.max_iterations = 4999});
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), crypto::errc::unsupported);
    EXPECT_TRUE(crypto::pkcs12::parse(many, "pw", {.max_iterations = 5000}));
    // nothing, a byte cut off anywhere, a byte changed anywhere: refused, never read
    EXPECT_FALSE(crypto::pkcs12::parse(slice<const byte>(), "pw"));
    for (size_t n = 0; n < file.size(); n += 37) {
        EXPECT_FALSE(crypto::pkcs12::parse(slice<const byte>(file.data(), n), "pw")) << n;
    }
    for (size_t i = 0; i < file.size(); i += 29) {
        auto bad = file;
        bad[i] = byte(uint8_t(bad[i]) ^ 0x20);
        auto r = crypto::pkcs12::parse(bad, "pw");
        if (r) {
            // a change in a tag's class bit the parse reads alike: the contents are still the file's
            EXPECT_EQ(r->certificates().size(), 1u) << i;
        }
    }
    // the password is the bytes given: one of other bytes is refused
    const char pw0[3] = {'p', 'w', 0};
    EXPECT_FALSE(crypto::pkcs12::parse(file, slice<const byte>(reinterpret_cast<const byte*>(pw0), 3)));
    EXPECT_FALSE(crypto::pkcs12::parse(file, "Pw"));
}
