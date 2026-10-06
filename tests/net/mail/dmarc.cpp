//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dmarc: records parsed and written, discovery at the domain and at
// its organizational domain (the Public Suffix List: co.uk), alignment
// relaxed and strict, sp= and np=, pct='s sample, DNS failures; an
// aggregate report of RFC 7489 Appendix C parsed.
#include "common.h"

using namespace mail_test;
namespace dmarc = sgcl::net::dmarc;
namespace spf = sgcl::net::spf;
namespace dkim = sgcl::net::dkim;

namespace {
    spf::result spf_of(spf::status s, const char* domain) {
        spf::result r;
        r.status = s;
        r.domain = domain;
        return r;
    }

    dkim::result dkim_of(dkim::status s, const char* domain) {
        dkim::result r;
        r.status = s;
        r.domain = domain;
        return r;
    }

    dmarc::result check(Behaviour& b, const char* from, const spf::result& s, const vector<dkim::result>& d = {}) {
        Server srv(b);
        EXPECT_TRUE(srv.ok());
        dmarc::options o;
        o.dns = srv.options();
        return dmarc::check(from, s, d, o);
    }
}

TEST(MailDmarc, RecordParse) {
    auto r = dmarc::record::parse("v=DMARC1; p=reject; sp=quarantine; np=reject; adkim=s; aspf=r; pct=25; fo=1:d; "
                                  "rua=mailto:agg@example.com, mailto:x@example.net!10m; ruf=mailto:f@example.com; ri=3600; rf=afrf; x-unknown=1");
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_EQ(r->policy, dmarc::policy::reject);
    EXPECT_EQ(r->subdomain_policy, dmarc::policy::quarantine);
    EXPECT_EQ(r->nonexistent_policy, dmarc::policy::reject);
    EXPECT_TRUE(r->strict_dkim);
    EXPECT_FALSE(r->strict_spf);
    EXPECT_EQ(r->percent, 25);
    EXPECT_EQ(str(r->failure_options), "1:d");
    ASSERT_EQ(r->aggregate_reports.size(), 2u);
    EXPECT_EQ(str(r->aggregate_reports[1]), "mailto:x@example.net!10m");
    EXPECT_EQ(r->report_interval, 3600 * second);
    EXPECT_EQ(str(r->to_string()), "v=DMARC1; p=reject; sp=quarantine; np=reject; adkim=s; pct=25; fo=1:d; "
                                   "rua=mailto:agg@example.com,mailto:x@example.net!10m; ruf=mailto:f@example.com; ri=3600");
    EXPECT_EQ(dmarc::record::parse(r->to_string()).value(), *r);
    // defaults
    auto d = dmarc::record("v=DMARC1; p=none");
    EXPECT_EQ(d.percent, 100);
    EXPECT_FALSE(d.subdomain_policy);
    EXPECT_EQ(str(d.to_string()), "v=DMARC1; p=none");
    // bad values of other tags keep their defaults (§6.3)
    auto lax = dmarc::record::parse("v=DMARC1; p=quarantine; pct=150; adkim=x; sp=bogus; ri=-1");
    ASSERT_TRUE(lax);
    EXPECT_EQ(lax->percent, 100);
    EXPECT_FALSE(lax->strict_dkim);
    EXPECT_FALSE(lax->subdomain_policy);
    // no valid p= with rua: p=none (§6.6.3); without: refused
    auto rua = dmarc::record::parse("v=DMARC1; p=bogus; rua=mailto:a@example.com");
    ASSERT_TRUE(rua);
    EXPECT_EQ(rua->policy, dmarc::policy::none);
    for (const char* bad : {"v=DMARC1; p=bogus", "v=DMARC1", "p=reject; v=DMARC1", "v=DMARC2; p=none", "v=dmarc1; p=none", "", "garbage", "v=DMARC1; p=none; p=reject"}) {
        auto e = dmarc::record::parse(bad);
        ASSERT_FALSE(e) << bad;
        EXPECT_EQ(e.error().code(), net::errc::malformed_dmarc) << bad;
    }
    EXPECT_THROW(dmarc::record("nope"), sgcl::bad_expected_access<io::error>);
    EXPECT_EQ(str(dmarc::to_string(dmarc::policy::quarantine)), "quarantine");
    EXPECT_EQ(str(dmarc::to_string(dmarc::status::temperror)), "temperror");
}

