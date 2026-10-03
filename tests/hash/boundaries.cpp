//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of every hasher (DESIGN 408): every empty form of the data,
// a null one among them, is nothing; a copy and an assignment to itself are
// the same hasher; a stream that is empty, fails at once or throws half-way;
// combine and resume over zero lengths and at the limits of their values;
// seeds and keys at their limits; a slice of more than 4 GiB in one call.
#include "common.h"

using namespace sgcl::async;

#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

#if !defined(_WIN32)
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
    using namespace hash_test;
    namespace io = sgcl::io;

    template<class H>
    class Hash_Boundaries : public ::testing::Test {};

    TYPED_TEST_SUITE(Hash_Boundaries, All);

    // A reader that hands out n bytes of 'x' in pieces of at most 1000 and
    // then throws, from read and from async_read alike
    class throwing final : public io::mixin::reader<throwing> {
    public:
        explicit throwing(size_t n) : _left(n) {}

        expected<size_t, io::error> read(sgcl::slice<byte> out) {
            if (_left == 0) {
                throw std::runtime_error("the stream broke");
            }
            size_t k = std::min({out.size(), _left, size_t(1000)});
            std::memset(out.data(), 'x', k);
            _left -= k;
            return k;
        }

        sgcl::async::task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }

    private:
        size_t _left;
    };

    // A reader that fails at its first read
    class broken final : public io::mixin::reader<broken> {
    public:
        expected<size_t, io::error> read(sgcl::slice<byte>) {
            return sgcl::unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "broken"));
        }

        sgcl::async::task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }
    };
}

// Every empty form of the data, the null ones among them (a slice, a
// string, a std::string_view and a std::span made with nothing), is
// nothing: through update, before and after other data, and through of
TYPED_TEST(Hash_Boundaries, EveryEmptyFormIsNothing) {
    using H = TypeParam;
    const auto none = text(fresh<H>().value());
    sgcl::slice<const byte> null_bytes;
    sgcl::slice<const char> null_text;
    sgcl::slice<char> null_chars;
    sgcl::string empty_string;
    std::string_view null_view;
    std::span<const byte> null_span;
    std::span<byte> null_writable;
    char nul_only[1] = {0};
    const char* empty_c = "";
    H h = fresh<H>();
    h.update(null_bytes);
    h.update(null_text);
    h.update(null_chars);
    h.update(empty_string);
    h.update(null_view);
    h.update(null_span);
    h.update(null_writable);
    h.update(nul_only);
    h.update(empty_c);
    h.update("");
    EXPECT_EQ(text(h.value()), none);
    EXPECT_EQ(text(one<H>(null_bytes)), none);
    EXPECT_EQ(text(one<H>(empty_string)), none);
    EXPECT_EQ(text(one<H>(null_view)), none);
    EXPECT_EQ(text(one<H>(null_span)), none);
    EXPECT_EQ(text(one<H>(nul_only)), none);
    EXPECT_EQ(text(one<H>(empty_c)), none);
    // between pieces of data, at every fill of a block
    auto data = pattern(0, 300);
    for (size_t cut : {size_t(1), size_t(7), size_t(8), size_t(63), size_t(64), size_t(256), size_t(299)}) {
        H g = fresh<H>();
        g.update(null_bytes);
        g.update(bytes(data.data(), cut));
        g.update(null_bytes);
        g.update(null_view);
        g.update(bytes(data.data() + cut, data.size() - cut));
        g.update(null_span);
        EXPECT_EQ(text(g.value()), of<H>(data)) << cut;
    }
    // a hasher reset after data is the empty one again, and its digest the empty one's
    H r = fresh<H>();
    r.update(bytes(data));
    r.reset();
    EXPECT_EQ(text(r.value()), none);
    EXPECT_EQ(text(r.digest()), text(fresh<H>().digest()));
}

