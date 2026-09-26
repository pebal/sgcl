//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The common shape, over every hasher of the module: data in pieces hashes
// as data in one call, wherever it lies in memory; value() ends nothing, a
// copy is a branch, reset() is new; the text overloads are the bytes; the
// digest is the value's bytes, most significant first; copy_from reads a
// stream to its end, in whatever pieces the stream gives.
#include "common.h"

using namespace sgcl::async;

#include <memory>
#include <random>
#include <span>
#include <string_view>

namespace {
    using namespace hash_test;
    namespace io = sgcl::io;

    template<class H>
    class Hash_Common : public ::testing::Test {};

    TYPED_TEST_SUITE(Hash_Common, All);

    // A reader that hands out its bytes in pieces of at most n
    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(const std::vector<unsigned char>& data, size_t n) : _data(data), _n(n) {}

        expected<size_t, io::error> read(sgcl::slice<byte> out) {
            size_t k = std::min({out.size(), _n, _data.size() - _at});
            std::memcpy(out.data(), _data.data() + _at, k);
            _at += k;
            return k;
        }

        sgcl::async::task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }

    private:
        std::vector<unsigned char> _data;
        size_t _n;
        size_t _at = 0;
    };

    // A reader that fails after its bytes
    class failing final : public io::mixin::reader<failing> {
    public:
        explicit failing(size_t n) : _left(n) {}

        expected<size_t, io::error> read(sgcl::slice<byte> out) {
            if (_left == 0) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "failing"));
            }
            size_t k = std::min(out.size(), _left);
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
}

TYPED_TEST(Hash_Common, StaticShape) {
    using H = TypeParam;
    static_assert(hash::req::hasher<H>);
    static_assert(std::is_trivially_copyable_v<H>);
    static_assert(std::is_trivially_destructible_v<H>);
    static_assert(sizeof(decltype(std::declval<const H&>().digest())) == H::digest_size);
    static_assert(noexcept(std::declval<H&>().update(std::declval<const sgcl::slice<const byte>&>())));
    if constexpr (std::is_same_v<H, hash::siphash>) {
        static_assert(noexcept(H::of("x", std::declval<const sgcl::array<byte, 16>&>())));
        static_assert(!std::is_default_constructible_v<H>);   // the key is mandatory
    } else {
        static_assert(noexcept(H::of("x")));
    }
    static_assert(noexcept(std::declval<const H&>().value()));
    SUCCEED();
}

// Every split of every input up to 300 bytes into two pieces, and a split
// into three at random places; then one byte at a time
TYPED_TEST(Hash_Common, PiecesHashAsOneCall) {
    using H = TypeParam;
    auto data = pattern(0, 300);
    for (size_t n = 0; n <= 300; ++n) {
        auto whole = of<H>(data.data(), n);
        for (size_t cut = 0; cut <= n; ++cut) {
            H h = fresh<H>();
            h.update(bytes(data.data(), cut));
            h.update(bytes(data.data() + cut, n - cut));
            ASSERT_EQ(text(h.value()), whole) << "n " << n << " cut " << cut;
        }
    }
    std::mt19937 rng(5);
    for (int round = 0; round < 2000; ++round) {
        size_t n = rng() % 301;
        size_t a = rng() % (n + 1);
        size_t b = a + rng() % (n - a + 1);
        H h = fresh<H>();
        h.update(bytes(data.data(), a));
        h.update(bytes(data.data() + a, b - a));
        h.update(bytes(data.data() + b, n - b));
        ASSERT_EQ(text(h.value()), of<H>(data.data(), n)) << "n " << n << " at " << a << ", " << b;
    }
    auto longer = pattern(0, 5000);
    H h = fresh<H>();
    for (size_t i = 0; i < longer.size(); ++i) {
        h.update(bytes(longer.data() + i, 1));
    }
    EXPECT_EQ(text(h.value()), of<H>(longer));
}

