//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What acme::client makes of answers that break RFC 8555, from a CA of the
// test's own that answers as it is told: a directory that is not JSON or
// lacks a resource, objects without what the RFC requires, a problem of an
// unknown type and one without a document, a 503 with a Retry-After as an
// HTTP date, an account without its Location, Link fields of several
// relations, a chain that is not PEM; and the readers of the objects
// themselves on the edges of what they take.
#include "acme_test.h"
#include "sgcl/net/acme/detail/parse.h"

#include <atomic>

using namespace sgcl;
using namespace acme_test;
namespace ad = sgcl::net::acme::detail;

namespace {
    // A CA whose every answer is the test's: the directory at /dir, a nonce
    // with every answer, the rest of the routes the test sets
    struct Scripted {
        net::http::server srv;
        optional<net::listener> l;
        optional<async::task<expected<void, io::error>>> serving;
        std::string base;

        explicit Scripted(const std::string& directory_json = {}) {
            srv.on_error = [](const string&) {};
            l = *net::tcp::listen("127.0.0.1:0");
            base = "http://127.0.0.1:" + std::to_string(l->local_endpoint().port());
            std::string dir = directory_json.empty()
                                ? "{\"newNonce\":\"" + base + "/nonce\",\"newAccount\":\"" + base + "/account\",\"newOrder\":\"" + base +
                                      "/order\",\"revokeCert\":\"" + base + "/revoke\"}"
                                : directory_json;
            srv.route("GET /dir", [dir](net::http::request, net::http::response_writer w) {
                w.set_header("Content-Type", "application/json");
                w.write(string(dir));
            });
            srv.route("HEAD /nonce", [](net::http::request, net::http::response_writer w) {
                w.set_header("Replay-Nonce", "n");
            });
        }

        template<class H>
        void on(const std::string& pattern, H h) {
            srv.route(string(pattern), [h](net::http::request req, net::http::response_writer w) -> async::task<> {
                (void)co_await req.async_text();
                w.set_header("Replay-Nonce", "n");
                h(req, w);
            });
        }

        string url() const {
            return string(base + "/dir");
        }

        ~Scripted() {
            srv.close();
            (void)serving->wait();
        }

        void start() {
            serving = async::spawn(srv.async_serve(*l));
        }
    };

    net::acme::client::options known(const Scripted& s) {
        net::acme::client::options o;
        o.account_url = string(s.base + "/acct/1");
        o.poll_interval = std::chrono::milliseconds(20);
        o.poll_timeout = std::chrono::seconds(2);
        return o;
    }
}

TEST(AcmeResponses, DirectoriesThatAreNotOnes) {
    for (std::string body : {std::string("not json"), std::string("[]"), std::string("{\"newNonce\":\"x\"}"), std::string("{\"newNonce\":1,\"newAccount\":\"a\",\"newOrder\":\"o\"}"),
                             std::string("{\"newNonce\":\"n\",\"newAccount\":\"a\",\"newOrder\":\"o\",\"meta\":{\"externalAccountRequired\":\"yes\"}}")}) {
        Scripted s(body);
        s.start();
        net::acme::client c(s.url(), net::acme::account_key());
        auto d = c.directory();
        ASSERT_FALSE(d.has_value()) << body;
        EXPECT_EQ(d.error().code(), net::acme::errc::malformed_response) << body;
    }
    // relative URLs made absolute
    Scripted s("{\"newNonce\":\"/n\",\"newAccount\":\"/a\",\"newOrder\":\"o\",\"meta\":{\"caaIdentities\":[\"ca.test\"],\"website\":\"https://ca.test\"}}");
    s.start();
    net::acme::client c(s.url(), net::acme::account_key());
    auto d = c.directory();
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(text(d->new_nonce), s.base + "/n");
    EXPECT_EQ(text(d->new_order), s.base + "/o");
    EXPECT_EQ(d->caa_identities.size(), 1u);
    EXPECT_EQ(text(d->website), "https://ca.test");
}

