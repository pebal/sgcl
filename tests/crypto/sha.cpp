//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The digests (SHA-1, SHA-2, SHA-3) and SHAKE: the examples of FIPS 180-4
// and FIPS 202 (the vectors NIST publishes with them: "abc", the 448- and
// 896-bit messages, a million 'a', 200 bytes of 0xA3), OpenSSL on random
// data of every length 0…1024 and a few large ones, fed at once and in
// pieces of 1, 2, 3 and 7 bytes and of random sizes, the hasher's shape
// (a copy branches, value() ends nothing, reset, of, copy_from, the text
// forms), hash_id, and on arm64 the instructions' path against the
// portable one block by block. The whole file runs on both paths:
// tests_crypto_portable, built with SGCL_CRYPTO_PORTABLE, runs every vector
// on the plain C++.
#include "digest_common.h"

#include "sgcl/io/stream.h"

#include <map>

using namespace crypto_test;

namespace {
    // FIPS 180-4 / FIPS 202 examples, by message and digest
    const std::map<std::string, bytes_t>& messages() {
        static const std::map<std::string, bytes_t> m = {
            {"empty", {}},
            {"abc", text("abc")},
            {"448", text("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")},
            {"896", text("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")},
            {"a3x200", repeat(0xa3, 200)},
            {"million", repeat('a', 1000000)},
        };
        return m;
    }

    struct Kat {
        const char* algorithm;
        const char* message;
        const char* digest;
    };

