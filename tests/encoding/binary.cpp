//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// big_endian, little_endian and varint. The varints against Go's
// encoding/binary: the bytes of every number named, signed and not, and
// what a read makes of bytes that are cut or too long, from a slice and
// from a stream.
#include "common.h"
#include "encoding_tests.h"

using namespace sgcl::encoding;

#include <string>

using namespace enc_test;

TEST(Binary_Tests, ByteOrders) {
    sgcl::vector<byte> b(8);
    big_endian::write_u32(b, 0xCAFEBABE);
    EXPECT_EQ(hex::encode(b), "cafebabe00000000");
    EXPECT_EQ(big_endian::read_u32(b), 0xCAFEBABEu);
    EXPECT_EQ(little_endian::read_u32(b), 0xBEBAFECAu);
    EXPECT_EQ(big_endian::read_u16(b), 0xCAFEu);
    little_endian::write_u64(b, 0x0102030405060708ull);
    EXPECT_EQ(hex::encode(b), "0807060504030201");
    EXPECT_EQ(big_endian::read_u64(b), 0x0807060504030201ull);
    EXPECT_EQ(little_endian::read_u64(b), 0x0102030405060708ull);
    little_endian::write_u16(b.as_slice(6), 0xABCD);
    EXPECT_EQ(hex::encode(b), "080706050403cdab");
    big_endian::write_u16(b, 0xFFFF);
    EXPECT_EQ(little_endian::read_u16(b), 0xFFFFu);

    sgcl::vector<byte> out;
    big_endian::append_u16(out, 0x0102);
    big_endian::append_u32(out, 0x03040506);
    big_endian::append_u64(out, 0x0708090A0B0C0D0Eull);
    little_endian::append_u16(out, 0x0102);
    little_endian::append_u32(out, 0x03040506);
    little_endian::append_u64(out, 0x0708090A0B0C0D0Eull);
    EXPECT_EQ(hex::encode(out), "0102030405060708090a0b0c0d0e" "0201060504030e0d0c0b0a090807");
    // a read at an offset: the slice from there
    EXPECT_EQ(big_endian::read_u32(out.as_slice(2)), 0x03040506u);
    EXPECT_EQ(little_endian::read_u32(out.as_slice(16)), 0x03040506u);
}

TEST(Binary_Tests, UvarintsAgainstGo) {
    for (auto& c : oracle::Uvarints) {
        sgcl::vector<byte> out;
        varint::append(out, c.value);
        EXPECT_EQ(hex::encode(out), c.hex) << c.value;
        byte room[varint::max_size];
        size_t n = varint::write(slice<byte>(room, varint::max_size), c.value);
        EXPECT_EQ(hex::encode(slice<const byte>(room, n)), c.hex) << c.value;
        auto r = varint::read(out);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->first, c.value);
        EXPECT_EQ(r->second, out.size());
    }
}

TEST(Binary_Tests, SignedVarintsAgainstGo) {
    for (auto& c : oracle::Varints) {
        sgcl::vector<byte> out;
        varint::append_signed(out, c.value);
        EXPECT_EQ(hex::encode(out), c.hex) << c.value;
        byte room[varint::max_size];
        size_t n = varint::write_signed(slice<byte>(room, varint::max_size), c.value);
        EXPECT_EQ(hex::encode(slice<const byte>(room, n)), c.hex) << c.value;
        auto r = varint::read_signed(out);
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->first, c.value);
        EXPECT_EQ(r->second, out.size());
    }
}

// Bytes Go reads as a uvarint: the same value and length, or the same
// failure — cut short (n 0) or past 64 bits (n -k, at byte k - 1); from a
// stream, nullopt where Go says io.EOF, unexpected_eof where it says
// io.ErrUnexpectedEOF. One difference, by name: a tenth byte with its high
// bit set is past 64 bits whatever follows, and is refused at that byte;
// Go's Uvarint calls ten such bytes cut short and names the eleventh when
// there is one (its ReadUvarint refuses at the tenth, as this does)
TEST(Binary_Tests, ReadsAgainstGo) {
    for (auto& c : oracle::VarintReads) {
        auto bytes = from_hex(c.hex);
        auto r = varint::read(as_slice(bytes));
        if (c.n > 0) {
            ASSERT_TRUE(r.has_value()) << c.hex;
            EXPECT_EQ(r->first, c.value) << c.hex;
            EXPECT_EQ(r->second, size_t(c.n)) << c.hex;
        } else if (bytes.size() >= varint::max_size && (c.n == 0 || c.n < -int(varint::max_size))) {
            ASSERT_FALSE(r.has_value()) << c.hex;
            EXPECT_EQ(r.error().code(), encoding::errc::out_of_range) << c.hex;
            EXPECT_EQ(r.error().offset(), varint::max_size - 1) << c.hex;
        } else if (c.n == 0) {
            ASSERT_FALSE(r.has_value()) << c.hex;
            EXPECT_EQ(r.error().code(), encoding::errc::unexpected_end) << c.hex;
            EXPECT_EQ(r.error().offset(), bytes.size()) << c.hex;
        } else {
            ASSERT_FALSE(r.has_value()) << c.hex;
            EXPECT_EQ(r.error().code(), encoding::errc::out_of_range) << c.hex;
            EXPECT_EQ(r.error().offset(), uint64_t(-c.n - 1)) << c.hex;
        }

        io::buffered_reader in(make_tracked<dribble>(std::string(view_of(bytes)), 1));
        auto s = varint::read(in);
        switch (c.stream) {
            case 'v':
                ASSERT_TRUE(s.has_value()) << c.hex;
                ASSERT_TRUE(s->has_value()) << c.hex;
                EXPECT_EQ(**s, c.value) << c.hex;
                break;
            case 'E':
                ASSERT_TRUE(s.has_value()) << c.hex;
                EXPECT_FALSE(s->has_value()) << c.hex;
                break;
            case 'U':
                ASSERT_FALSE(s.has_value()) << c.hex;
                EXPECT_TRUE(s.error().is_eof()) << c.hex;
                break;
            case 'O':
                ASSERT_FALSE(s.has_value()) << c.hex;
                EXPECT_EQ(s.error().code(), make_error_code(encoding::errc::out_of_range)) << c.hex;
                break;
        }
    }
}