TEST(AcmeResponses, ProblemsAndRetries) {
    Scripted s;
    std::atomic<int> calls = 0;
    s.on("POST /order", [&calls](net::http::request, net::http::response_writer w) {
        int n = calls.fetch_add(1);
        if (n == 0) {
            // a 503 whose Retry-After is an HTTP date two seconds away: waited for
            w.set_header("Retry-After", time::datetime::from_unix(time::now().unix() + 2, time::zone::utc()).format(time::http));
            w.set_status(503);
            return;
        }
        if (n == 1) {
            // a problem of a type the RFC does not have
            w.set_header("Content-Type", "application/problem+json");
            w.set_status(400);
            w.write("{\"type\":\"urn:example:other\",\"detail\":\"something else\"}");
            return;
        }
        if (n == 2) {
            // a problem without a document
            w.set_status(500);
            w.write("oops");
            return;
        }
        // an order without its finalize
        w.set_status(201);
        w.set_header("Location", "/o/1");
        w.write("{\"status\":\"pending\",\"identifiers\":[],\"authorizations\":[]}");
    });
    s.start();
    net::acme::client c(s.url(), net::acme::account_key(), known(s));
    auto unknown = c.new_order({"example.test"});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code(), net::acme::errc::unknown_problem);
    EXPECT_NE(text(unknown.error().message()).find("something else"), std::string::npos);
    EXPECT_EQ(calls.load(), 2);   // the 503 waited for and sent again
    auto bare = c.new_order({"example.test"});
    ASSERT_FALSE(bare.has_value());
    EXPECT_EQ(bare.error().code(), net::acme::errc::unknown_problem);
    EXPECT_NE(text(bare.error().message()).find("HTTP 500"), std::string::npos);
    auto broken = c.new_order({"example.test"});
    ASSERT_FALSE(broken.has_value());
    EXPECT_EQ(broken.error().code(), net::acme::errc::malformed_response);
    EXPECT_EQ(text(broken.error().op()), "acme new-order");
}

TEST(AcmeResponses, WhatTheClientNeedsOfAnAnswer) {
    Scripted s;
    s.on("POST /account", [](net::http::request, net::http::response_writer w) {
        w.set_status(201);   // no Location
        w.write("{\"status\":\"valid\"}");
    });
    s.on("POST /cert/1", [](net::http::request, net::http::response_writer w) {
        w.add_header("Link", "</cert/1/a>;rel=\"alternate\", </cert/1/b> ; rel=alternate");
        w.add_header("Link", "</dir>;rel=\"index\", </cert/1/c>;rel=\"up alternate\"");
        w.write("not a chain");
    });
    s.on("POST /cert/2", [](net::http::request, net::http::response_writer w) {
        w.write("-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n");
    });
    s.on("POST /cert/3", [](net::http::request, net::http::response_writer w) {
        w.write("");
    });
    s.on("POST /authz/1", [](net::http::request, net::http::response_writer w) {
        w.write("{\"identifier\":{\"type\":\"dns\",\"value\":\"a.test\"},\"status\":\"pending\",\"challenges\":[{\"type\":\"http-01\",\"url\":\"/c\",\"status\":\"pending\"}]}");
    });
    s.on("POST /authz/2", [](net::http::request, net::http::response_writer w) {
        w.write("{\"identifier\":{\"type\":\"dns\",\"value\":\"a.test\"},\"status\":\"blue\",\"challenges\":[]}");
    });
    s.start();
    net::acme::client c(s.url(), net::acme::account_key());
    auto a = c.register_account({.terms_agreed = true});
    ASSERT_FALSE(a.has_value());
    EXPECT_EQ(a.error().code(), net::acme::errc::malformed_response);
    net::acme::client k(s.url(), net::acme::account_key(), known(s));
    auto chain = k.certificate(string(s.base + "/cert/1"));
    ASSERT_FALSE(chain.has_value());
    EXPECT_EQ(chain.error().code(), net::acme::errc::malformed_response);
    for (const char* u : {"/cert/2", "/cert/3"}) {
        auto other = k.certificate(string(s.base + u));
        ASSERT_FALSE(other.has_value()) << u;
        EXPECT_EQ(other.error().code(), net::acme::errc::malformed_response) << u;
    }
    // the links of several relations, several a field
    net::http::headers h;
    h.add("Link", "</cert/1/a>;rel=\"alternate\", </cert/1/b> ; rel=alternate");
    h.add("Link", "</dir>;rel=\"index\", </cert/1/c>;rel=\"up alternate\"");
    auto alts = ad::links_of(h, "alternate", string(s.base + "/cert/1"));
    ASSERT_EQ(alts.size(), 3u);
    EXPECT_EQ(text(alts[0]), s.base + "/cert/1/a");
    EXPECT_EQ(text(alts[2]), s.base + "/cert/1/c");
    EXPECT_EQ(ad::links_of(h, "up", string(s.base)).size(), 1u);
    EXPECT_TRUE(ad::links_of(h, "next", string(s.base)).empty());
    // a challenge of §8 without a token, a status of no state
    for (const char* u : {"/authz/1", "/authz/2"}) {
        auto z = k.authorization(string(s.base + u));
        ASSERT_FALSE(z.has_value()) << u;
        EXPECT_EQ(z.error().code(), net::acme::errc::malformed_response) << u;
    }
}

