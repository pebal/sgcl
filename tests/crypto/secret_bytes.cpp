//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::secret_bytes (secret.h): no copy, a move leaves the source
// empty, clone, resize keeping the prefix and zeroing what it adds and
// drops, equality; what it leaves in memory: the inline bytes of a
// destroyed object zeroed, every block zeroed before it is freed (the one a
// growth leaves too, seen through the allocator's probe); and no allocation
// at all up to 64 bytes (operator new counted on this thread). The secrets
// the module gives as secret_bytes (hkdf, pbkdf2, SHAKE, random::secret),
// the plaintexts it does not (an AEAD's open, rsa::decrypt_oaep: a
// vector<byte>), and read_secret.
#include "digest_common.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <new>
#include <type_traits>
#include <vector>

using namespace crypto_test;

namespace {
    thread_local size_t news = 0;
    thread_local bool counting = false;
}

// operator new counted while a test asks, on its own thread
void* operator new(size_t n) {
    if (counting) {
        ++news;
    }
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

namespace {
    using crypto::secret_bytes;

    slice<const std::byte> bytes_of(const std::string& t) {
        return slice<const std::byte>(reinterpret_cast<const std::byte*>(t.data()), t.size());
    }

    static_assert(!std::is_copy_constructible_v<secret_bytes>);
    static_assert(!std::is_copy_assignable_v<secret_bytes>);
    static_assert(std::is_nothrow_move_constructible_v<secret_bytes>);
    static_assert(std::is_nothrow_move_assignable_v<secret_bytes>);
    static_assert(secret_bytes::inline_capacity == 64);

    void fill(secret_bytes& s, uint8_t v) {
        for (auto& b : s.as_slice()) {
            b = std::byte(v);
        }
    }

    bool all(const slice<const std::byte>& s, uint8_t v) {
        for (auto b : s) {
            if (b != std::byte(v)) {
                return false;
            }
        }
        return true;
    }

    // What the allocator's probe saw: each block as it went, zeroed or not
    struct Seen {
        size_t blocks = 0;
        size_t bytes = 0;
        bool all_zero = true;
    };

    Seen seen;

    void record(const void* block, size_t n) noexcept {
        ++seen.blocks;
        seen.bytes += n;
        const auto* p = static_cast<const unsigned char*>(block);
        for (size_t i = 0; i < n; ++i) {
            if (p[i]) {
                seen.all_zero = false;
            }
        }
    }

    struct Probe {
        Probe() {
            seen = Seen{};
            crypto::detail::WipingPolicy::probe = record;
        }

        ~Probe() {
            crypto::detail::WipingPolicy::probe = nullptr;
        }
    };
}

TEST(Crypto_SecretBytes, SizeZerosAndResize) {
    secret_bytes empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    for (size_t n : {size_t(1), size_t(32), size_t(64), size_t(65), size_t(1000)}) {
        secret_bytes s(n);
        EXPECT_EQ(s.size(), n);
        EXPECT_TRUE(all(s.as_slice(), 0)) << n;
    }
    // growth keeps the prefix and zeroes the new bytes; inline to a block and on
    secret_bytes s(10);
    fill(s, 0xA5);
    for (size_t n : {size_t(40), size_t(64), size_t(100), size_t(5000)}) {
        s.resize(n);
        ASSERT_EQ(s.size(), n);
        EXPECT_TRUE(all(s.as_slice().subslice(0, 10), 0xA5)) << n;
        EXPECT_TRUE(all(s.as_slice().subslice(10), 0)) << n;
    }
    // a shrink zeroes what it drops: grown again within the room, zeros
    s.resize(5);
    EXPECT_TRUE(all(s.as_slice(), 0xA5));
    s.resize(10);
    EXPECT_TRUE(all(s.as_slice().subslice(5), 0));
    secret_bytes small(8);
    fill(small, 0x11);
    small.resize(3);
    small.resize(8);
    EXPECT_TRUE(all(small.as_slice().subslice(0, 3), 0x11));
    EXPECT_TRUE(all(small.as_slice().subslice(3), 0));
}

TEST(Crypto_SecretBytes, MoveCloneAndEquality) {
    for (size_t n : {size_t(0), size_t(32), size_t(64), size_t(65), size_t(300)}) {
        secret_bytes a(n);
        fill(a, 0x5A);
        const std::byte* where = a.as_slice().data();
        secret_bytes b = std::move(a);
        EXPECT_TRUE(a.empty()) << n;
        EXPECT_EQ(b.size(), n);
        EXPECT_TRUE(all(b.as_slice(), 0x5A));
        if (n > 64) {
            EXPECT_EQ(b.as_slice().data(), where) << "a block is passed on, not copied";
        }
        secret_bytes c = b.clone();
        EXPECT_TRUE(c == b);
        if (n) {
            EXPECT_NE(c.as_slice().data(), b.as_slice().data());
            c.as_slice()[n - 1] = std::byte(0);
            EXPECT_FALSE(c == b);
        }
        secret_bytes d(3);
        d = std::move(b);
        EXPECT_TRUE(b.empty());
        EXPECT_EQ(d.size(), n);
    }
    secret_bytes x(4), y(5);
    EXPECT_FALSE(x == y);
    // taken where the module takes bytes
    secret_bytes key(32);
    fill(key, 7);
    const slice<const std::byte> view = key;
    EXPECT_EQ(view.size(), 32u);
}

TEST(Crypto_SecretBytes, InlineBytesZeroedByTheDestructor) {
    for (size_t n : {size_t(1), size_t(32), size_t(64)}) {
        alignas(secret_bytes) unsigned char storage[sizeof(secret_bytes)];
        std::memset(storage, 0, sizeof storage);   // no stale byte to look like the pattern
        auto* s = new (storage) secret_bytes(n);
        fill(*s, 0xA5);
        s->~secret_bytes();
        for (unsigned char c : storage) {
            ASSERT_NE(c, 0xA5) << n;
        }
    }
    // a move leaves the source's inline bytes zeroed
    alignas(secret_bytes) unsigned char storage[sizeof(secret_bytes)];
    std::memset(storage, 0, sizeof storage);
    auto* s = new (storage) secret_bytes(48);
    fill(*s, 0xA5);
    secret_bytes taken = std::move(*s);
    for (unsigned char c : storage) {
        ASSERT_NE(c, 0xA5);
    }
    s->~secret_bytes();
    EXPECT_TRUE(all(taken.as_slice(), 0xA5));
}

TEST(Crypto_SecretBytes, EveryBlockZeroedBeforeItIsFreed) {
    {
        Probe probe;
        {
            secret_bytes s(200);
            fill(s, 0xA5);
        }
        EXPECT_EQ(seen.blocks, 1u);
        EXPECT_EQ(seen.bytes, 200u);
        EXPECT_TRUE(seen.all_zero);
    }
    {
        // growth: the old block zeroed as it goes, then the new one
        Probe probe;
        {
            secret_bytes s(100);
            fill(s, 0xA5);
            s.resize(1000);
            EXPECT_EQ(seen.blocks, 1u);
            EXPECT_TRUE(seen.all_zero);
            fill(s, 0x3C);
        }
        EXPECT_EQ(seen.blocks, 2u);
        EXPECT_EQ(seen.bytes, 1100u);
        EXPECT_TRUE(seen.all_zero);
    }
    {
        // a move frees nothing; move-assignment frees what it replaces, zeroed
        Probe probe;
        secret_bytes a(300), b(400);
        fill(a, 0xA5);
        fill(b, 0x5A);
        secret_bytes c = std::move(a);
        EXPECT_EQ(seen.blocks, 0u);
        b = std::move(c);
        EXPECT_EQ(seen.blocks, 1u);
        EXPECT_EQ(seen.bytes, 400u);
        EXPECT_TRUE(seen.all_zero);
        EXPECT_TRUE(all(b.as_slice(), 0xA5));
    }
}

TEST(Crypto_SecretBytes, NoAllocationUpTo64Bytes) {
    std::vector<size_t> counts;
    for (size_t n : {size_t(16), size_t(32), size_t(48), size_t(64), size_t(65), size_t(1024)}) {
        news = 0;
        counting = true;
        {
            secret_bytes s(n);
            fill(s, 1);
            secret_bytes t = std::move(s);
            (void)t;
        }
        counting = false;
        counts.push_back(news);
    }
    EXPECT_EQ(counts, (std::vector<size_t>{0, 0, 0, 0, 1, 1}));
    // a clone of a small one allocates nothing either
    secret_bytes k(32);
    news = 0;
    counting = true;
    {
        secret_bytes c = k.clone();
        (void)c;
    }
    counting = false;
    EXPECT_EQ(news, 0u);
}

TEST(Crypto_SecretBytes, TheSecretsOfTheModuleAreSecretBytes) {
    // what hkdf, pbkdf2, SHAKE and random::secret give:
    // no allocation up to 64 bytes, one plain block past them
    const std::string salt = "salt", ikm = "input keying material", info = "info";
    auto derive = [&](size_t n) {
        news = 0;
        counting = true;
        {
            secret_bytes k = crypto::hkdf<crypto::sha256>::derive(bytes_of(salt), bytes_of(ikm), bytes_of(info), n);
            EXPECT_EQ(k.size(), n);
        }
        counting = false;
        return news;
    };
    EXPECT_EQ(derive(32), 0u);
    EXPECT_EQ(derive(64), 0u);
    EXPECT_EQ(derive(1024), 1u);
    // the same bytes as the _to form
    secret_bytes k = crypto::hkdf<crypto::sha256>::derive(bytes_of(salt), bytes_of(ikm), bytes_of(info), 42);
    std::byte to[42];
    crypto::hkdf<crypto::sha256>::derive_to(slice<std::byte>(to, 42), bytes_of(salt), bytes_of(ikm), bytes_of(info));
    EXPECT_EQ(std::memcmp(k.as_slice().data(), to, 42), 0);
    secret_bytes p = crypto::pbkdf2<crypto::sha256>::derive(bytes_of(std::string("password")), bytes_of(salt), 2, 32);
    EXPECT_EQ(p.size(), 32u);
    secret_bytes s = crypto::shake256::of(bytes_of(std::string("abc")), 64);
    EXPECT_EQ(s.size(), 64u);
    secret_bytes r1 = crypto::random::secret(48), r2 = crypto::random::secret(48);
    EXPECT_EQ(r1.size(), 48u);
    EXPECT_FALSE(r1 == r2);
    // not a secret: an AEAD's plaintext and an OAEP message are the user's
    // data, a vector<byte>; a key unwrapped goes through open_to into a
    // secret_bytes of the program's
    static_assert(std::is_same_v<decltype(std::declval<crypto::chacha20_poly1305&>().open(slice<const std::byte>(), slice<const std::byte>())),
                                 expected<vector<std::byte>, crypto::error>>);
    static_assert(std::is_same_v<decltype(std::declval<crypto::xchacha20_poly1305&>().open_random(slice<const std::byte>())),
                                 expected<vector<std::byte>, crypto::error>>);
    static_assert(std::is_same_v<decltype(std::declval<crypto::rsa::private_key&>().decrypt_oaep(crypto::hash_id::sha256, slice<const std::byte>())),
                                 expected<vector<std::byte>, crypto::error>>);
    secret_bytes key = crypto::random::secret(32);
    crypto::chacha20_poly1305 aead(key);
    const std::string message = "a message of some length, but short";
    const std::string nonce = "123456789012";
    auto sealed = aead.seal(bytes_of(nonce), bytes_of(message));
    auto opened = aead.open(bytes_of(nonce), sealed);
    ASSERT_TRUE(opened);
    ASSERT_EQ(opened->size(), message.size());
    EXPECT_EQ(std::memcmp(opened->data(), message.data(), message.size()), 0);
    secret_bytes unwrapped(message.size());
    auto n = aead.open_to(unwrapped, bytes_of(nonce), sealed);
    ASSERT_TRUE(n);
    EXPECT_EQ(std::memcmp(unwrapped.as_slice().data(), message.data(), message.size()), 0);
}

// A secret_bytes the program may write is taken where the module writes
// (an AEAD's open_to, RSA's decrypt_oaep_to, random::fill) without
// as_slice(); a const one, and one about to go (an rvalue, whose bytes no
// one would read), are not
TEST(Crypto_SecretBytes, TakenAsTheOutput) {
    EXPECT_TRUE((std::is_convertible_v<secret_bytes&, slice<std::byte>>));
    EXPECT_FALSE((std::is_convertible_v<const secret_bytes&, slice<std::byte>>));
    EXPECT_FALSE((std::is_convertible_v<secret_bytes&&, slice<std::byte>>));
    EXPECT_TRUE((std::is_convertible_v<secret_bytes&, slice<const std::byte>>));
    EXPECT_TRUE((std::is_convertible_v<const secret_bytes&, slice<const std::byte>>));
    EXPECT_TRUE((std::is_convertible_v<secret_bytes&&, slice<const std::byte>>));
    // a key unwrapped with RSA-OAEP straight into one
    auto rsa = crypto::rsa::private_key::generate(2048);
    secret_bytes key(32);
    crypto::random::fill(key);
    auto wrapped = rsa.public_key().encrypt_oaep(crypto::hash_id::sha256, key);
    secret_bytes unwrapped(rsa.public_key().max_oaep_message_size(crypto::hash_id::sha256));
    auto n = rsa.decrypt_oaep_to(unwrapped, crypto::hash_id::sha256, wrapped);
    ASSERT_TRUE(n);
    unwrapped.resize(*n);
    EXPECT_TRUE(unwrapped == key);
}

TEST(Crypto_SecretBytes, ReadSecret) {
    const auto path = std::filesystem::temp_directory_path() / "sgcl_read_secret_test.key";
    for (size_t n : {size_t(0), size_t(10), size_t(64), size_t(65), size_t(5000)}) {
        std::string content(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            content[i] = char('a' + i % 26);
        }
        {
            std::ofstream out(path, std::ios::binary);
            out << content;
        }
        auto s = crypto::read_secret(string(path.string()));
        ASSERT_TRUE(s) << n;
        ASSERT_EQ(s->size(), n);
        EXPECT_TRUE(n == 0 || std::memcmp(s->as_slice().data(), content.data(), n) == 0) << n;
    }
    std::filesystem::remove(path);
    auto missing = crypto::read_secret("/nonexistent/sgcl/key");
    EXPECT_FALSE(missing);
}
