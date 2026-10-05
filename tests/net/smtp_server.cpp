//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp's server driven by raw command scripts: every command and its
// error paths, the order of pipelined replies, DATA's dots, BDAT with LAST,
// AUTH PLAIN and LOGIN, STARTTLS with clear text pipelined after it
// (CVE-2011-0411), the limits, the timeout, the handler's forms, the
// shutdown and the close; and the machine itself fed byte by byte.
#include "smtp_common.h"

#include <atomic>

using namespace smtp_test;
using namespace std::chrono_literals;

namespace {
    // A client of raw lines
    struct Talk {
        net::connection c;

        explicit Talk(const sgcl::string& address) {
            auto conn = net::tcp::connect(address);
            EXPECT_TRUE(conn);
            c = *conn;
            c.set_deadline(sgcl::clock::now() + 10s);
        }

        explicit Talk(const net::connection& conn)
        : c(conn) {
            c.set_deadline(sgcl::clock::now() + 10s);
        }

        void send(const std::string& text) {
            ASSERT_TRUE(c.write(sgcl::string(text)));
        }

        // One whole reply, its lines joined by "\n"; "EOF" at the end
        std::string reply() {
            std::string out;
            for (;;) {
                auto line = c.read_line();
                if (!line || !*line) {
                    return out.empty() ? "EOF" : out + "\nEOF";
                }
                std::string l = str(**line);
                if (!l.empty() && l.back() == '\r') {
                    l.pop_back();
                }
                if (!out.empty()) {
                    out += "\n";
                }
                out += l;
                if (l.size() < 4 || l[3] != '-') {
                    return out;
                }
            }
        }

        std::string say(const std::string& line) {
            send(line + "\r\n");
            return reply();
        }

        std::string code(const std::string& line) {
            return say(line).substr(0, 3);
        }
    };

    std::string first_line(const std::string& r) {
        return r.substr(0, r.find('\n'));
    }
}

TEST(SmtpServer_Tests, GreetingAndEhlo) {
    Running r;
    r.srv.hostname = "mx.test";
    r.start();
    Talk t(r.address());
    EXPECT_EQ(t.reply(), "220 mx.test ESMTP ready");
    auto ehlo = t.say("EHLO client.test");
    EXPECT_EQ(ehlo,
              "250-mx.test greets client.test\n250-PIPELINING\n250-SIZE 33554432\n250-8BITMIME\n250-SMTPUTF8\n250-ENHANCEDSTATUSCODES\n"
              "250-CHUNKING\n250-BINARYMIME\n250-DSN\n250 HELP");
    EXPECT_EQ(t.say("HELO client.test"), "250 mx.test");
    EXPECT_EQ(t.say("NOOP"), "250 2.0.0 OK");
    EXPECT_EQ(t.code("VRFY bob"), "252");
    EXPECT_EQ(t.code("EXPN list"), "502");
    EXPECT_EQ(t.code("HELP"), "214");
    EXPECT_EQ(t.code("STARTTLS"), "502");   // not offered without a config
    EXPECT_EQ(t.code("AUTH PLAIN AGFAYg=="), "502");
    EXPECT_EQ(t.say("QUIT"), "221 2.0.0 Bye");
    EXPECT_EQ(t.reply(), "EOF");
    r.stop();
}

