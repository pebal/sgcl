//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// RSA: known answers (a key made by OpenSSL, PKCS #1 v1.5 signatures, PSS
// with a given salt and OAEP with a given seed made by Go's crypto/rsa and
// checked by OpenSSL's command line), the Montgomery arithmetic against
// OpenSSL's BIGNUM on moduli of 1 to 65 words, OpenSSL as the oracle in loops
// on keys of 2048, 3072 and 4096 bits (v1.5 signatures equal byte for byte,
// PSS and OAEP in both directions, every hash_id, labels, MGF1 over another
// hash), refusals (a bit flipped, a signature cut or lengthened, another
// digest or hash, encodings a lax verifier takes, OAEP's failures all one
// error), PKCS #1, PKCS #8 and SPKI byte for byte with OpenSSL and the keys
// refused (inconsistent, multi-prime, RSASSA-PSS, small, bad exponent), keys
// made here checked by OpenSSL, unbalanced primes, the fault check, the
// secrets (every block a key frees is zero), and Wycheproof's files when
// they lie in ~/Programming/oracles/wycheproof/testvectors_v1 (skipped
// otherwise).
#include "rsa_common.h"

#include "sgcl/encoding/json.h"

#include <cstdlib>
#include <fstream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace rsa_test;

namespace {
    using PK = crypto::rsa::public_key;
    using SK = crypto::rsa::private_key;
    using Access = crypto::detail::RsaAccess;
    namespace bn = crypto::detail::bn;

    // --- known answers: a 2048-bit key of OpenSSL, the message "abc" ----------
    // (Go 1.27 crypto/rsa made the signatures and ciphertexts; OpenSSL 3.6's
    // pkeyutl verified and decrypted every one)

    const char* const kat_pkcs8 =
        "308204bd020100300d06092a864886f70d0101010500048204a7308204a30201000282010100ad586aed82d75f3f55c2ef7f1500312d27db"
        "b238541c1b98aae017153bb83a218085719733f560cc16501a8d21e3c9e60b724c4decfd40a971dc166c4cfc3522f50dae368d7790c79b39"
        "c3dbaf7d3fbbdccb857df2a69af411d0884d37c6abd5e6d21ca637cd95df790823dc32a1b54912bb0a9d430bd22526c09de02a42cf0415e4"
        "7ebd02c3d73e6944b4624fd22ed0ab295f3bd11a51d7c7c70ac9e626d9fc53cb83ca99a02b2e548f12e99aaca270d4befebcb30a26ea2b3b"
        "4a9bbb881f953b7e40e0301bd6faaf4724b2dafd1a983edd09ebcfcd3ac9a135b87f86d951a55ad54c7aab2078958c8b9ce6607346a42610"
        "448aa67a0d9bab146a3f171917e10203010001028201001b03c40a46d154c53492e02b30752ab4cc58e741f30dbc59430c105cdd453d3dfa"
        "5f11904c6729d2348a00514d5e481943606936ddbfac239c1c7e1c1bb5547ca1af239ed9d62fe883aef1709bbd4bc0f5cc7bfde06289948e"
        "f02e45c672fe55a416cba335e022c2c48479be37dfceb658267125880ce00fad25e894c6514a1ca98dcfbc7f623ef0e1888c71e6890baed0"
        "2526108b48e1e8f6474221ee6e6c584894dae6e89f8f4f4bea9eb19cfad88394842f24fea5ff71cbf22a6b40620b52fd2327fd1768dcc805"
        "4493267a20c30b63f1f78a9b1cd61d88f7714fb3e9e284745a115361f8561201f6c477e79fe66026f03ef86a9ad163404065f47566d36f02"
        "818100e2763b4dc723915617f846cab48ffc11505e071dd79163072871bf2b21d87fd999dc28992805ef7e121263c40ec50ff0d9b248f605"
        "1e167eb548ee008a21098fd4970bb56142c5a18e5f2f49f119aaa47ac1d31ae0faa53ee8d9521fc21f56ffd9901f6a810f74891331de95a5"
        "21543e4d103c35455dc90395a68463629cbd1302818100c3f4942922d17ad42a832ece560b1670b49714261cb0b1829a70a4b55fac9d3771"
        "bb6f4d13bf74c7001ad73833f3f78d004815aeac24886f7c83c0fe3aae8a6f1d2f0a59a822eaa6a3f15c29747750d15d61abfae33b463d67"
        "d0c8192e1f3e2cdefcb8b9cef675655d9f365f2ab02d9ff3e27ef2ec33b7607c813af1ca6f79bb02818031ceab306a120a8f12ea8a375f0b"
        "23f772e39b5116ee650757ffdc79d211d9dcb41855dd898df4a497b8efb45943424448bd0b8b0089238d6eb5c192e0ce6f59f0f9df9b5eee"
        "dea4afb9839f11a6017ff777d5c45da302e1193962f4952f6b650e26c2a21c0c9826e5cfdc12b1a4e8f062e6a5375a304c5bbb784a39c9a0"
        "8b7d028180624117ad4cad16d69d7e6b90c06a0cd57467b08dafa3154eb45bb8eacccf43819280763d82a2644e8809ed525c127f54ef0835"
        "ff0087ccf581cc9a8f9f22f77ce1783dcb58d8312a5f57dd6b57df9fe16ade579a94279b7f9cb77e2dca0796686f62eec914325608fe999f"
        "6cb67de252aaba2e0766524427982640450ec9389302818100c68ce8ebbaab1f7f9e0aed9aec9f53f39b394b3c548b863f4d9ed2cda1ec7b"
        "fd37b2d16942aa6afbc46b8d5f63ff1342524d3e7f947072822ce151504ce3e93b6efca3960a1d50751c4a2da71b3b83418edc8b66d82add"
        "bada0b8a49b5825acba5d9a07ad67e7a8e6ba57d63e74bcc6c032cf9b57af072b6d6db37d26538f356";

    const char* const kat_v15_sha1 =
        "55118e8ee316de1fdd8ee310183ff0dbd9e6a2fbe4c93f2712273da1664387f20ec932e57e75c2b94ae680e5d4a16a0760fe233d79395fda"
        "fffcca38f7a175ce4aaebbbb929e24da3511a2449e3c881e962006d4d2d404641ce8a91ca006b6571f2f8e29ddc3630b81dbcb5c1aee6052"
        "50ff8a2f59fe2e2ee493c8f02d4c1f21a8ab77daa959844e0f520c419a7bc31bee321eef6199eac42a67c80e345994e260b8004bc8362540"
        "e93d4f652567bad0615eada6afe3ded02740b5b3a0eb508da8388e775e73e4a9316925427e66cd58000b235c38bfbe4215104162164195af"
        "fc475133675eb5a89479cb4e8dd72666e143df80ac6e8b3d1d4ad20155538a67";

    const char* const kat_v15_sha256 =
        "a76c0635700aa8a129e263d2c539f1fc56c332b9f6c86dbe611779ea6576750b7cf1019abc3b023412cee6e4457372da737d4c6ed6b2a88f"
        "a3866ec7faa39f5ea712ae61fdad9ccf061ee7d222425c0a6ad6a937c68194cc6bb6d355b9d7a455a9834e54c5c17d7a6021d49040cfdef6"
        "1dd9072670951c9aa16f42168ef898489edd1c689303b7c5d1f7df0c963367b5eb58183ce776b5b15ca8f58847b54f011f60ffbb41ff38e4"
        "fa9f4d427c28b1846d68574c62e429f7cf5d8bc0a32b9a31fee6b7b98fec23b3a364871fc978b58278f69d5f26a747230432bb9fccb6aca8"
        "54a441a821e834e93993fc95b93fb5a5335f47ebef5cadbf429949dd221090b7";

    const char* const kat_v15_sha384 =
        "9016e410bb700e81ee397c26a942ab18107b7fa172a2f4d0b19c8518afc4d6bde37a280ebdd85ba107fe503f79a9496b81e25883ec41f646"
        "01651c8463bd61948a1595285b2506bd09b597dd31b21cd9fa1544906ca84a93ea816145ea44b640ac49fae18b8ab3b0c1553f0830fde996"
        "7941071e3e1f519e4a1a5226f7984d62d9c19bf3604d80dcce6ab81d378d734bdd22a592ff925d1597ece4ca11fee9c6ac39bc68a5e516f5"
        "b614e64a9d5e7439370c5113da68b01d74044b690d39e5a61860ebb02d64aff61b2fd3c1460cc052559cdf16b09c86a269dbd0d32583a6d9"
        "746916d9a848f54ed5f02354fc494cc6063b471666bb92c079f76396b1872b26";

    const char* const kat_v15_sha512 =
        "864019bd92d89522a2aee6a27e4cf625a5e6c679cfc58114690619d194c93641c50fadffccdc3f01ad8b6dff45b84a71deee5d61c9bf92ef"
        "db8e2fa416510a95e79efbb8680a214089f648458ef6d9e025fcbffd347cf72aff5d9545edde4d14a91e82757cc55c7b116d9c010a00ff09"
        "3d1bb0f72c1ff9f8ee58e0e242603a5ed561ee5c73d5601134a724fe62e268cae363fb2fc2664512eb66e4f04f7fa8376f8cb099056773a0"
        "f05f5ba0c71074ea8a1b333640e40199546665f269b2f077fc04c8bdabdc0abd17cb0aa0aead0c68d333ed445964a17f861aa9782dcbca93"
        "d503bab73420eddb6cbf2c8c8ef02c19d080dc3a5c13de4c9d2f2a69a45dca7a";

    const char* const kat_pss_sha256 =
        "6320458eb3ebed503f128fc56f5c94c19c7216d680ac7cc301af63bd862042360c6aff6f310f977487d608dfb74171ffdcb393dd10298781"
        "9ab0e59f3ef4099ec281280aa818ac11fdcf3443ac119c34b0f2aac08a85c0387280e90102ae041e6950077a4dc8b59402bdc768fcb4c788"
        "ea124d7898d20d10913b27ce9cd7e59f6dbdb73305ba5f13eef39c77fad52c19100dca3ea89d5fcfe7a527b75b10ab64add2d837a6d416e8"
        "9c77a93d470343038722e5754c82431151ab0bf8abb6f1e31fe4f9140d3c6f804a33d12684138f27283bbd41dbe2c3e42518fc2e15627d58"
        "2aa8a2a741083ada69c47de40b616ee1f462a6b5969aeeb8cefed10b3f71fc22";

    const char* const kat_pss_sha512 =
        "87119d2166611d5eb4133b0d41170af732324bcb21332067d4b34704389de7f77940191597534aaa7056b84676ebcf013738a48d61de140a"
        "c8154cf26b10b4184c43623b80e21650352ec326c9ef10c1c5da6b2eca995ee475f1713974a276181a28cb9adf5f7cca4bc6bb42cc915fcb"
        "2a93113802aab53ce4bf2dec45641db10d4da59b5eba45932b90bda72c8830eacfb51eff120bf1e3293d07eb20cbaba52657bf946778024e"
        "572f1a06a1526f61347d5d971fab39b4bdcfa473c81c25b6e0bef001447f15ebd45a0f0e6f102a25a73f787c63bb2278df084705e525cd07"
        "fe1f4cdccd6c3d0901666e86c9372dfba80a8b00ec591afc0e1d3189542099b3";

