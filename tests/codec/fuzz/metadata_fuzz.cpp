//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The metadata reader (metadata.h) on any bytes: as a file of any format
// (the containers of JPEG, PNG, WebP, TIFF and HEIF walked, their EXIF and
// XMP read), and the same bytes as an EXIF block and an XMP packet. Every
// field is asked for, with a zone and without; what is read must be in its
// range (orientation 1..8, a place on the globe, a rating -1..5), and no
// read may pass the bytes (ASan), loop (the timeout) or throw.
#include "sgcl/codec/codec.h"

#include <cmath>
#include <cstdlib>

namespace {
    using namespace sgcl;

    void ask(const codec::metadata& m) {
        (void)m.make();
        (void)m.model();
        (void)m.lens_make();
        (void)m.lens_model();
        (void)m.software();
        (void)m.artist();
        (void)m.copyright();
        (void)m.description();
        (void)m.date_time_original();
        (void)m.date_time_digitized(time::zone::fixed(std::chrono::minutes(-330)));
        (void)m.date_time();
        (void)m.gps_time();
        (void)m.exposure_time();
        (void)m.f_number();
        (void)m.iso();
        (void)m.exposure_bias();
        (void)m.focal_length();
        (void)m.focal_length_35mm();
        (void)m.flash_fired();
        (void)m.width();
        (void)m.height();
        if (m.orientation() < 1 || m.orientation() > 8) {
            std::abort();
        }
        if (auto r = m.rating(); r && (*r < -1 || *r > 5)) {
            std::abort();
        }
        if (auto l = m.location(); l && !(std::fabs(l->latitude) <= 90 && std::fabs(l->longitude) <= 180)) {
            std::abort();
        }
        (void)m.exif();
        (void)m.xmp();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const slice<const byte> bytes(reinterpret_cast<const byte*>(data), size);
    if (auto m = codec::metadata::read(bytes, {.max_pixels = 1 << 20, .max_metadata = 1 << 20})) {
        ask(*m);
    }
    const size_t half = size / 2;
    ask(codec::metadata::from_exif(slice<const byte>(bytes.data(), half),
                                   string(reinterpret_cast<const char*>(data) + half, size - half)));
    return 0;
}