TEST(SmtpServer_Tests, CommandErrorPaths) {
    Running r;
    r.srv.max_errors = 100;
    r.start();
    Talk t(r.address());
    t.reply();
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test>"), "503");   // before EHLO
    EXPECT_EQ(t.code("EHLO"), "501");
    EXPECT_EQ(t.code("EHLO c.test"), "250");
    EXPECT_EQ(t.code("RCPT TO:<x@y.test>"), "503");     // before MAIL
    EXPECT_EQ(t.code("DATA"), "503");
    EXPECT_EQ(t.code("MAIL FROM:a@b.test"), "501");     // no angle brackets
    EXPECT_EQ(t.code("MAIL TO:<a@b.test>"), "501");
    EXPECT_EQ(t.code("MAIL FROM:<not an address>"), "553");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> FOO=1"), "555");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> SIZE=x"), "501");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> SIZE=99999999999"), "552");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> BODY=9BIT"), "501");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> RET=ALL"), "501");
    EXPECT_EQ(t.code("MAIL FROM:<łucja@b.test>"), "553");   // no SMTPUTF8
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test>x"), "501");
    EXPECT_EQ(t.code("MAIL FROM: <a@b.test> SIZE=10 BODY=8BITMIME RET=FULL ENVID=id+2B1 AUTH=<>"), "250");
    EXPECT_EQ(t.code("MAIL FROM:<c@d.test>"), "503");   // nested
    EXPECT_EQ(t.code("RCPT TO:<>"), "553");
    EXPECT_EQ(t.code("RCPT TO:x@y.test"), "501");
    EXPECT_EQ(t.code("RCPT TO:<x@y.test> NOTIFY=SOMETIMES"), "501");
    EXPECT_EQ(t.code("RCPT TO:<x@y.test> XYZ=1"), "555");
    EXPECT_EQ(t.code("DATA"), "554");   // no recipient yet
    EXPECT_EQ(t.code("RCPT TO:<Postmaster>"), "250");
    EXPECT_EQ(t.code("RCPT TO:<@relay.test:x@y.test> NOTIFY=SUCCESS,DELAY ORCPT=rfc822;x+2Bz@y.test"), "250");
    EXPECT_EQ(t.code("DATA extra"), "501");
    EXPECT_EQ(t.code("RSET extra"), "501");
    EXPECT_EQ(t.code("RSET"), "250");
    EXPECT_EQ(t.code("RCPT TO:<x@y.test>"), "503");   // the transaction gone
    EXPECT_EQ(t.code("FOO"), "500");
    EXPECT_EQ(t.code(""), "500");
    EXPECT_EQ(t.code("BDAT x"), "501");
    EXPECT_EQ(t.code("STARTTLS now"), "502");
    EXPECT_EQ(t.code("QUIT"), "221");
    r.stop();
}

TEST(SmtpServer_Tests, AMessageByData) {
    Running r;
    r.start();
    Talk t(r.address());
    t.reply();
    t.say("EHLO c.test");
    EXPECT_EQ(t.code("MAIL FROM:<a@b.test> RET=HDRS ENVID=x+2By"), "250");
    EXPECT_EQ(t.code("RCPT TO:<x@y.test> NOTIFY=NEVER"), "250");
    EXPECT_EQ(t.code("RCPT TO:<z@y.test>"), "250");
    EXPECT_EQ(t.say("DATA"), "354 Start mail input; end with <CRLF>.<CRLF>");
    t.send("Subject: dots\r\n\r\n..leading\r\n. space\r\n...\r\nlast line\r\n.\r\n");
    EXPECT_EQ(t.reply(), "250 2.0.0 OK: message accepted");
    ASSERT_EQ(r.taken->size(), 1u);
    EXPECT_EQ(r.taken->text(0), "Subject: dots\r\n\r\n.leading\r\n space\r\n..\r\nlast line\r\n");
    auto e = r.taken->envelope(0);
    EXPECT_EQ(str(e.ret), "HDRS");
    EXPECT_EQ(str(e.envid), "x+y");
    ASSERT_EQ(e.notify.size(), 2u);
    EXPECT_EQ(str(e.notify[0]), "NEVER");
    EXPECT_EQ(str(e.notify[1]), "");
    EXPECT_TRUE(e.orcpt.empty());
    EXPECT_EQ(str(e.helo), "c.test");
    // a second transaction on the same session, data in pieces of a byte
    t.say("MAIL FROM:<>");
    t.say("RCPT TO:<x@y.test>");
    t.say("DATA");
    std::string body = "a\r\n.\r\r\n.b\r\n\r\n.\r\n";
    for (char ch : body) {
        t.send(std::string(1, ch));
    }
    EXPECT_EQ(t.reply(), "250 2.0.0 OK: message accepted");
    EXPECT_EQ(r.taken->text(1), "a\r\n\r\r\nb\r\n\r\n");
    EXPECT_EQ(str(r.taken->envelope(1).from), "");
    r.stop();
}

