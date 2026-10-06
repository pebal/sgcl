//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dkim: RFC 8463's message with both of its signatures (the known
// answer of relaxed canonicalization, oversigned fields, h= with spaces,
// both algorithms), RFC 6376 §3.4.5's canonicalization example, signatures
// of ours verified by ours in every canonicalization and by a verifier
// written apart in Python with OpenSSL's signatures; every failure a
// verifier reports, the keys' records, the boundaries of the signer.
#include "common.h"

#include <chrono>

using namespace mail_test;
using namespace std::chrono_literals;
namespace dkim = sgcl::net::dkim;

namespace {
    // A signer of a key made once per process (RSA keys take a while)
    dkim::signer rsa_signer(const char* domain = "example.com", const char* selector = "s1") {
        static crypto::secret_bytes pem = dkim::signer::generate("example.com", "s1").private_key_pem();
        return dkim::signer(domain, selector, pem);
    }

    dkim::signer ed_signer(const char* domain = "example.com", const char* selector = "e1") {
        auto seed = encoding::base64::standard.decode(rfc8463_ed25519_seed);
        auto key = crypto::ed25519::private_key::from_seed(*seed);
        return dkim::signer(domain, selector, key->to_pem());
    }

    std::string record_owner(const dkim::signer& s) {
        return str(s.record_name()) + ".";
    }

    // A zone with the signers' records
    struct Zone {
        Behaviour b;
        explicit Zone(std::initializer_list<dkim::signer> signers) {
            for (auto& s : signers) {
                b.zone.push_back(txt_rr(record_owner(s), str(s.record())));
            }
        }
    };

    vector<dkim::result> verify_with(Behaviour& b, const std::string& message, dkim::verify_options o = {}) {
        Server srv(b);
        EXPECT_TRUE(srv.ok());
        o.dns = srv.options();
        return dkim::verify(sgcl::string(message), o);
    }

    std::string sign(const dkim::signer& s, const std::string& message, const dkim::sign_options& o = {}) {
        auto r = s.sign(sgcl::string(message), o);
        EXPECT_TRUE(r) << (r ? "" : str(r.error().message()));
        return r ? str(*r) : std::string();
    }

    std::string replace(std::string s, const std::string& from, const std::string& to) {
        size_t at = s.find(from);
        EXPECT_NE(at, std::string::npos) << from;
        if (at != std::string::npos) {
            s.replace(at, from.size(), to);
        }
        return s;
    }
}

TEST(MailDkim, Rfc8463BothSignaturesVerify) {
    Behaviour b;
    b.zone.push_back(txt_rr("brisbane._domainkey.football.example.com.", rfc8463_ed25519_record));
    b.zone.push_back(txt_rr("test._domainkey.football.example.com.", rfc8463_rsa_record));
    auto r = verify_with(b, rfc8463_message);
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0].status, dkim::status::pass) << str(r[0].reason);
    EXPECT_EQ(str(r[0].algorithm), "ed25519-sha256");
    EXPECT_EQ(str(r[0].domain), "football.example.com");
    EXPECT_EQ(str(r[0].selector), "brisbane");
    EXPECT_EQ(str(r[0].identity), "@football.example.com");
    EXPECT_EQ(str(r[0].signature).substr(0, 8), "/gCrinpc");
    EXPECT_EQ(r[1].status, dkim::status::pass) << str(r[1].reason);
    EXPECT_EQ(str(r[1].algorithm), "rsa-sha256");
    EXPECT_FALSE(r[1].testing);
}

