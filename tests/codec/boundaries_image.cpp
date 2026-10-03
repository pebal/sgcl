//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec at its boundaries (DESIGN 408): the image in memory, the pixel
// formats, sniff, the error and its category. What image.cpp holds already
// is not repeated: the zero sides and a format outside the list
// (TheContractThrows), the eight orientations (TheEightOrientations), a
// moved-from image (AMovedFromImageIsStillTheImage), an orientation outside
// 1..8 (AnOrientationOutsideOneToEightIsOne), the default error
// (DefaultAndCopies).
#include "common.h"

#include <cstring>
#include <limits>
#include <random>
#include <vector>

using namespace sgcl;
using codec::pixel_format;

namespace {
    constexpr pixel_format AllFormats[] = {
        pixel_format::gray8, pixel_format::gray_alpha8, pixel_format::rgb8, pixel_format::rgba8,
        pixel_format::gray16, pixel_format::gray_alpha16, pixel_format::rgb16, pixel_format::rgba16,
        pixel_format::cmyk8
    };

    void fill_random(codec::image& im, uint32_t seed) {
        std::mt19937 rng(seed);
        for (auto& b : im.pixels()) {
            b = static_cast<std::byte>(rng());
        }
    }

    bool same_pixels(const codec::image& a, const codec::image& b) {
        auto x = a.pixels();
        auto y = b.pixels();
        return a.width() == b.width() && a.height() == b.height() && a.format() == b.format() && x.size() == y.size() &&
               std::memcmp(x.data(), y.data(), x.size()) == 0;
    }

    std::vector<std::byte> block_of(const std::string& s) {
        std::vector<std::byte> out(s.size());
        std::memcpy(out.data(), s.data(), s.size());
        return out;
    }

    // EXIF, big-endian: the header, IFD0 at `ifd` with `count` entries, the
    // first of them the orientation (type, value count, value) unless `tag`
    // says another
    std::string exif_be(uint32_t ifd, uint16_t count, uint16_t tag = 0x0112, uint16_t type = 3, uint32_t values = 1, uint16_t value = 6) {
        std::string s = std::string("MM\0*", 4);
        s += std::string{char(ifd >> 24), char(ifd >> 16), char(ifd >> 8), char(ifd)};
        s.resize(ifd, '\0');
        s += std::string{char(count >> 8), char(count)};
        s += std::string{char(tag >> 8), char(tag), char(type >> 8), char(type)};
        s += std::string{char(values >> 24), char(values >> 16), char(values >> 8), char(values)};
        s += std::string{char(value >> 8), char(value), 0, 0};
        return s;
    }
}

TEST(CodecImageBounds_Tests, OnePixelOfEveryFormat) {
    // 1 × 1: each member, every conversion, every orientation, a copy
    for (pixel_format f : AllFormats) {
        codec::image dot(1, 1, f);
        fill_random(dot, unsigned(f) + 1);
        EXPECT_EQ(dot.stride(), codec::detail::bytes_per_pixel(f));
        EXPECT_EQ(dot.pixels().size(), dot.stride());
        EXPECT_EQ(dot.row(0).data(), dot.pixels().data());
        EXPECT_THROW((void)dot.row(1), std::out_of_range);
        EXPECT_THROW((void)std::as_const(dot).row(1), std::out_of_range);
        EXPECT_THROW((void)dot.row(std::numeric_limits<uint32_t>::max()), std::out_of_range);
        EXPECT_TRUE(same_pixels(dot.clone(), dot));
        for (pixel_format to : AllFormats) {
            codec::image c = dot.convert(to);
            EXPECT_EQ(c.width(), 1u);
            EXPECT_EQ(c.height(), 1u);
            EXPECT_EQ(c.format(), to);
            // through a format holding more and back: the same pixel
            if (to == f) {
                EXPECT_TRUE(same_pixels(c, dot));
            }
        }
        for (unsigned o = 1; o <= 8; ++o) {
            dot.set_orientation(o);
            codec::image shown = dot.oriented();
            EXPECT_EQ(shown.orientation(), 1u);
            EXPECT_TRUE(std::memcmp(shown.pixels().data(), dot.pixels().data(), dot.stride()) == 0) << int(f) << " " << o;
        }
    }
}

TEST(CodecImageBounds_Tests, ASingleRowAndASingleColumn) {
    // the sides of one pixel each way, every orientation against its
    // definition: pixel (x, y) shown is the source's at the place the
    // orientation names
    for (auto [w, h] : {std::pair{7u, 1u}, std::pair{1u, 7u}}) {
        codec::image line(w, h, pixel_format::rgb16);
        fill_random(line, w * 10 + h);
        for (unsigned o = 1; o <= 8; ++o) {
            line.set_orientation(o);
            codec::image shown = line.oriented();
            const bool swap = o >= 5;
            ASSERT_EQ(shown.width(), swap ? h : w);
            ASSERT_EQ(shown.height(), swap ? w : h);
            // turned back by the inverse orientation: the source
            const unsigned inverse[9] = {0, 1, 2, 3, 4, 5, 8, 7, 6};
            codec::image back = shown.clone();
            back.set_orientation(inverse[o]);
            EXPECT_TRUE(std::memcmp(back.oriented().pixels().data(), line.pixels().data(), line.pixels().size()) == 0) << o;
        }
    }
}

