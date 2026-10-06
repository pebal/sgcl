//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <cstddef>
#include <cstdint>

// The rules of X.690 that every reader of ASN.1 in the library shares: the
// header of an element (the tag and the length, BER's forms and DER's one),
// an OBJECT IDENTIFIER's arcs, an INTEGER's shortest form, the character sets
// of the restricted strings, and the two times (UTCTime and GeneralizedTime,
// X.680 §46-47, X.690 §11.7-11.8). encoding::asn1 reads with them, and so does
// crypto's DER reader of keys and certificates. Plain functions over bytes,
// no allocation, nothing thrown.
namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    // Why a header or a content is not what X.690 allows, the words of the
    // error built from it
    enum class Asn1Fault : uint8_t {
        none,
        end,                    // the header or the content runs past the input
        long_tag,               // a tag number past 2^28
        tag_not_minimal,        // a high-tag-number form for a number under 31, or a leading 0x80
        length_reserved,        // the length byte 0xFF
        length_too_long,        // a length of more than eight bytes
        length_not_minimal,     // DER: a long form where the short does, or a leading zero byte
        indefinite_in_der,      // DER: the indefinite form
        indefinite_primitive    // the indefinite form on a primitive element
    };

    inline const char* asn1_fault_text(Asn1Fault f) noexcept {
        switch (f) {
            case Asn1Fault::none: return "";
            case Asn1Fault::end: return "unexpected end of input";
            case Asn1Fault::long_tag: return "a tag number past 2^28";
            case Asn1Fault::tag_not_minimal: return "a tag number not in its shortest form";
            case Asn1Fault::length_reserved: return "the reserved length 0xFF";
            case Asn1Fault::length_too_long: return "a length of more than eight bytes";
            case Asn1Fault::length_not_minimal: return "a length not in its shortest form, which DER requires";
            case Asn1Fault::indefinite_in_der: return "an indefinite length, which DER does not allow";
            case Asn1Fault::indefinite_primitive: return "an indefinite length on a primitive element";
        }
        return "";
    }

    // The header of one element: the class (the top two bits of the first
    // byte), constructed or not, the tag's number, the bytes of the
    // header, and the content's length (none when indefinite)
    struct Asn1Header {
        uint8_t cls = 0;
        bool constructed = false;
        bool indefinite = false;
        uint8_t size = 0;      // the bytes of the tag and the length
        uint32_t number = 0;
        size_t length = 0;     // the content's bytes; 0 when indefinite
    };

    // The header at p (n bytes left): false with the fault for what X.690
    // does not allow — in DER (ber false) also a length not in its shortest
    // form and the indefinite length. The content's length is checked
    // against the n bytes; an indefinite one is not (its end is found by
    // the walk)
    inline bool asn1_header(const uint8_t* p, size_t n, bool ber, Asn1Header& h, Asn1Fault& fault) noexcept {
        if (n < 2) {
            fault = Asn1Fault::end;
            return false;
        }
        uint8_t b = p[0];
        h.cls = uint8_t(b >> 6);
        h.constructed = (b & 0x20) != 0;
        size_t at = 1;
        uint32_t number = b & 0x1F;
        if (number == 0x1F) {
            // the high-tag-number form: base 128, the first byte not 0x80,
            // at most four bytes (28 bits), and a number of 31 or more
            number = 0;
            for (;;) {
                if (at >= n) {
                    fault = Asn1Fault::end;
                    return false;
                }
                if (at == 5) {
                    fault = Asn1Fault::long_tag;
                    return false;
                }
                uint8_t c = p[at++];
                if (at == 2 && c == 0x80) {
                    fault = Asn1Fault::tag_not_minimal;
                    return false;
                }
                number = number << 7 | (c & 0x7F);
                if ((c & 0x80) == 0) {
                    break;
                }
            }
            if (number < 31) {
                fault = Asn1Fault::tag_not_minimal;
                return false;
            }
        }
        if (at >= n) {
            fault = Asn1Fault::end;
            return false;
        }
        uint8_t l = p[at++];
        size_t length = l;
        bool indefinite = false;
        if (l == 0x80) {
            if (!ber) {
                fault = Asn1Fault::indefinite_in_der;
                return false;
            }
            if (!h.constructed) {
                fault = Asn1Fault::indefinite_primitive;
                return false;
            }
            indefinite = true;
            length = 0;
        } else if (l > 0x80) {
            if (l == 0xFF) {
                fault = Asn1Fault::length_reserved;
                return false;
            }
            size_t k = l & 0x7F;
            if (k > n - at) {
                fault = Asn1Fault::end;
                return false;
            }
            if (!ber && p[at] == 0) {
                fault = Asn1Fault::length_not_minimal;
                return false;
            }
            // leading zero bytes of BER's lengths skipped: AD and OpenLDAP
            // write four bytes always (0x84 00 00 00 05)
            size_t i = 0;
            while (i < k && p[at + i] == 0) {
                ++i;
            }
            if (k - i > 8) {
                fault = Asn1Fault::length_too_long;
                return false;
            }
            length = 0;
            for (; i < k; ++i) {
                length = length << 8 | p[at + i];
            }
            at += k;
            if (!ber && length < 0x80) {
                fault = Asn1Fault::length_not_minimal;
                return false;
            }
        }
        if (!indefinite && length > n - at) {
            fault = Asn1Fault::end;
            return false;
        }
        h.number = number;
        h.indefinite = indefinite;
        h.size = uint8_t(at);
        h.length = length;
        return true;
    }

    // The header of an element known good (one the walk of a parse took),
    // read without a check
    SGCL_INLINE_HOT Asn1Header asn1_trusted_header(const uint8_t* p) noexcept {
        Asn1Header h;
        uint8_t b = p[0];
        h.cls = uint8_t(b >> 6);
        h.constructed = (b & 0x20) != 0;
        size_t at = 1;
        uint32_t number = b & 0x1F;
        if (number == 0x1F) {
            number = 0;
            uint8_t c;
            do {
                c = p[at++];
                number = number << 7 | (c & 0x7F);
            } while (c & 0x80);
        }
        uint8_t l = p[at++];
        size_t length = l;
        if (l & 0x80) {
            size_t k = l & 0x7F;
            length = 0;
            for (size_t i = 0; i < k; ++i) {
                length = length << 8 | p[at + i];
            }
            at += k;
        }
        h.number = number;
        h.size = uint8_t(at);
        h.length = length;
        return h;
    }

    // The bytes DER's header of a tag number and a content length takes
    SGCL_INLINE_HOT size_t asn1_header_size(uint32_t number, size_t length) noexcept {
        size_t s = 2;
        if (number >= 31) {
            for (uint32_t v = number; v != 0; v >>= 7) {
                ++s;
            }
        }
        if (length >= 0x80) {
            for (size_t v = length; v != 0; v >>= 8) {
                ++s;
            }
        }
        return s;
    }

    // DER's header written at out (asn1_header_size bytes); the first
    // byte's class and constructed bit from first (its low five bits are
    // the number's or 0x1F)
    inline uint8_t* asn1_put_header(uint8_t* out, uint8_t cls, bool constructed, uint32_t number, size_t length) noexcept {
        uint8_t first = uint8_t(cls << 6 | (constructed ? 0x20 : 0));
        if (number < 31) {
            *out++ = uint8_t(first | number);
        } else {
            *out++ = uint8_t(first | 0x1F);
            int shift = 28;
            while (shift > 0 && (number >> shift) == 0) {
                shift -= 7;
            }
            for (; shift > 0; shift -= 7) {
                *out++ = uint8_t(0x80 | ((number >> shift) & 0x7F));
            }
            *out++ = uint8_t(number & 0x7F);
        }
        if (length < 0x80) {
            *out++ = uint8_t(length);
        } else {
            int k = 0;
            for (size_t v = length; v != 0; v >>= 8) {
                ++k;
            }
            *out++ = uint8_t(0x80 | k);
            for (int i = k - 1; i >= 0; --i) {
                *out++ = uint8_t(length >> (8 * i));
            }
        }
        return out;
    }

    // An OBJECT IDENTIFIER's (or a RELATIVE-OID's) content: at least one
    // byte, every arc in its shortest form (no leading 0x80), the last byte
    // ending an arc; an arc of more than max_arc_bytes bytes refused (0:
    // any size, as 2.25.<a UUID> has 128 bits)
    inline bool asn1_oid_valid(const uint8_t* p, size_t n, unsigned max_arc_bytes = 0) noexcept {
        if (n == 0 || (p[n - 1] & 0x80) != 0) {
            return false;
        }
        bool start = true;
        unsigned bytes = 0;
        for (size_t i = 0; i < n; ++i) {
            if (start && p[i] == 0x80) {
                return false;
            }
            ++bytes;
            if (max_arc_bytes && bytes > max_arc_bytes) {
                return false;
            }
            start = (p[i] & 0x80) == 0;
            if (start) {
                bytes = 0;
            }
        }
        return true;
    }

    // An INTEGER's (or ENUMERATED's) content: at least one byte, the first
    // nine bits not all equal (X.690 §8.3.2, in BER as in DER)
    SGCL_INLINE_HOT bool asn1_integer_valid(const uint8_t* p, size_t n) noexcept {
        return n != 0 && !(n > 1 && ((p[0] == 0x00 && (p[1] & 0x80) == 0) || (p[0] == 0xFF && (p[1] & 0x80) != 0)));
    }

    // A BIT STRING's content: the count of unused bits first, 0 to 7, 0 for
    // no bits; in DER the unused bits zero
    SGCL_INLINE_HOT bool asn1_bit_string_valid(const uint8_t* p, size_t n, bool ber) noexcept {
        return n != 0 && p[0] <= 7 && (n > 1 || p[0] == 0) && (ber || n == 1 || (p[n - 1] & ((1u << p[0]) - 1)) == 0);
    }

    // The character sets of the restricted strings (X.680 §41): each byte
    // checked by a table, no branch a byte
    struct Asn1CharSets {
        // bit 0: PrintableString, bit 1: NumericString, bit 2: VisibleString,
        // bit 3: IA5String
        uint8_t table[256] = {};

        constexpr Asn1CharSets() noexcept {
            for (int c = 0; c < 0x80; ++c) {
                table[c] |= 8;
            }
            for (int c = 0x20; c < 0x7F; ++c) {
                table[c] |= 4;
            }
            for (int c = '0'; c <= '9'; ++c) {
                table[c] |= 2;
            }
            table[int(' ')] |= 2;
            for (int c = 'A'; c <= 'Z'; ++c) {
                table[c] |= 1;
            }
            for (int c = 'a'; c <= 'z'; ++c) {
                table[c] |= 1;
            }
            for (int c = '0'; c <= '9'; ++c) {
                table[c] |= 1;
            }
            for (char c : {' ', '\'', '(', ')', '+', ',', '-', '.', '/', ':', '=', '?'}) {
                table[uint8_t(c)] |= 1;
            }
        }
    };

    inline constexpr Asn1CharSets asn1_char_sets{};

    inline constexpr uint8_t Asn1Printable = 1;
    inline constexpr uint8_t Asn1Numeric = 2;
    inline constexpr uint8_t Asn1Visible = 4;
    inline constexpr uint8_t Asn1Ia5 = 8;

    // The first byte of p outside the set, or n when every one is in it:
    // the whole run ANDed (no branch a byte), then the place found only
    // when it failed
    inline size_t asn1_outside_set(const uint8_t* p, size_t n, uint8_t set) noexcept {
        uint8_t all = set;
        for (size_t i = 0; i < n; ++i) {
            all &= asn1_char_sets.table[p[i]];
        }
        if (all) {
            return n;
        }
        for (size_t i = 0; i < n; ++i) {
            if (!(asn1_char_sets.table[p[i]] & set)) {
                return i;
            }
        }
        return n;
    }

    // A time of UTCTime or GeneralizedTime: the seconds since 1970 of the
    // instant, the nanoseconds below them, and the offset of the zone the
    // text gives (in seconds east of UTC; has_offset false for Z and for
    // BER's local time, which is read as UTC)
    struct Asn1Time {
        int64_t seconds = 0;
        uint32_t nanoseconds = 0;
        int32_t offset = 0;
        bool has_offset = false;
    };

    SGCL_INLINE_HOT bool asn1_digit(uint8_t c) noexcept {
        return uint8_t(c - '0') < 10;
    }

    SGCL_INLINE_HOT int asn1_two(const uint8_t* p) noexcept {
        return (p[0] - '0') * 10 + (p[1] - '0');
    }

    // The days from 1970-01-01 to a date of the civil calendar
    SGCL_INLINE_HOT int64_t asn1_days_from_civil(int64_t year, int month, int day) noexcept {
        int64_t y = month <= 2 ? year - 1 : year;
        int64_t era = (y >= 0 ? y : y - 399) / 400;
        int64_t yoe = y - era * 400;
        int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    SGCL_INLINE_HOT bool asn1_date_valid(int64_t year, int month, int day) noexcept {
        static constexpr int days_in[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return month >= 1 && month <= 12 && day >= 1 && day <= days_in[month - 1] + (month == 2 && leap ? 1 : 0);
    }

    // The time of a UTCTime (utc true) or a GeneralizedTime. DER (X.690
    // §11.7-11.8): YYMMDDHHMMSSZ and YYYYMMDDHHMMSS[.f]Z, a fraction with no
    // trailing zero. BER (X.680 §46-47) besides: UTCTime without its
    // seconds and with an offset (+hhmm, -hhmm); GeneralizedTime down to
    // the hour, a fraction of the last field given (hour, minute or second)
    // after '.' or ',', Z, an offset (+hh or +hhmm) or nothing (local time,
    // read as UTC). The years of a UTCTime are 1950 to 2049 (RFC 5280). No
    // second 60
    inline bool asn1_time(bool utc, const uint8_t* p, size_t n, bool ber, Asn1Time& out) noexcept {
        size_t at = 0;
        auto digits = [&](size_t k) {
            if (n - at < k) {
                return false;
            }
            for (size_t i = 0; i < k; ++i) {
                if (!asn1_digit(p[at + i])) {
                    return false;
                }
            }
            return true;
        };
        int64_t year;
        if (utc) {
            if (!digits(2)) {
                return false;
            }
            year = asn1_two(p);
            year += year < 50 ? 2000 : 1900;
            at = 2;
        } else {
            if (!digits(4)) {
                return false;
            }
            year = asn1_two(p) * 100 + asn1_two(p + 2);
            at = 4;
        }
        if (!digits(6)) {
            return false;
        }
        int month = asn1_two(p + at), day = asn1_two(p + at + 2), hour = asn1_two(p + at + 4);
        at += 6;
        int minute = 0, second = 0;
        int last = 0;   // the field a fraction belongs to: 0 hour, 1 minute, 2 second
        bool has_minute = digits(2);
        if (has_minute) {
            minute = asn1_two(p + at);
            at += 2;
            last = 1;
            if (digits(2)) {
                second = asn1_two(p + at);
                at += 2;
                last = 2;
            }
        }
        if (utc && !has_minute) {
            return false;
        }
        if (!ber && last != 2) {
            return false;
        }
        // the fraction, GeneralizedTime's alone
        uint64_t fraction = 0;   // in units of 10^-18 of the last field
        if (!utc && at < n && (p[at] == '.' || (ber && p[at] == ','))) {
            if (!ber && last != 2) {
                return false;
            }
            ++at;
            size_t first = at;
            uint64_t scale = 100000000000000000ull;
            while (at < n && asn1_digit(p[at])) {
                if (scale) {
                    fraction += uint64_t(p[at] - '0') * scale;
                    scale /= 10;
                }
                ++at;
            }
            if (at == first || (!ber && p[at - 1] == '0')) {
                return false;   // no digit, or DER's trailing zero
            }
        }
        // the zone
        int32_t offset = 0;
        bool has_offset = false;
        if (at < n && p[at] == 'Z') {
            ++at;
        } else if (ber && at < n && (p[at] == '+' || p[at] == '-')) {
            int sign = p[at] == '-' ? -1 : 1;
            ++at;
            if (!digits(2)) {
                return false;
            }
            int oh = asn1_two(p + at);
            at += 2;
            int om = 0;
            if (digits(2)) {
                om = asn1_two(p + at);
                at += 2;
            } else if (utc) {
                return false;   // UTCTime's offset is hhmm
            }
            if (oh > 23 || om > 59) {
                return false;
            }
            offset = sign * (oh * 3600 + om * 60);
            has_offset = true;
        } else if (!ber || utc) {
            return false;   // DER ends in Z; a UTCTime always has a zone
        }
        if (at != n) {
            return false;
        }
        if (!asn1_date_valid(year, month, day) || hour > 23 || minute > 59 || second > 59) {
            return false;
        }
        int64_t seconds = asn1_days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
        // the fraction of the last field, in nanoseconds: 10^-18 units of
        // an hour, a minute or a second
        uint64_t unit = last == 0 ? 3600 : last == 1 ? 60 : 1;
        unsigned __int128 ns = (unsigned __int128)fraction * unit / 1000000000ull;
        seconds += int64_t(ns / 1000000000ull);
        out.seconds = seconds - offset;
        out.nanoseconds = uint32_t(ns % 1000000000ull);
        out.offset = offset;
        out.has_offset = has_offset;
        return true;
    }
}
