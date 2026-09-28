//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HMAC, HKDF and PBKDF2: the vectors of RFC 4231 (HMAC-SHA-224…512), RFC
// 5869 (HKDF), RFC 6070 (PBKDF2-HMAC-SHA1) and RFC 7914 §11 (PBKDF2-HMAC-
// SHA256); OpenSSL on random keys of every length around the blocks, on
// random messages fed in pieces, on random salts, infos and output sizes;
// verify in constant time; the secrets: no copy, a move zeroes the source,
// the destructor zeroes the object (a probe reads the storage after it).
// Wycheproof's files are read when they lie in
// ~/Programming/oracles/wycheproof/testvectors_v1 and skipped otherwise.
#include "digest_common.h"

#include "sgcl/encoding/json.h"

#include <cstdlib>
#include <fstream>
#include <new>
#include <sstream>

using namespace crypto_test;

namespace {
    template<class H>
    std::string mac(const bytes_t& key, const bytes_t& msg) {
        return hex(crypto::hmac<H>::of(view(msg), view(key)));
    }

    template<class T>
    bool all_zero(const T& object) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&object);
        for (size_t i = 0; i < sizeof(T); ++i) {
            if (p[i] != 0) {
                return false;
            }
        }
        return true;
    }

    template<class H>
    class Crypto_Hmac : public ::testing::Test {};

    TYPED_TEST_SUITE(Crypto_Hmac, Digests);
}

// RFC 4231 §4, all seven cases for the four SHA-2 digests (case 5's tag
// is cut to 128 bits by the RFC)
TEST(Crypto_Hmac, Rfc4231) {
    struct Case {
        bytes_t key, data;
        const char *h224, *h256, *h384, *h512;
    };
    const Case cases[] = {
        {repeat(0x0b, 20), text("Hi There"),
         "896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22",
         "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
         "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6",
         "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854"},
        {text("Jefe"), text("what do ya want for nothing?"),
         "a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44",
         "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
         "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649",
         "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737"},
        {repeat(0xaa, 20), repeat(0xdd, 50),
         "7fb3cb3588c6c1f6ffa9694d7d6ad2649365b0c1f65d69d1ec8333ea",
         "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
         "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27",
         "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb"},
        {unhex("0102030405060708090a0b0c0d0e0f10111213141516171819"), repeat(0xcd, 50),
         "6c11506874013cac6a2abc1bb382627cec6a90d86efc012de7afec5a",
         "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
         "3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e1f573b4e6801dd23c4a7d679ccf8a386c674cffb",
         "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3dba91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd"},
        {repeat(0x0c, 20), text("Test With Truncation"),
         "0e2aea68a90c8d37c988bcdb9fca6fa8", "a3b6167473100ee06e0c796c2955552b",
         "3abf34c3503b2a23a46efc619baef897", "415fad6271580a531d4179bc891d87a6"},
        {repeat(0xaa, 131), text("Test Using Larger Than Block-Size Key - Hash Key First"),
         "95e9a0db962095adaebe9b2d6f0dbce2d499f112f2d2b7273fa6870e",
         "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
         "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952",
         "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598"},
        {repeat(0xaa, 131),
         text("This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm."),
         "3a854166ac5d9f023f54d517d0b39dbd946770db9c2b95c9f6f565d1",
         "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
         "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5a678cc31e799176d3860e6110c46523e",
         "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58"},
    };
    int n = 0;
    for (const Case& c : cases) {
        SCOPED_TRACE("case " + std::to_string(++n));
        EXPECT_EQ(mac<crypto::sha224>(c.key, c.data).substr(0, std::strlen(c.h224)), c.h224);
        EXPECT_EQ(mac<crypto::sha256>(c.key, c.data).substr(0, std::strlen(c.h256)), c.h256);
        EXPECT_EQ(mac<crypto::sha384>(c.key, c.data).substr(0, std::strlen(c.h384)), c.h384);
        EXPECT_EQ(mac<crypto::sha512>(c.key, c.data).substr(0, std::strlen(c.h512)), c.h512);
        // and the aliases, in pieces of 3
        crypto::hmac_sha256 m(view(c.key));
        feed(m, c.data, 3);
        EXPECT_EQ(hex(m.value()).substr(0, std::strlen(c.h256)), c.h256);
    }
}

