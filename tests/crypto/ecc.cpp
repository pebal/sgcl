//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// P-256 and P-384: the group law at its corners (the identity, a point and
// its negative, n*G), RFC 6979's vectors (A.2.5, A.2.6) through the
// deterministic signer, OpenSSL as the oracle in loops on random keys and
// digests of every length (our signatures verified by OpenSSL and its by us,
// RFC 6979 signatures equal to OpenSSL's for SHA-256/384/512, ECDH secrets
// equal, SEC 1 points accepted and refused alike, PKCS#8, SEC 1 and SPKI in
// both directions), strict DER signatures, the secrets (no copy, a move and
// the destructor zero the scalar), and Wycheproof's files when they lie in
// ~/Programming/oracles/wycheproof/testvectors_v1 (skipped otherwise).
// A file of Go's vectors (crypto/ecdsa, crypto/ecdh, crypto/x509) is read
// when SGCL_CRYPTO_GO_VECTORS names one, and our signatures are written for
// Go to check when SGCL_CRYPTO_GO_OUT names a file.
#include "ecc_common.h"

#include "sgcl/encoding/json.h"

#include <cstdlib>
#include <fstream>
#include <new>
#include <sstream>
#include <type_traits>

using namespace ecc_test;

namespace {
    template<class T>
    class Crypto_Ec : public ::testing::Test {};

    TYPED_TEST_SUITE(Crypto_Ec, Curves);

    // The curve's arithmetic, reached through detail
    template<class T>
    struct Arith {
        using E = crypto::detail::Curve<typename T::curve>;
        using F = typename E::F;
        using point = typename E::point;
        using affine = typename E::affine;

        static bytes_t encode(const point& p) {
            if (E::identity_mask(p) != 0) {
                return bytes_t{0};
            }
            affine a = E::to_affine(p);
            bytes_t out(1 + 2 * T::size);
            out[0] = 4;
            crypto::detail::limbs_to_be(out.data() + 1, F::from_mont(a.x));
            crypto::detail::limbs_to_be(out.data() + 1 + T::size, F::from_mont(a.y));
            return out;
        }

        static point base(const bytes_t& k) {
            return E::base_mult(k.data());
        }

        static point mult(const point& p, const bytes_t& k) {
            return E::scalar_mult(p, k.data());
        }

        static point neg(const point& p) {
            point r = p;
            F::sub(r.y, typename E::fe{}, p.y);
            return r;
        }
    };

    std::string text_of(const crypto::error& e) {
        auto m = e.message();
        return std::string(m.data(), m.size());
    }

    bytes_t digest_of(const char* md, const bytes_t& msg) {
        bytes_t out(EVP_MAX_MD_SIZE);
        unsigned int n = 0;
        EVP_Digest(msg.data(), msg.size(), out.data(), &n, EVP_get_digestbyname(md), nullptr);
        out.resize(n);
        return out;
    }
}

// --- the group law ------------------------------------------------------------

// The generator's table and the windowed multiplication agree with each
// other and with OpenSSL; the corners of the complete formulas: P + (-P),
// O + P, P + P against doubling, O doubled, n*G, (n-1)*G = -G, 0*P
TYPED_TEST(Crypto_Ec, GroupLaw) {
    using A = Arith<TypeParam>;
    using E = typename A::E;
    random_source r(1);
    bytes_t zero(TypeParam::size, 0);
    bytes_t one = zero;
    one.back() = 1;
    bytes_t n = unhex(TypeParam::order);
    bytes_t n1 = n;
    n1.back() -= 1;

    auto g = E::generator();
    EXPECT_EQ(A::encode(A::base(one)), A::encode(g));
    EXPECT_EQ(A::encode(A::base(zero)), bytes_t{0});
    EXPECT_EQ(A::encode(A::base(n)), bytes_t{0});
    EXPECT_EQ(A::encode(A::mult(g, n)), bytes_t{0});
    EXPECT_EQ(A::encode(A::base(n1)), A::encode(A::neg(g)));
    EXPECT_EQ(A::encode(A::mult(g, zero)), bytes_t{0});

    for (int i = 0; i < 50; ++i) {
        bytes_t k = r.bytes(TypeParam::size);
        auto p = A::base(k);
        ASSERT_EQ(A::encode(p), A::encode(A::mult(g, k))) << hex(k);
        ASSERT_EQ(A::encode(p), ossl_public_of(TypeParam::nid, k)) << hex(k);
        typename E::affine pa = E::to_affine(p);
        EXPECT_TRUE(E::on_curve(pa.x, pa.y));

        typename E::point s, d, o;
        E::add(s, p, A::neg(p));
        EXPECT_EQ(A::encode(s), bytes_t{0});
        E::add(s, E::identity(), p);
        EXPECT_EQ(A::encode(s), A::encode(p));
        E::add(s, p, E::identity());
        EXPECT_EQ(A::encode(s), A::encode(p));
        E::add(s, p, p);
        E::dbl(d, p);
        EXPECT_EQ(A::encode(s), A::encode(d));
        E::dbl(o, E::identity());
        EXPECT_EQ(A::encode(o), bytes_t{0});
        // the mixed addition against the full one, from the identity too
        typename E::point q = A::base(r.bytes(TypeParam::size));
        typename E::point m, f;
        E::add_mixed(m, q, pa);
        E::add(f, q, typename E::point{pa.x, pa.y, E::F::one()});
        EXPECT_EQ(A::encode(m), A::encode(f));
        E::add_mixed(m, E::identity(), pa);
        EXPECT_EQ(A::encode(m), A::encode(p));
        E::add_mixed(m, p, pa);
        EXPECT_EQ(A::encode(m), A::encode(d));
        E::add_mixed(m, A::neg(p), pa);
        EXPECT_EQ(A::encode(m), bytes_t{0});
        // k * (j * G) = (k j) * G through OpenSSL's scalar product
        bytes_t j = r.bytes(TypeParam::size);
        auto kj = A::mult(A::base(j), k);
        BIGNUM *bk = BN_bin2bn(k.data(), int(k.size()), nullptr), *bj = BN_bin2bn(j.data(), int(j.size()), nullptr);
        BIGNUM *bn = BN_bin2bn(n.data(), int(n.size()), nullptr), *prod = BN_new();
        BN_CTX* ctx = BN_CTX_new();
        BN_mod_mul(prod, bk, bj, bn, ctx);
        bytes_t kjb(TypeParam::size);
        BN_bn2binpad(prod, kjb.data(), int(kjb.size()));
        EXPECT_EQ(A::encode(kj), A::encode(A::base(kjb)));
        BN_CTX_free(ctx);
        BN_free(bk);
        BN_free(bj);
        BN_free(bn);
        BN_free(prod);
    }
}

