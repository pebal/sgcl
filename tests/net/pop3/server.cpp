//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::pop3's server driven by raw command lines: RFC 1939's states and
// every command with its errors, CAPA, RESP-CODES (AUTH, IN-USE,
// SYS/TEMP), PIPELINING, AUTH PLAIN in both forms, APOP (RFC 1939's own
// example digest), STLS and what came pipelined in clear text, the
// limits, the end without QUIT, the shutdown; MD5's known answers.
#include "helpers.h"

#include <atomic>

using namespace pop3_test;

TEST(Pop3Md5, Rfc1321Vectors) {
    namespace d = sgcl::net::pop3::detail;
    EXPECT_EQ(d::md5_hex(""), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(d::md5_hex("a"), "0cc175b9c0f1b6a831c399e269772661");
    EXPECT_EQ(d::md5_hex("abc"), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(d::md5_hex("message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
    EXPECT_EQ(d::md5_hex("abcdefghijklmnopqrstuvwxyz"), "c3fcd3d76192e4007dfb496cca67e13b");
    EXPECT_EQ(d::md5_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"), "d174ab98d277d9f5a5611c2c9f419d9f");
    EXPECT_EQ(d::md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890"), "57edf4a22be3c955ac49da2e2107b67a");
    // RFC 1939 §7's APOP example
    EXPECT_EQ(d::md5_hex("<1896.697170952@dbc.mtview.ca.us>tanstaaf"), "c4c9334bac560ecc979e58001b3e22fb");
}

TEST(Pop3Server, Session) {
    Server s;
    Talk t(s.address());
    EXPECT_EQ(t.line(), "+OK POP3 server ready");
    EXPECT_EQ(t.ask("CAPA"), "+OK Capability list follows");
    std::string caps = t.lines();
    for (const char* c : {"TOP\n", "UIDL\n", "USER\n", "SASL PLAIN\n", "PIPELINING\n", "RESP-CODES\n", "AUTH-RESP-CODE\n"}) {
        EXPECT_NE(caps.find(c), std::string::npos) << c;
    }
    EXPECT_EQ(caps.find("STLS"), std::string::npos);   // no tls configured
    EXPECT_EQ(t.ask("STAT"), "-ERR Not in this state");
    EXPECT_EQ(t.ask("PASS secret"), "-ERR USER first");
    EXPECT_EQ(t.ask("USER alice"), "+OK Send PASS");
    EXPECT_EQ(t.ask("PASS wrong"), "-ERR [AUTH] Authentication failed");
    EXPECT_EQ(t.ask("user alice"), "+OK Send PASS");
    EXPECT_EQ(t.ask("pass secret").substr(0, 3), "+OK");
    EXPECT_EQ(t.ask("STAT"), "+OK 2 " + std::to_string(67 + 76));
    EXPECT_EQ(t.ask("LIST"), "+OK Scan listing follows");
    EXPECT_EQ(t.lines(), "1 67\n2 76\n");
    EXPECT_EQ(t.ask("LIST 2"), "+OK 2 76");
    EXPECT_EQ(t.ask("LIST 3"), "-ERR No such message");
    EXPECT_EQ(t.ask("LIST 0"), "-ERR No such message");
    EXPECT_EQ(t.ask("LIST x"), "-ERR No such message");
    EXPECT_EQ(t.ask("UIDL"), "+OK Unique ids follow");
    std::string uids = t.lines();
    EXPECT_NE(uids.find(".1\n"), std::string::npos);
    EXPECT_NE(uids.find(".2\n"), std::string::npos);
    EXPECT_EQ(t.ask("RETR 1"), "+OK 67 octets");
    EXPECT_EQ(t.lines(), "From: Bob <bob@example.com>\nSubject: Lunch\n\nNoon?\n..hidden dot\n");   // stuffed on the wire
    EXPECT_EQ(t.ask("TOP 2 1"), "+OK Top of message follows");
    EXPECT_EQ(t.lines(), "From: Carol <carol@example.com>\nSubject: Report\n\nLine 1\n");
    EXPECT_EQ(t.ask("TOP 2 0"), "+OK Top of message follows");
    EXPECT_EQ(t.lines(), "From: Carol <carol@example.com>\nSubject: Report\n\n");
    EXPECT_EQ(t.ask("TOP 2"), "-ERR Syntax: TOP msg n");
    EXPECT_EQ(t.ask("DELE 1"), "+OK Marked to be deleted");
    EXPECT_EQ(t.ask("DELE 1"), "-ERR No such message");
    EXPECT_EQ(t.ask("RETR 1"), "-ERR No such message");
    EXPECT_EQ(t.ask("STAT"), "+OK 1 76");
    EXPECT_EQ(t.ask("RSET").substr(0, 3), "+OK");
    EXPECT_EQ(t.ask("STAT"), "+OK 2 143");
    EXPECT_EQ(t.ask("DELE 2"), "+OK Marked to be deleted");
    EXPECT_EQ(t.ask("NOOP"), "+OK");
    EXPECT_EQ(t.ask("USER bob"), "-ERR Not in this state");
    EXPECT_EQ(t.ask("FOO"), "-ERR Unknown command");
    EXPECT_EQ(t.ask("QUIT"), "+OK Bye");
    EXPECT_EQ(t.line(), "EOF");
    auto box = s.mail.open("alice", "INBOX");
    ASSERT_TRUE(box);
    ASSERT_EQ(box->messages.size(), 1u);   // message 2 removed at QUIT
}

TEST(Pop3Server, EndWithoutQuitRemovesNothing) {
    Server s;
    {
        Talk t(s.address());
        t.line();
        t.ask("USER alice");
        t.ask("PASS secret");
        EXPECT_EQ(t.ask("DELE 1"), "+OK Marked to be deleted");
        (void)t.c.close();
    }
    // the session's end lets the maildrop go; a new login takes it
    for (int i = 0; i < 200; ++i) {
        Talk t(s.address());
        t.line();
        t.ask("USER alice");
        std::string r = t.ask("PASS secret");
        if (r.rfind("+OK", 0) == 0) {
            EXPECT_EQ(t.ask("STAT").substr(0, 5), "+OK 2");
            t.ask("QUIT");
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    FAIL() << "the maildrop stayed locked";
}

TEST(Pop3Server, InUse) {
    Server s;
    Talk a(s.address()), b(s.address());
    a.line();
    b.line();
    a.ask("USER alice");
    EXPECT_EQ(a.ask("PASS secret").substr(0, 3), "+OK");
    b.ask("USER alice");
    EXPECT_EQ(b.ask("PASS secret"), "-ERR [IN-USE] Maildrop already locked");
    b.ask("USER bob");
    EXPECT_EQ(b.ask("PASS hunter2"), "+OK Maildrop has 0 messages (0 octets)");
    EXPECT_EQ(b.ask("LIST"), "+OK Scan listing follows");
    EXPECT_EQ(b.lines(), "");
    EXPECT_EQ(a.ask("QUIT"), "+OK Bye");
    Talk c(s.address());
    c.line();
    c.ask("USER alice");
    EXPECT_EQ(c.ask("PASS secret").substr(0, 3), "+OK");   // free at once after QUIT's reply
}

TEST(Pop3Server, SaslPlain) {
    Server s;
    Talk t(s.address());
    t.line();
    // "\0alice\0wrong", then the continuation form, then a cancel
    EXPECT_EQ(t.ask("AUTH PLAIN AGFsaWNlAHdyb25n"), "-ERR [AUTH] Authentication failed");
    EXPECT_EQ(t.ask("AUTH PLAIN"), "+ ");
    EXPECT_EQ(t.ask("*"), "-ERR Authentication cancelled");
    EXPECT_EQ(t.ask("AUTH"), "+OK Methods follow");
    EXPECT_EQ(t.lines(), "PLAIN\n");
    EXPECT_EQ(t.ask("AUTH CRAM-MD5"), "-ERR Unsupported mechanism");
    EXPECT_EQ(t.ask("AUTH PLAIN"), "+ ");
    EXPECT_EQ(t.ask("AGFsaWNlAHNlY3JldA==").substr(0, 3), "+OK");   // "\0alice\0secret"
    EXPECT_EQ(t.ask("STAT").substr(0, 5), "+OK 2");
    // an authorization identity of another user is refused
    Talk u(s.address());
    u.line();
    EXPECT_EQ(u.ask("AUTH PLAIN Ym9iAGFsaWNlAHNlY3JldA=="), "-ERR [AUTH] Authentication failed");   // "bob\0alice\0secret"
    EXPECT_EQ(u.ask("AUTH PLAIN !!!"), "-ERR Invalid base64");
}

TEST(Pop3Server, Apop) {
    Server s([](pop3::server& srv) {
        srv.hostname = "mail.example.com";
        srv.apop_secret = [](const sgcl::string& user) -> optional<sgcl::string> {
            if (user.view() == "alice") {
                return sgcl::string("tanstaaf");
            }
            return nullopt;
        };
    });
    Talk t(s.address());
    std::string greeting = t.line();
    size_t lt = greeting.find('<'), gt = greeting.find('>');
    ASSERT_NE(lt, std::string::npos);
    ASSERT_NE(greeting.find("@mail.example.com>"), std::string::npos);
    std::string stamp = greeting.substr(lt, gt - lt + 1);
    std::string digest = sgcl::net::pop3::detail::md5_hex(stamp + "tanstaaf");
    EXPECT_EQ(t.ask("APOP alice 00000000000000000000000000000000"), "-ERR [AUTH] Authentication failed");
    EXPECT_EQ(t.ask("APOP carol " + digest), "-ERR [AUTH] Authentication failed");
    std::string upper = digest;
    for (char& c : upper) {
        c = char(std::toupper(c));
    }
    EXPECT_EQ(t.ask("APOP alice " + upper).substr(0, 3), "+OK");
    // without a secret, no APOP and no timestamp
    Server plain;
    Talk p(plain.address());
    EXPECT_EQ(p.line().find('<'), std::string::npos);
    EXPECT_EQ(p.ask("APOP alice " + digest), "-ERR APOP not available");
}

TEST(Pop3Server, AuthFailuresClose) {
    Server s([](pop3::server& srv) { srv.max_auth_failures = 2; });
    Talk t(s.address());
    t.line();
    t.ask("USER alice");
    EXPECT_EQ(t.ask("PASS a"), "-ERR [AUTH] Authentication failed");
    t.ask("USER alice");
    EXPECT_EQ(t.ask("PASS b"), "-ERR [AUTH] Too many failures");
    EXPECT_EQ(t.line(), "EOF");
}

TEST(Pop3Server, CheckPassword) {
    Server s([](pop3::server& srv) {
        srv.check_password = [](const sgcl::string& u, const sgcl::string& p) { return u.view() == "alice" && p.view() == "from-callback"; };
    });
    Talk t(s.address());
    t.line();
    t.ask("USER alice");
    EXPECT_EQ(t.ask("PASS secret"), "-ERR [AUTH] Authentication failed");
    t.ask("USER alice");
    EXPECT_EQ(t.ask("PASS from-callback").substr(0, 3), "+OK");
}

TEST(Pop3Server, Pipelining) {
    Server s;
    Talk t(s.address());
    t.line();
    t.send("USER alice\r\nPASS secret\r\nSTAT\r\nLIST 1\r\nUIDL 2\r\nNOOP\r\nQUIT\r\n");
    EXPECT_EQ(t.line(), "+OK Send PASS");
    EXPECT_EQ(t.line().substr(0, 3), "+OK");
    EXPECT_EQ(t.line(), "+OK 2 143");
    EXPECT_EQ(t.line(), "+OK 1 67");
    EXPECT_EQ(t.line().substr(0, 6), "+OK 2 ");
    EXPECT_EQ(t.line(), "+OK");
    EXPECT_EQ(t.line(), "+OK Bye");
    EXPECT_EQ(t.line(), "EOF");
}

TEST(Pop3Server, StlsAndClearTextRefused) {
    Server s([](pop3::server& srv) { srv.tls = server_tls(); });
    Talk t(s.address());
    t.line();
    EXPECT_EQ(t.ask("CAPA"), "+OK Capability list follows");
    std::string caps = t.lines();
    EXPECT_NE(caps.find("STLS"), std::string::npos);
    EXPECT_EQ(caps.find("USER"), std::string::npos);
    EXPECT_EQ(t.ask("USER alice"), "-ERR [AUTH] Credentials only over TLS: use STLS first");
    // what is pipelined after STLS in clear text is dropped
    t.send("STLS\r\nUSER alice\r\n");
    EXPECT_EQ(t.line(), "+OK Begin TLS negotiation");
    auto tc = net::tls::client(t.c, client_tls());
    ASSERT_TRUE(tc) << str(tc.error().message());
    Talk secure(s.address());
    secure.c = *tc;
    secure.c.set_deadline(sgcl::clock::now() + 10s);
    EXPECT_EQ(secure.ask("CAPA"), "+OK Capability list follows");
    caps = secure.lines();
    EXPECT_EQ(caps.find("STLS"), std::string::npos);
    EXPECT_NE(caps.find("USER"), std::string::npos);
    EXPECT_EQ(secure.ask("STLS"), "-ERR Already in TLS");
    EXPECT_EQ(secure.ask("USER alice"), "+OK Send PASS");
    EXPECT_EQ(secure.ask("PASS secret").substr(0, 3), "+OK");
    // allow_insecure_auth: clear text all the same
    Server lax([](pop3::server& srv) {
        srv.tls = server_tls();
        srv.allow_insecure_auth = true;
    });
    Talk l(lax.address());
    l.line();
    EXPECT_EQ(l.ask("USER alice"), "+OK Send PASS");
}

TEST(Pop3Server, LimitsAndTimeout) {
    Server s([](pop3::server& srv) {
        srv.max_connections = 1;
        srv.idle_timeout = 300ms;
    });
    Talk a(s.address());
    EXPECT_EQ(a.line(), "+OK POP3 server ready");
    Talk b(s.address());
    EXPECT_EQ(b.line(), "-ERR [SYS/TEMP] Too many connections");
    EXPECT_EQ(b.line(), "EOF");
    EXPECT_EQ(a.line(), "EOF");   // idle past the timeout
    Talk c(s.address());
    c.line();
    c.send(std::string(5000, 'x'));
    EXPECT_EQ(c.line(), "-ERR Line too long");
}

TEST(Pop3Server, LineFeedMessagesAndShutdown) {
    Server s;
    (void)s.mail.append("bob", "INBOX", "From: x@example.com\nSubject: lf\n\n.\nend");
    Talk t(s.address());
    t.line();
    t.ask("USER bob");
    t.ask("PASS hunter2");
    EXPECT_EQ(t.ask("RETR 1").substr(0, 3), "+OK");
    EXPECT_EQ(t.lines(), "From: x@example.com\nSubject: lf\n\n..\nend\n");
    std::string raw = t.buf;   // nothing more waits
    EXPECT_TRUE(raw.empty());
    std::atomic<bool> done{false};
    std::thread stopper([&] {
        s.srv.shutdown();
        done = true;
    });
    EXPECT_EQ(t.line(), "-ERR [SYS/TEMP] Server shutting down");
    EXPECT_EQ(t.line(), "EOF");
    stopper.join();
    EXPECT_TRUE(done);
    auto again = s.srv.serve("127.0.0.1:0");
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), net::errc::server_closed);
}

TEST(Pop3Server, MaildirBackend) {
    auto dir = std::filesystem::temp_directory_path() / ("sgcl-pop3-maildir-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    {
        net::imap::maildir_backend md(sgcl::string(dir.string()));
        ASSERT_TRUE(md.add_user("dave", "pw"));
        ASSERT_TRUE(md.append("dave", "INBOX", "From: e@example.com\r\nSubject: md\r\n\r\nin a maildir\r\n"));
        pop3::server srv;
        srv.backend = md;
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l);
        auto serving = async::spawn(srv.async_serve(*l));
        auto c = pop3::client::connect(l->local_endpoint().to_string(), plain_options("dave", "pw"));
        ASSERT_TRUE(c) << str(c.error().message());
        auto m = c->retrieve(1);
        ASSERT_TRUE(m);
        EXPECT_NE(m->find("in a maildir"), sgcl::string::npos);
        ASSERT_TRUE(c->remove(1));
        ASSERT_TRUE(c->quit());
        auto box = md.open("dave", "INBOX");
        ASSERT_TRUE(box);
        EXPECT_TRUE(box->messages.empty());
        srv.close();
        serving.wait();
    }
    std::filesystem::remove_all(dir);
}
