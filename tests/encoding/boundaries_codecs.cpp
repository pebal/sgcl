//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the error, the byte orders, varint, the byte codecs,
// their streams and PEM (DESIGN 408): the default and the moved-from
// values, empty input and one byte, the limits of the sizes and the values,
// the padding at its edges, a stream that fails half-way. What the suites
// beside it cover already is not repeated: every length 0..300 and every
// byte at every position against Go (codecs.cpp), the pieces of 1, 2, 3 and
// 7 bytes (streams.cpp), the sizes at the edge of size_t of base64 and
// base32 (Codecs_Tests.SizesAtTheEdgeOfSizeT), the varint reads of Go
// (Binary_Tests.ReadsAgainstGo).
#include "common.h"

using namespace sgcl::encoding;

#include <climits>
#include <cstring>
#include <string>

using namespace enc_test;

namespace {
    std::string text_of(const sgcl::vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // A slice of n bytes that are never read: a size check refuses it first
    sgcl::slice<const byte> unread(size_t n) {
        return sgcl::slice<const byte>(reinterpret_cast<const byte*>(uintptr_t(0x1000)), n);
    }
}

// --- error ---

TEST(EncodingBoundaries_Tests, ErrorDefaultCopiedAndMoved) {
    encoding::error e;
    EXPECT_EQ(e.code(), encoding::errc::syntax);
    EXPECT_EQ(e.offset(), 0u);
    EXPECT_EQ(e.line(), 0u);
    EXPECT_EQ(e.column(), 0u);
    EXPECT_TRUE(e.path().empty());
    EXPECT_FALSE(e.io_error());
    EXPECT_EQ(e.message(), "offset 0: syntax error");
    // a copy is equal; assigned to itself it is unchanged
    encoding::error full(encoding::errc::type_mismatch, 7, "words");
    full.set_position(2, 3).set_path("/a");
    encoding::error copy = full;
    EXPECT_EQ(copy, full);
    auto& same = copy;
    copy = same;
    EXPECT_EQ(copy, full);
    // moved from: still an error whose members answer
    encoding::error moved = std::move(copy);
    EXPECT_EQ(moved, full);
    EXPECT_EQ(copy.code(), encoding::errc::type_mismatch);
    (void)copy.message();
    // a code outside the list has words of its own; 0 is no code of it
    EXPECT_EQ(encoding_category().message(0), "unknown encoding error");
    EXPECT_EQ(encoding_category().message(int(encoding::errc::limit_exceeded) + 1), "unknown encoding error");
    EXPECT_EQ(encoding_category().message(-1), "unknown encoding error");
}

TEST(EncodingBoundaries_Tests, ErrorPlaceAtItsEdges) {
    // the offset past the end of the text, and at the edge of uint64_t: the end
    encoding::error e(encoding::errc::syntax, UINT64_MAX);
    e.locate("ab\ncd");
    EXPECT_EQ(e.line(), 2u);
    EXPECT_EQ(e.column(), 3u);
    EXPECT_EQ(e.message(), "2:3: syntax error");
    encoding::error empty(encoding::errc::unexpected_end, 5);
    empty.locate("");
    EXPECT_EQ(empty.line(), 1u);
    EXPECT_EQ(empty.column(), 1u);
    // a line of 0 is no place: the offset is told
    encoding::error zero(encoding::errc::syntax, 9);
    zero.set_position(0, 4);
    EXPECT_EQ(zero.message(), "offset 9: syntax error");
    EXPECT_EQ(encoding::error(encoding::errc::syntax, UINT64_MAX).message(), "offset 18446744073709551615: syntax error");
    // an empty detail is no detail: the code's words
    EXPECT_EQ(encoding::error(encoding::errc::depth_limit, 1, "").message(), "offset 1: nesting too deep");
}

// --- big_endian, little_endian ---

TEST(EncodingBoundaries_Tests, ByteOrdersAtTheLimitsOfTheValues) {
    byte b[8];
    for (uint64_t v : {uint64_t(0), UINT64_MAX, uint64_t(1), uint64_t(1) << 63}) {
        big_endian::write_u64(slice<byte>(b, 8), v);
        EXPECT_EQ(big_endian::read_u64(slice<const byte>(b, 8)), v);
        little_endian::write_u64(slice<byte>(b, 8), v);
        EXPECT_EQ(little_endian::read_u64(slice<const byte>(b, 8)), v);
    }
    // a slice of exactly the number's size
    big_endian::write_u16(slice<byte>(b, 2), 0xFFFF);
    EXPECT_EQ(big_endian::read_u16(slice<const byte>(b, 2)), 0xFFFFu);
    little_endian::write_u32(slice<byte>(b, 4), UINT32_MAX);
    EXPECT_EQ(little_endian::read_u32(slice<const byte>(b, 4)), UINT32_MAX);
    little_endian::write_u32(slice<byte>(b, 4), 0);
    EXPECT_EQ(big_endian::read_u32(slice<const byte>(b, 4)), 0u);
    // appended after what the vector holds
    sgcl::vector<byte> out;
    out.push_back(byte(0xAA));
    big_endian::append_u16(out, 0);
    little_endian::append_u64(out, UINT64_MAX);
    EXPECT_EQ(hex::encode(out), "aa0000ffffffffffffffff");
}

// --- varint ---

TEST(EncodingBoundaries_Tests, VarintAtItsEdges) {
    // nothing to read
    auto none = varint::read(slice<const byte>());
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), encoding::errc::unexpected_end);
    EXPECT_EQ(none.error().offset(), 0u);
    EXPECT_FALSE(varint::read_signed(slice<const byte>()));
    // one byte, and a read that stops before the bytes do
    byte zero[3] = {byte(0), byte(0x7F), byte(0x80)};
    auto z = varint::read(slice<const byte>(zero, 3));
    ASSERT_TRUE(z);
    EXPECT_EQ(z->first, 0u);
    EXPECT_EQ(z->second, 1u);
    // the largest, in exactly max_size bytes, into a slice of exactly that
    byte room[varint::max_size];
    EXPECT_EQ(varint::write(slice<byte>(room, varint::max_size), UINT64_MAX), varint::max_size);
    auto max = varint::read(slice<const byte>(room, varint::max_size));
    ASSERT_TRUE(max);
    EXPECT_EQ(max->first, UINT64_MAX);
    // the tenth byte: 1 is the 64th bit, 2 past it, 0x80 past it whatever follows
    for (uint8_t tenth : {uint8_t(2), uint8_t(0x80), uint8_t(0xFF)}) {
        byte b[11];
        std::memset(b, 0xFF, 9);
        b[9] = byte(tenth);
        b[10] = byte(0);
        auto r = varint::read(slice<const byte>(b, 11));
        ASSERT_FALSE(r) << int(tenth);
        EXPECT_EQ(r.error().code(), encoding::errc::out_of_range);
        EXPECT_EQ(r.error().offset(), 9u);
    }
    // the signed limits both ways
    for (int64_t v : {INT64_MIN, INT64_MAX, int64_t(-1), int64_t(0)}) {
        byte s[varint::max_size];
        size_t n = varint::write_signed(slice<byte>(s, varint::max_size), v);
        auto r = varint::read_signed(slice<const byte>(s, n));
        ASSERT_TRUE(r) << v;
        EXPECT_EQ(r->first, v);
        EXPECT_EQ(r->second, n);
    }
    // a stream: empty is the end of the numbers, a failure before the
    // first byte and inside a number is the stream's
    io::buffered_reader empty(make_tracked<dribble>(std::string(), 1));
    auto e = varint::read(empty);
    ASSERT_TRUE(e);
    EXPECT_FALSE(e->has_value());
    io::buffered_reader failed_first(make_tracked<failing>(std::string()));
    auto f = varint::read_signed(failed_first);
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().path(), "failing");
    io::buffered_reader failed_inside(make_tracked<failing>("\xFF\xFF"));
    auto g = varint::read(failed_inside);
    ASSERT_FALSE(g);
    EXPECT_EQ(g.error().path(), "failing");
    // after the end of the stream, the end again
    io::buffered_reader one(make_tracked<dribble>(std::string("\x05", 1), 1));
    EXPECT_EQ(**varint::read(one), 5u);
    auto end = varint::read(one);
    ASSERT_TRUE(end);
    EXPECT_FALSE(*end);
    auto again = varint::read(one);
    ASSERT_TRUE(again);
    EXPECT_FALSE(*again);
}