// Verification's variable-time u1 G + u2 Q against the constant-time
// multiplications and the complete addition: random pairs, the scalars'
// edges (0, 1, n - 1, all ones), Q = G (the digits of both scalars meet on
// one point: the additions' doubling case), u2 = n - u1 with Q = G (the sum
// is the identity), and the Jacobian additions' corners: O + P, P + O,
// P + P against doubling, P + (-P), mixed and full
TYPED_TEST(Crypto_Ec, VariableTimeDoubleMult) {
    using A = Arith<TypeParam>;
    using E = typename A::E;
    using F = typename E::F;
    random_source r(2);
    auto encode_j = [](const typename E::jacobian& j) {
        typename E::point p;
        E::from_jacobian(p, j);
        return A::encode(p);
    };
    auto reference = [](const bytes_t& u1, const typename E::affine& q, const bytes_t& u2) {
        typename E::point x = E::base_mult(u1.data());
        typename E::point y = E::scalar_mult(typename E::point{q.x, q.y, F::one()}, u2.data());
        E::add(x, x, y);
        return A::encode(x);
    };
    bytes_t zero(TypeParam::size, 0);
    bytes_t one = zero;
    one.back() = 1;
    bytes_t n = unhex(TypeParam::order);
    bytes_t n1 = n;
    n1.back() -= 1;
    bytes_t ones(TypeParam::size, 0xff);
    const typename E::affine g = E::to_affine(E::generator());
    std::vector<bytes_t> edges = {zero, one, n1, ones};
    for (int i = 0; i < 40; ++i) {
        typename E::affine q = i % 4 == 0 ? g : E::to_affine(A::base(r.bytes(TypeParam::size)));
        bytes_t u1 = i < 16 ? edges[size_t(i) % 4] : r.bytes(TypeParam::size);
        bytes_t u2 = i < 16 ? edges[size_t(i) / 4] : i % 3 == 0 ? u1 : r.bytes(TypeParam::size);
        EXPECT_EQ(encode_j(E::double_mult_vartime(u1.data(), q, u2.data())), reference(u1, q, u2)) << hex(u1) << ' ' << hex(u2);
    }
    for (int i = 0; i < 20; ++i) {
        bytes_t u1 = r.bytes(TypeParam::size);
        u1.front() &= 0x7f;   // below n
        BIGNUM *b1 = BN_bin2bn(u1.data(), int(u1.size()), nullptr), *bn = BN_bin2bn(n.data(), int(n.size()), nullptr), *b2 = BN_new();
        BN_sub(b2, bn, b1);
        bytes_t u2(TypeParam::size);
        BN_bn2binpad(b2, u2.data(), int(u2.size()));
        BN_free(b1);
        BN_free(b2);
        BN_free(bn);
        EXPECT_TRUE(E::is_zero(E::double_mult_vartime(u1.data(), g, u2.data()).z)) << hex(u1);
    }
    for (int i = 0; i < 20; ++i) {
        typename E::point p = A::base(r.bytes(TypeParam::size));
        typename E::affine pa = E::to_affine(p);
        typename E::point d;
        E::dbl(d, p);
        typename E::jacobian pj{pa.x, pa.y, F::one()};
        typename E::jacobian dj = pj;
        E::dbl_jacobian(dj);   // 2P with Z other than 1, the full addition's both inputs general
        typename E::jacobian neg = pj;
        F::sub(neg.y, typename E::fe{}, neg.y);
        typename E::jacobian t = E::jacobian_identity();
        E::add_affine_vartime(t, pa);
        EXPECT_EQ(encode_j(t), A::encode(p));
        E::add_affine_vartime(t, pa);
        EXPECT_EQ(encode_j(t), A::encode(d));
        t = neg;
        E::add_affine_vartime(t, pa);
        EXPECT_EQ(encode_j(t), bytes_t{0});
        t = E::jacobian_identity();
        E::add_vartime(t, dj);
        EXPECT_EQ(encode_j(t), A::encode(d));
        E::add_vartime(t, E::jacobian_identity());
        EXPECT_EQ(encode_j(t), A::encode(d));
        E::add_vartime(t, dj);   // 2P + 2P
        typename E::point q;
        E::dbl(q, d);
        EXPECT_EQ(encode_j(t), A::encode(q));
        typename E::jacobian nd = dj;
        F::sub(nd.y, typename E::fe{}, nd.y);
        t = dj;
        E::add_vartime(t, nd);
        EXPECT_EQ(encode_j(t), bytes_t{0});
        t = pj;
        E::add_vartime(t, dj);   // P + 2P = 3P
        typename E::point three;
        E::add(three, d, p);
        EXPECT_EQ(encode_j(t), A::encode(three));
    }
}

// The field and the scalars: Montgomery products against OpenSSL's modular
// arithmetic on random numbers and on the edges (0, 1, m - 1), inverses
TYPED_TEST(Crypto_Ec, ModularArithmetic) {
    using E = typename Arith<TypeParam>::E;
    random_source r(2);
    for (const char* mod_hex : {TypeParam::prime, TypeParam::order}) {
        bool field = mod_hex == TypeParam::prime;
        bytes_t m = unhex(mod_hex);
        BIGNUM* bm = BN_bin2bn(m.data(), int(m.size()), nullptr);
        BN_CTX* ctx = BN_CTX_new();
        auto check = [&](const bytes_t& a, const bytes_t& b) {
            auto la = crypto::detail::limbs_from_be<TypeParam::curve::words>(a.data());
            auto lb = crypto::detail::limbs_from_be<TypeParam::curve::words>(b.data());
            typename E::fe x, y, s, d, p, inv;
            auto run = [&](auto mont) {
                using M = decltype(mont);
                x = M::to_mont(la);
                y = M::to_mont(lb);
                M::add(s, x, y);
                M::sub(d, x, y);
                M::mul(p, x, y);
                inv = M::inverse(x);
                s = M::from_mont(s);
                d = M::from_mont(d);
                p = M::from_mont(p);
                inv = M::from_mont(inv);
            };
            if (field) {
                run(typename E::F{});
            } else {
                run(typename E::S{});
            }
            BIGNUM *ba = BN_bin2bn(a.data(), int(a.size()), nullptr), *bb = BN_bin2bn(b.data(), int(b.size()), nullptr), *t = BN_new();
            auto expect = [&](const typename E::fe& got, const char* what) {
                bytes_t want(TypeParam::size), have(TypeParam::size);
                BN_bn2binpad(t, want.data(), int(want.size()));
                crypto::detail::limbs_to_be(have.data(), got);
                EXPECT_EQ(hex(have), hex(want)) << what << " a=" << hex(a) << " b=" << hex(b);
            };
            BN_mod_add(t, ba, bb, bm, ctx);
            expect(s, "add");
            BN_mod_sub(t, ba, bb, bm, ctx);
            expect(d, "sub");
            BN_mod_mul(t, ba, bb, bm, ctx);
            expect(p, "mul");
            if (BN_is_zero(ba)) {
                BN_zero(t);
            } else {
                BN_mod_inverse(t, ba, bm, ctx);
            }
            expect(inv, "inverse");
            BN_free(ba);
            BN_free(bb);
            BN_free(t);
        };
        bytes_t zero(TypeParam::size, 0), one = zero, m1 = m;
        one.back() = 1;
        m1.back() -= 1;
        std::vector<bytes_t> edges = {zero, one, m1};
        for (auto& a : edges) {
            for (auto& b : edges) {
                check(a, b);
            }
        }
        for (int i = 0; i < 500; ++i) {
            bytes_t a = r.bytes(TypeParam::size), b = r.bytes(TypeParam::size);
            if (!(a < m) || !(b < m)) {
                continue;
            }
            check(a, b);
        }
        BN_CTX_free(ctx);
        BN_free(bm);
    }
}

// --- RFC 6979 -----------------------------------------------------------------

namespace {
    template<class C, class H>
    bytes_t deterministic(const bytes_t& d, const bytes_t& digest) {
        using Ecdsa = crypto::detail::Ecdsa<C>;
        bytes_t sig(2 * C::size);
        Ecdsa::template sign<H>(sig.data(), crypto::detail::limbs_from_be<C::words>(d.data()), digest.data(), digest.size(), nullptr, 0);
        return sig;
    }

    template<class H>
    bytes_t hash_of(const std::string& msg) {
        return to_bytes(H::of(msg));
    }
}

