//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's reading of responses (detail/response.h) against the
// examples of the RFCs: RFC 9051 §8's session and §7's responses, RFC
// 3501's BODYSTRUCTURE, RFC 5256's THREAD, RFC 4731's ESEARCH, RFC 2342's
// NAMESPACE, RFC 7162's VANISHED, RFC 4315's COPYUID and APPENDUID, RFC
// 9208's QUOTA; then responses that break the grammar.
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

    d::Response parse(std::string_view s) {
        d::Response r;
        EXPECT_TRUE(d::parse_response(s, r)) << s;
        return r;
    }

    d::Lexer data(std::string_view s, const d::Response& r) {
        d::Lexer x(s.substr(r.data_at));
        x.set_lenient(true);
        return x;
    }
}

TEST(ImapResponse, StatusResponses) {
    auto r = parse("* OK IMAP4rev2 Service Ready");
    EXPECT_EQ(r.kind, d::Response::Kind::untagged);
    EXPECT_EQ(r.status, d::Status::ok);
    EXPECT_EQ(r.text, "IMAP4rev2 Service Ready");
    r = parse("a002 OK [READ-WRITE] SELECT completed");
    EXPECT_EQ(r.kind, d::Response::Kind::tagged);
    EXPECT_EQ(r.tag, "a002");
    EXPECT_EQ(r.code, "READ-WRITE");
    EXPECT_EQ(r.text, "SELECT completed");
    r = parse("* OK [UIDVALIDITY 3857529045] UIDs valid");
    EXPECT_EQ(r.code, "UIDVALIDITY");
    EXPECT_EQ(r.code_data, "3857529045");
    r = parse("* OK [PERMANENTFLAGS (\\Deleted \\Seen \\*)] Limited");
    EXPECT_EQ(r.code, "PERMANENTFLAGS");
    EXPECT_EQ(r.code_data, "(\\Deleted \\Seen \\*)");
    r = parse("A1 NO [BADCHARSET (US-ASCII \"UTF-8\")] no such charset");
    EXPECT_EQ(r.status, d::Status::no);
    EXPECT_EQ(r.code, "BADCHARSET");
    r = parse("* BYE IMAP4rev2 server terminating connection");
    EXPECT_EQ(r.status, d::Status::bye);
    r = parse("* PREAUTH IMAP4rev2 server logged in as Smith");
    EXPECT_EQ(r.status, d::Status::preauth);
    r = parse("+ Ready for additional command text");
    EXPECT_EQ(r.kind, d::Response::Kind::continuation);
    r = parse("+");
    EXPECT_EQ(r.kind, d::Response::Kind::continuation);
    r = parse("A3 OK");   // no text: taken
    EXPECT_EQ(r.status, d::Status::ok);
}

TEST(ImapResponse, CodesAsErrors) {
    EXPECT_EQ(d::code_of("NONEXISTENT", false), imap::errc::nonexistent);
    EXPECT_EQ(d::code_of("trycreate", false), imap::errc::nonexistent);
    EXPECT_EQ(d::code_of("ALREADYEXISTS", false), imap::errc::already_exists);
    EXPECT_EQ(d::code_of("OVERQUOTA", false), imap::errc::over_quota);
    EXPECT_EQ(d::code_of("AUTHENTICATIONFAILED", false), imap::errc::authentication_failed);
    EXPECT_EQ(d::code_of("", false), imap::errc::no);
    EXPECT_EQ(d::code_of("", true), imap::errc::bad);
    EXPECT_EQ(text(io::error(imap::errc::over_quota, "APPEND").message()), "APPEND: over quota");
}

