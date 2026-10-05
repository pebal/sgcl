//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// acme::client against acme::test_server on the loopback: every flow of RFC
// 8555 and its errors — the directory, accounts (new, existing, looked up,
// updated, deactivated; contact and terms refused), External Account
// Binding, key rollover, orders of DNS names, wildcards, IP addresses and
// IDNs, the three challenges solved for real (http-01 by the module's http
// server, tls-alpn-01 by its TLS listener, dns-01 by a board of records),
// an invalid challenge, the order's states with processing and Retry-After,
// finalize's refusals, the chain and its alternates, revocation (by the
// account, by the certificate's key, twice, a bad reason), the renewal
// information and a replacement (RFC 9773), badNonce retried, rateLimited
// waited for or refused, polling past its timeout, a CA over https, many
// orders at once, the async forms.
#include "acme_test.h"

#include <atomic>
#include <thread>

using namespace sgcl;
using namespace acme_test;

TEST(AcmeClient, TheDirectory) {
    net::acme::test_server::options so;
    so.terms_of_service = "https://ca.test/terms";
    so.profiles["classic"] = "the default";
    so.profiles["shortlived"] = "six days";
    net::acme::test_server ca(so);
    net::acme::client c(ca.directory_url(), net::acme::account_key());
    auto d = c.directory();
    ASSERT_TRUE(d.has_value()) << text(d.error().message());
    EXPECT_FALSE(d->new_nonce.empty());
    EXPECT_FALSE(d->new_account.empty());
    EXPECT_FALSE(d->new_order.empty());
    EXPECT_FALSE(d->revoke_cert.empty());
    EXPECT_FALSE(d->key_change.empty());
    EXPECT_FALSE(d->renewal_info.empty());
    EXPECT_TRUE(d->new_authz.empty());
    EXPECT_EQ(text(d->terms_of_service), "https://ca.test/terms");
    EXPECT_FALSE(d->external_account_required);
    EXPECT_EQ(d->profiles.size(), 2u);
    EXPECT_EQ(text(d->profiles["shortlived"]), "six days");
    EXPECT_EQ(text(c.directory_url()), text(ca.directory_url()));
    // a directory that is not there
    net::acme::client none("http://127.0.0.1:1/directory", net::acme::account_key());
    auto e = none.directory();
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), std::errc::connection_refused);
}

TEST(AcmeClient, Accounts) {
    net::acme::test_server::options so;
    so.terms_of_service = "https://ca.test/terms";
    net::acme::test_server ca(so);
    net::acme::account_key key;
    net::acme::client c(ca.directory_url(), key);
    EXPECT_TRUE(c.account_url().empty());
    // no account yet: looked up by the key, accountDoesNotExist
    auto missing = c.account();
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code(), net::acme::errc::account_does_not_exist);
    // the terms not agreed
    auto refused = c.register_account({.contact = {"mailto:a@example.test"}});
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), net::acme::errc::malformed);
    // a contact of another scheme, an address that is not one
    auto tel = c.register_account({.contact = {"tel:+48123"}, .terms_agreed = true});
    ASSERT_FALSE(tel.has_value());
    EXPECT_EQ(tel.error().code(), net::acme::errc::unsupported_contact);
    auto bad = c.register_account({.contact = {"mailto:nobody"}, .terms_agreed = true});
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code(), net::acme::errc::invalid_contact);
    auto a = c.register_account({.contact = {"mailto:a@example.test"}, .terms_agreed = true});
    ASSERT_TRUE(a.has_value()) << text(a.error().message());
    EXPECT_EQ(a->status, net::acme::status::valid);
    EXPECT_TRUE(a->terms_agreed);
    ASSERT_EQ(a->contact.size(), 1u);
    EXPECT_EQ(text(a->contact[0]), "mailto:a@example.test");
    EXPECT_FALSE(a->orders.empty());
    EXPECT_EQ(text(c.account_url()), text(a->url));
    // the same key again: the existing account
    net::acme::client again(ca.directory_url(), key);
    auto b = again.register_account({.terms_agreed = true});
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(text(b->url), text(a->url));
    // looked up by a client that does not know its URL
    net::acme::client looked(ca.directory_url(), key);
    auto l = looked.account();
    ASSERT_TRUE(l.has_value()) << text(l.error().message());
    EXPECT_EQ(text(l->url), text(a->url));
    // only_return_existing for a key without one
    net::acme::client other(ca.directory_url(), net::acme::account_key());
    auto o = other.register_account({.only_return_existing = true});
    ASSERT_FALSE(o.has_value());
    EXPECT_EQ(o.error().code(), net::acme::errc::account_does_not_exist);
    // the URL given through the options
    net::acme::client::options opts;
    opts.account_url = a->url;
    net::acme::client known(ca.directory_url(), key, opts);
    EXPECT_EQ(text(known.account_url()), text(a->url));
    // updated, then deactivated: nothing signed by it is taken
    auto u = c.update_account({"mailto:b@example.test", "mailto:c@example.test"});
    ASSERT_TRUE(u.has_value()) << text(u.error().message());
    ASSERT_EQ(u->contact.size(), 2u);
    EXPECT_EQ(text(u->contact[1]), "mailto:c@example.test");
    auto d = c.deactivate_account();
    ASSERT_TRUE(d.has_value()) << text(d.error().message());
    EXPECT_EQ(d->status, net::acme::status::deactivated);
    auto after = c.new_order({"example.test"});
    ASSERT_FALSE(after.has_value());
    EXPECT_EQ(after.error().code(), net::acme::errc::unauthorized);
}

