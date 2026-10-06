//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/exif.h"
#include "detail/pixels.h"
#include "detail/orient.h"
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

    // Which way image::flipped mirrors
    enum class flip : uint8_t {
        horizontal,   // left to right: each row's pixels reversed
        vertical      // top to bottom: the rows reversed
    };

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

            // bytes as image_bytes gives them: within what an address holds
            SGCL_INLINE_HOT ImageState(uint32_t w, uint32_t h, pixel_format f, size_t row, size_t bytes) noexcept
            : width(w), height(h), format(f), stride(row), pixels(bytes) {
            }
        };

        // The row of `width` pixels of format f, in bytes, and the whole
        // buffer; a broken contract thrown (a zero side, a format outside
        // the list, a buffer past what an address holds)
        SGCL_INLINE_HOT size_t row_bytes(uint32_t width, pixel_format f) {
            if (!valid(f)) {
                throw invalid_argument("sgcl::codec::image: pixel format outside the list");
            }
            return size_t(width) * bytes_per_pixel(f);
        }

        SGCL_INLINE_HOT size_t image_bytes(uint32_t width, uint32_t height, pixel_format f) {
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

            // The orientation a decoder read (EXIF's tag, ImageIO's number,
            // as wide as either): 1 to 8 as it is, anything else 1, the
            // image as it is stored
            static void set_orientation(const image& im, long long value) noexcept;
        };
    }

    // An image: its size, its pixel format and its pixels, plus the
    // metadata the file had (EXIF and the ICC profile, as bytes). A handle
    // of one word: copies share the pixels, clone() makes new ones; a move
    // copies the word, as a tracked_ptr's does, so a moved-from image is
    // still the image. The pixels lie row after row in one managed buffer,
    // stride() bytes a row; a slice of them (pixels(), row()) keeps the
    // buffer alive.
    class image {
    public:
        // width × height pixels of the format, all zero (black, and
        // transparent where there is alpha; white for cmyk8, no ink). A
        // side of zero or a format outside the list is invalid_argument, a
        // buffer past what an address holds length_error: a contract, not
        // a condition of the data (a decoder checks the size of a file
        // against its limits before it makes the image).
        SGCL_INLINE_HOT image(uint32_t width, uint32_t height, pixel_format f)
        : _s(make_tracked<detail::ImageState>(width, height, f, detail::row_bytes(width, f), detail::image_bytes(width, height, f))) {
        }

        SGCL_INLINE_HOT uint32_t width() const noexcept {
            return _s->width;
        }

        SGCL_INLINE_HOT uint32_t height() const noexcept {
            return _s->height;
        }

        SGCL_INLINE_HOT pixel_format format() const noexcept {
            return _s->format;
        }

        // The bytes of a row: width() times the bytes of a pixel, with no
        // padding at its end
        SGCL_INLINE_HOT size_t stride() const noexcept {
            return _s->stride;
        }

        // Every row, from the top
        SGCL_INLINE_HOT slice<byte> pixels() noexcept {
            return _s->pixels.as_slice();
        }

        SGCL_INLINE_HOT slice<const byte> pixels() const noexcept {
            return std::as_const(_s->pixels).as_slice();
        }

        // Row y from the top; y past the last row is out_of_range
        SGCL_INLINE_HOT slice<byte> row(uint32_t y) {
            _check_row(y);
            return _s->pixels.as_slice(size_t(y) * _s->stride, _s->stride);
        }

        SGCL_INLINE_HOT slice<const byte> row(uint32_t y) const {
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
        SGCL_INLINE_HOT image clone() const noexcept {
            image out(_s->width, _s->height, _s->format);
            sgcl::detail::copy_bytes(out._s->pixels.data(), _s->pixels.data(), _s->pixels.size());
            out._copy_metadata(*this);
            return out;
        }

        // The EXIF block (a TIFF structure, from its byte-order mark) and
        // the ICC profile as the file had them; empty when it had none, or
        // when decode_options.metadata was false
        SGCL_INLINE_HOT slice<const byte> exif() const noexcept {
            return std::as_const(_s->exif).as_slice();
        }

        SGCL_INLINE_HOT slice<const byte> icc() const noexcept {
            return std::as_const(_s->icc).as_slice();
        }

        // The orientation of EXIF: 1 when the rows are stored as they are
        // shown (and when the file said nothing), 2 to 8 for a mirror
        // and/or a turn by a multiple of 90 degrees. The decoder does not
        // turn the image (neither do Go and libjpeg): oriented() does.
        SGCL_INLINE_HOT uint8_t orientation() const noexcept {
            return _s->orientation;
        }

        // The orientation set, 1 to 8 (one outside is invalid_argument, a
        // contract), and the tag of exif() set to it when the block has
        // one, so that a file written from the image says the same. Like
        // the pixels, the metadata is the image's: every copy sees it.
        SGCL_INLINE_HOT void set_orientation(unsigned value) {
            if (value < 1 || value > 8) {
                throw invalid_argument("sgcl::codec::image::set_orientation: an orientation outside 1..8");
            }
            _set_orientation(value);
        }

        // The EXIF block set to a copy of the bytes (empty: none), as the
        // encoders write it; orientation() becomes the block's when it has
        // the tag, and stays as it was when not
        SGCL_INLINE_HOT void set_exif(const slice<const byte>& bytes) noexcept {
            _s->exif = _copy_of(bytes);
            const auto* p = reinterpret_cast<const uint8_t*>(_s->exif.data());
            bool little = false;
            if (detail::exif_orientation_at(p, _s->exif.size(), little) != _s->exif.size()) {
                _s->orientation = static_cast<uint8_t>(detail::exif_orientation(p, _s->exif.size()));
            }
        }

        // The ICC profile set to a copy of the bytes (empty: none)
        SGCL_INLINE_HOT void set_icc(const slice<const byte>& bytes) noexcept {
            _s->icc = _copy_of(bytes);
        }

        // A new image as it is meant to be shown: turned and mirrored by
        // orientation(), sides swapped for 5 to 8, orientation() 1, and the
        // tag of its EXIF block 1 where the block has one (the rest of the
        // block as it was), so that a file written from it is not turned
        // again by a viewer.
        image oriented() const noexcept {
            image out = _transformed(_s->orientation);
            out._set_orientation(1);
            return out;
        }

        // A new image of the rectangle at (x, y), width × height pixels: its
        // pixels copied, the metadata with them. A side of zero or a
        // rectangle reaching past the image is out_of_range (a contract,
        // as row()'s)
        image cropped(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const {
            if (width == 0 || height == 0 || x >= _s->width || y >= _s->height || width > _s->width - x || height > _s->height - y) {
                throw out_of_range("sgcl::codec::image::cropped: a rectangle outside the image");
            }
            image out(width, height, _s->format);
            const size_t b = detail::bytes_per_pixel(_s->format);
            const std::byte* src = _s->pixels.data() + size_t(y) * _s->stride + size_t(x) * b;
            std::byte* dst = out._s->pixels.data();
            for (uint32_t r = 0; r < height; ++r) {
                sgcl::detail::copy_bytes(dst + size_t(r) * out._s->stride, src + size_t(r) * _s->stride, out._s->stride);
            }
            out._copy_metadata(*this);
            return out;
        }

        // A new image mirrored, left to right unless told otherwise; the
        // metadata with it, orientation() as it was
        image flipped(flip direction = flip::horizontal) const noexcept {
            image out = _transformed(direction == flip::vertical ? 4u : 2u);
            return out;
        }

        // A new image turned clockwise by `degrees`, a multiple of 90
        // (negative turns counterclockwise, 0 and 360 give a copy); the
        // sides swapped for a quarter turn, the metadata with it,
        // orientation() as it was. Any other angle is invalid_argument (a
        // contract)
        image rotated(int degrees) const {
            if (degrees % 90 != 0) {
                throw invalid_argument("sgcl::codec::image::rotated: an angle that is not a multiple of 90 degrees");
            }
            const int quarter = ((degrees / 90) % 4 + 4) % 4;
            static constexpr unsigned transform[4] = {1, 6, 3, 8};   // EXIF's: none, clockwise, 180, counterclockwise
            return _transformed(transform[quarter]);
        }

        // The image into the file at path, in the format its extension
        // names: .png, .jpg or .jpeg, .gif, .webp, .bmp, .tif or .tiff, .ico,
        // .cur, .qoi, .pbm/.pgm/.ppm/.pam/.pnm, .heic or .heif where the
        // system writes HEIC; errc::unsupported for any other (.avif is
        // read, not written). Written as path + ".part" and renamed over path
        // when whole. img.save("photo.jpg", {.quality = 90}). Defined with
        // codec::load and codec::save in files.h, which codec.h brings in
        expected<void, error> save(const string& path) const;
        expected<void, error> save(const string& path, const save_options& o) const;
        async::task<expected<void, error>> async_save(const string& path) const noexcept;
        async::task<expected<void, error>> async_save(const string& path, const save_options& o) const noexcept;

    private:
        friend struct detail::ImageAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT image(sgcl::detail::FromWord, const tracked_ptr<detail::ImageState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ImageState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ImageState>& _handle_word() const noexcept {
            return _s;
        }

        SGCL_INLINE_HOT void _check_row(uint32_t y) const {
            if (y >= _s->height) {
                throw out_of_range("sgcl::codec::image::row");
            }
        }

        // A new block of the bytes (which may be the image's own: a slice
        // keeps them alive while they are copied)
        SGCL_INLINE_HOT static vector<byte> _copy_of(const slice<const byte>& bytes) noexcept {
            vector<byte> out(bytes.size());
            sgcl::detail::copy_bytes(out.data(), bytes.data(), bytes.size());
            return out;
        }

        // orientation() and the tag of the EXIF block, a value of 1 to 8
        SGCL_INLINE_HOT void _set_orientation(unsigned value) noexcept {
            _s->orientation = static_cast<uint8_t>(value);
            detail::set_exif_orientation(reinterpret_cast<uint8_t*>(_s->exif.data()), _s->exif.size(), value);
        }

        SGCL_INLINE_HOT void _copy_metadata(const image& from) noexcept {
            _s->orientation = from._s->orientation;
            _s->exif = from._s->exif;
            _s->icc = from._s->icc;
        }

        // A new image of the pixels under one of the eight transforms of
        // EXIF's orientation (detail/orient.h), the metadata copied
        image _transformed(unsigned o) const noexcept {
            const bool swap = o >= 5 && o <= 8;
            image out(swap ? _s->height : _s->width, swap ? _s->width : _s->height, _s->format);
            detail::orient::apply(reinterpret_cast<const uint8_t*>(_s->pixels.data()), _s->width, _s->height,
                                     detail::bytes_per_pixel(_s->format), o, reinterpret_cast<uint8_t*>(out._s->pixels.data()));
            out._copy_metadata(*this);
            return out;
        }

        tracked_ptr<detail::ImageState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT ImageState& ImageAccess::state(const image& im) noexcept {
            return *im._s;
        }

        SGCL_INLINE_HOT tracked_ptr<ImageState> ImageAccess::word(const image& im) noexcept {
            return im._s;
        }

        SGCL_INLINE_HOT void ImageAccess::set_orientation(const image& im, long long value) noexcept {
            im._s->orientation = static_cast<uint8_t>(value >= 1 && value <= 8 ? value : 1);
        }
    }
}
