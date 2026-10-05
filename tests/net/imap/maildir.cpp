//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::maildir_backend on disk: the layout (cur/new/tmp, flags in
// names, sgcl-uidlist, sgcl-keywords, Maildir++ folders with modified
// UTF-7 and "." escaped), what other programs do to the files (a delivery
// into new/ with LF line ends, a flag renamed, a file removed) taken in
// with new UIDs and mod-sequences, a broken uidlist rebuilt, two handles
// on one directory (two processes) agreeing on UIDs, keywords past 26,
// renaming INBOX, quota, subscriptions kept; a server over it whose idle
// client hears a delivery made by another program; the parsers of names
// and lists (detail/maildir_names.h).
#include "tests/net/imap/helpers.h"

#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace sgcl_test::imap;
namespace imap = sgcl::net::imap;
namespace d = sgcl::net::imap::detail;
namespace fs = std::filesystem;

namespace {
    struct Dir {
        std::string path;

        explicit Dir(const std::string& name)
        : path(scratch_dir(name)) {
        }

        ~Dir() {
            fs::remove_all(path);
        }
    };

    std::vector<std::string> files(const std::string& dir) {
        std::vector<std::string> out;
        if (fs::exists(dir)) {
            for (const auto& e : fs::directory_iterator(dir)) {
                out.push_back(e.path().filename().string());
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void write(const std::string& path, const std::string& data) {
        std::ofstream f(path, std::ios::binary);
        f << data;
    }

    bool has_flag(const imap::stored_message& m, const char* f) {
        for (const auto& x : m.flags) {
            if (x == f) {
                return true;
            }
        }
        return false;
    }
}

TEST(ImapMaildir, Layout) {
    Dir dir("layout");
    imap::maildir_backend md{sgcl::string(dir.path)};
    ASSERT_TRUE(md.add_user("alice", "pw"));
    EXPECT_TRUE(fs::is_directory(dir.path + "/alice/cur"));
    EXPECT_TRUE(fs::is_directory(dir.path + "/alice/new"));
    EXPECT_TRUE(fs::is_directory(dir.path + "/alice/tmp"));
    EXPECT_TRUE(md.authenticate("alice", "pw"));
    EXPECT_FALSE(md.authenticate("alice", "x"));
    auto r = md.append("alice", "INBOX", message("one"), {sgcl::string("\\Seen"), sgcl::string("\\Flagged")});
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->uid, 1u);
    auto cur = files(dir.path + "/alice/cur");
    ASSERT_EQ(cur.size(), 1u);
    EXPECT_TRUE(contains(cur[0], ":2,FS")) << cur[0];
    EXPECT_TRUE(contains(cur[0], ",S=" + std::to_string(msg_text("one").size()))) << cur[0];
    EXPECT_TRUE(files(dir.path + "/alice/tmp").empty());
    std::string list = slurp(dir.path + "/alice/sgcl-uidlist");
    EXPECT_EQ(list.rfind("SGCL1 ", 0), 0u) << list;
    EXPECT_TRUE(contains(list, "\n1 ")) << list;
    auto c = md.open("alice", "INBOX");
    ASSERT_TRUE(c);
    ASSERT_EQ(c->messages.size(), 1u);
    EXPECT_TRUE(has_flag(c->messages[0], "\\Seen"));
    auto text_back = md.read("alice", "INBOX", 1);
    ASSERT_TRUE(text_back);
    EXPECT_EQ(text(*text_back), msg_text("one"));
    EXPECT_FALSE(md.add_user("../evil", "x"));
    EXPECT_FALSE(md.add_user(".hidden", "x"));
    EXPECT_FALSE(md.add_user("a/b", "x"));
}

TEST(ImapMaildir, FoldersAndNames) {
    Dir dir("folders");
    imap::maildir_backend md{sgcl::string(dir.path)};
    md.add_user("alice");
    ASSERT_TRUE(md.create("alice", "Mr. Smith/Zażółć", imap::special_use::archive));
    EXPECT_TRUE(fs::is_directory(dir.path + "/alice/.Mr&AC4- Smith.Za&AXwA8wFCAQc-/cur"));
    EXPECT_TRUE(fs::exists(dir.path + "/alice/.Mr&AC4- Smith.Za&AXwA8wFCAQc-/maildirfolder"));
    auto boxes = md.mailboxes("alice");
    ASSERT_TRUE(boxes);
    bool found = false;
    for (const auto& b : *boxes) {
        if (text(b.name) == "Mr. Smith/Zażółć") {
            found = true;
            EXPECT_TRUE(b.has_attribute(sgcl::string("\\Archive")));
        }
    }
    EXPECT_TRUE(found);
    EXPECT_FALSE(md.create("alice", "Mr. Smith/Zażółć", ""));
    EXPECT_FALSE(md.create("alice", "INBOX", ""));
    EXPECT_FALSE(md.create("alice", "bad//name", ""));
    EXPECT_FALSE(md.create("alice", "wild*", ""));
    ASSERT_TRUE(md.subscribe("alice", "Mr. Smith/Zażółć", true));
    ASSERT_TRUE(md.subscribe("alice", "Gone", true));
    EXPECT_TRUE(contains(slurp(dir.path + "/alice/subscriptions"), "Mr. Smith/Zażółć\n"));
    boxes = md.mailboxes("alice");
    int nonexistent = 0;
    for (const auto& b : *boxes) {
        nonexistent += b.has_attribute(sgcl::string("\\NonExistent"));
    }
    EXPECT_EQ(nonexistent, 1);
    ASSERT_TRUE(md.rename("alice", "Mr. Smith", "Mrs. Smith"));   // a parent never made: its children renamed
    EXPECT_TRUE(fs::is_directory(dir.path + "/alice/.Mrs&AC4- Smith.Za&AXwA8wFCAQc-"));
    ASSERT_TRUE(md.remove("alice", "Mrs. Smith/Zażółć"));
    EXPECT_FALSE(fs::exists(dir.path + "/alice/.Mrs&AC4- Smith.Za&AXwA8wFCAQc-"));
    EXPECT_FALSE(md.remove("alice", "INBOX"));
    EXPECT_FALSE(md.open("alice", "Nowhere"));
    // the parsers of names
    std::string n;
    EXPECT_TRUE(d::folder_name_of("a&AC4-b.c", n));
    EXPECT_EQ(n, "a.b/c");
    EXPECT_FALSE(d::folder_name_of("a..b", n));
    EXPECT_FALSE(d::folder_name_of(".a", n));
    EXPECT_FALSE(d::folder_name_of("INBOX", n));
    EXPECT_FALSE(d::folder_name_of("a&-zz", n) && n != "a&zz");
    EXPECT_EQ(d::folder_dir_name("a.b/c"), "a&AC4-b.c");
    std::string base, letters;
    d::split_file_name("123.M4P5.host,S=10:2,SRa", base, letters);
    EXPECT_EQ(base, "123.M4P5.host,S=10");
    EXPECT_EQ(letters, "RSa");
    d::split_file_name("123.x:1,whatever", base, letters);
    EXPECT_TRUE(letters.empty());
    EXPECT_EQ(d::size_in_name("1.x,S=10,W=12", 3), 12u);
    EXPECT_EQ(d::size_in_name("1.x,S=10", 3), 10u);
    EXPECT_EQ(d::size_in_name("1.x", 3), 3u);
}

TEST(ImapMaildir, WhatOtherProgramsDo) {
    Dir dir("outside");
    imap::maildir_backend md{sgcl::string(dir.path)};
    md.add_user("alice");
    ASSERT_TRUE(md.append("alice", "INBOX", message("ours")));
    const uint64_t rev0 = md.revision("alice", "INBOX");
    // an MTA's delivery: LF line ends, into tmp/ then new/
    write(dir.path + "/alice/tmp/1700000000.M1P1.mta", "Subject: from the mta\nFrom: x@y\n\nline\n");
    fs::rename(dir.path + "/alice/tmp/1700000000.M1P1.mta", dir.path + "/alice/new/1700000000.M1P1.mta");
    EXPECT_NE(md.revision("alice", "INBOX"), rev0);
    auto c = md.open("alice", "INBOX");
    ASSERT_TRUE(c);
    ASSERT_EQ(c->messages.size(), 2u);
    EXPECT_EQ(c->messages[1].uid, 2u);
    EXPECT_TRUE(files(dir.path + "/alice/new").empty());   // moved into cur/
    auto t = md.read("alice", "INBOX", 2);
    ASSERT_TRUE(t);
    EXPECT_EQ(text(*t), "Subject: from the mta\r\nFrom: x@y\r\n\r\nline\r\n");
    EXPECT_EQ(c->messages[1].size, t->size());   // the size as IMAP counts it
    // another MUA marks it seen by a rename
    const uint64_t before = c->messages[1].modseq;
    fs::rename(dir.path + "/alice/cur/1700000000.M1P1.mta:2,", dir.path + "/alice/cur/1700000000.M1P1.mta:2,S");
    c = md.open("alice", "INBOX");
    ASSERT_TRUE(c);
    EXPECT_TRUE(has_flag(c->messages[1], "\\Seen"));
    EXPECT_GT(c->messages[1].modseq, before);
    EXPECT_GE(c->highest_modseq, c->messages[1].modseq);
    // and removes it
    fs::remove(dir.path + "/alice/cur/1700000000.M1P1.mta:2,S");
    c = md.open("alice", "INBOX");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->messages.size(), 1u);
    auto gone = md.read("alice", "INBOX", 2);
    ASSERT_FALSE(gone);
    EXPECT_EQ(gone.error().code(), imap::errc::expunged);
    // a broken list: rebuilt, with a new UIDVALIDITY
    const uint32_t validity = c->uid_validity;
    write(dir.path + "/alice/sgcl-uidlist", "garbage");
    c = md.open("alice", "INBOX");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->messages.size(), 1u);
    EXPECT_NE(c->uid_validity, validity);
    EXPECT_TRUE(fs::exists(dir.path + "/alice/sgcl-uidlist.broken"));
}