// One byte of every value: the shortest input, through update and of
TYPED_TEST(Hash_Boundaries, OneByteOfEveryValue) {
    using H = TypeParam;
    for (unsigned v = 0; v < 256; ++v) {
        unsigned char b = (unsigned char)v;
        H h = fresh<H>();
        h.update(bytes(&b, 1));
        ASSERT_EQ(text(h.value()), of<H>(&b, 1)) << v;
    }
}

// A copy and an assignment to itself leave the hasher as it was; a hasher
// moved from is a copy, since it is a plain value
TYPED_TEST(Hash_Boundaries, AssignedToItselfOrMovedFrom) {
    using H = TypeParam;
    auto data = pattern(0, 500);
    H h = fresh<H>();
    h.update(bytes(data.data(), 333));
    const auto before = text(h.value());
    H& same = h;
    h = same;
    EXPECT_EQ(text(h.value()), before);
    h = std::move(same);
    EXPECT_EQ(text(h.value()), before);
    H moved = std::move(h);
    EXPECT_EQ(text(moved.value()), before);
    EXPECT_EQ(text(h.value()), before);   // NOLINT: a plain value, the move copied it
    h.update(bytes(data.data() + 333, 167));
    EXPECT_EQ(text(h.value()), of<H>(data));
    // a hasher's own digest hashed into it: a value made before the call
    H d = fresh<H>();
    d.update(bytes(data));
    H e = d;
    d.update(d.digest());
    auto digest = e.digest();
    e.update(digest);
    EXPECT_EQ(text(d.value()), text(e.value()));
}

// A stream with nothing in it is 0 bytes and the hasher untouched; one that
// fails at once is its error and the hasher untouched
TYPED_TEST(Hash_Boundaries, CopyFromAnEmptyOrABrokenStream) {
    using H = TypeParam;
    const auto none = text(fresh<H>().value());
    H h = fresh<H>();
    io::buffer empty;
    auto n = h.copy_from(empty);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 0u);
    EXPECT_EQ(text(h.value()), none);
    sgcl::tracked_ptr r = sgcl::make_tracked<broken>();
    auto e = h.copy_from(*r);
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), std::make_error_code(std::errc::io_error));
    EXPECT_EQ(text(h.value()), none);
    h.update("abc");
    auto again = h.copy_from(*r);
    EXPECT_FALSE(again);
    EXPECT_EQ(text(h.value()), text(one<H>("abc")));
}

// A read that throws half-way: the exception comes out of copy_from, and
// out of the co_await of async_copy_from; what was read before it has been
// hashed
TYPED_TEST(Hash_Boundaries, CopyFromAStreamThatThrowsHalfWay) {
    using H = TypeParam;
    std::vector<unsigned char> xs(2500, 'x');
    const auto read_before = of<H>(xs);
    sgcl::tracked_ptr r = sgcl::make_tracked<throwing>(2500);
    H h = fresh<H>();
    EXPECT_THROW((void)h.copy_from(*r), std::runtime_error);
    EXPECT_EQ(text(h.value()), read_before);
    sgcl::tracked_ptr at_once = sgcl::make_tracked<throwing>(0);
    H g = fresh<H>();
    EXPECT_THROW((void)g.copy_from(*at_once), std::runtime_error);
    EXPECT_EQ(text(g.value()), text(fresh<H>().value()));

    auto t = sgcl::async::spawn([]() -> sgcl::async::task<std::string> {
        sgcl::tracked_ptr s = sgcl::make_tracked<throwing>(2500);
        H a = fresh<H>();
        try {
            (void)co_await a.async_copy_from(*s);
        } catch (const std::runtime_error&) {
            co_return text(a.value());
        }
        co_return "no exception";
    });
    EXPECT_EQ(t.wait(), read_before);
    sgcl::async::scheduler::stop();
}

// The CRCs: combine over pieces of no length and from the CRC of nothing
// (0), resume from the CRC of nothing and from the limits of the value
namespace {
    template<class H>
    class Hash_CrcBoundaries : public ::testing::Test {};

    using Crcs = ::testing::Types<hash::crc32, hash::crc32c, hash::crc64, hash::crc64_iso>;
    TYPED_TEST_SUITE(Hash_CrcBoundaries, Crcs);
}

