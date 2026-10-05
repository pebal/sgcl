//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's SEARCH, SORT and THREAD (detail/search.h): every search key
// of RFC 9051 §6.4.4 read and judged (flags, numbers, sizes, dates of the
// day in the message's own zone, header and body text without regard to
// case, the encoded words and transfer encodings undone, Unicode's case
// folding), MODSEQ, OLDER and YOUNGER, "$"; the keys that do not read; the
// base subject of RFC 5256 §2.1; SORT's keys; THREAD's two algorithms on
// a set of replies.
#include "tests/types.h"
#include "sgcl/net/imap.h"

#include <memory>
#include <string>
#include <vector>

using namespace sgcl;
namespace imap = sgcl::net::imap;
namespace d = sgcl::net::imap::detail;

namespace {
    struct Msg {
        uint32_t uid;
        uint8_t flags;
        std::vector<std::string> keywords;
        uint64_t size;
        d::DateTime date;
        uint64_t modseq;
        std::string content;
    };

    // The UIDs the search keys take of the messages
    std::vector<uint32_t> run(const std::string& keys, const std::vector<Msg>& msgs, bool* parsed = nullptr, std::string charset = {}) {
        d::SearchKey root;
        d::SearchParse sp;
        sp.charset = charset;
        d::Lexer x(keys);
        std::string scratch;
        const bool ok = d::parse_search_keys(x, root, sp, scratch, 0) && x.at_end();
        if (parsed) {
            *parsed = ok;
        }
        std::vector<uint32_t> out;
        if (!ok) {
            return out;
        }
        std::vector<uint32_t> saved = {2};
        for (size_t i = 0; i < msgs.size(); ++i) {
            const Msg& m = msgs[i];
            d::SearchMessage s;
            s.seq = uint32_t(i + 1);
            s.uid = m.uid;
            s.flags = m.flags;
            s.keywords = &m.keywords;
            s.size = m.size;
            s.internal_date = m.date;
            s.modseq = m.modseq;
            s.largest_seq = uint32_t(msgs.size());
            s.largest_uid = msgs.back().uid;
            s.now = 2000000000;
            s.saved = &saved;
            s.content = [&m]() -> const std::string* { return &m.content; };
            std::unique_ptr<d::SearchText> text;
            if (d::judge(root, s, text)) {
                out.push_back(m.uid);
            }
        }
        return out;
    }

    std::vector<Msg> mailbox() {
        d::DateTime oct5{1791190800, 120};   // 2026-10-05 10:20 +02:00
        d::DateTime oct4_late{1791155400, -420};   // 2026-10-04 17:30 -07:00 (2026-10-05 00:30 UTC)
        return {
            {1, d::FlagSeen, {}, 100, oct5, 5,
             "From: Alice <alice@example.com>\r\nTo: bob@example.com\r\nSubject: Lunch\r\nDate: Mon, 5 Oct 2026 10:00:00 +0200\r\n\r\nShall we meet at noon?\r\n"},
            {2, d::FlagFlagged | d::FlagAnswered, {"$Work"}, 5000, oct4_late, 9,
             "From: =?UTF-8?Q?Pawe=C5=82?= <pawel@example.pl>\r\nSubject: =?UTF-8?B?WmHFvMOzxYLEhw==?=\r\nDate: Sun, 4 Oct 2026 17:30:00 -0700\r\n"
             "Content-Type: text/plain; charset=iso-8859-2\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\nG=EA=B6la ja=BC=F1\r\n"},
            {7, d::FlagDeleted, {}, 300, oct5, 12,
             "From: carol@example.org\r\nSubject: STRASSE\r\nX-Spam: yes\r\nContent-Type: multipart/mixed; boundary=b\r\n\r\n--b\r\nContent-Type: text/plain\r\n\r\n"
             "straße\r\n--b\r\nContent-Type: application/zip\r\nContent-Transfer-Encoding: base64\r\n\r\nc2VjcmV0\r\n--b--\r\n"},
        };
    }
}