TEST(SmtpServer_Tests, PipelinedBatch) {
    Running r;
    r.srv.hostname = "mx.test";
    r.srv.on_recipient = [](const smtp::envelope&, const sgcl::string& to) {
        return to.view().starts_with("bad") ? smtp::reply{550, sgcl::string("5.1.1"), sgcl::string("no")} : smtp::reply();
    };
    r.start();
    Talk t(r.address());
    t.reply();
    t.send("EHLO c.test\r\nMAIL FROM:<a@b.test>\r\nRCPT TO:<bad@y.test>\r\nRCPT TO:<ok@y.test>\r\nDATA\r\nSubject: p\r\n\r\nbody\r\n.\r\nNOOP\r\n");
    EXPECT_EQ(first_line(t.reply()), "250-mx.test greets c.test");
    EXPECT_EQ(t.reply(), "250 2.1.0 Sender OK");
    EXPECT_EQ(t.reply(), "550 5.1.1 no");
    EXPECT_EQ(t.reply(), "250 2.1.5 Recipient OK");
    EXPECT_EQ(first_line(t.reply()), "354 Start mail input; end with <CRLF>.<CRLF>");
    EXPECT_EQ(t.reply(), "250 2.0.0 OK: message accepted");
    EXPECT_EQ(t.reply(), "250 2.0.0 OK");
    ASSERT_EQ(r.taken->size(), 1u);
    ASSERT_EQ(r.taken->envelope(0).to.size(), 1u);
    // every recipient refused, DATA pipelined after them: 554, the bytes read as commands
    t.send("MAIL FROM:<a@b.test>\r\nRCPT TO:<bad@y.test>\r\nDATA\r\nRSET\r\n");
    EXPECT_EQ(t.reply(), "250 2.1.0 Sender OK");
    EXPECT_EQ(t.reply(), "550 5.1.1 no");
    EXPECT_EQ(t.reply(), "554 5.5.1 No valid recipients");
    EXPECT_EQ(t.reply(), "250 2.0.0 OK");
    r.stop();
}

TEST(SmtpServer_Tests, Bdat) {
    Running r;
    r.start();
    Talk t(r.address());
    t.reply();
    t.say("EHLO c.test");
    t.say("MAIL FROM:<a@b.test> BODY=BINARYMIME");
    t.say("RCPT TO:<x@y.test>");
    EXPECT_EQ(t.code("DATA"), "503");   // BINARYMIME takes BDAT
    t.send(std::string("BDAT 5\r\nab\0\r\n", 13));
    EXPECT_EQ(t.reply(), "250 2.0.0 Chunk received");
    t.send(std::string("BDAT 3 LAST\r\nxyz", 16));
    EXPECT_EQ(t.reply(), "250 2.0.0 OK: message accepted");
    ASSERT_EQ(r.taken->size(), 1u);
    EXPECT_EQ(r.taken->text(0), std::string("ab\0\r\nxyz", 8));
    EXPECT_EQ(str(r.taken->envelope(0).body), "BINARYMIME");
    // BDAT 0 LAST, pipelined with the rest
    t.send("MAIL FROM:<a@b.test>\r\nRCPT TO:<x@y.test>\r\nBDAT 0 LAST\r\n");
    t.reply();
    t.reply();
    EXPECT_EQ(t.reply(), "250 2.0.0 OK: message accepted");
    EXPECT_EQ(r.taken->text(1), "");
    // BDAT without a transaction: the bytes read and refused, the session in step
    t.send("BDAT 4\r\nQUIT");
    EXPECT_EQ(t.code("NOOP"), "503");
    EXPECT_EQ(t.reply().substr(0, 3), "250");
    // DATA after a chunk is refused
    t.say("MAIL FROM:<a@b.test>");
    t.say("RCPT TO:<x@y.test>");
    t.send("BDAT 2\r\nhi");
    t.reply();
    EXPECT_EQ(t.code("DATA"), "503");
    r.stop();
}

