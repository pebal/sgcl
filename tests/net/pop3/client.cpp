//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::pop3's client against the module's server: every call and its
// errors, each mechanism (SASL PLAIN, USER/PASS, APOP), STLS, pop3s, the
// security levels, retrieve's batch pipelined or not, the boundaries; and
// Python's poplib against the server.
#include "helpers.h"

using namespace pop3_test;

TEST(Pop3Client, Session) {
    Server s;
    auto c = pop3::client::connect(s.address(), plain_options());
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(*c);
    EXPECT_FALSE(c->is_tls());
    EXPECT_EQ(str(c->greeting()), "POP3 server ready");
    EXPECT_TRUE(c->has("top"));
    EXPECT_TRUE(c->has("PIPELINING"));
    EXPECT_FALSE(c->has("STLS"));
    EXPECT_FALSE(c->capabilities().empty());
    auto st = c->status();
    ASSERT_TRUE(st);
    EXPECT_EQ(*st, (pop3::mailbox_status{2, 143}));
    auto ls = c->list();
    ASSERT_TRUE(ls);
    ASSERT_EQ(ls->size(), 2u);
    EXPECT_EQ((*ls)[0].number, 1u);
    EXPECT_EQ((*ls)[0].size, 67u);
    EXPECT_FALSE((*ls)[0].uid.empty());
    auto one = c->list(2);
    ASSERT_TRUE(one);
    EXPECT_EQ(*one, (*ls)[1]);
    auto m = c->retrieve(1);
    ASSERT_TRUE(m);
    EXPECT_EQ(str(*m), "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n.hidden dot\r\n");
    auto top = c->top(2, 2);
    ASSERT_TRUE(top);
    EXPECT_EQ(str(*top), "From: Carol <carol@example.com>\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\n");
    auto both = c->retrieve(vector<uint32_t>{2, 1});
    ASSERT_TRUE(both);
    ASSERT_EQ(both->size(), 2u);
    EXPECT_EQ((*both)[1], *m);
    EXPECT_TRUE(c->retrieve(vector<uint32_t>{})->empty());
    ASSERT_TRUE(c->remove(1));
    auto gone = c->retrieve(1);
    ASSERT_FALSE(gone);
    EXPECT_EQ(gone.error().code(), pop3::errc::no_such_message);
    EXPECT_EQ(c->remove(1).error().code(), pop3::errc::no_such_message);
    EXPECT_EQ(c->list(9).error().code(), pop3::errc::no_such_message);
    auto partly = c->retrieve(vector<uint32_t>{1, 2});
    ASSERT_FALSE(partly);
    EXPECT_EQ(partly.error().code(), pop3::errc::no_such_message);
    ASSERT_TRUE(c->noop());   // the session goes on after a refused batch
    ASSERT_TRUE(c->reset());
    EXPECT_EQ(c->status()->messages, 2u);
    ASSERT_TRUE(c->remove(2));
    ASSERT_TRUE(c->quit());
    EXPECT_FALSE(c->noop());   // closed
    auto again = pop3::client::connect(s.address(), plain_options());
    ASSERT_TRUE(again);
    EXPECT_EQ(again->status()->messages, 1u);
    EXPECT_TRUE(again->close());
}

TEST(Pop3Client, Mechanisms) {
    Server s([](pop3::server& srv) {
        srv.apop_secret = [](const sgcl::string& user) -> optional<sgcl::string> {
            return user.view() == "alice" ? optional<sgcl::string>(sgcl::string("secret")) : nullopt;
        };
    });
    for (auto how : {pop3::mechanism::automatic, pop3::mechanism::plain, pop3::mechanism::user, pop3::mechanism::apop}) {
        auto o = plain_options();
        o.mechanism = how;
        auto c = pop3::client::connect(s.address(), o);
        ASSERT_TRUE(c) << int(how) << " " << str(c.error().message());
        EXPECT_EQ(c->status()->messages, 2u);
        c->quit();
        o.password = "wrong";
        auto bad = pop3::client::connect(s.address(), o);
        ASSERT_FALSE(bad);
        EXPECT_EQ(bad.error().code(), pop3::errc::authentication_failed) << int(how);
    }
    // APOP from a server that offers none
    Server plain;
    auto o = plain_options();
    o.mechanism = pop3::mechanism::apop;
    auto c = pop3::client::connect(plain.address(), o);
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), pop3::errc::not_supported);
    // no credentials: connected, not logged in
    pop3::client::options none;
    none.security = pop3::security::none;
    auto anon = pop3::client::connect(plain.address(), none);
    ASSERT_TRUE(anon);
    EXPECT_FALSE(anon->status());
}

TEST(Pop3Client, InUseIsAnError) {
    Server s;
    auto a = pop3::client::connect(s.address(), plain_options());
    ASSERT_TRUE(a);
    auto b = pop3::client::connect(s.address(), plain_options());
    ASSERT_FALSE(b);
    EXPECT_EQ(b.error().code(), pop3::errc::in_use);
}

