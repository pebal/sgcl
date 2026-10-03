//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Ed25519: the vectors of RFC 8032 §7.1 (TEST 1, 2, 3, 1024, SHA(abc));
// OpenSSL on random seeds and messages of every length 0…1024 and a few
// large, both ways (the same signature, each verifies the other's); every
// bit of a signature flipped; the edge cases of verification (S of L and
// more, non-canonical A and R, points of small and of mixed order, points
// off the curve), each with the verdict of Go's crypto/ed25519 beside ours
// and both held against a slow reference on big integers; the strict
// decoding of public keys against the reference on random bytes; the keys'
// forms (seed, 64 bytes, DER against OpenSSL's) and the secret zeroed.
// Files of other oracles are read when on disk and skipped otherwise:
// Wycheproof's ed25519_test.json, the cases of "ed25519 speccheck", the
// SUPERCOP vectors Go's source tree carries (crypto/ed25519/testdata/
// sign.input.gz), and a file of Go's own results (SGCL_CRYPTO_GO_VECTORS).
#include "curve25519_common.h"

#include "sgcl/encoding/json.h"

#include <algorithm>
#include <cstdio>
#include <new>
#include <type_traits>

using namespace curve_test;
namespace ed25519 = crypto::ed25519;

static_assert(!std::is_copy_constructible_v<ed25519::private_key>);
static_assert(!std::is_copy_assignable_v<ed25519::private_key>);
static_assert(std::is_nothrow_move_constructible_v<ed25519::private_key>);
static_assert(std::is_copy_constructible_v<ed25519::public_key>);

namespace {
    ed25519::private_key key(const bytes_t& seed) {
        auto k = ed25519::private_key::from_seed(view(seed));
        if (!k) {
            throw std::runtime_error("from_seed");
        }
        return std::move(*k);
    }