TYPED_TEST(Hash_CrcBoundaries, CombineAndResumeAtZeroLengths) {
    using H = TypeParam;
    using V = decltype(H().value());
    const V none = H().value();
    EXPECT_EQ(none, V(0));   // the CRC of nothing, these four have a final XOR equal to their start
    auto data = pattern(0, 1000);
    const V a = H::of(bytes(data));
    EXPECT_EQ(H::combine(none, none, 0), none);
    EXPECT_EQ(H::combine(a, none, 0), a);      // an empty second piece
    EXPECT_EQ(H::combine(none, a, 1000), a);   // an empty first piece
    static_assert(H::combine(0, 0, 0) == 0);
    // a nonzero "CRC of nothing" is no CRC of an empty piece: combine takes it as given
    EXPECT_EQ(H::combine(a, V(1), 0), V(a ^ V(1)));
    for (V v : {V(0), V(1), std::numeric_limits<V>::max(), V(std::numeric_limits<V>::max() - 1), a}) {
        H h = H::resume(v);
        EXPECT_EQ(h.value(), v);          // nothing after it: the value given
        h.update(sgcl::slice<const byte>());
        EXPECT_EQ(h.value(), v);
        h.update(bytes(data.data(), 10));
        EXPECT_EQ(h.value(), H::combine(v, H::of(bytes(data.data(), 10)), 10)) << v;
        h.reset();
        EXPECT_EQ(h.value(), none);       // reset forgets the value resumed from
    }
    EXPECT_EQ(H::resume(none).value(), H().value());
    // the longest second piece a length can say, from the CRC of nothing and from a value
    const uint64_t longest = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ(H::combine(none, a, longest), a);   // zeros moved any distance are zeros
    EXPECT_EQ(H::combine(H::combine(a, 0, longest), 0, 1), H::combine(H::combine(a, 0, 1), 0, longest));
}

// Adler-32: the checksum of nothing is 1, not 0; combine over empty pieces;
// resume from 1, from 0 and from the largest valid sums
TEST(Hash_AdlerBoundaries, CombineAndResumeAtZeroLengths) {
    const uint32_t none = hash::adler32().value();
    EXPECT_EQ(none, 1u);
    auto data = pattern(2, 6000);   // 0xFF: the sums at their largest
    const uint32_t a = hash::adler32::of(bytes(data));
    EXPECT_EQ(hash::adler32::combine(none, none, 0), none);
    EXPECT_EQ(hash::adler32::combine(a, none, 0), a);
    EXPECT_EQ(hash::adler32::combine(none, a, data.size()), a);
    static_assert(hash::adler32::combine(1, 1, 0) == 1);
    const uint32_t largest = 65520u << 16 | 65520u;   // both sums one below the modulus
    for (uint32_t v : {uint32_t(0), uint32_t(1), largest, a}) {
        hash::adler32 h = hash::adler32::resume(v);
        EXPECT_EQ(h.value(), v);
        h.update(sgcl::slice<const byte>());
        EXPECT_EQ(h.value(), v);
        h.update(bytes(data.data(), 5600));   // past one run of the vector loop
        EXPECT_EQ(h.value(), hash::adler32::combine(v, hash::adler32::of(bytes(data.data(), 5600)), 5600)) << v;
        h.reset();
        EXPECT_EQ(h.value(), none);
    }
    // the longest length: n modulo 65521 is what counts, so 2^64 - 1 is 2^64 - 1 mod 65521 bytes of nothing
    const uint64_t longest = std::numeric_limits<uint64_t>::max();
    const uint64_t rest = longest % 65521;
    EXPECT_EQ(hash::adler32::combine(a, none, longest), hash::adler32::combine(a, none, rest));
    EXPECT_EQ(hash::adler32::combine(largest, largest, longest), hash::adler32::combine(largest, largest, rest));
    // values whose halves are past the modulus are taken modulo, as resume takes them
    EXPECT_EQ(hash::adler32::combine(0xFFFFFFFFu, none, 0), hash::adler32::resume(0xFFFFFFFFu).value());
}