TEST(Pop3Client, Security) {
    Server s([](pop3::server& srv) { srv.tls = server_tls(); });
    // automatic: STLS taken
    auto o = plain_options();
    o.security = pop3::security::automatic;
    o.tls = client_tls();
    auto c = pop3::client::connect(s.address(), o);
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(c->is_tls());
    EXPECT_FALSE(c->has("STLS"));
    EXPECT_EQ(c->status()->messages, 2u);
    c->quit();
    // none: refused in clear text by the server
    auto clear = pop3::client::connect(s.address(), plain_options());
    ASSERT_FALSE(clear);
    // a server without STLS: automatic and starttls refuse to go on
    Server bare;
    o.security = pop3::security::starttls;
    auto refused = pop3::client::connect(bare.address(), o);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), pop3::errc::starttls_unavailable);
    // TLS from the first byte: pop3s://
    Server implicit(nullptr, true);
    auto t = pop3::client::connect(sgcl::string("pop3s://alice:secret@" + str(implicit.address())), [] {
        pop3::client::options x;
        x.tls = client_tls();
        return x;
    }());
    ASSERT_TRUE(t) << str(t.error().message());
    EXPECT_TRUE(t->is_tls());
    EXPECT_EQ(t->list()->size(), 2u);
    // pop3:// with credentials in the URL
    auto u = pop3::client::connect(sgcl::string("pop3://bob:hunter2@" + str(bare.address())), [] {
        pop3::client::options x;
        x.security = pop3::security::none;
        return x;
    }());
    ASSERT_TRUE(u) << str(u.error().message());
    EXPECT_EQ(u->status()->messages, 0u);
}

TEST(Pop3Client, Boundaries) {
    pop3::client none;
    EXPECT_FALSE(none);
    Server s;
    auto c = pop3::client::connect(s.address(), plain_options());
    ASSERT_TRUE(c);
    pop3::client copy = *c;
    EXPECT_TRUE(copy == *c);
    pop3::client moved = std::move(copy);
    EXPECT_TRUE(moved == *c);
    EXPECT_FALSE(pop3::client::connect("pop3://", plain_options()));
    auto bad = pop3::client::connect("127.0.0.1:1", plain_options());
    ASSERT_FALSE(bad);
    // a server that is no POP3 server
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto junk = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            std::string hello = "220 smtp here\r\n";
            (void)co_await c->async_write(slice<const byte>(reinterpret_cast<const byte*>(hello.data()), hello.size()));
            (void)co_await c->async_close();
        }
    }(*l));
    auto m = pop3::client::connect(l->local_endpoint().to_string(), plain_options());
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), pop3::errc::malformed_response);
    junk.wait();
    // a connection the program made
    auto conn = net::tcp::connect(s.address());
    ASSERT_TRUE(conn);
    auto over = pop3::client::connect(*conn, plain_options("bob", "hunter2"));
    ASSERT_TRUE(over) << str(over.error().message());
    EXPECT_EQ(over->list()->size(), 0u);
    // error texts and the category
    error_code e = pop3::errc::in_use;
    EXPECT_EQ(std::string(e.category().name()), "pop3");
    EXPECT_EQ(e.message(), "the maildrop is in use");
}

TEST(Pop3Client, AsyncForms) {
    Server s;
    auto t = [](sgcl::string address) -> async::task<size_t> {
        auto c = co_await pop3::client::async_connect(address, plain_options());
        if (!c) {
            co_return 0;
        }
        auto ls = co_await c->async_list();
        auto m = co_await c->async_retrieve(1);
        auto st = co_await c->async_status();
        auto top = co_await c->async_top(1, 0);
        (void)co_await c->async_noop();
        (void)co_await c->async_quit();
        co_return ls && m && st && top ? ls->size() : 0;
    }(s.address());
    EXPECT_EQ(t.wait(), 2u);
}

// Python's poplib against the server: STAT, LIST, UIDL, RETR, TOP, DELE,
// RSET, CAPA, APOP, STLS, QUIT
TEST(Pop3Interop, PythonPoplib) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    Server s([](pop3::server& srv) {
        srv.tls = server_tls();
        srv.apop_secret = [](const sgcl::string& user) -> optional<sgcl::string> {
            return user.view() == "alice" ? optional<sgcl::string>(sgcl::string("secret")) : nullopt;
        };
    });
    std::string script = (source_root() / "tests/net/pop3/python/session.py").string();
    int status = 0;
    std::string out = run_command("python3 " + script + " " + std::to_string(s.listener.local_endpoint().port()) + " " + testdata("ca.pem") + " 2>&1", &status);
    EXPECT_EQ(status, 0) << out;
    EXPECT_EQ(out, "stat 2 143\nlist 2\nuidl 2\nretr ok\ntop ok\ncapa ok\nquit ok\napop ok\nafter 1\n") << out;
}