// --- base64, base32, hex, ascii85 ---

// Empty input and one character: nothing at all, and a group cut at its start
TEST(EncodingBoundaries_Tests, CodecsOfNothingAndOneCharacter) {
    EXPECT_EQ(base64::standard.encode(slice<const byte>()), "");
    EXPECT_EQ(base32::standard.encode(slice<const byte>()), "");
    EXPECT_EQ(hex::encode(slice<const byte>()), "");
    EXPECT_EQ(hex::encode_upper(slice<const byte>()), "");
    EXPECT_EQ(ascii85::encode(slice<const byte>()), "");
    EXPECT_EQ(hex::dump(slice<const byte>()), "");
    EXPECT_TRUE(value_of(base64::standard.decode("")).empty());
    EXPECT_TRUE(value_of(base64::raw_url.lenient().decode("")).empty());
    EXPECT_TRUE(value_of(base32::hex.decode("")).empty());
    EXPECT_TRUE(value_of(hex::decode("")).empty());
    EXPECT_TRUE(value_of(ascii85::decode("")).empty());
    // into an empty buffer, for an empty input: nothing written, nothing refused
    EXPECT_EQ(base64::standard.encode_to(slice<char>(), slice<const byte>()), 0u);
    EXPECT_EQ(hex::encode_to(slice<char>(), slice<const byte>()), 0u);
    EXPECT_EQ(ascii85::encode_to(slice<char>(), slice<const byte>()), 0u);
    EXPECT_EQ(value_of(base32::standard.decode_to(slice<byte>(), "")), 0u);
    EXPECT_EQ(value_of(ascii85::decode_to(slice<byte>(), "")), 0u);
    // a lenient text of line endings alone is nothing
    EXPECT_TRUE(value_of(base64::standard.lenient().decode("\r\n\n")).empty());
    // one character: a byte cut at its start, at the end of the input
    struct Cut {
        const char* what;
        expected<sgcl::vector<byte>, encoding::error> r;
        encoding::errc code;
        uint64_t offset;
    };
    const Cut cuts[] = {
        {"base64", base64::standard.decode("Q"), encoding::errc::unexpected_end, 1},
        {"raw base64", base64::raw_standard.decode("Q"), encoding::errc::unexpected_end, 1},
        {"base32", base32::standard.decode("M"), encoding::errc::unexpected_end, 1},
        {"raw base32", base32::standard.without_padding().decode("M"), encoding::errc::unexpected_end, 1},
        {"hex", hex::decode("a"), encoding::errc::unexpected_end, 1},
        {"ascii85", ascii85::decode("!"), encoding::errc::unexpected_end, 1},
        {"base64 *", base64::standard.decode("*"), encoding::errc::invalid_character, 0},
        {"base64 =", base64::standard.decode("="), encoding::errc::syntax, 0},
        {"hex g", hex::decode("g"), encoding::errc::invalid_character, 0},
        {"ascii85 v", ascii85::decode("v"), encoding::errc::invalid_character, 0},
        {"ascii85 ~", ascii85::decode("~"), encoding::errc::invalid_character, 0},
    };
    for (auto& c : cuts) {
        ASSERT_FALSE(c.r) << c.what;
        EXPECT_EQ(c.r.error().code(), c.code) << c.what;
        EXPECT_EQ(c.r.error().offset(), c.offset) << c.what;
    }
    // one character that is a whole value: ascii85's 'z', and its white space
    EXPECT_EQ(text_of(value_of(ascii85::decode("z"))), std::string(4, '\0'));
    EXPECT_TRUE(value_of(ascii85::decode(" \t\n")).empty());
    // one byte encoded
    byte zero[1] = {byte(0)};
    auto one = slice<const byte>(zero, 1);
    EXPECT_EQ(base64::standard.encode(one), "AA==");
    EXPECT_EQ(base64::raw_url.encode(one), "AA");
    EXPECT_EQ(base32::standard.encode(one), "AA======");
    EXPECT_EQ(base32::hex.without_padding().encode(one), "00");
    EXPECT_EQ(hex::encode(one), "00");
    EXPECT_EQ(ascii85::encode(one), "!!");
    EXPECT_EQ(hex::dump(one), "00000000  00                                                |.|\n");
}