TEST(MailDmarc, DiscoveryAndAlignment) {
    Behaviour b;
    b.zone.push_back(txt_rr("_dmarc.example.co.uk.", "v=DMARC1; p=reject; sp=quarantine"));
    b.zone.push_back(txt_rr("_dmarc.strict.example.", "v=DMARC1; p=reject; adkim=s; aspf=s"));
    b.zone.push_back(txt_rr("_dmarc.example.com.", "v=DMARC1; p=quarantine"));
    // the domain's own record
    auto own = check(b, "example.co.uk", spf_of(spf::status::pass, "example.co.uk"));
    EXPECT_EQ(own.status, dmarc::status::pass);
    EXPECT_TRUE(own.spf_aligned);
    EXPECT_EQ(str(own.record_domain), "example.co.uk");
    EXPECT_EQ(own.policy, dmarc::policy::reject);
    EXPECT_EQ(own.disposition, dmarc::policy::none);
    // a subdomain: the organizational domain's record, sp=; relaxed alignment
    auto sub = check(b, "news.example.co.uk", spf_of(spf::status::pass, "bounces.example.co.uk"));
    EXPECT_EQ(sub.status, dmarc::status::pass);
    EXPECT_EQ(str(sub.record_domain), "example.co.uk");
    EXPECT_EQ(sub.policy, dmarc::policy::quarantine);
    auto fail = check(b, "news.example.co.uk", spf_of(spf::status::pass, "other.co.uk"), {dkim_of(dkim::status::pass, "other.co.uk")});
    EXPECT_EQ(fail.status, dmarc::status::fail);
    EXPECT_FALSE(fail.spf_aligned);
    EXPECT_FALSE(fail.dkim_aligned);
    EXPECT_EQ(fail.disposition, dmarc::policy::quarantine);
    // co.uk is a public suffix: example.co.uk and other.co.uk are not one organization
    auto dkim_only = check(b, "example.co.uk", spf_of(spf::status::fail, "example.co.uk"), {dkim_of(dkim::status::fail, "example.co.uk"), dkim_of(dkim::status::pass, "mail.example.co.uk")});
    EXPECT_EQ(dkim_only.status, dmarc::status::pass);
    EXPECT_TRUE(dkim_only.dkim_aligned);
    // strict: the same name only
    auto strict = check(b, "strict.example", spf_of(spf::status::pass, "mail.strict.example"), {dkim_of(dkim::status::pass, "mail.strict.example")});
    EXPECT_EQ(strict.status, dmarc::status::fail);
    EXPECT_EQ(strict.disposition, dmarc::policy::reject);
    auto exact = check(b, "strict.example", spf_of(spf::status::pass, "STRICT.example."));
    EXPECT_EQ(exact.status, dmarc::status::pass);
    // softfail is not a pass
    EXPECT_EQ(check(b, "example.com", spf_of(spf::status::softfail, "example.com")).status, dmarc::status::fail);
    // no record anywhere
    auto none = check(b, "example.org", spf_of(spf::status::pass, "example.org"));
    EXPECT_EQ(none.status, dmarc::status::none);
    EXPECT_FALSE(none.record);
}

