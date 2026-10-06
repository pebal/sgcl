//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// OpenLDAP's command line clients (/usr/bin/ldapsearch, ldapwhoami,
// ldapadd, ldapmodify, ldapcompare, ldapmodrdn, ldapdelete) against the
// test server: what they send is read by encoding::asn1 and answered, what
// it answers is read by OpenLDAP's own BER library — the server the
// client's tests stand on, checked by an implementation of its own. Each
// test is skipped where the tool is missing.
#include "server.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace ldap_test;
namespace ldap = sgcl::net::ldap;

namespace {
    std::string run(const std::string& cmd, int* status = nullptr) {
        std::string out;
        FILE* p = popen((cmd + " 2>&1").c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        int st = pclose(p);
        if (status) {
            *status = st;
        }
        return out;
    }

    bool have(const char* tool) {
        return std::filesystem::exists(tool);
    }

    std::string uri(const Server& s, const char* host = "127.0.0.1") {
        return str(s.url("ldap", host));
    }

    size_t count(const std::string& text, const std::string& what) {
        size_t n = 0;
        for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) {
            ++n;
        }
        return n;
    }
}

TEST(LdapInterop, Ldapsearch) {
    if (!have("/usr/bin/ldapsearch")) {
        GTEST_SKIP() << "no ldapsearch";
    }
    Server s;
    int st = 0;
    std::string out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -b ou=people,dc=example,dc=com '(&(objectClass=person)(sn=Smith))' cn mail", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_NE(out.find("dn: uid=alice,ou=people,dc=example,dc=com"), std::string::npos) << out;
    EXPECT_NE(out.find("cn: Carol Smith"), std::string::npos) << out;
    EXPECT_EQ(out.find("age:"), std::string::npos) << out;
    // a simple bind, substrings and an extensible match
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -D cn=admin,dc=example,dc=com -w secret -b ou=people,dc=example,dc=com '(|(cn=*o*)(givenName:caseExactMatch:=Eve))' uid", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_EQ(count(out, "uid: "), 4u) << out;
    // a wrong password: 49
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -D cn=admin,dc=example,dc=com -w nope -b dc=example,dc=com", &st);
    EXPECT_NE(st, 0);
    EXPECT_NE(out.find("Invalid credentials (49)"), std::string::npos) << out;
    // paged results, 2 a page
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -E pr=2/noprompt -b ou=people,dc=example,dc=com -s one '(objectClass=*)' uid", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_EQ(count(out, "uid: "), 5u) << out;
    // a size limit
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -z 2 -b ou=people,dc=example,dc=com '(objectClass=*)' uid", &st);
    EXPECT_NE(out.find("Size limit exceeded (4)"), std::string::npos) << out;
    // no such object, and a referral
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -b uid=zed,ou=people,dc=example,dc=com", &st);
    EXPECT_NE(out.find("No such object (32)"), std::string::npos) << out;
    EXPECT_NE(out.find("Matched DN: ou=people,dc=example,dc=com"), std::string::npos) << out;
    out = run("/usr/bin/ldapsearch -x -LLL -H " + uri(s) + " -b ou=elsewhere,dc=example,dc=com", &st);
    EXPECT_NE(out.find("ldap://other.example.com/ou=elsewhere,dc=example,dc=com"), std::string::npos) << out;
    // a search reference
    out = run("/usr/bin/ldapsearch -x -H " + uri(s) + " -b dc=example,dc=com '(cn=staff)' cn", &st);
    EXPECT_NE(out.find("ldap://replica.example.com/dc=example,dc=com"), std::string::npos) << out;
}

// TLS is not checked here: the system's OpenLDAP is built on SecureTransport,
// which speaks TLS 1.2 at most, and the module's TLS server is TLS 1.3
// alone (net::tls); StartTLS and ldaps:// are checked between the client
// and the test server (client.cpp, Tls)
TEST(LdapInterop, WhoAmI) {
    if (!have("/usr/bin/ldapwhoami")) {
        GTEST_SKIP() << "no ldapwhoami";
    }
    Server s;
    int st = 0;
    std::string out = run("/usr/bin/ldapwhoami -x -H " + uri(s) + " -D cn=admin,dc=example,dc=com -w secret", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_NE(out.find("dn:cn=admin,dc=example,dc=com"), std::string::npos) << out;
    out = run("/usr/bin/ldapwhoami -x -H " + uri(s), &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_NE(out.find("anonymous"), std::string::npos) << out;
}

TEST(LdapInterop, UpdatesByOpenLdap) {
    if (!have("/usr/bin/ldapmodify") || !have("/usr/bin/ldapcompare") || !have("/usr/bin/ldapmodrdn") || !have("/usr/bin/ldapdelete")) {
        GTEST_SKIP() << "no ldapmodify";
    }
    Server s;
    auto dir = std::filesystem::temp_directory_path() / ("sgcl-ldap-" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    auto ldif = (dir / "add.ldif").string();
    {
        std::ofstream f(ldif);
        f << "dn: uid=gina,ou=people,dc=example,dc=com\nobjectClass: person\nuid: gina\ncn: Gina Lee\nsn: Lee\n\n"
          << "dn: uid=gina,ou=people,dc=example,dc=com\nchangetype: modify\nadd: mail\nmail: gina@example.com\n-\nreplace: cn\ncn: Gina L.\n-\n";
    }
    int st = 0;
    std::string auth = " -x -H " + uri(s) + " -D cn=admin,dc=example,dc=com -w secret ";
    std::string out = run("/usr/bin/ldapmodify -a" + auth + "-f " + ldif, &st);
    EXPECT_EQ(st, 0) << out;
    // what OpenLDAP wrote, read by our client
    auto c = ldap::client::connect(s.url(), [] {
        ldap::client::options o;
        o.security = ldap::security::none;
        return o;
    }()).value();
    auto e = c.search("uid=gina,ou=people,dc=example,dc=com", "(objectClass=*)");
    ASSERT_TRUE(e) << str(e.error().message());
    EXPECT_EQ(str(e->entries[0].get("cn")), "Gina L.");
    EXPECT_EQ(str(e->entries[0].get("mail")), "gina@example.com");
    out = run("/usr/bin/ldapcompare" + auth + "uid=gina,ou=people,dc=example,dc=com cn:Gina\\ L.", &st);
    EXPECT_NE(out.find("TRUE"), std::string::npos) << out;
    out = run("/usr/bin/ldapmodrdn -r" + auth + "uid=gina,ou=people,dc=example,dc=com uid=georgina", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_TRUE(c.search("uid=georgina,ou=people,dc=example,dc=com", "(objectClass=*)"));
    out = run("/usr/bin/ldapdelete" + auth + "uid=georgina,ou=people,dc=example,dc=com", &st);
    EXPECT_EQ(st, 0) << out;
    EXPECT_FALSE(c.search("uid=georgina,ou=people,dc=example,dc=com", "(objectClass=*)"));
    std::filesystem::remove_all(dir);
}
