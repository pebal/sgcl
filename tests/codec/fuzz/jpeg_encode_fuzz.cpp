//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The JPEG encoder on any image: the first bytes pick the pixel format, the
// size (up to 80 × 80), the quality, the sampling, -optimize and whether
// EXIF and an ICC profile come along; the rest are the pixels (repeated to
// fill the image, every byte written). The file must decode through the
// module to the size and the kind of the image (gray or color) with its
// metadata; encoding it again must give the same bytes, and the stream's
// bytes must be the vector's.
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
    if (size < 6) {
        return 0;
    }
    const auto f = static_cast<codec::pixel_format>(data[0] % 9);
    const uint32_t w = 1 + data[1] % 80;
    const uint32_t h = 1 + data[2] % 80;
    codec::jpeg::options o;
    o.quality = 1 + data[3] % 100;
    o.subsampling = static_cast<codec::jpeg::subsampling>(data[4] % 3);
    o.optimize = data[4] & 4;
    const bool meta = data[5] & 1;
    data += 6;
    size -= 6;
    codec::image picture(w, h, f);
    auto px = picture.pixels();
    for (size_t i = 0; i < px.size(); ++i) {
        px[i] = size ? std::byte(data[i % size]) : std::byte(i * 7);
    }
    if (meta && size) {
        auto& s = codec::detail::ImageAccess::state(picture);
        const size_t n = size / 2;
        s.exif.insert(s.exif.end(), reinterpret_cast<const byte*>(data), reinterpret_cast<const byte*>(data) + n);
        s.icc.insert(s.icc.end(), reinterpret_cast<const byte*>(data) + n, reinterpret_cast<const byte*>(data) + size);
    }
    vector<byte> file = codec::jpeg::encode(picture, o);
    auto back = codec::jpeg::decode(file);
    if (!back) {
        std::abort();
    }
    const bool gray = f == codec::pixel_format::gray8 || f == codec::pixel_format::gray16 || f == codec::pixel_format::gray_alpha8 ||
                      f == codec::pixel_format::gray_alpha16;
    if (back->width() != w || back->height() != h || back->format() != (gray ? codec::pixel_format::gray8 : codec::pixel_format::rgb8)) {
        std::abort();
    }
    const auto& s = codec::detail::ImageAccess::state(picture);
    if (back->exif().size() != s.exif.size() || back->icc().size() != s.icc.size() ||
        (s.exif.size() && std::memcmp(back->exif().data(), s.exif.data(), s.exif.size()) != 0) ||
        (s.icc.size() && std::memcmp(back->icc().data(), s.icc.data(), s.icc.size()) != 0)) {
        std::abort();
    }
    vector<byte> again = codec::jpeg::encode(picture, o);
    vector<byte> streamed;
    collect c{&streamed};
    if (again.size() != file.size() || std::memcmp(again.data(), file.data(), file.size()) != 0 ||
        !codec::jpeg::encode(picture, io::writer(c), o) || streamed.size() != file.size() ||
        std::memcmp(streamed.data(), file.data(), file.size()) != 0) {
        std::abort();
    }
    return 0;
}
