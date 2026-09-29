//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The PNG encoder on any image: the first bytes pick the pixel format, the
// size (up to 96 × 96), the zlib level and whether EXIF and an ICC profile
// come along; the rest are the pixels (repeated to fill the image, every
// byte written). The file must decode to the image (a cmyk8 image to its
// rgb8 conversion) with its metadata, and the stream's bytes must be the
// vector's.
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <cstring>

namespace {
    using namespace sgcl;

    struct collect {
        vector<byte>* out;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            out->insert(out->end(), b.begin(), b.end());
            return b.size();
        }
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 5) {
        return 0;
    }
    const auto f = static_cast<codec::pixel_format>(data[0] % 9);
    const uint32_t w = 1 + data[1] % 96;
    const uint32_t h = 1 + data[2] % 96;
    const int levels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, compress::level::huffman_only};
    const int level = levels[data[3] % 11];
    const bool meta = data[4] & 1;
    data += 5;
    size -= 5;
    codec::image picture(w, h, f);
    auto px = picture.pixels();
    for (size_t i = 0; i < px.size(); ++i) {
        px[i] = size ? std::byte(data[i % size]) : std::byte(i * 7);
    }
    if (meta) {
        auto& s = codec::detail::ImageAccess::state(picture);
        const size_t n = size / 2;
        s.exif.insert(s.exif.end(), reinterpret_cast<const byte*>(data), reinterpret_cast<const byte*>(data) + n);
        s.icc.insert(s.icc.end(), reinterpret_cast<const byte*>(data) + n, reinterpret_cast<const byte*>(data) + size);
    }
    vector<byte> file = codec::png::encode(picture, {.level = level});
    auto back = codec::png::decode(file);
    if (!back) {
        std::abort();
    }
    const auto readable = f == codec::pixel_format::cmyk8 ? codec::pixel_format::rgb8 : f;
    codec::image expected_pixels = picture.convert(readable);
    if (back->format() != readable || back->width() != w || back->height() != h ||
        std::memcmp(back->pixels().data(), expected_pixels.pixels().data(), expected_pixels.pixels().size()) != 0) {
        std::abort();
    }
    const auto& s = codec::detail::ImageAccess::state(picture);
    if (back->exif().size() != s.exif.size() || back->icc().size() != s.icc.size() ||
        (s.exif.size() && std::memcmp(back->exif().data(), s.exif.data(), s.exif.size()) != 0) ||
        (s.icc.size() && std::memcmp(back->icc().data(), s.icc.data(), s.icc.size()) != 0)) {
        std::abort();
    }
    vector<byte> streamed;
    collect c{&streamed};
    if (!codec::png::encode(picture, io::writer(c), {.level = level}) || streamed.size() != file.size() ||
        std::memcmp(streamed.data(), file.data(), file.size()) != 0) {
        std::abort();
    }
    return 0;
}
