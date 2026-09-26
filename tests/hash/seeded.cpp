//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The hashes with a seed or a key — xxh3_64, xxh3_128, maphash, siphash:
// against the vectors tools/hash_seeded_oracle.go wrote (seeded_vectors.h,
// whose generator says which cases were asked; each value agreed on by two
// oracles), in one call and in pieces; what a seed and a key change and
// what reset() keeps; maphash's seed of the process, which differs from run
// to run unless SGCL_HASH_SEED fixes it.
#include "common.h"
#include "seeded_vectors.h"

#include <cstdlib>
#include <random>
#include <string>

namespace {
    using namespace hash_test;

    const std::vector<unsigned char>& longest(int kind) {
        static const std::vector<unsigned char> p[4] = {
            pattern(0, (1 << 20) + 1), pattern(1, (1 << 20) + 1), pattern(2, (1 << 20) + 1), pattern(3, (1 << 20) + 1)};
        return p[kind];
    }

    sgcl::array<byte, 16> key(int i) {
        sgcl::array<byte, 16> k;
        for (size_t b = 0; b < 16; ++b) {
            k[b] = byte(seeded_vectors::keys[i][b]);
        }
        return k;
    }

    // H in pieces of `piece` bytes, the last one shorter
    template<class H>
    auto in_pieces(H h, const unsigned char* p, size_t n, size_t piece) {
        for (size_t at = 0; at < n; at += piece) {
            h.update(bytes(p + at, std::min(piece, n - at)));
        }
        return h.value();
    }
}

// Every row in one call, and in pieces of a size that changes from row to
// row (1…300, and a stripe, a buffer and a block about their edges), so
// that a seeded state crosses its buffer and its first stripe everywhere
TEST(Hash_Seeded, Xxh3AgainstTheOracles) {
    const size_t sizes[] = {1, 2, 3, 5, 7, 8, 13, 16, 17, 31, 63, 64, 65, 100, 127, 128, 129, 239, 240, 241, 255, 256, 257, 300, 1023, 1024, 1025};
    size_t rows = 0;
    for (const auto& r : seeded_vectors::xxh3) {
        const unsigned char* p = longest(r.pattern).data();
        const size_t n = r.length;
        const uint64_t seed = seeded_vectors::seeds[r.seed];
        SCOPED_TRACE("pattern " + std::to_string(r.pattern) + " length " + std::to_string(n) + " seed " + text(seed));
        const auto want64 = text(r.xxh3_64);
        const auto want128 = text128(r.xxh3_128_high, r.xxh3_128_low);
        EXPECT_EQ(text(hash::xxh3_64::of(bytes(p, n), seed)), want64);
        EXPECT_EQ(text(hash::xxh3_128::of(bytes(p, n), seed)), want128);
        const size_t piece = sizes[rows % std::size(sizes)];
        EXPECT_EQ(text(in_pieces(hash::xxh3_64(seed), p, n, piece)), want64) << "pieces of " << piece;
        EXPECT_EQ(text(in_pieces(hash::xxh3_128(seed), p, n, piece)), want128) << "pieces of " << piece;
        if (seed == 0) {
            EXPECT_EQ(text(hash::xxh3_64::of(bytes(p, n))), want64);
            EXPECT_EQ(text(in_pieces(hash::xxh3_64(), p, n, piece)), want64);
            EXPECT_EQ(text(in_pieces(hash::xxh3_128(), p, n, piece)), want128);
        }
        ++rows;
        if (::testing::Test::HasFailure()) {
            break;
        }
    }
    EXPECT_GT(rows, 5000u);
}

TEST(Hash_Seeded, SipHashAgainstTheOracles) {
    const size_t sizes[] = {1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 64, 100, 4096};
    size_t rows = 0;
    for (const auto& r : seeded_vectors::siphash) {
        const unsigned char* p = longest(r.pattern).data();
        const size_t n = r.length;
        SCOPED_TRACE("pattern " + std::to_string(r.pattern) + " length " + std::to_string(n) + " key " + std::to_string(r.key));
        EXPECT_EQ(text(hash::siphash::of(bytes(p, n), key(r.key))), text(r.siphash));
        const size_t piece = sizes[rows % std::size(sizes)];
        EXPECT_EQ(text(in_pieces(hash::siphash(key(r.key)), p, n, piece)), text(r.siphash)) << "pieces of " << piece;
        ++rows;
        if (::testing::Test::HasFailure()) {
            break;
        }
    }
    EXPECT_GT(rows, 1300u);
}

