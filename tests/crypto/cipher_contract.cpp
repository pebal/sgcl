//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the ciphers promise besides their output: a forged or damaged
// message is errc::authentication and not one byte of its plaintext is
// written (the output zeroed, in place too); a broken contract is an
// exception of its kind; keys are move-only, zeroed when destroyed and when
// moved from, and unusable after a move; the nonce counter never repeats.
#include "cipher_test.h"

#include <new>
#include <type_traits>

using namespace cipher_test;

static_assert(!std::is_copy_constructible_v<crypto::aes>);
static_assert(!std::is_copy_constructible_v<crypto::aes_gcm>);
static_assert(!std::is_copy_constructible_v<crypto::aes_ctr>);
static_assert(!std::is_copy_constructible_v<crypto::chacha20>);
static_assert(!std::is_copy_constructible_v<crypto::chacha20_poly1305>);
static_assert(!std::is_copy_constructible_v<crypto::xchacha20_poly1305>);
static_assert(!std::is_copy_constructible_v<crypto::nonce_counter>);
static_assert(!std::is_copy_assignable_v<crypto::aes_gcm>);
static_assert(!std::is_copy_assignable_v<crypto::chacha20_poly1305>);
static_assert(std::is_nothrow_move_constructible_v<crypto::aes_gcm>);
static_assert(std::is_nothrow_move_constructible_v<crypto::chacha20_poly1305>);
static_assert(std::is_nothrow_move_constructible_v<crypto::xchacha20_poly1305>);

namespace {
    // Every way of damaging a sealed message: each must fail to open,
    // through open and through open_to (into a buffer and in place), with
    // the bytes open_to would have written zeroed and nothing after them
    // touched; and OpenSSL, where it has the cipher, must agree
    template<class Aead>
    void rejects_damage(size_t key_size, const EVP_CIPHER* oracle, uint64_t seed) {
        std::mt19937_64 rng(seed);
        for (size_t n : {size_t(0), size_t(1), size_t(15), size_t(16), size_t(17), size_t(100), size_t(128), size_t(1000), size_t(5000)}) {
            SCOPED_TRACE(n);
            bytes key = random_bytes(rng, key_size), nonce = random_bytes(rng, Aead::nonce_size);
            bytes pt = random_bytes(rng, n), aad = random_bytes(rng, 1 + rng() % 40);
            Aead a(key);
            auto sealed_v = a.seal(nonce, pt, aad);
            bytes sealed(sealed_v.begin(), sealed_v.end());
            struct Case {
                std::string what;
                bytes nonce, sealed, aad;
            };
            std::vector<Case> cases;
            for (int i = 0; i < 8; ++i) {
                bytes s = sealed;
                size_t bit = n * 8 + rng() % 128;   // in the tag
                s[bit / 8] ^= std::byte(1u << (bit % 8));
                cases.push_back({"tag bit", nonce, s, aad});
            }
            if (n > 0) {
                for (int i = 0; i < 4; ++i) {
                    bytes s = sealed;
                    size_t bit = rng() % (n * 8);   // in the ciphertext
                    s[bit / 8] ^= std::byte(1u << (bit % 8));
                    cases.push_back({"ciphertext bit", nonce, s, aad});
                }
            }
            for (size_t cut : {size_t(1), size_t(2), size_t(15), size_t(16)}) {
                if (cut <= sealed.size()) {
                    cases.push_back({"truncated", nonce, bytes(sealed.begin(), sealed.end() - ptrdiff_t(cut)), aad});
                }
            }
            cases.push_back({"extended", nonce, concat(sealed, bytes(1)), aad});
            {
                bytes d = aad;
                d[rng() % d.size()] ^= std::byte(0x80);
                cases.push_back({"aad bit", nonce, sealed, d});
                cases.push_back({"aad longer", nonce, sealed, concat(aad, bytes(1))});
                cases.push_back({"aad shorter", nonce, sealed, bytes(aad.begin(), aad.end() - 1)});
                cases.push_back({"aad missing", nonce, sealed, bytes()});
            }
            {
                bytes other = nonce;
                other[rng() % other.size()] ^= std::byte(1);
                cases.push_back({"another nonce", other, sealed, aad});
            }
            {
                bytes other_key = key;
                other_key[0] ^= std::byte(1);
                Aead b(other_key);
                auto r = b.open(nonce, sealed, aad);
                ASSERT_FALSE(r.has_value());
                EXPECT_EQ(r.error().code(), crypto::errc::authentication);
            }
            for (auto& c : cases) {
                SCOPED_TRACE(c.what);
                auto r = a.open(c.nonce, c.sealed, c.aad);
                ASSERT_FALSE(r.has_value());
                EXPECT_EQ(r.error().code(), crypto::errc::authentication);
                EXPECT_EQ(r.error().message(), "message authentication failed");
                if (c.sealed.size() < 16) {
                    continue;
                }
                const size_t m = c.sealed.size() - 16;
                bytes out(m + 8, std::byte(0xaa));
                auto t = a.open_to(out, c.nonce, c.sealed, c.aad);
                ASSERT_FALSE(t.has_value());
                EXPECT_EQ(t.error().code(), crypto::errc::authentication);
                EXPECT_TRUE(all_zero(out.data(), m));
                for (size_t i = m; i < out.size(); ++i) {
                    ASSERT_EQ(out[i], std::byte(0xaa));
                }
                bytes in_place = c.sealed;
                auto u = a.open_to(in_place, c.nonce, in_place, c.aad);
                ASSERT_FALSE(u.has_value());
                EXPECT_TRUE(all_zero(in_place.data(), m));
                if (oracle) {
                    bytes back;
                    EXPECT_FALSE(ossl_open(oracle, key, c.nonce, c.sealed, c.aad, back));
                }
            }
        }
    }