TEST(AcmeClient, EveryKindOfAccountKey) {
    net::acme::test_server ca;
    for (auto alg : {net::acme::key_algorithm::es256, net::acme::key_algorithm::es384, net::acme::key_algorithm::eddsa, net::acme::key_algorithm::rs256}) {
        net::acme::account_key key(alg);
        EXPECT_EQ(key.algorithm(), alg);
        net::acme::client c(ca.directory_url(), key);
        auto a = c.register_account({.terms_agreed = true});
        ASSERT_TRUE(a.has_value()) << text(a.error().message());
        auto o = c.new_order({"example.test"});
        ASSERT_TRUE(o.has_value()) << text(o.error().message());
    }
}

TEST(AcmeClient, ExternalAccountBinding) {
    net::acme::test_server::options so;
    so.require_external_account = true;
    auto mac = crypto::random::bytes(32);
    so.external_accounts.push_back({"kid-1", encoding::base64::raw_url.encode(mac.as_slice())});
    net::acme::test_server ca(so);
    net::acme::client c(ca.directory_url(), net::acme::account_key());
    auto d = c.directory();
    ASSERT_TRUE(d.has_value());
    EXPECT_TRUE(d->external_account_required);
    // refused by the client itself before a request: the CA requires one
    auto none = c.register_account({.terms_agreed = true});
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::acme::errc::external_account_required);
    // an unknown key id, a wrong key, a key that is not base64url
    auto unknown = c.register_account({.terms_agreed = true, .external_account = net::acme::external_account{"kid-2", encoding::base64::raw_url.encode(mac.as_slice())}});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code(), net::acme::errc::unauthorized);
    auto wrong = c.register_account({.terms_agreed = true, .external_account = net::acme::external_account{"kid-1", encoding::base64::raw_url.encode(crypto::random::bytes(32).as_slice())}});
    ASSERT_FALSE(wrong.has_value());
    EXPECT_EQ(wrong.error().code(), net::acme::errc::unauthorized);
    auto garbled = c.register_account({.terms_agreed = true, .external_account = net::acme::external_account{"kid-1", "!!!"}});
    ASSERT_FALSE(garbled.has_value());
    EXPECT_EQ(garbled.error().code(), net::acme::errc::malformed);
    // the binding of the CA's key: the account made; a padded key taken too
    auto a = c.register_account({.terms_agreed = true, .external_account = net::acme::external_account{"kid-1", encoding::base64::url.encode(mac.as_slice())}});
    ASSERT_TRUE(a.has_value()) << text(a.error().message());
    EXPECT_FALSE(a->url.empty());
}

