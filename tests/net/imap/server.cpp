//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::server over raw connections: RFC 9051 §8's session, every
// command in the states it is allowed in and refused in the others,
// literals synchronizing and not, the limits, malformed commands, the
// mechanisms of AUTHENTICATE, STARTTLS and the clear text injected behind
// it, implicit TLS, modified UTF-7 names for a client without UTF-8, two
// sessions on one mailbox (IDLE told of EXISTS, FETCH and EXPUNGE, an
// expunge while the other fetches), CONDSTORE, QRESYNC, ESEARCH,
// SEARCHRES, LIST-EXTENDED and LIST-STATUS, SPECIAL-USE, MOVE, BINARY,
// SORT, THREAD, QUOTA, COMPRESS=DEFLATE, the shutdown.
#include "tests/net/imap/helpers.h"

#include <string>
#include <thread>

using namespace sgcl;
using namespace sgcl_test::imap;
namespace imap = sgcl::net::imap;

TEST(ImapServer, Rfc9051Session) {
    auto mail = standard_mail();
    for (int i = 1; i <= 12; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i), "line one\r\nline two\r\n"));
    }
    Server s(mail);
    Raw c(s.port);
    std::string g = c.line();
    EXPECT_TRUE(contains(g, "* OK [CAPABILITY IMAP4rev1 IMAP4rev2")) << g;
    std::string r = c.cmd("a001", "login alice secret");
    EXPECT_TRUE(contains(r, "a001 OK [CAPABILITY ")) << r;
    r = c.cmd("a002", "select inbox");
    EXPECT_TRUE(contains(r, "* 12 EXISTS")) << r;
    EXPECT_TRUE(contains(r, "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)")) << r;
    EXPECT_TRUE(contains(r, "* OK [UIDVALIDITY ")) << r;
    EXPECT_TRUE(contains(r, "* OK [UIDNEXT 13]")) << r;
    EXPECT_TRUE(contains(r, "a002 OK [READ-WRITE] SELECT completed")) << r;
    r = c.cmd("a003", "fetch 12 full");
    EXPECT_TRUE(contains(r, "* 12 FETCH (FLAGS () INTERNALDATE \"")) << r;
    EXPECT_TRUE(contains(r, "ENVELOPE (\"Mon, 5 Oct 2026 10:00:00 +0200\" \"m12\" ((\"Bob\" NIL \"bob\" \"example.com\"))")) << r;
    EXPECT_TRUE(contains(r, "BODY (\"TEXT\" \"PLAIN\" (\"CHARSET\" \"us-ascii\") NIL NIL \"7BIT\" 20 2))")) << r;
    r = c.cmd("a004", "fetch 12 body[header]");
    EXPECT_TRUE(contains(r, "* 12 FETCH (BODY[HEADER] {")) << r;
    EXPECT_TRUE(contains(r, "FLAGS (\\Seen))")) << r;   // a body read marks it seen
    r = c.cmd("a005", "store 12 +flags \\deleted");
    EXPECT_TRUE(contains(r, "* 12 FETCH (FLAGS (\\Seen \\Deleted))")) << r;
    EXPECT_TRUE(contains(r, "a005 OK")) << r;
    r = c.cmd("a006", "logout");
    EXPECT_TRUE(contains(r, "* BYE")) << r;
    EXPECT_TRUE(contains(r, "a006 OK")) << r;
    EXPECT_EQ(c.line(), "<eof>");
}

TEST(ImapServer, CommandsByState) {
    auto mail = standard_mail();
    Server s(mail);
    Raw c(s.port);
    c.line();
    EXPECT_TRUE(contains(c.cmd("t1", "SELECT INBOX"), "t1 BAD"));
    EXPECT_TRUE(contains(c.cmd("t2", "UID FETCH 1 FLAGS"), "t2 BAD"));
    EXPECT_TRUE(contains(c.cmd("t3", "NOOP"), "t3 OK"));
    EXPECT_TRUE(contains(c.cmd("t4", "CAPABILITY"), "* CAPABILITY IMAP4rev1"));
    EXPECT_TRUE(contains(c.cmd("t5", "FROBNICATE"), "t5 BAD"));
    EXPECT_TRUE(contains(c.cmd("t6", "LOGIN alice secret"), "t6 OK"));
    EXPECT_TRUE(contains(c.cmd("t7", "LOGIN alice secret"), "t7 BAD"));
    EXPECT_TRUE(contains(c.cmd("t8", "FETCH 1 FLAGS"), "t8 BAD No mailbox selected"));
    EXPECT_TRUE(contains(c.cmd("t9", "EXPUNGE"), "t9 BAD"));
    EXPECT_TRUE(contains(c.cmd("t10", "SELECT nonexistent"), "t10 NO [NONEXISTENT]"));
    EXPECT_TRUE(contains(c.cmd("t11", "EXAMINE INBOX"), "t11 OK [READ-ONLY]"));
    std::string ro = c.cmd("t12", "STORE 1 +FLAGS \\Seen");   // read only, and no message 1
    EXPECT_TRUE(contains(ro, "t12 NO") || contains(ro, "t12 BAD")) << ro;
    EXPECT_TRUE(contains(c.cmd("t13", "UNSELECT"), "t13 OK"));
    EXPECT_TRUE(contains(c.cmd("t14", "UNSELECT"), "t14 BAD"));
    c.send("\r\n");                                   // an empty line: passed over
    std::string r = c.cmd("t15", "NOOP");
    EXPECT_TRUE(contains(r, "t15 OK")) << r;
    c.send("+tag NOOP\r\n");                          // "+" is not a tag character
    EXPECT_TRUE(contains(c.line(), "BAD"));
}