    const char* const kat_oaep_sha256 =
        "604f78c57803482e481c99e43f9cd6b3d930d5117341a72d1fc712f4871ababfd99f9affa706ea68fbf1b686a885158b7d6105d03981a3ad"
        "b45a1994fdd4cfde8f96e7a54d5eea9e1b7ee7f86a63ec9625a655aeabf87468423c22ddaa5df288d19c846c37724e608ba14fb198ecd0bf"
        "4adba76dab4da803fc319c0b15c366c718dbec1e208eb47cc52e9e8dd2a130174dc4596ed2748061abfbceeadf6dc2e35db93cc068b7a646"
        "4f3b0e175d6193aa450db2aa2d66ffb33dd7c65b968a333e7e0ba68fd63ca906a3a97a87772bff1c4ae0c2dcd6e3091dde52d84778c6fa5b"
        "44654ccd87d46234af923b6eb75173af098ad6fd3db6ef6c43d06b60d19a22a6";

    const char* const kat_oaep_sha1 =
        "0709097edbd166a12b209133d8beab5b67e750dc701a6e8b4e9081be28c72e49a20d7839373ccc42142e508d959b7874041426304887136a"
        "21047051d1f8be41f7a4f2d619c320370b8ded16d996dc7c20e6ee1b7934bfafaffd9093dbf68eb655793e9086d859952659b4bc46ea84f6"
        "cf7594b5c64d706168d1b1f39f6e649612214ad6633cfac2de54546e626481519781732768ba6461a977eef2fdbf3d64d41f23b561a179da"
        "2a3e3cbb50c8d4056e1192897c8f20e26fcfb66e90c115a40bf8cdddebcbb89525aecb8c7990c599d74e67db930b8e22e9cb9c014d97eae8"
        "c098173c0101583e43fd647e3410e151cf33caf5e26b6145f49a5c34c5278d4a";

    std::string text_of(const crypto::error& e) {
        auto m = e.message();
        return std::string(m.data(), m.size());
    }

    bytes_t seq(size_t n, unsigned char start) {
        bytes_t b(n);
        for (size_t i = 0; i < n; ++i) {
            b[i] = static_cast<unsigned char>(start + i);
        }
        return b;
    }

    const unsigned sizes[] = {2048, 3072, 4096};

    // A DER INTEGER of a big-endian magnitude, in its shortest form
    bytes_t der_integer(bytes_t m) {
        while (m.size() > 1 && m[0] == 0) {
            m.erase(m.begin());
        }
        if (m.empty()) {
            m.push_back(0);
        }
        if (m[0] & 0x80) {
            m.insert(m.begin(), 0);
        }
        bytes_t out = {0x02};
        size_t n = m.size();
        if (n < 0x80) {
            out.push_back((unsigned char)n);
        } else if (n < 0x100) {
            out.push_back(0x81);
            out.push_back((unsigned char)n);
        } else {
            out.push_back(0x82);
            out.push_back((unsigned char)(n >> 8));
            out.push_back((unsigned char)n);
        }
        out.insert(out.end(), m.begin(), m.end());
        return out;
    }

    bytes_t der_wrap(unsigned char tag, const bytes_t& body) {
        bytes_t out = {tag};
        size_t n = body.size();
        if (n < 0x80) {
            out.push_back((unsigned char)n);
        } else if (n < 0x100) {
            out.push_back(0x81);
            out.push_back((unsigned char)n);
        } else {
            out.push_back(0x82);
            out.push_back((unsigned char)(n >> 8));
            out.push_back((unsigned char)n);
        }
        out.insert(out.end(), body.begin(), body.end());
        return out;
    }

    bytes_t bn_bytes(const BIGNUM* b) {
        bytes_t out(size_t(BN_num_bytes(b)));
        BN_bn2bin(b, out.data());
        return out;
    }

    // The eight numbers of an RSAPrivateKey: n, e, d, p, q, dP, dQ, qInv
    struct Numbers {
        bytes_t v[8];

        bytes_t der(unsigned char version = 0) const {
            bytes_t body = {0x02, 0x01, version};
            for (const auto& x : v) {
                auto i = der_integer(x);
                body.insert(body.end(), i.begin(), i.end());
            }
            return der_wrap(0x30, body);
        }
    };

    Numbers numbers_of(const Pkey& key) {
        static const char* names[8] = {OSSL_PKEY_PARAM_RSA_N, OSSL_PKEY_PARAM_RSA_E, OSSL_PKEY_PARAM_RSA_D, OSSL_PKEY_PARAM_RSA_FACTOR1,
                                       OSSL_PKEY_PARAM_RSA_FACTOR2, OSSL_PKEY_PARAM_RSA_EXPONENT1, OSSL_PKEY_PARAM_RSA_EXPONENT2,
                                       OSSL_PKEY_PARAM_RSA_COEFFICIENT1};
        Numbers n;
        for (int i = 0; i < 8; ++i) {
            BIGNUM* b = ossl_bn_param(key, names[i]);
            n.v[i] = bn_bytes(b);
            BN_clear_free(b);
        }
        return n;
    }

    // The key of two primes of the bit lengths given, e = 65537, its
    // numbers computed by OpenSSL's BIGNUM (d = e^-1 mod lcm(p - 1, q - 1))
    Numbers numbers_of_primes(int pbits, int qbits) {
        BN_CTX* ctx = BN_CTX_new();
        BIGNUM *p = BN_new(), *q = BN_new(), *e = BN_new(), *n = BN_new(), *d = BN_new(), *p1 = BN_new(), *q1 = BN_new();
        BIGNUM *g = BN_new(), *l = BN_new(), *dp = BN_new(), *dq = BN_new(), *qi = BN_new(), *t = BN_new();
        BN_set_word(e, 65537);
        for (;;) {
            BN_generate_prime_ex(p, pbits, 0, nullptr, nullptr, nullptr);
            BN_generate_prime_ex(q, qbits, 0, nullptr, nullptr, nullptr);
            BN_sub(p1, p, BN_value_one());
            BN_sub(q1, q, BN_value_one());
            BN_gcd(t, p1, e, ctx);
            bool ok = BN_is_one(t);
            BN_gcd(t, q1, e, ctx);
            ok = ok && BN_is_one(t) && BN_cmp(p, q) != 0;
            if (ok) {
                break;
            }
        }
        BN_mul(n, p, q, ctx);
        BN_gcd(g, p1, q1, ctx);
        BN_mul(l, p1, q1, ctx);
        BN_div(l, nullptr, l, g, ctx);
        BN_mod_inverse(d, e, l, ctx);
        BN_mod(dp, d, p1, ctx);
        BN_mod(dq, d, q1, ctx);
        BN_mod_inverse(qi, q, p, ctx);
        Numbers out;
        const BIGNUM* all[8] = {n, e, d, p, q, dp, dq, qi};
        for (int i = 0; i < 8; ++i) {
            out.v[i] = bn_bytes(all[i]);
        }
        for (BIGNUM* b : {p, q, e, n, d, p1, q1, g, l, dp, dq, qi, t}) {
            BN_clear_free(b);
        }
        BN_CTX_free(ctx);
        return out;
    }

    // Where a block of words goes back: counted, and checked to be zero
    struct Probe {
        static inline size_t blocks = 0;
        static inline size_t dirty = 0;

        static void release(void* p, size_t n) noexcept {
            const unsigned char* b = static_cast<const unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) {
                if (b[i] != 0) {
                    ++dirty;
                    break;
                }
            }
            ++blocks;
            ::operator delete(p);
        }
    };

    using ProbeKey = crypto::detail::RsaPrivateKey<Probe>;
}

static_assert(!std::is_copy_constructible_v<SK>);
static_assert(!std::is_copy_assignable_v<SK>);
static_assert(std::is_nothrow_move_constructible_v<SK>);
static_assert(std::is_nothrow_move_assignable_v<SK>);
static_assert(std::is_copy_constructible_v<PK>);

// --- known answers ---------------------------------------------------------------

TEST(Crypto_Rsa, KnownAnswers) {
    bytes_t der = unhex(kat_pkcs8);
    auto key = SK::from_pkcs8_der(view(der));
    ASSERT_TRUE(key.has_value()) << text_of(key.error());
    EXPECT_EQ(key->bits(), 2048u);
    EXPECT_EQ(hex(to_bytes(key->to_pkcs8_der())), kat_pkcs8);
    auto pub = key->public_key();
    bytes_t msg = text("abc");
    struct {
        hash_id id;
        const char* sig;
    } v15[] = {{hash_id::sha1, kat_v15_sha1}, {hash_id::sha256, kat_v15_sha256}, {hash_id::sha384, kat_v15_sha384}, {hash_id::sha512, kat_v15_sha512}};
    for (auto& c : v15) {
        bytes_t d = ossl_digest(c.id, msg);
        EXPECT_EQ(hex(to_bytes(key->sign_digest(c.id, view(d)))), c.sig);
        EXPECT_TRUE(pub.verify_digest(c.id, view(d), view(unhex(c.sig))));
    }
    bytes_t d256 = ossl_digest(hash_id::sha256, msg);
    bytes_t d512 = ossl_digest(hash_id::sha512, msg);
    EXPECT_EQ(hex(to_bytes(Access::sign_pss(*key, hash_id::sha256, view(d256), view(seq(32, 0x00))))), kat_pss_sha256);
    EXPECT_EQ(hex(to_bytes(Access::sign_pss(*key, hash_id::sha512, view(d512), view(seq(64, 0x40))))), kat_pss_sha512);
    EXPECT_TRUE(pub.verify_digest_pss(hash_id::sha256, view(d256), view(unhex(kat_pss_sha256))));
    EXPECT_TRUE(pub.verify_digest_pss(hash_id::sha512, view(d512), view(unhex(kat_pss_sha512))));
    bytes_t secret = text("a secret message");
    bytes_t label = text("label");
    EXPECT_EQ(hex(to_bytes(Access::encrypt_oaep(pub, hash_id::sha256, hash_id::sha256, view(secret), view(label), view(seq(32, 0x80))))), kat_oaep_sha256);
    EXPECT_EQ(hex(to_bytes(Access::encrypt_oaep(pub, hash_id::sha1, hash_id::sha1, view(secret), view(bytes_t{}), view(seq(20, 0x80))))), kat_oaep_sha1);
    auto m1 = key->decrypt_oaep(hash_id::sha256, view(unhex(kat_oaep_sha256)), view(label));
    ASSERT_TRUE(m1.has_value());
    EXPECT_EQ(to_bytes(*m1), secret);
    auto m2 = key->decrypt_oaep(hash_id::sha1, view(unhex(kat_oaep_sha1)));
    ASSERT_TRUE(m2.has_value());
    EXPECT_EQ(to_bytes(*m2), secret);
}

// --- the arithmetic -----------------------------------------------------------------

namespace {
    BIGNUM* to_bn(const std::vector<uint64_t>& w) {
        bytes_t b(8 * w.size());
        bn::to_be(b.data(), b.size(), w.data(), w.size());
        return BN_bin2bn(b.data(), int(b.size()), nullptr);
    }

    std::string words_hex(const uint64_t* w, size_t k) {
        bytes_t b(8 * k);
        bn::to_be(b.data(), b.size(), w, k);
        return hex(b);
    }

    std::string bn_hex(const BIGNUM* b, size_t k) {
        bytes_t out(8 * k);
        BN_bn2binpad(b, out.data(), int(out.size()));
        return hex(out);
    }

    std::vector<uint64_t> random_words(random_source& r, size_t k) {
        std::vector<uint64_t> w(k);
        for (auto& x : w) {
            x = r.g();
        }
        return w;
    }

    // a random number below m (by clearing top bits and subtracting)
    std::vector<uint64_t> below(random_source& r, const std::vector<uint64_t>& m) {
        size_t k = m.size();
        auto a = random_words(r, k);
        size_t bits = bn::bit_length(m.data(), k);
        for (size_t i = 0; i < k; ++i) {
            if (64 * i >= bits) {
                a[i] = 0;
            } else if (bits - 64 * i < 64) {
                a[i] &= (uint64_t(1) << (bits - 64 * i)) - 1;
            }
        }
        std::vector<uint64_t> t(k);
        if (bn::sub(t.data(), a.data(), m.data(), k) == 0) {
            a = t;
        }
        return a;
    }
}