    // A key's bytes nowhere in the memory an object occupied, after its
    // destructor or after a move out of it: any 4-byte window of the key
    // would be found
    bool holds_key(const unsigned char* p, size_t size, const bytes& key) {
        for (size_t i = 0; i + 4 <= key.size(); ++i) {
            for (size_t j = 0; j + 4 <= size; ++j) {
                if (std::memcmp(p + j, u8(key) + i, 4) == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    size_t nonzero(const unsigned char* p, size_t size) {
        size_t n = 0;
        for (size_t i = 0; i < size; ++i) {
            n += p[i] != 0;
        }
        return n;
    }

    template<class T, class... A>
    void zeroed_after_destructor(const bytes& key, A&&... a) {
        alignas(T) unsigned char storage[sizeof(T)];
        T* t = new (storage) T(key, std::forward<A>(a)...);
        ASSERT_TRUE(holds_key(storage, sizeof storage, key) || nonzero(storage, sizeof storage) > 64);
        t->~T();
        // read through a volatile pointer: the memory is the test's own
        const volatile unsigned char* v = storage;
        unsigned char copy[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); ++i) {
            copy[i] = v[i];
        }
        EXPECT_FALSE(holds_key(copy, sizeof copy, key));
        EXPECT_LE(nonzero(copy, sizeof copy), 16u);   // flags and positions, never key material
    }

    template<class T, class... A>
    void zeroed_after_move(const bytes& key, A&&... a) {
        T from(key, std::forward<A>(a)...);
        T to(std::move(from));
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&from);
        EXPECT_FALSE(holds_key(p, sizeof(T), key));
        EXPECT_LE(nonzero(p, sizeof(T)), 16u);
    }
}

#if SGCL_TEST_OPENSSL
TEST(CryptoContract_Tests, AesGcmRejectsDamage) {
    rejects_damage<crypto::aes_gcm>(16, EVP_aes_128_gcm(), 21);
    rejects_damage<crypto::aes_gcm>(32, EVP_aes_256_gcm(), 22);
}

TEST(CryptoContract_Tests, ChachaPolyRejectsDamage) {
    rejects_damage<crypto::chacha20_poly1305>(32, EVP_chacha20_poly1305(), 23);
}
#else
TEST(CryptoContract_Tests, AesGcmRejectsDamage) {
    rejects_damage<crypto::aes_gcm>(16, no_oracle(), 21);
    rejects_damage<crypto::aes_gcm>(32, no_oracle(), 22);
}

TEST(CryptoContract_Tests, ChachaPolyRejectsDamage) {
    rejects_damage<crypto::chacha20_poly1305>(32, no_oracle(), 23);
}
#endif

TEST(CryptoContract_Tests, XChachaPolyRejectsDamage) {
    rejects_damage<crypto::xchacha20_poly1305>(32, nullptr, 24);
}

TEST(CryptoContract_Tests, ShorterThanATagIsAuthentication) {
    crypto::aes_gcm g(bytes(16));
    crypto::chacha20_poly1305 c(bytes(32));
    bytes nonce(12);
    for (size_t n = 0; n < 16; ++n) {
        bytes s(n);
        EXPECT_EQ(error_of(g.open(nonce, s)).code(), crypto::errc::authentication);
        EXPECT_EQ(error_of(c.open(nonce, s)).code(), crypto::errc::authentication);
        bytes out(4);
        EXPECT_FALSE(g.open_to(out, nonce, s).has_value());
    }
}

TEST(CryptoContract_Tests, WrongKeyLengthsThrowOrFail) {
    for (size_t n : {size_t(0), size_t(1), size_t(15), size_t(17), size_t(31), size_t(33), size_t(64)}) {
        bytes key(n);
        EXPECT_THROW(crypto::aes{key}, std::invalid_argument);
        EXPECT_THROW(crypto::aes_gcm{key}, std::invalid_argument);
        EXPECT_THROW((crypto::aes_ctr{key, bytes(16)}), std::invalid_argument);
        auto r = crypto::aes_gcm::from_key(key);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), crypto::errc::invalid_key);
        EXPECT_EQ(r.error().message(), "aes_gcm: a key of " + std::to_string(n) + " bytes");
        EXPECT_FALSE(crypto::aes::from_key(key).has_value());
        EXPECT_FALSE(crypto::aes_ctr::from_key(key, bytes(16)).has_value());
    }
    for (size_t n : {size_t(0), size_t(16), size_t(24), size_t(31), size_t(33)}) {
        bytes key(n);
        EXPECT_THROW(crypto::chacha20_poly1305{key}, std::invalid_argument);
        EXPECT_THROW(crypto::xchacha20_poly1305{key}, std::invalid_argument);
        EXPECT_THROW((crypto::chacha20{key, bytes(12)}), std::invalid_argument);
        EXPECT_EQ(error_of(crypto::chacha20_poly1305::from_key(key)).code(), crypto::errc::invalid_key);
        EXPECT_EQ(error_of(crypto::xchacha20_poly1305::from_key(key)).code(), crypto::errc::invalid_key);
        EXPECT_FALSE(crypto::chacha20::from_key(key, bytes(12)).has_value());
    }
    for (size_t n : {size_t(16), size_t(24), size_t(32)}) {
        EXPECT_TRUE(crypto::aes_gcm::from_key(bytes(n)).has_value());
        EXPECT_EQ(value_of(crypto::aes_gcm::from_key(bytes(n))).key_size(), n);
    }
    EXPECT_TRUE(crypto::chacha20_poly1305::from_key(bytes(32)).has_value());
}

