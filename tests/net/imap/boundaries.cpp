//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The public functions of net::imap at their edges (DESIGN 408): default
// and moved-from handles (a client, a server, a backend), empty and
// largest values (sequence sets, criteria, UIDs at 2^32 - 1), the backends'
// methods on users and mailboxes that are not there, a connect stopped by
// its token and by its timeout, a server serving after its shutdown, an
// empty fetch, store, copy and expunge, a mailbox name at its limits.
#include "tests/net/imap/helpers.h"

#include <string>
#include <thread>

using namespace sgcl;
using namespace sgcl_test::imap;
namespace imap = sgcl::net::imap;

TEST(ImapBoundaries, Handles) {
    imap::client none;
    EXPECT_FALSE(bool(none));
    imap::server a;
    imap::server b = a;   // the same server: copies share the connections
    EXPECT_EQ(a.connections(), 0u);
    b = std::move(a);
    EXPECT_EQ(b.connections(), 0u);
    imap::backend first;   // a memory_backend of its own
    imap::backend second = first;
    second = std::move(first);
    imap::memory_backend m;
    imap::memory_backend copy = m;
    copy.add_user("u", "p");
    EXPECT_TRUE(m.authenticate("u", "p"));   // copies share the mail
    imap::memory_backend moved = std::move(copy);
    EXPECT_TRUE(moved.authenticate("u", "p"));
}

TEST(ImapBoundaries, SequenceSets) {
    imap::sequence_set empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_TRUE(empty.expand(100).empty());
    EXPECT_EQ(text(empty.to_string()), "");
    EXPECT_FALSE(empty.contains(1));
    imap::sequence_set top(UINT32_MAX);
    EXPECT_EQ(text(top.to_string()), "4294967295");
    EXPECT_TRUE(top.contains(UINT32_MAX));
    imap::sequence_set star(imap::last);
    EXPECT_EQ(text(star.to_string()), "*");
    EXPECT_TRUE(star.contains(7, 7));
    EXPECT_FALSE(star.contains(6, 7));
    EXPECT_TRUE(imap::sequence_set::all().expand(0).empty());   // "1:*" of an empty mailbox
    auto big = imap::sequence_set(sgcl::vector<uint32_t>{UINT32_MAX - 1, UINT32_MAX});
    EXPECT_EQ(text(big.to_string()), "4294967294:4294967295");
    imap::sequence_set moved = std::move(big);
    EXPECT_EQ(text(moved.to_string()), "4294967294:4294967295");
    EXPECT_FALSE(imap::sequence_set::parse(sgcl::string(std::string(200001 * 2, ','))));
}