// Keys of every length 0…300 (under, at and past each digest's block, a
// long key hashed first), random messages, at once and in pieces
TYPED_TEST(Crypto_Hmac, AgainstOpenSsl) {
    using H = TypeParam;
    random_source r(11);
    for (size_t k = 0; k <= 300; ++k) {
        bytes_t key = r.bytes(k);
        bytes_t msg = r.bytes(r.below(700));
        std::string expected = hex(ossl_hmac<H>(key, msg));
        ASSERT_EQ(mac<H>(key, msg), expected) << "key " << k << " message " << msg.size();
        crypto::hmac<H> m(view(key));
        feed(m, msg, 1 + k % 7);
        ASSERT_EQ(hex(m.value()), expected) << "key " << k << " in pieces of " << (1 + k % 7);
    }
    for (int i = 0; i < 100; ++i) {
        bytes_t key = r.bytes(r.below(200));
        bytes_t msg = r.bytes(r.below(3000));
        crypto::hmac<H> m(view(key));
        feed_random(m, msg, r, 500);
        ASSERT_EQ(hex(m.value()), hex(ossl_hmac<H>(key, msg)));
    }
}

// value() ends nothing, reset() starts the message again under the same
// key, clone() branches, verify() compares in constant time
TYPED_TEST(Crypto_Hmac, ShapeAndVerify) {
    using H = TypeParam;
    static_assert(!std::is_copy_constructible_v<crypto::hmac<H>>);
    static_assert(!std::is_copy_assignable_v<crypto::hmac<H>>);
    static_assert(std::is_nothrow_move_constructible_v<crypto::hmac<H>>);
    static_assert(sgcl::hash::req::hasher<crypto::hmac<H>>);
    static_assert(crypto::hmac<H>::digest_size == H::digest_size);

    bytes_t key = text("key"), a = text("first part "), b = text("second part");
    bytes_t ab = a;
    ab.insert(ab.end(), b.begin(), b.end());
    crypto::hmac<H> m("key");
    m.update(view(a));
    EXPECT_EQ(hex(m.value()), mac<H>(key, a));
    crypto::hmac<H> branch = m.clone();
    m.update(view(b));
    EXPECT_EQ(hex(m.value()), mac<H>(key, ab));
    EXPECT_EQ(hex(branch.value()), mac<H>(key, a));
    EXPECT_EQ(hex(m.digest()), hex(m.value()));

    auto tag = m.value();
    EXPECT_TRUE(m.verify(tag));
    for (size_t i = 0; i < tag.size(); ++i) {
        auto bad = tag;
        bad[i] ^= byte(0x01);
        EXPECT_FALSE(m.verify(bad)) << "bit flipped in byte " << i;
        bad = tag;
        bad[i] ^= byte(0x80);
        EXPECT_FALSE(m.verify(bad)) << "top bit flipped in byte " << i;
    }
    EXPECT_FALSE(m.verify(tag.as_slice(0, tag.size() - 1)));   // cut
    bytes_t longer(tag.size() + 1);
    std::memcpy(longer.data(), tag.data(), tag.size());
    EXPECT_FALSE(m.verify(view(longer)));
    EXPECT_FALSE(m.verify(view(bytes_t())));

    m.reset();
    EXPECT_EQ(hex(m.value()), mac<H>(key, bytes_t()));
    m.update(view(ab));
    EXPECT_TRUE(m.verify(tag));
}

// The secret leaves nothing behind: the object moved from is zeroed, and so
// is the storage of one destroyed (read through the bytes it lay in)
TYPED_TEST(Crypto_Hmac, ZeroedWhenMovedAndDestroyed) {
    using H = TypeParam;
    using M = crypto::hmac<H>;
    M a("a key that must not stay in memory");
    a.update("message");
    std::string tag = hex(a.value());
    EXPECT_FALSE(all_zero(a));
    M b(std::move(a));
    EXPECT_TRUE(all_zero(a));
    EXPECT_EQ(hex(b.value()), tag);
    M c("other");
    c = std::move(b);
    EXPECT_TRUE(all_zero(b));
    EXPECT_EQ(hex(c.value()), tag);

    alignas(M) unsigned char storage[sizeof(M)];
    M* p = new (storage) M("key");
    p->update("x");
    bool any = false;
    for (unsigned char x : storage) {
        any |= x != 0;
    }
    EXPECT_TRUE(any);
    p->~M();
    for (size_t i = 0; i < sizeof storage; ++i) {
        ASSERT_EQ(storage[i], 0) << "byte " << i << " of " << sizeof storage;
    }
}

