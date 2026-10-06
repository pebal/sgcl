//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../format.h"
#include "../options.h"
#include "../../compress/brotli.h"
#include "../../compress/zlib.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../encoding/xml.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

// The reading of a file's metadata (metadata.h): EXIF (CIPA DC-008, the
// TIFF structure of IFD0, the Exif IFD and the GPS IFD) and XMP (ISO
// 16684-1, RDF/XML in the namespaces of Adobe's XMP Specification Part 2:
// tiff, exif, exifEX, aux, xmp, photoshop, dc), found in the containers
// of JPEG, PNG, WebP, TIFF and HEIF without decoding their pixels.
//
// Lenient inside the blocks, as exiftool and ImageIO are: a field of a
// wrong type, a value past the block or an impossible date is absent, and
// the rest of the block is read. Lenient in the containers too: a file
// that breaks after its metadata gives what came before the break.
namespace sgcl::codec {
    // Where a photo was taken (metadata::location)
    struct location {
        double latitude = 0;          // degrees, north positive
        double longitude = 0;         // degrees, east positive
        optional<double> altitude;    // meters above sea level, negative below
    };

    namespace detail {
        // A date and a time as a file writes it, before a zone is chosen
        struct MetaStamp {
            int year = 0;
            int month = 0;
            int day = 0;
            int hour = 0;
            int minute = 0;
            int second = 0;
            uint32_t nanoseconds = 0;
            bool has_offset = false;
            int32_t offset = 0;   // seconds east of UTC
        };

        // What metadata holds: the raw blocks and every field read, the
        // EXIF's first and the XMP's where EXIF has none
        struct MetadataState {
            vector<byte> exif;
            string xmp;
            optional<string> make, model, lens_make, lens_model, software, artist, copyright, description;
            optional<MetaStamp> original, digitized, modified, gps_time;
            optional<double> exposure_time, f_number, exposure_bias, focal_length;
            optional<uint32_t> iso, focal_length_35mm, width, height;
            optional<bool> flash_fired;
            optional<codec::location> place;
            optional<int> rating;
            uint8_t orientation = 0;   // 0: absent
        };

        // ---------------------------------------------------------------
        // Small readers of text

        SGCL_INLINE_HOT bool meta_digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        // n digits at p as a number; false when one is not a digit
        inline bool meta_number(std::string_view s, size_t at, size_t n, int& out) noexcept {
            if (at + n > s.size()) {
                return false;
            }
            int v = 0;
            for (size_t i = 0; i < n; ++i) {
                if (!meta_digit(s[at + i])) {
                    return false;
                }
                v = v * 10 + (s[at + i] - '0');
            }
            out = v;
            return true;
        }

        // A stamp whose fields make a date and a time of the calendar
        inline bool meta_plausible(const MetaStamp& t) noexcept {
            if (t.year < 1 || t.month < 1 || t.month > 12 || t.day < 1 || t.hour > 23 || t.minute > 59 || t.second > 60) {
                return false;
            }
            static constexpr int days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (t.day > days[t.month - 1]) {
                return false;
            }
            if (t.month == 2 && t.day == 29) {
                const bool leap = (t.year % 4 == 0 && t.year % 100 != 0) || t.year % 400 == 0;
                return leap;
            }
            return true;
        }

        // An offset "+hh:mm", "-hh:mm" (EXIF's OffsetTime*, XMP's TZD
        // also "Z", "+hhmm" and "+hh"); false for anything else
        inline bool meta_offset(std::string_view s, int32_t& seconds) noexcept {
            while (!s.empty() && (s.back() == '\0' || s.back() == ' ')) {
                s.remove_suffix(1);
            }
            if (s == "Z") {
                seconds = 0;
                return true;
            }
            if (s.size() < 3 || (s[0] != '+' && s[0] != '-')) {
                return false;
            }
            int h = 0, m = 0;
            if (!meta_number(s, 1, 2, h)) {
                return false;
            }
            if (s.size() == 6 && s[3] == ':') {
                if (!meta_number(s, 4, 2, m)) {
                    return false;
                }
            } else if (s.size() == 5) {
                if (!meta_number(s, 3, 2, m)) {
                    return false;
                }
            } else if (s.size() != 3) {
                return false;
            }
            if (h > 23 || m > 59) {
                return false;
            }
            seconds = (s[0] == '-' ? -1 : 1) * (h * 3600 + m * 60);
            return true;
        }

        // Digits of a fraction of a second, "123" as 123 000 000 ns; the
        // digits past nine dropped, anything else ending them
        inline uint32_t meta_fraction(std::string_view s) noexcept {
            uint32_t v = 0;
            size_t i = 0;
            for (; i < s.size() && i < 9 && meta_digit(s[i]); ++i) {
                v = v * 10 + uint32_t(s[i] - '0');
            }
            for (size_t k = i; k < 9; ++k) {
                v *= 10;
            }
            return i == 0 ? 0 : v;
        }

        // EXIF's "YYYY:MM:DD HH:MM:SS" (dashes taken for the colons of the
        // date, as some writers put them); blanks and zeros are absent
        inline optional<MetaStamp> meta_exif_stamp(std::string_view s) noexcept {
            MetaStamp t;
            if (s.size() < 19 || (s[4] != ':' && s[4] != '-') || s[7] != s[4] || (s[10] != ' ' && s[10] != 'T') || s[13] != ':' || s[16] != ':') {
                return nullopt;
            }
            if (!meta_number(s, 0, 4, t.year) || !meta_number(s, 5, 2, t.month) || !meta_number(s, 8, 2, t.day) ||
                !meta_number(s, 11, 2, t.hour) || !meta_number(s, 14, 2, t.minute) || !meta_number(s, 17, 2, t.second)) {
                return nullopt;
            }
            if (!meta_plausible(t)) {
                return nullopt;
            }
            return t;
        }

        // XMP's dates (ISO 8601 as XMP's Part 1 takes it): YYYY-MM-DD,
        // then THH:MM, :SS, .s and a TZD; a year or a month alone is too
        // little for a datetime and is absent
        inline optional<MetaStamp> meta_xmp_stamp(std::string_view s) noexcept {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\t' || s.front() == '\r')) {
                s.remove_prefix(1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t' || s.back() == '\r')) {
                s.remove_suffix(1);
            }
            MetaStamp t;
            if (s.size() < 10 || s[4] != '-' || s[7] != '-' || !meta_number(s, 0, 4, t.year) || !meta_number(s, 5, 2, t.month) ||
                !meta_number(s, 8, 2, t.day)) {
                return nullopt;
            }
            size_t i = 10;
            if (i < s.size()) {
                if (s[i] != 'T' || !meta_number(s, i + 1, 2, t.hour) || i + 3 >= s.size() || s[i + 3] != ':' || !meta_number(s, i + 4, 2, t.minute)) {
                    return nullopt;
                }
                i += 6;
                if (i < s.size() && s[i] == ':') {
                    if (!meta_number(s, i + 1, 2, t.second)) {
                        return nullopt;
                    }
                    i += 3;
                    if (i < s.size() && (s[i] == '.' || s[i] == ',')) {
                        size_t j = i + 1;
                        while (j < s.size() && meta_digit(s[j])) {
                            ++j;
                        }
                        t.nanoseconds = meta_fraction(s.substr(i + 1, j - i - 1));
                        i = j;
                    }
                }
                if (i < s.size()) {
                    if (!meta_offset(s.substr(i), t.offset)) {
                        return nullopt;
                    }
                    t.has_offset = true;
                }
            }
            if (!meta_plausible(t)) {
                return nullopt;
            }
            return t;
        }