TEST(AcmeClient, KeyRollover) {
    net::acme::test_server ca;
    net::acme::account_key old_key;
    auto c = registered(ca, old_key);
    net::acme::account_key next(net::acme::key_algorithm::eddsa);
    auto r = c.change_key(next);
    ASSERT_TRUE(r.has_value()) << text(r.error().message());
    EXPECT_TRUE(c.key() == next);
    // the client goes on with the new key, the account the same
    auto o = c.new_order({"example.test"});
    ASSERT_TRUE(o.has_value()) << text(o.error().message());
    // the old key has no account any more
    net::acme::client by_old(ca.directory_url(), old_key);
    auto lookup = by_old.register_account({.only_return_existing = true});
    ASSERT_FALSE(lookup.has_value());
    EXPECT_EQ(lookup.error().code(), net::acme::errc::account_does_not_exist);
    net::acme::client by_new(ca.directory_url(), next);
    auto found = by_new.register_account({.only_return_existing = true});
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(text(found->url), text(c.account_url()));
    // a key that is another account's: refused, the client's key unchanged
    net::acme::account_key taken;
    auto other = registered(ca, taken);
    auto conflict = c.change_key(taken);
    ASSERT_FALSE(conflict.has_value());
    EXPECT_TRUE(c.key() == next);
}

TEST(AcmeClient, Http01EndToEnd) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    auto c = registered(ca);
    auto chain = issue(c, s, {"example.test", "www.example.test"}, "http-01");
    ASSERT_TRUE(chain.has_value()) << text(chain.error().message());
    ASSERT_EQ(chain->certificates.size(), 2u);
    EXPECT_TRUE(verifies(ca, *chain, "www.example.test"));
    EXPECT_GE(s.board->http_requests, 2);
    EXPECT_FALSE(chain->pem.empty());
    EXPECT_TRUE(chain->alternates.empty());
}

TEST(AcmeClient, TlsAlpn01EndToEnd) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    auto c = registered(ca);
    auto chain = issue(c, s, {"example.test"}, "tls-alpn-01");
    ASSERT_TRUE(chain.has_value()) << text(chain.error().message());
    EXPECT_TRUE(verifies(ca, *chain, "example.test"));
    EXPECT_GE(s.board->alpn_hellos, 1);
}

TEST(AcmeClient, Dns01AndAWildcard) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    auto c = registered(ca);
    auto chain = issue(c, s, {"*.example.test", "example.test"}, "dns-01");
    ASSERT_TRUE(chain.has_value()) << text(chain.error().message());
    EXPECT_TRUE(verifies(ca, *chain, "a.example.test"));
    EXPECT_TRUE(verifies(ca, *chain, "example.test"));
    // the wildcard's authorization: its name without "*.", dns-01 alone
    auto o = c.new_order({"*.other.test"});
    ASSERT_TRUE(o.has_value());
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(az.has_value());
    EXPECT_TRUE(az->wildcard);
    EXPECT_EQ(text(az->identifier.value), "other.test");
    ASSERT_EQ(az->challenges.size(), 1u);
    EXPECT_EQ(text(az->challenges[0].type), "dns-01");
    EXPECT_EQ(text(net::acme::client::dns01_name("*.other.test")), "_acme-challenge.other.test");
}

TEST(AcmeClient, IpAddressesAndNames) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    auto c = registered(ca);
    // RFC 8738: an address by http-01 and by tls-alpn-01 (its reverse name the SNI)
    auto by_http = issue(c, s, {"127.0.0.1"}, "http-01");
    ASSERT_TRUE(by_http.has_value()) << text(by_http.error().message());
    EXPECT_TRUE(verifies(ca, *by_http, "127.0.0.1"));
    {
        std::lock_guard<std::mutex> g(s.board->lock);
        s.board->alpn.clear();
    }
    auto o = c.new_order({"::1"});
    ASSERT_TRUE(o.has_value()) << text(o.error().message());
    ASSERT_EQ(o->identifiers.size(), 1u);
    EXPECT_EQ(text(o->identifiers[0].type), "ip");
    EXPECT_EQ(text(o->identifiers[0].value), "::1");
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(az.has_value());
    for (auto& ch : az->challenges) {
        EXPECT_NE(text(ch.type), "dns-01");
    }
    // the identifiers as the client writes them: lower case, no trailing
    // dot, an IDN in A-labels, an address in RFC 5952's text
    auto n = c.new_order({"Example.TEST.", "żółw.test", "[2001:DB8::1]"});
    ASSERT_TRUE(n.has_value()) << text(n.error().message());
    ASSERT_EQ(n->identifiers.size(), 3u);
    EXPECT_EQ(text(n->identifiers[0].value), "example.test");
    EXPECT_EQ(text(n->identifiers[1].value), "xn--w-uga1v8h.test");
    EXPECT_EQ(text(n->identifiers[2].value), "2001:db8::1");
    // what is no name
    for (const char* bad : {"", "a..b", "-.", "exa mple.test", "a/b"}) {
        auto r = c.new_order({bad});
        ASSERT_FALSE(r.has_value()) << bad;
        EXPECT_EQ(r.error().code(), net::acme::errc::rejected_identifier) << bad;
    }
    auto empty = c.new_order({});
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code(), net::acme::errc::malformed);
}