// RFC 6979 A.2.5 (P-256) and A.2.6 (P-384), messages "sample" and "test",
// SHA-256, SHA-384 and SHA-512 (the HMAC of the nonce generator is the
// message's digest); the public keys U of the appendix from the private
// ones. Also Go's vector that makes the generator loop (a candidate k
// out of range) for P-256
TEST(Crypto_Ec, Rfc6979) {
    using crypto::detail::P256;
    using crypto::detail::P384;
    bytes_t x256 = unhex("C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721");
    auto k256 = crypto::p256::private_key::from_bytes(view(x256));
    ASSERT_TRUE(k256.has_value());
    EXPECT_EQ(hex(to_bytes(k256->public_key().bytes())),
              "04" "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
                   "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");
    struct Case {
        const char* msg;
        int sha;
        const char *r, *s;
    };
    const Case p256[] = {
        {"sample", 256, "EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716", "F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8"},
        {"sample", 384, "0EAFEA039B20E9B42309FB1D89E213057CBF973DC0CFC8F129EDDDC800EF7719", "4861F0491E6998B9455193E34E7B0D284DDD7149A74B95B9261F13ABDE940954"},
        {"sample", 512, "8496A60B5E9B47C825488827E0495B0E3FA109EC4568FD3F8D1097678EB97F00", "2362AB1ADBE2B8ADF9CB9EDAB740EA6049C028114F2460F96554F61FAE3302FE"},
        {"test", 256, "F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D38367", "019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083"},
        {"test", 384, "83910E8B48BB0C74244EBDF7F07A1C5413D61472BD941EF3920E623FBCCEBEB6", "8DDBEC54CF8CD5874883841D712142A56A8D0F218F5003CB0296B6B509619F2C"},
        {"test", 512, "461D93F31B6540894788FD206C07CFA0CC35F46FA3C91816FFF1040AD1581A04", "39AF9F15DE0DB8D97E72719C74820D304CE5226E32DEDAE67519E840D1194E55"},
        {"wv[vnX", 256, "EFD9073B652E76DA1B5A019C0E4A2E3FA529B035A6ABB91EF67F0ED7A1F21234", "3DB4706C9D9F4A4FE13BB5E08EF0FAB53A57DBAB2061C83A35FA411C68D2BA33"},
    };
    for (const Case& c : p256) {
        bytes_t sig = c.sha == 256 ? deterministic<P256, crypto::sha256>(x256, hash_of<crypto::sha256>(c.msg))
                    : c.sha == 384 ? deterministic<P256, crypto::sha384>(x256, hash_of<crypto::sha384>(c.msg))
                                   : deterministic<P256, crypto::sha512>(x256, hash_of<crypto::sha512>(c.msg));
        std::string want = c.r;
        want += c.s;
        EXPECT_EQ(hex(sig), hex(unhex(want))) << "P-256 " << c.msg << " SHA-" << c.sha;
        bytes_t digest = c.sha == 256 ? hash_of<crypto::sha256>(c.msg) : c.sha == 384 ? hash_of<crypto::sha384>(c.msg) : hash_of<crypto::sha512>(c.msg);
        EXPECT_TRUE(k256->public_key().verify_digest_raw(view(digest), view(sig)));
    }

    bytes_t x384 = unhex("6B9D3DAD2E1B8C1C05B19875B6659F4DE23C3B667BF297BA9AA47740787137D896D5724E4C70A825F872C9EA60D2EDF5");
    auto k384 = crypto::p384::private_key::from_bytes(view(x384));
    ASSERT_TRUE(k384.has_value());
    EXPECT_EQ(hex(to_bytes(k384->public_key().bytes())),
              "04" "ec3a4e415b4e19a4568618029f427fa5da9a8bc4ae92e02e06aae5286b300c64def8f0ea9055866064a254515480bc13"
                   "8015d9b72d7d57244ea8ef9ac0c621896708a59367f9dfb9f54ca84b3f1c9db1288b231c3ae0d4fe7344fd2533264720");
    const Case p384[] = {
        {"sample", 256, "21B13D1E013C7FA1392D03C5F99AF8B30C570C6F98D4EA8E354B63A21D3DAA33BDE1E888E63355D92FA2B3C36D8FB2CD", "F3AA443FB107745BF4BD77CB3891674632068A10CA67E3D45DB2266FA7D1FEEBEFDC63ECCD1AC42EC0CB8668A4FA0AB0"},
        {"sample", 384, "94EDBB92A5ECB8AAD4736E56C691916B3F88140666CE9FA73D64C4EA95AD133C81A648152E44ACF96E36DD1E80FABE46", "99EF4AEB15F178CEA1FE40DB2603138F130E740A19624526203B6351D0A3A94FA329C145786E679E7B82C71A38628AC8"},
        {"sample", 512, "ED0959D5880AB2D869AE7F6C2915C6D60F96507F9CB3E047C0046861DA4A799CFE30F35CC900056D7C99CD7882433709", "512C8CCEEE3890A84058CE1E22DBC2198F42323CE8ACA9135329F03C068E5112DC7CC3EF3446DEFCEB01A45C2667FDD5"},
        {"test", 256, "6D6DEFAC9AB64DABAFE36C6BF510352A4CC27001263638E5B16D9BB51D451559F918EEDAF2293BE5B475CC8F0188636B", "2D46F3BECBCC523D5F1A1256BF0C9B024D879BA9E838144C8BA6BAEB4B53B47D51AB373F9845C0514EEFB14024787265"},
        {"test", 384, "8203B63D3C853E8D77227FB377BCF7B7B772E97892A80F36AB775D509D7A5FEB0542A7F0812998DA8F1DD3CA3CF023DB", "DDD0760448D42D8A43AF45AF836FCE4DE8BE06B485E9B61B827C2F13173923E06A739F040649A667BF3B828246BAA5A5"},
        {"test", 512, "A0D5D090C9980FAF3C2CE57B7AE951D31977DD11C775D314AF55F76C676447D06FB6495CD21B4B6E340FC236584FB277", "976984E59B4C77B0E8E4460DCA3D9F20E07B9BB1F63BEEFAF576F6B2E8B224634A2092CD3792E0159AD9CEE37659C736"},
    };
    for (const Case& c : p384) {
        bytes_t sig = c.sha == 256 ? deterministic<P384, crypto::sha256>(x384, hash_of<crypto::sha256>(c.msg))
                    : c.sha == 384 ? deterministic<P384, crypto::sha384>(x384, hash_of<crypto::sha384>(c.msg))
                                   : deterministic<P384, crypto::sha512>(x384, hash_of<crypto::sha512>(c.msg));
        std::string want = c.r;
        want += c.s;
        EXPECT_EQ(hex(sig), hex(unhex(want))) << "P-384 " << c.msg << " SHA-" << c.sha;
    }
}

// RFC 6979 against OpenSSL's deterministic nonces (3.2 and later) on random
// keys and messages, for the three SHA-2 digests: the same k, so the same
// signature byte for byte
TYPED_TEST(Crypto_Ec, DeterministicAgainstOpenSsl) {
    random_source r(3);
    for (int i = 0; i < 60; ++i) {
        bytes_t d = random_scalar<TypeParam>(r);
        auto key = TypeParam::private_key::from_bytes(view(d));
        ASSERT_TRUE(key.has_value());
        Pkey ossl = ossl_key(TypeParam::group, &d, to_bytes(key->public_key().bytes()));
        ASSERT_TRUE(ossl);
        bytes_t msg = r.bytes(r.below(200));
        using C = typename TypeParam::curve;
        // the public form, sign_digest(digest, crypto::deterministic),
        // takes RFC 6979's HMAC from the digest's length: every SHA-2 and
        // SHA-1 gives OpenSSL's signature, DER and raw alike
        for (const char* md : {"SHA1", "SHA224", "SHA256", "SHA384", "SHA512"}) {
            bytes_t digest = digest_of(md, msg);
            bytes_t theirs = ossl_sign_deterministic(ossl, md, digest);
            ASSERT_FALSE(theirs.empty());
            EXPECT_EQ(hex(to_bytes(key->sign_digest(view(digest), crypto::deterministic))), hex(theirs)) << md << " d=" << hex(d);
            bytes_t raw = to_bytes(key->sign_digest_raw(view(digest), crypto::deterministic));
            unsigned char der[crypto::detail::Ecdsa<C>::max_der_size];
            size_t n = crypto::detail::Ecdsa<C>::encode_signature(der, raw.data());
            EXPECT_EQ(hex(bytes_t(der, der + n)), hex(theirs)) << md;
        }
        // with the hash named: SHA-3 and SHA-512/256 too, whose lengths
        // are SHA-2's
        struct {
            const char* md;
            crypto::hash_id id;
        } named[] = {{"SHA256", crypto::hash_id::sha256}, {"SHA512-256", crypto::hash_id::sha512_256},
                     {"SHA3-256", crypto::hash_id::sha3_256}, {"SHA3-384", crypto::hash_id::sha3_384},
                     {"SHA3-512", crypto::hash_id::sha3_512}, {"SHA224", crypto::hash_id::sha224}};
        for (auto& c : named) {
            bytes_t digest = digest_of(c.md, msg);
            bytes_t theirs = ossl_sign_deterministic(ossl, c.md, digest);
            ASSERT_FALSE(theirs.empty()) << c.md;
            EXPECT_EQ(hex(to_bytes(key->sign_digest(view(digest), crypto::deterministic, c.id))), hex(theirs)) << c.md;
            bytes_t raw = to_bytes(key->sign_digest_raw(view(digest), crypto::deterministic, c.id));
            unsigned char der[crypto::detail::Ecdsa<C>::max_der_size];
            size_t n = crypto::detail::Ecdsa<C>::encode_signature(der, raw.data());
            EXPECT_EQ(hex(bytes_t(der, der + n)), hex(theirs)) << c.md;
        }
        // digests that are not below n as numbers (all ones, n, n + 1):
        // RFC 6979's bits2octets reduces them before the HMAC takes them
        bytes_t n = unhex(TypeParam::order), n1 = n;
        n1.back() += 1;
        for (const bytes_t& digest : {bytes_t(TypeParam::size, 0xff), n, n1}) {
            const char* md = TypeParam::size == 32 ? "SHA256" : "SHA384";
            bytes_t mine = deterministic<C, typename TypeParam::hash>(d, digest);
            unsigned char der[crypto::detail::Ecdsa<C>::max_der_size];
            size_t len = crypto::detail::Ecdsa<C>::encode_signature(der, mine.data());
            EXPECT_EQ(hex(bytes_t(der, der + len)), hex(ossl_sign_deterministic(ossl, md, digest))) << hex(digest);
        }
    }
}

