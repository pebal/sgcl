//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::spf: RFC 7208 §7.4's macro examples, every mechanism and modifier
// against a zone on the loopback, the qualifiers, the limits of §4.6.4,
// the records that are no record or more than one, DNS failures, the null
// sender, IPv6.
#include "common.h"

using namespace mail_test;
namespace spf = sgcl::net::spf;

namespace {
    spf::result check(Behaviour& b, const char* ip, const char* sender, const char* helo = "mail.example.test") {
        Server srv(b);
        EXPECT_TRUE(srv.ok());
        spf::options o;
        o.dns = srv.options();
        o.receiver = "mx.receiver.test";
        return spf::check(net::ip_address(ip), sender, helo, o);
    }

    std::string expand(const char* ip, const char* sender, const char* domain, const char* macro, bool exp = false) {
        namespace d = sgcl::net::spf::detail;
        tracked_ptr st = make_tracked<d::SpfState>();
        st->ip = net::ip_address(ip);
        std::string s = sender;
        st->sender = s;
        st->local = s.substr(0, s.find('@'));
        st->sender_domain = s.substr(s.find('@') + 1);
        st->helo = "helo.example.test";
        st->receiver = "mx.receiver.test";
        st->now = 1700000000;
        std::string out;
        bool ok = d::spf_expand(st, domain, macro, exp, &out).wait();
        return ok ? out : std::string("<error>");
    }
}

// RFC 7208 §7.4: the examples of macro expansion
TEST(MailSpf, Rfc7208MacroExamples) {
    const char* s = "strong-bad@email.example.com";
    const char* d = "email.example.com";
    const char* ip = "192.0.2.3";
    EXPECT_EQ(expand(ip, s, d, "%{s}"), "strong-bad@email.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{o}"), "email.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d}"), "email.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d4}"), "email.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d3}"), "email.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d2}"), "example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d1}"), "com");
    EXPECT_EQ(expand(ip, s, d, "%{dr}"), "com.example.email");
    EXPECT_EQ(expand(ip, s, d, "%{d2r}"), "example.email");
    EXPECT_EQ(expand(ip, s, d, "%{l}"), "strong-bad");
    EXPECT_EQ(expand(ip, s, d, "%{l-}"), "strong.bad");
    EXPECT_EQ(expand(ip, s, d, "%{lr}"), "strong-bad");
    EXPECT_EQ(expand(ip, s, d, "%{lr-}"), "bad.strong");
    EXPECT_EQ(expand(ip, s, d, "%{l1r-}"), "strong");
    EXPECT_EQ(expand(ip, s, d, "%{ir}.%{v}._spf.%{d2}"), "3.2.0.192.in-addr._spf.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{lr-}.lp._spf.%{d2}"), "bad.strong.lp._spf.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{lr-}.lp.%{ir}.%{v}._spf.%{d2}"), "bad.strong.lp.3.2.0.192.in-addr._spf.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{ir}.%{v}.%{l1r-}.lp._spf.%{d2}"), "3.2.0.192.in-addr.strong.lp._spf.example.com");
    EXPECT_EQ(expand(ip, s, d, "%{d2}.trusted-domains.example.net"), "example.com.trusted-domains.example.net");
    EXPECT_EQ(expand("2001:db8::cb01", s, d, "%{ir}.%{v}._spf.%{d2}"),
              "1.0.b.c.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6._spf.example.com");
    // escapes, upper case URL-escaped, the letters of exp
    EXPECT_EQ(expand(ip, s, d, "%%%_%-"), "% %20");
    EXPECT_EQ(expand(ip, "a+b@example.com", d, "%{L}"), "a%2Bb");
    EXPECT_EQ(expand(ip, s, d, "%{h} %{c} %{r} %{t}", true), "helo.example.test 192.0.2.3 mx.receiver.test 1700000000");
    EXPECT_EQ(expand(ip, s, d, "%{d0}"), "<error>");
}

TEST(MailSpf, MacroSyntax) {
    namespace d = sgcl::net::spf::detail;
    EXPECT_TRUE(d::spf_macro_valid("%{ir}.%{v}._spf.%{d2}", false));
    EXPECT_FALSE(d::spf_macro_valid("%{c}", false));
    EXPECT_TRUE(d::spf_macro_valid("%{c}", true));
    EXPECT_FALSE(d::spf_macro_valid("%{x}", false));
    EXPECT_FALSE(d::spf_macro_valid("%{d", false));
    EXPECT_FALSE(d::spf_macro_valid("%x", false));
    EXPECT_FALSE(d::spf_macro_valid("%", false));
    EXPECT_TRUE(d::spf_domain_spec_valid("example.com"));
    EXPECT_TRUE(d::spf_domain_spec_valid("%{d}"));
    EXPECT_FALSE(d::spf_domain_spec_valid("example"));
    EXPECT_FALSE(d::spf_domain_spec_valid("example.123"));
    EXPECT_FALSE(d::spf_domain_spec_valid("example.-com"));
}

