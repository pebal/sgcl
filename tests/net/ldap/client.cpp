//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ldap's client against the test server (tests/net/ldap/server.h):
// every operation and its errors, the binds, StartTLS and ldaps, Who am I?,
// search with each filter, scope, limit and page, references and
// referrals, the Notice of Disconnection, operations at once, the
// boundaries; RFC 4515's filters read to their BER (§4's examples).
#include "server.h"

#include <set>

using namespace ldap_test;
namespace ldap = sgcl::net::ldap;

namespace {
    ldap::client::options plain() {
        ldap::client::options o;
        o.security = ldap::security::none;
        return o;
    }

    net::tls::config client_tls() {
        net::tls::config tls;
        tls.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        tls.server_name = sgcl::string("localhost");
        return tls;
    }

    std::set<std::string> dns(const ldap::search_result& r) {
        std::set<std::string> out;
        for (auto& e : r.entries) {
            out.insert(lower(str(e.dn)));
        }
        return out;
    }

    std::set<std::string> uids(const ldap::search_result& r) {
        std::set<std::string> out;
        for (auto& e : r.entries) {
            out.insert(str(e.get("uid")));
        }
        return out;
    }

    std::string hex(slice<const byte> b) {
        static constexpr char d[] = "0123456789abcdef";
        std::string out;
        for (auto x : b) {
            out += d[uint8_t(x) >> 4];
            out += d[uint8_t(x) & 15];
        }
        return out;
    }
}

TEST(LdapFilter, Rfc4515Examples) {
    for (const char* f : {"(cn=Babs Jensen)", "(!(cn=Tim Howes))", "(&(objectClass=Person)(|(sn=Jensen)(cn=Babs J*)))", "(o=univ*of*mich*)", "(seeAlso=)",
                          "(cn:caseExactMatch:=Fred Flintstone)", "(cn:=Betty Rubble)", "(sn:dn:2.4.6.8.10:=Barney Rubble)", "(o:dn:=Ace Industry)", "(:1.2.3:=Wilma Flintstone)",
                          "(:DN:2.4.6.8.10:=Dino)", "(o=Parens R Us \\28for all your parenthetical needs\\29)", "(cn=*\\2A*)", "(filename=C:\\5cMyFile)",
                          "(bin=\\00\\00\\00\\04)", "(sn=Lu\\c4\\8di\\c4\\87)", "(1.3.6.1.4.1.1466.0=\\04\\02\\48\\69)", "(&)", "(|)", "(age>=30)", "(age<=30)",
                          "(cn~=Babs)", "(cn=*)", "cn=no parens"}) {
        EXPECT_TRUE(ld::ldap_filter(f)) << f;
    }
    for (const char* f : {"", "(", "()", "(cn=a", "(cn=a))", "(cn)", "(=a)", "(cn=a(b)", "(cn=\\zz)", "(cn=\\4)", "(cn**=a)", "(!cn=a)", "(:=a)", "(c n=a)", "(cn=a**b)"}) {
        EXPECT_FALSE(ld::ldap_filter(f)) << f;
    }
    // the bytes RFC 4511 gives these: equality [3], present [7], substrings [4]
    EXPECT_EQ(hex(ld::ldap_filter("(cn=a)").bytes()), "a3070402636e040161");
    EXPECT_EQ(hex(ld::ldap_filter("(cn=*)").bytes()), "8702636e");
    EXPECT_EQ(hex(ld::ldap_filter("(cn=a*b*c)").bytes()), "a40f0402636e3009800161810162820163");
    EXPECT_EQ(hex(ld::ldap_filter("(!(cn=a))").bytes()), "a209a3070402636e040161");
    EXPECT_EQ(str(ldap::filter_escape("a*b(c)\\d")), "a\\2ab\\28c\\29\\5cd");
    EXPECT_TRUE(ld::ldap_filter(std::string("(cn=") + str(ldap::filter_escape("x*y)(z")) + ")"));
}

