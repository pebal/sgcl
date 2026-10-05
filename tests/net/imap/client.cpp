//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::client against the module's own server, over both backends
// (memory and Maildir): connect by address and by URL, STARTTLS required
// by default and refused when absent, implicit TLS, every login
// mechanism, every method (list, status, create, remove, rename,
// subscribe, select with QRESYNC, examine, unselect, close_mailbox, fetch
// of every item, search typed and as text, count, sort, threads, store
// with CONDSTORE, copy, move, expunge, append, idle with a stop and a
// timeout, namespaces, quota, command, fetch_unseen), COMPRESS, the
// updates pushed, pipelined commands from several tasks, errors as codes,
// a closed client, the async forms.
#include "tests/net/imap/helpers.h"

#include <atomic>
#include <string>
#include <thread>

using namespace sgcl;
using namespace sgcl_test::imap;
namespace imap = sgcl::net::imap;

namespace {
    imap::client::options plain() {
        imap::client::options o;
        o.security = imap::security::none;
        o.user = "alice";
        o.password = "secret";
        o.timeout = 10s;
        return o;
    }

    // A backend of each kind with alice and bob
    struct Backends {
        std::string dir = scratch_dir("client");

        imap::backend make(bool maildir) {
            if (!maildir) {
                return standard_mail();
            }
            imap::maildir_backend m{sgcl::string(dir)};
            m.add_user("alice", "secret");
            m.add_user("bob", "hunter2");
            return m;
        }

        ~Backends() {
            std::filesystem::remove_all(dir);
        }
    };

    class ImapClientBoth : public ::testing::TestWithParam<bool> {};
}

TEST(ImapClient, ConnectForms) {
    Server s(standard_mail());
    // an address and options
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->has("IMAP4rev2"));
    EXPECT_TRUE(c->has("imap4rev2"));
    EXPECT_FALSE(c->is_tls());
    EXPECT_FALSE(c->is_closed());
    EXPECT_TRUE(bool(*c));
    EXPECT_TRUE(c->logout());
    EXPECT_TRUE(c->is_closed());
    // a URL with the credentials and a mailbox to select
    imap::client::options o;
    o.security = imap::security::none;
    auto u = imap::client::connect(sgcl::string("imap://alice:secret@127.0.0.1:" + std::to_string(s.port) + "/INBOX"), o);
    ASSERT_TRUE(u) << text(u.error().message());
    EXPECT_EQ(text(u->mailbox().name), "INBOX");
    // STARTTLS is required by default: a server without it is refused
    auto t = imap::client::connect(sgcl::string(s.address()));
    ASSERT_FALSE(t);
    EXPECT_EQ(t.error().code(), imap::errc::starttls_unavailable);
    // wrong credentials
    auto o2 = plain();
    o2.password = "nope";
    auto w = imap::client::connect(sgcl::string(s.address()), o2);
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().code(), imap::errc::authentication_failed);
    // nothing listening
    auto n = imap::client::connect(sgcl::string("127.0.0.1:1"), plain());
    EXPECT_FALSE(n);
    // a URL that is not one
    auto bad = imap::client::connect(sgcl::string("imap://[::1"), plain());
    ASSERT_FALSE(bad);
    // a default client
    imap::client empty;
    EXPECT_FALSE(bool(empty));
}

TEST(ImapClient, StartTlsAndImplicitTls) {
    Server s(standard_mail(), false, [](imap::server& srv) { srv.tls = server_tls(); });
    imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.tls = client_tls();
    auto c = imap::client::connect(sgcl::string(s.address()), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->is_tls());
    EXPECT_FALSE(c->has("STARTTLS"));
    // without the CA: the chain does not verify
    imap::client::options o2 = o;
    o2.tls = net::tls::config();
    auto refused = imap::client::connect(sgcl::string(s.address()), o2);
    EXPECT_FALSE(refused);
    // implicit TLS (imaps://)
    Server s2(standard_mail(), true);
    imap::client::options o3;
    o3.tls = client_tls();
    auto c3 = imap::client::connect(sgcl::string("imaps://alice:secret@localhost:" + std::to_string(s2.port)), o3);
    ASSERT_TRUE(c3) << text(c3.error().message());
    EXPECT_TRUE(c3->is_tls());
    EXPECT_TRUE(c3->noop());
}