TEST(ImapServer, MalformedCommandsKeepTheSession) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("x"));
    Server s(mail);
    Raw c(s.port);
    c.login();
    c.cmd("s", "SELECT INBOX");
    const char* bad[] = {"FETCH", "FETCH 1", "FETCH 1 (FLAGS", "FETCH 1 BODY[9.X]", "FETCH 0 FLAGS", "FETCH 1:x FLAGS", "FETCH 1 (FLAGS) (BOGUS)",
                         "STORE 1 FLAGS", "STORE 1 +FLAGS (\\Bogus)", "STORE 1 COLOR red", "SEARCH", "SEARCH NOTAKEY", "SEARCH (SEEN", "COPY 1",
                         "LIST", "LIST \"\"", "STATUS INBOX", "STATUS INBOX ()", "STATUS INBOX (BOGUS)", "APPEND INBOX", "APPEND INBOX (\\Recent) {1+}\r\nx",
                         "SELECT", "CREATE", "RENAME a", "SORT (DATE)", "SORT () UTF-8 ALL", "THREAD BOGUS UTF-8 ALL", "ENABLE", "ID (\"a\")", "UID",
                         "UID BOGUS 1", "UID EXPUNGE", "IDLE extra", "SUBSCRIBE", "FETCH 1 BINARY[1.HEADER]", "FETCH 1 BODY[1]<5>", "FETCH 1 BODY[]<1.0>"};
    int n = 0;
    for (const char* b : bad) {
        const std::string tag = "b" + std::to_string(++n);
        std::string r = c.cmd(tag, b);
        EXPECT_TRUE(contains(r, tag + " BAD") || contains(r, tag + " NO")) << b << " -> " << r;
    }
    EXPECT_TRUE(contains(c.cmd("z", "NOOP"), "z OK"));
}

TEST(ImapServer, Literals) {
    auto mail = standard_mail();
    Server s(mail);
    Raw c(s.port);
    c.line();
    // synchronizing: the continuation comes before each literal
    c.send("l1 LOGIN {5}\r\n");
    EXPECT_EQ(c.line().rfind("+ ", 0), 0u);
    c.send("alice {6}\r\n");
    EXPECT_EQ(c.line().rfind("+ ", 0), 0u);
    c.send("secret\r\n");
    EXPECT_TRUE(contains(c.until("l1"), "l1 OK"));
    // non-synchronizing: no continuation
    const std::string msg = msg_text("plus");
    c.send("l2 APPEND INBOX (\\Flagged) {" + std::to_string(msg.size()) + "+}\r\n" + msg + "\r\n");
    std::string r = c.until("l2");
    EXPECT_TRUE(contains(r, "l2 OK [APPENDUID ")) << r;
    EXPECT_FALSE(contains(r, "+ Ready"));
    // a literal inside a search
    c.cmd("s", "SELECT INBOX");
    c.send("l3 SEARCH SUBJECT {4+}\r\nplus\r\n");
    r = c.until("l3");
    EXPECT_TRUE(contains(r, "* ESEARCH") || contains(r, "* SEARCH 1")) << r;
    // MULTIAPPEND with a date and literal8
    c.send("l4 APPEND INBOX \"17-Jul-1996 02:44:25 -0700\" {3+}\r\nA\r\n (\\Seen) ~{3+}\r\nB\r\n\r\n");
    r = c.until("l4");
    EXPECT_TRUE(contains(r, "l4 OK [APPENDUID ")) << r;
    EXPECT_TRUE(contains(r, ":")) << r;   // two UIDs, a range
    r = c.cmd("l5", "FETCH 2 INTERNALDATE");
    EXPECT_TRUE(contains(r, "INTERNALDATE \"17-Jul-1996 02:44:25 -0700\"")) << r;
}

TEST(ImapServer, Limits) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) {
        srv.max_literal_bytes = 100;
        srv.max_command_bytes = 200;
    });
    Raw c(s.port);
    c.login();
    EXPECT_TRUE(contains(c.cmd("c", "CAPABILITY"), "APPENDLIMIT=100"));
    // a synchronizing literal too big: refused, the session goes on
    c.send("a1 APPEND INBOX {101}\r\n");
    std::string r = c.line();
    EXPECT_TRUE(contains(r, "a1 BAD [TOOBIG]")) << r;
    EXPECT_TRUE(contains(c.cmd("a2", "NOOP"), "a2 OK"));
    // a line too long ends the connection
    c.send("a3 NOOP " + std::string(300, 'x') + "\r\n");
    r = c.line();
    EXPECT_TRUE(contains(r, "* BYE [TOOBIG]")) << r;
    EXPECT_EQ(c.line(), "<eof>");
    // a non-synchronizing literal too big: refused and the connection ended
    Raw c2(s.port);
    c2.login();
    c2.send("b1 APPEND INBOX {500+}\r\n" + std::string(500, 'x') + "\r\n");
    r = c2.until("b1");
    EXPECT_TRUE(contains(r, "b1 BAD [TOOBIG]")) << r;
    EXPECT_TRUE(contains(c2.line(), "* BYE"));
}