TEST(ImapResponse, Rfc9051SessionFetch) {
    // RFC 9051 §8, a003's answer
    const std::string s =
        "* 12 FETCH (FLAGS (\\Seen) INTERNALDATE \"17-Jul-1996 02:44:25 -0700\" RFC822.SIZE 4286 ENVELOPE (\"Wed, 17 Jul 1996 02:23:25 -0700 (PDT)\" "
        "\"IMAP4rev2 WG mtg summary and minutes\" ((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) ((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) "
        "((\"Terry Gray\" NIL \"gray\" \"cac.washington.edu\")) ((NIL NIL \"imap\" \"cac.washington.edu\")) ((NIL NIL \"minutes\" \"CNRI.Reston.VA.US\")"
        "(\"John Klensin\" NIL \"KLENSIN\" \"MIT.EDU\")) NIL NIL \"<B27397-0100000@cac.washington.edu>\") BODY (\"TEXT\" \"PLAIN\" (\"CHARSET\" \"US-ASCII\") "
        "NIL NIL \"7BIT\" 3028 92))";
    auto r = parse(s);
    EXPECT_TRUE(r.numbered);
    EXPECT_EQ(r.number, 12u);
    EXPECT_EQ(r.name, "FETCH");
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    ASSERT_EQ(m.flags.size(), 1u);
    EXPECT_EQ(text(m.flags[0]), "\\Seen");
    EXPECT_EQ(m.size, 4286u);
    ASSERT_TRUE(m.internal_date);
    EXPECT_EQ(m.internal_date->unix(), 837596665);
    ASSERT_TRUE(m.envelope);
    EXPECT_EQ(text(m.envelope->subject), "IMAP4rev2 WG mtg summary and minutes");
    ASSERT_EQ(m.envelope->from.size(), 1u);
    EXPECT_EQ(text(m.envelope->from[0].name), "Terry Gray");
    EXPECT_EQ(text(m.envelope->from[0].email()), "gray@cac.washington.edu");
    ASSERT_EQ(m.envelope->cc.size(), 2u);
    EXPECT_EQ(text(m.envelope->cc[1].email()), "KLENSIN@MIT.EDU");
    EXPECT_TRUE(m.envelope->bcc.empty());
    EXPECT_EQ(text(m.envelope->message_id), "<B27397-0100000@cac.washington.edu>");
    ASSERT_TRUE(m.body_structure);
    EXPECT_EQ(text(m.body_structure->type), "text");
    EXPECT_EQ(text(m.body_structure->subtype), "plain");
    EXPECT_EQ(m.body_structure->size, 3028u);
    EXPECT_EQ(m.body_structure->lines, 92u);
    EXPECT_EQ(text(m.body_structure->parameter("charset")), "US-ASCII");
    EXPECT_EQ(text(m.body_structure->part), "1");
}

TEST(ImapResponse, Rfc9051SessionLiteralAndFlags) {
    const std::string s = "* 12 FETCH (BODY[HEADER] {14}\r\nSubject: x\r\n\r\n)";
    auto r = parse(s);
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    ASSERT_TRUE(m.section("HEADER"));
    EXPECT_EQ(text(*m.section("HEADER")), "Subject: x\r\n\r\n");
    EXPECT_FALSE(m.section("TEXT"));
    const std::string s2 = "* 12 FETCH (FLAGS (\\Seen \\Deleted))";
    r = parse(s2);
    imap::message m2;
    auto x2 = data(s2, r);
    ASSERT_TRUE(d::parse_fetch(x2, m2, scratch));
    EXPECT_TRUE(m2.has_flag("\\deleted"));
}

TEST(ImapResponse, Rfc3501BodyStructure) {
    const std::string s =
        "* 1 FETCH (BODYSTRUCTURE ((\"TEXT\" \"PLAIN\" (\"CHARSET\" \"US-ASCII\") NIL NIL \"7BIT\" 1152 23)(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"US-ASCII\" \"NAME\" "
        "\"cc.diff\") \"<960723163407.20117h@cac.washington.edu>\" \"Compiler diff\" \"BASE64\" 4554 73) \"MIXED\"))";
    auto r = parse(s);
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    ASSERT_TRUE(m.body_structure);
    const auto& b = *m.body_structure;
    EXPECT_TRUE(b.is_multipart());
    EXPECT_EQ(text(b.subtype), "mixed");
    EXPECT_EQ(text(b.part), "");
    ASSERT_EQ(b.parts.size(), 2u);
    EXPECT_EQ(text(b.parts[0].part), "1");
    EXPECT_EQ(text(b.parts[1].part), "2");
    EXPECT_EQ(text(b.parts[1].id), "<960723163407.20117h@cac.washington.edu>");
    EXPECT_EQ(text(b.parts[1].description), "Compiler diff");
    EXPECT_EQ(text(b.parts[1].encoding), "base64");
    EXPECT_EQ(text(b.parts[1].filename()), "cc.diff");
    EXPECT_EQ(b.parts[1].lines, 73u);
}