TEST(MailDkim, Rfc8463ChangedSubjectFailsBoth) {
    Behaviour b;
    b.zone.push_back(txt_rr("brisbane._domainkey.football.example.com.", rfc8463_ed25519_record));
    b.zone.push_back(txt_rr("test._domainkey.football.example.com.", rfc8463_rsa_record));
    auto r = verify_with(b, replace(rfc8463_message, "Is dinner ready?", "Is lunch ready?"));
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0].status, dkim::status::fail);
    EXPECT_EQ(str(r[0].reason), "signature did not verify");
    EXPECT_EQ(r[1].status, dkim::status::fail);
    // relaxed: whitespace inside the head and the body is no change
    auto spaced = replace(replace(rfc8463_message, "Subject: Is dinner ready?", "Subject:   Is \t dinner ready?  "), "Joe.\r\n", "Joe.   \r\n\r\n\r\n");
    auto s = verify_with(b, spaced);
    EXPECT_EQ(s[0].status, dkim::status::pass) << str(s[0].reason);
    EXPECT_EQ(s[1].status, dkim::status::pass) << str(s[1].reason);
}

// RFC 6376 §3.4.5: the example of both canonicalizations
TEST(MailDkim, Rfc6376CanonicalizationExample) {
    namespace d = sgcl::net::dkim::detail;
    std::string message = " C \r\nD \t E\r\n\r\n\r\n";
    std::string head = "A: X\r\nB : Y\t\r\n\tZ  \r\n";
    std::string whole = head + "\r\n" + message;
    auto split = sgcl::net::detail::mail_split(whole);
    ASSERT_EQ(split.fields.size(), 2u);
    std::string relaxed, simple;
    for (auto& f : split.fields) {
        d::dkim_field(relaxed, f, dkim::canonicalization::relaxed);
        d::dkim_field(simple, f, dkim::canonicalization::simple);
    }
    EXPECT_EQ(relaxed, "a:X\r\nb:Y Z\r\n");
    EXPECT_EQ(simple, head);
    auto body_of = [&](dkim::canonicalization c) {
        uint64_t total = 0;
        auto h = d::dkim_body_hash(split.body, c, UINT64_MAX, total);
        return std::make_pair(h, total);
    };
    auto expect = [](const std::string& text) {
        crypto::sha256 h;
        h.update(text);
        return h.value();
    };
    EXPECT_EQ(body_of(dkim::canonicalization::relaxed).first, expect(" C\r\nD E\r\n"));
    EXPECT_EQ(body_of(dkim::canonicalization::simple).first, expect(" C \r\nD \t E\r\n"));
    // an empty body: CRLF in simple, nothing in relaxed (§3.4.3, §3.4.4)
    uint64_t total = 0;
    EXPECT_EQ(d::dkim_body_hash("", dkim::canonicalization::simple, UINT64_MAX, total), expect("\r\n"));
    EXPECT_EQ(d::dkim_body_hash("\r\n\r\n", dkim::canonicalization::relaxed, UINT64_MAX, total), expect(""));
    EXPECT_EQ(total, 0u);
    // a last line without CRLF gets one
    EXPECT_EQ(d::dkim_body_hash("abc", dkim::canonicalization::simple, UINT64_MAX, total), expect("abc\r\n"));
    EXPECT_EQ(d::dkim_body_hash("a  b \t", dkim::canonicalization::relaxed, UINT64_MAX, total), expect("a b\r\n"));
}