TEST(MailSpf, Mechanisms) {
    Behaviour b;
    b.zone.push_back(txt_rr("ip.test.", "v=spf1 ip4:192.0.2.0/24 ip6:2001:db8::/32 -all"));
    b.zone.push_back(txt_rr("a.test.", "v=spf1 a -all"));
    b.zone.push_back(rr_a("a.test.", 192, 0, 2, 10));
    b.zone.push_back(txt_rr("acidr.test.", "v=spf1 a:a.test/24 -all"));
    b.zone.push_back(txt_rr("mx.test.", "v=spf1 mx -all"));
    b.zone.push_back(rr_mx("mx.test.", 10, "mail.mx.test."));
    b.zone.push_back(rr_a("mail.mx.test.", 192, 0, 2, 20));
    b.zone.push_back(txt_rr("ptr.test.", "v=spf1 ptr -all"));
    b.zone.push_back(rr_named("3.2.0.192.in-addr.arpa.", dns_test::type::ptr, "host.ptr.test."));
    b.zone.push_back(rr_a("host.ptr.test.", 192, 0, 2, 3));
    b.zone.push_back(txt_rr("exists.test.", "v=spf1 exists:%{i}.ex.exists.test -all"));
    b.zone.push_back(rr_a("192.0.2.3.ex.exists.test.", 127, 0, 0, 2));
    b.zone.push_back(txt_rr("inc.test.", "v=spf1 include:inner.inc.test -all"));
    b.zone.push_back(txt_rr("inner.inc.test.", "v=spf1 ip4:192.0.2.3 -all"));
    auto pass = [&](const char* domain, const char* ip = "192.0.2.3") {
        auto r = check(b, ip, (std::string("user@") + domain).c_str());
        EXPECT_EQ(r.status, spf::status::pass) << domain << " " << str(r.reason) << " " << str(r.mechanism);
        return r;
    };
    EXPECT_EQ(str(pass("ip.test").mechanism), "ip4:192.0.2.0/24");
    EXPECT_EQ(str(pass("ip.test", "2001:db8::1").mechanism), "ip6:2001:db8::/32");
    EXPECT_EQ(check(b, "198.51.100.1", "user@ip.test").status, spf::status::fail);
    EXPECT_EQ(str(check(b, "198.51.100.1", "user@ip.test").mechanism), "-all");
    pass("a.test", "192.0.2.10");
    EXPECT_EQ(check(b, "192.0.2.11", "user@a.test").status, spf::status::fail);
    pass("acidr.test", "192.0.2.200");
    pass("mx.test", "192.0.2.20");
    EXPECT_EQ(check(b, "192.0.2.21", "user@mx.test").status, spf::status::fail);
    pass("ptr.test");
    pass("exists.test");
    EXPECT_EQ(check(b, "192.0.2.4", "user@exists.test").status, spf::status::fail);
    auto inc = pass("inc.test");
    EXPECT_EQ(str(inc.mechanism), "include:inner.inc.test");
    EXPECT_EQ(inc.lookups, 1u);
    // an IPv4-mapped client is the IPv4 address
    pass("ip.test", "::ffff:192.0.2.3");
}

TEST(MailSpf, QualifiersAndDefault) {
    Behaviour b;
    b.zone.push_back(txt_rr("soft.test.", "v=spf1 ~all"));
    b.zone.push_back(txt_rr("neutral.test.", "v=spf1 ?all"));
    b.zone.push_back(txt_rr("plus.test.", "v=spf1 +ip4:192.0.2.3"));
    b.zone.push_back(txt_rr("nothing.test.", "v=spf1 ip4:10.0.0.1"));
    b.zone.push_back(txt_rr("upper.test.", "V=SPF1 IP4:192.0.2.3 -ALL"));
    EXPECT_EQ(check(b, "192.0.2.3", "a@soft.test").status, spf::status::softfail);
    EXPECT_EQ(check(b, "192.0.2.3", "a@neutral.test").status, spf::status::neutral);
    EXPECT_EQ(check(b, "192.0.2.3", "a@plus.test").status, spf::status::pass);
    auto def = check(b, "192.0.2.3", "a@nothing.test");
    EXPECT_EQ(def.status, spf::status::neutral);
    EXPECT_EQ(str(def.mechanism), "default");
    EXPECT_EQ(check(b, "192.0.2.3", "a@upper.test").status, spf::status::pass);
}

