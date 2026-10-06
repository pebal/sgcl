//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// concurrent::bloom_filter, hyperloglog and count_min_sketch: the shapes
// from their targets (the textbook m and k, e/ε and ln(1/δ)), the rates
// and errors measured against the targets over a million keys, the three
// kinds of key (text, bytes, numbers) and their overloads, merge (the
// union, another shape refused, a sketch with itself), clear, clone,
// to_bytes and from_bytes both ways and every malformed header refused at
// its byte, the hashes pinned to XXH3 (a sketch read by another process),
// and many threads adding at once: nothing lost (every key added found,
// every count summed, the estimate of the distinct as of one thread).
#include "tests/types.h"

#include <atomic>
#include <cmath>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    slice<const byte> bytes_of(const vector<byte>& v) {
        return v;
    }
}

// The textbook shape: m = -n ln p / ln²2, k = m/n ln 2
TEST(BloomFilter_Test, TheShape) {
    concurrent::bloom_filter f(1'000'000, 0.01);
    double m = std::ceil(-1e6 * std::log(0.01) / (std::log(2.0) * std::log(2.0)));
    EXPECT_EQ(f.bit_count(), size_t((uint64_t(m) + 63) / 64 * 64));
    EXPECT_EQ(f.hash_count(), 7u);
    concurrent::bloom_filter tiny(0);   // zero is one
    EXPECT_GE(tiny.bit_count(), 64u);
    EXPECT_GE(tiny.hash_count(), 1u);
    auto sized = concurrent::bloom_filter::with_size(100, 3);
    EXPECT_EQ(sized.bit_count(), 128u);
    EXPECT_EQ(sized.hash_count(), 3u);
    EXPECT_EQ(concurrent::bloom_filter::with_size(0, 1).bit_count(), 64u);
    EXPECT_THROW(concurrent::bloom_filter(10, 0.0), std::invalid_argument);
    EXPECT_THROW(concurrent::bloom_filter(10, 1.0), std::invalid_argument);
    EXPECT_THROW(concurrent::bloom_filter(10, std::nan("")), std::invalid_argument);
    EXPECT_THROW(concurrent::bloom_filter::with_size(64, 0), std::invalid_argument);
    EXPECT_THROW(concurrent::bloom_filter::with_size(64, 65), std::invalid_argument);
}