        // A string as a file writes it: trailing NULs and spaces trimmed
        // (Make is often padded), bytes that are not UTF-8 taken as
        // Latin-1; empty is absent
        inline optional<string> meta_text(const uint8_t* p, size_t n) noexcept {
            size_t end = 0;
            // up to the first NUL: ASCII's count includes its NUL, and
            // some writers leave garbage after it
            while (end < n && p[end] != 0) {
                ++end;
            }
            while (end > 0 && (p[end - 1] == ' ' || p[end - 1] == '\t' || p[end - 1] == '\n' || p[end - 1] == '\r')) {
                --end;
            }
            size_t begin = 0;
            while (begin < end && p[begin] == ' ') {
                ++begin;
            }
            if (begin == end) {
                return nullopt;
            }
            string s(reinterpret_cast<const char*>(p + begin), end - begin);
            if (s.is_valid_utf8()) {
                return s;
            }
            std::string latin;
            for (size_t i = begin; i < end; ++i) {
                const uint8_t c = p[i];
                if (c < 0x80) {
                    latin += char(c);
                } else {
                    latin += char(0xC0 | c >> 6);
                    latin += char(0x80 | (c & 0x3F));
                }
            }
            return string(latin.data(), latin.size());
        }

        // ---------------------------------------------------------------
        // EXIF: the TIFF structure

        class ExifReader {
        public:
            ExifReader(const uint8_t* p, size_t n, MetadataState& s) noexcept : _p(p), _n(n), _s(s) {
            }

            bool xmp_past_limit = false;   // tag 700 past limits.max_metadata

            // The block from its byte-order mark; with_xmp: tag 700 of
            // IFD0 read as an XMP packet too (a TIFF file's)
            void run(bool with_xmp, string* xmp, size_t max_xmp) noexcept {
                if (_n < 8) {
                    return;
                }
                if (_p[0] == 'I' && _p[1] == 'I') {
                    _little = true;
                } else if (_p[0] == 'M' && _p[1] == 'M') {
                    _little = false;
                } else {
                    return;
                }
                if (_u16(2) != 42) {
                    return;
                }
                const uint64_t ifd0 = _u32(4);
                uint64_t exif_ifd = 0, gps_ifd = 0;
                _walk(ifd0, [&](unsigned tag, const Entry& e) {
                    switch (tag) {
                        case 0x010E: _text(e, _s.description); break;
                        case 0x010F: _text(e, _s.make); break;
                        case 0x0110: _text(e, _s.model); break;
                        case 0x0131: _text(e, _s.software); break;
                        case 0x013B: _text(e, _s.artist); break;
                        case 0x8298: _text(e, _s.copyright); break;
                        case 0x0132: _stamp(e, _s.modified); break;
                        case 0x0112: {
                            double v;
                            if (_number(e, 0, v) && v >= 1 && v <= 8) {
                                _s.orientation = uint8_t(v);
                            }
                            break;
                        }
                        case 0x0100: _whole(e, _image_width); break;
                        case 0x0101: _whole(e, _image_height); break;
                        case 0x4746: {
                            double v;
                            if (_number(e, 0, v) && v >= -1 && v <= 5) {
                                _s.rating = int(v);
                            }
                            break;
                        }
                        case 0x8769: exif_ifd = _pointer(e); break;
                        case 0x8825: gps_ifd = _pointer(e); break;
                        case 700:
                            if (with_xmp && xmp && (e.type == 1 || e.type == 7) && e.bytes > max_xmp) {
                                xmp_past_limit = true;
                            } else if (with_xmp && xmp && (e.type == 1 || e.type == 7)) {
                                if (const uint8_t* v = _value(e)) {
                                    *xmp = string(reinterpret_cast<const char*>(v), size_t(e.bytes));
                                }
                            }
                            break;
                        default: break;
                    }
                });
                optional<string> off_original, off_digitized, off_modified, sub_original, sub_digitized, sub_modified;
                optional<uint32_t> iso_speed;
                optional<double> shutter, aperture;
                if (exif_ifd && exif_ifd != ifd0) {
                    _walk(exif_ifd, [&](unsigned tag, const Entry& e) {
                        switch (tag) {
                            case 0x829A: _real(e, _s.exposure_time, true); break;
                            case 0x829D: _real(e, _s.f_number, true); break;
                            case 0x8827: {
                                double v;
                                if (_number(e, 0, v) && v >= 1 && v <= 4294967295.0) {
                                    _s.iso = uint32_t(v);
                                }
                                break;
                            }
                            case 0x8833: _whole(e, iso_speed); break;
                            case 0x9003: _stamp(e, _s.original); break;
                            case 0x9004: _stamp(e, _s.digitized); break;
                            case 0x9010: _text(e, off_modified); break;
                            case 0x9011: _text(e, off_original); break;
                            case 0x9012: _text(e, off_digitized); break;
                            case 0x9290: _text(e, sub_modified); break;
                            case 0x9291: _text(e, sub_original); break;
                            case 0x9292: _text(e, sub_digitized); break;
                            case 0x9201: _real(e, shutter, false); break;
                            case 0x9202: _real(e, aperture, false); break;
                            case 0x9204: _real(e, _s.exposure_bias, false); break;
                            case 0x9209: {
                                double v;
                                if (_number(e, 0, v) && v >= 0 && v <= 65535) {
                                    _s.flash_fired = (uint32_t(v) & 1) != 0;
                                }
                                break;
                            }
                            case 0x920A: _real(e, _s.focal_length, true); break;
                            case 0xA405: {
                                double v;
                                if (_number(e, 0, v) && v >= 1 && v <= 4294967295.0) {
                                    _s.focal_length_35mm = uint32_t(v);
                                }
                                break;
                            }
                            case 0xA002: _whole(e, _s.width); break;
                            case 0xA003: _whole(e, _s.height); break;
                            case 0xA433: _text(e, _s.lens_make); break;
                            case 0xA434: _text(e, _s.lens_model); break;
                            default: break;
                        }
                    });
                }
                // ISO 65535 means "65535 or more": the exact value is
                // ISOSpeed's (CIPA DC-008 4.6.5)
                if (iso_speed && *iso_speed > 0 && (!_s.iso || *_s.iso == 65535)) {
                    _s.iso = *iso_speed;
                }
                // APEX when the plain values are missing: Tv = -log2 t,
                // Av = 2 log2 N
                if (!_s.exposure_time && shutter && std::isfinite(*shutter) && std::fabs(*shutter) < 64) {
                    _s.exposure_time = std::exp2(-*shutter);
                }
                if (!_s.f_number && aperture && std::isfinite(*aperture) && *aperture >= 0 && *aperture < 64) {
                    _s.f_number = std::exp2(*aperture / 2);
                }
                _complete(_s.original, off_original, sub_original);
                _complete(_s.digitized, off_digitized, sub_digitized);
                _complete(_s.modified, off_modified, sub_modified);
                if (!_s.width) {
                    _s.width = _image_width;
                }
                if (!_s.height) {
                    _s.height = _image_height;
                }
                if (gps_ifd && gps_ifd != ifd0 && gps_ifd != exif_ifd) {
                    _gps(gps_ifd);
                }
            }

