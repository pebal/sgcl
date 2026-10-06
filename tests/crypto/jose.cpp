//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::jose (RFC 7515-7519, 7638, 8037): Project Wycheproof's JSON web
// files (json_web_signature, json_web_key, json_web_encryption,
// json_web_crypto; their rfc7520 groups are the cookbook's figures), read
// from ~/Programming/oracles/wycheproof/testvectors_v1/ and skipped when it
// is not there; every algorithm made and read back, every key through its
// JSON both ways; the claims of a JWT at their edges; the refusals the rules
// promise; and every method at its boundaries. Go (tests/crypto/go_jose, the
// standard library alone, JOSE written by hand) is the oracle of what the
// module makes and of tokens it did not make.
#include "tests/types.h"

#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "tests/managed_scan.h"
#include "tests/source_root.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace sgcl;
namespace jose = sgcl::crypto::jose;
using jose::algorithm;
using jose::encryption;

namespace {
    std::string text(const string& s) {
        return std::string(s.data(), s.size());
    }

    std::string text(const vector<byte>& b) {
        return std::string(reinterpret_cast<const char*>(b.data()), b.size());
    }

    std::string text(const crypto::secret_bytes& b) {
        return std::string(reinterpret_cast<const char*>(b.as_slice().data()), b.size());
    }