TEST(CodecImageBounds_Tests, ABufferPastAnAddressIsLengthError) {
    // the largest height whose bytes an address holds, and one past it:
    // past is length_error before anything is allocated (the size itself
    // would be refused by the collector, which ends the program)
    const uint32_t w = 0xFFFFFFFFu;
    const uint64_t most = uint64_t(std::numeric_limits<ptrdiff_t>::max()) / w;   // 2^31 rows of gray8
    ASSERT_EQ(most, uint64_t(1) << 31);
    EXPECT_THROW(codec::image(w, uint32_t(most + 1), pixel_format::gray8), std::length_error);
    EXPECT_THROW(codec::image(w, 0xFFFFFFFFu, pixel_format::gray8), std::length_error);
    // eight bytes a pixel: 2^28 rows of 2^32 - 1 pixels
    const uint64_t most16 = uint64_t(std::numeric_limits<ptrdiff_t>::max()) / (uint64_t(w) * 8);
    EXPECT_THROW(codec::image(w, uint32_t(most16 + 1), pixel_format::rgba16), std::length_error);
    // the contract is checked before the size: a zero side wins
    EXPECT_THROW(codec::image(0, 0xFFFFFFFFu, pixel_format::rgba16), std::invalid_argument);
    EXPECT_THROW(codec::image(w, w, static_cast<pixel_format>(255)), std::invalid_argument);
}

TEST(CodecImageBounds_Tests, SettersGivenTheImagesOwnBytes) {
    // set_exif and set_icc given a slice of the image's own block, whole or
    // a part, or of the other block: a copy is made before the old block goes
    codec::image picture(2, 2, pixel_format::rgb8);
    const auto exif = block_of(exif_be(8, 1));
    picture.set_exif(slice<const byte>(exif.data(), exif.size()));
    ASSERT_EQ(picture.orientation(), 6u);
    picture.set_exif(picture.exif());
    ASSERT_EQ(picture.exif().size(), exif.size());
    EXPECT_EQ(std::memcmp(picture.exif().data(), exif.data(), exif.size()), 0);
    EXPECT_EQ(picture.orientation(), 6u);
    // a part of itself: its last 10 bytes
    picture.set_exif(picture.exif().subspan(exif.size() - 10));
    ASSERT_EQ(picture.exif().size(), 10u);
    EXPECT_EQ(std::memcmp(picture.exif().data(), exif.data() + exif.size() - 10, 10), 0);
    EXPECT_EQ(picture.orientation(), 6u);   // a block without the tag leaves it
    std::vector<std::byte> profile(100);
    for (size_t i = 0; i < profile.size(); ++i) {
        profile[i] = std::byte(i);
    }
    picture.set_icc(slice<const byte>(profile.data(), profile.size()));
    picture.set_icc(picture.icc());
    ASSERT_EQ(picture.icc().size(), 100u);
    EXPECT_EQ(std::memcmp(picture.icc().data(), profile.data(), 100), 0);
    picture.set_icc(picture.icc().subspan(1, 50));
    ASSERT_EQ(picture.icc().size(), 50u);
    EXPECT_EQ(std::memcmp(picture.icc().data(), profile.data() + 1, 50), 0);
    // the one block from the other
    picture.set_exif(picture.icc());
    EXPECT_EQ(picture.exif().size(), 50u);
    EXPECT_NE(picture.exif().data(), picture.icc().data());
    // a slice of the pixels
    picture.set_icc(picture.pixels());
    EXPECT_EQ(picture.icc().size(), picture.pixels().size());
    // a slice kept from the block set over keeps the old bytes
    slice<const byte> old = picture.icc();
    picture.set_icc({});
    EXPECT_TRUE(picture.icc().empty());
    EXPECT_EQ(old.size(), 12u);
}