TEST(ImapClient, Mechanisms) {
    Server s(standard_mail(), false, [](imap::server& srv) {
        srv.check_token = [](const sgcl::string& u, const sgcl::string& t) { return u == "alice" && t == "token-1"; };
    });
    for (auto m : {imap::mechanism::automatic, imap::mechanism::plain, imap::mechanism::login, imap::mechanism::login_command}) {
        auto o = plain();
        o.mechanism = m;
        auto c = imap::client::connect(sgcl::string(s.address()), o);
        EXPECT_TRUE(c) << int(m);
    }
    for (auto m : {imap::mechanism::automatic, imap::mechanism::xoauth2, imap::mechanism::oauthbearer}) {
        imap::client::options o;
        o.security = imap::security::none;
        o.user = "alice";
        o.token = "token-1";
        o.mechanism = m;
        auto c = imap::client::connect(sgcl::string(s.address()), o);
        EXPECT_TRUE(c) << int(m);
        o.token = "wrong";
        auto bad = imap::client::connect(sgcl::string(s.address()), o);
        ASSERT_FALSE(bad);
        EXPECT_EQ(bad.error().code(), imap::errc::authentication_failed);
    }
}

TEST_P(ImapClientBoth, Mailboxes) {
    Backends b;
    Server s(b.make(GetParam()));
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c) << text(c.error().message());
    ASSERT_TRUE(c->create("Archive/2026"));
    ASSERT_TRUE(c->create("Sent", imap::special_use::sent));
    ASSERT_TRUE(c->create("Zażółć gęślą"));
    auto dup = c->create("Sent");
    ASSERT_FALSE(dup);
    EXPECT_EQ(dup.error().code(), imap::errc::already_exists);
    auto all = c->list();
    ASSERT_TRUE(all);
    std::vector<std::string> names;
    for (const auto& e : *all) {
        names.push_back(text(e.name));
    }
    EXPECT_EQ(names.front(), "INBOX");
    EXPECT_NE(std::find(names.begin(), names.end(), "Archive/2026"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Zażółć gęślą"), names.end());
    imap::list_options special;
    special.special_use = true;
    auto su = c->list(sgcl::string("*"), special);
    ASSERT_TRUE(su);
    ASSERT_EQ(su->size(), 1u);
    EXPECT_TRUE((*su)[0].has_attribute(sgcl::string(imap::special_use::sent)));
    ASSERT_TRUE(c->subscribe("Archive/2026"));
    imap::list_options subs;
    subs.subscribed = true;
    auto sl = c->list(sgcl::string("*"), subs);
    ASSERT_TRUE(sl);
    ASSERT_EQ(sl->size(), 1u);
    ASSERT_TRUE(c->unsubscribe("Archive/2026"));
    imap::list_options with_status;
    with_status.status = true;
    auto ls = c->list(sgcl::string("INBOX"), with_status);
    ASSERT_TRUE(ls);
    ASSERT_EQ(ls->size(), 1u);
    ASSERT_TRUE((*ls)[0].status);
    ASSERT_TRUE(c->rename("Archive", "Old"));
    auto renamed = c->list(sgcl::string("Old/*"));
    ASSERT_TRUE(renamed);
    ASSERT_EQ(renamed->size(), 1u);
    EXPECT_EQ(text((*renamed)[0].name), "Old/2026");
    ASSERT_TRUE(c->remove("Old/2026"));
    auto gone = c->remove("Old/2026");
    ASSERT_FALSE(gone);
    EXPECT_EQ(gone.error().code(), imap::errc::nonexistent);
    auto st = c->status("INBOX");
    ASSERT_TRUE(st);
    EXPECT_EQ(st->messages, 0u);
    EXPECT_GT(st->uid_validity, 0u);
    auto ns = c->namespaces();
    ASSERT_TRUE(ns);
    EXPECT_EQ(ns->personal.size(), 1u);
}

TEST_P(ImapClientBoth, Messages) {
    Backends b;
    Server s(b.make(GetParam()));
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c) << text(c.error().message());
    auto u1 = c->append("INBOX", message("first", "alpha body\r\n", "Alice <alice@example.com>"));
    ASSERT_TRUE(u1) << text(u1.error().message());
    EXPECT_EQ(*u1, 1u);
    auto u2 = c->append("INBOX", message("second", "beta body\r\n"), {imap::flag::flagged, "$Work"},
                        time::datetime::from_unix(1791190800, time::zone::fixed(std::chrono::hours(2))));
    ASSERT_TRUE(u2);
    auto multipart = c->append("INBOX",
                               "Subject: parts\r\nContent-Type: multipart/mixed; boundary=q\r\n\r\n--q\r\nContent-Type: text/plain\r\n\r\ntext part\r\n--q\r\n"
                               "Content-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"a.bin\"\r\nContent-Transfer-Encoding: base64\r\n\r\n"
                               "AAEC\r\n--q--\r\n");
    ASSERT_TRUE(multipart);
    auto sel = c->select("INBOX");
    ASSERT_TRUE(sel) << text(sel.error().message());
    EXPECT_EQ(sel->exists, 3u);
    EXPECT_EQ(sel->uid_next, 4u);
    EXPECT_GT(sel->highest_modseq, 0u);
    EXPECT_FALSE(sel->read_only);
    // fetch of every item
    imap::fetch_options o;
    o.envelope = true;
    o.body_structure = true;
    o.size = true;
    o.internal_date = true;
    o.modseq = true;
    o.sections = {"", "HEADER.FIELDS (SUBJECT)", "TEXT"};
    auto ms = c->fetch(imap::sequence_set::all(), o);
    ASSERT_TRUE(ms) << text(ms.error().message());
    ASSERT_EQ(ms->size(), 3u);
    const auto& m1 = (*ms)[0];
    EXPECT_EQ(m1.uid, 1u);
    EXPECT_EQ(text(m1.envelope->subject), "first");
    EXPECT_EQ(text(m1.envelope->from[0].email()), "alice@example.com");
    EXPECT_EQ(text(*m1.section("TEXT")), "alpha body\r\n");
    EXPECT_EQ(text(*m1.section("HEADER.FIELDS (SUBJECT)")), "Subject: first\r\n\r\n");
    EXPECT_EQ(m1.size, m1.text().size());
    EXPECT_TRUE(m1.internal_date);
    EXPECT_GT(m1.modseq, 0u);
    EXPECT_FALSE(m1.has_flag(imap::flag::seen));   // PEEK
    const auto& m2 = (*ms)[1];
    EXPECT_TRUE(m2.has_flag(imap::flag::flagged));
    EXPECT_TRUE(m2.has_flag("$Work"));
    EXPECT_EQ(m2.internal_date->unix(), 1791190800);
    const auto& m3 = (*ms)[2];
    ASSERT_TRUE(m3.body_structure);
    ASSERT_EQ(m3.body_structure->parts.size(), 2u);
    EXPECT_EQ(text(m3.body_structure->parts[1].filename()), "a.bin");
    // binary of the attachment, decoded by the server
    imap::fetch_options bin;
    bin.flags = false;
    bin.binary = true;
    bin.sections = {"2"};
    auto bm = c->fetch(3, bin);
    ASSERT_TRUE(bm);
    EXPECT_EQ(text(*(*bm)[0].section("2")), std::string("\0\1\2", 3));
    // partial
    imap::fetch_options part;
    part.sections = {""};
    part.partial_offset = 0;
    part.partial_length = 8;
    auto pm = c->fetch(1, part);
    ASSERT_TRUE(pm);
    EXPECT_EQ(text(*(*pm)[0].section("")), "From: Al");
    auto whole = c->fetch_message(1);
    ASSERT_TRUE(whole);
    EXPECT_EQ(text(*whole).rfind("From: Alice", 0), 0u);
    auto none = c->fetch_message(99);
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), imap::errc::expunged);
    // search typed and as text
    auto f = c->search(imap::criteria::flagged());
    ASSERT_TRUE(f);
    EXPECT_EQ(*f, (sgcl::vector<uint32_t>{2}));
    auto t = c->search(sgcl::string("OR SUBJECT first SUBJECT parts"));
    ASSERT_TRUE(t);
    EXPECT_EQ(*t, (sgcl::vector<uint32_t>{1, 3}));
    auto body = c->search(imap::criteria::body("BETA"));
    ASSERT_TRUE(body);
    EXPECT_EQ(*body, (sgcl::vector<uint32_t>{2}));
    auto all = c->search();
    ASSERT_TRUE(all);
    EXPECT_EQ(all->size(), 3u);
    auto n = c->count(imap::criteria::unseen());
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 3u);
    auto utf = c->search(imap::criteria::subject("żółw"));
    ASSERT_TRUE(utf);
    EXPECT_TRUE(utf->empty());
    // sort and threads
    auto sorted = c->sort({imap::order::subject});
    ASSERT_TRUE(sorted);
    EXPECT_EQ(*sorted, (sgcl::vector<uint32_t>{1, 3, 2}));   // first, parts, second
    auto desc = c->sort({imap::order::arrival}, imap::criteria(), true);
    ASSERT_TRUE(desc);
    auto th = c->threads();
    ASSERT_TRUE(th);
    EXPECT_EQ(th->size(), 3u);
    auto th2 = c->threads(imap::criteria(), imap::threading::ordered_subject);
    ASSERT_TRUE(th2);
    // store
    ASSERT_TRUE(c->add_flags(1, {imap::flag::seen, imap::flag::answered}));
    ASSERT_TRUE(c->remove_flags(1, {imap::flag::answered}));
    auto after = c->fetch(1);
    ASSERT_TRUE(after);
    EXPECT_TRUE((*after)[0].has_flag(imap::flag::seen));
    EXPECT_FALSE((*after)[0].has_flag(imap::flag::answered));
    ASSERT_TRUE(c->set_flags(1, {"$Done"}));
    after = c->fetch(1);
    EXPECT_EQ((*after)[0].flags.size(), 1u);
    auto badflag = c->add_flags(1, {"two words"});
    ASSERT_FALSE(badflag);
    // CONDSTORE: a store unchanged since an old modseq leaves the message alone
    auto mod = c->store(1, imap::store_mode::add, {imap::flag::flagged}, 1);
    ASSERT_TRUE(mod);
    EXPECT_EQ(*mod, (sgcl::vector<uint32_t>{1}));
    // changed since
    imap::fetch_options since;
    since.changed_since = sel->highest_modseq;
    auto ch = c->fetch(imap::sequence_set::all(), since);
    ASSERT_TRUE(ch);
    EXPECT_EQ(ch->size(), 1u);   // only message 1 changed
    // copy, move, expunge
    ASSERT_TRUE(c->create("Done"));
    auto cp = c->copy(imap::sequence_set(1, 2), "Done");
    ASSERT_TRUE(cp) << text(cp.error().message());
    EXPECT_EQ(cp->source, (sgcl::vector<uint32_t>{1, 2}));
    EXPECT_EQ(cp->destination, (sgcl::vector<uint32_t>{1, 2}));
    auto mv = c->move(3, "Done");
    ASSERT_TRUE(mv);
    EXPECT_EQ(mv->destination, (sgcl::vector<uint32_t>{3}));
    EXPECT_EQ(c->mailbox().exists, 2u);
    auto nowhere = c->copy(1, "Nowhere");
    ASSERT_FALSE(nowhere);
    EXPECT_EQ(nowhere.error().code(), imap::errc::nonexistent);
    ASSERT_TRUE(c->add_flags(imap::sequence_set::all(), {imap::flag::deleted}));
    ASSERT_TRUE(c->expunge(2));
    EXPECT_EQ(c->mailbox().exists, 1u);
    ASSERT_TRUE(c->expunge());
    EXPECT_EQ(c->mailbox().exists, 0u);
    // examine, unselect, close_mailbox
    auto ex = c->examine("Done");
    ASSERT_TRUE(ex);
    EXPECT_TRUE(ex->read_only);
    EXPECT_EQ(ex->exists, 3u);
    auto ro = c->add_flags(1, {imap::flag::seen});
    EXPECT_FALSE(ro);
    ASSERT_TRUE(c->unselect());
    auto nosel = c->fetch(1);
    ASSERT_FALSE(nosel);
    EXPECT_EQ(nosel.error().code(), imap::errc::bad);
    ASSERT_TRUE(c->select("Done"));
    ASSERT_TRUE(c->add_flags(1, {imap::flag::deleted}));
    ASSERT_TRUE(c->close_mailbox());
    auto st = c->status("Done");
    ASSERT_TRUE(st);
    EXPECT_EQ(st->messages, 2u);
    auto raw = c->command("NOOP");
    ASSERT_TRUE(raw);
    auto rawbad = c->command("BOGUS");
    ASSERT_FALSE(rawbad);
    EXPECT_EQ(rawbad.error().code(), imap::errc::bad);
    auto rawcrlf = c->command("NOOP\r\nX");
    EXPECT_FALSE(rawcrlf);
    auto q = c->quota();
    EXPECT_TRUE(q) << text(q.error().message());
}