TEST(AcmeClient, AnInvalidChallenge) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    auto c = registered(ca);
    auto o = c.new_order({"example.test"});
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->status, net::acme::status::pending);
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(az.has_value());
    // the response served is wrong
    for (auto& ch : az->challenges) {
        if (text(ch.type) == "http-01") {
            {
                std::lock_guard<std::mutex> g(s.board->lock);
                s.board->http[ch.token] = string("not the key authorization");
            }
            auto a = c.accept(ch);
            ASSERT_TRUE(a.has_value());
            EXPECT_TRUE(a->status == net::acme::status::processing || a->status == net::acme::status::invalid);
        }
    }
    auto done = c.wait_authorization(o->authorizations[0]);
    ASSERT_FALSE(done.has_value());
    EXPECT_EQ(done.error().code(), net::acme::errc::authorization_invalid);
    EXPECT_NE(text(done.error().message()).find("http-01"), std::string::npos);
    auto now = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(now.has_value());
    EXPECT_EQ(now->status, net::acme::status::invalid);
    bool seen = false;
    for (auto& ch : now->challenges) {
        if (ch.error) {
            seen = true;
            EXPECT_EQ(ch.error->code(), net::acme::errc::incorrect_response);
        }
    }
    EXPECT_TRUE(seen);
    auto order = c.wait_order(o->url);
    ASSERT_FALSE(order.has_value());
    EXPECT_EQ(order.error().code(), net::acme::errc::order_invalid);
    // a challenge nothing answers: connection
    auto o2 = c.new_order({"nothing.test"});
    ASSERT_TRUE(o2.has_value());
    auto az2 = c.authorization(o2->authorizations[0]);
    for (auto& ch : az2->challenges) {
        if (text(ch.type) == "tls-alpn-01") {
            ASSERT_TRUE(c.accept(ch).has_value());
        }
    }
    auto d2 = c.wait_authorization(o2->authorizations[0]);
    ASSERT_FALSE(d2.has_value());
    EXPECT_EQ(d2.error().code(), net::acme::errc::authorization_invalid);
}

TEST(AcmeClient, TheOrdersStates) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    so.processing_time = std::chrono::milliseconds(1200);
    net::acme::test_server ca(so);
    auto c = registered(ca);
    auto o = c.new_order({"example.test"});
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->status, net::acme::status::pending);
    EXPECT_TRUE(o->expires.has_value());
    EXPECT_FALSE(o->finalize.empty());
    EXPECT_TRUE(o->certificate.empty());
    // finalize before ready: orderNotReady
    auto early = c.finalize(*o, csr_of({"example.test"}));
    ASSERT_FALSE(early.has_value());
    EXPECT_EQ(early.error().code(), net::acme::errc::order_not_ready);
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(c.accept(az->challenges[0]).has_value());
    ASSERT_TRUE(c.wait_authorization(o->authorizations[0]).has_value());
    auto ready = c.wait_order(o->url);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ(ready->status, net::acme::status::ready);
    // processing, polled at the CA's Retry-After, then valid
    auto start = sgcl::clock::now();
    auto fin = c.finalize(*ready, csr_of({"example.test"}));
    ASSERT_TRUE(fin.has_value()) << text(fin.error().message());
    EXPECT_EQ(fin->status, net::acme::status::valid);
    EXPECT_FALSE(fin->certificate.empty());
    EXPECT_GE(sgcl::clock::now() - start, std::chrono::milliseconds(1000));
    auto again = c.order(o->url);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->status, net::acme::status::valid);
    // an order that is not the account's, one that is not there
    auto stranger = registered(ca);
    auto theirs = stranger.order(o->url);
    ASSERT_FALSE(theirs.has_value());
    auto missing = c.order(string::concat(o->url, "0"));
    ASSERT_FALSE(missing.has_value());
}