TEST(CryptoContract_Tests, WrongNonceLengthsThrow) {
    crypto::aes_gcm g(bytes(16));
    crypto::chacha20_poly1305 c(bytes(32));
    crypto::xchacha20_poly1305 x(bytes(32));
    bytes pt(10), out(100);
    for (size_t n : {size_t(0), size_t(8), size_t(11), size_t(13), size_t(16), size_t(23), size_t(25)}) {
        bytes nonce(n);
        EXPECT_THROW(g.seal(nonce, pt), std::invalid_argument);
        EXPECT_THROW((void)g.open(nonce, bytes(40)), std::invalid_argument);
        EXPECT_THROW(g.seal_to(out, nonce, pt), std::invalid_argument);
        EXPECT_THROW((void)g.open_to(out, nonce, bytes(40)), std::invalid_argument);
        EXPECT_THROW(c.seal(nonce, pt), std::invalid_argument);
        EXPECT_THROW((void)c.open(nonce, bytes(40)), std::invalid_argument);
        EXPECT_THROW(x.seal(nonce, pt), std::invalid_argument);
        EXPECT_THROW((void)x.open(nonce, bytes(40)), std::invalid_argument);
    }
    EXPECT_THROW(x.seal(bytes(12), pt), std::invalid_argument);
    EXPECT_THROW((crypto::chacha20{bytes(32), bytes(16)}), std::invalid_argument);
    EXPECT_THROW((crypto::chacha20{bytes(32), bytes(8)}), std::invalid_argument);
    EXPECT_THROW((crypto::aes_ctr{bytes(16), bytes(12)}), std::invalid_argument);
    EXPECT_THROW((void)crypto::chacha20::from_key(bytes(32), bytes(16)), std::invalid_argument);
}

