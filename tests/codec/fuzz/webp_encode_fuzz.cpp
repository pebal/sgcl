//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The WebP encoder on any image or animation: the first bytes pick the pixel
// format, the size (up to 48 × 48), the frames (1 to 3), lossless or lossy,
// the quality and the loop count; the rest are the pixels (repeated to fill
// every frame, each frame shifted so that frames differ in places, some
// frames repeated). The file must decode, frame by frame, to the canvases
// given (as rgba8): pixel for pixel when lossless; with the same alpha when
// lossy; the delays and the loop count as given, the stream's bytes the
// vector's.
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <cstring>

namespace {
    using namespace sgcl;

    // A file in a buffer of exactly its size from malloc, for the decoder:
    // ASan sees a read past it, which it does not in the managed heap
    struct exact {
        void* p;
        size_t n;

        explicit exact(const vector<byte>& v) : p(std::malloc(v.size() ? v.size() : 1)), n(v.size()) {
            if (n) {
                std::memcpy(p, v.data(), n);
            }
        }

        exact(const exact&) = delete;
        exact& operator=(const exact&) = delete;

        ~exact() {
            std::free(p);
        }

        slice<const byte> bytes() const {
            return slice<const byte>(static_cast<const byte*>(p), n);
        }
    };

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
    const uint32_t w = 1 + data[1] % 48;
    const uint32_t h = 1 + data[2] % 48;
    const size_t count = 1 + data[3] % 3;
    const bool lossless = data[4] & 1;
    const bool animated = data[4] & 2;
    const bool repeat = data[4] & 4;
    const int quality = 1 + data[5] % 100;
    const uint32_t loops = data[4] >> 3;
    data += 6;
    size -= 6;
    vector<codec::frame> frames;
    for (size_t k = 0; k < count; ++k) {
        if (repeat && k > 0) {
            frames.push_back({frames[k - 1].picture, std::chrono::milliseconds(7 * int(k))});
            continue;
        }
        codec::image picture(w, h, f);
        auto px = picture.pixels();
        for (size_t i = 0; i < px.size(); ++i) {
            px[i] = size ? std::byte(data[(i + k * 5 * (i / 11 % 2)) % size]) : std::byte(i * 3 + k);
        }
        frames.push_back({picture, std::chrono::milliseconds(7 * int(k))});
    }
    const codec::webp::options o{.lossless = lossless, .quality = quality, .loop_count = loops};
    auto file = animated ? codec::webp::encode(frames, o) : codec::webp::encode(frames[0].picture, o);
    if (!file) {
        std::abort();
    }
    const exact held(*file);
    auto back = codec::webp::frames(held.bytes());
    if (!back) {
        std::abort();
    }
    const size_t given = animated ? count : 1;
    for (size_t k = 0; k < given; ++k) {
        auto n = back->next();
        if (!n || !*n) {
            std::abort();
        }
        const codec::image want = frames[k].picture.convert(codec::pixel_format::rgba8);
        const codec::image got = (*n)->picture;
        if (got.width() != w || got.height() != h || got.format() != codec::pixel_format::rgba8) {
            std::abort();
        }
        auto a = want.pixels(), b = got.pixels();
        if (lossless) {
            if (std::memcmp(a.data(), b.data(), a.size()) != 0) {
                std::abort();
            }
        } else {
            for (size_t i = 3; i < a.size(); i += 4) {
                if (a[i] != b[i]) {
                    std::abort();
                }
            }
        }
        if (animated && (*n)->delay.milliseconds() != int64_t(7 * k)) {
            std::abort();
        }
    }
    auto end = back->next();
    if (!end || *end) {
        std::abort();
    }
    if (animated && back->loop_count() != loops) {
        std::abort();
    }
    vector<byte> streamed;
    collect c{&streamed};
    auto s = animated ? codec::webp::encode(frames, io::writer(c), o) : codec::webp::encode(frames[0].picture, io::writer(c), o);
    if (!s || streamed.size() != file->size() || std::memcmp(streamed.data(), file->data(), file->size()) != 0) {
        std::abort();
    }
    return 0;
}
