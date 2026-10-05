//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp against other implementations: the client against a minimal
// server written by hand in Go (go_smtp/main.go: sequential and pipelined,
// DATA with its dots, BDAT, DSN, SMTPUTF8, SIZE, STARTTLS and its downgrade
// and injection, AUTH PLAIN, LOGIN and XOAUTH2, HELO only, refusals and
// broken peers), Go's net/smtp client against the server, and curl as a
// client of the server (smtp:// with STARTTLS required, smtps://, AUTH).
#include "smtp_common.h"

using namespace smtp_test;
using namespace std::chrono_literals;

namespace {
    bool go_ready() {
        return !go_smtp().empty();
    }

    encoding::json record(const std::string& line) {
        auto j = encoding::json::parse(sgcl::string(line));
        EXPECT_TRUE(j) << line;
        return j ? *j : encoding::json();
    }
}

TEST(SmtpInterop_Tests, GoServerSequentialWithDots) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    GoServer g("plain");
    ASSERT_GT(g.port, 0);
    smtp::envelope e;
    e.from = "a@example.com";
    e.to.push_back(sgcl::string("b@example.com"));
    e.to.push_back(sgcl::string("c@example.com"));
    auto c = smtp::client::connect(g.url());
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_FALSE(c->has_extension("PIPELINING"));
    auto r = c->send(e, ".\n..\n. leading\nlf\nbare\rcr\nend without a break");
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_EQ(str(r->reply.text), "queued as GO1");
    ASSERT_TRUE(c->send(simple_message()));
    (void)c->quit();
    auto recs = g.records(2);
    ASSERT_EQ(recs.size(), 2u);
    auto j = record(recs[0]);
    EXPECT_EQ(str(j["data"].as_string("")), ".\r\n..\r\n. leading\r\nlf\r\nbare\r\ncr\r\nend without a break\r\n");
    EXPECT_EQ(j["to"].size(), 2u);
    EXPECT_EQ(str(j["mail"].as_string("")), "MAIL FROM:<a@example.com>");
    EXPECT_FALSE(j["bdat"].as_bool(true));
    auto k = record(recs[1]);
    auto m = encoding::email::parse(k["data"].as_string(""));
    ASSERT_TRUE(m);
    EXPECT_EQ(str(m->subject()), "Hello");
}

TEST(SmtpInterop_Tests, GoServerPipelinedWithARefusal) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    GoServer g("pipelining+rejectbad");
    encoding::email m("a@example.com", "good@example.com, bad@example.com, fine@example.com", "p", "pipelined\n");
    auto r = smtp::send(g.url(), m);
    ASSERT_TRUE(r) << str(r.error().message());
    ASSERT_EQ(r->rejected.size(), 1u);
    EXPECT_EQ(str(r->rejected[0].recipient), "bad@example.com");
    EXPECT_EQ(r->rejected[0].reply.code, 550);
    EXPECT_EQ(str(r->rejected[0].reply.enhanced), "5.1.1");
    auto recs = g.records(1);
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(record(recs[0])["to"].size(), 2u);
    // every recipient refused with DATA pipelined: the error, the session in step
    auto c = smtp::client::connect(g.url());
    ASSERT_TRUE(c);
    encoding::email none("a@example.com", "bad1@example.com, bad2@example.com", "p", "x");
    auto n = c->send(none);
    ASSERT_FALSE(n);
    EXPECT_EQ(smtp::reply_of(n.error())->code, 550);
    ASSERT_TRUE(c->send(m));
    (void)c->quit();
    EXPECT_EQ(g.records(2).size(), 2u);
}