// Montgomery products, squares, powers (secret and public exponents), the
// reductions and the inverse against OpenSSL's BIGNUM, on moduli of 1 to 65
// words: random ones, ones with the top word small, all ones, and edges of
// the operands (0, 1, m - 1)
TEST(Crypto_Rsa, ModularArithmetic) {
    random_source r(11);
    BN_CTX* ctx = BN_CTX_new();
    for (size_t k : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 23, 24, 25, 31, 32, 33, 48, 64, 65}) {
        for (int shape = 0; shape < 3; ++shape) {
            std::vector<uint64_t> m = random_words(r, k);
            if (shape == 1) {
                m[k - 1] &= 0xff;   // the top word small
                m[k - 1] |= 1;
            } else if (shape == 2) {
                for (auto& x : m) {
                    x = ~uint64_t(0);
                }
            }
            m[0] |= 1;
            if (k == 1 && m[0] < 3) {
                m[0] = 3;
            }
            size_t scratch = std::max({bn::pow_scratch(k), bn::inverse_scratch(k), bn::reduce_scratch(k)}) + 8 * k;
            std::vector<uint64_t> rr(k), rrr(k), s(scratch);
            uint64_t m0 = bn::mont_m0inv(m[0]);
            bn::mont_constants(rr.data(), rrr.data(), m.data(), k, m0, s.data());
            bn::Modulus mod{m.data(), rr.data(), rrr.data(), m0, k};
            BIGNUM* bm = to_bn(m);
            for (int round = 0; round < (k <= 17 ? 6 : 3); ++round) {
                auto a = below(r, m);
                auto b = below(r, m);
                if (round == 0) {
                    std::fill(a.begin(), a.end(), 0);
                } else if (round == 1) {
                    std::fill(a.begin(), a.end(), 0);
                    a[0] = 1;
                } else if (round == 2) {
                    a = m;
                    a[0] -= 1;
                }
                BIGNUM *ba = to_bn(a), *bb = to_bn(b), *want = BN_new();
                std::vector<uint64_t> am(k), bm2(k), x(k), e = random_words(r, k);
                bn::to_mont(am.data(), a.data(), mod, s.data());
                bn::to_mont(bm2.data(), b.data(), mod, s.data());
                // product and square
                bn::mont_mul(x.data(), am.data(), bm2.data(), mod, s.data());
                bn::from_mont(x.data(), x.data(), mod, s.data());
                BN_mod_mul(want, ba, bb, bm, ctx);
                ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "mul k=" << k;
                bn::mont_sqr(x.data(), am.data(), mod, s.data());
                bn::from_mont(x.data(), x.data(), mod, s.data());
                BN_mod_mul(want, ba, ba, bm, ctx);
                ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "sqr k=" << k;
                // a power by a secret exponent of all the words, and by a
                // public one
                BIGNUM* be = to_bn(e);
                bn::mont_pow(x.data(), am.data(), e.data(), 64 * k, mod, s.data());
                bn::from_mont(x.data(), x.data(), mod, s.data());
                BN_mod_exp(want, ba, be, bm, ctx);
                ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "pow k=" << k;
                uint64_t pe = r.g() | 1;
                BN_set_word(be, pe);
                bn::mont_pow_public(x.data(), am.data(), pe, mod, s.data());
                bn::from_mont(x.data(), x.data(), mod, s.data());
                BN_mod_exp(want, ba, be, bm, ctx);
                ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "pow_public k=" << k;
                BN_free(be);
                // x of 2k words below m R into the form, and back
                std::vector<uint64_t> wide = random_words(r, 2 * k);
                // below m R: the top k words below m
                auto hi = below(r, m);
                for (size_t i = 0; i < k; ++i) {
                    wide[k + i] = hi[i];
                }
                BIGNUM* bw = to_bn(wide);
                bn::reduce_to_mont(x.data(), wide.data(), 2 * k, mod, s.data());
                bn::from_mont(x.data(), x.data(), mod, s.data());
                BN_nnmod(want, bw, bm, ctx);
                ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "reduce_to_mont k=" << k;
                // the plain reduction, also modulo an even number
                std::vector<uint64_t> even = m;
                even[0] ^= 1;
                if (bn::zero_mask(even.data(), k) == 0) {
                    BIGNUM* bev = to_bn(even);
                    bn::reduce(x.data(), wide.data(), 2 * k, even.data(), k, s.data());
                    BN_nnmod(want, bw, bev, ctx);
                    ASSERT_EQ(words_hex(x.data(), k), bn_hex(want, k)) << "reduce k=" << k;
                    BN_free(bev);
                }
                BN_free(bw);
                // the inverse
                if (round != 0) {
                    bool ok = bn::inverse_vartime(x.data(), a.data(), m.data(), k, s.data());
                    BIGNUM* inv = BN_mod_inverse(nullptr, ba, bm, ctx);
                    ASSERT_EQ(ok, inv != nullptr) << "inverse k=" << k;
                    if (inv) {
                        ASSERT_EQ(words_hex(x.data(), k), bn_hex(inv, k)) << "inverse k=" << k;
                    }
                    BN_free(inv);
                    ERR_clear_error();
                }
                BN_free(ba);
                BN_free(bb);
                BN_free(want);
            }
            BN_free(bm);
        }
    }
    // an inverse that does not exist: a shares a factor with m
    {
        std::vector<uint64_t> m = {15, 0}, a = {10, 0}, x(2), s(bn::inverse_scratch(2));
        EXPECT_FALSE(bn::inverse_vartime(x.data(), a.data(), m.data(), 2, s.data()));
        a = {7, 0};
        EXPECT_TRUE(bn::inverse_vartime(x.data(), a.data(), m.data(), 2, s.data()));
        EXPECT_EQ(x[0], 13u);   // 7 * 13 = 91 = 6 * 15 + 1
    }
    BN_CTX_free(ctx);
}

// --- against OpenSSL ---------------------------------------------------------------------

// PKCS #1 v1.5 is deterministic: our signature is OpenSSL's, byte for byte,
// for every hash, and each side verifies the other's
// The key generation's constant-time gcd and division, and the small
// residue, against OpenSSL's BIGNUM: even numbers with common factors of
// 2 (as p - 1 and q - 1 have), odd ones, one a multiple of the other
TEST(Crypto_Rsa, KeygenArithmetic) {
    namespace kg = crypto::detail::rsa_keygen;
    random_source r(31);
    BN_CTX* ctx = BN_CTX_new();
    for (size_t k : {size_t(1), size_t(2), size_t(5), size_t(16)}) {
        for (int round = 0; round < 60; ++round) {
            std::vector<uint64_t> a = random_words(r, k), b = random_words(r, k);
            // shared factors: a common power of two, sometimes a common odd factor
            unsigned twos = unsigned(r.below(6));
            a[0] = (a[0] >> twos) << twos;
            b[0] = (b[0] >> twos) << twos;
            if (round % 7 == 0) {
                b = a;                                    // gcd = a
            }
            if (round % 11 == 0) {
                b[0] |= 1;                                // odd and even
            }
            a[0] |= a[0] == 0 && k == 1 ? 1 : 0;
            std::vector<uint64_t> g(k), scratch(2 * k);
            kg::gcd_consttime(g.data(), a.data(), b.data(), k, scratch.data());
            BIGNUM *ba = to_bn(a), *bb = to_bn(b), *want = BN_new();
            BN_gcd(want, ba, bb, ctx);
            EXPECT_EQ(words_hex(g.data(), k), bn_hex(want, k)) << "k " << k << " round " << round;
            // a b / g, and the residue mod e
            std::vector<uint64_t> prod(2 * k), q(2 * k), s2(2 * (k + 1));
            bn::mul(prod.data(), a.data(), k, b.data(), k);
            kg::div_consttime(q.data(), prod.data(), 2 * k, g.data(), k, s2.data());
            BIGNUM *bp = to_bn(prod), *bq = BN_new(), *rem = BN_new();
            BN_div(bq, rem, bp, want, ctx);
            EXPECT_EQ(words_hex(q.data(), 2 * k), bn_hex(bq, 2 * k));
            kg::SmallModulus e(65537);
            EXPECT_EQ(kg::mod_small(q.data(), 2 * k, e), BN_mod_word(bq, 65537));
            BN_free(ba);
            BN_free(bb);
            BN_free(want);
            BN_free(bp);
            BN_free(bq);
            BN_free(rem);
        }
    }
    BN_CTX_free(ctx);
}

TEST(Crypto_Rsa, Pkcs1v15AgainstOpenSsl) {
    random_source r(21);
    for (unsigned bits : sizes) {
        const TestKey& tk = test_key(bits);
        auto key = our_key(tk);
        auto pub = key.public_key();
        EXPECT_EQ(key.bits(), bits);
        for (hash_id id : all_hashes) {
            for (int i = 0; i < 3; ++i) {
                bytes_t d = ossl_digest(id, r.bytes(r.below(200)));
                bytes_t ours = to_bytes(key.sign_digest(id, view(d)));
                bytes_t theirs = ossl_sign(tk.ossl, id, d, false);
                ASSERT_EQ(ours.size(), bits / 8);
                EXPECT_EQ(hex(ours), hex(theirs)) << bits << " " << int(id);
                EXPECT_TRUE(ossl_verify(tk.ossl, id, d, ours, false));
                EXPECT_TRUE(pub.verify_digest(id, view(d), view(theirs)));
            }
        }
    }
}

// PSS: our signatures (salt as long as the digest) verified by OpenSSL, its
// signatures with every salt length verified by us (the length read from
// the signature)
TEST(Crypto_Rsa, PssAgainstOpenSsl) {
    random_source r(22);
    for (unsigned bits : sizes) {
        const TestKey& tk = test_key(bits);
        auto key = our_key(tk);
        auto pub = key.public_key();
        for (hash_id id : all_hashes) {
            bytes_t d = ossl_digest(id, r.bytes(r.below(200)));
            bytes_t ours = to_bytes(key.sign_digest_pss(id, view(d)));
            ASSERT_EQ(ours.size(), bits / 8);
            EXPECT_TRUE(ossl_verify(tk.ossl, id, d, ours, true, RSA_PSS_SALTLEN_DIGEST)) << bits << " " << int(id);
            EXPECT_TRUE(ossl_verify(tk.ossl, id, d, ours, true, RSA_PSS_SALTLEN_AUTO));
            EXPECT_TRUE(pub.verify_digest_pss(id, view(d), view(ours)));
            // a new salt each time
            EXPECT_NE(hex(ours), hex(to_bytes(key.sign_digest_pss(id, view(d)))));
            for (int salt : {RSA_PSS_SALTLEN_DIGEST, RSA_PSS_SALTLEN_MAX, 0, 1, 7}) {
                bytes_t theirs = ossl_sign(tk.ossl, id, d, true, salt);
                ASSERT_FALSE(theirs.empty());
                EXPECT_TRUE(pub.verify_digest_pss(id, view(d), view(theirs))) << bits << " " << int(id) << " salt " << salt;
            }
        }
    }
}