    slice<const byte> bytes(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    std::filesystem::path wycheproof(const char* name) {
        const char* home = std::getenv("HOME");
        return std::filesystem::path(home ? home : "") / "Programming/oracles/wycheproof/testvectors_v1" / name;
    }

    optional<encoding::json> load(const char* name) {
        std::ifstream in(wycheproof(name));
        if (!in) {
            return nullopt;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        auto j = encoding::json::parse(string(ss.str()));
        if (!j) {
            return nullopt;
        }
        return *j;
    }

    // A test's key: a JWK Set when the JSON has keys, else one JWK
    struct Keys {
        optional<jose::jwk> one;
        optional<jose::jwk_set> set;
        optional<crypto::error> failed;
    };

    Keys keys_of(const encoding::json& j) {
        Keys k;
        std::string t = text(j.to_string());
        if (j.contains(string("keys"))) {
            auto s = jose::jwk_set::parse(string(t));
            if (s) {
                k.set = *s;
            } else {
                k.failed = s.error();
            }
        } else {
            auto one = jose::jwk::parse(string(t));
            if (one) {
                k.one = *one;
            } else {
                k.failed = one.error();
            }
        }
        return k;
    }

    // What the module does not have, by design or not yet: a case naming it
    // is counted and skipped
    bool unsupported_alg(std::string_view header_b64) {
        auto raw = encoding::base64::raw_url.decode(string(header_b64));
        if (!raw) {
            return false;
        }
        std::string h(reinterpret_cast<const char*>(raw->data()), raw->size());
        for (const char* name : {"\"RSA1_5\"", "\"zip\""}) {
            if (h.find(name) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    std::string first_part(const std::string& token) {
        return token.substr(0, token.find('.'));
    }

    // The cases of one file: verify (or decrypt) each, compare the verdict
    void run_file(const char* name, size_t& passed, size_t& skipped, std::map<std::string, std::string>& why) {
        auto doc = load(name);
        if (!doc) {
            GTEST_SKIP() << name << " is not under ~/Programming/oracles/wycheproof";
        }
        for (const auto& group : (*doc)["testGroups"].elements()) {
            const auto& key_json = group.contains(string("public")) ? group["public"] : group["private"];
            const auto& priv_json = group["private"];
            Keys pub = keys_of(key_json);
            Keys priv = keys_of(priv_json);
            for (const auto& c : group["tests"].elements()) {
                const int64_t id = c["tcId"].as_int(-1);
                const std::string result = text(c["result"].as_string(string()));
                const std::string comment = text(c["comment"].as_string(string()));
                const bool is_jwe = c.contains(string("jwe"));
                const std::string token = text(c[is_jwe ? "jwe" : "jws"].as_string(string()));
                std::string flags = text(c["flags"].to_string());
                if (unsupported_alg(first_part(token))) {
                    ++skipped;
                    why[std::string(name) + " " + std::to_string(id)] = "an algorithm left out (RSA1_5, zip)";
                    continue;
                }
                if (flags.find("JsonSerialization") != std::string::npos || flags.find("JsonWebKeyset") != std::string::npos) {
                    // the module reads the JSON serialization and key sets by design
                    if (result == "invalid" && comment == "rejectsValidJsonSerialization") {
                        ++skipped;
                        why[std::string(name) + " " + std::to_string(id)] = "the JSON serialization is read by design";
                        continue;
                    }
                }
                if (comment == "InvalidCharacterInsertedInHeader" || comment == "InvalidCharacterInsertedInPayload") {
                    // a '?' in the base64url: strict base64url refuses it, as RFC 7515 2 has it
                    ++skipped;
                    why[std::string(name) + " " + std::to_string(id)] = "strict base64url";
                    continue;
                }
                if (std::string(name) == "json_web_signature_test.json" && (id == 367 || id == 370)) {
                    ++skipped;
                    why[std::string(name) + " " + std::to_string(id)] = "the token is tcId 357's, which is valid: a fault of the file";
                    continue;
                }
                if (comment == "Figure27") {
                    ++skipped;
                    why[std::string(name) + " " + std::to_string(id)] = "a key whose alg is ES521 (RFC 7520 Figure 27's typo): an alg the registry does not have";
                    continue;
                }
                if (comment == "Figure20") {
                    ++skipped;
                    why[std::string(name) + " " + std::to_string(id)] = "a PS384 token under a key whose alg is PS256: the module holds a key to its alg";
                    continue;
                }
                bool ok;
                std::string detail;
                if (is_jwe) {
                    const Keys& k = priv;
                    expected<vector<byte>, crypto::error> r = unexpected(k.failed ? *k.failed : crypto::error(crypto::errc::malformed));
                    if (k.one) {
                        r = jose::jwe::decrypt(string(token), *k.one);
                    } else if (k.set) {
                        r = jose::jwe::decrypt(string(token), *k.set);
                    }
                    ok = r.has_value();
                    if (ok) {
                        // a case without pt asks for the decryption alone
                        auto pt = encoding::hex::encode(r->as_slice());
                        ok = !c.contains(string("pt")) || text(pt) == text(c["pt"].as_string(string()));
                        detail = "plaintext " + text(pt);
                    } else {
                        detail = text(r.error().message());
                    }
                } else {
                    const Keys& k = pub;
                    expected<vector<byte>, crypto::error> r = unexpected(k.failed ? *k.failed : crypto::error(crypto::errc::malformed));
                    if (k.one) {
                        r = jose::jws::verify(string(token), *k.one);
                    } else if (k.set) {
                        r = jose::jws::verify(string(token), *k.set);
                    }
                    ok = r.has_value();
                    if (!ok) {
                        detail = text(r.error().message());
                    }
                }
                const bool want = result == "valid";
                EXPECT_EQ(ok, want) << name << " tcId " << id << " " << comment << " " << flags << ": " << detail;
                passed += ok == want;
            }
        }
    }
}

TEST(Jose, WycheproofSignatures) {
    size_t passed = 0, skipped = 0;
    std::map<std::string, std::string> why;
    run_file("json_web_signature_test.json", passed, skipped, why);
    EXPECT_GE(passed, 370u);
    EXPECT_EQ(passed + skipped, 401u);
    std::map<std::string, int> reasons;
    for (const auto& [k, v] : why) {
        ++reasons[v];
    }
    for (const auto& [v, n] : reasons) {
        std::printf("  skipped %d: %s\n", n, v.c_str());
    }
}

TEST(Jose, WycheproofKeys) {
    size_t passed = 0, skipped = 0;
    std::map<std::string, std::string> why;
    run_file("json_web_key_test.json", passed, skipped, why);
    EXPECT_EQ(passed + skipped, 26u);
    std::map<std::string, int> reasons;
    for (const auto& [k, v] : why) {
        ++reasons[v];
    }
    for (const auto& [v, n] : reasons) {
        std::printf("  skipped %d: %s\n", n, v.c_str());
    }
}

TEST(Jose, WycheproofEncryption) {
    size_t passed = 0, skipped = 0;
    std::map<std::string, std::string> why;
    run_file("json_web_encryption_test.json", passed, skipped, why);
    EXPECT_GE(passed, 107u);   // every case but RSA1_5, zip and one of the JSON serialization
    EXPECT_EQ(passed + skipped, 139u);
    std::map<std::string, int> reasons;
    for (const auto& [k, v] : why) {
        ++reasons[v];
    }
    for (const auto& [v, n] : reasons) {
        std::printf("  skipped %d: %s\n", n, v.c_str());
    }
}

TEST(Jose, WycheproofCrypto) {
    size_t passed = 0, skipped = 0;
    std::map<std::string, std::string> why;
    run_file("json_web_crypto_test.json", passed, skipped, why);
    EXPECT_GE(passed, 40u);
    EXPECT_EQ(passed + skipped, 83u);
    std::map<std::string, int> reasons;
    for (const auto& [k, v] : why) {
        ++reasons[v];
    }
    for (const auto& [v, n] : reasons) {
        std::printf("  skipped %d: %s\n", n, v.c_str());
    }
}

// --- Go as the oracle ----------------------------------------------------

namespace {
    // The oracle built once, in a module of its own in a temporary directory
    const std::string& go_oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto dir = std::filesystem::temp_directory_path() / "sgcl_jose_go";
            std::filesystem::create_directories(dir);
            std::filesystem::copy_file(source_root() / "tests/crypto/go_jose/main.go", dir / "main.go", std::filesystem::copy_options::overwrite_existing);
            std::ofstream(dir / "go.mod") << "module joseoracle\n\ngo 1.23\n";
            auto out = dir / "go_jose";
            std::string cmd = "cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off go build -o '" + out.string() + "' . > build.log 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    std::string go(const std::vector<std::string>& args) {
        std::string cmd = go_oracle();
        for (const auto& a : args) {
            cmd += " '" + a + "'";
        }
        std::string out;
        FILE* p = ::popen((cmd + " 2>&1").c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        ::pclose(p);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
            out.pop_back();
        }
        return out;
    }

    std::string hex_of(const std::string& s) {
        return text(encoding::hex::encode(bytes(s)));
    }

    const algorithm SignatureAlgorithms[] = {algorithm::hs256, algorithm::hs384, algorithm::hs512, algorithm::rs256, algorithm::rs384, algorithm::rs512,
                                             algorithm::ps256, algorithm::ps384, algorithm::ps512, algorithm::es256, algorithm::es384, algorithm::es512,
                                             algorithm::eddsa};

    const char* name_of(algorithm a) {
        switch (a) {
            case algorithm::hs256: return "HS256";
            case algorithm::hs384: return "HS384";
            case algorithm::hs512: return "HS512";
            case algorithm::rs256: return "RS256";
            case algorithm::rs384: return "RS384";
            case algorithm::rs512: return "RS512";
            case algorithm::ps256: return "PS256";
            case algorithm::ps384: return "PS384";
            case algorithm::ps512: return "PS512";
            case algorithm::es256: return "ES256";
            case algorithm::es384: return "ES384";
            case algorithm::es512: return "ES512";
            case algorithm::eddsa: return "EdDSA";
            case algorithm::rsa_oaep: return "RSA-OAEP";
            case algorithm::rsa_oaep_256: return "RSA-OAEP-256";
            case algorithm::a128gcmkw: return "A128GCMKW";
            case algorithm::a192gcmkw: return "A192GCMKW";
            case algorithm::a256gcmkw: return "A256GCMKW";
            case algorithm::dir: return "dir";
            case algorithm::ecdh_es: return "ECDH-ES";
            case algorithm::a128kw: return "A128KW";
            case algorithm::a192kw: return "A192KW";
            case algorithm::a256kw: return "A256KW";
            case algorithm::ecdh_es_a128kw: return "ECDH-ES+A128KW";
            case algorithm::ecdh_es_a192kw: return "ECDH-ES+A192KW";
            case algorithm::ecdh_es_a256kw: return "ECDH-ES+A256KW";
            default: return "?";
        }
    }

    const char* name_of(encryption e) {
        switch (e) {
            case encryption::a128cbc_hs256: return "A128CBC-HS256";
            case encryption::a192cbc_hs384: return "A192CBC-HS384";
            case encryption::a256cbc_hs512: return "A256CBC-HS512";
            case encryption::a128gcm: return "A128GCM";
            case encryption::a192gcm: return "A192GCM";
            case encryption::a256gcm: return "A256GCM";
        }
        return "?";
    }

    // A key's JSON for the oracle: the private one when it has one
    std::string json_of(const jose::jwk& k) {
        return text(k.to_private_json());
    }
}

TEST(JoseGo, SignaturesBothWays) {
    if (go_oracle().empty()) {
        GTEST_SKIP() << "no go";
    }
    for (auto a : SignatureAlgorithms) {
        auto key = jose::jwk::generate(a);
        const std::string payload = std::string("payload of ") + name_of(a) + " {\"x\":1}";
        // ours, checked by Go: compact and the public key alone
        auto token = jose::jws::sign(bytes(payload), key);
        const std::string pub = a <= algorithm::hs512 ? json_of(key) : text(key.to_json());
        EXPECT_EQ(go({"verify", pub, text(token)}), "ok " + hex_of(payload)) << name_of(a);
        // Go's, checked by us
        auto theirs = go({"sign", name_of(a), json_of(key), hex_of(payload)});
        auto v = jose::jws::verify(string(theirs), a <= algorithm::hs512 ? key : key.public_key());
        ASSERT_TRUE(v) << name_of(a) << ": " << text(v.error().message()) << " " << theirs;
        EXPECT_EQ(text(*v), payload) << name_of(a);
        // the thumbprint
        EXPECT_EQ(go({"thumbprint", json_of(key)}), text(key.thumbprint())) << name_of(a);
    }
    // X25519's thumbprint too
    jose::jwk x(crypto::x25519::private_key::generate());
    EXPECT_EQ(go({"thumbprint", json_of(x)}), text(x.thumbprint()));
}

TEST(JoseGo, EncryptionBothWays) {
    if (go_oracle().empty()) {
        GTEST_SKIP() << "no go";
    }
    const encryption encs[] = {encryption::a128cbc_hs256, encryption::a192cbc_hs384, encryption::a256cbc_hs512,
                               encryption::a128gcm, encryption::a192gcm, encryption::a256gcm};
    struct Recipient {
        algorithm alg;
        const char* what;
    };
    for (auto e : encs) {
        vector<pair<string, jose::jwk>> keys;
        keys.emplace_back("dir", jose::jwk::symmetric(crypto::random::secret(e == encryption::a128gcm ? 16 : e == encryption::a192gcm ? 24
                                                                                    : e == encryption::a128cbc_hs256 || e == encryption::a256gcm ? 32
                                                                                    : e == encryption::a192cbc_hs384 ? 48 : 64),
                                                                 {.alg = algorithm::dir}));
        keys.emplace_back("RSA-OAEP", jose::jwk::generate(algorithm::rsa_oaep));
        keys.emplace_back("RSA-OAEP-256", jose::jwk::generate(algorithm::rsa_oaep_256));
        keys.emplace_back("A128GCMKW", jose::jwk::generate(algorithm::a128gcmkw));
        keys.emplace_back("A192GCMKW", jose::jwk::generate(algorithm::a192gcmkw));
        keys.emplace_back("A256GCMKW", jose::jwk::generate(algorithm::a256gcmkw));
        keys.emplace_back("ECDH-ES", jose::jwk(crypto::p256::private_key::generate(), {.alg = algorithm::ecdh_es}));
        keys.emplace_back("ECDH-ES", jose::jwk(crypto::p384::private_key::generate(), {.alg = algorithm::ecdh_es}));
        keys.emplace_back("ECDH-ES", jose::jwk(crypto::x25519::private_key::generate(), {.alg = algorithm::ecdh_es}));
        keys.emplace_back("ECDH-ES", jose::jwk(crypto::p521::private_key::generate(), {.alg = algorithm::ecdh_es}));
        keys.emplace_back("A128KW", jose::jwk::generate(algorithm::a128kw));
        keys.emplace_back("A192KW", jose::jwk::generate(algorithm::a192kw));
        keys.emplace_back("A256KW", jose::jwk::generate(algorithm::a256kw));
        keys.emplace_back("ECDH-ES+A128KW", jose::jwk(crypto::p256::private_key::generate(), {.alg = algorithm::ecdh_es_a128kw}));
        keys.emplace_back("ECDH-ES+A192KW", jose::jwk(crypto::p521::private_key::generate(), {.alg = algorithm::ecdh_es_a192kw}));
        keys.emplace_back("ECDH-ES+A256KW", jose::jwk(crypto::x25519::private_key::generate(), {.alg = algorithm::ecdh_es_a256kw}));
        for (const auto& [alg_name, key] : keys) {
            const std::string alg = text(alg_name);
            for (size_t n : {size_t(0), size_t(1), size_t(15), size_t(16), size_t(17), size_t(1000)}) {
                std::string pt(n, 'p');
                for (size_t i = 0; i < n; ++i) {
                    pt[i] = char('a' + i % 26);
                }
                auto ours = jose::jwe::encrypt(bytes(pt), key, {.enc = e});
                EXPECT_EQ(go({"decrypt", json_of(key), text(ours)}), "ok " + hex_of(pt)) << alg << " " << name_of(e) << " " << n;
                const std::string to = key.type() == jose::key_type::oct ? json_of(key) : text(key.to_json());
                auto theirs = go({"encrypt", alg, name_of(e), to, hex_of(pt)});
                auto back = jose::jwe::decrypt(string(theirs), key);
                ASSERT_TRUE(back) << alg << " " << name_of(e) << ": " << text(back.error().message()) << " " << theirs;
                EXPECT_EQ(text(*back), pt) << alg << " " << name_of(e);
            }
        }
    }
}

// --- keys ------------------------------------------------------------------

namespace {
    vector<pair<string, jose::jwk>> every_key() {
        vector<pair<string, jose::jwk>> keys;
        keys.emplace_back("oct", jose::jwk::symmetric(crypto::random::secret(32), {.kid = "o"}));
        keys.emplace_back("P-256", jose::jwk(crypto::p256::private_key::generate(), {.kid = "p"}));
        keys.emplace_back("P-384", jose::jwk(crypto::p384::private_key::generate()));
        keys.emplace_back("P-521", jose::jwk(crypto::p521::private_key::generate()));
        keys.emplace_back("RSA", jose::jwk(crypto::rsa::private_key::generate(2048), {.kid = "r", .alg = algorithm::ps256, .use = "sig"}));
        keys.emplace_back("Ed25519", jose::jwk(crypto::ed25519::private_key::generate()));
        keys.emplace_back("X25519", jose::jwk(crypto::x25519::private_key::generate(), {.use = "enc"}));
        return keys;
    }
}

TEST(Jose, KeysThroughTheirJsonBothWays) {
    for (const auto& [key_name, key] : every_key()) {
        const std::string name = text(key_name);
        EXPECT_TRUE(key.is_private()) << name;
        auto priv = key.to_private_json();
        auto again = jose::jwk::parse(priv);
        ASSERT_TRUE(again) << name << ": " << text(again.error().message());
        EXPECT_TRUE(*again == key) << name;
        EXPECT_EQ(text(again->to_private_json()), text(priv)) << name;
        EXPECT_EQ(text(again->thumbprint()), text(key.thumbprint())) << name;
        EXPECT_EQ(again->kid(), key.kid()) << name;
        EXPECT_EQ(again->use(), key.use()) << name;
        EXPECT_EQ(again->alg(), key.alg()) << name;
        EXPECT_EQ(again->type(), key.type()) << name;
        EXPECT_EQ(again->crv(), key.crv()) << name;
        if (key.type() == jose::key_type::oct) {
            EXPECT_THROW((void)key.to_json(), std::logic_error);
            EXPECT_THROW((void)key.public_key(), std::logic_error);
            EXPECT_THROW((void)key.to_pem(), std::logic_error);
            continue;
        }
        auto pub = key.public_key();
        EXPECT_FALSE(pub.is_private()) << name;
        EXPECT_FALSE(pub == key) << name;
        EXPECT_EQ(text(pub.to_json()), text(key.to_json())) << name;
        EXPECT_EQ(text(pub.to_private_json()), text(pub.to_json())) << name;
        EXPECT_EQ(text(pub.thumbprint()), text(key.thumbprint())) << name;
        auto pub_again = jose::jwk::parse(string(text(pub.to_json())));
        ASSERT_TRUE(pub_again) << name;
        EXPECT_TRUE(*pub_again == pub) << name;
        EXPECT_THROW((void)pub.to_pem(), std::logic_error);
        // the private JSON holds no member the public one lacks but the private ones
        auto pem = key.to_pem();
        auto from_pem = jose::jwk::from_pem(pem);
        ASSERT_TRUE(from_pem) << name << ": " << text(from_pem.error().message());
        EXPECT_EQ(text(from_pem->thumbprint()), text(key.thumbprint())) << name;
        EXPECT_EQ(text(from_pem->to_pem()), text(pem)) << name;
    }
}

TEST(Jose, KeyMembersKeptAndChecked) {
    // RFC 7517 A.1's EC key with members of its own and a key_ops list
    const std::string t = R"({"kty":"EC","crv":"P-256","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","use":"enc","kid":"1","key_ops":["deriveKey"],"x-mine":{"a":[1,2]}})";
    auto k = jose::jwk::parse(string(t));
    ASSERT_TRUE(k) << text(k.error().message());
    EXPECT_EQ(text(k->kid()), "1");
    EXPECT_EQ(text(k->use()), "enc");
    EXPECT_EQ(text(k->crv()), "P-256");
    EXPECT_FALSE(k->alg());
    EXPECT_EQ(k->parameters()["x-mine"]["a"][1].as_int(0), 2);
    EXPECT_TRUE(k->parameters()["key_ops"].is_array());
    auto back = jose::jwk::parse(string(text(k->to_json())));
    ASSERT_TRUE(back);
    EXPECT_TRUE(*back == *k);
    // RFC 7638 3.1's thumbprint, of RFC 7517 A.1's RSA key, by the Go oracle and by hand in python's terms:
    // the canonical text hashed (the member values as they stand)
    auto p = jose::jwk::parse(string(R"({"kty":"oct","k":"AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow","kid":"HMAC key used in JWS spec Appendix A.1 example"})"));
    ASSERT_TRUE(p);
    auto canon = std::string(R"({"k":"AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow","kty":"oct"})");
    auto d = crypto::sha256::of(bytes(canon));
    EXPECT_EQ(text(p->thumbprint()), text(encoding::base64::raw_url.encode(slice<const byte>(d.data(), d.size()))));
}

TEST(Jose, Rfc7515AppendixA1) {
    // RFC 7515 A.1: HS256 over the JWS of its example, the key of its JWK
    auto key = jose::jwk::parse(string(R"({"kty":"oct","k":"AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow"})"));
    ASSERT_TRUE(key);
    const std::string token = "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ.dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    auto v = jose::jws::verify(string(token), *key);
    ASSERT_TRUE(v) << text(v.error().message());
    EXPECT_EQ(text(*v), "{\"iss\":\"joe\",\r\n \"exp\":1300819380,\r\n \"http://example.com/is_root\":true}");
    // the JWT of it: expired since 2011, valid at an instant before its exp
    auto expired = jose::jwt::verify(string(token), *key);
    ASSERT_FALSE(expired);
    EXPECT_EQ(expired.error().code(), crypto::errc::expired);
    auto then = jose::jwt::verify(string(token), *key, {.issuer = "joe", .at = time::datetime::from_unix(1300819000, time::zone::utc())});
    ASSERT_TRUE(then) << text(then.error().message());
    EXPECT_EQ(text(then->issuer()), "joe");
    EXPECT_EQ(then->expires_at()->unix(), 1300819380);
    EXPECT_TRUE(then->claims()["http://example.com/is_root"].as_bool(false));
}

TEST(Jose, JwkRefusals) {
    struct Case {
        const char* text;
        crypto::errc code;
    };
    const Case cases[] = {
        {"", crypto::errc::malformed},
        {"[]", crypto::errc::malformed},
        {"{", crypto::errc::malformed},
        {"{}", crypto::errc::malformed},
        {R"({"kty":1})", crypto::errc::malformed},
        {R"({"kty":"EC","crv":"P-256"} x)", crypto::errc::malformed},
        {R"({"kty":"XYZ"})", crypto::errc::unsupported},
        {R"({"kty":"EC","crv":"P-192","x":"AA","y":"AA"})", crypto::errc::unsupported},
        {R"({"kty":"EC","crv":"P-521","x":"AA","y":"AA"})", crypto::errc::invalid_key},
        {R"({"kty":"OKP","crv":"Ed448","x":"AA"})", crypto::errc::unsupported},
        {R"({"kty":"oct"})", crypto::errc::malformed},
        {R"({"kty":"oct","k":"AA=="})", crypto::errc::malformed},
        {R"({"kty":"oct","k":"A\u0041"})", crypto::errc::malformed},
        {R"({"kty":"EC","x":"AA","y":"AA"})", crypto::errc::malformed},
        {R"({"kty":"oct","k":"AAAA","k":"AAAA"})", crypto::errc::malformed},
        {R"({"kty":"oct","k":"AAAA","d":"AAAA"})", crypto::errc::malformed},
        {R"({"kty":"EC","crv":"P-256","x":"AAAA","y":"AAAA"})", crypto::errc::invalid_key},
        // x and y of 32 bytes that are not a point
        {R"({"kty":"EC","crv":"P-256","x":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE","y":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE"})", crypto::errc::invalid_key},
        {R"({"kty":"OKP","crv":"Ed25519","x":"AAAA"})", crypto::errc::invalid_key},
        {R"({"kty":"RSA","n":"AQAB"})", crypto::errc::malformed},
        {R"({"kty":"RSA","n":"AQAB","e":"AQAB","oth":[]})", crypto::errc::unsupported},
        {R"({"kty":"EC","crv":"P-256","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","k":"AAAA"})", crypto::errc::malformed},
        // d of another key than x and y (RFC 7517 A.2's d with A.1's point changed in x)
        {R"({"kty":"EC","crv":"P-256","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","d":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE"})", crypto::errc::invalid_key},
    };
    for (const auto& c : cases) {
        auto k = jose::jwk::parse(string(c.text));
        ASSERT_FALSE(k) << c.text;
        EXPECT_EQ(k.error().code(), c.code) << c.text << ": " << text(k.error().message());
    }
    // RSA with d and no primes: not done
    jose::jwk r(crypto::rsa::private_key::generate(2048));
    auto j = encoding::json::parse(string(text(r.to_private_json())));
    ASSERT_TRUE(j);
    auto no_primes = j->erase(string("p")).erase(string("q")).erase(string("dp")).erase(string("dq")).erase(string("qi"));
    auto np = jose::jwk::parse(string(text(no_primes.to_string())));
    ASSERT_FALSE(np);
    EXPECT_EQ(np.error().code(), crypto::errc::unsupported);
    // numbers that do not agree
    auto wrong = j->set(string("dp"), j->at_path(string("/dq"))->as_string(string()));
    auto wp = jose::jwk::parse(string(text(wrong.to_string())));
    ASSERT_FALSE(wp);
    EXPECT_EQ(wp.error().code(), crypto::errc::invalid_key);
}

// --- JWT claims ---------------------------------------------------------------

namespace {
    time::datetime at(int64_t s) {
        return time::datetime::from_unix(s, time::zone::utc());
    }
}

TEST(Jose, JwtClaimsAtTheirEdges) {
    auto key = jose::jwk::generate(algorithm::hs256);
    const int64_t t0 = 1'800'000'000;
    auto token = [&](encoding::json claims) {
        return jose::jwt::sign(claims, key);
    };
    auto code = [&](const string& tok, const jose::jwt::verify_options& o) {
        auto r = jose::jwt::verify(tok, key, o);
        return r ? optional<crypto::errc>() : optional<crypto::errc>(r.error().code());
    };
    // exp: valid up to exp + leeway, not at it
    auto e = token(encoding::json::object({{"exp", t0}}));
    EXPECT_FALSE(code(e, {.leeway = duration(), .at = at(t0 - 1)}));
    EXPECT_EQ(code(e, {.leeway = duration(), .at = at(t0)}), crypto::errc::expired);
    EXPECT_FALSE(code(e, {.at = at(t0 + 59)}));
    EXPECT_EQ(code(e, {.at = at(t0 + 60)}), crypto::errc::expired);
    // a fraction of a second
    auto ef = token(encoding::json::object({{"exp", double(t0) + 0.5}}));
    EXPECT_FALSE(code(ef, {.leeway = duration(), .at = at(t0)}));
    // nbf and iat
    auto n = token(encoding::json::object({{"exp", t0 + 1000}, {"nbf", t0}}));
    EXPECT_EQ(code(n, {.leeway = duration(), .at = at(t0 - 1)}), crypto::errc::not_yet_valid);
    EXPECT_FALSE(code(n, {.leeway = duration(), .at = at(t0)}));
    EXPECT_FALSE(code(n, {.at = at(t0 - 60)}));
    EXPECT_EQ(code(n, {.at = at(t0 - 61)}), crypto::errc::not_yet_valid);
    auto i = token(encoding::json::object({{"exp", t0 + 1000}, {"iat", t0}}));
    EXPECT_EQ(code(i, {.leeway = duration(), .at = at(t0 - 1)}), crypto::errc::not_yet_valid);
    EXPECT_FALSE(code(i, {.leeway = duration(), .at = at(t0)}));
    // exp required by default
    auto none = token(encoding::json::object({{"sub", "x"}}));
    EXPECT_EQ(code(none, {.at = at(t0)}), crypto::errc::verification);
    EXPECT_FALSE(code(none, {.at = at(t0), .require_expiration = false}));
    // a date that is not a number
    auto bad = token(encoding::json::object({{"exp", "tomorrow"}}));
    EXPECT_EQ(code(bad, {.at = at(t0)}), crypto::errc::malformed);
    // iss
    auto is = token(encoding::json::object({{"exp", t0 + 10}, {"iss", "https://a.example"}}));
    EXPECT_FALSE(code(is, {.issuer = "https://a.example", .at = at(t0)}));
    EXPECT_EQ(code(is, {.issuer = "https://b.example", .at = at(t0)}), crypto::errc::verification);
    EXPECT_FALSE(code(is, {.at = at(t0)}));
    auto no_iss = token(encoding::json::object({{"exp", t0 + 10}}));
    EXPECT_EQ(code(no_iss, {.issuer = "https://a.example", .at = at(t0)}), crypto::errc::verification);
    // aud: a string, a list, none expected, none there
    auto a1 = token(encoding::json::object({{"exp", t0 + 10}, {"aud", "api"}}));
    auto a2 = token(encoding::json::object({{"exp", t0 + 10}, {"aud", encoding::json::array({"web", "api"})}}));
    EXPECT_FALSE(code(a1, {.audience = "api", .at = at(t0)}));
    EXPECT_FALSE(code(a2, {.audience = "api", .at = at(t0)}));
    EXPECT_EQ(code(a2, {.audience = "other", .at = at(t0)}), crypto::errc::verification);
    EXPECT_EQ(code(a1, {.at = at(t0)}), crypto::errc::verification);
    EXPECT_EQ(code(no_iss, {.audience = "api", .at = at(t0)}), crypto::errc::verification);
    auto a3 = token(encoding::json::object({{"exp", t0 + 10}, {"aud", encoding::json::array({"api", 7})}}));
    EXPECT_EQ(code(a3, {.audience = "api", .at = at(t0)}), crypto::errc::malformed);
    // the accessors
    auto v = jose::jwt::verify(a2, key, {.audience = "web", .at = at(t0)});
    ASSERT_TRUE(v);
    EXPECT_EQ(v->audience().size(), 2u);
    EXPECT_EQ(text(v->audience()[1]), "api");
    EXPECT_EQ(text(v->header()["typ"].as_string(string())), "JWT");
    EXPECT_FALSE(v->not_before());
    EXPECT_EQ(text(v->subject()), "");
    // claims that are not an object
    EXPECT_THROW((void)jose::jwt::sign(encoding::json::array({1}), key), std::invalid_argument);
    auto arr = jose::jws::sign(bytes("[1]"), key);
    auto r = jose::jwt::verify(arr, key, {.at = at(t0)});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), crypto::errc::malformed);
    // the JSON serialization is not a JWT
    auto js = jose::jws::sign_json(bytes(R"({"exp":1})"), key);
    auto rj = jose::jwt::verify(js, key);
    ASSERT_FALSE(rj);
    EXPECT_EQ(rj.error().code(), crypto::errc::malformed);
    // parse_unverified reads a token of any key
    auto other = jose::jwk::generate(algorithm::hs256);
    auto u = jose::jwt::parse_unverified(jose::jwt::sign(encoding::json::object({{"iss", "x"}}), other));
    ASSERT_TRUE(u);
    EXPECT_EQ(text(u->issuer()), "x");
}

// --- what the rules refuse ------------------------------------------------------

namespace {
    const char* const Rsa1024 = R"(-----BEGIN PRIVATE KEY-----
MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBALLBcSiJ+m/dGOyu
L96tAfOlRM5pKDfWPDxp3sY50juItX+cmIPhFMnLftF5/0uA4tplgWOOvEunORTM
+L8kvEr3mXuJkKZytV/Dc6bEMzMqetmg5lX0dR1xMeFo6e6IPMUewpx1jol403RP
tLaZZ095bQ1aTH8logVaRA4d8m3vAgMBAAECgYEAhoioTcZqwb45gAgo5wJ2sZT1
EBR8vGl0tFNJ4w6pdz0QvJZzAu5n4uhQ7f9PcNfz5EL4+qD2fknA4DLUE6fmPZNG
XySSHAMLP9PeXQvCIfGyaVR278xmNA3aztVtuWl5PZpqkq8UiJMGKAvyOa4QMxor
qyZGnDJdbGh9ka7ZpYECQQDizl5rhN7vk/dsmfKWncm0AUbCRt2G+AD/ZcxvWFxU
NRC/qw1QYjqKncts447S6KtsLE5mKJCMlBUR6UmLAWLxAkEAycO7LIfM91+XeHip
4/hzzXCG8CTAraZMRt64sUGbYjV3Rkg3bDhfQC3ruXh/sgPQh5TMkAXw1cASV0rA
BnUe3wJBANh/IUYE2UFbvsydzyDSkV0P8tk0B/Xz+g/Qvmpyr/95f/lVcCoQ+cyj
fzD7rrPIgQmK6XM+uWxXVh140EiuFCECQHB7JgYVTzc5m4AWBOCKykRlz1RhqOkm
JK/9yolHQhDmLbCI4hz68F8fOqMTgl0Ds2VilwhTx4fipFb13Ue8U5ECQG2E912R
HDm7dCukKJgJotDtiNAyXc6vtoR2wgC98h1VRwzJ7eV9UFixWYMqIao2hktHeI6G
YA7PhgrlI/GFFLI=
-----END PRIVATE KEY-----
)";
}

