//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's view of a message (detail/mime.h): the parts by their
// octets, RFC 9051 §6.4.5's numbering of sections (its example message,
// nested multiparts and messages), HEADER, TEXT, MIME, HEADER.FIELDS and
// .NOT, the ENVELOPE and BODYSTRUCTURE written (and read back by the
// client's parser), addresses with groups, comments and quoted names, RFC
// 2231 parameters, the transfer encodings undone; malformed messages.
#include "tests/types.h"
#include "sgcl/net/imap.h"

#include <string>

using namespace sgcl;
namespace imap = sgcl::net::imap;
namespace d = sgcl::net::imap::detail;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // RFC 9051 §6.4.5's example: a multipart/mixed with a text, an
    // octet-stream, a message/rfc822 of a multipart, and a multipart with
    // an image and another message of a multipart/alternative
    const std::string Example =
        "From: a@example.com\r\n"
        "Subject: the example\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: MULTIPART/MIXED; boundary=\"outer\"\r\n"
        "\r\n"
        "preamble\r\n"
        "--outer\r\n"
        "Content-Type: TEXT/PLAIN\r\n"
        "\r\n"
        "part 1\r\n"
        "--outer\r\n"
        "Content-Type: APPLICATION/OCTET-STREAM\r\n"
        "Content-Transfer-Encoding: base64\r\n"
        "\r\n"
        "AAECAw==\r\n"
        "--outer\r\n"
        "Content-Type: MESSAGE/RFC822\r\n"
        "\r\n"
        "Subject: inner 3\r\n"
        "Content-Type: multipart/mixed; boundary=b3\r\n"
        "\r\n"
        "--b3\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "part 3.1\r\n"
        "--b3\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        "part 3.2\r\n"
        "--b3--\r\n"
        "--outer\r\n"
        "Content-Type: MULTIPART/MIXED; boundary=b4\r\n"
        "\r\n"
        "--b4\r\n"
        "Content-Type: IMAGE/GIF\r\n"
        "Content-ID: <gif@x>\r\n"
        "\r\n"
        "GIF89a\r\n"
        "--b4\r\n"
        "Content-Type: MESSAGE/RFC822\r\n"
        "\r\n"
        "Subject: inner 4.2\r\n"
        "Content-Type: multipart/mixed; boundary=b42\r\n"
        "\r\n"
        "--b42\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "part 4.2.1\r\n"
        "--b42\r\n"
        "Content-Type: multipart/alternative; boundary=b422\r\n"
        "\r\n"
        "--b422\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "part 4.2.2.1\r\n"
        "--b422\r\n"
        "Content-Type: text/richtext\r\n"
        "\r\n"
        "part 4.2.2.2\r\n"
        "--b422--\r\n"
        "--b42--\r\n"
        "--b4--\r\n"
        "--outer--\r\n"
        "epilogue\r\n";

    std::string sec_of(const std::string& msg, const char* spec) {
        d::Structure st(msg);
        d::Section sec;
        EXPECT_TRUE(d::parse_section(spec, sec)) << spec;
        std::string out;
        if (!d::section_bytes(msg, st.root, sec, out)) {
            return "<none>";
        }
        return out;
    }
}

TEST(ImapMime, Rfc9051SectionNumbers) {
    EXPECT_EQ(sec_of(Example, "1"), "part 1");
    EXPECT_EQ(sec_of(Example, "2"), "AAECAw==");
    EXPECT_EQ(sec_of(Example, "3.HEADER"), "Subject: inner 3\r\nContent-Type: multipart/mixed; boundary=b3\r\n\r\n");
    EXPECT_EQ(sec_of(Example, "3.1"), "part 3.1");
    EXPECT_EQ(sec_of(Example, "3.2"), "part 3.2");
    EXPECT_EQ(sec_of(Example, "4.1"), "GIF89a");
    EXPECT_EQ(sec_of(Example, "4.1.MIME"), "Content-Type: IMAGE/GIF\r\nContent-ID: <gif@x>\r\n\r\n");
    EXPECT_EQ(sec_of(Example, "4.2.HEADER"), "Subject: inner 4.2\r\nContent-Type: multipart/mixed; boundary=b42\r\n\r\n");
    EXPECT_EQ(sec_of(Example, "4.2.1"), "part 4.2.1");
    EXPECT_EQ(sec_of(Example, "4.2.2.1"), "part 4.2.2.1");
    EXPECT_EQ(sec_of(Example, "4.2.2.2"), "part 4.2.2.2");
    EXPECT_EQ(sec_of(Example, "HEADER.FIELDS (SUBJECT)"), "Subject: the example\r\n\r\n");
    EXPECT_EQ(sec_of(Example, "HEADER.FIELDS.NOT (SUBJECT MIME-VERSION CONTENT-TYPE)"), "From: a@example.com\r\n\r\n");
    EXPECT_EQ(sec_of(Example, ""), Example);
    // parts that are not there
    EXPECT_EQ(sec_of(Example, "5"), "<none>");
    EXPECT_EQ(sec_of(Example, "1.1"), "<none>");
    EXPECT_EQ(sec_of(Example, "1.HEADER"), "<none>");   // a text part has no header of a message
    d::Section bad;
    EXPECT_FALSE(d::parse_section("MIME", bad));          // MIME needs a part
    d::Section bad2;
    EXPECT_FALSE(d::parse_section("0", bad2));
    d::Section bad3;
    EXPECT_FALSE(d::parse_section("HEADER.FIELDS ()", bad3));
    d::Section bad4;
    EXPECT_FALSE(d::parse_section("TEXT.1", bad4));
}

