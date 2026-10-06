//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp's side of mail authentication: Authentication-Results parsed
// (RFC 8601 Appendix B's examples) and written, the checks of one message
// (smtp::check_sender), and the server that makes them after DATA for its
// handler, its field put first and forged ones taken out, DMARC's reject
// refused; the client that signs every message it sends.
#include "common.h"

#include <atomic>
#include <mutex>

using namespace mail_test;
using namespace std::chrono_literals;
namespace smtp = sgcl::net::smtp;
namespace dkim = sgcl::net::dkim;
namespace spf = sgcl::net::spf;
namespace dmarc = sgcl::net::dmarc;

namespace {
    dkim::signer test_signer() {
        auto seed = encoding::base64::standard.decode(rfc8463_ed25519_seed);
        auto key = crypto::ed25519::private_key::from_seed(*seed);
        return dkim::signer("example.com", "e1", key->to_pem());
    }

    smtp::authentication_results ar(const char* text) {
        auto r = smtp::authentication_results::parse(text);
        EXPECT_TRUE(r) << text;
        return r ? *r : smtp::authentication_results();
    }
}

TEST(MailAuthenticationResults, Rfc8601Examples) {
    auto none = ar("example.org 1; none");
    EXPECT_EQ(str(none.authserv_id), "example.org");
    EXPECT_TRUE(none.results.empty());
    EXPECT_EQ(str(none.to_string()), "example.org; none");

    auto spf = ar("example.com; spf=pass smtp.mailfrom=example.net");
    ASSERT_EQ(spf.results.size(), 1u);
    EXPECT_EQ(str(spf.results[0].method), "spf");
    EXPECT_EQ(str(spf.results[0].result), "pass");
    ASSERT_EQ(spf.results[0].properties.size(), 1u);
    EXPECT_EQ(str(spf.results[0].properties[0].first), "smtp.mailfrom");
    EXPECT_EQ(str(spf.results[0].properties[0].second), "example.net");

    auto two = ar("example.com;\r\n          auth=pass (cram-md5) smtp.auth=sender@example.net;\r\n          spf=pass smtp.mailfrom=example.net");
    ASSERT_EQ(two.results.size(), 2u);
    EXPECT_EQ(str(two.results[0].method), "auth");
    EXPECT_EQ(str(two.results[0].properties[0].second), "sender@example.net");

    auto dkims = ar("mail-router.example.net;\r\n  dkim=pass (good signature) header.d=newyork.example.com\r\n        header.b=oINEO8hg;\r\n"
                    "  dkim=fail (bad signature) header.d=newyork.example.net\r\n        header.b=EToRSuvU");
    ASSERT_EQ(dkims.results.size(), 2u);
    EXPECT_EQ(str(dkims.results[1].result), "fail");
    EXPECT_EQ(str(dkims.results[1].properties[1].second), "EToRSuvU");
    EXPECT_EQ(str(dkims.to_string()), "mail-router.example.net; dkim=pass header.d=newyork.example.com header.b=oINEO8hg; "
                                      "dkim=fail header.d=newyork.example.net header.b=EToRSuvU");

    auto reason = ar("\"quoted id\" (c (nested)) ; dkim/1 = fail reason=\"bad; sig\" header.i=@example.com");
    EXPECT_EQ(str(reason.authserv_id), "quoted id");
    EXPECT_EQ(str(reason.results[0].reason), "bad; sig");
    EXPECT_EQ(str(reason.to_string()), "\"quoted id\"; dkim=fail reason=\"bad; sig\" header.i=@example.com");
    EXPECT_EQ(smtp::authentication_results::parse(reason.to_string()).value(), reason);
}

TEST(MailAuthenticationResults, Malformed) {
    for (const char* bad : {"", ";", "example.com spf=pass", "example.com; spf", "example.com; spf=pass smtp", "example.com; spf=pass smtp.mailfrom",
                            "example.com 2; none", "example.com; none; spf=pass", "example.com (unclosed; spf=pass", "example.com; =pass"}) {
        auto r = smtp::authentication_results::parse(bad);
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), net::errc::malformed_message) << bad;
    }
    EXPECT_THROW(smtp::authentication_results(""), sgcl::bad_expected_access<io::error>);
    EXPECT_EQ(str(smtp::authentication_results("x; spf=pass;").to_string()), "x; spf=pass");
}

