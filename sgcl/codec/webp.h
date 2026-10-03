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
#include "detail/webp_container.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
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
    class webp {
    public:
        // The image, the file in memory read in place
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::webp_first(in, o);
        }

        // The image from a stream, read as it comes: memory is the image,
        // its ARGB words and a block of the stream
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::webp_first(source, o);
        }

        // Every frame, read one by one as next() asks. The bytes are held
        // while the frames live (a slice of unmanaged memory must outlive
        // them)
        static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {}) noexcept {

            return detail::webp_frames<detail::MemoryInput>(data, data, o);
        }

        // Every frame from a stream, read as next() asks
        static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {}) {
            return detail::webp_frames<detail::ReaderInput>(slice<const byte>(), in, o);
        }
    };
}