TEST(MailDkim, SignAndVerifyEveryCanonicalization) {
    auto rsa = rsa_signer();
    auto ed = ed_signer();
    Zone z{rsa, ed};
    Server srv(z.b);
    ASSERT_TRUE(srv.ok());
    dkim::verify_options vo;
    vo.dns = srv.options();
    for (auto* s : {&rsa, &ed}) {
        for (auto h : {dkim::canonicalization::simple, dkim::canonicalization::relaxed}) {
            for (auto bd : {dkim::canonicalization::simple, dkim::canonicalization::relaxed}) {
                dkim::sign_options o;
                o.header = h;
                o.body = bd;
                auto signed_message = sign(*s, plain_message, o);
                ASSERT_TRUE(signed_message.find(plain_message) == signed_message.size() - std::string(plain_message).size());
                auto r = dkim::verify(sgcl::string(signed_message), vo);
                ASSERT_EQ(r.size(), 1u);
                EXPECT_EQ(r[0].status, dkim::status::pass) << str(r[0].reason) << "\n" << signed_message;
                EXPECT_EQ(str(r[0].domain), "example.com");
                // simple body: trailing whitespace is a change
                auto changed = replace(signed_message, "noon?\r\n", "noon? \r\n");
                auto c = dkim::verify(sgcl::string(changed), vo);
                EXPECT_EQ(c[0].status, bd == dkim::canonicalization::simple ? dkim::status::fail : dkim::status::pass);
                // simple head: a header's case is a change
                auto cased = replace(signed_message, "Subject: Lunch", "subject: Lunch");
                auto k = dkim::verify(sgcl::string(cased), vo);
                EXPECT_EQ(k[0].status, h == dkim::canonicalization::simple ? dkim::status::fail : dkim::status::pass);
            }
        }
    }
}

TEST(MailDkim, TamperedMessagesFail) {
    auto s = ed_signer();
    Zone z{s};
    auto m = sign(s, plain_message);
    auto body = verify_with(z.b, replace(m, "noon", "one"));
    EXPECT_EQ(body[0].status, dkim::status::fail);
    EXPECT_EQ(str(body[0].reason), "body hash did not verify");
    auto subject = verify_with(z.b, replace(m, "Subject: Lunch", "Subject: Dinner"));
    EXPECT_EQ(str(subject[0].reason), "signature did not verify");
    // a second Subject above the first: oversigned, so a change
    auto added = verify_with(z.b, replace(m, "From: Alice", "Subject: Free money\r\nFrom: Alice"));
    EXPECT_EQ(added[0].status, dkim::status::fail);
    // a field the signature does not name may be added
    auto other = verify_with(z.b, replace(m, "From: Alice", "X-Spam: no\r\nFrom: Alice"));
    EXPECT_EQ(other[0].status, dkim::status::pass) << str(other[0].reason);
    // without oversigning, a field added above the signed one replaces it in the hash
    dkim::sign_options o;
    o.oversign = false;
    auto plain = sign(s, plain_message, o);
    auto below = verify_with(z.b, replace(plain, "\r\n\r\nShall", "\r\nCc: eve@example.net\r\n\r\nShall"));
    EXPECT_EQ(below[0].status, dkim::status::pass) << str(below[0].reason);
}

TEST(MailDkim, BodyLength) {
    auto s = ed_signer();
    Zone z{s};
    dkim::sign_options o;
    o.body_length = 10;
    auto m = sign(s, plain_message, o);
    EXPECT_NE(m.find("l=10;"), std::string::npos);
    auto r = verify_with(z.b, m + "Appended after the signed part.\r\n");
    EXPECT_EQ(r[0].status, dkim::status::pass) << str(r[0].reason);
    // a body shorter than l=
    auto cut = replace(m, "Shall we meet at noon?\r\n\r\nAlice\r\n", "Shall\r\n");
    auto c = verify_with(z.b, cut);
    EXPECT_EQ(c[0].status, dkim::status::permerror);
    EXPECT_EQ(str(c[0].reason), "body length tag exceeds the body");
    // l= past the body refused by the signer
    o.body_length = 100000;
    auto e = s.sign(sgcl::string(plain_message), o);
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), net::errc::malformed_message);
}