TEST(SmtpServer_Tests, Authentication) {
    Running r;
    r.srv.auth = [](const sgcl::string& u, const sgcl::string& p) { return u == "alice" && p == "secret"; };
    r.srv.allow_insecure_auth = true;
    r.start();
    auto b64 = [](std::string_view s) { return str(encoding::base64::standard.encode(sgcl::string(s))); };
    {
        Talk t(r.address());
        t.reply();
        EXPECT_EQ(t.code("AUTH PLAIN x"), "503");   // before EHLO
        EXPECT_NE(t.say("EHLO c.test").find("250-AUTH PLAIN LOGIN"), std::string::npos);
        EXPECT_EQ(t.code("AUTH CRAM-MD5"), "504");
        EXPECT_EQ(t.code("AUTH PLAIN !!!"), "501");
        EXPECT_EQ(t.code("AUTH PLAIN " + b64(std::string("\0alice\0wrong", 12))), "535");
        EXPECT_EQ(t.code("AUTH PLAIN " + b64(std::string("bob\0alice\0secret", 16))), "535");   // another authorization identity
        EXPECT_EQ(t.code("AUTH PLAIN " + b64("no nuls")), "501");
        EXPECT_EQ(t.say("AUTH PLAIN"), "334 ");
        EXPECT_EQ(t.code("*"), "501");
        EXPECT_EQ(t.say("AUTH PLAIN"), "334 ");
        EXPECT_EQ(t.say(b64(std::string("alice\0alice\0secret", 18))), "235 2.7.0 Authentication successful");
        EXPECT_EQ(t.code("AUTH PLAIN x"), "503");   // once
        t.say("MAIL FROM:<a@b.test>");
        t.say("RCPT TO:<x@y.test>");
        t.say("DATA");
        EXPECT_EQ(t.say("x\r\n."), "250 2.0.0 OK: message accepted");
        EXPECT_EQ(str(r.taken->envelope(0).user), "alice");
    }
    {
        Talk t(r.address());
        t.reply();
        t.say("EHLO c.test");
        EXPECT_EQ(t.say("AUTH LOGIN"), "334 VXNlcm5hbWU6");
        EXPECT_EQ(t.say(b64("alice")), "334 UGFzc3dvcmQ6");
        EXPECT_EQ(t.code(b64("secret")), "235");
    }
    {
        Talk t(r.address());
        t.reply();
        t.say("EHLO c.test");
        EXPECT_EQ(t.say("AUTH LOGIN " + b64("alice")), "334 UGFzc3dvcmQ6");
        EXPECT_EQ(t.code(b64("bad")), "535");
        EXPECT_EQ(t.say("AUTH LOGIN"), "334 VXNlcm5hbWU6");
        EXPECT_EQ(t.code("&&&"), "501");
        t.say("MAIL FROM:<a@b.test>");
        EXPECT_EQ(t.code("AUTH PLAIN x"), "503");   // not inside a transaction
    }
    r.stop();

    Running tls_only;
    tls_only.srv.auth = [](const sgcl::string&, const sgcl::string&) { return true; };
    tls_only.start();
    Talk t(tls_only.address());
    t.reply();
    EXPECT_EQ(t.say("EHLO c.test").find("AUTH"), std::string::npos);
    EXPECT_EQ(t.code("AUTH PLAIN AGEAYg=="), "538");
    tls_only.stop();
}