        private:
            struct Entry {
                unsigned type;
                uint32_t count;
                uint64_t bytes;     // count × the type's size
                uint64_t at;        // where the value lies in the block
            };

            SGCL_INLINE_HOT uint32_t _u16(uint64_t at) const noexcept {
                return _little ? uint32_t(_p[at]) | uint32_t(_p[at + 1]) << 8 : uint32_t(_p[at]) << 8 | uint32_t(_p[at + 1]);
            }

            SGCL_INLINE_HOT uint32_t _u32(uint64_t at) const noexcept {
                return _little ? _u16(at) | _u16(at + 2) << 16 : _u16(at) << 16 | _u16(at + 2);
            }

            static unsigned _size(unsigned type) noexcept {
                switch (type) {
                    case 1: case 2: case 6: case 7: case 129: return 1;
                    case 3: case 8: return 2;
                    case 4: case 9: case 11: case 13: return 4;
                    case 5: case 10: case 12: return 8;
                    default: return 0;
                }
            }

            // Each entry of the IFD at `at` whose value lies in the block
            template<class F>
            void _walk(uint64_t at, F&& f) noexcept {
                if (at < 8 || at + 2 > _n) {
                    return;
                }
                const uint32_t count = _u16(at);
                for (uint32_t i = 0; i < count; ++i) {
                    const uint64_t e = at + 2 + 12 * uint64_t(i);
                    if (e + 12 > _n) {
                        return;
                    }
                    Entry en;
                    en.type = _u16(e + 2);
                    en.count = _u32(e + 4);
                    const unsigned size = _size(en.type);
                    if (size == 0) {
                        continue;
                    }
                    en.bytes = uint64_t(en.count) * size;
                    en.at = en.bytes <= 4 ? e + 8 : _u32(e + 8);
                    if (en.at > _n || en.bytes > _n - en.at) {
                        continue;
                    }
                    f(_u16(e), en);
                }
            }

            // An IFD's offset: a LONG, an IFD (13) or a SHORT, as written
            uint64_t _pointer(const Entry& e) const noexcept {
                if (e.count < 1 || (e.type != 4 && e.type != 13 && e.type != 3)) {
                    return 0;
                }
                return e.type == 3 ? _u16(e.at) : _u32(e.at);
            }

            const uint8_t* _value(const Entry& e) const noexcept {
                return _p + e.at;
            }

            void _text(const Entry& e, optional<string>& out) noexcept {
                if (out || (e.type != 2 && e.type != 129 && e.type != 7 && e.type != 1)) {
                    return;
                }
                out = meta_text(_value(e), size_t(e.bytes));
            }

            void _stamp(const Entry& e, optional<MetaStamp>& out) noexcept {
                if (out || e.type != 2) {
                    return;
                }
                const auto* v = reinterpret_cast<const char*>(_value(e));
                out = meta_exif_stamp(std::string_view(v, size_t(e.bytes)));
            }

            // The i-th value as a number, of any numeric type; false for a
            // rational of denominator 0, or a type that is not one
            bool _number(const Entry& e, uint32_t i, double& v) const noexcept {
                if (i >= e.count) {
                    return false;
                }
                const uint64_t at = e.at + uint64_t(i) * _size(e.type);
                switch (e.type) {
                    case 1: case 7: v = _p[at]; return true;
                    case 6: v = int8_t(_p[at]); return true;
                    case 3: v = _u16(at); return true;
                    case 8: v = int16_t(_u16(at)); return true;
                    case 4: case 13: v = _u32(at); return true;
                    case 9: v = int32_t(_u32(at)); return true;
                    case 5: {
                        const uint32_t d = _u32(at + 4);
                        if (d == 0) {
                            return false;
                        }
                        v = double(_u32(at)) / d;
                        return true;
                    }
                    case 10: {
                        const int32_t d = int32_t(_u32(at + 4));
                        if (d == 0) {
                            return false;
                        }
                        v = double(int32_t(_u32(at))) / d;
                        return true;
                    }
                    case 11: {
                        v = std::bit_cast<float>(_u32(at));
                        return std::isfinite(v);
                    }
                    case 12: {
                        const uint64_t b = _little ? uint64_t(_u32(at)) | uint64_t(_u32(at + 4)) << 32 : uint64_t(_u32(at)) << 32 | _u32(at + 4);
                        v = std::bit_cast<double>(b);
                        return std::isfinite(v);
                    }
                    default: return false;
                }
            }

            void _real(const Entry& e, optional<double>& out, bool positive) const noexcept {
                double v;
                if (!out && _number(e, 0, v) && (!positive || v > 0)) {
                    out = v;
                }
            }

            void _whole(const Entry& e, optional<uint32_t>& out) const noexcept {
                double v;
                if (!out && _number(e, 0, v) && v >= 1 && v <= 4294967295.0) {
                    out = uint32_t(v);
                }
            }

            static void _complete(optional<MetaStamp>& t, const optional<string>& offset, const optional<string>& sub) noexcept {
                if (!t) {
                    return;
                }
                if (offset) {
                    int32_t s;
                    if (meta_offset(std::string_view(offset->data(), offset->size()), s)) {
                        t->has_offset = true;
                        t->offset = s;
                    }
                }
                if (sub) {
                    t->nanoseconds = meta_fraction(std::string_view(sub->data(), sub->size()));
                }
            }

            // Degrees of three rationals: degrees, minutes, seconds
            bool _degrees(const Entry& e, double& out) const noexcept {
                double d, m = 0, s = 0;
                if (e.count < 1 || !_number(e, 0, d)) {
                    return false;
                }
                if (e.count >= 2 && !_number(e, 1, m)) {
                    return false;
                }
                if (e.count >= 3 && !_number(e, 2, s)) {
                    return false;
                }
                out = d + m / 60 + s / 3600;
                return std::isfinite(out);
            }