TEST(ImapResponse, ExtendedBodyStructureWithMessage) {
    // a multipart with a message/rfc822 inside it, extension data at each level
    const std::string s =
        "* 3 FETCH (UID 9 BODYSTRUCTURE ((\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"quoted-printable\" 20 2 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL NIL \"7bit\" 300 (NIL \"inner\" NIL NIL NIL NIL NIL NIL NIL NIL) ((\"text\" \"plain\" NIL NIL NIL \"7bit\" 5 1)"
        "(\"image\" \"png\" (\"name\" \"a.png\") NIL NIL \"base64\" 100 NIL (\"attachment\" (\"filename\" \"a.png\")) NIL NIL) \"related\") 12 NIL NIL (\"en\" \"pl\") NIL)"
        " \"mixed\" (\"boundary\" \"b1\") (\"inline\" NIL) \"en\" \"http://example.com/\" \"ext\"))";
    auto r = parse(s);
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    EXPECT_EQ(m.uid, 9u);
    const auto& b = *m.body_structure;
    EXPECT_EQ(text(b.parameter("boundary")), "b1");
    EXPECT_EQ(text(b.disposition), "inline");
    EXPECT_EQ(text(b.location), "http://example.com/");
    ASSERT_EQ(b.parts.size(), 2u);
    const auto& msg = b.parts[1];
    EXPECT_EQ(text(msg.type), "message");
    ASSERT_TRUE(msg.envelope);
    EXPECT_EQ(text(msg.envelope->subject), "inner");
    ASSERT_EQ(msg.parts.size(), 1u);
    const auto& inner = msg.parts[0];
    EXPECT_TRUE(inner.is_multipart());
    ASSERT_EQ(inner.parts.size(), 2u);
    EXPECT_EQ(text(inner.parts[0].part), "2.1");
    EXPECT_EQ(text(inner.parts[1].part), "2.2");
    EXPECT_EQ(text(inner.parts[1].disposition), "attachment");
    EXPECT_EQ(text(inner.parts[1].filename()), "a.png");
    EXPECT_EQ(msg.lines, 12u);
    ASSERT_EQ(msg.language.size(), 2u);
    EXPECT_EQ(text(msg.language[1]), "pl");
}

TEST(ImapResponse, EncodedWordsInTheEnvelope) {
    const std::string s =
        "* 1 FETCH (ENVELOPE (NIL \"=?UTF-8?B?WmHFvMOzxYLEhw==?= =?ISO-8859-2?Q?g=EAsi?=\" ((\"=?utf-8?q?J=C3=B3zef?=\" NIL \"jozef\" \"example.pl\")) NIL NIL NIL NIL NIL NIL NIL))";
    auto r = parse(s);
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    EXPECT_EQ(text(m.envelope->subject), "Zażółćgęsi");   // the blank between two encoded words dropped
    EXPECT_EQ(text(m.envelope->from[0].name), "Józef");
}

TEST(ImapResponse, Groups) {
    const std::string s = "* 1 FETCH (ENVELOPE (NIL NIL NIL NIL NIL ((NIL NIL \"team\" NIL)(NIL NIL \"a\" \"x.org\")(NIL NIL NIL NIL)) NIL NIL NIL NIL))";
    auto r = parse(s);
    imap::message m;
    std::string scratch;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_fetch(x, m, scratch));
    ASSERT_EQ(m.envelope->to.size(), 1u);
    EXPECT_EQ(text(m.envelope->to[0].email()), "a@x.org");
}

