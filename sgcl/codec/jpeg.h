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
#include "detail/jpeg_decoder.h"
#include "detail/jpeg_encoder.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

namespace sgcl::codec {
    // JPEG: the DCT modes with Huffman coding of 8-bit samples (baseline,
    // extended and progressive, successive approximation included), every
    // sampling factor, restart intervals, gray, YCbCr, RGB, CMYK and YCCK;
    // the pixels those of libjpeg-turbo's `djpeg -dct int` bit for bit (its
    // integer IDCT, its "fancy" upsampling, its YCbCr conversion). EXIF
    // (orientation() from it; the image is not turned) and the ICC profile
    // kept as bytes. The image comes gray8, rgb8 or cmyk8 unless
    // decode_options.want asks for another. Arithmetic coding, 12-bit
    // samples, lossless and hierarchical files: unsupported.
    //
    // Encoding: baseline JPEG, byte for byte what libjpeg-turbo's cjpeg
    // writes with `-dct int -baseline` and the same quality, sampling and
    // -optimize: the tables of Annex K scaled by the IJG's quality, its
    // integer FDCT, JFIF; the image's EXIF and ICC profile as APP1 and
    // APP2. A gray image (gray8, gray16, gray with alpha) becomes a gray
    // JPEG, any other YCbCr (alpha dropped, 16 bits to 8, CMYK through
    // RGB).
    class jpeg {
    public:
        // The resolution of the chrominance: whole (4:4:4), half across
        // (4:2:2), half both ways (4:2:0)
        enum class subsampling : uint8_t {
            s444,
            s422,
            s420
        };

        struct options {
            int quality = 85;                                            // 1..100, the IJG's scale
            jpeg::subsampling subsampling = jpeg::subsampling::s420;
            bool optimize = false;                                       // Huffman tables made for the image: smaller, two passes
        };

        // The file in memory, read in place
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) {
            detail::MemoryInput in(data);
            return detail::JpegDecoder<detail::MemoryInput>(in, o).run();
        }

        // The file from a stream, read as it comes: memory is the image and
        // three rows of MCUs (a file of one scan per component: the
        // components whole), not the file
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::JpegDecoder<detail::ReaderInput>(source, o).run();
        }

        // The file as bytes: a valid image always encodes; a quality
        // outside 1..100 is invalid_argument (a contract). (The overloads
        // without options stand for a default argument, which a nested
        // struct with member initializers cannot be inside its class.)
        static vector<byte> encode(const image& im) {
            return encode(im, options{});
        }

        static vector<byte> encode(const image& im, const options& o) {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            detail::JpegEncoder<detail::VectorSink>(im, _settings(o), sink).run();
            return out;
        }

        // The file into a stream: errc::io when the stream fails, at the
        // offset of the bytes written before
        static expected<void, error> encode(const image& im, const io::writer& out) {
            return encode(im, out, options{});
        }

        static expected<void, error> encode(const image& im, const io::writer& out, const options& o) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::JpegEncoder<detail::WriterSink>(im, _settings(o), sink).run()) {
                return unexpected(*sink.failure);
            }
            return {};
        }

    private:
        static detail::JpegEncodeSettings _settings(const options& o) {
            if (o.quality < 1 || o.quality > 100) {
                throw invalid_argument("sgcl::codec::jpeg::encode: quality outside 1..100");
            }
            detail::JpegEncodeSettings s;
            s.quality = o.quality;
            s.h = o.subsampling == subsampling::s444 ? 1 : 2;
            s.v = o.subsampling == subsampling::s420 ? 2 : 1;
            s.optimize = o.optimize;
            return s;
        }
    };
}