TEST(AcmeClient, FinalizeRefusesABadCsr) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    net::acme::test_server ca(so);
    net::acme::account_key key;
    auto c = registered(ca, key);
    auto o = c.new_order({"example.test"});
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(c.accept(az->challenges[0]).has_value());
    auto ready = c.wait_order(o->url);
    ASSERT_TRUE(ready.has_value());
    // other names
    auto other = c.finalize(*ready, csr_of({"example.test", "more.test"}));
    ASSERT_FALSE(other.has_value());
    EXPECT_EQ(other.error().code(), net::acme::errc::bad_csr);
    // the account's own key
    auto pem = key.to_pem();
    auto account_key = crypto::p256::private_key::from_pem(pem.as_slice());
    ASSERT_TRUE(account_key.has_value());
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.test"};
    auto same = c.finalize(*ready, crypto::x509::create_certificate_request(t, *account_key));
    ASSERT_FALSE(same.has_value());
    EXPECT_EQ(same.error().code(), net::acme::errc::bad_csr);
    // the right one still goes
    auto fin = c.finalize(*ready, csr_of({"example.test"}));
    ASSERT_TRUE(fin.has_value()) << text(fin.error().message());
}

TEST(AcmeClient, AlternateChains) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    so.alternate_chains = 2;
    net::acme::test_server ca(so);
    auto c = registered(ca);
    Solvers unused;
    auto chain = issue(c, unused, {"example.test"}, "http-01");
    ASSERT_TRUE(chain.has_value()) << text(chain.error().message());
    ASSERT_EQ(chain->alternates.size(), 2u);
    EXPECT_TRUE(verifies(ca, *chain, "example.test"));
    for (auto& url : chain->alternates) {
        auto alt = c.certificate(url);
        ASSERT_TRUE(alt.has_value()) << text(alt.error().message());
        EXPECT_TRUE(alt->certificates[0] == chain->certificates[0]);
        EXPECT_FALSE(alt->certificates[1] == chain->certificates[1]);
        EXPECT_TRUE(verifies(ca, *alt, "example.test"));
        EXPECT_EQ(alt->alternates.size(), 2u);   // the main chain and the other alternate
    }
}

TEST(AcmeClient, Revocation) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    net::acme::test_server ca(so);
    auto c = registered(ca);
    Solvers unused;
    auto a = issue(c, unused, {"a.test"}, "http-01");
    ASSERT_TRUE(a.has_value());
    // by the account
    EXPECT_FALSE(ca.revoked(a->certificates[0]));
    auto r = c.revoke(a->certificates[0], net::acme::revocation_reason::superseded);
    ASSERT_TRUE(r.has_value()) << text(r.error().message());
    EXPECT_TRUE(ca.revoked(a->certificates[0]));
    auto twice = c.revoke(a->certificates[0]);
    ASSERT_FALSE(twice.has_value());
    EXPECT_EQ(twice.error().code(), net::acme::errc::already_revoked);
    // a reason RFC 5280 does not have
    auto b = issue(c, unused, {"b.test"}, "http-01");
    ASSERT_TRUE(b.has_value());
    auto reason = c.revoke(b->certificates[0], net::acme::revocation_reason(7));
    ASSERT_FALSE(reason.has_value());
    EXPECT_EQ(reason.error().code(), net::acme::errc::bad_revocation_reason);
    // another account may not
    auto stranger = registered(ca);
    auto theirs = stranger.revoke(b->certificates[0]);
    ASSERT_FALSE(theirs.has_value());
    EXPECT_EQ(theirs.error().code(), net::acme::errc::unauthorized);
    // by the certificate's key: the identity's
    auto o = c.new_order({"c.test"});
    auto az = c.authorization(o->authorizations[0]);
    ASSERT_TRUE(c.accept(az->challenges[0]).has_value());
    auto ready = c.wait_order(o->url);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"c.test"};
    auto fin = c.finalize(*ready, crypto::x509::create_certificate_request(t, key));
    ASSERT_TRUE(fin.has_value());
    auto chain = c.certificate(fin->certificate);
    auto key_pem = key.to_pem();
    auto id = net::tls::identity::from_pem(chain->pem, key_pem.as_slice());
    ASSERT_TRUE(id.has_value());
    // a client of another account (or none) revokes by the key
    net::acme::client anyone(ca.directory_url(), net::acme::account_key());
    auto by_key = anyone.revoke(*id, net::acme::revocation_reason::key_compromise);
    ASSERT_TRUE(by_key.has_value()) << text(by_key.error().message());
    EXPECT_TRUE(ca.revoked(chain->certificates[0]));
}