TEST(ImapServer, TooManyConnections) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) { srv.max_connections = 1; });
    Raw a(s.port);
    EXPECT_TRUE(contains(a.line(), "* OK"));
    Raw b(s.port);
    EXPECT_TRUE(contains(b.line(), "* BYE [UNAVAILABLE]"));
}

TEST(ImapServer, LoginTimeout) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) { srv.login_timeout = 300ms; });
    Raw a(s.port);
    a.line();
    EXPECT_EQ(a.line(3000ms), "<eof>");
}

TEST(ImapServer, LoginFailures) {
    auto mail = standard_mail();
    Server s(mail);
    Raw c(s.port);
    c.line();
    EXPECT_TRUE(contains(c.cmd("f1", "LOGIN alice wrong"), "f1 NO [AUTHENTICATIONFAILED]"));
    EXPECT_TRUE(contains(c.cmd("f2", "LOGIN nobody x"), "f2 NO [AUTHENTICATIONFAILED]"));
    std::string r = c.cmd("f3", "LOGIN \"\" x");
    EXPECT_TRUE(contains(r, "f3 NO")) << r;
    EXPECT_TRUE(contains(c.line(), "* BYE"));   // after the third failure
    EXPECT_EQ(c.line(), "<eof>");
}

TEST(ImapServer, AuthenticateMechanisms) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) {
        srv.check_token = [](const sgcl::string& user, const sgcl::string& token) { return user == "alice" && token == "tok"; };
    });
    auto b64 = [](const std::string& v) {
        std::string out;
        net::imap::detail::encode_base64(v, out);
        return out;
    };
    {
        Raw c(s.port);
        std::string g = c.line();
        EXPECT_TRUE(contains(g, "AUTH=PLAIN AUTH=LOGIN AUTH=XOAUTH2 AUTH=OAUTHBEARER")) << g;
        // PLAIN with SASL-IR
        std::string r = c.cmd("p1", "AUTHENTICATE PLAIN " + b64(std::string("\0alice\0secret", 13)));
        EXPECT_TRUE(contains(r, "p1 OK [CAPABILITY")) << r;
    }
    {
        Raw c(s.port);
        c.line();
        // PLAIN through a continuation, an authorization identity of another user refused
        c.send("p2 AUTHENTICATE PLAIN\r\n");
        EXPECT_EQ(c.line(), "+ ");
        c.send(b64(std::string("bob\0alice\0secret", 16)) + "\r\n");
        EXPECT_TRUE(contains(c.until("p2"), "p2 NO [AUTHORIZATIONFAILED]"));
        // cancelled
        c.send("p3 AUTHENTICATE PLAIN\r\n");
        c.line();
        c.send("*\r\n");
        EXPECT_TRUE(contains(c.until("p3"), "p3 BAD"));
        // LOGIN mechanism
        c.send("p4 AUTHENTICATE LOGIN\r\n");
        EXPECT_EQ(c.line(), "+ VXNlcm5hbWU6");
        c.send(b64("alice") + "\r\n");
        EXPECT_EQ(c.line(), "+ UGFzc3dvcmQ6");
        c.send(b64("secret") + "\r\n");
        EXPECT_TRUE(contains(c.until("p4"), "p4 OK"));
    }
    {
        Raw c(s.port);
        c.line();
        std::string r = c.cmd("x1", "AUTHENTICATE XOAUTH2 " + b64("user=alice\x01" "auth=Bearer tok\x01\x01"));
        EXPECT_TRUE(contains(r, "x1 OK")) << r;
    }
    {
        Raw c(s.port);
        c.line();
        // OAUTHBEARER refused: the error as a challenge, the empty answer, NO
        c.send("o1 AUTHENTICATE OAUTHBEARER " + b64("n,a=alice,\x01host=x\x01port=1\x01" "auth=Bearer bad\x01\x01") + "\r\n");
        std::string l = c.line();
        EXPECT_EQ(l.rfind("+ eyJ", 0), 0u) << l;
        c.send("\r\n");
        EXPECT_TRUE(contains(c.until("o1"), "o1 NO [AUTHENTICATIONFAILED]"));
        std::string r = c.cmd("o2", "AUTHENTICATE OAUTHBEARER " + b64("n,a=alice,\x01" "auth=Bearer tok\x01\x01"));
        EXPECT_TRUE(contains(r, "o2 OK")) << r;
    }
    {
        Raw c(s.port);
        c.line();
        EXPECT_TRUE(contains(c.cmd("u1", "AUTHENTICATE CRAM-MD5"), "u1 NO"));
        EXPECT_TRUE(contains(c.cmd("u2", "AUTHENTICATE PLAIN !!!"), "u2 BAD"));
    }
}