// PSS with the salt's length fixed (what a certificate's parameters name):
// OpenSSL's signatures with salts of 0, 20, 32 and 64 bytes, each taken
// at the length it has and at no other, as OpenSSL takes them; the form
// without a length takes them all
TEST(Crypto_Rsa, PssWithAFixedSaltLength) {
    random_source r(29);
    const TestKey& tk = test_key(2048);
    auto pub = our_key(tk).public_key();
    for (hash_id id : {hash_id::sha256, hash_id::sha512}) {
        bytes_t d = ossl_digest(id, r.bytes(100));
        for (int salt : {0, 20, 32, 64}) {
            bytes_t sig = ossl_sign(tk.ossl, id, d, true, salt);
            ASSERT_FALSE(sig.empty());
            EXPECT_TRUE(pub.verify_digest_pss(id, view(d), view(sig))) << int(id) << " salt " << salt;
            for (int want : {0, 20, 32, 64}) {
                EXPECT_EQ(pub.verify_digest_pss(id, view(d), view(sig), size_t(want)), want == salt) << int(id) << " salt " << salt << " wanted " << want;
                EXPECT_EQ(ossl_verify(tk.ossl, id, d, sig, true, want), want == salt) << "OpenSSL: " << int(id) << " salt " << salt << " wanted " << want;
            }
            // a length the key has no room for, a digest of another length: false
            EXPECT_FALSE(pub.verify_digest_pss(id, view(d), view(sig), size_t(1) << 40));
            EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha384, view(d), view(sig), size_t(salt)));
            bytes_t flipped = sig;
            flipped[r.below(flipped.size())] ^= 0x10;
            EXPECT_FALSE(pub.verify_digest_pss(id, view(d), view(flipped), size_t(salt)));
        }
    }
}

// OAEP both ways: every length up to the longest, labels empty, short and
// long, MGF1 over the label's hash and over another one
TEST(Crypto_Rsa, OaepAgainstOpenSsl) {
    random_source r(23);
    struct {
        hash_id id, mgf;
    } pairs[] = {{hash_id::sha256, hash_id::sha256}, {hash_id::sha1, hash_id::sha1}, {hash_id::sha256, hash_id::sha1},
                 {hash_id::sha384, hash_id::sha384}, {hash_id::sha512, hash_id::sha256}, {hash_id::sha3_256, hash_id::sha3_256},
                 {hash_id::sha224, hash_id::sha512_256}};
    for (unsigned bits : sizes) {
        const TestKey& tk = test_key(bits);
        auto key = our_key(tk);
        auto pub = key.public_key();
        for (auto p : pairs) {
            size_t max = pub.max_oaep_message_size(p.id);
            EXPECT_EQ(max, bits / 8 - 2 * crypto::digest_size(p.id) - 2);
            for (size_t len : {size_t(0), size_t(1), size_t(17), max - 1, max}) {
                bytes_t msg = r.bytes(len);
                for (int l = 0; l < 3; ++l) {
                    bytes_t label = l == 0 ? bytes_t{} : r.bytes(l == 1 ? 5 : 300);
                    bytes_t ours = p.id == p.mgf ? (l == 0 ? to_bytes(pub.encrypt_oaep(p.id, view(msg))) : to_bytes(pub.encrypt_oaep(p.id, view(msg), view(label))))
                                                 : to_bytes(pub.encrypt_oaep(p.id, p.mgf, view(msg), view(label)));
                    ASSERT_EQ(ours.size(), bits / 8);
                    auto back = ossl_decrypt_oaep(tk.ossl, p.id, p.mgf, ours, label);
                    ASSERT_TRUE(back.has_value()) << bits << " " << int(p.id) << " len " << len;
                    EXPECT_EQ(*back, msg);
                    bytes_t theirs = ossl_encrypt_oaep(tk.ossl, p.id, p.mgf, msg, label);
                    ASSERT_EQ(theirs.size(), bits / 8);
                    auto mine = p.id == p.mgf && l == 0 ? key.decrypt_oaep(p.id, view(theirs)) : key.decrypt_oaep(p.id, p.mgf, view(theirs), view(label));
                    ASSERT_TRUE(mine.has_value()) << bits << " " << int(p.id) << " len " << len;
                    EXPECT_EQ(to_bytes(*mine), msg);
                    // into the program's buffer: the same bytes, the rest untouched
                    bytes_t buffer(max + 3, 0xee);
                    sgcl::slice<byte> out(reinterpret_cast<byte*>(buffer.data()), buffer.size());
                    auto n = p.id == p.mgf && l == 0 ? key.decrypt_oaep_to(out, p.id, view(theirs)) : key.decrypt_oaep_to(out, p.id, p.mgf, view(theirs), view(label));
                    ASSERT_TRUE(n.has_value());
                    ASSERT_EQ(*n, len);
                    EXPECT_EQ(bytes_t(buffer.begin(), buffer.begin() + len), msg);
                    EXPECT_EQ(buffer[len], 0xee);
                }
            }
            // one byte more is the program's mistake
            bytes_t too_long(max + 1);
            EXPECT_THROW((void)pub.encrypt_oaep(p.id, p.mgf, view(too_long), view(bytes_t{})), std::invalid_argument);
        }
    }
}

// --- refusals ---------------------------------------------------------------------------------

// What must not verify: a bit flipped anywhere, a signature cut or
// lengthened, zero, n and above, another digest, another hash of the same
// length, the other scheme's signature; and encodings a lax PKCS #1 v1.5
// verifier takes (DigestInfo without the NULL, bytes after it, a short
// padding), made with OpenSSL's raw private operation
TEST(Crypto_Rsa, SignaturesRefused) {
    random_source r(24);
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    auto pub = key.public_key();
    size_t k = pub.size();
    bytes_t d = ossl_digest(hash_id::sha256, text("message"));
    bytes_t v15 = to_bytes(key.sign_digest(hash_id::sha256, view(d)));
    bytes_t pss = to_bytes(key.sign_digest_pss(hash_id::sha256, view(d)));
    ASSERT_TRUE(pub.verify_digest(hash_id::sha256, view(d), view(v15)));
    ASSERT_TRUE(pub.verify_digest_pss(hash_id::sha256, view(d), view(pss)));
    for (int i = 0; i < 64; ++i) {
        size_t bit = r.below(8 * k);
        bytes_t a = v15, b = pss;
        a[bit / 8] ^= (unsigned char)(1 << (bit % 8));
        b[bit / 8] ^= (unsigned char)(1 << (bit % 8));
        EXPECT_FALSE(pub.verify_digest(hash_id::sha256, view(d), view(a))) << bit;
        EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha256, view(d), view(b))) << bit;
        bytes_t e = d;
        e[bit % e.size()] ^= 1;
        EXPECT_FALSE(pub.verify_digest(hash_id::sha256, view(e), view(v15)));
        EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha256, view(e), view(pss)));
    }
    for (const bytes_t* s : {&v15, &pss}) {
        bytes_t cut(s->begin(), s->end() - 1);
        bytes_t longer = *s;
        longer.push_back(0);
        bytes_t front = *s;
        front.insert(front.begin(), 0);
        for (const bytes_t& x : {cut, longer, front, bytes_t{}, bytes_t(k, 0), bytes_t(k, 0xff), to_bytes(pub.modulus())}) {
            EXPECT_FALSE(pub.verify_digest(hash_id::sha256, view(d), view(x)));
            EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha256, view(d), view(x)));
        }
    }
    // the other scheme, another hash of the digest's length
    EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha256, view(d), view(v15)));
    EXPECT_FALSE(pub.verify_digest(hash_id::sha256, view(d), view(pss)));
    EXPECT_FALSE(pub.verify_digest(hash_id::sha3_256, view(d), view(v15)));
    EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha3_256, view(d), view(pss)));
    EXPECT_FALSE(pub.verify_digest(hash_id::sha512_256, view(d), view(v15)));
    // a digest of another length than the hash's: a verification says
    // false (the hash may come with a certificate), a signature throws
    EXPECT_FALSE(pub.verify_digest(hash_id::sha384, view(d), view(v15)));
    EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha1, view(d), view(pss)));
    EXPECT_THROW((void)key.sign_digest(hash_id::sha512, view(d)), std::invalid_argument);
    EXPECT_THROW((void)key.sign_digest_pss(hash_id::sha224, view(d)), std::invalid_argument);
    EXPECT_THROW((void)key.sign_digest(static_cast<hash_id>(99), view(d)), std::invalid_argument);
    // lax encodings: EM = 00 01 FF.. 00 || T', signed with the raw operation
    static const unsigned char prefix_null[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    static const unsigned char prefix_no_null[] = {0x30, 0x2f, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x04, 0x20};
    auto em_of = [&](const bytes_t& t, size_t ps) {
        bytes_t em = {0x00, 0x01};
        em.insert(em.end(), ps, 0xff);
        em.push_back(0x00);
        em.insert(em.end(), t.begin(), t.end());
        return em;
    };
    bytes_t t_ok(prefix_null, prefix_null + sizeof prefix_null);
    t_ok.insert(t_ok.end(), d.begin(), d.end());
    // the right one, through the raw operation: accepted (the harness works)
    bytes_t good = ossl_raw_private(tk.ossl, em_of(t_ok, k - 3 - t_ok.size()));
    EXPECT_EQ(hex(good), hex(v15));
    bytes_t t_no_null(prefix_no_null, prefix_no_null + sizeof prefix_no_null);
    t_no_null.insert(t_no_null.end(), d.begin(), d.end());
    bytes_t t_trailing = t_ok;
    t_trailing.push_back(0x00);
    const bytes_t lax[] = {
        em_of(t_no_null, k - 3 - t_no_null.size()),        // no NULL parameters
        em_of(t_trailing, k - 3 - t_trailing.size()),      // a byte after the digest
        [&] {                                              // the digest, then garbage in the padding's place
            bytes_t em = em_of(t_ok, k - 3 - t_ok.size());
            em[5] = 0x00;
            return em;
        }(),
        [&] {                                              // block type 02
            bytes_t em = em_of(t_ok, k - 3 - t_ok.size());
            em[1] = 0x02;
            return em;
        }(),
    };
    for (const auto& em : lax) {
        ASSERT_EQ(em.size(), k);
        bytes_t sig = ossl_raw_private(tk.ossl, em);
        ASSERT_EQ(sig.size(), k);
        EXPECT_FALSE(pub.verify_digest(hash_id::sha256, view(d), view(sig)));
    }
    // PSS with the bit above emBits set (8 emLen - emBits = 1 for a
    // 2048-bit key): the rest of the encoding right, so that only the
    // check of that bit refuses it; below n, signed with the raw operation
    bytes_t n = to_bytes(pub.modulus());
    int made = 0;
    for (int i = 0; i < 200 && made < 3; ++i) {
        bytes_t em(k);
        bytes_t salt = r.bytes(32);
        ASSERT_TRUE(crypto::detail::rsa_pad::pss_encode(hash_id::sha256, d.data(), 32, salt.data(), 32, em.data(), 8 * k - 1));
        bytes_t sig = ossl_raw_private(tk.ossl, em);
        ASSERT_TRUE(pub.verify_digest_pss(hash_id::sha256, view(d), view(sig)));
        em[0] |= 0x80;
        if (em >= n) {
            continue;
        }
        sig = ossl_raw_private(tk.ossl, em);
        ASSERT_EQ(sig.size(), k);
        EXPECT_FALSE(pub.verify_digest_pss(hash_id::sha256, view(d), view(sig)));
        EXPECT_FALSE(ossl_verify(tk.ossl, hash_id::sha256, d, sig, true));
        ++made;
    }
    EXPECT_EQ(made, 3);
}

