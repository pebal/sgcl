//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// GIF on any bytes: every frame read from memory and through a stream fed in
// pieces of the size the last byte picks, the two alike frame by frame (the
// same pixels, delays and loop count, or an error of the same code at the
// same frame); decode's image the first frame. At most 64 frames a file.
// Seeds: giflib's pictures, Go's testdata and animations of every disposal
// (seeds/gif_decode/); the dictionary tests/fuzz/dict/gif_decode.dict.
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
               std::memcmp(a.pixels().data(), b.pixels().data(), a.pixels().size()) == 0;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    codec::decode_options o;
    o.limits.max_pixels = 1u << 20;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    auto a = codec::gif::frames(in, o);
    pieces p{data, size, size ? 1 + data[size - 1] % 61u : 1};
    auto b = codec::gif::frames(io::reader(p), o);
    if (bool(a) != bool(b)) {
        std::abort();
    }
    if (!a) {
        if (a.error().code() != b.error().code()) {
            std::abort();
        }
        return 0;
    }
    if (a->width() != b->width() || a->height() != b->height()) {
        std::abort();
    }
    auto first = codec::gif::decode(in, o);
    for (int k = 0; k < 64; ++k) {
        auto x = a->next();
        auto y = b->next();
        if (bool(x) != bool(y)) {
            std::abort();
        }
        if (!x) {
            if (x.error().code() != y.error().code()) {
                std::abort();
            }
            break;
        }
        if (bool(*x) != bool(*y)) {
            std::abort();
        }
        if (!*x) {
            break;
        }
        if (!same((*x)->picture, (*y)->picture) || (*x)->delay != (*y)->delay) {
            std::abort();
        }
        if (k == 0 && (!first || !same(*first, (*x)->picture))) {
            std::abort();
        }
    }
    if (a->loop_count() != b->loop_count()) {
        std::abort();
    }
    return 0;
}