TEST(ImapServer, StartTlsDropsWhatWasInjected) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) { srv.tls = server_tls(); });
    Raw c(s.port);
    std::string g = c.line();
    EXPECT_TRUE(contains(g, "STARTTLS LOGINDISABLED")) << g;
    EXPECT_FALSE(contains(g, "AUTH=PLAIN")) << g;
    EXPECT_TRUE(contains(c.cmd("n1", "LOGIN alice secret"), "n1 NO [PRIVACYREQUIRED]"));
    // a command sent in the clear behind STARTTLS, in the same packet
    c.send("s1 STARTTLS\r\ns2 LOGIN alice secret\r\n");
    std::string r = c.line();
    EXPECT_TRUE(contains(r, "s1 OK")) << r;
    auto t = net::tls::client(c.c, client_tls());
    ASSERT_TRUE(t) << text(t.error().message());
    Raw secure(*t);
    // the injected s2 is not answered: the first answer is to our own
    r = secure.cmd("s3", "CAPABILITY");
    EXPECT_FALSE(contains(r, "s2")) << r;
    EXPECT_FALSE(contains(r, "STARTTLS")) << r;
    EXPECT_TRUE(contains(r, "AUTH=PLAIN")) << r;
    EXPECT_TRUE(contains(secure.cmd("s4", "LOGIN alice secret"), "s4 OK"));
    EXPECT_TRUE(contains(secure.cmd("s5", "STARTTLS"), "s5 BAD"));
}

TEST(ImapServer, ImplicitTls) {
    auto mail = standard_mail();
    Server s(mail, true);
    auto tcp = net::tcp::connect(sgcl::string(s.address())).value();
    auto t = net::tls::client(tcp, client_tls());
    ASSERT_TRUE(t) << text(t.error().message());
    Raw c(*t);
    std::string g = c.line();
    EXPECT_TRUE(contains(g, "* OK")) << g;
    EXPECT_FALSE(contains(g, "STARTTLS"));
    EXPECT_TRUE(contains(c.cmd("a", "LOGIN alice secret"), "a OK"));
}

TEST(ImapServer, ModifiedUtf7ForClientsWithoutUtf8) {
    auto mail = standard_mail();
    Server s(mail);
    Raw c(s.port);
    c.login();
    EXPECT_TRUE(contains(c.cmd("c1", "CREATE ~peter/mail/&U,BTFw-/&ZeVnLIqe-"), "c1 OK"));
    std::string r = c.cmd("l1", "LIST \"\" \"~peter/*\"");
    EXPECT_TRUE(contains(r, "\"/\" ~peter/mail/&U,BTFw-/&ZeVnLIqe-")) << r;
    EXPECT_TRUE(contains(c.cmd("c2", "CREATE &AGE-"), "c2 NO"));   // not RFC 3501's form
    EXPECT_TRUE(contains(c.cmd("e1", "ENABLE UTF8=ACCEPT"), "* ENABLED UTF8=ACCEPT"));
    r = c.cmd("l2", "LIST \"\" \"~peter/*\"");
    EXPECT_TRUE(contains(r, "\"~peter/mail/\xe5\x8f\xb0\xe5\x8c\x97/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\"")) << r;
    // the backend holds the name in UTF-8
    auto boxes = mail.mailboxes("alice");
    bool found = false;
    for (const auto& b : *boxes) {
        found |= text(b.name) == "~peter/mail/\xe5\x8f\xb0\xe5\x8c\x97/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
    }
    EXPECT_TRUE(found);
}

TEST(ImapServer, TwoSessionsIdleIsTold) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("one"));
    mail.append("alice", "INBOX", message("two"));
    Server s(mail);
    Raw a(s.port), b(s.port);
    a.login();
    b.login();
    a.cmd("a1", "SELECT INBOX");
    b.cmd("b1", "SELECT INBOX");
    a.send("a2 IDLE\r\n");
    EXPECT_EQ(a.line(), "+ idling");
    // b appends: a hears EXISTS at once
    const std::string m = msg_text("three");
    b.send("b2 APPEND INBOX {" + std::to_string(m.size()) + "+}\r\n" + m + "\r\n");
    EXPECT_TRUE(contains(b.until("b2"), "b2 OK"));
    EXPECT_EQ(a.line(), "* 3 EXISTS");
    EXPECT_EQ(a.line(), "* 0 RECENT");   // an IMAP4rev1 client is told RECENT too
    // b changes flags: a hears FETCH
    EXPECT_TRUE(contains(b.cmd("b3", "STORE 1 +FLAGS (\\Flagged)"), "b3 OK"));
    std::string l = a.line();
    EXPECT_TRUE(contains(l, "* 1 FETCH (UID 1 FLAGS (\\Flagged))")) << l;
    // b expunges: a hears EXPUNGE
    b.cmd("b4", "STORE 2 +FLAGS.SILENT (\\Deleted)");
    a.line();   // the FETCH of the flag
    EXPECT_TRUE(contains(b.cmd("b5", "EXPUNGE"), "* 2 EXPUNGE"));
    EXPECT_EQ(a.line(), "* 2 EXPUNGE");
    a.send("DONE\r\n");
    EXPECT_TRUE(contains(a.until("a2"), "a2 OK"));
    std::string r = a.cmd("a3", "FETCH 1:* UID");
    EXPECT_TRUE(contains(r, "* 1 FETCH (UID 1)")) << r;
    EXPECT_TRUE(contains(r, "* 2 FETCH (UID 3)")) << r;
}

