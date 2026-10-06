//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// BMP, TIFF, ICO/CUR, QOI and Netpbm on any bytes: the first byte picks the
// format (its decoder takes the rest, whatever its signature) or none
// (codec::decode tells it by its signature); the file read from memory and
// through a stream fed in pieces of the size the last byte picks, the two
// alike (the same pixels, or an error of the same code); every page of a
// TIFF and every entry of an icon read. Seeds: files of each format made by
// the module and by ffmpeg (seeds/formats_decode/).
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

    void alike(const expected<codec::image, codec::error>& a, const expected<codec::image, codec::error>& b) {
        if (bool(a) != bool(b) || (a && !same(*a, *b)) || (!a && a.error().code() != b.error().code())) {
            std::abort();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const unsigned which = data[0] % 6;
    ++data;
    --size;
    codec::decode_options o;
    o.limits.max_pixels = 1u << 20;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    pieces p{data, size, size ? 1 + data[size - 1] % 61u : 1};
    const io::reader stream(p);
    switch (which) {
        case 0:
            alike(codec::bmp::decode(in, o), codec::bmp::decode(stream, o));
            break;
        case 1: {
            alike(codec::tiff::decode(in, o), codec::tiff::decode(stream, o));
            auto all = codec::tiff::decode_all(in, o);
            (void)all;
            break;
        }
        case 2: {
            alike(codec::ico::decode(in, o), codec::ico::decode(stream, o));
            auto all = codec::ico::decode_all(in, o);
            (void)all;
            break;
        }
        case 3:
            alike(codec::qoi::decode(in, o), codec::qoi::decode(stream, o));
            break;
        case 4:
            alike(codec::pnm::decode(in, o), codec::pnm::decode(stream, o));
            break;
        default:
            alike(codec::decode(in, o), codec::decode(stream, o));
            break;
    }
    return 0;
}