// Every failed decryption is one error, the same object whatever failed:
// a bit of the ciphertext, a length, another label, hash or MGF1 hash, a
// ciphertext not below n, an encoding whose first byte is not 0, whose
// label hash or separator is wrong (made with the raw public operation)
TEST(Crypto_Rsa, OaepFailuresAreOneError) {
    random_source r(25);
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    auto pub = key.public_key();
    size_t k = pub.size();
    bytes_t msg = text("attack at dawn");
    bytes_t label = text("label");
    bytes_t ct = to_bytes(pub.encrypt_oaep(hash_id::sha256, view(msg), view(label)));
    auto ok = key.decrypt_oaep(hash_id::sha256, view(ct), view(label));
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(to_bytes(*ok), msg);
    // decrypt_oaep_to: a buffer shorter than the longest message is the
    // program's mistake, decided from the key alone; a failure is the same
    // one error and leaves the buffer as it was
    size_t max = pub.max_oaep_message_size(hash_id::sha256);
    bytes_t small(max - 1), buffer(max, 0xee);
    EXPECT_THROW((void)key.decrypt_oaep_to(sgcl::slice<byte>(reinterpret_cast<byte*>(small.data()), small.size()), hash_id::sha256, view(ct), view(label)), std::length_error);
    bytes_t bad = ct;
    bad[k / 2] ^= 1;
    auto refused = key.decrypt_oaep_to(sgcl::slice<byte>(reinterpret_cast<byte*>(buffer.data()), buffer.size()), hash_id::sha256, view(bad), view(label));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), crypto::errc::authentication);
    EXPECT_EQ(buffer, bytes_t(max, 0xee));
    // what each error was (an error holds a managed string: not in a
    // std::vector)
    struct Seen {
        crypto::errc code;
        uint64_t offset;
        std::string text;
    };
    std::vector<Seen> errors;
    auto expect_error = [&](const auto& e, const char* what) {
        ASSERT_FALSE(e.has_value()) << what;
        errors.push_back(Seen{e.error().code(), e.error().offset(), text_of(e.error())});
    };
    for (int i = 0; i < 32; ++i) {
        bytes_t bad = ct;
        size_t bit = r.below(8 * k);
        bad[bit / 8] ^= (unsigned char)(1 << (bit % 8));
        auto e = key.decrypt_oaep(hash_id::sha256, view(bad), view(label));
        expect_error(e, "a bit");
    }
    bytes_t cut(ct.begin(), ct.end() - 1);
    bytes_t longer = ct;
    longer.push_back(0);
    expect_error(key.decrypt_oaep(hash_id::sha256, view(cut), view(label)), "cut");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(longer), view(label)), "longer");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(bytes_t{}), view(label)), "empty");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ct), view(text("lab3l"))), "label");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ct)), "no label");
    expect_error(key.decrypt_oaep(hash_id::sha3_256, hash_id::sha256, view(ct), view(label)), "hash");
    expect_error(key.decrypt_oaep(hash_id::sha256, hash_id::sha1, view(ct), view(label)), "mgf");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(to_bytes(pub.modulus())), view(label)), "n");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(bytes_t(k, 0xff)), view(label)), "above n");
    // encodings built by hand: EM = Y || maskedSeed || maskedDB
    auto encode = [&](unsigned char y, bool bad_hash, int separator, unsigned char in_padding) {
        size_t h = 32;
        bytes_t db(k - h - 1, 0);
        bytes_t lh = ossl_digest(hash_id::sha256, label);
        if (bad_hash) {
            lh[3] ^= 1;
        }
        std::copy(lh.begin(), lh.end(), db.begin());
        size_t at = db.size() - msg.size() - 1;
        db[at] = (unsigned char)separator;
        db[h + 7] = in_padding;   // a byte of PS
        std::copy(msg.begin(), msg.end(), db.begin() + at + 1);
        bytes_t seed = r.bytes(h);
        auto mgf = [&](const bytes_t& s, size_t n) {
            bytes_t out;
            for (uint32_t c = 0; out.size() < n; ++c) {
                bytes_t in = s;
                in.push_back((unsigned char)(c >> 24));
                in.push_back((unsigned char)(c >> 16));
                in.push_back((unsigned char)(c >> 8));
                in.push_back((unsigned char)c);
                bytes_t hh = ossl_digest(hash_id::sha256, in);
                out.insert(out.end(), hh.begin(), hh.end());
            }
            out.resize(n);
            return out;
        };
        bytes_t dbmask = mgf(seed, db.size());
        for (size_t i = 0; i < db.size(); ++i) {
            db[i] ^= dbmask[i];
        }
        bytes_t smask = mgf(db, h);
        for (size_t i = 0; i < h; ++i) {
            seed[i] ^= smask[i];
        }
        bytes_t em = {y};
        em.insert(em.end(), seed.begin(), seed.end());
        em.insert(em.end(), db.begin(), db.end());
        return em;
    };
    // the harness itself: a right encoding decrypts
    {
        bytes_t c = ossl_raw_public(tk.ossl, encode(0x00, false, 0x01, 0));
        auto m = key.decrypt_oaep(hash_id::sha256, view(c), view(label));
        ASSERT_TRUE(m.has_value());
        EXPECT_EQ(to_bytes(*m), msg);
    }
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x01, false, 0x01, 0))), view(label)), "Y = 1");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x80, false, 0x01, 0))), view(label)), "Y = 80");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x00, true, 0x01, 0))), view(label)), "lHash");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x00, false, 0x02, 0))), view(label)), "separator 02");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x00, false, 0x00, 0))), view(label)), "no separator");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x00, false, 0x01, 0x05))), view(label)), "05 in the padding");
    expect_error(key.decrypt_oaep(hash_id::sha256, view(ossl_raw_public(tk.ossl, encode(0x00, false, 0x01, 0xff))), view(label)), "ff in the padding");
    ASSERT_FALSE(errors.empty());
    for (const auto& e : errors) {
        EXPECT_EQ(e.code, crypto::errc::authentication);
        EXPECT_EQ(e.offset, 0u);
        EXPECT_EQ(e.text, "sgcl::crypto::rsa: decryption error");
    }
}

// --- encodings --------------------------------------------------------------------------------

// PKCS #1 (private and public), PKCS #8 and SPKI: what we write is what
// OpenSSL writes, byte for byte, for its keys (1024 bits too, which is read
// though never made); what it writes we read
TEST(Crypto_Rsa, DerAgainstOpenSsl) {
    for (unsigned bits : {1024u, 2048u, 3072u, 4096u}) {
        const TestKey& tk = test_key(bits);
        auto key = our_key(tk);
        auto pub = key.public_key();
        EXPECT_EQ(hex(to_bytes(key.to_pkcs8_der())), hex(tk.pkcs8));
        EXPECT_EQ(hex(to_bytes(key.to_pkcs1_der())), hex(ossl_pkcs1_private(tk.ossl)));
        EXPECT_EQ(hex(to_bytes(pub.to_pkix_der())), hex(ossl_spki(tk.ossl)));
        EXPECT_EQ(hex(to_bytes(pub.to_pkcs1_der())), hex(ossl_pkcs1_public(tk.ossl)));
        auto k1 = SK::from_pkcs1_der(view(ossl_pkcs1_private(tk.ossl)));
        ASSERT_TRUE(k1.has_value()) << text_of(k1.error());
        EXPECT_EQ(hex(to_bytes(k1->to_pkcs8_der())), hex(tk.pkcs8));
        auto p1 = PK::from_pkix_der(view(ossl_spki(tk.ossl)));
        auto p2 = PK::from_pkcs1_der(view(ossl_pkcs1_public(tk.ossl)));
        ASSERT_TRUE(p1.has_value());
        ASSERT_TRUE(p2.has_value());
        EXPECT_TRUE(*p1 == pub);
        EXPECT_TRUE(*p2 == pub);
        Numbers n = numbers_of(tk.ossl);
        EXPECT_EQ(hex(to_bytes(pub.modulus())), hex(n.v[0]));
        EXPECT_EQ(pub.exponent(), 65537u);
        EXPECT_EQ(pub.bits(), bits);
        EXPECT_EQ(pub.size(), bits / 8);
        auto p3 = PK::from_modulus(view(n.v[0]), 65537);
        ASSERT_TRUE(p3.has_value());
        EXPECT_TRUE(*p3 == pub);
        bytes_t padded = n.v[0];
        padded.insert(padded.begin(), 3, 0);
        auto p4 = PK::from_modulus(view(padded), 65537);
        ASSERT_TRUE(p4.has_value());
        EXPECT_TRUE(*p4 == pub);
        // a public key is a value: copied, compared
        PK copy = pub;
        EXPECT_TRUE(copy == pub);
    }
    EXPECT_FALSE(our_public(test_key(2048)) == our_public(test_key(3072)));
}