            void _gps(uint64_t at) noexcept {
                char lat_ref = 0, lon_ref = 0;
                optional<double> lat, lon, alt;
                bool below = false;
                optional<string> date;
                double hms[3] = {0, 0, 0};
                bool have_time = false;
                _walk(at, [&](unsigned tag, const Entry& e) {
                    double v;
                    switch (tag) {
                        case 1: if (e.type == 2 && e.count >= 1) { lat_ref = char(_p[e.at]); } break;
                        case 3: if (e.type == 2 && e.count >= 1) { lon_ref = char(_p[e.at]); } break;
                        case 2: if (_degrees(e, v)) { lat = v; } break;
                        case 4: if (_degrees(e, v)) { lon = v; } break;
                        case 5: if (_number(e, 0, v)) { below = v == 1; } break;
                        case 6: if (_number(e, 0, v)) { alt = v; } break;
                        case 7:
                            have_time = e.count >= 3 && _number(e, 0, hms[0]) && _number(e, 1, hms[1]) && _number(e, 2, hms[2]);
                            break;
                        case 0x1D: _text(e, date); break;
                        default: break;
                    }
                });
                if (lat && lon && std::fabs(*lat) <= 90 && std::fabs(*lon) <= 180) {
                    codec::location l;
                    l.latitude = lat_ref == 'S' ? -*lat : *lat;
                    l.longitude = lon_ref == 'W' ? -*lon : *lon;
                    if (alt) {
                        l.altitude = below ? -*alt : *alt;
                    }
                    _s.place = l;
                }
                if (date && have_time && hms[0] >= 0 && hms[0] < 24 && hms[1] >= 0 && hms[1] < 60 && hms[2] >= 0 && hms[2] < 61) {
                    MetaStamp t;
                    std::string_view d(date->data(), date->size());
                    if (d.size() >= 10 && meta_number(d, 0, 4, t.year) && meta_number(d, 5, 2, t.month) && meta_number(d, 8, 2, t.day)) {
                        t.hour = int(hms[0]);
                        t.minute = int(hms[1]);
                        t.second = int(hms[2]);
                        t.nanoseconds = uint32_t((hms[2] - t.second) * 1e9);
                        t.has_offset = true;
                        if (meta_plausible(t)) {
                            _s.gps_time = t;
                        }
                    }
                }
            }

            const uint8_t* _p;
            size_t _n;
            MetadataState& _s;
            bool _little = true;
            optional<uint32_t> _image_width, _image_height;
        };

        // ---------------------------------------------------------------
        // XMP: RDF/XML

        inline constexpr std::string_view XmpRdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
        inline constexpr std::string_view XmpTiff = "http://ns.adobe.com/tiff/1.0/";
        inline constexpr std::string_view XmpExif = "http://ns.adobe.com/exif/1.0/";
        inline constexpr std::string_view XmpExifEx = "http://cipa.jp/exif/1.0/";
        inline constexpr std::string_view XmpAux = "http://ns.adobe.com/exif/1.0/aux/";
        inline constexpr std::string_view XmpBasic = "http://ns.adobe.com/xap/1.0/";
        inline constexpr std::string_view XmpPhotoshop = "http://ns.adobe.com/photoshop/1.0/";
        inline constexpr std::string_view XmpDc = "http://purl.org/dc/elements/1.1/";

        // The properties read, by namespace and local name
        enum class XmpField : uint8_t {
            none, make, model, lens_make, lens_model, aux_lens, software, creator_tool, artist, copyright, description,
            date_original, date_created, date_digitized, create_date, date_time, modify_date, exposure_time, f_number,
            iso, sensitivity, exposure_bias, focal_length, focal_35mm, flash, width, height, image_width, image_height,
            orientation, gps_latitude, gps_longitude, gps_altitude, gps_altitude_ref, gps_time, rating, shutter, aperture
        };

        inline XmpField xmp_field(std::string_view ns, std::string_view local) noexcept {
            struct Name {
                std::string_view ns, local;
                XmpField f;
            };
            static constexpr Name names[] = {
                {XmpTiff, "Make", XmpField::make}, {XmpTiff, "Model", XmpField::model}, {XmpTiff, "Software", XmpField::software},
                {XmpTiff, "Artist", XmpField::artist}, {XmpTiff, "Copyright", XmpField::copyright},
                {XmpTiff, "ImageDescription", XmpField::description}, {XmpTiff, "DateTime", XmpField::date_time},
                {XmpTiff, "ImageWidth", XmpField::image_width}, {XmpTiff, "ImageLength", XmpField::image_height},
                {XmpTiff, "Orientation", XmpField::orientation},
                {XmpExif, "DateTimeOriginal", XmpField::date_original}, {XmpExif, "DateTimeDigitized", XmpField::date_digitized},
                {XmpExif, "ExposureTime", XmpField::exposure_time}, {XmpExif, "FNumber", XmpField::f_number},
                {XmpExif, "ISOSpeedRatings", XmpField::iso}, {XmpExif, "ExposureBiasValue", XmpField::exposure_bias},
                {XmpExif, "FocalLength", XmpField::focal_length}, {XmpExif, "FocalLengthIn35mmFilm", XmpField::focal_35mm},
                {XmpExif, "Flash", XmpField::flash}, {XmpExif, "PixelXDimension", XmpField::width},
                {XmpExif, "PixelYDimension", XmpField::height}, {XmpExif, "GPSLatitude", XmpField::gps_latitude},
                {XmpExif, "GPSLongitude", XmpField::gps_longitude}, {XmpExif, "GPSAltitude", XmpField::gps_altitude},
                {XmpExif, "GPSAltitudeRef", XmpField::gps_altitude_ref}, {XmpExif, "GPSTimeStamp", XmpField::gps_time},
                {XmpExif, "ShutterSpeedValue", XmpField::shutter}, {XmpExif, "ApertureValue", XmpField::aperture},
                {XmpExifEx, "LensMake", XmpField::lens_make}, {XmpExifEx, "LensModel", XmpField::lens_model},
                {XmpExifEx, "PhotographicSensitivity", XmpField::sensitivity},
                {XmpAux, "Lens", XmpField::aux_lens}, {XmpAux, "LensModel", XmpField::aux_lens},
                {XmpBasic, "CreatorTool", XmpField::creator_tool}, {XmpBasic, "CreateDate", XmpField::create_date},
                {XmpBasic, "ModifyDate", XmpField::modify_date}, {XmpBasic, "Rating", XmpField::rating},
                {XmpPhotoshop, "DateCreated", XmpField::date_created},
                {XmpDc, "creator", XmpField::artist}, {XmpDc, "rights", XmpField::copyright},
                {XmpDc, "description", XmpField::description},
            };
            for (const Name& n : names) {
                if (n.local == local && n.ns == ns) {
                    return n.f;
                }
            }
            return XmpField::none;
        }

        // The values of XMP's properties, as text, gathered before they
        // are typed (the order the packet gives them in does not matter)
        struct XmpValues {
            std::string v[size_t(XmpField::aperture) + 1];
            bool have[size_t(XmpField::aperture) + 1] = {};
            optional<bool> fired;

            void put(XmpField f, std::string_view text) {
                if (f == XmpField::none || have[size_t(f)]) {
                    return;
                }
                have[size_t(f)] = true;
                v[size_t(f)] = std::string(text);
            }
        };

        SGCL_INLINE_HOT std::string_view meta_view(const string& s) noexcept {
            return std::string_view(s.data(), s.size());
        }

        SGCL_INLINE_HOT std::string_view meta_local(std::string_view qname) noexcept {
            const size_t colon = qname.find(':');
            return colon == std::string_view::npos ? qname : qname.substr(colon + 1);
        }

