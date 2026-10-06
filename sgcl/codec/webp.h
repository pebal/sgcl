//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "frames.h"
#include "image.h"
#include "options.h"
#include "detail/input.h"
#include "detail/output.h"
#include "detail/webp_container.h"
#include "detail/webp_encoder.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

namespace sgcl::codec {
    // WebP (RFC 9649): the container, simple and extended (VP8X, ICCP, EXIF,
    // ANIM and ANMF), lossless images (VP8L) and lossy ones (VP8, RFC 6386,
    // their alpha in ALPH). decode gives a still image in the file's own
    // format (rgba8 when it says it has alpha, rgb8 when not) and an
    // animation's first frame on its canvas; frames gives every frame,
    // each the whole canvas in rgba8. decode_options set another pixel
    // format, other limits, no metadata.
    //
    // The canvas of an animation is composed here, by RFC 9649: transparent
    // at the start (the background color of ANIM is a hint, left as
    // libwebp's WebPAnimDecoder and browsers leave it), a frame disposed of
    // cleared to transparent, a frame alpha-blended by RFC 9649's formula
    // in integers, rounded to the nearest. The frames' pixels are libwebp's
    // bit for bit; a blended pixel may differ from WebPAnimDecoder's, whose
    // blend is an approximation in fixed point: by one in alpha, and in a
    // color by up to about 255 / A for a pixel of alpha A.
    //
    // Encoding (detail/webp_encoder.h): lossless (VP8L, every pixel as it
    // is, detail/vp8l_encoder.h) or lossy (VP8, its alpha lossless in ALPH,
    // detail/vp8_encoder.h) at options.quality, cwebp's -q; an animation of
    // whole canvases as ANMF frames of what changed. The image's EXIF and
    // ICC profile are written with it.
    class webp {
    public:
        using options = detail::WebpOptions;

        // The image, the file in memory read in place
        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::webp_first(in, o);
        }

        // The image from a stream, read as it comes: memory is the image,
        // its ARGB words and a block of the stream
        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::webp_first(source, o);
        }

        // Every frame, read one by one as next() asks. The bytes are held
        // while the frames live (a slice of unmanaged memory must outlive
        // them)
        SGCL_INLINE_HOT static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {}) noexcept {

            return detail::webp_frames<detail::MemoryInput>(data, data, o);
        }

        // Every frame from a stream, read as next() asks
        SGCL_INLINE_HOT static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {}) {
            return detail::webp_frames<detail::ReaderInput>(slice<const byte>(), in, o);
        }

        // The file of a still image as bytes: errc::invalid_argument for a
        // side past 16384 pixels (VP8's and VP8L's 14 bits) or
        // options.quality outside 1..100 (any other image encodes)
        SGCL_INLINE_HOT static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::WebpEncoder<detail::VectorSink>(sink, o).still(im)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        // The file into a stream, written whole once made (RIFF's size
        // comes first): the same refusals, nothing written; errc::io when
        // the stream fails
        SGCL_INLINE_HOT static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::WebpEncoder<detail::WriterSink>(sink, o).still(im)) {
                return unexpected(*sink.failure);
            }
            return {};
        }

        // An animation of whole canvases as bytes, each frame shown for its
        // delay, played options.loop_count times: errc::invalid_argument
        // also for no frame or a frame of another size than the first
        SGCL_INLINE_HOT static expected<vector<byte>, error> encode(const slice<const frame>& animation, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::WebpEncoder<detail::VectorSink>(sink, o).animation(animation)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        SGCL_INLINE_HOT static expected<void, error> encode(const slice<const frame>& animation, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::WebpEncoder<detail::WriterSink>(sink, o).animation(animation)) {
                return unexpected(*sink.failure);
            }
            return {};
        }
    };
}