TEST(AcmeClient, RenewalInformation) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    net::acme::test_server ca(so);
    auto c = registered(ca);
    Solvers unused;
    auto a = issue(c, unused, {"a.test"}, "http-01");
    ASSERT_TRUE(a.has_value());
    const auto& leaf = a->certificates[0];
    auto id = net::acme::client::renewal_id(leaf);
    ASSERT_TRUE(id.has_value());
    EXPECT_NE(text(*id).find('.'), std::string::npos);
    auto info = c.renewal_info(leaf);
    ASSERT_TRUE(info.has_value()) << text(info.error().message());
    EXPECT_LE(leaf.not_before().unix(), info->start.unix());
    EXPECT_LE(info->start.unix(), info->end.unix());
    EXPECT_LE(info->end.unix(), leaf.not_after().unix());
    EXPECT_EQ(info->retry_after, std::chrono::seconds(21600));
    // the window the test sets
    auto now = time::now();
    ca.set_renewal_window(now - std::chrono::hours(1), now);
    auto due = c.renewal_info(leaf);
    ASSERT_TRUE(due.has_value());
    EXPECT_EQ(due->end.unix(), now.unix());
    // a replacement: once
    auto renewed = issue(c, unused, {"a.test"}, "http-01", {.replaces = *id});
    ASSERT_TRUE(renewed.has_value()) << text(renewed.error().message());
    auto again = c.new_order({"a.test"}, {.replaces = *id});
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code(), net::acme::errc::already_replaced);
    // a CA without renewalInfo
    net::acme::test_server::options plain;
    plain.renewal_info = false;
    net::acme::test_server ca2(plain);
    auto c2 = registered(ca2);
    auto none = c2.renewal_info(leaf);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::acme::errc::unsupported);
    auto no_replace = c2.new_order({"a.test"}, {.replaces = *id});
    ASSERT_FALSE(no_replace.has_value());
    EXPECT_EQ(no_replace.error().code(), net::acme::errc::unsupported);
}

TEST(AcmeClient, BadNoncesAreRetried) {
    net::acme::test_server ca;
    auto c = registered(ca);
    ca.fail_next(net::acme::errc::bad_nonce, 3, "new-order");
    auto o = c.new_order({"example.test"});
    ASSERT_TRUE(o.has_value()) << text(o.error().message());
    // past the retries: the error
    net::acme::client::options few;
    few.bad_nonce_retries = 1;
    few.account_url = c.account_url();
    net::acme::client impatient(ca.directory_url(), c.key(), few);
    ca.fail_next(net::acme::errc::bad_nonce, 2, "new-order");
    auto r = impatient.new_order({"example.test"});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), net::acme::errc::bad_nonce);
}

TEST(AcmeClient, RateLimitsHonoured) {
    net::acme::test_server ca;
    auto c = registered(ca);
    // a Retry-After within max_retry_after: waited for, then sent again
    ca.fail_next(net::acme::errc::rate_limited, 1, "new-order", std::chrono::seconds(1));
    auto start = sgcl::clock::now();
    auto o = c.new_order({"example.test"});
    ASSERT_TRUE(o.has_value()) << text(o.error().message());
    EXPECT_GE(sgcl::clock::now() - start, std::chrono::milliseconds(900));
    // past it: the error, with the wait in its text
    ca.fail_next(net::acme::errc::rate_limited, 1, "new-order", std::chrono::seconds(3600));
    auto r = c.new_order({"example.test"});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), net::acme::errc::rate_limited);
    EXPECT_NE(text(r.error().message()).find("retry after 3600 s"), std::string::npos) << text(r.error().message());
    // a server error: no retry
    ca.fail_next(net::acme::errc::server_internal, 1, "new-order");
    auto e = c.new_order({"example.test"});
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), net::acme::errc::server_internal);
}

