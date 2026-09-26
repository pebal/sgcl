//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../error.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The reader of the Time Zone Information Format, versions 1 to 4
// (RFC 9636, which replaced RFC 8536): the files of the tz database that
// every Unix system keeps under /usr/share/zoneinfo. Written from the
// RFC. What it gives back is plain data, no managed object in it, so
// that a file from anywhere is read, checked and refused before anything
// is built from it; zone.h builds the zone.
namespace sgcl::time::detail {
    // A local time type of the file: the offset from UTC in seconds east,
    // whether it is daylight saving time, and its designation ("CEST")
    struct tzif_type {
        int32_t offset = 0;
        bool dst = false;
        std::string abbreviation;
    };

    struct tzif_data {
        int version = 1;                    // 1, 2, 3 or 4
        std::vector<int64_t> at;            // the transitions, seconds since 1970 UTC, strictly ascending
        std::vector<uint8_t> type_of;       // the type each transition goes to
        std::vector<tzif_type> types;       // at least one; type 0 is the time before the first transition
        std::string footer;                 // the POSIX TZ string of versions 2 and later, for after the last transition; may be empty
    };

    class tzif_reader {
    public:
        explicit tzif_reader(const slice<const byte>& bytes) noexcept
        : _p(reinterpret_cast<const unsigned char*>(bytes.data()))
        , _n(bytes.size()) {
        }

        expected<tzif_data, error> read() {
            tzif_data out;
            Header first;
            if (auto e = _header(first)) {
                return unexpected(*e);
            }
            out.version = first.version;
            if (first.version == 1) {
                if (auto e = _block(first, 4, out)) {
                    return unexpected(*e);
                }
                return out;
            }
            // Versions 2 and later: the block of 32-bit times first, kept
            // for readers of version 1 only and skipped here, then a
            // second header and the block of 64-bit times, then the footer
            size_t skip = first.size(4);
            if (_n - _i < skip) {
                return unexpected(error("TZif: the data of version 1 is cut short", _n));
            }
            _i += skip;
            Header second;
            if (auto e = _header(second)) {
                return unexpected(*e);
            }
            if (second.version != first.version) {
                return unexpected(error("TZif: the two headers give different versions", _i - 44 + 4));
            }
            if (auto e = _block(second, 8, out)) {
                return unexpected(*e);
            }
            // The footer: a newline, the TZ string, a newline, and then
            // the end of the file (RFC 9636 3.3)
            if (_i >= _n || _p[_i] != '\n') {
                return unexpected(error("TZif: a newline before the footer expected", _i));
            }
            size_t from = ++_i;
            while (_i < _n && _p[_i] != '\n') {
                if (_p[_i] == 0) {
                    return unexpected(error("TZif: a NUL in the footer", _i));
                }
                ++_i;
            }
            if (_i >= _n) {
                return unexpected(error("TZif: the footer does not end with a newline", _i));
            }
            out.footer.assign(reinterpret_cast<const char*>(_p + from), _i - from);
            ++_i;
            if (_i != _n) {
                return unexpected(error("TZif: bytes after the footer", _i));
            }
            return out;
        }

    private:
        struct Header {
            int version = 1;
            uint32_t isutcnt = 0, isstdcnt = 0, leapcnt = 0, timecnt = 0, typecnt = 0, charcnt = 0;

            // The bytes of the data block that follows, for times of
            // `width` bytes; in 64 bits, the counts being 32-bit each
            size_t size(size_t width) const noexcept {
                return size_t(timecnt) * width + timecnt + size_t(typecnt) * 6 + charcnt
                     + size_t(leapcnt) * (width + 4) + isstdcnt + isutcnt;
            }
        };

        uint32_t _u32(size_t at) const noexcept {
            return (uint32_t(_p[at]) << 24) | (uint32_t(_p[at + 1]) << 16) | (uint32_t(_p[at + 2]) << 8) | uint32_t(_p[at + 3]);
        }

        int64_t _time(size_t at, size_t width) const noexcept {
            if (width == 4) {
                return int64_t(int32_t(_u32(at)));
            }
            return int64_t((uint64_t(_u32(at)) << 32) | _u32(at + 4));
        }

