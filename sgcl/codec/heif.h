//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "options.h"
#include "detail/heif_apple.h"
#include "detail/input.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <cstdint>
#include <utility>

namespace sgcl::codec {
    // HEIF: HEIC photos (HEVC) and, where the system reads them, AVIF
    // (AV1, the same container), through the system's codec: ImageIO on
    // macOS. The module has no HEVC or AV1 decoder of its own; elsewhere
    // every call is errc::unsupported. The first image of the file.
    //
    // Decoding: gray or RGB as the file has it (another color model comes
    // in sRGB), alpha straight, 16 bits a channel when the file has more
    // than 8 (10-bit HDR among them, its profile in icc(), no tone
    // mapping). A file whose alpha ImageIO gives premultiplied is
    // unpremultiplied, which loses what premultiplying lost. orientation()
    // is ImageIO's reading of the file's rotation and mirror; exif() is
    // empty (ImageIO gives the tags, not the block). A stream is read to
    // its end before ImageIO sees it. A file with an HEVC slice whose entry
    // points lie past its data, on which VideoToolbox waits for ever, is
    // errc::corrupt before ImageIO sees it.
    //
    // Encoding: HEIC at a quality of 1 to 100 (85 by default), the image's
    // ICC profile and orientation with it; 16-bit images as the system's
    // encoder writes them (10 bits); CMYK through RGB, 16-bit gray with
    // alpha through RGBA (the system writes neither). errc::unsupported
    // when the system has no HEVC encoder (some virtual machines).
    class heif {
    public:
        // What an encoding takes: the quality, 1 to 100; one outside is
        // errc::invalid_argument at encode
        struct options {
            int quality = 85;
        };

        // The image, the file in memory read in place
        static expected<image, error> decode(const slice<const byte>& data) noexcept {
            return detail::heif_decode_bytes(data, decode_options());
        }

        // The image from a stream
        static expected<image, error> decode(const io::reader& in) {
            detail::ReaderInput source(in);
            return detail::heif_decode_stream(source, decode_options());
        }

        // The file as bytes. (The overloads without options stand for a
        // default argument, which a nested struct with member initializers
        // cannot be inside its class.)
        static expected<vector<byte>, error> encode(const image& im) noexcept {
            return encode(im, options{});
        }

        static expected<vector<byte>, error> encode(const image& im, const options& o) noexcept {
#if defined(__APPLE__)
            return detail::heif_encode(im, o.quality);
#else
            (void)im;
            (void)o;
            return unexpected(detail::heif_unsupported());
#endif
        }

        // The file into a stream: errc::io when the stream fails, at the
        // offset of the bytes written before
        static expected<void, error> encode(const image& im, const io::writer& out) {
            return encode(im, out, options{});
        }

        static expected<void, error> encode(const image& im, const io::writer& out, const options& o) {
#if defined(__APPLE__)
            return detail::heif_encode(im, o.quality, out);
#else
            (void)im;
            (void)out;
            (void)o;
            return unexpected(detail::heif_unsupported());
#endif
        }
    };
}