TEST(LdapClient, BindAndWhoAmI) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain());
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_EQ(str(c->who_am_i().value()), "");
    auto bad = c->bind("cn=admin,dc=example,dc=com", "wrong");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), ldap::errc::invalid_credentials);
    auto res = ldap::result_of(bad.error());
    ASSERT_TRUE(res);
    EXPECT_EQ(res->code, 49);
    EXPECT_EQ(str(res->message), "invalid credentials");
    ASSERT_TRUE(c->bind("cn=admin,dc=example,dc=com", "secret"));
    EXPECT_EQ(str(c->who_am_i().value()), "dn:cn=admin,dc=example,dc=com");
    ASSERT_TRUE(c->bind_plain("alice", "alice-pw"));
    EXPECT_EQ(str(c->who_am_i().value()), "u:alice");
    EXPECT_EQ(c->bind_plain("alice", "nope").error().code(), ldap::errc::invalid_credentials);
    EXPECT_EQ(c->bind_external().error().code(), ldap::errc::inappropriate_authentication);   // no TLS
    ASSERT_TRUE(c->bind("", ""));
    EXPECT_EQ(str(c->who_am_i().value()), "");
    // a DN without a password: refused here, never sent as an unauthenticated bind
    auto empty = c->bind("cn=admin,dc=example,dc=com", "");
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code(), ldap::errc::invalid_credentials);
    EXPECT_EQ(str(c->who_am_i().value()), "");
    // credentials in the URL
    auto u = ldap::client::connect(sgcl::string("ldap://cn=admin,dc=example,dc=com:secret@127.0.0.1:" + std::to_string(s.port())), plain());
    ASSERT_TRUE(u) << str(u.error().message());
    EXPECT_EQ(str(u->who_am_i().value()), "dn:cn=admin,dc=example,dc=com");
    ASSERT_TRUE(c->unbind());
    EXPECT_FALSE(c->who_am_i());
}

TEST(LdapClient, Search) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain()).value();
    auto all = c.search("ou=people,dc=example,dc=com", "(objectClass=person)");
    ASSERT_TRUE(all) << str(all.error().message());
    EXPECT_EQ(uids(*all), (std::set<std::string>{"alice", "bob", "carol", "dave", "eve"}));
    auto smiths = c.search("dc=example,dc=com", "(&(objectClass=person)(sn=Smith))", {"cn", "mail"});
    ASSERT_TRUE(smiths);
    ASSERT_EQ(smiths->entries.size(), 2u);
    for (auto& e : smiths->entries) {
        EXPECT_EQ(e.attributes.size(), 2u);
        EXPECT_TRUE(str(e.get("MAIL")).ends_with("@example.com"));
        EXPECT_TRUE(e.get("uid").empty());
    }
    EXPECT_EQ(smiths->referrals.size(), 1u);   // the subtree of dc=example,dc=com has a reference
    EXPECT_EQ(str(smiths->referrals[0]), "ldap://replica.example.com/dc=example,dc=com");
    auto filter = [&](const char* f) {
        auto r = c.search("ou=people,dc=example,dc=com", f);
        EXPECT_TRUE(r) << f;
        return r ? uids(*r) : std::set<std::string>();
    };
    EXPECT_EQ(filter("(|(uid=alice)(uid=bob))"), (std::set<std::string>{"alice", "bob"}));
    EXPECT_EQ(filter("(&(sn=Smith)(!(uid=carol)))"), (std::set<std::string>{"alice"}));
    EXPECT_EQ(filter("(cn=*o*)"), (std::set<std::string>{"bob", "carol", "dave"}));
    EXPECT_EQ(filter("(cn=A*th)"), (std::set<std::string>{"alice"}));
    EXPECT_EQ(filter("(age>=35)"), (std::set<std::string>{"carol", "dave"}));
    EXPECT_EQ(filter("(age<=28)"), (std::set<std::string>{"bob", "eve"}));
    EXPECT_EQ(filter("(givenName~=EVE)"), (std::set<std::string>{"eve"}));
    EXPECT_EQ(filter("(givenName:caseExactMatch:=eve)"), (std::set<std::string>{}));
    EXPECT_EQ(filter("(givenName:caseExactMatch:=Eve)"), (std::set<std::string>{"eve"}));
    EXPECT_EQ(filter("(:caseIgnoreMatch:=Brown)"), (std::set<std::string>{"dave"}));
    EXPECT_EQ(filter("(&(objectClass=person)(mail=*))").size(), 5u);
    EXPECT_EQ(filter("(&)").size(), 6u);   // absolute true: the ou too
    EXPECT_TRUE(filter("(|)").empty());
    // scopes
    ldap::search_request r;
    r.base = "ou=people,dc=example,dc=com";
    r.scope = ldap::scope::base;
    EXPECT_EQ(c.search(r)->entries.size(), 1u);
    r.scope = ldap::scope::one;
    EXPECT_EQ(c.search(r)->entries.size(), 5u);
    r.scope = ldap::scope::subtree;
    EXPECT_EQ(c.search(r)->entries.size(), 6u);
    // types only, a size limit
    r.types_only = true;
    r.filter = "(uid=alice)";
    auto t = c.search(r);
    ASSERT_EQ(t->entries.size(), 1u);
    EXPECT_TRUE(t->entries[0].get_all("cn").empty());
    EXPECT_FALSE(t->entries[0].attributes.empty());
    r.types_only = false;
    r.filter = "(objectClass=*)";
    r.size_limit = 2;
    auto limited = c.search(r);
    ASSERT_TRUE(limited) << str(limited.error().message());
    EXPECT_TRUE(limited->truncated);   // the entries until the limit
    EXPECT_EQ(limited->entries.size(), 2u);
    r.size_limit = 0;
    EXPECT_FALSE(c.search(r)->truncated);
    // no such base: the matched DN in the result
    auto missing = c.search("uid=zed,ou=people,dc=example,dc=com", "(objectClass=*)");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ldap::errc::no_such_object);
    EXPECT_EQ(str(ldap::result_of(missing.error())->matched_dn), "ou=people,dc=example,dc=com");
    // a referral: reported with its URL, not followed
    auto ref = c.search("ou=elsewhere,dc=example,dc=com", "(objectClass=*)");
    ASSERT_FALSE(ref);
    EXPECT_EQ(ref.error().code(), ldap::errc::referral);
    ASSERT_EQ(ldap::result_of(ref.error())->referrals.size(), 1u);
    EXPECT_EQ(str(ldap::result_of(ref.error())->referrals[0]), "ldap://other.example.com/ou=elsewhere,dc=example,dc=com");
    // a filter that is none: refused before anything is sent
    int before = s.dir.searches.load();
    auto badf = c.search("dc=example,dc=com", "(cn=a");
    ASSERT_FALSE(badf);
    EXPECT_EQ(badf.error().code(), ldap::errc::invalid_filter);
    EXPECT_EQ(s.dir.searches.load(), before);
}

