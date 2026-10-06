//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client against OpenLDAP's own server: slapd (macOS ships it as
// /usr/libexec/slapd, Debian as /usr/sbin/slapd) started here on a port of
// the loopback over a directory of its ldif backend in a temporary
// directory, the root DN cn=admin,dc=example,dc=com. Every operation of the
// client against an implementation it was not written beside; skipped where
// there is no slapd.
#include "server.h"

#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

using namespace ldap_test;
namespace ldap = sgcl::net::ldap;

namespace {
    // "secret" as slappasswd writes it: a server may refuse a password of
    // clear text in its configuration and its entries (Apple's does)
    constexpr const char* Secret = "{SSHA}r8pO/3FjAPRxOr45AY8gLXKnCQ3Kvlvn";

    const char* slapd_path() {
        for (const char* p : {"/usr/libexec/slapd", "/usr/sbin/slapd"}) {
            if (std::filesystem::exists(p)) {
                return p;
            }
        }
        return nullptr;
    }

    std::string schema_dir() {
        for (const char* p : {"/etc/openldap/schema", "/etc/ldap/schema"}) {
            if (std::filesystem::exists(std::string(p) + "/inetorgperson.schema")) {
                return p;
            }
        }
        return {};
    }

    struct Slapd {
        pid_t pid = 0;
        uint16_t port = 0;
        std::filesystem::path dir;

        bool start() {
            const char* bin = slapd_path();
            std::string schema = schema_dir();
            if (!bin || schema.empty()) {
                return false;
            }
            dir = std::filesystem::temp_directory_path() / ("sgcl-slapd-" + std::to_string(::getpid()));
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir / "db");
            {
                std::ofstream conf(dir / "slapd.conf");
                conf << "include " << schema << "/core.schema\n"
                     << "include " << schema << "/cosine.schema\n"
                     << "include " << schema << "/inetorgperson.schema\n"
                     << "include " << schema << "/nis.schema\n"
                     << "pidfile " << (dir / "slapd.pid").string() << "\n"
                     << "argsfile " << (dir / "slapd.args").string() << "\n"
                     << "access to attrs=userPassword by self write by anonymous auth by * none\n"
                     << "access to * by * read\n"
                     << "database ldif\n"
                     << "suffix \"dc=example,dc=com\"\n"
                     << "rootdn \"cn=admin,dc=example,dc=com\"\n"
                     << "rootpw " << Secret << "\n"
                     << "directory " << (dir / "db").string() << "\n";
            }
            {
                auto l = net::tcp::listen("127.0.0.1:0").value();   // a free port, given back
                port = l.local_endpoint().port();
                (void)l.close();
            }
            std::string conf = (dir / "slapd.conf").string();
            std::string url = "ldap://127.0.0.1:" + std::to_string(port) + "/";
            std::vector<char*> argv{const_cast<char*>(bin), const_cast<char*>("-f"), conf.data(), const_cast<char*>("-h"), url.data(),
                                    const_cast<char*>("-d"), const_cast<char*>("0"), nullptr};
            posix_spawn_file_actions_t fa;
            posix_spawn_file_actions_init(&fa);
            posix_spawn_file_actions_addopen(&fa, 1, (dir / "slapd.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            posix_spawn_file_actions_adddup2(&fa, 1, 2);
            int rc = posix_spawn(&pid, bin, &fa, nullptr, argv.data(), environ);
            posix_spawn_file_actions_destroy(&fa);
            if (rc != 0) {
                pid = 0;
                return false;
            }
            for (int i = 0; i < 100; ++i) {   // up to 5 s for the port to answer
                if (net::tcp::connect(sgcl::string("127.0.0.1:" + std::to_string(port)))) {
                    return true;
                }
                ::usleep(50000);
            }
            return false;
        }

        std::string url() const {
            return "ldap://127.0.0.1:" + std::to_string(port);
        }

        ~Slapd() {
            if (pid > 0) {
                ::kill(pid, SIGTERM);
                int status = 0;
                ::waitpid(pid, &status, 0);
            }
            if (!dir.empty()) {
                std::error_code e;
                std::filesystem::remove_all(dir, e);
            }
        }
    };

    ldap::client::options plain() {
        ldap::client::options o;
        o.security = ldap::security::none;
        return o;
    }

    ldap::entry person(const std::string& uid, const std::string& cn, const std::string& sn, const std::string& mail, int number) {
        ldap::entry e;
        e.dn = sgcl::string("uid=" + uid + ",ou=people,dc=example,dc=com");
        e.attributes = {{"objectClass", {"inetOrgPerson", "posixAccount"}},
                        {"uid", {sgcl::string(uid)}},
                        {"cn", {sgcl::string(cn)}},
                        {"sn", {sgcl::string(sn)}},
                        {"mail", {sgcl::string(mail)}},
                        {"uidNumber", {sgcl::string(std::to_string(number))}},
                        {"gidNumber", {"100"}},
                        {"homeDirectory", {sgcl::string("/home/" + uid)}},
                        {"userPassword", {Secret}}};
        return e;
    }

