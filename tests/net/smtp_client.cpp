//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp's client against the module's own server: the one-line send and
// its envelope, a session of several messages, refusals of MAIL and RCPT
// (partial acceptance), SIZE, STARTTLS and implicit TLS, AUTH, SMTPUTF8,
// 8BITMIME, DSN, a large message, the timeouts and the stop, replies that
// break the protocol, the refusals of what cannot be sent, and delivery to
// a domain's exchanger by its MX record.
#include "smtp_common.h"
#include "dns_server.h"

#include <atomic>

using namespace smtp_test;
using namespace std::chrono_literals;

namespace {
    sgcl::vector<byte> bytes_of(std::string_view s) {
        sgcl::vector<byte> v;
        for (char c : s) {
            v.push_back(byte(uint8_t(c)));
        }
        return v;
    }
}

TEST(SmtpClient_Tests, OneLineSendAndItsEnvelope) {
    Running r;
    r.start();
    encoding::email m("Alice <alice@example.com>", "bob@example.org, Carol <carol@example.net>", "Hello", "Hi.\n");
    m.add_cc("dave@example.org").add_bcc("hidden@example.org").add_cc("bob@example.org");
    auto got = smtp::send(r.url(), m);
    ASSERT_TRUE(got) << str(got.error().message());
    EXPECT_EQ(got->reply.code, 250);
    EXPECT_EQ(str(got->reply.enhanced), "2.0.0");
    EXPECT_TRUE(got->rejected.empty());
    ASSERT_EQ(r.taken->size(), 1u);
    auto e = r.taken->envelope(0);
    EXPECT_EQ(str(e.from), "alice@example.com");
    ASSERT_EQ(e.to.size(), 4u);   // To, Cc, Bcc, each once
    EXPECT_EQ(str(e.to[0]), "bob@example.org");
    EXPECT_EQ(str(e.to[1]), "carol@example.net");
    EXPECT_EQ(str(e.to[2]), "dave@example.org");
    EXPECT_EQ(str(e.to[3]), "hidden@example.org");
    EXPECT_EQ(e.client.address().to_string(), sgcl::string("127.0.0.1"));
    EXPECT_FALSE(e.helo.empty());
    auto text = r.taken->text(0);
    EXPECT_EQ(text.find("Bcc:"), std::string::npos);   // never in what is sent
    EXPECT_EQ(text.find("hidden@"), std::string::npos);
    auto back = encoding::email::parse(sgcl::string(text));
    ASSERT_TRUE(back);
    EXPECT_EQ(str(back->subject()), "Hello");
    EXPECT_EQ(str(back->text()), "Hi.\n");
    auto in_task = smtp::async_send(r.url(), m).wait();
    ASSERT_TRUE(in_task);
    EXPECT_EQ(r.taken->size(), 2u);
    r.stop();
}