TEST(ImapResponse, ListStatusNamespace) {
    std::string scratch;
    std::string s = "* LIST (\\HasNoChildren \\Sent) \"/\" \"Sent Items\"";
    auto r = parse(s);
    imap::list_entry e;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_list(x, e, true, scratch));
    EXPECT_EQ(text(e.name), "Sent Items");
    EXPECT_EQ(e.delimiter, '/');
    EXPECT_TRUE(e.has_attribute("\\sent"));
    s = "* LIST () \".\" ~peter.mail.&U,BTFw-";
    r = parse(s);
    imap::list_entry e2;
    auto x2 = data(s, r);
    ASSERT_TRUE(d::parse_list(x2, e2, false, scratch));
    EXPECT_EQ(text(e2.name), "~peter.mail.\xe5\x8f\xb0\xe5\x8c\x97");
    s = "* LIST () \"/\" INBOX (\"OLDNAME\" (\"inbox\"))";
    r = parse(s);
    imap::list_entry e3;
    auto x3 = data(s, r);
    ASSERT_TRUE(d::parse_list(x3, e3, true, scratch));
    EXPECT_EQ(text(e3.name), "INBOX");
    s = "* LIST (\\Noselect) NIL \"\"";
    r = parse(s);
    imap::list_entry e4;
    auto x4 = data(s, r);
    ASSERT_TRUE(d::parse_list(x4, e4, true, scratch));
    EXPECT_EQ(e4.delimiter, 0);
    s = "* STATUS blurdybloop (MESSAGES 231 UIDNEXT 44292 HIGHESTMODSEQ 7011231777 SIZE 10000)";
    r = parse(s);
    sgcl::string name;
    imap::status st;
    auto x5 = data(s, r);
    ASSERT_TRUE(d::parse_status(x5, name, st, true, scratch));
    EXPECT_EQ(text(name), "blurdybloop");
    EXPECT_EQ(st.messages, 231u);
    EXPECT_EQ(st.uid_next, 44292u);
    EXPECT_EQ(st.highest_modseq, 7011231777u);
    EXPECT_EQ(st.size, 10000u);
    s = "* NAMESPACE ((\"\" \"/\")) ((\"~\" \"/\")) ((\"#shared/\" \"/\")(\"#public/\" \"/\")(\"#ftp/\" \"/\")(\"#news.\" \".\"))";
    r = parse(s);
    imap::namespaces ns;
    auto x6 = data(s, r);
    ASSERT_TRUE(d::parse_namespace(x6, ns, scratch));
    ASSERT_EQ(ns.personal.size(), 1u);
    ASSERT_EQ(ns.other_users.size(), 1u);
    EXPECT_EQ(text(ns.other_users[0].prefix), "~");
    ASSERT_EQ(ns.shared.size(), 4u);
    EXPECT_EQ(ns.shared[3].delimiter, '.');
    s = "* NAMESPACE ((\"\" \"/\")) NIL NIL";
    r = parse(s);
    imap::namespaces ns2;
    auto x7 = data(s, r);
    ASSERT_TRUE(d::parse_namespace(x7, ns2, scratch));
    EXPECT_TRUE(ns2.shared.empty());
}

TEST(ImapResponse, SearchEsearchSortThread) {
    std::string scratch;
    std::string s = "* SEARCH 2 84 882";
    auto r = parse(s);
    sgcl::vector<uint32_t> n;
    uint64_t modseq = 0;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_numbers(x, n, modseq));
    EXPECT_EQ(n.size(), 3u);
    s = "* SEARCH 2 5 6 7 11 12 18 19 20 23 (MODSEQ 917162500)";
    r = parse(s);
    sgcl::vector<uint32_t> n2;
    auto x2 = data(s, r);
    ASSERT_TRUE(d::parse_numbers(x2, n2, modseq));
    EXPECT_EQ(n2.size(), 10u);
    EXPECT_EQ(modseq, 917162500u);
    s = "* SEARCH";
    r = parse(s);
    sgcl::vector<uint32_t> n3;
    auto x3 = data(s, r);
    EXPECT_TRUE(d::parse_numbers(x3, n3, modseq));
    EXPECT_TRUE(n3.empty());
    s = "* ESEARCH (TAG \"A285\") UID MIN 7 MAX 3800";
    r = parse(s);
    d::Esearch e;
    auto x4 = data(s, r);
    ASSERT_TRUE(d::parse_esearch(x4, e, scratch));
    EXPECT_EQ(text(e.tag), "A285");
    EXPECT_TRUE(e.uid);
    EXPECT_EQ(e.min, 7u);
    EXPECT_EQ(e.max, 3800u);
    s = "* ESEARCH (TAG \"A283\") ALL 2,10:11 COUNT 3";
    r = parse(s);
    d::Esearch e2;
    auto x5 = data(s, r);
    ASSERT_TRUE(d::parse_esearch(x5, e2, scratch));
    EXPECT_EQ(text(e2.all.to_string()), "2,10:11");
    EXPECT_EQ(e2.count, 3u);
    // RFC 5256 §4
    s = "* THREAD (2)(3 6 (4 23)(44 7 96))";
    r = parse(s);
    sgcl::vector<imap::thread> t;
    auto x6 = data(s, r);
    ASSERT_TRUE(d::parse_threads(x6, t));
    ASSERT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0].uid, 2u);
    EXPECT_TRUE(t[0].children.empty());
    EXPECT_EQ(t[1].uid, 3u);
    ASSERT_EQ(t[1].children.size(), 1u);
    EXPECT_EQ(t[1].children[0].uid, 6u);
    ASSERT_EQ(t[1].children[0].children.size(), 2u);
    EXPECT_EQ(t[1].children[0].children[0].uid, 4u);
    EXPECT_EQ(t[1].children[0].children[0].children[0].uid, 23u);
    EXPECT_EQ(t[1].children[0].children[1].children[0].children[0].uid, 96u);
    s = "* THREAD ((3)(5))";
    r = parse(s);
    sgcl::vector<imap::thread> t2;
    auto x7 = data(s, r);
    ASSERT_TRUE(d::parse_threads(x7, t2));
    ASSERT_EQ(t2.size(), 1u);
    EXPECT_EQ(t2[0].uid, 0u);   // a dummy holding two threads
    EXPECT_EQ(t2[0].children.size(), 2u);
}