// RFC 5869 appendix A, all seven: cases 1–3 (SHA-256), 4–7 (SHA-1; 7
// with the salt left out, which is an empty one here)
TEST(Crypto_Hkdf, Rfc5869) {
    struct Case {
        bool sha1;
        bytes_t ikm, salt, info;
        size_t length;
        const char *prk, *okm;
    };
    auto range = [](int from, int to) {
        bytes_t v;
        for (int i = from; i < to; ++i) {
            v.push_back((unsigned char)i);
        }
        return v;
    };
    const Case cases[] = {
        {false, repeat(0x0b, 22), range(0, 13), range(0xf0, 0xfa), 42,
         "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5",
         "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"},
        {false, range(0, 0x50), range(0x60, 0xb0), range(0xb0, 0x100), 82,
         "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244",
         "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87"},
        {false, repeat(0x0b, 22), {}, {}, 42,
         "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04",
         "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"},
        {true, repeat(0x0b, 11), range(0, 13), range(0xf0, 0xfa), 42,
         "9b6c18c432a7bf8f0e71c8eb88f4b30baa2ba243",
         "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2c22e422478d305f3f896"},
        {true, range(0, 0x50), range(0x60, 0xb0), range(0xb0, 0x100), 82,
         "8adae09a2a307059478d309b26c4115a224cfaf6",
         "0bd770a74d1160f7c9f12cd5912a06ebff6adcae899d92191fe4305673ba2ffe8fa3f1a4e5ad79f3f334b3b202b2173c486ea37ce3d397ed034c7f9dfeb15c5e927336d0441f4c4300e2cff0d0900b52d3b4"},
        {true, repeat(0x0b, 22), {}, {}, 42,
         "da8c8a73c7fa77288ec6f5e7c297786aa0d32d01",
         "0ac1af7002b3d761d1e55298da9d0506b9ae52057220a306e07b6b87e8df21d0ea00033de03984d34918"},
        {true, repeat(0x0c, 22), {}, {}, 42,
         "2adccada18779e7c2077ad2eb19d3f3e731385dd",
         "2c91117204d745f3500d636a62f64f0ab3bae548aa53d423b0d1f27ebba6f5e5673a081d70cce7acfc48"},
    };
    int n = 0;
    for (const Case& c : cases) {
        SCOPED_TRACE("case " + std::to_string(++n));
        if (c.sha1) {
            using K = crypto::hkdf<crypto::sha1>;
            auto prk = K::extract(view(c.salt), view(c.ikm));
            EXPECT_EQ(hex(prk.bytes()), c.prk);
            EXPECT_EQ(hex(K::expand(prk, view(c.info), c.length)), c.okm);
            EXPECT_EQ(hex(K::derive(view(c.salt), view(c.ikm), view(c.info), c.length)), c.okm);
        } else {
            using K = crypto::hkdf_sha256;
            auto prk = K::extract(view(c.salt), view(c.ikm));
            EXPECT_EQ(hex(prk.bytes()), c.prk);
            EXPECT_EQ(hex(K::expand(prk, view(c.info), c.length)), c.okm);
            EXPECT_EQ(hex(K::expand(prk.bytes(), view(c.info), c.length)), c.okm);   // the PRK as bytes
            EXPECT_EQ(hex(K::derive(view(c.salt), view(c.ikm), view(c.info), c.length)), c.okm);
            bytes_t out(c.length);
            K::derive_to(out_view(out), view(c.salt), view(c.ikm), view(c.info));
            EXPECT_EQ(hex(out), c.okm);
        }
    }
}