// The FNVs: resume from the value of nothing (the offset basis) is a new
// hasher, from 0 and from the largest value goes on by the definition
namespace {
    template<class H, class V>
    void fnv_resumes_at_the_limits(const std::vector<V>& values) {
        auto data = pattern(0, 100);
        EXPECT_EQ(text(H::resume(H().value()).value()), text(H().value()));
        for (const V& v : values) {
            H h = H::resume(v);
            EXPECT_EQ(text(h.value()), text(v));
            h.update(sgcl::slice<const byte>());
            EXPECT_EQ(text(h.value()), text(v));
            h.update(bytes(data));
            H g = H::resume(v);
            for (auto b : data) {
                g.update(bytes(&b, 1));
            }
            EXPECT_EQ(text(h.value()), text(g.value()));
            h.reset();
            EXPECT_EQ(text(h.value()), text(H().value()));
        }
    }
}

TEST(Hash_FnvBoundaries, ResumeAtTheLimits) {
    fnv_resumes_at_the_limits<hash::fnv32>(std::vector<uint32_t>{0u, 0xFFFFFFFFu});
    fnv_resumes_at_the_limits<hash::fnv32a>(std::vector<uint32_t>{0u, 0xFFFFFFFFu});
    fnv_resumes_at_the_limits<hash::fnv64>(std::vector<uint64_t>{0u, ~uint64_t(0)});
    fnv_resumes_at_the_limits<hash::fnv64a>(std::vector<uint64_t>{0u, ~uint64_t(0)});
    sgcl::array<byte, 16> zeros {}, ones;
    for (auto& b : ones) {
        b = byte(0xff);
    }
    fnv_resumes_at_the_limits<hash::fnv128>(std::vector<sgcl::array<byte, 16>>{zeros, ones});
    fnv_resumes_at_the_limits<hash::fnv128a>(std::vector<sgcl::array<byte, 16>>{zeros, ones});
    // FNV-0: from the value 0 a run of zero bytes stays 0
    hash::fnv64 z = hash::fnv64::resume(0);
    z.update(bytes(pattern(1, 50)));
    EXPECT_EQ(z.value(), 0u);
}

// Seeds and keys at their limits: the seed 0 is the hasher made without
// one, the largest seed hashes in pieces as in one call, at every length
// about the thresholds of XXH3 (16, 128, 240 bytes, a stripe, a block)
TEST(Hash_SeededBoundaries, SeedsAndKeysAtTheirLimits) {
    auto data = pattern(0, 1100);
    const uint64_t largest = std::numeric_limits<uint64_t>::max();
    for (size_t n : {0, 1, 3, 4, 8, 9, 16, 17, 128, 129, 240, 241, 255, 256, 257, 1024, 1025, 1100}) {
        auto in = bytes(data.data(), n);
        EXPECT_EQ(hash::xxh3_64::of(in, 0), hash::xxh3_64::of(in)) << n;
        EXPECT_EQ(text(hash::xxh3_128::of(in, 0)), text(hash::xxh3_128::of(in))) << n;
        hash::xxh3_64 a(0), b;
        a.update(in);
        b.update(in);
        EXPECT_EQ(a.value(), b.value()) << n;
        for (uint64_t seed : {uint64_t(1), largest}) {
            hash::xxh3_64 x(seed);
            hash::xxh3_128 y(seed);
            hash::maphash m(seed);
            for (size_t at = 0; at < n; at += 7) {
                auto piece = bytes(data.data() + at, std::min<size_t>(7, n - at));
                x.update(piece);
                y.update(piece);
                m.update(piece);
            }
            EXPECT_EQ(x.value(), hash::xxh3_64::of(in, seed)) << n << " " << seed;
            EXPECT_EQ(text(y.value()), text(hash::xxh3_128::of(in, seed))) << n << " " << seed;
            EXPECT_EQ(m.value(), hash::maphash::of(in, seed)) << n << " " << seed;
        }
        sgcl::array<byte, 16> zeros {}, ones;
        for (auto& k : ones) {
            k = byte(0xff);
        }
        for (const auto& key : {zeros, ones}) {
            hash::siphash s(key);
            for (size_t at = 0; at < n; at += 5) {
                s.update(bytes(data.data() + at, std::min<size_t>(5, n - at)));
            }
            EXPECT_EQ(s.value(), hash::siphash::of(in, key)) << n;
        }
    }
    sgcl::array<byte, 16> zero_key {}, last_bit {};
    last_bit[15] = byte(1);
    EXPECT_NE(hash::siphash::of("", zero_key), hash::siphash::of("", last_bit));
    // the length goes into SipHash's last word modulo 256: 256 bytes are not 0
    auto zeros = pattern(1, 256);
    EXPECT_NE(hash::siphash::of(bytes(zeros), sip_key()), hash::siphash::of("", sip_key()));
}