// No false negatives; false positives at the target rate; the estimates
TEST(BloomFilter_Test, TheRates) {
    for (double p : {0.01, 0.001}) {
        concurrent::bloom_filter f(200'000, p);
        size_t fresh = 0;
        for (uint64_t i = 0; i < 200'000; ++i) {
            fresh += f.add(i);
        }
        EXPECT_GT(fresh, 199'000u);   // a key that sets no new bit: a collision of all k, rare
        for (uint64_t i = 0; i < 200'000; ++i) {
            ASSERT_TRUE(f.contains(i));
        }
        size_t positives = 0;
        for (uint64_t i = 1'000'000; i < 2'000'000; ++i) {
            positives += f.contains(i);
        }
        double rate = double(positives) / 1e6;
        EXPECT_GT(rate, p * 0.7);
        EXPECT_LT(rate, p * 1.3);
        EXPECT_NEAR(f.approximate_count(), 200'000, 2'000);
        EXPECT_NEAR(f.false_positive_rate(), p, p * 0.3);
    }
    concurrent::bloom_filter empty(100);
    EXPECT_EQ(empty.approximate_count(), 0.0);
    EXPECT_EQ(empty.false_positive_rate(), 0.0);
    auto full = concurrent::bloom_filter::with_size(64, 64);
    for (uint64_t i = 0; i < 1000; ++i) {
        full.add(i);
    }
    EXPECT_TRUE(std::isinf(full.approximate_count()));
    EXPECT_EQ(full.false_positive_rate(), 1.0);
}

// The three kinds of key; a key added once is not new the second time
TEST(BloomFilter_Test, TheKeys) {
    concurrent::bloom_filter f(1000, 0.0001);
    EXPECT_TRUE(f.add("ann"));
    EXPECT_FALSE(f.add("ann"));
    EXPECT_TRUE(f.contains(std::string("ann")));
    EXPECT_TRUE(f.contains(string("ann")));
    EXPECT_TRUE(f.contains(std::string_view("ann")));
    EXPECT_FALSE(f.contains("bob"));
    EXPECT_TRUE(f.add(""));   // the empty key is a key
    EXPECT_TRUE(f.contains(""));
    vector<byte> raw = {byte(1), byte(2), byte(3)};
    EXPECT_TRUE(f.add(raw));
    EXPECT_TRUE(f.contains(bytes_of(raw)));
    EXPECT_TRUE(f.add(uint64_t(0)));
    EXPECT_TRUE(f.contains(0));
    EXPECT_TRUE(f.add(42));
    EXPECT_TRUE(f.contains(uint64_t(42)));
    // a number is its eight bytes, little-endian
    vector<byte> le = {byte(42), byte(0), byte(0), byte(0), byte(0), byte(0), byte(0), byte(0)};
    concurrent::bloom_filter g(1000, 0.0001);
    g.add(le);
    EXPECT_TRUE(g.contains(42));
    // text and its bytes are the same key
    vector<byte> ann = {byte('a'), byte('n'), byte('n')};
    EXPECT_TRUE(f.contains(ann));
}

// merge: the union; another shape refused; itself left as it is
TEST(BloomFilter_Test, Merge) {
    concurrent::bloom_filter a(1000, 0.001), b(1000, 0.001);
    for (uint64_t i = 0; i < 500; ++i) {
        a.add(i);
        b.add(i + 500);
    }
    a.merge(b);
    for (uint64_t i = 0; i < 1000; ++i) {
        ASSERT_TRUE(a.contains(i));
    }
    double before = a.approximate_count();
    a.merge(a);
    EXPECT_EQ(a.approximate_count(), before);
    concurrent::bloom_filter other(2000, 0.001);
    EXPECT_THROW(a.merge(other), std::invalid_argument);
    EXPECT_THROW(a.merge(concurrent::bloom_filter::with_size(a.bit_count(), a.hash_count() + 1)), std::invalid_argument);
}

// clear, clone, copies sharing the bits, ==
TEST(BloomFilter_Test, ClearCloneAndCopies) {
    concurrent::bloom_filter a(100, 0.01);
    a.add("x");
    concurrent::bloom_filter shared = a;
    concurrent::bloom_filter own = a.clone();
    EXPECT_TRUE(shared == a);
    EXPECT_FALSE(own == a);
    a.add("y");
    EXPECT_TRUE(shared.contains("y"));
    EXPECT_FALSE(own.contains("y"));
    EXPECT_TRUE(own.contains("x"));
    a.clear();
    EXPECT_FALSE(shared.contains("x"));
    EXPECT_TRUE(own.contains("x"));
    EXPECT_EQ(a.approximate_count(), 0.0);
}

// to_bytes and from_bytes; the format pinned; every bad header refused
TEST(BloomFilter_Test, Bytes) {
    concurrent::bloom_filter a(1000, 0.01);
    for (uint64_t i = 0; i < 100; ++i) {
        a.add(i);
    }
    vector<byte> b = a.to_bytes();
    ASSERT_EQ(b.size(), 16 + a.bit_count() / 8);
    EXPECT_EQ(char(b[0]), 'S');
    EXPECT_EQ(char(b[3]), 'F');
    EXPECT_EQ(int(b[4]), 1);
    EXPECT_EQ(int(b[5]), int(a.hash_count()));
    auto r = concurrent::bloom_filter::from_bytes(b);
    ASSERT_TRUE(r);
    EXPECT_EQ(r->bit_count(), a.bit_count());
    EXPECT_EQ(r->hash_count(), a.hash_count());
    for (uint64_t i = 0; i < 100; ++i) {
        ASSERT_TRUE(r->contains(i));
    }
    EXPECT_EQ(r->to_bytes(), b);
    auto refused = [&](vector<byte> bad, size_t offset) {
        auto e = concurrent::bloom_filter::from_bytes(bad);
        ASSERT_FALSE(e);
        EXPECT_EQ(e.error().offset(), offset) << e.error().message().data();
    };
    refused(vector<byte>(), 0);
    refused(vector<byte>(b.begin(), b.begin() + 15), 15);
    auto bad = b;
    bad[0] = byte('X');
    refused(bad, 0);
    bad = b;
    bad[4] = byte(2);
    refused(bad, 4);
    bad = b;
    bad[5] = byte(0);
    refused(bad, 5);
    bad[5] = byte(65);
    refused(bad, 5);
    bad = b;
    bad[7] = byte(1);
    refused(bad, 7);
    bad = b;
    bad[8] = byte(1);   // m not a multiple of 64
    refused(bad, 8);
    bad = b;
    bad.push_back(byte(0));
    refused(bad, bad.size());
    bad = b;
    bad.resize(b.size() - 8);
    refused(bad, bad.size());
}

// The hashes are XXH3 of the key's bytes: a filter of a known key has the
// bits XXH3-128 gives (another process, another build reads them alike)
TEST(BloomFilter_Test, PinnedToXxh3) {
    auto f = concurrent::bloom_filter::with_size(1 << 20, 1);
    f.add("pinned");
    auto h = hash::xxh3_128::of("pinned");   // big-endian bytes: high word first
    uint64_t low = 0;
    for (int i = 8; i < 16; ++i) {
        low = low << 8 | uint64_t(h[i]);
    }
    uint64_t bit = uint64_t(((unsigned __int128)low * (1u << 20)) >> 64);
    vector<byte> b = f.to_bytes();
    EXPECT_EQ(int(b[16 + bit / 8]), 1 << (bit % 8));
}

// Threads adding at once: every key found after
TEST(BloomFilter_Test, ManyThreads) {
    concurrent::bloom_filter f(400'000, 0.001);
    std::vector<std::thread> ts;
    std::atomic<long> fresh{0};
    for (int t : range(8)) {
        ts.emplace_back([&, t] {
            concurrent::bloom_filter mine = f;   // the handle on this thread's stack
            long k = 0;
            for (uint64_t i = 0; i < 50'000; ++i) {
                k += mine.add(uint64_t(t) * 1'000'000 + i);
                (void)mine.contains(i);
            }
            fresh += k;
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    for (int t : range(8)) {
        for (uint64_t i = 0; i < 50'000; ++i) {
            ASSERT_TRUE(f.contains(uint64_t(t) * 1'000'000 + i));
        }
    }
    EXPECT_GT(fresh.load(), 399'000);
    EXPECT_NEAR(f.approximate_count(), 400'000, 4'000);
}

// The precision's range; the error at every precision within 3.5 sigma
TEST(HyperLogLog_Test, TheEstimates) {
    EXPECT_THROW(concurrent::hyperloglog(3), std::invalid_argument);
    EXPECT_THROW(concurrent::hyperloglog(19), std::invalid_argument);
    for (unsigned p : {4u, 10u, 14u, 18u}) {
        concurrent::hyperloglog h(p);
        EXPECT_EQ(h.precision(), p);
        EXPECT_EQ(h.estimate(), 0.0);
        double sigma = 1.04 / std::sqrt(double(1u << p));
        uint64_t added = 0;
        for (uint64_t n : {1ull, 10ull, 1000ull, 100'000ull, 2'000'000ull}) {
            for (; added < n; ++added) {
                h.add(added * 0x9E3779B97F4A7C15ull);
            }
            double e = h.estimate();
            EXPECT_NEAR(e / double(n), 1.0, 3.5 * sigma + (n < 100 ? 0.05 : 0)) << "p " << p << " n " << n;
        }
        for (uint64_t i = 0; i < 1000; ++i) {   // the same keys again change nothing
            h.add(i * 0x9E3779B97F4A7C15ull);
        }
        EXPECT_NEAR(h.estimate() / 2e6, 1.0, 3.5 * sigma);
    }
}

// The keys; merge, clear, clone; copies
TEST(HyperLogLog_Test, KeysMergeClearClone) {
    concurrent::hyperloglog a, b;
    a.add("x");
    a.add(std::string("x"));
    a.add(string("x"));
    vector<byte> x = {byte('x')};
    a.add(x);
    EXPECT_NEAR(a.estimate(), 1.0, 0.01);
    a.add(uint64_t(1));
    a.add(1);
    EXPECT_NEAR(a.estimate(), 2.0, 0.02);
    for (uint64_t i = 0; i < 50'000; ++i) {
        a.add(i);
        b.add(i + 25'000);
    }
    concurrent::hyperloglog both = a.clone();
    both.merge(b);
    EXPECT_NEAR(both.estimate() / 75'001, 1.0, 0.03);
    double before = both.estimate();
    both.merge(both);
    EXPECT_EQ(both.estimate(), before);
    EXPECT_THROW(a.merge(concurrent::hyperloglog(12)), std::invalid_argument);
    concurrent::hyperloglog shared = a;
    EXPECT_TRUE(shared == a);
    EXPECT_FALSE(both == a);
    a.clear();
    EXPECT_EQ(shared.estimate(), 0.0);
    EXPECT_GT(both.estimate(), 70'000);
}

// to_bytes and from_bytes; bad headers and registers refused
TEST(HyperLogLog_Test, Bytes) {
    concurrent::hyperloglog a(10);
    for (uint64_t i = 0; i < 5000; ++i) {
        a.add(i);
    }
    vector<byte> b = a.to_bytes();
    ASSERT_EQ(b.size(), 8u + 1024);
    auto r = concurrent::hyperloglog::from_bytes(b);
    ASSERT_TRUE(r);
    EXPECT_EQ(r->estimate(), a.estimate());
    EXPECT_EQ(r->to_bytes(), b);
    auto refused = [&](vector<byte> bad, size_t offset) {
        auto e = concurrent::hyperloglog::from_bytes(bad);
        ASSERT_FALSE(e);
        EXPECT_EQ(e.error().offset(), offset) << e.error().message().data();
    };
    refused(vector<byte>(3), 3);
    auto bad = b;
    bad[2] = byte('X');
    refused(bad, 0);
    bad = b;
    bad[5] = byte(3);
    refused(bad, 5);
    bad[5] = byte(19);
    refused(bad, 5);
    bad = b;
    bad[6] = byte(1);
    refused(bad, 6);
    bad = b;
    bad.pop_back();
    refused(bad, bad.size());
    bad = b;
    bad[8 + 7] = byte(56);   // 64 - 10 + 1 = 55 at most
    refused(bad, 15);
}

// Threads adding at once: the estimate of one thread's adds
TEST(HyperLogLog_Test, ManyThreads) {
    concurrent::hyperloglog h;
    concurrent::hyperloglog alone;
    for (uint64_t i = 0; i < 400'000; ++i) {
        alone.add(i);
    }
    std::vector<std::thread> ts;
    for (int t : range(8)) {
        ts.emplace_back([&, t] {
            concurrent::hyperloglog mine = h;
            for (uint64_t i = 0; i < 100'000; ++i) {
                mine.add(uint64_t(t % 4) * 100'000 + i);   // each key by two threads
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(h.estimate(), alone.estimate());   // the same registers whatever the order
}

// The shape from ε and δ; with_size; bad arguments
TEST(CountMinSketch_Test, TheShape) {
    concurrent::count_min_sketch c(0.001, 0.01);
    EXPECT_EQ(c.width(), 2719u);   // ceil(e / 0.001)
    EXPECT_EQ(c.depth(), 5u);      // ceil(ln 100)
    auto s = concurrent::count_min_sketch::with_size(100, 3);
    EXPECT_EQ(s.width(), 100u);
    EXPECT_EQ(s.depth(), 3u);
    EXPECT_THROW(concurrent::count_min_sketch(0, 0.5), std::invalid_argument);
    EXPECT_THROW(concurrent::count_min_sketch(0.5, 1), std::invalid_argument);
    EXPECT_THROW(concurrent::count_min_sketch::with_size(0, 1), std::invalid_argument);
    EXPECT_THROW(concurrent::count_min_sketch::with_size(1, 0), std::invalid_argument);
    EXPECT_THROW(concurrent::count_min_sketch::with_size(1, 65), std::invalid_argument);
}

// Never under the truth; over it by more than εN rarely
TEST(CountMinSketch_Test, TheBounds) {
    concurrent::count_min_sketch plain(0.001, 0.01);
    std::vector<uint64_t> truth(10'000);
    uint64_t z = 1;
    for (int i : range(200'000)) {
        (void)i;
        z = z * 6364136223846793005ull + 1442695040888963407ull;
        uint64_t key = (z >> 33) % 10'000;
        key = key * key / 10'000;   // skewed: the small keys frequent
        ++truth[key];
        plain.add(key);
    }
    EXPECT_EQ(plain.total(), 200'000u);
    size_t over = 0;
    for (uint64_t k = 0; k < 10'000; ++k) {
        uint64_t e = plain.estimate(k);
        ASSERT_GE(e, truth[k]);
        over += e - truth[k] > uint64_t(0.001 * 200'000);
    }
    EXPECT_LT(over, 10'000u / 100 * 2);
    plain.add("ann", 7);
    plain.add(std::string("ann"), 3);
    vector<byte> ann = {byte('a'), byte('n'), byte('n')};
    EXPECT_GE(plain.estimate(ann), 10u);
    plain.add(ann, 0);
    EXPECT_GE(plain.estimate("ann"), 10u);
}

// merge, clear, clone, copies
TEST(CountMinSketch_Test, MergeClearClone) {
    auto a = concurrent::count_min_sketch::with_size(64, 4);
    auto b = concurrent::count_min_sketch::with_size(64, 4);
    a.add("x", 3);
    b.add("x", 4);
    b.add("y", 1);
    auto c = a.clone();
    c.merge(b);
    EXPECT_GE(c.estimate("x"), 7u);
    EXPECT_EQ(c.total(), 8u);
    EXPECT_EQ(a.total(), 3u);
    c.merge(c);   // itself: everything twice
    EXPECT_EQ(c.total(), 16u);
    EXPECT_THROW(a.merge(concurrent::count_min_sketch::with_size(64, 3)), std::invalid_argument);
    auto shared = a;
    EXPECT_TRUE(shared == a);
    a.clear();
    EXPECT_EQ(shared.estimate("x"), 0u);
    EXPECT_EQ(shared.total(), 0u);
}

// to_bytes and from_bytes; bad headers refused
TEST(CountMinSketch_Test, Bytes) {
    auto a = concurrent::count_min_sketch::with_size(10, 2);
    a.add("x", 5);
    vector<byte> b = a.to_bytes();
    ASSERT_EQ(b.size(), 32u + 8 * 20);
    auto r = concurrent::count_min_sketch::from_bytes(b);
    ASSERT_TRUE(r);
    EXPECT_EQ(r->estimate("x"), 5u);
    EXPECT_EQ(r->total(), 5u);
    EXPECT_EQ(r->to_bytes(), b);
    auto refused = [&](vector<byte> bad, size_t offset) {
        auto e = concurrent::count_min_sketch::from_bytes(bad);
        ASSERT_FALSE(e);
        EXPECT_EQ(e.error().offset(), offset) << e.error().message().data();
    };
    refused(vector<byte>(31), 31);
    auto bad = b;
    bad[1] = byte('X');
    refused(bad, 0);
    bad = b;
    bad[6] = byte(1);
    refused(bad, 6);
    bad = b;
    bad[8] = byte(0);   // width 0
    refused(bad, 8);
    bad = b;
    bad[16] = byte(65);
    refused(bad, 16);
    bad = b;
    bad[16] = byte(3);   // the size of 2 rows
    refused(bad, bad.size());
}

// Threads adding at once: every count summed, none lost
TEST(CountMinSketch_Test, ManyThreads) {
    auto c = concurrent::count_min_sketch::with_size(1000, 4);
    std::vector<std::thread> ts;
    for (int t : range(8)) {
        ts.emplace_back([&, t] {
            concurrent::count_min_sketch mine = c;
            for (int i : range(10'000)) {
                (void)i;
                mine.add("hot", uint64_t(t % 2 + 1));
                mine.add(uint64_t(i));
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(c.total(), 120'000u + 80'000u);
    EXPECT_GE(c.estimate("hot"), 120'000u);   // never below; above by the collisions with the numbers
    for (uint64_t i = 0; i < 10'000; ++i) {
        ASSERT_GE(c.estimate(i), 8u);
    }
}
