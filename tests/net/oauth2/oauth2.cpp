//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::oauth2 against an authorization server of Go's standard library on
// the loopback (go_oauth/main.go): the authorization code grant with PKCE
// (the redirect followed by hand, the state kept, the verifier checked),
// client_secret_basic and _post and a public client, client credentials,
// refresh (the refresh token rotated, kept when the server sends none), the
// device flow (authorization_pending, slow_down, access_denied, a stop),
// revocation and introspection, the server's errors as oauth2::error, a
// token source shared by many tasks (one refresh), and its http::client
// (Bearer on every request, a 401 invalid_token refreshed once). PKCE by
// RFC 7636 Appendix B. The boundaries against a server of the library's.
#include "tests/net/oauth2/go_as.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace sgcl;
using oauth2_test::GoAs;
using oauth2_test::text;


TEST(OAuth2_Tests, PkceOfTheRfc) {
    // RFC 7636 Appendix B: the verifier and its S256 challenge
    auto h = crypto::sha256::of(sgcl::string("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"));
    EXPECT_EQ(text(encoding::base64::raw_url.encode(slice<const byte>(h.data(), h.size()))), "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
    auto p = net::oauth2::pkce::generate();
    EXPECT_EQ(p.verifier.size(), 43u);
    auto h2 = crypto::sha256::of(p.verifier);
    EXPECT_EQ(p.challenge, encoding::base64::raw_url.encode(slice<const byte>(h2.data(), h2.size())));
    EXPECT_NE(net::oauth2::pkce::generate().verifier, p.verifier);
}

TEST(OAuth2_Tests, AuthorizationCodeWithPkce) {
    GoAs as;
    if (as.base.empty()) {
        GTEST_SKIP() << "no go";
    }
    for (auto auth : {net::oauth2::client_auth::basic, net::oauth2::client_auth::post}) {
        auto cfg = as.config("web", "web-secret");
        cfg.auth = auth;
        auto p = net::oauth2::pkce::generate();
        auto url = cfg.authorization_url("st-1", p, {{"nonce", "n-1"}});
        EXPECT_NE(text(url).find("code_challenge_method=S256"), std::string::npos);
        EXPECT_NE(text(url).find("scope=read+write"), std::string::npos);
        auto [code, state] = as.authorize(url);
        ASSERT_FALSE(code.empty());
        EXPECT_EQ(state, "st-1");
        auto t = cfg.exchange(sgcl::string(code), p);
        ASSERT_TRUE(t) << text(t.error().message());
        EXPECT_FALSE(t->access_token.empty());
        EXPECT_FALSE(t->refresh_token.empty());
        EXPECT_EQ(t->token_type, "Bearer");
        EXPECT_EQ(t->scope, "read write");
        EXPECT_TRUE(t->valid());
        ASSERT_TRUE(t->expiry);
        // the code once only, and a wrong verifier refused
        auto again = cfg.exchange(sgcl::string(code), p);
        ASSERT_FALSE(again);
        EXPECT_EQ(again.error().code(), "invalid_grant");
        EXPECT_EQ(again.error().status(), 400);
        auto [code2, state2] = as.authorize(cfg.authorization_url("st-2", p));
        auto wrong = cfg.exchange(sgcl::string(code2), net::oauth2::pkce::generate());
        ASSERT_FALSE(wrong);
        EXPECT_EQ(text(wrong.error().message()), "invalid_grant: PKCE verification failed");
    }
    // a public client, PKCE required
    auto spa = as.config("spa", "");
    spa.auth = net::oauth2::client_auth::none;
    auto p = net::oauth2::pkce::generate();
    auto [code, state] = as.authorize(spa.authorization_url("s", p));
    auto t = spa.exchange(sgcl::string(code), p);
    ASSERT_TRUE(t) << text(t.error().message());
    // a wrong secret: invalid_client, 401
    auto bad = as.config("web", "nope");
    auto refused = bad.client_credentials();
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), "invalid_client");
    EXPECT_EQ(refused.error().status(), 401);
}

