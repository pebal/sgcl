//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HOTP and TOTP: RFC 4226's Appendix D and RFC 6238's Appendix B (the time
// of year 2603 through HOTP's counter: a datetime ends in 2261), Python's
// hmac and hashlib as a hand-written oracle on random secrets, counters and
// lengths, the windows of verify at their edges, codes that are not codes,
// options out of range; otpauth:// URIs read and written, Google's example
// and those of other programs, and the URIs refused.
#include "digest_common.h"

#include "sgcl/crypto/otp.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <stdexcept>

using namespace crypto_test;

namespace {
    const char rfc_secret[] = "12345678901234567890";

    sgcl::time::datetime at(int64_t unix) {
        return sgcl::time::datetime::from_unix(unix, sgcl::time::zone::utc());
    }

    std::string s(const sgcl::string& x) {
        return std::string(x.data(), x.size());
    }

    // Python's hmac: the codes of a secret over counters, the oracle written by hand
    std::vector<std::string> python_codes(const bytes_t& secret, uint64_t first, int count, const char* algorithm, int digits) {
        std::string cmd = "python3 -c \"import hmac,hashlib,struct\nk=bytes.fromhex('" + hex(secret) + "')\nfor c in range(" + std::to_string(first) + "," +
                          std::to_string(first + uint64_t(count)) + "):\n d=hmac.new(k,struct.pack('>Q',c),hashlib." + algorithm +
                          ").digest();o=d[-1]&15;v=(int.from_bytes(d[o:o+4],'big')&0x7fffffff)%10**" + std::to_string(digits) +
                          ";print(str(v).zfill(" + std::to_string(digits) + "))\"";
        std::vector<std::string> out;
        FILE* f = ::popen(cmd.c_str(), "r");
        if (!f) {
            return out;
        }
        char line[64];
        while (std::fgets(line, sizeof line, f)) {
            std::string l(line);
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) {
                l.pop_back();
            }
            out.push_back(l);
        }
        ::pclose(f);
        return out;
    }
}

TEST(Crypto_Otp, Rfc4226) {
    const char* expected[] = {"755224", "287082", "359152", "969429", "338314", "254676", "287922", "162583", "399871", "520489"};
    for (uint64_t c = 0; c < 10; ++c) {
        EXPECT_EQ(s(crypto::hotp::generate(rfc_secret, c)), expected[c]);
        auto v = crypto::hotp::verify(rfc_secret, c, sgcl::string(expected[c]));
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, c);
    }
}

TEST(Crypto_Otp, Rfc6238) {
    const std::string s32 = "12345678901234567890123456789012";
    const std::string s64 = "1234567890123456789012345678901234567890123456789012345678901234";
    struct Row {
        int64_t time;
        const char* sha1;
        const char* sha256;
        const char* sha512;
    };
    const Row rows[] = {
        {59, "94287082", "46119246", "90693936"},
        {1111111109, "07081804", "68084774", "25091201"},
        {1111111111, "14050471", "67062674", "99943326"},
        {1234567890, "89005924", "91819424", "93441116"},
        {2000000000, "69279037", "90698825", "38618901"},
    };
    for (const Row& r : rows) {
        EXPECT_EQ(s(crypto::totp::generate(rfc_secret, at(r.time), {.digits = 8})), r.sha1);
        EXPECT_EQ(s(crypto::totp::generate(view(s32), at(r.time), {.algorithm = crypto::hash_id::sha256, .digits = 8})), r.sha256);
        EXPECT_EQ(s(crypto::totp::generate(view(s64), at(r.time), {.algorithm = crypto::hash_id::sha512, .digits = 8})), r.sha512);
        auto v = crypto::totp::verify(rfc_secret, sgcl::string(r.sha1), at(r.time), {.digits = 8});
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, uint64_t(r.time) / 30);
    }
    // the row of 20000000000 (year 2603): its step through HOTP
    EXPECT_EQ(s(crypto::hotp::generate(rfc_secret, 0x27BC86AA, {.digits = 8})), "65353130");
    EXPECT_EQ(s(crypto::hotp::generate(view(s32), 0x27BC86AA, {.algorithm = crypto::hash_id::sha256, .digits = 8})), "77737706");
    EXPECT_EQ(s(crypto::hotp::generate(view(s64), 0x27BC86AA, {.algorithm = crypto::hash_id::sha512, .digits = 8})), "47863826");
}