        optional<error> _header(Header& h) {
            if (_n - _i < 44) {
                return error("TZif: the header is cut short", _n);
            }
            if (_p[_i] != 'T' || _p[_i + 1] != 'Z' || _p[_i + 2] != 'i' || _p[_i + 3] != 'f') {
                return error("TZif: not a TZif file (no \"TZif\" at the start)", _i);
            }
            unsigned char v = _p[_i + 4];
            if (v == 0) {
                h.version = 1;
            } else if (v >= '2' && v <= '4') {
                h.version = v - '0';
            } else {
                return error("TZif: a version this reader does not know", _i + 4);
            }
            size_t c = _i + 20;
            h.isutcnt = _u32(c);
            h.isstdcnt = _u32(c + 4);
            h.leapcnt = _u32(c + 8);
            h.timecnt = _u32(c + 12);
            h.typecnt = _u32(c + 16);
            h.charcnt = _u32(c + 20);
            // RFC 9636 3.1: typecnt and charcnt not zero, isutcnt and
            // isstdcnt zero or typecnt
            if (h.typecnt == 0) {
                return error("TZif: no local time type", c + 16);
            }
            if (h.typecnt > 256) {
                return error("TZif: more than 256 local time types", c + 16);   // an index is one byte
            }
            if (h.charcnt == 0) {
                return error("TZif: no designation characters", c + 20);
            }
            if (h.isutcnt != 0 && h.isutcnt != h.typecnt) {
                return error("TZif: isutcnt is neither zero nor typecnt", c);
            }
            if (h.isstdcnt != 0 && h.isstdcnt != h.typecnt) {
                return error("TZif: isstdcnt is neither zero nor typecnt", c + 4);
            }
            _i += 44;
            return nullopt;
        }

        optional<error> _block(const Header& h, size_t width, tzif_data& out) {
            if (_n - _i < h.size(width)) {
                return error("TZif: the data is cut short", _n);
            }
            size_t times = _i;
            size_t indices = times + size_t(h.timecnt) * width;
            size_t types = indices + h.timecnt;
            size_t chars = types + size_t(h.typecnt) * 6;
            size_t leaps = chars + h.charcnt;
            size_t stds = leaps + size_t(h.leapcnt) * (width + 4);
            size_t uts = stds + h.isstdcnt;
            _i = uts + h.isutcnt;

            out.at.clear();
            out.type_of.clear();
            out.types.clear();
            out.at.reserve(h.timecnt);
            out.type_of.reserve(h.timecnt);
            for (uint32_t k = 0; k < h.timecnt; ++k) {
                int64_t t = _time(times + k * width, width);
                if (k != 0 && t <= out.at.back()) {
                    return error("TZif: the transition times are not in ascending order", times + k * width);
                }
                unsigned char type = _p[indices + k];
                if (type >= h.typecnt) {
                    return error("TZif: a transition to a local time type that is not there", indices + k);
                }
                out.at.push_back(t);
                out.type_of.push_back(type);
            }
            const char* designations = reinterpret_cast<const char*>(_p + chars);
            for (uint32_t k = 0; k < h.typecnt; ++k) {
                size_t at = types + k * 6;
                tzif_type type;
                uint32_t offset = _u32(at);
                // RFC 9636 3.2: more than -25 hours and less than 26; one
                // past that is no place's, and would put a time of the clock
                // more than a day from its instant
                if (int32_t(offset) < -89999 || int32_t(offset) > 93599) {
                    return error("TZif: an offset of 26 hours or more, past RFC 9636's range", at);
                }
                type.offset = int32_t(offset);
                if (_p[at + 4] > 1) {
                    return error("TZif: isdst neither 0 nor 1", at + 4);
                }
                type.dst = _p[at + 4] == 1;
                unsigned idx = _p[at + 5];
                if (idx >= h.charcnt) {
                    return error("TZif: a designation index past the characters", at + 5);
                }
                size_t end = idx;
                while (end < h.charcnt && designations[end] != 0) {
                    ++end;
                }
                if (end == h.charcnt) {
                    return error("TZif: a designation with no NUL after it", chars + idx);
                }
                type.abbreviation.assign(designations + idx, end - idx);
                out.types.push_back(std::move(type));
            }
            // The leap-second records are read past: the times here are
            // POSIX times, as Go's and std::chrono::sys_time are, and a
            // file of the "right/" kind is not read as one. Their shape
            // is still checked, so a file is either whole or refused.
            for (uint32_t k = 0; k < h.leapcnt; ++k) {
                size_t at = leaps + k * (width + 4);
                int64_t occurrence = _time(at, width);
                if (k == 0 ? occurrence < 0 : occurrence <= _time(at - width - 4, width)) {
                    return error("TZif: the leap-second records are not in ascending order", at);
                }
            }
            for (uint32_t k = 0; k < h.isstdcnt; ++k) {
                if (_p[stds + k] > 1) {
                    return error("TZif: a standard/wall indicator neither 0 nor 1", stds + k);
                }
            }
            for (uint32_t k = 0; k < h.isutcnt; ++k) {
                if (_p[uts + k] > 1) {
                    return error("TZif: a UT/local indicator neither 0 nor 1", uts + k);
                }
                if (_p[uts + k] == 1 && (h.isstdcnt == 0 || _p[stds + k] != 1)) {
                    return error("TZif: a UT indicator set on a type whose standard indicator is not", uts + k);
                }
            }
            return nullopt;
        }

        const unsigned char* _p;
        size_t _n;
        size_t _i = 0;
    };

    inline expected<tzif_data, error> read_tzif(const slice<const byte>& bytes) {
        return tzif_reader(bytes).read();
    }
}
