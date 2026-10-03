//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// base64, base32, hex and ascii85 as whole texts: the vectors of RFC 4648
// section 10, every length from 0 to 300 against Go, every byte at every
// position of a valid text against Go, the offsets of the errors, the
// sizes at the edge of size_t, the caller's buffers. The streams are in
// streams.cpp.
#include "common.h"
#include "encoding_tests.h"

using namespace sgcl::encoding;

#include <climits>
#include <cstring>
#include <memory>
#include <string>

using namespace enc_test;

namespace {
    std::string text_of(const sgcl::vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // The alphabet of a codec by the oracle's name, for the sanity of an
    // error: the byte an invalid_character names must be outside it
    std::string_view alphabet_of(std::string_view name) {
        if (name.starts_with("base64::url") || name.starts_with("base64::raw_url")) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        }
        if (name.starts_with("base64")) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        }
        if (name.starts_with("base32::hex")) {
            return "0123456789ABCDEFGHIJKLMNOPQRSTUV";
        }
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    }

    std::string replaced(std::string_view text, int position, int b) {
        std::string s(text);
        if (size_t(position) == s.size()) {
            s.push_back(char(b));
        } else {
            s[position] = char(b);
        }
        return s;
    }

    int go_offset(char c) {
        return c <= '9' ? c - '0' : c - 'a' + 10;
    }
}