INSTANTIATE_TEST_SUITE_P(Backends, ImapClientBoth, ::testing::Values(false, true), [](const ::testing::TestParamInfo<bool>& i) {
    return i.param ? std::string("Maildir") : std::string("Memory");
});

TEST(ImapClient, IdleAndUpdates) {
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("old"));
    Server s(mail);
    std::atomic<int> pushed{0};
    auto o = plain();
    o.on_update = [&pushed](const imap::update&) { pushed.fetch_add(1); };
    auto c = imap::client::connect(sgcl::string(s.address()), o);
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->select("INBOX"));
    // a delivery while idling ends the idle with its update
    std::thread deliver([&mail] {
        std::this_thread::sleep_for(200ms);
        mail.append("alice", "INBOX", message("new"));
    });
    auto u = c->idle(5s);
    deliver.join();
    ASSERT_TRUE(u) << text(u.error().message());
    ASSERT_FALSE(u->empty());
    EXPECT_EQ((*u)[0].kind, imap::update::kind::exists);
    EXPECT_EQ((*u)[0].number, 2u);
    EXPECT_EQ(c->mailbox().exists, 2u);
    EXPECT_GT(pushed.load(), 0);
    // a timeout: nothing
    auto t = c->idle(300ms);
    ASSERT_TRUE(t);
    EXPECT_TRUE(t->empty());
    // a stop
    async::stop_source stop;
    std::thread stopper([&stop] {
        std::this_thread::sleep_for(200ms);
        stop.request_stop();
    });
    auto st = c->idle(stop.token(), 10s);
    stopper.join();
    ASSERT_TRUE(st);
    EXPECT_TRUE(st->empty());
    // another session's changes reach this one through NOOP
    auto other = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(other);
    ASSERT_TRUE(other->select("INBOX"));
    ASSERT_TRUE(other->add_flags(1, {imap::flag::flagged}));
    ASSERT_TRUE(other->add_flags(1, {imap::flag::deleted}));
    ASSERT_TRUE(other->expunge());
    const int before = pushed.load();
    ASSERT_TRUE(c->noop());
    EXPECT_GT(pushed.load(), before);
    EXPECT_EQ(c->mailbox().exists, 1u);
}