        // The value of a property element: its text, or the items of an
        // rdf:Seq or rdf:Bag inside joined by "; " (as EXIF's Artist joins
        // several, and ImageIO joins dc:creator), or of an rdf:Alt the
        // x-default item (else the first)
        inline optional<string> xmp_value(const encoding::xml& e) {
            for (const encoding::xml& c : e.children()) {
                if (!c.is_element() || meta_view(c.namespace_uri()) != XmpRdf) {
                    continue;
                }
                const std::string_view kind = meta_view(c.local_name());
                if (kind != "Seq" && kind != "Bag" && kind != "Alt") {
                    continue;
                }
                optional<string> first;
                std::string joined;
                for (const encoding::xml& li : c.children()) {
                    if (!li.is_element() || meta_view(li.local_name()) != "li") {
                        continue;
                    }
                    if (kind == "Alt") {
                        if (auto lang = li.attribute("xml:lang"); lang && meta_view(*lang) == "x-default") {
                            return li.text();
                        }
                        if (!first) {
                            first = li.text();
                        }
                        continue;
                    }
                    const string t = li.text();
                    if (!joined.empty()) {
                        joined += "; ";
                    }
                    joined += meta_view(t);
                }
                if (kind == "Alt") {
                    return first;
                }
                if (joined.empty()) {
                    return nullopt;
                }
                return string(joined.data(), joined.size());
            }
            for (const encoding::xml& c : e.children()) {
                if (c.is_element()) {
                    return nullopt;   // a structure: read by its fields (Flash)
                }
            }
            return e.text();
        }

        // exif:Flash's field Fired, as an attribute or an element of the
        // structure, at any depth of it
        inline void xmp_fired(const encoding::xml& e, XmpValues& out) {
            for (const auto& a : e.attributes()) {
                if (meta_view(a.namespace_uri) == XmpExif && meta_local(meta_view(a.name)) == "Fired") {
                    out.fired = meta_view(a.value) == "True" || meta_view(a.value) == "true";
                    return;
                }
            }
            for (const encoding::xml& c : e.children()) {
                if (!c.is_element()) {
                    continue;
                }
                if (meta_view(c.namespace_uri()) == XmpExif && meta_view(c.local_name()) == "Fired") {
                    const string t = c.text();
                    out.fired = meta_view(t) == "True" || meta_view(t) == "true";
                    return;
                }
                xmp_fired(c, out);
                if (out.fired) {
                    return;
                }
            }
        }

        // Every rdf:Description of the tree, its properties as attributes
        // and as elements
        inline void xmp_walk(const encoding::xml& e, XmpValues& out, int depth) {
            if (depth > 64) {
                return;
            }
            const bool description = meta_view(e.namespace_uri()) == XmpRdf && meta_view(e.local_name()) == "Description";
            if (description) {
                for (const auto& a : e.attributes()) {
                    out.put(xmp_field(meta_view(a.namespace_uri), meta_local(meta_view(a.name))), meta_view(a.value));
                }
            }
            for (const encoding::xml& c : e.children()) {
                if (!c.is_element()) {
                    continue;
                }
                if (description) {
                    const XmpField f = xmp_field(meta_view(c.namespace_uri()), meta_view(c.local_name()));
                    if (f == XmpField::flash) {
                        xmp_fired(c, out);
                        continue;
                    }
                    if (f != XmpField::none) {
                        if (auto v = xmp_value(c)) {
                            out.put(f, meta_view(*v));
                        }
                        continue;
                    }
                }
                xmp_walk(c, out, depth + 1);
            }
        }

        // "1/125", "0.008", "28/10"
        inline optional<double> xmp_real(std::string_view s) noexcept {
            while (!s.empty() && s.front() == ' ') {
                s.remove_prefix(1);
            }
            if (s.empty()) {
                return nullopt;
            }
            std::string t(s);
            char* end = nullptr;
            const double a = std::strtod(t.c_str(), &end);
            if (end == t.c_str()) {
                return nullopt;
            }
            double v = a;
            if (*end == '/') {
                char* end2 = nullptr;
                const double b = std::strtod(end + 1, &end2);
                if (end2 == end + 1 || b == 0) {
                    return nullopt;
                }
                v = a / b;
            }
            if (!std::isfinite(v)) {
                return nullopt;
            }
            return v;
        }

        // XMP's GPSCoordinate: "DDD,MM,SSk" or "DDD,MM.mmk", k one of
        // NSEW (Part 2, 1.2.7.4); a plain signed decimal too
        inline optional<double> xmp_coordinate(std::string_view s) noexcept {
            while (!s.empty() && s.back() == ' ') {
                s.remove_suffix(1);
            }
            if (s.empty()) {
                return nullopt;
            }
            const char k = s.back();
            int sign = 1;
            if (k == 'N' || k == 'S' || k == 'E' || k == 'W') {
                sign = k == 'S' || k == 'W' ? -1 : 1;
                s.remove_suffix(1);
            } else {
                auto v = xmp_real(s);
                return v;
            }
            double parts[3] = {0, 0, 0};
            int n = 0;
            while (!s.empty() && n < 3) {
                const size_t comma = s.find(',');
                auto v = xmp_real(s.substr(0, comma));
                if (!v || *v < 0) {
                    return nullopt;
                }
                parts[n++] = *v;
                if (comma == std::string_view::npos) {
                    break;
                }
                s.remove_prefix(comma + 1);
            }
            if (n < 2) {
                return nullopt;
            }
            return sign * (parts[0] + parts[1] / 60 + parts[2] / 3600);
        }

