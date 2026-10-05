//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::email: the examples of RFC 5322 Appendix A, RFC 2046 §5.1.1,
// RFC 2047 §8 and RFC 2231, a message built and read back, the transfer
// encodings each content gets, the folding, the boundary against the
// content, the address grammar with its obsolete forms, the limits, and the
// boundaries of every public call.
#include "common.h"

#include <filesystem>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string s(const sgcl::string& t) {
        return std::string(t.view());
    }

    std::string s(const sgcl::vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    email parse(std::string_view text) {
        auto m = email::parse(sgcl::string(text));
        EXPECT_TRUE(m) << (m ? "" : s(m.error().message()));
        return m ? *m : email();
    }

    sgcl::vector<byte> vbytes(std::string_view t) {
        sgcl::vector<byte> v;
        for (char c : t) {
            v.push_back(byte(uint8_t(c)));
        }
        return v;
    }

    std::string addrs(const sgcl::vector<email::address>& list) {
        std::string out;
        for (auto& a : list) {
            if (!out.empty()) {
                out += " | ";
            }
            out += s(a.name()) + " <" + s(a.addr()) + ">";
        }
        return out;
    }

    // every line of a written message ends with CRLF, none longer than 998
    // and none of the head longer than 78 unless one word makes it so
    void well_formed(const std::string& t) {
        size_t start = 0;
        bool head = true;
        while (start < t.size()) {
            size_t nl = t.find('\n', start);
            if (nl == std::string::npos) {   // the last line of a body without a line break
                ASSERT_FALSE(head);
                ASSERT_LE(t.size() - start, 998u);
                return;
            }
            ASSERT_EQ(t[nl - 1], '\r') << t.substr(start, nl - start);
            std::string_view line(t.data() + start, nl - 1 - start);
            ASSERT_LE(line.size(), 998u);
            if (head) {
                if (line.empty()) {
                    head = false;
                } else if (line.size() > 78) {
                    ASSERT_EQ(line.find(' ', line.find(':') + 2), std::string_view::npos) << line;
                }
            }
            ASSERT_EQ(line.find('\r'), std::string_view::npos);
            start = nl + 1;
        }
    }
}

// ---- RFC 5322 Appendix A

TEST(Email_Tests, Rfc5322A11SimpleAddressing) {
    auto m = parse(
        "From: John Doe <jdoe@machine.example>\r\n"
        "To: Mary Smith <mary@example.net>\r\n"
        "Subject: Saying Hello\r\n"
        "Date: Fri, 21 Nov 1997 09:55:06 -0600\r\n"
        "Message-ID: <1234@local.machine.example>\r\n"
        "\r\n"
        "This is a message just to say hello.\r\n"
        "So, \"Hello\".\r\n");
    EXPECT_EQ(s(m.from()->name()), "John Doe");
    EXPECT_EQ(s(m.from()->addr()), "jdoe@machine.example");
    EXPECT_EQ(addrs(m.to()), "Mary Smith <mary@example.net>");
    EXPECT_EQ(s(m.subject()), "Saying Hello");
    ASSERT_TRUE(m.date());
    EXPECT_EQ(m.date()->unix(), 880127706);
    EXPECT_EQ(s(m.message_id()), "<1234@local.machine.example>");
    EXPECT_EQ(s(m.text()), "This is a message just to say hello.\nSo, \"Hello\".\n");
    EXPECT_EQ(s(m.html()), "");
    EXPECT_TRUE(m.attachments().empty());
}

TEST(Email_Tests, Rfc5322A12DifferentTypesOfMailboxes) {
    auto m = parse(
        "From: \"Joe Q. Public\" <john.q.public@example.com>\r\n"
        "To: Mary Smith <mary@x.test>, jdoe@example.org, Who? <one@y.test>\r\n"
        "Cc: <boss@nil.test>, \"Giant; \\\"Big\\\" Box\" <sysservices@example.net>\r\n"
        "Date: Tue, 1 Jul 2003 10:52:37 +0200\r\n"
        "Message-ID: <5678.21-Nov-1997@example.com>\r\n"
        "\r\n"
        "Hi everyone.\r\n");
    EXPECT_EQ(s(m.from()->name()), "Joe Q. Public");
    EXPECT_EQ(addrs(m.to()), "Mary Smith <mary@x.test> |  <jdoe@example.org> | Who? <one@y.test>");
    EXPECT_EQ(addrs(m.cc()), " <boss@nil.test> | Giant; \"Big\" Box <sysservices@example.net>");
    EXPECT_EQ(m.date()->unix(), 1057049557);
}

TEST(Email_Tests, Rfc5322A13GroupAddresses) {
    auto m = parse(
        "From: Pete <pete@silly.example>\r\n"
        "To: A Group:Ed Jones <c@a.test>,joe@where.test,John <jdoe@one.test>;\r\n"
        "Cc: Undisclosed recipients:;\r\n"
        "Date: Thu, 13 Feb 1969 23:32:54 -0330\r\n"
        "\r\n"
        "Testing.\r\n");
    EXPECT_EQ(addrs(m.to()), "Ed Jones <c@a.test> |  <joe@where.test> | John <jdoe@one.test>");
    EXPECT_TRUE(m.cc().empty());
    EXPECT_EQ(m.date()->unix(), -27723426);
}