TEST(MailDkim, ExpirationAndTime) {
    auto s = ed_signer();
    Zone z{s};
    dkim::sign_options o;
    o.time = time::datetime::from_unix(1700000000, time::zone::utc());
    o.expiration = 3600s;
    auto m = sign(s, plain_message, o);
    EXPECT_NE(m.find("t=1700000000;"), std::string::npos);
    EXPECT_NE(m.find("x=1700003600;"), std::string::npos);
    dkim::verify_options vo;
    vo.at = time::datetime::from_unix(1700000100, time::zone::utc());
    EXPECT_EQ(verify_with(z.b, m, vo)[0].status, dkim::status::pass);
    vo.at = time::datetime::from_unix(1700003601, time::zone::utc());
    auto late = verify_with(z.b, m, vo);
    EXPECT_EQ(late[0].status, dkim::status::permerror);
    EXPECT_EQ(str(late[0].reason), "signature expired");
    // x= before t=
    auto bad = replace(m, "x=1700003600;", "x=1600000000;");
    EXPECT_EQ(str(verify_with(z.b, bad)[0].reason), "signature syntax error");
}

TEST(MailDkim, Identity) {
    auto s = ed_signer();
    Zone z{s};
    dkim::sign_options o;
    o.identity = "alice@mail.example.com";
    auto m = sign(s, plain_message, o);
    auto r = verify_with(z.b, m);
    EXPECT_EQ(r[0].status, dkim::status::pass) << str(r[0].reason);
    EXPECT_EQ(str(r[0].identity), "alice@mail.example.com");
    o.identity = "alice@example.org";
    auto refused = s.sign(sgcl::string(plain_message), o);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::invalid_address);
    // an i= outside d= made by hand
    auto forged = replace(m, "i=alice@mail.example.com", "i=alice@example.org");
    auto f = verify_with(z.b, forged);
    EXPECT_EQ(f[0].status, dkim::status::permerror);
    EXPECT_EQ(str(f[0].reason), "domain mismatch");
    // t=s in the key: i='s domain must be d= itself
    Behaviour strict;
    strict.zone.push_back(txt_rr(record_owner(s), str(s.record()) + "; t=s"));
    auto st = verify_with(strict, m);
    EXPECT_EQ(st[0].status, dkim::status::permerror);
    Behaviour testing;
    testing.zone.push_back(txt_rr(record_owner(s), str(s.record()) + "; t=y"));
    auto t = verify_with(testing, m);
    EXPECT_EQ(t[0].status, dkim::status::pass);
    EXPECT_TRUE(t[0].testing);
}

TEST(MailDkim, KeyRecords) {
    auto s = ed_signer();
    auto m = sign(s, plain_message);
    auto with_record = [&](const std::string& rec) {
        Behaviour b;
        b.zone.push_back(txt_rr(record_owner(s), rec));
        auto r = verify_with(b, m);
        EXPECT_EQ(r.size(), 1u);
        return r.empty() ? dkim::result() : r[0];
    };
    std::string p = str(s.record()).substr(str(s.record()).find("p="));
    EXPECT_EQ(with_record("v=DKIM1; k=ed25519; p=").status, dkim::status::permerror);
    EXPECT_EQ(str(with_record("v=DKIM1; k=ed25519; p=").reason), "key revoked");
    EXPECT_EQ(str(with_record("v=DKIM1; k=rsa; " + p).reason), "inappropriate key algorithm");
    EXPECT_EQ(str(with_record("v=DKIM1; k=ed25519; h=sha1; " + p).reason), "inappropriate hash algorithm");
    EXPECT_EQ(str(with_record("v=DKIM1; k=ed25519; s=other; " + p).reason), "inappropriate service type");
    EXPECT_EQ(str(with_record("k=ed25519; v=DKIM1; " + p).reason), "key syntax error");
    EXPECT_EQ(str(with_record("v=DKIM1; k=ed25519; p=!!!").reason), "key syntax error");
    EXPECT_EQ(str(with_record("v=DKIM1; k=ed25519; p=AAAA").reason), "key syntax error");
    EXPECT_EQ(with_record("k=ed25519; s=email:*; h=sha256; " + p).status, dkim::status::pass);
    // folding whitespace inside p=
    std::string folded = p.substr(0, 12) + " \t " + p.substr(12);
    EXPECT_EQ(with_record("v=DKIM1; k=ed25519; " + folded).status, dkim::status::pass);
    // no key at all
    Behaviour none;
    auto r = verify_with(none, m);
    EXPECT_EQ(r[0].status, dkim::status::permerror);
    EXPECT_EQ(str(r[0].reason), "no key for signature");
    // the server fails: a temporary error
    Behaviour failing;
    failing.rcode[record_owner(s)] = 2;
    auto t = verify_with(failing, m);
    EXPECT_EQ(t[0].status, dkim::status::temperror);
}