// RFC 4648 section 10, all four encodings
TEST(Codecs_Tests, Rfc4648Vectors) {
    const char* inputs[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
    const char* b64[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
    const char* b32[] = {"", "MY======", "MZXQ====", "MZXW6===", "MZXW6YQ=", "MZXW6YTB", "MZXW6YTBOI======"};
    const char* b32hex[] = {"", "CO======", "CPNG====", "CPNMU===", "CPNMUOG=", "CPNMUOJ1", "CPNMUOJ1E8======"};
    const char* b16[] = {"", "66", "666F", "666F6F", "666F6F62", "666F6F6261", "666F6F626172"};
    for (size_t i = 0; i < 7; ++i) {
        string in(inputs[i]);
        EXPECT_EQ(base64::standard.encode(in), b64[i]);
        EXPECT_EQ(base32::standard.encode(in), b32[i]);
        EXPECT_EQ(base32::hex.encode(in), b32hex[i]);
        EXPECT_EQ(hex::encode_upper(in), b16[i]);
        EXPECT_EQ(text_of(value_of(base64::standard.decode(b64[i]))), inputs[i]);
        EXPECT_EQ(text_of(value_of(base32::standard.decode(b32[i]))), inputs[i]);
        EXPECT_EQ(text_of(value_of(base32::hex.decode(b32hex[i]))), inputs[i]);
        EXPECT_EQ(text_of(value_of(hex::decode(b16[i]))), inputs[i]);
        EXPECT_EQ(text_of(value_of(hex::decode(string(b16[i]).to_lower()))), inputs[i]);
    }
}

// Every length from 0 to 300 through the ten codecs, the text compared
// with Go's by its hash, and every text decoded back to its bytes
TEST(Codecs_Tests, EveryLengthAgainstGo) {
    ASSERT_EQ(std::size(oracle::EncodeCodecs), 10u);
    for (size_t c = 0; c < std::size(oracle::EncodeCodecs); ++c) {
        auto codec = codec_named(oracle::EncodeCodecs[c], false);
        for (size_t n = 0; n <= 300; ++n) {
            auto in = input(n);
            auto text = codec.encode(as_slice(in));
            ASSERT_EQ(fnv(text.view()), oracle::EncodeHashes[n][c]) << oracle::EncodeCodecs[c] << " length " << n;
            auto back = codec.decode(text);
            ASSERT_TRUE(back.has_value()) << oracle::EncodeCodecs[c] << " length " << n << ": " << back.error().message();
            ASSERT_EQ(bytes_of(*back), in) << oracle::EncodeCodecs[c] << " length " << n;
        }
    }
    for (auto& t : oracle::EncodeTexts) {
        auto codec = codec_named(t.codec, false);
        EXPECT_EQ(codec.encode(as_slice(input(t.length))), t.text) << t.codec << " " << t.length;
    }
}

TEST(Codecs_Tests, Ascii85ZeroGroupsAgainstGo) {
    for (auto& c : oracle::Ascii85Zeros) {
        auto in = from_hex(c.hex);
        EXPECT_EQ(ascii85::encode(as_slice(in)), c.text) << c.hex;
        auto back = ascii85::decode(c.text);
        ASSERT_TRUE(back.has_value()) << c.hex;
        EXPECT_EQ(bytes_of(*back), in) << c.hex;
    }
}

// Every byte at every position of a valid text, and one byte more at its
// end, against Go: accepted or refused, and where. The differences the
// oracle's source names are allowed here by name and nowhere else:
//   - a line ending in a strict decoding is refused at its offset;
//   - bits past the data in the last character of base32, which Go takes
//     (it has no strict base32), are refused by the strict decoding;
//   - a short group of base32 without padding that cannot end there is
//     refused at the end, where Go returns nothing and no error;
//   - a character after the padding of base32 is refused, where Go drops
//     a group of fewer than eight characters after it, whatever they are;
//   - the byte 0xFF in base32 without padding is a byte outside the
//     alphabet, where Go takes it for its padding (NoPadding is -1, and
//     -1 as a byte is 0xFF);
//   - an ascii85 group past 32 bits is refused, where Go wraps it;
//   - a group cut short is refused at the end of the input, where Go
//     names the group's first character;
//   - a syntax error is refused where the input stops being the start of
//     a valid text, which is not always the character Go names.
// Every refusal of invalid_character names the same byte Go names; every
// acceptance of a strict decoding is canonical: the bytes encode back to
// the same text.
TEST(Codecs_Tests, EveryByteAtEveryPositionAgainstGo) {
    size_t cases = 0, same_offset = 0, other_offset = 0;
    for (auto& c : oracle::ByteCases) {
        std::string_view name = c.codec;
        bool is32 = name.starts_with("base32");
        bool is85 = name == "ascii85";
        // base32 has one row per text, Go's default: the lenient decoding
        // is held to it, and the strict one with the differences above
        for (bool lenient : is32 ? std::initializer_list<bool>{true, false} : std::initializer_list<bool>{c.lenient}) {
            auto codec = codec_named(name, lenient);
            for (int b = 0; b < 256; ++b) {
                ++cases;
                auto s = replaced(c.text, c.position, b);
                auto r = codec.decode(string(s));
                char go = c.outcomes[b];
                auto where = std::string(c.codec) + (lenient ? " lenient " : " strict ") + "\"" + s + "\" byte " + std::to_string(b) + " at " + std::to_string(c.position);
                if (go == '.') {
                    if (r) {
                        if (!lenient && !is85) {
                            EXPECT_EQ(codec.encode(r->as_slice()), string(s)) << where << ": a strict decoding took a text that is not canonical";
                        }
                        continue;
                    }
                    auto& e = r.error();
                    bool line_end = b == '\r' || b == '\n';
                    if (!lenient && line_end && (e.code() == encoding::errc::invalid_character || e.code() == encoding::errc::syntax) && e.offset() == uint64_t(c.position)) {
                        continue;   // after the padding it is data after the padding
                    }
                    if (is32 && codec.padded && size_t(c.position) == std::strlen(c.text) && e.code() == encoding::errc::syntax && e.offset() == uint64_t(c.position)) {
                        continue;   // Go drops a short group after the padding, whatever it holds
                    }
                    if (is85 && e.code() == encoding::errc::out_of_range) {
                        continue;
                    }
                    if (is32 && !codec.padded && b == 0xFF && e.code() == encoding::errc::invalid_character && e.offset() == uint64_t(c.position)) {
                        continue;   // Go's padding, which ends the text for it
                    }
                    if (is32 && !lenient && e.code() == encoding::errc::syntax) {
                        // bits past the data: the text is not what its bytes encode to
                        auto loose = codec_named(name, true).decode(string(s));
                        ASSERT_TRUE(loose.has_value()) << where;
                        EXPECT_NE(codec.encode(loose->as_slice()), string(s)) << where;
                        continue;
                    }
                    if (is32 && !codec.padded && e.code() == encoding::errc::unexpected_end && e.offset() == s.size()) {
                        size_t data = 0;
                        for (char ch : s) {
                            data += ch != '\r' && ch != '\n';
                        }
                        size_t tail = data % 8;
                        EXPECT_TRUE(tail == 1 || tail == 3 || tail == 6) << where;
                        continue;
                    }
                    ADD_FAILURE() << where << ": Go accepts, this refuses: " << e.message();
                    continue;
                }
                if (!r) {
                    auto& e = r.error();
                    int want = go_offset(go);
                    if (!lenient && (b == '\r' || b == '\n') && (e.code() == encoding::errc::invalid_character || e.code() == encoding::errc::syntax) && e.offset() == uint64_t(c.position)) {
                        continue;   // Go skips it and fails later, or elsewhere
                    }
                    if (e.offset() == uint64_t(want)) {
                        ++same_offset;
                    } else {
                        ++other_offset;
                    }
                    if (is32 && !codec.padded && b == 0xFF && e.code() == encoding::errc::invalid_character && e.offset() == uint64_t(c.position)) {
                        continue;   // Go's NoPadding is -1, which as a byte is 0xFF: its padding
                    }
                    switch (e.code()) {
                        case encoding::errc::invalid_character:
                            EXPECT_EQ(e.offset(), uint64_t(want)) << where << ": " << e.message();
                            if (!is85) {
                                EXPECT_EQ(alphabet_of(name).find(s[e.offset()]), std::string_view::npos) << where;
                            }
                            break;
                        case encoding::errc::unexpected_end:
                            EXPECT_EQ(e.offset(), s.size()) << where;
                            break;
                        case encoding::errc::syntax:
                        case encoding::errc::out_of_range:
                            EXPECT_LT(e.offset(), s.size()) << where;
                            break;
                        default:
                            ADD_FAILURE() << where << ": " << e.message();
                    }
                    continue;
                }
                ADD_FAILURE() << where << ": Go refuses at " << go_offset(go) << ", this accepts";
            }
        }
    }
    EXPECT_GT(cases, 50000u);
    // Most refusals are the same byte on both sides; the rest are the
    // named differences in where a syntax error or a cut is placed
    EXPECT_GT(same_offset, other_offset * 4);
}

TEST(Codecs_Tests, HexEveryByteAtEveryPositionAgainstGo) {
    std::string_view digits = "0123456789abcdefABCDEF";
    for (auto& c : oracle::HexCases) {
        for (int b = 0; b < 256; ++b) {
            auto s = replaced(c.text, c.position, b);
            auto r = hex::decode(string(s));
            auto where = "\"" + s + "\" byte " + std::to_string(b);
            switch (c.outcomes[b]) {
                case '.':
                    EXPECT_TRUE(r.has_value()) << where;
                    if (r) {
                        EXPECT_EQ(hex::encode(r->as_slice()), string(s).to_lower()) << where;
                    }
                    break;
                case 'I':
                    ASSERT_FALSE(r.has_value()) << where;
                    EXPECT_EQ(r.error().code(), encoding::errc::invalid_character) << where;
                    EXPECT_EQ(digits.find(s[r.error().offset()]), std::string_view::npos) << where;
                    for (size_t i = 0; i < r.error().offset(); ++i) {
                        EXPECT_NE(digits.find(s[i]), std::string_view::npos) << where;
                    }
                    break;
                case 'L':
                    ASSERT_FALSE(r.has_value()) << where;
                    EXPECT_EQ(r.error().code(), encoding::errc::unexpected_end) << where;
                    EXPECT_EQ(r.error().offset(), s.size()) << where;
                    break;
            }
        }
    }
}

// Hand-picked texts: what Go accepts, the same bytes; what it refuses,
// refused, with the offset where both sides agree on it
TEST(Codecs_Tests, NamedTextsAgainstGo) {
    for (auto& c : oracle::DecodeCases) {
        auto codec = codec_named(c.codec, c.lenient);
        std::string s(c.text);
        auto r = codec.decode(string(s));
        auto where = std::string(c.codec) + (c.lenient ? " lenient \"" : " strict \"") + s + "\"";
        bool line_end = s.find_first_of("\r\n") != std::string::npos;
        std::string_view name = c.codec;
        if (c.offset < 0) {
            if (!c.lenient && line_end && !name.starts_with("base32") && name != "ascii85") {
                ASSERT_FALSE(r.has_value()) << where;
                auto code = r.error().code();
                EXPECT_TRUE(code == encoding::errc::invalid_character || code == encoding::errc::syntax) << where;
                continue;
            }
            if (name.starts_with("base32") && name.ends_with("without_padding()") && !r) {
                EXPECT_EQ(r.error().code(), encoding::errc::unexpected_end) << where;   // Go: nothing and no error
                continue;
            }
            if (name.starts_with("base32") && !name.ends_with("without_padding()") && !r && r.error().code() == encoding::errc::syntax) {
                EXPECT_EQ(s[r.error().offset() - 1], '=') << where;   // Go: what follows the padding dropped
                continue;
            }
            if (name == "ascii85" && !r) {
                EXPECT_EQ(r.error().code(), encoding::errc::out_of_range) << where;     // Go: modulo 2^32
                continue;
            }
            ASSERT_TRUE(r.has_value()) << where << ": " << r.error().message();
            EXPECT_EQ(bytes_of(*r), from_hex(c.hex)) << where;
        } else {
            ASSERT_FALSE(r.has_value()) << where;
            bool strict_line_end = !c.lenient && (s[r.error().offset()] == '\r' || s[r.error().offset()] == '\n');
            if (r.error().code() == encoding::errc::invalid_character && !strict_line_end) {
                EXPECT_EQ(r.error().offset(), uint64_t(c.offset)) << where;
            }
        }
    }
}

// Where the decoding says the text went wrong: where it stops being the
// start of a valid text, the end when it is cut
TEST(Codecs_Tests, ErrorOffsets) {
    struct Case {
        const base64& codec;
        const char* text;
        encoding::errc code;
        uint64_t offset;
    };
    const Case cases[] = {
        {base64::standard, "Q", encoding::errc::unexpected_end, 1},
        {base64::standard, "QQ", encoding::errc::unexpected_end, 2},         // Go: 0, the group's start
        {base64::standard, "QQ=", encoding::errc::unexpected_end, 3},
        {base64::standard, "QQ=x", encoding::errc::syntax, 3},               // Go: 2, the '='
        {base64::standard, "QQ==x", encoding::errc::syntax, 4},
        {base64::standard, "QQ===", encoding::errc::syntax, 4},
        {base64::standard, "QR==", encoding::errc::syntax, 1},               // the character with the bits past the data
        {base64::standard, "QUJ=", encoding::errc::syntax, 2},
        {base64::standard, "Q===", encoding::errc::syntax, 1},
        {base64::standard, "=", encoding::errc::syntax, 0},
        {base64::standard, "QUJD=", encoding::errc::syntax, 4},
        {base64::standard, "QUJ*", encoding::errc::invalid_character, 3},
        {base64::standard, "QUJD\n", encoding::errc::invalid_character, 4},   // strict: a line ending is outside the alphabet
        {base64::standard, "QUJDRA==QUJD", encoding::errc::syntax, 8},
        {base64::standard, "-_-_", encoding::errc::invalid_character, 0},
        {base64::raw_standard, "QUJDR", encoding::errc::unexpected_end, 5},   // Go: 4
        {base64::raw_standard, "QR", encoding::errc::syntax, 1},
        {base64::raw_standard, "QQ==", encoding::errc::invalid_character, 2},
        {base64::url, "+/+/", encoding::errc::invalid_character, 0},
    };
    for (auto& c : cases) {
        auto r = c.codec.decode(c.text);
        ASSERT_FALSE(r.has_value()) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().offset(), c.offset) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), 0u) << c.text;
    }
    // lenient: line endings anywhere, and the bits past the data
    EXPECT_EQ(text_of(value_of(base64::standard.lenient().decode("QU\r\nJD\nRA=\n=\n"))), "ABCD");
    EXPECT_EQ(text_of(value_of(base64::standard.lenient().decode("QR=="))), "A");
    EXPECT_EQ(error_of(base64::standard.lenient().decode("QU JD")).offset(), 2u);   // a space is not a line ending
    // the message says where and what
    EXPECT_EQ(error_of(base64::standard.decode("QUJ*")).message(), "offset 3: invalid character '*'");
    EXPECT_EQ(error_of(base64::standard.decode(string("QUJ\xFF"))).message(), "offset 3: invalid character 0xFF");
    EXPECT_EQ(error_of(hex::decode("abc")).message(), "offset 3: the input ends inside a byte");
    EXPECT_EQ(error_of(hex::decode("0g")).offset(), 1u);
    EXPECT_EQ(error_of(ascii85::decode("87cUz")).code(), encoding::errc::syntax);
    EXPECT_EQ(error_of(ascii85::decode("87cUz")).offset(), 4u);
    EXPECT_EQ(error_of(ascii85::decode("s8W-\"")).code(), encoding::errc::out_of_range);
    EXPECT_EQ(error_of(ascii85::decode("s8W-\"")).offset(), 4u);
    EXPECT_EQ(error_of(ascii85::decode("s8W-")).code(), encoding::errc::out_of_range);   // padded with 'u', past 32 bits
    EXPECT_EQ(text_of(value_of(ascii85::decode("s8W-!"))), std::string(4, '\xFF'));
    EXPECT_EQ(error_of(ascii85::decode("zz!")).code(), encoding::errc::unexpected_end);
    EXPECT_EQ(error_of(ascii85::decode("zz!")).offset(), 3u);
}