TEST(Email_Tests, Rfc5322A5WhiteSpaceCommentsAndOtherOddities) {
    auto m = parse(
        "From: Pete(A nice \\) chap) <pete(his account)@silly.test(his host)>\r\n"
        "To:A Group(Some people)\r\n"
        "     :Chris Jones <c@(Chris's host.)public.example>,\r\n"
        "         joe@example.org,\r\n"
        "  John <jdoe@one.test> (my dear friend); (the end of the group)\r\n"
        "Cc:(Empty list)(start)Hidden recipients  :(nobody(that I know))  ;\r\n"
        "Date: Thu,\r\n"
        "      13\r\n"
        "        Feb\r\n"
        "          1969\r\n"
        "      23:32\r\n"
        "               -0330 (Newfoundland Time)\r\n"
        "Message-ID:              <testabcd.1234@silly.test>\r\n"
        "\r\n"
        "Testing.\r\n");
    EXPECT_EQ(s(m.from()->name()), "Pete");
    EXPECT_EQ(s(m.from()->addr()), "pete@silly.test");
    EXPECT_EQ(addrs(m.to()), "Chris Jones <c@public.example> |  <joe@example.org> | John <jdoe@one.test>");
    EXPECT_TRUE(m.cc().empty());
    ASSERT_TRUE(m.date());
    EXPECT_EQ(m.date()->unix(), -27723480);
    EXPECT_EQ(s(m.message_id()), "<testabcd.1234@silly.test>");
}

TEST(Email_Tests, Rfc5322A61ObsoleteAddressing) {
    auto m = parse(
        "From: Joe Q. Public <john.q.public@example.com>\r\n"
        "To: Mary Smith <@node.test:mary@example.net>, , jdoe@test  . example\r\n"
        "Date: Tue, 1 Jul 2003 10:52:37 +0200\r\n"
        "Message-ID: <5678.21-Nov-1997@example.com>\r\n"
        "\r\n"
        "Hi everyone.\r\n");
    EXPECT_EQ(s(m.from()->name()), "Joe Q. Public");
    EXPECT_EQ(addrs(m.to()), "Mary Smith <mary@example.net> |  <jdoe@test.example>");
}

TEST(Email_Tests, Rfc5322A62ObsoleteDates) {
    auto m = parse(
        "From: John Doe <jdoe@machine.example>\r\n"
        "To: Mary Smith <mary@example.net>\r\n"
        "Subject: Saying Hello\r\n"
        "Date: 21 Nov 97 09:55:06 GMT\r\n"
        "Message-ID: <1234@local.machine.example>\r\n"
        "\r\n"
        "This is a message just to say hello.\r\n");
    ASSERT_TRUE(m.date());
    EXPECT_EQ(m.date()->unix(), 880106106);
}

TEST(Email_Tests, Rfc5322A63ObsoleteWhiteSpaceAndComments) {
    auto m = parse(
        "From  : John Doe <jdoe@machine(comment).  example>\r\n"
        "To    : Mary Smith\r\n"
        "  \r\n"
        "          <mary@example.net>\r\n"
        "Subject     : Saying Hello\r\n"
        "Date  : Fri, 21 Nov 1997 09(comment):   55  :  06 -0600\r\n"
        "Message-ID  : <1234   @   local(blah)  .machine .example>\r\n"
        "\r\n"
        "This is a message just to say hello.\r\n");
    EXPECT_EQ(s(m.from()->addr()), "jdoe@machine.example");
    EXPECT_EQ(addrs(m.to()), "Mary Smith <mary@example.net>");
    EXPECT_EQ(s(m.subject()), "Saying Hello");
    ASSERT_TRUE(m.date());
    EXPECT_EQ(m.date()->unix(), 880127706);
}

TEST(Email_Tests, Rfc5322A4TraceFieldsKeepTheirOrder) {
    auto m = parse(
        "Received: from x.y.test\r\n"
        "   by example.net\r\n"
        "   via TCP\r\n"
        "   with ESMTP\r\n"
        "   id ABC12345\r\n"
        "   for <mary@example.net>;  21 Nov 1997 10:05:43 -0600\r\n"
        "Received: from node.example by x.y.test; 21 Nov 1997 10:01:22 -0600\r\n"
        "From: John Doe <jdoe@node.example>\r\n"
        "To: Mary Smith <mary@example.net>\r\n"
        "\r\n"
        "This is a message just to say hello.\r\n");
    auto r = m.header_all("received");
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(s(r[0]), "from x.y.test   by example.net   via TCP   with ESMTP   id ABC12345   for <mary@example.net>;  21 Nov 1997 10:05:43 -0600");
    EXPECT_EQ(s(r[1]), "from node.example by x.y.test; 21 Nov 1997 10:01:22 -0600");
    // the trace written back as it came
    auto out = s(m.to_string());
    EXPECT_NE(out.find("Received: from x.y.test\r\n   by example.net\r\n"), std::string::npos) << out;
}

// ---- RFC 2046 §5.1.1, RFC 2047 §8, RFC 2231

TEST(Email_Tests, Rfc2046MultipartExample) {
    auto m = parse(
        "From: Nathaniel Borenstein <nsb@bellcore.com>\r\n"
        "To: Ned Freed <ned@innosoft.com>\r\n"
        "Date: Sun, 21 Mar 1993 23:56:48 -0800 (PST)\r\n"
        "Subject: Sample message\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-type: multipart/mixed; boundary=\"simple boundary\"\r\n"
        "\r\n"
        "This is the preamble.  It is to be ignored, though it\r\n"
        "is a handy place for composition agents to include an\r\n"
        "explanatory note to non-MIME conformant readers.\r\n"
        "\r\n"
        "--simple boundary\r\n"
        "\r\n"
        "This is implicitly typed plain US-ASCII text.\r\n"
        "It does NOT end with a linebreak.\r\n"
        "--simple boundary\r\n"
        "Content-type: text/plain; charset=us-ascii\r\n"
        "\r\n"
        "This is explicitly typed plain US-ASCII text.\r\n"
        "It DOES end with a linebreak.\r\n"
        "\r\n"
        "--simple boundary--\r\n"
        "\r\n"
        "This is the epilogue.  It is also to be ignored.\r\n");
    auto body = m.body();
    EXPECT_TRUE(body.is_multipart());
    EXPECT_EQ(s(body.content_type()), "multipart/mixed");
    EXPECT_EQ(s(body.param("boundary")), "simple boundary");
    auto parts = body.parts();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(s(parts[0].content_type()), "text/plain");
    EXPECT_EQ(s(parts[0].content()), "This is implicitly typed plain US-ASCII text.\r\nIt does NOT end with a linebreak.");
    EXPECT_EQ(s(parts[1].charset()), "us-ascii");
    EXPECT_EQ(s(parts[1].content()), "This is explicitly typed plain US-ASCII text.\r\nIt DOES end with a linebreak.\r\n");
    EXPECT_EQ(s(m.text()), "This is implicitly typed plain US-ASCII text.\nIt does NOT end with a linebreak.");
    EXPECT_EQ(m.date()->unix(), 732787008);
}