// Keys that are refused, and why: every cut of a PKCS #8 key and of an SPKI,
// data after them, a multi-prime version, an RSASSA-PSS key, parameters that
// are not NULL, a modulus below 1024 bits or even, an exponent even, 1 or
// above 2^31 - 1, numbers that do not agree (qInv, dP, dQ, d, n, p and q
// swapped), a negative number
TEST(Crypto_Rsa, KeysRefused) {
    const TestKey& tk = test_key(2048);
    bytes_t p8 = tk.pkcs8;
    bytes_t spki = ossl_spki(tk.ossl);
    for (size_t n = 0; n < p8.size(); n += (n < 64 ? 1 : 37)) {
        bytes_t cut(p8.begin(), p8.begin() + n);
        auto k = SK::from_pkcs8_der(view(cut));
        ASSERT_FALSE(k.has_value()) << n;
        EXPECT_EQ(k.error().code(), crypto::errc::malformed) << n;
    }
    for (size_t n = 0; n < spki.size(); ++n) {
        bytes_t cut(spki.begin(), spki.begin() + n);
        EXPECT_FALSE(PK::from_pkix_der(view(cut)).has_value()) << n;
    }
    bytes_t after = p8;
    after.push_back(0);
    EXPECT_EQ(error_of(SK::from_pkcs8_der(view(after))).code(), crypto::errc::malformed);
    after = spki;
    after.push_back(0);
    EXPECT_EQ(error_of(PK::from_pkix_der(view(after))).code(), crypto::errc::malformed);
    // an element after a PKCS#8 key's fields, inside its SEQUENCE
    {
        bytes_t alg = {0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};
        bytes_t body = {0x02, 0x01, 0x00};
        body.insert(body.end(), alg.begin(), alg.end());
        auto inner = der_wrap(0x04, ossl_pkcs1_private(tk.ossl));
        body.insert(body.end(), inner.begin(), inner.end());
        EXPECT_EQ(hex(der_wrap(0x30, body)), hex(p8));
        body.push_back(0x05);
        body.push_back(0x00);
        auto k = SK::from_pkcs8_der(view(der_wrap(0x30, body)));
        ASSERT_FALSE(k.has_value());
        EXPECT_EQ(k.error().code(), crypto::errc::malformed);
    }
    // the numbers, re-encoded
    Numbers good = numbers_of(tk.ossl);
    ASSERT_TRUE(SK::from_pkcs1_der(view(good.der())).has_value());
    EXPECT_EQ(hex(good.der()), hex(ossl_pkcs1_private(tk.ossl)));
    auto code_of = [](const bytes_t& der) {
        auto k = SK::from_pkcs1_der(view(der));
        return k ? crypto::errc{} : k.error().code();
    };
    EXPECT_EQ(code_of(good.der(1)), crypto::errc::unsupported);   // multi-prime
    EXPECT_EQ(code_of(good.der(2)), crypto::errc::malformed);
    auto changed = [&](int i, auto f) {
        Numbers n = good;
        f(n.v[i]);
        return code_of(n.der());
    };
    auto plus2 = [](bytes_t& v) {
        for (size_t i = v.size(); i-- > 0;) {
            unsigned s = v[i] + (i == v.size() - 1 ? 2 : 0);
            v[i] = (unsigned char)s;
            if (s < 256) {
                break;
            }
        }
    };
    for (int i : {0, 2, 3, 4, 5, 6, 7}) {
        EXPECT_EQ(changed(i, plus2), crypto::errc::invalid_key) << i;
    }
    {
        Numbers n = good;
        std::swap(n.v[3], n.v[4]);   // p and q swapped: qInv no longer theirs
        EXPECT_EQ(code_of(n.der()), crypto::errc::invalid_key);
        std::swap(n.v[5], n.v[6]);
        EXPECT_EQ(code_of(n.der()), crypto::errc::invalid_key);
    }
    EXPECT_EQ(changed(1, [](bytes_t& v) { v = {0x01, 0x00, 0x03}; }), crypto::errc::invalid_key);   // e = 65539: dP no longer its inverse
    EXPECT_EQ(changed(1, [](bytes_t& v) { v = {0x01, 0x00, 0x00}; }), crypto::errc::invalid_key);   // even
    EXPECT_EQ(changed(1, [](bytes_t& v) { v = {0x01}; }), crypto::errc::invalid_key);
    EXPECT_EQ(changed(1, [](bytes_t& v) { v = {0x01, 0x00, 0x00, 0x00, 0x01}; }), crypto::errc::unsupported);   // 2^32 + 1
    EXPECT_EQ(changed(3, [](bytes_t& v) { v = {0x01}; }), crypto::errc::invalid_key);   // p = 1
    EXPECT_EQ(changed(4, [](bytes_t& v) { v = {0x02}; }), crypto::errc::invalid_key);   // q = 2
    {
        // a negative number
        bytes_t der = good.der();
        auto i = der_integer(good.v[2]);
        bytes_t neg = {0x02, 0x01, 0xff};
        Numbers n = good;
        bytes_t body = {0x02, 0x01, 0x00};
        for (int j = 0; j < 8; ++j) {
            auto x = j == 2 ? neg : der_integer(n.v[j]);
            body.insert(body.end(), x.begin(), x.end());
        }
        EXPECT_EQ(code_of(der_wrap(0x30, body)), crypto::errc::malformed);
    }
    // a small key, an even modulus, exponents out of range
    const TestKey& small = test_key(512);
    auto s = SK::from_pkcs8_der(view(small.pkcs8));
    ASSERT_FALSE(s.has_value());
    EXPECT_EQ(s.error().code(), crypto::errc::unsupported);
    EXPECT_EQ(error_of(PK::from_pkix_der(view(ossl_spki(small.ossl)))).code(), crypto::errc::unsupported);
    bytes_t n = good.v[0];
    EXPECT_TRUE(PK::from_modulus(view(n), 3).has_value());
    EXPECT_TRUE(PK::from_modulus(view(n), (uint64_t(1) << 31) - 1).has_value());
    EXPECT_EQ(error_of(PK::from_modulus(view(n), 1)).code(), crypto::errc::invalid_key);
    EXPECT_EQ(error_of(PK::from_modulus(view(n), 65536)).code(), crypto::errc::invalid_key);
    EXPECT_EQ(error_of(PK::from_modulus(view(n), uint64_t(1) << 31 | 1)).code(), crypto::errc::unsupported);
    bytes_t even = n;
    even.back() ^= 1;
    EXPECT_EQ(error_of(PK::from_modulus(view(even), 65537)).code(), crypto::errc::invalid_key);
    EXPECT_EQ(error_of(PK::from_modulus(view(bytes_t{}), 65537)).code(), crypto::errc::unsupported);
    bytes_t big(2049, 0xff);
    EXPECT_EQ(error_of(PK::from_modulus(view(big), 65537)).code(), crypto::errc::unsupported);
    // RSASSA-PSS keys (OID 1.2.840.113549.1.1.10) are not read
    Pkey pss(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA-PSS", size_t(2048)));
    ASSERT_TRUE(pss);
    EXPECT_EQ(error_of(SK::from_pkcs8_der(view(ossl_pkcs8(pss)))).code(), crypto::errc::unsupported);
    EXPECT_EQ(error_of(PK::from_pkix_der(view(ossl_spki(pss)))).code(), crypto::errc::unsupported);
    // the AlgorithmIdentifier without its NULL
    {
        bytes_t alg = {0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
        bytes_t bits = {0x00};
        bytes_t pk1 = ossl_pkcs1_public(tk.ossl);
        bits.insert(bits.end(), pk1.begin(), pk1.end());
        bytes_t body = alg;
        auto bs = der_wrap(0x03, bits);
        body.insert(body.end(), bs.begin(), bs.end());
        auto e = PK::from_pkix_der(view(der_wrap(0x30, body)));
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), crypto::errc::malformed);
        EXPECT_NE(e.error().offset(), 0u);
    }
    // messages name the key and the offset
    auto e = SK::from_pkcs8_der(view(bytes_t{0x30, 0x03, 0x02, 0x01, 0x05}));
    ASSERT_FALSE(e.has_value());
    EXPECT_NE(text_of(e.error()).find("sgcl::crypto::rsa: "), std::string::npos) << text_of(e.error());
}

// --- keys made here ----------------------------------------------------------------------------

TEST(Crypto_Rsa, Generate) {
    EXPECT_THROW((void)SK::generate(1024), std::invalid_argument);
    EXPECT_THROW((void)SK::generate(2047), std::invalid_argument);
    EXPECT_THROW((void)SK::generate(16385), std::invalid_argument);
    random_source r(26);
    for (unsigned bits : {2048u, 2049u, 3072u}) {
        auto key = SK::generate(bits);
        auto pub = key.public_key();
        EXPECT_EQ(key.bits(), bits);
        EXPECT_EQ(pub.exponent(), 65537u);
        // OpenSSL's full check: p and q prime, n = p q, d e = 1 mod lambda,
        // the CRT values
        bytes_t p8 = to_bytes(key.to_pkcs8_der());
        Pkey ossl = ossl_private_from_der(p8);
        ASSERT_TRUE(ossl);
        EXPECT_TRUE(ossl_check_private(ossl)) << bits;
        EXPECT_EQ(hex(ossl_pkcs8(ossl)), hex(p8));
        Numbers n = numbers_of(ossl);
        EXPECT_EQ(n.v[3].size() * 8 >= (bits + 1) / 2 - 7, true);
        // FIPS 186-5 B.3.1: d is the smallest, below lcm(p - 1, q - 1)
        EXPECT_TRUE(ossl_d_below_lambda(ossl)) << bits;
        bytes_t d = ossl_digest(hash_id::sha256, r.bytes(40));
        bytes_t sig = to_bytes(key.sign_digest(hash_id::sha256, view(d)));
        EXPECT_EQ(hex(sig), hex(ossl_sign(ossl, hash_id::sha256, d, false)));
        bytes_t ct = ossl_encrypt_oaep(ossl, hash_id::sha256, hash_id::sha256, text("hi"), bytes_t{});
        auto m = key.decrypt_oaep(hash_id::sha256, view(ct));
        ASSERT_TRUE(m.has_value());
        EXPECT_EQ(to_bytes(*m), text("hi"));
    }
    // two keys are two keys
    EXPECT_FALSE(SK::generate(2048).public_key() == SK::generate(2048).public_key());
    // d below λ on keys whose gcd(p - 1, q - 1) is more than 2 too (about
    // half of them), which a d modulo (p - 1)(q - 1) fails
    for (int i = 0; i < 8; ++i) {
        Pkey k = ossl_private_from_der(to_bytes(SK::generate(2048).to_pkcs8_der()));
        EXPECT_TRUE(ossl_d_below_lambda(k)) << i;
        EXPECT_TRUE(ossl_check_private(k)) << i;
    }
}

// Primes of different lengths (the larger's words for both in CRT), and
// moduli of bit lengths that are not a multiple of 8 or 64: signatures
// equal to OpenSSL's, decryption of its ciphertexts
TEST(Crypto_Rsa, UnbalancedPrimes) {
    random_source r(27);
    struct {
        int p, q;
    } shapes[] = {{1000, 1048}, {1048, 1000}, {1500, 548}, {600, 1449}, {1025, 1024}, {1536, 1536}, {513, 511}};
    for (auto s : shapes) {
        Numbers n = numbers_of_primes(s.p, s.q);
        bytes_t der = n.der();
        auto key = SK::from_pkcs1_der(view(der));
        ASSERT_TRUE(key.has_value()) << s.p << "/" << s.q << ": " << text_of(key.error());
        Pkey ossl = ossl_private_from_der(der);
        ASSERT_TRUE(ossl);
        EXPECT_EQ(hex(to_bytes(key->to_pkcs1_der())), hex(der));
        for (int i = 0; i < 3; ++i) {
            bytes_t d = ossl_digest(hash_id::sha256, r.bytes(30));
            EXPECT_EQ(hex(to_bytes(key->sign_digest(hash_id::sha256, view(d)))), hex(ossl_sign(ossl, hash_id::sha256, d, false))) << s.p << "/" << s.q;
            bytes_t pss = to_bytes(key->sign_digest_pss(hash_id::sha256, view(d)));
            EXPECT_TRUE(ossl_verify(ossl, hash_id::sha256, d, pss, true));
            bytes_t msg = r.bytes(20);
            auto m = key->decrypt_oaep(hash_id::sha1, view(ossl_encrypt_oaep(ossl, hash_id::sha1, hash_id::sha1, msg, bytes_t{})));
            ASSERT_TRUE(m.has_value());
            EXPECT_EQ(to_bytes(*m), msg);
        }
    }
}

// --- the secrets ---------------------------------------------------------------------------------

// Every block of words a private key frees — its own and the scratch of
// every operation — is zero when it is freed; a move leaves the source
// empty (a call on it is std::logic_error, never an operation under a zero
// key); clone() is the same key
TEST(Crypto_Rsa, SecretsAreZeroed) {
    Probe::blocks = 0;
    Probe::dirty = 0;
    {
        const TestKey& tk = test_key(2048);
        auto key = ProbeKey::from_pkcs8_der(view(tk.pkcs8));
        ASSERT_TRUE(key.has_value());
        bytes_t d = ossl_digest(hash_id::sha256, text("x"));
        (void)key->sign_digest(hash_id::sha256, view(d));
        (void)key->sign_digest_pss(hash_id::sha256, view(d));
        bytes_t ct = to_bytes(key->public_key().encrypt_oaep(hash_id::sha256, view(text("y"))));
        EXPECT_TRUE(key->decrypt_oaep(hash_id::sha256, view(ct)).has_value());
        (void)key->to_pkcs1_der();
        auto c = key->clone();
        EXPECT_EQ(hex(to_bytes(c.to_pkcs8_der())), hex(tk.pkcs8));
        ProbeKey moved = std::move(*key);   // key: checked by the ASSERT_TRUE above; the accesses below throw on the key moved from, not on the expected
        EXPECT_THROW((void)key->sign_digest(hash_id::sha256, view(d)), std::logic_error);
        EXPECT_THROW((void)key->decrypt_oaep(hash_id::sha256, view(ct)), std::logic_error);
        EXPECT_THROW((void)key->to_pkcs8_der(), std::logic_error);
        EXPECT_THROW((void)key->public_key(), std::logic_error);
        EXPECT_THROW((void)key->clone(), std::logic_error);
        // bits() and size() too: they once gave the old key's values
        EXPECT_THROW((void)key->bits(), std::logic_error);
        EXPECT_THROW((void)key->size(), std::logic_error);
        EXPECT_FALSE(noexcept(key->bits()));
        EXPECT_FALSE(noexcept(key->size()));
        EXPECT_EQ(hex(to_bytes(moved.to_pkcs8_der())), hex(tk.pkcs8));
        auto g = ProbeKey::generate(2048);
        (void)g.sign_digest(hash_id::sha256, view(d));
        g = std::move(moved);
    }
    EXPECT_GT(Probe::blocks, 20u);
    EXPECT_EQ(Probe::dirty, 0u);
    // a moved-from public key
    PK a = our_public(test_key(2048));
    PK b = std::move(a);
    EXPECT_THROW((void)a.to_pkix_der(), std::logic_error);
    bytes_t d = ossl_digest(hash_id::sha256, text("x"));
    EXPECT_THROW((void)a.verify_digest(hash_id::sha256, view(d), view(d)), std::logic_error);
    // bits(), size(), exponent(), max_oaep_message_size() and == too: they
    // once gave the old key's values and compared the empty modulus
    EXPECT_THROW((void)a.bits(), std::logic_error);
    EXPECT_THROW((void)a.size(), std::logic_error);
    EXPECT_THROW((void)a.exponent(), std::logic_error);
    EXPECT_THROW((void)a.max_oaep_message_size(hash_id::sha256), std::logic_error);
    EXPECT_THROW((void)(a == b), std::logic_error);
    EXPECT_THROW((void)(b == a), std::logic_error);
    EXPECT_FALSE(noexcept(a.bits()));
    EXPECT_FALSE(noexcept(a.size()));
    EXPECT_FALSE(noexcept(a.exponent()));
    EXPECT_FALSE(noexcept(a == b));
    EXPECT_EQ(b.bits(), 2048u);
    EXPECT_TRUE(b == our_public(test_key(2048)));
}

