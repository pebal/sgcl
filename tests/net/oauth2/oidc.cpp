//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::oidc: against the Go server (go_oauth/main.go) — discovery, the
// authorization code grant with "openid" and a nonce, the ID token it
// signs with ES256 verified, a wrong nonce and a wrong client refused,
// userinfo with its subject — and against a provider of the library's whose
// tokens the test signs with crypto::jose: a rotated key fetched again (at
// most once a minute), an expired token, a token of another issuer, several
// audiences without azp, a token without iat, "none", discovery naming
// another issuer or no jwks_uri, request_secrets.
#include "tests/net/oauth2/go_as.h"
#include "sgcl/net/oidc.h"
#include "sgcl/crypto/jose.h"

#include <atomic>
#include <string>

using namespace sgcl;
using oauth2_test::GoAs;
using oauth2_test::text;

TEST(Oidc_Tests, AgainstGo) {
    GoAs as;
    if (as.base.empty()) {
        GTEST_SKIP() << "no go";
    }
    auto p = net::oidc::provider::discover(sgcl::string(as.base));
    ASSERT_TRUE(p) << text(p.error().message());
    EXPECT_EQ(text(p->issuer()), as.base);
    EXPECT_EQ(text(p->endpoints().token), as.base + "/token");
    EXPECT_EQ(p->metadata()["code_challenge_methods_supported"][0].as_string(""), "S256");
    auto cfg = p->config("web", "web-secret", "http://127.0.0.1:1/callback", {sgcl::string("email")});
    ASSERT_EQ(cfg.scopes.size(), 2u);
    EXPECT_EQ(cfg.scopes[0], "openid");
    auto secrets = net::oidc::request_secrets::generate();
    auto pk = net::oauth2::pkce::generate();
    auto [code, state] = as.authorize(cfg.authorization_url(secrets.state, pk, {{sgcl::string("nonce"), secrets.nonce}}));
    ASSERT_FALSE(code.empty());
    EXPECT_EQ(state, text(secrets.state));
    auto t = cfg.exchange(sgcl::string(code), pk);
    ASSERT_TRUE(t) << text(t.error().message());
    ASSERT_FALSE(t->id_token.empty());
    auto id = p->verify(t->id_token, {.client_id = "web", .nonce = secrets.nonce});
    ASSERT_TRUE(id) << text(id.error().message());
    EXPECT_EQ(id->subject, "ann");
    EXPECT_EQ(text(id->issuer), as.base);
    ASSERT_EQ(id->audience.size(), 1u);
    EXPECT_EQ(id->audience[0], "web");
    EXPECT_EQ(id->nonce, secrets.nonce);
    EXPECT_EQ(id->claims["email"].as_string(""), "ann@example.org");
    EXPECT_GT(id->expiry.unix(), id->issued_at.unix());
    auto wrong_nonce = p->verify(t->id_token, {.client_id = "web", .nonce = "other"});
    ASSERT_FALSE(wrong_nonce);
    EXPECT_EQ(wrong_nonce.error().code(), "invalid_id_token");
    auto wrong_client = p->verify(t->id_token, {.client_id = "spa"});
    ASSERT_FALSE(wrong_client);
    EXPECT_EQ(wrong_client.error().code(), "invalid_id_token");
    std::string tampered = text(t->id_token);
    tampered[tampered.size() / 2] = tampered[tampered.size() / 2] == 'A' ? 'B' : 'A';
    EXPECT_FALSE(p->verify(sgcl::string(tampered), {.client_id = "web"}));
    auto info = p->userinfo(*t, id->subject);
    ASSERT_TRUE(info) << text(info.error().message());
    EXPECT_EQ((*info)["name"].as_string(""), "Ann Example");
    auto other = p->userinfo(*t, "bob");
    ASSERT_FALSE(other);
    EXPECT_EQ(other.error().code(), "invalid_userinfo");
    net::oauth2::token bad;
    bad.access_token = "nope";
    auto refused = p->userinfo(bad);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().status(), 401);
}

namespace jose = sgcl::crypto::jose;