TEST(CryptoContract_Tests, OutputTooSmallOrOverlappingThrows) {
    crypto::aes_gcm g(bytes(16));
    bytes nonce(12), pt(40);
    bytes small(55);
    EXPECT_THROW(g.seal_to(small, nonce, pt), std::length_error);
    auto sealed = g.seal(nonce, pt);
    bytes small2(39);
    EXPECT_THROW((void)g.open_to(small2, nonce, sealed), std::length_error);
    // shifted by a byte either way: not in place and not apart
    bytes buf(100);
    EXPECT_THROW(g.seal_to(slice<std::byte>(buf.data() + 1, 60), nonce, slice<const std::byte>(buf.data(), 40)), std::invalid_argument);
    EXPECT_THROW(g.seal_to(slice<std::byte>(buf.data(), 60), nonce, slice<const std::byte>(buf.data() + 1, 40)), std::invalid_argument);
    std::memcpy(buf.data() + 1, sealed.data(), sealed.size());
    EXPECT_THROW((void)g.open_to(slice<std::byte>(buf.data(), 40), nonce, slice<const std::byte>(buf.data() + 1, 56)), std::invalid_argument);
    // exactly in place and exactly apart are fine
    EXPECT_EQ(g.seal_to(slice<std::byte>(buf.data(), 56), nonce, slice<const std::byte>(buf.data(), 40)), 56u);
    EXPECT_EQ(g.seal_to(slice<std::byte>(buf.data() + 40, 56), nonce, slice<const std::byte>(buf.data(), 40)), 56u);
    // the streams
    crypto::aes_ctr ctr(bytes(16), bytes(16));
    crypto::chacha20 ch(bytes(32), bytes(12));
    bytes in(20), out(19);
    EXPECT_THROW(ctr.xor_key_stream(out, in), std::length_error);
    EXPECT_THROW(ch.xor_key_stream(out, in), std::length_error);
    EXPECT_THROW(ch.xor_key_stream(slice<std::byte>(buf.data() + 1, 20), slice<const std::byte>(buf.data(), 20)), std::invalid_argument);
    EXPECT_THROW(ctr.xor_key_stream(slice<std::byte>(buf.data(), 20), slice<const std::byte>(buf.data() + 3, 20)), std::invalid_argument);
}

// 2^32 blocks of keystream, then std::length_error, as Go panics
TEST(CryptoContract_Tests, ChachaKeystreamEnds) {
    crypto::chacha20 c(bytes(32), bytes(12));
    c.seek(0xffffffffu);
    bytes in(64), out(64);
    c.xor_key_stream(out, in);   // the last block
    bytes one(1);
    EXPECT_THROW(c.xor_key_stream(one, one), std::length_error);
    c.seek(0xffffffffu);
    bytes big(65);
    EXPECT_THROW(c.xor_key_stream(big, big), std::length_error);
    c.seek(0xfffffffeu);
    c.xor_key_stream(big, big);   // two blocks: the last one begun
    bytes rest(63);
    c.xor_key_stream(rest, rest);   // the rest of the last block
    EXPECT_THROW(c.xor_key_stream(one, one), std::length_error);
}