TEST(AcmeClient, PollingEndsAtItsTimeout) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    net::acme::client::options o;
    o.poll_timeout = std::chrono::milliseconds(1500);
    o.poll_interval = std::chrono::milliseconds(100);
    net::acme::client c(ca.directory_url(), net::acme::account_key(), o);
    ASSERT_TRUE(c.register_account({.terms_agreed = true}).has_value());
    auto order = c.new_order({"example.test"});
    ASSERT_TRUE(order.has_value());
    // nothing answered: the authorization stays pending
    auto start = sgcl::clock::now();
    auto w = c.wait_authorization(order->authorizations[0]);
    ASSERT_FALSE(w.has_value());
    EXPECT_TRUE(w.error().is_timeout());
    EXPECT_LT(sgcl::clock::now() - start, std::chrono::seconds(4));
    auto wo = c.wait_order(order->url);
    ASSERT_FALSE(wo.has_value());
    EXPECT_TRUE(wo.error().is_timeout());
    // deactivated: the order invalid
    auto d = c.deactivate_authorization(order->authorizations[0]);
    ASSERT_TRUE(d.has_value()) << text(d.error().message());
    EXPECT_EQ(d->status, net::acme::status::deactivated);
    auto after = c.order(order->url);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->status, net::acme::status::invalid);
}

TEST(AcmeClient, ACaOverHttps) {
    Solvers s;
    auto so = s.server_options();
    so.tls = true;
    net::acme::test_server ca(so);
    EXPECT_EQ(text(ca.directory_url()).substr(0, 8), "https://");
    // the CA's roots trusted
    net::acme::client::options o;
    o.http.tls.roots = ca.roots();
    net::acme::client c(ca.directory_url(), net::acme::account_key(), o);
    ASSERT_TRUE(c.register_account({.terms_agreed = true}).has_value());
    auto chain = issue(c, s, {"example.test"}, "http-01");
    ASSERT_TRUE(chain.has_value()) << text(chain.error().message());
    // not trusted: the TLS error
    net::acme::client untrusted(ca.directory_url(), net::acme::account_key());
    auto d = untrusted.directory();
    ASSERT_FALSE(d.has_value());
}

TEST(AcmeClient, ManyOrdersAtOnce) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    net::acme::test_server ca(so);
    auto c = registered(ca);
    Solvers unused;
    std::atomic<int> ok = 0;
    {
        async::wait_group wg;
        for (int i = 0; i < 16; ++i) {
            wg.add();
            async::go([](net::acme::client c, int i, async::wait_group wg, std::atomic<int>* ok) -> async::task<> {
                string name = string::concat("host", string(std::to_string(i)), ".test");
                auto o = co_await c.async_new_order({name});
                if (o) {
                    auto az = co_await c.async_authorization(o->authorizations[0]);
                    if (az && co_await c.async_accept(az->challenges[0])) {
                        auto ready = co_await c.async_wait_order(o->url);
                        if (ready) {
                            auto fin = co_await c.async_finalize(*ready, csr_of({std::string(name.view())}));
                            if (fin) {
                                auto chain = co_await c.async_certificate(fin->certificate);
                                if (chain && chain->certificates[0].dns_names()[0] == name) {
                                    ok->fetch_add(1);
                                }
                            }
                        }
                    }
                }
                wg.done();
            }(c, i, wg, &ok));
        }
        wg.wait();
    }
    EXPECT_EQ(ok.load(), 16);
    EXPECT_EQ(ca.certificates(), 16u);
}

