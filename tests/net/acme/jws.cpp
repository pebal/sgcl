//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The account's key and its JSON Web Signatures: the vectors of RFC 7515
// (A.1 HS256, the MAC of an external account binding; A.3 ES256 verified),
// RFC 8037 (A.3 the Ed25519 JWK's thumbprint, A.4 its signature byte for
// byte), RFC 7638 (§3.1 the thumbprint of an RSA JWK); Wycheproof's JWS
// tests of ES256 and RS256 against the verifier the test server uses
// (~/Programming/oracles/wycheproof, when it is there); the JWK of each
// kind, PEM both ways, the requests the client signs read back.
#include "acme_test.h"
#include "sgcl/net/acme/detail/jws_verify.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace sgcl;
using namespace acme_test;
namespace jd = sgcl::net::acme::detail;

namespace {
    vector<byte> b64d(const char* s) {
        return encoding::base64::raw_url.decode(string(s)).value();
    }

    // A PKCS #8 of an Ed25519 seed (RFC 8410 §7)
    vector<byte> ed25519_pkcs8(const vector<byte>& seed) {
        static const unsigned char head[] = {0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x04, 0x22, 0x04, 0x20};
        vector<byte> out(reinterpret_cast<const byte*>(head), reinterpret_cast<const byte*>(head) + sizeof head);
        for (auto b : seed) {
            out.push_back(b);
        }
        return out;
    }