TEST(ImapServer, ExpungeWhileTheOtherFetches) {
    auto mail = standard_mail();
    for (int i = 1; i <= 3; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i)));
    }
    Server s(mail);
    Raw a(s.port), b(s.port);
    a.login();
    b.login();
    a.cmd("a1", "SELECT INBOX");
    b.cmd("b1", "SELECT INBOX");
    b.cmd("b2", "STORE 2 +FLAGS.SILENT (\\Deleted)");
    b.cmd("b3", "EXPUNGE");
    // a still numbers three messages; FETCH may not announce the expunge
    std::string r = a.cmd("a2", "FETCH 1:3 (UID RFC822.SIZE)");
    EXPECT_TRUE(contains(r, "* 1 FETCH (UID 1")) << r;
    EXPECT_TRUE(contains(r, "* 3 FETCH (UID 3")) << r;
    EXPECT_FALSE(contains(r, "* 2 EXPUNGE")) << r;
    EXPECT_TRUE(contains(r, "a2 NO [EXPUNGEISSUE]")) << r;
    // a UID command may
    r = a.cmd("a3", "UID FETCH 1:* FLAGS");
    EXPECT_TRUE(contains(r, "* 2 EXPUNGE")) << r;
    r = a.cmd("a4", "FETCH 2 UID");
    EXPECT_TRUE(contains(r, "* 2 FETCH (UID 3)")) << r;
}

TEST(ImapServer, CondstoreAndQresync) {
    auto mail = standard_mail();
    for (int i = 1; i <= 4; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i)));
    }
    Server s(mail);
    Raw c(s.port);
    c.login();
    std::string r = c.cmd("e", "ENABLE QRESYNC");
    EXPECT_TRUE(contains(r, "* ENABLED QRESYNC")) << r;
    r = c.cmd("s", "SELECT INBOX");
    EXPECT_TRUE(contains(r, "* OK [HIGHESTMODSEQ 5]")) << r;
    std::string validity = r.substr(r.find("[UIDVALIDITY ") + 13);
    validity = validity.substr(0, validity.find(']'));
    r = c.cmd("f1", "FETCH 1 (MODSEQ)");
    EXPECT_TRUE(contains(r, "* 1 FETCH (MODSEQ (2))")) << r;
    // conditional STORE: message 1 unchanged since 2, message 2 changed since
    c.cmd("x", "STORE 2 +FLAGS.SILENT (\\Answered)");
    r = c.cmd("st", "STORE 1:2 (UNCHANGEDSINCE 3) +FLAGS.SILENT (\\Seen)");
    EXPECT_TRUE(contains(r, "st OK [MODIFIED 2]")) << r;
    EXPECT_TRUE(contains(r, "* 1 FETCH (UID 1 MODSEQ (")) << r;
    r = c.cmd("q", "SEARCH MODSEQ 6");
    EXPECT_TRUE(contains(r, "MODSEQ")) << r;
    // expunges come as VANISHED
    c.cmd("d", "STORE 3 +FLAGS.SILENT (\\Deleted)");
    r = c.cmd("x2", "EXPUNGE");
    EXPECT_TRUE(contains(r, "* VANISHED 3")) << r;
    EXPECT_TRUE(contains(r, "x2 OK [HIGHESTMODSEQ ")) << r;
    r = c.cmd("v", "UID FETCH 1:* (FLAGS) (CHANGEDSINCE 5 VANISHED)");
    EXPECT_TRUE(contains(r, "* VANISHED (EARLIER) 3")) << r;
    EXPECT_FALSE(contains(r, "UID 4")) << r;   // unchanged since 5
    // a new session resyncs: the UIDs gone and the messages changed
    Raw c2(s.port);
    c2.login();
    c2.cmd("e", "ENABLE QRESYNC");
    r = c2.cmd("s2", "SELECT INBOX (QRESYNC (" + validity + " 5 1:4))");
    EXPECT_TRUE(contains(r, "* VANISHED (EARLIER) 3")) << r;
    EXPECT_TRUE(contains(r, "FETCH (UID 1 FLAGS (\\Seen) MODSEQ")) << r;
    EXPECT_TRUE(contains(r, "FETCH (UID 2 FLAGS (\\Answered) MODSEQ")) << r;
    EXPECT_TRUE(contains(c2.cmd("s3", "SELECT INBOX (QRESYNC)"), "s3 BAD"));
}

TEST(ImapServer, EsearchAndSearchres) {
    auto mail = standard_mail();
    for (int i = 1; i <= 5; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i)), i % 2 ? sgcl::vector<sgcl::string>{} : sgcl::vector<sgcl::string>{sgcl::string("\\Seen")});
    }
    Server s(mail);
    Raw c(s.port);
    c.login();
    c.cmd("s", "SELECT INBOX");
    std::string r = c.cmd("e1", "SEARCH RETURN (MIN MAX COUNT ALL) UNSEEN");
    EXPECT_TRUE(contains(r, "* ESEARCH (TAG \"e1\") MIN 1 MAX 5 ALL 1,3,5 COUNT 3")) << r;
    r = c.cmd("e2", "UID SEARCH RETURN () SEEN");
    EXPECT_TRUE(contains(r, "* ESEARCH (TAG \"e2\") UID ALL 2,4")) << r;
    r = c.cmd("e3", "SEARCH RETURN (COUNT) DELETED");
    EXPECT_TRUE(contains(r, "COUNT 0")) << r;
    r = c.cmd("e4", "SEARCH UNSEEN");   // IMAP4rev1's form
    EXPECT_TRUE(contains(r, "* SEARCH 1 3 5")) << r;
    r = c.cmd("r1", "SEARCH RETURN (SAVE) SEEN");
    EXPECT_FALSE(contains(r, "ESEARCH")) << r;
    r = c.cmd("r2", "FETCH $ (UID)");
    EXPECT_TRUE(contains(r, "* 2 FETCH (UID 2)")) << r;
    EXPECT_TRUE(contains(r, "* 4 FETCH (UID 4)")) << r;
    r = c.cmd("r3", "SEARCH $ UID 4");
    EXPECT_TRUE(contains(r, "* SEARCH 4")) << r;
    r = c.cmd("c1", "SEARCH CHARSET KOI8-R SUBJECT x");
    EXPECT_TRUE(contains(r, "c1 OK")) << r;
    r = c.cmd("c2", "SEARCH CHARSET X-UNKNOWN SUBJECT x");
    EXPECT_TRUE(contains(r, "c2 NO [BADCHARSET (US-ASCII UTF-8)]")) << r;
}