TEST(LdapClient, PagedResults) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain()).value();
    ldap::search_request r;
    r.base = "ou=people,dc=example,dc=com";
    r.scope = ldap::scope::one;
    r.page_size = 2;
    int before = s.dir.searches.load();
    auto out = c.search(r);
    ASSERT_TRUE(out) << str(out.error().message());
    EXPECT_EQ(uids(*out).size(), 5u);
    EXPECT_EQ(s.dir.searches.load() - before, 3);   // pages of 2, 2, 1
}

TEST(LdapClient, Updates) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain()).value();
    ldap::entry e;
    e.dn = "uid=frank,ou=people,dc=example,dc=com";
    e.attributes = {{"objectClass", {"person"}}, {"uid", {"frank"}}, {"cn", {"Frank Ocean"}}, {"age", {"40"}}, {"jpegPhoto", {sgcl::string(std::string("\x00\xff\x10", 3))}}};
    ASSERT_TRUE(c.add(e));
    EXPECT_EQ(c.add(e).error().code(), ldap::errc::entry_already_exists);
    ldap::entry orphan;
    orphan.dn = "uid=x,ou=nowhere,dc=example,dc=com";
    orphan.attributes = {{"uid", {"x"}}};
    EXPECT_EQ(c.add(orphan).error().code(), ldap::errc::no_such_object);
    auto got = c.search(e.dn, "(objectClass=*)");
    ASSERT_EQ(got->entries.size(), 1u);
    EXPECT_EQ(str(got->entries[0].get("jpegPhoto")), std::string("\x00\xff\x10", 3));
    ASSERT_TRUE(c.modify(e.dn, {{ldap::modify_op::add, "mail", {"frank@example.com", "f@example.com"}},
                                {ldap::modify_op::replace, "cn", {"Frank O."}},
                                {ldap::modify_op::increment, "age", {"2"}},
                                {ldap::modify_op::remove, "mail", {"f@example.com"}}}));
    auto m = c.search(e.dn, "(objectClass=*)")->entries[0];
    EXPECT_EQ(str(m.get("cn")), "Frank O.");
    EXPECT_EQ(str(m.get("age")), "42");
    EXPECT_EQ(m.get_all("mail").size(), 1u);
    EXPECT_EQ(c.modify(e.dn, {{ldap::modify_op::add, "mail", {"frank@example.com"}}}).error().code(), ldap::errc::attribute_or_value_exists);
    EXPECT_EQ(c.modify("uid=nobody,dc=example,dc=com", {}).error().code(), ldap::errc::no_such_object);
    EXPECT_TRUE(c.compare(e.dn, "cn", "frank o.").value());
    EXPECT_FALSE(c.compare(e.dn, "cn", "Someone").value());
    EXPECT_EQ(c.compare(e.dn, "nothere", "x").error().code(), ldap::errc::no_such_attribute);
    ASSERT_TRUE(c.rename(e.dn, "uid=franklin"));
    EXPECT_FALSE(c.search(e.dn, "(objectClass=*)"));
    auto renamed = c.search("uid=franklin,ou=people,dc=example,dc=com", "(objectClass=*)");
    ASSERT_TRUE(renamed);
    EXPECT_EQ(renamed->entries[0].get_all("uid").size(), 1u);   // the old RDN's value deleted
    ASSERT_TRUE(c.rename("uid=franklin,ou=people,dc=example,dc=com", "uid=frank", false, "ou=groups,dc=example,dc=com"));
    auto moved = c.search("uid=frank,ou=groups,dc=example,dc=com", "(objectClass=*)");
    ASSERT_TRUE(moved);
    EXPECT_EQ(moved->entries[0].get_all("uid").size(), 2u);   // kept: franklin and frank
    EXPECT_EQ(c.remove("ou=people,dc=example,dc=com").error().code(), ldap::errc::not_allowed_on_non_leaf);
    ASSERT_TRUE(c.remove("uid=frank,ou=groups,dc=example,dc=com"));
    EXPECT_EQ(c.remove("uid=frank,ou=groups,dc=example,dc=com").error().code(), ldap::errc::no_such_object);
    // an extended operation the server does not know
    auto ext = c.extended("1.2.3.4.5");
    ASSERT_FALSE(ext);
    EXPECT_EQ(ext.error().code(), ldap::errc::protocol_error);
}