TEST(OAuth2_Tests, CredentialsRefreshRevokeIntrospect) {
    GoAs as;
    if (as.base.empty()) {
        GTEST_SKIP() << "no go";
    }
    auto cfg = as.config("web", "web-secret");
    auto cc = cfg.client_credentials();
    ASSERT_TRUE(cc) << text(cc.error().message());
    EXPECT_TRUE(cc->refresh_token.empty());
    auto p = net::oauth2::pkce::generate();
    auto [code, st] = as.authorize(cfg.authorization_url("s", p));
    auto t = cfg.exchange(sgcl::string(code), p);
    ASSERT_TRUE(t);
    auto r = cfg.refresh(*t);
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_NE(r->access_token, t->access_token);
    EXPECT_NE(r->refresh_token, t->refresh_token);   // rotated
    auto old = cfg.refresh(*t);
    ASSERT_FALSE(old);
    EXPECT_EQ(old.error().code(), "invalid_grant");
    auto info = cfg.introspect(r->access_token);
    ASSERT_TRUE(info) << text(info.error().message());
    EXPECT_TRUE(info->active);
    EXPECT_EQ(info->client_id, "web");
    EXPECT_EQ(info->subject, "ann");
    ASSERT_EQ(info->audience.size(), 2u);
    EXPECT_TRUE(info->expiry);
    ASSERT_TRUE(cfg.revoke(r->access_token, "access_token"));
    auto gone = cfg.introspect(r->access_token);
    ASSERT_TRUE(gone);
    EXPECT_FALSE(gone->active);
    net::oauth2::token none;
    auto norefresh = cfg.refresh(none);
    ASSERT_FALSE(norefresh);
    EXPECT_EQ(norefresh.error().code(), "invalid_grant");
    // no endpoint: the transport's error
    auto noend = cfg;
    noend.endpoints.revocation = "";
    auto e = noend.revoke("x");
    ASSERT_FALSE(e);
    EXPECT_TRUE(e.error().transport());
    // nothing listening
    auto dead = cfg;
    dead.endpoints.token = "http://127.0.0.1:1/token";
    auto d = dead.client_credentials();
    ASSERT_FALSE(d);
    EXPECT_TRUE(d.error().transport());
}

TEST(OAuth2_Tests, DeviceFlow) {
    GoAs as;
    if (as.base.empty()) {
        GTEST_SKIP() << "no go";
    }
    auto cfg = as.config("web", "web-secret");
    auto d = cfg.device_authorize();
    ASSERT_TRUE(d) << text(d.error().message());
    EXPECT_EQ(d->user_code, "WDJB-MJHT");
    EXPECT_EQ(d->interval, duration(std::chrono::seconds(1)));
    auto t = cfg.device_token(*d);   // pending twice, then the token
    ASSERT_TRUE(t) << text(t.error().message());
    EXPECT_FALSE(t->access_token.empty());
    auto deny = cfg;
    deny.scopes = {sgcl::string("deny")};
    auto dd = deny.device_authorize();
    ASSERT_TRUE(dd);
    auto denied = deny.device_token(*dd);
    ASSERT_FALSE(denied);
    EXPECT_EQ(denied.error().code(), "access_denied");
    auto ds = cfg.device_authorize();
    ASSERT_TRUE(ds);
    async::stop_source stop;
    stop.request_stop();
    auto stopped = cfg.device_token(*ds, stop.token());
    ASSERT_FALSE(stopped);
    ASSERT_TRUE(stopped.error().transport());
    EXPECT_EQ(stopped.error().transport()->code(), std::errc::operation_canceled);
}