// --- ECDSA against OpenSSL ------------------------------------------------------

// Random keys, digests of every length 1..100 (truncated to the order's
// bits, as FIPS 186-5 has it): our signatures verify under OpenSSL and its
// under us, in DER and raw; a flipped bit of the digest or the signature,
// another key, a signature cut short, all refuse
TYPED_TEST(Crypto_Ec, EcdsaAgainstOpenSsl) {
    random_source r(4);
    for (int i = 1; i < 150; ++i) {
        auto key = TypeParam::private_key::generate();
        auto pub = key.public_key();
        bytes_t d = to_bytes(key.bytes().bytes());
        Pkey ossl = ossl_key(TypeParam::group, &d, to_bytes(pub.bytes()));
        ASSERT_TRUE(ossl);
        bytes_t digest = r.bytes(i < 101 ? size_t(i) : 1 + r.below(100));

        bytes_t mine = to_bytes(key.sign_digest(view(digest)));
        EXPECT_LE(mine.size(), TypeParam::private_key::max_signature_size);
        EXPECT_TRUE(ossl_verify(ossl, digest, mine)) << "len " << digest.size();
        EXPECT_TRUE(pub.verify_digest(view(digest), view(mine)));

        bytes_t theirs = ossl_sign(ossl, digest);
        ASSERT_FALSE(theirs.empty());
        EXPECT_TRUE(pub.verify_digest(view(digest), view(theirs))) << "len " << digest.size();

        bytes_t raw = to_bytes(key.sign_digest_raw(view(digest)));
        ASSERT_EQ(raw.size(), 2 * TypeParam::size);
        EXPECT_TRUE(pub.verify_digest_raw(view(digest), view(raw)));
        EXPECT_FALSE(pub.verify_digest(view(digest), view(raw)));    // raw is not DER
        EXPECT_FALSE(pub.verify_digest_raw(view(digest), view(mine))); // DER is not raw

        // hedged: two signatures of one digest differ
        EXPECT_NE(mine, to_bytes(key.sign_digest(view(digest))));

        bytes_t bad = mine;
        bad[r.below(bad.size())] ^= (unsigned char)(1u << r.below(8));
        EXPECT_FALSE(pub.verify_digest(view(digest), view(bad)));
        EXPECT_FALSE(ossl_verify(ossl, digest, bad));
        bad = raw;
        bad[r.below(bad.size())] ^= (unsigned char)(1u << r.below(8));
        EXPECT_FALSE(pub.verify_digest_raw(view(digest), view(bad)));
        if (!digest.empty()) {
            bytes_t other = digest;
            other[r.below(std::min<size_t>(other.size(), TypeParam::size))] ^= 1;
            EXPECT_FALSE(pub.verify_digest(view(other), view(mine)));
        }
        EXPECT_FALSE(pub.verify_digest(view(digest), view(bytes_t(mine.begin(), mine.end() - 1))));
        EXPECT_FALSE(pub.verify_digest_raw(view(digest), view(bytes_t(raw.begin(), raw.end() - 1))));
        auto stranger = TypeParam::private_key::generate();
        EXPECT_FALSE(stranger.public_key().verify_digest(view(digest), view(mine)));
    }
}

// An empty digest: signing it is std::invalid_argument, as Go refuses it,
// and no signature verifies it
TYPED_TEST(Crypto_Ec, EmptyDigestIsRefused) {
    auto key = TypeParam::private_key::generate();
    bytes_t none;
    EXPECT_THROW((void)key.sign_digest(view(none)), std::invalid_argument);
    EXPECT_THROW((void)key.sign_digest_raw(view(none)), std::invalid_argument);
    EXPECT_THROW((void)key.sign_digest(view(none), crypto::deterministic), std::invalid_argument);
    EXPECT_THROW((void)key.sign_digest(view(none), crypto::deterministic, crypto::hash_id::sha256), std::invalid_argument);
    // a digest of another length than the named hash's
    EXPECT_THROW((void)key.sign_digest(view(bytes_t(20, 1)), crypto::deterministic, crypto::hash_id::sha256), std::invalid_argument);
    bytes_t zero(1, 0);
    bytes_t sig = to_bytes(key.sign_digest(view(zero)));
    bytes_t raw = to_bytes(key.sign_digest_raw(view(zero)));
    EXPECT_TRUE(key.public_key().verify_digest(view(zero), view(sig)));
    EXPECT_FALSE(key.public_key().verify_digest(view(none), view(sig)));
    EXPECT_FALSE(key.public_key().verify_digest_raw(view(none), view(raw)));
}

// The deterministic signature is one: the same key and digest give the
// same bytes, and it verifies
TYPED_TEST(Crypto_Ec, DeterministicIsRepeatable) {
    random_source r(8);
    auto key = TypeParam::private_key::generate();
    bytes_t digest = r.bytes(TypeParam::size);
    bytes_t a = to_bytes(key.sign_digest(view(digest), crypto::deterministic));
    EXPECT_EQ(a, to_bytes(key.sign_digest(view(digest), crypto::deterministic)));
    EXPECT_NE(a, to_bytes(key.sign_digest(view(digest))));
    EXPECT_TRUE(key.public_key().verify_digest(view(digest), view(a)));
}

// Only the digest's leftmost order-size bytes count: a longer digest that
// shares them verifies the same signature, a shorter one is a number of
// its own
TYPED_TEST(Crypto_Ec, DigestTruncation) {
    random_source r(5);
    auto key = TypeParam::private_key::generate();
    auto pub = key.public_key();
    bytes_t digest = r.bytes(TypeParam::size + 16);
    bytes_t sig = to_bytes(key.sign_digest(view(digest)));
    bytes_t tail = digest;
    tail.back() ^= 0xff;
    EXPECT_TRUE(pub.verify_digest(view(tail), view(sig)));
    bytes_t cut(digest.begin(), digest.begin() + TypeParam::size);
    EXPECT_TRUE(pub.verify_digest(view(cut), view(sig)));
    bytes_t shorter(digest.begin(), digest.begin() + TypeParam::size - 1);
    EXPECT_FALSE(pub.verify_digest(view(shorter), view(sig)));
    // a short digest is left-padded: 00 || h is the same number as h
    bytes_t h = r.bytes(20);
    bytes_t sig2 = to_bytes(key.sign_digest(view(h)));
    bytes_t padded(TypeParam::size - 20, 0);
    padded.insert(padded.end(), h.begin(), h.end());
    EXPECT_TRUE(pub.verify_digest(view(padded), view(sig2)));
}

// Strict DER: r and s in their shortest form, non-negative, nothing after;
// r or s of 0 or not below n refuse; (r, n - s) verifies as (r, s) does,
// as ECDSA has it (Go and OpenSSL alike)
TYPED_TEST(Crypto_Ec, SignatureEncoding) {
    random_source r(6);
    auto key = TypeParam::private_key::generate();
    auto pub = key.public_key();
    bytes_t digest = r.bytes(TypeParam::size);
    bytes_t raw = to_bytes(key.sign_digest_raw(view(digest)));
    bytes_t rb(raw.begin(), raw.begin() + TypeParam::size), sb(raw.begin() + TypeParam::size, raw.end());
    auto integer = [](bytes_t v, bool pad) {
        while (v.size() > 1 && v[0] == 0) {
            v.erase(v.begin());
        }
        if (pad || (v[0] & 0x80)) {
            v.insert(v.begin(), 0);
        }
        bytes_t out = {0x02, (unsigned char)v.size()};
        out.insert(out.end(), v.begin(), v.end());
        return out;
    };
    auto sequence = [](const bytes_t& a, const bytes_t& b, bool long_form) {
        bytes_t out = {0x30};
        if (long_form) {
            out.push_back(0x81);
        }
        out.push_back((unsigned char)(a.size() + b.size()));
        out.insert(out.end(), a.begin(), a.end());
        out.insert(out.end(), b.begin(), b.end());
        return out;
    };
    bytes_t good = sequence(integer(rb, false), integer(sb, false), false);
    EXPECT_TRUE(pub.verify_digest(view(digest), view(good)));
    // an extra zero byte in front of an integer that did not need it
    if (!(rb[0] & 0x80)) {
        EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(rb, true), integer(sb, false), false))));
    }
    // a long-form length where the short one fits
    bytes_t lf = sequence(integer(rb, false), integer(sb, false), true);
    if (lf[2] < 0x80) {
        EXPECT_FALSE(pub.verify_digest(view(digest), view(lf)));
    }
    // trailing data, inside and outside the sequence
    bytes_t trailing = good;
    trailing.push_back(0);
    EXPECT_FALSE(pub.verify_digest(view(digest), view(trailing)));
    bytes_t inner = integer(sb, false);
    inner.push_back(0x05);
    inner.push_back(0x00);
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(rb, false), inner, false))));
    // a negative r: its top bit set with no zero in front
    bytes_t neg = rb;
    neg[0] |= 0x80;
    bytes_t negint = {0x02, (unsigned char)neg.size()};
    negint.insert(negint.end(), neg.begin(), neg.end());
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(negint, integer(sb, false), false))));
    // r = 0, s = 0, s = n, r = n
    bytes_t z = {0};
    bytes_t n = unhex(TypeParam::order);
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(z, false), integer(sb, false), false))));
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(rb, false), integer(z, false), false))));
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(rb, false), integer(n, false), false))));
    EXPECT_FALSE(pub.verify_digest(view(digest), view(sequence(integer(n, false), integer(sb, false), false))));
    bytes_t zeros(2 * TypeParam::size, 0);
    EXPECT_FALSE(pub.verify_digest_raw(view(digest), view(zeros)));
    // s' = n - s
    BIGNUM *bn = BN_bin2bn(n.data(), int(n.size()), nullptr), *bs = BN_bin2bn(sb.data(), int(sb.size()), nullptr);
    BN_sub(bs, bn, bs);
    bytes_t s2(TypeParam::size);
    BN_bn2binpad(bs, s2.data(), int(s2.size()));
    BN_free(bn);
    BN_free(bs);
    bytes_t raw2 = rb;
    raw2.insert(raw2.end(), s2.begin(), s2.end());
    EXPECT_TRUE(pub.verify_digest_raw(view(digest), view(raw2)));
    // r + n (when it fits) is not r
    EXPECT_FALSE(pub.verify_digest(view(digest), view(bytes_t{})));
    EXPECT_FALSE(pub.verify_digest(view(digest), view(bytes_t{0x30, 0x00})));
}