TEST(ImapClient, QresyncSelect) {
    auto mail = standard_mail();
    for (int i = 0; i < 4; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i)));
    }
    Server s(mail);
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c);
    auto first = c->select("INBOX");
    ASSERT_TRUE(first);
    ASSERT_TRUE(c->add_flags(2, {imap::flag::seen}));
    ASSERT_TRUE(c->add_flags(3, {imap::flag::deleted}));
    ASSERT_TRUE(c->expunge());
    ASSERT_TRUE(c->logout());
    auto c2 = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c2);
    imap::select_options o;
    o.uid_validity = first->uid_validity;
    o.modseq = first->highest_modseq;
    auto again = c2->select("INBOX", o);
    ASSERT_TRUE(again) << text(again.error().message());
    EXPECT_TRUE(again->vanished.contains(3));
    ASSERT_EQ(again->changed.size(), 1u);
    EXPECT_EQ(again->changed[0].uid, 2u);
    EXPECT_TRUE(again->changed[0].has_flag(imap::flag::seen));
}

TEST(ImapClient, PipelinedFromManyTasks) {
    auto mail = standard_mail();
    for (int i = 0; i < 50; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i)));
    }
    Server s(mail);
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->select("INBOX"));
    // independent commands at once: statuses, searches and fetches of
    // single messages, their answers each to its own
    auto run = [](imap::client cl) -> async::task<int> {
        int ok = 0;
        std::vector<async::task<expected<imap::status, io::error>>> st;
        for (int i = 0; i < 10; ++i) {
            st.push_back(async::spawn(cl.async_status("INBOX")));
        }
        for (auto& t : st) {
            auto r = co_await t;
            ok += r && r->messages == 50;
        }
        for (uint32_t uid = 1; uid <= 20; ++uid) {
            auto r = co_await cl.async_fetch(uid);
            ok += r && r->size() == 1 && (*r)[0].uid == uid;
        }
        co_return ok;
    };
    EXPECT_EQ(run(*c).wait(), 30);
}

