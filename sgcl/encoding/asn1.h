//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/asn1_core.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/utf8.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"
#include "../math/big_integer.h"
#include "../time/datetime.h"
#include "../time/zone.h"
#include "../txt/format.h"

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        struct Asn1Access;
        class Asn1Walk;

        // A tiny unsigned number in decimal digits, the arcs of an OBJECT
        // IDENTIFIER of any size (2.25.<a 128-bit UUID>) turned between
        // decimal text and base 128 at compile time as at run time: at
        // most 63 bytes of base 128, 441 bits, 133 digits
        struct Asn1Decimal {
            static constexpr size_t Max = 136;
            uint8_t digits[Max] = {};   // least significant first
            size_t size = 0;            // 0 is zero

            constexpr void times_add(uint32_t m, uint32_t a) noexcept {
                uint32_t carry = a;
                for (size_t i = 0; i < size; ++i) {
                    uint32_t v = digits[i] * m + carry;
                    digits[i] = uint8_t(v % 10);
                    carry = v / 10;
                }
                while (carry && size < Max) {
                    digits[size++] = uint8_t(carry % 10);
                    carry /= 10;
                }
            }

            // Divided by d, the remainder returned
            constexpr uint32_t divide(uint32_t d) noexcept {
                uint32_t r = 0;
                for (size_t i = size; i-- > 0;) {
                    uint32_t v = r * 10 + digits[i];
                    digits[i] = uint8_t(v / d);
                    r = v % d;
                }
                while (size && digits[size - 1] == 0) {
                    --size;
                }
                return r;
            }

            constexpr bool is_zero() const noexcept {
                return size == 0;
            }

            // Less 80 or 40 (the first subidentifier's arcs); false when
            // the number is smaller
            constexpr bool subtract_small(uint32_t s) noexcept {
                uint32_t v = 0;
                for (size_t i = std::min(size, size_t(4)); i-- > 0;) {
                    v = v * 10 + digits[i];
                }
                if (size <= 4 && v < s) {
                    return false;
                }
                int borrow = 0;
                uint32_t t = s;
                for (size_t i = 0; i < size; ++i) {
                    int d = int(digits[i]) - int(t % 10) - borrow;
                    t /= 10;
                    borrow = d < 0;
                    digits[i] = uint8_t(d + (borrow ? 10 : 0));
                }
                while (size && digits[size - 1] == 0) {
                    --size;
                }
                return true;
            }
        };

        // The base-128 bytes of an arc given in decimal, appended at out
        // (room bytes left): false past the room
        constexpr bool asn1_arc_bytes(Asn1Decimal d, uint8_t* out, size_t room, size_t& written) noexcept {
            uint8_t rev[64] = {};
            size_t n = 0;
            do {
                if (n == 64) {
                    return false;
                }
                rev[n++] = uint8_t(d.divide(128));
            } while (!d.is_zero());
            if (n > room) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                out[i] = uint8_t(rev[n - 1 - i] | (i + 1 < n ? 0x80 : 0));
            }
            written = n;
            return true;
        }

        // The DER content of a dotted OBJECT IDENTIFIER into out (room
        // bytes): false for what is not one (fewer than two arcs, a first
        // arc past 2, a second past 39 under 0 and 1, a leading zero, a
        // character but digits and dots) or past the room (limit true)
        constexpr bool asn1_oid_from_text(std::string_view text, uint8_t* out, size_t room, size_t& size, bool& limit, size_t& at) noexcept {
            limit = false;
            at = 0;
            size_t written = 0;
            size_t arcs = 0;
            uint32_t first = 0;
            while (true) {
                size_t start = at;
                Asn1Decimal d;
                while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
                    if (d.size >= Asn1Decimal::Max - 1) {
                        limit = true;
                        return false;
                    }
                    d.times_add(10, uint32_t(text[at] - '0'));
                    ++at;
                }
                if (at == start || (at - start > 1 && text[start] == '0')) {
                    at = start;
                    return false;
                }
                if (arcs == 0) {
                    if (at - start != 1 || text[start] > '2') {
                        at = start;
                        return false;
                    }
                    first = uint32_t(text[start] - '0');
                } else {
                    if (arcs == 1) {
                        if (first < 2 && (at - start > 2 || (d.size == 2 && d.digits[1] * 10 + d.digits[0] > 39))) {
                            at = start;
                            return false;
                        }
                        d.times_add(1, first * 40);
                    }
                    size_t w = 0;
                    if (!asn1_arc_bytes(d, out + written, room - written, w)) {
                        limit = true;
                        return false;
                    }
                    written += w;
                }
                ++arcs;
                if (at == text.size()) {
                    break;
                }
                if (text[at] != '.') {
                    return false;
                }
                ++at;
            }
            if (arcs < 2) {
                at = text.size();
                return false;
            }
            size = written;
            return true;
        }

        // The decimal digits of a base-128 arc of n bytes appended to s
        inline void asn1_arc_text(const uint8_t* p, size_t n, uint32_t subtract, std::string& s) noexcept {
            uint64_t v = 0;
            if (n <= 9) {
                for (size_t i = 0; i < n; ++i) {
                    v = v << 7 | (p[i] & 0x7F);
                }
                s += std::to_string(v - subtract);
                return;
            }
            Asn1Decimal d;
            for (size_t i = 0; i < n; ++i) {
                d.times_add(128, p[i] & 0x7F);
            }
            d.subtract_small(subtract);
            if (d.is_zero()) {
                s += '0';
            }
            for (size_t i = d.size; i-- > 0;) {
                s += char('0' + d.digits[i]);
            }
        }
    }

    // One element of ASN.1's BER or DER (X.690): a tag and its content,
    // immutable — a view of 32 bytes into the bytes it was read from or
    // made into, so a value that lives where a tracked_ptr may. The same
    // type is read and written, as json is: parse gives an element whose
    // elements are walked by [] or an iterator and whose value is taken by
    // the as_* functions; sequence, integer and the rest make one, and
    // bytes() is its encoding, which is DER.
    class asn1 {
    public:
        using error = encoding::error;

        // The class of a tag (X.680 §8.1): the top two bits of its first byte
        enum class tag_class : uint8_t {
            universal,
            application,
            context_specific,
            private_use
        };

        // The universal types by their tag numbers (X.680 §8.6)
        enum class type : uint32_t {
            boolean = 1,
            integer = 2,
            bit_string = 3,
            octet_string = 4,
            null = 5,
            object_identifier = 6,
            object_descriptor = 7,
            external = 8,
            real = 9,
            enumerated = 10,
            embedded_pdv = 11,
            utf8_string = 12,
            relative_oid = 13,
            sequence = 16,
            set = 17,
            numeric_string = 18,
            printable_string = 19,
            t61_string = 20,
            videotex_string = 21,
            ia5_string = 22,
            utc_time = 23,
            generalized_time = 24,
            graphic_string = 25,
            visible_string = 26,
            general_string = 27,
            universal_string = 28,
            character_string = 29,
            bmp_string = 30
        };

        class oid;
        struct bits;
        class iterator;

        // What a parse accepts. DER by default; ber takes the indefinite
        // length, the constructed strings, the lengths in a longer form than
        // they need, and BER's forms of the values (a BOOLEAN of any byte,
        // a BIT STRING's unused bits set, the times without their seconds,
        // with an offset or a comma)
        struct options {
            bool ber = false;
            uint32_t max_depth = 512;               // elements inside one another
            size_t max_size = size_t(64) << 20;     // the longest element read from a stream
        };

        static const options der;                   // the defaults
        static const options ber;                   // ber set

        // --- making one ---

        // No element: false, what [] gives past the end; a sequence or a
        // set leaves it out, so an absent OPTIONAL component is asn1()
        asn1() noexcept = default;

        static asn1 boolean(bool value) noexcept;

        // An INTEGER of any integral type but bool, in its shortest form
        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>)
        static asn1 integer(I value) noexcept {
            return _integer(uint32_t(type::integer), (__int128)value);
        }

        static asn1 integer(const math::big_integer& value) noexcept;

        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>)
        static asn1 enumerated(I value) noexcept {
            return _integer(uint32_t(type::enumerated), (__int128)value);
        }

        // A BIT STRING of every bit of the bytes, or of the first `length`
        // bits (invalid_argument past them); the bits past the length are
        // written zero, as DER requires
        static asn1 bit_string(const slice<const byte>& bytes) noexcept;
        static asn1 bit_string(const slice<const byte>& bytes, size_t length);

        static asn1 octet_string(const slice<const byte>& bytes) noexcept;
        static asn1 null() noexcept;

        // invalid_argument for an oid() of no arcs
        static asn1 object_identifier(const oid& id);

        // The strings, each holding its character set: invalid_argument for
        // a text outside it — invalid UTF-8; for PrintableString anything
        // but A-Z a-z 0-9, space and '()+,-./:=?; ASCII for IA5String;
        // digits and space for NumericString; printable ASCII for
        // VisibleString; a code point past U+FFFF for BMPString (UCS-2)
        static asn1 utf8_string(const string& text);
        static asn1 printable_string(const string& text);
        static asn1 ia5_string(const string& text);
        static asn1 numeric_string(const string& text);
        static asn1 visible_string(const string& text);
        static asn1 bmp_string(const string& text);

        // The instant in UTC: YYMMDDHHMMSSZ, the years 1950 to 2049 only
        // (invalid_argument outside them), the part of a second dropped;
        // YYYYMMDDHHMMSS[.f]Z with the fraction's trailing zeros dropped
        static asn1 utc_time(const time::datetime& t);
        static asn1 generalized_time(const time::datetime& t) noexcept;

        // A SEQUENCE of the elements in their order; a SET of them in DER's
        // order (X.690 §11.6: by their encodings). An asn1() among them is
        // left out
        static asn1 sequence(std::initializer_list<asn1> elements) noexcept;
        static asn1 sequence(const vector<asn1>& elements) noexcept;
        static asn1 set(std::initializer_list<asn1> elements) noexcept;
        static asn1 set(const vector<asn1>& elements) noexcept;

        // [number] EXPLICIT: a constructed element holding inner; [number]
        // IMPLICIT: inner's content under the tag. asn1() for asn1();
        // invalid_argument for a number past 2^28
        static asn1 explicit_tag(uint32_t number, const asn1& inner, tag_class c = tag_class::context_specific);
        static asn1 implicit_tag(uint32_t number, const asn1& inner, tag_class c = tag_class::context_specific);

        // Any tag over the content given: invalid_argument for one DER does
        // not take (a constructed content that is not elements, an INTEGER
        // not in its shortest form, a number past 2^28)
        static asn1 raw(tag_class c, uint32_t number, bool constructed, const slice<const byte>& content);

        // --- reading ---

        // Exactly one element of the bytes, its structure and the content
        // of every universal type checked; the element is a view into the
        // bytes (into a new buffer for BER with an indefinite length or a
        // constructed string)
        static expected<asn1, error> parse(const slice<const byte>& bytes) noexcept;
        static expected<asn1, error> parse(const slice<const byte>& bytes, const options& o) noexcept;

        // One element of a stream, and no byte past it: its header a byte
        // at a time, so a buffered_reader under a socket. A declared length
        // past max_size is errc::limit_exceeded before anything is held
        static expected<asn1, error> parse(const io::reader& in);
        static expected<asn1, error> parse(const io::reader& in, const options& o);
        static async::task<expected<asn1, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<asn1, error>> async_parse(io::reader in, options o) noexcept;

        // --- what it is ---

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return !_bytes.empty();
        }

        SGCL_INLINE_HOT tag_class cls() const noexcept {
            return tag_class(_flags & 3);
        }

        // The tag's number
        SGCL_INLINE_HOT uint32_t tag() const noexcept {
            return _number;
        }

        SGCL_INLINE_HOT bool constructed() const noexcept {
            return (_flags & Constructed) != 0;
        }

        // Of the universal type t
        SGCL_INLINE_HOT bool is(type t) const noexcept {
            return (_flags & 3) == 0 && _number == uint32_t(t) && !_bytes.empty();
        }

        // [number], context-specific
        SGCL_INLINE_HOT bool is_context(uint32_t number) const noexcept {
            return (_flags & 3) == uint8_t(tag_class::context_specific) && _number == number;
        }

        // --- inside it ---

        // The elements inside a constructed element (0 for a primitive one)
        size_t size() const noexcept;

        SGCL_INLINE_HOT bool empty() const noexcept {
            return !constructed() || _bytes.size() == _header;
        }

        // The element at the index, asn1() past the end
        asn1 operator[](size_t index) const noexcept;

        iterator begin() const noexcept;
        iterator end() const noexcept;

        // The content octets, and the whole element (its tag and length
        // with them): the bytes a signature covers
        SGCL_INLINE_HOT slice<const byte> content() const noexcept {
            return _bytes.last(_bytes.size() - _header);
        }

        SGCL_INLINE_HOT slice<const byte> bytes() const noexcept {
            return _bytes;
        }

        // --- its value: nullopt for another type, or for a content its
        // type does not allow. An element of a class other than universal
        // and primitive ([1] IMPLICIT INTEGER) is read as the type asked ---

        optional<bool> as_bool() const noexcept;
        optional<int64_t> as_int() const noexcept;
        optional<math::big_integer> as_big_integer() const noexcept;
        optional<slice<const byte>> as_bytes() const noexcept;
        optional<bits> as_bits() const noexcept;
        optional<oid> as_oid() const noexcept;
        optional<string> as_string() const noexcept;
        optional<time::datetime> as_time() const noexcept;

        // An indented dump, one element a line: the type and its value
        string to_string() const noexcept;

        // The same bytes
        friend bool operator==(const asn1& a, const asn1& b) noexcept {
            return a._bytes.size() == b._bytes.size()
                && (a._bytes.empty() || std::memcmp(a._bytes.data(), b._bytes.data(), a._bytes.size()) == 0);
        }

        size_t hash() const noexcept {
            return string::hash_of(std::string_view(reinterpret_cast<const char*>(_bytes.data()), _bytes.size()));
        }

    private:
        friend struct detail::Asn1Access;
        friend class detail::Asn1Walk;
        friend class iterator;

        static constexpr uint8_t Constructed = 4;
        static constexpr uint8_t Ber = 8;

        slice<const byte> _bytes;   // the whole element
        uint32_t _number = 0;
        uint8_t _header = 0;        // the bytes of the tag and the length
        uint8_t _flags = 0;         // the class (bits 0-1), Constructed, Ber

        SGCL_INLINE_HOT asn1(const slice<const byte>& whole, uint32_t number, uint8_t header, uint8_t flags) noexcept
        : _bytes(whole), _number(number), _header(header), _flags(flags) {
        }

        SGCL_INLINE_HOT const uint8_t* _p() const noexcept {
            return reinterpret_cast<const uint8_t*>(_bytes.data());
        }

        SGCL_INLINE_HOT bool _ber() const noexcept {
            return (_flags & Ber) != 0;
        }

        // Universal of the number, or of another class and primitive: what
        // an as_* reads
        SGCL_INLINE_HOT bool _readable_as(uint32_t t) const noexcept {
            if (_bytes.empty() || constructed()) {
                return false;
            }
            return (_flags & 3) != 0 || _number == t;
        }

        static asn1 _integer(uint32_t number, __int128 v) noexcept;

        // An element of DER's header and length bytes of content that fill
        // writes
        template<class Fill>
        static asn1 _make(uint8_t cls, bool constructed, uint32_t number, size_t length, Fill&& fill) noexcept;

        static asn1 _list(uint32_t number, const asn1* first, size_t n, bool sorted) noexcept;

        // The child at the offset of the content (a header known good)
        SGCL_INLINE_HOT asn1 _child(size_t at) const noexcept {
            auto p = _p() + _header + at;
            auto h = detail::asn1_trusted_header(p);
            size_t whole = h.size + h.length;
            return asn1(slice<const byte>(_bytes.owner(), _bytes.data() + _header + at, whole), h.number, h.size,
                        uint8_t(h.cls | (h.constructed ? Constructed : 0) | (_flags & Ber)));
        }

        // The pieces of a constructed element of another class joined, when
        // every one is a primitive OCTET STRING (BER's strings in pieces)
        optional<slice<const byte>> _joined() const noexcept;

        static void _check_number(uint32_t number);
    };

    // A BIT STRING read: its bytes, and how many of their bits it holds,
    // the first bit the top one of the first byte
    struct asn1::bits {
        slice<const byte> bytes;
        size_t length = 0;

        // The bit at the index; index < length
        SGCL_INLINE_HOT bool operator[](size_t index) const noexcept {
            return (uint8_t(bytes[index >> 3]) >> (7 - (index & 7))) & 1;
        }
    };

    // The elements inside a constructed element, in their order: a forward
    // iterator whose * makes the element (a view, nothing copied)
    class asn1::iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = asn1;
        using difference_type = std::ptrdiff_t;
        using reference = asn1;
        using pointer = void;

        iterator() noexcept = default;

        SGCL_INLINE_HOT asn1 operator*() const noexcept {
            return _parent._child(_at);
        }

        SGCL_INLINE_HOT iterator& operator++() noexcept {
            auto h = detail::asn1_trusted_header(_parent._p() + _parent._header + _at);
            _at += h.size + h.length;
            return *this;
        }

        SGCL_INLINE_HOT iterator operator++(int) noexcept {
            iterator t = *this;
            ++*this;
            return t;
        }

        SGCL_INLINE_HOT friend bool operator==(const iterator& a, const iterator& b) noexcept {
            return a._at == b._at;
        }

    private:
        friend class asn1;

        asn1 _parent;
        size_t _at = 0;

        SGCL_INLINE_HOT iterator(const asn1& parent, size_t at) noexcept
        : _parent(parent), _at(at) {
        }
    };

    // An OBJECT IDENTIFIER: the DER of its arcs kept inline, 63 bytes at
    // most (2.25.<a UUID> takes 20), so a plain value of 64 bytes with no
    // tracked word: a constant or a global may hold one. A literal is
    // checked at compile time; a text from outside is parsed
    class asn1::oid {
    public:
        static constexpr size_t max_size = 63;   // the bytes of its DER content

        // No arcs: false, and no OBJECT IDENTIFIER
        constexpr oid() noexcept = default;

        // A dotted literal, "1.2.840.113549.1.1.1": one that is not an OID
        // is an error of the compiler (DESIGN 234: a text constructs
        // explicitly)
        template<size_t N>
        explicit consteval oid(const char (&text)[N]) {
            size_t size = 0;
            size_t at = 0;
            bool limit = false;
            if (!detail::asn1_oid_from_text(std::string_view(text, N - 1), _bytes, max_size, size, limit, at)) {
                throw "not an OBJECT IDENTIFIER";
            }
            _size = uint8_t(size);
        }

        // parse's value, or bad_expected_access<encoding::error>
        explicit oid(const string& text)
        : oid(parse(text).value()) {
        }

        // From the arcs: invalid_argument for fewer than two, a first past
        // 2, a second past 39 under 0 and 1, or more than 63 bytes of DER
        oid(std::initializer_list<uint64_t> arcs);

        // A dotted text: errc::syntax for one that is not an OID,
        // errc::limit_exceeded past 63 bytes of DER
        static expected<oid, error> parse(const string& text) noexcept;

        // The arcs (the first two count as two)
        size_t size() const noexcept;

        // The arc at the index; nullopt past the end or past 64 bits
        optional<uint64_t> arc(size_t index) const noexcept;

        bool starts_with(const oid& prefix) const noexcept;

        // Dotted, the arcs of any size
        string to_string() const noexcept;

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return _size != 0;
        }

        SGCL_INLINE_HOT friend bool operator==(const oid& a, const oid& b) noexcept {
            return a._size == b._size && std::memcmp(a._bytes, b._bytes, a._size) == 0;
        }

        // Arc by arc, a prefix first: the order of the shortest base 128
        // compared byte by byte is the order of the arcs
        friend std::strong_ordering operator<=>(const oid& a, const oid& b) noexcept {
            size_t n = std::min(a._size, b._size);
            int c = n ? std::memcmp(a._bytes, b._bytes, n) : 0;
            if (c != 0) {
                return c < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
            }
            return a._size <=> b._size;
        }

        size_t hash() const noexcept {
            return string::hash_of(std::string_view(reinterpret_cast<const char*>(_bytes), _size));
        }

    private:
        friend class asn1;
        friend struct detail::Asn1Access;

        uint8_t _size = 0;
        uint8_t _bytes[max_size] = {};
    };

    static_assert(sizeof(asn1::oid) == 64);
    static_assert(sizeof(asn1) == 32, "a slice, the number, the header, the flags");

    inline const asn1::options asn1::der{};
    inline const asn1::options asn1::ber{true};

    inline void format_value(txt::format_sink& out, const asn1::oid& id, const txt::format_spec& spec) noexcept {
        auto s = id.to_string();
        txt::write_padded(out, s.view(), spec);
    }

    namespace detail {
        struct Asn1Access {
            SGCL_INLINE_HOT static asn1 make(const slice<const byte>& whole, uint32_t number, uint8_t header, uint8_t flags) noexcept {
                return asn1(whole, number, header, flags);
            }

            SGCL_INLINE_HOT static const uint8_t* oid_bytes(const asn1::oid& id) noexcept {
                return id._bytes;
            }

            SGCL_INLINE_HOT static size_t oid_size(const asn1::oid& id) noexcept {
                return id._size;
            }

            // An oid of DER content, false past its room or for content that
            // is not an OID's
            static bool oid_of(const uint8_t* p, size_t n, asn1::oid& out) noexcept {
                if (n > asn1::oid::max_size || !asn1_oid_valid(p, n)) {
                    return false;
                }
                out._size = uint8_t(n);
                sgcl::detail::copy_bytes(out._bytes, p, n);
                return true;
            }
        };

        // The universal types whose content is checked by a parse, and how
        enum class Asn1Content : uint8_t {
            any, boolean, integer, null, oid, bit_string, utf8, printable, numeric, visible, ia5, bmp,
            universal, utc_time, generalized_time
        };

        SGCL_INLINE_HOT Asn1Content asn1_content_of(uint32_t number) noexcept {
            switch (number) {
                case 1: return Asn1Content::boolean;
                case 2:
                case 10: return Asn1Content::integer;
                case 5: return Asn1Content::null;
                case 6:
                case 13: return Asn1Content::oid;
                case 3: return Asn1Content::bit_string;
                case 12: return Asn1Content::utf8;
                case 18: return Asn1Content::numeric;
                case 19: return Asn1Content::printable;
                case 22: return Asn1Content::ia5;
                case 26: return Asn1Content::visible;
                case 28: return Asn1Content::universal;
                case 30: return Asn1Content::bmp;
                case 23: return Asn1Content::utc_time;
                case 24: return Asn1Content::generalized_time;
                default: return Asn1Content::any;
            }
        }

        // The universal types BER may cut in pieces: the strings (and the
        // times, VisibleStrings)
        SGCL_INLINE_HOT bool asn1_is_string(uint32_t number) noexcept {
            return number == 3 || number == 4 || number == 7 || number == 12 || (number >= 18 && number <= 30 && number != 29);
        }

        // Of the types X.680 makes primitive, and constructed
        SGCL_INLINE_HOT bool asn1_primitive_only(uint32_t number) noexcept {
            return number == 1 || number == 2 || number == 5 || number == 6 || number == 9 || number == 10 || number == 13;
        }

        SGCL_INLINE_HOT bool asn1_constructed_only(uint32_t number) noexcept {
            return number == 16 || number == 17 || number == 8 || number == 11 || number == 29;
        }

        // A BMPString's content as UCS-2 (X.680 §41: the code points of the
        // Basic Multilingual Plane, a surrogate none of them, as Go reads
        // it): its UTF-8 appended to out when out is given
        inline bool asn1_bmp(const uint8_t* p, size_t n, std::string* out) noexcept {
            if (n % 2) {
                return false;
            }
            for (size_t i = 0; i < n; i += 2) {
                char32_t c = char32_t(p[i] << 8 | p[i + 1]);
                if (c >= 0xD800 && c <= 0xDFFF) {
                    return false;
                }
                if (out) {
                    char b[4];
                    out->append(b, sgcl::utf8::encode(c, b));
                }
            }
            return true;
        }

        // A UniversalString's content as UCS-4
        inline bool asn1_ucs4(const uint8_t* p, size_t n, std::string* out) noexcept {
            if (n % 4) {
                return false;
            }
            for (size_t i = 0; i < n; i += 4) {
                uint32_t c = uint32_t(p[i]) << 24 | uint32_t(p[i + 1]) << 16 | uint32_t(p[i + 2]) << 8 | p[i + 3];
                if (!sgcl::utf8::valid(char32_t(c))) {
                    return false;
                }
                if (out) {
                    char b[4];
                    out->append(b, sgcl::utf8::encode(char32_t(c), b));
                }
            }
            return true;
        }

        // A content of the universal type checked: nullptr when it is good,
        // else the words of the error and in at the offset of the content
        // where it broke (0 for the whole)
        inline const char* asn1_check_content(Asn1Content k, const uint8_t* p, size_t n, bool ber, size_t& at, errc& code) noexcept {
            at = 0;
            code = errc::syntax;
            switch (k) {
                case Asn1Content::any:
                    return nullptr;
                case Asn1Content::boolean:
                    if (n != 1) {
                        return "a BOOLEAN of other than one byte";
                    }
                    if (!ber && p[0] != 0 && p[0] != 0xFF) {
                        return "a BOOLEAN not 00 or FF, which DER requires";
                    }
                    return nullptr;
                case Asn1Content::integer:
                    return asn1_integer_valid(p, n) ? nullptr : n == 0 ? "an INTEGER of no bytes" : "an INTEGER not in its shortest form";
                case Asn1Content::null:
                    return n == 0 ? nullptr : "a NULL with content";
                case Asn1Content::oid:
                    return asn1_oid_valid(p, n) ? nullptr : "an OBJECT IDENTIFIER whose arcs are not in their shortest form";
                case Asn1Content::bit_string:
                    if (asn1_bit_string_valid(p, n, ber)) {
                        return nullptr;
                    }
                    return n == 0 ? "a BIT STRING of no bytes" : p[0] > 7 || (n == 1 && p[0] != 0) ? "a BIT STRING with a wrong count of unused bits" : "a BIT STRING whose unused bits are not zero, which DER requires";
                case Asn1Content::utf8: {
                    std::string_view v(reinterpret_cast<const char*>(p), n);
                    if (sgcl::utf8::valid(v)) {
                        return nullptr;
                    }
                    for (size_t i = 0; i < n;) {
                        auto [c, w] = sgcl::utf8::decode(v, i);
                        if (c == sgcl::utf8::replacement && w == 1 && !(n - i >= 3 && p[i] == 0xEF && p[i + 1] == 0xBF && p[i + 2] == 0xBD)) {
                            at = i;
                            break;
                        }
                        i += w;
                    }
                    code = errc::invalid_utf8;
                    return "invalid UTF-8 in a UTF8String";
                }
                case Asn1Content::printable:
                case Asn1Content::numeric:
                case Asn1Content::visible:
                case Asn1Content::ia5: {
                    uint8_t set = k == Asn1Content::printable ? Asn1Printable : k == Asn1Content::numeric ? Asn1Numeric : k == Asn1Content::visible ? Asn1Visible : Asn1Ia5;
                    size_t i = asn1_outside_set(p, n, set);
                    if (i == n) {
                        return nullptr;
                    }
                    at = i;
                    code = errc::invalid_character;
                    return k == Asn1Content::printable ? "a character a PrintableString does not allow"
                         : k == Asn1Content::numeric ? "a character a NumericString does not allow"
                         : k == Asn1Content::visible ? "a character a VisibleString does not allow"
                                                     : "a byte past ASCII in an IA5String";
                }
                case Asn1Content::bmp:
                    if (asn1_bmp(p, n, nullptr)) {
                        return nullptr;
                    }
                    code = errc::invalid_character;
                    return n % 2 ? "a BMPString of an odd count of bytes" : "a surrogate in a BMPString, which UCS-2 does not have";
                case Asn1Content::universal:
                    if (asn1_ucs4(p, n, nullptr)) {
                        return nullptr;
                    }
                    code = errc::invalid_character;
                    return "a UniversalString that is not UCS-4";
                case Asn1Content::utc_time:
                case Asn1Content::generalized_time: {
                    Asn1Time t;
                    if (asn1_time(k == Asn1Content::utc_time, p, n, ber, t)) {
                        return nullptr;
                    }
                    return k == Asn1Content::utc_time ? (ber ? "a UTCTime that is not a time" : "a UTCTime not of the form YYMMDDHHMMSSZ, which DER requires")
                                                      : (ber ? "a GeneralizedTime that is not a time" : "a GeneralizedTime not of the form YYYYMMDDHHMMSS[.f]Z, which DER requires");
                }
            }
            return nullptr;
        }

        // The walk of a parse: every header and every universal content
        // checked, without recursion; in BER the sizes of the definite
        // form counted on the way, and when the input has an indefinite
        // length or a constructed string a second walk records its elements
        // and the definite form is written from them
        class Asn1Walk {
        public:
            struct Node {
                size_t start = 0;      // the header in the input
                size_t content = 0;    // the content in the input
                size_t length = 0;     // the content's bytes in the input (a primitive)
                size_t out_length = 0; // the content's bytes in the definite form
                size_t after = 0;      // the node after this one's elements
                uint32_t number = 0;
                uint8_t cls = 0;
                bool constructed = false;
                bool string_root = false;   // a universal string in pieces: written primitive
                bool in_string = false;     // a piece of one
                uint8_t unused = 0;         // a BIT STRING root's unused bits (its last piece's)
            };

            Asn1Walk(const uint8_t* p, size_t n, const asn1::options& o, std::vector<Node>* nodes) noexcept
            : _p(p), _n(n), _o(o), _nodes(nodes) {
            }

            // False with the error; needs_rewrite: BER that DER's form needs
            // writing for
            bool run(error& e, bool& needs_rewrite) noexcept {
                needs_rewrite = false;
                struct Frame {
                    size_t end;
                    size_t limit;         // where its elements must end: its own end, or (indefinite) its nearest definite holder's
                    size_t node;
                    size_t sum;
                    size_t root;          // the frame of the string this one is a piece of, or npos
                    uint32_t number;
                    uint8_t cls;
                    bool indefinite;
                    bool string_root;
                    bool in_string;
                    uint8_t segment;      // the tag of the pieces, 3 or 4, inside a string
                    uint8_t unused;       // the last BIT STRING piece's
                };
                constexpr size_t npos = size_t(-1);
                std::vector<Frame> stack;
                size_t pos = 0;
                bool started = false;
                bool ber = _o.ber;
                auto fail = [&](errc code, size_t at, const char* text) {
                    e = error(code, at, string(text));
                    return false;
                };
                // what a closed element adds to the frame that holds it
                auto add_whole = [&](size_t content_length, uint32_t number) {
                    if (!stack.empty()) {
                        Frame& parent = stack.back();
                        if (!parent.in_string) {
                            parent.sum += asn1_header_size(number, content_length) + content_length;
                        }
                    }
                };
                for (;;) {
                    if (!stack.empty()) {
                        Frame& f = stack.back();
                        bool closing = false;
                        if (pos > f.limit) {
                            return fail(errc::syntax, f.limit, "an element past the end of the element that holds it");
                        }
                        if (f.indefinite) {
                            if (f.limit - pos >= 2 && _p[pos] == 0 && _p[pos + 1] == 0) {
                                pos += 2;
                                closing = true;
                            } else if (pos >= f.limit) {
                                if (f.limit == _n) {
                                    return fail(errc::unexpected_end, _n, "unexpected end of input inside an element of indefinite length");
                                }
                                return fail(errc::syntax, f.limit, "an element of indefinite length without its end-of-contents inside the element that holds it");
                            }
                        } else if (pos == f.end) {
                            closing = true;
                        }
                        if (closing) {
                            Frame done = f;
                            stack.pop_back();
                            size_t content_length = done.sum;
                            if (done.string_root) {
                                content_length += done.segment == 3 ? 1 : 0;
                            }
                            if (_nodes) {
                                Node& nd = (*_nodes)[done.node];
                                nd.out_length = content_length;
                                nd.after = _nodes->size();
                                nd.unused = done.unused;
                            }
                            if (done.string_root || !done.in_string) {
                                add_whole(content_length, done.number);
                            }
                            if (stack.empty()) {
                                break;
                            }
                            continue;
                        }
                    } else if (started) {
                        break;
                    }
                    started = true;
                    size_t limit = stack.empty() ? _n : stack.back().limit;
                    Asn1Header h;
                    Asn1Fault fault = Asn1Fault::none;
                    if (!asn1_header(_p + pos, limit - pos, ber, h, fault)) {
                        if (fault == Asn1Fault::end) {
                            return fail(errc::unexpected_end, pos, limit == _n ? "unexpected end of input inside an element" : "an element past the end of the element that holds it");
                        }
                        return fail(fault == Asn1Fault::long_tag || fault == Asn1Fault::length_too_long ? errc::out_of_range : errc::syntax, pos, asn1_fault_text(fault));
                    }
                    bool universal = h.cls == 0;
                    if (universal && h.number == 0) {
                        return fail(errc::syntax, pos, ber ? "an end-of-contents where no element of indefinite length ends" : "the tag 0, which only BER's end-of-contents has");
                    }
                    bool in_string = !stack.empty() && stack.back().in_string;
                    if (in_string) {
                        Frame& root = stack[stack.back().root];
                        if (!universal || h.number != root.segment) {
                            return fail(errc::syntax, pos, root.segment == 3 ? "a piece of a constructed BIT STRING that is not a BIT STRING" : "a piece of a constructed string that is not an OCTET STRING");
                        }
                    } else if (universal) {
                        if (h.constructed && asn1_primitive_only(h.number)) {
                            return fail(errc::syntax, pos, "a constructed element of a type that is always primitive");
                        }
                        if (!h.constructed && asn1_constructed_only(h.number)) {
                            return fail(errc::syntax, pos, "a primitive SEQUENCE, SET or other type that is always constructed");
                        }
                        if (h.constructed && !ber && asn1_is_string(h.number)) {
                            return fail(errc::syntax, pos, "a constructed string, which DER does not allow");
                        }
                    }
                    if (h.constructed) {
                        if (stack.size() + 1 > _o.max_depth) {
                            return fail(errc::depth_limit, pos, "elements nested deeper than max_depth");
                        }
                        Frame f{};
                        f.end = h.indefinite ? 0 : pos + h.size + h.length;
                        f.limit = h.indefinite ? limit : f.end;
                        f.indefinite = h.indefinite;
                        f.number = h.number;
                        f.cls = h.cls;
                        f.root = npos;
                        f.node = _nodes ? _nodes->size() : 0;
                        if (in_string) {
                            f.in_string = true;
                            f.root = stack.back().root;
                        } else if (universal && asn1_is_string(h.number)) {
                            f.string_root = true;
                            f.in_string = true;
                            f.root = stack.size();
                            f.segment = h.number == 3 ? 3 : 4;
                        }
                        if (h.indefinite || f.string_root) {
                            needs_rewrite = true;
                        }
                        if (_nodes) {
                            Node nd;
                            nd.start = pos;
                            nd.content = pos + h.size;
                            nd.number = h.number;
                            nd.cls = h.cls;
                            nd.constructed = !f.string_root;
                            nd.string_root = f.string_root;
                            nd.in_string = in_string;
                            _nodes->push_back(nd);
                        }
                        stack.push_back(f);
                        pos += h.size;
                        continue;
                    }
                    // a primitive element
                    const uint8_t* c = _p + pos + h.size;
                    if (in_string) {
                        Frame& root = stack[stack.back().root];
                        if (root.segment == 3) {
                            if (root.unused != 0) {
                                return fail(errc::syntax, pos, "a piece of a constructed BIT STRING after one with unused bits");
                            }
                            if (h.length == 0 || c[0] > 7 || (h.length == 1 && c[0] != 0)) {
                                return fail(errc::syntax, pos, "a piece of a BIT STRING with a wrong count of unused bits");
                            }
                            root.unused = c[0];
                            root.sum += h.length - 1;
                        } else {
                            root.sum += h.length;
                        }
                    } else {
                        if (universal) {
                            size_t at;
                            errc code;
                            if (auto why = asn1_check_content(asn1_content_of(h.number), c, h.length, ber, at, code)) {
                                return fail(code, at ? pos + h.size + at : pos, why);
                            }
                        }
                        add_whole(h.length, h.number);
                    }
                    if (_nodes) {
                        Node nd;
                        nd.start = pos;
                        nd.content = pos + h.size;
                        nd.length = h.length;
                        nd.out_length = h.length;
                        nd.number = h.number;
                        nd.cls = h.cls;
                        nd.in_string = in_string;
                        nd.after = _nodes->size() + 1;
                        _nodes->push_back(nd);
                    }
                    pos += h.size + h.length;
                    if (stack.empty()) {
                        break;
                    }
                }
                if (pos != _n) {
                    return fail(errc::syntax, pos, "bytes after the element");
                }
                return true;
            }

            // The definite form of the nodes into out: false with the error
            // for a string joined that its type does not allow
            static bool write(const uint8_t* in, const std::vector<Node>& nodes, uint8_t* out, error& e) noexcept {
                size_t i = 0;
                while (i < nodes.size()) {
                    const Node& nd = nodes[i];
                    if (nd.string_root) {
                        out = asn1_put_header(out, nd.cls, false, nd.number, nd.out_length);
                        uint8_t* content = out;
                        if (nd.number == 3) {
                            *out++ = nd.unused;
                        }
                        for (size_t j = i + 1; j < nd.after; ++j) {
                            const Node& piece = nodes[j];
                            if (!piece.constructed && !piece.string_root) {
                                size_t skip = nd.number == 3 ? 1 : 0;
                                sgcl::detail::copy_bytes(out, in + piece.content + skip, piece.length - skip);
                                out += piece.length - skip;
                            }
                        }
                        size_t at;
                        errc code;
                        if (auto why = asn1_check_content(asn1_content_of(nd.number), content, nd.out_length, true, at, code)) {
                            e = error(code, nd.start, string(why));
                            return false;
                        }
                        i = nd.after;
                    } else if (nd.constructed) {
                        out = asn1_put_header(out, nd.cls, true, nd.number, nd.out_length);
                        ++i;
                    } else {
                        out = asn1_put_header(out, nd.cls, false, nd.number, nd.length);
                        sgcl::detail::copy_bytes(out, in + nd.content, nd.length);
                        out += nd.length;
                        ++i;
                    }
                }
                return true;
            }

            // The parse of bytes: the element, or the error
            static expected<asn1, error> parse(const slice<const byte>& bytes, const asn1::options& o) noexcept {
                auto p = reinterpret_cast<const uint8_t*>(bytes.data());
                size_t n = bytes.size();
                error e;
                bool rewrite = false;
                if (n == 0) {
                    return unexpected<error>(error(errc::unexpected_end, 0, string("no element: the input is empty")));
                }
                if (!Asn1Walk(p, n, o, nullptr).run(e, rewrite)) {
                    return unexpected<error>(std::move(e));
                }
                uint8_t flags = o.ber ? asn1::Ber : 0;
                if (!rewrite) {
                    auto h = asn1_trusted_header(p);
                    return asn1(bytes, h.number, h.size, uint8_t(h.cls | (h.constructed ? asn1::Constructed : 0) | flags));
                }
                std::vector<Node> nodes;
                bool again = false;
                Asn1Walk(p, n, o, &nodes).run(e, again);
                const Node& top = nodes[0];
                size_t total = asn1_header_size(top.number, top.out_length) + top.out_length;
                vector<byte> out;
                VectorOverwrite::resize(out, total);
                if (!write(p, nodes, reinterpret_cast<uint8_t*>(out.data()), e)) {
                    return unexpected<error>(std::move(e));
                }
                auto whole = slice<const byte>(out.as_slice());
                auto h = asn1_trusted_header(reinterpret_cast<const uint8_t*>(whole.data()));
                return asn1(whole, h.number, h.size, uint8_t(h.cls | (h.constructed ? asn1::Constructed : 0) | flags));
            }

        private:
            const uint8_t* _p;
            size_t _n;
            const asn1::options& _o;
            std::vector<Node>* _nodes;
        };

        // The reading of one element from a stream: the header's bytes as
        // they come, then the content, an indefinite length's elements
        // until their end-of-contents. read(n) appends n bytes of the
        // stream to the buffer, false with the error
        struct Asn1StreamState {
            vector<byte> buffer;
            size_t open = 0;      // the elements of indefinite length not yet ended
            bool done = false;
        };

        // What the header just read asks for next: the bytes of a header
        // so far are buffer[start, end); returns the count of bytes to read
        // for the header to be whole (0 when it is), or the error
        inline bool asn1_header_needs(const uint8_t* p, size_t n, size_t& more) noexcept {
            // the tag
            if (n < 1) {
                more = 1;
                return true;
            }
            size_t at = 1;
            if ((p[0] & 0x1F) == 0x1F) {
                for (;;) {
                    if (at >= n) {
                        more = 1;
                        return true;
                    }
                    if (at >= 5) {
                        return false;   // past 2^28: the parse of the header says so
                    }
                    if ((p[at++] & 0x80) == 0) {
                        break;
                    }
                }
            }
            if (at >= n) {
                more = 1;
                return true;
            }
            uint8_t l = p[at++];
            if (l > 0x80 && l != 0xFF) {
                size_t k = l & 0x7F;
                if (n - at < k) {
                    more = k - (n - at);
                    return true;
                }
            }
            more = 0;
            return true;
        }
    }

    // --- making ---

    template<class Fill>
    asn1 asn1::_make(uint8_t cls, bool constructed, uint32_t number, size_t length, Fill&& fill) noexcept {
        size_t hs = detail::asn1_header_size(number, length);
        vector<byte> v;
        sgcl::detail::VectorOverwrite::resize(v, hs + length);
        auto p = reinterpret_cast<uint8_t*>(v.data());
        detail::asn1_put_header(p, cls, constructed, number, length);
        fill(p + hs);
        return asn1(slice<const byte>(v.as_slice()), number, uint8_t(hs), uint8_t(cls | (constructed ? Constructed : 0)));
    }

    inline void asn1::_check_number(uint32_t number) {
        if (number >= (1u << 28)) {
            throw invalid_argument("sgcl::encoding::asn1: a tag number past 2^28");
        }
    }

    inline asn1 asn1::boolean(bool value) noexcept {
        return _make(0, false, uint32_t(type::boolean), 1, [&](uint8_t* p) {
            p[0] = value ? 0xFF : 0x00;
        });
    }

    inline asn1 asn1::_integer(uint32_t number, __int128 v) noexcept {
        uint8_t b[17];
        for (int i = 16; i >= 0; --i) {
            b[i] = uint8_t(v);
            v >>= 8;
        }
        // b[0] is the sign's byte (the value fits 128 bits as unsigned
        // 64-bit numbers do); the shortest form drops the bytes the next
        // one's top bit repeats
        size_t at = 0;
        while (at < 16 && ((b[at] == 0x00 && (b[at + 1] & 0x80) == 0) || (b[at] == 0xFF && (b[at + 1] & 0x80) != 0))) {
            ++at;
        }
        size_t n = 17 - at;
        return _make(0, false, number, n, [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, b + at, n);
        });
    }

    inline asn1 asn1::integer(const math::big_integer& value) noexcept {
        if (value.bit_length() < 64) {
            int64_t small = 0;
            auto mag = value.abs().to_bytes();
            for (auto x : mag) {
                small = small << 8 | uint8_t(x);
            }
            return _integer(uint32_t(type::integer), value.sign() < 0 ? -(__int128)small : (__int128)small);
        }
        bool negative = value.sign() < 0;
        // a negative number is the complement of -value - 1 in as many bytes
        // as leave the top bit set
        math::big_integer m = negative ? -value - math::big_integer(1) : value;
        auto mag = m.to_bytes();
        auto mp = reinterpret_cast<const uint8_t*>(mag.data());
        size_t n = mag.size();
        bool pad = n == 0 || (mp[0] & 0x80) != 0;
        return _make(0, false, uint32_t(type::integer), n + pad, [&](uint8_t* p) {
            if (pad) {
                *p++ = negative ? 0xFF : 0x00;
            }
            for (size_t i = 0; i < n; ++i) {
                p[i] = negative ? uint8_t(~mp[i]) : mp[i];
            }
        });
    }

    inline asn1 asn1::bit_string(const slice<const byte>& bytes) noexcept {
        return _make(0, false, uint32_t(type::bit_string), bytes.size() + 1, [&](uint8_t* p) {
            p[0] = 0;
            sgcl::detail::copy_bytes(p + 1, bytes.data(), bytes.size());
        });
    }

    inline asn1 asn1::bit_string(const slice<const byte>& bytes, size_t length) {
        if (length > bytes.size() * 8) {
            throw invalid_argument("sgcl::encoding::asn1::bit_string: a length past the bits of the bytes");
        }
        size_t n = (length + 7) / 8;
        uint8_t unused = uint8_t(n * 8 - length);
        return _make(0, false, uint32_t(type::bit_string), n + 1, [&](uint8_t* p) {
            p[0] = unused;
            sgcl::detail::copy_bytes(p + 1, bytes.data(), n);
            if (n) {
                p[n] &= uint8_t(0xFF << unused);
            }
        });
    }

    inline asn1 asn1::octet_string(const slice<const byte>& bytes) noexcept {
        return _make(0, false, uint32_t(type::octet_string), bytes.size(), [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, bytes.data(), bytes.size());
        });
    }

    inline asn1 asn1::null() noexcept {
        return _make(0, false, uint32_t(type::null), 0, [](uint8_t*) {});
    }

    inline asn1 asn1::object_identifier(const oid& id) {
        if (!id) {
            throw invalid_argument("sgcl::encoding::asn1::object_identifier: an OID of no arcs");
        }
        return _make(0, false, uint32_t(type::object_identifier), id._size, [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, id._bytes, id._size);
        });
    }

    inline asn1 asn1::utf8_string(const string& text) {
        if (!sgcl::utf8::valid(text.view())) {
            throw invalid_argument("sgcl::encoding::asn1::utf8_string: invalid UTF-8");
        }
        return _make(0, false, uint32_t(type::utf8_string), text.size(), [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, text.data(), text.size());
        });
    }

    namespace detail {
        inline asn1 asn1_restricted(uint32_t number, const string& text, uint8_t set, const char* what);
    }

    inline asn1 asn1::printable_string(const string& text) {
        return detail::asn1_restricted(uint32_t(type::printable_string), text, detail::Asn1Printable, "printable_string");
    }

    inline asn1 asn1::ia5_string(const string& text) {
        return detail::asn1_restricted(uint32_t(type::ia5_string), text, detail::Asn1Ia5, "ia5_string");
    }

    inline asn1 asn1::numeric_string(const string& text) {
        return detail::asn1_restricted(uint32_t(type::numeric_string), text, detail::Asn1Numeric, "numeric_string");
    }

    inline asn1 asn1::visible_string(const string& text) {
        return detail::asn1_restricted(uint32_t(type::visible_string), text, detail::Asn1Visible, "visible_string");
    }

    inline asn1 asn1::bmp_string(const string& text) {
        auto v = text.view();
        if (!sgcl::utf8::valid(v)) {
            throw invalid_argument("sgcl::encoding::asn1::bmp_string: invalid UTF-8");
        }
        size_t units = 0;
        for (size_t i = 0; i < v.size();) {
            auto [c, w] = sgcl::utf8::decode(v, i);
            if (c > 0xFFFF) {
                throw invalid_argument("sgcl::encoding::asn1::bmp_string: a code point past U+FFFF, which UCS-2 does not hold");
            }
            ++units;
            i += w;
        }
        return _make(0, false, uint32_t(type::bmp_string), units * 2, [&](uint8_t* p) {
            for (size_t i = 0; i < v.size();) {
                auto [c, w] = sgcl::utf8::decode(v, i);
                *p++ = uint8_t(c >> 8);
                *p++ = uint8_t(c);
                i += w;
            }
        });
    }

    namespace detail {
        inline asn1 asn1_restricted(uint32_t number, const string& text, uint8_t set, const char* what) {
            auto p = reinterpret_cast<const uint8_t*>(text.data());
            if (asn1_outside_set(p, text.size(), set) != text.size()) {
                throw invalid_argument(std::string("sgcl::encoding::asn1::") + what + ": a character the type does not allow");
            }
            return asn1::implicit_tag(number, asn1::octet_string(slice<const byte>(text)), asn1::tag_class::universal);
        }

        // The two digits of v at p
        SGCL_INLINE_HOT void asn1_put_two(uint8_t* p, int v) noexcept {
            p[0] = uint8_t('0' + v / 10);
            p[1] = uint8_t('0' + v % 10);
        }
    }

    inline asn1 asn1::utc_time(const time::datetime& t) {
        auto u = t.utc();
        int year = u.year();
        if (year < 1950 || year > 2049) {
            throw invalid_argument("sgcl::encoding::asn1::utc_time: a year outside 1950 to 2049, which UTCTime does not hold");
        }
        return _make(0, false, uint32_t(type::utc_time), 13, [&](uint8_t* p) {
            detail::asn1_put_two(p, year % 100);
            detail::asn1_put_two(p + 2, int(u.month()));
            detail::asn1_put_two(p + 4, u.day());
            detail::asn1_put_two(p + 6, u.hour());
            detail::asn1_put_two(p + 8, u.minute());
            detail::asn1_put_two(p + 10, u.second());
            p[12] = 'Z';
        });
    }

    inline asn1 asn1::generalized_time(const time::datetime& t) noexcept {
        auto u = t.utc();
        int year = u.year();
        uint32_t ns = uint32_t(u.nanosecond());
        char fraction[10];
        size_t fn = 0;
        if (ns) {
            fraction[fn++] = '.';
            uint32_t scale = 100000000;
            while (ns) {
                fraction[fn++] = char('0' + ns / scale);
                ns %= scale;
                scale /= 10;
            }
        }
        return _make(0, false, uint32_t(type::generalized_time), 15 + fn, [&](uint8_t* p) {
            detail::asn1_put_two(p, year / 100);
            detail::asn1_put_two(p + 2, year % 100);
            detail::asn1_put_two(p + 4, int(u.month()));
            detail::asn1_put_two(p + 6, u.day());
            detail::asn1_put_two(p + 8, u.hour());
            detail::asn1_put_two(p + 10, u.minute());
            detail::asn1_put_two(p + 12, u.second());
            sgcl::detail::copy_bytes(p + 14, fraction, fn);
            p[14 + fn] = 'Z';
        });
    }

    inline asn1 asn1::_list(uint32_t number, const asn1* first, size_t n, bool sorted) noexcept {
        size_t length = 0;
        for (size_t i = 0; i < n; ++i) {
            length += first[i]._bytes.size();
        }
        std::vector<size_t> order;
        order.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (first[i]) {
                order.push_back(i);
            }
        }
        if (sorted) {
            // X.690 §11.6: the encodings compared as octet strings, the
            // shorter padded at its end with zeros
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                auto& x = first[a]._bytes;
                auto& y = first[b]._bytes;
                size_t m = std::min(x.size(), y.size());
                int c = std::memcmp(x.data(), y.data(), m);
                if (c != 0) {
                    return c < 0;
                }
                if (x.size() < y.size()) {
                    auto q = reinterpret_cast<const uint8_t*>(y.data());
                    for (size_t i = m; i < y.size(); ++i) {
                        if (q[i]) {
                            return true;
                        }
                    }
                }
                return false;
            });
        }
        return _make(0, true, number, length, [&](uint8_t* p) {
            for (size_t i : order) {
                auto& b = first[i]._bytes;
                sgcl::detail::copy_bytes(p, b.data(), b.size());
                p += b.size();
            }
        });
    }

    inline asn1 asn1::sequence(std::initializer_list<asn1> elements) noexcept {
        return _list(uint32_t(type::sequence), elements.begin(), elements.size(), false);
    }

    inline asn1 asn1::sequence(const vector<asn1>& elements) noexcept {
        return _list(uint32_t(type::sequence), elements.data(), elements.size(), false);
    }

    inline asn1 asn1::set(std::initializer_list<asn1> elements) noexcept {
        return _list(uint32_t(type::set), elements.begin(), elements.size(), true);
    }

    inline asn1 asn1::set(const vector<asn1>& elements) noexcept {
        return _list(uint32_t(type::set), elements.data(), elements.size(), true);
    }

    inline asn1 asn1::explicit_tag(uint32_t number, const asn1& inner, tag_class c) {
        _check_number(number);
        if (!inner) {
            return asn1();
        }
        auto& b = inner._bytes;
        return _make(uint8_t(c), true, number, b.size(), [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, b.data(), b.size());
        });
    }

    inline asn1 asn1::implicit_tag(uint32_t number, const asn1& inner, tag_class c) {
        _check_number(number);
        if (!inner) {
            return asn1();
        }
        auto content = inner.content();
        if (c == tag_class::universal) {
            // the content must be what the universal type allows
            return raw(c, number, inner.constructed(), content);
        }
        return _make(uint8_t(c), inner.constructed(), number, content.size(), [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, content.data(), content.size());
        });
    }

    inline asn1 asn1::raw(tag_class c, uint32_t number, bool constructed, const slice<const byte>& content) {
        _check_number(number);
        auto e = _make(uint8_t(c), constructed, number, content.size(), [&](uint8_t* p) {
            sgcl::detail::copy_bytes(p, content.data(), content.size());
        });
        auto checked = parse(e._bytes);
        if (!checked) {
            auto m = checked.error().message();
            throw invalid_argument(std::string("sgcl::encoding::asn1::raw: not DER: ") + std::string(m.view()));
        }
        return e;
    }

    // --- reading ---

    inline expected<asn1, asn1::error> asn1::parse(const slice<const byte>& bytes) noexcept {
        return detail::Asn1Walk::parse(bytes, der);
    }

    inline expected<asn1, asn1::error> asn1::parse(const slice<const byte>& bytes, const options& o) noexcept {
        return detail::Asn1Walk::parse(bytes, o);
    }

    namespace detail {
        // After the header now whole at buffer[start, end): how many bytes
        // of content follow, and whether an element of indefinite length
        // opened or one ended; false with the error
        inline bool asn1_stream_step(vector<byte>& buffer, size_t start, const asn1::options& o, size_t& open, size_t& content, error& e) noexcept {
            auto p = reinterpret_cast<const uint8_t*>(buffer.data()) + start;
            size_t n = buffer.size() - start;
            if (open && n == 2 && p[0] == 0 && p[1] == 0) {
                --open;
                content = 0;
                return true;
            }
            Asn1Header h;
            Asn1Fault fault = Asn1Fault::none;
            // the content is not here yet: the header alone is checked
            if (!asn1_header(p, size_t(-1), o.ber, h, fault)) {
                e = error(fault == Asn1Fault::long_tag || fault == Asn1Fault::length_too_long ? errc::out_of_range : errc::syntax, start, string(asn1_fault_text(fault)));
                return false;
            }
            if (h.indefinite) {
                if (open + 1 > o.max_depth) {
                    e = error(errc::depth_limit, start, string("elements nested deeper than max_depth"));
                    return false;
                }
                ++open;
                content = 0;
                return true;
            }
            if (h.length > o.max_size || buffer.size() + h.length > o.max_size) {
                e = error(errc::limit_exceeded, start, string("an element longer than max_size"));
                return false;
            }
            content = h.length;
            return true;
        }
    }

    inline expected<asn1, asn1::error> asn1::parse(const io::reader& in) {
        return parse(in, der);
    }

    inline expected<asn1, asn1::error> asn1::parse(const io::reader& in, const options& o) {
        vector<byte> buffer;
        size_t open = 0;
        auto take = [&](size_t k) -> expected<void, error> {
            size_t at = buffer.size();
            buffer.resize(at + k);
            auto got = io::read_full(in, buffer.as_slice(at, k));
            if (!got) {
                if (got.error().is_eof()) {
                    return unexpected<error>(error(errc::unexpected_end, at + got.error().count(), string("unexpected end of the stream inside an element")));
                }
                return unexpected<error>(error(got.error(), at + got.error().count()));
            }
            if (*got < k) {
                return unexpected<error>(error(errc::unexpected_end, at, string(at == 0 ? "no element: the stream ended" : "unexpected end of the stream inside an element")));
            }
            return {};
        };
        do {
            size_t start = buffer.size();
            size_t more = 1;
            while (more) {
                if (auto r = take(more); !r) {
                    return unexpected<error>(std::move(r.error()));
                }
                if (!detail::asn1_header_needs(reinterpret_cast<const uint8_t*>(buffer.data()) + start, buffer.size() - start, more)) {
                    more = 0;
                }
            }
            size_t content = 0;
            error e;
            if (!detail::asn1_stream_step(buffer, start, o, open, content, e)) {
                return unexpected<error>(std::move(e));
            }
            if (content) {
                if (auto r = take(content); !r) {
                    return unexpected<error>(std::move(r.error()));
                }
            }
        } while (open);
        return parse(buffer.as_slice(), o);
    }

    inline async::task<expected<asn1, asn1::error>> asn1::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), der);
    }

    inline async::task<expected<asn1, asn1::error>> asn1::async_parse(io::reader in, options o) noexcept {
        vector<byte> buffer;
        size_t open = 0;
        do {
            size_t start = buffer.size();
            size_t more = 1;
            while (more) {
                size_t at = buffer.size();
                buffer.resize(at + more);
                auto got = co_await io::async_read_full(in, buffer.as_slice(at, more));
                if (!got) {
                    if (got.error().is_eof()) {
                        co_return unexpected<error>(error(errc::unexpected_end, at + got.error().count(), string("unexpected end of the stream inside an element")));
                    }
                    co_return unexpected<error>(error(got.error(), at + got.error().count()));
                }
                if (*got < more) {
                    co_return unexpected<error>(error(errc::unexpected_end, at, string(at == 0 ? "no element: the stream ended" : "unexpected end of the stream inside an element")));
                }
                if (!detail::asn1_header_needs(reinterpret_cast<const uint8_t*>(buffer.data()) + start, buffer.size() - start, more)) {
                    more = 0;
                }
            }
            size_t content = 0;
            error e;
            if (!detail::asn1_stream_step(buffer, start, o, open, content, e)) {
                co_return unexpected<error>(std::move(e));
            }
            if (content) {
                size_t at = buffer.size();
                buffer.resize(at + content);
                auto got = co_await io::async_read_full(in, buffer.as_slice(at, content));
                if (!got) {
                    if (got.error().is_eof()) {
                        co_return unexpected<error>(error(errc::unexpected_end, at + got.error().count(), string("unexpected end of the stream inside an element")));
                    }
                    co_return unexpected<error>(error(got.error(), at + got.error().count()));
                }
                if (*got < content) {
                    co_return unexpected<error>(error(errc::unexpected_end, at, string("unexpected end of the stream inside an element")));
                }
            }
        } while (open);
        co_return parse(buffer.as_slice(), o);
    }

    // --- inside ---

    inline size_t asn1::size() const noexcept {
        if (!constructed()) {
            return 0;
        }
        size_t n = 0;
        size_t end = _bytes.size() - _header;
        for (size_t at = 0; at < end; ++n) {
            auto h = detail::asn1_trusted_header(_p() + _header + at);
            at += h.size + h.length;
        }
        return n;
    }

    inline asn1 asn1::operator[](size_t index) const noexcept {
        if (!constructed()) {
            return asn1();
        }
        size_t end = _bytes.size() - _header;
        size_t at = 0;
        for (; at < end && index; --index) {
            auto h = detail::asn1_trusted_header(_p() + _header + at);
            at += h.size + h.length;
        }
        if (at >= end) {
            return asn1();
        }
        return _child(at);
    }

    inline asn1::iterator asn1::begin() const noexcept {
        return iterator(*this, 0);
    }

    inline asn1::iterator asn1::end() const noexcept {
        return iterator(*this, constructed() ? _bytes.size() - _header : 0);
    }

    // --- values ---

    inline optional<bool> asn1::as_bool() const noexcept {
        if (!_readable_as(uint32_t(type::boolean))) {
            return nullopt;
        }
        auto c = content();
        if (c.size() != 1) {
            return nullopt;
        }
        uint8_t b = uint8_t(c[0]);
        if (!_ber() && b != 0 && b != 0xFF) {
            return nullopt;
        }
        return b != 0;
    }

    inline optional<int64_t> asn1::as_int() const noexcept {
        if (_bytes.empty() || constructed() || ((_flags & 3) == 0 && _number != uint32_t(type::integer) && _number != uint32_t(type::enumerated))) {
            return nullopt;
        }
        auto c = content();
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        if (!detail::asn1_integer_valid(p, c.size()) || c.size() > 8) {
            return nullopt;
        }
        int64_t v = int8_t(p[0]);
        for (size_t i = 1; i < c.size(); ++i) {
            v = int64_t(uint64_t(v) << 8 | p[i]);
        }
        return v;
    }

    inline optional<math::big_integer> asn1::as_big_integer() const noexcept {
        if (_bytes.empty() || constructed() || ((_flags & 3) == 0 && _number != uint32_t(type::integer) && _number != uint32_t(type::enumerated))) {
            return nullopt;
        }
        auto c = content();
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        if (!detail::asn1_integer_valid(p, c.size())) {
            return nullopt;
        }
        if (c.size() <= 8) {
            return math::big_integer(*as_int());
        }
        auto magnitude = math::big_integer::from_bytes(c);
        if (p[0] & 0x80) {
            return magnitude - (math::big_integer(1) << uint64_t(8 * c.size()));
        }
        return magnitude;
    }

    inline optional<slice<const byte>> asn1::_joined() const noexcept {
        size_t total = 0;
        for (auto e : *this) {
            if (!e.is(type::octet_string) || e.constructed()) {
                return nullopt;
            }
            total += e.content().size();
        }
        vector<byte> v;
        sgcl::detail::VectorOverwrite::resize(v, total);
        auto out = reinterpret_cast<uint8_t*>(v.data());
        for (auto e : *this) {
            auto c = e.content();
            sgcl::detail::copy_bytes(out, c.data(), c.size());
            out += c.size();
        }
        return slice<const byte>(v.as_slice());
    }

    inline optional<slice<const byte>> asn1::as_bytes() const noexcept {
        if (_bytes.empty()) {
            return nullopt;
        }
        if (constructed()) {
            if ((_flags & 3) != 0 && _ber()) {
                return _joined();
            }
            return nullopt;
        }
        if ((_flags & 3) == 0 && _number != uint32_t(type::octet_string)) {
            return nullopt;
        }
        return content();
    }

    inline optional<asn1::bits> asn1::as_bits() const noexcept {
        if (!_readable_as(uint32_t(type::bit_string))) {
            return nullopt;
        }
        auto c = content();
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        if (!detail::asn1_bit_string_valid(p, c.size(), _ber())) {
            return nullopt;
        }
        return bits{c.last(c.size() - 1), (c.size() - 1) * 8 - p[0]};
    }

    inline optional<asn1::oid> asn1::as_oid() const noexcept {
        if (!_readable_as(uint32_t(type::object_identifier))) {
            return nullopt;
        }
        auto c = content();
        oid id;
        if (!detail::Asn1Access::oid_of(reinterpret_cast<const uint8_t*>(c.data()), c.size(), id)) {
            return nullopt;
        }
        return id;
    }

    inline optional<string> asn1::as_string() const noexcept {
        if (_bytes.empty()) {
            return nullopt;
        }
        slice<const byte> c;
        uint32_t number = _number;
        if ((_flags & 3) != 0) {
            if (constructed()) {
                if (!_ber()) {
                    return nullopt;
                }
                auto j = _joined();
                if (!j) {
                    return nullopt;
                }
                c = *j;
            } else {
                c = content();
            }
            number = uint32_t(type::utf8_string);
        } else {
            if (constructed() || !detail::asn1_is_string(number) || number == uint32_t(type::bit_string) || number == uint32_t(type::octet_string)) {
                return nullopt;
            }
            c = content();
        }
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        size_t n = c.size();
        std::string out;
        switch (number) {
            case uint32_t(type::utf8_string):
                if (!sgcl::utf8::valid(std::string_view(reinterpret_cast<const char*>(p), n))) {
                    return nullopt;
                }
                return string(std::string_view(reinterpret_cast<const char*>(p), n));
            case uint32_t(type::numeric_string):
            case uint32_t(type::printable_string):
            case uint32_t(type::ia5_string):
            case uint32_t(type::visible_string):
            case uint32_t(type::utc_time):
            case uint32_t(type::generalized_time): {
                size_t at;
                errc code;
                if (detail::asn1_check_content(detail::asn1_content_of(number), p, n, _ber(), at, code) && number != uint32_t(type::utc_time) && number != uint32_t(type::generalized_time)) {
                    return nullopt;
                }
                if (detail::asn1_outside_set(p, n, detail::Asn1Ia5) != n) {
                    return nullopt;
                }
                return string(std::string_view(reinterpret_cast<const char*>(p), n));
            }
            case uint32_t(type::bmp_string):
                if (!detail::asn1_bmp(p, n, &out)) {
                    return nullopt;
                }
                return string(out);
            case uint32_t(type::universal_string):
                if (!detail::asn1_ucs4(p, n, &out)) {
                    return nullopt;
                }
                return string(out);
            default:
                // T61String, VideotexString, GraphicString, GeneralString,
                // ObjectDescriptor: ISO 8859-1, as OpenSSL reads them
                out.reserve(n);
                for (size_t i = 0; i < n; ++i) {
                    if (p[i] < 0x80) {
                        out += char(p[i]);
                    } else {
                        out += char(0xC0 | (p[i] >> 6));
                        out += char(0x80 | (p[i] & 0x3F));
                    }
                }
                return string(out);
        }
    }

    inline optional<time::datetime> asn1::as_time() const noexcept {
        if (_bytes.empty() || constructed()) {
            return nullopt;
        }
        auto c = content();
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        detail::Asn1Time t;
        bool ok;
        if ((_flags & 3) == 0) {
            if (_number != uint32_t(type::utc_time) && _number != uint32_t(type::generalized_time)) {
                return nullopt;
            }
            ok = detail::asn1_time(_number == uint32_t(type::utc_time), p, c.size(), _ber(), t);
        } else {
            ok = detail::asn1_time(false, p, c.size(), _ber(), t) || detail::asn1_time(true, p, c.size(), _ber(), t);
        }
        if (!ok) {
            return nullopt;
        }
        auto zone = t.has_offset ? time::zone::fixed(duration(std::chrono::seconds(t.offset))) : time::zone::utc();
        // within the nanoseconds of 64 bits, the instant exact; past them
        // the end of the range, as from_unix gives it
        if (t.seconds > -9223372036 && t.seconds < 9223372036) {
            return time::datetime::from_unix_nano(t.seconds * 1000000000 + int64_t(t.nanoseconds), zone);
        }
        return time::datetime::from_unix(t.seconds, zone);
    }

    // --- the dump ---

    namespace detail {
        inline const char* asn1_type_name(uint32_t n) noexcept {
            switch (n) {
                case 1: return "BOOLEAN";
                case 2: return "INTEGER";
                case 3: return "BIT STRING";
                case 4: return "OCTET STRING";
                case 5: return "NULL";
                case 6: return "OBJECT IDENTIFIER";
                case 7: return "ObjectDescriptor";
                case 8: return "EXTERNAL";
                case 9: return "REAL";
                case 10: return "ENUMERATED";
                case 11: return "EMBEDDED PDV";
                case 12: return "UTF8String";
                case 13: return "RELATIVE-OID";
                case 16: return "SEQUENCE";
                case 17: return "SET";
                case 18: return "NumericString";
                case 19: return "PrintableString";
                case 20: return "T61String";
                case 21: return "VideotexString";
                case 22: return "IA5String";
                case 23: return "UTCTime";
                case 24: return "GeneralizedTime";
                case 25: return "GraphicString";
                case 26: return "VisibleString";
                case 27: return "GeneralString";
                case 28: return "UniversalString";
                case 29: return "CHARACTER STRING";
                case 30: return "BMPString";
                default: return nullptr;
            }
        }

        inline void asn1_hex(std::string& s, const uint8_t* p, size_t n) noexcept {
            static constexpr char digits[] = "0123456789abcdef";
            size_t shown = n > 32 ? 32 : n;
            for (size_t i = 0; i < shown; ++i) {
                s += digits[p[i] >> 4];
                s += digits[p[i] & 15];
            }
            if (shown < n) {
                s += "...";
            }
        }

        inline void asn1_quoted(std::string& s, std::string_view v) noexcept {
            static constexpr char digits[] = "0123456789abcdef";
            s += '"';
            for (char ch : v) {
                auto c = uint8_t(ch);
                if (c == '"' || c == '\\') {
                    s += '\\';
                    s += ch;
                } else if (c < 0x20 || c == 0x7F) {
                    s += "\\x";
                    s += digits[c >> 4];
                    s += digits[c & 15];
                } else {
                    s += ch;
                }
            }
            s += '"';
        }

        // One line of the dump: the tag and the value
        inline void asn1_line(std::string& s, const asn1& e) noexcept {
            uint32_t n = e.tag();
            const char* name = e.cls() == asn1::tag_class::universal ? asn1_type_name(n) : nullptr;
            if (name) {
                s += name;
            } else {
                s += '[';
                switch (e.cls()) {
                    case asn1::tag_class::universal: s += "UNIVERSAL "; break;
                    case asn1::tag_class::application: s += "APPLICATION "; break;
                    case asn1::tag_class::private_use: s += "PRIVATE "; break;
                    case asn1::tag_class::context_specific: break;
                }
                s += std::to_string(n);
                s += ']';
            }
            if (e.constructed()) {
                return;
            }
            auto c = e.content();
            auto p = reinterpret_cast<const uint8_t*>(c.data());
            if (name) {
                switch (n) {
                    case 1:
                        if (auto b = e.as_bool()) {
                            s += *b ? " true" : " false";
                            return;
                        }
                        break;
                    case 2:
                    case 10:
                        if (auto v = e.as_big_integer()) {
                            s += ' ';
                            auto t = v->to_string();
                            s.append(t.data(), t.size());
                            return;
                        }
                        break;
                    case 5:
                        return;
                    case 6:
                        if (auto id = e.as_oid()) {
                            s += ' ';
                            auto t = id->to_string();
                            s.append(t.data(), t.size());
                            return;
                        }
                        break;
                    case 3:
                        if (auto b = e.as_bits()) {
                            s += " (" + std::to_string(b->length) + " bits) ";
                            asn1_hex(s, reinterpret_cast<const uint8_t*>(b->bytes.data()), b->bytes.size());
                            return;
                        }
                        break;
                    case 23:
                    case 24:
                        s += ' ';
                        s.append(reinterpret_cast<const char*>(p), c.size());
                        return;
                    default:
                        if (asn1_is_string(n) && n != 4) {
                            if (auto t = e.as_string()) {
                                s += ' ';
                                asn1_quoted(s, t->view());
                                return;
                            }
                        }
                        break;
                }
            }
            s += " (" + std::to_string(c.size()) + (c.size() == 1 ? " byte)" : " bytes)");
            if (c.size()) {
                s += ' ';
                asn1_hex(s, p, c.size());
            }
        }
    }

    inline string asn1::to_string() const noexcept {
        if (!*this) {
            return string();
        }
        std::string s;
        struct Level {
            iterator at;
            iterator end;
        };
        vector<Level> stack;   // iterators hold the element: a managed vector
        detail::asn1_line(s, *this);
        s += '\n';
        if (constructed()) {
            stack.push_back({begin(), end()});
        }
        while (!stack.empty()) {
            Level& l = stack.back();
            if (l.at == l.end) {
                stack.pop_back();
                continue;
            }
            asn1 e = *l.at;
            ++l.at;
            s.append(2 * stack.size(), ' ');
            detail::asn1_line(s, e);
            s += '\n';
            if (e.constructed()) {
                stack.push_back({e.begin(), e.end()});
            }
        }
        return string(s);
    }

    // --- oid ---

    inline asn1::oid::oid(std::initializer_list<uint64_t> arcs) {
        if (arcs.size() < 2) {
            throw invalid_argument("sgcl::encoding::asn1::oid: fewer than two arcs");
        }
        auto it = arcs.begin();
        uint64_t a = it[0], b = it[1];
        if (a > 2 || (a < 2 && b > 39)) {
            throw invalid_argument("sgcl::encoding::asn1::oid: a first arc past 2 or a second past 39");
        }
        auto put = [&](unsigned __int128 v) {
            uint8_t rev[19];
            size_t n = 0;
            do {
                rev[n++] = uint8_t(v & 0x7F);
                v >>= 7;
            } while (v);
            if (_size + n > max_size) {
                throw invalid_argument("sgcl::encoding::asn1::oid: more than 63 bytes of DER");
            }
            for (size_t i = 0; i < n; ++i) {
                _bytes[_size++] = uint8_t(rev[n - 1 - i] | (i + 1 < n ? 0x80 : 0));
            }
        };
        put((unsigned __int128)a * 40 + b);
        for (size_t i = 2; i < arcs.size(); ++i) {
            put(it[i]);
        }
    }

    inline expected<asn1::oid, asn1::error> asn1::oid::parse(const string& text) noexcept {
        oid id;
        size_t size = 0;
        size_t at = 0;
        bool limit = false;
        if (!detail::asn1_oid_from_text(text.view(), id._bytes, max_size, size, limit, at)) {
            if (limit) {
                return unexpected<error>(error(errc::limit_exceeded, at, string("an OBJECT IDENTIFIER of more than 63 bytes of DER")));
            }
            return unexpected<error>(error(errc::syntax, at, string("not an OBJECT IDENTIFIER")));
        }
        id._size = uint8_t(size);
        return id;
    }

    inline size_t asn1::oid::size() const noexcept {
        if (!_size) {
            return 0;
        }
        size_t n = 1;
        for (size_t i = 0; i < _size; ++i) {
            n += (_bytes[i] & 0x80) == 0;
        }
        return n;
    }

    inline optional<uint64_t> asn1::oid::arc(size_t index) const noexcept {
        size_t at = 0;
        size_t arc = 0;
        while (at < _size) {
            size_t start = at;
            while (_bytes[at] & 0x80) {
                ++at;
            }
            ++at;
            bool big = at - start > 18;   // past 126 bits
            unsigned __int128 v = 0;
            if (!big) {
                for (size_t i = start; i < at; ++i) {
                    v = v << 7 | (_bytes[i] & 0x7F);
                }
            }
            if (arc == 0) {
                // the first subidentifier holds two arcs: 40 · first + second
                uint64_t first = big || v >= 80 ? 2 : v >= 40 ? 1 : 0;
                if (index == 0) {
                    return first;
                }
                if (index == 1) {
                    unsigned __int128 second = v - first * 40;
                    return big || (second >> 64) ? nullopt : optional<uint64_t>(uint64_t(second));
                }
                arc = 2;
                continue;
            }
            if (arc == index) {
                return big || (v >> 64) ? nullopt : optional<uint64_t>(uint64_t(v));
            }
            ++arc;
        }
        return nullopt;
    }

    inline bool asn1::oid::starts_with(const oid& prefix) const noexcept {
        // the prefix's last byte ends an arc, so a prefix of the bytes is
        // one of the arcs (the order of the bytes is the order of the arcs)
        return prefix._size <= _size && std::memcmp(prefix._bytes, _bytes, prefix._size) == 0;
    }

    inline string asn1::oid::to_string() const noexcept {
        std::string s;
        size_t at = 0;
        bool first = true;
        while (at < _size) {
            size_t start = at;
            while (_bytes[at] & 0x80) {
                ++at;
            }
            ++at;
            if (first) {
                // the first subidentifier: 40 · first arc + second arc
                uint64_t lead = 0;
                if (at - start <= 9) {
                    for (size_t i = start; i < at; ++i) {
                        lead = lead << 7 | (_bytes[i] & 0x7F);
                    }
                }
                bool big = at - start > 9;
                uint32_t a = big || lead >= 80 ? 2 : lead >= 40 ? 1 : 0;
                s += char('0' + a);
                s += '.';
                detail::asn1_arc_text(_bytes + start, at - start, a * 40, s);
                first = false;
            } else {
                s += '.';
                detail::asn1_arc_text(_bytes + start, at - start, 0, s);
            }
        }
        return string(s);
    }
}

// Which specifications an oid takes: none but the width, the fill and the
// alignment; the writing is format_value's, above
template<>
struct sgcl::txt::formatter<sgcl::encoding::asn1::oid> {
    SGCL_INLINE_HOT static constexpr bool takes(char type) noexcept {
        return !type;
    }

    SGCL_INLINE_HOT static constexpr bool takes_precision() noexcept {
        return false;
    }
};

template<>
struct std::hash<sgcl::encoding::asn1::oid> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::asn1::oid& id) const noexcept {
        return id.hash();
    }
};

template<>
struct std::hash<sgcl::encoding::asn1> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::asn1& e) const noexcept {
        return e.hash();
    }
};