TEST(SmtpClient_Tests, ASessionOfSeveralMessages) {
    Running r;
    r.srv.hostname = "mx.test";
    r.start();
    auto c = smtp::client::connect(r.url());
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(bool(*c));
    EXPECT_EQ(str(c->greeting()), "mx.test ESMTP ready");
    EXPECT_TRUE(c->has_extension("pipelining"));
    EXPECT_TRUE(c->has_extension("CHUNKING"));
    EXPECT_FALSE(c->has_extension("STARTTLS"));
    EXPECT_EQ(str(c->extension("SIZE")), std::to_string(32 << 20));
    EXPECT_EQ(c->max_size(), uint64_t(32) << 20);
    EXPECT_EQ(str(c->extension("NONE")), "");
    EXPECT_FALSE(c->is_tls());
    for (int i : range(5)) {
        encoding::email m("a@example.com", "b@example.com", sgcl::string("n" + std::to_string(i)), "x");
        auto s = c->send(m);
        ASSERT_TRUE(s) << str(s.error().message());
    }
    ASSERT_TRUE(c->noop());
    ASSERT_TRUE(c->reset());
    auto v = c->verify("b@example.com");
    ASSERT_TRUE(v);
    EXPECT_EQ(v->code, 252);
    EXPECT_TRUE(c->async_noop().wait());
    EXPECT_TRUE(c->async_reset().wait());
    EXPECT_TRUE(c->async_verify("x@example.com").wait());
    smtp::envelope e;
    e.from = "raw@example.com";
    e.to.push_back(sgcl::string("r@example.com"));
    ASSERT_TRUE(c->send(e, "Subject: raw\r\n\r\nraw body\r\n"));
    ASSERT_TRUE(c->async_send(e, "Subject: raw2\r\n\r\nbody\r\n").wait());
    encoding::email m("a@example.com", "b@example.com", "env", "x");
    smtp::envelope other;
    other.from = "bounce@example.com";
    other.to.push_back(sgcl::string("elsewhere@example.com"));
    ASSERT_TRUE(c->send(m, other));
    ASSERT_TRUE(c->async_send(m, other).wait());
    ASSERT_TRUE(c->async_send(m).wait());
    ASSERT_TRUE(c->quit());
    EXPECT_EQ(r.taken->size(), 10u);
    EXPECT_EQ(str(r.taken->envelope(7).from), "bounce@example.com");
    EXPECT_EQ(str(r.taken->envelope(5).from), "raw@example.com");
    EXPECT_NE(r.taken->text(5).find("raw body"), std::string::npos);
    // after QUIT the session is over
    auto after = c->send(m);
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
    auto again = smtp::client::async_connect(r.url()).wait();
    ASSERT_TRUE(again);
    EXPECT_FALSE(*again == *c);
    EXPECT_TRUE(*again == *again);
    EXPECT_TRUE(again->close());
    EXPECT_FALSE(again->noop());
    EXPECT_TRUE(again->async_quit().wait());
    smtp::client none;
    EXPECT_FALSE(none);
    r.stop();
}

TEST(SmtpClient_Tests, RefusedRecipientsAndSenders) {
    Running r;
    r.srv.on_recipient = [](const smtp::envelope&, const sgcl::string& to) {
        if (to.view().starts_with("bad")) {
            return smtp::reply{550, sgcl::string("5.1.1"), sgcl::string("No such user here")};
        }
        return smtp::reply();
    };
    r.srv.on_sender = [](const smtp::envelope& e) {
        if (e.from.view().starts_with("spammer")) {
            return smtp::reply{550, sgcl::string("5.7.1"), sgcl::string("Go away\nfor good")};
        }
        return smtp::reply();
    };
    r.start();
    encoding::email m("a@example.com", "good@example.com, bad1@example.com, also@example.com, bad2@example.com", "s", "t");
    auto got = smtp::send(r.url(), m);
    ASSERT_TRUE(got) << str(got.error().message());
    ASSERT_EQ(got->rejected.size(), 2u);
    EXPECT_EQ(str(got->rejected[0].recipient), "bad1@example.com");
    EXPECT_EQ(got->rejected[0].reply.code, 550);
    EXPECT_EQ(str(got->rejected[0].reply.enhanced), "5.1.1");
    EXPECT_EQ(str(got->rejected[0].reply.text), "No such user here");
    ASSERT_EQ(r.taken->size(), 1u);
    EXPECT_EQ(r.taken->envelope(0).to.size(), 2u);

    encoding::email none("a@example.com", "bad@example.com, bad3@example.com", "s", "t");
    auto all = smtp::send(r.url(), none);
    ASSERT_FALSE(all);
    EXPECT_EQ(all.error().code(), net::errc::smtp_reply);
    auto rep = smtp::reply_of(all.error());
    ASSERT_TRUE(rep);
    EXPECT_EQ(rep->code, 550);
    EXPECT_EQ(str(rep->enhanced), "5.1.1");
    EXPECT_EQ(str(rep->text), "No such user here");
    EXPECT_NE(str(all.error().message()).find("RCPT TO:<bad3@example.com>"), std::string::npos) << str(all.error().message());

    encoding::email spam("spammer@example.com", "good@example.com", "s", "t");
    auto refused = smtp::send(r.url(), spam);
    ASSERT_FALSE(refused);
    auto sr = smtp::reply_of(refused.error());
    ASSERT_TRUE(sr);
    EXPECT_EQ(sr->code, 550);
    EXPECT_EQ(str(sr->text), "Go away\nfor good");
    EXPECT_EQ(str(sr->to_string()), "550 5.7.1 Go away; for good");
    EXPECT_EQ(r.taken->size(), 1u);
    // the session goes on after a refusal
    auto c = smtp::client::connect(r.url());
    ASSERT_TRUE(c);
    EXPECT_FALSE(c->send(spam));
    EXPECT_TRUE(c->send(m));
    EXPECT_FALSE(c->send(none));
    EXPECT_TRUE(c->send(m));
    (void)c->quit();
    EXPECT_EQ(r.taken->size(), 3u);
    r.stop();
}

