//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// uuid: RFC 9562. The test vectors of its appendix A (v1, v3, v4, v5, v6, v7)
// read with their versions, variants and times; v4 and v7 made here read by
// Python's uuid module (the version, the variant, the text, v7's
// millisecond), and v7 kept in order across threads; the forms parse takes
// and where it refuses; the boundaries of every member.
#include "common.h"

#include <cstdio>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string str(const uuid& u) {
        return std::string(u.to_string().view());
    }

    std::string run(const std::string& command, const std::string& input) {
        auto dir = std::filesystem::temp_directory_path() / ("sgcl_uuid_" + std::to_string(::getpid()) + ".txt");
        {
            FILE* f = std::fopen(dir.c_str(), "w");
            std::fwrite(input.data(), 1, input.size(), f);
            std::fclose(f);
        }
        std::string out;
        FILE* p = popen((command + " < '" + dir.string() + "'").c_str(), "r");
        if (p) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) {
                out.append(buf, n);
            }
            pclose(p);
        }
        std::filesystem::remove(dir);
        return out;
    }
}

TEST(Uuid_Tests, TheRfcsVectors) {
    // RFC 9562 appendix A: the version, the variant, the instant
    struct {
        const char* text;
        int version;
        int64_t unix_nanos;   // -1: no time
    } vectors[] = {
        {"C232AB00-9414-11EC-B3C8-9F6BDECED846", 1, 1645557742000000000},
        {"5df41881-3aed-3515-88a7-2f4a814cf09e", 3, -1},
        {"919108f7-52d1-4320-9bac-f847db4148a8", 4, -1},
        {"2ed6657d-e927-568b-95e1-2665a8aea6a2", 5, -1},
        {"1EC9414C-232A-6B00-B3C8-9F6BDECED846", 6, 1645557742000000000},
        {"017F22E2-79B0-7CC3-98C4-DC0C0C07398F", 7, 1645557742000000000},
        {"2489E9AD-2EE2-8E00-8EC9-32D5F69181C0", 8, -1},
    };
    for (auto& v : vectors) {
        auto u = uuid::parse(v.text);
        ASSERT_TRUE(u) << v.text;
        EXPECT_EQ(u->version(), v.version);
        EXPECT_EQ(u->variant(), uuid::variant_kind::rfc9562);
        std::string lower = v.text;
        for (auto& c : lower) {
            c = char(std::tolower(c));
        }
        EXPECT_EQ(str(*u), lower);
        auto t = u->timestamp();
        if (v.unix_nanos < 0) {
            EXPECT_FALSE(t) << v.text;
        } else {
            ASSERT_TRUE(t) << v.text;
            EXPECT_EQ(t->unix_nano(), v.unix_nanos) << v.text;
        }
    }
    // the variants of byte 8
    EXPECT_EQ(uuid("00000000-0000-0000-7000-000000000000").variant(), uuid::variant_kind::ncs);
    EXPECT_EQ(uuid("00000000-0000-0000-bfff-000000000000").variant(), uuid::variant_kind::rfc9562);
    EXPECT_EQ(uuid("00000000-0000-0000-c000-000000000000").variant(), uuid::variant_kind::microsoft);
    EXPECT_EQ(uuid("00000000-0000-0000-e000-000000000000").variant(), uuid::variant_kind::future);
    EXPECT_FALSE(uuid("00000000-0000-1000-c000-000000000000").timestamp());   // v1 of another variant
    // a v1 before 1970 (1900-01-01, Python's reading), and the first
    // instant of the Gregorian count, before a datetime's first, which it
    // is then
    EXPECT_EQ(uuid("3c230000-a32f-1163-8000-000000000000").timestamp()->unix(), -2208988800);
    EXPECT_EQ(uuid("00000000-0000-1000-8000-000000000000").timestamp()->unix(), sgcl::time::datetime::from_unix(INT64_MIN).unix());
}

