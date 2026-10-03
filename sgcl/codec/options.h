//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/pixels.h"
#include "../core/aliases.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace sgcl::codec {
    // What a decoder accepts before it allocates: the pixels a file may
    // claim (a small file can claim a huge image, a pixel bomb) and the
    // bytes of metadata it may carry. Past either, errc::too_large, with
    // nothing allocated for the image. Pillow warns from about 89 million
    // pixels and refuses from about 179 million; Go has no limit.
    struct limits {
        uint64_t max_pixels = 100'000'000;
        size_t max_metadata = 64u << 20;
    };

    struct decode_options {
        // The pixel format of the result: when not given, the file's own
        // (a gray PNG stays gray8 or gray16, a CMYK JPEG cmyk8); when
        // given, each row is converted as it is decoded, with no second
        // pass over the image (image::convert says how)
        optional<pixel_format> want;
        codec::limits limits;
        // EXIF and the ICC profile; false leaves both empty (orientation
        // 1)
        bool metadata = true;
    };

    namespace detail {
        // The size a file claims, against the limits: corrupt for a side
        // of zero, too_large past max_pixels, and past what the image's
        // buffer can be at 8 bytes a pixel (rgba16) whatever max_pixels
        // says (the image's constructor would throw length_error there; PNG
        // and HEIF sides reach it), at the offset of the header that
        // claimed it
        inline optional<error> check_size(uint32_t width, uint32_t height, const codec::limits& l, uint64_t offset) noexcept {
            if (width == 0 || height == 0) {
                return error(errc::corrupt, offset, "image of zero pixels");
            }
            const uint64_t pixels = uint64_t(width) * height;
            if (pixels > l.max_pixels || pixels > uint64_t(std::numeric_limits<ptrdiff_t>::max()) / 8) {
                return error(errc::too_large, offset);
            }
            return nullopt;
        }
    }
}