TEST(LdapClient, Tls) {
    Server s;
    ldap::client::options o;
    o.tls = client_tls();   // security automatic: StartTLS
    auto c = ldap::client::connect(s.url("ldap", "localhost"), o);
    ASSERT_TRUE(c) << str(c.error().message());
    EXPECT_TRUE(c->is_tls());
    ASSERT_TRUE(c->bind_external());
    EXPECT_EQ(str(c->who_am_i().value()), "dn:cn=tls-client");
    EXPECT_EQ(c->start_tls(client_tls()).error().code(), ldap::errc::operations_error);   // already
    EXPECT_EQ(c->search("dc=example,dc=com", "(uid=bob)")->entries.size(), 1u);
    // start_tls() on a session of none
    auto later = ldap::client::connect(s.url("ldap", "localhost"), plain()).value();
    EXPECT_FALSE(later.is_tls());
    ASSERT_TRUE(later.start_tls(client_tls()));
    EXPECT_TRUE(later.is_tls());
    EXPECT_TRUE(later.bind("cn=admin,dc=example,dc=com", "secret"));
    // ldaps://
    Server secure(true);
    auto t = ldap::client::connect(secure.url("ldaps", "localhost"), o);
    ASSERT_TRUE(t) << str(t.error().message());
    EXPECT_TRUE(t->is_tls());
    EXPECT_EQ(t->search("ou=people,dc=example,dc=com", "(uid=*)")->entries.size(), 5u);
}

TEST(LdapClient, ManyOperationsAtOnce) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain()).value();
    auto one = [](ldap::client c, std::string uid) -> async::task<bool> {
        auto r = co_await c.async_search(sgcl::string("ou=people,dc=example,dc=com"), sgcl::string("(uid=" + uid + ")"));
        co_return r && r->entries.size() == 1 && str(r->entries[0].get("uid")) == uid;
    };
    vector<async::task<bool>> all;
    for (int i = 0; i < 50; ++i) {
        all.push_back(async::spawn(one(c, std::vector<std::string>{"alice", "bob", "carol", "dave", "eve"}[size_t(i % 5)])));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
}

