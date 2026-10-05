//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The grammar both sides of net::imap share (detail/syntax.h): the lexer
// over atoms, numbers, quoted strings and literals; modified UTF-7 against
// RFC 3501 §5.1.3's example and its invalid forms; the date-time of APPEND
// and INTERNALDATE, the date of SEARCH; the writing of strings as atoms,
// quoted strings or literals; literal heads at the end of a line; the
// public values: sequence_set and criteria.
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
}

TEST(ImapSyntax, AtomsAndNumbers) {
    d::Lexer x("FETCH 12 4294967295 4294967296 0");
    EXPECT_EQ(x.atom(), "FETCH");
    EXPECT_TRUE(x.sp());
    uint32_t n;
    EXPECT_TRUE(x.number32(n));
    EXPECT_EQ(n, 12u);
    EXPECT_TRUE(x.sp());
    EXPECT_TRUE(x.number32(n));
    EXPECT_EQ(n, 4294967295u);
    EXPECT_TRUE(x.sp());
    EXPECT_FALSE(x.number32(n));   // past 32 bits
    d::Lexer z("0");
    EXPECT_FALSE(z.nz_number(n));
    d::Lexer w("Fetch(");
    EXPECT_TRUE(w.word("FETCH"));
    d::Lexer w2("FETCHX");
    EXPECT_FALSE(w2.word("FETCH"));   // a word ends where an atom does
}

TEST(ImapSyntax, QuotedStrings) {
    std::string scratch;
    std::string_view v;
    d::Lexer a("\"plain\"");
    ASSERT_TRUE(a.string(v, scratch));
    EXPECT_EQ(v, "plain");
    d::Lexer b(R"("with \"quote\" and \\")");
    ASSERT_TRUE(b.string(v, scratch));
    EXPECT_EQ(v, "with \"quote\" and \\");
    d::Lexer c("\"cut");
    EXPECT_FALSE(c.string(v, scratch));
    d::Lexer e("\"bad \\x escape\"");
    EXPECT_FALSE(e.string(v, scratch));
    d::Lexer f("\"line\r\nbreak\"");
    EXPECT_FALSE(f.string(v, scratch));
    d::Lexer g("\"\"");
    ASSERT_TRUE(g.string(v, scratch));
    EXPECT_TRUE(v.empty());
}

TEST(ImapSyntax, Literals) {
    std::string scratch;
    std::string_view v;
    d::Lexer a("{5}\r\nhello rest");
    ASSERT_TRUE(a.string(v, scratch));
    EXPECT_EQ(v, "hello");
    EXPECT_EQ(a.rest(), " rest");
    d::Lexer b("{3+}\r\nabc");
    ASSERT_TRUE(b.string(v, scratch));
    EXPECT_EQ(v, "abc");
    d::Lexer c("~{3}\r\na\0b");
    std::string with_nul("~{3}\r\na\0b", 9);
    d::Lexer c2(with_nul);
    EXPECT_FALSE(c2.string(v, scratch, false));   // literal8 only where it is taken
    d::Lexer c3(with_nul);
    ASSERT_TRUE(c3.string(v, scratch, true));
    EXPECT_EQ(v.size(), 3u);
    d::Lexer cut("{10}\r\nshort");
    EXPECT_FALSE(cut.string(v, scratch));
    d::Lexer nocrlf("{2}ab");
    EXPECT_FALSE(nocrlf.string(v, scratch));
    d::Lexer huge("{99999999999}\r\n");
    EXPECT_FALSE(huge.string(v, scratch));
    d::Lexer minus("{2-}\r\nab");
    EXPECT_FALSE(minus.string(v, scratch));
}

TEST(ImapSyntax, LiteralAtTheEndOfALine) {
    auto h = d::literal_at_end("A1 APPEND INBOX {310}");
    EXPECT_TRUE(h.found);
    EXPECT_TRUE(h.sync);
    EXPECT_EQ(h.size, 310u);
    h = d::literal_at_end("A1 APPEND INBOX {310+}");
    EXPECT_TRUE(h.found);
    EXPECT_FALSE(h.sync);
    h = d::literal_at_end("A1 APPEND INBOX ~{7}");
    EXPECT_TRUE(h.found && h.eight);
    EXPECT_FALSE(d::literal_at_end("A1 NOOP").found);
    EXPECT_FALSE(d::literal_at_end("{}").found);
    EXPECT_FALSE(d::literal_at_end("}").found);
    EXPECT_FALSE(d::literal_at_end("x{1-}").found);
    h = d::literal_at_end("x{99999999999999999999999}");
    EXPECT_TRUE(h.found && h.overflow);
}