TEST(Email_Tests, Rfc2047Examples) {
    struct Case {
        const char* in;
        const char* out;
    } cases[] = {
        {"(=?ISO-8859-1?Q?a?=)", "(a)"},
        {"(=?ISO-8859-1?Q?a?= b)", "(a b)"},
        {"(=?ISO-8859-1?Q?a?= =?ISO-8859-1?Q?b?=)", "(ab)"},
        {"(=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=)", "(ab)"},
        {"(=?ISO-8859-1?Q?a?=\r\n    =?ISO-8859-1?Q?b?=)", "(ab)"},
        {"(=?ISO-8859-1?Q?a_b?=)", "(a b)"},
        {"(=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=)", "(a b)"},
        {"=?US-ASCII?Q?Keith_Moore?=", "Keith Moore"},
        {"=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?=", "Keld Jørn Simonsen"},
        {"=?ISO-8859-1?Q?Andr=E9?= Pirard", "André Pirard"},
        {"=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=\r\n =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=",
         "If you can read this you understand the example."},
        {"=?US-ASCII*EN?Q?Keith_Moore?=", "Keith Moore"},
        {"=?utf-8?b?xbs=?= =?utf-8?b?w7M=?=", "Żó"},
        {"=?utf-8?q?=C5?= =?utf-8?q?=BB?=", "Ż"},   // a character split between two words
        {"=?x-unknown?q?abc?= tail", "=?x-unknown?q?abc?= tail"},
        {"=?utf-8?x?abc?=", "=?utf-8?x?abc?="},
        {"=?utf-8?q?unterminated", "=?utf-8?q?unterminated"},
        {"plain text", "plain text"},
    };
    for (auto& c : cases) {
        auto m = parse(std::string("Subject: ") + c.in + "\r\n\r\n");
        EXPECT_EQ(s(m.subject()), c.out) << c.in;
    }
    auto m = parse("From: =?US-ASCII?Q?Keith_Moore?= <moore@cs.utk.edu>\r\n"
                   "To: =?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>\r\n"
                   "CC: =?ISO-8859-1?Q?Andr=E9?= Pirard <PIRARD@vm1.ulg.ac.be>\r\n\r\n");
    EXPECT_EQ(s(m.from()->name()), "Keith Moore");
    EXPECT_EQ(addrs(m.to()), "Keld Jørn Simonsen <keld@dkuug.dk>");
    EXPECT_EQ(addrs(m.cc()), "André Pirard <PIRARD@vm1.ulg.ac.be>");
}

TEST(Email_Tests, Rfc2231Parameters) {
    auto m = parse(
        "Content-Type: multipart/mixed; boundary=b\r\n\r\n"
        "--b\r\n"
        "Content-Type: message/external-body; access-type=URL;\r\n"
        " URL*0=\"ftp://\";\r\n"
        " URL*1=\"cs.utk.edu/pub/moore/bulk-mailer/bulk-mailer.tar\"\r\n\r\n"
        "--b\r\n"
        "Content-Type: application/x-stuff;\r\n"
        " title*=us-ascii'en-us'This%20is%20%2A%2A%2Afun%2A%2A%2A\r\n\r\n"
        "--b\r\n"
        "Content-Type: application/x-stuff;\r\n"
        " title*0*=us-ascii'en'This%20is%20even%20more%20;\r\n"
        " title*1*=%2A%2A%2Afun%2A%2A%2A%20;\r\n"
        " title*2=\"isn't it!\"\r\n\r\n"
        "--b\r\n"
        "Content-Type: application/pdf\r\n"
        "Content-Disposition: attachment; filename*=iso-8859-2''%BF%F3%B3w.pdf\r\n\r\n"
        "--b\r\n"
        "Content-Type: application/pdf; name=\"=?utf-8?q?=C5=BC.pdf?=\"\r\n\r\n"
        "--b--\r\n");
    auto parts = m.body().parts();
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_EQ(s(parts[0].param("url")), "ftp://cs.utk.edu/pub/moore/bulk-mailer/bulk-mailer.tar");
    EXPECT_EQ(s(parts[0].param("URL")), "ftp://cs.utk.edu/pub/moore/bulk-mailer/bulk-mailer.tar");
    EXPECT_EQ(s(parts[0].param("access-type")), "URL");
    EXPECT_EQ(s(parts[1].param("title")), "This is ***fun***");
    EXPECT_EQ(s(parts[2].param("title")), "This is even more ***fun*** isn't it!");
    EXPECT_EQ(s(parts[3].filename()), std::string("żó") + "\xC5\x82" + "w.pdf");
    EXPECT_EQ(s(parts[4].filename()), "ż.pdf");
}

// ---- building

