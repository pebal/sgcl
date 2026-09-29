//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/pixels.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/detail/handle_word.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sgcl::codec {
    class image;
    struct save_options;   // files.h

    namespace detail {
        using namespace sgcl::detail;

        // The state of an image, a type of its own (a pool of its own):
        // the size, the format and the metadata, and the pixels in one
        // managed buffer of the exact size. The buffer has no pointer map,
        // so marking never walks it.
        struct ImageState {
            uint32_t width;
            uint32_t height;
            pixel_format format;
            uint8_t orientation = 1;
            size_t stride;
            dynamic_array<std::byte> pixels;
            vector<byte> exif;
            vector<byte> icc;

            ImageState(uint32_t w, uint32_t h, pixel_format f, size_t row, size_t bytes)
            : width(w), height(h), format(f), stride(row), pixels(bytes) {
            }
        };

        // The row of `width` pixels of format f, in bytes, and the whole
        // buffer; a broken contract thrown (a zero side, a format outside
        // the list, a buffer past what an address holds)
        inline size_t row_bytes(uint32_t width, pixel_format f) {
            if (!valid(f)) {
                throw invalid_argument("sgcl::codec::image: pixel format outside the list");
            }
            return size_t(width) * bytes_per_pixel(f);
        }

        inline size_t image_bytes(uint32_t width, uint32_t height, pixel_format f) {
            if (width == 0 || height == 0) {
                throw invalid_argument("sgcl::codec::image: a side of zero pixels");
            }
            size_t row = row_bytes(width, f);
            if (height > std::numeric_limits<ptrdiff_t>::max() / row) {
                throw length_error("sgcl::codec::image: larger than memory can address");
            }
            return row * height;
        }

        struct ImageAccess {
            static ImageState& state(const image& im) noexcept;
            static tracked_ptr<ImageState> word(const image& im) noexcept;

            // The orientation of EXIF (1 to 8); anything else is 1, the
            // image as it is stored
            static void set_orientation(const image& im, unsigned value) noexcept;
        };
    }

    // An image: its size, its pixel format and its pixels, plus the
    // metadata the file had (EXIF and the ICC profile, as bytes). A handle
    // of one word: copies share the pixels, clone() makes new ones. The
    // pixels lie row after row in one managed buffer, stride() bytes a
    // row; a slice of them (pixels(), row()) keeps the buffer alive.
    class image {
    public:
        // width × height pixels of the format, all zero (black, and
        // transparent where there is alpha). A side of zero or a format
        // outside the list is invalid_argument, a buffer past what an
        // address holds length_error: a contract, not a condition of the
        // data (a decoder checks the size of a file against its limits
        // before it makes the image).
        image(uint32_t width, uint32_t height, pixel_format f)
        : _s(make_tracked<detail::ImageState>(width, height, f, detail::row_bytes(width, f), detail::image_bytes(width, height, f))) {
        }

        uint32_t width() const noexcept {
            return _s->width;
        }

        uint32_t height() const noexcept {
            return _s->height;
        }

        pixel_format format() const noexcept {
            return _s->format;
        }

        // The bytes of a row: width() times the bytes of a pixel, with no
        // padding at its end
        size_t stride() const noexcept {
            return _s->stride;
        }

        // Every row, from the top
        slice<byte> pixels() {
            return _s->pixels.as_slice();
        }

        slice<const byte> pixels() const {
            return std::as_const(_s->pixels).as_slice();
        }

        // Row y from the top; y past the last row is out_of_range
        slice<byte> row(uint32_t y) {
            _check_row(y);
            return _s->pixels.as_slice(size_t(y) * _s->stride, _s->stride);
        }

        slice<const byte> row(uint32_t y) const {
            _check_row(y);
            return std::as_const(_s->pixels).as_slice(size_t(y) * _s->stride, _s->stride);
        }

        // A new image of the same pixels in format f (the metadata with
        // them). Between depths a channel scales to the nearest value (v *
        // 257 up, round(v / 257) down); color to gray is the luma of Rec.
        // 601; a format without alpha drops it, one with alpha where the
        // source had none gets it opaque; CMYK and RGB go through each
        // other with no profile, (1 - c)(1 - k). Always a copy, the format
        // the same or not.
        image convert(pixel_format f) const {
            if (!detail::valid(f)) {
                throw invalid_argument("sgcl::codec::image::convert: pixel format outside the list");
            }
            image out(_s->width, _s->height, f);
            const auto run = detail::converter(_s->format, f);
            const std::byte* src = _s->pixels.data();
            std::byte* dst = out._s->pixels.data();
            for (uint32_t y = 0; y < _s->height; ++y) {
                run(src + size_t(y) * _s->stride, dst + size_t(y) * out._s->stride, _s->width);
            }
            out._copy_metadata(*this);
            return out;
        }

        // A new image of the same format, pixels and metadata
        image clone() const {
            image out(_s->width, _s->height, _s->format);
            sgcl::detail::copy_bytes(out._s->pixels.data(), _s->pixels.data(), _s->pixels.size());
            out._copy_metadata(*this);
            return out;
        }

        // The EXIF block (a TIFF structure, from its byte-order mark) and
        // the ICC profile as the file had them; empty when it had none, or
        // when decode_options.metadata was false
        slice<const byte> exif() const noexcept {
            return std::as_const(_s->exif).as_slice();
        }

        slice<const byte> icc() const noexcept {
            return std::as_const(_s->icc).as_slice();
        }

        // The orientation of EXIF: 1 when the rows are stored as they are
        // shown (and when the file said nothing), 2 to 8 for a mirror
        // and/or a turn by a multiple of 90 degrees. The decoder does not
        // turn the image (neither do Go and libjpeg): oriented() does.
        uint8_t orientation() const noexcept {
            return _s->orientation;
        }

        // A new image as it is meant to be shown: turned and mirrored by
        // orientation(), sides swapped for 5 to 8, orientation() 1. The
        // EXIF bytes stay as they were (the tag in them still says what it
        // said of the stored image).
        image oriented() const {
            const unsigned o = _s->orientation;
            const bool swap = o >= 5;
            image out(swap ? _s->height : _s->width, swap ? _s->width : _s->height, _s->format);
            switch (detail::bytes_per_pixel(_s->format)) {
                case 1: _orient<1>(out); break;
                case 2: _orient<2>(out); break;
                case 3: _orient<3>(out); break;
                case 4: _orient<4>(out); break;
                case 6: _orient<6>(out); break;
                case 8: _orient<8>(out); break;
            }
            out._copy_metadata(*this);
            out._s->orientation = 1;
            return out;
        }

        // The image into the file at path, in the format its extension
        // names: .png, .jpg or .jpeg, .heic or .heif where the system writes
        // HEIC; errc::unsupported for any other (.gif, .webp and .avif are
        // read, not written). Written as path + ".part" and renamed over path
        // when whole. img.save("photo.jpg", {.quality = 90}). Defined with
        // codec::load and codec::save in files.h, which codec.h brings in
        expected<void, error> save(const string& path) const;
        expected<void, error> save(const string& path, const save_options& o) const;
        async::task<expected<void, error>> async_save(const string& path) const;
        async::task<expected<void, error>> async_save(const string& path, const save_options& o) const;

    private:
        friend struct detail::ImageAccess;
        friend struct sgcl::detail::HandleWord;

        image(sgcl::detail::FromWord, const tracked_ptr<detail::ImageState>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<detail::ImageState>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<detail::ImageState>& _handle_word() const noexcept {
            return _s;
        }

        void _check_row(uint32_t y) const {
            if (y >= _s->height) {
                throw out_of_range("sgcl::codec::image::row");
            }
        }

        void _copy_metadata(const image& from) {
            _s->orientation = from._s->orientation;
            _s->exif = from._s->exif;
            _s->icc = from._s->icc;
        }

        // Output pixel (x, y) is the source pixel at base + x * dx + y *
        // dy (bytes), for the eight orientations of EXIF: 1 as stored, 2
        // mirrored left to right, 3 turned by 180 degrees, 4 mirrored top
        // to bottom, 5 transposed (source (y, x)), 6 turned clockwise, 7
        // transposed across the other diagonal, 8 turned counterclockwise
        template<unsigned B>
        void _orient(image& out) const {
            const ptrdiff_t w = _s->width;
            const ptrdiff_t h = _s->height;
            const ptrdiff_t s = static_cast<ptrdiff_t>(_s->stride);
            const ptrdiff_t b = B;
            ptrdiff_t base = 0, dx = b, dy = s;
            switch (_s->orientation) {
                case 2: base = (w - 1) * b; dx = -b; dy = s; break;
                case 3: base = (w - 1) * b + (h - 1) * s; dx = -b; dy = -s; break;
                case 4: base = (h - 1) * s; dx = b; dy = -s; break;
                case 5: base = 0; dx = s; dy = b; break;
                case 6: base = (h - 1) * s; dx = -s; dy = b; break;
                case 7: base = (w - 1) * b + (h - 1) * s; dx = -s; dy = -b; break;
                case 8: base = (w - 1) * b; dx = s; dy = -b; break;
                default: break;
            }
            const std::byte* src = _s->pixels.data();
            std::byte* dst = out._s->pixels.data();
            const uint32_t ow = out._s->width;
            const uint32_t oh = out._s->height;
            for (uint32_t y = 0; y < oh; ++y) {
                ptrdiff_t at = base + ptrdiff_t(y) * dy;   // an offset, not a pointer: the last step lands outside
                std::byte* q = dst + size_t(y) * out._s->stride;
                for (uint32_t x = 0; x < ow; ++x) {
                    std::memcpy(q, src + at, B);
                    at += dx;
                    q += B;
                }
            }
        }

        tracked_ptr<detail::ImageState> _s;
    };

    namespace detail {
        inline ImageState& ImageAccess::state(const image& im) noexcept {
            return *im._s;
        }

        inline tracked_ptr<ImageState> ImageAccess::word(const image& im) noexcept {
            return im._s;
        }

        inline void ImageAccess::set_orientation(const image& im, unsigned value) noexcept {
            im._s->orientation = static_cast<uint8_t>(value >= 1 && value <= 8 ? value : 1);
        }
    }
}