    const Kat kats[] = {
        {"sha1", "empty", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
        {"sha1", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {"sha1", "448", "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
        {"sha1", "896", "a49b2446a02c645bf419f995b67091253a04a259"},
        {"sha1", "million", "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
        {"sha224", "empty", "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"},
        {"sha224", "abc", "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
        {"sha224", "448", "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525"},
        {"sha224", "million", "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"},
        {"sha256", "empty", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"sha256", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"sha256", "448", "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {"sha256", "896", "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"},
        {"sha256", "million", "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
        {"sha384", "empty", "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b"},
        {"sha384", "abc", "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"},
        {"sha384", "896", "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039"},
        {"sha384", "million", "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985"},
        {"sha512", "empty", "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"},
        {"sha512", "abc", "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
        {"sha512", "896", "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"},
        {"sha512", "million", "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b"},
        {"sha512_256", "abc", "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"},
        {"sha512_256", "896", "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a"},
        {"sha3_224", "empty", "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7"},
        {"sha3_224", "abc", "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf"},
        {"sha3_224", "a3x200", "9376816aba503f72f96ce7eb65ac095deee3be4bf9bbc2a1cb7e11e0"},
        {"sha3_256", "empty", "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"},
        {"sha3_256", "abc", "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"},
        {"sha3_256", "448", "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376"},
        {"sha3_256", "a3x200", "79f38adec5c20307a98ef76e8324afbfd46cfd81b22e3973c65fa1bd9de31787"},
        {"sha3_256", "million", "5c8875ae474a3634ba4fd55ec85bffd661f32aca75c6d699d0cdcb6c115891c1"},
        {"sha3_384", "empty", "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004"},
        {"sha3_384", "abc", "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25"},
        {"sha3_384", "a3x200", "1881de2ca7e41ef95dc4732b8f5f002b189cc1e42b74168ed1732649ce1dbcdd76197a31fd55ee989f2d7050dd473e8f"},
        {"sha3_512", "empty", "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"},
        {"sha3_512", "abc", "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0"},
        {"sha3_512", "a3x200", "e76dfad22084a8b1467fcf2ffa58361bec7628edf5f3fdc0e4805dc48caeeca81b7c13c30adf52a3659584739a2df46be589c51ca1a4a8416df6545a1ce8ba00"},
        {"sha3_512", "million", "3c3a876da14034ab60627c077bb98f7e120a2a5370212dffb3385a18d4f38859ed311d0a9d5141ce9cc5c66ee689b266a8aa18ace8282a0e0db596c90b0a7b87"},
    };

    // The digest of a message by the algorithm a KAT names, through hash_id
    crypto::hash_id id_of(const std::string& name) {
        static const std::map<std::string, crypto::hash_id> ids = {
            {"sha1", crypto::hash_id::sha1},           {"sha224", crypto::hash_id::sha224},
            {"sha256", crypto::hash_id::sha256},       {"sha384", crypto::hash_id::sha384},
            {"sha512", crypto::hash_id::sha512},       {"sha512_256", crypto::hash_id::sha512_256},
            {"sha3_224", crypto::hash_id::sha3_224},   {"sha3_256", crypto::hash_id::sha3_256},
            {"sha3_384", crypto::hash_id::sha3_384},   {"sha3_512", crypto::hash_id::sha3_512},
        };
        return ids.at(name);
    }

    template<class H>
    std::string of(const bytes_t& data) {
        return hex(H::of(view(data)));
    }

    template<class H>
    std::string streamed(const bytes_t& data, size_t chunk) {
        H h;
        feed(h, data, chunk);
        return hex(h.value());
    }

    template<class H>
    class Crypto_Digest : public ::testing::Test {};

    TYPED_TEST_SUITE(Crypto_Digest, Digests);
}

// Every example of the standards, by the type and by hash_id, at once and
// a byte at a time (the million 'a' in pieces of 7)
TEST(Crypto_Sha, StandardsExamples) {
    for (const Kat& k : kats) {
        SCOPED_TRACE(std::string(k.algorithm) + " " + k.message);
        const bytes_t& m = messages().at(k.message);
        crypto::hash_id id = id_of(k.algorithm);
        EXPECT_EQ(hex(crypto::digest(id, view(m))), k.digest);
        EXPECT_EQ(crypto::digest_size(id) * 2, std::strlen(k.digest));
        size_t chunk = m.size() > 1000 ? 7 : 1;
        std::string streamed_digest = crypto::detail::visit_hash(id, [&](auto t) {
            typename decltype(t)::type h;
            feed(h, m, chunk);
            return hex(h.value());
        });
        EXPECT_EQ(streamed_digest, k.digest);
    }
}

TEST(Crypto_Sha, ShakeExamples) {
    // FIPS 202 examples (NIST's SHAKE vectors), the first 64 bytes of output
    EXPECT_EQ(hex(crypto::shake128::of(view(bytes_t()), 64)),
              "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef263cb1eea988004b93103cfb0aeefd2a686e01fa4a58e8a3639ca8a1e3f9ae57e2");
    EXPECT_EQ(hex(crypto::shake256::of(view(bytes_t()), 64)),
              "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be");
    EXPECT_EQ(hex(crypto::shake128::of("abc", 64)),
              "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc844c50af32acd3f2cdd066568706f509bc1bdde58295dae3f891a9a0fca578378");
    EXPECT_EQ(hex(crypto::shake256::of("abc", 64)),
              "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4");
    EXPECT_EQ(hex(crypto::shake128::of(view(repeat(0xa3, 200)), 64)),
              "131ab8d2b594946b9c81333f9bb6e0ce75c3b93104fa3469d3917457385da037cf232ef7164a6d1eb448c8908186ad852d3f85a5cf28da1ab6fe343817197846");
    EXPECT_EQ(hex(crypto::shake256::of(view(repeat(0xa3, 200)), 64)),
              "cd8a920ed141aa0407a22d59288652e9d9f1a7ee0c1e7c1ca699424da84a904d2d700caae7396ece96604440577da4f3aa22aeb8857f961c4cd8e06f0ae6610b");
}

// Every length 0…1024 of random bytes, and a few large ones, against
// OpenSSL in one call
TYPED_TEST(Crypto_Digest, EveryLengthAgainstOpenSsl) {
    using H = TypeParam;
    random_source r(1);
    bytes_t data = r.bytes(1 << 20 | 13);
    for (size_t n = 0; n <= 1024; ++n) {
        ASSERT_EQ(hex(H::of(view(data.data(), n))), hex(ossl_digest<H>(data.data(), n))) << "length " << n;
    }
    for (size_t n : {4095u, 4096u, 65537u, (1u << 20) | 13u}) {
        ASSERT_EQ(hex(H::of(view(data.data(), n))), hex(ossl_digest<H>(data.data(), n))) << "length " << n;
    }
}

// The same fed in pieces: 1, 2, 3 and 7 bytes an update over every length
// to 400 (past three blocks of each), random pieces to 1100
TYPED_TEST(Crypto_Digest, InPiecesAgainstOpenSsl) {
    using H = TypeParam;
    random_source r(2);
    for (size_t n = 0; n <= 400; ++n) {
        bytes_t data = r.bytes(n);
        std::string expected = hex(ossl_digest<H>(data.data(), n));
        for (size_t chunk : {1u, 2u, 3u, 7u}) {
            ASSERT_EQ(streamed<H>(data, chunk), expected) << "length " << n << " chunk " << chunk;
        }
    }
    for (int i = 0; i < 200; ++i) {
        bytes_t data = r.bytes(r.below(1100));
        H h;
        feed_random(h, data, r, 300);
        ASSERT_EQ(hex(h.value()), hex(ossl_digest<H>(data.data(), data.size()))) << "length " << data.size();
    }
}

// The hasher's shape: value() ends nothing, a copy is a branch, reset()
// is as new, the text forms hash the text's bytes, copy_from reads a stream
TYPED_TEST(Crypto_Digest, TheHashersShape) {
    using H = TypeParam;
    static_assert(sgcl::hash::req::hasher<H>);
    static_assert(std::is_trivially_copyable_v<H>);
    static_assert(H::digest_size == decltype(H().value())().size());
    EXPECT_EQ(H::digest_size, size_t(EVP_MD_get_size(ossl_md<H>())));
    EXPECT_EQ(H::block_size, size_t(EVP_MD_get_block_size(ossl_md<H>())));

    random_source r(3);
    bytes_t a = r.bytes(300), b = r.bytes(500);
    bytes_t ab = a;
    ab.insert(ab.end(), b.begin(), b.end());

    H h;
    h.update(view(a));
    auto after_a = h.value();
    EXPECT_EQ(hex(after_a), of<H>(a));
    H branch = h;   // a copy is a branch
    h.update(view(b));
    EXPECT_EQ(hex(h.value()), of<H>(ab));
    EXPECT_EQ(hex(branch.value()), of<H>(a));
    EXPECT_EQ(hex(h.digest()), hex(h.value()));
    h.reset();
    EXPECT_EQ(hex(h.value()), of<H>(bytes_t()));

    // text: a literal, a string, a std::string_view, a C string
    std::string t = "The quick brown fox";
    EXPECT_EQ(hex(H::of("The quick brown fox")), of<H>(text(t)));
    EXPECT_EQ(hex(H::of(sgcl::string("The quick brown fox"))), of<H>(text(t)));
    EXPECT_EQ(hex(H::of(std::string_view(t))), of<H>(text(t)));
    EXPECT_EQ(hex(H::of(t.c_str())), of<H>(text(t)));

    // a stream to its end
    sgcl::io::buffer buf(view(ab));
    H s;
    auto n = s.copy_from(buf);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(*n, ab.size());
    EXPECT_EQ(hex(s.value()), of<H>(ab));
}

TEST(Crypto_Sha, HashIdSizes) {
    EXPECT_EQ(crypto::digest_size(crypto::hash_id::sha256), 32u);
    EXPECT_EQ(crypto::block_size(crypto::hash_id::sha256), 64u);
    EXPECT_EQ(crypto::digest_size(crypto::hash_id::sha512_256), 32u);
    EXPECT_EQ(crypto::block_size(crypto::hash_id::sha512_256), 128u);
    EXPECT_EQ(crypto::block_size(crypto::hash_id::sha3_256), 136u);
    EXPECT_EQ(crypto::digest_size(crypto::hash_id::sha3_512), 64u);
    EXPECT_THROW((void)crypto::digest_size(crypto::hash_id(0)), std::invalid_argument);
    EXPECT_THROW((void)crypto::digest(crypto::hash_id(200), "abc"), std::invalid_argument);
    EXPECT_EQ(hex(crypto::digest(crypto::hash_id::sha3_256, "abc")), hex(crypto::sha3_256::of("abc")));
}

// SHAKE against OpenSSL: random inputs of every length to 400 in pieces,
// the output read at once and in pieces of 1, 7 and 200 bytes (reads that
// cross the rate's blocks)
template<class X>
void shake_against_openssl() {
    random_source r(4);
    for (size_t n = 0; n <= 400; ++n) {
        bytes_t data = r.bytes(n);
        size_t out = 1 + r.below(600);
        std::string expected = hex(ossl_xof<X>(data.data(), n, out));
        ASSERT_EQ(hex(X::of(view(data), out)), expected) << "length " << n;
        for (size_t piece : {1u, 7u, 200u}) {
            X x;
            feed(x, data, 3);
            bytes_t got(out);
            for (size_t i = 0; i < out; i += piece) {
                size_t take = std::min(piece, out - i);
                x.read_to(sgcl::slice<byte>(reinterpret_cast<byte*>(got.data() + i), take));
            }
            ASSERT_EQ(hex(got), expected) << "length " << n << " read by " << piece;
        }
    }
}

TEST(Crypto_Shake, Shake128AgainstOpenSsl) {
    shake_against_openssl<crypto::shake128>();
}

TEST(Crypto_Shake, Shake256AgainstOpenSsl) {
    shake_against_openssl<crypto::shake256>();
}

TEST(Crypto_Shake, ReadsGoOnAndInputCloses) {
    crypto::shake256 x;
    x.update("seed");
    auto first = x.read(16);
    auto second = x.read(16);
    auto whole = crypto::shake256::of("seed", 32);
    EXPECT_EQ(hex(first) + hex(second), hex(whole));
    EXPECT_THROW(x.update("more"), std::invalid_argument);
    crypto::shake256 branch = x;   // a copy is a branch, reading on alike
    EXPECT_EQ(hex(branch.read(8)), hex(x.read(8)));
    x.reset();
    x.update("seed");
    EXPECT_EQ(hex(x.read(32)), hex(whole));
    EXPECT_EQ(x.read(0).size(), 0u);
    static_assert(!sgcl::hash::req::hasher<crypto::shake128>);
}

#if defined(SGCL_CRYPTO_ARM64)
// The instructions' path against the plain C++, block by block, on random
// blocks and random chaining values (the two agree on every state, not
// only on the ones the initial values lead to)
TEST(Crypto_Sha, TheTwoPathsAgree) {
    namespace d = crypto::detail;
    random_source r(5);
    for (int round = 0; round < 200; ++round) {
        size_t blocks = 1 + r.below(9);
        bytes_t data = r.bytes(blocks * 168);
        namespace cpu = sgcl::detail::cpu;
        if (cpu::crypto()) {
            uint32_t a[5], b[5];
            for (auto& w : a) w = uint32_t(r.g());
            std::memcpy(b, a, sizeof a);
            d::sha1_compress_portable(a, data.data(), blocks);
            d::sha1_compress_arm64(b, data.data(), blocks);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "sha1";
        }
        if (cpu::crypto()) {
            uint32_t a[8], b[8];
            for (auto& w : a) w = uint32_t(r.g());
            std::memcpy(b, a, sizeof a);
            d::sha256_compress_portable(a, data.data(), blocks);
            d::sha256_compress_arm64(b, data.data(), blocks);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "sha256";
        }
        if (cpu::sha512()) {
            uint64_t a[8], b[8];
            for (auto& w : a) w = r.g();
            std::memcpy(b, a, sizeof a);
            d::sha512_compress_portable(a, data.data(), blocks);
            d::sha512_compress_arm64(b, data.data(), blocks);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "sha512";
        }
        if (cpu::sha3()) {
            uint64_t a[25], b[25];
            for (auto& w : a) w = r.g();
            std::memcpy(b, a, sizeof a);
            d::keccak_absorb_portable(a, data.data(), blocks, 168);
            d::keccak_absorb_arm64<168>(b, data.data(), blocks);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "keccak absorb";
            d::keccak_absorb_portable(a, data.data(), blocks, 72);
            d::keccak_absorb_arm64<72>(b, data.data(), blocks);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "keccak absorb 72";
            d::keccak_permute_portable(a);
            d::keccak_permute_arm64(b);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "keccak permute";
        }
    }
}
#endif

// Which path this build takes, in the output: the portable build's run
// says so, the instructions' run says which features it found
TEST(Crypto_Sha, ThePathOfThisBuild) {
#if defined(SGCL_CRYPTO_ARM64)
    namespace cpu = sgcl::detail::cpu;
    std::printf("[ path     ] arm64: crypto (AES, PMULL, SHA-1, SHA-256, CRC32) %d sha512 %d sha3 %d\n", cpu::crypto(), cpu::sha512(), cpu::sha3());
#else
    std::printf("[ path     ] portable\n");
#endif
    SUCCEED();
}

// of_file: the digest of a whole file, what of() of its bytes gives
TEST(Crypto_Sha, OfFile) {
    namespace io = sgcl::io;
    auto dir = io::make_temp_dir({}, "crypto-test-*");
    ASSERT_TRUE(dir);
    random_source r(11);
    bytes_t data = r.bytes(100003);
    sgcl::string path = io::path::join(*dir, "data.bin");
    ASSERT_TRUE(io::write_file(path, view(data)));
    auto d = crypto::sha256::of_file(path);
    ASSERT_TRUE(d);
    EXPECT_EQ(hex(*d), hex(crypto::sha256::of(view(data))));
    auto d3 = crypto::sha3_256::of_file(path);
    ASSERT_TRUE(d3);
    EXPECT_EQ(hex(*d3), hex(crypto::sha3_256::of(view(data))));
    auto t = sgcl::async::spawn([path]() -> sgcl::async::task<std::string> {
        auto a = co_await crypto::sha512::async_of_file(path);
        co_return a ? hex(*a) : std::string("failed");
    });
    EXPECT_EQ(t.wait(), hex(crypto::sha512::of(view(data))));
    sgcl::async::scheduler::stop();
    EXPECT_FALSE(crypto::sha256::of_file(io::path::join(*dir, "missing")));
    io::remove_all(*dir);
}

// digest_file: the digest of a whole file by hash_id, what digest() of its
// bytes gives, for every id; at its edges: an empty file, a read block's
// size and one past it, a missing path, an empty path, a directory, an id
// that is none of the list (thrown by the call, and out of the task's
// co_await), the caller's path gone before the task runs
TEST(Crypto_Sha, DigestFile) {
    namespace io = sgcl::io;
    auto dir = io::make_temp_dir({}, "crypto-test-*");
    ASSERT_TRUE(dir);
    random_source r(12);
    const crypto::hash_id ids[] = {
        crypto::hash_id::sha1,       crypto::hash_id::sha224,   crypto::hash_id::sha256,
        crypto::hash_id::sha384,     crypto::hash_id::sha512,   crypto::hash_id::sha512_256,
        crypto::hash_id::sha3_224,   crypto::hash_id::sha3_256, crypto::hash_id::sha3_384,
        crypto::hash_id::sha3_512,
    };
    sgcl::string path = io::path::join(*dir, "data.bin");
    for (size_t n : {size_t(0), size_t(1), size_t(65536), size_t(65537), size_t(100003)}) {
        bytes_t data = r.bytes(n);
        ASSERT_TRUE(io::write_file(path, view(data)));
        for (auto id : ids) {
            auto d = crypto::digest_file(id, path);
            ASSERT_TRUE(d) << n;
            EXPECT_EQ(d->size(), crypto::digest_size(id));
            EXPECT_EQ(hex(*d), hex(crypto::digest(id, view(data)))) << n;
            auto a = sgcl::async::run(crypto::async_digest_file(id, path));
            ASSERT_TRUE(a) << n;
            EXPECT_EQ(hex(*a), hex(*d)) << n;
        }
    }
    auto missing = crypto::digest_file(crypto::hash_id::sha256, io::path::join(*dir, "missing"));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    auto missing_async = sgcl::async::run(crypto::async_digest_file(crypto::hash_id::sha256, io::path::join(*dir, "missing")));
    ASSERT_FALSE(missing_async);
    EXPECT_TRUE(missing_async.error().is_not_found());
    EXPECT_FALSE(crypto::digest_file(crypto::hash_id::sha256, *dir));   // a directory: its read fails
    EXPECT_FALSE(crypto::digest_file(crypto::hash_id::sha256, ""));
    EXPECT_THROW((void)crypto::digest_file(crypto::hash_id(200), path), std::invalid_argument);
    auto bad = crypto::async_digest_file(crypto::hash_id(200), path);   // the call itself does not throw
    EXPECT_THROW((void)sgcl::async::run(std::move(bad)), std::invalid_argument);
    auto later = [&] {
        sgcl::string gone = io::path::join(*dir, "data.bin");
        return crypto::async_digest_file(crypto::hash_id::sha256, gone);
    }();
    auto kept = sgcl::async::run(std::move(later));
    ASSERT_TRUE(kept);
    EXPECT_EQ(kept->size(), 32u);
    sgcl::async::scheduler::stop();
    io::remove_all(*dir);
}