TEST(SmtpClient_Tests, AHandlersRefusal) {
    Running r;
    r.srv.handle([](smtp::message m) {
        if (m.size() > 2000) {
            return smtp::reply{554, sgcl::string("5.6.0"), sgcl::string("Content rejected")};
        }
        return smtp::reply{250, sgcl::string("2.0.0"), sgcl::string("Ok: queued as XYZ")};
    });
    r.start(false);
    auto ok = smtp::send(r.url(), simple_message());
    ASSERT_TRUE(ok) << str(ok.error().message());
    EXPECT_EQ(str(ok->reply.text), "Ok: queued as XYZ");
    encoding::email big("a@example.com", "b@example.com", "s", sgcl::string(std::string(5000, 'x')));
    auto no = smtp::send(r.url(), big);
    ASSERT_FALSE(no);
    EXPECT_EQ(smtp::reply_of(no.error())->code, 554);
    r.stop();
}

TEST(SmtpClient_Tests, SizeIsCheckedBeforeSending) {
    Running r;
    r.srv.max_message_bytes = 1000;
    r.start();
    encoding::email big("a@example.com", "b@example.com", "s", sgcl::string(std::string(5000, 'x')));
    auto got = smtp::send(r.url(), big);
    ASSERT_FALSE(got);
    EXPECT_EQ(got.error().code(), net::errc::smtp_unsupported);
    EXPECT_EQ(r.taken->size(), 0u);
    EXPECT_TRUE(smtp::send(r.url(), simple_message()));
    r.stop();
}

TEST(SmtpClient_Tests, StartTlsAndImplicitTls) {
    Running r;
    r.srv.starttls = serving();
    r.start();
    smtp::options o;
    o.tls = trusted();
    auto c = smtp::client::connect(r.url(), o);
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(c->is_tls());
    EXPECT_FALSE(c->has_extension("STARTTLS"));   // the second EHLO's list
    ASSERT_TRUE(c->send(simple_message()));
    (void)c->quit();
    EXPECT_TRUE(r.taken->envelope(0).tls);
    // an untrusted certificate is the error, never clear text
    smtp::options untrusted;
    auto bad = smtp::client::connect(r.url(), untrusted);
    ASSERT_FALSE(bad);
    // STARTTLS off: clear text, by choice
    smtp::options plain;
    plain.starttls = false;
    auto p = smtp::client::connect(r.url(), plain);
    ASSERT_TRUE(p);
    EXPECT_FALSE(p->is_tls());
    EXPECT_TRUE(p->has_extension("STARTTLS"));
    (void)p->quit();
    r.stop();

    // a server without STARTTLS: required is refused
    Running clear;
    clear.start();
    smtp::options req;
    req.require_tls = true;
    auto refused = smtp::client::connect(clear.url(), req);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::smtp_tls_required);
    auto opportunistic = smtp::client::connect(clear.url());
    ASSERT_TRUE(opportunistic);
    EXPECT_FALSE(opportunistic->is_tls());
    (void)opportunistic->quit();
    clear.stop();

    // smtps://: TLS from the first byte
    Running implicit;
    implicit.start(true, true);
    auto s = smtp::send(implicit.url("smtps"), simple_message(), o);
    ASSERT_TRUE(s) << str(s.error().message());
    EXPECT_TRUE(implicit.taken->envelope(0).tls);
    implicit.stop();
}