TEST(OAuth2_Tests, TheSourceAndItsClient) {
    GoAs as;
    if (as.base.empty()) {
        GTEST_SKIP() << "no go";
    }
    auto cfg = as.config("short", "short-secret");   // tokens of 12 s: expired to the client after 2 s
    auto p = net::oauth2::pkce::generate();
    auto [code, st] = as.authorize(cfg.authorization_url("s", p));
    auto t = cfg.exchange(sgcl::string(code), p);
    ASSERT_TRUE(t);
    auto source = cfg.source(*t);
    auto web = source.client();
    auto first = web.get(sgcl::string(as.base + "/api"));
    ASSERT_TRUE(first);
    EXPECT_EQ(*first->text(), "data for short");
    // many tasks at once after the token expired: one refresh, the same new token
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    vector<async::task<expected<net::oauth2::token, net::oauth2::error>>> all;
    for (int i : range(8)) {
        (void)i;
        all.push_back(async::spawn(source.async_token()));
    }
    sgcl::string seen;
    for (auto& task : all) {
        auto got = task.wait();
        ASSERT_TRUE(got) << text(got.error().message());
        if (seen.empty()) {
            seen = got->access_token;
        }
        EXPECT_EQ(got->access_token, seen);
    }
    EXPECT_NE(seen, t->access_token);
    auto after = web.get(sgcl::string(as.base + "/api"));
    ASSERT_TRUE(after);
    EXPECT_EQ(after->status(), 200);
    (void)after->text();
    // a token the server forgot (revoked): 401 invalid_token, refreshed and sent again once
    ASSERT_TRUE(cfg.revoke(seen));
    auto retried = web.get(sgcl::string(as.base + "/api"));
    ASSERT_TRUE(retried);
    EXPECT_EQ(retried->status(), 200);
    (void)retried->text();
    // a source of client credentials
    auto cc = as.config("web", "web-secret").source();
    auto ct = cc.token();
    ASSERT_TRUE(ct);
    EXPECT_FALSE(ct->access_token.empty());
    EXPECT_EQ(cc.token()->access_token, ct->access_token);   // kept while valid
}