// maphash::update_value at the smallest and largest object: a byte and a
// struct of 300 bytes are their bytes
TEST(Hash_SeededBoundaries, UpdateValueAtItsSizes) {
    struct big {
        unsigned char bytes[300];
    };
    big b;
    for (size_t i = 0; i < sizeof b.bytes; ++i) {
        b.bytes[i] = (unsigned char)(i * 13);
    }
    hash::maphash m(5), n(5);
    m.update_value(b);
    n.update(bytes(b.bytes, sizeof b.bytes));
    EXPECT_EQ(m.value(), n.value());
    unsigned char one = 0xab;
    hash::maphash p(6), q(6);
    p.update_value(one);
    q.update(bytes(&one, 1));
    EXPECT_EQ(p.value(), q.value());
}

// of_file and async_of_file of an empty file are the hash of nothing; of a
// directory, the error of its read
TEST(Hash_OfFileBoundaries, AnEmptyFileAndADirectory) {
    auto dir = io::make_temp_dir({}, "hash-test-*");
    ASSERT_TRUE(dir);
    sgcl::string path = io::path::join(*dir, "empty.bin");
    ASSERT_TRUE(io::write_file(path, sgcl::slice<const byte>()));
    auto crc = hash::crc32::of_file(path);
    ASSERT_TRUE(crc);
    EXPECT_EQ(*crc, 0u);
    auto adler = hash::adler32::of_file(path);
    ASSERT_TRUE(adler);
    EXPECT_EQ(*adler, 1u);
    // a class with a seed hashes as one made without it: maphash with the
    // process's seed, not with the seed 0
    sgcl::string some = io::path::join(*dir, "some.bin");
    ASSERT_TRUE(io::write_file(some, "some bytes"));
    auto m = hash::maphash::of_file(some);
    ASSERT_TRUE(m);
    EXPECT_EQ(*m, hash::maphash::of("some bytes"));
    EXPECT_NE(*m, hash::maphash::of("some bytes", 0));
    auto x = hash::xxh3_64::of_file(some);
    ASSERT_TRUE(x);
    EXPECT_EQ(*x, hash::xxh3_64::of("some bytes", 0));
    auto t = sgcl::async::spawn([path, d = *dir]() -> sgcl::async::task<std::string> {
        auto empty = co_await hash::xxh3_64::async_of_file(path);
        auto directory = co_await hash::xxh3_64::async_of_file(d);
        if (!empty) {
            co_return "the empty file was not read";
        }
        if (directory) {
            co_return "a directory was read";
        }
        co_return text(*empty);
    });
    EXPECT_EQ(t.wait(), text(hash::xxh3_64::of("")));
    sgcl::async::scheduler::stop();
    io::remove_all(*dir);
}

#if !defined(_WIN32)
namespace {
    // A slice of more than 4 GiB that costs 64 MiB: one file of 64 MiB
    // mapped 65 times one after another into a reserved range. Its unit
    // is the random pattern, so no two places of the slice line up by chance
    class huge_input {
    public:
        static constexpr size_t unit = size_t(64) << 20;
        static constexpr size_t units = 65;