TEST(ImapSyntax, ModifiedUtf7) {
    // RFC 3501 §5.1.3
    std::string out;
    ASSERT_TRUE(d::utf7_decode("~peter/mail/&U,BTFw-/&ZeVnLIqe-", out));
    EXPECT_EQ(out, "~peter/mail/\xe5\x8f\xb0\xe5\x8c\x97/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    EXPECT_EQ(d::utf7_encode(out), "~peter/mail/&U,BTFw-/&ZeVnLIqe-");
    EXPECT_EQ(d::utf7_encode("A&B"), "A&-B");
    ASSERT_TRUE(d::utf7_decode("A&-B", out));
    EXPECT_EQ(out, "A&B");
    EXPECT_EQ(d::utf7_encode("Zażółć"), "Za&AXwA8wFCAQc-");
    ASSERT_TRUE(d::utf7_decode("Za&AXwA8wFCAQc-", out));
    EXPECT_EQ(out, "Zażółć");
    // a character outside the BMP, as a surrogate pair
    const std::string emoji = "\xf0\x9f\x98\x80";
    std::string enc = d::utf7_encode(emoji);
    ASSERT_TRUE(d::utf7_decode(enc, out));
    EXPECT_EQ(out, emoji);
    // what RFC 3501 does not allow
    EXPECT_FALSE(d::utf7_decode("&AGE-", out));        // "a" encoded
    EXPECT_TRUE(d::utf7_decode("&AGE-", out, true));   // ... taken when lenient (maildir names)
    EXPECT_FALSE(d::utf7_decode("&U,BTFw", out));      // not ended
    EXPECT_FALSE(d::utf7_decode("&-&", out));
    EXPECT_FALSE(d::utf7_decode("&2D0-", out));        // a lone surrogate
    EXPECT_FALSE(d::utf7_decode("a\xc3\xa9", out));    // 8-bit
    EXPECT_FALSE(d::utf7_decode("&U,BTF-", out));      // bits left over
    EXPECT_TRUE(d::utf7_decode("&-", out));
}

TEST(ImapSyntax, DateTime) {
    d::DateTime t;
    ASSERT_TRUE(d::parse_date_time("17-Jul-1996 02:44:25 -0700", t));
    EXPECT_EQ(t.offset, -420);
    EXPECT_EQ(t.unix, 837596665);
    EXPECT_EQ(d::format_date_time(t), "17-Jul-1996 02:44:25 -0700");
    ASSERT_TRUE(d::parse_date_time(" 5-Oct-2026 09:00:00 +0200", t));
    EXPECT_EQ(d::format_date_time(t), " 5-Oct-2026 09:00:00 +0200");
    ASSERT_TRUE(d::parse_date_time("5-Oct-2026 09:00:00 +0000", t));
    EXPECT_FALSE(d::parse_date_time("17-Jux-1996 02:44:25 -0700", t));
    EXPECT_FALSE(d::parse_date_time("17-Jul-1996 02:44:25", t));
    EXPECT_FALSE(d::parse_date_time("17-Jul-1996 24:00:00 +0000", t));
    EXPECT_FALSE(d::parse_date_time("", t));
    int64_t days;
    ASSERT_TRUE(d::parse_date("1-Feb-1994", days));
    EXPECT_EQ(d::format_date(days), "1-Feb-1994");
    ASSERT_TRUE(d::parse_date("01-Feb-1994", days));
    EXPECT_FALSE(d::parse_date("1-Feb-94", days));
    EXPECT_FALSE(d::parse_date("1 Feb 1994", days));
}

TEST(ImapSyntax, StringsGoOutAsTheyCan) {
    std::string o;
    d::put_astring(o, "INBOX", false);
    EXPECT_EQ(o, "INBOX");
    o.clear();
    d::put_astring(o, "with space", false);
    EXPECT_EQ(o, "\"with space\"");
    o.clear();
    d::put_astring(o, "NIL", false);
    EXPECT_EQ(o, "\"NIL\"");
    o.clear();
    d::put_astring(o, "", false);
    EXPECT_EQ(o, "\"\"");
    o.clear();
    d::put_astring(o, "a\r\nb", false);
    EXPECT_EQ(o, "{4}\r\na\r\nb");
    o.clear();
    d::put_astring(o, "żółw", false);   // 8-bit without UTF-8: a literal
    EXPECT_EQ(o, "{7}\r\nżółw");
    o.clear();
    d::put_astring(o, "żółw", true);
    EXPECT_EQ(o, "\"żółw\"");
    o.clear();
    d::put_string(o, "q\"t", false);
    EXPECT_EQ(o, "\"q\\\"t\"");
}

TEST(ImapSequenceSet, ParseAndWrite) {
    imap::sequence_set s(sgcl::string("1:4,7,10:*"));
    EXPECT_EQ(text(s.to_string()), "1:4,7,10:*");
    EXPECT_TRUE(s.contains(3));
    EXPECT_FALSE(s.contains(5));
    EXPECT_TRUE(s.contains(12, 20));
    EXPECT_FALSE(s.contains(21, 20));
    auto all = s.expand(11);
    EXPECT_EQ(all.size(), 7u);   // 1 2 3 4 7 10 11
    EXPECT_EQ(all.back(), 11u);
    EXPECT_THROW(imap::sequence_set(sgcl::string("1:")), std::invalid_argument);
    EXPECT_FALSE(imap::sequence_set::parse(sgcl::string("0")));
    EXPECT_FALSE(imap::sequence_set::parse(sgcl::string("")));
    EXPECT_FALSE(imap::sequence_set::parse(sgcl::string("1,")));
    EXPECT_FALSE(imap::sequence_set::parse(sgcl::string("4294967296")));
    EXPECT_TRUE(imap::sequence_set::parse(sgcl::string("$"))->is_saved());
    // reversed ranges are the same range
    EXPECT_TRUE(imap::sequence_set(sgcl::string("9:3")).contains(5));
    EXPECT_EQ(text(imap::sequence_set(9, 3).to_string()), "3:9");
}

TEST(ImapSequenceSet, FromNumbers) {
    imap::sequence_set s(sgcl::vector<uint32_t>{5, 1, 2, 3, 9, 10, 3});
    EXPECT_EQ(text(s.to_string()), "1:3,5,9:10");
    EXPECT_TRUE(imap::sequence_set().empty());
    EXPECT_EQ(text(imap::sequence_set::all().to_string()), "1:*");
    imap::sequence_set one(42);
    EXPECT_EQ(text(one.to_string()), "42");
    one.add(50, imap::last);
    EXPECT_EQ(text(one.to_string()), "42,50:*");
    EXPECT_TRUE(imap::sequence_set(sgcl::string("1:2")) == imap::sequence_set(1, 2));
}

TEST(ImapCriteria, TheText) {
    using imap::criteria;
    EXPECT_EQ(text(criteria().to_string()), "ALL");
    EXPECT_EQ(text((criteria::unseen() && criteria::from("alice")).to_string()), "UNSEEN FROM \"alice\"");
    EXPECT_EQ(text((criteria::seen() || criteria::flagged()).to_string()), "OR SEEN FLAGGED");
    EXPECT_EQ(text((!(criteria::seen() && criteria::draft())).to_string()), "NOT (SEEN DRAFT)");
    EXPECT_EQ(text(criteria::since(time::date(2026, 10, 1)).to_string()), "SINCE 1-Oct-2026");
    EXPECT_EQ(text(criteria::header("X-Spam", "yes").to_string()), "HEADER \"X-Spam\" \"yes\"");
    EXPECT_EQ(text(criteria::larger(1000).to_string()), "LARGER 1000");
    EXPECT_EQ(text(criteria::uid(imap::sequence_set(1, 5)).to_string()), "UID 1:5");
    EXPECT_EQ(text(criteria::keyword("$Forwarded").to_string()), "KEYWORD $Forwarded");
    EXPECT_EQ(text(criteria::younger(std::chrono::hours(1)).to_string()), "YOUNGER 3600");
    EXPECT_EQ(text(criteria::modseq(77).to_string()), "MODSEQ 77");
    EXPECT_EQ(text((criteria::all() && criteria::deleted()).to_string()), "DELETED");
    EXPECT_EQ(text(criteria::subject("say \"hi\"").to_string()), "SUBJECT \"say \\\"hi\\\"\"");
}
