//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "options.h"
#include "detail/input.h"
#include "detail/metadata_reader.h"
#include "../core/aliases.h"
#include "../core/detail/handle_word.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../core/duration.h"
#include "../io/file.h"
#include "../io/stream.h"
#include "../time/date.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <chrono>
#include <cstdint>
#include <utility>

namespace sgcl::codec {
    // What a file says about its picture, typed: EXIF (IFD0, the Exif IFD
    // and the GPS IFD of CIPA DC-008) and XMP (the tiff, exif, exifEX,
    // aux, xmp, photoshop and dc namespaces), read from JPEG, PNG, WebP,
    // TIFF, HEIF and AVIF without decoding their pixels, or from an
    // image's EXIF block. Each field is EXIF's where EXIF has it and
    // XMP's otherwise; absent (nullopt) where neither has it, or has it
    // in a form that does not read.
    //
    //     codec::metadata m = codec::metadata::load("photo.jpg").value();
    //     if (auto taken = m.date_time_original()) { ... }
    //     if (auto place = m.location()) { place->latitude ... }
    //
    // A handle of one tracked word: a copy shares the fields, which never
    // change once read.
    class metadata {
    public:
        // Nothing known: every field absent
        metadata()
        : _s(make_tracked<detail::MetadataState>()) {
        }

        // The metadata of a file in memory, of any format the module
        // sniffs (none for GIF, BMP, ICO, QOI and the Netpbm formats);
        // errc::unsupported for bytes of no format the module reads,
        // errc::too_large for an EXIF block or an XMP packet past
        // l.max_metadata
        static expected<metadata, error> read(const slice<const byte>& file, const limits& l = {}) noexcept {
            metadata m;
            error err;
            if (!detail::meta_scan(reinterpret_cast<const uint8_t*>(file.data()), file.size(), l.max_metadata, *m._s, err)) {
                return unexpected(err);
            }
            return m;
        }

        // The file from a stream, read to its end first (a TIFF's or a
        // HEIF's metadata lies anywhere in it); errc::too_large for a
        // stream longer than eight bytes a pixel of l.max_pixels and 64 MB
        static expected<metadata, error> read(const io::reader& in, const limits& l = {}) {
            detail::ReaderInput source(in);
            vector<byte> all;
            const uint64_t most = l.max_pixels * 8 + (uint64_t(64) << 20);
            for (;;) {
                const uint8_t* p;
                size_t got;
                if (!source.peek(65536, p, got)) {
                    return unexpected(*source.failure);
                }
                if (got == 0) {
                    break;
                }
                if (all.size() + got > most) {
                    return unexpected(error(errc::too_large, all.size()));
                }
                all.insert(all.end(), reinterpret_cast<const byte*>(p), reinterpret_cast<const byte*>(p) + got);
                source.consume(got);
            }
            return read(all.as_slice(), l);
        }

        // The file at path: errc::io when it does not read
        static expected<metadata, error> load(const string& path, const limits& l = {}) {
            auto bytes = io::read_file(path);
            if (!bytes) {
                return unexpected(error(bytes.error(), 0));
            }
            return read(bytes->as_slice(), l);
        }

        // From an EXIF block as image::exif() holds it (the TIFF
        // structure, from its byte-order mark) and an XMP packet
        static metadata from_exif(const slice<const byte>& exif, const string& xmp = {}) noexcept {
            metadata m;
            detail::MetaFound f;
            f.exif = exif.empty() ? nullptr : reinterpret_cast<const uint8_t*>(exif.data());
            f.exif_size = exif.size();
            f.xmp = xmp.empty() ? nullptr : reinterpret_cast<const uint8_t*>(xmp.data());
            f.xmp_size = xmp.size();
            error ignored;
            detail::meta_fill(f, SIZE_MAX, *m._s, ignored);
            return m;
        }

        // The camera and the picture
        SGCL_INLINE_HOT optional<string> make() const noexcept {
            return _s->make;
        }