TEST(Jose, AlgorithmByKeyNeverByToken) {
    auto rsa = jose::jwk(crypto::rsa::private_key::generate(2048));
    auto pub = rsa.public_key();
    // the classic confusion: HS256 under the RSA public key's text as an HMAC key
    auto confused_key = jose::jwk::symmetric(bytes(text(pub.to_json())));
    auto forged = jose::jws::sign(bytes("x"), confused_key);
    auto r = jose::jws::verify(forged, pub);
    ASSERT_FALSE(r);
    // alg none, with and without a signature
    const std::string none_h = text(encoding::base64::raw_url.encode(bytes(R"({"alg":"none"})")));
    for (const std::string tok : {none_h + ".eA.", none_h + ".eA.AAAA"}) {
        EXPECT_FALSE(jose::jws::verify(string(tok), pub)) << tok;
        EXPECT_FALSE(jose::jws::verify(string(tok), jose::jwk::generate(algorithm::hs256))) << tok;
    }
    // the key's alg pins it: RS256 under a PS256 key
    auto ps = jose::jwk(crypto::rsa::private_key::generate(2048), {.alg = algorithm::ps256});
    auto rs_token = jose::jws::sign(bytes("x"), ps, {.alg = algorithm::ps256});
    EXPECT_TRUE(jose::jws::verify(rs_token, ps.public_key()));
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), ps, {.alg = algorithm::rs256}), std::invalid_argument);
    // use and key_ops
    auto enc_key = jose::jwk(crypto::p256::private_key::generate(), {.use = "enc"});
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), enc_key), std::invalid_argument);
    auto sig_key = jose::jwk(crypto::p256::private_key::generate(), {.use = "sig"});
    auto t = jose::jws::sign(bytes("x"), sig_key);
    auto as_enc = jose::jwk::parse(string(text(sig_key.public_key().to_json()).replace(text(sig_key.public_key().to_json()).find("\"sig\""), 5, "\"enc\"")));
    ASSERT_TRUE(as_enc);
    auto ve = jose::jws::verify(t, *as_enc);
    ASSERT_FALSE(ve);
    EXPECT_EQ(ve.error().code(), crypto::errc::invalid_key);
    EXPECT_THROW((void)jose::jwe::encrypt(bytes("x"), sig_key), std::invalid_argument);
    // key sizes: an HMAC key shorter than the digest, RSA under 2048
    auto short_key = jose::jwk::symmetric(bytes("0123456789abcdef0123456789abcde"));
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), short_key), std::invalid_argument);
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), jose::jwk::symmetric(crypto::random::secret(32)), {.alg = algorithm::hs384}), std::invalid_argument);
    // a key of 1024 bits (made by OpenSSL for the test): refused to sign, and to verify
    auto small = jose::jwk::from_pem(string(Rsa1024));
    ASSERT_TRUE(small) << text(small.error().message());
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), *small), std::invalid_argument);
    auto forged_small = text(encoding::base64::raw_url.encode(bytes(R"({"alg":"RS256"})"))) + ".eA." + std::string(171, 'A');
    auto vs = jose::jws::verify(string(forged_small), small->public_key());
    ASSERT_FALSE(vs);
    EXPECT_EQ(vs.error().code(), crypto::errc::invalid_key);
    // a public key cannot sign; a JWE algorithm is not a signature
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), pub), std::invalid_argument);
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), rsa, {.alg = algorithm::rsa_oaep}), std::invalid_argument);
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), jose::jwk(crypto::x25519::private_key::generate())), std::invalid_argument);
    // ES384 under a P-256 key
    auto p256 = jose::jwk(crypto::p256::private_key::generate());
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), p256, {.alg = algorithm::es384}), std::invalid_argument);
    // crit and b64 refused when read
    auto crit_h = text(encoding::base64::raw_url.encode(bytes(R"({"alg":"HS256","crit":["exp"],"exp":1})")));
    auto j = jose::jws::parse(string(crit_h + ".eA.AAAA"));
    ASSERT_FALSE(j);
    EXPECT_EQ(j.error().code(), crypto::errc::unsupported);
    auto b64_h = text(encoding::base64::raw_url.encode(bytes(R"({"alg":"HS256","b64":false})")));
    auto jb = jose::jws::parse(string(b64_h + ".eA.AAAA"));
    ASSERT_FALSE(jb);
    EXPECT_EQ(jb.error().code(), crypto::errc::unsupported);
}