// s + n is s modulo n and would verify were the range not checked: a
// signature is made with s = 1 (the digest chosen as k - r d), so that
// s + n fits in the curve's size: (r, 1 + n) must refuse, and (r, 1)
// itself verify
TYPED_TEST(Crypto_Ec, SignatureOutOfRange) {
    random_source r(10);
    bytes_t n = unhex(TypeParam::order);
    BN_CTX* ctx = BN_CTX_new();
    BIGNUM* bn = BN_bin2bn(n.data(), int(n.size()), nullptr);
    for (int i = 0; i < 8; ++i) {
        bytes_t d = random_scalar<TypeParam>(r);
        bytes_t k = random_scalar<TypeParam>(r);
        auto key = TypeParam::private_key::from_bytes(view(d));
        bytes_t kg = ossl_public_of(TypeParam::nid, k);
        BIGNUM *bx = BN_bin2bn(kg.data() + 1, int(TypeParam::size), nullptr), *br = BN_new();
        BN_nnmod(br, bx, bn, ctx);
        BIGNUM *bd = BN_bin2bn(d.data(), int(d.size()), nullptr), *bk = BN_bin2bn(k.data(), int(k.size()), nullptr), *be = BN_new();
        BN_mod_mul(be, br, bd, bn, ctx);
        BN_mod_sub(be, bk, be, bn, ctx);   // e = k - r d, so s = k^-1 (e + r d) = 1
        bytes_t digest(TypeParam::size), rb(TypeParam::size);
        BN_bn2binpad(be, digest.data(), int(digest.size()));
        BN_bn2binpad(br, rb.data(), int(rb.size()));
        bytes_t one(TypeParam::size, 0);
        one.back() = 1;
        bytes_t sig = rb;
        sig.insert(sig.end(), one.begin(), one.end());
        auto pub = key->public_key();
        EXPECT_TRUE(pub.verify_digest_raw(view(digest), view(sig)));
        bytes_t n1 = n;
        n1.back() += 1;   // n + 1: n ends in an odd byte below 0xff
        bytes_t bad = rb;
        bad.insert(bad.end(), n1.begin(), n1.end());
        EXPECT_FALSE(pub.verify_digest_raw(view(digest), view(bad)));
        BN_free(bx);
        BN_free(br);
        BN_free(bd);
        BN_free(bk);
        BN_free(be);
    }
    BN_free(bn);
    BN_CTX_free(ctx);
}

// The x of the point u1 G + u2 Q may be n or more (below p): verification
// compares x mod n with r. A point with x = n + i is found by decompression,
// the public key made Q = r^-1 P with r = x - n, so that (r, s = 1) signs
// the digest 0 (u1 = 0, u2 = r); OpenSSL agrees
TYPED_TEST(Crypto_Ec, VerifyReducesX) {
    bytes_t n = unhex(TypeParam::order);
    EC_GROUP* g = EC_GROUP_new_by_curve_name(TypeParam::nid);
    BN_CTX* ctx = BN_CTX_new();
    BIGNUM* bn = BN_bin2bn(n.data(), int(n.size()), nullptr);
    int found = 0;
    for (unsigned i = 0; i < 64 && found < 4; ++i) {
        BIGNUM* bx = BN_dup(bn);
        BN_add_word(bx, i);
        bytes_t comp(1 + TypeParam::size);
        comp[0] = 2;
        BN_bn2binpad(bx, comp.data() + 1, int(TypeParam::size));
        EC_POINT* p = EC_POINT_new(g);
        if (EC_POINT_oct2point(g, p, comp.data(), comp.size(), ctx) == 1) {
            ++found;
            BIGNUM *br = BN_new(), *brinv = BN_new();
            BN_sub(br, bx, bn);   // r = x - n = i
            if (!BN_is_zero(br)) {
                BN_mod_inverse(brinv, br, bn, ctx);
                EC_POINT* q = EC_POINT_new(g);
                EC_POINT_mul(g, q, nullptr, p, brinv, ctx);
                bytes_t qb(1 + 2 * TypeParam::size);
                EC_POINT_point2oct(g, q, POINT_CONVERSION_UNCOMPRESSED, qb.data(), qb.size(), ctx);
                auto pub = TypeParam::public_key::from_bytes(view(qb));
                ASSERT_TRUE(pub.has_value());
                bytes_t sig(2 * TypeParam::size, 0);
                BN_bn2binpad(br, sig.data(), int(TypeParam::size));
                sig.back() = 1;
                bytes_t digest(TypeParam::size, 0);
                EXPECT_TRUE(pub->verify_digest_raw(view(digest), view(sig))) << "x = n + " << i;
                using Ecdsa = crypto::detail::Ecdsa<typename TypeParam::curve>;
                unsigned char der[Ecdsa::max_der_size];
                size_t len = Ecdsa::encode_signature(der, sig.data());
                EXPECT_TRUE(pub->verify_digest(view(digest), view(bytes_t(der, der + len))));
                Pkey ossl = ossl_key(TypeParam::group, nullptr, qb);
                EXPECT_TRUE(ossl_verify(ossl, digest, bytes_t(der, der + len)));
                EC_POINT_free(q);
            }
            BN_free(br);
            BN_free(brinv);
        }
        EC_POINT_free(p);
        BN_free(bx);
    }
    EXPECT_GT(found, 1);
    BN_free(bn);
    BN_CTX_free(ctx);
    EC_GROUP_free(g);
}

// A coordinate given as itself plus p (the same number modulo p) is not a
// coordinate: a point with a small x is found, then x + p refused in both
// encodings, as OpenSSL refuses it
TYPED_TEST(Crypto_Ec, CoordinatePlusPRefused) {
    using PK = typename TypeParam::public_key;
    bytes_t p = unhex(TypeParam::prime);
    for (unsigned x = 1; x < 64; ++x) {
        bytes_t comp(1 + TypeParam::size, 0);
        comp[0] = 2;
        comp.back() = (unsigned char)x;
        auto k = PK::from_bytes(view(comp));
        if (!k) {
            continue;
        }
        bytes_t u = to_bytes(k->bytes());
        // x + p, which fits in the coordinate's bytes for a small x
        BIGNUM* b = BN_bin2bn(p.data(), int(p.size()), nullptr);
        BN_add_word(b, x);
        bytes_t xp(TypeParam::size);
        ASSERT_EQ(BN_bn2binpad(b, xp.data(), int(xp.size())), int(TypeParam::size));
        BN_free(b);
        bytes_t bad = u;
        std::copy(xp.begin(), xp.end(), bad.begin() + 1);
        EXPECT_FALSE(PK::from_bytes(view(bad)).has_value());
        EXPECT_FALSE(ossl_point_valid(TypeParam::nid, bad));
        bytes_t badc = comp;
        std::copy(xp.begin(), xp.end(), badc.begin() + 1);
        EXPECT_FALSE(PK::from_bytes(view(badc)).has_value());
        return;
    }
    FAIL() << "no small x on the curve";
}