TEST(MailAuthenticationResults, OfChecks) {
    smtp::sender_verdict a;
    EXPECT_EQ(str(a.results("mx.example").to_string()), "mx.example; none");
    spf::result s;
    s.status = spf::status::softfail;
    s.domain = "example.com";
    s.mechanism = "~all";
    a.spf = s;
    dkim::result d;
    d.status = dkim::status::pass;
    d.domain = "example.com";
    d.selector = "s1";
    d.identity = "@example.com";
    d.algorithm = "ed25519-sha256";
    d.signature = "abcdefghijkl";
    a.dkim = vector<dkim::result>{d};
    dmarc::result m;
    m.status = dmarc::status::pass;
    m.domain = "example.com";
    m.record = dmarc::record("v=DMARC1; p=reject");
    a.dmarc = m;
    EXPECT_EQ(str(a.results("mx.example").to_string()),
              "mx.example; spf=softfail reason=~all smtp.mailfrom=example.com; dkim=pass header.d=example.com header.i=@example.com header.s=s1 "
              "header.a=ed25519-sha256 header.b=abcdefgh; dmarc=pass policy.dmarc=none header.from=example.com");
    a.dkim = vector<dkim::result>();
    a.spf = nullopt;
    a.dmarc = nullopt;
    EXPECT_EQ(str(a.results("mx.example").to_string()), "mx.example; dkim=none");
}

TEST(MailSenderChecks, ChecksOfOneMessage) {
    auto signer = test_signer();
    Behaviour b;
    b.zone.push_back(txt_rr("e1._domainkey.example.com.", str(signer.record())));
    b.zone.push_back(txt_rr("example.com.", "v=spf1 ip4:192.0.2.0/24 -all"));
    b.zone.push_back(txt_rr("_dmarc.example.com.", "v=DMARC1; p=reject"));
    Server srv(b);
    smtp::sender_checks o;
    o.dns = srv.options();
    smtp::envelope e;
    e.from = "alice@example.com";
    e.helo = "mail.example.com";
    e.client = net::endpoint(net::ip_address("192.0.2.7"), 25000);
    auto m = signer.sign(sgcl::string(plain_message));
    ASSERT_TRUE(m);
    auto a = smtp::check_sender(*m, e, o);
    ASSERT_TRUE(a.spf && a.dkim && a.dmarc);
    EXPECT_EQ(a.spf->status, spf::status::pass);
    ASSERT_EQ(a.dkim->size(), 1u);
    EXPECT_EQ((*a.dkim)[0].status, dkim::status::pass);
    EXPECT_EQ(a.dmarc->status, dmarc::status::pass);
    EXPECT_TRUE(a.dmarc->spf_aligned && a.dmarc->dkim_aligned);
    // from elsewhere, unsigned: DMARC fails with reject
    e.client = net::endpoint(net::ip_address("198.51.100.1"), 25000);
    auto f = smtp::check_sender(plain_message, e, o);
    EXPECT_EQ(f.spf->status, spf::status::fail);
    EXPECT_TRUE(f.dkim->empty());
    EXPECT_EQ(f.dmarc->status, dmarc::status::fail);
    EXPECT_EQ(f.dmarc->disposition, dmarc::policy::reject);
    // two From fields, or From of two domains: permerror
    auto two = smtp::check_sender(sgcl::string(std::string("From: a@example.com\r\n") + plain_message), e, o);
    EXPECT_EQ(two.dmarc->status, dmarc::status::permerror);
    auto mixed = smtp::check_sender("From: a@example.com, b@example.org\r\n\r\nx\r\n", e, o);
    EXPECT_EQ(mixed.dmarc->status, dmarc::status::permerror);
    auto same = smtp::check_sender("From: a@example.com, \"B\" <b@EXAMPLE.com>\r\n\r\nx\r\n", e, o);
    EXPECT_EQ(same.dmarc->domain.view(), "example.com");
    // checks turned off
    o.spf = false;
    o.dkim = false;
    auto only = smtp::check_sender(*m, e, o);
    EXPECT_FALSE(only.spf);
    EXPECT_FALSE(only.dkim);
    ASSERT_TRUE(only.dmarc);
    EXPECT_EQ(only.dmarc->status, dmarc::status::pass);   // DKIM still checked for DMARC
    o.dmarc = false;
    auto nothing = smtp::check_sender(*m, e, o);
    EXPECT_FALSE(nothing.spf || nothing.dkim || nothing.dmarc);
}