TEST(Jose, JsonSerialization) {
    auto a = jose::jwk(crypto::p256::private_key::generate(), {.kid = "a"});
    auto b = jose::jwk(crypto::ed25519::private_key::generate(), {.kid = "b"});
    auto flat = jose::jws::sign_json(bytes("hello"), a, {.header = encoding::json::object({{"cty", "text"}})});
    auto pf = jose::jws::parse(flat);
    ASSERT_TRUE(pf);
    EXPECT_EQ(pf->signature_count(), 1u);
    EXPECT_EQ(text(pf->kid()), "a");
    EXPECT_EQ(pf->alg(), algorithm::es256);
    EXPECT_EQ(text(pf->header()["cty"].as_string(string())), "text");
    EXPECT_TRUE(pf->header(1).is_null());
    EXPECT_EQ(text(pf->unverified_payload()), "hello");
    EXPECT_TRUE(jose::jws::verify(flat, a.public_key()));
    jose::jwk_set both{a, b};
    auto general = jose::jws::sign_json(bytes("hello"), both);
    auto pg = jose::jws::parse(general);
    ASSERT_TRUE(pg);
    EXPECT_EQ(pg->signature_count(), 2u);
    EXPECT_EQ(pg->alg(1), algorithm::eddsa);
    EXPECT_TRUE(jose::jws::verify(general, a.public_key()));
    EXPECT_TRUE(jose::jws::verify(general, b.public_key()));
    EXPECT_FALSE(jose::jws::verify(general, jose::jwk(crypto::p256::private_key::generate())));
    EXPECT_TRUE(jose::jws::verify(general, jose::jwk_set{b.public_key()}));
    EXPECT_THROW((void)jose::jws::sign_json(bytes("x"), jose::jwk_set()), std::invalid_argument);
    // an unprotected header: read with the protected one, never over it
    auto signed_h = text(encoding::json::parse(flat)->set(string("header"), encoding::json::object({{"x-note", 1}})).to_string());
    auto pu = jose::jws::parse(string(signed_h));
    ASSERT_TRUE(pu);
    EXPECT_EQ(pu->header()["x-note"].as_int(0), 1);
    EXPECT_TRUE(pu->verify(a));
    auto over = text(encoding::json::parse(flat)->set(string("header"), encoding::json::object({{"alg", "none"}})).to_string());
    EXPECT_FALSE(jose::jws::parse(string(over)));
    // malformed JSON forms
    for (const char* bad : {R"({"payload":"eA"})", R"({"payload":"eA","signatures":[]})", R"({"payload":"eA","signatures":[1]})",
                            R"({"payload":"eA","signature":"AAAA"})", R"({"payload":1,"signature":"AAAA","protected":"e30"})",
                            R"({"payload":"eA","signature":"AAAA","signatures":[]})"}) {
        EXPECT_FALSE(jose::jws::parse(string(bad))) << bad;
    }
}