TEST(ImapBoundaries, BackendsOnWhatIsNotThere) {
    imap::memory_backend m;
    EXPECT_FALSE(m.authenticate("nobody", ""));
    EXPECT_EQ(m.open("nobody", "Nope").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.read("nobody", "INBOX", 1).error().code(), imap::errc::expunged);
    EXPECT_EQ(m.append("nobody", "Nope", "x").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.store("nobody", "Nope", {}).error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.expunge("nobody", "Nope", {}).error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.copy("nobody", "INBOX", {1}, "Nope").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.remove("nobody", "INBOX").error().code(), imap::errc::cannot);
    EXPECT_EQ(m.rename("nobody", "Nope", "Else").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(m.create("nobody", "", "").error().code(), imap::errc::cannot);
    EXPECT_EQ(m.create("nobody", sgcl::string(std::string(1025, 'a')), "").error().code(), imap::errc::cannot);
    EXPECT_TRUE(m.create("nobody", sgcl::string(std::string(1024, 'a')), ""));
    EXPECT_EQ(m.revision("nobody", "Nope"), 0u);
    // an empty store and expunge change nothing but the mod-sequence
    auto before = m.open("nobody", "INBOX");
    ASSERT_TRUE(before);
    auto st = m.store("nobody", "INBOX", {});
    ASSERT_TRUE(st);
    EXPECT_GT(*st, before->highest_modseq);
    auto cp = m.copy("nobody", "INBOX", {99}, "INBOX");
    ASSERT_TRUE(cp);
    EXPECT_TRUE(cp->empty());
    const std::string dir = scratch_dir("bounds");
    imap::maildir_backend md{sgcl::string(dir)};
    EXPECT_EQ(md.open("nobody", "Nope").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(md.read("nobody", "Nope", 1).error().code(), imap::errc::nonexistent);
    EXPECT_EQ(md.append("nobody", "Nope", "x").error().code(), imap::errc::nonexistent);
    EXPECT_EQ(md.create("nobody", sgcl::string(std::string(300, 'a')), "").error().code(), imap::errc::cannot);   // past a file name
    EXPECT_EQ(md.mailboxes("../x").error().code(), imap::errc::cannot);
    EXPECT_TRUE(md.append("nobody", "INBOX", "x"));   // a user's INBOX made by the first delivery
    std::filesystem::remove_all(dir);
}

TEST(ImapBoundaries, ConnectStoppedAndTimedOut) {
    // a listener that never answers: the dial completes, the greeting never comes
    auto silent = net::tcp::listen("127.0.0.1:0").value();
    const std::string address = "127.0.0.1:" + std::to_string(silent.local_endpoint().port());
    imap::client::options o;
    o.security = imap::security::none;
    o.timeout = 300ms;
    auto t0 = sgcl::clock::now();
    auto r = imap::client::connect(sgcl::string(address), o);
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout()) << text(r.error().message());
    EXPECT_LT(sgcl::clock::now() - t0, std::chrono::seconds(5));
    // a stop before the dial: refused at once
    async::stop_source stop;
    stop.request_stop();
    imap::client::options o2;
    o2.security = imap::security::none;
    o2.stop = stop.token();
    auto r2 = imap::client::connect(sgcl::string("192.0.2.1:143"), o2);   // TEST-NET-1: never answers
    ASSERT_FALSE(r2);
}

TEST(ImapBoundaries, ServerAfterShutdown) {
    imap::server s;
    s.shutdown();
    auto l = net::tcp::listen("127.0.0.1:0").value();
    auto r = s.serve(l);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::server_closed);
    imap::server s2;
    s2.close();
    auto r2 = s2.serve(sgcl::string("127.0.0.1:0"));
    ASSERT_FALSE(r2);
    EXPECT_EQ(r2.error().code(), net::errc::server_closed);
}

TEST(ImapBoundaries, EmptySetsOnTheClient) {
    Server s(standard_mail());
    imap::client::options o;
    o.security = imap::security::none;
    o.user = "alice";
    o.password = "secret";
    auto c = imap::client::connect(sgcl::string(s.address()), o);
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->select("INBOX"));
    // nothing asked: nothing sent, nothing returned
    auto f = c->fetch(imap::sequence_set());
    ASSERT_TRUE(f);
    EXPECT_TRUE(f->empty());
    auto st = c->store(imap::sequence_set(), imap::store_mode::add, {imap::flag::seen});
    ASSERT_TRUE(st);
    auto cp = c->copy(imap::sequence_set(), "INBOX");
    ASSERT_TRUE(cp);
    EXPECT_TRUE(cp->source.empty());
    // a set of an empty mailbox: no messages
    auto all = c->fetch(imap::sequence_set::all());
    ASSERT_TRUE(all);
    EXPECT_TRUE(all->empty());
    auto none = c->search(imap::criteria::seen());
    ASSERT_TRUE(none);
    EXPECT_TRUE(none->empty());
    EXPECT_EQ(*c->count(), 0u);
    // the largest UID
    auto top = c->fetch(UINT32_MAX);
    ASSERT_TRUE(top);
    EXPECT_TRUE(top->empty());
    // a name of 8-bit text, quoted for a UTF-8 connection
    ASSERT_TRUE(c->create("Ünïcode \"quoted\" \\ name"));
    auto l = c->list("Ü*");
    ASSERT_TRUE(l);
    ASSERT_EQ(l->size(), 1u);
    EXPECT_EQ(text((*l)[0].name), "Ünïcode \"quoted\" \\ name");
}