// The operation that reads for itself gives the others their answers while
// it waits; when it times out, the reading passes on, and when no one reads
// the next operation reads
TEST(LdapClient, ReadingPassesOn) {
    Server s;
    ldap::client::options o = plain();
    o.timeout = std::chrono::milliseconds(800);
    auto c = ldap::client::connect(s.url(), o).value();
    auto search = [](ldap::client c, std::string base, std::string filter) -> async::task<expected<ldap::search_result, io::error>> {
        co_return co_await c.async_search(sgcl::string(base), sgcl::string(filter));
    };
    auto start = sgcl::clock::now();
    auto never = async::spawn(search(c, "ou=never,dc=example,dc=com", "(objectClass=*)"));
    async::spawn([]() -> async::task<> { co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(50)); }()).wait();
    auto quick = search(c, "ou=people,dc=example,dc=com", "(uid=alice)").wait();
    ASSERT_TRUE(quick) << str(quick.error().message());
    EXPECT_EQ(quick->entries.size(), 1u);
    EXPECT_LT(sgcl::clock::now() - start, std::chrono::milliseconds(700));   // answered before the first one's timeout
    auto late = never.wait();
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    // many at once, one of them never answered: the others all answered, the reading passed on
    vector<async::task<expected<ldap::search_result, io::error>>> all;
    for (int i = 0; i < 30; ++i) {
        all.push_back(async::spawn(search(c, i == 3 ? "ou=never,dc=example,dc=com" : "ou=people,dc=example,dc=com", "(uid=bob)")));
    }
    for (int i = 0; i < 30; ++i) {
        auto r = all[size_t(i)].wait();
        if (i == 3) {
            EXPECT_TRUE(!r && r.error().is_timeout());
        } else {
            EXPECT_TRUE(r && r->entries.size() == 1u);
        }
    }
    // after the reader task: one at a time again
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(c.who_am_i());
    }
}

TEST(LdapClient, NoticeOfDisconnectionAndBoundaries) {
    Server s;
    auto c = ldap::client::connect(s.url(), plain()).value();
    s.disconnect_next_search = true;
    auto r = c.search("dc=example,dc=com", "(objectClass=*)");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), ldap::errc::unavailable);
    EXPECT_FALSE(c.who_am_i());
    ldap::client none;
    EXPECT_FALSE(none);
    auto copy = ldap::client::connect(s.url(), plain()).value();
    auto same = copy;
    EXPECT_TRUE(same == copy);
    EXPECT_FALSE(ldap::client::connect("ldap://", plain()));
    EXPECT_FALSE(ldap::client::connect("127.0.0.1:1", plain()));
    EXPECT_EQ(ldap::client::connect("127.0.0.1:99999", plain()).error().code(), net::errc::invalid_address);
    // a server without StartTLS: automatic refuses to go on in clear text
    auto refused = ldap::client::connect(s.url("ldap", "127.0.0.1"), [] {
        ldap::client::options o;
        o.security = ldap::security::starttls;
        o.tls.server_name = "nope.invalid";
        return o;
    }());
    EXPECT_FALSE(refused);
    EXPECT_TRUE(copy.close());
    EXPECT_FALSE(copy.unbind());
    // result_of of other errors, the category
    EXPECT_FALSE(ldap::result_of(io::error(io::errc::closed, "x", sgcl::string())));
    EXPECT_FALSE(ldap::result_of(ld::ldap_error(ldap::errc::invalid_filter, "x")));
    error_code e = ldap::errc::no_such_object;
    EXPECT_EQ(std::string(e.category().name()), "ldap");
    EXPECT_EQ(e.message(), "no such object");
    // a timeout of an operation the server never answers: none here answers late, so a search of a
    // server that is no LDAP server
    auto l = net::tcp::listen("127.0.0.1:0").value();
    auto quiet = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            char b[256];
            (void)co_await c->async_read(slice<byte>(reinterpret_cast<byte*>(b), sizeof b));
            co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(600));
            (void)c->close();
        }
    }(l));
    ldap::client::options fast = plain();
    fast.timeout = std::chrono::milliseconds(200);
    auto q = ldap::client::connect(l.local_endpoint().to_string(), fast).value();
    auto late = q.who_am_i();
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_timeout());
    quiet.wait();
}