TEST(MailSpf, RedirectAndExplanation) {
    Behaviour b;
    b.zone.push_back(txt_rr("red.test.", "v=spf1 redirect=target.test"));
    b.zone.push_back(txt_rr("target.test.", "v=spf1 ip4:192.0.2.3 -all exp=why.%{d}"));
    b.zone.push_back(txt_rr("why.target.test.", "%{i} is not one of %{d}'s designated mail servers."));
    b.zone.push_back(txt_rr("all.test.", "v=spf1 ?all redirect=target.test"));
    b.zone.push_back(txt_rr("void.test.", "v=spf1 redirect=nowhere.test"));
    b.zone.push_back(txt_rr("exp.test.", "v=spf1 -all exp=why.target.test"));
    EXPECT_EQ(check(b, "192.0.2.3", "a@red.test").status, spf::status::pass);
    auto f = check(b, "192.0.2.9", "a@red.test");
    EXPECT_EQ(f.status, spf::status::fail);
    EXPECT_EQ(str(f.explanation), "192.0.2.9 is not one of target.test's designated mail servers.");
    EXPECT_EQ(str(f.domain), "red.test");
    // an "all" anywhere: the redirect is ignored
    EXPECT_EQ(check(b, "192.0.2.3", "a@all.test").status, spf::status::neutral);
    EXPECT_EQ(check(b, "192.0.2.3", "a@void.test").status, spf::status::permerror);
    auto e = check(b, "192.0.2.3", "a@exp.test");
    EXPECT_EQ(str(e.explanation), "192.0.2.3 is not one of exp.test's designated mail servers.");
}

TEST(MailSpf, NoRecordAndBadRecords) {
    Behaviour b;
    b.zone.push_back(txt_rr("two.test.", "v=spf1 -all"));
    b.zone.push_back(txt_rr("two.test.", "v=spf1 +all"));
    b.zone.push_back(txt_rr("other.test.", "google-site-verification=abc"));
    b.zone.push_back(txt_rr("spf10.test.", "v=spf10 +all"));
    b.zone.push_back(txt_rr("syntax.test.", "v=spf1 ip4:192.0.2.0/33 -all"));
    b.zone.push_back(txt_rr("unknown.test.", "v=spf1 foo -all"));
    b.zone.push_back(txt_rr("toplabel.test.", "v=spf1 a:example -all"));
    b.zone.push_back(txt_rr("modifier.test.", "v=spf1 ip4:192.0.2.3 x-custom=%{d} -all"));
    b.zone.push_back(txt_rr("dup.test.", "v=spf1 redirect=a.test redirect=b.test"));
    b.rcode["servfail.test."] = 2;
    EXPECT_EQ(check(b, "192.0.2.3", "a@two.test").status, spf::status::permerror);
    EXPECT_EQ(check(b, "192.0.2.3", "a@other.test").status, spf::status::none);
    EXPECT_EQ(check(b, "192.0.2.3", "a@spf10.test").status, spf::status::none);
    EXPECT_EQ(check(b, "192.0.2.3", "a@missing.test").status, spf::status::none);
    EXPECT_EQ(check(b, "192.0.2.3", "a@syntax.test").status, spf::status::permerror);
    EXPECT_EQ(check(b, "192.0.2.3", "a@unknown.test").status, spf::status::permerror);
    EXPECT_EQ(check(b, "192.0.2.3", "a@toplabel.test").status, spf::status::permerror);
    EXPECT_EQ(check(b, "192.0.2.3", "a@modifier.test").status, spf::status::pass);
    EXPECT_EQ(check(b, "192.0.2.3", "a@dup.test").status, spf::status::permerror);
    EXPECT_EQ(check(b, "192.0.2.3", "a@servfail.test").status, spf::status::temperror);
    // no domain to check
    EXPECT_EQ(check(b, "192.0.2.3", "a@single").status, spf::status::none);
    EXPECT_EQ(check(b, "192.0.2.3", "a@bad..test").status, spf::status::none);
    std::string long_label = "a@" + std::string(64, 'x') + ".test";
    EXPECT_EQ(check(b, "192.0.2.3", long_label.c_str()).status, spf::status::none);
}