TEST(SmtpInterop_Tests, GoServerChunkingDsnUtf8AndSize) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    {
        GoServer g("chunking+dsn+utf8+pipelining");
        auto c = smtp::client::connect(g.url());
        ASSERT_TRUE(c);
        smtp::envelope e;
        e.from = "łucja@example.pl";
        e.to.push_back(sgcl::string("zoë@example.fr"));
        e.ret = "FULL";
        e.envid = "id 1";
        e.notify.push_back(sgcl::string("FAILURE,DELAY"));
        e.orcpt.push_back(sgcl::string("rfc822;zoë@example.fr"));
        encoding::email m("Łucja <łucja@example.pl>", "zoë@example.fr", "Zażółć", "treść\n");
        auto r = c->send(m, e);
        ASSERT_TRUE(r) << str(r.error().message());
        (void)c->quit();
        auto recs = g.records(1);
        ASSERT_EQ(recs.size(), 1u);
        auto j = record(recs[0]);
        EXPECT_TRUE(j["bdat"].as_bool(false));
        EXPECT_EQ(str(j["mail"].as_string("")).substr(0, 30), "MAIL FROM:<łucja@example.pl> ");
        EXPECT_NE(str(j["mail"].as_string("")).find(" SMTPUTF8"), std::string::npos);
        EXPECT_NE(str(j["mail"].as_string("")).find(" BODY=8BITMIME"), std::string::npos);
        EXPECT_NE(str(j["mail"].as_string("")).find(" RET=FULL ENVID=id+201"), std::string::npos) << str(j["mail"].as_string(""));
        EXPECT_EQ(str(j["rcpt"][0].as_string("")), "RCPT TO:<zoë@example.fr> NOTIFY=FAILURE,DELAY ORCPT=rfc822;zo+C3+AB@example.fr");
        EXPECT_NE(str(j["data"].as_string("")).find("Subject: Zażółć"), std::string::npos);
    }
    {
        // no SMTPUTF8: an address past ASCII is not sent
        GoServer g("pipelining");
        encoding::email m("łucja@example.pl", "b@example.com", "s", "t");
        auto r = smtp::send(g.url(), m);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), net::errc::smtp_unsupported);
        // no DSN: its fields are left out
        smtp::envelope e;
        e.from = "a@example.com";
        e.to.push_back(sgcl::string("b@example.com"));
        e.ret = "HDRS";
        e.notify.push_back(sgcl::string("NEVER"));
        auto c = smtp::client::connect(g.url());
        ASSERT_TRUE(c);
        ASSERT_TRUE(c->send(e, "Subject: x\r\n\r\ny\r\n"));
        (void)c->quit();
        auto j = record(g.records(1)[0]);
        EXPECT_EQ(str(j["mail"].as_string("")), "MAIL FROM:<a@example.com>");
        EXPECT_EQ(str(j["rcpt"][0].as_string("")), "RCPT TO:<b@example.com>");
    }
    {
        GoServer g("size");
        encoding::email big("a@example.com", "b@example.com", "s", sgcl::string(std::string(2000, 'x')));
        auto r = smtp::send(g.url(), big);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), net::errc::smtp_unsupported);
        ASSERT_TRUE(smtp::send(g.url(), simple_message()));
        EXPECT_NE(str(record(g.records(1)[0])["mail"].as_string("")).find(" SIZE="), std::string::npos);
    }
}

TEST(SmtpInterop_Tests, GoServerStartTlsDowngradeAndInjection) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    smtp::options o;
    o.tls = trusted();
    o.require_tls = true;
    {
        GoServer g("starttls+pipelining");
        auto r = smtp::send(g.url(), simple_message(), o);
        ASSERT_TRUE(r) << str(r.error().message());
        EXPECT_TRUE(record(g.records(1)[0])["tls"].as_bool(false));
    }
    {
        // a server (or a man in the middle) that does not offer STARTTLS
        GoServer g("pipelining");
        auto r = smtp::send(g.url(), simple_message(), o);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), net::errc::smtp_tls_required);
        EXPECT_TRUE(g.records(1, 300).empty());
    }
    {
        // a reply injected after the 220 of STARTTLS, in clear text
        GoServer g("starttls+injectdata");
        smtp::options plain;
        plain.tls = trusted();
        auto r = smtp::send(g.url(), simple_message(), plain);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), net::errc::malformed_smtp_reply);
    }
}

TEST(SmtpInterop_Tests, GoServerAuth) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    GoServer g("auth+pipelining");
    smtp::options o;
    o.allow_insecure_auth = true;
    o.username = "alice";
    o.password = "secret";
    ASSERT_TRUE(smtp::send(g.url(), simple_message(), o));
    o.auth = "LOGIN";
    ASSERT_TRUE(smtp::send(g.url(), simple_message(), o));
    smtp::options x;
    x.allow_insecure_auth = true;
    x.username = "alice";
    x.oauth_token = "token";
    ASSERT_TRUE(smtp::send(g.url(), simple_message(), x));
    x.oauth_token = "expired";
    auto bad = smtp::send(g.url(), simple_message(), x);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::smtp_auth_failed);
    o.auth = "";
    o.password = "wrong";
    auto wrong = smtp::send(g.url(), simple_message(), o);
    ASSERT_FALSE(wrong);
    EXPECT_EQ(smtp::reply_of(wrong.error())->code, 535);
    auto recs = g.records(3);
    ASSERT_EQ(recs.size(), 3u);
    EXPECT_EQ(str(record(recs[0])["user"].as_string("")), "alice");
}

TEST(SmtpInterop_Tests, GoServerHeloTemporaryAndBroken) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    {
        GoServer g("helo");
        auto c = smtp::client::connect(g.url());
        ASSERT_TRUE(c) << str(c.error().message());
        EXPECT_FALSE(c->has_extension("8BITMIME"));
        ASSERT_TRUE(c->send(simple_message()));
        (void)c->quit();
        EXPECT_EQ(g.records(1).size(), 1u);
    }
    {
        GoServer g("tempfail");
        auto r = smtp::send(g.url(), simple_message());
        ASSERT_FALSE(r);
        EXPECT_EQ(smtp::reply_of(r.error())->code, 451);
    }
    {
        GoServer g("garbage");
        EXPECT_EQ(smtp::send(g.url(), simple_message()).error().code(), net::errc::malformed_smtp_reply);
    }
    {
        GoServer g("hangup");
        auto r = smtp::send(g.url(), simple_message());
        ASSERT_FALSE(r);
    }
    {
        GoServer g("silent");
        smtp::options o;
        o.timeout = 300ms;
        auto r = smtp::send(g.url(), simple_message(), o);
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().is_timeout());
    }
}