// A fault in one half of CRT (a bit of dP changed after the key's checks)
// never leaves the key: the signature fails its check with e and is not
// given out (it would factor n: gcd(s^e - m, n) = q), the decryption is
// the one error
TEST(Crypto_Rsa, FaultIsCaught) {
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    bytes_t d = ossl_digest(hash_id::sha256, text("x"));
    bytes_t ct = to_bytes(key.public_key().encrypt_oaep(hash_id::sha256, view(text("y"))));
    ASSERT_TRUE(key.decrypt_oaep(hash_id::sha256, view(ct)).has_value());
    Access::break_dp(key, 100);
    EXPECT_THROW((void)key.sign_digest(hash_id::sha256, view(d)), std::runtime_error);
    EXPECT_THROW((void)key.sign_digest_pss(hash_id::sha256, view(d)), std::runtime_error);
    auto m = key.decrypt_oaep(hash_id::sha256, view(ct));
    ASSERT_FALSE(m.has_value());
    EXPECT_EQ(text_of(m.error()), "sgcl::crypto::rsa: decryption error");
    Access::break_dp(key, 100);
    EXPECT_TRUE(key.decrypt_oaep(hash_id::sha256, view(ct)).has_value());
}

// The raw private operation is the inverse of the public one, blinded or
// not: m^d^e = m for random m below n, 0, 1 and n - 1, against OpenSSL's
TEST(Crypto_Rsa, PrivateOperation) {
    random_source r(28);
    for (unsigned bits : sizes) {
        const TestKey& tk = test_key(bits);
        auto key = our_key(tk);
        size_t k = bits / 8;
        bytes_t n = to_bytes(key.public_key().modulus());
        bytes_t n1 = n;
        n1.back() -= 1;
        for (int i = 0; i < 8; ++i) {
            bytes_t m = i == 0 ? bytes_t(k, 0) : i == 1 ? bytes_t(k, 0) : i == 2 ? n1 : r.bytes(k);
            if (i == 1) {
                m.back() = 1;
            }
            if (i > 2) {
                m[0] &= 0x7f;
            }
            bytes_t out(k);
            ASSERT_TRUE(Access::private_op(key, out.data(), m.data()));
            EXPECT_EQ(hex(out), hex(ossl_raw_private(tk.ossl, m))) << bits << " " << i;
        }
    }
}

// --- Wycheproof ---------------------------------------------------------------------------------

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

    optional<hash_id> hash_named(const std::string& sha) {
        if (sha == "SHA-1") return hash_id::sha1;
        if (sha == "SHA-224") return hash_id::sha224;
        if (sha == "SHA-256") return hash_id::sha256;
        if (sha == "SHA-384") return hash_id::sha384;
        if (sha == "SHA-512") return hash_id::sha512;
        if (sha == "SHA-512/256") return hash_id::sha512_256;
        if (sha == "SHA3-224") return hash_id::sha3_224;
        if (sha == "SHA3-256") return hash_id::sha3_256;
        if (sha == "SHA3-384") return hash_id::sha3_384;
        if (sha == "SHA3-512") return hash_id::sha3_512;
        return nullopt;
    }

    // The public key of a group: SPKI in publicKeyDer (or keyDer)
    optional<PK> group_key(const sgcl::encoding::json& group) {
        std::string der = field(group, "publicKeyDer");
        if (der.empty()) {
            der = field(group, "keyDer");
        }
        auto k = PK::from_pkix_der(view(unhex(der)));
        if (!k) {
            return nullopt;
        }
        return *k;
    }

    // rsa_signature_*_test.json (PKCS #1 v1.5) and rsa_pss_*_test.json:
    // "valid" must verify, "invalid" must not (for PSS, unless OpenSSL with
    // the salt length read from the signature takes it too: our
    // verification reads the length, the file's groups fix it), and
    // "acceptable" either
    void wycheproof_signatures(const char* file, bool pss) {
        auto doc = wycheproof(file);
        if (!doc) {
            GTEST_SKIP() << file << " not on disk";
        }
        size_t cases = 0;
        for (auto& group : (*doc)["testGroups"].elements()) {
            auto id = hash_named(field(group, "sha"));
            auto key = group_key(group);
            if (!id || !key || (pss && field(group, "mgfSha") != field(group, "sha"))) {
                continue;
            }
            auto spki = to_bytes(key->to_pkix_der());
            Pkey ossl = ossl_public_from_spki(spki);
            for (auto& t : group["tests"].elements()) {
                bytes_t digest = ossl_digest(*id, unhex(field(t, "msg")));
                bytes_t sig = unhex(field(t, "sig"));
                bool ok = pss ? key->verify_digest_pss(*id, view(digest), view(sig)) : key->verify_digest(*id, view(digest), view(sig));
                std::string result = field(t, "result");
                if (result == "valid") {
                    EXPECT_TRUE(ok) << file << " tcId " << t["tcId"].as_int().value_or(0);
                } else if (result == "invalid" && ok) {
                    bool auto_ok = pss && ossl_verify(ossl, *id, digest, sig, true, RSA_PSS_SALTLEN_AUTO);
                    EXPECT_TRUE(auto_ok) << file << " tcId " << t["tcId"].as_int().value_or(0);
                }
                ++cases;
            }
        }
        EXPECT_GT(cases, 0u);
    }

    // rsa_oaep_*_test.json: the private key as PKCS #8 (privateKeyPkcs8),
    // "valid" decrypts to msg, "invalid" is the one error
    void wycheproof_oaep(const char* file) {
        auto doc = wycheproof(file);
        if (!doc) {
            GTEST_SKIP() << file << " not on disk";
        }
        size_t cases = 0;
        for (auto& group : (*doc)["testGroups"].elements()) {
            auto id = hash_named(field(group, "sha"));
            auto mgf = hash_named(field(group, "mgfSha"));
            auto key = SK::from_pkcs8_der(view(unhex(field(group, "privateKeyPkcs8"))));
            if (!id || !mgf || !key) {
                continue;
            }
            for (auto& t : group["tests"].elements()) {
                auto m = key->decrypt_oaep(*id, *mgf, view(unhex(field(t, "ct"))), view(unhex(field(t, "label"))));
                std::string result = field(t, "result");
                if (result == "valid") {
                    ASSERT_TRUE(m.has_value()) << file << " tcId " << t["tcId"].as_int().value_or(0);
                    EXPECT_EQ(hex(to_bytes(*m)), field(t, "msg"));
                } else if (result == "invalid") {
                    EXPECT_FALSE(m.has_value()) << file << " tcId " << t["tcId"].as_int().value_or(0);
                }
                ++cases;
            }
        }
        EXPECT_GT(cases, 0u);
    }
}

TEST(Crypto_Wycheproof, RsaPkcs1v15Signatures) {
    wycheproof_signatures("rsa_signature_2048_sha256_test.json", false);
    wycheproof_signatures("rsa_signature_3072_sha384_test.json", false);
    wycheproof_signatures("rsa_signature_4096_sha512_test.json", false);
}

TEST(Crypto_Wycheproof, RsaPss) {
    wycheproof_signatures("rsa_pss_2048_sha256_mgf1_32_test.json", true);
    wycheproof_signatures("rsa_pss_3072_sha256_mgf1_32_test.json", true);
    wycheproof_signatures("rsa_pss_4096_sha512_mgf1_64_test.json", true);
}

TEST(Crypto_Wycheproof, RsaOaep) {
    wycheproof_oaep("rsa_oaep_2048_sha256_mgf1sha256_test.json");
    wycheproof_oaep("rsa_oaep_2048_sha1_mgf1sha1_test.json");
    wycheproof_oaep("rsa_oaep_3072_sha256_mgf1sha256_test.json");
    wycheproof_oaep("rsa_oaep_misc_test.json");
}

// --- the smallest and the largest moduli (DESIGN 408) ------------------------------------------