// The padding at its edges: every count of '=' a base32 group may end with,
// one too few and one too many, and padding where no group can end
TEST(EncodingBoundaries_Tests, PaddingAtItsEdges) {
    const std::pair<const char*, const char*> good[] = {
        {"MY======", "f"}, {"MZXQ====", "fo"}, {"MZXW6===", "foo"}, {"MZXW6YQ=", "foob"}, {"MZXW6YTB", "fooba"},
    };
    for (auto& [text, bytes] : good) {
        EXPECT_EQ(text_of(value_of(base32::standard.decode(text))), bytes) << text;
    }
    struct Case {
        const char* text;
        encoding::errc code;
        uint64_t offset;
    };
    const Case bad32[] = {
        {"MY=====", encoding::errc::unexpected_end, 7},    // one '=' short
        {"MY=======", encoding::errc::syntax, 8},          // one too many
        {"M=======", encoding::errc::syntax, 1},           // a group cannot end after one character
        {"MZX=====", encoding::errc::syntax, 3},           // nor after three
        {"MZXW6Y==", encoding::errc::syntax, 6},           // nor after six
        {"========", encoding::errc::syntax, 0},
        {"MZ======", encoding::errc::syntax, 1},           // bits past the data in the last character
    };
    for (auto& c : bad32) {
        auto r = base32::standard.decode(c.text);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text;
        EXPECT_EQ(r.error().offset(), c.offset) << c.text;
    }
    EXPECT_EQ(text_of(value_of(base32::standard.lenient().decode("MZ======"))), "f");
    // without padding: the short group as it is, an '=' is outside the alphabet
    auto raw = base32::standard.without_padding();
    EXPECT_EQ(text_of(value_of(raw.decode("MY"))), "f");
    EXPECT_EQ(error_of(raw.decode("MY=")).code(), encoding::errc::invalid_character);
    EXPECT_EQ(error_of(raw.decode("MY=")).offset(), 2u);
    EXPECT_EQ(error_of(raw.decode("MZX")).code(), encoding::errc::unexpected_end);
    EXPECT_EQ(error_of(raw.decode("MZX")).offset(), 3u);
    // base64: the padding alone, a group whose padding is missing, a line
    // ending after the padding (strict: data after it; lenient: skipped)
    EXPECT_EQ(error_of(base64::standard.decode("====")).offset(), 0u);
    EXPECT_EQ(error_of(base64::standard.decode("QUI")).code(), encoding::errc::unexpected_end);
    EXPECT_EQ(text_of(value_of(base64::raw_standard.decode("QUI"))), "AB");
    EXPECT_EQ(error_of(base64::standard.decode("QQ==\r\n")).code(), encoding::errc::syntax);
    EXPECT_EQ(error_of(base64::standard.decode("QQ==\r\n")).offset(), 4u);
    EXPECT_EQ(text_of(value_of(base64::standard.lenient().decode("QQ==\r\n"))), "A");
    EXPECT_EQ(text_of(value_of(base64::standard.lenient().decode("Q\nQ\n=\n=\n"))), "A");
}