    std::set<std::string> uids(const ldap::search_result& r) {
        std::set<std::string> out;
        for (auto& e : r.entries) {
            out.insert(str(e.get("uid")));
        }
        return out;
    }
}

TEST(LdapSlapd, EveryOperation) {
    Slapd d;
    if (!d.start()) {
        GTEST_SKIP() << "no slapd to start";
    }
    auto c = ldap::client::connect(sgcl::string(d.url()), plain()).value();
    // anonymous first, then the root DN
    EXPECT_EQ(str(c.who_am_i().value()), "");
    auto wrong = c.bind("cn=admin,dc=example,dc=com", "nope");
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), ldap::errc::invalid_credentials);
    ASSERT_TRUE(c.bind("cn=admin,dc=example,dc=com", "secret"));
    EXPECT_EQ(str(c.who_am_i().value()), "dn:cn=admin,dc=example,dc=com");
    // the tree
    ldap::entry root;
    root.dn = "dc=example,dc=com";
    root.attributes = {{"objectClass", {"dcObject", "organization"}}, {"dc", {"example"}}, {"o", {"Example"}}};
    ASSERT_TRUE(c.add(root));
    ldap::entry people;
    people.dn = "ou=people,dc=example,dc=com";
    people.attributes = {{"objectClass", {"organizationalUnit"}}, {"ou", {"people"}}};
    ASSERT_TRUE(c.add(people));
    ASSERT_TRUE(c.add(person("alice", "Alice Liddell", "Liddell", "alice@example.com", 1001)));
    ASSERT_TRUE(c.add(person("bob", "Bob Builder", "Builder", "bob@example.com", 1002)));
    ASSERT_TRUE(c.add(person("carol", "Carol Danvers", "Danvers", "carol@example.org", 1003)));
    ASSERT_TRUE(c.add(person("dave", "Dave Bowman", "Bowman", "dave@example.org", 1004)));
    auto again = c.add(person("bob", "Bob", "B", "b@example.com", 1));
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), ldap::errc::entry_already_exists);
    // filters of every kind slapd evaluates
    auto base = sgcl::string("ou=people,dc=example,dc=com");
    auto find = [&](const char* filter) { return uids(c.search(base, sgcl::string(filter)).value()); };
    EXPECT_EQ(find("(uid=*)"), (std::set<std::string>{"alice", "bob", "carol", "dave"}));
    EXPECT_EQ(find("(mail=*@example.org)"), (std::set<std::string>{"carol", "dave"}));
    EXPECT_EQ(find("(cn=*li*)"), (std::set<std::string>{"alice"}));
    EXPECT_EQ(find("(&(objectClass=posixAccount)(uidNumber>=1003))"), (std::set<std::string>{"carol", "dave"}));
    EXPECT_EQ(find("(uidNumber<=1001)"), (std::set<std::string>{"alice"}));
    EXPECT_EQ(find("(|(sn=Builder)(sn=Bowman))"), (std::set<std::string>{"bob", "dave"}));
    EXPECT_EQ(find("(&(uid=*)(!(mail=*@example.com)))"), (std::set<std::string>{"carol", "dave"}));
    EXPECT_EQ(find("(cn:caseExactMatch:=Bob Builder)"), (std::set<std::string>{"bob"}));
    EXPECT_EQ(find("(cn=ALICE LIDDELL)"), (std::set<std::string>{"alice"}));
    std::string escaped = "(cn=" + str(ldap::filter_escape("Carol*")) + ")";   // a star of the user's text matches a star
    EXPECT_EQ(find(escaped.c_str()), (std::set<std::string>{}));
    // the attributes asked, the scopes, types only
    ldap::search_request q;
    q.base = base;
    q.filter = "(uid=alice)";
    q.attributes = {"mail", "cn"};
    auto one = c.search(q).value();
    ASSERT_EQ(one.entries.size(), 1u);
    EXPECT_EQ(lower(str(one.entries[0].dn)), "uid=alice,ou=people,dc=example,dc=com");
    EXPECT_EQ(str(one.entries[0].get("mail")), "alice@example.com");
    EXPECT_TRUE(one.entries[0].get_all("sn").empty());
    q.filter = "(objectClass=*)";
    q.attributes = {};
    q.scope = ldap::scope::base;
    EXPECT_EQ(c.search(q)->entries.size(), 1u);
    q.scope = ldap::scope::one;
    EXPECT_EQ(c.search(q)->entries.size(), 4u);
    q.types_only = true;
    auto types = c.search(q).value();
    EXPECT_TRUE(types.entries[0].get_all("uid").empty() && !types.entries[0].attributes.empty());
    q.types_only = false;
    // paged results (RFC 2696): pages of 3 until the last, every entry once
    q.scope = ldap::scope::subtree;
    q.base = "dc=example,dc=com";
    q.page_size = 3;
    auto paged = c.search(q).value();
    EXPECT_EQ(paged.entries.size(), 6u);
    EXPECT_FALSE(paged.truncated);
    // the size limit: the entries until it
    q.page_size = 0;
    q.size_limit = 2;
    auto limited = c.search(q).value();
    EXPECT_TRUE(limited.truncated);
    EXPECT_EQ(limited.entries.size(), 2u);
    q.size_limit = 0;
    // a search of a base that is not there
    auto missing = c.search("ou=nobody,dc=example,dc=com", "(objectClass=*)");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ldap::errc::no_such_object);
    EXPECT_EQ(ldap::result_of(missing.error())->code, 32);
    // compare
    EXPECT_TRUE(c.compare("uid=alice,ou=people,dc=example,dc=com", "sn", "liddell").value());
    EXPECT_FALSE(c.compare("uid=alice,ou=people,dc=example,dc=com", "sn", "Builder").value());
    auto no_attr = c.compare("uid=alice,ou=people,dc=example,dc=com", "title", "x");
    EXPECT_TRUE(!no_attr || !*no_attr);
    // modify: add, replace, remove a value, remove all, increment (RFC 4525), all or none
    ASSERT_TRUE(c.modify("uid=alice,ou=people,dc=example,dc=com",
                         {{ldap::modify_op::add, "telephoneNumber", {"+1 555 0100", "+1 555 0101"}},
                          {ldap::modify_op::replace, "mail", {"alice@wonderland.example"}},
                          {ldap::modify_op::remove, "telephoneNumber", {"+1 555 0100"}},
                          {ldap::modify_op::increment, "uidNumber", {"10"}}}));
    auto alice = c.search(base, "(uid=alice)").value().entries[0];
    EXPECT_EQ(str(alice.get("mail")), "alice@wonderland.example");
    EXPECT_EQ(alice.get_all("telephoneNumber").size(), 1u);
    EXPECT_EQ(str(alice.get("telephoneNumber")), "+1 555 0101");
    EXPECT_EQ(str(alice.get("uidNumber")), "1011");
    auto bad = c.modify("uid=alice,ou=people,dc=example,dc=com",
                        {{ldap::modify_op::replace, "mail", {"never@example.com"}}, {ldap::modify_op::remove, "title", {}}});
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), ldap::errc::no_such_attribute);
    EXPECT_EQ(str(c.search(base, "(uid=alice)").value().entries[0].get("mail")), "alice@wonderland.example");
    // rename: the RDN, then moved under another superior
    ldap::entry staff;
    staff.dn = "ou=staff,dc=example,dc=com";
    staff.attributes = {{"objectClass", {"organizationalUnit"}}, {"ou", {"staff"}}};
    ASSERT_TRUE(c.add(staff));
    ASSERT_TRUE(c.rename("uid=dave,ou=people,dc=example,dc=com", "uid=david"));
    auto david = c.search(base, "(uid=david)").value();
    ASSERT_EQ(david.entries.size(), 1u);
    EXPECT_TRUE(david.entries[0].get_all("uid").size() == 1);   // the old RDN's value dropped
    ASSERT_TRUE(c.rename("uid=david,ou=people,dc=example,dc=com", "uid=david", true, "ou=staff,dc=example,dc=com"));
    EXPECT_EQ(c.search("ou=staff,dc=example,dc=com", "(uid=david)").value().entries.size(), 1u);
    // remove: a leaf, not a parent
    auto parent = c.remove("ou=people,dc=example,dc=com");
    ASSERT_FALSE(parent);
    EXPECT_EQ(parent.error().code(), ldap::errc::not_allowed_on_non_leaf);
    ASSERT_TRUE(c.remove("uid=carol,ou=people,dc=example,dc=com"));
    EXPECT_EQ(find("(uid=*)"), (std::set<std::string>{"alice", "bob"}));
    // Who am I? as an extended operation of its own
    auto ext = c.extended("1.3.6.1.4.1.4203.1.11.3").value();
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(ext.second.data()), ext.second.size()), "dn:cn=admin,dc=example,dc=com");
    // a user of the tree binds with its password
    ASSERT_TRUE(c.bind("uid=bob,ou=people,dc=example,dc=com", "secret"));
    EXPECT_EQ(lower(str(c.who_am_i().value())), "dn:uid=bob,ou=people,dc=example,dc=com");
    // many at once over one connection, each answered as its own
    vector<async::task<bool>> all;
    for (int i = 0; i < 40; ++i) {
        all.push_back(async::spawn([](ldap::client c, std::string uid) -> async::task<bool> {
            auto r = co_await c.async_search(sgcl::string("ou=people,dc=example,dc=com"), sgcl::string("(uid=" + uid + ")"));
            co_return r && r->entries.size() == 1 && str(r->entries[0].get("uid")) == uid;
        }(c, i % 2 ? "alice" : "bob")));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
    // StartTLS where the server has no TLS: refused, the session goes on in clear text
    EXPECT_FALSE(c.start_tls());
    EXPECT_TRUE(c.who_am_i());
    EXPECT_TRUE(c.unbind());
}