TEST(SmtpServer_Tests, StartTlsDropsWhatCameInClearText) {
    Running r;
    r.srv.starttls = serving();
    r.start();
    Talk t(r.address());
    t.reply();
    EXPECT_NE(t.say("EHLO c.test").find("250-STARTTLS"), std::string::npos);
    // a command pipelined after STARTTLS in clear text (CVE-2011-0411)
    t.send("STARTTLS\r\nMAIL FROM:<injected@evil.test>\r\nRCPT TO:<x@y.test>\r\n");
    EXPECT_EQ(t.reply(), "220 2.0.0 Ready to start TLS");
    auto tc = net::tls::client(t.c, trusted());
    ASSERT_TRUE(tc) << str(tc.error().message());
    Talk s(*tc);
    // the session starts again: no EHLO, no transaction from before
    EXPECT_EQ(s.code("RCPT TO:<x@y.test>"), "503");
    EXPECT_EQ(s.code("MAIL FROM:<a@b.test>"), "503");
    auto ehlo = s.say("EHLO c.test");
    EXPECT_EQ(ehlo.find("STARTTLS"), std::string::npos);
    EXPECT_EQ(s.code("STARTTLS"), "502");
    EXPECT_EQ(s.code("MAIL FROM:<a@b.test>"), "250");
    EXPECT_EQ(s.code("RCPT TO:<x@y.test>"), "250");
    EXPECT_EQ(s.code("DATA"), "354");
    EXPECT_EQ(s.code("over tls\r\n."), "250");
    EXPECT_TRUE(r.taken->envelope(0).tls);
    EXPECT_EQ(str(r.taken->envelope(0).from), "a@b.test");
    // STARTTLS before EHLO
    Talk u(r.address());
    u.reply();
    EXPECT_EQ(u.code("STARTTLS"), "503");
    r.stop();
}

TEST(SmtpServer_Tests, Limits) {
    Running r;
    r.srv.max_message_bytes = 100;
    r.srv.max_recipients = 2;
    r.srv.max_line_bytes = 64;
    r.srv.max_errors = 3;
    r.srv.max_junk_commands = 5;
    r.start();
    {
        Talk t(r.address());
        t.reply();
        t.say("EHLO c.test");
        t.say("MAIL FROM:<a@b.test>");
        EXPECT_EQ(t.code("RCPT TO:<1@y.test>"), "250");
        EXPECT_EQ(t.code("RCPT TO:<2@y.test>"), "250");
        EXPECT_EQ(t.say("RCPT TO:<3@y.test>"), "452 4.5.3 Too many recipients");
        t.say("DATA");
        t.send(std::string(300, 'x') + "\r\n.\r\n");
        EXPECT_EQ(t.reply(), "552 5.3.4 Message size exceeds the limit");
        EXPECT_EQ(t.code("MAIL FROM:<a@b.test> SIZE=101"), "552");
        t.say("MAIL FROM:<a@b.test>");
        t.say("RCPT TO:<1@y.test>");
        t.send("BDAT 200 LAST\r\n" + std::string(200, 'y'));
        EXPECT_EQ(t.reply(), "552 5.3.4 Message size exceeds the limit");
        EXPECT_EQ(r.taken->size(), 0u);
        EXPECT_EQ(t.say(std::string(100, 'L')), "500 5.5.2 Line too long");
        EXPECT_EQ(t.code("NOOP"), "250");   // the long line skipped whole
        EXPECT_EQ(t.say("FOO"), "500 5.5.1 Command not recognized");
        EXPECT_EQ(t.reply(), "421 4.7.0 Too many errors, closing");
        EXPECT_EQ(t.reply(), "EOF");
    }
    {
        Talk t(r.address());
        t.reply();
        t.say("EHLO c.test");
        for (int i : range(4)) {
            (void)i;
            EXPECT_EQ(t.code("NOOP"), "250");
        }
        EXPECT_EQ(t.say("NOOP"), "250 2.0.0 OK");
        EXPECT_EQ(t.reply(), "421 4.7.0 Too many commands without a message, closing");
        EXPECT_EQ(t.reply(), "EOF");
    }
    r.stop();
}