        // The packet's properties into the fields EXIF left empty
        inline void xmp_read(const string& packet, MetadataState& s) {
            if (packet.empty()) {
                return;
            }
            auto tree = encoding::xml::parse(packet);
            if (!tree) {
                return;
            }
            XmpValues x;
            xmp_walk(*tree, x, 0);
            auto text = [&](XmpField f, optional<string>& out) {
                if (!out && x.have[size_t(f)]) {
                    out = meta_text(reinterpret_cast<const uint8_t*>(x.v[size_t(f)].data()), x.v[size_t(f)].size());
                }
            };
            auto stamp = [&](XmpField f, optional<MetaStamp>& out) {
                if (!out && x.have[size_t(f)]) {
                    out = meta_xmp_stamp(x.v[size_t(f)]);
                }
            };
            auto real = [&](XmpField f, optional<double>& out, bool positive) {
                if (!out && x.have[size_t(f)]) {
                    auto v = xmp_real(x.v[size_t(f)]);
                    if (v && (!positive || *v > 0)) {
                        out = *v;
                    }
                }
            };
            auto whole = [&](XmpField f, optional<uint32_t>& out) {
                optional<double> v;
                real(f, v, true);
                if (!out && v && *v >= 1 && *v <= 4294967295.0) {
                    out = uint32_t(*v);
                }
            };
            text(XmpField::make, s.make);
            text(XmpField::model, s.model);
            text(XmpField::lens_make, s.lens_make);
            text(XmpField::lens_model, s.lens_model);
            text(XmpField::aux_lens, s.lens_model);
            text(XmpField::creator_tool, s.software);
            text(XmpField::software, s.software);
            text(XmpField::artist, s.artist);
            text(XmpField::copyright, s.copyright);
            text(XmpField::description, s.description);
            stamp(XmpField::date_original, s.original);
            stamp(XmpField::date_created, s.original);
            stamp(XmpField::date_digitized, s.digitized);
            stamp(XmpField::create_date, s.digitized);
            stamp(XmpField::modify_date, s.modified);
            stamp(XmpField::date_time, s.modified);
            real(XmpField::exposure_time, s.exposure_time, true);
            real(XmpField::f_number, s.f_number, true);
            if (!s.exposure_time && x.have[size_t(XmpField::shutter)]) {
                if (auto tv = xmp_real(x.v[size_t(XmpField::shutter)]); tv && std::fabs(*tv) < 64) {
                    s.exposure_time = std::exp2(-*tv);
                }
            }
            if (!s.f_number && x.have[size_t(XmpField::aperture)]) {
                if (auto av = xmp_real(x.v[size_t(XmpField::aperture)]); av && *av >= 0 && *av < 64) {
                    s.f_number = std::exp2(*av / 2);
                }
            }
            whole(XmpField::iso, s.iso);
            whole(XmpField::sensitivity, s.iso);
            real(XmpField::exposure_bias, s.exposure_bias, false);
            real(XmpField::focal_length, s.focal_length, true);
            whole(XmpField::focal_35mm, s.focal_length_35mm);
            if (!s.flash_fired && x.fired) {
                s.flash_fired = x.fired;
            }
            whole(XmpField::width, s.width);
            whole(XmpField::image_width, s.width);
            whole(XmpField::height, s.height);
            whole(XmpField::image_height, s.height);
            if (!s.orientation && x.have[size_t(XmpField::orientation)]) {
                if (auto v = xmp_real(x.v[size_t(XmpField::orientation)]); v && *v >= 1 && *v <= 8 && *v == std::floor(*v)) {
                    s.orientation = uint8_t(*v);
                }
            }
            if (!s.rating && x.have[size_t(XmpField::rating)]) {
                if (auto v = xmp_real(x.v[size_t(XmpField::rating)]); v && *v >= -1 && *v <= 5) {
                    s.rating = int(std::floor(*v));
                }
            }
            if (!s.place && x.have[size_t(XmpField::gps_latitude)] && x.have[size_t(XmpField::gps_longitude)]) {
                auto lat = xmp_coordinate(x.v[size_t(XmpField::gps_latitude)]);
                auto lon = xmp_coordinate(x.v[size_t(XmpField::gps_longitude)]);
                if (lat && lon && std::fabs(*lat) <= 90 && std::fabs(*lon) <= 180) {
                    codec::location l;
                    l.latitude = *lat;
                    l.longitude = *lon;
                    if (x.have[size_t(XmpField::gps_altitude)]) {
                        if (auto a = xmp_real(x.v[size_t(XmpField::gps_altitude)])) {
                            const bool below = x.have[size_t(XmpField::gps_altitude_ref)] && x.v[size_t(XmpField::gps_altitude_ref)] == "1";
                            l.altitude = below ? -*a : *a;
                        }
                    }
                    s.place = l;
                }
            }
            if (!s.gps_time && x.have[size_t(XmpField::gps_time)]) {
                auto t = meta_xmp_stamp(x.v[size_t(XmpField::gps_time)]);
                if (t) {
                    if (!t->has_offset) {
                        t->has_offset = true;   // GPS time is UTC
                        t->offset = 0;
                    }
                    s.gps_time = t;
                }
            }
        }

        // ---------------------------------------------------------------
        // The containers

        // What a container scan finds: the EXIF block (a range of the file
        // or bytes of its own) and the XMP packet
        struct MetaFound {
            const uint8_t* exif = nullptr;
            size_t exif_size = 0;
            bool exif_is_file = false;      // TIFF: the file is the structure
            std::vector<uint8_t> exif_own;  // an EXIF assembled from extents (HEIF)
            const uint8_t* xmp = nullptr;
            size_t xmp_size = 0;
            std::vector<uint8_t> xmp_own;   // an XMP decompressed (PNG) or assembled (HEIF)
            bool past_limit = false;        // an item of HEIF past limits.max_metadata
        };

        SGCL_INLINE_HOT uint32_t meta_be32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        SGCL_INLINE_HOT uint32_t meta_le32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        }

