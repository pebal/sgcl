//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HEIF and AVIF on any bytes (macOS): the module's wrapping of ImageIO, not
// ImageIO's decoder, is what is fuzzed:
//
//   - sniff on every prefix of the input agrees with itself (a longer head
//     never unsays a format);
//   - the file from memory and through a stream of pieces: taken or refused
//     alike, the same image;
//   - the limits: a canvas past 4 million pixels too_large, never decoded;
//     a limit of metadata below the profile too_large;
//   - every pixel format asked for: the image of the native one converted;
//   - one image in eight decoded, encoded again and decoded: taken (when the system
//     has an encoder), the same size.
//
// A difference aborts; a crash is the sanitizers'.
//
//   SGCL_FUZZ_LIBS="-framework ImageIO -framework CoreGraphics -framework CoreFoundation -framework Accelerate" \
//       tests/fuzz/run.sh tests/codec/fuzz/heif_decode_fuzz.cpp 300 -timeout=30
//
// -timeout=30 in place of run.sh's 5 s: the system's HEVC (VideoToolbox) can
// take 5 to 7 s on one input under the sanitizers with other fuzzers
// running (an input of 2 KB that decodes in 5.5 s alone); the time is the
// system's, not the module's.
//
// Seeds: seeds/heif_decode/ (PngSuite images made HEIC by sips, AVIF by
// ImageIO, and the module's own HEIC); the dictionary
// tests/fuzz/dict/heif_decode.dict (the boxes of ISOBMFF and HEIF).
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
            std::fprintf(stderr, "heif_decode_fuzz: %s\n", what);
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
    if (!said || (*said != codec::format::heif && *said != codec::format::avif)) {
        return 0;
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
    // encoded again (when the system encodes), and read back
    if (data[size - 1] % 8 != 0) {   // one input in eight: the encoder is the slow half
        return 0;
    }
    auto again = codec::heif::encode(*ours);
    if (again) {
        auto back = codec::heif::decode(slice<const byte>(again->data(), again->size()));
        check(bool(back), "the module's own file refused");
        check(back->width() == ours->width() && back->height() == ours->height(), "the module's own file of another size");
    } else {
        check(again.error().code() == codec::errc::unsupported, "an image the encoder refuses");
    }
    return 0;
}