TEST(Email_Tests, TheOneLineFormAndItsHead) {
    email m("Alice <alice@example.com>", "bob@example.org, \"Doe, John\" <john@example.net>", "Hi", "Hello, Bob.\n");
    EXPECT_EQ(s(m.from()->name()), "Alice");
    EXPECT_EQ(addrs(m.to()), " <bob@example.org> | Doe, John <john@example.net>");
    EXPECT_EQ(s(m.subject()), "Hi");
    EXPECT_EQ(s(m.text()), "Hello, Bob.\n");
    ASSERT_TRUE(m.date());
    EXPECT_LE(std::abs(m.date()->unix() - time::now().unix()), 5);
    auto id = s(m.message_id());
    EXPECT_EQ(id.front(), '<');
    EXPECT_NE(id.find("@example.com>"), std::string::npos) << id;   // the domain of From
    auto out = s(m.to_string());
    well_formed(out);
    EXPECT_NE(out.find("From: Alice <alice@example.com>\r\n"), std::string::npos);
    EXPECT_NE(out.find("To: bob@example.org, \"Doe, John\" <john@example.net>\r\n"), std::string::npos);
    EXPECT_NE(out.find("MIME-Version: 1.0\r\n"), std::string::npos);
    EXPECT_NE(out.find("Content-Type: text/plain; charset=utf-8\r\n\r\nHello, Bob.\r\n"), std::string::npos) << out;
    EXPECT_EQ(out.find("Content-Transfer-Encoding"), std::string::npos);   // 7bit, the default, not written
    EXPECT_THROW(email("not an address", "b@x", "s", "t"), sgcl::invalid_argument);
    EXPECT_THROW(email("a@x", "b@x,,,<", "s", "t"), sgcl::invalid_argument);
}

TEST(Email_Tests, MessageIdFollowsFromUntilSet) {
    email m;
    EXPECT_NE(s(m.message_id()).find("@localhost>"), std::string::npos);
    m.set_from("Ann <ann@example.pl>");
    EXPECT_NE(s(m.message_id()).find("@example.pl>"), std::string::npos);
    m.set_message_id("fixed@id");
    EXPECT_EQ(s(m.message_id()), "<fixed@id>");
    m.set_from("x@other.test");
    EXPECT_EQ(s(m.message_id()), "<fixed@id>");
    m.set_message_id("<brackets@kept>");
    EXPECT_EQ(s(m.message_id()), "<brackets@kept>");
    email n;
    n.set_header("Message-ID", "<mine@x>");
    n.set_from("a@example.com");
    EXPECT_EQ(s(n.message_id()), "<mine@x>");
    EXPECT_NE(s(email().message_id()), s(email().message_id()));
}

TEST(Email_Tests, NonAsciiHeadIsEncodedAndReadBack) {
    email m("Łucja Żółć <lucja@example.pl>", "Zoë <zoe@example.fr>", "Zażółć gęślą jaźń — and some ASCII words after it that go on and on past the line", "x");
    m.add_cc("\"Nowak, Jan\" <jan@example.pl>");
    m.set_header("X-Note", "café\r\ninjected: header");
    auto out = s(m.to_string());
    well_formed(out);
    for (unsigned char c : out) {
        ASSERT_LT(c, 0x80) << "a byte past ASCII in a 7bit message";
    }
    EXPECT_EQ(out.find("\r\ninjected:"), std::string::npos);   // a value never ends its line
    auto back = parse(out);
    EXPECT_EQ(s(back.subject()), "Zażółć gęślą jaźń — and some ASCII words after it that go on and on past the line");
    EXPECT_EQ(s(back.from()->name()), "Łucja Żółć");
    EXPECT_EQ(addrs(back.to()), "Zoë <zoe@example.fr>");
    EXPECT_EQ(addrs(back.cc()), "Nowak, Jan <jan@example.pl>");
    EXPECT_EQ(s(back.header("x-note")), "café  injected: header");
}

TEST(Email_Tests, Utf8HeadWhenAllowed) {
    email m("Łucja <łucja@example.pl>", "zoë@exämple.fr", "Zażółć", "treść");
    email::write_options o;
    o.allow_utf8 = true;
    o.allow_8bit = true;
    auto out = s(m.to_string(o));
    EXPECT_NE(out.find("From: Łucja <łucja@example.pl>\r\n"), std::string::npos) << out;
    EXPECT_NE(out.find("Subject: Zażółć\r\n"), std::string::npos);
    EXPECT_NE(out.find("Content-Transfer-Encoding: 8bit\r\n\r\ntreść"), std::string::npos);
    auto back = parse(out);
    EXPECT_EQ(s(back.from()->addr()), "łucja@example.pl");
    EXPECT_EQ(s(back.subject()), "Zażółć");
    EXPECT_EQ(s(back.text()), "treść");
    // without RFC 6532 the domain goes by IDNA
    auto ascii = s(m.to_string());
    EXPECT_NE(ascii.find("To: zoë@xn--exmple-cua.fr\r\n"), std::string::npos) << ascii;
}

TEST(Email_Tests, TextAndHtmlMakeAnAlternative) {
    email m("a@example.com", "b@example.com", "s", "plain");
    m.set_html("<p>rich</p>");
    EXPECT_EQ(s(m.body().content_type()), "multipart/alternative");
    auto parts = m.body().parts();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(s(parts[0].content_type()), "text/plain");
    EXPECT_EQ(s(parts[1].content_type()), "text/html");
    m.set_text("plain 2");
    EXPECT_EQ(s(m.text()), "plain 2");
    EXPECT_EQ(s(m.html()), "<p>rich</p>");
    auto back = parse(s(m.to_string()));
    EXPECT_EQ(s(back.text()), "plain 2");
    EXPECT_EQ(s(back.html()), "<p>rich</p>");
    email h;
    h.set_html("<b>only</b>");
    EXPECT_EQ(s(h.body().content_type()), "text/html");
    EXPECT_EQ(s(h.text()), "");
}