TEST(MailDkim, RsaKeyRecordForms) {
    auto s = rsa_signer();
    auto m = sign(s, plain_message);
    // the key as PKCS #1 RSAPublicKey, as some publish it
    auto pem = s.private_key_pem();
    auto key = crypto::rsa::private_key::from_pem(pem);
    ASSERT_TRUE(key);
    auto pkcs1 = key->public_key().to_pkcs1_der();
    Behaviour b;
    b.zone.push_back(txt_rr(record_owner(s), "v=DKIM1; p=" + str(encoding::base64::standard.encode(pkcs1))));
    auto r = verify_with(b, m);
    EXPECT_EQ(r[0].status, dkim::status::pass) << str(r[0].reason);
}

TEST(MailDkim, MalformedSignatures) {
    auto s = ed_signer();
    Zone z{s};
    auto m = sign(s, plain_message);
    auto reason = [&](const std::string& message) {
        auto r = verify_with(z.b, message);
        EXPECT_EQ(r.size(), 1u);
        return r.empty() ? std::string() : str(r[0].reason);
    };
    EXPECT_EQ(reason(replace(m, "v=1;", "v=2;")), "incompatible version");
    EXPECT_EQ(reason(replace(m, "a=ed25519-sha256", "a=rsa-sha1")), "rsa-sha1 is not accepted (RFC 8301)");
    EXPECT_EQ(reason(replace(m, "a=ed25519-sha256", "a=foo-bar")), "unsupported algorithm");
    EXPECT_EQ(reason(replace(m, "bh=", "xh=")), "signature missing required tag");
    EXPECT_EQ(reason(replace(m, "h=from:", "h=:")), "signature syntax error");
    EXPECT_EQ(reason(replace(m, "c=relaxed/relaxed", "c=fancy/relaxed")), "unsupported canonicalization");
    EXPECT_EQ(reason(replace(m, "v=1;", "v=1; v=1;")), "signature syntax error");
    EXPECT_EQ(reason(replace(m, "d=example.com;", "d=example.com; q=http;")), "unsupported query method");
    EXPECT_EQ(reason(replace(m, "d=example.com;", "d=example.com; l=x;")), "signature syntax error");
    // h= without From
    std::string no_from = m;
    size_t h = no_from.find("h=");
    size_t semi = no_from.find(';', h);
    no_from.replace(h, semi - h, "h=to:subject");
    EXPECT_EQ(reason(no_from), "From field not signed");
}

TEST(MailDkim, ManySignaturesAndNone) {
    auto s = ed_signer();
    Zone z{s};
    std::string m = plain_message;
    for (int i = 0; i < 7; ++i) {
        m = sign(s, m);
    }
    auto r = verify_with(z.b, m);
    EXPECT_EQ(r.size(), 5u);
    for (auto& x : r) {
        EXPECT_EQ(x.status, dkim::status::pass) << str(x.reason);
    }
    dkim::verify_options o;
    o.max_signatures = 2;
    EXPECT_EQ(verify_with(z.b, m, o).size(), 2u);
    EXPECT_TRUE(verify_with(z.b, plain_message).empty());
    EXPECT_TRUE(verify_with(z.b, "").empty());
    EXPECT_TRUE(verify_with(z.b, "garbage without a head").empty());
}