TEST(MailDmarc, RecordsThatAreNone) {
    Behaviour b;
    b.zone.push_back(txt_rr("_dmarc.two.example.", "v=DMARC1; p=reject"));
    b.zone.push_back(txt_rr("_dmarc.two.example.", "v=DMARC1; p=none"));
    b.zone.push_back(txt_rr("_dmarc.junk.example.", "v=DMARC1; p=bogus"));
    b.zone.push_back(txt_rr("_dmarc.other.example.", "something else"));
    b.rcode["_dmarc.down.example."] = 2;
    EXPECT_EQ(check(b, "two.example", spf_of(spf::status::fail, "x")).status, dmarc::status::none);
    EXPECT_EQ(check(b, "junk.example", spf_of(spf::status::fail, "x")).status, dmarc::status::none);
    EXPECT_EQ(check(b, "other.example", spf_of(spf::status::fail, "x")).status, dmarc::status::none);
    EXPECT_EQ(check(b, "down.example", spf_of(spf::status::fail, "x")).status, dmarc::status::temperror);
    EXPECT_EQ(check(b, "", spf_of(spf::status::fail, "x")).status, dmarc::status::permerror);
    EXPECT_EQ(check(b, "bad..example", spf_of(spf::status::fail, "x")).status, dmarc::status::permerror);
}

TEST(MailDmarc, NonexistentNamesAndPct) {
    Behaviour b;
    b.zone.push_back(txt_rr("_dmarc.example.net.", "v=DMARC1; p=none; sp=quarantine; np=reject"));
    b.zone.push_back(rr_a("www.example.net.", 192, 0, 2, 1));
    b.zone.push_back(txt_rr("_dmarc.sample.example.", "v=DMARC1; p=reject; pct=0"));
    b.zone.push_back(txt_rr("_dmarc.half.example.", "v=DMARC1; p=quarantine; pct=0"));
    // a name that exists: sp=; one that does not: np=
    auto existing = check(b, "www.example.net", spf_of(spf::status::fail, "x"));
    EXPECT_EQ(existing.policy, dmarc::policy::quarantine);
    auto ghost = check(b, "ghost.example.net", spf_of(spf::status::fail, "x"));
    EXPECT_EQ(ghost.policy, dmarc::policy::reject);
    // pct=0: no failing message in the sample, each one step milder
    auto sample = check(b, "sample.example", spf_of(spf::status::fail, "x"));
    EXPECT_EQ(sample.policy, dmarc::policy::reject);
    EXPECT_EQ(sample.disposition, dmarc::policy::quarantine);
    auto half = check(b, "half.example", spf_of(spf::status::fail, "x"));
    EXPECT_EQ(half.disposition, dmarc::policy::none);
}

TEST(MailDmarc, Lookup) {
    Behaviour b;
    b.zone.push_back(txt_rr("_dmarc.example.co.uk.", "v=DMARC1; p=reject"));
    b.zone.push_back(txt_rr("_dmarc.broken.example.", "v=DMARC1; p=bogus"));
    b.rcode["_dmarc.down.example."] = 2;
    Server srv(b);
    dmarc::options o;
    o.dns = srv.options();
    auto r = dmarc::lookup("shop.example.co.uk", o);
    ASSERT_TRUE(r);
    EXPECT_EQ(r->policy, dmarc::policy::reject);
    auto none = dmarc::lookup("example.org", o);
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::no_data);
    auto broken = dmarc::lookup("broken.example", o);
    ASSERT_FALSE(broken);
    EXPECT_EQ(broken.error().code(), net::errc::malformed_dmarc);
    auto down = dmarc::lookup("down.example", o);
    ASSERT_FALSE(down);
    EXPECT_EQ(down.error().code(), net::errc::server_failure);
    auto bad = dmarc::lookup("", o);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_address);
    auto t = dmarc::async_lookup("example.co.uk", o);
    EXPECT_TRUE(t.wait());
    auto c = dmarc::async_check("example.co.uk", spf_of(spf::status::pass, "example.co.uk"), {}, o);
    EXPECT_EQ(c.wait().status, dmarc::status::pass);
}