TEST(ImapClient, CompressAndId) {
    auto mail = standard_mail();
    for (int i = 0; i < 10; ++i) {
        mail.append("alice", "INBOX", message("m" + std::to_string(i), std::string(5000, 'q') + "\r\n"));
    }
    Server s(mail);
    auto o = plain();
    o.compress = true;
    o.id = {{sgcl::string("name"), sgcl::string("tests")}};
    auto c = imap::client::connect(sgcl::string(s.address()), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->is_compressed());
    auto id = c->server_id();
    ASSERT_EQ(id.size(), 1u);
    EXPECT_EQ(text(id[0].second), "sgcl");
    auto unseen = c->fetch_unseen();
    ASSERT_TRUE(unseen) << text(unseen.error().message());
    ASSERT_EQ(unseen->size(), 10u);
    EXPECT_EQ((*unseen)[9].text().size(), msg_text("m9", std::string(5000, 'q') + "\r\n").size());
    EXPECT_TRUE((*unseen)[0].envelope);
    // the parsed message (encoding::email)
    auto parsed = (*unseen)[0].email();
    ASSERT_TRUE(parsed);
    EXPECT_EQ(text(parsed->subject()), text((*unseen)[0].envelope->subject));
    // a message whose whole was not fetched parses as an empty one
    auto empty = imap::message().email();
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->subject().empty());
    // still unseen: the fetch peeked
    auto n = c->count(imap::criteria::unseen());
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 10u);
}