// Values that stand in their documents: the paper's appendix (key 00…0f,
// the fifteen bytes 00…0e), xxhsum's hashes of nothing
TEST(Hash_Seeded, PublishedValues) {
    unsigned char m[15];
    for (size_t i = 0; i < 15; ++i) {
        m[i] = (unsigned char)i;
    }
    EXPECT_EQ(hash::siphash::of(bytes(m, 15), sip_key()), 0xa129ca6149be45e5ull);
    EXPECT_EQ(hash::xxh3_64::of(""), 0x2d06800538d394c2ull);
    EXPECT_EQ(text(hash::xxh3_128::of("")), "99aa06d3014798d86001c324468d497f");
}

// Every split into two of every input up to 1100 bytes, in both widths and
// with a seed: the buffer of four stripes fills, spills, takes stripes
// straight from the input, keeps the stripe the last one reaches back into
TEST(Hash_Seeded, Xxh3EverySplit) {
    auto data = pattern(0, 1100);
    for (uint64_t seed : {uint64_t(0), uint64_t(0x9e3779b97f4a7c15ull)}) {
        for (size_t n = 0; n <= 1100; ++n) {
            const uint64_t whole64 = hash::xxh3_64::of(bytes(data.data(), n), seed);
            const auto whole128 = text(hash::xxh3_128::of(bytes(data.data(), n), seed));
            for (size_t cut = 0; cut <= n; cut += n < 600 ? 1 : 7) {
                hash::xxh3_64 a(seed);
                a.update(bytes(data.data(), cut));
                a.update(bytes(data.data() + cut, n - cut));
                ASSERT_EQ(a.value(), whole64) << "n " << n << " cut " << cut << " seed " << seed;
                hash::xxh3_128 b(seed);
                b.update(bytes(data.data(), cut));
                b.update(bytes(data.data() + cut, n - cut));
                ASSERT_EQ(text(b.value()), whole128) << "n " << n << " cut " << cut << " seed " << seed;
            }
        }
    }
    // three pieces at random places, up to a few blocks
    auto longer = pattern(0, 5000);
    std::mt19937 rng(9);
    for (int round = 0; round < 3000; ++round) {
        size_t n = rng() % 5001;
        size_t a = rng() % (n + 1);
        size_t b = a + rng() % (n - a + 1);
        hash::xxh3_64 h {uint64_t(round)};
        h.update(bytes(longer.data(), a));
        h.update(bytes(longer.data() + a, b - a));
        h.update(bytes(longer.data() + b, n - b));
        ASSERT_EQ(h.value(), hash::xxh3_64::of(bytes(longer.data(), n), uint64_t(round))) << "n " << n << " at " << a << ", " << b;
    }
}

// A seed changes the value, and reset() keeps the seed (and siphash its
// key); a copy made after a seeded secret was made goes on alike
TEST(Hash_Seeded, SeedsAndKeys) {
    auto data = pattern(0, 3000);
    for (size_t n : {size_t(0), size_t(3), size_t(8), size_t(16), size_t(100), size_t(200), size_t(250), size_t(3000)}) {
        EXPECT_NE(hash::xxh3_64::of(bytes(data.data(), n), 1), hash::xxh3_64::of(bytes(data.data(), n), 2)) << n;
        EXPECT_NE(text(hash::xxh3_128::of(bytes(data.data(), n), 1)), text(hash::xxh3_128::of(bytes(data.data(), n), 2))) << n;
        EXPECT_NE(hash::siphash::of(bytes(data.data(), n), key(1)), hash::siphash::of(bytes(data.data(), n), key(2))) << n;
        EXPECT_NE(hash::maphash::of(bytes(data.data(), n), 1), hash::maphash::of(bytes(data.data(), n), 2)) << n;
    }
    hash::xxh3_64 x(77);
    x.update(bytes(data.data(), 2000));   // the seed's secret made
    auto branch = x;
    x.reset();
    x.update(bytes(data.data(), 500));
    EXPECT_EQ(x.value(), hash::xxh3_64::of(bytes(data.data(), 500), 77));
    branch.update(bytes(data.data() + 2000, 1000));
    EXPECT_EQ(branch.value(), hash::xxh3_64::of(bytes(data), 77));
    hash::xxh3_128 y(78);
    y.update(bytes(data.data(), 2000));
    y.reset();
    EXPECT_EQ(text(y.value()), text(hash::xxh3_128::of("", 78)));
    hash::siphash s(key(3));
    s.update(bytes(data.data(), 77));
    s.reset();
    s.update(bytes(data.data(), 5));
    EXPECT_EQ(s.value(), hash::siphash::of(bytes(data.data(), 5), key(3)));
    hash::maphash m(79);
    m.update(bytes(data.data(), 2000));
    m.reset();
    EXPECT_EQ(m.value(), hash::maphash::of("", 79));
    hash::maphash process;
    process.update("x");
    process.reset();
    EXPECT_EQ(process.value(), hash::maphash::of(""));
}