        inline void meta_jpeg(const uint8_t* p, size_t n, MetaFound& f) noexcept {
            static constexpr char xap[] = "http://ns.adobe.com/xap/1.0/";   // and its NUL
            size_t i = 2;
            while (i + 4 <= n) {
                if (p[i] != 0xFF) {
                    return;
                }
                const uint8_t m = p[i + 1];
                if (m == 0xFF) {
                    ++i;   // fill
                    continue;
                }
                if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) {
                    i += 2;
                    continue;
                }
                if (m == 0xDA || m == 0xD9) {
                    return;   // the scan: no metadata after it
                }
                const size_t len = size_t(p[i + 2]) << 8 | p[i + 3];
                if (len < 2 || i + 2 + len > n) {
                    return;
                }
                const uint8_t* body = p + i + 4;
                const size_t size = len - 2;
                if (m == 0xE1) {
                    if (!f.exif && size >= 6 && std::memcmp(body, "Exif\0", 5) == 0) {
                        f.exif = body + 6;
                        f.exif_size = size - 6;
                    } else if (!f.xmp && size >= sizeof(xap) && std::memcmp(body, xap, sizeof(xap)) == 0) {
                        f.xmp = body + sizeof(xap);
                        f.xmp_size = size - sizeof(xap);
                    }
                }
                i += 2 + len;
            }
        }

        // false with the error (too_large) when a compressed packet passes
        // the limit
        inline bool meta_png(const uint8_t* p, size_t n, MetaFound& f, size_t max_metadata, error& err) {
            static constexpr char key[] = "XML:com.adobe.xmp";
            size_t i = 8;
            while (i + 12 <= n) {
                const uint32_t len = meta_be32(p + i);
                const uint8_t* type = p + i + 4;
                if (len > n - i - 12) {
                    return true;
                }
                const uint8_t* body = p + i + 8;
                if (std::memcmp(type, "eXIf", 4) == 0 && !f.exif) {
                    f.exif = body;
                    f.exif_size = len;
                } else if (std::memcmp(type, "iTXt", 4) == 0 && !f.xmp && len >= sizeof(key) + 2 && std::memcmp(body, key, sizeof(key)) == 0) {
                    // keyword and NUL, the compression flag and method, the
                    // language tag and NUL, the translated keyword and NUL
                    size_t at = sizeof(key);
                    const uint8_t compressed = body[at];
                    at += 2;
                    for (int k = 0; k < 2 && at < len; ++k) {
                        while (at < len && body[at] != 0) {
                            ++at;
                        }
                        ++at;
                    }
                    if (at <= len) {
                        if (compressed == 1) {
                            auto text = compress::zlib::decompress(slice<const byte>(reinterpret_cast<const byte*>(body + at), len - at),
                                                                   compress::limits{.max_size = max_metadata});
                            if (text) {
                                f.xmp_own.assign(reinterpret_cast<const uint8_t*>(text->data()), reinterpret_cast<const uint8_t*>(text->data()) + text->size());
                                f.xmp = f.xmp_own.data();
                                f.xmp_size = f.xmp_own.size();
                            } else if (text.error().code() == compress::errc::too_large) {
                                err = error(errc::too_large, i, "metadata: an XMP packet past limits.max_metadata");
                                return false;
                            }
                        } else if (compressed == 0) {
                            f.xmp = body + at;
                            f.xmp_size = len - at;
                        }
                    }
                } else if (std::memcmp(type, "IEND", 4) == 0) {
                    return true;
                }
                i += 12 + size_t(len);
            }
            return true;
        }

        inline void meta_webp(const uint8_t* p, size_t n, MetaFound& f) noexcept {
            size_t i = 12;
            const size_t end = std::min<size_t>(n, size_t(meta_le32(p + 4)) + 8);
            while (i + 8 <= end) {
                const uint32_t size = meta_le32(p + i + 4);
                if (size > end - i - 8) {
                    return;
                }
                const uint8_t* body = p + i + 8;
                if (std::memcmp(p + i, "EXIF", 4) == 0 && !f.exif) {
                    // some writers put JPEG's "Exif\0\0" in front
                    const bool prefixed = size >= 6 && std::memcmp(body, "Exif\0\0", 6) == 0;
                    f.exif = body + (prefixed ? 6 : 0);
                    f.exif_size = size - (prefixed ? 6 : 0);
                } else if (std::memcmp(p + i, "XMP ", 4) == 0 && !f.xmp) {
                    f.xmp = body;
                    f.xmp_size = size;
                }
                i += 8 + size_t(size) + (size & 1);
            }
        }

        // HEIF and AVIF (ISO/IEC 23008-12, 14496-12): the meta box's items
        // of type Exif and of type mime "application/rdf+xml", through
        // iinf for their types and iloc for where their bytes lie (in the
        // file, construction method 0, or in idat, method 1)
        class MetaHeif {
        public:
            MetaHeif(const uint8_t* p, size_t n) noexcept : _p(p), _n(n) {
            }

            void run(MetaFound& f, size_t max_metadata) {
                uint64_t at = 0;
                while (at + 8 <= _n) {
                    uint64_t size, body;
                    if (!_box(at, _n, size, body)) {
                        return;
                    }
                    if (std::memcmp(_p + at + 4, "meta", 4) == 0) {
                        _meta(body + 4, at + size, f, max_metadata);
                        return;
                    }
                    at += size;
                }
            }

        private:
            struct Item {
                uint32_t id;
                bool exif;
            };

            // The box at `at` within [.., end): its size and where its body
            // begins; false when it does not fit
            bool _box(uint64_t at, uint64_t end, uint64_t& size, uint64_t& body) const noexcept {
                if (at + 8 > end) {
                    return false;
                }
                size = meta_be32(_p + at);
                body = at + 8;
                if (size == 1) {
                    if (at + 16 > end) {
                        return false;
                    }
                    size = uint64_t(meta_be32(_p + at + 8)) << 32 | meta_be32(_p + at + 12);
                    body = at + 16;
                } else if (size == 0) {
                    size = end - at;
                }
                return size >= body - at && size <= end - at;
            }

            uint64_t _uint(uint64_t& at, unsigned bytes, uint64_t end, bool& ok) const noexcept {
                if (at + bytes > end) {
                    ok = false;
                    return 0;
                }
                uint64_t v = 0;
                for (unsigned i = 0; i < bytes; ++i) {
                    v = v << 8 | _p[at + i];
                }
                at += bytes;
                return v;
            }

            void _meta(uint64_t at, uint64_t end, MetaFound& f, size_t max_metadata) {
                std::vector<Item> items;
                uint64_t iloc = 0, iloc_end = 0, idat = 0, idat_end = 0;
                while (at + 8 <= end) {
                    uint64_t size, body;
                    if (!_box(at, end, size, body)) {
                        break;
                    }
                    const uint8_t* type = _p + at + 4;
                    if (std::memcmp(type, "iinf", 4) == 0) {
                        _iinf(body, at + size, items);
                    } else if (std::memcmp(type, "iloc", 4) == 0) {
                        iloc = body;
                        iloc_end = at + size;
                    } else if (std::memcmp(type, "idat", 4) == 0) {
                        idat = body;
                        idat_end = at + size;
                    }
                    at += size;
                }
                if (items.empty() || !iloc) {
                    return;
                }
                _iloc(iloc, iloc_end, idat, idat_end, items, f, max_metadata);
            }

            void _iinf(uint64_t at, uint64_t end, std::vector<Item>& items) {
                bool ok = true;
                const unsigned version = unsigned(_uint(at, 1, end, ok));
                at += 3;
                const uint64_t count = _uint(at, version == 0 ? 2 : 4, end, ok);
                for (uint64_t k = 0; ok && k < count && at + 8 <= end; ++k) {
                    uint64_t size, body;
                    if (!_box(at, end, size, body)) {
                        return;
                    }
                    const uint64_t box_end = at + size;
                    if (std::memcmp(_p + at + 4, "infe", 4) == 0) {
                        uint64_t q = body;
                        const unsigned v = unsigned(_uint(q, 1, box_end, ok));
                        q += 3;
                        if (ok && v >= 2) {
                            const uint32_t id = uint32_t(_uint(q, v == 2 ? 2 : 4, box_end, ok));
                            q += 2;   // protection index
                            if (ok && q + 4 <= box_end) {
                                const uint8_t* t = _p + q;
                                q += 4;
                                if (std::memcmp(t, "Exif", 4) == 0) {
                                    items.push_back({id, true});
                                } else if (std::memcmp(t, "mime", 4) == 0) {
                                    while (q < box_end && _p[q] != 0) {
                                        ++q;   // the item's name
                                    }
                                    ++q;
                                    static constexpr char rdf[] = "application/rdf+xml";
                                    if (q + sizeof(rdf) - 1 <= box_end && std::memcmp(_p + q, rdf, sizeof(rdf) - 1) == 0) {
                                        items.push_back({id, false});
                                    }
                                }
                            }
                        }
                    }
                    at = box_end;
                }
            }

            void _iloc(uint64_t at, uint64_t end, uint64_t idat, uint64_t idat_end, const std::vector<Item>& items, MetaFound& f, size_t max_metadata) {
                bool ok = true;
                const unsigned version = unsigned(_uint(at, 1, end, ok));
                at += 3;
                const unsigned a = unsigned(_uint(at, 1, end, ok));
                const unsigned b = unsigned(_uint(at, 1, end, ok));
                const unsigned offset_size = a >> 4, length_size = a & 15, base_size = b >> 4;
                const unsigned index_size = version >= 1 ? b & 15 : 0;
                if (!ok || version > 2) {
                    return;
                }
                const uint64_t count = _uint(at, version < 2 ? 2 : 4, end, ok);
                for (uint64_t k = 0; ok && k < count; ++k) {
                    const uint32_t id = uint32_t(_uint(at, version < 2 ? 2 : 4, end, ok));
                    unsigned method = 0;
                    if (version >= 1) {
                        method = unsigned(_uint(at, 2, end, ok)) & 15;
                    }
                    _uint(at, 2, end, ok);   // data reference index
                    const uint64_t base = _uint(at, base_size, end, ok);
                    const uint64_t extents = _uint(at, 2, end, ok);
                    const Item* item = nullptr;
                    for (const Item& it : items) {
                        if (it.id == id) {
                            item = &it;
                        }
                    }
                    std::vector<uint8_t> bytes;
                    bool usable = item && (method == 0 || method == 1) && ((item->exif && f.exif_own.empty() && !f.exif) || (!item->exif && !f.xmp));
                    for (uint64_t x = 0; ok && x < extents; ++x) {
                        if (index_size) {
                            _uint(at, index_size, end, ok);
                        }
                        const uint64_t off = _uint(at, offset_size, end, ok);
                        const uint64_t len = _uint(at, length_size, end, ok);
                        if (!usable || !ok) {
                            continue;
                        }
                        const uint64_t from = method == 0 ? base + off : idat + base + off;
                        const uint64_t limit = method == 0 ? _n : idat_end;
                        if (from < base || from > limit || len > limit - from) {
                            usable = false;
                            continue;
                        }
                        if (bytes.size() + len > max_metadata + 4) {
                            f.past_limit = true;
                            usable = false;
                            continue;
                        }
                        bytes.insert(bytes.end(), _p + from, _p + from + len);
                    }
                    if (!usable || bytes.empty()) {
                        continue;
                    }
                    if (item->exif) {
                        // the offset of the TIFF header after this field
                        if (bytes.size() < 4) {
                            continue;
                        }
                        const uint64_t skip = 4 + uint64_t(meta_be32(bytes.data()));
                        if (skip > bytes.size()) {
                            continue;
                        }
                        f.exif_own.assign(bytes.begin() + ptrdiff_t(skip), bytes.end());
                        f.exif = f.exif_own.data();
                        f.exif_size = f.exif_own.size();
                    } else {
                        f.xmp_own = std::move(bytes);
                        f.xmp = f.xmp_own.data();
                        f.xmp_size = f.xmp_own.size();
                    }
                }
            }

            const uint8_t* _p;
            size_t _n;
        };

        // The blocks into the state, then their fields: EXIF first, XMP
        // where EXIF has none. false with the error for a block past the
        // limit
        inline bool meta_fill(const MetaFound& f, size_t max_metadata, MetadataState& s, error& err) {
            if (f.past_limit || (f.exif_size > max_metadata && !f.exif_is_file)) {
                err = error(errc::too_large, 0, "metadata: an EXIF block or an XMP packet past limits.max_metadata");
                return false;
            }
            if (f.xmp_size > max_metadata) {
                err = error(errc::too_large, 0, "metadata: an XMP packet past limits.max_metadata");
                return false;
            }
            if (f.xmp) {
                size_t n = f.xmp_size;
                // a packet's padding and its NULs (some writers end it with one)
                while (n > 0 && f.xmp[n - 1] == 0) {
                    --n;
                }
                s.xmp = string(reinterpret_cast<const char*>(f.xmp), n);
            }
            if (f.exif) {
                if (!f.exif_is_file) {
                    s.exif = vector<byte>(f.exif_size);
                    sgcl::detail::copy_bytes(s.exif.data(), f.exif, f.exif_size);
                }
                string tiff_xmp;
                ExifReader reader(f.exif, f.exif_size, s);
                reader.run(f.exif_is_file && !f.xmp, &tiff_xmp, max_metadata);
                if (reader.xmp_past_limit) {
                    err = error(errc::too_large, 0, "metadata: an XMP packet past limits.max_metadata");
                    return false;
                }
                if (!tiff_xmp.empty()) {
                    size_t n = tiff_xmp.size();
                    while (n > 0 && tiff_xmp.data()[n - 1] == 0) {
                        --n;
                    }
                    s.xmp = string(tiff_xmp.data(), n);
                }
            }
            xmp_read(s.xmp, s);
            return true;
        }

        // JPEG XL's container (ISO/IEC 18181-2): the boxes "Exif" (4 bytes of
        // the TIFF header's offset, then the block) and "xml " (the XMP
        // packet) at its top level, or either compressed in a "brob" box
        // (its type, then a Brotli stream: what cjxl writes by default); a
        // bare codestream has none. false with the error (too_large) for a
        // compressed box past the limit
        inline bool meta_jxl(const uint8_t* p, size_t n, MetaFound& f, size_t max_metadata, error& err) {
            uint64_t at = 0;
            while (at + 8 <= n) {
                uint64_t size = meta_be32(p + at), body = at + 8;
                if (size == 1) {
                    if (at + 16 > n) {
                        return true;
                    }
                    size = uint64_t(meta_be32(p + at + 8)) << 32 | meta_be32(p + at + 12);
                    body = at + 16;
                } else if (size == 0) {
                    size = n - at;
                }
                if (size < body - at || size > n - at) {
                    return true;
                }
                const uint8_t* type = p + at + 4;
                size_t len = size_t(size - (body - at));
                const uint8_t* content = p + body;
                std::vector<uint8_t> inflated;
                if (std::memcmp(type, "brob", 4) == 0 && len >= 4 &&
                    ((std::memcmp(content, "Exif", 4) == 0 && !f.exif) || (std::memcmp(content, "xml ", 4) == 0 && !f.xmp))) {
                    auto out = compress::brotli::decompress(slice<const byte>(reinterpret_cast<const byte*>(content + 4), len - 4),
                                                            compress::limits{.max_size = max_metadata + 4});
                    if (!out) {
                        if (out.error().code() == compress::errc::too_large) {
                            err = error(errc::too_large, at, "metadata: a compressed box past limits.max_metadata");
                            return false;
                        }
                        at += size;
                        continue;
                    }
                    type = content;
                    inflated.assign(reinterpret_cast<const uint8_t*>(out->data()), reinterpret_cast<const uint8_t*>(out->data()) + out->size());
                    content = inflated.data();
                    len = inflated.size();
                }
                if (std::memcmp(type, "Exif", 4) == 0 && !f.exif && len >= 4) {
                    const uint64_t skip = 4 + uint64_t(meta_be32(content));
                    if (skip <= len) {
                        if (!inflated.empty()) {
                            f.exif_own.assign(inflated.begin() + ptrdiff_t(skip), inflated.end());
                            f.exif = f.exif_own.data();
                            f.exif_size = f.exif_own.size();
                        } else {
                            f.exif = content + skip;
                            f.exif_size = size_t(len - skip);
                        }
                    }
                } else if (std::memcmp(type, "xml ", 4) == 0 && !f.xmp) {
                    if (!inflated.empty()) {
                        f.xmp_own = std::move(inflated);
                        f.xmp = f.xmp_own.data();
                        f.xmp_size = f.xmp_own.size();
                    } else {
                        f.xmp = content;
                        f.xmp_size = len;
                    }
                }
                at += size;
            }
            return true;
        }

        // The metadata of a file of any format sniff knows
        inline bool meta_scan(const uint8_t* p, size_t n, size_t max_metadata, MetadataState& s, error& err) {
            const auto fmt = sniff(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            if (!fmt) {
                err = error(errc::unsupported, 0, "metadata: no format the module reads");
                return false;
            }
            MetaFound f;
            switch (*fmt) {
                case format::jpeg: meta_jpeg(p, n, f); break;
                case format::png:
                    if (!meta_png(p, n, f, max_metadata, err)) {
                        return false;
                    }
                    break;
                case format::webp: meta_webp(p, n, f); break;
                case format::tiff:
                    f.exif = p;
                    f.exif_size = n;
                    f.exif_is_file = true;
                    break;
                case format::heif:
                case format::avif: MetaHeif(p, n).run(f, max_metadata); break;
                case format::jxl:
                    if (!meta_jxl(p, n, f, max_metadata, err)) {
                        return false;
                    }
                    break;
                default: break;   // GIF, BMP, ICO, QOI, the Netpbm formats: none
            }
            return meta_fill(f, max_metadata, s, err);
        }
    }
}