TEST(Crypto_Otp, AgainstPython) {
    random_source r(6238);
    const std::pair<crypto::hash_id, const char*> algorithms[] = {
        {crypto::hash_id::sha1, "sha1"}, {crypto::hash_id::sha256, "sha256"}, {crypto::hash_id::sha512, "sha512"}};
    for (int k = 0; k < 6; ++k) {
        bytes_t secret = r.bytes(1 + r.below(100));
        auto [id, name] = algorithms[k % 3];
        uint32_t digits = 6 + uint32_t(r.below(5));
        uint64_t first = r.below(1u << 30) * (k % 2 ? 1u << 20 : 1u);
        auto codes = python_codes(secret, first, 20, name, int(digits));
        if (codes.empty()) {
            GTEST_SKIP() << "no python3";
        }
        ASSERT_EQ(codes.size(), 20u);
        for (int i = 0; i < 20; ++i) {
            EXPECT_EQ(s(crypto::hotp::generate(view(secret), first + uint64_t(i), {.algorithm = id, .digits = digits})), codes[i]);
        }
    }
}

TEST(Crypto_Otp, Windows) {
    // HOTP looks ahead skew counters, never behind
    auto c5 = crypto::hotp::generate(rfc_secret, 5);
    EXPECT_EQ(crypto::hotp::verify(rfc_secret, 3, c5, {.skew = 2}).value_or(99), 5u);
    EXPECT_FALSE(crypto::hotp::verify(rfc_secret, 3, c5, {.skew = 1}).has_value());
    EXPECT_FALSE(crypto::hotp::verify(rfc_secret, 6, c5, {.skew = 10}).has_value());
    EXPECT_EQ(crypto::hotp::verify(rfc_secret, 5, c5, {.skew = 0}).value_or(99), 5u);
    // the counter's end: no wrap past 2^64 - 1
    auto last = crypto::hotp::generate(rfc_secret, UINT64_MAX);
    EXPECT_EQ(crypto::hotp::verify(rfc_secret, UINT64_MAX - 1, last, {.skew = 5}).value_or(0), UINT64_MAX);
    // TOTP: skew periods either side
    const int64_t t = 1111111111;
    auto before = crypto::totp::generate(rfc_secret, at(t - 30));
    auto after = crypto::totp::generate(rfc_secret, at(t + 30));
    auto far = crypto::totp::generate(rfc_secret, at(t + 60));
    EXPECT_EQ(crypto::totp::verify(rfc_secret, before, at(t)).value_or(0), uint64_t(t - 30) / 30);
    EXPECT_EQ(crypto::totp::verify(rfc_secret, after, at(t)).value_or(0), uint64_t(t + 30) / 30);
    EXPECT_FALSE(crypto::totp::verify(rfc_secret, far, at(t)).has_value());
    EXPECT_FALSE(crypto::totp::verify(rfc_secret, before, at(t), {.skew = 0}).has_value());
    EXPECT_TRUE(crypto::totp::verify(rfc_secret, far, at(t), {.skew = 2}).has_value());
    // the epoch's first period, skew below it
    auto zero = crypto::totp::generate(rfc_secret, at(0));
    EXPECT_EQ(crypto::totp::verify(rfc_secret, zero, at(10), {.skew = 3}).value_or(99), 0u);
    // the period
    EXPECT_EQ(s(crypto::totp::generate(rfc_secret, at(119), {.period = 60})), s(crypto::hotp::generate(rfc_secret, 1)));
    // now: the code of now verifies now
    EXPECT_TRUE(crypto::totp::verify(rfc_secret, crypto::totp::generate(rfc_secret)).has_value());
}

