//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Argon2 (RFC 9106): the RFC's three vectors (§5), OpenSSL's ARGON2D, ARGON2I
// and ARGON2ID KDFs on random parameters (the lanes on the scheduler's
// workers and on the calling thread, outputs through both forms of H'), the
// reference implementation's PHC strings, and the edges: every option at
// its limit and one past it, a wrong password, a secret, and strings that
// are not Argon2's or ask too much.
#include "digest_common.h"

#include "sgcl/crypto/argon2.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace crypto_test;

namespace {
    const char* argon2_ossl_name(crypto::argon2::variant v) {
        switch (v) {
            case crypto::argon2::variant::d: return "ARGON2D";
            case crypto::argon2::variant::i: return "ARGON2I";
            case crypto::argon2::variant::id: return "ARGON2ID";
        }
        return "";
    }

    bytes_t ossl_argon2(crypto::argon2::variant v, const bytes_t& password, const bytes_t& salt, const bytes_t& secret,
                        const bytes_t& ad, uint32_t m, uint32_t t, uint32_t p, size_t n) {
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, argon2_ossl_name(v), nullptr);
        EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
        uint32_t threads = 1, version = 0x13;
        OSSL_PARAM params[10];
        int i = 0;
        params[i++] = OSSL_PARAM_construct_octet_string("pass", nonnull(password), password.size());
        params[i++] = OSSL_PARAM_construct_octet_string("salt", nonnull(salt), salt.size());
        if (!secret.empty()) {
            params[i++] = OSSL_PARAM_construct_octet_string("secret", nonnull(secret), secret.size());
        }
        if (!ad.empty()) {
            params[i++] = OSSL_PARAM_construct_octet_string("ad", nonnull(ad), ad.size());
        }
        params[i++] = OSSL_PARAM_construct_uint32("iter", &t);
        params[i++] = OSSL_PARAM_construct_uint32("threads", &threads);
        params[i++] = OSSL_PARAM_construct_uint32("lanes", &p);
        params[i++] = OSSL_PARAM_construct_uint32("memcost", &m);
        params[i++] = OSSL_PARAM_construct_uint32("version", &version);
        params[i] = OSSL_PARAM_construct_end();
        bytes_t out(n);
        int ok = EVP_KDF_derive(c, out.data(), n, params);
        EVP_KDF_CTX_free(c);
        EVP_KDF_free(kdf);
        if (ok != 1) {
            throw std::runtime_error("EVP_KDF_derive ARGON2");
        }
        return out;
    }

    sgcl::string phc_of(const std::string& s) {
        return sgcl::string(s.data(), s.size());
    }

    crypto::errc code_of(const sgcl::expected<void, crypto::error>& r) {
        return r ? crypto::errc(0) : r.error().code();
    }
}

TEST(Crypto_Argon2, Rfc9106Vectors) {
    bytes_t password(32, 1), salt(16, 2), secret(8, 3), ad(12, 4);
    const std::pair<crypto::argon2::variant, const char*> vectors[] = {
        {crypto::argon2::variant::d, "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb"},
        {crypto::argon2::variant::i, "c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8"},
        {crypto::argon2::variant::id, "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"},
    };
    for (auto& [variant, expected] : vectors) {
        crypto::argon2::options o{.variant = variant, .memory = 32, .iterations = 3, .parallelism = 4,
                                 .secret = view(secret), .associated_data = view(ad)};
        EXPECT_EQ(hex(crypto::argon2::derive(view(password), view(salt), 32, o)), expected);
        bytes_t out(32);
        crypto::argon2::derive_to(out_view(out), view(password), view(salt), o);
        EXPECT_EQ(hex(out), expected);
    }
}