TEST(MailDkim, LineFeedsAndEmptyBody) {
    auto s = ed_signer();
    Zone z{s};
    auto m = sign(s, plain_message);
    std::string lf;
    for (size_t i = 0; i < m.size(); ++i) {
        if (!(m[i] == '\r' && i + 1 < m.size() && m[i + 1] == '\n')) {
            lf += m[i];
        }
    }
    EXPECT_EQ(verify_with(z.b, lf)[0].status, dkim::status::pass);
    // a message of LF lines signed: the same signature as of its CRLF form
    auto from_lf = sign(s, "From: a@example.com\nSubject: x\n\nbody\n");
    EXPECT_EQ(verify_with(z.b, from_lf)[0].status, dkim::status::pass);
    for (auto c : {dkim::canonicalization::simple, dkim::canonicalization::relaxed}) {
        dkim::sign_options o;
        o.body = c;
        auto empty = sign(s, "From: a@example.com\r\nSubject: empty\r\n\r\n", o);
        EXPECT_EQ(verify_with(z.b, empty)[0].status, dkim::status::pass);
        auto headless_body = sign(s, "From: a@example.com\r\nSubject: no body", o);
        EXPECT_EQ(verify_with(z.b, headless_body)[0].status, dkim::status::pass);
    }
}

TEST(MailDkim, SignerBoundaries) {
    auto s = ed_signer("Example.COM", "E1");
    EXPECT_EQ(str(s.domain()), "example.com");
    EXPECT_EQ(str(s.selector()), "e1");
    EXPECT_EQ(s.algorithm(), dkim::algorithm::ed25519_sha256);
    EXPECT_EQ(str(s.record_name()), "e1._domainkey.example.com");
    EXPECT_EQ(str(s.record()), "v=DKIM1; k=ed25519; p=11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=");
    auto copy = s;
    EXPECT_TRUE(copy == s);
    EXPECT_FALSE(copy == ed_signer());
    // the key read back from what it writes
    auto again = dkim::signer::from_pem("example.com", "e1", s.private_key_pem());
    ASSERT_TRUE(again);
    EXPECT_EQ(str(again->record()), str(s.record()));
    // RSA from PKCS #1
    auto r = rsa_signer();
    EXPECT_EQ(r.algorithm(), dkim::algorithm::rsa_sha256);
    EXPECT_EQ(str(r.record()).substr(0, 21), "v=DKIM1; k=rsa; p=MII");
    auto key = crypto::rsa::private_key::from_pem(r.private_key_pem());
    ASSERT_TRUE(key);
    auto der = key->to_pkcs1_der();
    std::string pkcs1 = "-----BEGIN RSA PRIVATE KEY-----\n" + str(encoding::base64::standard.encode(der)) + "\n-----END RSA PRIVATE KEY-----\n";
    auto from1 = dkim::signer::from_pem("example.com", "s1", slice<const byte>(reinterpret_cast<const byte*>(pkcs1.data()), pkcs1.size()));
    ASSERT_TRUE(from1);
    EXPECT_EQ(str(from1->record()), str(r.record()));
    // what is no key, no name
    std::string junk = "not a key";
    auto bad = dkim::signer::from_pem("example.com", "s1", slice<const byte>(reinterpret_cast<const byte*>(junk.data()), junk.size()));
    ASSERT_FALSE(bad);
    EXPECT_EQ(str(bad.error().op()), "dkim");
    auto p256 = crypto::p256::private_key::generate();
    auto ec = dkim::signer::from_pem("example.com", "s1", p256.to_pem());
    ASSERT_FALSE(ec);
    EXPECT_EQ(ec.error().code(), crypto::make_error_code(crypto::errc::unsupported));
    auto name = dkim::signer::from_pem("exa mple..com", "s1", s.private_key_pem());
    ASSERT_FALSE(name);
    EXPECT_EQ(name.error().code(), net::errc::invalid_address);
    EXPECT_THROW(dkim::signer("example.com", "s1", slice<const byte>(reinterpret_cast<const byte*>(junk.data()), junk.size())), sgcl::bad_expected_access<io::error>);
    EXPECT_THROW(dkim::signer::generate("", "s1"), sgcl::bad_expected_access<io::error>);
}