    std::string read_oracle(const char* name) {
        const char* home = std::getenv("HOME");
        if (!home) {
            return {};
        }
        std::ifstream in(std::string(home) + "/Programming/oracles/wycheproof/testvectors_v1/" + name, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
}

TEST(AcmeJws, Rfc7515HmacSha256) {
    // RFC 7515 A.1: the header and the payload with their CR LF, the key k
    const char* k = "AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow";
    std::string input = "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ";
    auto key = b64d(k);
    auto tag = crypto::hmac<crypto::sha256>::of(slice<const byte>(reinterpret_cast<const byte*>(input.data()), input.size()), key.as_slice());
    EXPECT_EQ(text(encoding::base64::raw_url.encode(tag)), "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
}

TEST(AcmeJws, Rfc7515EcdsaP256Verified) {
    // RFC 7515 A.3: the key's JWK and the example's signature
    auto jwk = encoding::json::parse(string(R"({"kty":"EC","crv":"P-256","x":"f83OJ3D2xF1Bg8vub9tLe1gHMzV76e8Tus9uPHvRVEU","y":"x_FEzRu9m36HLN_tue659LNpXW6pCyStikYjKIWI5a0"})"));
    ASSERT_TRUE(jwk.has_value());
    auto key = jd::jwk_key(*jwk);
    ASSERT_TRUE(key.has_value());
    std::string input = "eyJhbGciOiJFUzI1NiJ9.eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ";
    auto sig = b64d("DtEhU3ljbEg8L38VWAfUAqOyKAM6-Xx-F4GawxaepmXFCgfTjDxw5djxLa8ISlSApmWQxfKTUJqPP3-Kg6NU1Q");
    auto in = slice<const byte>(reinterpret_cast<const byte*>(input.data()), input.size());
    EXPECT_TRUE(jd::jws_verify(*key, "ES256", in, sig.as_slice()));
    EXPECT_FALSE(jd::jws_verify(*key, "ES384", in, sig.as_slice()));
    sig[5] ^= byte(1);
    EXPECT_FALSE(jd::jws_verify(*key, "ES256", in, sig.as_slice()));
}

TEST(AcmeJws, Rfc8037Ed25519) {
    // RFC 8037 A.1 (the key), A.3 (its thumbprint), A.4 (the signature)
    auto seed = b64d("nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A");
    auto der = ed25519_pkcs8(seed);
    auto pem = encoding::pem("PRIVATE KEY", der).to_string();
    auto key = net::acme::account_key::from_pem(slice<const byte>(reinterpret_cast<const byte*>(pem.data()), pem.size()));
    ASSERT_TRUE(key.has_value()) << text(key.error().message());
    EXPECT_EQ(key->algorithm(), net::acme::key_algorithm::eddsa);
    EXPECT_EQ(text(key->jwk()), R"({"crv":"Ed25519","kty":"OKP","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    EXPECT_EQ(text(key->thumbprint()), "kPrK_qmxVWaYVA9wwBF6Iuo3vVzz7TxHCTwXBygrS4k");
    const auto& st = jd::KeyAccess::state(*key);
    string jws = jd::jws(*st.key, string(R"({"alg":"EdDSA"})"), string("Example of Ed25519 signing"));
    auto j = encoding::json::parse(jws);
    ASSERT_TRUE(j.has_value());
    EXPECT_EQ(text((*j)["protected"].as_string(string())), "eyJhbGciOiJFZERTQSJ9");
    EXPECT_EQ(text((*j)["payload"].as_string(string())), "RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc");
    EXPECT_EQ(text((*j)["signature"].as_string(string())), "hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg");
}

TEST(AcmeJws, Rfc7638Thumbprint) {
    // RFC 7638 §3.1: an RSA JWK, members in any order and more of them
    auto jwk = encoding::json::parse(string(R"({"kty":"RSA","n":"0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw","e":"AQAB","alg":"RS256","kid":"2011-04-29"})"));
    ASSERT_TRUE(jwk.has_value());
    auto key = jd::jwk_key(*jwk);
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(text(key->thumbprint), "NzbLsXh8uDCcd-6MNwXF4W_7noWXFZAfHkxZsRGC9Xs");
}

TEST(AcmeJws, WycheproofSignatures) {
    std::string doc = read_oracle("json_web_signature_test.json");
    if (doc.empty()) {
        GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/json_web_signature_test.json";
    }
    auto j = encoding::json::parse(string(doc));
    ASSERT_TRUE(j.has_value());
    size_t cases = 0;
    for (const auto& g : (*j)["testGroups"].elements()) {
        const auto& pub = g["public"];
        std::string alg = text(pub["alg"].as_string(string()));
        if (alg != "ES256" && alg != "RS256") {
            continue;
        }
        auto key = jd::jwk_key(pub);
        ASSERT_TRUE(key.has_value());
        EXPECT_EQ(std::string(key->alg()), alg);
        for (const auto& t : g["tests"].elements()) {
            std::string compact = text(t["jws"].as_string(string()));
            std::string result = text(t["result"].as_string(string()));
            size_t d1 = compact.find('.'), d2 = compact.rfind('.');
            bool valid = false;
            if (d1 != std::string::npos && d2 != d1) {
                auto header = encoding::base64::raw_url.decode(string(compact.substr(0, d1)));
                auto sig = encoding::base64::raw_url.decode(string(compact.substr(d2 + 1)));
                if (header && sig) {
                    auto h = encoding::json::parse(string(std::string_view(reinterpret_cast<const char*>(header->data()), header->size())));
                    if (h && h->is_object() && text((*h)["alg"].as_string(string())) == alg && (*h)["crit"].is_null()) {
                        std::string input = compact.substr(0, d2);
                        valid = jd::jws_verify(*key, alg, slice<const byte>(reinterpret_cast<const byte*>(input.data()), input.size()), sig->as_slice());
                    }
                }
            }
            EXPECT_EQ(valid, result == "valid") << "tcId " << t["tcId"].as_int(0) << " " << text(t["comment"].as_string(string()));
            ++cases;
        }
    }
    EXPECT_GT(cases, 200u);
    std::printf("[ wycheproof ] json_web_signature ES256, RS256: %zu cases\n", cases);
}

TEST(AcmeJws, EveryKindSignsAndVerifies) {
    for (auto alg : {net::acme::key_algorithm::es256, net::acme::key_algorithm::es384, net::acme::key_algorithm::eddsa, net::acme::key_algorithm::rs256}) {
        net::acme::account_key key(alg);
        const auto& st = jd::KeyAccess::state(key);
        // the JWK: RFC 7638's members in their order, read back the same
        auto parsed = jd::jwk_key(*encoding::json::parse(key.jwk()));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(text(parsed->canonical), text(key.jwk()));
        EXPECT_EQ(text(parsed->thumbprint), text(key.thumbprint()));
        EXPECT_EQ(std::string(parsed->alg()), std::string(st.alg));
        // a request as the client signs it, read as the server reads it
        string header = jd::header(st, string(), string("nonce-1"), string("https://ca.test/acme/new-account"));
        string body = jd::jws(*st.key, header, string(R"({"termsOfServiceAgreed":true})"));
        auto p = jd::parse_jws(body);
        ASSERT_TRUE(p.has_value()) << text(p.error());
        EXPECT_EQ(text(p->alg), std::string(st.alg));
        EXPECT_EQ(text(p->nonce), "nonce-1");
        EXPECT_EQ(text(p->url), "https://ca.test/acme/new-account");
        EXPECT_TRUE(p->kid.empty());
        EXPECT_FALSE(p->jwk.is_null());
        EXPECT_EQ(text(p->payload), R"({"termsOfServiceAgreed":true})");
        string input = p->signing_input();
        EXPECT_TRUE(jd::jws_verify(*parsed, p->alg.view(), slice<const byte>(reinterpret_cast<const byte*>(input.data()), input.size()), p->signature.as_slice()));
        // with a kid, and a POST-as-GET's empty payload
        string h2 = jd::header(st, string("https://ca.test/acme/acct/1"), string("n2"), string("https://ca.test/acme/order/1"));
        auto p2 = jd::parse_jws(jd::jws(*st.key, h2, string()));
        ASSERT_TRUE(p2.has_value());
        EXPECT_EQ(text(p2->kid), "https://ca.test/acme/acct/1");
        EXPECT_TRUE(p2->jwk.is_null());
        EXPECT_TRUE(p2->payload.empty());
        EXPECT_EQ(text(p2->payload_b64), "");
        // PEM both ways: the same key
        auto pem = key.to_pem();
        auto back = net::acme::account_key::from_pem(pem.as_slice());
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == key);
        EXPECT_EQ(back->algorithm(), alg);
    }
}

TEST(AcmeJws, ExternalAccountBindingMac) {
    net::acme::account_key key;
    const auto& st = jd::KeyAccess::state(key);
    auto mac = crypto::random::bytes(32);
    string eab = jd::eab(st, string("kid-1"), mac.as_slice(), string("https://ca.test/acme/new-account"));
    auto p = jd::parse_jws(eab);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(text(p->alg), "HS256");
    EXPECT_EQ(text(p->kid), "kid-1");
    EXPECT_TRUE(p->nonce.empty());
    EXPECT_EQ(text(p->payload), text(key.jwk()));
    string input = p->signing_input();
    auto tag = crypto::hmac<crypto::sha256>::of(slice<const byte>(reinterpret_cast<const byte*>(input.data()), input.size()), mac.as_slice());
    EXPECT_TRUE(crypto::constant_time::equal(tag, p->signature.as_slice()));
}

TEST(AcmeJws, KeysThatAreRefused) {
    // PEM that is no key, a key of no kind ACME signs with, RSA under 2048 bits
    auto none = net::acme::account_key::from_pem(slice<const byte>());
    ASSERT_FALSE(none.has_value());
    auto x = crypto::x25519::private_key::generate();
    auto xpem = x.to_pem();
    auto x25519 = net::acme::account_key::from_pem(xpem.as_slice());
    ASSERT_FALSE(x25519.has_value());
    EXPECT_EQ(x25519.error().code(), crypto::errc::malformed);
    // JWKs refused by the verifier
    for (const char* jwk : {R"({"kty":"EC","crv":"P-521","x":"AA","y":"AA"})", R"({"kty":"EC","crv":"P-256","x":"AA","y":"AA"})", R"({"kty":"OKP","crv":"X25519","x":"AA"})",
                            R"({"kty":"RSA","n":"AQAB","e":"AQAB"})", R"({"kty":"oct","k":"AA"})", R"([])", R"({"kty":7})"}) {
        auto j = encoding::json::parse(string(jwk));
        ASSERT_TRUE(j.has_value());
        EXPECT_FALSE(jd::jwk_key(*j).has_value()) << jwk;
    }
    // JWS that are not ones
    for (const char* body : {"", "[]", R"({"protected":"e30","payload":""})", R"({"protected":"!!","payload":"","signature":""})", R"({"protected":"e30","payload":"","signature":""})",
                             R"({"protected":"eyJhbGciOiJFUzI1NiJ9","payload":"","signature":"","header":{}})"}) {
        EXPECT_FALSE(jd::parse_jws(string(body)).has_value()) << body;
    }
}