TEST(CryptoContract_Tests, MovedFromIsUnusableAndMovedToWorks) {
    bytes key(32, std::byte(7)), nonce(12);
    crypto::aes_gcm g(key);
    auto want = g.seal(nonce, bytes(33));
    crypto::aes_gcm h(std::move(g));
    EXPECT_EQ(to_hex(h.seal(nonce, bytes(33))), to_hex(want));
    EXPECT_THROW(g.seal(nonce, bytes(33)), std::logic_error);
    EXPECT_THROW((void)g.open(nonce, want), std::logic_error);
    EXPECT_THROW((void)g.clone(), std::logic_error);
    EXPECT_EQ(g.key_size(), 0u);
    // assigned back, it works again
    g = std::move(h);
    EXPECT_EQ(to_hex(g.seal(nonce, bytes(33))), to_hex(want));
    EXPECT_THROW(h.seal(nonce, bytes(33)), std::logic_error);
    // a clone is its own key
    auto k = g.clone();
    EXPECT_EQ(to_hex(k.seal(nonce, bytes(33))), to_hex(want));

    crypto::chacha20_poly1305 c(key);
    auto cw = c.seal(nonce, bytes(5));
    crypto::chacha20_poly1305 d = std::move(c);
    EXPECT_THROW(c.seal(nonce, bytes(5)), std::logic_error);
    EXPECT_EQ(to_hex(d.clone().seal(nonce, bytes(5))), to_hex(cw));

    crypto::xchacha20_poly1305 x(key);
    crypto::xchacha20_poly1305 y = std::move(x);
    EXPECT_THROW(x.seal_random(bytes(5)), std::logic_error);
    EXPECT_TRUE(y.open_random(y.seal_random(bytes(5))).has_value());

    crypto::aes a(bytes(16));
    crypto::aes b = std::move(a);
    EXPECT_THROW(a.encrypt_block({}), std::logic_error);
    EXPECT_EQ(to_hex(b.clone().encrypt_block({})), to_hex(b.encrypt_block({})));

    crypto::chacha20 s(key, nonce);
    bytes o1(10), o2(10);
    s.xor_key_stream(o1, bytes(10));
    auto t = s.clone();   // the clone continues where s is
    bytes o3(10);
    s.xor_key_stream(o2, bytes(10));
    t.xor_key_stream(o3, bytes(10));
    EXPECT_EQ(o2, o3);
    crypto::chacha20 u = std::move(s);
    EXPECT_THROW(s.seek(0), std::logic_error);

    crypto::aes_ctr r(bytes(16), bytes(16));
    crypto::aes_ctr q = std::move(r);
    EXPECT_THROW(r.xor_key_stream(o1, o1), std::logic_error);
    q.xor_key_stream(o1, o1);
}

TEST(CryptoContract_Tests, KeysAreZeroedByTheDestructor) {
    std::mt19937_64 rng(31);
    bytes k16 = random_bytes(rng, 16), k32 = random_bytes(rng, 32);
    zeroed_after_destructor<crypto::aes>(k16);
    zeroed_after_destructor<crypto::aes>(k32);
    zeroed_after_destructor<crypto::aes_gcm>(k16);
    zeroed_after_destructor<crypto::aes_gcm>(k32);
    zeroed_after_destructor<crypto::aes_ctr>(k16, bytes(16));
    zeroed_after_destructor<crypto::chacha20>(k32, bytes(12));
    zeroed_after_destructor<crypto::chacha20_poly1305>(k32);
    zeroed_after_destructor<crypto::xchacha20_poly1305>(k32);
}

TEST(CryptoContract_Tests, KeysAreZeroedInTheObjectMovedFrom) {
    std::mt19937_64 rng(32);
    bytes k16 = random_bytes(rng, 16), k32 = random_bytes(rng, 32);
    zeroed_after_move<crypto::aes>(k32);
    zeroed_after_move<crypto::aes_gcm>(k16);
    zeroed_after_move<crypto::aes_ctr>(k16, bytes(16));
    zeroed_after_move<crypto::chacha20>(k32, bytes(24));
    zeroed_after_move<crypto::chacha20_poly1305>(k32);
    zeroed_after_move<crypto::xchacha20_poly1305>(k32);
}

