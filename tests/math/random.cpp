//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "c2sp_vectors.h"
#include "random_vectors.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
    math::random from_text_key(const char* text) {
        unsigned char key[32];
        std::memcpy(key, text, 32);
        return math::detail::RandomAccess::from_key(key);
    }

    // Kolmogorov–Smirnov: the largest distance between the sample's
    // distribution and the one it should have
    template<class Cdf>
    double ks_distance(std::vector<double> xs, Cdf cdf) {
        std::sort(xs.begin(), xs.end());
        double n = double(xs.size());
        double d = 0;
        for (size_t i = 0; i < xs.size(); ++i) {
            double f = cdf(xs[i]);
            d = std::max(d, std::max(f - double(i) / n, double(i + 1) / n - f));
        }
        return d;
    }

    // Whether pick takes an argument of this type and category (R&& of a
    // non-reference R is an rvalue)
    template<class R>
    concept can_pick = requires(math::random& g, R&& r) { g.pick(std::forward<R>(r)); };

    double chi_square(const std::vector<long>& counts, double expected) {
        double chi = 0;
        for (long c : counts) {
            chi += (double(c) - expected) * (double(c) - expected) / expected;
        }
        return chi;
    }
}

// The specification's own sample output: three iterations, two rekeyings
TEST(Random_Tests, C2spVectors) {
    auto r = from_text_key(math_vectors::c2sp_key);
    for (size_t i = 0; i < std::size(math_vectors::c2sp_output); ++i) {
        ASSERT_EQ(r.next_uint64(), math_vectors::c2sp_output[i]) << "word " << i;
    }
}

// Go's ChaCha8 past what the specification lists, and the key random(42)
// stands for
TEST(Random_Tests, SameStreamAsGo) {
    auto r = from_text_key(math_vectors::c2sp_key);
    for (size_t i = 0; i < std::size(math_vectors::go_ascii_key); ++i) {
        ASSERT_EQ(r.next_uint64(), math_vectors::go_ascii_key[i]) << "word " << i;
    }
    math::random seeded(42);
    for (size_t i = 0; i < std::size(math_vectors::go_seed_42); ++i) {
        ASSERT_EQ(seeded.next_uint64(), math_vectors::go_seed_42[i]) << "word " << i;
    }
}

// Bounds that take the mask (powers of two), Lemire's multiplication, and
// its rejection (3·2^61 rejects a quarter of the draws, 2^63 - 1 and
// 2^62 + 1 about half and a quarter): the numbers Go's Int64N gives
TEST(Random_Tests, NextIntAsGo) {
    for (auto& t : math_vectors::go_int64n) {
        math::random r(7);
        for (int i = 0; i < 20; ++i) {
            ASSERT_EQ(r.next_int(t.bound), t.draws[i]) << "bound " << t.bound << " draw " << i;
        }
    }
    math::random r(7);
    for (double expected : math_vectors::go_float64) {
        ASSERT_EQ(r.next_double(), expected);
    }
}

TEST(Random_Tests, PermutationAndShuffleAsGo) {
    math::random r(7);
    auto p = r.permutation(10);
    ASSERT_EQ(p.size(), 10u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(p[i], math_vectors::go_perm_10[i]);
    }
    math::random s(7);
    vector<size_t> v;
    for (auto i : range(size_t(1000))) {
        v.push_back(i);
    }
    s.shuffle(v);
    for (size_t i = 0; i < 1000; ++i) {
        ASSERT_EQ(v[i], math_vectors::go_perm_1000[i]);
    }
    std::vector<int> std_vector = {1, 2, 3};
    s.shuffle(std_vector);                    // any range of random access
    std::sort(std_vector.begin(), std_vector.end());
    EXPECT_EQ(std_vector, (std::vector<int>{1, 2, 3}));
    vector<int> empty;
    s.shuffle(empty);
    EXPECT_EQ(s.permutation(0).size(), 0u);
}

TEST(Random_Tests, Ranges) {
    math::random r(1);
    for (int i = 0; i < 10000; ++i) {
        auto die = r.next_int(1, 7);
        ASSERT_TRUE(die >= 1 && die <= 6);
        auto x = r.next_int(-3, 2);
        ASSERT_TRUE(x >= -3 && x < 2);
        ASSERT_EQ(r.next_int(1), 0);
        ASSERT_EQ(r.next_int(5, 6), 5);
        auto whole = r.next_int(INT64_MIN, INT64_MAX);
        ASSERT_LT(whole, INT64_MAX);
        double d = r.next_double();
        ASSERT_TRUE(d >= 0 && d < 1);
    }
    EXPECT_THROW(r.next_int(0), std::domain_error);
    EXPECT_THROW(r.next_int(-5), std::domain_error);
    EXPECT_THROW(r.next_int(3, 3), std::domain_error);
    EXPECT_THROW(r.next_int(4, 3), std::domain_error);
    EXPECT_THROW(r.next_normal(0, -1), std::domain_error);
    EXPECT_THROW(r.next_normal(0, std::nan("")), std::domain_error);
    EXPECT_THROW(r.next_exponential(0), std::domain_error);
    EXPECT_THROW(r.next_exponential(-1), std::domain_error);
    EXPECT_EQ(r.next_normal(5, 0), 5);
}