        huge_input() {
            char name[] = "/tmp/sgcl-hash-huge-XXXXXX";
            int fd = mkstemp(name);
            if (fd < 0) {
                return;
            }
            unlink(name);
            _unit = pattern(0, unit);
            bool written = true;
            for (size_t at = 0; at < unit && written;) {
                ssize_t w = ::write(fd, _unit.data() + at, unit - at);
                written = w > 0;
                at += written ? size_t(w) : 0;
            }
            void* base = written ? mmap(nullptr, unit * units, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0) : MAP_FAILED;
            if (base != MAP_FAILED) {
                _base = static_cast<unsigned char*>(base);
                for (size_t i = 0; i < units; ++i) {
                    if (mmap(_base + i * unit, unit, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
                        munmap(_base, unit * units);
                        _base = nullptr;
                        break;
                    }
                }
            }
            close(fd);
        }

        ~huge_input() {
            if (_base) {
                munmap(_base, unit * units);
            }
        }

        const unsigned char* data() const {
            return _base;
        }

        const std::vector<unsigned char>& one_unit() const {
            return _unit;
        }

    private:
        unsigned char* _base = nullptr;
        std::vector<unsigned char> _unit;
    };

    const huge_input& huge() {
        static huge_input h;
        return h;
    }

    // 4 GiB and 5 bytes, in one call, against the units joined by combine
    template<class H>
    void combine_past_4_gib() {
        const auto& in = huge();
        if (!in.data()) {
            GTEST_SKIP() << "no 4 GiB of address space";
        }
        const size_t n = (size_t(1) << 32) + 5;
        auto whole = H::of(bytes(in.data(), n));
        auto unit = H::of(bytes(in.one_unit()));
        auto joined = unit;
        for (size_t i = 1; i < 64; ++i) {
            joined = H::combine(joined, unit, huge_input::unit);
        }
        joined = H::combine(joined, H::of(bytes(in.data(), 5)), 5);
        EXPECT_EQ(text(whole), text(joined));
        H streamed;
        streamed.update(bytes(in.data(), n - 3));
        streamed.update(bytes(in.data() + n - 3, 3));
        EXPECT_EQ(text(streamed.value()), text(whole));
    }

    // the same for a hasher without combine: one call against the hasher fed in pieces of a unit
    // (and, with whole_update, one update of the whole length)
    template<class H>
    void pieces_past_4_gib(bool whole_update) {
        const auto& in = huge();
        if (!in.data()) {
            GTEST_SKIP() << "no 4 GiB of address space";
        }
        const size_t n = (size_t(1) << 32) + 5;
        auto whole = one<H>(bytes(in.data(), n));
        H h = fresh<H>();
        for (size_t at = 0; at < n; at += huge_input::unit) {
            h.update(bytes(in.data() + at, std::min(huge_input::unit, n - at)));
        }
        EXPECT_EQ(text(h.value()), text(whole));
        if (whole_update) {
            H big = fresh<H>();
            big.update(bytes(in.data(), n));
            EXPECT_EQ(text(big.value()), text(whole));
        }
    }
}

// Lengths past 2^32 in one call: nothing counts them in 32 bits
TEST(Hash_HugeInput, Crc32PastFourGiB) {
    combine_past_4_gib<hash::crc32>();
}

TEST(Hash_HugeInput, Crc64PastFourGiB) {
    combine_past_4_gib<hash::crc64>();
}

TEST(Hash_HugeInput, Adler32PastFourGiB) {
    combine_past_4_gib<hash::adler32>();
}

TEST(Hash_HugeInput, Xxh3PastFourGiB) {
    pieces_past_4_gib<hash::xxh3_64>(true);
    pieces_past_4_gib<hash::xxh3_128>(true);
}

TEST(Hash_HugeInput, SipHashPastFourGiB) {
    pieces_past_4_gib<hash::siphash>(false);   // its update and its one-shot share no loop
}
#endif