TEST(Oidc_Tests, Boundaries) {
    net::http::server srv;
    sgcl::string jwks;
    sgcl::string issuer;
    std::atomic<int> fetched{0};
    bool with_jwks = true;
    bool other_issuer = false;
    sgcl::string* jwks_p = &jwks;
    sgcl::string* issuer_p = &issuer;
    std::atomic<int>* fetched_p = &fetched;
    bool* with_p = &with_jwks;
    bool* other_p = &other_issuer;
    srv.route("GET /.well-known/openid-configuration", [=](net::http::request, net::http::response_writer w) {
        std::string m = "{\"issuer\":\"" + text(*other_p ? sgcl::string("https://elsewhere") : *issuer_p) + "\"";
        if (*with_p) {
            m += ",\"jwks_uri\":\"" + text(*issuer_p) + "/keys\"";
        }
        m += ",\"token_endpoint\":\"" + text(*issuer_p) + "/token\"}";
        w.write(sgcl::string(m));
    });
    srv.route("GET /keys", [=](net::http::request, net::http::response_writer w) {
        ++*fetched_p;
        w.write(*jwks_p);
    });
    net::http::test_server ts(srv);
    issuer = ts.url();

    auto k1 = jose::jwk::generate(jose::algorithm::es256, {.kid = "k1"});
    auto k2 = jose::jwk::generate(jose::algorithm::es256, {.kid = "k2"});
    jwks = jose::jwk_set{k1.public_key()}.to_json();
    auto claims = [&](int64_t exp_in, bool iat = true) {
        auto c = encoding::json::object({{"iss", issuer}, {"sub", "ann"}, {"aud", "web"}, {"exp", time::now().unix() + exp_in}});
        return iat ? c.set("iat", time::now().unix()) : c;
    };
    auto p = net::oidc::provider::discover(issuer);
    ASSERT_TRUE(p) << text(p.error().message());
    EXPECT_EQ(fetched.load(), 0);   // the keys when first needed
    auto good = jose::jwt::sign(claims(3600), k1);
    ASSERT_TRUE(p->verify(good, {.client_id = "web"}));
    EXPECT_EQ(fetched.load(), 1);
    ASSERT_TRUE(p->verify(good, {.client_id = "web"}));
    EXPECT_EQ(fetched.load(), 1);   // kept
    // a token of a key the set lacks: fetched again only after a minute
    jwks = jose::jwk_set{k1.public_key(), k2.public_key()}.to_json();
    auto rotated = jose::jwt::sign(claims(3600), k2);
    EXPECT_FALSE(p->verify(rotated, {.client_id = "web"}));
    EXPECT_EQ(fetched.load(), 1);
    // a fresh provider fetches at once and takes it
    auto q = net::oidc::provider::discover(issuer);
    ASSERT_TRUE(q);
    EXPECT_TRUE(q->verify(rotated, {.client_id = "web"}));
    // the claims
    auto expired = q->verify(jose::jwt::sign(claims(-3600), k1), {.client_id = "web"});
    ASSERT_FALSE(expired);
    EXPECT_EQ(expired.error().code(), "invalid_id_token");
    EXPECT_TRUE(q->verify(jose::jwt::sign(claims(-30), k1), {.client_id = "web"}));   // within the minute of leeway
    EXPECT_FALSE(q->verify(jose::jwt::sign(claims(-30), k1), {.client_id = "web", .leeway = std::chrono::seconds(5)}));
    EXPECT_FALSE(q->verify(jose::jwt::sign(claims(3600, false), k1), {.client_id = "web"}));   // no iat
    auto foreign = claims(3600);
    foreign = foreign.set("iss", "https://elsewhere");
    EXPECT_FALSE(q->verify(jose::jwt::sign(foreign, k1), {.client_id = "web"}));
    auto many = claims(3600);
    many = many.set("aud", encoding::json::array({"web", "api"}));
    EXPECT_FALSE(q->verify(jose::jwt::sign(many, k1), {.client_id = "web"}));   // several audiences, no azp
    many = many.set("azp", "web");
    EXPECT_TRUE(q->verify(jose::jwt::sign(many, k1), {.client_id = "web"}));
    many = many.set("azp", "api");
    EXPECT_FALSE(q->verify(jose::jwt::sign(many, k1), {.client_id = "web"}));
    auto nosub = claims(3600);
    nosub = nosub.erase("sub");
    EXPECT_FALSE(q->verify(jose::jwt::sign(nosub, k1), {.client_id = "web"}));
    // "none" and what is not a token
    EXPECT_FALSE(q->verify("eyJhbGciOiJub25lIn0.eyJzdWIiOiJhbm4ifQ.", {.client_id = "web"}));
    EXPECT_FALSE(q->verify("", {.client_id = "web"}));
    EXPECT_FALSE(q->verify("a.b.c", {.client_id = "web"}));
    // a nonce asked for and absent
    EXPECT_FALSE(q->verify(good, {.client_id = "web", .nonce = "n"}));
    // discovery: another issuer, no jwks_uri, nothing listening
    other_issuer = true;
    auto elsewhere = net::oidc::provider::discover(issuer);
    ASSERT_FALSE(elsewhere);
    EXPECT_EQ(elsewhere.error().code(), "invalid_issuer");
    other_issuer = false;
    with_jwks = false;
    auto nokeys = net::oidc::provider::discover(issuer);
    ASSERT_FALSE(nokeys);
    EXPECT_EQ(nokeys.error().code(), "invalid_metadata");
    EXPECT_TRUE(net::oidc::provider::discover("http://127.0.0.1:1").error().transport());
    // a key set that does not parse: the verification's error
    with_jwks = true;
    jwks = "not json";
    auto broken = net::oidc::provider::discover(issuer);
    ASSERT_TRUE(broken);
    auto v = broken->verify(good, {.client_id = "web"});
    ASSERT_FALSE(v);
    EXPECT_EQ(v.error().code(), "invalid_jwks");
    EXPECT_FALSE(broken->userinfo(net::oauth2::token()).error().transport() == nullopt);   // no userinfo endpoint
    // the request's secrets
    auto a = net::oidc::request_secrets::generate();
    auto b = net::oidc::request_secrets::generate();
    EXPECT_EQ(a.state.size(), 22u);
    EXPECT_EQ(a.nonce.size(), 22u);
    EXPECT_NE(a.state, b.state);
    EXPECT_NE(a.state, a.nonce);
    ts.close();
}