TEST(SmtpServer_Tests, TooManyErrorsEndTheSession) {
    Running r;
    r.srv.max_errors = 3;
    r.start();
    Talk t(r.address());
    t.reply();
    EXPECT_EQ(t.code("FOO"), "500");
    EXPECT_EQ(t.code("BAR"), "500");
    EXPECT_EQ(t.say("BAZ"), "500 5.5.1 Command not recognized");
    EXPECT_EQ(t.reply(), "421 4.7.0 Too many errors, closing");
    EXPECT_EQ(t.reply(), "EOF");
    r.stop();
}

TEST(SmtpServer_Tests, TimeoutSays421) {
    Running r;
    r.srv.timeout = 200ms;
    r.start();
    Talk t(r.address());
    t.reply();
    auto start = sgcl::clock::now();
    EXPECT_EQ(t.reply(), "421 4.4.2 Timeout, closing");
    EXPECT_EQ(t.reply(), "EOF");
    EXPECT_LT(sgcl::clock::now() - start, 3s);
    r.stop();
}

TEST(SmtpServer_Tests, HandlerForms) {
    {
        Running r;
        r.srv.handle([](smtp::message m) -> async::task<smtp::reply> {
            co_await async::sleep(10ms);
            sgcl::vector<byte> b(4);
            auto n = co_await m.async_read(b.as_slice());
            co_return smtp::reply{250, sgcl::string("2.0.0"), sgcl::string(std::to_string(m.size()) + " bytes, " + std::to_string(*n))};
        });
        r.start(false);
        auto got = smtp::send(r.url(), simple_message());
        ASSERT_TRUE(got);
        EXPECT_NE(str(got->reply.text).find(" bytes, 4"), std::string::npos);
        r.stop();
    }
    {
        Running r;
        std::atomic<int> runs{0};
        r.srv.handle([&runs](smtp::message) -> async::task<> {
            ++runs;
            co_return;
        });
        r.start(false);
        ASSERT_TRUE(smtp::send(r.url(), simple_message()));
        EXPECT_EQ(runs.load(), 1);
        r.stop();
    }
    {
        Running r;
        std::atomic<int> reported{0};
        r.srv.on_error = [&reported](const sgcl::string& what) {
            if (what.view().find("boom") != std::string_view::npos) {
                ++reported;
            }
        };
        r.srv.handle([](smtp::message) { throw std::runtime_error("boom"); });
        r.start(false);
        auto got = smtp::send(r.url(), simple_message());
        ASSERT_FALSE(got);
        EXPECT_EQ(smtp::reply_of(got.error())->code, 451);
        EXPECT_EQ(reported.load(), 1);
        r.stop();
    }
    {
        // no handler: accepted and dropped
        Running r;
        r.start(false);
        EXPECT_TRUE(smtp::send(r.url(), simple_message()));
        r.stop();
    }
}