template<class H>
void hkdf_against_openssl() {
    using K = crypto::hkdf<H>;
    random_source r(12);
    for (int i = 0; i < 300; ++i) {
        bytes_t salt = r.bytes(r.below(3) == 0 ? 0 : r.below(200));
        bytes_t ikm = r.bytes(r.below(300));
        bytes_t info = r.bytes(r.below(3) == 0 ? 0 : r.below(300));
        size_t n = i < 8 ? size_t(i) : 1 + r.below(K::max_size);
        ASSERT_EQ(hex(K::derive(view(salt), view(ikm), view(info), n)), hex(ossl_hkdf<H>(salt, ikm, info, n)))
            << "salt " << salt.size() << " ikm " << ikm.size() << " info " << info.size() << " n " << n;
    }
    // the most one PRK gives, and one byte past it
    bytes_t ikm = r.bytes(32);
    EXPECT_EQ(hex(K::derive("", view(ikm), "x", K::max_size)), hex(ossl_hkdf<H>({}, ikm, text("x"), K::max_size)));
    EXPECT_THROW((void)K::derive("", view(ikm), "x", K::max_size + 1), std::invalid_argument);
    auto prk = K::extract("salt", view(ikm));
    bytes_t big(K::max_size + 1);
    EXPECT_THROW(K::expand_to(out_view(big), prk, "x"), std::invalid_argument);
}

TEST(Crypto_Hkdf, AgainstOpenSsl) {
    hkdf_against_openssl<crypto::sha256>();
    hkdf_against_openssl<crypto::sha1>();
    hkdf_against_openssl<crypto::sha384>();
    hkdf_against_openssl<crypto::sha512>();
    hkdf_against_openssl<crypto::sha3_256>();
}

TEST(Crypto_Hkdf, ThePrkIsASecret) {
    using K = crypto::hkdf_sha256;
    using P = K::prk;
    static_assert(!std::is_copy_constructible_v<P>);
    static_assert(!std::is_copy_assignable_v<P>);
    P a = K::extract("salt", "input keying material");
    std::string bytes = hex(a.bytes());
    P b(std::move(a));
    EXPECT_TRUE(all_zero(a));
    EXPECT_EQ(hex(b.bytes()), bytes);
    P c = b.clone();
    EXPECT_EQ(hex(c.bytes()), bytes);

    alignas(P) unsigned char storage[sizeof(P)];
    P* p = new (storage) P(K::extract("salt", "ikm"));
    EXPECT_FALSE(all_zero(*p));
    p->~P();
    for (unsigned char x : storage) {
        ASSERT_EQ(x, 0);
    }
}