TEST(ImapServer, ListExtendedSpecialUseStatus) {
    auto mail = standard_mail();
    Server s(mail);
    Raw c(s.port);
    c.login();
    EXPECT_TRUE(contains(c.cmd("c1", "CREATE Sent (USE (\\Sent))"), "c1 OK"));
    EXPECT_TRUE(contains(c.cmd("c2", "CREATE Work/Projects/"), "c2 OK"));
    EXPECT_TRUE(contains(c.cmd("c3", "CREATE Bad (USE (\\Bogus))"), "c3 NO [USEATTR]"));
    EXPECT_TRUE(contains(c.cmd("c4", "CREATE Sent"), "c4 NO [ALREADYEXISTS]"));
    EXPECT_TRUE(contains(c.cmd("c5", "CREATE inbox"), "c5 NO [ALREADYEXISTS]"));
    c.cmd("u", "SUBSCRIBE Work/Projects");
    std::string r = c.cmd("l1", "LIST \"\" *");
    EXPECT_TRUE(contains(r, "* LIST (\\HasNoChildren) \"/\" INBOX")) << r;
    EXPECT_TRUE(contains(r, "* LIST (\\HasNoChildren \\Sent) \"/\" Sent")) << r;
    EXPECT_TRUE(contains(r, "* LIST (\\Noselect \\HasChildren) \"/\" Work")) << r;
    r = c.cmd("l2", "LIST \"\" %");
    EXPECT_FALSE(contains(r, "Work/Projects")) << r;
    r = c.cmd("l3", "LIST (SPECIAL-USE) \"\" *");
    EXPECT_TRUE(contains(r, "Sent")) << r;
    EXPECT_FALSE(contains(r, "INBOX")) << r;
    r = c.cmd("l4", "LIST (SUBSCRIBED RECURSIVEMATCH) \"\" %");
    EXPECT_TRUE(contains(r, "\"/\" Work (\"CHILDINFO\" (\"SUBSCRIBED\"))")) << r;
    r = c.cmd("l5", "LIST \"\" (INBOX Sent) RETURN (STATUS (MESSAGES UIDNEXT))");
    EXPECT_TRUE(contains(r, "* STATUS INBOX (MESSAGES 0 UIDNEXT 1)")) << r;
    EXPECT_TRUE(contains(r, "* STATUS Sent (MESSAGES 0 UIDNEXT 1)")) << r;
    r = c.cmd("l6", "LSUB \"\" *");
    EXPECT_TRUE(contains(r, "* LSUB () \"/\" Work/Projects")) << r;
    r = c.cmd("l7", "LIST \"\" \"\"");
    EXPECT_TRUE(contains(r, "* LIST (\\Noselect) \"/\" \"\"")) << r;
    r = c.cmd("l8", "LIST Work/ %");
    EXPECT_TRUE(contains(r, "Work/Projects")) << r;
    EXPECT_TRUE(contains(c.cmd("n", "NAMESPACE"), "* NAMESPACE ((\"\" \"/\")) NIL NIL"));
    r = c.cmd("i", "ID (\"name\" \"test\")");
    EXPECT_TRUE(contains(r, "* ID (\"name\" \"sgcl\")")) << r;
    // DELETE and RENAME
    EXPECT_TRUE(contains(c.cmd("d1", "DELETE INBOX"), "d1 NO"));
    EXPECT_TRUE(contains(c.cmd("r1", "RENAME Work Play"), "r1 OK"));
    r = c.cmd("l9", "LIST \"\" Play/*");
    EXPECT_TRUE(contains(r, "Play/Projects")) << r;
    EXPECT_TRUE(contains(c.cmd("d2", "DELETE Play/Projects"), "d2 OK"));
    EXPECT_TRUE(contains(c.cmd("d3", "DELETE Play/Projects"), "d3 NO [NONEXISTENT]"));
}