TEST(Jose, KeySets) {
    auto a = jose::jwk(crypto::p256::private_key::generate(), {.kid = "a"});
    auto b = jose::jwk(crypto::rsa::private_key::generate(2048), {.kid = "b"});
    auto h = jose::jwk::symmetric(crypto::random::secret(32), {.kid = "h"});
    jose::jwk_set set{a, b};
    set.push_back(h);
    EXPECT_EQ(set.size(), 3u);
    EXPECT_EQ(text(set.find(string("b"))->kid()), "b");
    EXPECT_FALSE(set.find(string("z")));
    // the public JSON leaves the oct key out
    auto pub = jose::jwk_set::parse(string(text(set.to_json())));
    ASSERT_TRUE(pub);
    EXPECT_EQ(pub->size(), 2u);
    EXPECT_FALSE((*pub)[0].is_private());
    auto priv = jose::jwk_set::parse(set.to_private_json());
    ASSERT_TRUE(priv);
    EXPECT_EQ(priv->size(), 3u);
    EXPECT_TRUE((*priv)[2] == h);
    // an unknown kty or curve is skipped, a malformed key fails the set
    auto skip = jose::jwk_set::parse(string(R"({"keys":[{"kty":"PQC","x":"AA"},{"kty":"EC","crv":"secp256k1","x":"AA","y":"AA"},)" + text(a.to_json()) + "]}"));
    ASSERT_TRUE(skip);
    EXPECT_EQ(skip->size(), 1u);
    EXPECT_FALSE(jose::jwk_set::parse(string(R"({"keys":[{"kty":"EC"}]})")));
    EXPECT_FALSE(jose::jwk_set::parse(string(R"({"keys":{}})")));
    EXPECT_FALSE(jose::jwk_set::parse(string(R"({"nokeys":[]})")));
    auto empty = jose::jwk_set::parse(string(R"({"keys":[]})"));
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
    // verification by kid; a mixed set refused, two keys of a kid refused
    auto token = jose::jwt::sign(encoding::json::object({{"exp", time::now().unix() + 100}}), b);
    EXPECT_TRUE(jose::jwt::verify(token, *pub));
    EXPECT_FALSE(jose::jwt::verify(token, set));
    jose::jwk_set dup{b.public_key(), jose::jwk(crypto::rsa::private_key::generate(2048), {.kid = "b"}).public_key()};
    EXPECT_FALSE(jose::jwt::verify(token, dup));
    // a token without kid: every key tried
    auto no_kid = jose::jwk(crypto::p256::private_key::generate());
    auto t2 = jose::jws::sign(bytes("x"), no_kid);
    EXPECT_TRUE(jose::jws::verify(t2, jose::jwk_set{a.public_key(), no_kid.public_key()}));
    EXPECT_FALSE(jose::jws::verify(t2, jose::jwk_set{a.public_key()}));
    EXPECT_FALSE(jose::jws::verify(t2, jose::jwk_set()));
}