TEST(CryptoContract_Tests, SealRandomDrawsAFreshNonce) {
    std::mt19937_64 rng(33);
    bytes key = random_bytes(rng, 32);
    crypto::xchacha20_poly1305 x(key);
    for (size_t n : {size_t(0), size_t(1), size_t(100), size_t(5000)}) {
        bytes pt = random_bytes(rng, n), aad = random_bytes(rng, 9);
        auto a = x.seal_random(pt, aad);
        auto b = x.seal_random(pt, aad);
        ASSERT_EQ(a.size(), n + 24 + 16);
        EXPECT_NE(to_hex(a.data(), 24), to_hex(b.data(), 24));
        // the same as seal with the nonce it drew
        auto again = x.seal(slice<const std::byte>(a.data(), 24), pt, aad);
        EXPECT_EQ(to_hex(again), to_hex(a.data() + 24, a.size() - 24));
        auto opened = x.open_random(a, aad);
        ASSERT_TRUE(opened.has_value());
        EXPECT_EQ(to_hex(*opened), to_hex(pt));
        EXPECT_FALSE(x.open_random(a).has_value());
        bytes damaged(a.begin(), a.end());
        damaged[3] ^= std::byte(1);   // in the nonce
        EXPECT_EQ(error_of(x.open_random(damaged, aad)).code(), crypto::errc::authentication);
    }
    for (size_t n = 0; n < 40; ++n) {
        EXPECT_EQ(error_of(x.open_random(bytes(n))).code(), crypto::errc::authentication);
    }
}

TEST(CryptoContract_Tests, NonceCounterNeverRepeats) {
    crypto::nonce_counter c;
    EXPECT_EQ(to_hex(c.next()), "000000000000000000000000");
    EXPECT_EQ(to_hex(c.next()), "000000000000000000000001");
    sgcl::array<std::byte, 12> start;
    std::memcpy(start.data(), hex("00000000ffffffffffffffff").data(), 12);
    crypto::nonce_counter d(start);
    EXPECT_EQ(to_hex(d.next()), "00000000ffffffffffffffff");
    EXPECT_EQ(to_hex(d.next()), "000000010000000000000000");
    std::memcpy(start.data(), hex("fffffffffffffffffffffffe").data(), 12);
    crypto::nonce_counter e(start);
    EXPECT_EQ(to_hex(e.next()), "fffffffffffffffffffffffe");
    EXPECT_EQ(to_hex(e.next()), "ffffffffffffffffffffffff");
    EXPECT_THROW(e.next(), std::out_of_range);
    EXPECT_THROW(e.next(), std::out_of_range);
    // moved from: spent, so that no two counters hand out the same nonce
    crypto::nonce_counter f = std::move(c);
    EXPECT_EQ(to_hex(f.next()), "000000000000000000000002");
    EXPECT_THROW(c.next(), std::out_of_range);
    // with a cipher
    crypto::aes_gcm g(bytes(16));
    auto n1 = f.next(), n2 = f.next();
    EXPECT_NE(to_hex(g.seal(n1, bytes(4))), to_hex(g.seal(n2, bytes(4))));
}

TEST(CryptoContract_Tests, ConstantTimeEqualAndSecureZero) {
    bytes a = hex("00112233445566778899aabbccddeeff00"), b = a;
    EXPECT_TRUE(crypto::constant_time::equal(a, b));
    for (size_t i = 0; i < a.size(); ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            bytes c = a;
            c[i] ^= std::byte(1 << bit);
            EXPECT_FALSE(crypto::constant_time::equal(a, c));
        }
    }
    EXPECT_FALSE(crypto::constant_time::equal(a, bytes(a.begin(), a.end() - 1)));
    EXPECT_TRUE(crypto::constant_time::equal(bytes(), bytes()));
    crypto::secure_zero(a);
    EXPECT_TRUE(all_zero(a.data(), a.size()));
}

TEST(CryptoContract_Tests, ErrorValues) {
    crypto::error e(crypto::errc::authentication);
    EXPECT_EQ(e.code(), crypto::errc::authentication);
    EXPECT_EQ(e.offset(), 0u);
    EXPECT_EQ(e.message(), "message authentication failed");
    EXPECT_EQ(e, crypto::error(crypto::errc::authentication));
    EXPECT_FALSE(e == crypto::error(crypto::errc::invalid_key));
    std::error_code ec = crypto::errc::invalid_key;
    EXPECT_EQ(ec.category().name(), std::string("crypto"));
    EXPECT_EQ(ec.message(), "invalid key");
}