TEST(SmtpClient_Tests, Authentication) {
    Running r;
    r.srv.starttls = serving();
    r.srv.auth = [](const sgcl::string& user, const sgcl::string& pass) { return user == "alice" && pass == "p@ss:word"; };
    r.start();
    smtp::options o;
    o.tls = trusted();
    o.username = "alice";
    o.password = "p@ss:word";
    ASSERT_TRUE(smtp::send(r.url(), simple_message(), o));
    EXPECT_EQ(str(r.taken->envelope(0).user), "alice");
    // the credentials in the URL, percent-encoded
    smtp::options tls_only;
    tls_only.tls = trusted();
    auto by_url = smtp::send(r.url("smtp", "alice:p%40ss%3Aword@"), simple_message(), tls_only);
    ASSERT_TRUE(by_url) << str(by_url.error().message());
    // LOGIN asked for
    o.auth = "login";
    ASSERT_TRUE(smtp::send(r.url(), simple_message(), o));
    o.auth = "";
    // a wrong password
    o.password = "nope";
    auto bad = smtp::send(r.url(), simple_message(), o);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::smtp_auth_failed);
    EXPECT_EQ(smtp::reply_of(bad.error())->code, 535);
    EXPECT_EQ(str(smtp::reply_of(bad.error())->enhanced), "5.7.8");
    // a mechanism the server does not offer
    o.password = "p@ss:word";
    o.auth = "XOAUTH2";
    auto mech = smtp::send(r.url(), simple_message(), o);
    ASSERT_FALSE(mech);
    EXPECT_EQ(mech.error().code(), net::errc::smtp_unsupported);
    // never in clear text unless allowed
    smtp::options clear;
    clear.starttls = false;
    clear.username = "alice";
    clear.password = "p@ss:word";
    auto refused = smtp::send(r.url(), simple_message(), clear);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::smtp_tls_required);
    clear.allow_insecure_auth = true;
    auto unoffered = smtp::send(r.url(), simple_message(), clear);   // the server offers AUTH over TLS only
    ASSERT_FALSE(unoffered);
    EXPECT_EQ(unoffered.error().code(), net::errc::smtp_unsupported);
    EXPECT_EQ(r.taken->size(), 3u);
    r.stop();

    Running lax;
    lax.srv.auth = [](const sgcl::string& user, const sgcl::string& pass) { return user == "u" && pass == "p"; };
    lax.srv.allow_insecure_auth = true;
    lax.start();
    smtp::options l;
    l.username = "u";
    l.password = "p";
    l.allow_insecure_auth = true;
    ASSERT_TRUE(smtp::send(lax.url(), simple_message(), l));
    EXPECT_EQ(str(lax.taken->envelope(0).user), "u");
    lax.stop();
}

TEST(SmtpClient_Tests, Utf8EightBitAndDsn) {
    Running r;
    r.start();
    encoding::email m("Łucja <łucja@przykład.pl>", "zoë@example.fr", "Zażółć", "Treść ąę\n");
    auto got = smtp::send(r.url(), m);
    ASSERT_TRUE(got) << str(got.error().message());
    auto e = r.taken->envelope(0);
    EXPECT_TRUE(e.smtputf8);
    EXPECT_EQ(str(e.from), "łucja@przykład.pl");
    EXPECT_EQ(str(e.body), "8BITMIME");
    auto text = r.taken->text(0);
    EXPECT_NE(text.find("From: Łucja <łucja@przykład.pl>"), std::string::npos) << text;   // RFC 6532 in the head
    EXPECT_NE(text.find("Content-Transfer-Encoding: 8bit"), std::string::npos);
    // ASCII addresses: no SMTPUTF8, the head encoded, the body 8bit
    encoding::email a("Łucja <lucja@example.pl>", "zoe@example.fr", "Zażółć", "Treść\n");
    ASSERT_TRUE(smtp::send(r.url(), a));
    EXPECT_FALSE(r.taken->envelope(1).smtputf8);
    EXPECT_EQ(r.taken->text(1).find("Łucja"), std::string::npos);

    auto c = smtp::client::connect(r.url());
    ASSERT_TRUE(c);
    smtp::envelope dsn;
    dsn.from = "a@example.com";
    dsn.to.push_back(sgcl::string("b@example.com"));
    dsn.to.push_back(sgcl::string("c@example.com"));
    dsn.ret = "hdrs";
    dsn.envid = "QQ314159 +=x";
    dsn.notify.push_back(sgcl::string("SUCCESS,FAILURE"));
    dsn.orcpt.push_back(sgcl::string("rfc822;b@example.com"));
    dsn.orcpt.push_back(sgcl::string("c+x@example.com"));
    ASSERT_TRUE(c->send(simple_message(), dsn));
    (void)c->quit();
    auto d = r.taken->envelope(2);
    EXPECT_EQ(str(d.ret), "HDRS");
    EXPECT_EQ(str(d.envid), "QQ314159 +=x");
    ASSERT_EQ(d.notify.size(), 2u);
    EXPECT_EQ(str(d.notify[0]), "SUCCESS,FAILURE");
    EXPECT_EQ(str(d.notify[1]), "SUCCESS,FAILURE");
    ASSERT_EQ(d.orcpt.size(), 2u);
    EXPECT_EQ(str(d.orcpt[0]), "rfc822;b@example.com");
    EXPECT_EQ(str(d.orcpt[1]), "rfc822;c+x@example.com");
    EXPECT_GT(d.size, 0u);
    r.stop();
}