TEST(Uuid_Tests, MadeHereAsPythonReadsThem) {
    std::string input;
    std::vector<uuid> made;
    for (int i = 0; i < 200; ++i) {
        made.push_back(i % 2 ? uuid::v7() : uuid::v4());
        input += str(made.back()) + "\n";
    }
    std::string out = run("python3 -c \"import sys, uuid\n"
                          "for line in sys.stdin:\n"
                          "    u = uuid.UUID(line.strip())\n"
                          "    print(u, u.version, u.variant == uuid.RFC_4122, u.time if u.version == 7 else 0)\"",
                          input);
    if (out.empty()) {
        GTEST_SKIP() << "no python3";
    }
    size_t at = 0;
    for (auto& u : made) {
        size_t nl = out.find('\n', at);
        std::string line = out.substr(at, nl - at);
        at = nl + 1;
        char text[64];
        int version = 0;
        char rfc[8];
        long long ms = 0;
        ASSERT_EQ(std::sscanf(line.c_str(), "%63s %d %7s %lld", text, &version, rfc, &ms), 4) << line;
        EXPECT_EQ(std::string(text), str(u));
        EXPECT_EQ(version, u.version());
        EXPECT_EQ(std::string(rfc), "True");
        if (version == 7) {
            EXPECT_EQ(ms, u.timestamp()->unix_milli());
        }
    }
}

TEST(Uuid_Tests, V4) {
    std::set<std::string> seen;
    int ones[128] = {};
    for (int i = 0; i < 20000; ++i) {
        auto u = uuid::v4();
        EXPECT_EQ(u.version(), 4);
        EXPECT_EQ(u.variant(), uuid::variant_kind::rfc9562);
        EXPECT_TRUE(seen.insert(str(u)).second);
        auto b = u.bytes();
        for (int k = 0; k < 128; ++k) {
            ones[k] += (b[size_t(k / 8)] >> (7 - k % 8)) & 1;
        }
        EXPECT_FALSE(u.timestamp());
    }
    // the 122 random bits are each set about half the time; the six fixed
    // ones always as written
    for (int k = 0; k < 128; ++k) {
        bool fixed = (k >= 48 && k < 52) || k == 64 || k == 65;
        if (!fixed) {
            EXPECT_GT(ones[k], 9000) << k;
            EXPECT_LT(ones[k], 11000) << k;
        }
    }
    EXPECT_EQ(ones[48] + ones[50] + ones[51], 0);
    EXPECT_EQ(ones[49], 20000);
    EXPECT_EQ(ones[64], 20000);
    EXPECT_EQ(ones[65], 0);
}

TEST(Uuid_Tests, V7InOrderAcrossThreads) {
    auto before = sgcl::time::datetime::from_unix_milli(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), sgcl::time::zone::utc());
    constexpr int threads = 4, each = 20000;
    std::vector<std::vector<std::array<uint8_t, 16>>> got(threads);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&got, t] {
            for (int i = 0; i < each; ++i) {
                auto b = uuid::v7().bytes();
                std::array<uint8_t, 16> raw;
                for (size_t k = 0; k < 16; ++k) {
                    raw[k] = b[k];
                }
                got[size_t(t)].push_back(raw);
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }
    std::set<std::array<uint8_t, 16>> all;
    for (auto& g : got) {
        for (size_t i = 1; i < g.size(); ++i) {
            ASSERT_LT(g[i - 1], g[i]);   // each thread's in order
        }
        all.insert(g.begin(), g.end());
    }
    EXPECT_EQ(all.size(), size_t(threads * each));   // and no two alike
    // the time is the clock's (a millisecond counted ahead at most a few)
    auto last = uuid(sgcl::array<uint8_t, 16>{});
    uuid u = uuid::v7();
    EXPECT_EQ(u.version(), 7);
    EXPECT_GE(u.timestamp()->unix_milli(), before.unix_milli());
    EXPECT_LT(u.timestamp()->unix_milli(), before.unix_milli() + 60000);
    EXPECT_GT(u, last);
    // one thread: strictly greater each time, even within a millisecond
    uuid prev = uuid::v7();
    for (int i = 0; i < 100000; ++i) {
        uuid next = uuid::v7();
        ASSERT_GT(next, prev);
        prev = next;
    }
}