TEST(MailSpf, Limits) {
    Behaviour b;
    // eleven includes in a chain
    for (int i = 0; i < 11; ++i) {
        b.zone.push_back(txt_rr("l" + std::to_string(i) + ".test.", "v=spf1 include:l" + std::to_string(i + 1) + ".test -all"));
    }
    b.zone.push_back(txt_rr("l11.test.", "v=spf1 +all"));
    auto r = check(b, "192.0.2.3", "a@l0.test");
    EXPECT_EQ(r.status, spf::status::permerror);
    EXPECT_EQ(str(r.reason), "too many DNS lookups");
    EXPECT_EQ(check(b, "192.0.2.3", "a@l2.test").status, spf::status::pass);   // nine: within
    // three void lookups
    b.zone.push_back(txt_rr("voids.test.", "v=spf1 a:v1.test a:v2.test a:v3.test -all"));
    auto v = check(b, "192.0.2.3", "a@voids.test");
    EXPECT_EQ(v.status, spf::status::permerror);
    EXPECT_EQ(str(v.reason), "too many void DNS lookups");
    // eleven MX records
    b.zone.push_back(txt_rr("manymx.test.", "v=spf1 mx -all"));
    for (int i = 0; i < 11; ++i) {
        b.zone.push_back(rr_mx("manymx.test.", uint16_t(i), "m" + std::to_string(i) + ".manymx.test."));
    }
    EXPECT_EQ(check(b, "192.0.2.3", "a@manymx.test").status, spf::status::permerror);
    // the options' own limits
    Server srv(b);
    spf::options o;
    o.dns = srv.options();
    o.max_lookups = 2;
    EXPECT_EQ(spf::check(net::ip_address("192.0.2.3"), "a@l8.test", "h.test", o).status, spf::status::permerror);
}

TEST(MailSpf, NullSenderAndHelo) {
    Behaviour b;
    b.zone.push_back(txt_rr("mail.example.test.", "v=spf1 ip4:192.0.2.3 -all"));
    auto r = check(b, "192.0.2.3", "", "mail.example.test");
    EXPECT_EQ(r.status, spf::status::pass);
    EXPECT_EQ(str(r.domain), "mail.example.test");
    // the HELO identity checked by itself (§2.3)
    EXPECT_EQ(check(b, "192.0.2.9", "mail.example.test", "mail.example.test").status, spf::status::fail);
    // an empty local part is postmaster
    b.zone.push_back(txt_rr("lp.test.", "v=spf1 exists:%{l}.lp.test -all"));
    b.zone.push_back(rr_a("postmaster.lp.test.", 127, 0, 0, 2));
    EXPECT_EQ(check(b, "192.0.2.3", "@lp.test").status, spf::status::pass);
    // no address
    Server srv(b);
    spf::options o;
    o.dns = srv.options();
    EXPECT_EQ(spf::check(net::ip_address(), "a@lp.test", "h", o).status, spf::status::none);
}

TEST(MailSpf, Ipv6AndDualCidr) {
    Behaviour b;
    b.zone.push_back(txt_rr("six.test.", "v=spf1 a:host.six.test//64 -all"));
    b.zone.push_back(rr_aaaa("host.six.test."));   // 2001:db8::...
    b.zone.push_back(txt_rr("dual.test.", "v=spf1 a:host.dual.test/24//48 -all"));
    b.zone.push_back(rr_a("host.dual.test.", 192, 0, 2, 1));
    b.zone.push_back(rr_aaaa("host.dual.test."));
    EXPECT_EQ(check(b, "2001:db8::1234", "a@six.test").status, spf::status::pass);
    EXPECT_EQ(check(b, "2001:db9::1", "a@six.test").status, spf::status::fail);
    EXPECT_EQ(check(b, "192.0.2.99", "a@dual.test").status, spf::status::pass);
    EXPECT_EQ(check(b, "2001:db8:0:ffff::1", "a@dual.test").status, spf::status::pass);
    EXPECT_EQ(check(b, "2001:db9::1", "a@dual.test").status, spf::status::fail);
}

TEST(MailSpf, ToStringAndAsync) {
    EXPECT_EQ(str(spf::to_string(spf::status::softfail)), "softfail");
    EXPECT_EQ(str(spf::to_string(spf::status::temperror)), "temperror");
    Behaviour b;
    b.zone.push_back(txt_rr("ip.test.", "v=spf1 ip4:192.0.2.0/24 -all"));
    Server srv(b);
    spf::options o;
    o.dns = srv.options();
    auto t = spf::async_check(net::ip_address("192.0.2.1"), "x@ip.test", "h.test", o);
    EXPECT_EQ(t.wait().status, spf::status::pass);
}