// A seed or a key after the data only where the class has one: a CRC does
// not go on from a value through of(), and a siphash has no hash without
// its key
namespace {
    template<class H, class... Args>
    concept hashes_in_one_call = requires(const sgcl::slice<const byte>& data, const Args&... args) { H::of(data, args...); };

    template<class H, class T>
    concept takes_a_value = requires(H& h, const T& value) { h.update_value(value); };
}

TEST(Hash_Seeded, OfTakesASeedOnlyWhereThereIsOne) {
    using key_type = sgcl::array<byte, 16>;
    static_assert(hashes_in_one_call<hash::xxh3_64, uint64_t>);
    static_assert(hashes_in_one_call<hash::xxh3_128, int>);
    static_assert(hashes_in_one_call<hash::maphash, int>);
    static_assert(hashes_in_one_call<hash::maphash>);
    static_assert(hashes_in_one_call<hash::siphash, key_type>);
    static_assert(!hashes_in_one_call<hash::siphash>);
    static_assert(!hashes_in_one_call<hash::crc32, unsigned>);
    static_assert(!hashes_in_one_call<hash::fnv64a, unsigned long long>);
    static_assert(!hashes_in_one_call<hash::xxh3_64, key_type>);
    static_assert(!hashes_in_one_call<hash::siphash, uint64_t>);
    static_assert(requires { hash::xxh3_128::of("text", 1); });
    SUCCEED();
}

// Two maphashes of a process agree, in one call and in pieces; an explicit
// seed is today XXH3-64 with that seed (the algorithm is not promised: this
// test says what it is now, and changes with it)
TEST(Hash_Maphash, AgreesWithinTheProcess) {
    auto data = pattern(0, 5000);
    for (size_t n : {size_t(0), size_t(1), size_t(8), size_t(17), size_t(240), size_t(241), size_t(256), size_t(257), size_t(5000)}) {
        const uint64_t v = hash::maphash::of(bytes(data.data(), n));
        hash::maphash a, b;
        a.update(bytes(data.data(), n));
        b.update(bytes(data.data(), n / 3));
        b.update(bytes(data.data() + n / 3, n - n / 3));
        EXPECT_EQ(a.value(), v) << n;
        EXPECT_EQ(b.value(), v) << n;
        EXPECT_EQ(hash::maphash::of(bytes(data.data(), n), 12345), hash::xxh3_64::of(bytes(data.data(), n), 12345)) << n;
        hash::maphash seeded(12345);
        seeded.update(bytes(data.data(), n));
        EXPECT_EQ(seeded.value(), hash::xxh3_64::of(bytes(data.data(), n), 12345)) << n;
    }
}