TEST(Crypto_Argon2, AgainstOpenSsl) {
    random_source r(9106);
    const crypto::argon2::variant variants[] = {crypto::argon2::variant::d, crypto::argon2::variant::i,
                                               crypto::argon2::variant::id};
    for (int k = 0; k < 24; ++k) {
        crypto::argon2::variant v = variants[k % 3];
        uint32_t p = 1 + uint32_t(r.below(5));
        uint32_t m = 8 * p + uint32_t(r.below(300));
        uint32_t t = 1 + uint32_t(r.below(3));
        // tags through H' of one hash, several, and a last piece of 1..64 bytes
        const size_t sizes[] = {4, 16, 32, 64, 65, 96, 127, 200, 1024};
        size_t n = sizes[k % 9];
        bytes_t password = r.bytes(r.below(40)), salt = r.bytes(8 + r.below(30));
        bytes_t secret = k % 4 == 0 ? r.bytes(1 + r.below(20)) : bytes_t();
        bytes_t ad = k % 5 == 0 ? r.bytes(1 + r.below(20)) : bytes_t();
        crypto::argon2::options o{.variant = v, .memory = m, .iterations = t, .parallelism = p,
                                 .secret = view(secret), .associated_data = view(ad)};
        EXPECT_EQ(hex(crypto::argon2::derive(view(password), view(salt), n, o)), hex(ossl_argon2(v, password, salt, secret, ad, m, t, p, n)))
            << k << " m=" << m << " t=" << t << " p=" << p << " n=" << n;
    }
}

TEST(Crypto_Argon2, LanesOnTheWorkers) {
    // segments of 64 blocks and more: the lanes run through parallel_for
    bytes_t password = text("correct horse battery staple"), salt = text("0123456789abcdef");
    for (uint32_t p : {2u, 4u, 7u}) {
        uint32_t m = 4 * 64 * p + 100;
        for (auto v : {crypto::argon2::variant::i, crypto::argon2::variant::id}) {
            EXPECT_EQ(hex(crypto::argon2::derive(view(password), view(salt), 32, {.variant = v, .memory = m, .iterations = 2, .parallelism = p})),
                      hex(ossl_argon2(v, password, salt, {}, {}, m, 2, p, 32)))
                << p;
        }
    }
}

TEST(Crypto_Argon2, LimitsOfTheOptions) {
    bytes_t salt8(8, 1), salt7(7, 1);
    EXPECT_NO_THROW(crypto::argon2::derive("pw", view(salt8), 4, {.memory = 8, .iterations = 1, .parallelism = 1}));
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt7), 32, {.memory = 8, .iterations = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt8), 3, {.memory = 8, .iterations = 1, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 8, .iterations = 0, .parallelism = 1}), std::invalid_argument);
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 8, .iterations = 1, .parallelism = 0}), std::invalid_argument);
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 15, .iterations = 1, .parallelism = 2}), std::invalid_argument);
    EXPECT_THROW(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 1u << 30, .iterations = 1, .parallelism = 1u << 24}), std::invalid_argument);
    bytes_t out3(3);
    EXPECT_THROW(crypto::argon2::derive_to(out_view(out3), "pw", view(salt8), {.memory = 8, .iterations = 1, .parallelism = 1}), std::invalid_argument);
    // 8 KiB a lane at the least, a memory not a multiple of 4p rounded down
    EXPECT_EQ(hex(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 16, .iterations = 1, .parallelism = 2})),
              hex(ossl_argon2(crypto::argon2::variant::id, text("pw"), salt8, {}, {}, 16, 1, 2, 32)));
    EXPECT_EQ(hex(crypto::argon2::derive("pw", view(salt8), 32, {.memory = 37, .iterations = 1, .parallelism = 3})),
              hex(ossl_argon2(crypto::argon2::variant::id, text("pw"), salt8, {}, {}, 37, 1, 3, 32)));
    // an empty password
    EXPECT_EQ(hex(crypto::argon2::derive("", view(salt8), 32, {.memory = 8, .iterations = 1, .parallelism = 1})),
              hex(ossl_argon2(crypto::argon2::variant::id, {}, salt8, {}, {}, 8, 1, 1, 32)));
    // the output over the password: hashed whole before a byte is written
    bytes_t buf = text("password and more");
    bytes_t expected = ossl_argon2(crypto::argon2::variant::id, buf, salt8, {}, {}, 8, 1, 1, buf.size());
    crypto::argon2::derive_to(out_view(buf), view(buf), view(salt8), {.memory = 8, .iterations = 1, .parallelism = 1});
    EXPECT_EQ(hex(buf), hex(expected));
}