// Go's net/smtp client sends to the server: in clear text, and with
// STARTTLS and PLAIN
TEST(SmtpInterop_Tests, GoClientToOurServer) {
    if (!go_ready()) {
        GTEST_SKIP() << "no go";
    }
    Running r;
    r.srv.starttls = serving();
    r.srv.auth = [](const sgcl::string& u, const sgcl::string& p) { return u == "gopher" && p == "pw"; };
    r.start();
    auto msg = scratch() + "/msg.eml";
    std::ofstream(msg, std::ios::binary) << "Subject: from go\r\n\r\n.dot line\r\nbody\r\n";
    {
        Running clear;   // SendMail takes STARTTLS when offered, with the system's roots
        clear.start();
        auto plain = run("'" + go_smtp() + "' client " + str(clear.address()) + " a@example.com b@example.com c@example.com < '" + msg + "'");
        EXPECT_EQ(plain, "ok\n");
        ASSERT_EQ(clear.taken->size(), 1u);
        EXPECT_EQ(clear.taken->text(0), "Subject: from go\r\n\r\n.dot line\r\nbody\r\n");
        EXPECT_EQ(clear.taken->envelope(0).to.size(), 2u);
        clear.stop();
    }
    auto tls = run("SMTP_CA='" + testdata() + "ca.pem' SMTP_USER=gopher SMTP_PASS=pw '" + go_smtp() + "' client " + str(r.address()) +
                   " a@example.com b@example.com < '" + msg + "'");
    EXPECT_EQ(tls, "ok\n");
    ASSERT_EQ(r.taken->size(), 1u);
    EXPECT_TRUE(r.taken->envelope(0).tls);
    EXPECT_EQ(str(r.taken->envelope(0).user), "gopher");
    auto bad = run("SMTP_CA='" + testdata() + "ca.pem' SMTP_USER=gopher SMTP_PASS=no '" + go_smtp() + "' client " + str(r.address()) +
                   " a@example.com b@example.com < '" + msg + "'");
    EXPECT_NE(bad.find("535"), std::string::npos) << bad;
    r.stop();
}

// curl as a client: --mail-from, --mail-rcpt, --upload-file; STARTTLS
// required (--ssl-reqd), implicit TLS (smtps://), AUTH (--user)
TEST(SmtpInterop_Tests, CurlToOurServer) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    auto msg = scratch() + "/curl.eml";
    std::ofstream(msg, std::ios::binary) << "From: a@example.com\r\nTo: b@example.com\r\nSubject: from curl\r\n\r\n.dot\r\nbody\r\n";
    const std::string ca = testdata() + "ca.pem";
    {
        Running r;
        r.srv.starttls = serving();
        r.srv.auth = [](const sgcl::string& u, const sgcl::string& p) { return u == "curl" && p == "pw"; };
        r.start();
        auto base = "curl -sS --max-time 20 --cacert '" + ca + "' --mail-from a@example.com --mail-rcpt b@example.com --mail-rcpt c@example.com --upload-file '" + msg + "' ";
        auto out = run(base + "smtp://127.0.0.1:" + std::to_string(r.port) + " 2>&1; echo rc=$?");
        EXPECT_NE(out.find("rc=0"), std::string::npos) << out;
        auto tls = run(base + "--ssl-reqd --user curl:pw smtp://localhost:" + std::to_string(r.port) + " 2>&1; echo rc=$?");
        EXPECT_NE(tls.find("rc=0"), std::string::npos) << tls;
        ASSERT_EQ(r.taken->size(), 2u);
        EXPECT_EQ(r.taken->envelope(0).to.size(), 2u);
        EXPECT_NE(r.taken->text(0).find("\r\n.dot\r\nbody\r\n"), std::string::npos);
        EXPECT_TRUE(r.taken->envelope(1).tls);
        EXPECT_EQ(str(r.taken->envelope(1).user), "curl");
        auto wrong = run(base + "--ssl-reqd --user curl:no smtp://localhost:" + std::to_string(r.port) + " 2>&1; echo rc=$?");
        EXPECT_EQ(wrong.find("rc=0"), std::string::npos) << wrong;
        r.stop();
    }
    {
        Running r;
        r.start(true, true);
        auto out = run("curl -sS --max-time 20 --cacert '" + ca + "' --mail-from a@example.com --mail-rcpt b@example.com --upload-file '" + msg +
                       "' smtps://localhost:" + std::to_string(r.port) + " 2>&1; echo rc=$?");
        EXPECT_NE(out.find("rc=0"), std::string::npos) << out;
        ASSERT_EQ(r.taken->size(), 1u);
        EXPECT_TRUE(r.taken->envelope(0).tls);
        r.stop();
    }
}