// RFC 6070 (HMAC-SHA1; the case of 16 777 216 iterations is left to a
// Release build) and RFC 7914 §11 (HMAC-SHA256)
TEST(Crypto_Pbkdf2, Rfc6070AndRfc7914) {
    using P1 = crypto::pbkdf2<crypto::sha1>;
    EXPECT_EQ(hex(P1::derive("password", "salt", 1, 20)), "0c60c80f961f0e71f3a9b524af6012062fe037a6");
    EXPECT_EQ(hex(P1::derive("password", "salt", 2, 20)), "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957");
    EXPECT_EQ(hex(P1::derive("password", "salt", 4096, 20)), "4b007901b765489abead49d926f721d065a429c1");
    EXPECT_EQ(hex(P1::derive("passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 25)),
              "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038");
    EXPECT_EQ(hex(P1::derive(view(text(std::string("pass\0word", 9))), view(text(std::string("sa\0lt", 5))), 4096, 16)),
              "56fa6aa75548099dcc37d7f03425e0c3");
#if defined(NDEBUG)
    EXPECT_EQ(hex(P1::derive("password", "salt", 16777216, 20)), "eefe3d61cd4da4e4e9945b3d6ba2158c2634e984");
#endif
    using P256 = crypto::pbkdf2<crypto::sha256>;
    EXPECT_EQ(hex(P256::derive("passwd", "salt", 1, 64)),
              "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    EXPECT_EQ(hex(P256::derive("Password", "NaCl", 80000, 64)),
              "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
    bytes_t out(64);
    P256::derive_to(out_view(out), "passwd", "salt", 1);
    EXPECT_EQ(hex(out), "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    EXPECT_THROW((void)P256::derive("p", "s", 0, 32), std::invalid_argument);
    EXPECT_EQ(P256::derive("p", "s", 1, 0).size(), 0u);
}

template<class H>
void pbkdf2_against_openssl() {
    random_source r(13);
    for (int i = 0; i < 60; ++i) {
        bytes_t password = r.bytes(r.below(200));
        bytes_t salt = r.bytes(r.below(100));
        uint32_t iterations = 1 + uint32_t(r.below(i < 50 ? 20 : 2000));
        size_t n = r.below(3 * H::digest_size + 5);
        ASSERT_EQ(hex(crypto::pbkdf2<H>::derive(view(password), view(salt), iterations, n)),
                  hex(ossl_pbkdf2<H>(password, salt, iterations, n)))
            << "password " << password.size() << " salt " << salt.size() << " iterations " << iterations << " n " << n;
    }
}

TEST(Crypto_Pbkdf2, AgainstOpenSsl) {
    pbkdf2_against_openssl<crypto::sha1>();
    pbkdf2_against_openssl<crypto::sha256>();
    pbkdf2_against_openssl<crypto::sha512>();
    pbkdf2_against_openssl<crypto::sha3_256>();
}

// --- Wycheproof ----------------------------------------------------------------

namespace {
    // A Wycheproof file of testvectors_v1, parsed, or null when absent
    optional<sgcl::encoding::json> wycheproof(const char* name) {
        const char* home = std::getenv("HOME");
        if (!home) {
            return nullopt;
        }
        std::ifstream in(std::string(home) + "/Programming/oracles/wycheproof/testvectors_v1/" + name);
        if (!in) {
            return nullopt;
        }
        std::stringstream s;
        s << in.rdbuf();
        auto doc = sgcl::encoding::json::parse(sgcl::string(s.str()));
        if (!doc) {
            ADD_FAILURE() << name << ": " << std::string(doc.error().message().data(), doc.error().message().size());
            return nullopt;
        }
        return *doc;
    }

    std::string field(const sgcl::encoding::json& v, const char* key) {
        auto s = v[sgcl::string(key)].as_string();
        return s ? std::string(s->data(), s->size()) : std::string();
    }
}

// HMAC, HKDF and PBKDF2 over Wycheproof's vectors, when they are on disk:
// the files are named by the digest; a "valid" case must give the tag, an
// "invalid" one must not (a tag cut to tagSize is compared by its prefix)
template<class H>
void wycheproof_hmac(const char* file) {
    auto doc = wycheproof(file);
    if (!doc) {
        GTEST_SKIP() << file << " not on disk";
    }
    size_t cases = 0;
    for (auto& group : (*doc)["testGroups"].elements()) {
        size_t tag_bits = size_t(group["tagSize"].as_int().value_or(0));
        for (auto& t : group["tests"].elements()) {
            bytes_t key = unhex(field(t, "key")), msg = unhex(field(t, "msg"));
            std::string tag = field(t, "tag");
            std::string mine = mac<H>(key, msg).substr(0, tag_bits / 4);
            bool valid = field(t, "result") == "valid";
            EXPECT_EQ(mine == tag, valid) << file << " tcId " << t["tcId"].as_int().value_or(0);
            ++cases;
        }
    }
    EXPECT_GT(cases, 0u);
}

TEST(Crypto_Wycheproof, Hmac) {
    wycheproof_hmac<crypto::sha256>("hmac_sha256_test.json");
    wycheproof_hmac<crypto::sha512>("hmac_sha512_test.json");
    wycheproof_hmac<crypto::sha3_256>("hmac_sha3_256_test.json");
}

template<class H>
void wycheproof_hkdf(const char* file) {
    auto doc = wycheproof(file);
    if (!doc) {
        GTEST_SKIP() << file << " not on disk";
    }
    size_t cases = 0;
    for (auto& group : (*doc)["testGroups"].elements()) {
        for (auto& t : group["tests"].elements()) {
            size_t n = size_t(t["size"].as_int().value_or(0));
            bool valid = field(t, "result") == "valid";
            int id = int(t["tcId"].as_int().value_or(0));
            if (n > crypto::hkdf<H>::max_size) {
                EXPECT_FALSE(valid) << file << " tcId " << id;
                EXPECT_THROW((void)crypto::hkdf<H>::derive(view(unhex(field(t, "salt"))), view(unhex(field(t, "ikm"))),
                                                           view(unhex(field(t, "info"))), n),
                             std::invalid_argument);
            } else {
                auto okm = crypto::hkdf<H>::derive(view(unhex(field(t, "salt"))), view(unhex(field(t, "ikm"))),
                                                   view(unhex(field(t, "info"))), n);
                EXPECT_EQ(hex(okm) == field(t, "okm"), valid) << file << " tcId " << id;
            }
            ++cases;
        }
    }
    EXPECT_GT(cases, 0u);
}

TEST(Crypto_Wycheproof, Hkdf) {
    wycheproof_hkdf<crypto::sha256>("hkdf_sha256_test.json");
    wycheproof_hkdf<crypto::sha512>("hkdf_sha512_test.json");
}

template<class H>
void wycheproof_pbkdf2(const char* file) {
    auto doc = wycheproof(file);
    if (!doc) {
        GTEST_SKIP() << file << " not on disk";
    }
    size_t cases = 0;
    for (auto& group : (*doc)["testGroups"].elements()) {
        for (auto& t : group["tests"].elements()) {
            uint32_t iterations = uint32_t(t["iterationCount"].as_int().value_or(0));
            size_t n = size_t(t["dkLen"].as_int().value_or(0));
            bool valid = field(t, "result") == "valid";
            if (iterations == 0) {
                EXPECT_FALSE(valid);
                continue;
            }
            auto dk = crypto::pbkdf2<H>::derive(view(unhex(field(t, "password"))), view(unhex(field(t, "salt"))), iterations, n);
            EXPECT_EQ(hex(dk) == field(t, "dk"), valid) << file << " tcId " << t["tcId"].as_int().value_or(0);
            ++cases;
        }
    }
    EXPECT_GT(cases, 0u);
}

TEST(Crypto_Wycheproof, Pbkdf2) {
    wycheproof_pbkdf2<crypto::sha256>("pbkdf2_hmacsha256_test.json");
    wycheproof_pbkdf2<crypto::sha512>("pbkdf2_hmacsha512_test.json");
}

// Keys, salts, passwords, messages and labels are parameters of bytes
// (const slice<const byte>&) that take a text as its bytes: a string, a
// text slice, a literal, a std::string_view, and raw arrays, each giving
// what the same bytes give
TEST(Crypto_TextAsBytes, EveryFormGivesTheSameBytes) {
    sgcl::string key = "key";
    std::string_view key_view = "key";
    const unsigned char key_raw[] = {'k', 'e', 'y'};
    std::array<uint8_t, 3> key_array = {'k', 'e', 'y'};
    sgcl::string message = "The quick brown fox";
    auto expected = hex(crypto::hmac<crypto::sha256>::of(view(std::string("The quick brown fox")), view(std::string("key"))));
    EXPECT_EQ(hex(crypto::hmac<crypto::sha256>::of(message, key)), expected);
    EXPECT_EQ(hex(crypto::hmac<crypto::sha256>::of("The quick brown fox", "key")), expected);
    EXPECT_EQ(hex(crypto::hmac<crypto::sha256>::of(message.as_slice(), key_view)), expected);
    EXPECT_EQ(hex(crypto::hmac<crypto::sha256>::of(message, key_raw)), expected);
    EXPECT_EQ(hex(crypto::hmac<crypto::sha256>::of(message, key_array)), expected);
    crypto::hmac<crypto::sha256> mac(key);
    mac.update(message);
    EXPECT_EQ(hex(mac.value()), expected);
    using K = crypto::hkdf<crypto::sha256>;
    EXPECT_EQ(hex(K::derive("salt", "secret", "info", 32)), hex(K::derive(sgcl::string("salt"), sgcl::string("secret"), sgcl::string("info"), 32)));
    using P = crypto::pbkdf2<crypto::sha256>;
    sgcl::string password = "passwd";
    EXPECT_EQ(hex(P::derive(password, "salt", 1, 32)), hex(P::derive("passwd", "salt", 1, 32)));
    EXPECT_EQ(hex(crypto::digest(crypto::hash_id::sha256, message)), hex(crypto::sha256::of("The quick brown fox")));
    EXPECT_EQ(hex(crypto::shake128::of(message, 16)), hex(crypto::shake128::of("The quick brown fox", 16)));
    auto k = crypto::ed25519::private_key::generate();
    auto signature = k.sign(message);
    EXPECT_TRUE(k.public_key().verify("The quick brown fox", signature));
    EXPECT_FALSE(k.public_key().verify("The quick brown fax", signature));
}