TEST(SmtpClient_Tests, ALargeMessage) {
    Running r;
    r.start();
    encoding::email m("a@example.com", "b@example.com", "big", "see the file");
    std::string data(5 << 20, '\0');
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = char(i * 2654435761u >> 13);
    }
    m.attach("big.bin", bytes_of(data));
    auto got = smtp::send(r.url(), m);
    ASSERT_TRUE(got) << str(got.error().message());
    auto back = encoding::email::parse(sgcl::string(r.taken->text(0)));
    ASSERT_TRUE(back);
    auto atts = back->attachments();
    ASSERT_EQ(atts.size(), 1u);
    EXPECT_EQ(str(atts[0].content()), data);
    r.stop();
}

TEST(SmtpClient_Tests, DotsLineBreaksAndBinaryOfARawMessage) {
    Running r;
    r.start();
    auto c = smtp::client::connect(r.url());
    ASSERT_TRUE(c);
    smtp::envelope e;
    e.from = "a@example.com";
    e.to.push_back(sgcl::string("b@example.com"));
    // BDAT (the server offers CHUNKING): bytes as they are, line breaks CRLF
    ASSERT_TRUE(c->send(e, ".\n..\n. x\nlf only\nbare\rcr\nno end"));
    EXPECT_EQ(r.taken->text(0), ".\r\n..\r\n. x\r\nlf only\r\nbare\r\ncr\r\nno end\r\n");
    ASSERT_TRUE(c->send(e, ""));
    EXPECT_EQ(r.taken->text(1), "");
    (void)c->quit();
    r.stop();
}

