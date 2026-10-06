//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "options.h"
#include "detail/input.h"
#include "detail/jxl_apple.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../io/stream.h"

namespace sgcl::codec {
    // JPEG XL (ISO/IEC 18181), decoded through the system's codec as HEIF
    // is: ImageIO on macOS (14 and later), errc::unsupported elsewhere. The
    // module has no decoder of its own and writes none. The first frame of
    // the file, in its native format (gray or RGB, alpha when it has any,
    // 16 bits when its samples have more than 8);
    // the orientation of its header and its color profile come with it.
    // As heif's, decode takes the defaults of decode_options; codec::decode
    // takes the options for anything else.
    class jxl {
    public:
        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data) noexcept {
            return detail::jxl_decode_bytes(data, decode_options());
        }

        // The stream read to its end, then decoded as bytes are
        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in) {
            detail::ReaderInput source(in);
            return detail::jxl_decode_stream(source, decode_options());
        }
    };
}