// The alphabets of one's own, and the ones that cannot be
TEST(Codecs_Tests, CustomAlphabets) {
    // the alphabet of bcrypt and crypt(3): ./ first, no padding
    base64 crypt("./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", nullopt);
    EXPECT_FALSE(crypt.padded());
    auto in = input(40);
    auto text = crypt.encode(as_slice(in));
    EXPECT_EQ(text.size(), 54u);
    EXPECT_EQ(bytes_of(value_of(crypt.decode(text))), in);
    // a padding of one's own
    base64 star("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", '*');
    EXPECT_EQ(star.encode("f"), "Zg**");
    EXPECT_EQ(text_of(value_of(star.decode("Zg**"))), "f");
    EXPECT_FALSE(star.decode("Zg==").has_value());
    base32 lower("abcdefghijklmnopqrstuvwxyz234567");
    EXPECT_EQ(lower.encode("foobar"), "mzxw6ytboi======");
    // a repeated character, a line ending, the padding in the alphabet
    EXPECT_THROW(base64("AACDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"), std::invalid_argument);
    EXPECT_THROW(base64("\nBCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"), std::invalid_argument);
    EXPECT_THROW(base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", 'A'), std::invalid_argument);
    EXPECT_THROW(base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", '\n'), std::invalid_argument);
    // the length is the array's: 65 characters with no terminator, or a
    // terminator before the 64th, is an alphabet of the wrong length and
    // nothing past the array is read
    char unterminated[65];
    std::memcpy(unterminated, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", 64);
    unterminated[64] = '-';
    EXPECT_THROW(base64{unterminated}, std::invalid_argument);
    auto heap = std::make_unique<char[]>(65);
    std::memcpy(heap.get(), unterminated, 65);
    EXPECT_THROW(base64{*reinterpret_cast<char(*)[65]>(heap.get())}, std::invalid_argument);
    char early[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+";
    EXPECT_THROW(base64{early}, std::invalid_argument);
    char early32[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ23456";
    EXPECT_THROW(base32{early32}, std::invalid_argument);
    // in a constant it is an error at compile time; a good one is a constant
    static constexpr base64 dots("./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", nullopt);
    static_assert(!dots.padded());
    static_assert(base64::raw_url.encoded_size(4) == 6);
    static_assert(base64::standard.encoded_size(4) == 8);
}

// The sizes: exact, and never wrapped to a small number at the edge of
// size_t, which is how an encoder writes past a buffer it sized itself
TEST(Codecs_Tests, SizesAtTheEdgeOfSizeT) {
    using u128 = unsigned __int128;
    auto check = [](auto codec, size_t group_bytes, size_t group_chars, size_t bits) {
        for (size_t n : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4), size_t(5), size_t(1000)}) {
            EXPECT_EQ(codec.encoded_size(n), codec.encode(as_slice(input(n))).size());
            EXPECT_EQ(codec.without_padding().encoded_size(n), codec.without_padding().encode(as_slice(input(n))).size());
        }
        for (size_t d = 0; d < 200; ++d) {
            for (size_t n : {SIZE_MAX - d, SIZE_MAX / 2 - d, SIZE_MAX / group_chars * group_bytes - d, SIZE_MAX / group_chars * group_bytes + d}) {
                u128 groups = n / group_bytes;
                u128 rest = n % group_bytes;
                u128 padded = groups * group_chars + (rest ? group_chars : 0);
                u128 raw = groups * group_chars + (rest * 8 + bits - 1) / bits;
                u128 max = SIZE_MAX;
                EXPECT_EQ(codec.encoded_size(n), size_t(padded > max ? max : padded)) << n;
                EXPECT_EQ(codec.without_padding().encoded_size(n), size_t(raw > max ? max : raw)) << n;
                // the most a text of n characters decodes to: no wrap either,
                // and never less than what n characters can carry
                u128 bits_in = u128(n) * bits;
                EXPECT_GE(u128(codec.max_decoded_size(n)), bits_in / 8 / group_bytes * group_bytes) << n;
                EXPECT_LE(u128(codec.max_decoded_size(n)), u128(n)) << n;
                EXPECT_EQ(codec.without_padding().max_decoded_size(n), size_t(bits_in / 8)) << n;
            }
        }
    };
    check(base64::standard, 3, 4, 6);
    check(base32::standard, 5, 8, 5);
    // a text longer than a string holds is refused before anything is written
    auto huge = slice<const byte>(reinterpret_cast<const byte*>(uintptr_t(0x1000)), size_t(UINT32_MAX));
    EXPECT_THROW(base64::standard.encode(huge), std::length_error);
}

// Into the caller's buffer, and the buffer that is too small
TEST(Codecs_Tests, CallersBuffers) {
    for (size_t n = 0; n <= 40; ++n) {
        auto in = input(n);
        for (auto& codec : {base64::standard, base64::raw_url}) {
            std::string out(codec.encoded_size(n), '\0');
            size_t k = codec.encode_to(slice<char>(out.data(), out.size()), as_slice(in));
            EXPECT_EQ(k, out.size());
            EXPECT_EQ(string(out), codec.encode(as_slice(in)));
            std::vector<byte> back(codec.max_decoded_size(out.size()));
            auto r = codec.decode_to(slice<byte>(back.data(), back.size()), string(out));
            ASSERT_TRUE(r.has_value());
            back.resize(*r);
            EXPECT_EQ(back, in);
        }
        std::string out32(base32::hex.encoded_size(n), '\0');
        base32::hex.encode_to(slice<char>(out32.data(), out32.size()), as_slice(in));
        EXPECT_EQ(string(out32), base32::hex.encode(as_slice(in)));
    }
    char small[3];
    auto in = input(3);
    EXPECT_THROW(base64::standard.encode_to(slice<char>(small, 3), as_slice(in)), std::length_error);
    byte two[2];
    EXPECT_THROW(base64::standard.decode_to(slice<byte>(two, 2), "QUJD"), std::length_error);
    // an error into a buffer is the error, the buffer's bytes unspecified
    byte three[3];
    EXPECT_EQ(error_of(base64::standard.decode_to(slice<byte>(three, 3), "QU*D")).offset(), 2u);
}

TEST(Codecs_Tests, HexDumpAgainstGo) {
    for (size_t n = 0; n <= 70; ++n) {
        auto d = hex::dump(as_slice(input(n)));
        EXPECT_EQ(fnv(d.view()), oracle::DumpHashes[n]) << n << "\n" << d.view();
    }
    EXPECT_EQ(hex::dump(as_slice(input(40))), oracle::Dump40);
    EXPECT_EQ(hex::dump(as_slice(bytes_of("Hello, World!\n"))),
              "00000000  48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a        |Hello, World!.|\n");
}

TEST(Codecs_Tests, EncodeTakesTextAndBytes) {
    // the bytes of a text and the same bytes as bytes encode the same
    auto bytes = bytes_of("ala:sekret");
    EXPECT_EQ(base64::standard.encode("ala:sekret"), base64::standard.encode(as_slice(bytes)));
    EXPECT_EQ(base64::standard.encode("ala:sekret"), "YWxhOnNla3JldA==");
    EXPECT_EQ(hex::encode("\x01\xAB"), "01ab");
    EXPECT_EQ(hex::encode_upper("\x01\xAB"), "01AB");
    EXPECT_EQ(ascii85::encode("Hello"), "87cURDZ");
    sgcl::vector<byte> v(bytes.begin(), bytes.end());
    EXPECT_EQ(base32::standard.encode(v), base32::standard.encode("ala:sekret"));
}

// decode_to from characters that lie elsewhere (a slice of a buffer, no
// string made), strict and lenient: the bytes of decode, the same errors
TEST(Codecs_Tests, Base64DecodeToFromASlice) {
    const std::string lines = "QUJD\r\nREVG\nR0g=";   // "ABCDEFGH" in lines
    std::byte out[12];   // the bound of 14 characters (the line endings count)
    auto strict = base64::standard.decode_to(slice<byte>(out, sizeof out), slice<const char>(lines.data(), lines.size()));
    EXPECT_FALSE(strict);   // a line ending is not base64 in the strict codec
    auto lenient = base64::standard.lenient().decode_to(slice<byte>(out, sizeof out), slice<const char>(lines.data(), lines.size()));
    ASSERT_TRUE(lenient);
    ASSERT_EQ(*lenient, 8u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(out), 8), "ABCDEFGH");
    // the same as the string's overload, byte for byte, on every length
    for (size_t n = 0; n < 64; ++n) {
        std::string data(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            data[i] = char(i * 37 + 11);
        }
        string text = base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(data.data()), n));
        std::byte a[64], b[64];
        auto x = base64::standard.decode_to(slice<byte>(a, 64), text);
        auto y = base64::standard.decode_to(slice<byte>(b, 64), slice<const char>(text.data(), text.size()));
        ASSERT_TRUE(x && y) << n;
        ASSERT_EQ(*x, n);
        ASSERT_EQ(*y, n);
        EXPECT_EQ(std::memcmp(a, b, n), 0) << n;
    }
    // a literal still goes to one overload
    std::byte three[3];
    auto r = base64::standard.decode_to(slice<byte>(three, 3), "QUJD");
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 3u);
    EXPECT_EQ(error_of(base64::standard.decode_to(slice<byte>(three, 3), slice<const char>("QU*D", 4))).offset(), 2u);
}