    // Our verdict on (pub, msg, sig) for any 32 bytes of pub: a key that
    // does not decode verifies nothing
    bool verify(const bytes_t& pub, const bytes_t& msg, const bytes_t& sig) {
        auto k = ed25519::public_key::from_bytes(view(pub));
        return k && k->verify(view(msg), view(sig));
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

    std::string join(std::initializer_list<const char*> parts) {
        std::string s;
        for (const char* p : parts) {
            s += p;
        }
        return s;
    }
}

// RFC 8032 §7.1: the public key, the signature, its verification
TEST(Crypto_Ed25519, Rfc8032) {
    struct Case {
        const char* name;
        std::string seed, pub, msg, sig;
    };
    auto abc = crypto::sha512::of("abc");
    const Case cases[] = {
        {"TEST 1", "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
         "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
         "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
        {"TEST 2", "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
         "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
         "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
        {"TEST 3", "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
         "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
         "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
        {"TEST 1024", "f5e5767cf153319517630f226876b86c8160cc583bc013744c6bf255f5cc0ee5",
         "278117fc144c72340f67d0f2316e8386ceffbf2b2428c9c51fef7c597f1d426e",
         join({
        "08b8b2b733424243760fe426a4b54908632110a66c2f6591eabd3345e3e4eb98fa6e264bf09efe12ee50f8f54e9f77b1",
        "e355f6c50544e23fb1433ddf73be84d879de7c0046dc4996d9e773f4bc9efe5738829adb26c81b37c93a1b270b20329d",
        "658675fc6ea534e0810a4432826bf58c941efb65d57a338bbd2e26640f89ffbc1a858efcb8550ee3a5e1998bd177e93a",
        "7363c344fe6b199ee5d02e82d522c4feba15452f80288a821a579116ec6dad2b3b310da903401aa62100ab5d1a36553e",
        "06203b33890cc9b832f79ef80560ccb9a39ce767967ed628c6ad573cb116dbefefd75499da96bd68a8a97b928a8bbc10",
        "3b6621fcde2beca1231d206be6cd9ec7aff6f6c94fcd7204ed3455c68c83f4a41da4af2b74ef5c53f1d8ac70bdcb7ed1",
        "85ce81bd84359d44254d95629e9855a94a7c1958d1f8ada5d0532ed8a5aa3fb2d17ba70eb6248e594e1a2297acbbb39d",
        "502f1a8c6eb6f1ce22b3de1a1f40cc24554119a831a9aad6079cad88425de6bde1a9187ebb6092cf67bf2b13fd65f270",
        "88d78b7e883c8759d2c4f5c65adb7553878ad575f9fad878e80a0c9ba63bcbcc2732e69485bbc9c90bfbd62481d9089b",
        "eccf80cfe2df16a2cf65bd92dd597b0707e0917af48bbb75fed413d238f5555a7a569d80c3414a8d0859dc65a46128ba",
        "b27af87a71314f318c782b23ebfe808b82b0ce26401d2e22f04d83d1255dc51addd3b75a2b1ae0784504df543af8969b",
        "e3ea7082ff7fc9888c144da2af58429ec96031dbcad3dad9af0dcbaaaf268cb8fcffead94f3c7ca495e056a9b47acdb7",
        "51fb73e666c6c655ade8297297d07ad1ba5e43f1bca32301651339e22904cc8c42f58c30c04aafdb038dda0847dd988d",
        "cda6f3bfd15c4b4c4525004aa06eeff8ca61783aacec57fb3d1f92b0fe2fd1a85f6724517b65e614ad6808d6f6ee34df",
        "f7310fdc82aebfd904b01e1dc54b2927094b2db68d6f903b68401adebf5a7e08d78ff4ef5d63653a65040cf9bfd4aca7",
        "984a74d37145986780fc0b16ac451649de6188a7dbdf191f64b5fc5e2ab47b57f7f7276cd419c17a3ca8e1b939ae49e4",
        "88acba6b965610b5480109c8b17b80e1b7b750dfc7598d5d5011fd2dcc5600a32ef5b52a1ecc820e308aa342721aac09",
        "43bf6686b64b2579376504ccc493d97e6aed3fb0f9cd71a43dd497f01f17c0e2cb3797aa2a2f256656168e6c496afc5f",
        "b93246f6b1116398a346f1a641f3b041e989f7914f90cc2c7fff357876e506b50d334ba77c225bc307ba537152f3f161",
        "0e4eafe595f6d9d90d11faa933a15ef1369546868a7f3a45a96768d40fd9d03412c091c6315cf4fde7cb68606937380d",
        "b2eaaa707b4c4185c32eddcdd306705e4dc1ffc872eeee475a64dfac86aba41c0618983f8741c5ef68d3a101e8a3b8ca",
        "c60c905c15fc910840b94c00a0b9d0"
         }),
         "0aab4c900501b3e24d7cdf4663326a3a87df5e4843b2cbdb67cbf6e460fec350aa5371b1508f9f4528ecea23c436d94b5e8fcd4f681e30a6ac00a9704a188a03"},
        {"TEST SHA(abc)", "833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
         "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf", hex(abc),
         "dc2a4459e7369633a52b1bf277839a00201009a3efbf3ecb69bea2186c26b58909351fc9ac90b3ecfdfbc7c66431e0303dca179c138ac17ad9bef1177331a704"},
    };
    ASSERT_EQ(cases[3].msg.size(), 2046u);
    ASSERT_EQ(cases[4].msg, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    for (const Case& c : cases) {
        SCOPED_TRACE(c.name);
        auto k = key(unhex(c.seed));
        bytes_t msg = unhex(c.msg);
        EXPECT_EQ(hex(k.public_key().bytes()), c.pub);
        EXPECT_EQ(hex(k.sign(view(msg))), c.sig);
        auto pub = ed25519::public_key::from_bytes(view(unhex(c.pub)));
        ASSERT_TRUE(pub.has_value());
        EXPECT_TRUE(pub->verify(view(msg), view(unhex(c.sig))));
        EXPECT_EQ(hex(k.seed()), c.seed);
    }
}

// The same signatures as OpenSSL (the scheme is deterministic), and each
// side verifies the other's: seeds at random, messages of every length
// 0…1024 and a few large ones
TEST(Crypto_Ed25519, AgainstOpenSsl) {
    random_source r(8032);
    auto one = [&](size_t length) {
        bytes_t seed = r.bytes(32), msg = r.bytes(length);
        auto k = key(seed);
        bytes_t pub = to_bytes(k.public_key().bytes());
        ASSERT_EQ(hex(pub), hex(ossl_ed25519_public(seed))) << length;
        bytes_t mine = to_bytes(k.sign(view(msg)));
        bytes_t theirs = ossl_ed25519_sign(seed, msg);
        ASSERT_EQ(hex(mine), hex(theirs)) << length;
        ASSERT_TRUE(ossl_ed25519_verify(pub, msg, mine)) << length;
        ASSERT_TRUE(k.public_key().verify(view(msg), view(theirs))) << length;
        if (!msg.empty()) {
            msg[r.below(msg.size())] ^= 1;
            ASSERT_FALSE(k.public_key().verify(view(msg), view(mine))) << length;
            ASSERT_FALSE(ossl_ed25519_verify(pub, msg, mine)) << length;
        }
    };
    for (size_t n = 0; n <= 1024; ++n) {
        one(n);
    }
    for (size_t n : {4095, 65536, 1000003}) {
        one(n);
    }
}

// Every bit of a signature flipped, every length other than 64, the
// message changed: false. (A flip in bit 255 of S makes S >= L; a flip
// elsewhere changes R or S.)
TEST(Crypto_Ed25519, EveryBitOfTheSignature) {
    auto k = key(unhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    bytes_t msg = {'s', 'g', 'c', 'l'};
    bytes_t sig = to_bytes(k.sign(view(msg)));
    auto pub = k.public_key();
    ASSERT_TRUE(pub.verify(view(msg), view(sig)));
    for (size_t bit = 0; bit < 512; ++bit) {
        bytes_t bad = sig;
        bad[bit / 8] ^= (unsigned char)(1 << (bit % 8));
        ASSERT_FALSE(pub.verify(view(msg), view(bad))) << bit;
    }
    for (size_t n : {0, 1, 32, 63, 65, 128}) {
        bytes_t other = sig;
        other.resize(n);
        EXPECT_FALSE(pub.verify(view(msg), view(other))) << n;
    }
    EXPECT_FALSE(pub.verify("sgcm", view(sig)));
    EXPECT_TRUE(pub.verify("sgcl", view(sig)));   // text is its bytes
    EXPECT_EQ(hex(k.sign("sgcl")), hex(sig));
    // another key
    auto other = key(bytes_t(32, 7));
    EXPECT_FALSE(other.public_key().verify(view(msg), view(sig)));
}

// The edge cases of verification: the name, the public key, the message,
// the signature, our verdict and Go 1.27's crypto/ed25519.Verify (written
// by edges.py and godiff, the programs the report names). Ours differs
// from Go's and OpenSSL's in two cases only, both a non-canonical encoding
// of A that they reduce and RFC 8032 §5.1.3 refuses — a deliberate
// deviation, docs/sgcl/crypto/ed25519.md. The two are listed in
// expected_differences: a case where Go or OpenSSL differs from us and
// that is not on the list fails, and so does one on the list where they
// agree (a later Go or OpenSSL that refuses them too updates the list, not
// the code). Each case is also judged by the slow reference (strict
// decoding, no cofactor), which must agree with us.
TEST(Crypto_Ed25519, EdgeCases) {
    struct Case {
        const char* name;
        const char* pub;
        const char* msg;
        const char* sig;
        bool ours;
        bool go;
    };
    const Case cases[] = {
        {"valid",
         "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8",
         "65646765",
         "856a2ed09cd799d8512e5cec83e272a53855915510251ef8df50e909184d175bda3ba883d9b4bfa51c148a4bb62388eea31a6afde72d70eaef6ef5719b31f800",
         true, true},
        {"s_plus_l",
         "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8",
         "65646765",
         "856a2ed09cd799d8512e5cec83e272a53855915510251ef8df50e909184d175bc70f9ee0f317d2fdf2b081ee941d6703a41a6afde72d70eaef6ef5719b31f810",
         false, false},
        {"s_high_bit",
         "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8",
         "65646765",
         "856a2ed09cd799d8512e5cec83e272a53855915510251ef8df50e909184d175bda3ba883d9b4bfa51c148a4bb62388eea31a6afde72d70eaef6ef5719b31f880",
         false, false},
        {"identity_a_identity_r_s0",
         "0100000000000000000000000000000000000000000000000000000000000000",
         "65646765",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         true, true},
        {"noncanonical_identity_a",
         "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
         "65646765",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         false, true},
        {"negative_zero_identity_a",
         "0100000000000000000000000000000000000000000000000000000000000080",
         "65646765",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         false, true},
        {"noncanonical_identity_r",
         "0100000000000000000000000000000000000000000000000000000000000000",
         "65646765",
         "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f0000000000000000000000000000000000000000000000000000000000000000",
         false, false},
        {"order8_a_k0",
         "c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a",
         "6f72646572382d39",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         true, true},
        {"order8_a_k1",
         "c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a",
         "6f72646572382d30",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         false, false},
        {"mixed_order_a_k0",
         "b502ff3d92e31d8190b4aa4ea0414005167fad089c4de9dac8a2fc850fed4f58",
         "6d697865642d37",
         "e6e9a01845026e3e0e42e346573efad3c5e649c6dff74b31b5beafe5ac7a31c34eeaf43188bd878296f594434d4b879e99b1a3ddccb3255bd80f9ec5ee5f890a",
         true, true},
        {"mixed_order_a_k1",
         "b502ff3d92e31d8190b4aa4ea0414005167fad089c4de9dac8a2fc850fed4f58",
         "6d697865642d30",
         "7267124b92e356e28d92b859d92ba7dae52c746e31d6921dd4afc8d8593eea2f37644bd4fd7d3e9c9bc3ee8cfd1c16c3b02d9d68e2bf58652d0a2322e651c509",
         false, false},
        {"a_off_curve",
         "0200000000000000000000000000000000000000000000000000000000000000",
         "65646765",
         "856a2ed09cd799d8512e5cec83e272a53855915510251ef8df50e909184d175bda3ba883d9b4bfa51c148a4bb62388eea31a6afde72d70eaef6ef5719b31f800",
         false, false},
        {"noncanonical_y_p_a",
         "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
         "65646765",
         "01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
         false, false},
        {"r_off_curve",
         "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8",
         "65646765",
         "0200000000000000000000000000000000000000000000000000000000000000da3ba883d9b4bfa51c148a4bb62388eea31a6afde72d70eaef6ef5719b31f800",
         false, false},
        {"s_equals_l",
         "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8",
         "65646765",
         "856a2ed09cd799d8512e5cec83e272a53855915510251ef8df50e909184d175bedd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010",
         false, false},
        {"identity_s_equals_l",
         "0100000000000000000000000000000000000000000000000000000000000000",
         "65646765",
         "0100000000000000000000000000000000000000000000000000000000000000edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010",
         false, false},
    };
    const std::string expected_differences[] = {"noncanonical_identity_a", "negative_zero_identity_a"};
    reference ref;
    for (const Case& c : cases) {
        SCOPED_TRACE(c.name);
        bytes_t pub = unhex(c.pub), msg = unhex(c.msg), sig = unhex(c.sig);
        EXPECT_EQ(verify(pub, msg, sig), c.ours);
        EXPECT_EQ(ref.verify(pub, msg, sig), c.ours);
        bool listed = std::find(std::begin(expected_differences), std::end(expected_differences), c.name) != std::end(expected_differences);
        EXPECT_EQ(c.go != c.ours, listed) << "Go " << (c.go ? "accepts" : "rejects");
        bool openssl = ossl_ed25519_verify(pub, msg, sig);
        EXPECT_EQ(openssl != c.ours, listed) << "OpenSSL " << (openssl ? "accepts" : "rejects");
    }
}

// The scalar arithmetic mod L against big integers: sc_reduce of 64 bytes
// (random, and the values around multiples of L where Barrett's estimate
// needs its corrections) and sc_muladd of random scalars
TEST(Crypto_Ed25519, ScalarsAgainstBigIntegers) {
    using big = sgcl::math::big_integer;
    reference ref;
    auto le = [](const big& v, size_t n) {
        auto be = v.to_bytes(n);
        bytes_t out(n);
        for (size_t i = 0; i < n; ++i) {
            out[i] = (unsigned char)be[n - 1 - i];
        }
        return out;
    };
    auto check = [&](const big& x) {
        bytes_t in = le(x, 64);
        unsigned char out[32];
        crypto::detail::sc_reduce(out, in.data());
        ASSERT_EQ(hex(out, 32), hex(le(x.mod(ref.L), 32))) << x.to_string(16).data();
    };
    random_source r(252);
    for (int i = 0; i < 20000; ++i) {
        check(reference::from_le(r.bytes(64).data(), 64));
    }
    big top = (big(1) << 512) - 1;
    for (int k = 0; k < 200; ++k) {
        // q·L + e for q near the largest and for small ones, e around 0 and L
        big q = k < 100 ? top / ref.L - k : big(k);
        for (int e : {-2, -1, 0, 1, 2}) {
            for (const big& base : {q * ref.L, q * ref.L + ref.L - 1}) {
                big x = base + e;
                if (x >= 0 && x <= top) {
                    check(x);
                }
            }
        }
    }
    check(top);
    for (int i = 0; i < 5000; ++i) {
        bytes_t a = r.bytes(32), b = r.bytes(32), c = r.bytes(32);
        a[31] &= 0x1f;   // below 2^253, as k is
        c[31] &= 0x1f;
        unsigned char out[32];
        crypto::detail::sc_muladd(out, a.data(), b.data(), c.data());
        big expected = (reference::from_le(a.data(), 32) * reference::from_le(b.data(), 32) + reference::from_le(c.data(), 32)).mod(ref.L);
        ASSERT_EQ(hex(out, 32), hex(le(expected, 32))) << i;
    }
}

// from_bytes on random 32 bytes (and on each y near p): a key exactly when
// the reference decodes one, and then the same bytes back
TEST(Crypto_Ed25519, StrictDecodingAgainstTheReference) {
    reference ref;
    random_source r(5);
    size_t points = 0;
    auto check = [&](const bytes_t& b) {
        auto k = ed25519::public_key::from_bytes(view(b));
        bool expected = ref.decode(b.data()).has_value();
        ASSERT_EQ(k.has_value(), expected) << hex(b);
        if (k) {
            ++points;
            EXPECT_EQ(hex(k->bytes()), hex(b));
        } else {
            EXPECT_EQ(k.error().code(), crypto::errc::invalid_key);
        }
    };
    for (int i = 0; i < 300; ++i) {
        check(r.bytes(32));
    }
    // y = p - 20 … 2^255 - 1, both signs: the canonical ones below p, the
    // non-canonical from p on
    for (int delta = -20; delta <= 18; ++delta) {
        bytes_t b(32, 0xff);
        b[31] = 0x7f;
        b[0] = (unsigned char)(0xed + delta);   // p = 2^255 - 19 ends in 0xed
        for (int sign = 0; sign < 2; ++sign) {
            bytes_t s = b;
            s[31] |= (unsigned char)(sign << 7);
            check(s);
        }
    }
    // y = 0 … 18 and 1 with the sign bit (-0)
    for (int y = 0; y <= 18; ++y) {
        bytes_t b(32, 0);
        b[0] = (unsigned char)y;
        check(b);
        b[31] = 0x80;
        check(b);
    }
    EXPECT_GT(points, 100u);
}

// Random mutations of good signatures and random signatures under good and
// bad keys, judged by the reference: the fast verification must agree
TEST(Crypto_Ed25519, VerifyAgainstTheReference) {
    reference ref;
    random_source r(99);
    for (int i = 0; i < 60; ++i) {
        bytes_t seed = r.bytes(32), msg = r.bytes(r.below(80));
        auto k = key(seed);
        bytes_t pub = to_bytes(k.public_key().bytes());
        bytes_t sig = to_bytes(k.sign(view(msg)));
        switch (i % 4) {
            case 0: break;                                           // good
            case 1: sig[r.below(64)] ^= (unsigned char)(1 << r.below(8)); break;
            case 2: sig = r.bytes(64); sig[63] &= 0x0f; break;       // random, S < 2^252
            case 3: pub = r.bytes(32); break;                         // another key, maybe no point
        }
        ASSERT_EQ(verify(pub, msg, sig), ref.verify(pub, msg, sig)) << i;
    }
}

TEST(Crypto_Ed25519, KeysOfTheWrongSize) {
    for (size_t n : {0, 1, 31, 33, 64}) {
        bytes_t b(n, 1);
        auto p = ed25519::public_key::from_bytes(view(b));
        ASSERT_FALSE(p.has_value());
        EXPECT_EQ(p.error().code(), crypto::errc::invalid_key);
        auto s = ed25519::private_key::from_seed(view(b));
        ASSERT_FALSE(s.has_value());
        EXPECT_EQ(s.error().code(), crypto::errc::invalid_key);
    }
    for (size_t n : {0, 32, 63, 65}) {
        bytes_t b(n, 1);
        auto k = ed25519::private_key::from_private_bytes(view(b));
        ASSERT_FALSE(k.has_value());
        EXPECT_EQ(k.error().code(), crypto::errc::invalid_key);
    }
}

// The 64 bytes (seed and public key) round-trip; a public half that is not
// the seed's is refused
TEST(Crypto_Ed25519, PrivateBytes) {
    auto k = ed25519::private_key::generate();
    bytes_t b = to_bytes(k.bytes());
    auto back = ed25519::private_key::from_private_bytes(view(b));
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == k);
    EXPECT_EQ(hex(back->public_key().bytes()), hex(k.public_key().bytes()));
    b[40] ^= 1;
    auto bad = ed25519::private_key::from_private_bytes(view(b));
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code(), crypto::errc::invalid_key);
    // generate() gives OpenSSL's public key for its seed
    EXPECT_EQ(hex(k.public_key().bytes()), hex(ossl_ed25519_public(to_bytes(k.seed()))));
    EXPECT_FALSE(k == ed25519::private_key::generate());
}

TEST(Crypto_Ed25519, TheSecretIsZeroed) {
    bytes_t seed = unhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    auto k = key(seed);
    auto c = k.clone();
    EXPECT_TRUE(c == k);
    EXPECT_EQ(hex(c.sign("x")), hex(k.sign("x")));
    auto moved = std::move(k);
    EXPECT_TRUE(all_zero(k));
    EXPECT_TRUE(moved == c);
    ed25519::private_key assigned = ed25519::private_key::generate();
    assigned = std::move(moved);
    EXPECT_TRUE(all_zero(moved));
    EXPECT_TRUE(assigned == c);

    alignas(ed25519::private_key) unsigned char storage[sizeof(ed25519::private_key)];
    auto* probe = new (storage) ed25519::private_key(key(seed));
    EXPECT_FALSE(all_zero(*probe));
    probe->~private_key();
    for (unsigned char b : storage) {
        ASSERT_EQ(b, 0);
    }
}

// A key moved from is used by no operation: std::logic_error, as every
// key of the module (it once signed with the scalar 0, and its public key,
// all zeros, accepted any signature with R = 0)
TEST(Crypto_Ed25519, AKeyMovedFromIsUsedByNothing) {
    auto a = ed25519::private_key::generate();
    auto b = std::move(a);
    EXPECT_THROW((void)a.sign("m"), std::logic_error);
    EXPECT_THROW((void)a.public_key(), std::logic_error);
    EXPECT_THROW((void)a.seed(), std::logic_error);
    EXPECT_THROW((void)a.bytes(), std::logic_error);
    EXPECT_THROW((void)a.to_pkcs8_der(), std::logic_error);
    auto sig = b.sign("m");
    EXPECT_TRUE(b.public_key().verify("m", sig));
    // clone() and == too: they once gave another key moved from and
    // compared its zeros
    EXPECT_THROW((void)a.clone(), std::logic_error);
    EXPECT_THROW((void)(a == b), std::logic_error);
    EXPECT_THROW((void)(b == a), std::logic_error);
    EXPECT_FALSE(noexcept(a.clone()));
    EXPECT_FALSE(noexcept(a == b));
}

TEST(Crypto_Ed25519, DerAgainstOpenSsl) {
    random_source r(112);
    for (int i = 0; i < 50; ++i) {
        bytes_t seed = r.bytes(32);
        auto k = key(seed);
        bytes_t pkcs8 = to_bytes(k.to_pkcs8_der());
        bytes_t pkix = to_bytes(k.public_key().to_pkix_der());
        ASSERT_EQ(hex(pkcs8), hex(ossl_pkcs8(EVP_PKEY_ED25519, seed)));
        ASSERT_EQ(hex(pkix), hex(ossl_pkix(EVP_PKEY_ED25519, ossl_ed25519_public(seed))));
        EXPECT_EQ(hex(ossl_read_pkcs8(pkcs8)), hex(seed));
        auto back = ed25519::private_key::from_pkcs8_der(view(pkcs8));
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == k);
        auto pb = ed25519::public_key::from_pkix_der(view(pkix));
        ASSERT_TRUE(pb.has_value());
        EXPECT_TRUE(*pb == k.public_key());
    }
    // an X25519 key is unsupported here; a SubjectPublicKeyInfo whose key
    // is no point is invalid_key
    bytes_t x = ossl_pkcs8(EVP_PKEY_X25519, bytes_t(32, 3));
    auto wrong = ed25519::private_key::from_pkcs8_der(view(x));
    ASSERT_FALSE(wrong.has_value());
    EXPECT_EQ(wrong.error().code(), crypto::errc::unsupported);
    bytes_t pkix = to_bytes(key(bytes_t(32, 1)).public_key().to_pkix_der());
    std::memset(pkix.data() + 12, 0xff, 32);   // y = 2^255 - 1 and the sign: not canonical
    auto nopoint = ed25519::public_key::from_pkix_der(view(pkix));
    ASSERT_FALSE(nopoint.has_value());
    EXPECT_EQ(nopoint.error().code(), crypto::errc::invalid_key);
    // version 1 with the public key [1]: the seed's is read, another's is
    // invalid_key; a SubjectPublicKeyInfo with an element after the key is
    // malformed
    bytes_t seed1(32, 1);
    bytes_t pub1 = to_bytes(key(seed1).public_key().bytes());
    auto v1 = [&](const bytes_t& p) {
        bytes_t body = {0x02, 0x01, 0x01, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x04, 0x22, 0x04, 0x20};
        body.insert(body.end(), seed1.begin(), seed1.end());
        body.insert(body.end(), {0x81, 0x21, 0x00});
        body.insert(body.end(), p.begin(), p.end());
        bytes_t der = {0x30, (unsigned char)body.size()};
        der.insert(der.end(), body.begin(), body.end());
        return der;
    };
    auto good = ed25519::private_key::from_pkcs8_der(view(v1(pub1)));
    ASSERT_TRUE(good.has_value());
    EXPECT_TRUE(*good == key(seed1));
    bytes_t other = pub1;
    other[0] ^= 1;
    auto mismatch = ed25519::private_key::from_pkcs8_der(view(v1(other)));
    ASSERT_FALSE(mismatch.has_value());
    EXPECT_EQ(mismatch.error().code(), crypto::errc::invalid_key);
    bytes_t extra = to_bytes(key(seed1).public_key().to_pkix_der());
    extra[1] += 2;
    extra.insert(extra.end(), {0x05, 0x00});   // a NULL inside the SEQUENCE
    auto trailing = ed25519::public_key::from_pkix_der(view(extra));
    ASSERT_FALSE(trailing.has_value());
    EXPECT_EQ(trailing.error().code(), crypto::errc::malformed);
    // every cut is an error
    bytes_t pkcs8 = to_bytes(key(bytes_t(32, 1)).to_pkcs8_der());
    for (size_t n = 0; n < pkcs8.size(); ++n) {
        EXPECT_FALSE(ed25519::private_key::from_pkcs8_der(view(bytes_t(pkcs8.begin(), pkcs8.begin() + n))).has_value());
    }
}

// --- the files of other oracles ------------------------------------------------

namespace {
    std::string field(const sgcl::encoding::json& v, const char* key) {
        auto s = v[sgcl::string(key)].as_string();
        return s ? std::string(s->data(), s->size()) : std::string();
    }
}

// Wycheproof's ed25519_test.json: "valid" verifies, "invalid" does not
TEST(Crypto_Ed25519, Wycheproof) {
    auto text = oracle_file("wycheproof/testvectors_v1/ed25519_test.json");
    if (!text) {
        GTEST_SKIP() << "ed25519_test.json not on disk";
    }
    auto doc = sgcl::encoding::json::parse(sgcl::string(*text));
    ASSERT_TRUE(doc.has_value());
    size_t cases = 0;
    for (auto& group : (*doc)["testGroups"].elements()) {
        bytes_t pub = unhex(field(group["publicKey"], "pk"));
        for (auto& t : group["tests"].elements()) {
            bytes_t msg = unhex(field(t, "msg")), sig = unhex(field(t, "sig"));
            std::string result = field(t, "result");
            EXPECT_EQ(verify(pub, msg, sig), result == "valid") << field(t, "tcId") << " " << field(t, "comment");
            ++cases;
        }
    }
    EXPECT_GT(cases, 0u);
}

// The cases of "ed25519 speccheck" (Chalkias, Garillot, Nikolaenko, 2020),
// cases.json from its repository: each judged by the reference, which
// holds our policy (strict decoding, S < L, no cofactor)
TEST(Crypto_Ed25519, Speccheck) {
    auto text = oracle_file("ed25519-speccheck/cases.json");
    if (!text) {
        GTEST_SKIP() << "ed25519-speccheck/cases.json not on disk";
    }
    auto doc = sgcl::encoding::json::parse(sgcl::string(*text));
    ASSERT_TRUE(doc.has_value());
    reference ref;
    size_t cases = 0;
    for (auto& t : doc->elements()) {
        bytes_t pub = unhex(field(t, "pub_key")), msg = unhex(field(t, "message")), sig = unhex(field(t, "signature"));
        bool mine = verify(pub, msg, sig);
        EXPECT_EQ(mine, ref.verify(pub, msg, sig)) << "case " << cases;
        std::printf("  speccheck %zu: %s\n", cases, mine ? "accepted" : "rejected");
        ++cases;
    }
    EXPECT_GT(cases, 0u);
}

namespace {
    // Go's tree: GOROOT from the environment or Homebrew's place
    optional<std::string> go_sign_input() {
        std::vector<std::string> roots;
        if (const char* g = std::getenv("GOROOT")) {
            roots.push_back(g);
        }
        roots.push_back("/opt/homebrew/opt/go/libexec");
        roots.push_back("/usr/local/go");
        for (const std::string& root : roots) {
            std::string path = root + "/src/crypto/ed25519/testdata/sign.input.gz";
            if (!std::ifstream(path)) {
                continue;
            }
            FILE* f = ::popen(("gzip -dc '" + path + "'").c_str(), "r");
            if (!f) {
                return nullopt;
            }
            std::string out;
            char buf[65536];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
                out.append(buf, n);
            }
            ::pclose(f);
            return out;
        }
        return nullopt;
    }
}

// The SUPERCOP vectors of Go's crypto/ed25519/testdata/sign.input.gz (128
// of SUPERCOP's 1024 in Go 1.27):
// "secret||public:public:message:signature||message:" per line
TEST(Crypto_Ed25519, SupercopVectorsFromGo) {
    auto text = go_sign_input();
    if (!text) {
        GTEST_SKIP() << "Go's sign.input.gz not found";
    }
    std::istringstream in(*text);
    std::string line;
    size_t cases = 0;
    while (std::getline(in, line)) {
        std::vector<std::string> parts;
        std::istringstream s(line);
        std::string p;
        while (std::getline(s, p, ':')) {
            parts.push_back(p);
        }
        ASSERT_GE(parts.size(), 4u);
        bytes_t sk = unhex(parts[0]), pub = unhex(parts[1]), msg = unhex(parts[2]), sm = unhex(parts[3]);
        bytes_t seed(sk.begin(), sk.begin() + 32);
        auto k = key(seed);
        ASSERT_EQ(hex(k.public_key().bytes()), hex(pub)) << cases;
        bytes_t sig(sm.begin(), sm.begin() + 64);
        ASSERT_EQ(hex(k.sign(view(msg))), hex(sig)) << cases;
        ASSERT_TRUE(k.public_key().verify(view(msg), view(sig))) << cases;
        ++cases;
    }
    EXPECT_GE(cases, 128u);
}

// Go's crypto/ed25519 on random seeds and messages, from a file its program
// wrote: lines "ed seed message signature"
TEST(Crypto_Ed25519, AgainstGo) {
    const char* path = std::getenv("SGCL_CRYPTO_GO_VECTORS");
    if (!path) {
        GTEST_SKIP() << "SGCL_CRYPTO_GO_VECTORS not set";
    }
    std::ifstream in(path);
    ASSERT_TRUE(in.good());
    std::string line;
    size_t cases = 0;
    while (std::getline(in, line)) {
        // split at single spaces: an empty message is an empty field
        std::vector<std::string> f;
        std::istringstream s(line);
        std::string part;
        while (std::getline(s, part, ' ')) {
            f.push_back(part);
        }
        if (f.size() != 4 || f[0] != "ed") {
            continue;
        }
        const std::string& seed = f[1];
        const std::string& msg = f[2];
        const std::string& sig = f[3];
        auto k = key(unhex(seed));
        bytes_t m = unhex(msg);
        ASSERT_EQ(hex(k.sign(view(m))), sig) << cases;
        ASSERT_TRUE(k.public_key().verify(view(m), view(unhex(sig))));
        ++cases;
    }
    EXPECT_GT(cases, 0u);
}
