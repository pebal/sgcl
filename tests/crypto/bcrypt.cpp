//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// bcrypt: the vectors of OpenBSD's and Openwall's crypt_blowfish (verified
// against Go's x/crypto/bcrypt when they were taken), hashes Go's
// GenerateFromPassword made, hashes made here read back, and the edges: the
// 72-byte rule both ways, every cost at its limits, the three prefixes
// read and the two refused, strings that are not hashes.
#include "digest_common.h"

#include "sgcl/crypto/bcrypt.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace crypto_test;

namespace {
    crypto::errc code_of(const sgcl::expected<void, crypto::error>& r) {
        return r ? crypto::errc(0) : r.error().code();
    }

    sgcl::string str(const std::string& s) {
        return sgcl::string(s.data(), s.size());
    }

    const std::string long_password = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789chars after 72 are ignored";
}

TEST(Crypto_Bcrypt, KnownHashes) {
    const std::pair<std::string, std::string> vectors[] = {
        {"U*U", "$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW"},
        {"U*U*", "$2a$05$CCCCCCCCCCCCCCCCCCCCC.VGOzA784oUp/Z0DY336zx7pLYAy0lwK"},
        {"U*U*U", "$2a$05$XXXXXXXXXXXXXXXXXXXXXOAcXxm9kjPGEMsLznoKqmqw7tc8WCx4a"},
        {"", "$2a$05$CCCCCCCCCCCCCCCCCCCCC.7uG0VCzI2bS7j6ymqJi9CdcdxiRTWNy"},
        {long_password, "$2a$05$abcdefghijklmnopqrstuu5s2v8.iXieOjg/.AySBTTZIIVFJeBui"},
        // Go's GenerateFromPassword
        {"correct horse battery staple", "$2a$04$i2NfaZfrTcdgu.W3KoKIfefH36540JoT3ZlqV/9YwrMoG8Q5Ag3mq"},
        {std::string(72, 'x'), "$2a$05$mnMSD4Ko/VIHceSLqv7da.ohqv/C5KWNlCb4wHwE59MvPsYVSf4qu"},
        {"", "$2a$04$u0yWLfv3b2wU2M28jFhBd.PC7/TfXWJckF/hKUC1G6QqEdo0DRGBi"},
        {"p\xc3\xa4ssw\xc3\xb6rd", "$2a$07$Dojv794M.I7exGOmXdNC..7dcBqXRdlC.ytJm2OppW71WiVS/z.52"},
    };
    for (auto& [password, hash] : vectors) {
        EXPECT_TRUE(crypto::bcrypt::verify(view(password), str(hash))) << hash;
        // $2b$ and $2y$ are the same function
        for (char v : {'b', 'y'}) {
            std::string other = hash;
            other[2] = v;
            EXPECT_TRUE(crypto::bcrypt::verify(view(password), str(other))) << other;
        }
        if (password.size() < 72) {   // past 72 bytes nothing appended counts
            EXPECT_EQ(code_of(crypto::bcrypt::verify(view(password + "!"), str(hash))), crypto::errc::authentication);
        }
        EXPECT_EQ(*crypto::bcrypt::cost(str(hash)), hash[5] - '0' + 10 * (hash[4] - '0'));
    }
}

TEST(Crypto_Bcrypt, GenerateAndVerify) {
    for (int cost : {4, 5}) {
        auto h = crypto::bcrypt::generate("correct horse battery staple", cost);
        ASSERT_TRUE(h.has_value());
        std::string s(h->data(), h->size());
        EXPECT_EQ(s.size(), 60u);
        EXPECT_EQ(s.substr(0, 7), cost == 4 ? "$2b$04$" : "$2b$05$");
        EXPECT_TRUE(crypto::bcrypt::verify("correct horse battery staple", *h));
        EXPECT_EQ(code_of(crypto::bcrypt::verify("correct horse battery stapl", *h)), crypto::errc::authentication);
        EXPECT_EQ(*crypto::bcrypt::cost(*h), cost);
    }
    // two hashes of one password differ by their salt
    EXPECT_NE(std::string(crypto::bcrypt::generate("x", 4)->data(), 29), std::string(crypto::bcrypt::generate("x", 4)->data(), 29));
    // the empty password
    auto e = crypto::bcrypt::generate("", 4);
    ASSERT_TRUE(e.has_value());
    EXPECT_TRUE(crypto::bcrypt::verify("", *e));
    EXPECT_EQ(code_of(crypto::bcrypt::verify(sgcl::slice<const byte>(), *e)), crypto::errc(0));
}