// --- ECDH against OpenSSL -------------------------------------------------------

TYPED_TEST(Crypto_Ec, EcdhAgainstOpenSsl) {
    for (int i = 0; i < 100; ++i) {
        auto a = TypeParam::ecdh_key::generate();
        auto b = TypeParam::ecdh_key::generate();
        auto ab = a.shared_secret(b.public_key());
        auto ba = b.shared_secret(a.public_key());
        ASSERT_TRUE(ab.has_value());
        ASSERT_TRUE(ba.has_value());
        EXPECT_EQ(to_bytes(ab->bytes()), to_bytes(ba->bytes()));
        bytes_t da = to_bytes(a.bytes().bytes());
        Pkey oa = ossl_key(TypeParam::group, &da, to_bytes(a.public_key().bytes()));
        Pkey ob = ossl_key(TypeParam::group, nullptr, to_bytes(b.public_key().bytes()));
        ASSERT_TRUE(oa && ob);
        EXPECT_EQ(hex(to_bytes(ab->bytes())), hex(ossl_derive(oa, ob)));
    }
    // an ECDSA key's scalar as an ECDH key: the same public point
    auto s = TypeParam::private_key::generate();
    auto e = s.to_ecdh();
    EXPECT_TRUE(s.public_key() == e.public_key());
    EXPECT_EQ(to_bytes(s.bytes().bytes()), to_bytes(e.bytes().bytes()));
}

// --- keys from bytes --------------------------------------------------------------

// SEC 1 points: uncompressed and compressed round trips; random bytes,
// bit flips of valid points, coordinates not below p, the point at
// infinity, other lengths and first bytes, accepted exactly when OpenSSL
// accepts them
TYPED_TEST(Crypto_Ec, PublicKeyFromBytes) {
    using PK = typename TypeParam::public_key;
    random_source r(7);
    const size_t cs = TypeParam::size;
    auto agree = [&](const bytes_t& b) {
        auto k = PK::from_bytes(view(b));
        bool ossl = ossl_point_valid(TypeParam::nid, b);
        EXPECT_EQ(k.has_value(), ossl) << hex(b);
        if (!k) {
            EXPECT_EQ(k.error().code(), crypto::errc::invalid_key);
        }
        return k.has_value();
    };
    for (int i = 0; i < 200; ++i) {
        auto key = TypeParam::private_key::generate();
        bytes_t u = to_bytes(key.public_key().bytes());
        bytes_t c = to_bytes(key.public_key().bytes_compressed());
        ASSERT_EQ(u.size(), 1 + 2 * cs);
        ASSERT_EQ(c.size(), 1 + cs);
        EXPECT_EQ(c[0], 2 + (u.back() & 1));
        auto from_c = PK::from_bytes(view(c));
        ASSERT_TRUE(from_c.has_value());
        EXPECT_EQ(to_bytes(from_c->bytes()), u);
        EXPECT_TRUE(agree(u));
        EXPECT_TRUE(agree(c));
        // one bit flipped: off the curve nearly always, OpenSSL decides
        bytes_t f = u;
        f[1 + r.below(2 * cs)] ^= (unsigned char)(1u << r.below(8));
        agree(f);
        bytes_t fc = c;
        fc[1 + r.below(cs)] ^= (unsigned char)(1u << r.below(8));
        agree(fc);
        bytes_t rnd = r.bytes(1 + 2 * cs);
        rnd[0] = 4;
        EXPECT_FALSE(agree(rnd));
        bytes_t rndc = r.bytes(1 + cs);
        rndc[0] = 2 + (unsigned char)r.below(2);
        agree(rndc);   // about half of all x have a y
    }
    // x or y not below p
    bytes_t p = unhex(TypeParam::prime);
    auto key = TypeParam::private_key::generate();
    bytes_t u = to_bytes(key.public_key().bytes());
    bytes_t big = u;
    std::copy(p.begin(), p.end(), big.begin() + 1);
    EXPECT_FALSE(PK::from_bytes(view(big)).has_value());
    big = u;
    std::copy(p.begin(), p.end(), big.begin() + 1 + cs);
    EXPECT_FALSE(PK::from_bytes(view(big)).has_value());
    bytes_t bigc = {2};
    bigc.insert(bigc.end(), p.begin(), p.end());
    EXPECT_FALSE(PK::from_bytes(view(bigc)).has_value());
    // x + p in place of x (the same number mod p) is refused too
    // the point at infinity, and the other shapes
    EXPECT_EQ(error_of(PK::from_bytes(view(bytes_t{0}))).code(), crypto::errc::invalid_key);
    EXPECT_FALSE(PK::from_bytes(view(bytes_t{})).has_value());
    bytes_t hybrid = u;
    hybrid[0] = 6 + (u.back() & 1);
    EXPECT_FALSE(PK::from_bytes(view(hybrid)).has_value());
    EXPECT_FALSE(PK::from_bytes(view(bytes_t(u.begin(), u.end() - 1))).has_value());
    bytes_t longer = u;
    longer.push_back(0);
    EXPECT_FALSE(PK::from_bytes(view(longer)).has_value());
    bytes_t zeros(1 + 2 * cs, 0);
    zeros[0] = 4;
    EXPECT_FALSE(PK::from_bytes(view(zeros)).has_value());
    EXPECT_FALSE(PK::from_bytes(view(bytes_t(1 + 2 * cs, 0))).has_value());
}

// Private scalars: 1 and n - 1 are keys, 0, n, n + 1 and all ones are not,
// nor another length; the public point of a scalar is OpenSSL's
TYPED_TEST(Crypto_Ec, PrivateKeyFromBytes) {
    using SK = typename TypeParam::private_key;
    bytes_t n = unhex(TypeParam::order);
    bytes_t zero(TypeParam::size, 0), one = zero, n1 = n, n_plus = n;
    one.back() = 1;
    n1.back() -= 1;
    n_plus.back() += 1;
    EXPECT_TRUE(SK::from_bytes(view(one)).has_value());
    EXPECT_TRUE(SK::from_bytes(view(n1)).has_value());
    for (const bytes_t& b : {zero, n, n_plus, bytes_t(TypeParam::size, 0xff), bytes_t(TypeParam::size - 1, 1), bytes_t(TypeParam::size + 1, 1)}) {
        auto k = SK::from_bytes(view(b));
        ASSERT_FALSE(k.has_value()) << hex(b);
        EXPECT_EQ(k.error().code(), crypto::errc::invalid_key);
        EXPECT_FALSE(TypeParam::ecdh_key::from_bytes(view(b)).has_value());
    }
    random_source r(8);
    for (int i = 0; i < 50; ++i) {
        bytes_t d = random_scalar<TypeParam>(r);
        auto k = SK::from_bytes(view(d));
        ASSERT_TRUE(k.has_value());
        EXPECT_EQ(to_bytes(k->public_key().bytes()), ossl_public_of(TypeParam::nid, d));
        EXPECT_EQ(to_bytes(k->bytes().bytes()), d);
        auto e = TypeParam::ecdh_key::from_bytes(view(d));
        ASSERT_TRUE(e.has_value());
        EXPECT_TRUE(e->public_key() == k->public_key());
    }
    // the error names the curve
    std::string msg = text_of(error_of(SK::from_bytes(view(zero))));
    EXPECT_NE(msg.find(TypeParam::size == 32 ? "p256" : "p384"), std::string::npos) << msg;
}

// --- DER --------------------------------------------------------------------------

