//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The encoders of BMP, TIFF, ICO, QOI and Netpbm on any image: the first
// bytes pick the pixel format, the size (up to 64 × 64), the format and its
// option (TIFF's compression, Netpbm's kind and plain form); the rest are
// the pixels. The file must decode to what the format holds of the image,
// pixel for pixel (TIFF the image itself; QOI, BMP and ICO its 8-bit RGB,
// RGBA or gray; Netpbm by its kind), and the stream's bytes must be the
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

    void equal(const codec::image& got, const codec::image& want) {
        if (got.width() != want.width() || got.height() != want.height() || got.format() != want.format() ||
            std::memcmp(got.pixels().data(), want.pixels().data(), want.pixels().size()) != 0) {
            std::abort();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 5) {
        return 0;
    }
    const auto f = static_cast<codec::pixel_format>(data[0] % 9);
    const uint32_t w = 1 + data[1] % 64, h = 1 + data[2] % 64;
    const unsigned which = data[3] % 5, option = data[4];
    data += 5;
    size -= 5;
    codec::image im(w, h, f);
    auto px = im.pixels();
    for (size_t i = 0; i < px.size(); ++i) {
        px[i] = size ? std::byte(data[i % size]) : std::byte(i * 13);
    }
    const bool alpha = codec::detail::alpha(f);
    vector<byte> file, streamed;
    collect c{&streamed};
    switch (which) {
        case 0: {
            file = *codec::bmp::encode(im);
            if (!codec::bmp::encode(im, io::writer(c))) {
                std::abort();
            }
            const auto held = alpha ? codec::pixel_format::rgba8 : f == codec::pixel_format::gray8 ? codec::pixel_format::gray8 : codec::pixel_format::rgb8;
            equal(*codec::bmp::decode(exact(file).bytes()), im.convert(held));
            break;
        }
        case 1: {
            const codec::tiff::options o{.compression = codec::tiff::compression(option % 3)};
            file = *codec::tiff::encode(im, o);
            if (!codec::tiff::encode(im, io::writer(c), o)) {
                std::abort();
            }
            equal(*codec::tiff::decode(exact(file).bytes()), im);
            break;
        }
        case 2: {
            file = *codec::ico::encode(im);
            if (!codec::ico::encode(im, io::writer(c))) {
                std::abort();
            }
            equal(*codec::ico::decode(exact(file).bytes()), im.convert(codec::pixel_format::rgba8));
            break;
        }
        case 3: {
            file = *codec::qoi::encode(im);
            if (!codec::qoi::encode(im, io::writer(c))) {
                std::abort();
            }
            equal(*codec::qoi::decode(exact(file).bytes()), im.convert(alpha ? codec::pixel_format::rgba8 : codec::pixel_format::rgb8));
            break;
        }
        default: {
            const auto kind = codec::pnm::kind(option % 5);
            const bool plain = (option / 5) & 1;
            auto r = codec::pnm::encode(im, {.kind = kind, .plain = plain});
            const bool pam = kind == codec::pnm::kind::pam || (kind == codec::pnm::kind::automatic && alpha);
            if (pam && plain) {
                if (r) {
                    std::abort();
                }
                return 0;
            }
            file = *r;
            if (!codec::pnm::encode(im, io::writer(c), {.kind = kind, .plain = plain})) {
                std::abort();
            }
            const exact copy(file);
            auto back = codec::pnm::decode(copy.bytes());
            if (!back) {
                std::abort();
            }
            break;
        }
    }
    if (streamed.size() != file.size() || std::memcmp(streamed.data(), file.data(), file.size()) != 0) {
        std::abort();
    }
    return 0;
}
