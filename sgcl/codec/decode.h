//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "format.h"
#include "image.h"
#include "options.h"
#include "gif.h"
#include "heif.h"
#include "jpeg.h"
#include "png.h"
#include "webp.h"
#include "detail/input.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../io/stream.h"

namespace sgcl::codec {
    namespace detail {
        inline error unknown_format() noexcept {
            return error(errc::unsupported, 0, "not an image format the module reads");
        }

        inline error not_animated() noexcept {
            return error(errc::unsupported, 0, "not an animation format the module reads (GIF, WebP)");
        }
    }

    // An image in any of the module's formats, told by its signature
    // (sniff): unsupported for anything else
    inline expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
        auto f = sniff(data);
        if (!f) {
            return unexpected(detail::unknown_format());
        }
        switch (*f) {
            case format::png:
                return png::decode(data, o);
            case format::jpeg:
                return jpeg::decode(data, o);
            case format::gif:
                return gif::decode(data, o);
            case format::webp: {
                detail::MemoryInput in(data);
                return detail::webp_first(in, o);
            }
            case format::heif:
            case format::avif:
                return detail::heif_decode_bytes(data, o);
        }
        return unexpected(detail::unknown_format());
    }

    // The same from a stream, read as it comes
    inline expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
        detail::ReaderInput source(in);
        auto head = source.head(detail::SniffBytes);
        if (!head) {
            return unexpected(*source.failure);
        }
        auto f = sniff(*head);
        if (!f) {
            return unexpected(detail::unknown_format());
        }
        switch (*f) {
            case format::png:
                return detail::PngDecoder<detail::ReaderInput>(source, o).run();
            case format::jpeg:
                return detail::JpegDecoder<detail::ReaderInput>(source, o).run();
            case format::gif:
                return detail::gif_first(source, o);
            case format::webp:
                return detail::webp_first(source, o);
            case format::heif:
            case format::avif:
                return detail::heif_decode_stream(source, o);
        }
        return unexpected(detail::unknown_format());
    }

    // Every frame of an animation (GIF, WebP; a still WebP is one frame),
    // told by its signature, read as next() asks; the bytes held while
    // the frames live. Another format is errc::unsupported
    inline expected<codec::frames, error> decode_frames(const slice<const byte>& data, const decode_options& o = {}) noexcept {

        auto f = sniff(data);
        if (f == format::gif) {
            return gif::frames(data, o);
        }
        if (f == format::webp) {
            return webp::frames(data, o);
        }
        return unexpected(detail::not_animated());
    }

    // Every frame from a stream
    inline expected<codec::frames, error> decode_frames(const io::reader& in, const decode_options& o = {}) {
        detail::ReaderInput source(in);
        auto head = source.head(12);
        if (!head) {
            return unexpected(*source.failure);
        }
        auto f = sniff(*head);
        if (f == format::gif) {
            return detail::gif_frames<detail::ReaderInput>(slice<const byte>(), source, o);
        }
        if (f == format::webp) {
            return detail::webp_frames<detail::ReaderInput>(slice<const byte>(), source, o);
        }
        return unexpected(detail::not_animated());
    }
}