TEST(MailSenderChecks, ServerChecksAndClientSigns) {
    auto signer = test_signer();
    Behaviour b;
    b.zone.push_back(txt_rr("e1._domainkey.example.com.", str(signer.record())));
    b.zone.push_back(txt_rr("example.com.", "v=spf1 ip4:127.0.0.1 -all"));
    b.zone.push_back(txt_rr("_dmarc.example.com.", "v=DMARC1; p=reject"));
    b.zone.push_back(txt_rr("example.org.", "v=spf1 -all"));
    b.zone.push_back(txt_rr("_dmarc.example.org.", "v=DMARC1; p=reject"));
    Server dns(b);
    ASSERT_TRUE(dns.ok());

    std::mutex lock;
    std::vector<std::string> seen;
    std::vector<std::string> results;
    smtp::server srv;
    srv.hostname = "mx.receiver.test";
    smtp::sender_checks checks;
    checks.dns = dns.options();
    checks.reject = true;
    srv.sender_checks = checks;
    srv.handle([&](smtp::message m) {
        std::lock_guard g(lock);
        auto bytes = m.bytes();
        seen.push_back(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        auto& a = m.sender_verdict();
        results.push_back(a ? str(a->results("mx.receiver.test").to_string()) : std::string("none"));
    });
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto serving = async::spawn(srv.async_serve(*l));
    std::string url = "smtp://" + str(l->local_endpoint().to_string());

    smtp::options o;
    o.dkim = signer;
    o.hostname = "client.example.com";
    std::string forged = "Authentication-Results: mx.receiver.test; dkim=pass header.d=bank.example\r\n"
                         "Authentication-Results: other.example; spf=pass smtp.mailfrom=x\r\n";
    smtp::envelope e;
    e.from = "alice@example.com";
    e.to.push_back("bob@receiver.test");
    auto sent = smtp::client::connect(sgcl::string(url), o);
    ASSERT_TRUE(sent) << str(sent.error().message());
    auto r = sent->send(e, sgcl::string(forged + plain_message));
    ASSERT_TRUE(r) << str(r.error().message());
    // a message of example.org from here: DMARC reject refused
    smtp::envelope bad;
    bad.from = "mallory@example.org";
    bad.to.push_back("bob@receiver.test");
    auto refused = sent->send(bad, sgcl::string("From: Mallory <mallory@example.org>\r\nSubject: x\r\n\r\nx\r\n"));
    ASSERT_FALSE(refused);
    auto reply = smtp::reply_of(refused.error());
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->code, 550);
    EXPECT_EQ(str(reply->enhanced), "5.7.1");
    sent->quit();
    srv.close();
    serving.wait();

    std::lock_guard g(lock);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(results[0], "mx.receiver.test; spf=pass smtp.mailfrom=example.com; dkim=pass header.d=example.com header.i=@example.com "
                          "header.s=e1 header.a=ed25519-sha256 header.b=" + results[0].substr(results[0].find("header.b=") + 9, 8) +
                          "; dmarc=pass policy.dmarc=none header.from=example.com");
    // the field first, folded; the forged one of this receiver gone, the other kept; the signature after it
    const std::string& m = seen[0];
    EXPECT_EQ(m.rfind("Authentication-Results: mx.receiver.test;", 0), 0u) << m;
    EXPECT_EQ(m.find("bank.example"), std::string::npos) << m;
    EXPECT_NE(m.find("Authentication-Results: other.example; spf=pass"), std::string::npos);
    EXPECT_NE(m.find("\r\nDKIM-Signature: v=1; a=ed25519-sha256;"), std::string::npos);
    for (size_t at = 0, eol; (eol = m.find("\r\n", at)) != std::string::npos && at < m.find("\r\n\r\n"); at = eol + 2) {
        EXPECT_LE(eol - at, 78u) << m.substr(at, eol - at);
    }
}

TEST(MailSenderChecks, ClientSigningRefusesMalformed) {
    smtp::server srv;
    srv.handle([](smtp::message) {});
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto serving = async::spawn(srv.async_serve(*l));
    smtp::options o;
    o.dkim = test_signer();
    auto c = smtp::client::connect(sgcl::string("smtp://" + str(l->local_endpoint().to_string())), o);
    ASSERT_TRUE(c);
    smtp::envelope e;
    e.from = "a@example.com";
    e.to.push_back("b@example.org");
    auto r = c->send(e, sgcl::string("Subject: no From\r\n\r\nx\r\n"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::malformed_message);
    // an encoding::email is signed whole
    auto ok = c->send(encoding::email("a@example.com", "b@example.org", "Hi", "Hello"));
    EXPECT_TRUE(ok);
    c->quit();
    srv.close();
    serving.wait();
}