TEST(ImapServer, AppendCopyMove) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("a"));
    mail.append("alice", "INBOX", message("b"));
    Server s(mail);
    Raw c(s.port);
    c.login();
    EXPECT_TRUE(contains(c.cmd("a1", "APPEND Nowhere {1+}\r\nx"), "a1 NO [TRYCREATE]"));
    EXPECT_TRUE(contains(c.cmd("a2", "APPEND INBOX {0+}\r\n"), "a2 NO"));
    EXPECT_TRUE(contains(c.cmd("a3", "APPEND INBOX CATENATE (TEXT {1+}\r\nx)"), "a3 NO"));
    c.cmd("c", "CREATE Archive");
    c.cmd("s", "SELECT INBOX");
    std::string r = c.cmd("c1", "COPY 1:2 Archive");
    EXPECT_TRUE(contains(r, "c1 OK [COPYUID ")) << r;
    EXPECT_TRUE(contains(r, " 1:2 1:2]")) << r;
    EXPECT_TRUE(contains(c.cmd("c2", "COPY 1 Nowhere"), "c2 NO [TRYCREATE]"));
    r = c.cmd("m1", "UID MOVE 1 Archive");
    EXPECT_TRUE(contains(r, "* OK [COPYUID ")) << r;
    EXPECT_TRUE(contains(r, "* 1 EXPUNGE")) << r;
    EXPECT_TRUE(contains(r, "m1 OK")) << r;
    r = c.cmd("st", "STATUS Archive (MESSAGES UIDNEXT UNSEEN SIZE DELETED)");
    EXPECT_TRUE(contains(r, "MESSAGES 3")) << r;
    EXPECT_TRUE(contains(r, "UIDNEXT 4")) << r;
    // UID EXPUNGE of a set only
    c.cmd("f", "STORE 1 +FLAGS.SILENT (\\Deleted)");
    r = c.cmd("x", "UID EXPUNGE 1");   // uid 1 is gone already: nothing
    EXPECT_FALSE(contains(r, "EXPUNGE\r")) << r;
    r = c.cmd("x2", "UID EXPUNGE 2");
    EXPECT_TRUE(contains(r, "* 1 EXPUNGE")) << r;
    // CLOSE expunges, UNSELECT does not
    c.cmd("s2", "SELECT Archive");
    c.cmd("f2", "STORE 1 +FLAGS.SILENT (\\Deleted)");
    c.cmd("u", "UNSELECT");
    EXPECT_TRUE(contains(c.cmd("st2", "STATUS Archive (MESSAGES)"), "MESSAGES 3"));
    c.cmd("s3", "SELECT Archive");
    c.cmd("cl", "CLOSE");
    EXPECT_TRUE(contains(c.cmd("st3", "STATUS Archive (MESSAGES)"), "MESSAGES 2"));
    // EXAMINE: read only
    c.cmd("ex", "EXAMINE Archive");
    EXPECT_TRUE(contains(c.cmd("w", "STORE 1 +FLAGS (\\Seen)"), "w NO [READ-ONLY]"));
    EXPECT_TRUE(contains(c.cmd("w2", "EXPUNGE"), "w2 NO"));
    EXPECT_FALSE(contains(c.cmd("w3", "FETCH 1 BODY[]"), "\\Seen"));   // read only: nothing marked
}

TEST(ImapServer, BinaryAndPartial) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX",
                "Subject: b\r\nContent-Type: multipart/mixed; boundary=z\r\n\r\n--z\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n"
                "caf=C3=A9\r\n--z\r\nContent-Type: application/octet-stream\r\nContent-Transfer-Encoding: base64\r\n\r\nAAECAw==\r\n--z--\r\n");
    Server s(mail);
    Raw c(s.port);
    c.login();
    c.cmd("s", "SELECT INBOX");
    std::string r = c.cmd("b1", "FETCH 1 (BINARY.PEEK[1] BINARY.SIZE[2])");
    EXPECT_TRUE(contains(r, "BINARY[1] {5}")) << r;
    EXPECT_TRUE(contains(r, "café")) << r;
    EXPECT_TRUE(contains(r, "BINARY.SIZE[2] 4")) << r;
    r = c.cmd("b2", "FETCH 1 BINARY.PEEK[2]");
    EXPECT_TRUE(contains(r, "BINARY[2] ~{4}")) << r;   // NUL inside: literal8
    r = c.cmd("p1", "FETCH 1 BODY.PEEK[]<0.10>");
    EXPECT_TRUE(contains(r, "BODY[]<0> {10}")) << r;
    EXPECT_TRUE(contains(r, "Subject: b")) << r;
    r = c.cmd("p2", "FETCH 1 BODY.PEEK[1]<100.10>");
    EXPECT_TRUE(contains(r, "BODY[1]<100> {0}")) << r;
    r = c.cmd("p3", "FETCH 1 (RFC822.SIZE BODY.PEEK[TEXT] RFC822.HEADER)");
    EXPECT_TRUE(contains(r, "RFC822.HEADER {")) << r;
    r = c.cmd("f", "FETCH 1 FLAGS");
    EXPECT_TRUE(contains(r, "FLAGS ()")) << r;   // nothing above marked it seen
    r = c.cmd("p4", "FETCH 1 RFC822.TEXT");
    EXPECT_TRUE(contains(r, "\\Seen")) << r;
}