// Every value as likely: small bounds, a range, a bound whose draws are
// rejected a quarter of the time, and a coin
TEST(Random_Tests, UniformByChiSquare) {
    math::random r(2);
    const long n = 600000;
    std::vector<long> six(6);
    std::vector<long> range(5);
    std::vector<long> thirds(3);
    std::vector<long> coin(2);
    for (long i = 0; i < n; ++i) {
        ++six[size_t(r.next_int(6))];
        ++range[size_t(r.next_int(-2, 3) + 2)];
        ++thirds[size_t(r.next_int(int64_t(3) << 61) >> 61)];
        ++coin[r.next_bool()];
    }
    // p = 0.001 for 5, 4, 2 and 1 degrees of freedom
    EXPECT_LT(chi_square(six, n / 6.0), 20.5);
    EXPECT_LT(chi_square(range, n / 5.0), 18.5);
    EXPECT_LT(chi_square(thirds, n / 3.0), 13.8);
    EXPECT_LT(chi_square(coin, n / 2.0), 10.8);
}

// All 24 orders of four elements, as often each
TEST(Random_Tests, ShuffleOfFourByChiSquare) {
    math::random r(3);
    std::vector<long> counts(24);
    const long n = 240000;
    for (long i = 0; i < n; ++i) {
        int v[4] = {0, 1, 2, 3};
        r.shuffle(v);
        // The rank of the permutation (Lehmer code)
        int rank = 0;
        for (int a = 0; a < 4; ++a) {
            int smaller = 0;
            for (int b = a + 1; b < 4; ++b) {
                smaller += v[b] < v[a];
            }
            rank = rank * (4 - a) + smaller;
        }
        ++counts[size_t(rank)];
    }
    EXPECT_LT(chi_square(counts, n / 24.0), 49.7);   // p = 0.001, 23 degrees of freedom
}

TEST(Random_Tests, NormalDistribution) {
    math::random r(4);
    const size_t n = 400000;
    std::vector<double> xs(n);
    double sum = 0;
    double squares = 0;
    size_t tail = 0;
    for (auto& x : xs) {
        x = r.next_normal();
        sum += x;
        squares += x * x;
        tail += std::fabs(x) > 3.442619855899;
    }
    double mean = sum / n;
    double variance = squares / n - mean * mean;
    EXPECT_NEAR(mean, 0, 5 / std::sqrt(double(n)));
    EXPECT_NEAR(variance, 1, 5 * std::sqrt(2.0 / n));
    double d = ks_distance(xs, [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); });
    EXPECT_LT(d, 1.95 / std::sqrt(double(n)));   // p = 0.001
    // The tail beyond the base layer, sampled apart: 2(1 - Φ(3.4426)) of the draws
    double expected = 2 * n * 0.5 * std::erfc(3.442619855899 / std::sqrt(2.0));
    EXPECT_NEAR(double(tail), expected, 5 * std::sqrt(expected));
    math::random s(4);
    double scaled = s.next_normal(100, 15);
    math::random t(4);
    EXPECT_EQ(scaled, 100 + 15 * t.next_normal());
}

TEST(Random_Tests, ExponentialDistribution) {
    math::random r(5);
    const size_t n = 400000;
    std::vector<double> xs(n);
    double sum = 0;
    size_t tail = 0;
    for (auto& x : xs) {
        x = r.next_exponential();
        ASSERT_GE(x, 0);
        sum += x;
        tail += x > 7.697117470131487;
    }
    EXPECT_NEAR(sum / n, 1, 5 / std::sqrt(double(n)));
    double d = ks_distance(xs, [](double x) { return 1 - std::exp(-x); });
    EXPECT_LT(d, 1.95 / std::sqrt(double(n)));
    double expected = n * std::exp(-7.697117470131487);
    EXPECT_NEAR(double(tail), expected, 5 * std::sqrt(expected));
    math::random s(5);
    math::random t(5);
    EXPECT_EQ(s.next_exponential(4), t.next_exponential() / 4);
}