TEST(Email_Tests, AttachmentsAndInlineParts) {
    email m("a@example.com", "b@example.com", "files", "see");
    m.set_html("<img src=\"cid:x\">");
    auto bytes = input(100000);
    sgcl::vector<byte> data(bytes.begin(), bytes.end());
    m.attach("dane ż.bin", data);
    m.attach("note.txt", vbytes("hello\n"));
    auto cid = m.embed("logo.png", sgcl::vector<byte>(10, byte(0x89)));
    EXPECT_NE(s(cid).find('@'), std::string::npos);
    auto top = m.body();
    EXPECT_EQ(s(top.content_type()), "multipart/mixed");
    auto parts = top.parts();
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(s(parts[0].content_type()), "multipart/alternative");
    auto alt = parts[0].parts();
    ASSERT_EQ(alt.size(), 2u);
    EXPECT_EQ(s(alt[1].content_type()), "multipart/related");
    EXPECT_EQ(s(alt[1].param("type")), "text/html");
    auto rel = alt[1].parts();
    ASSERT_EQ(rel.size(), 2u);
    EXPECT_EQ(s(rel[1].content_id()), s(cid));
    EXPECT_EQ(s(rel[1].disposition()), "inline");
    auto out = s(m.to_string());
    well_formed(out);
    EXPECT_NE(out.find("filename*=utf-8''dane%20%C5%BC.bin"), std::string::npos);
    EXPECT_NE(out.find("Content-Type: application/octet-stream\r\nContent-Disposition: attachment;"), std::string::npos) << out.substr(0, 3000);
    auto back = parse(out);
    auto atts = back.attachments();
    ASSERT_EQ(atts.size(), 2u);
    EXPECT_EQ(s(atts[0].filename()), "dane ż.bin");
    EXPECT_EQ(atts[0].content().size(), data.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), atts[0].content().begin()));
    EXPECT_EQ(s(atts[1].filename()), "note.txt");
    EXPECT_EQ(s(atts[1].text()), "hello\n");
    EXPECT_TRUE(atts[0].is_attachment());
    EXPECT_EQ(s(back.html()), "<img src=\"cid:x\">");
    EXPECT_EQ(s(back.text()), "see");
}

TEST(Email_Tests, AttachFromAFile) {
    auto dir = std::filesystem::temp_directory_path() / "sgcl_email_attach";
    std::filesystem::create_directories(dir);
    auto path = (dir / "report.pdf").string();
    ASSERT_TRUE(io::write_file(sgcl::string(path), sgcl::string("%PDF-1.7 ...")));
    email m("a@example.com", "b@example.com", "s", "t");
    ASSERT_TRUE(m.attach(sgcl::string(path)));
    auto a = m.attachments();
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(s(a[0].filename()), "report.pdf");
    EXPECT_EQ(s(a[0].content_type()), "application/pdf");
    auto missing = m.attach(sgcl::string((dir / "none.pdf").string()));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    EXPECT_EQ(m.attachments().size(), 1u);   // unchanged
    auto png = (dir / "logo.png").string();
    ASSERT_TRUE(io::write_file(sgcl::string(png), sgcl::string("\x89PNG")));
    auto cid = m.embed(sgcl::string(png));
    ASSERT_TRUE(cid);
    EXPECT_FALSE(m.embed(sgcl::string((dir / "none.png").string())));
    std::filesystem::remove_all(dir);
}

TEST(Email_Tests, TransferEncodingsByContent) {
    auto cte = [](const email& m) {
        auto out = s(m.to_string());
        auto at = out.find("Content-Transfer-Encoding: ");
        return at == std::string::npos ? std::string("none") : out.substr(at + 27, out.find('\r', at) - at - 27);
    };
    EXPECT_EQ(cte(email("a@x.test", "b@x.test", "s", "ascii only")), "none");
    EXPECT_EQ(cte(email("a@x.test", "b@x.test", "s", "mostly ascii with one ą")), "quoted-printable");
    EXPECT_EQ(cte(email("a@x.test", "b@x.test", "s", "ąęśćżźółń ąęśćżźółń")), "base64");
    EXPECT_EQ(cte(email("a@x.test", "b@x.test", "s", sgcl::string(std::string(2000, 'x')))), "quoted-printable");   // a line past 998
    email bin;
    bin.set_body(email::part("application/octet-stream", sgcl::vector<byte>(5000, byte(0))));
    EXPECT_EQ(cte(bin), "base64");
    email nul;
    nul.set_body(email::part("text/plain", sgcl::string(std::string("plain text with one \0 byte in it", 33))));
    EXPECT_EQ(cte(nul), "quoted-printable");
}

TEST(Email_Tests, BoundaryIsCheckedAgainstTheContent) {
    // a text that holds the boundary a parsed message names
    auto m = parse(
        "Content-Type: multipart/mixed; boundary=\"XYZ\"\r\n\r\n"
        "--XYZ\r\nContent-Type: text/plain\r\n\r\nfirst\r\n--XYZ--\r\n");
    auto parts = m.body().parts();
    ASSERT_EQ(parts.size(), 1u);
    m.set_body(email::part("multipart/mixed; boundary=XYZ"));
    auto body = m.body();
    body.add(email::part("text/plain", sgcl::string("line\r\n--XYZ\r\nnot a delimiter")));
    body.add(email::part("text/plain", sgcl::string("second")));
    auto out = s(m.to_string());
    EXPECT_EQ(out.find("boundary=XYZ"), std::string::npos) << out;
    auto back = parse(out);
    auto p = back.body().parts();
    ASSERT_EQ(p.size(), 2u);
    EXPECT_EQ(s(p[0].text()), "line\n--XYZ\nnot a delimiter");
}