TEST(ImapClient, ClosedClient) {
    Server s(standard_mail());
    auto c = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->close());
    EXPECT_TRUE(c->is_closed());
    auto r = c->noop();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed());
    // the server going away: the next command fails
    auto c2 = imap::client::connect(sgcl::string(s.address()), plain());
    ASSERT_TRUE(c2);
    s.srv.close();
    auto r2 = c2->noop();
    EXPECT_FALSE(r2);
}

TEST(ImapClient, AsyncForms) {
    Server s(standard_mail());
    auto flow = [](std::string address) -> async::task<std::string> {
        imap::client::options o;
        o.security = imap::security::none;
        o.user = "alice";
        o.password = "secret";
        auto c = co_await imap::client::async_connect(sgcl::string(address), o);
        if (!c) {
            co_return "connect";
        }
        if (!co_await c->async_create("Box")) {
            co_return "create";
        }
        auto uid = co_await c->async_append("Box", "Subject: a\r\n\r\nb\r\n");
        if (!uid) {
            co_return "append";
        }
        auto sel = co_await c->async_select("Box");
        if (!sel || sel->exists != 1) {
            co_return "select";
        }
        auto f = co_await c->async_search(imap::criteria::subject("a"));
        if (!f || f->size() != 1) {
            co_return "search";
        }
        auto m = co_await c->async_fetch_message((*f)[0]);
        if (!m) {
            co_return "fetch";
        }
        if (!co_await c->async_logout()) {
            co_return "logout";
        }
        co_return text(*m);
    };
    EXPECT_EQ(flow(s.address()).wait(), "Subject: a\r\n\r\nb\r\n");
}
