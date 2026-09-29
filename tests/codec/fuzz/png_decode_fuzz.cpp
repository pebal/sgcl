//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PNG on any bytes: decoded in memory and through a stream fed in pieces of
// the size the last byte picks, the two alike (the same pixels, or an error
// of the same code); an image that decodes decodes again into the format
// the length picks, as the native one converted. Seeds: PngSuite files
// (seeds/png_decode/), the dictionary tests/fuzz/dict/png_decode.dict.
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <cstring>

namespace {
    using namespace sgcl;

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

    bool same(const codec::image& a, const codec::image& b) {
        return a.width() == b.width() && a.height() == b.height() && a.format() == b.format() &&
               std::memcmp(a.pixels().data(), b.pixels().data(), a.pixels().size()) == 0 &&
               a.exif().size() == b.exif().size() && a.icc().size() == b.icc().size() && a.orientation() == b.orientation();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    codec::decode_options o;
    o.limits.max_pixels = 1u << 22;
    o.limits.max_metadata = 1u << 20;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    auto whole = codec::png::decode(in, o);
    pieces p{data, size, size ? 1 + data[size - 1] % 61u : 1};
    auto streamed = codec::png::decode(io::reader(p), o);
    if (bool(whole) != bool(streamed)) {
        std::abort();
    }
    if (!whole) {
        if (whole.error().code() != streamed.error().code()) {
            std::abort();
        }
        return 0;
    }
    if (!same(*whole, *streamed)) {
        std::abort();
    }
    auto want = static_cast<codec::pixel_format>(size % 9);
    o.want = want;
    auto converted = codec::png::decode(in, o);
    if (!converted || !same(*converted, whole->convert(want))) {
        std::abort();
    }
    return 0;
}