TEST(ImapMime, SinglePartMessage) {
    const std::string m = "Subject: one\r\n\r\nbody line\r\n";
    EXPECT_EQ(sec_of(m, "1"), "body line\r\n");
    EXPECT_EQ(sec_of(m, "TEXT"), "body line\r\n");
    EXPECT_EQ(sec_of(m, "HEADER"), "Subject: one\r\n\r\n");
    EXPECT_EQ(sec_of(m, "1.MIME"), "Subject: one\r\n\r\n");
    EXPECT_EQ(sec_of(m, "2"), "<none>");
    // no header at all, no body at all
    EXPECT_EQ(sec_of(std::string("\r\nonly body"), "TEXT"), "only body");
    EXPECT_EQ(sec_of(std::string("Subject: x\r\n"), "TEXT"), "");
}

TEST(ImapMime, BodyStructureWrittenAndReadBack) {
    d::Structure st(Example);
    std::string bs;
    d::put_body(bs, Example, st.root, true, false);
    // through the client's parser
    std::string line = "* 1 FETCH (BODYSTRUCTURE " + bs + ")";
    d::Response r;
    ASSERT_TRUE(d::parse_response(line, r));
    d::Lexer x(std::string_view(line).substr(r.data_at));
    imap::message m;
    std::string scratch;
    ASSERT_TRUE(d::parse_fetch(x, m, scratch)) << bs;
    const auto& b = *m.body_structure;
    EXPECT_EQ(text(b.subtype), "mixed");
    ASSERT_EQ(b.parts.size(), 4u);
    EXPECT_EQ(text(b.parts[1].encoding), "base64");
    EXPECT_EQ(b.parts[1].size, 8u);
    EXPECT_EQ(text(b.parts[2].type), "message");
    ASSERT_TRUE(b.parts[2].envelope);
    EXPECT_EQ(text(b.parts[2].envelope->subject), "inner 3");
    ASSERT_EQ(b.parts[2].parts.size(), 1u);
    EXPECT_EQ(b.parts[2].parts[0].parts.size(), 2u);
    EXPECT_EQ(text(b.parts[3].parts[0].id), "<gif@x>");
    EXPECT_EQ(text(b.parts[3].parts[1].parts[0].parts[1].parts[1].subtype), "richtext");
    EXPECT_EQ(text(b.parts[3].parts[1].parts[0].parts[1].parts[1].part), "4.2.2.2");
    // the non-extensible BODY
    std::string body;
    d::put_body(body, Example, st.root, false, false);
    EXPECT_EQ(body.find("boundary"), std::string::npos);
}

TEST(ImapMime, TextDefaultsAndLines) {
    const std::string m = "Subject: x\r\n\r\na\r\nb\r\nc";
    d::Structure st(m);
    std::string bs;
    d::put_body(bs, m, st.root, true, false);
    EXPECT_EQ(bs, "(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"us-ascii\") NIL NIL \"7BIT\" 7 3 NIL NIL NIL NIL)");
}