TEST(ImapMaildir, TwoHandlesOneDirectory) {
    Dir dir("two");
    imap::maildir_backend a{sgcl::string(dir.path)};
    imap::maildir_backend b{sgcl::string(dir.path)};
    a.add_user("alice");
    std::vector<std::thread> ts;
    std::atomic<int> failures{0};
    for (int k = 0; k < 2; ++k) {
        ts.emplace_back([&, k] {
            for (int i = 0; i < 25; ++i) {
                auto r = (k ? b : a).append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
                failures += !r;
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(failures.load(), 0);
    auto ca = a.open("alice", "INBOX");
    auto cb = b.open("alice", "INBOX");
    ASSERT_TRUE(ca && cb);
    EXPECT_EQ(ca->messages.size(), 50u);
    EXPECT_EQ(cb->messages.size(), 50u);
    std::vector<uint32_t> uids;
    for (const auto& m : ca->messages) {
        uids.push_back(m.uid);
    }
    std::sort(uids.begin(), uids.end());
    EXPECT_EQ(std::unique(uids.begin(), uids.end()), uids.end());
    EXPECT_EQ(uids.back(), 50u);
    EXPECT_EQ(ca->uid_validity, cb->uid_validity);
}

TEST(ImapMaildir, KeywordsRenameInboxQuota) {
    Dir dir("keywords");
    imap::maildir_backend md{sgcl::string(dir.path)};
    md.add_user("alice");
    auto r = md.append("alice", "INBOX", message("k"));
    ASSERT_TRUE(r);
    imap::flag_update u;
    u.uid = 1;
    u.flags = {sgcl::string("\\Answered"), sgcl::string("$Label1")};
    ASSERT_TRUE(md.store("alice", "INBOX", {u}));
    auto cur = files(dir.path + "/alice/cur");
    ASSERT_EQ(cur.size(), 1u);
    EXPECT_TRUE(contains(cur[0], ":2,Ra")) << cur[0];
    EXPECT_EQ(slurp(dir.path + "/alice/sgcl-keywords"), "0 $Label1\n");
    // 26 keywords, then the 27th refused
    for (int i = 2; i <= 26; ++i) {
        u.flags.push_back(sgcl::string("$L" + std::to_string(i)));
    }
    EXPECT_TRUE(md.store("alice", "INBOX", {u}));
    u.flags.push_back(sgcl::string("$TooMany"));
    auto over = md.store("alice", "INBOX", {u});
    ASSERT_FALSE(over);
    EXPECT_EQ(over.error().code(), imap::errc::limit);
    // copy keeps flags
    md.create("alice", "Copy", "");
    auto cp = md.copy("alice", "INBOX", {1}, "Copy");
    ASSERT_TRUE(cp);
    ASSERT_EQ(cp->size(), 1u);
    EXPECT_TRUE(has_flag((*cp)[0], "\\Answered"));
    // RENAME INBOX
    ASSERT_TRUE(md.rename("alice", "INBOX", "Old Inbox"));
    auto inbox = md.open("alice", "INBOX");
    ASSERT_TRUE(inbox);
    EXPECT_TRUE(inbox->messages.empty());
    auto old = md.open("alice", "Old Inbox");
    ASSERT_TRUE(old);
    EXPECT_EQ(old->messages.size(), 1u);
    // quota
    md.set_quota("alice", 0, 2);
    auto q = md.quota("alice");
    ASSERT_TRUE(q);
    EXPECT_EQ(q->messages_used, 2u);
    auto full = md.append("alice", "INBOX", message("over"));
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().code(), imap::errc::over_quota);
    auto e = md.expunge("alice", "Copy", {1});
    ASSERT_TRUE(e);
    EXPECT_TRUE(md.append("alice", "INBOX", message("fits")));
}

TEST(ImapMaildir, ServerHearsADeliveryOfAnotherProgram) {
    Dir dir("server");
    imap::maildir_backend md{sgcl::string(dir.path)};
    md.add_user("alice", "secret");
    Server s(md);
    imap::client::options o;
    o.security = imap::security::none;
    o.user = "alice";
    o.password = "secret";
    auto c = imap::client::connect(sgcl::string(s.address()), o);
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->select("INBOX"));
    // another process's delivery: a file appearing in new/, no call into
    // this backend; the idling session finds it on its poll
    std::thread mta([&] {
        std::this_thread::sleep_for(100ms);
        write(dir.path + "/alice/tmp/1800000000.M2P2.mta", "Subject: polled\r\n\r\nx\r\n");
        fs::rename(dir.path + "/alice/tmp/1800000000.M2P2.mta", dir.path + "/alice/new/1800000000.M2P2.mta");
    });
    auto u = c->idle(5s);
    mta.join();
    ASSERT_TRUE(u) << text(u.error().message());
    ASSERT_FALSE(u->empty());
    EXPECT_EQ((*u)[0].kind, imap::update::kind::exists);
    auto m = c->fetch_message(1);
    ASSERT_TRUE(m);
    EXPECT_EQ(text(*m), "Subject: polled\r\n\r\nx\r\n");
    // a delivery through the backend (an SMTP handler's) wakes it at once
    std::thread smtp([&] {
        std::this_thread::sleep_for(100ms);
        md.append("alice", "INBOX", message("direct"));
    });
    auto u2 = c->idle(5s);
    smtp.join();
    ASSERT_TRUE(u2);
    ASSERT_FALSE(u2->empty());
}

TEST(ImapMaildir, UidlistParser) {
    struct M {
        uint32_t uid = 0;
        uint64_t modseq = 0;
        std::string base, letters;
    };
    std::vector<M> out;
    uint32_t v, n;
    uint64_t ms;
    EXPECT_TRUE(d::parse_uidlist("SGCL1 7 4 9\n1 2 S a.b\n3 9 - c.d\n", v, n, ms, out));
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[1].letters, "");
    EXPECT_EQ(v, 7u);
    for (const char* bad : {"", "SGCL1 7 4 9", "SGCL2 7 4 9\n", "SGCL1 0 4 9\n", "SGCL1 7 4 9\n3 1 - a\n2 1 - b\n", "SGCL1 7 4 9\n4 1 - a\n",
                            "SGCL1 7 4 9\n1 10 - a\n", "SGCL1 7 4 9\n1 1 S a/b\n", "SGCL1 7 4 9\n1 1 S\n", "SGCL1 7 4 9\n1 1 S! a\n", "SGCL1 7 4 9\n1 1 - a"}) {
        std::vector<M> o;
        EXPECT_FALSE(d::parse_uidlist(bad, v, n, ms, o)) << bad;
    }
    std::vector<std::string> kw;
    d::parse_keywords("0 $A\n1 $B\n3 $D\n", kw);
    EXPECT_EQ(kw.size(), 2u);
}