TEST(CodecImageBounds_Tests, AnExifBlockAtItsEdges) {
    // the orientation read only from a whole entry of IFD0: a directory at
    // the last place it fits, one byte short of it, an entry count past the
    // block, a tag of the wrong type or of no values, a byte order unknown
    auto orientation_of = [](const std::string& s) {
        codec::image im(1, 1, pixel_format::gray8);
        im.set_orientation(2);
        const auto b = block_of(s);
        im.set_exif(slice<const byte>(b.data(), b.size()));
        return unsigned(im.orientation());
    };
    const std::string whole = exif_be(8, 1);
    ASSERT_EQ(whole.size(), 22u);
    EXPECT_EQ(orientation_of(whole), 6u);
    EXPECT_EQ(orientation_of(whole.substr(0, 21)), 2u);   // the entry one byte short: no tag, as it was
    EXPECT_EQ(orientation_of(whole.substr(0, 20)), 2u);   // the value's own bytes past the end
    EXPECT_EQ(orientation_of(whole.substr(0, 8)), 2u);
    EXPECT_EQ(orientation_of(whole.substr(0, 7)), 2u);
    EXPECT_EQ(orientation_of(""), 2u);
    EXPECT_EQ(orientation_of(exif_be(8, 2)), 6u);                        // a second entry past the block: the first is whole
    EXPECT_EQ(orientation_of(exif_be(8, 0)), 2u);                        // no entries
    EXPECT_EQ(orientation_of(exif_be(8, 1, 0x0112, 4)), 2u);             // a LONG, not a SHORT
    EXPECT_EQ(orientation_of(exif_be(8, 1, 0x0112, 3, 0)), 2u);          // no values
    EXPECT_EQ(orientation_of(exif_be(8, 1, 0x0112, 3, 0xFFFFFFFF)), 6u); // many values: the first
    EXPECT_EQ(orientation_of(exif_be(8, 1, 0x0110)), 2u);                // another tag
    EXPECT_EQ(orientation_of(exif_be(8, 0xFFFF)), 6u);                   // a count past the block: the entries there
    std::string far = exif_be(8, 1);
    far[7] = char(21);   // IFD0 at the last two bytes: its count, no entry
    EXPECT_EQ(orientation_of(far), 2u);
    far[7] = char(22);   // past the block
    EXPECT_EQ(orientation_of(far), 2u);
    far[4] = far[5] = far[6] = far[7] = char(0xFF);   // an offset of 2^32 - 1
    EXPECT_EQ(orientation_of(far), 2u);
    std::string order = whole;
    order[0] = order[1] = 'X';
    EXPECT_EQ(orientation_of(order), 2u);
    std::string magic = whole;
    magic[3] = 43;
    EXPECT_EQ(orientation_of(magic), 2u);
    // a block whose tag is outside 1..8: 1, and set_orientation writes it
    codec::image im(1, 1, pixel_format::gray8);
    const auto nine = block_of(exif_be(8, 1, 0x0112, 3, 1, 9));
    im.set_exif(slice<const byte>(nine.data(), nine.size()));
    EXPECT_EQ(im.orientation(), 1u);
    im.set_orientation(8);
    EXPECT_EQ(uint8_t(im.exif()[19]), 8u);
    EXPECT_EQ(uint8_t(im.exif()[18]), 0u);
}

TEST(CodecImageBounds_Tests, SetOrientationAtItsBounds) {
    codec::image im(1, 1, pixel_format::gray8);
    im.set_orientation(1);
    im.set_orientation(8);
    EXPECT_EQ(im.orientation(), 8u);
    for (unsigned v : {0u, 9u, 256u + 6u, std::numeric_limits<unsigned>::max(), unsigned(-1)}) {
        EXPECT_THROW(im.set_orientation(v), std::invalid_argument) << v;
        EXPECT_EQ(im.orientation(), 8u);
    }
}

TEST(CodecImageBounds_Tests, ConvertAndOrientedKeepEveryChannelAtItsExtremes) {
    // the channels' minimum and maximum through every pair of formats: no
    // value out of range, white stays white and black stays black
    for (pixel_format from : AllFormats) {
        for (int v : {0, 1}) {
            codec::image im(3, 2, from);
            for (auto& b : im.pixels()) {
                b = v ? std::byte{0xFF} : std::byte{0};
            }
            for (pixel_format to : AllFormats) {
                codec::image c = im.convert(to);
                codec::image again = c.convert(from);
                // (CMYK has no place in the others; alpha dropped on the
                // way comes back opaque)
                const bool keeps = !codec::detail::alpha(from) || codec::detail::alpha(to) || v == 1;
                if (from != pixel_format::cmyk8 && to != pixel_format::cmyk8 && keeps) {
                    EXPECT_TRUE(same_pixels(again, im)) << int(from) << " -> " << int(to) << " v " << v;
                }
            }
        }
    }
}