TEST(Crypto_Bcrypt, The72ByteRule) {
    std::string p72(72, 'a'), p73(73, 'a');
    auto h = crypto::bcrypt::generate(view(p72), 4);
    ASSERT_TRUE(h.has_value());
    auto refused = crypto::bcrypt::generate(view(p73), 4);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), crypto::errc::invalid_key);
    // verify reads 72 bytes, as every implementation does
    EXPECT_TRUE(crypto::bcrypt::verify(view(p73), *h));
    EXPECT_TRUE(crypto::bcrypt::verify(view(long_password.substr(0, 72) + "anything"),
                                       "$2a$05$abcdefghijklmnopqrstuu5s2v8.iXieOjg/.AySBTTZIIVFJeBui"));
    std::string p71(71, 'a');
    EXPECT_EQ(code_of(crypto::bcrypt::verify(view(p71), *h)), crypto::errc::authentication);
}

TEST(Crypto_Bcrypt, Costs) {
    EXPECT_THROW(crypto::bcrypt::generate("pw", 3), std::invalid_argument);
    EXPECT_THROW(crypto::bcrypt::generate("pw", 32), std::invalid_argument);
    EXPECT_THROW(crypto::bcrypt::generate("pw", -1), std::invalid_argument);
    EXPECT_EQ(crypto::bcrypt::min_cost, 4);
    EXPECT_EQ(crypto::bcrypt::max_cost, 31);
    EXPECT_EQ(crypto::bcrypt::default_cost, 10);
}

TEST(Crypto_Bcrypt, StringsRefused) {
    const std::string good = "$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW";
    const std::pair<std::string, crypto::errc> cases[] = {
        {"", crypto::errc::malformed},
        {good.substr(0, 59), crypto::errc::malformed},
        {good + "W", crypto::errc::malformed},
        {"$2c" + good.substr(3), crypto::errc::malformed},
        {"$3a" + good.substr(3), crypto::errc::malformed},
        {"x2a" + good.substr(3), crypto::errc::malformed},
        {"$2x" + good.substr(3), crypto::errc::unsupported},
        {"$2$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW", crypto::errc::unsupported},
        {"$2a$03" + good.substr(6), crypto::errc::malformed},
        {"$2a$32" + good.substr(6), crypto::errc::malformed},
        {"$2a$5$" + good.substr(6), crypto::errc::malformed},
        {"$2a$0a" + good.substr(6), crypto::errc::malformed},
        {"$2a$05#" + good.substr(7), crypto::errc::malformed},
        {"$2a$05$CCCCCCCCCCCCCCCCCCCCC!E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW", crypto::errc::malformed},
        {"$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOe=", crypto::errc::malformed},
    };
    for (auto& [s, code] : cases) {
        EXPECT_EQ(code_of(crypto::bcrypt::verify("U*U", str(s))), code) << s;
        auto c = crypto::bcrypt::cost(str(s));
        ASSERT_FALSE(c.has_value()) << s;
        EXPECT_EQ(c.error().code(), code);
    }
    // the salt's last character carries 4 bits past the 16 bytes, which are ignored as every reader ignores them
    std::string loose = good;
    loose[28] = '/';   // '.' is 0, '/' is 1: a bit past the salt
    EXPECT_TRUE(crypto::bcrypt::verify("U*U", str(loose)));
}