// random(42) is a promise across versions of the library: these values
// were written down when the distributions were made and must not move
TEST(Random_Tests, GoldenValues) {
    math::random r(42);
    EXPECT_EQ(r.next_uint64(), math_vectors::go_seed_42[0]);
    EXPECT_EQ(r.next_int(100), 20);
    EXPECT_EQ(r.next_normal(), 0x1.d27b8160d0717p-3);
    EXPECT_EQ(r.next_exponential(), 0x1.c4706f7ed3e4bp+0);
    EXPECT_EQ(r.next_bool(), true);
    EXPECT_EQ(r.next_double(), 0x1.43e6b4369249ap-2);
}

TEST(Random_Tests, Bytes) {
    math::random r(9);
    math::random s(9);
    byte buffer[21];
    r.next_bytes(slice<byte>(buffer, buffer + sizeof buffer));
    for (size_t i = 0; i < sizeof buffer; i += 8) {
        uint64_t w = s.next_uint64();
        for (size_t b = 0; b < 8 && i + b < sizeof buffer; ++b) {
            ASSERT_EQ(buffer[i + b], byte(w >> (8 * b)));
        }
    }
    EXPECT_EQ(r.next_uint64(), s.next_uint64());   // the rest of the last draw dropped
    r.next_bytes(slice<byte>());
    EXPECT_EQ(r.next_uint64(), s.next_uint64());
}

TEST(Random_Tests, Pick) {
    math::random r(10);
    vector<int> cards = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<long> counts(9);
    for (int i = 0; i < 80000; ++i) {
        ++counts[size_t(r.pick(cards))];
    }
    counts.erase(counts.begin());
    EXPECT_LT(chi_square(counts, 10000), 24.3);   // p = 0.001, 7 degrees of freedom
    int& chosen = r.pick(cards);                   // the element itself
    chosen = 100;
    EXPECT_TRUE(std::find(cards.begin(), cards.end(), 100) != cards.end());
    const vector<int>& view = cards;
    const int& seen = r.pick(view);
    EXPECT_TRUE(seen >= 1);
    vector<int> empty;
    EXPECT_THROW(r.pick(empty), std::out_of_range);
    int plain[] = {7};
    EXPECT_EQ(r.pick(plain), 7);
    // A temporary, const or not, is refused: the element handed back would
    // outlive it
    static_assert(!can_pick<std::vector<int>>);
    static_assert(!can_pick<const std::vector<int>>);
    static_assert(!can_pick<const vector<int>>);
    static_assert(can_pick<const std::vector<int>&>);
    static_assert(can_pick<vector<int>&>);
}

TEST(Random_Tests, StandardGenerator) {
    static_assert(std::uniform_random_bit_generator<math::random>);
    math::random r(11);
    math::random s(11);
    EXPECT_EQ(r(), s.next_uint64());
    std::uniform_int_distribution<int> dist(1, 6);
    int x = dist(r);
    EXPECT_TRUE(x >= 1 && x <= 6);
    std::vector<int> from = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> out;
    std::ranges::sample(from, std::back_inserter(out), 3, r);
    EXPECT_EQ(out.size(), 3u);
}

// A copy draws what the original draws
TEST(Random_Tests, CopyCopiesTheStream) {
    math::random r(12);
    r.next_uint64();
    math::random c = r;
    for (int i = 0; i < 300; ++i) {
        ASSERT_EQ(r.next_uint64(), c.next_uint64());
    }
}

// Defaults never repeat one another: in one thread, and across threads
// each keyed from the system on its first default
TEST(Random_Tests, DefaultsAreDistinct) {
    std::mutex m;
    std::set<uint64_t> seen;
    auto draw = [&] {
        std::vector<uint64_t> mine;
        for (int i = 0; i < 200; ++i) {
            math::random r;
            mine.push_back(r.next_uint64());
        }
        std::lock_guard lock(m);
        seen.insert(mine.begin(), mine.end());
    };
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back(draw);
    }
    draw();
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(seen.size(), 9u * 200u);
}

#if !defined(_WIN32)
// A child of fork() has a copy of its parent's generator for the thread;
// it must key itself again, or the two draw the same numbers
TEST(Random_Tests, ForkedChildDiffers) {
    math::random warm;          // the thread's generator exists before the fork
    (void)warm;
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        math::random child;
        uint64_t v = child.next_uint64();
        (void)!::write(fds[1], &v, sizeof v);
        ::_exit(0);
    }
    math::random parent;
    uint64_t mine = parent.next_uint64();
    uint64_t theirs = 0;
    ASSERT_EQ(::read(fds[0], &theirs, sizeof theirs), ssize_t(sizeof theirs));
    int status = 0;
    ::waitpid(pid, &status, 0);
    ::close(fds[0]);
    ::close(fds[1]);
    EXPECT_NE(mine, theirs);
}
#endif