TEST(Email_Tests, ParsedMessageWritesBack) {
    std::string src =
        "From: a@example.com\r\n"
        "To: b@example.com\r\n"
        "Subject: =?utf-8?q?caf=C3=A9?=\r\n"
        "X-Custom: kept\r\n"
        "  folded\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=\"outer\"\r\n\r\n"
        "--outer\r\n"
        "Content-Type: text/plain; charset=iso-8859-2\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
        "Za=BF=F3=B3=E6\r\n"
        "--outer\r\n"
        "Content-Type: message/rfc822\r\n\r\n"
        "From: inner@example.com\r\nSubject: inner\r\n\r\nInner body\r\n"
        "--outer\r\n"
        "Content-Type: image/gif\r\n"
        "Content-Transfer-Encoding: base64\r\n"
        "Content-Disposition: attachment; filename=a.gif\r\n\r\n"
        "R0lGODlh\r\n"
        "--outer--\r\n";
    auto m = parse(src);
    EXPECT_EQ(s(m.text()), "Zażółć");   // the CRLF before a delimiter is the delimiter's
    auto atts = m.attachments();
    ASSERT_EQ(atts.size(), 2u);
    ASSERT_TRUE(atts[0].message());
    EXPECT_EQ(s(atts[0].message()->subject()), "inner");
    EXPECT_EQ(s(atts[0].message()->text()), "Inner body");
    EXPECT_EQ(s(atts[1].content()), "GIF89a");
    auto out = s(m.to_string());
    well_formed(out);
    EXPECT_NE(out.find("X-Custom: kept\r\n  folded\r\n"), std::string::npos);
    EXPECT_NE(out.find("boundary=\"outer\""), std::string::npos);
    EXPECT_NE(out.find("Za=BF=F3=B3=E6"), std::string::npos);   // its charset and encoding kept
    auto again = parse(out);
    EXPECT_EQ(s(again.text()), "Zażółć");
    EXPECT_EQ(s(again.attachments()[0].message()->subject()), "inner");
    EXPECT_EQ(s(again.attachments()[1].content()), "GIF89a");
    EXPECT_EQ(s(again.to_string()), out);   // a fixed point
}

TEST(Email_Tests, LenientInput) {
    // LF only, no closing delimiter, a line that is not a field ending the head
    auto m = parse("Subject: lf\nContent-Type: multipart/alternative; boundary=b\n\n--b\nContent-Type: text/plain\n\none\n--b\nContent-Type: text/html\n\n<p>two</p>\n");
    EXPECT_EQ(s(m.subject()), "lf");
    EXPECT_EQ(s(m.text()), "one");
    EXPECT_EQ(s(m.html()), "<p>two</p>\n");
    auto n = parse("Subject: x\r\nthis is not a field\r\nmore body\r\n");
    EXPECT_EQ(s(n.text()), "this is not a field\nmore body\n");
    auto e = parse("");
    EXPECT_EQ(s(e.text()), "");
    EXPECT_TRUE(e.headers().empty());
    auto mbox = parse("From someone@example.com Mon Oct  5 12:00:00 2026\nSubject: mbox\n\nbody\n");
    EXPECT_EQ(s(mbox.subject()), "mbox");
    auto badb64 = parse("Content-Transfer-Encoding: base64\r\n\r\nSGVs bG8*h\r\n");
    EXPECT_EQ(s(badb64.body().content()), "Hello!");
    auto noboundary = parse("Content-Type: multipart/mixed\r\n\r\njust text\r\n");
    EXPECT_EQ(s(noboundary.body().content()), "just text\r\n");
    auto badtype = parse("Content-Type: garbage\r\n\r\ntext\r\n");
    EXPECT_EQ(s(badtype.body().content_type()), "text/plain");
    auto digest = parse("Content-Type: multipart/digest; boundary=d\r\n\r\n--d\r\n\r\nSubject: in digest\r\n\r\nx\r\n--d--\r\n");
    ASSERT_EQ(digest.body().parts().size(), 1u);
    EXPECT_EQ(s(digest.body().parts()[0].content_type()), "message/rfc822");
    ASSERT_TRUE(digest.body().parts()[0].message());
    EXPECT_EQ(s(digest.body().parts()[0].message()->subject()), "in digest");
}

TEST(Email_Tests, Limits) {
    email::limits l;
    l.max_header_bytes = 64;
    auto big = email::parse(sgcl::string("Subject: " + std::string(100, 'x') + "\r\n\r\nbody"), l);
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), errc::limit_exceeded);
    EXPECT_TRUE(email::parse(sgcl::string("Subject: short\r\n\r\n" + std::string(1000, 'b')), l));

    std::string many = "Content-Type: multipart/mixed; boundary=b\r\n\r\n";
    for (int i = 0; i < 20; ++i) {
        many += "--b\r\n\r\npart\r\n";
    }
    many += "--b--\r\n";
    email::limits few;
    few.max_parts = 10;
    auto r = email::parse(sgcl::string(many), few);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::limit_exceeded);
    EXPECT_TRUE(email::parse(sgcl::string(many)));

    std::string deep;
    for (int i = 0; i < 40; ++i) {
        deep += "Content-Type: multipart/mixed; boundary=b" + std::to_string(i) + "\r\n\r\n--b" + std::to_string(i) + "\r\n";
    }
    deep += "\r\nleaf\r\n";
    auto d = email::parse(sgcl::string(deep));
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code(), errc::depth_limit);
    email::limits deeper;
    deeper.max_depth = 64;
    EXPECT_TRUE(email::parse(sgcl::string(deep), deeper));
    std::string nested;
    for (int i = 0; i < 40; ++i) {
        nested += "Content-Type: message/rfc822\r\n\r\n";
    }
    auto n = email::parse(sgcl::string(nested));
    ASSERT_FALSE(n);
    EXPECT_EQ(n.error().code(), errc::depth_limit);
}

// ---- addresses