TEST(AcmeResponses, TheReadersOnTheirEdges) {
    auto j = [](const char* text) {
        return *encoding::json::parse(string(text));
    };
    // a problem: about:blank without a type, a status past HTTP's refused
    auto p = ad::parse_problem(j("{}"));
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(text(p->type), "about:blank");
    EXPECT_EQ(p->code(), net::acme::errc::unknown_problem);
    EXPECT_FALSE(ad::parse_problem(j("{\"status\":1000}")).has_value());
    EXPECT_FALSE(ad::parse_problem(j("{\"subproblems\":{}}")).has_value());
    EXPECT_FALSE(ad::parse_problem(j("[]")).has_value());
    auto sub = ad::parse_problem(j("{\"type\":\"urn:ietf:params:acme:error:compound\",\"subproblems\":[{\"type\":\"urn:ietf:params:acme:error:caa\",\"detail\":\"no\",\"identifier\":{\"type\":\"dns\",\"value\":\"a.test\"}}]}"));
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->code(), net::acme::errc::compound);
    auto e = ad::problem_error(*sub, "acme x", duration::zero());
    EXPECT_EQ(e.code(), net::acme::errc::compound);
    EXPECT_NE(text(e.message()).find("a.test: no"), std::string::npos);
    // every type of RFC 8555 §6.7 and the client's codes have a text
    for (int c = 1; c <= int(net::acme::errc::host_not_allowed); ++c) {
        EXPECT_NE(net::acme::category().message(c), "acme: unknown error") << c;
    }
    EXPECT_EQ(net::acme::category().message(0), "acme: unknown error");
    // times of RFC 3339 with fractions; a window backwards refused
    auto o = ad::parse_order(j("{\"status\":\"ready\",\"expires\":\"2026-10-05T10:00:00.123456Z\",\"identifiers\":[],\"authorizations\":[],\"finalize\":\"f\"}"), string("u"));
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->expires->unix(), 1791194400);
    EXPECT_FALSE(ad::parse_renewal_info(j("{\"suggestedWindow\":{\"start\":\"2026-10-06T00:00:00Z\",\"end\":\"2026-10-05T00:00:00Z\"}}")).has_value());
    EXPECT_FALSE(ad::parse_renewal_info(j("{\"suggestedWindow\":{\"start\":\"2026-10-05\"}}")).has_value());
    // every status by its name, an unknown one refused
    for (const char* s : {"pending", "ready", "processing", "valid", "invalid", "revoked", "deactivated", "expired"}) {
        auto st = ad::status_of(s);
        ASSERT_TRUE(st.has_value());
        EXPECT_STREQ(net::acme::to_string(*st), s);
    }
    EXPECT_FALSE(ad::status_of("Valid").has_value());
    EXPECT_STREQ(net::acme::to_string(net::acme::status(200)), "unknown");
    // Retry-After: seconds, a date, garbage, a year at most
    net::http::headers h;
    h.set("Retry-After", "120");
    EXPECT_EQ(ad::retry_after_of(h), std::chrono::seconds(120));
    h.set("Retry-After", "soon");
    EXPECT_EQ(ad::retry_after_of(h), duration::zero());
    h.set("Retry-After", "99999999999");
    EXPECT_EQ(ad::retry_after_of(h), std::chrono::hours(365 * 24));
    h.set("Retry-After", "Thu, 01 Jan 1970 00:00:00 GMT");
    EXPECT_EQ(ad::retry_after_of(h), duration::zero());
}
