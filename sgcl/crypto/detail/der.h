//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../secret.h"
#include "../secure_zero.h"
#include "../../core/detail/bytes.h"
#include "../../encoding/detail/asn1_core.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

// DER (X.690), the module's one reader and writer of it: the keys of X25519
// and Ed25519 (RFC 8410), of the NIST curves (SEC 1, RFC 5480, 5915, PKCS
// #8) and of RSA (PKCS #1, RFC 8017 §A.1), the ECDSA-Sig-Value of a
// signature, and X.509's certificates (RFC 5280). Strict DER, as Go's
// cryptobyte reads it: single-byte tags (the high-tag-number form refused),
// definite lengths in their shortest form of at most four bytes, every
// length checked against what holds it, a BOOLEAN one byte of 00 or FF, an
// OBJECT IDENTIFIER with every arc in its shortest form, a BIT STRING whose
// unused bits are zero, the times of RFC 5280 §4.1.2.5 only in their one
// form. The reader walks as far as its caller asks, one level at a time,
// and never recurses; an element nested deeper than max_depth levels is
// refused (§11 A7 of the design: no input makes the walk deeper than a
// certificate is), and the limits of size and count a certificate needs
// are the certificate parser's, over this reader. The rules of X.690 it
// shares with encoding::asn1 (a header, an arc, the times) are
// encoding/detail/asn1_core.h's; the narrower ones of a certificate are
// here.
namespace sgcl::crypto::detail {
    namespace der {
        inline constexpr unsigned char boolean = 0x01;
        inline constexpr unsigned char integer = 0x02;
        inline constexpr unsigned char bit_string = 0x03;
        inline constexpr unsigned char octet_string = 0x04;
        inline constexpr unsigned char null = 0x05;
        inline constexpr unsigned char object_identifier = 0x06;
        inline constexpr unsigned char utf8_string = 0x0c;
        inline constexpr unsigned char numeric_string = 0x12;
        inline constexpr unsigned char printable_string = 0x13;
        inline constexpr unsigned char t61_string = 0x14;
        inline constexpr unsigned char ia5_string = 0x16;
        inline constexpr unsigned char utc_time = 0x17;
        inline constexpr unsigned char generalized_time = 0x18;
        inline constexpr unsigned char bmp_string = 0x1e;
        inline constexpr unsigned char sequence = 0x30;
        inline constexpr unsigned char set = 0x31;
        inline constexpr unsigned char context0 = 0xa0;   // [0], constructed
        inline constexpr unsigned char context1 = 0xa1;   // [1], constructed
        inline constexpr unsigned char context3 = 0xa3;   // [3], constructed
        inline constexpr unsigned char implicit0 = 0x80;  // [0] IMPLICIT, primitive
        inline constexpr unsigned char implicit1 = 0x81;  // [1] IMPLICIT, primitive
        inline constexpr unsigned char implicit2 = 0x82;  // [2] IMPLICIT, primitive

        // The deepest an element may lie below the reader over the whole
        // input: a certificate's deepest is ten (a GeneralName in a
        // subtree of its name constraints), a key's five
        inline constexpr unsigned max_depth = 16;
    }

    // A reader over DER, each element read with the tag it must have. A
    // read that fails leaves the reader where it was; offset() is the
    // position in the whole input, for the error
    class DerReader {
    public:
        DerReader() noexcept = default;

        SGCL_INLINE_HOT DerReader(const unsigned char* p, size_t n, size_t base = 0) noexcept
        : _p(p), _n(n), _base(base) {
        }

        // The levels this reader lies below the reader over the whole
        // input
        SGCL_INLINE_HOT unsigned depth() const noexcept {
            return _depth;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _pos == _n;
        }

        SGCL_INLINE_HOT size_t offset() const noexcept {
            return _base + _pos;
        }

        SGCL_INLINE_HOT const unsigned char* data() const noexcept {
            return _p + _pos;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _n - _pos;
        }

        SGCL_INLINE_HOT bool peek(unsigned char tag) const noexcept {
            return _pos < _n && _p[_pos] == tag;
        }