TEST(AcmeClient, TheAsyncForms) {
    Solvers s;
    net::acme::test_server ca(s.server_options());
    net::acme::account_key key;
    auto r = async::spawn([](string url, net::acme::account_key key) -> async::task<std::string> {
        net::acme::client c(url, key);
        auto d = co_await c.async_directory();
        if (!d) {
            co_return "directory";
        }
        auto a = co_await c.async_register_account({.terms_agreed = true});
        if (!a) {
            co_return "account";
        }
        auto acct = co_await c.async_account();
        auto upd = co_await c.async_update_account({"mailto:x@example.test"});
        if (!acct || !upd) {
            co_return "account update";
        }
        auto ch = co_await c.async_change_key(net::acme::account_key());
        if (!ch) {
            co_return "change_key";
        }
        auto o = co_await c.async_new_order({"example.test"});
        if (!o) {
            co_return "order";
        }
        auto again = co_await c.async_order(o->url);
        auto az = co_await c.async_authorization(o->authorizations[0]);
        if (!again || !az) {
            co_return "order, authorization";
        }
        auto one = co_await c.async_challenge(az->challenges[0].url);
        if (!one || one->token != az->challenges[0].token) {
            co_return "challenge";
        }
        auto deact = co_await c.async_deactivate_authorization(o->authorizations[0]);
        auto wa = co_await c.async_wait_authorization(o->authorizations[0]);
        if (!deact || wa) {
            co_return "deactivate";
        }
        auto gone = co_await c.async_deactivate_account();
        if (!gone || gone->status != net::acme::status::deactivated) {
            co_return "deactivate account";
        }
        co_return "";
    }(ca.directory_url(), key));
    EXPECT_EQ(r.wait(), "");
}

TEST(AcmeClient, TheChallengesValues) {
    net::acme::account_key key;
    net::acme::client c("http://127.0.0.1:1/directory", key);
    EXPECT_EQ(text(c.key_authorization("tok")), "tok." + text(key.thumbprint()));
    EXPECT_EQ(text(net::acme::client::http01_path("tok")), "/.well-known/acme-challenge/tok");
    auto ka = c.key_authorization("tok");
    auto h = crypto::sha256::of(ka);
    EXPECT_EQ(text(c.dns01_value("tok")), text(encoding::base64::raw_url.encode(h)));
    EXPECT_EQ(text(net::acme::client::dns01_name("example.test")), "_acme-challenge.example.test");
    // tls-alpn-01's certificate (RFC 8737 §3): the name alone, acmeIdentifier critical
    auto id = c.tls_alpn01_identity("tok", "example.test");
    ASSERT_TRUE(id.has_value());
    const auto& cert = id->certificates()[0];
    ASSERT_EQ(cert.dns_names().size(), 1u);
    EXPECT_EQ(text(cert.dns_names()[0]), "example.test");
    bool found = false;
    for (auto& e : cert.extensions()) {
        if (text(e.oid) == "1.3.6.1.5.5.7.1.31") {
            found = true;
            EXPECT_TRUE(e.critical);
            ASSERT_EQ(e.value.size(), 34u);
            for (size_t i = 0; i < 32; ++i) {
                EXPECT_EQ(e.value[2 + i], h[i]);
            }
        }
    }
    EXPECT_TRUE(found);
    auto ip = c.tls_alpn01_identity("tok", "192.0.2.1");
    ASSERT_TRUE(ip.has_value());
    EXPECT_TRUE(ip->certificates()[0].dns_names().empty());
    ASSERT_EQ(ip->certificates()[0].ip_addresses().size(), 1u);
    EXPECT_EQ(ip->certificates()[0].ip_addresses()[0].size, 4);
    // a certificate without an authorityKeyIdentifier has no ARI id
    auto self = net::acme::client::renewal_id(cert);
    ASSERT_FALSE(self.has_value());
    EXPECT_EQ(self.error().code(), net::acme::errc::malformed);
}

TEST(AcmeClient, ACopyIsTheSameClient) {
    net::acme::test_server ca;
    net::acme::account_key key;
    net::acme::client c(ca.directory_url(), key);
    net::acme::client copy = c;
    ASSERT_TRUE(c.register_account({.terms_agreed = true}).has_value());
    EXPECT_EQ(text(copy.account_url()), text(c.account_url()));
    net::acme::client moved = std::move(copy);
    EXPECT_EQ(text(moved.account_url()), text(c.account_url()));
}