TEST(ImapServer, SortAndThread) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX", "Message-ID: <a@x>\r\nSubject: zebra\r\nDate: Mon, 5 Oct 2026 10:00:00 +0000\r\nFrom: c@x\r\n\r\nccc\r\n");
    mail.append("alice", "INBOX", "Message-ID: <b@x>\r\nSubject: Re: zebra\r\nIn-Reply-To: <a@x>\r\nDate: Mon, 5 Oct 2026 09:00:00 +0000\r\nFrom: a@x\r\n\r\na\r\n");
    mail.append("alice", "INBOX", "Message-ID: <c@x>\r\nSubject: apple\r\nDate: Mon, 5 Oct 2026 11:00:00 +0000\r\nFrom: b@x\r\n\r\nbbbbbbbb\r\n");
    Server s(mail);
    Raw c(s.port);
    c.login();
    c.cmd("s", "SELECT INBOX");
    EXPECT_TRUE(contains(c.cmd("o1", "SORT (SUBJECT) UTF-8 ALL"), "* SORT 3 1 2"));
    EXPECT_TRUE(contains(c.cmd("o2", "SORT (DATE) UTF-8 ALL"), "* SORT 2 1 3"));
    EXPECT_TRUE(contains(c.cmd("o3", "SORT (REVERSE SIZE) UTF-8 ALL"), "* SORT 2 3 1"));
    EXPECT_TRUE(contains(c.cmd("o4", "SORT (FROM) UTF-8 ALL"), "* SORT 2 3 1"));
    EXPECT_TRUE(contains(c.cmd("o5", "UID SORT (ARRIVAL) UTF-8 SUBJECT zebra"), "* SORT 1 2"));
    EXPECT_TRUE(contains(c.cmd("t1", "THREAD REFERENCES UTF-8 ALL"), "* THREAD (1 2)(3)"));
    EXPECT_TRUE(contains(c.cmd("t2", "THREAD ORDEREDSUBJECT UTF-8 ALL"), "* THREAD (2 1)(3)"));   // one reply: a chain
}

TEST(ImapServer, Quota) {
    auto mail = standard_mail();
    mail.set_quota("alice", 1, 2);
    Server s(mail);
    Raw c(s.port);
    c.login();
    std::string r = c.cmd("q1", "GETQUOTAROOT INBOX");
    EXPECT_TRUE(contains(r, "* QUOTAROOT INBOX \"\"")) << r;
    EXPECT_TRUE(contains(r, "* QUOTA \"\" (STORAGE 0 1 MESSAGE 0 2)")) << r;
    EXPECT_TRUE(contains(c.cmd("a1", "APPEND INBOX {3+}\r\nA\r\n"), "a1 OK"));
    EXPECT_TRUE(contains(c.cmd("a2", "APPEND INBOX {3+}\r\nB\r\n"), "a2 OK"));
    EXPECT_TRUE(contains(c.cmd("a3", "APPEND INBOX {3+}\r\nC\r\n"), "a3 NO [OVERQUOTA]"));
    EXPECT_TRUE(contains(c.cmd("q2", "GETQUOTA \"\""), "MESSAGE 2 2"));
    EXPECT_TRUE(contains(c.cmd("q3", "SETQUOTA \"\" (STORAGE 9)"), "q3 NO"));
}

TEST(ImapServer, CompressDeflate) {
    auto mail = standard_mail();
    for (int i = 0; i < 20; ++i) {
        mail.append("alice", "INBOX", message("compressible " + std::to_string(i), std::string(2000, 'z') + "\r\n"));
    }
    Server s(mail);
    Raw c(s.port);
    c.login();
    EXPECT_TRUE(contains(c.cmd("c0", "CAPABILITY"), "COMPRESS=DEFLATE"));
    c.send("c1 COMPRESS DEFLATE\r\n");
    std::string r = c.line();
    EXPECT_TRUE(contains(r, "c1 OK")) << r;
    // from here both ways compressed
    Raw z(net::imap::detail::deflate_connection(c.c, c.buf));
    c.buf.clear();
    r = z.cmd("z1", "SELECT INBOX");
    EXPECT_TRUE(contains(r, "* 20 EXISTS")) << r;
    r = z.cmd("z2", "FETCH 1:* BODY.PEEK[]");
    EXPECT_TRUE(contains(r, "z2 OK")) << r;
    EXPECT_TRUE(contains(z.cmd("z3", "COMPRESS DEFLATE"), "z3 NO [COMPRESSIONACTIVE]"));
}

TEST(ImapServer, DeletedUnderAnotherSession) {
    auto mail = standard_mail();
    Server s(mail);
    Raw a(s.port), b(s.port);
    a.login();
    b.login();
    a.cmd("c", "CREATE Doomed");
    b.cmd("s", "SELECT Doomed");
    EXPECT_TRUE(contains(a.cmd("d", "DELETE Doomed"), "d OK"));
    std::string r = b.cmd("n", "NOOP");
    EXPECT_TRUE(contains(r, "* BYE [NONEXISTENT]")) << r;
}

TEST(ImapServer, GracefulShutdown) {
    auto mail = standard_mail();
    Server s(mail);
    Raw a(s.port);
    a.login();
    a.cmd("s", "SELECT INBOX");
    std::thread t([&s] { s.srv.shutdown(); });
    std::string l = a.line();
    EXPECT_TRUE(contains(l, "* BYE [UNAVAILABLE]")) << l;
    t.join();
    EXPECT_EQ(s.srv.connections(), 0u);
}

TEST(ImapServer, IdleWakesOnADeliveryOfTheProgram) {
    auto mail = standard_mail();
    Server s(mail, false, [](net::imap::server& srv) { srv.poll_interval = duration::zero(); });
    Raw a(s.port);
    a.login();
    a.cmd("s", "SELECT INBOX");
    a.send("i IDLE\r\n");
    EXPECT_EQ(a.line(), "+ idling");
    // the program delivers straight into the backend (an SMTP handler's way)
    mail.append("alice", "INBOX", message("delivered"));
    EXPECT_EQ(a.line(), "* 1 EXISTS");
    a.send("DONE\r\n");
    EXPECT_TRUE(contains(a.until("i"), "i OK"));
}