TEST(Crypto_Otp, CodesThatAreNot) {
    for (const char* code : {"", "75522", "7552244", "75522a", " 755224", "755224 ", "-55224", "７55224"}) {
        EXPECT_FALSE(crypto::hotp::verify(rfc_secret, 0, sgcl::string(code)).has_value()) << code;
    }
    EXPECT_FALSE(crypto::hotp::verify(rfc_secret, 0, "755225").has_value());
    EXPECT_FALSE(crypto::totp::verify(rfc_secret, "000000", at(-100)).has_value());
}

TEST(Crypto_Otp, OptionsOutOfRange) {
    EXPECT_THROW(crypto::hotp::generate(rfc_secret, 0, {.algorithm = crypto::hash_id::sha384}), std::invalid_argument);
    EXPECT_THROW(crypto::hotp::generate(rfc_secret, 0, {.digits = 5}), std::invalid_argument);
    EXPECT_THROW(crypto::hotp::generate(rfc_secret, 0, {.digits = 11}), std::invalid_argument);
    EXPECT_THROW(crypto::totp::generate(rfc_secret, at(59), {.period = 0}), std::invalid_argument);
    EXPECT_THROW(crypto::totp::generate(rfc_secret, at(-1)), std::invalid_argument);
    EXPECT_THROW((void)crypto::hotp::verify(rfc_secret, 0, "123456", {.digits = 4}), std::invalid_argument);
    // ten digits: the 31 bits whole
    EXPECT_EQ(s(crypto::hotp::generate(rfc_secret, 0, {.digits = 10})).size(), 10u);
    EXPECT_EQ(s(crypto::hotp::generate(rfc_secret, 0, {.digits = 10})), "1284755224");
    // an empty secret is HMAC's empty key: allowed, as the RFCs do not forbid it
    EXPECT_EQ(s(crypto::hotp::generate(sgcl::slice<const byte>(), 0)).size(), 6u);
}

