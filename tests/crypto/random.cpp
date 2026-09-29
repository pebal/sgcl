//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// random, constant_time::equal, secure_zero and the module's error: random
// bytes of every size, never the same twice (across threads too), roughly
// even over the byte values; the generator's erasure, its reseeds after a
// mebibyte and after a minute, its state zeroed at a thread's end, a
// forked child's stream of its own; equal on every length and every
// single difference; the zeros a buffer is left with; the error's words.
#include "digest_common.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <set>
#include <thread>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace crypto_test;

TEST(Crypto_Random, FillsWholeBuffersPastOneCall) {
    for (size_t n : {0u, 1u, 255u, 256u, 257u, 1000u, 4096u, 100000u}) {
        auto b = crypto::random::bytes(n);
        ASSERT_EQ(b.size(), n);
        if (n >= 257) {
            // the last call's piece is filled too: 32 zero bytes at the end
            // of a random buffer have a chance of 2^-256
            bool tail_zero = true;
            for (size_t i = n - 32; i < n; ++i) {
                tail_zero &= b[i] == byte(0);
            }
            EXPECT_FALSE(tail_zero) << n;
        }
    }
    bytes_t buf(1000, 0);
    crypto::random::fill(out_view(buf));
    size_t zeros = 0;
    for (auto x : buf) {
        zeros += x == 0;
    }
    EXPECT_LT(zeros, 30u);   // about 4 expected
}

TEST(Crypto_Random, NeverTheSameAndEvenlySpread) {
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i) {
        auto b = crypto::random::bytes(16);
        EXPECT_TRUE(seen.insert(hex(b)).second);
    }
    // 256 000 bytes: each value about 1000 times; 800…1200 is past six
    // standard deviations (sqrt(1000) ≈ 31.6)
    std::array<size_t, 256> count {};
    auto b = crypto::random::bytes(256000);
    for (auto x : b) {
        ++count[size_t(x)];
    }
    for (size_t v = 0; v < 256; ++v) {
        EXPECT_GT(count[v], 800u) << v;
        EXPECT_LT(count[v], 1200u) << v;
    }
}

// The generator of detail/drbg.h: the thread's state, its reseeds, its
// end, a fork
namespace {
    using sgcl::crypto::detail::DrbgState;
    namespace drbg = sgcl::crypto::detail;

    DrbgState& state() {
        return drbg::drbg_state;
    }

    std::string take_hex(size_t n) {
        bytes_t b(n);
        crypto::random::fill(out_view(b));
        return hex(b);
    }
}