        SGCL_INLINE_HOT optional<string> model() const noexcept {
            return _s->model;
        }

        SGCL_INLINE_HOT optional<string> lens_make() const noexcept {
            return _s->lens_make;
        }

        SGCL_INLINE_HOT optional<string> lens_model() const noexcept {
            return _s->lens_model;
        }

        SGCL_INLINE_HOT optional<string> software() const noexcept {
            return _s->software;
        }

        SGCL_INLINE_HOT optional<string> artist() const noexcept {
            return _s->artist;
        }

        SGCL_INLINE_HOT optional<string> copyright() const noexcept {
            return _s->copyright;
        }

        SGCL_INLINE_HOT optional<string> description() const noexcept {
            return _s->description;
        }

        // When: in a fixed zone of the file's offset, or, for a time the
        // file gives without one, a time of z (UTC unless another is)
        optional<time::datetime> date_time_original(const time::zone& z = time::zone::utc()) const {
            return _at(_s->original, z);
        }

        optional<time::datetime> date_time_digitized(const time::zone& z = time::zone::utc()) const {
            return _at(_s->digitized, z);
        }

        optional<time::datetime> date_time(const time::zone& z = time::zone::utc()) const {
            return _at(_s->modified, z);
        }

        // Exposure
        SGCL_INLINE_HOT optional<double> exposure_time() const noexcept {
            return _s->exposure_time;
        }

        SGCL_INLINE_HOT optional<double> f_number() const noexcept {
            return _s->f_number;
        }

        SGCL_INLINE_HOT optional<uint32_t> iso() const noexcept {
            return _s->iso;
        }

        SGCL_INLINE_HOT optional<double> exposure_bias() const noexcept {
            return _s->exposure_bias;
        }

        SGCL_INLINE_HOT optional<double> focal_length() const noexcept {
            return _s->focal_length;
        }

        SGCL_INLINE_HOT optional<uint32_t> focal_length_35mm() const noexcept {
            return _s->focal_length_35mm;
        }

        SGCL_INLINE_HOT optional<bool> flash_fired() const noexcept {
            return _s->flash_fired;
        }

        // Where, and the time of the GPS fix (UTC)
        SGCL_INLINE_HOT optional<codec::location> location() const noexcept {
            return _s->place;
        }

        optional<time::datetime> gps_time() const {
            return _at(_s->gps_time, time::zone::utc());
        }

        // The image as the metadata describes it
        SGCL_INLINE_HOT unsigned orientation() const noexcept {
            return _s->orientation ? _s->orientation : 1;
        }

        SGCL_INLINE_HOT optional<uint32_t> width() const noexcept {
            return _s->width;
        }

        SGCL_INLINE_HOT optional<uint32_t> height() const noexcept {
            return _s->height;
        }

        SGCL_INLINE_HOT optional<int> rating() const noexcept {
            return _s->rating;
        }

        // The blocks as the file holds them
        SGCL_INLINE_HOT slice<const byte> exif() const noexcept {
            return std::as_const(_s->exif).as_slice();
        }

        SGCL_INLINE_HOT string xmp() const noexcept {
            return _s->xmp;
        }

    private:
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT metadata(sgcl::detail::FromWord, const tracked_ptr<detail::MetadataState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::MetadataState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::MetadataState>& _handle_word() const noexcept {
            return _s;
        }

        static optional<time::datetime> _at(const optional<detail::MetaStamp>& t, const time::zone& z) {
            if (!t) {
                return nullopt;
            }
            const time::zone zone = t->has_offset ? time::zone::fixed(duration(std::chrono::seconds(t->offset))) : z;
            const int second = t->second > 59 ? 59 : t->second;   // a leap second as the second before
            return time::date(t->year, t->month, t->day).at(t->hour, t->minute, second, zone) + duration(std::chrono::nanoseconds(t->nanoseconds));
        }

        tracked_ptr<detail::MetadataState> _s;
    };
}
