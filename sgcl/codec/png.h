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
#include "detail/png_decoder.h"
#include "detail/png_encoder.h"
#include "../compress/level.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

namespace sgcl::codec {
    // PNG: every color type and bit depth, Adam7, tRNS, the CRC of every
    // chunk and the Adler-32 of the image data checked; EXIF (eXIf) and
    // the ICC profile (iCCP) kept as bytes. The image comes in the file's
    // own pixel format unless decode_options.want asks for another
    // (detail/png_decoder.h says which is which). An APNG decodes to its
    // default image.
    //
    // Encoding: any image, as the PNG type that holds its format (8 or 16
    // bits a channel; cmyk8 as truecolor), each row filtered adaptively,
    // the zlib stream at options.level; EXIF and the ICC profile of the
    // image written as eXIf and iCCP (left out past 2^31 - 1 bytes, what a
    // chunk holds). Not interlaced, never a palette.
    namespace detail {
        // PNG's default level, a constant: copied, not made by
        // compress::level(int), which may throw, so that the options and
        // the forms that take them by default are noexcept
        inline constexpr compress::level PngLevel{7};

        // png::options, outside the class: a default member initializer of a
        // nested struct is not usable in the enclosing class's default arguments
        struct PngOptions {
            compress::level level = PngLevel;   // compress's: 0 stores, 1 fastest, 9 smallest; 7 the default here, the first of the chain levels (the filtered strategy, libpng's size)
        };
    }

    class png {
    public:
        using options = detail::PngOptions;

        // The file in memory, read in place
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::PngDecoder<detail::MemoryInput>(in, o).run();
        }

        // The file from a stream, read as it comes: memory is the image and
        // a constant (a block of the stream, the window of the zlib stream,
        // three rows), not the file
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::PngDecoder<detail::ReaderInput>(source, o).run();
        }

        // The file as bytes: errc::invalid_argument for a side past 2^31 - 1
        // pixels, which IHDR cannot hold (any other image encodes)
        static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::PngEncoder<detail::VectorSink>(im, o.level.value(), sink).run()) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        // The file into a stream: errc::invalid_argument for a side past
        // 2^31 - 1 pixels, nothing written; errc::io when the stream fails,
        // at the offset of the bytes written before
        static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::PngEncoder<detail::WriterSink>(im, o.level.value(), sink).run()) {
                return unexpected(*sink.failure);
            }
            return {};
        }
    };
}