namespace {
    // A key of 16384 bits, the largest read: made once by OpenSSL 3
    // (openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:16384),
    // since making one takes a minute or more
    bytes_t largest_key_pem() {
        std::string f = __FILE__;
        std::ifstream in(f.substr(0, f.rfind('/')) + "/data/rsa16384.pem", std::ios::binary);
        return bytes_t(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // An odd number of exactly `bits` bits, as a modulus's bytes
    bytes_t odd_of_bits(size_t bits) {
        bytes_t n((bits + 7) / 8, 0xa5);
        n[0] = (unsigned char)(1u << ((bits - 1) % 8));
        n.back() |= 1;
        return n;
    }
}

// The bounds of the modulus, bit for bit: 1023 bits is refused and 1024
// read, 16384 read and 16385 refused, through from_modulus and through a
// key OpenSSL made; leading zero bytes do not count
TEST(Crypto_Rsa, TheBoundsOfTheModulus) {
    for (size_t bits : {size_t(1023), size_t(16385)}) {
        auto k = PK::from_modulus(view(odd_of_bits(bits)), 65537);
        ASSERT_FALSE(k.has_value()) << bits;
        EXPECT_EQ(k.error().code(), crypto::errc::unsupported) << bits;
    }
    for (size_t bits : {size_t(1024), size_t(16384)}) {
        auto n = odd_of_bits(bits);
        auto k = PK::from_modulus(view(n), 65537);
        ASSERT_TRUE(k.has_value()) << bits;
        EXPECT_EQ(k->bits(), bits);
        EXPECT_EQ(k->size(), bits / 8);
        bytes_t padded(5, 0);
        padded.insert(padded.end(), n.begin(), n.end());
        auto p = PK::from_modulus(view(padded), 65537);
        ASSERT_TRUE(p.has_value()) << bits;
        EXPECT_TRUE(*p == *k);
        EXPECT_EQ(hex(to_bytes(p->modulus())), hex(n));
        // the DER written is read back, by us and by OpenSSL
        auto spki = to_bytes(k->to_pkix_der());
        auto back = PK::from_pkix_der(view(spki));
        ASSERT_TRUE(back.has_value()) << bits;
        EXPECT_TRUE(*back == *k);
        EXPECT_TRUE(ossl_public_from_spki(spki)) << bits;
    }
    // a key of 1023 bits that OpenSSL made, in PKCS #8 and as an SPKI
    Pkey small = ossl_generate(1023);
    EXPECT_EQ(error_of(SK::from_pkcs8_der(view(ossl_pkcs8(small)))).code(), crypto::errc::unsupported);
    EXPECT_EQ(error_of(PK::from_pkix_der(view(ossl_spki(small)))).code(), crypto::errc::unsupported);
}

// The smallest key: every hash signs in PKCS #1 v1.5; PSS refuses SHA-512,
// whose digest and salt do not fit 1024 bits, and takes SHA-384; OAEP under
// SHA-512 has no room for any message (the maximum is 0, an empty message
// is refused, a decryption is the one error)
TEST(Crypto_Rsa, TheSmallestKey) {
    const TestKey& tk = test_key(1024);
    auto key = our_key(tk);
    auto pub = key.public_key();
    EXPECT_EQ(key.bits(), 1024u);
    for (hash_id id : all_hashes) {
        bytes_t digest = ossl_digest(id, {'m'});
        auto sig = to_bytes(key.sign_digest(id, view(digest)));
        EXPECT_TRUE(ossl_verify(tk.ossl, id, digest, sig, false)) << int(id);
        EXPECT_TRUE(pub.verify_digest(id, view(digest), view(sig))) << int(id);
    }
    bytes_t d512 = ossl_digest(hash_id::sha512, {'m'});
    EXPECT_THROW((void)key.sign_digest_pss(hash_id::sha512, view(d512)), std::invalid_argument);
    bytes_t d384 = ossl_digest(hash_id::sha384, {'m'});
    auto pss = to_bytes(key.sign_digest_pss(hash_id::sha384, view(d384)));
    EXPECT_TRUE(ossl_verify(tk.ossl, hash_id::sha384, d384, pss, true));
    EXPECT_TRUE(pub.verify_digest_pss(hash_id::sha384, view(d384), view(pss)));
    // OAEP: SHA-384 leaves 128 - 98 = 30 bytes, SHA-512 nothing at all
    EXPECT_EQ(pub.max_oaep_message_size(hash_id::sha384), 30u);
    bytes_t thirty(30, 7);
    auto ct = to_bytes(pub.encrypt_oaep(hash_id::sha384, view(thirty)));
    auto theirs = ossl_decrypt_oaep(tk.ossl, hash_id::sha384, hash_id::sha384, ct, {});
    ASSERT_TRUE(theirs.has_value());
    EXPECT_EQ(hex(*theirs), hex(thirty));
    EXPECT_THROW((void)pub.encrypt_oaep(hash_id::sha384, view(bytes_t(31))), std::invalid_argument);
    EXPECT_EQ(pub.max_oaep_message_size(hash_id::sha512), 0u);
    EXPECT_THROW((void)pub.encrypt_oaep(hash_id::sha512, view(bytes_t())), std::invalid_argument);
    EXPECT_THROW((void)pub.encrypt_oaep(hash_id::sha512, sgcl::slice<const byte>()), std::invalid_argument);
    bytes_t any(128, 1);
    EXPECT_EQ(error_of(key.decrypt_oaep(hash_id::sha512, view(any))).code(), crypto::errc::authentication);
    sgcl::slice<byte> nowhere;
    EXPECT_EQ(error_of(key.decrypt_oaep_to(nowhere, hash_id::sha512, view(any))).code(), crypto::errc::authentication);
}

// A key moved onto itself is the same key, as every other key of the
// module is (a move out of one: Crypto_Rsa.SecretsAreZeroed and the
// logic_error of a moved-from key, Crypto_Rsa.PrivateOperation)
TEST(Crypto_Rsa, KeysMovedOntoThemselves) {
    auto key = our_key(test_key(2048));
    auto pub = key.public_key();
    bytes_t digest = ossl_digest(hash_id::sha256, {'s'});
    auto sig = to_bytes(key.sign_digest(hash_id::sha256, view(digest)));
    SK& same_key = key;
    key = std::move(same_key);
    PK& same_pub = pub;
    pub = std::move(same_pub);
    EXPECT_EQ(key.bits(), 2048u);
    EXPECT_EQ(hex(to_bytes(key.sign_digest(hash_id::sha256, view(digest)))), hex(sig));
    EXPECT_TRUE(pub.verify_digest(hash_id::sha256, view(digest), view(sig)));
    EXPECT_TRUE(pub == key.public_key());
    // a key moved from, then given one again
    SK other = std::move(key);
    EXPECT_THROW((void)key.bits(), std::logic_error);
    key = std::move(other);
    EXPECT_EQ(hex(to_bytes(key.sign_digest(hash_id::sha256, view(digest)))), hex(sig));
}

// The largest key, 16384 bits: read from PEM, its PKCS #8 and PKCS #1
// written as OpenSSL writes them (the writer's room is sized for it),
// signatures both ways, OAEP at its longest message
TEST(Crypto_Rsa, TheLargestKey) {
    bytes_t pem = largest_key_pem();
    ASSERT_FALSE(pem.empty()) << "tests/crypto/data/rsa16384.pem";
    auto read = SK::from_pem(view(pem));
    ASSERT_TRUE(read.has_value()) << text_of(read.error());
    auto key = std::move(*read);
    EXPECT_EQ(key.bits(), 16384u);
    EXPECT_EQ(key.size(), 2048u);
    bytes_t p8 = to_bytes(key.to_pkcs8_der());
    EXPECT_LE(p8.size(), crypto::detail::Rsa::der_capacity);
    Pkey ossl = ossl_private_from_der(p8);
    ASSERT_TRUE(ossl);
    EXPECT_EQ(hex(ossl_pkcs8(ossl)), hex(p8));
    EXPECT_EQ(hex(to_bytes(key.to_pkcs1_der())), hex(ossl_pkcs1_private(ossl)));
    auto again = SK::from_pem(key.to_pem().as_slice());
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(hex(to_bytes(again->to_pkcs8_der())), hex(p8));
    auto pub = key.public_key();
    EXPECT_EQ(hex(to_bytes(pub.to_pkix_der())), hex(ossl_spki(ossl)));
    EXPECT_EQ(hex(to_bytes(pub.to_pkcs1_der())), hex(ossl_pkcs1_public(ossl)));
    // signatures, ours checked by OpenSSL and theirs by us
    bytes_t digest = ossl_digest(hash_id::sha512, {'x'});
    auto sig = to_bytes(key.sign_digest(hash_id::sha512, view(digest)));
    EXPECT_EQ(sig.size(), 2048u);
    EXPECT_TRUE(ossl_verify(ossl, hash_id::sha512, digest, sig, false));
    auto pss = to_bytes(key.sign_digest_pss(hash_id::sha512, view(digest)));
    EXPECT_TRUE(ossl_verify(ossl, hash_id::sha512, digest, pss, true));
    auto theirs = ossl_sign(ossl, hash_id::sha512, digest, true);
    EXPECT_TRUE(pub.verify_digest_pss(hash_id::sha512, view(digest), view(theirs)));
    // OAEP at its longest message under SHA-256: 2048 - 66 bytes
    const size_t max = pub.max_oaep_message_size(hash_id::sha256);
    EXPECT_EQ(max, 2048u - 66u);
    bytes_t msg(max, 0x5c);
    auto ct = to_bytes(pub.encrypt_oaep(hash_id::sha256, view(msg)));
    auto back = ossl_decrypt_oaep(ossl, hash_id::sha256, hash_id::sha256, ct, {});
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(hex(*back), hex(msg));
    auto ours = key.decrypt_oaep(hash_id::sha256, view(ossl_encrypt_oaep(ossl, hash_id::sha256, hash_id::sha256, msg, {})));
    ASSERT_TRUE(ours.has_value());
    EXPECT_EQ(hex(to_bytes(*ours)), hex(msg));
    EXPECT_THROW((void)pub.encrypt_oaep(hash_id::sha256, view(bytes_t(max + 1))), std::invalid_argument);
}

// sign, sign_pss, verify and verify_pss of a message, hashed inside: for
// every hash and the messages of no bytes (empty and null), one, and
// longer than a block, what the digest's forms give of OpenSSL's digest
// (PKCS #1 v1.5 byte for byte with OpenSSL; PSS verified by OpenSSL); a
// message changed by a bit, a signature of another hash, a salt length
// fixed, an id that is none of hash_id's, the smallest key too small for
// PSS-SHA-512, a key moved from
TEST(Crypto_Rsa, SignAndVerifyAMessage) {
    random_source r(41);
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    auto pub = key.public_key();
    for (hash_id id : all_hashes) {
        for (size_t n : {size_t(0), size_t(1), size_t(200)}) {
            bytes_t m = r.bytes(n);
            bytes_t d = ossl_digest(id, m);
            bytes_t sig = to_bytes(key.sign(id, view(m)));
            EXPECT_EQ(hex(sig), hex(ossl_sign(tk.ossl, id, d, false))) << int(id) << " " << n;
            EXPECT_TRUE(pub.verify(id, view(m), view(sig)));
            EXPECT_TRUE(pub.verify_digest(id, view(d), view(sig)));
            bytes_t pss = to_bytes(key.sign_pss(id, view(m)));
            EXPECT_TRUE(ossl_verify(tk.ossl, id, d, pss, true, RSA_PSS_SALTLEN_DIGEST)) << int(id) << " " << n;
            EXPECT_TRUE(pub.verify_pss(id, view(m), view(pss)));
            EXPECT_TRUE(pub.verify_pss(id, view(m), view(pss), crypto::digest_size(id)));
            EXPECT_FALSE(pub.verify_pss(id, view(m), view(pss), crypto::digest_size(id) + 1));
            EXPECT_FALSE(pub.verify(id, view(m), view(pss)));   // a PSS signature is no PKCS #1 v1.5 one
            bytes_t changed = m;
            changed.push_back(0);
            EXPECT_FALSE(pub.verify(id, view(changed), view(sig)));
            EXPECT_FALSE(pub.verify_pss(id, view(changed), view(pss)));
        }
    }
    auto empty = to_bytes(key.sign(hash_id::sha256, sgcl::slice<const byte>()));
    EXPECT_TRUE(pub.verify(hash_id::sha256, view(bytes_t()), view(empty)));
    EXPECT_FALSE(pub.verify(hash_id::sha384, view(bytes_t()), view(empty)));
    EXPECT_FALSE(pub.verify(hash_id::sha256, view(bytes_t()), sgcl::slice<const byte>()));
    EXPECT_THROW((void)key.sign(hash_id(200), view(bytes_t(1))), std::invalid_argument);
    EXPECT_THROW((void)key.sign_pss(hash_id(0), view(bytes_t(1))), std::invalid_argument);
    EXPECT_THROW((void)pub.verify(hash_id(200), view(bytes_t(1)), view(empty)), std::invalid_argument);
    EXPECT_THROW((void)pub.verify_pss(hash_id(200), view(bytes_t(1)), view(empty)), std::invalid_argument);
    auto small = our_key(test_key(1024));
    EXPECT_THROW((void)small.sign_pss(hash_id::sha512, view(bytes_t(1))), std::invalid_argument);
    EXPECT_TRUE(small.public_key().verify(hash_id::sha512, view(bytes_t(1)), view(to_bytes(small.sign(hash_id::sha512, view(bytes_t(1)))))));
    SK gone = std::move(key);
    EXPECT_THROW((void)key.sign(hash_id::sha256, view(bytes_t(1))), std::logic_error);
    EXPECT_THROW((void)key.sign_pss(hash_id::sha256, view(bytes_t(1))), std::logic_error);
    PK moved_pub = std::move(pub);
    EXPECT_THROW((void)pub.verify(hash_id::sha256, view(bytes_t(1)), view(empty)), std::logic_error);
}