TEST(EmailAddress_Tests, ParseAndText) {
    email::address a("Alice Doe <alice@example.com>");
    EXPECT_EQ(s(a.name()), "Alice Doe");
    EXPECT_EQ(s(a.addr()), "alice@example.com");
    EXPECT_EQ(s(a.local_part()), "alice");
    EXPECT_EQ(s(a.domain()), "example.com");
    EXPECT_EQ(s(a.to_string()), "Alice Doe <alice@example.com>");
    EXPECT_EQ(s(email::address("Doe, Alice", "a@x.test").to_string()), "\"Doe, Alice\" <a@x.test>");
    EXPECT_EQ(s(email::address("say \"hi\"", "a@x.test").to_string()), "\"say \\\"hi\\\"\" <a@x.test>");
    EXPECT_EQ(s(email::address("Łucja", "l@x.test").to_string()), "=?utf-8?b?xYF1Y2ph?= <l@x.test>");
    EXPECT_EQ(s(email::address("", "a@x.test").to_string()), "a@x.test");
    EXPECT_EQ(s(email::address("\"quoted local\"@x.test").addr()), "\"quoted local\"@x.test");
    EXPECT_EQ(s(email::address("\"plain\"@x.test").addr()), "plain@x.test");
    EXPECT_EQ(s(email::address("a@[127.0.0.1]").domain()), "[127.0.0.1]");
    EXPECT_EQ(s(email::address("jdoe@example.org (John Doe)").name()), "John Doe");
    EXPECT_EQ(email::address("A <a@x>"), email::address("A", "a@x"));
    EXPECT_FALSE(email::address("A <a@x>") == email::address("B", "a@x"));
    email::address empty;
    EXPECT_EQ(s(empty.name()), "");
    EXPECT_EQ(s(empty.addr()), "");
    EXPECT_EQ(s(empty.domain()), "");
    EXPECT_EQ(s(empty.local_part()), "");
}

TEST(EmailAddress_Tests, Refusals) {
    for (const char* bad : {"", "no at sign", "<a@x", "a@", "@x", "A <a@x> trailing", "a@x, b@y", "\"open@x", "a@x (open"}) {
        auto r = email::address::parse(bad);
        EXPECT_FALSE(r) << bad;
        if (!r) {
            EXPECT_EQ(r.error().code(), errc::syntax);
        }
        EXPECT_THROW(email::address{sgcl::string(bad)}, sgcl::invalid_argument) << bad;
    }
    auto r = email::address::parse("a@x, b@y");
    EXPECT_EQ(s(r.error().message()), "offset 0: more than one address");
    EXPECT_EQ(s(email::address::parse("  ").error().message()), "offset 2: no address");
    EXPECT_EQ(s(email::address::parse("Ann <ann@x").error().message()), "offset 10: invalid address");
    auto at = email::address::parse("Ann <ann@x");
    EXPECT_GT(at.error().offset(), 0u);
}

TEST(EmailAddress_Tests, Lists) {
    auto l = email::address::parse_list("a@x.test, B <b@x.test>,, Team: c@x.test, \"D\" <d@x.test>; Empty:;");
    ASSERT_TRUE(l);
    EXPECT_EQ(addrs(*l), " <a@x.test> | B <b@x.test> |  <c@x.test> | D <d@x.test>");
    auto e = email::address::parse_list("");
    ASSERT_TRUE(e);
    EXPECT_TRUE(e->empty());
    EXPECT_FALSE(email::address::parse_list("Team: a@x.test"));   // a group not closed
    EXPECT_FALSE(email::address::parse_list("a@x.test b@y.test"));
}

// ---- parts and the head

TEST(EmailPart_Tests, BuildAShapeOfOnesOwn) {
    email m("a@x.test", "b@x.test", "invite", "unused");
    email::part alt("multipart/alternative");
    alt.add(email::part("text/plain", "You are invited."));
    alt.add(email::part("text/calendar; method=REQUEST", "BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n"));
    m.set_body(alt);
    EXPECT_EQ(s(m.body().content_type()), "multipart/alternative");
    auto out = s(m.to_string());
    EXPECT_NE(out.find("Content-Type: text/calendar; method=REQUEST; charset=utf-8"), std::string::npos) << out;
    auto back = parse(out);
    EXPECT_EQ(s(back.text()), "You are invited.");
    auto p = back.body().parts();
    ASSERT_EQ(p.size(), 2u);
    EXPECT_EQ(s(p[1].param("method")), "REQUEST");
    EXPECT_EQ(s(p[1].text()), "BEGIN:VCALENDAR\nEND:VCALENDAR\n");
    EXPECT_THROW(email::part("nonsense", "x"), sgcl::invalid_argument);
    email::part file("application/pdf", sgcl::vector<byte>(3, byte('x')));
    file.set_filename("ćma.pdf");
    EXPECT_EQ(s(file.disposition()), "attachment");
    EXPECT_EQ(s(file.filename()), "ćma.pdf");
    EXPECT_TRUE(file.is_attachment());
    file.set_content_id("<id@x>");
    EXPECT_EQ(s(file.content_id()), "id@x");
    EXPECT_EQ(s(file.disposition()), "inline");
    EXPECT_EQ(s(file.filename()), "ćma.pdf");
    EXPECT_FALSE(file.is_attachment());
    EXPECT_FALSE(file.is_multipart());
    EXPECT_FALSE(file.message());
    EXPECT_TRUE(alt.is_multipart());
    EXPECT_EQ(alt.parts().size(), 2u);
    EXPECT_TRUE(alt.content().empty());
}

TEST(EmailPart_Tests, HeadsOfParts) {
    email::part p("text/plain", "x");
    p.set_header("X-A", "1").add_header("X-A", "2").set_header("Content-Description", "żółw");
    EXPECT_TRUE(p.has_header("x-a"));
    EXPECT_EQ(s(p.header("X-A")), "1");
    EXPECT_EQ(p.header_all("x-a").size(), 2u);
    EXPECT_EQ(p.headers().size(), 4u);
    p.remove_header("x-a");
    EXPECT_FALSE(p.has_header("X-A"));
    EXPECT_EQ(s(p.header("missing")), "");
    EXPECT_THROW(p.set_header("bad name", "v"), sgcl::invalid_argument);
    EXPECT_THROW(p.add_header("", "v"), sgcl::invalid_argument);
    email m;
    m.set_body(p);
    auto back = parse(s(m.to_string()));
    EXPECT_EQ(s(back.body().header("content-description")), "żółw");
}