TEST(SmtpServer_Tests, TheMessage) {
    Running r;
    std::mutex m;
    sgcl::tracked_ptr<sgcl::vector<sgcl::string>> seen = sgcl::make_tracked<sgcl::vector<sgcl::string>>();
    r.srv.handle([seen, &m](smtp::message msg) {
        std::string read;
        sgcl::vector<byte> buf(7);
        for (;;) {
            auto n = msg.read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            read.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        auto again = msg.read(buf.as_slice());
        std::lock_guard g(m);
        seen->push_back(sgcl::string(read));
        seen->push_back(sgcl::string(std::to_string(msg.size()) + " " + std::to_string(*again)));
        seen->push_back(msg.email()->subject());
        encoding::email::limits l;
        l.max_header_bytes = 4;
        seen->push_back(sgcl::string(msg.email(l) ? "parsed" : "limited"));
        seen->push_back(sgcl::string(str(msg.bytes()) == read ? "same" : "differs"));
        seen->push_back(sgcl::string(bool(msg) ? "message" : "none"));
    });
    r.start(false);
    ASSERT_TRUE(smtp::send(r.url(), simple_message()));
    std::lock_guard g(m);
    ASSERT_EQ(seen->size(), 6u);
    EXPECT_NE(str((*seen)[0]).find("Subject: Hello"), std::string::npos);
    EXPECT_EQ(str((*seen)[1]), std::to_string((*seen)[0].size()) + " 0");
    EXPECT_EQ(str((*seen)[2]), "Hello");
    EXPECT_EQ(str((*seen)[3]), "limited");
    EXPECT_EQ(str((*seen)[4]), "same");
    EXPECT_EQ(str((*seen)[5]), "message");
    smtp::message none;
    EXPECT_FALSE(none);
    r.stop();
}

TEST(SmtpServer_Tests, ShutdownAndClose) {
    Running r;
    r.start();
    Talk idle(r.address());
    idle.reply();
    idle.say("EHLO c.test");
    auto done = async::spawn(r.srv.async_shutdown());
    done.wait();
    EXPECT_EQ(idle.reply(), "EOF");
    auto served = r.serving.wait();
    ASSERT_FALSE(served);
    EXPECT_EQ(served.error().code(), net::errc::server_closed);
    auto after = r.srv.serve(*net::tcp::listen("127.0.0.1:0"));
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), net::errc::server_closed);

    Running c;
    c.start();
    Talk busy(c.address());
    busy.reply();
    c.srv.close();
    EXPECT_EQ(busy.reply(), "EOF");
    EXPECT_EQ(c.serving.wait().error().code(), net::errc::server_closed);

    // a copy is the same server; serve_tls and serve(address)
    smtp::server srv;
    smtp::server copy = srv;
    copy.close();
    EXPECT_FALSE(srv.serve("127.0.0.1:0"));
    smtp::server s2;
    auto t = async::spawn(s2.async_serve_tls("127.0.0.1:0", serving()));
    std::this_thread::sleep_for(50ms);
    s2.shutdown();
    EXPECT_EQ(t.wait().error().code(), net::errc::server_closed);
    smtp::server s3;
    auto t3 = async::spawn(s3.async_serve("127.0.0.1:0"));
    std::this_thread::sleep_for(50ms);
    s3.close();
    EXPECT_EQ(t3.wait().error().code(), net::errc::server_closed);
    EXPECT_FALSE(smtp::server().serve("not an address"));
    EXPECT_FALSE(smtp::server().serve_tls("not an address", serving()));
}

// The machine itself: the same session fed whole and a byte at a time
// gives the same replies
TEST(SmtpServer_Tests, TheMachineInPieces) {
    const std::string script =
        "EHLO c.test\r\nMAIL FROM:<a@b.test> SIZE=20\r\nRCPT TO:<x@y.test>\r\nDATA\r\nSubject: s\r\n\r\n.x\r\n.\r\n"
        "MAIL FROM:<a@b.test>\r\nRCPT TO:<x@y.test>\r\nBDAT 3\r\nabcBDAT 1 LAST\r\nd"
        "BOGUS\r\nQUIT\r\n";
    auto run_it = [&](size_t piece) {
        smtp::detail::ServerMachine m;
        m.cfg.hostname = "mx.test";
        m.greet();
        std::string data;
        for (size_t i = 0; i < script.size(); i += piece) {
            m.feed(std::string_view(script).substr(i, piece));
            for (;;) {
                auto s = m.step();
                if (s == smtp::detail::MachineStep::message) {
                    data += m.data() + "|";
                    m.message_done(smtp::reply());
                    continue;
                }
                break;
            }
        }
        return m.out + "#" + data;
    };
    auto whole = run_it(script.size());
    EXPECT_NE(whole.find("221 2.0.0 Bye"), std::string::npos) << whole;
    EXPECT_NE(whole.find("#Subject: s\r\n\r\nx\r\n|abcd|"), std::string::npos) << whole;
    for (size_t p : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(64)}) {
        EXPECT_EQ(run_it(p), whole) << p;
    }
}