TEST(SmtpClient_Tests, RefusalsOfWhatCannotBeSent) {
    Running r;
    r.start();
    auto c = smtp::client::connect(r.url());
    ASSERT_TRUE(c);
    smtp::envelope empty;
    empty.from = "a@example.com";
    auto none = c->send(empty, "x");
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), std::errc::invalid_argument);
    smtp::envelope inject;
    inject.from = "a@example.com>\r\nRCPT TO:<evil@example.com";
    inject.to.push_back(sgcl::string("b@example.com"));
    auto i = c->send(inject, "x");
    ASSERT_FALSE(i);
    EXPECT_EQ(i.error().code(), std::errc::invalid_argument);
    smtp::envelope bad_to;
    bad_to.from = "a@example.com";
    bad_to.to.push_back(sgcl::string("b@example.com>"));
    EXPECT_FALSE(c->send(bad_to, "x"));
    bad_to.to.clear();
    bad_to.to.push_back(sgcl::string(""));
    EXPECT_FALSE(c->send(bad_to, "x"));
    ASSERT_TRUE(c->noop());   // nothing was sent: the session is whole
    (void)c->quit();
    EXPECT_EQ(r.taken->size(), 0u);
    encoding::email no_rcpt;
    no_rcpt.set_from("a@example.com");
    EXPECT_FALSE(smtp::send(r.url(), no_rcpt));
    r.stop();

    EXPECT_EQ(smtp::send("http://x/", simple_message()).error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(smtp::send("smtp://", simple_message()).error().code(), net::errc::invalid_url);
    EXPECT_EQ(smtp::send("not a url", simple_message()).error().code(), net::errc::invalid_url);
    EXPECT_EQ(smtp::send("smtp://127.0.0.1:1", simple_message()).error().code(), std::errc::connection_refused);
    EXPECT_FALSE(smtp::reply_of(io::error(io::errc::closed, "x")));
    EXPECT_FALSE(smtp::reply_of(net::detail::net_error(net::errc::smtp_reply, "x", sgcl::string("garbage"))));
    auto plain = smtp::reply_of(net::detail::net_error(net::errc::smtp_reply, "x", sgcl::string("421 Service closing")));
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->code, 421);
    EXPECT_EQ(str(plain->enhanced), "");
    EXPECT_EQ(str(plain->text), "Service closing");
    EXPECT_TRUE(smtp::reply{354}.positive());
    EXPECT_FALSE(smtp::reply{450}.positive());
    EXPECT_FALSE(smtp::reply().positive());
    EXPECT_EQ(str(smtp::reply{250}.to_string()), "250");
}

TEST(SmtpClient_Tests, MalformedReplies) {
    auto expect = [](std::vector<std::string> script, error_code code, const char* what) {
        RawPeer p(std::move(script));
        auto c = smtp::client::connect(p.url());
        ASSERT_FALSE(c) << what;
        EXPECT_EQ(c.error().code(), code) << what << ": " << str(c.error().message());
    };
    expect({"hello there\r\n"}, net::errc::malformed_smtp_reply, "not a reply");
    expect({"220-first\r\n221 other code\r\n"}, net::errc::malformed_smtp_reply, "codes differ");
    expect({"220x no space\r\n"}, net::errc::malformed_smtp_reply, "no separator");
    expect({"220 " + std::string(5000, 'x') + "\r\n"}, net::errc::malformed_smtp_reply, "line too long");
    expect({"220-going on\r\n"}, io::errc::unexpected_eof, "cut short");
    expect({}, io::errc::unexpected_eof, "nothing");
    expect({"554 5.7.1 No service for you\r\n"}, net::errc::smtp_reply, "refused greeting");
    expect({"220 ok\r\n", "250-x\r\n999 bad\r\n"}, net::errc::malformed_smtp_reply, "EHLO broken");
    expect({"220 ok\r\n", "451 4.3.0 later\r\n"}, net::errc::smtp_reply, "EHLO temporary");
    // EHLO unknown: HELO, no extensions
    {
        RawPeer p({"220 old\r\n", "500 what\r\n", "250 old.test\r\n", "221 bye\r\n"});
        auto c = smtp::client::connect(p.url());
        ASSERT_TRUE(c) << str(c.error().message());
        EXPECT_FALSE(c->has_extension("PIPELINING"));
        EXPECT_EQ(c->max_size(), 0u);
        (void)c->quit();
    }
    // a multi-line greeting
    {
        RawPeer p({"220-mx.test\r\n220-second line\r\n220 third\r\n", "250-mx.test\r\n250-SIZE 100\r\n250 AUTH=LOGIN\r\n", "221 bye\r\n"});
        auto c = smtp::client::connect(p.url());
        ASSERT_TRUE(c) << str(c.error().message());
        EXPECT_EQ(str(c->greeting()), "mx.test\nsecond line\nthird");
        EXPECT_EQ(c->max_size(), 100u);
        EXPECT_EQ(str(c->extension("AUTH")), "LOGIN");
        (void)c->quit();
    }
    // the reply to the data never comes: the connection's end
    {
        RawPeer p({"220 ok\r\n", "250 ok\r\n", "250 ok\r\n", "250 ok\r\n", "354 go\r\n"});
        auto c = smtp::client::connect(p.url());
        ASSERT_TRUE(c);
        auto s = c->send(simple_message());
        ASSERT_FALSE(s);
        EXPECT_TRUE(s.error().is_eof() || s.error().code() == std::errc::connection_reset || s.error().code() == std::errc::broken_pipe)
            << str(s.error().message());
        EXPECT_FALSE(c->noop());   // broken
    }
}