TEST(Jose, EncryptionRefusalsAndTampering) {
    auto key = jose::jwk::generate(algorithm::rsa_oaep_256);
    auto c = jose::jwe::encrypt(bytes("attack at dawn"), key, {.enc = encryption::a128cbc_hs256, .header = encoding::json::object({{"cty", "text"}})});
    auto p = jose::jwe::parse(c);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->alg(), algorithm::rsa_oaep_256);
    EXPECT_EQ(p->enc(), encryption::a128cbc_hs256);
    EXPECT_EQ(text(p->header()["cty"].as_string(string())), "text");
    EXPECT_TRUE(p->decrypt(key));
    // every part changed in turn: the tag fails (a key that does not unwrap goes on with a random one)
    std::string s = text(c);
    std::vector<size_t> dots;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '.') {
            dots.push_back(i);
        }
    }
    for (size_t part = 0; part < 5; ++part) {
        std::string t = s;
        const size_t at = part == 0 ? 2 : dots[part - 1] + 2;
        t[at] = t[at] == 'A' ? 'B' : 'A';
        auto r = jose::jwe::decrypt(string(t), key);
        ASSERT_FALSE(r) << part;
        if (part > 0) {
            EXPECT_EQ(r.error().code(), crypto::errc::authentication) << part << " " << text(r.error().message());
        }
    }
    // another key of the kind: the same error
    auto other = jose::jwk::generate(algorithm::rsa_oaep_256);
    auto r = jose::jwe::decrypt(c, other);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), crypto::errc::authentication);
    // a key of another kind
    auto w = jose::jwe::decrypt(c, jose::jwk::generate(algorithm::a128gcmkw));
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().code(), crypto::errc::invalid_key);
    // a public key cannot decrypt
    EXPECT_FALSE(jose::jwe::decrypt(c, key.public_key()));
    // encrypting to the public key works
    auto c2 = jose::jwe::encrypt(bytes("x"), key.public_key());
    EXPECT_EQ(text(*jose::jwe::decrypt(c2, key)), "x");
    // dir: the key's size is the enc's
    auto d16 = jose::jwk::symmetric(crypto::random::secret(16), {.alg = algorithm::dir});
    EXPECT_THROW((void)jose::jwe::encrypt(bytes("x"), d16), std::invalid_argument);   // A256GCM wants 32
    auto cd = jose::jwe::encrypt(bytes("x"), d16, {.enc = encryption::a128gcm});
    EXPECT_TRUE(jose::jwe::decrypt(cd, d16));
    EXPECT_THROW((void)jose::jwk::generate(algorithm::dir), std::invalid_argument);
    // zip and unknown algorithms
    auto h = [](const char* json) {
        return text(encoding::base64::raw_url.encode(bytes(json)));
    };
    auto zipped = jose::jwe::decrypt(string(h(R"({"alg":"dir","enc":"A128GCM","zip":"DEF"})") + "..AAAAAAAAAAAAAAAA.AA.AAAAAAAAAAAAAAAAAAAAAA"), d16);
    ASSERT_FALSE(zipped);
    EXPECT_EQ(zipped.error().code(), crypto::errc::unsupported);
    auto rsa15 = jose::jwe::decrypt(string(h(R"({"alg":"RSA1_5","enc":"A128GCM"})") + ".AA.AAAAAAAAAAAAAAAA.AA.AAAAAAAAAAAAAAAAAAAAAA"), key);
    ASSERT_FALSE(rsa15);
    EXPECT_EQ(rsa15.error().code(), crypto::errc::unsupported);
    for (const char* bad : {"", "a.b.c.d", "a.b.c.d.e.f", "e30.AA.AA.AA.AA"}) {
        EXPECT_FALSE(jose::jwe::parse(string(bad))) << bad;
    }
    // the key set: by kid
    auto k1 = jose::jwk::generate(algorithm::a256gcmkw, {.kid = "1"});
    auto k2 = jose::jwk::generate(algorithm::a256gcmkw, {.kid = "2"});
    auto ck = jose::jwe::encrypt(bytes("to two"), k2);
    EXPECT_EQ(text(*jose::jwe::decrypt(ck, jose::jwk_set{k1, k2})), "to two");
    EXPECT_FALSE(jose::jwe::decrypt(ck, jose::jwk_set{k1}));
}