        // The next element, which must have the tag: its content in
        // content. False for another tag, an indefinite length, a length
        // not in its shortest form, a length past the end
        SGCL_INLINE_HOT bool read(unsigned char tag, DerReader& content) noexcept {
            if (_pos >= _n || _p[_pos] != tag) {
                return false;
            }
            unsigned char t;
            return read_any(t, content);
        }

        // The next element whatever its tag (in tag), its content in
        // content; false as read, for a tag of the high-tag-number form
        // (low five bits all ones), which nothing here has, for the
        // universal tag 0, end-of-contents, which only BER's indefinite
        // lengths have, for a universal type in the constructed form other
        // than SEQUENCE and SET (a string cut in pieces, which DER
        // forbids), and for SEQUENCE and SET in the primitive form
        bool read_any(unsigned char& tag, DerReader& content) noexcept {
            size_t pos = _pos;
            if (pos >= _n || _depth >= der::max_depth) {
                return false;
            }
            unsigned char t0 = _p[pos];
            if ((t0 & 0x1f) == 0x1f || (t0 & 0xdf) == 0x00 || ((t0 & 0xe0) == 0x20 && t0 != der::sequence && t0 != der::set) || t0 == 0x10 || t0 == 0x11) {
                return false;
            }
            encoding::detail::Asn1Header h;
            encoding::detail::Asn1Fault fault;
            // DER's header (a length in its shortest form, never indefinite),
            // its length of at most four bytes
            if (!encoding::detail::asn1_header(_p + pos, _n - pos, false, h, fault) || h.size > 6) {
                return false;
            }
            const unsigned char* c = _p + pos + h.size;
            if (!_content_ok(t0, c, h.length)) {
                return false;
            }
            content = DerReader(c, h.length, _base + pos + h.size);
            content._depth = _depth + 1;
            tag = t0;
            _pos = pos + h.size + h.length;
            return true;
        }

        // The next element, which must have the tag, whole: its tag and
        // length with its content, for the bytes a signature covers (the
        // TBSCertificate) or a name is compared by (the issuer, the
        // subject). The element's reader is at the depth of this one
        bool read_element(unsigned char tag, DerReader& element) noexcept {
            size_t start = _pos;
            DerReader c;
            if (!read(tag, c)) {
                return false;
            }
            element = DerReader(_p + start, _pos - start, _base + start);
            element._depth = _depth;
            return true;
        }

        // An element of the tag if the next one has it: present says
        // whether it did; false only for one that has the tag and is not
        // DER
        SGCL_INLINE_HOT bool read_optional(unsigned char tag, DerReader& content, bool& present) noexcept {
            present = peek(tag);
            return !present || read(tag, content);
        }

        // A BOOLEAN: one byte, 00 or FF
        bool read_bool(bool& value) noexcept {
            DerReader c;
            size_t pos = _pos;
            if (!read(der::boolean, c) || c._n != 1 || (c._p[0] != 0x00 && c._p[0] != 0xff)) {
                _pos = pos;
                return false;
            }
            value = c._p[0] != 0;
            return true;
        }

        // An OBJECT IDENTIFIER, its content in content: at least one byte,
        // every arc in its shortest form (no leading 80), the last byte
        // ending an arc
        SGCL_INLINE_HOT bool read_oid(DerReader& content) noexcept {
            size_t pos = _pos;
            if (!read(der::object_identifier, content) || !oid_valid(content._p, content._n)) {
                _pos = pos;
                return false;
            }
            return true;
        }

        // Every arc in its shortest form and of at most 63 bits
        SGCL_INLINE_HOT static bool oid_valid(const unsigned char* p, size_t n) noexcept {
            return encoding::detail::asn1_oid_valid(p, n, 9);
        }

        // A BIT STRING of DER: the count of unused bits first (0 to 7, 0
        // for an empty string) and those bits zero. The bytes after the
        // count in p and n, the count in unused
        bool read_bit_string(const unsigned char*& p, size_t& n, unsigned& unused) noexcept {
            DerReader c;
            size_t pos = _pos;
            if (!read(der::bit_string, c) || c._n == 0) {
                _pos = pos;
                return false;
            }
            unsigned u = c._p[0];
            if (u > 7 || (c._n == 1 && u != 0) || (c._n > 1 && (c._p[c._n - 1] & ((1u << u) - 1)) != 0)) {
                _pos = pos;
                return false;
            }
            p = c._p + 1;
            n = c._n - 1;
            unused = u;
            return true;
        }