TEST(SmtpClient_Tests, TimeoutsAndTheStop) {
    {
        RawPeer p({"<wait>"}, false);   // never greets
        smtp::options o;
        o.timeout = 200ms;
        auto start = sgcl::clock::now();
        auto c = smtp::client::connect(p.url(), o);
        ASSERT_FALSE(c);
        EXPECT_TRUE(c.error().is_timeout()) << str(c.error().message());
        EXPECT_LT(sgcl::clock::now() - start, 2s);
    }
    {
        RawPeer p({"<wait>"}, false);
        async::stop_source stop;
        smtp::options o;
        o.stop = stop.token();
        auto t = async::spawn(smtp::client::async_connect(p.url(), o));
        std::this_thread::sleep_for(100ms);
        auto start = sgcl::clock::now();
        stop.request_stop();
        auto c = t.wait();
        ASSERT_FALSE(c);
        EXPECT_EQ(c.error().code(), std::errc::operation_canceled);
        EXPECT_LT(sgcl::clock::now() - start, 2s);
    }
    {
        async::stop_source stop;
        stop.request_stop();
        smtp::options o;
        o.stop = stop.token();
        Running r;
        r.start();
        auto c = smtp::client::connect(r.url(), o);
        ASSERT_FALSE(c);
        EXPECT_EQ(c.error().code(), std::errc::operation_canceled);
        r.stop();
    }
    {
        // a stop during a session's send
        Running r;
        r.srv.handle([](smtp::message) -> async::task<> { co_await async::sleep(3s); });
        r.start(false);
        async::stop_source stop;
        smtp::options o;
        o.stop = stop.token();
        auto c = smtp::client::connect(r.url(), o);
        ASSERT_TRUE(c);
        auto t = async::spawn(c->async_send(simple_message()));
        std::this_thread::sleep_for(100ms);
        stop.request_stop();
        auto s = t.wait();
        ASSERT_FALSE(s);
        EXPECT_EQ(s.error().code(), std::errc::operation_canceled);
        EXPECT_FALSE(c->noop());
        r.stop();
    }
}

TEST(SmtpClient_Tests, DeliveryByMx) {
    using namespace dns_test;
    Running r;
    r.start();
    Behaviour b;
    b.zone = {rr_mx("example.test.", 10, "localhost."), rr_mx("null.test.", 0, ".")};
    b.rcode["gone.test."] = 3;   // NXDOMAIN
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    smtp::options o;
    o.dns = dns.options();
    o.port = r.port;
    encoding::email m("a@sender.test", "x@example.test, y@example.test", "direct", "hi");
    auto got = smtp::deliver(m, o);
    ASSERT_TRUE(got) << str(got.error().message());
    EXPECT_TRUE(got->rejected.empty());
    ASSERT_EQ(r.taken->size(), 1u);
    EXPECT_EQ(r.taken->envelope(0).to.size(), 2u);
    // a domain that takes no mail, one that is not there: rejections beside the delivery
    encoding::email mixed("a@sender.test", "x@example.test, n@null.test, g@gone.test", "direct", "hi");
    auto part = smtp::async_deliver(mixed, o).wait();
    ASSERT_TRUE(part) << str(part.error().message());
    ASSERT_EQ(part->rejected.size(), 2u);
    EXPECT_EQ(str(part->rejected[0].recipient), "n@null.test");
    EXPECT_EQ(part->rejected[0].reply.code, 556);
    EXPECT_EQ(part->rejected[1].reply.code, 0);
    encoding::email nowhere("a@sender.test", "n@null.test", "direct", "hi");
    auto failed = smtp::deliver(nowhere, o);
    ASSERT_FALSE(failed);
    EXPECT_EQ(smtp::reply_of(failed.error())->code, 556);
    r.stop();
}