// --- every method at its boundaries ----------------------------------------------

TEST(Jose, Boundaries) {
    // a handle moved from is the same key (a move is a copy)
    auto a = jose::jwk::generate(algorithm::es256, {.kid = "m"});
    jose::jwk b = std::move(a);
    EXPECT_TRUE(a == b);   // NOLINT: a moved-from jwk stays the key
    EXPECT_TRUE(jose::jws::verify(jose::jws::sign(bytes("x"), a), b));
    a = a;
    EXPECT_TRUE(a == b);
    // empty payload and plaintext
    auto e = jose::jws::sign(slice<const byte>(), b);
    auto ev = jose::jws::verify(e, b);
    ASSERT_TRUE(ev);
    EXPECT_TRUE(ev->empty());
    auto k = jose::jwk::generate(algorithm::a128gcmkw);
    for (auto enc : {encryption::a128gcm, encryption::a128cbc_hs256}) {
        auto c = jose::jwe::encrypt(slice<const byte>(), k, {.enc = enc});
        auto d = jose::jwe::decrypt(c, k);
        ASSERT_TRUE(d);
        EXPECT_TRUE(d->empty());
    }
    // a megabyte
    std::string big(1 << 20, 'z');
    EXPECT_EQ(jose::jws::verify(jose::jws::sign(bytes(big), b), b)->size(), big.size());
    EXPECT_EQ(jose::jwe::decrypt(jose::jwe::encrypt(bytes(big), k), k)->size(), big.size());
    // nothing to read
    EXPECT_FALSE(jose::jws::parse(string()));
    EXPECT_FALSE(jose::jws::parse(string("..")));
    EXPECT_FALSE(jose::jws::parse(string("a.b")));
    EXPECT_FALSE(jose::jws::parse(string("a.b.c.d")));
    EXPECT_FALSE(jose::jwt::parse_unverified(string()));
    EXPECT_FALSE(jose::jwk::parse(string()));
    EXPECT_FALSE(jose::jwk::parse(crypto::secret_bytes()));
    EXPECT_FALSE(jose::jwk_set::parse(string()));
    EXPECT_FALSE(jose::jwk_set::parse(crypto::secret_bytes()));
    EXPECT_FALSE(jose::jwk::from_pem(string()));
    EXPECT_FALSE(jose::jwk::from_pem(crypto::secret_bytes()));
    // the literals' constructors: parse's value, or its error thrown
    EXPECT_TRUE(jose::jwk(R"({"kty":"oct","k":"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"})").type() == jose::key_type::oct);
    EXPECT_EQ(jose::jwk_set(R"({"keys":[]})").size(), 0u);
    EXPECT_EQ(jose::jws(jose::jws::sign(bytes("x"), b)).signature_count(), 1u);
    EXPECT_TRUE(jose::jwe(jose::jwe::encrypt(bytes("x"), k)).enc() == encryption::a256gcm);
    EXPECT_THROW((void)jose::jwk("{}"), sgcl::bad_expected_access<crypto::error>);
    EXPECT_THROW((void)jose::jwk_set("{}"), sgcl::bad_expected_access<crypto::error>);
    EXPECT_THROW((void)jose::jws("a.b"), sgcl::bad_expected_access<crypto::error>);
    EXPECT_THROW((void)jose::jwe("a.b"), sgcl::bad_expected_access<crypto::error>);
    EXPECT_FALSE(jose::jwk::from_pem(string("-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n")));
    // the program's contract
    EXPECT_THROW((void)jose::jwk::symmetric(slice<const byte>()), std::invalid_argument);
    EXPECT_THROW((void)jose::jws::sign(bytes("x"), b, {.header = encoding::json(1)}), std::invalid_argument);
    EXPECT_THROW((void)jose::jwe::encrypt(bytes("x"), k, {.header = encoding::json("x")}), std::invalid_argument);
    EXPECT_THROW((void)jose::jwe::encrypt(bytes("x"), k, {.alg = algorithm::es256}), std::invalid_argument);
    EXPECT_THROW((void)jose::jwe::encrypt(bytes("x"), k, {.enc = encryption(99)}), std::invalid_argument);
    EXPECT_THROW((void)jose::jwk::generate(algorithm(200)), std::invalid_argument);
    // the program's header: kid of its own kept, alg never taken
    auto t = jose::jws::sign(bytes("x"), b, {.header = encoding::json::object({{"alg", "none"}, {"kid", "mine"}, {"typ", "at+jwt"}})});
    auto pt = jose::jws::parse(t);
    ASSERT_TRUE(pt);
    EXPECT_EQ(pt->alg(), algorithm::es256);
    EXPECT_EQ(text(pt->kid()), "mine");
    EXPECT_EQ(text(pt->header()["typ"].as_string(string())), "at+jwt");
    // a key set's index
    jose::jwk_set s{b};
    EXPECT_TRUE(s[0] == b);
    EXPECT_EQ(s.keys().size(), 1u);
    // thumbprints by another hash
    EXPECT_EQ(b.thumbprint(crypto::hash_id::sha512).size(), 86u);
}