// The sizes of hex and ascii85 at the edge of size_t (base64's and
// base32's: Codecs_Tests.SizesAtTheEdgeOfSizeT), and a text past what a
// string holds refused before a byte is read
TEST(EncodingBoundaries_Tests, SizesAndTheStringLimit) {
    EXPECT_EQ(hex::encoded_size(0), 0u);
    EXPECT_EQ(hex::encoded_size(SIZE_MAX / 2), SIZE_MAX - 1);
    EXPECT_EQ(hex::encoded_size(SIZE_MAX / 2 + 1), SIZE_MAX);
    EXPECT_EQ(hex::encoded_size(SIZE_MAX), SIZE_MAX);
    EXPECT_EQ(hex::max_decoded_size(SIZE_MAX), SIZE_MAX / 2);
    EXPECT_EQ(hex::max_decoded_size(1), 0u);
    EXPECT_EQ(ascii85::max_encoded_size(0), 0u);
    EXPECT_EQ(ascii85::max_encoded_size(1), 2u);
    EXPECT_EQ(ascii85::max_encoded_size(4), 5u);
    EXPECT_EQ(ascii85::max_encoded_size(5), 7u);
    EXPECT_EQ(ascii85::max_encoded_size(SIZE_MAX), SIZE_MAX);
    EXPECT_EQ(ascii85::max_encoded_size(SIZE_MAX / 5 * 4), SIZE_MAX / 5 * 5);
    EXPECT_EQ(ascii85::max_decoded_size(SIZE_MAX / 4), SIZE_MAX / 4 * 4);
    EXPECT_EQ(ascii85::max_decoded_size(SIZE_MAX / 4 + 1), SIZE_MAX);
    EXPECT_EQ(base32::hex.max_decoded_size(SIZE_MAX), SIZE_MAX / 8 * 5 + 5);
    // a text past UINT32_MAX characters: length_error, nothing read
    EXPECT_THROW(hex::encode(unread(size_t(UINT32_MAX) / 2 + 1)), std::length_error);
    EXPECT_THROW(hex::encode_upper(unread(size_t(UINT32_MAX) / 2 + 1)), std::length_error);
    EXPECT_THROW(base32::standard.encode(unread(size_t(UINT32_MAX) / 8 * 5 + 5)), std::length_error);
    EXPECT_THROW(base32::standard.without_padding().encode(unread(size_t(UINT32_MAX) / 8 * 5 + 5)), std::length_error);
    EXPECT_THROW(ascii85::encode(unread(size_t(UINT32_MAX) / 5 * 4 + 4)), std::length_error);
    EXPECT_THROW(hex::dump(unread(size_t(UINT32_MAX) / 95 * 16)), std::length_error);
}