// A megabyte in pieces of sizes about the thresholds of the paths (8, 32,
// 64, 128) and odd ones, against one call
TYPED_TEST(Hash_Common, LargeInputInPieces) {
    using H = TypeParam;
    auto data = pattern(0, (1 << 20) + 3);
    auto whole = of<H>(data);
    for (size_t piece : {1, 2, 3, 7, 8, 9, 31, 33, 63, 64, 65, 127, 128, 129, 240, 241, 255, 256, 257, 1000, 1023, 1024, 1025, 4097, 65536, 100003}) {
        H h = fresh<H>();
        for (size_t at = 0; at < data.size(); at += piece) {
            h.update(bytes(data.data() + at, std::min(piece, data.size() - at)));
        }
        EXPECT_EQ(text(h.value()), whole) << "pieces of " << piece;
    }
}

// Every offset 0…15 of the start, for every length 0…300 and a megabyte:
// the arm64 path loads 16 bytes at a time without asking for alignment
TYPED_TEST(Hash_Common, AnyAlignment) {
    using H = TypeParam;
    auto data = pattern(0, (1 << 20) + 1);   // random: no period for a misplaced load to hide in
    std::vector<unsigned char> room(data.size() + 32);
    for (size_t offset = 0; offset < 16; ++offset) {
        std::memcpy(room.data() + offset, data.data(), data.size());
        for (size_t n = 0; n <= 300; ++n) {
            ASSERT_EQ(of<H>(room.data() + offset, n), of<H>(data.data(), n)) << "offset " << offset << " n " << n;
        }
        EXPECT_EQ(of<H>(room.data() + offset, data.size()), of<H>(data)) << "offset " << offset;
    }
}

// Each input in a heap block that ends where the input ends, the input at
// every offset 0…15 from the block's start: under the address sanitizer a
// load of 16 or 8 bytes past the end is a failure here rather than a quiet
// read of the neighbour
TYPED_TEST(Hash_Common, ReadsNothingPastTheEnd) {
    using H = TypeParam;
    auto data = pattern(0, 1100);
    for (size_t offset = 0; offset < 16; ++offset) {
        for (size_t n = 0; n <= 1100; n += n < 300 ? 1 : 37) {
            std::unique_ptr<unsigned char[]> block(new unsigned char[offset + n]);
            std::memcpy(block.get() + offset, data.data(), n);
            ASSERT_EQ(of<H>(block.get() + offset, n), of<H>(data.data(), n)) << "offset " << offset << " n " << n;
        }
    }
}

// An array of char is read to its first NUL or to its end, never past it:
// a buffer with no NUL is its four bytes, not what lies after it; a literal
// with a NUL inside stops there, as a std::string_view made from it would.
// A pointer, const or not, is a C string
TYPED_TEST(Hash_Common, CharArraysStopAtTheirEnd) {
    using H = TypeParam;
    struct {
        char buffer[4];
        char after[4];
    } s = {{'a', 'b', 'c', 'd'}, {'x', 'y', 'z', 0}};
    auto four = text(one<H>(std::string_view("abcd")));
    H h = fresh<H>();
    h.update(s.buffer);
    EXPECT_EQ(text(h.value()), four);
    EXPECT_EQ(text(one<H>(s.buffer)), four);
    char padded[8] = "ab";
    EXPECT_EQ(text(one<H>(padded)), text(one<H>(std::string_view("ab"))));
    EXPECT_EQ(text(one<H>("a\0b")), text(one<H>(std::string_view("a"))));
    char* mutable_pointer = padded;
    const char* pointer = padded;
    H a = fresh<H>(), b = fresh<H>();
    a.update(mutable_pointer);
    b.update(pointer);
    EXPECT_EQ(text(a.value()), text(one<H>(std::string_view("ab"))));
    EXPECT_EQ(text(b.value()), text(one<H>(std::string_view("ab"))));
    EXPECT_EQ(text(one<H>(mutable_pointer)), text(one<H>(std::string_view("ab"))));
}