TEST(ImapMime, EnvelopeFields) {
    const std::string m =
        "Date: Wed, 17 Jul 1996 02:23:25 -0700 (PDT)\r\n"
        "From: \"Terry Gray\" <gray@cac.washington.edu>\r\n"
        "To: imap@cac.washington.edu, (comment) \"Klensin, John\" <KLENSIN@MIT.EDU>\r\n"
        "Cc: team: a@x.org, b@y.org;, solo@z.org\r\n"
        "Subject: =?utf-8?q?a?=\r\n"
        "  folded\r\n"
        "Message-ID: <B27397-0100000@cac.washington.edu>\r\n"
        "\r\n";
    d::Structure st(m);
    std::string env;
    d::put_envelope(env, st.root, false);
    std::string line = "* 1 FETCH (ENVELOPE " + env + ")";
    d::Response r;
    ASSERT_TRUE(d::parse_response(line, r));
    d::Lexer x(std::string_view(line).substr(r.data_at));
    imap::message msg;
    std::string scratch;
    ASSERT_TRUE(d::parse_fetch(x, msg, scratch)) << env;
    const auto& e = *msg.envelope;
    EXPECT_EQ(text(e.date), "Wed, 17 Jul 1996 02:23:25 -0700 (PDT)");
    EXPECT_EQ(text(e.subject), "a  folded");
    ASSERT_EQ(e.from.size(), 1u);
    EXPECT_EQ(text(e.from[0].name), "Terry Gray");
    // sender and reply-to default to from
    ASSERT_EQ(e.sender.size(), 1u);
    ASSERT_EQ(e.reply_to.size(), 1u);
    ASSERT_EQ(e.to.size(), 2u);
    EXPECT_EQ(text(e.to[1].name), "Klensin, John");
    EXPECT_EQ(text(e.to[1].email()), "KLENSIN@MIT.EDU");
    ASSERT_EQ(e.cc.size(), 3u);   // the group flattened
    EXPECT_EQ(text(e.cc[2].email()), "solo@z.org");
    EXPECT_TRUE(e.in_reply_to.empty());
    EXPECT_EQ(text(e.message_id), "<B27397-0100000@cac.washington.edu>");
    // the envelope keeps the group markers on the wire
    EXPECT_NE(env.find("(NIL NIL \"team\" NIL)"), std::string::npos);
    EXPECT_NE(env.find("(NIL NIL NIL NIL)"), std::string::npos);
}

TEST(ImapMime, Rfc2231Parameters) {
    auto p = d::parse_params_field("attachment; filename*0*=utf-8''Za%C5%BC; filename*1*=%C3%B3%C5%82%C4%87.txt; size=10");
    EXPECT_EQ(p.value, "attachment");
    ASSERT_TRUE(p.find("filename"));
    EXPECT_EQ(*p.find("filename"), "Zażółć.txt");
    EXPECT_EQ(*p.find("size"), "10");
    auto t = d::parse_params_field("Text/HTML ; charset = \"UTF-8\" (comment); name=\"a;b.html\"");
    EXPECT_EQ(t.value, "text/html");
    EXPECT_EQ(*t.find("charset"), "UTF-8");
    EXPECT_EQ(*t.find("name"), "a;b.html");
}

TEST(ImapMime, TransferEncodingsUndone) {
    std::string out;
    ASSERT_TRUE(d::decoded_content("AAEC\r\nAw==\r\n", "base64", out));
    EXPECT_EQ(out, std::string("\0\1\2\3", 4));
    out.clear();
    ASSERT_TRUE(d::decoded_content("caf=C3=A9 =\r\nsoft", "quoted-printable", out));
    EXPECT_EQ(out, "café soft");
    out.clear();
    EXPECT_FALSE(d::decoded_content("x", "x-uuencode", out));
    EXPECT_EQ(d::decode_words("=?UTF-8?B?zrHOss6z?="), "αβγ");
    // a charset nobody knows: the word as it was written (encoding::email)
    EXPECT_EQ(d::decode_words("plain =?bogus?Q?x?="), "plain =?bogus?Q?x?=");
    // adjacent words of one charset converted together: a character split
    EXPECT_EQ(d::decode_words("=?UTF-8?Q?=C3?= =?UTF-8?Q?=A9?="), "é");
    EXPECT_EQ(d::decode_words("=?utf-8?q?not ended"), "=?utf-8?q?not ended");
}

TEST(ImapMime, Malformed) {
    // a part without its closing boundary, a boundary never used, nesting
    const std::string cut = "Content-Type: multipart/mixed; boundary=x\r\n\r\n--x\r\nContent-Type: text/plain\r\n\r\nunfinished";
    EXPECT_EQ(sec_of(cut, "1"), "unfinished");
    const std::string unused = "Content-Type: multipart/mixed; boundary=zz\r\n\r\nno parts here\r\n";
    d::Structure st(unused);
    std::string bs;
    d::put_body(bs, unused, st.root, true, false);
    EXPECT_EQ(bs.substr(0, 15), "(\"TEXT\" \"PLAIN\"");   // an empty multipart as text
    std::string deep;
    for (int i = 0; i < 200; ++i) {
        deep += "Content-Type: message/rfc822\r\n\r\n";
    }
    deep += "body";
    d::Structure st2(deep);
    std::string bs2;
    d::put_body(bs2, deep, st2.root, true, false);
    EXPECT_FALSE(bs2.empty());
    // a header line without a colon, a field name with a space
    const std::string odd = "no colon here\r\nX Bad: y\r\nSubject: kept\r\n\r\nb";
    d::Structure st3(odd);
    ASSERT_TRUE(st3.root.field("subject"));
    EXPECT_FALSE(st3.root.field("x bad"));
}