TEST(Uuid_Tests, ParseForms) {
    uuid want("f81d4fae-7dec-11d0-a765-00a0c91e6bf6");
    for (const char* text : {"f81d4fae-7dec-11d0-a765-00a0c91e6bf6", "F81D4FAE-7DEC-11D0-A765-00A0C91E6BF6",
                             "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}", "urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
                             "URN:UUID:f81d4fae-7dec-11d0-a765-00a0c91e6bf6", "f81d4fae7dec11d0a76500a0c91e6bf6"}) {
        auto u = uuid::parse(text);
        ASSERT_TRUE(u) << text;
        EXPECT_EQ(*u, want) << text;
    }
    struct {
        const char* text;
        errc code;
        uint64_t offset;
    } bad[] = {
        {"", errc::syntax, 0},
        {"f81d4fae-7dec-11d0-a765-00a0c91e6bf", errc::syntax, 35},
        {"f81d4fae-7dec-11d0-a765-00a0c91e6bf6a", errc::syntax, 37},
        {"f81d4fae_7dec-11d0-a765-00a0c91e6bf6", errc::syntax, 8},
        {"f81d4fae-7dec-11d0-a765+00a0c91e6bf6", errc::syntax, 23},
        {"g81d4fae-7dec-11d0-a765-00a0c91e6bf6", errc::invalid_character, 0},
        {"f81d4fae-7dec-11d0-a765-00a0c91e6bfx", errc::invalid_character, 35},
        {"{f81d4fae-7dec-11d0-a765-00a0c91e6bf6)", errc::syntax, 37},
        {"(f81d4fae-7dec-11d0-a765-00a0c91e6bf6}", errc::syntax, 0},
        {"urn:uid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6a", errc::syntax, 45},
        {"f81d4fae7dec11d0a76500a0c91e6bfZ", errc::invalid_character, 31},
        {"f81d4fae-7dec-11d0-a765-00a0c91e6b-6", errc::invalid_character, 34},
    };
    for (auto& b : bad) {
        auto u = uuid::parse(b.text);
        ASSERT_FALSE(u) << b.text;
        EXPECT_EQ(u.error().code(), b.code) << b.text;
        EXPECT_EQ(u.error().offset(), b.offset) << b.text << ": " << u.error().message();
    }
    EXPECT_EQ(uuid::parse("x").error().message(), "offset 1: not a UUID: its length is 1");
    EXPECT_THROW(uuid(sgcl::string("nope")), sgcl::bad_expected_access<error>);
}

TEST(Uuid_Tests, Boundaries) {
    uuid nil;
    EXPECT_TRUE(nil.is_nil());
    EXPECT_EQ(nil, uuid::nil());
    EXPECT_EQ(str(nil), "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(nil.version(), 0);
    EXPECT_EQ(nil.variant(), uuid::variant_kind::ncs);
    EXPECT_FALSE(nil.timestamp());
    EXPECT_EQ(str(uuid::max()), "ffffffff-ffff-ffff-ffff-ffffffffffff");
    EXPECT_EQ(uuid::max().version(), 15);
    EXPECT_EQ(uuid::max().variant(), uuid::variant_kind::future);
    EXPECT_FALSE(uuid::max().is_nil());
    EXPECT_LT(nil, uuid::max());
    static_assert(uuid("00000000-0000-0000-0000-000000000001") > uuid());
    constexpr uuid literal("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    static_assert(literal.version() == 1);
    // bytes, both ways
    std::vector<byte> b(16);
    for (size_t i = 0; i < 16; ++i) {
        b[i] = byte(uint8_t(i * 17));
    }
    auto u = uuid::from_bytes(as_slice(b));
    ASSERT_TRUE(u);
    EXPECT_EQ(str(*u), "00112233-4455-6677-8899-aabbccddeeff");
    EXPECT_EQ(uuid(u->bytes()), *u);
    EXPECT_EQ(uuid::from_bytes(sgcl::slice<const byte>(b.data(), 15)).error().code(), errc::syntax);
    EXPECT_FALSE(uuid::from_bytes(sgcl::slice<const byte>()));
    std::vector<byte> seventeen(17);
    EXPECT_FALSE(uuid::from_bytes(as_slice(seventeen)));
    // text round trip, hashing, formatting
    for (int i = 0; i < 1000; ++i) {
        auto x = uuid::v4();
        EXPECT_EQ(uuid::parse(x.to_string()).value(), x);
    }
    sgcl::set<uuid> keys;
    keys.insert(literal);
    keys.insert(uuid("6BA7B810-9DAD-11D1-80B4-00C04FD430C8"));
    EXPECT_EQ(keys.size(), 1u);
    EXPECT_EQ(std::hash<uuid>()(literal), literal.hash());
    EXPECT_EQ(sgcl::txt::format("{}", literal), "6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    EXPECT_EQ(sgcl::txt::format("[{:>38}]", literal), "[  6ba7b810-9dad-11d1-80b4-00c04fd430c8]");
}