        // A non-negative INTEGER of at most 63 bits, in its shortest form
        bool read_small_unsigned(uint64_t& value) noexcept {
            const unsigned char* p;
            size_t n;
            size_t pos = _pos;
            if (!read_unsigned_bytes(p, n) || n > 8 || (n == 8 && (p[0] & 0x80) != 0)) {
                _pos = pos;
                return false;
            }
            uint64_t v = 0;
            for (size_t i = 0; i < n; ++i) {
                v = v << 8 | p[i];
            }
            value = v;
            return true;
        }

        // A time of RFC 5280 §4.1.2.5 in the seconds since 1970: a
        // UTCTime YYMMDDHHMMSSZ (the years 1950 to 2049) or a
        // GeneralizedTime YYYYMMDDHHMMSSZ, nothing else — no fraction, no
        // offset, no time without its seconds, every field in its range (a
        // day the month has, no second 60)
        bool read_time(int64_t& seconds) noexcept {
            DerReader c;
            size_t pos = _pos;
            unsigned char tag = peek(der::utc_time) ? der::utc_time : der::generalized_time;
            if (!read(tag, c) || !time_of(tag, c._p, c._n, seconds)) {
                _pos = pos;
                return false;
            }
            return true;
        }

        // The seconds of a UTCTime or GeneralizedTime's content in the one
        // form read_time takes; false for any other
        static bool time_of(unsigned char tag, const unsigned char* p, size_t n, int64_t& seconds) noexcept {
            // DER's forms (X.690 §11.7-11.8) without the fraction RFC 5280 has
            // no place for: thirteen and fifteen characters
            bool utc = tag == der::utc_time;
            encoding::detail::Asn1Time t;
            if (n != (utc ? 13u : 15u) || !encoding::detail::asn1_time(utc, p, n, false, t)) {
                return false;
            }
            seconds = t.seconds;
            return true;
        }

        // An element of the tag whose content is exactly the bytes given
        // (a version INTEGER, an OBJECT IDENTIFIER)
        SGCL_INLINE_HOT bool read_exact(unsigned char tag, const unsigned char* bytes, size_t n) noexcept {
            DerReader c;
            size_t pos = _pos;
            if (!read(tag, c) || c.size() != n || std::memcmp(c.data(), bytes, n) != 0) {
                _pos = pos;
                return false;
            }
            return true;
        }

        // A non-negative INTEGER in its shortest form, written big-endian
        // into size bytes (padded with zeros on the left); false for a
        // negative number, a padded one, one of more than size bytes
        bool read_unsigned(unsigned char* out, size_t size) noexcept {
            DerReader c;
            size_t pos = _pos;
            if (!read(der::integer, c)) {
                return false;
            }
            const unsigned char* p = c.data();
            size_t n = c.size();
            bool ok = n != 0 && (p[0] & 0x80) == 0 && !(n > 1 && p[0] == 0 && (p[1] & 0x80) == 0);
            if (ok && p[0] == 0 && n > 1) {
                ++p;
                --n;
            }
            if (!ok || n > size) {
                _pos = pos;
                return false;
            }
            sgcl::detail::fill_bytes(out, 0, size - n);
            sgcl::detail::copy_bytes(out + size - n, p, n);
            return true;
        }

        // A non-negative INTEGER in its shortest form, of any length: its
        // magnitude in p and n, without the zero byte in front of a top bit
        // that is set (a zero is the one byte 00); false for a negative
        // number or a padded one. The bytes are the input's, not copied:
        // RSA's numbers, as long as the input allows
        bool read_unsigned_bytes(const unsigned char*& p, size_t& n) noexcept {
            DerReader c;
            size_t pos = _pos;
            if (!read(der::integer, c)) {
                return false;
            }
            const unsigned char* q = c.data();
            size_t m = c.size();
            if (m == 0 || (q[0] & 0x80) != 0 || (m > 1 && q[0] == 0 && (q[1] & 0x80) == 0)) {
                _pos = pos;
                return false;
            }
            if (q[0] == 0 && m > 1) {
                ++q;
                --m;
            }
            p = q;
            n = m;
            return true;
        }

