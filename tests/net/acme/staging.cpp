//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Against Let's Encrypt's staging CA, by hand: it needs the internet, a DNS
// name of this machine and its port 80 reachable from the internet (http-01),
// so it is DISABLED_ and never runs by default. To run it:
//
//   SGCL_ACME_STAGING_DOMAIN=host.example.com \
//   SGCL_ACME_STAGING_LISTEN=:80 \
//   build-rel/tests/tests_acme --gtest_also_run_disabled_tests --gtest_filter='*Staging*'
//
// SGCL_ACME_STAGING_LISTEN is where the http-01 handler listens (":80" by
// default, which needs the privilege or a port forwarded to another one);
// SGCL_ACME_STAGING_CONTACT an email for the account (none by default). A
// certificate is obtained by the manager (the account's terms agreed to),
// its chain checked to be the staging CA's, and it is revoked at the end.
#include "acme_test.h"

#include <cstdlib>

using namespace sgcl;
using namespace acme_test;

TEST(AcmeStaging, DISABLED_LetsEncryptStaging) {
    const char* domain = std::getenv("SGCL_ACME_STAGING_DOMAIN");
    if (!domain || !*domain) {
        GTEST_SKIP() << "SGCL_ACME_STAGING_DOMAIN is not set";
    }
    const char* listen = std::getenv("SGCL_ACME_STAGING_LISTEN");
    const char* contact = std::getenv("SGCL_ACME_STAGING_CONTACT");
    net::acme::manager::options o;
    o.directory_url = net::acme::lets_encrypt_staging_url;
    o.accept_terms = true;
    o.challenges = {"http-01"};
    if (contact && *contact) {
        o.contact = {string::concat("mailto:", contact)};
    }
    net::acme::manager m({domain}, o);
    net::http::server http;
    http.route("/", m.http_handler());
    auto l = net::tcp::listen(listen && *listen ? listen : ":80");
    ASSERT_TRUE(l.has_value()) << text(l.error().message());
    auto serving = async::spawn(http.async_serve(*l));
    auto id = m.certificate(domain);
    ASSERT_TRUE(id.has_value()) << text(id.error().message());
    const auto& chain = id->certificates();
    ASSERT_GE(chain.size(), 2u);
    EXPECT_EQ(text(chain[0].dns_names()[0]), domain);
    EXPECT_NE(text(chain[1].subject().organization()[0]).find("STAGING"), std::string::npos) << text(chain[1].subject().organization()[0]);
    // the renewal information of the staging CA
    auto c = m.client();
    ASSERT_TRUE(c.has_value());
    auto info = c->renewal_info(chain[0]);
    EXPECT_TRUE(info.has_value()) << text(info.error().message());
    auto r = c->revoke(chain[0], net::acme::revocation_reason::cessation_of_operation);
    EXPECT_TRUE(r.has_value()) << text(r.error().message());
    m.close();
    http.close();
    (void)serving.wait();
}