namespace {
    // decode_to of characters that lie where they are: an overload of its
    // own (not the string's, through a conversion)
    template<class C>
    constexpr bool decodes_in_place = requires { static_cast<expected<size_t, error> (C::*)(const slice<byte>&, const slice<const char>&) const>(&C::decode_to); };

    template<class C>
    constexpr bool decodes_in_place_static = requires { static_cast<expected<size_t, error> (*)(const slice<byte>&, const slice<const char>&)>(&C::decode_to); };

    // The slice's, the string_view's and the literal's overloads give the
    // bytes and the errors of the string's
    template<class Encode, class Decode>
    void same_as_the_string(Encode encode, Decode decode_to, const char* bad, uint64_t bad_offset) {
        for (size_t n = 0; n < 64; ++n) {
            std::string data(n, '\0');
            for (size_t i = 0; i < n; ++i) {
                data[i] = char(i * 37 + 11);
            }
            string text = encode(slice<const byte>(reinterpret_cast<const byte*>(data.data()), n));
            std::byte a[128], b[128], c[128];   // the bound of the text, past the bytes
            auto x = decode_to(slice<byte>(a, 128), text);
            auto y = decode_to(slice<byte>(b, 128), slice<const char>(text.data(), text.size()));
            auto z = decode_to(slice<byte>(c, 128), std::string_view(text.data(), text.size()));
            ASSERT_TRUE(x && y && z) << n;
            ASSERT_EQ(*x, n);
            ASSERT_EQ(*y, n);
            ASSERT_EQ(*z, n);
            EXPECT_EQ(std::memcmp(a, b, n), 0) << n;
            EXPECT_EQ(std::memcmp(a, c, n), 0) << n;
        }
        std::byte out[16];
        EXPECT_EQ(error_of(decode_to(slice<byte>(out, 16), slice<const char>(bad, std::strlen(bad)))).offset(), bad_offset);
        EXPECT_EQ(error_of(decode_to(slice<byte>(out, 16), bad)).offset(), bad_offset);
    }
}

// decode_to has the same three forms in every codec: a string, characters
// read where they lie (a slice), and a literal or a std::string_view
TEST(Codecs_Tests, DecodeToFromASliceInEveryCodec) {
    EXPECT_TRUE(decodes_in_place<base64>);
    EXPECT_TRUE(decodes_in_place<base32>);
    EXPECT_TRUE(decodes_in_place_static<hex>);
    EXPECT_TRUE(decodes_in_place_static<ascii85>);
    same_as_the_string([](const slice<const byte>& d) { return base32::standard.encode(d); },
                       [](const slice<byte>& out, const auto& t) { return base32::standard.decode_to(out, t); }, "MZ*G", 2);
    same_as_the_string([](const slice<const byte>& d) { return hex::encode(d); },
                       [](const slice<byte>& out, const auto& t) { return hex::decode_to(out, t); }, "0aZ1", 2);
    same_as_the_string([](const slice<const byte>& d) { return ascii85::encode(d); },
                       [](const slice<byte>& out, const auto& t) { return ascii85::decode_to(out, t); }, "87c~D", 3);
    std::byte two[2];
    auto r = hex::decode_to(slice<byte>(two, 2), "4142");
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 2u);
}