TEST(Email_Tests, HeadCalls) {
    email m;
    m.set_header("X-One", "a").add_header("X-One", "b").add_header("Received", "r1");
    EXPECT_EQ(s(m.header("x-one")), "a");
    auto all = m.header_all("X-ONE");
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(s(all[1]), "b");
    m.set_header("X-One", "c");
    EXPECT_EQ(m.header_all("x-one").size(), 1u);
    EXPECT_TRUE(m.has_header("received"));
    m.remove_header("received");
    EXPECT_FALSE(m.has_header("Received"));
    EXPECT_THROW(m.set_header("Bad:Name", "x"), sgcl::invalid_argument);
    EXPECT_THROW(m.add_header("bad name", "x"), sgcl::invalid_argument);
    auto hs = m.headers();
    ASSERT_GE(hs.size(), 3u);
    EXPECT_EQ(s(hs[0].first), "Date");
    m.set_subject("");
    EXPECT_EQ(s(m.subject()), "");
    m.remove_header("date");
    EXPECT_FALSE(m.date());
    m.set_header("Date", "not a date");
    EXPECT_FALSE(m.date());
    auto t = time::datetime::from_unix(1791203400, time::zone::utc());
    m.set_date(t);
    EXPECT_EQ(s(m.header("date")), "Mon, 05 Oct 2026 12:30:00 +0000");
    EXPECT_EQ(m.date()->unix(), 1791203400);
    m.add_bcc("hidden@x.test").add_reply_to("Reply <r@x.test>");
    EXPECT_EQ(addrs(m.bcc()), " <hidden@x.test>");
    EXPECT_EQ(addrs(m.reply_to()), "Reply <r@x.test>");
    EXPECT_NE(s(m.to_string()).find("Bcc: hidden@x.test"), std::string::npos);
    email::write_options o;
    o.write_bcc = false;
    EXPECT_EQ(s(m.to_string(o)).find("Bcc:"), std::string::npos);
    m.add_to(email::address("X", "x@x.test")).add_cc(email::address("", "c@x.test"));
    m.add_bcc(email::address("", "h2@x.test")).add_reply_to(email::address("", "r2@x.test"));
    EXPECT_EQ(m.bcc().size(), 2u);
    EXPECT_FALSE(email().from());
    EXPECT_TRUE(email().to().empty());
    email copy = m;
    EXPECT_TRUE(copy == m);
    EXPECT_FALSE(copy == email());
}

TEST(Email_Tests, FoldingKeepsLinesShort) {
    email m("a@x.test", "b@x.test", "s", "t");
    std::string list;
    for (int i = 0; i < 30; ++i) {
        list += (i ? ", " : "") + std::string("Person Number ") + std::to_string(i) + " <person" + std::to_string(i) + "@example.com>";
    }
    m.add_cc(sgcl::string(list));
    m.set_subject(sgcl::string(std::string(50, 'w') + " " + std::string(50, 'v') + " ąę " + std::string(200, 'z')));
    auto out = s(m.to_string());
    well_formed(out);
    auto back = parse(out);
    EXPECT_EQ(back.cc().size(), 30u);
    EXPECT_EQ(s(back.subject()), std::string(50, 'w') + " " + std::string(50, 'v') + " ąę " + std::string(200, 'z'));
}

TEST(Email_Tests, ReadersFilesAndStreams) {
    email m("a@x.test", "b@x.test", "stream", "body");
    sgcl::tracked_ptr buf = sgcl::make_tracked<io::buffer>();
    ASSERT_TRUE(m.write_to(io::writer(buf)));
    auto back = email::parse(io::reader(buf));
    ASSERT_TRUE(back);
    EXPECT_EQ(s(back->subject()), "stream");
    auto bytes = m.to_string();
    auto b2 = email::parse(slice<const byte>(bytes));
    ASSERT_TRUE(b2);
    EXPECT_EQ(s(b2->text()), "body");
    auto dir = std::filesystem::temp_directory_path() / "sgcl_email_files";
    std::filesystem::create_directories(dir);
    auto path = sgcl::string((dir / "m.eml").string());
    ASSERT_TRUE(m.save(path));
    auto l = email::load(path);
    ASSERT_TRUE(l);
    EXPECT_EQ(s(l->text()), "body");
    auto none = email::load(sgcl::string((dir / "none.eml").string()));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), errc::io);
    ASSERT_TRUE(m.async_save(path).wait());
    auto al = email::async_load(path).wait();
    ASSERT_TRUE(al);
    EXPECT_EQ(s(al->subject()), "stream");
    email::write_options o;
    o.allow_8bit = true;
    ASSERT_TRUE(m.save(path, o));
    ASSERT_TRUE(m.async_save(path, o).wait());
    email::limits lim;
    ASSERT_TRUE(email::load(path, lim));
    ASSERT_TRUE(email::async_load(path, lim).wait());
    ASSERT_TRUE(email::parse(bytes, lim));
    ASSERT_TRUE(email::parse(slice<const byte>(bytes), lim));
    sgcl::tracked_ptr buf2 = sgcl::make_tracked<io::buffer>(bytes);
    ASSERT_TRUE(email::parse(io::reader(buf2), lim));
    sgcl::tracked_ptr sink = sgcl::make_tracked<io::buffer>();
    ASSERT_TRUE(m.write_to(io::writer(sink), o));
    // a literal and a std::string_view are read where they lie
    EXPECT_EQ(s(email::parse("Subject: lit\r\n\r\n")->subject()), "lit");
    EXPECT_EQ(s(email::parse(std::string_view("Subject: view\r\n\r\n"), lim)->subject()), "view");
    EXPECT_FALSE(email::parse("Subject: too long for the limit\r\n\r\n", email::limits{4, 32, 10}));
    std::filesystem::remove_all(dir);
}