// DESIGN 408: the boundaries, against a server of the library's answering
// what an authorization server should not (or may): a body that is not
// JSON, a JSON that is not an object, a token without access_token, a
// status without an OAuth error, a device answer without its codes; an
// empty revocation, an inactive token, an audience as one string; a
// refresh answered without a refresh token; a failed refresh through the
// token source and its client; the device's codes already expired; the
// authorization URL of an endpoint with a query; the values alone.
TEST(OAuth2_Tests, Boundaries) {
    net::http::server srv;
    auto answer = [&srv](const char* path, int status, const char* body) {
        sgcl::string b = body;
        srv.route(sgcl::string("POST ") + path, [status, b](net::http::request, net::http::response_writer w) {
            w.set_status(status);
            w.write(b);
        });
    };
    answer("/notjson", 200, "hello");
    answer("/array", 200, "[1, 2]");
    answer("/noaccess", 200, R"({"token_type":"Bearer"})");
    answer("/plain500", 500, "oops");
    answer("/denied", 400, R"({"error":"invalid_scope","error_description":"no admin","error_uri":"https://as/e"})");
    answer("/device_incomplete", 200, R"({"device_code":"d"})");
    answer("/device_zero", 200, R"({"device_code":"d","user_code":"U","verification_uri":"https://as/d","interval":0})");
    answer("/revoke", 200, "");
    answer("/inactive", 200, R"({"active":false})");
    answer("/aud", 200, R"({"active":true,"aud":"api","exp":"soon"})");
    answer("/rotate_none", 200, R"({"access_token":"at-2"})");
    net::http::test_server ts(srv);
    std::string base = text(ts.url());
    auto at = [&](const char* path) {
        net::oauth2::config c;
        c.client_id = "c";
        c.client_secret = "s";
        c.endpoints.token = sgcl::string(base + path);
        c.endpoints.device_authorization = sgcl::string(base + path);
        c.endpoints.revocation = sgcl::string(base + path);
        c.endpoints.introspection = sgcl::string(base + path);
        return c;
    };
    auto notjson = at("/notjson").client_credentials();
    ASSERT_FALSE(notjson);
    ASSERT_TRUE(notjson.error().transport());
    EXPECT_EQ(notjson.error().transport()->code(), net::errc::malformed_response);
    EXPECT_TRUE(notjson.error().code().empty());
    auto array = at("/array").client_credentials();
    ASSERT_FALSE(array);
    EXPECT_TRUE(array.error().transport());
    auto noaccess = at("/noaccess").client_credentials();
    ASSERT_FALSE(noaccess);
    EXPECT_EQ(noaccess.error().code(), "invalid_response");
    auto plain = at("/plain500").client_credentials();
    ASSERT_FALSE(plain);
    EXPECT_EQ(plain.error().code(), "http_status");
    EXPECT_EQ(plain.error().status(), 500);
    EXPECT_EQ(text(plain.error().message()), "http_status: 500");
    auto denied = at("/denied").client_credentials();
    ASSERT_FALSE(denied);
    EXPECT_EQ(denied.error().code(), "invalid_scope");
    EXPECT_EQ(denied.error().description(), "no admin");
    EXPECT_EQ(denied.error().uri(), "https://as/e");
    EXPECT_EQ(denied.error().status(), 400);
    EXPECT_FALSE(denied.error().transport());
    auto incomplete = at("/device_incomplete").device_authorize();
    ASSERT_FALSE(incomplete);
    EXPECT_EQ(incomplete.error().code(), "invalid_response");
    auto zero = at("/device_zero").device_authorize();
    ASSERT_TRUE(zero);
    EXPECT_EQ(zero->interval, duration(std::chrono::seconds(5)));   // 0 is no interval: the RFC's default
    EXPECT_GT(zero->expiry.unix_milli(), time::now().unix_milli() + 590000);   // no expires_in: ten minutes
    EXPECT_TRUE(at("/revoke").revoke("t"));
    auto inactive = at("/inactive").introspect("t");
    ASSERT_TRUE(inactive);
    EXPECT_FALSE(inactive->active);
    EXPECT_TRUE(inactive->audience.empty());
    EXPECT_FALSE(inactive->expiry);
    auto aud = at("/aud").introspect("t");
    ASSERT_TRUE(aud);
    ASSERT_EQ(aud->audience.size(), 1u);
    EXPECT_EQ(aud->audience[0], "api");
    EXPECT_FALSE(aud->expiry);   // "soon" is not a time
    net::oauth2::token old;
    old.access_token = "at-1";
    old.refresh_token = "rt-1";
    auto kept = at("/rotate_none").refresh(old);
    ASSERT_TRUE(kept);
    EXPECT_EQ(kept->access_token, "at-2");
    EXPECT_EQ(kept->refresh_token, "rt-1");
    EXPECT_FALSE(kept->expiry);
    EXPECT_TRUE(kept->valid());

    // a failed refresh: the source's error, the client's io::error, and a second try
    net::oauth2::token expired;
    expired.access_token = "at-0";
    expired.refresh_token = "rt-0";
    expired.expiry = time::datetime::from_unix(1000, time::zone::utc());
    auto source = at("/denied").source(expired);
    auto failed = source.token();
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code(), "invalid_scope");
    auto web = source.client();
    auto refused = web.get(sgcl::string(base + "/anything"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), std::errc::permission_denied);
    EXPECT_NE(text(refused.error().message()).find("invalid_scope"), std::string::npos);
    EXPECT_FALSE(source.token());

    // the device's codes already expired: expired_token, nothing sent
    net::oauth2::device_authorization gone;
    gone.device_code = "d";
    gone.expiry = time::datetime::from_unix(1000, time::zone::utc());
    auto late = at("/rotate_none").device_token(gone);
    ASSERT_FALSE(late);
    EXPECT_EQ(late.error().code(), "expired_token");

    // the authorization URL: an endpoint with a query, the extra parameters in their order, no PKCE
    net::oauth2::config c;
    c.client_id = "a b";
    c.endpoints.authorization = "https://as/authorize?tenant=x";
    auto url = c.authorization_url("s&t", {}, {{sgcl::string("prompt"), sgcl::string("consent")}, {sgcl::string("nonce"), sgcl::string("n")}});
    EXPECT_EQ(text(url), "https://as/authorize?tenant=x&response_type=code&client_id=a+b&state=s%26t&prompt=consent&nonce=n");

    // the values alone
    net::oauth2::token none;
    EXPECT_FALSE(none.valid());
    none.access_token = "x";
    EXPECT_TRUE(none.valid());
    none.expiry = time::datetime::from_unix_milli(time::now().unix_milli() + 9000, time::zone::utc());
    EXPECT_FALSE(none.valid());   // within the margin of 10 s
    none.expiry = time::datetime::from_unix_milli(time::now().unix_milli() + 11000, time::zone::utc());
    EXPECT_TRUE(none.valid());
    net::oauth2::error empty;
    EXPECT_TRUE(empty.code().empty());
    EXPECT_TRUE(empty.message().empty());
    EXPECT_EQ(empty.status(), 0);
    EXPECT_EQ(text(net::oauth2::error("slow_down", "").message()), "slow_down");
    net::oauth2::config noend;
    auto nothing = noend.client_credentials();
    ASSERT_FALSE(nothing);
    ASSERT_TRUE(nothing.error().transport());
    EXPECT_EQ(nothing.error().transport()->code(), std::errc::invalid_argument);
}
