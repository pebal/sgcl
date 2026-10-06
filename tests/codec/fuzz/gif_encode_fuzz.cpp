//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The GIF encoder on any image or animation: the first bytes pick the pixel
// format, the size (up to 64 × 64), the frames (1 to 4), the colors (2 to
// 256), dithering and the loop count; the rest are the pixels (repeated to
// fill every frame, each frame shifted so that frames differ in places).
// The file must decode, frame by frame, to the canvases given as GIF shows
// them (alpha below 128 transparent): pixel for pixel where a frame has no
// more colors than the palette holds, else with the same transparent
// pixels and no more colors than asked; the loop count and the delays as
// given, and the stream's bytes the vector's.
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <cstring>
#include <set>

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

    // The frame as GIF shows it, rgba8
    codec::image shown(const codec::image& im) {
        codec::image out = im.convert(codec::pixel_format::rgba8);
        auto px = out.pixels();
        for (size_t i = 0; i < px.size(); i += 4) {
            if (uint8_t(px[i + 3]) < 128) {
                px[i] = px[i + 1] = px[i + 2] = px[i + 3] = std::byte(0);
            } else {
                px[i + 3] = std::byte(255);
            }
        }
        return out;
    }

    size_t colors_of(const codec::image& rgba, bool& transparent) {
        std::set<uint32_t> s;
        transparent = false;
        auto px = rgba.pixels();
        for (size_t i = 0; i < px.size(); i += 4) {
            uint32_t v;
            std::memcpy(&v, px.data() + i, 4);
            if (v == 0) {
                transparent = true;
            } else {
                s.insert(v);
            }
        }
        return s.size();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 6) {
        return 0;
    }
    const auto f = static_cast<codec::pixel_format>(data[0] % 9);
    const uint32_t w = 1 + data[1] % 64;
    const uint32_t h = 1 + data[2] % 64;
    const size_t count = 1 + data[3] % 4;
    const int colors = 2 + data[4] % 255;
    const bool dither = data[5] & 1;
    const bool animated = data[5] & 2;
    const uint32_t loops = data[5] >> 2;
    data += 6;
    size -= 6;
    vector<codec::frame> frames;
    for (size_t k = 0; k < count; ++k) {
        codec::image picture(w, h, f);
        auto px = picture.pixels();
        for (size_t i = 0; i < px.size(); ++i) {
            px[i] = size ? std::byte(data[(i + k * 7 * (i / 13 % 2)) % size]) : std::byte(i * 7 + k);
        }
        frames.push_back({picture, std::chrono::milliseconds(10 * int(k))});
    }
    const codec::gif::options o{.colors = colors, .dither = dither, .loop_count = loops};
    auto file = animated ? codec::gif::encode(frames, o) : codec::gif::encode(frames[0].picture, o);
    if (!file) {
        std::abort();
    }
    const exact held(*file);
    auto back = codec::gif::frames(held.bytes());
    if (!back) {
        std::abort();
    }
    const size_t given = animated ? count : 1;
    bool all_exact = true;   // every frame so far within the palette: the canvas as given
    for (size_t k = 0; k < given; ++k) {
        auto n = back->next();
        if (!n || !*n) {
            std::abort();
        }
        const codec::image want = shown(frames[k].picture);
        const codec::image& got = (*n)->picture;
        if (got.width() != w || got.height() != h || got.format() != codec::pixel_format::rgba8) {
            std::abort();
        }
        bool transparent = false;
        const size_t distinct = colors_of(want, transparent);
        all_exact = all_exact && distinct + (transparent ? 1 : 0) <= size_t(colors);
        if (all_exact) {
            if (std::memcmp(got.pixels().data(), want.pixels().data(), want.pixels().size()) != 0) {
                std::abort();
            }
        } else {
            // the transparent pixels where given; a canvas of one frame
            // with no more colors than asked (a later one shows the
            // palettes of several)
            auto a = want.pixels(), b = got.pixels();
            for (size_t i = 3; i < a.size(); i += 4) {
                if ((a[i] == std::byte(0)) != (b[i] == std::byte(0))) {
                    std::abort();
                }
            }
            bool t = false;
            if (k == 0 && colors_of(got, t) > size_t(colors)) {
                std::abort();
            }
        }
        if (animated && (*n)->delay.milliseconds() != int64_t(10 * k)) {
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
    auto s = animated ? codec::gif::encode(frames, io::writer(c), o) : codec::gif::encode(frames[0].picture, io::writer(c), o);
    if (!s || streamed.size() != file->size() || std::memcmp(streamed.data(), file->data(), file->size()) != 0) {
        std::abort();
    }
    return 0;
}