// update_value: the bytes of the value, and only for a type all of whose
// bytes are its value
TEST(Hash_Maphash, UpdateValue) {
    struct point {
        int32_t x;
        int32_t y;
    };
    struct padded {
        char c;
        int64_t v;
    };
    static_assert(takes_a_value<hash::maphash, point>);
    static_assert(takes_a_value<hash::maphash, uint64_t>);
    static_assert(!takes_a_value<hash::maphash, padded>);
    static_assert(!takes_a_value<hash::maphash, double>);
    static_assert(!takes_a_value<hash::maphash, float>);
    static_assert(!takes_a_value<hash::xxh3_64, uint64_t>);   // only where the result stays in the process
    point p {7, -3};
    hash::maphash a, b;
    a.update_value(p);
    a.update("name");
    b.update(bytes(reinterpret_cast<const unsigned char*>(&p), sizeof p));
    b.update("name");
    EXPECT_EQ(a.value(), b.value());
    hash::maphash c;
    c.update_value(point{7, -2});
    c.update("name");
    EXPECT_NE(a.value(), c.value());
}

namespace {
    const char* const PrintValue = "SGCL_HASH_TEST_PRINT_MAPHASH";

    // This program run again with only the test below, which prints the
    // process's maphash of a fixed text: the value, read from its named field
    std::string maphash_of_another_run(const char* fixed_seed) {
        namespace io = sgcl::io;
        io::command c(sgcl::string(::testing::internal::GetArgvs()[0].c_str()), "--gtest_filter=Hash_Maphash.PrintsItsValue");
        sgcl::vector<sgcl::pair<sgcl::string, sgcl::string>> env;
        env.push_back({PrintValue, "1"});
        if (fixed_seed) {
            env.push_back({"SGCL_HASH_SEED", fixed_seed});
        }
        c.env = std::move(env);
        auto out = c.output();
        if (!out) {
            return "failed: " + out.error().message().str();
        }
        std::string text = out->str();
        auto at = text.find("maphash=");
        if (at == std::string::npos) {
            return "no value in: " + text;
        }
        return text.substr(at + 8, 16);
    }
}

// Run only as the child of the test after it
TEST(Hash_Maphash, PrintsItsValue) {
    if (std::getenv(PrintValue)) {
        std::printf("maphash=%s\n", text(hash::maphash::of("the same text")).c_str());
    }
}

// The seed of the process is drawn in each run, so two runs disagree; with
// SGCL_HASH_SEED set, two runs agree
TEST(Hash_Maphash, TheSeedDiffersBetweenRuns) {
    auto one_run = maphash_of_another_run(nullptr);
    auto other_run = maphash_of_another_run(nullptr);
    ASSERT_EQ(one_run.size(), 16u) << one_run;
    ASSERT_EQ(other_run.size(), 16u) << other_run;
    EXPECT_NE(one_run, other_run);
    auto fixed = maphash_of_another_run("12345");
    EXPECT_EQ(fixed, maphash_of_another_run("12345"));
    EXPECT_NE(fixed, maphash_of_another_run("12346"));
}

// The run of stripes (on arm64 in NEON vectors) against the one stripe at a
// time of stripe() and scramble(), the scalar path, in one program: random
// lanes, every count of stripes to two and a half blocks, from every stripe
// of a block, the default secret and a seed's
TEST(Hash_Seeded, Xxh3TheTwoPathsAgree) {
    namespace detail = sgcl::hash::detail;
    auto data = pattern(0, 64 * 40);
    unsigned char seeded[detail::Xxh3SecretSize];
    detail::xxh3_derive_secret(seeded, 0x0123456789abcdefull);
    splitmix r {3};
    for (const unsigned char* secret : {detail::Xxh3Secret, static_cast<const unsigned char*>(seeded)}) {
        for (size_t count = 0; count <= 40; ++count) {
            for (size_t first = 0; first < detail::Xxh3StripesPerBlock; ++first) {
                detail::Xxh3Lanes run, one;
                for (auto& a : run.acc) {
                    a = r.next();
                }
                one = run;
                size_t next_run = run.stripes(data.data(), count, first, secret);
                size_t next_one = first;
                for (size_t k = 0; k < count; ++k) {
                    one.stripe(data.data() + 64 * k, secret + 8 * next_one);
                    if (++next_one == detail::Xxh3StripesPerBlock) {
                        one.scramble(secret + detail::Xxh3ScrambleAt);
                        next_one = 0;
                    }
                }
                ASSERT_EQ(next_run, next_one);
                for (size_t i = 0; i < 8; ++i) {
                    ASSERT_EQ(run.acc[i], one.acc[i]) << "count " << count << " first " << first << " lane " << i;
                }
            }
        }
    }
}