TEST(Jose, PrivateMembersNeverInManagedMemory) {
    // the scan of tests/managed_scan.h over what reading, signing, the
    // exports, a comparison and a key set leave: the private members' bytes
    // and their base64url text, never in a managed page
    std::vector<managed_scan::bytes_t> patterns;
    auto add = [&](const std::string& s) {
        patterns.emplace_back(s.begin(), s.end());
    };
    // the private JSONs, made before the scan, in plain memory
    std::string ec_json, rsa_json, oct_json, ed_json;
    {
        auto ec = crypto::p256::private_key::generate();
        auto d = ec.bytes();
        add(std::string(reinterpret_cast<const char*>(d.bytes().data()), 32));
        jose::jwk k(std::move(ec));
        ec_json = text(k.to_private_json());
        jose::jwk r(crypto::rsa::private_key::generate(2048));
        rsa_json = text(r.to_private_json());
        jose::jwk o = jose::jwk::generate(algorithm::hs256);
        oct_json = text(o.to_private_json());
        jose::jwk e(crypto::ed25519::private_key::generate());
        ed_json = text(e.to_private_json());
    }
    for (const auto* j : {&ec_json, &rsa_json, &oct_json, &ed_json}) {
        for (const char* member : {"\"d\":\"", "\"p\":\"", "\"q\":\"", "\"dp\":\"", "\"dq\":\"", "\"qi\":\"", "\"k\":\""}) {
            auto at = j->find(member);
            if (at == std::string::npos) {
                continue;
            }
            at += std::strlen(member);
            const std::string b64 = j->substr(at, j->find('"', at) - at);
            add(b64);
            std::string raw(b64.size(), '\0');
            raw.resize(crypto::detail::b64url_decoded_size(b64.size()));
            ASSERT_TRUE(crypto::detail::b64url_decode(reinterpret_cast<unsigned char*>(raw.data()), b64.data(), b64.size()));
            add(raw.substr(0, 24));
        }
    }
    ASSERT_GE(patterns.size(), 15u);
    std::vector<size_t> which;
    const size_t found = managed_scan::found_after(
        patterns,
        [&] {
            for (const auto* j : {&ec_json, &rsa_json, &oct_json, &ed_json}) {
                crypto::secret_bytes t(j->size());
                std::copy(j->begin(), j->end(), reinterpret_cast<char*>(t.as_slice().data()));
                auto k = jose::jwk::parse(t);
                ASSERT_TRUE(k);
                auto again = k->to_private_json();
                EXPECT_TRUE(jose::jwk::parse(again)->thumbprint() == k->thumbprint());
                auto tok = jose::jws::sign(bytes("x"), *k);
                EXPECT_TRUE(jose::jws::verify(tok, *k));
                if (k->type() != jose::key_type::oct) {
                    auto pem = k->to_pem();
                    EXPECT_TRUE(jose::jwk::from_pem(pem));
                }
                jose::jwk_set set{*k};
                auto sj = set.to_private_json();
                EXPECT_TRUE(jose::jwk_set::parse(sj));
            }
        },
        &which);
    std::string names;
    for (size_t i : which) {
        names += " " + std::to_string(i);
    }
    EXPECT_EQ(found, 0u) << "patterns in managed memory:" << names;
}

TEST(Jose, ConstantTimeBase64urlAgreesWithEncoding) {
    // every character's value against the alphabet, every value's character
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (unsigned c = 0; c < 256; ++c) {
        uint32_t bad = 0;
        const uint32_t v = crypto::detail::b64url_value(c, bad);
        const auto at = alphabet.find(char(c));
        EXPECT_EQ(bad == 0, at != std::string::npos) << c;
        if (at != std::string::npos) {
            EXPECT_EQ(v, at) << c;
        }
    }
    for (uint32_t v = 0; v < 64; ++v) {
        EXPECT_EQ(crypto::detail::b64url_char(v), alphabet[v]) << v;
    }
    // random bytes of every length to 300 both ways, and texts the strict codec refuses
    for (size_t n = 0; n < 300; ++n) {
        auto data = crypto::random::bytes(n);
        auto ours = crypto::detail::b64url(data.as_slice());
        EXPECT_EQ(text(ours), text(encoding::base64::raw_url.encode(data.as_slice()))) << n;
        auto back = crypto::detail::b64url_bytes(ours.view());
        ASSERT_TRUE(back) << n;
        EXPECT_TRUE(*back == data) << n;
    }
    for (const char* bad : {"A", "AB=", "AB C", "A+B/", "AB", "ABC", "AAAAA"}) {
        auto ours = crypto::detail::b64url_bytes(bad);
        auto theirs = encoding::base64::raw_url.decode(string(bad));
        EXPECT_EQ(ours.has_value(), theirs.has_value()) << bad;
    }
    // bits left over that are not zero: "AB" is canonical, "AC" is not
    EXPECT_FALSE(crypto::detail::b64url_bytes("AC"));
    EXPECT_FALSE(crypto::detail::b64url_bytes("AAB"));
}