// PKCS#8, SEC 1 and SPKI, both ways with OpenSSL: what we write OpenSSL
// reads to the same scalar and point, what OpenSSL writes we read; SPKI
// byte for byte; every prefix of a valid encoding and one byte more refuse
// with malformed; a key of the other curve is unsupported
TYPED_TEST(Crypto_Ec, DerAgainstOpenSsl) {
    using SK = typename TypeParam::private_key;
    using PK = typename TypeParam::public_key;
    for (int i = 0; i < 40; ++i) {
        auto key = SK::generate();
        bytes_t d = to_bytes(key.bytes().bytes());
        bytes_t u = to_bytes(key.public_key().bytes());
        Pkey ossl = ossl_key(TypeParam::group, &d, u);
        ASSERT_TRUE(ossl);

        bytes_t p8 = to_bytes(key.to_pkcs8_der());
        bytes_t s1 = to_bytes(key.to_sec1_der());
        bytes_t spki = to_bytes(key.public_key().to_pkix_der());
        EXPECT_EQ(ossl_scalar_of_der(p8, TypeParam::size), d);
        EXPECT_EQ(ossl_scalar_of_der(s1, TypeParam::size), d);
        EXPECT_EQ(hex(spki), hex(ossl_spki(ossl)));
        EXPECT_EQ(ossl_point_of_spki(spki), u);
        EXPECT_EQ(hex(p8), hex(ossl_pkcs8(ossl)));   // the same shape as OpenSSL's

        bytes_t their_p8 = ossl_pkcs8(ossl);
        bytes_t their_s1 = ossl_sec1(ossl);
        auto a = SK::from_pkcs8_der(view(their_p8));
        auto b = SK::from_sec1_der(view(their_s1));
        auto c = PK::from_pkix_der(view(ossl_spki(ossl)));
        auto e = TypeParam::ecdh_key::from_pkcs8_der(view(their_p8));
        ASSERT_TRUE(a.has_value()) << text_of(a.error());
        ASSERT_TRUE(b.has_value()) << text_of(b.error());
        ASSERT_TRUE(c.has_value()) << text_of(c.error());
        ASSERT_TRUE(e.has_value());
        EXPECT_EQ(to_bytes(a->bytes().bytes()), d);
        EXPECT_EQ(to_bytes(b->bytes().bytes()), d);
        EXPECT_TRUE(*c == key.public_key());
        EXPECT_EQ(to_bytes(e->to_pkcs8_der()), p8);

        if (i == 0) {
            for (const bytes_t* der : {&p8, &s1, &spki}) {
                for (size_t n = 0; n < der->size(); ++n) {
                    bytes_t cut(der->begin(), der->begin() + n);
                    bool refused = der == &spki ? !PK::from_pkix_der(view(cut)).has_value()
                                 : der == &p8   ? !SK::from_pkcs8_der(view(cut)).has_value()
                                                : !SK::from_sec1_der(view(cut)).has_value();
                    EXPECT_TRUE(refused) << n;
                }
                bytes_t more = *der;
                more.push_back(0);
                EXPECT_FALSE(der == &spki ? PK::from_pkix_der(view(more)).has_value()
                             : der == &p8 ? SK::from_pkcs8_der(view(more)).has_value()
                                          : SK::from_sec1_der(view(more)).has_value());
            }
            auto err = SK::from_pkcs8_der(view(bytes_t(p8.begin(), p8.begin() + 10)));
            ASSERT_FALSE(err);
            EXPECT_EQ(err.error().code(), crypto::errc::malformed);
        }
    }
    // the other curve's key
    using Other = std::conditional_t<std::is_same_v<TypeParam, P256>, P384, P256>;
    auto other = Other::private_key::generate();
    auto x = SK::from_pkcs8_der(view(to_bytes(other.to_pkcs8_der())));
    ASSERT_FALSE(x.has_value());
    EXPECT_EQ(x.error().code(), crypto::errc::unsupported);
    auto y = SK::from_sec1_der(view(to_bytes(other.to_sec1_der())));
    ASSERT_FALSE(y.has_value());
    EXPECT_EQ(y.error().code(), crypto::errc::unsupported);
    auto z = PK::from_pkix_der(view(to_bytes(other.public_key().to_pkix_der())));
    ASSERT_FALSE(z.has_value());
    EXPECT_EQ(z.error().code(), crypto::errc::unsupported);
    // an Ed25519 key in PKCS#8 (RFC 8410 §10.3): another algorithm
    bytes_t ed = unhex("302e020100300506032b657004220420d4ee72dbf913584ad5b6d8f1f769f8ad3afe7c28cbf1d4fbe097a88f44755842");
    EXPECT_EQ(error_of(SK::from_pkcs8_der(view(ed))).code(), crypto::errc::unsupported);
}

// A SEC 1 scalar with padding: a zero byte more on the left is dropped, a
// missing one is added (as Go and OpenSSL read them); a scalar out of range
// is invalid_key
TYPED_TEST(Crypto_Ec, Sec1ScalarPadding) {
    using SK = typename TypeParam::private_key;
    random_source r(9);
    bytes_t d = random_scalar<TypeParam>(r);
    d[0] = 0;   // one leading zero byte
    auto build = [](const bytes_t& scalar) {
        bytes_t inner = {0x02, 0x01, 0x01, 0x04, (unsigned char)scalar.size()};
        inner.insert(inner.end(), scalar.begin(), scalar.end());
        bytes_t out = {0x30, (unsigned char)inner.size()};
        out.insert(out.end(), inner.begin(), inner.end());
        return out;
    };
    auto a = SK::from_sec1_der(view(build(bytes_t(d.begin() + 1, d.end()))));
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(to_bytes(a->bytes().bytes()), d);
    bytes_t padded = {0};
    padded.insert(padded.end(), d.begin(), d.end());
    auto b = SK::from_sec1_der(view(build(padded)));
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(to_bytes(b->bytes().bytes()), d);
    bytes_t over = {1};
    over.insert(over.end(), d.begin(), d.end());
    EXPECT_EQ(error_of(SK::from_sec1_der(view(build(over)))).code(), crypto::errc::invalid_key);
    EXPECT_EQ(error_of(SK::from_sec1_der(view(build(bytes_t(TypeParam::size, 0))))).code(), crypto::errc::invalid_key);
    EXPECT_EQ(error_of(SK::from_sec1_der(view(build(unhex(TypeParam::order))))).code(), crypto::errc::invalid_key);
}

// --- the secrets --------------------------------------------------------------------

static_assert(!std::is_copy_constructible_v<crypto::p256::private_key>);
static_assert(!std::is_copy_assignable_v<crypto::p256::private_key>);
static_assert(!std::is_copy_constructible_v<crypto::p384::private_key>);
static_assert(!std::is_copy_constructible_v<crypto::p256::ecdh_key>);
static_assert(!std::is_copy_constructible_v<crypto::p384::ecdh_key>);
static_assert(!std::is_copy_constructible_v<crypto::secret<32>>);
static_assert(std::is_nothrow_move_constructible_v<crypto::p256::private_key>);
static_assert(std::is_copy_constructible_v<crypto::p256::public_key>);