TEST(ImapResponse, ExtensionCodes) {
    imap::copy_result c;
    ASSERT_TRUE(d::parse_copyuid("38505 304,319:320 3956:3958", c));
    EXPECT_EQ(c.uid_validity, 38505u);
    ASSERT_EQ(c.source.size(), 3u);
    EXPECT_EQ(c.source[1], 319u);
    EXPECT_EQ(c.destination[2], 3958u);
    imap::copy_result bad;
    EXPECT_FALSE(d::parse_copyuid("38505 304 3956:3958", bad));   // pairs that do not pair
    uint32_t validity;
    sgcl::vector<uint32_t> uids;
    ASSERT_TRUE(d::parse_appenduid("38505 3955", validity, uids));
    EXPECT_EQ(uids[0], 3955u);
    std::string scratch;
    std::string s = "* VANISHED (EARLIER) 41,43:116,118,120:211,214:540";
    auto r = parse(s);
    bool earlier;
    imap::sequence_set v;
    auto x = data(s, r);
    ASSERT_TRUE(d::parse_vanished(x, earlier, v));
    EXPECT_TRUE(earlier);
    EXPECT_TRUE(v.contains(300));
    s = "* QUOTA \"\" (STORAGE 10 512 MESSAGE 4 100)";
    r = parse(s);
    imap::quota q;
    auto x2 = data(s, r);
    ASSERT_TRUE(d::parse_quota(x2, q, scratch));
    EXPECT_EQ(q.storage_used, 10u);
    EXPECT_EQ(q.messages_limit, 100u);
    s = "* ID (\"name\" \"Cyrus\" \"version\" \"1.5\" \"os\" NIL)";
    r = parse(s);
    sgcl::vector<pair<sgcl::string, sgcl::string>> id;
    auto x3 = data(s, r);
    ASSERT_TRUE(d::parse_id(x3, id, scratch));
    ASSERT_EQ(id.size(), 3u);
    EXPECT_EQ(text(id[0].second), "Cyrus");
    EXPECT_TRUE(id[2].second.empty());
    std::vector<std::string> caps;
    d::parse_capabilities("IMAP4rev2 STARTTLS auth=plain", caps);
    ASSERT_EQ(caps.size(), 3u);
    EXPECT_EQ(caps[2], "AUTH=PLAIN");
}

TEST(ImapResponse, MalformedResponses) {
    d::Response r;
    EXPECT_FALSE(d::parse_response("", r));
    EXPECT_FALSE(d::parse_response("*", r));
    EXPECT_FALSE(d::parse_response("* 12", r));
    EXPECT_FALSE(d::parse_response("A1", r));
    EXPECT_FALSE(d::parse_response("A1 MAYBE done", r));
    EXPECT_FALSE(d::parse_response("* OK [ALERT", r));
    std::string scratch;
    for (std::string_view s : {"* 1 FETCH (UID)", "* 1 FETCH (UID 0)", "* 1 FETCH (FLAGS (\\Seen)", "* 1 FETCH (BODY[1] {5}\r\nab)", "* 1 FETCH (ENVELOPE (NIL))",
                               "* 1 FETCH (BODYSTRUCTURE (\"text\"))", "* 1 FETCH (INTERNALDATE \"yesterday\")", "* 1 FETCH (MODSEQ 5)", "* 1 FETCH UID 5"}) {
        d::Response rr;
        ASSERT_TRUE(d::parse_response(s, rr)) << s;
        imap::message m;
        d::Lexer x(s.substr(rr.data_at));
        EXPECT_FALSE(d::parse_fetch(x, m, scratch)) << s;
    }
    // nesting past the bound
    std::string deep = "* 1 FETCH (BODYSTRUCTURE ";
    for (int i = 0; i < 100; ++i) {
        deep += "(";
    }
    deep += ")";
    d::Response rr;
    ASSERT_TRUE(d::parse_response(deep, rr));
    imap::message m;
    d::Lexer x(std::string_view(deep).substr(rr.data_at));
    EXPECT_FALSE(d::parse_fetch(x, m, scratch));
}
