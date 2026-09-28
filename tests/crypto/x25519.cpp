//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X25519: the vectors of RFC 7748 (§5.2, the iterations 1 and 1000 — a
// million in a test that runs only when asked — and §6.1); OpenSSL on
// random keys and on random peer keys of any 32 bytes; the keys of small
// order refused; the DER forms against OpenSSL's, both ways, and damaged;
// the secret: no copy, a move zeroes the source, the destructor zeroes the
// object. Wycheproof's x25519_test.json is read when it lies in
// ~/Programming/oracles/wycheproof/testvectors_v1, and a file of Go's
// results (SGCL_CRYPTO_GO_VECTORS, written by the program the report
// names) when the variable is set; skipped otherwise.
#include "curve25519_common.h"

#include "sgcl/encoding/json.h"

#include <new>
#include <type_traits>

using namespace curve_test;
namespace x25519 = crypto::x25519;

static_assert(!std::is_copy_constructible_v<x25519::private_key>);
static_assert(!std::is_copy_assignable_v<x25519::private_key>);
static_assert(std::is_nothrow_move_constructible_v<x25519::private_key>);
static_assert(std::is_copy_constructible_v<x25519::public_key>);

namespace {
    x25519::private_key priv(const bytes_t& b) {
        auto k = x25519::private_key::from_bytes(view(b));
        if (!k) {
            throw std::runtime_error("from_bytes");
        }
        return std::move(*k);
    }

    x25519::public_key pub(const bytes_t& b) {
        return *x25519::public_key::from_bytes(view(b));
    }