TEST(Crypto_Random, ThreadsNeverRepeatAndSmallRequestsSpreadEvenly) {
    // eight threads, 500 requests of 32 bytes each: no two alike
    std::vector<std::vector<std::string>> got(8);
    std::vector<std::thread> threads;
    for (size_t t = 0; t < got.size(); ++t) {
        threads.emplace_back([&got, t] {
            for (int i = 0; i < 500; ++i) {
                got[t].push_back(take_hex(32));
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::set<std::string> seen;
    for (auto& g : got) {
        for (auto& x : g) {
            EXPECT_TRUE(seen.insert(x).second);
        }
    }
    // 256 000 bytes in requests of 32, through the buffer: each value
    // about 1000 times, as for the large request above
    std::array<size_t, 256> count {};
    bytes_t b(32);
    for (int i = 0; i < 8000; ++i) {
        crypto::random::fill(out_view(b));
        for (auto x : b) {
            ++count[x];
        }
    }
    for (size_t v = 0; v < 256; ++v) {
        EXPECT_GT(count[v], 800u) << v;
        EXPECT_LT(count[v], 1200u) << v;
    }
}

TEST(Crypto_Random, TheBytesGivenAreErased) {
    (void)take_hex(1);
    DrbgState& s = state();
    ASSERT_EQ(s.status, DrbgState::seeded);
    for (size_t n : {1u, 31u, 32u, 100u, 543u, 1000u}) {
        (void)take_hex(n);
        for (size_t i = 0; i < s.pos; ++i) {
            ASSERT_EQ(s.buffer[i], 0) << n << " at " << i;
        }
    }
    // a large request takes its one-time key from the buffer: 32 bytes
    if (s.pos > drbg::drbg_buffer_size - 32) {
        (void)take_hex(drbg::drbg_buffer_size - s.pos);
        (void)take_hex(1);
    }
    const uint32_t before = s.pos;
    (void)take_hex(5000);
    EXPECT_EQ(s.pos, before + 32);
}

TEST(Crypto_Random, ReseedAfterAMebibyte) {
    (void)take_hex(1);
    DrbgState& s = state();
    const uint64_t reseeds = s.reseeds;
    const uint64_t given = s.given;
    uint64_t made = 0;
    bytes_t b(512);
    while (s.reseeds == reseeds && made < 4 * drbg::drbg_reseed_bytes) {
        crypto::random::fill(out_view(b));
        made += b.size();
    }
    EXPECT_EQ(s.reseeds, reseeds + 1);
    // at the first refill past the limit: within a buffer and a request
    EXPECT_GE(given + made, drbg::drbg_reseed_bytes);
    EXPECT_LE(given + made, drbg::drbg_reseed_bytes + drbg::drbg_buffer_size + 512);
}

namespace {
    std::atomic<uint64_t> fake_now{0};

    uint64_t fake_clock() noexcept {
        return fake_now.load();
    }
}

TEST(Crypto_Random, ReseedAfterAMinute) {
    // a thread of its own, seeded on the fake clock
    fake_now = uint64_t(1000) * 1000000000u;
    drbg::drbg_clock = &fake_clock;
    uint64_t counts[4] = {};
    std::thread t([&counts] {
        (void)take_hex(1);
        DrbgState& s = state();
        counts[0] = s.reseeds;
        for (int i = 0; i < 3; ++i) {
            (void)take_hex(512);                    // refills (through the buffer), no time passed
        }
        counts[1] = s.reseeds;
        fake_now += drbg::drbg_reseed_ns - 1;
        (void)take_hex(600);
        counts[2] = s.reseeds;
        fake_now += 1;
        (void)take_hex(600);                        // a refill a minute on
        counts[3] = s.reseeds;
    });
    t.join();
    drbg::drbg_clock = &drbg::drbg_system_clock;
    EXPECT_EQ(counts[0], 1u);
    EXPECT_EQ(counts[1], 1u);
    EXPECT_EQ(counts[2], 1u);
    EXPECT_EQ(counts[3], 2u);
}

namespace {
    std::atomic<int> exits{0};
    std::atomic<bool> zeroed{false};
    std::atomic<bool> after_is_random{false};

    void on_exit(const void* p, size_t n) noexcept {
        auto b = static_cast<const unsigned char*>(p);
        auto s = static_cast<const DrbgState*>(p);
        bool z = n == sizeof(DrbgState) && s->status == DrbgState::dead;
        for (size_t i = 0; i < n; ++i) {
            if (i != offsetof(DrbgState, status)) {
                z = z && b[i] == 0;
            }
        }
        zeroed = z;
        // a call after the end: the system's bytes, the state left as it is
        unsigned char r[32] = {};
        crypto::random::fill(sgcl::slice<byte>(reinterpret_cast<byte*>(r), 32));
        bool any = false;
        for (auto x : r) {
            any = any || x != 0;
        }
        after_is_random = any && s->pos == 0 && s->status == DrbgState::dead;
        ++exits;
    }
}

TEST(Crypto_Random, TheStateIsZeroedWhenTheThreadEnds) {
    drbg::drbg_on_thread_exit = &on_exit;
    std::thread t([] {
        (void)take_hex(32);
    });
    t.join();
    drbg::drbg_on_thread_exit = nullptr;
    EXPECT_EQ(exits.load(), 1);
    EXPECT_TRUE(zeroed.load());
    EXPECT_TRUE(after_is_random.load());
}

#if !defined(_WIN32)
TEST(Crypto_Random, AForkedChildHasAStreamOfItsOwn) {
    (void)take_hex(1);
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const pid_t child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        unsigned char b[32];
        crypto::random::fill(sgcl::slice<byte>(reinterpret_cast<byte*>(b), 32));
        (void)!::write(fds[1], b, 32);
        ::_exit(0);
    }
    // the parent's next 32 bytes: what the child would have had from the
    // state it copied
    bytes_t mine(32);
    crypto::random::fill(out_view(mine));
    unsigned char theirs[32] = {};
    size_t got = 0;
    while (got < 32) {
        ssize_t r = ::read(fds[0], theirs + got, 32 - got);
        if (r <= 0) {
            break;
        }
        got += size_t(r);
    }
    int status = 0;
    ::waitpid(child, &status, 0);
    ::close(fds[0]);
    ::close(fds[1]);
    ASSERT_EQ(got, 32u);
    EXPECT_NE(std::memcmp(mine.data(), theirs, 32), 0);
}

TEST(Crypto_Random, AnotherProcessIdSeedsAfresh) {
    // a fork that ran no atfork handler: the process id, at the next refill
    (void)take_hex(1);
    DrbgState& s = state();
    const uint64_t reseeds = s.reseeds;
    s.pid += 1;
    (void)take_hex(drbg::drbg_buffer_size);         // at least one refill
    EXPECT_EQ(s.reseeds, reseeds + 1);
    EXPECT_EQ(s.pid, int(::getpid()));
}
#endif

TEST(Crypto_ConstantTime, Equal) {
    random_source r(21);
    for (size_t n = 0; n <= 70; ++n) {
        bytes_t a = r.bytes(n);
        bytes_t b = a;
        EXPECT_TRUE(crypto::constant_time::equal(view(a), view(b))) << n;
        for (size_t i = 0; i < n; ++i) {
            for (int bit = 0; bit < 8; ++bit) {
                b[i] ^= (unsigned char)(1 << bit);
                ASSERT_FALSE(crypto::constant_time::equal(view(a), view(b))) << n << " byte " << i << " bit " << bit;
                b[i] ^= (unsigned char)(1 << bit);
            }
        }
        bytes_t longer = a;
        longer.push_back(0);
        EXPECT_FALSE(crypto::constant_time::equal(view(a), view(longer)));
        EXPECT_FALSE(crypto::constant_time::equal(view(longer), view(a)));
    }
    // the forms that convert: an array (a digest), a vector
    auto d = crypto::sha256::of("abc");
    auto e = crypto::sha256::of("abc");
    EXPECT_TRUE(crypto::constant_time::equal(d, e));
    auto v = crypto::digest(crypto::hash_id::sha256, "abc");
    EXPECT_TRUE(crypto::constant_time::equal(d, v));
    EXPECT_FALSE(crypto::constant_time::equal(d, crypto::sha256::of("abd")));
}

TEST(Crypto_SecureZero, ZerosTheBuffer) {
    bytes_t secret = text("correct horse battery staple");
    crypto::secure_zero(out_view(secret));
    for (auto x : secret) {
        EXPECT_EQ(x, 0);
    }
    sgcl::array<byte, 32> key = crypto::sha256::of("k");
    crypto::secure_zero(key);
    for (auto x : key) {
        EXPECT_EQ(x, byte(0));
    }
    auto v = crypto::random::bytes(100);
    crypto::secure_zero(v);
    for (auto x : v) {
        EXPECT_EQ(x, byte(0));
    }
    crypto::secure_zero(sgcl::slice<byte>());   // nothing to zero
}

TEST(Crypto_Error, WordsAndCodes) {
    crypto::error e(crypto::errc::authentication);
    EXPECT_EQ(e.code(), crypto::errc::authentication);
    EXPECT_EQ(e.offset(), 0u);
    EXPECT_EQ(e.message(), "message authentication failed");
    crypto::error f(crypto::errc::malformed, 17);
    EXPECT_EQ(f.message(), "offset 17: malformed data");
    crypto::error g(crypto::errc::malformed, 4, "DER: length past the end");
    EXPECT_EQ(g.message(), "offset 4: DER: length past the end");
    crypto::error h(crypto::errc::unsupported, "curve secp256k1");
    EXPECT_EQ(h.message(), "curve secp256k1");
    EXPECT_EQ(f, crypto::error(crypto::errc::malformed, 17));
    EXPECT_FALSE(f == g);
    std::error_code c = crypto::errc::invalid_key;
    EXPECT_EQ(c.category().name(), std::string("crypto"));
    EXPECT_EQ(c.message(), "invalid key");
}
