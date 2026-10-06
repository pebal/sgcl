//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "imageio_decode.h"
#include "input.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../core/expected.h"
#include "../../core/slice.h"

#include <cstdint>

// JPEG XL (ISO/IEC 18181) through the system's codec: ImageIO on Apple's
// systems (macOS 14 and later decode it), unsupported elsewhere. The
// module has no decoder of its own: the first frame of the file as the
// system gives it, as HEIF is read (heif_apple.h, imageio_decode.h).
namespace sgcl::codec::detail {
    inline error jxl_unsupported() noexcept {
        return error(errc::unsupported, 0, "jxl: needs the system's codec (macOS ImageIO)");
    }

#if defined(__APPLE__)
    // The types ImageIO names JPEG XL by: a bare codestream and the ISOBMFF
    // container hold the same image
    inline bool jxl_type(apple::Ref type) noexcept {
        return imageio_type_in(type, {"public.jpeg-xl"});
    }

    // Whether this system's ImageIO decodes a JPEG XL of gray with alpha
    // wrong (macOS 26's does: its pixels come out 0 and 255 whatever the
    // file holds, and ImageIO draws such a file transparent). Asked of the
    // system once, on a file of two known pixels (gray 10 alpha 200, gray
    // 250 alpha 50, made by cjxl -d 0), so that a system that reads them
    // right is taken as it is
    inline bool jxl_gray_alpha_broken() noexcept {
        static const bool broken = [] {
            static constexpr uint8_t probe[] = {0xff, 0x0a, 0x00, 0x70, 0xb0, 0x28, 0x6e, 0x04, 0x08, 0x00, 0x10, 0x00, 0x30,
                                                0x00, 0x4b, 0x18, 0x8b, 0x15, 0xc2, 0x49, 0x55, 0x0e, 0x0a, 0x1e, 0xf2, 0x0a};
            static constexpr ImageioKind plain{"jxl", jxl_type, nullptr, "not a JPEG XL file to ImageIO"};
            auto im = imageio_decode_memory(probe, sizeof(probe), decode_options(), plain);
            if (!im || im->format() != pixel_format::gray_alpha8) {
                return true;
            }
            const auto row = im->row(0);
            return !(uint8_t(row[0]) == 10 && uint8_t(row[1]) == 200 && uint8_t(row[2]) == 250 && uint8_t(row[3]) == 50);
        }();
        return broken;
    }

    inline constexpr ImageioKind JxlKind{"jxl", jxl_type, nullptr, "not a JPEG XL file to ImageIO", jxl_gray_alpha_broken};
#endif

    // The first frame of a JPEG XL file in memory, read in place
    SGCL_INLINE_HOT expected<image, error> jxl_decode_bytes(const slice<const byte>& data, const decode_options& o) noexcept {
#if defined(__APPLE__)
        return imageio_decode_memory(reinterpret_cast<const uint8_t*>(data.data()), data.size(), o, JxlKind);
#else
        (void)data;
        (void)o;
        return unexpected(jxl_unsupported());
#endif
    }

    // The same from an input (a stream, its head already held), read to
    // its end first
    template<class Input>
    SGCL_INLINE_HOT expected<image, error> jxl_decode_stream(Input& in, const decode_options& o) {
#if defined(__APPLE__)
        return imageio_decode_input(in, o, JxlKind);
#else
        (void)in;
        (void)o;
        return unexpected(jxl_unsupported());
#endif
    }
}