// RFC 7489 Appendix C's shape, as a receiver sends it
TEST(MailDmarc, AggregateReport) {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8" ?>
<feedback>
  <version>1.0</version>
  <report_metadata>
    <org_name>receiver.example</org_name>
    <email>noreply-dmarc@receiver.example</email>
    <report_id>9391651994964116463</report_id>
    <date_range><begin>1335571200</begin><end>1335657599</end></date_range>
    <error>one</error>
  </report_metadata>
  <policy_published>
    <domain>example.com</domain>
    <adkim>r</adkim>
    <aspf>s</aspf>
    <p>quarantine</p>
    <sp>none</sp>
    <pct>50</pct>
  </policy_published>
  <record>
    <row>
      <source_ip>192.0.2.1</source_ip>
      <count>2</count>
      <policy_evaluated>
        <disposition>none</disposition>
        <dkim>fail</dkim>
        <spf>pass</spf>
        <reason><type>forwarded</type><comment>list</comment></reason>
      </policy_evaluated>
    </row>
    <identifiers><header_from>example.com</header_from><envelope_from>bounce.example.com</envelope_from></identifiers>
    <auth_results>
      <dkim><domain>example.com</domain><selector>s1</selector><result>fail</result><human_result></human_result></dkim>
      <spf><domain>example.com</domain><scope>mfrom</scope><result>pass</result></spf>
    </auth_results>
  </record>
  <record>
    <row><source_ip>2001:db8::5</source_ip><count>1</count>
      <policy_evaluated><disposition>quarantine</disposition><dkim>fail</dkim><spf>fail</spf></policy_evaluated></row>
    <identifiers><header_from>example.com</header_from></identifiers>
    <auth_results><spf><domain>example.com</domain><result>softfail</result></spf></auth_results>
  </record>
</feedback>)";
    auto r = dmarc::report::parse(xml);
    ASSERT_TRUE(r) << str(r.error().message());
    EXPECT_EQ(str(r->org_name), "receiver.example");
    EXPECT_EQ(str(r->report_id), "9391651994964116463");
    EXPECT_EQ(r->begin, 1335571200);
    EXPECT_EQ(r->end, 1335657599);
    ASSERT_EQ(r->errors.size(), 1u);
    EXPECT_EQ(str(r->domain), "example.com");
    EXPECT_EQ(r->policy.policy, dmarc::policy::quarantine);
    EXPECT_EQ(r->policy.subdomain_policy, dmarc::policy::none);
    EXPECT_TRUE(r->policy.strict_spf);
    EXPECT_EQ(r->policy.percent, 50);
    ASSERT_EQ(r->rows.size(), 2u);
    auto& w = r->rows[0];
    EXPECT_EQ(w.source, net::ip_address("192.0.2.1"));
    EXPECT_EQ(w.count, 2u);
    EXPECT_EQ(str(w.dkim), "fail");
    EXPECT_EQ(str(w.spf), "pass");
    ASSERT_EQ(w.reasons.size(), 1u);
    EXPECT_EQ(str(w.reasons[0]), "forwarded");
    EXPECT_EQ(str(w.envelope_from), "bounce.example.com");
    ASSERT_EQ(w.dkim_results.size(), 1u);
    EXPECT_EQ(str(w.dkim_results[0].selector), "s1");
    ASSERT_EQ(w.spf_results.size(), 1u);
    EXPECT_EQ(str(w.spf_results[0].scope), "mfrom");
    EXPECT_EQ(r->rows[1].disposition, dmarc::policy::quarantine);
    EXPECT_EQ(r->rows[1].source, net::ip_address("2001:db8::5"));
    for (const char* bad : {"", "<feedback/>", "<other><report_metadata/><policy_published/></other>", "not xml",
                            "<feedback><report_metadata><date_range><begin>1</begin><end>2</end></date_range></report_metadata><policy_published/>"
                            "<record><row><source_ip>nope</source_ip><count>1</count></row></record></feedback>"}) {
        auto e = dmarc::report::parse(bad);
        ASSERT_FALSE(e) << bad;
        EXPECT_EQ(e.error().code(), net::errc::malformed_dmarc);
    }
}