TEST(ImapSearch, FlagsNumbersSizes) {
    auto box = mailbox();
    EXPECT_EQ(run("ALL", box), (std::vector<uint32_t>{1, 2, 7}));
    EXPECT_EQ(run("SEEN", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("UNSEEN", box), (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(run("FLAGGED ANSWERED", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("DELETED", box), (std::vector<uint32_t>{7}));
    EXPECT_EQ(run("UNDELETED UNDRAFT", box), (std::vector<uint32_t>{1, 2}));
    EXPECT_EQ(run("KEYWORD $work", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("UNKEYWORD $Work", box), (std::vector<uint32_t>{1, 7}));
    EXPECT_EQ(run("KEYWORD \\Seen", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("LARGER 200", box), (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(run("SMALLER 300", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("2:*", box), (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(run("UID 2:6", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("UID *", box), (std::vector<uint32_t>{7}));
    EXPECT_EQ(run("$", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("UID $", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("MODSEQ 9", box), (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(run("MODSEQ \"/flags/\\\\draft\" all 10", box), (std::vector<uint32_t>{7}));
    EXPECT_EQ(run("NOT SEEN", box), (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(run("OR SEEN DELETED", box), (std::vector<uint32_t>{1, 7}));
    EXPECT_EQ(run("(OR SEEN DELETED) LARGER 150", box), (std::vector<uint32_t>{7}));
    EXPECT_EQ(run("OLDER 1000", box), (std::vector<uint32_t>{1, 2, 7}));   // "now" is 2033
    EXPECT_EQ(run("YOUNGER 1000", box), (std::vector<uint32_t>{}));
    EXPECT_EQ(run("YOUNGER 1000000000", box), (std::vector<uint32_t>{1, 2, 7}));
}

TEST(ImapSearch, DatesOfTheDay) {
    auto box = mailbox();
    // message 2 was received on the 4th in its own zone, the 5th in UTC
    EXPECT_EQ(run("ON 5-Oct-2026", box), (std::vector<uint32_t>{1, 7}));
    EXPECT_EQ(run("ON 4-Oct-2026", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("BEFORE 5-Oct-2026", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("SINCE 5-Oct-2026", box), (std::vector<uint32_t>{1, 7}));
    EXPECT_EQ(run("SINCE \"1-Jan-2027\"", box), (std::vector<uint32_t>{}));
    EXPECT_EQ(run("SENTON 4-Oct-2026", box), (std::vector<uint32_t>{2}));
    EXPECT_EQ(run("SENTSINCE 5-Oct-2026", box), (std::vector<uint32_t>{1}));   // 7 has no Date
    EXPECT_EQ(run("SENTBEFORE 5-Oct-2026", box), (std::vector<uint32_t>{2}));
}

TEST(ImapSearch, TextWithoutCase) {
    auto box = mailbox();
    EXPECT_EQ(run("FROM ALICE", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("FROM \"Paweł\"", box), (std::vector<uint32_t>{2}));   // the encoded word decoded
    EXPECT_EQ(run("SUBJECT żółć", box), (std::vector<uint32_t>{}));      // 8-bit as an atom does not read...
    EXPECT_EQ(run("SUBJECT \"ŻÓŁĆ\"", box), (std::vector<uint32_t>{2}));  // ...quoted, folded
    EXPECT_EQ(run("TO bob", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("HEADER X-Spam yes", box), (std::vector<uint32_t>{7}));
    EXPECT_EQ(run("HEADER X-Spam \"\"", box), (std::vector<uint32_t>{7}));   // any message with the field
    EXPECT_EQ(run("BODY noon", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("BODY \"gęśla\"", box), (std::vector<uint32_t>{2}));   // quoted-printable in ISO-8859-2
    EXPECT_EQ(run("BODY strasse", box), (std::vector<uint32_t>{7}));     // ß folds to ss
    EXPECT_EQ(run("BODY secret", box), (std::vector<uint32_t>{}));      // not in an attachment
    EXPECT_EQ(run("TEXT lunch", box), (std::vector<uint32_t>{1}));
    EXPECT_EQ(run("TEXT {4}\r\nnoon", box), (std::vector<uint32_t>{1}));   // a literal
    // CHARSET: the strings converted into UTF-8
    EXPECT_EQ(run("BODY \"g\xea\xb6la\"", box, nullptr, "iso-8859-2"), (std::vector<uint32_t>{2}));
}

TEST(ImapSearch, KeysThatDoNotRead) {
    auto box = mailbox();
    for (const char* k : {"", "FOO", "FROM", "LARGER x", "OR SEEN", "NOT", "(SEEN", "SEEN)", "ON 5-Octo-2026", "UID", "BEFORE 2026-10-05", "OLDER 0",
                          "KEYWORD", "HEADER X", "MODSEQ", "1:", "SEEN  UNSEEN"}) {
        bool parsed = true;
        run(k, box, &parsed);
        EXPECT_FALSE(parsed) << k;
    }
    // nesting past the bound
    std::string deep;
    for (int i = 0; i < 200; ++i) {
        deep += "NOT ";
    }
    deep += "SEEN";
    bool parsed = true;
    run(deep, box, &parsed);
    EXPECT_FALSE(parsed);
}

TEST(ImapSort, BaseSubject) {
    // RFC 5256 §2.1
    EXPECT_EQ(d::base_subject("Re: test"), "test");
    EXPECT_EQ(d::base_subject("re:re: RE: Re: test"), "test");
    EXPECT_EQ(d::base_subject("Fwd: test (fwd)"), "test");
    EXPECT_EQ(d::base_subject("[fwd: test]"), "test");
    EXPECT_EQ(d::base_subject("Re: [list] test"), "test");
    EXPECT_EQ(d::base_subject("[list] Re: test"), "test");
    EXPECT_EQ(d::base_subject("Re[2]: test"), "test");
    EXPECT_EQ(d::base_subject("  spaced   out  "), "spaced out");
    EXPECT_EQ(d::base_subject("[only a blob]"), "[only a blob]");
    EXPECT_EQ(d::base_subject("=?UTF-8?Q?Re=3A_caf=C3=A9?="), "café");
    bool reply = false;
    d::base_subject("Re: x", &reply);
    EXPECT_TRUE(reply);
    d::base_subject("x", &reply);
    EXPECT_FALSE(reply);
}

TEST(ImapSort, Keys) {
    std::vector<d::SortKey> keys;
    d::Lexer x("(REVERSE ARRIVAL SUBJECT)");
    ASSERT_TRUE(d::parse_sort_keys(x, keys));
    ASSERT_EQ(keys.size(), 2u);
    EXPECT_TRUE(keys[0].reverse);
    EXPECT_FALSE(keys[1].reverse);
    for (const char* bad : {"()", "(REVERSE)", "(REVERSE REVERSE DATE)", "(COLOR)", "DATE"}) {
        std::vector<d::SortKey> k;
        d::Lexer y(bad);
        EXPECT_FALSE(d::parse_sort_keys(y, k)) << bad;
    }
    // sorting the mailbox
    auto box = mailbox();
    std::vector<d::SortInfo> infos;
    for (const auto& m : box) {
        infos.push_back(d::sort_info(m.content, m.date));
    }
    std::vector<d::SortItem> items;
    for (size_t i = 0; i < box.size(); ++i) {
        items.push_back(d::SortItem{uint32_t(i + 1), box[i].uid, box[i].size, box[i].date, &infos[i]});
    }
    std::vector<d::SortKey> by_from = {{d::SortKey::Kind::from, false}};
    d::sort_items(items, by_from);
    EXPECT_EQ(items[0].uid, 1u);   // alice, carol, pawel
    EXPECT_EQ(items[2].uid, 2u);
    std::vector<d::SortKey> by_size = {{d::SortKey::Kind::size, true}};
    d::sort_items(items, by_size);
    EXPECT_EQ(items[0].uid, 2u);
    std::vector<d::SortKey> by_date = {{d::SortKey::Kind::date, false}};
    d::sort_items(items, by_date);
    EXPECT_EQ(items[0].uid, 2u);   // 00:30 UTC, then 08:00 UTC, then 7's internal date (no Date)
    EXPECT_EQ(items[2].uid, 7u);
}

TEST(ImapThread, OrderedSubjectAndReferences) {
    // a thread of replies and an unrelated message
    const std::vector<std::string> texts = {
        "Message-ID: <a@x>\r\nSubject: plan\r\nDate: Mon, 5 Oct 2026 10:00:00 +0000\r\n\r\n",
        "Message-ID: <b@x>\r\nIn-Reply-To: <a@x>\r\nSubject: Re: plan\r\nDate: Mon, 5 Oct 2026 11:00:00 +0000\r\n\r\n",
        "Message-ID: <c@x>\r\nSubject: other\r\nDate: Mon, 5 Oct 2026 10:30:00 +0000\r\n\r\n",
        "Message-ID: <d@x>\r\nReferences: <a@x> <b@x>\r\nSubject: Re: plan\r\nDate: Mon, 5 Oct 2026 12:00:00 +0000\r\n\r\n",
        "Message-ID: <e@x>\r\nReferences: <missing@x>\r\nSubject: Re: lost\r\nDate: Mon, 5 Oct 2026 13:00:00 +0000\r\n\r\n",
    };
    std::vector<d::SortInfo> infos;
    for (const auto& t : texts) {
        infos.push_back(d::sort_info(t, d::DateTime{}));
    }
    std::vector<d::SortItem> items;
    for (size_t i = 0; i < texts.size(); ++i) {
        items.push_back(d::SortItem{uint32_t(i + 1), uint32_t(10 + i), 0, d::DateTime{}, &infos[i]});
    }
    auto refs = d::thread_references(items);
    std::string out;
    for (const auto& r : refs) {
        d::put_thread(out, *r, items, true);
    }
    EXPECT_EQ(out, "(10 11 13)(12)(14)");
    auto ordered = d::thread_ordered_subject(items);
    std::string out2;
    for (const auto& r : ordered) {
        d::put_thread(out2, *r, items, false);
    }
    EXPECT_EQ(out2, "(1 (2)(4))(3)(5)");
    // the client reads what the server writes
    std::string line = "* THREAD " + out;
    d::Response resp;
    ASSERT_TRUE(d::parse_response(line, resp));
    d::Lexer x(std::string_view(line).substr(resp.data_at));
    sgcl::vector<imap::thread> t;
    ASSERT_TRUE(d::parse_threads(x, t));
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].children[0].children[0].uid, 13u);
}

TEST(ImapThread, CyclesInReferencesDoNotLoop) {
    const std::vector<std::string> texts = {
        "Message-ID: <a@x>\r\nReferences: <b@x>\r\nSubject: s\r\n\r\n",
        "Message-ID: <b@x>\r\nReferences: <a@x>\r\nSubject: s\r\n\r\n",
        "Message-ID: <a@x>\r\nSubject: duplicate id\r\n\r\n",
        "Subject: no id\r\n\r\n",
    };
    std::vector<d::SortInfo> infos;
    for (const auto& t : texts) {
        infos.push_back(d::sort_info(t, d::DateTime{}));
    }
    std::vector<d::SortItem> items;
    for (size_t i = 0; i < texts.size(); ++i) {
        items.push_back(d::SortItem{uint32_t(i + 1), uint32_t(i + 1), 0, d::DateTime{}, &infos[i]});
    }
    auto refs = d::thread_references(items);
    std::string out;
    for (const auto& r : refs) {
        d::put_thread(out, *r, items, true);
    }
    // every message once
    for (char c : std::string("1234")) {
        EXPECT_EQ(std::count(out.begin(), out.end(), c), 1) << out;
    }
}
