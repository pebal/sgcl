//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JPEG XL on any bytes (macOS): the module's wrapping of ImageIO, not
// ImageIO's decoder, is what is fuzzed, as heif_decode_fuzz does for HEIF:
//
//   - sniff on every prefix of the input agrees with itself;
//   - the file from memory and through a stream of pieces: taken or refused
//     alike, the same image;
//   - the limits: a canvas past 4 million pixels too_large, never decoded;
//     a limit of metadata below the profile too_large;
//   - every pixel format asked for: the image of the native one converted;
//   - metadata::read of the same bytes neither crashes nor loops (the
//     container's Exif, xml and brob boxes).
//
// A difference aborts; a crash is the sanitizers'. The input is parsed
// from libFuzzer's own buffer (ASan sees a read past it).
//
//   SGCL_FUZZ_LIBS="-framework ImageIO -framework CoreGraphics -framework CoreFoundation -framework Accelerate -framework Security" \
//       tests/fuzz/run.sh tests/codec/fuzz/jxl_decode_fuzz.cpp 300 -timeout=30
//
// Seeds: seeds/jxl_decode/ (PngSuite images and an animated GIF made JPEG
// XL by cjxl, lossless and lossy, bare and in the container, a JPEG
// recompressed with its EXIF in a brob box).
#include "sgcl/codec/codec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    using namespace sgcl;

    constexpr uint64_t MaxPixels = uint64_t(1) << 22;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "jxl_decode_fuzz: %s\n", what);
            std::abort();
        }
    }

    struct pieces {
        const uint8_t* data;
        size_t size;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            size_t n = std::min({step, b.size(), size - at});
            std::memcpy(b.data(), data + at, n);
            at += n;
            return n;
        }
    };

    bool same_pixels(const codec::image& a, const codec::image& b) {
        auto p = a.pixels(), q = b.pixels();
        return a.width() == b.width() && a.height() == b.height() && a.format() == b.format() && p.size() == q.size() &&
               std::memcmp(p.data(), q.data(), p.size()) == 0;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const slice<const byte> bytes(reinterpret_cast<const byte*>(data), size);
    // sniff: a longer head never unsays what a shorter one said
    optional<codec::format> said;
    for (size_t n = 0; n <= std::min<size_t>(size, codec::detail::SniffBytes); ++n) {
        auto f = codec::sniff(slice<const byte>(bytes.data(), n));
        if (said) {
            check(f && *f == *said, "sniff unsays a format on a longer head");
        }
        said = f;
    }
    if (!said || *said != codec::format::jxl) {
        return 0;
    }
    if (auto m = codec::metadata::read(bytes, {.max_pixels = MaxPixels, .max_metadata = 1 << 20})) {
        (void)m->make();
        (void)m->date_time_original();
    }
    const codec::decode_options d{.limits = {.max_pixels = MaxPixels}};
    auto ours = codec::decode(bytes, d);
    // the stream: the same answer, the same image
    pieces p{data, size, size ? size_t(data[size - 1]) % 61 + 1 : 1};
    auto streamed = codec::decode(io::reader(p), d);
    check(bool(ours) == bool(streamed), "a stream answers otherwise than memory");
    if (ours) {
        check(same_pixels(*ours, *streamed), "a stream's image is not memory's");
    } else {
        check(ours.error().code() == streamed.error().code(), "a stream's error is not memory's");
        const auto c = ours.error().code();
        check(c == codec::errc::corrupt || c == codec::errc::unexpected_end || c == codec::errc::unsupported || c == codec::errc::too_large,
              "an error of another kind");
        return 0;
    }
    check(uint64_t(ours->width()) * ours->height() <= MaxPixels, "an image past max_pixels");
    // every format asked for: the native image converted
    for (auto f : {codec::pixel_format::gray8, codec::pixel_format::rgba8, codec::pixel_format::rgb16, codec::pixel_format::cmyk8}) {
        codec::decode_options as = d;
        as.want = f;
        auto asked = codec::decode(bytes, as);
        check(bool(asked), "a format asked for refuses what the native one takes");
        check(same_pixels(*asked, ours->convert(f)), "a format asked for is not the native image converted");
    }
    // a limit of metadata below the profile
    if (!ours->icc().empty()) {
        codec::decode_options tight = d;
        tight.limits.max_metadata = ours->icc().size() - 1;
        auto small = codec::decode(bytes, tight);
        check(!small && small.error().code() == codec::errc::too_large, "a profile past max_metadata taken");
    }
    return 0;
}