// The forms bytes come in besides the slice: a slice of mutable chars,
// another hasher's digest, a std::span of bytes const or not
TYPED_TEST(Hash_Common, OtherFormsOfBytes) {
    using H = TypeParam;
    char chars[3] = {'q', 'r', 's'};
    sgcl::slice<char> mutable_text(chars, 3);
    H t = fresh<H>();
    t.update(mutable_text);
    EXPECT_EQ(text(t.value()), text(one<H>(std::string_view("qrs", 3))));
    EXPECT_EQ(text(one<H>(mutable_text)), text(one<H>(std::string_view("qrs", 3))));

    auto data = pattern(0, 40);
    hash::fnv128a g;
    g.update(bytes(data));
    auto d = g.digest();
    auto expected = of<H>(reinterpret_cast<const unsigned char*>(d.data()), d.size());
    H h = fresh<H>();
    h.update(d);
    EXPECT_EQ(text(h.value()), expected);
    EXPECT_EQ(text(one<H>(d)), expected);

    std::vector<byte> v(reinterpret_cast<const byte*>(data.data()), reinterpret_cast<const byte*>(data.data()) + data.size());
    std::span<byte> writable(v);
    std::span<const byte> readable(v);
    H s1 = fresh<H>(), s2 = fresh<H>();
    s1.update(writable);
    s2.update(readable);
    EXPECT_EQ(text(s1.value()), of<H>(data));
    EXPECT_EQ(text(s2.value()), of<H>(data));
    EXPECT_EQ(text(one<H>(writable)), of<H>(data));
    EXPECT_EQ(text(one<H>(readable)), of<H>(data));
    EXPECT_EQ(text(one<H>(v)), of<H>(data));
}

TYPED_TEST(Hash_Common, ValueEndsNothingCopyBranchesResetIsNew) {
    using H = TypeParam;
    auto data = pattern(0, 700);
    H h = fresh<H>();
    h.update(bytes(data.data(), 300));
    auto midway = h.value();
    EXPECT_EQ(text(midway), of<H>(data.data(), 300));
    H branch = h;
    h.update(bytes(data.data() + 300, 400));
    EXPECT_EQ(text(h.value()), of<H>(data));
    EXPECT_EQ(text(branch.value()), text(midway));
    branch.update(bytes(data.data() + 300, 10));
    EXPECT_EQ(text(branch.value()), of<H>(data.data(), 310));
    h.reset();
    EXPECT_EQ(text(h.value()), text(fresh<H>().value()));
    h.update(bytes(data.data(), 5));
    EXPECT_EQ(text(h.value()), of<H>(data.data(), 5));
}

// A string, a text slice, a literal, a pointer, a std::string_view and a
// std::string (through its view) are the same bytes, through update() and
// through of()
TYPED_TEST(Hash_Common, TextIsItsBytes) {
    using H = TypeParam;
    const char* chars = "zażółć gęślą jaźń";
    size_t n = std::strlen(chars);
    auto expected = of<H>(reinterpret_cast<const unsigned char*>(chars), n);
    sgcl::string s = chars;
    std::string std_text = chars;
    EXPECT_EQ(text(one<H>(s)), expected);
    EXPECT_EQ(text(one<H>(s.as_slice())), expected);
    EXPECT_EQ(text(one<H>("zażółć gęślą jaźń")), expected);
    EXPECT_EQ(text(one<H>(chars)), expected);
    EXPECT_EQ(text(one<H>(std::string_view(chars))), expected);
    EXPECT_EQ(text(one<H>(std_text)), expected);
    H a = fresh<H>(), b = fresh<H>(), c = fresh<H>(), d = fresh<H>(), e = fresh<H>();
    a.update(s);
    b.update(s.as_slice());
    c.update("zażółć gęślą jaźń");
    d.update(std::string_view(chars, 7));
    d.update(chars + 7);
    e.update(std_text);
    EXPECT_EQ(text(a.value()), expected);
    EXPECT_EQ(text(b.value()), expected);
    EXPECT_EQ(text(c.value()), expected);
    EXPECT_EQ(text(d.value()), expected);
    EXPECT_EQ(text(e.value()), expected);
    EXPECT_EQ(text(one<H>("")), text(fresh<H>().value()));
}