TEST(MailDkim, SignRefusesMalformedMessages) {
    auto s = ed_signer();
    for (const char* m : {"", "\r\nbody only\r\n", "To: b@example.org\r\n\r\nno From\r\n", "From: a@example.com\r\nno colon here\r\n\r\nx"}) {
        auto r = s.sign(sgcl::string(m));
        ASSERT_FALSE(r) << m;
        EXPECT_EQ(r.error().code(), net::errc::malformed_message) << m;
    }
    // explicit names: From first whatever is given, an absent name signed once
    dkim::sign_options o;
    o.headers = {sgcl::string("Subject"), sgcl::string("X-Absent"), sgcl::string("from")};
    o.oversign = false;
    auto m = sign(s, plain_message, o);
    EXPECT_NE(m.find("h=from:subject:x-absent;"), std::string::npos) << m;
    Zone z{s};
    EXPECT_EQ(verify_with(z.b, m)[0].status, dkim::status::pass);
    EXPECT_EQ(verify_with(z.b, replace(m, "From: Alice", "X-Absent: now here\r\nFrom: Alice"))[0].status, dkim::status::fail);
}

TEST(MailDkim, AsyncVerify) {
    auto s = ed_signer();
    Zone z{s};
    Server srv(z.b);
    ASSERT_TRUE(srv.ok());
    dkim::verify_options o;
    o.dns = srv.options();
    auto m = sign(s, plain_message);
    auto t = [](sgcl::string message, dkim::verify_options vo) -> async::task<size_t> {
        auto r = co_await dkim::async_verify(message, vo);
        co_return r.size() == 1 && r[0].status == dkim::status::pass ? 1 : 0;
    }(sgcl::string(m), o);
    EXPECT_EQ(t.wait(), 1u);
}

// A verifier written apart (Python's own parsing and canonicalization from
// RFC 6376, OpenSSL's RSA and Ed25519) checks the signatures of ours
TEST(MailDkim, IndependentVerifierAcceptsOurSignatures) {
    const char* openssl = "/opt/homebrew/opt/openssl@3/bin/openssl";
    if (!have("python3") || !std::filesystem::exists(openssl)) {
        GTEST_SKIP() << "no python3 or OpenSSL 3";
    }
    auto dir = temp_dir("dkim");
    std::string script = (source_root() / "tests/net/mail/python/dkim_verify.py").string();
    int n = 0;
    for (auto s : {rsa_signer(), ed_signer()}) {
        for (auto h : {dkim::canonicalization::simple, dkim::canonicalization::relaxed}) {
            for (auto bd : {dkim::canonicalization::simple, dkim::canonicalization::relaxed}) {
                dkim::sign_options o;
                o.header = h;
                o.body = bd;
                std::string body = std::string(plain_message) + "Trailing  spaces \t \r\n\r\n\r\n";
                auto m = sign(s, body, o);
                auto msg = dir / ("m" + std::to_string(n) + ".eml");
                auto rec = dir / ("m" + std::to_string(n) + ".txt");
                write_file(msg, m);
                write_file(rec, str(s.record()));
                int status = 0;
                std::string out = run_command("python3 " + script + " " + openssl + " " + msg.string() + " " + rec.string() + " 2>&1", &status);
                EXPECT_EQ(out, "pass\n") << m;
                // and a change it must see
                write_file(msg, replace(m, "Lunch", "Brunch"));
                out = run_command("python3 " + script + " " + openssl + " " + msg.string() + " " + rec.string() + " 2>&1", &status);
                EXPECT_EQ(out, "fail\n");
                ++n;
            }
        }
    }
    std::filesystem::remove_all(dir);
}
