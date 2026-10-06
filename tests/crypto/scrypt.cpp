//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// scrypt (RFC 7914): the RFC's vectors of §12 (all but the one of 1 GiB),
// its Salsa20/8 core vector (§8), OpenSSL's EVP_PBE_scrypt on random costs
// and lengths, and the edges: every cost at its limit and one past it, an
// empty password and salt, an output over the password and over the salt.
#include "digest_common.h"

#include "sgcl/crypto/scrypt.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace crypto_test;

namespace {
    bytes_t ossl_scrypt(const bytes_t& password, const bytes_t& salt, uint64_t n, uint64_t r, uint64_t p, size_t len) {
        bytes_t out(len);
        if (EVP_PBE_scrypt(reinterpret_cast<const char*>(nonnull(password)), password.size(), nonnull(salt), salt.size(), n, r, p,
                           uint64_t(1) << 40, out.data(), len) != 1) {
            throw std::runtime_error("EVP_PBE_scrypt");
        }
        return out;
    }
}

TEST(Crypto_Scrypt, Rfc7914Vectors) {
    EXPECT_EQ(hex(crypto::scrypt::derive("", "", 64, {.cost = 16, .block_size = 1, .parallelism = 1})),
              "77d6576238657b203b19ca42c18a0497f16b4844e3074ae8dfdffa3fede21442"
              "fcd0069ded0948f8326a753a0fc81f17e8d3e0fb2e0d3628cf35e20c38d18906");
    EXPECT_EQ(hex(crypto::scrypt::derive("password", "NaCl", 64, {.cost = 1024, .block_size = 8, .parallelism = 16})),
              "fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b373162"
              "2eaf30d92e22a3886ff109279d9830dac727afb94a83ee6d8360cbdfa2cc0640");
    EXPECT_EQ(hex(crypto::scrypt::derive("pleaseletmein", "SodiumChloride", 64, {.cost = 16384, .block_size = 8, .parallelism = 1})),
              "7023bdcb3afd7348461c06cd81fd38ebfda8fbba904f8e3ea9b543f6545da1f2"
              "d5432955613f0fcf62d49705242a9af9e61e85dc0d651e40dfcf017b45575887");
}

TEST(Crypto_Scrypt, Salsa20Core) {
    // §8: the input and output of Salsa20/8, as one XOR of the input with zeros
    bytes_t in = unhex("7e879a214f3ec9867ca940e641718f26baee555b8c61c1b50df846116dcd3b1d"
                       "ee24f319df9b3d8514121e4b5ac5aa3276021d2909c74829edebc68db8b8c25e");
    uint32_t x[16], zero[16] = {};
    for (int i = 0; i < 16; ++i) {
        x[i] = crypto::detail::load_le32(in.data() + 4 * i);
    }
    crypto::detail::salsa20_8_xor(x, zero);
    unsigned char out[64];
    for (int i = 0; i < 16; ++i) {
        crypto::detail::store_le32(out + 4 * i, x[i]);
    }
    EXPECT_EQ(hex(out, 64), "a41f859c6608cc993b81cacb020cef05044b2181a2fd337dfd7b1c6396682f29"
                            "b4393168e3c9e6bcfe6bc5b7a06d96bae424cc102c91745c24ad673dc7618f81");
}

TEST(Crypto_Scrypt, AgainstOpenSsl) {
    random_source g(7914);
    for (int k = 0; k < 20; ++k) {
        uint32_t n = 2u << g.below(10);
        uint32_t r = 1 + uint32_t(g.below(9));
        uint32_t p = 1 + uint32_t(g.below(4));
        size_t len = 1 + g.below(150);
        bytes_t password = g.bytes(g.below(70)), salt = g.bytes(g.below(40));
        EXPECT_EQ(hex(crypto::scrypt::derive(view(password), view(salt), len, {.cost = n, .block_size = r, .parallelism = p})),
                  hex(ossl_scrypt(password, salt, n, r, p, len)))
            << "N=" << n << " r=" << r << " p=" << p << " len=" << len;
    }
}

TEST(Crypto_Scrypt, Limits) {
    EXPECT_NO_THROW(crypto::scrypt::derive("pw", "salt", 1, {.cost = 2, .block_size = 1, .parallelism = 1}));
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 1, .block_size = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 0, .block_size = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 3, .block_size = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 1000, .block_size = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 16, .block_size = 0, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 16, .block_size = 1, .parallelism = 0}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 16, .block_size = 1u << 15, .parallelism = 1u << 15}), std::invalid_argument);
    EXPECT_THROW(crypto::scrypt::derive("pw", "salt", 32, {.cost = 1u << 31, .block_size = 1u << 29, .parallelism = 1}), std::invalid_argument);
    bytes_t none;
    crypto::scrypt::derive_to(out_view(none), "pw", "salt", {.cost = 2, .block_size = 1, .parallelism = 1});
    EXPECT_EQ(crypto::scrypt::derive("pw", "salt", 0, {.cost = 2, .block_size = 1, .parallelism = 1}).size(), 0u);
    // the output over the password: read whole first
    bytes_t buf = text("password!");
    bytes_t expected = ossl_scrypt(buf, text("salt"), 16, 2, 1, buf.size());
    crypto::scrypt::derive_to(out_view(buf), view(buf), "salt", {.cost = 16, .block_size = 2, .parallelism = 1});
    EXPECT_EQ(hex(buf), hex(expected));
    // over the salt: refused, nothing written
    bytes_t salted = text("saltsalt");
    EXPECT_THROW(crypto::scrypt::derive_to(out_view(salted), "pw", view(salted), {.cost = 16, .block_size = 1, .parallelism = 1}),
                 std::invalid_argument);
    EXPECT_EQ(std::string(salted.begin(), salted.end()), "saltsalt");
}