// digest() is value() as bytes, the most significant first; for the 128-bit
// ones the two are the same thing
TYPED_TEST(Hash_Common, DigestIsTheValueBigEndian) {
    using H = TypeParam;
    auto data = pattern(0, 77);
    H h = fresh<H>();
    h.update(bytes(data));
    auto d = h.digest();
    auto v = h.value();
    if constexpr (std::is_integral_v<decltype(v)>) {
        uint64_t back = 0;
        for (auto b : d) {
            back = back << 8 | uint64_t(b);
        }
        EXPECT_EQ(back, uint64_t(v));
    } else {
        EXPECT_EQ(text(d), text(v));
    }
}

TYPED_TEST(Hash_Common, CopyFromAStream) {
    using H = TypeParam;
    for (size_t n : {size_t(0), size_t(1), size_t(1000), size_t(100000)}) {
        auto data = pattern(0, n);
        auto expected = of<H>(data);
        for (size_t piece : {1, 2, 3, 7, 4096, 1 << 20}) {
            if (piece < 4 && n > 1000) {
                continue;   // a hundred thousand reads of one byte say nothing more
            }
            sgcl::tracked_ptr r = sgcl::make_tracked<dribble>(data, piece);
            H h = fresh<H>();
            auto got = h.copy_from(*r);
            ASSERT_TRUE(got);
            EXPECT_EQ(*got, n);
            EXPECT_EQ(text(h.value()), expected) << "n " << n << " pieces of " << piece;
        }
        sgcl::tracked_ptr b = sgcl::make_tracked<io::buffer>(bytes(data));
        H h = fresh<H>();
        h.update("prefix");
        auto got = h.copy_from(*b);
        ASSERT_TRUE(got);
        EXPECT_EQ(*got, n);
        H expect = fresh<H>();
        expect.update("prefix");
        expect.update(bytes(data));
        EXPECT_EQ(text(h.value()), text(expect.value()));
    }
}

TYPED_TEST(Hash_Common, CopyFromHandsOnTheStreamsError) {
    using H = TypeParam;
    sgcl::tracked_ptr r = sgcl::make_tracked<failing>(50000);
    H h = fresh<H>();
    auto got = h.copy_from(*r);
    ASSERT_FALSE(got);
    EXPECT_EQ(got.error().code(), std::make_error_code(std::errc::io_error));
    // what came before the error is hashed
    std::vector<unsigned char> xs(50000, 'x');
    EXPECT_EQ(text(h.value()), of<H>(xs));
}

TYPED_TEST(Hash_Common, AsyncCopyFrom) {
    using H = TypeParam;
    auto data = pattern(0, 70000);
    auto expected = of<H>(data);
    auto t = sgcl::async::spawn([data]() -> sgcl::async::task<std::string> {
        sgcl::tracked_ptr r = sgcl::make_tracked<dribble>(data, 3000);
        H h = fresh<H>();
        auto got = co_await h.async_copy_from(*r);
        if (!got || *got != data.size()) {
            co_return "copy failed";
        }
        sgcl::tracked_ptr bad = sgcl::make_tracked<failing>(10);
        H other = fresh<H>();
        auto failed = co_await other.async_copy_from(*bad);
        if (failed) {
            co_return "the error was lost";
        }
        co_return text(h.value());
    });
    EXPECT_EQ(t.wait(), expected);
    sgcl::async::scheduler::stop();
}

TEST(Hash_CopyFrom, AFile) {
    namespace io = sgcl::io;
    auto dir = io::make_temp_dir({}, "hash-test-*");
    ASSERT_TRUE(dir);
    auto data = hash_test::pattern(0, 300001);
    sgcl::string path = io::path::join(*dir, "data.bin");
    ASSERT_TRUE(io::write_file(path, hash_test::bytes(data)));
    auto f = io::open(path);
    ASSERT_TRUE(f);
    hash::crc32 h;
    auto n = h.copy_from(**f);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, data.size());
    EXPECT_EQ(hash_test::text(h.value()), hash_test::of<hash::crc32>(data));
    (*f)->close();
    io::remove_all(*dir);
}