TEST(CodecFormatBounds_Tests, SniffAtTheLengthOfEachSignature) {
    auto sniff = [](const std::string& s) { return codec::sniff(codec_test::bytes(s)); };
    // each signature whole is enough, one byte short is not
    EXPECT_EQ(sniff(std::string("\x89PNG\r\n\x1a\n", 8)), codec::format::png);
    EXPECT_EQ(sniff(std::string("\xFF\xD8\xFF", 3)), codec::format::jpeg);
    EXPECT_EQ(sniff("GIF87a"), codec::format::gif);
    EXPECT_EQ(sniff("GIF8"), nullopt);
    EXPECT_EQ(sniff(std::string("\xFF\xD8", 2)), nullopt);
    // ftyp: the major brand in 12 bytes, the box size what bounds the
    // compatible brands, and the bytes given
    auto ftyp = [](uint32_t size, const std::string& major, const std::string& brands) {
        return std::string{char(size >> 24), char(size >> 16), char(size >> 8), char(size)} + "ftyp" + major + std::string(4, '\0') + brands;
    };
    EXPECT_EQ(sniff(ftyp(12, "heic", "").substr(0, 12)), codec::format::heif);
    EXPECT_EQ(sniff(ftyp(12, "avif", "").substr(0, 12)), codec::format::avif);
    EXPECT_EQ(sniff(ftyp(12, "heic", "").substr(0, 11)), nullopt);
    EXPECT_EQ(sniff(ftyp(20, "mif1", "avif")), codec::format::avif);
    EXPECT_EQ(sniff(ftyp(19, "mif1", "avif")), nullopt);   // the brand past the box
    EXPECT_EQ(sniff(ftyp(20, "mif1", "avif").substr(0, 19)), nullopt);   // past the bytes given
    EXPECT_EQ(sniff(ftyp(0, "mif1", "avif")), nullopt);    // a box of size 0 (to the end of the file) holds no brand here
    EXPECT_EQ(sniff(ftyp(8, "mif1", "avif")), nullopt);    // a box smaller than its own header
    EXPECT_EQ(sniff(ftyp(0xFFFFFFFF, "mif1", "heicavif")), codec::format::heif);   // the first that is one
    EXPECT_EQ(sniff(ftyp(0xFFFFFFFF, "mif1", "isomavisheic")), codec::format::avif);
    // the 64th byte is the last a stream gives sniff: a brand ending there
    std::string brands;
    for (int i = 0; i < 11; ++i) {
        brands += "mif1";
    }
    const std::string at_end = ftyp(64, "mif1", brands + "heic");
    ASSERT_EQ(at_end.size(), 64u);
    EXPECT_EQ(sniff(at_end), codec::format::heif);
    EXPECT_EQ(codec::detail::SniffBytes, 64u);
}

TEST(CodecErrorBounds_Tests, CodesOffsetsAndWordsAtTheirEnds) {
    // a code outside the list: the category's words for it; the largest offset
    codec::error odd(static_cast<codec::errc>(200), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(odd.message(), "offset 18446744073709551615: unknown codec error");
    EXPECT_EQ(codec::codec_category().message(0), "unknown codec error");
    EXPECT_EQ(codec::codec_category().message(8), "unknown codec error");
    EXPECT_EQ(codec::codec_category().message(-1), "unknown codec error");
    EXPECT_EQ(&codec::codec_category(), &codec::codec_category());
    for (int c = 1; c <= 7; ++c) {
        const error_code ec = codec::make_error_code(static_cast<codec::errc>(c));
        EXPECT_EQ(ec.value(), c);
        EXPECT_EQ(&ec.category(), &codec::codec_category());
        EXPECT_TRUE(bool(ec));
        EXPECT_EQ(ec, static_cast<codec::errc>(c));
    }
    // an empty detail is no detail: the code's words
    EXPECT_EQ(codec::error(codec::errc::corrupt, 0, string()).message(), "offset 0: corrupt image data");
    // no code with words: still no error
    EXPECT_EQ(codec::error(codec::errc{}, 9, "x").message(), "no error");
    // the detail is part of the value
    EXPECT_NE(codec::error(codec::errc::corrupt, 0, "a"), codec::error(codec::errc::corrupt, 0, "b"));
    EXPECT_NE(codec::error(codec::errc::corrupt, 0), codec::error(codec::errc::corrupt, 1));
}

TEST(CodecErrorBounds_Tests, SelfAssignmentAndMovedFrom) {
    codec::error e(io::error(std::make_error_code(std::errc::io_error), "read", "x.png"), 3);
    const codec::error copy = e;
    codec::error& self = e;
    e = self;
    EXPECT_EQ(e, copy);
    e = std::move(self);
    EXPECT_EQ(e.code(), codec::errc::io);
    EXPECT_EQ(e.offset(), 3u);
    // a moved-from error is an error still: each member answers
    codec::error from(codec::errc::checksum, 5, "png: CRC-32 of chunk IHDR");
    codec::error to = std::move(from);
    EXPECT_EQ(to.message(), "offset 5: png: CRC-32 of chunk IHDR");
    EXPECT_EQ(from.code(), codec::errc::checksum);
    EXPECT_EQ(from.offset(), 5u);
    (void)from.message();
    (void)from.io_error();
    from = to;
    EXPECT_EQ(from, to);
}