// The caller's buffer one short of the bound, refused before the text is
// read (an invalid text too); exactly the bound taken
TEST(EncodingBoundaries_Tests, CallersBuffersAtTheBound) {
    byte in[5] = {byte(1), byte(2), byte(3), byte(4), byte(5)};
    char out[16];
    EXPECT_THROW(hex::encode_to(slice<char>(out, 9), slice<const byte>(in, 5)), std::length_error);
    EXPECT_EQ(hex::encode_to(slice<char>(out, 10), slice<const byte>(in, 5)), 10u);
    EXPECT_THROW(base32::standard.encode_to(slice<char>(out, 7), slice<const byte>(in, 5)), std::length_error);
    EXPECT_EQ(base32::standard.encode_to(slice<char>(out, 8), slice<const byte>(in, 5)), 8u);
    // ascii85's bound counts no 'z', which would make the text shorter
    byte zeros[4] = {};
    EXPECT_THROW(ascii85::encode_to(slice<char>(out, 4), slice<const byte>(zeros, 4)), std::length_error);
    EXPECT_EQ(ascii85::encode_to(slice<char>(out, 5), slice<const byte>(zeros, 4)), 1u);
    byte back[8];
    EXPECT_THROW(ascii85::decode_to(slice<byte>(back, 7), "zz"), std::length_error);
    EXPECT_EQ(value_of(ascii85::decode_to(slice<byte>(back, 8), "zz")), 8u);
    EXPECT_THROW(hex::decode_to(slice<byte>(back, 1), "abcd"), std::length_error);
    EXPECT_THROW(hex::decode_to(slice<byte>(), "zz"), std::length_error);   // invalid, but the buffer first
    EXPECT_EQ(value_of(hex::decode_to(slice<byte>(back, 2), "abcd")), 2u);
}

// --- the streams ---

TEST(EncodingBoundaries_Tests, StreamsOfNothing) {
    // a decoder over nothing: the end, and the end again
    auto dec = base64::standard.decoder_from(make_tracked<dribble>(std::string(), 1));
    byte buf[8];
    EXPECT_EQ(value_of(dec.read(slice<byte>(buf, 8))), 0u);
    EXPECT_EQ(value_of(dec.read(slice<byte>(buf, 8))), 0u);
    EXPECT_FALSE(dec.last_error());
    // a read into no room takes nothing; the bytes come to the next read
    auto some = hex::decoder_from(make_tracked<dribble>(std::string("4142"), 1));
    EXPECT_EQ(value_of(some.read(slice<byte>())), 0u);
    EXPECT_EQ(value_of(read_all(some, 3)), "AB");
    // an encoder given nothing writes nothing, and closes on nothing
    sgcl::tracked_ptr out = make_tracked<sink>();
    auto enc = base32::standard.encoder_to(out);
    EXPECT_EQ(value_of(enc.write(slice<const byte>())), 0u);
    EXPECT_TRUE(enc.close());
    EXPECT_EQ(out->text, "");
    EXPECT_EQ(out->writes, 0u);
    auto after = enc.write(slice<const byte>());
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
    auto dump = hex::dumper_to(out);
    EXPECT_EQ(value_of(dump.write(slice<const byte>())), 0u);
    EXPECT_TRUE(dump.close());
    EXPECT_EQ(out->text, "");
}