TEST(Crypto_Argon2, PhcGenerateAndVerify) {
    crypto::argon2::options o{.memory = 1024, .iterations = 2, .parallelism = 2};
    sgcl::string phc = crypto::argon2::generate("correct horse battery staple", o);
    std::string s(phc.data(), phc.size());
    EXPECT_EQ(s.substr(0, 30), "$argon2id$v=19$m=1024,t=2,p=2$");
    EXPECT_EQ(s.size(), 30u + 22 + 1 + 43);
    EXPECT_TRUE(crypto::argon2::verify("correct horse battery staple", phc));
    EXPECT_EQ(code_of(crypto::argon2::verify("correct horse battery stapl", phc)), crypto::errc::authentication);
    EXPECT_EQ(code_of(crypto::argon2::verify("", phc)), crypto::errc::authentication);
    // two strings of one password differ by their salt
    EXPECT_NE(std::string(crypto::argon2::generate("x", o).data(), 40), std::string(crypto::argon2::generate("x", o).data(), 40));
    // a secret: needed again at verify
    bytes_t pepper(32, 7);
    crypto::argon2::options peppered = o;
    peppered.secret = view(pepper);
    sgcl::string with = crypto::argon2::generate("pw", peppered);
    EXPECT_TRUE(crypto::argon2::verify("pw", with, view(pepper)));
    EXPECT_EQ(code_of(crypto::argon2::verify("pw", with)), crypto::errc::authentication);
    // associated data has no place in the string
    bytes_t ad(4, 1);
    crypto::argon2::options with_ad = o;
    with_ad.associated_data = view(ad);
    EXPECT_THROW(crypto::argon2::generate("pw", with_ad), std::invalid_argument);
    // the other variants
    for (auto v : {crypto::argon2::variant::i, crypto::argon2::variant::d}) {
        crypto::argon2::options ov = o;
        ov.variant = v;
        sgcl::string h = crypto::argon2::generate("pw", ov);
        EXPECT_TRUE(crypto::argon2::verify("pw", h));
        EXPECT_EQ(std::string(h.data(), 8), v == crypto::argon2::variant::i ? "$argon2i" : "$argon2d");
    }
}

TEST(Crypto_Argon2, PhcStringsOfOthers) {
    // the reference implementation's (and Python's argon2-cffi's) strings
    EXPECT_TRUE(crypto::argon2::verify("password", "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc"));
    // made here from OpenSSL's tag: argon2i and argon2d, a salt and a hash of other lengths
    bytes_t salt = text("a longer salt of 28 bytes!!!");
    for (auto v : {crypto::argon2::variant::i, crypto::argon2::variant::d}) {
        bytes_t tag = ossl_argon2(v, text("secret pw"), salt, {}, {}, 256, 3, 2, 50);
        std::string s = std::string(v == crypto::argon2::variant::i ? "$argon2i" : "$argon2d") + "$v=19$m=256,t=3,p=2$";
        sgcl::string b1 = sgcl::encoding::base64::raw_standard.encode(view(salt));
        sgcl::string b2 = sgcl::encoding::base64::raw_standard.encode(view(tag));
        s += std::string(b1.data(), b1.size()) + "$" + std::string(b2.data(), b2.size());
        EXPECT_TRUE(crypto::argon2::verify("secret pw", phc_of(s))) << s;
        EXPECT_EQ(code_of(crypto::argon2::verify("secret pX", phc_of(s))), crypto::errc::authentication);
    }
}

TEST(Crypto_Argon2, PhcStringsRefused) {
    const std::string good = "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc";
    const std::pair<std::string, crypto::errc> cases[] = {
        {"", crypto::errc::malformed},
        {"$", crypto::errc::malformed},
        {"argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2x$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=16$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$v=019$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=065536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$t=2,m=65536,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1,keyid=x$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc$", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ=$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29t!XNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$c2FsdA$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},   // a salt of 4 bytes
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$AQID", crypto::errc::malformed},   // a hash of 3 bytes
        {"$argon2id$v=19$m=65536,t=0,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=0$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=7,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=4194305,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$v=19$m=65536,t=65537,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$v=19$m=65536,t=2,p=256$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$v=19$m=99999999999,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::malformed},
        {"$argon2id$v=19$m=65536,t=2,p=1$" + std::string(90, 'A') + "$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc", crypto::errc::unsupported},
        {"$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$" + std::string(90, 'A'), crypto::errc::unsupported},
    };
    for (auto& [s, code] : cases) {
        EXPECT_EQ(code_of(crypto::argon2::verify("password", phc_of(s))), code) << s;
    }
    EXPECT_TRUE(crypto::argon2::verify("password", phc_of(good)));
}