TEST(Crypto_Otp, UriReadAndWritten) {
    // Google's example
    auto k = crypto::otp_key::parse("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example");
    ASSERT_TRUE(k.has_value());
    EXPECT_EQ(k->type, crypto::otp_type::totp);
    EXPECT_EQ(s(k->issuer), "Example");
    EXPECT_EQ(s(k->account), "alice@google.com");
    EXPECT_EQ(hex(k->secret), "48656c6c6f21deadbeef");
    EXPECT_EQ(k->options.digits, 6u);
    EXPECT_EQ(k->options.period, 30u);
    EXPECT_EQ(s(k->to_string()), "otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example");
    // every parameter, an HOTP key, percent-encoding, the issuer only in the label, lower case
    auto h = crypto::otp_key::parse("otpauth://hotp/ACME%20Co:%20john%2Bdoe@x.org?secret=jbswy3dpehpk3pxp====&algorithm=sha256&digits=8&counter=42");
    ASSERT_TRUE(h.has_value());
    EXPECT_EQ(h->type, crypto::otp_type::hotp);
    EXPECT_EQ(s(h->issuer), "ACME Co");
    EXPECT_EQ(s(h->account), "john+doe@x.org");
    EXPECT_EQ(h->options.algorithm, crypto::hash_id::sha256);
    EXPECT_EQ(h->options.digits, 8u);
    EXPECT_EQ(h->counter, 42u);
    EXPECT_EQ(s(h->to_string()), "otpauth://hotp/ACME%20Co:john%2Bdoe@x.org?secret=JBSWY3DPEHPK3PXP&issuer=ACME%20Co&algorithm=SHA256&digits=8&counter=42");
    EXPECT_EQ(s(h->code()), s(crypto::hotp::generate(h->secret, 42, {.algorithm = crypto::hash_id::sha256, .digits = 8})));
    EXPECT_EQ(h->verify(h->code()).value_or(0), 42u);
    // the issuer as a parameter only, a period, '+' as a space in a value
    auto p = crypto::otp_key::parse("OTPAUTH://TOTP/bob?secret=JBSWY3DPEHPK3PXP&issuer=Big+Corp&period=60&image=https://x/y.png");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(s(p->issuer), "Big Corp");
    EXPECT_EQ(s(p->account), "bob");
    EXPECT_EQ(p->options.period, 60u);
    EXPECT_EQ(s(p->to_string()), "otpauth://totp/Big%20Corp:bob?secret=JBSWY3DPEHPK3PXP&issuer=Big%20Corp&period=60");
    // read back what was written
    crypto::otp_key g = crypto::otp_key::generate("Example Inc.", "carol@example.com", {.algorithm = crypto::hash_id::sha512, .digits = 7});
    EXPECT_EQ(g.secret.size(), 20u);
    auto back = crypto::otp_key::parse(g.to_string());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(hex(back->secret), hex(g.secret));
    EXPECT_EQ(s(back->issuer), "Example Inc.");
    EXPECT_EQ(s(back->account), "carol@example.com");
    EXPECT_EQ(back->options.algorithm, crypto::hash_id::sha512);
    EXPECT_EQ(back->options.digits, 7u);
    // the colon encoded as the separator; a colon in an account without an issuer kept
    auto e = crypto::otp_key::parse("otpauth://totp/Corp%3Adave?secret=JBSWY3DPEHPK3PXP");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(s(e->issuer), "Corp");
    EXPECT_EQ(s(e->account), "dave");
    crypto::otp_key colon = e->clone();
    colon.issuer = "";
    colon.account = "x:y";
    auto again = crypto::otp_key::parse(colon.to_string());
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(s(again->issuer), "");
    EXPECT_EQ(s(again->account), "x:y");
    // the throwing constructor, clone
    crypto::otp_key c(sgcl::string("otpauth://totp/a?secret=JBSWY3DPEHPK3PXP"));
    crypto::otp_key d = c.clone();
    EXPECT_EQ(hex(d.secret), hex(c.secret));
    EXPECT_THROW(crypto::otp_key(sgcl::string("otpauth://totp/a")), sgcl::bad_expected_access<crypto::error>);
}

TEST(Crypto_Otp, UrisRefused) {
    const std::pair<const char*, crypto::errc> cases[] = {
        {"", crypto::errc::malformed},
        {"http://totp/a?secret=JBSWY3DPEHPK3PXP", crypto::errc::malformed},
        {"otpauth://totp", crypto::errc::malformed},
        {"otpauth://motp/a?secret=JBSWY3DPEHPK3PXP", crypto::errc::unsupported},
        {"otpauth://totp/a", crypto::errc::malformed},
        {"otpauth://totp/a?issuer=x", crypto::errc::malformed},
        {"otpauth://totp/a?secret=", crypto::errc::malformed},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PX1", crypto::errc::malformed},
        {"otpauth://totp/a%2?secret=JBSWY3DPEHPK3PXP", crypto::errc::malformed},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&issuer=%zz", crypto::errc::malformed},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&algorithm=MD5", crypto::errc::unsupported},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&digits=5", crypto::errc::unsupported},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&digits=11", crypto::errc::unsupported},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&digits=six", crypto::errc::malformed},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&period=0", crypto::errc::malformed},
        {"otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&period=99999999999", crypto::errc::malformed},
        {"otpauth://hotp/a?secret=JBSWY3DPEHPK3PXP", crypto::errc::malformed},
        {"otpauth://hotp/a?secret=JBSWY3DPEHPK3PXP&counter=-1", crypto::errc::malformed},
        {"otpauth://hotp/a?secret=JBSWY3DPEHPK3PXP&counter=99999999999999999999999", crypto::errc::malformed},
    };
    for (auto& [uri, code] : cases) {
        auto k = crypto::otp_key::parse(sgcl::string(uri));
        ASSERT_FALSE(k.has_value()) << uri;
        EXPECT_EQ(k.error().code(), code) << uri;
    }
}