// A stream of numbers read to its end, blocking and in a task
TEST(Binary_Tests, StreamOfVarints) {
    sgcl::vector<byte> out;
    std::vector<int64_t> values;
    for (int64_t v = -300; v <= 300; v += 7) {
        values.push_back(v * v * v);
        varint::append_signed(out, v * v * v);
    }
    auto text = std::string(reinterpret_cast<const char*>(out.data()), out.size());
    for (size_t piece : {1, 2, 3, 7, 4096}) {
        io::buffered_reader in(make_tracked<dribble>(text, piece));
        std::vector<int64_t> got;
        for (;;) {
            auto r = varint::read_signed(in);
            ASSERT_TRUE(r.has_value());
            if (!*r) {
                break;
            }
            got.push_back(**r);
        }
        EXPECT_EQ(got, values);
    }
    auto t = sgcl::async::spawn([](std::string text, std::vector<int64_t> values) -> async::task<int> {
        sgcl::tracked_ptr in = make_tracked<io::buffered_reader>(make_tracked<dribble>(text, 3));
        std::vector<int64_t> got;
        for (;;) {
            auto r = co_await varint::async_read_signed(*in);
            if (!r) {
                co_return -1;
            }
            if (!*r) {
                break;
            }
            got.push_back(**r);
        }
        auto u = co_await varint::async_read(*in);
        if (!u || u->has_value()) {
            co_return -2;
        }
        co_return got == values ? 1 : -3;
    }(text, values));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

namespace {
    // A reader living in a managed object of the caller's
    struct reader_box {
        io::buffered_reader in;
        explicit reader_box(const io::reader& r)
        : in(r) {
        }
    };
}

// async_read takes the reader by reference (its position is what the read
// moves on); a reader living in a managed object is kept by the task's
// frame, which the collector traces conservatively: made while the caller
// has the box, started after the caller has let it go and the collector has
// run, the task still reads
TEST(Binary_Tests, AsyncReadHoldsTheReadersObject) {
    sgcl::vector<byte> out;
    varint::append(out, 300);
    varint::append_signed(out, -12345);
    auto text = std::string(reinterpret_cast<const char*>(out.data()), out.size());
    sgcl::async::task<expected<optional<uint64_t>, io::error>> first;
    sgcl::async::task<expected<optional<int64_t>, io::error>> second;
    off_frame([&] {
        sgcl::tracked_ptr box = make_tracked<reader_box>(make_tracked<dribble>(text, 1));
        first = varint::async_read(box->in);
        second = varint::async_read_signed(box->in);
    });
    collector::clear_stack();
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    for (int i = 0; i < 1000; ++i) {
        (void)make_tracked<reader_box>(make_tracked<dribble>(std::string(64, 'x'), 1));   // what the dead box's memory would go to
    }
    auto a = sgcl::async::spawn(std::move(first)).wait();
    ASSERT_TRUE(a && *a);
    EXPECT_EQ(**a, 300u);
    auto b = sgcl::async::spawn(std::move(second)).wait();
    ASSERT_TRUE(b && *b);
    EXPECT_EQ(**b, -12345);
    sgcl::async::scheduler::stop();
}

TEST(Binary_Tests, BufferedStreamsAreNotCopied) {
    static_assert(!std::is_copy_constructible_v<io::buffered_reader> && !std::is_copy_assignable_v<io::buffered_reader>);
    static_assert(std::is_move_constructible_v<io::buffered_reader> && std::is_move_assignable_v<io::buffered_reader>);
    static_assert(!std::is_copy_constructible_v<io::buffered_writer> && !std::is_copy_assignable_v<io::buffered_writer>);
    static_assert(std::is_move_constructible_v<io::buffered_writer> && std::is_move_assignable_v<io::buffered_writer>);
}