// The source failing inside a group: the bytes of the whole groups before
// it came out, the group cut is gone with the failure, which last_error
// holds at the offset reached
TEST(EncodingBoundaries_Tests, ADecoderWhoseSourceFailsInsideAGroup) {
    for (auto [text, bytes, offset] : {std::tuple{"QUJ", "", 3}, std::tuple{"QUJDRA", "ABC", 6}}) {
        auto dec = base64::standard.decoder_from(make_tracked<failing>(text));
        std::string got;
        byte buf[16];
        expected<size_t, io::error> r;
        while ((r = dec.read(slice<byte>(buf, 16))) && *r) {
            got.append(reinterpret_cast<const char*>(buf), *r);
        }
        ASSERT_FALSE(r) << text;
        EXPECT_EQ(r.error().path(), "failing");
        EXPECT_EQ(got, bytes) << text;
        ASSERT_TRUE(dec.last_error());
        EXPECT_EQ(dec.last_error()->code(), encoding::errc::io);
        EXPECT_EQ(dec.last_error()->offset(), uint64_t(offset)) << text;
        EXPECT_FALSE(dec.read(slice<byte>(buf, 16)));   // kept for good
    }
}

// A handle moved from keeps its stream (a tracked word's move is a copy:
// core's tracked_ptr); a default one holds none
TEST(EncodingBoundaries_Tests, StreamHandlesMovedAndEmpty) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    auto enc = ascii85::encoder_to(out);
    auto moved = std::move(enc);
    EXPECT_TRUE(enc == moved);
    ASSERT_TRUE(enc.write("Hello"));
    ASSERT_TRUE(moved.close());
    EXPECT_EQ(out->text, "87cURDZ");
    EXPECT_TRUE(enc.is_closed());
    EXPECT_FALSE(base64::decoder());
    EXPECT_FALSE(base32::encoder());
    EXPECT_FALSE(hex::dumper());
    EXPECT_FALSE(hex::decoder());
    EXPECT_FALSE(ascii85::decoder());
    EXPECT_TRUE(base64::decoder() == base64::decoder());
}

// --- pem ---

TEST(EncodingBoundaries_Tests, PemAtItsEdges) {
    // nothing at all
    auto none = pem::parse("");
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), encoding::errc::unexpected_end);
    EXPECT_EQ(none.error().offset(), 0u);
    EXPECT_EQ(none.error().line(), 1u);
    EXPECT_EQ(none.error().column(), 1u);
    EXPECT_TRUE(value_of(pem::parse_all("")).empty());
    EXPECT_THROW(pem(string("")), sgcl::bad_expected_access<encoding::error>);
    // a block of no bytes and of no type, written and read back
    pem empty("", {});
    EXPECT_EQ(empty.to_string(), "-----BEGIN -----\n-----END -----\n");
    auto back = value_of(pem::parse(empty.to_string()));
    EXPECT_EQ(back.type(), "");
    EXPECT_TRUE(back.bytes().empty());
    // the lines of 64 characters: 48 bytes are one line, 49 two
    for (size_t n : {size_t(47), size_t(48), size_t(49), size_t(96)}) {
        auto in = input(n);
        pem block("X", sgcl::vector<byte>(in.begin(), in.end()));
        auto t = std::string(block.to_string().view());
        size_t lines = size_t(std::count(t.begin(), t.end(), '\n'));
        EXPECT_EQ(lines, 2 + (n + 47) / 48) << n;
        EXPECT_EQ(bytes_of(value_of(pem::parse(string(t))).bytes()), in) << n;
    }
    // a BEGIN line that ends the text; an END line with no line ending
    EXPECT_EQ(error_of(pem::parse("-----BEGIN X-----")).code(), encoding::errc::unexpected_end);
    EXPECT_EQ(text_of(value_of(pem::parse("-----BEGIN X-----\nQUJD\n-----END X-----")).bytes()), "ABC");
    // copied, assigned to itself, moved from: the members answer
    pem block("CERTIFICATE", sgcl::vector<byte>{byte(1)});
    pem copy = block;
    auto& same = copy;
    copy = same;
    EXPECT_EQ(copy.to_string(), block.to_string());
    pem moved = std::move(copy);
    EXPECT_EQ(moved.to_string(), block.to_string());
    (void)copy.type();
    (void)copy.bytes().size();
    (void)copy.headers().size();
    (void)copy.to_string();
}