    private:
        const unsigned char* _p = nullptr;
        size_t _n = 0;
        size_t _pos = 0;
        size_t _base = 0;
        unsigned _depth = 0;

        // The content of the universal primitive types whose DER is fixed,
        // wherever they are, an ANY included (the parameters of an
        // algorithm, a name's value): a BOOLEAN of 00 or FF, an INTEGER or
        // ENUMERATED in its shortest form, an empty NULL, an OBJECT
        // IDENTIFIER oid_valid takes, a BIT STRING whose count of unused
        // bits is 0 to 7 and whose unused bits are zero, a time in the one
        // form of RFC 5280
        static bool _content_ok(unsigned char tag, const unsigned char* p, size_t n) noexcept {
            switch (tag) {
                case der::boolean:
                    return n == 1 && (p[0] == 0x00 || p[0] == 0xff);
                case der::integer:
                case 0x0a:   // ENUMERATED
                    return encoding::detail::asn1_integer_valid(p, n);
                case der::null:
                    return n == 0;
                case der::object_identifier:
                    return oid_valid(p, n);
                case der::bit_string:
                    return encoding::detail::asn1_bit_string_valid(p, n, false);
                case der::utc_time:
                case der::generalized_time: {
                    int64_t s;
                    return time_of(tag, p, n, s);
                }
                default:
                    return true;
            }
        }
    };

    // A writer that builds DER from the end backwards, so that a length is
    // known when its header is written: the content first, then wrap()
    // puts the tag and the length of everything written since a mark in
    // front of it. A fixed buffer on the stack, never reallocated (a key's
    // bytes pass through it), zeroed when the writer goes
    template<size_t Capacity>
    class DerWriter {
    public:
        DerWriter() noexcept = default;
        DerWriter(const DerWriter&) = delete;
        DerWriter& operator=(const DerWriter&) = delete;

        SGCL_INLINE_HOT ~DerWriter() {
            secure_zero(_buf, Capacity);
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return Capacity - _pos;
        }

        SGCL_INLINE_HOT const unsigned char* data() const noexcept {
            return _buf + _pos;
        }

        SGCL_INLINE_HOT void put(const unsigned char* p, size_t n) noexcept {
            assert(_pos >= n && "DerWriter: past its capacity");
            _pos -= n;
            sgcl::detail::copy_bytes(_buf + _pos, p, n);
        }

        SGCL_INLINE_HOT void put(unsigned char b) noexcept {
            assert(_pos >= 1 && "DerWriter: past its capacity");
            _buf[--_pos] = b;
        }

        // The tag and the length of what was written since mark (a size())
        void wrap(unsigned char tag, size_t mark) noexcept {
            size_t len = size() - mark;
            if (len < 0x80) {
                put(static_cast<unsigned char>(len));
            } else {
                unsigned char k = 0;
                for (size_t l = len; l != 0; l >>= 8) {
                    put(static_cast<unsigned char>(l));
                    ++k;
                }
                put(static_cast<unsigned char>(0x80 | k));
            }
            put(tag);
        }

        // An INTEGER of a big-endian unsigned number of n bytes, in its
        // shortest form
        void put_unsigned(const unsigned char* p, size_t n) noexcept {
            while (n > 1 && p[0] == 0) {
                ++p;
                --n;
            }
            size_t mark = size();
            put(p, n);
            if (p[0] & 0x80) {
                put(0);
            }
            wrap(der::integer, mark);
        }

    private:
        unsigned char _buf[Capacity];
        size_t _pos = Capacity;
    };

    // What a DerWriter wrote, when it holds a secret (a private key): into
    // a secret_bytes, never managed memory (the writer zeroes its own
    // buffer when it goes)
    template<class W>
    SGCL_INLINE_HOT secret_bytes take_secret(const W& w) noexcept {
        secret_bytes out(w.size());
        sgcl::detail::copy_bytes(out.as_slice().data(), w.data(), w.size());
        return out;
    }
}