    // X25519(k, u) through the public API, "" for invalid_key
    std::string x25519_of(const bytes_t& k, const bytes_t& u) {
        auto s = priv(k).shared_secret(pub(u));
        if (!s) {
            EXPECT_EQ(s.error().code(), crypto::errc::invalid_key);
            return "";
        }
        return hex(*s);
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
}

// RFC 7748 §5.2: the two single multiplications (the second u has bit 255
// set, which the function ignores)
TEST(Crypto_X25519, Rfc7748Vectors) {
    EXPECT_EQ(x25519_of(unhex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4"),
                        unhex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c")),
              "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");
    EXPECT_EQ(x25519_of(unhex("4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d"),
                        unhex("e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493")),
              "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957");
}

namespace {
    // RFC 7748 §5.2's iteration: k = u = 9, then (k, u) = (X25519(k, u), k)
    std::string iterate(int n) {
        bytes_t k(32), u(32);
        k[0] = u[0] = 9;
        for (int i = 0; i < n; ++i) {
            auto s = priv(k).shared_secret(pub(u));
            u = k;
            k = to_bytes(*s);
        }
        return hex(k);
    }
}

TEST(Crypto_X25519, Rfc7748Iterations) {
    EXPECT_EQ(iterate(1), "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079");
    EXPECT_EQ(iterate(1000), "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51");
}

// A million: about a minute; run with --gtest_also_run_disabled_tests
TEST(Crypto_X25519, DISABLED_Rfc7748MillionIterations) {
    bytes_t k(32), u(32);
    k[0] = u[0] = 9;
    for (int i = 0; i < 1000000; ++i) {
        unsigned char out[32];
        x25519::detail::scalarmult(out, k.data(), u.data());
        u = k;
        k.assign(out, out + 32);
    }
    EXPECT_EQ(hex(k), "7c3911e0ab2586fd864497297e575e6f3bc601c0883c30df5f4dd2d24f665424");
}

// RFC 7748 §6.1: Alice and Bob
TEST(Crypto_X25519, Rfc7748DiffieHellman) {
    auto alice = priv(unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    auto bob = priv(unhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));
    EXPECT_EQ(hex(alice.public_key().bytes()), "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    EXPECT_EQ(hex(bob.public_key().bytes()), "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    const char* shared = "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742";
    EXPECT_EQ(hex(value_of(alice.shared_secret(bob.public_key()))), shared);
    EXPECT_EQ(hex(value_of(bob.shared_secret(alice.public_key()))), shared);
}

// The public key (made by the Edwards multiplication) and the shared secret
// (by the ladder) against OpenSSL, on random private keys and on peer keys
// of any 32 bytes: bit 255 set, values of p and more
TEST(Crypto_X25519, AgainstOpenSsl) {
    random_source r(25519);
    for (int i = 0; i < 1000; ++i) {
        bytes_t a = r.bytes(32), b = r.bytes(32), u = r.bytes(32);
        auto ka = priv(a);
        auto kb = priv(b);
        ASSERT_EQ(hex(ka.public_key().bytes()), hex(ossl_x25519_public(a))) << i;
        ASSERT_EQ(hex(value_of(ka.shared_secret(kb.public_key()))), hex(ossl_x25519(a, ossl_x25519_public(b)))) << i;
        ASSERT_EQ(hex(value_of(ka.shared_secret(kb.public_key()))), hex(value_of(kb.shared_secret(ka.public_key())))) << i;
        if (i % 4 == 0) {
            u[31] |= 0x80;
        }
        if (i % 16 == 1) {
            std::memset(u.data() + 1, 0xff, 31);   // p or more, reduced
            u[31] = 0x7f;
        }
        std::string expected = hex(ossl_x25519(a, u));
        ASSERT_EQ(x25519_of(a, u), expected) << i << " peer " << hex(u);
    }
}

// generate() gives a key that agrees with OpenSSL, and two calls differ
TEST(Crypto_X25519, Generate) {
    auto a = x25519::private_key::generate();
    auto b = x25519::private_key::generate();
    EXPECT_FALSE(a == b);
    bytes_t sa = to_bytes(a.bytes());
    EXPECT_EQ(hex(a.public_key().bytes()), hex(ossl_x25519_public(sa)));
    EXPECT_EQ(hex(value_of(a.shared_secret(b.public_key()))), hex(value_of(b.shared_secret(a.public_key()))));
}

// The points of small order (and their non-canonical encodings) give the
// all-zero secret: invalid_key, as OpenSSL refuses them too
TEST(Crypto_X25519, SmallOrderIsInvalidKey) {
    const char* points[] = {
        "0000000000000000000000000000000000000000000000000000000000000000",
        "0100000000000000000000000000000000000000000000000000000000000000",
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
        "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "0000000000000000000000000000000000000000000000000000000000000080",
        "0100000000000000000000000000000000000000000000000000000000000080",
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b880",
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f11d7",
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
    };
    random_source r(8);
    for (const char* h : points) {
        bytes_t u = unhex(h);
        for (int i = 0; i < 4; ++i) {
            bytes_t k = r.bytes(32);
            auto s = priv(k).shared_secret(pub(u));
            ASSERT_FALSE(s.has_value()) << h;
            EXPECT_EQ(s.error().code(), crypto::errc::invalid_key);
            EXPECT_TRUE(ossl_x25519(k, u).empty()) << h;
        }
    }
}

TEST(Crypto_X25519, KeysOfTheWrongSize) {
    for (size_t n : {0, 1, 31, 33, 64}) {
        bytes_t b(n, 1);
        auto p = x25519::public_key::from_bytes(view(b));
        ASSERT_FALSE(p.has_value());
        EXPECT_EQ(p.error().code(), crypto::errc::invalid_key);
        auto k = x25519::private_key::from_bytes(view(b));
        ASSERT_FALSE(k.has_value());
        EXPECT_EQ(k.error().code(), crypto::errc::invalid_key);
    }
}

// The secret: a clone is the same key, a move leaves zeros behind, the
// destructor zeroes the object (a probe reads its storage afterwards)
TEST(Crypto_X25519, TheSecretIsZeroed) {
    bytes_t a = unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto k = priv(a);
    auto c = k.clone();
    EXPECT_TRUE(c == k);
    EXPECT_EQ(hex(c.public_key().bytes()), hex(k.public_key().bytes()));
    auto moved = std::move(k);
    EXPECT_TRUE(all_zero(k));
    EXPECT_TRUE(moved == c);
    x25519::private_key assigned = x25519::private_key::generate();
    assigned = std::move(moved);
    EXPECT_TRUE(all_zero(moved));
    EXPECT_TRUE(assigned == c);

    alignas(x25519::private_key) unsigned char storage[sizeof(x25519::private_key)];
    auto* probe = new (storage) x25519::private_key(priv(a));
    EXPECT_FALSE(all_zero(*probe));
    probe->~private_key();
    for (unsigned char b : storage) {
        ASSERT_EQ(b, 0);
    }
}

// The DER forms are OpenSSL's byte for byte, and each side reads the other's
// A key moved from is used by no operation: std::logic_error, as every
// key of the module (it once computed with the clamped zero, 2^254, a
// secret anyone can derive)
TEST(Crypto_X25519, AKeyMovedFromIsUsedByNothing) {
    auto a = x25519::private_key::generate();
    auto peer = x25519::private_key::generate().public_key();
    auto b = std::move(a);
    EXPECT_THROW((void)a.shared_secret(peer), std::logic_error);
    EXPECT_THROW((void)a.public_key(), std::logic_error);
    EXPECT_THROW((void)a.bytes(), std::logic_error);
    EXPECT_THROW((void)a.to_pkcs8_der(), std::logic_error);
    EXPECT_TRUE(b.shared_secret(peer).has_value());
}

TEST(Crypto_X25519, DerAgainstOpenSsl) {
    random_source r(110);
    for (int i = 0; i < 50; ++i) {
        bytes_t a = r.bytes(32);
        auto k = priv(a);
        bytes_t pkcs8 = to_bytes(k.to_pkcs8_der());
        bytes_t pkix = to_bytes(k.public_key().to_pkix_der());
        ASSERT_EQ(hex(pkcs8), hex(ossl_pkcs8(EVP_PKEY_X25519, a)));
        ASSERT_EQ(hex(pkix), hex(ossl_pkix(EVP_PKEY_X25519, ossl_x25519_public(a))));
        EXPECT_EQ(hex(ossl_read_pkcs8(pkcs8)), hex(a));
        auto back = x25519::private_key::from_pkcs8_der(view(ossl_pkcs8(EVP_PKEY_X25519, a)));
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == k);
        auto pb = x25519::public_key::from_pkix_der(view(pkix));
        ASSERT_TRUE(pb.has_value());
        EXPECT_TRUE(*pb == k.public_key());
    }
}

// Damaged DER: every cut and every byte changed is an error (malformed,
// unsupported or invalid_key), never a crash, or a key that writes the
// same bytes back; a version 1 key with its public key is read, one whose
// public key is another's is invalid_key, attributes are skipped
TEST(Crypto_X25519, DamagedDer) {
    bytes_t a = unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto k = priv(a);
    bytes_t pkcs8 = to_bytes(k.to_pkcs8_der());
    bytes_t pkix = to_bytes(k.public_key().to_pkix_der());
    for (const bytes_t* der : {&pkcs8, &pkix}) {
        bool is_private = der == &pkcs8;
        for (size_t n = 0; n < der->size(); ++n) {
            bytes_t cut(der->begin(), der->begin() + n);
            EXPECT_FALSE(is_private ? x25519::private_key::from_pkcs8_der(view(cut)).has_value()
                                    : x25519::public_key::from_pkix_der(view(cut)).has_value()) << n;
        }
        bytes_t longer = *der;
        longer.push_back(0);
        EXPECT_FALSE(is_private ? x25519::private_key::from_pkcs8_der(view(longer)).has_value()
                                : x25519::public_key::from_pkix_der(view(longer)).has_value());
        for (size_t i = 0; i < der->size(); ++i) {
            for (unsigned char flip : {0x01, 0x80, 0xff}) {
                bytes_t bad = *der;
                bad[i] ^= flip;
                if (is_private) {
                    auto r = x25519::private_key::from_pkcs8_der(view(bad));
                    if (r) {
                        // the key bytes themselves, or version 1 with no
                        // public key (RFC 5958 allows it), written back as 0
                        bytes_t same = bad;
                        if (same[4] == 1) {
                            same[4] = 0;
                        }
                        EXPECT_EQ(hex(r->to_pkcs8_der()), hex(same)) << i;
                    }
                } else {
                    auto r = x25519::public_key::from_pkix_der(view(bad));
                    if (r) {
                        EXPECT_EQ(hex(r->to_pkix_der()), hex(bad)) << i;
                    }
                }
            }
        }
    }
    // an Ed25519 key is unsupported here
    bytes_t ed = ossl_pkcs8(EVP_PKEY_ED25519, a);
    auto wrong = x25519::private_key::from_pkcs8_der(view(ed));
    ASSERT_FALSE(wrong.has_value());
    EXPECT_EQ(wrong.error().code(), crypto::errc::unsupported);

    // version 1 with the public key [1], and with attributes [0] before it
    bytes_t pubkey = to_bytes(k.public_key().bytes());
    auto v1 = [&](bool attributes, const bytes_t& p) {
        bytes_t body = {0x02, 0x01, 0x01, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x04, 0x22, 0x04, 0x20};
        body.insert(body.end(), a.begin(), a.end());
        if (attributes) {
            bytes_t attr = {0xa0, 0x05, 0x30, 0x03, 0x06, 0x01, 0x00};
            body.insert(body.end(), attr.begin(), attr.end());
        }
        body.push_back(0x81);
        body.push_back(0x21);
        body.push_back(0x00);
        body.insert(body.end(), p.begin(), p.end());
        bytes_t der = {0x30, (unsigned char)body.size()};
        der.insert(der.end(), body.begin(), body.end());
        return der;
    };
    for (bool attributes : {false, true}) {
        auto good = x25519::private_key::from_pkcs8_der(view(v1(attributes, pubkey)));
        ASSERT_TRUE(good.has_value()) << good.error().message().data();
        EXPECT_TRUE(*good == k);
        bytes_t other = pubkey;
        other[3] ^= 1;
        auto bad = x25519::private_key::from_pkcs8_der(view(v1(attributes, other)));
        ASSERT_FALSE(bad.has_value());
        EXPECT_EQ(bad.error().code(), crypto::errc::invalid_key);
    }
    // a long form of a short length is not DER
    bytes_t loose = pkix;
    loose.erase(loose.begin() + 1);
    loose.insert(loose.begin() + 1, {0x81, 0x2a});
    EXPECT_FALSE(x25519::public_key::from_pkix_der(view(loose)).has_value());
}

// Wycheproof's x25519_test.json: a "valid" or "acceptable" case with a
// non-zero shared secret must give it; a zero one must be invalid_key
TEST(Crypto_X25519, Wycheproof) {
    auto text = oracle_file("wycheproof/testvectors_v1/x25519_test.json");
    if (!text) {
        GTEST_SKIP() << "x25519_test.json not on disk";
    }
    auto doc = sgcl::encoding::json::parse(sgcl::string(*text));
    ASSERT_TRUE(doc.has_value());
    auto field = [](const sgcl::encoding::json& v, const char* key) {
        auto s = v[sgcl::string(key)].as_string();
        return s ? std::string(s->data(), s->size()) : std::string();
    };
    size_t cases = 0;
    for (auto& group : (*doc)["testGroups"].elements()) {
        for (auto& t : group["tests"].elements()) {
            bytes_t u = unhex(field(t, "public")), k = unhex(field(t, "private"));
            std::string shared = field(t, "shared"), result = field(t, "result");
            if (u.size() != 32 || k.size() != 32) {
                EXPECT_EQ(result, "invalid");
                continue;
            }
            std::string mine = x25519_of(k, u);
            if (shared == std::string(64, '0')) {
                EXPECT_EQ(mine, "") << field(t, "tcId");
            } else if (result != "invalid") {
                EXPECT_EQ(mine, shared) << field(t, "tcId");
            }
            ++cases;
        }
    }
    EXPECT_GT(cases, 0u);
}

// Go's crypto/ecdh on the same random keys, from a file its program wrote:
// lines "x priv peer_priv peer_pub shared"
TEST(Crypto_X25519, AgainstGo) {
    const char* path = std::getenv("SGCL_CRYPTO_GO_VECTORS");
    if (!path) {
        GTEST_SKIP() << "SGCL_CRYPTO_GO_VECTORS not set";
    }
    std::ifstream in(path);
    ASSERT_TRUE(in.good());
    std::string kind, a, b, bp, shared;
    size_t cases = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        s >> kind;
        if (kind != "x") {
            continue;
        }
        s >> a >> b >> bp >> shared;
        auto kb = priv(unhex(b));
        ASSERT_EQ(hex(kb.public_key().bytes()), bp);
        ASSERT_EQ(x25519_of(unhex(a), unhex(bp)), shared);
        ++cases;
    }
    EXPECT_GT(cases, 0u);
}