// A move zeroes the scalar of the source, the destructor zeroes it where
// it lay (the scalar is the first member: a probe reads the storage after
// the destructor ran), clone() is the same key
TYPED_TEST(Crypto_Ec, SecretsAreZeroed) {
    using SK = typename TypeParam::private_key;
    using EK = typename TypeParam::ecdh_key;
    constexpr size_t words = 8 * TypeParam::curve::words;

    auto a = SK::generate();
    EXPECT_FALSE(all_zero(a, words));
    auto c = a.clone();
    EXPECT_EQ(to_bytes(c.bytes().bytes()), to_bytes(a.bytes().bytes()));
    SK b = std::move(a);
    EXPECT_TRUE(all_zero(a, words));
    EXPECT_FALSE(all_zero(b, words));
    SK d = SK::generate();
    d = std::move(b);
    EXPECT_TRUE(all_zero(b, words));
    EXPECT_EQ(to_bytes(d.bytes().bytes()), to_bytes(c.bytes().bytes()));

    alignas(SK) unsigned char storage[sizeof(SK)];
    SK* p = new (storage) SK(SK::generate());
    EXPECT_FALSE(all_zero(*p, words));
    p->~SK();
    EXPECT_TRUE(all_zero(storage, words));

    alignas(EK) unsigned char estorage[sizeof(EK)];
    EK* e = new (estorage) EK(EK::generate());
    EXPECT_FALSE(all_zero(*e, words));
    e->~EK();
    EXPECT_TRUE(all_zero(estorage, words));

    // the shared secret
    auto x = EK::generate();
    auto y = EK::generate();
    auto s = x.shared_secret(y.public_key());
    ASSERT_TRUE(s.has_value());
    using S = std::remove_cvref_t<decltype(*s)>;
    alignas(S) unsigned char sstorage[sizeof(S)];
    S* sp = new (sstorage) S(std::move(*s));
    EXPECT_TRUE(all_zero(*s));
    EXPECT_FALSE(all_zero(*sp));
    sp->~S();
    EXPECT_TRUE(all_zero(sstorage));

    // a key moved from is used by no operation: std::logic_error, as every
    // key of the module's (it once signed with d = 0 and gave a signature
    // well formed and worthless)
    EK z = EK::generate();
    EK w = std::move(z);
    EXPECT_THROW((void)z.shared_secret(w.public_key()), std::logic_error);
    EXPECT_THROW((void)z.public_key(), std::logic_error);
    EXPECT_THROW((void)z.bytes(), std::logic_error);
    EXPECT_THROW((void)z.to_pkcs8_der(), std::logic_error);
    SK from = SK::generate();
    SK to = std::move(from);
    bytes_t digest(TypeParam::size, 1);
    EXPECT_THROW((void)from.sign_digest(view(digest)), std::logic_error);
    EXPECT_THROW((void)from.sign_digest_raw(view(digest), crypto::deterministic), std::logic_error);
    EXPECT_THROW((void)from.to_ecdh(), std::logic_error);
    EXPECT_THROW((void)from.to_sec1_der(), std::logic_error);
    EXPECT_TRUE(to.public_key().verify_digest(view(digest), view(to_bytes(to.sign_digest(view(digest))))));
    // clone() too: it once gave another key moved from, silently
    EXPECT_THROW((void)from.clone(), std::logic_error);
    EXPECT_THROW((void)z.clone(), std::logic_error);
    EXPECT_FALSE(noexcept(from.clone()));
    EXPECT_FALSE(noexcept(z.clone()));
    // the message names the curve and the type, as every other error of
    // the curve names the curve
    const std::string curve = TypeParam::size == 32 ? "p256" : "p384";
    auto message = [](auto&& call) {
        try {
            call();
        } catch (const std::logic_error& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    EXPECT_EQ(message([&] { (void)from.public_key(); }), "sgcl::crypto::" + curve + "::private_key: used after being moved from");
    EXPECT_EQ(message([&] { (void)z.public_key(); }), "sgcl::crypto::" + curve + "::ecdh_key: used after being moved from");
}

// --- Wycheproof -----------------------------------------------------------------------

namespace {
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

    bytes_t digest_named(const std::string& sha, const bytes_t& msg) {
        const char* md = sha == "SHA-256" ? "SHA256" : sha == "SHA-384" ? "SHA384" : sha == "SHA-512" ? "SHA512"
                       : sha == "SHA3-256" ? "SHA3-256" : sha == "SHA3-384" ? "SHA3-384" : sha == "SHA3-512" ? "SHA3-512"
                       : sha == "SHA-224" ? "SHA224" : nullptr;
        return md ? digest_of(md, msg) : bytes_t{};
    }

    // ecdsa_<curve>_<sha>_test.json (DER) and ..._p1363_test.json (r || s):
    // "valid" must verify, "invalid" must not, "acceptable" either
    template<class T>
    void wycheproof_ecdsa(const char* file, bool p1363) {
        auto doc = wycheproof(file);
        if (!doc) {
            GTEST_SKIP() << file << " not on disk";
        }
        size_t cases = 0;
        for (auto& group : (*doc)["testGroups"].elements()) {
            bytes_t point = unhex(field(group["publicKey"], "uncompressed"));
            auto key = T::public_key::from_bytes(view(point));
            ASSERT_TRUE(key.has_value()) << file;
            std::string sha = field(group, "sha");
            for (auto& t : group["tests"].elements()) {
                bytes_t digest = digest_named(sha, unhex(field(t, "msg")));
                bytes_t sig = unhex(field(t, "sig"));
                bool ok = p1363 ? key->verify_digest_raw(view(digest), view(sig)) : key->verify_digest(view(digest), view(sig));
                std::string result = field(t, "result");
                if (result != "acceptable") {
                    EXPECT_EQ(ok, result == "valid") << file << " tcId " << t["tcId"].as_int().value_or(0);
                }
                ++cases;
            }
        }
        EXPECT_GT(cases, 0u);
    }

    // ecdh_<curve>_test.json: the peer as SPKI (some malformed, some with a
    // compressed point: "acceptable"), the private key as hex (sometimes
    // with a leading zero byte), the shared secret
    template<class T>
    void wycheproof_ecdh(const char* file) {
        auto doc = wycheproof(file);
        if (!doc) {
            GTEST_SKIP() << file << " not on disk";
        }
        size_t cases = 0;
        for (auto& group : (*doc)["testGroups"].elements()) {
            for (auto& t : group["tests"].elements()) {
                std::string result = field(t, "result");
                bytes_t priv = unhex(field(t, "private"));
                while (priv.size() > T::size && priv[0] == 0) {
                    priv.erase(priv.begin());
                }
                while (priv.size() < T::size) {
                    priv.insert(priv.begin(), 0);
                }
                auto key = T::ecdh_key::from_bytes(view(priv));
                auto peer = T::public_key::from_pkix_der(view(unhex(field(t, "public"))));
                bool ok = false;
                if (key && peer) {
                    auto s = key->shared_secret(*peer);
                    ok = s && hex(to_bytes(s->bytes())) == field(t, "shared");
                }
                if (result != "acceptable") {
                    EXPECT_EQ(ok, result == "valid") << file << " tcId " << t["tcId"].as_int().value_or(0);
                }
                ++cases;
            }
        }
        EXPECT_GT(cases, 0u);
    }
}

TEST(Crypto_Wycheproof, EcdsaP256) {
    wycheproof_ecdsa<P256>("ecdsa_secp256r1_sha256_test.json", false);
    wycheproof_ecdsa<P256>("ecdsa_secp256r1_sha512_test.json", false);
    wycheproof_ecdsa<P256>("ecdsa_secp256r1_sha256_p1363_test.json", true);
}

TEST(Crypto_Wycheproof, EcdsaP384) {
    wycheproof_ecdsa<P384>("ecdsa_secp384r1_sha384_test.json", false);
    wycheproof_ecdsa<P384>("ecdsa_secp384r1_sha512_test.json", false);
    wycheproof_ecdsa<P384>("ecdsa_secp384r1_sha384_p1363_test.json", true);
}

TEST(Crypto_Wycheproof, Ecdh) {
    wycheproof_ecdh<P256>("ecdh_secp256r1_test.json");
    wycheproof_ecdh<P384>("ecdh_secp384r1_test.json");
}

// --- Go -------------------------------------------------------------------------------

namespace {
    // One line per case, fields separated by spaces:
    //   curve d pub(uncompressed) digest sig(DER, Go's SignASN1) peer_pub shared pkcs8 sec1 spki
    // written by a Go program (crypto/ecdsa, crypto/ecdh, crypto/x509)
    template<class T>
    void go_case(std::istringstream& in, std::ofstream* out) {
        std::string d, pub, digest, sig, peer, shared, p8, s1, spki;
        in >> d >> pub >> digest >> sig >> peer >> shared >> p8 >> s1 >> spki;
        auto key = T::private_key::from_bytes(view(unhex(d)));
        ASSERT_TRUE(key.has_value());
        EXPECT_EQ(hex(to_bytes(key->public_key().bytes())), pub);
        EXPECT_TRUE(key->public_key().verify_digest(view(unhex(digest)), view(unhex(sig))));
        auto p = T::public_key::from_bytes(view(unhex(peer)));
        ASSERT_TRUE(p.has_value());
        auto s = key->to_ecdh().shared_secret(*p);
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(hex(to_bytes(s->bytes())), shared);
        EXPECT_EQ(hex(to_bytes(key->to_pkcs8_der())), p8);
        EXPECT_EQ(hex(to_bytes(key->to_sec1_der())), s1);
        EXPECT_EQ(hex(to_bytes(key->public_key().to_pkix_der())), spki);
        EXPECT_TRUE(T::private_key::from_pkcs8_der(view(unhex(p8))).has_value());
        EXPECT_TRUE(T::private_key::from_sec1_der(view(unhex(s1))).has_value());
        if (out) {
            // our signature of the same digest, for Go to verify
            *out << (T::size == 32 ? "P-256 " : "P-384 ") << pub << ' ' << digest << ' ' << hex(to_bytes(key->sign_digest(view(unhex(digest))))) << '\n';
        }
    }
}

TEST(Crypto_Ec, GoVectors) {
    const char* path = std::getenv("SGCL_CRYPTO_GO_VECTORS");
    if (!path) {
        GTEST_SKIP() << "SGCL_CRYPTO_GO_VECTORS not set";
    }
    std::ifstream in(path);
    ASSERT_TRUE(in) << path;
    const char* out_path = std::getenv("SGCL_CRYPTO_GO_OUT");
    std::ofstream out;
    if (out_path) {
        out.open(out_path);
    }
    std::string line;
    size_t cases = 0;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string curve;
        fields >> curve;
        if (curve == "P-256") {
            go_case<P256>(fields, out_path ? &out : nullptr);
        } else if (curve == "P-384") {
            go_case<P384>(fields, out_path ? &out : nullptr);
        } else {
            continue;
        }
        ++cases;
    }
    EXPECT_GT(cases, 0u);
}
