//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "base64.h"
#include "error.h"
#include "hex.h"
#include "json.h"
#include "detail/json_number.h"
#include "detail/item_stream.h"
#include "detail/utf8_check.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/detail/maker.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/utf8.h"
#include "../core/vector.h"
#include "../math/big_integer.h"
#include "../time/datetime.h"
#include "../time/layout.h"
#include "../time/zone.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;
        struct CborAccess;
        class CborParser;
        class CborWriter;
    }

    // One CBOR value (RFC 8949), immutable, as json is one JSON value: a
    // pointer, a word and a kind, 24 bytes, shared by copying. Made by its
    // constructors and the static functions that name what C++ has no
    // literal for, read by parse, written by to_bytes (the preferred
    // serialization, or the deterministic one) and to_string (the
    // diagnostic notation). The same value is MessagePack's (msgpack.h),
    // whose extension types are a kind of their own.
    class cbor {
    public:
        using error = encoding::error;

        // The kind of a value: CBOR's data model, and MessagePack's extension
        enum class kind : uint8_t {
            null = 0,
            undefined,
            boolean,
            integer,      // -2^64 to 2^64 - 1, as major types 0 and 1 hold them
            floating,
            bytes,
            text,
            array,
            map,
            tag,          // a number and the value it tags
            simple,       // a simple value other than false, true, null, undefined
            extension     // MessagePack's: a type of -128 to 127 and bytes
        };

        // A member of a map: a key of any kind and its value
        struct member;

        // What a parse accepts. A key given twice in one map, invalid
        // UTF-8 in a text string are errors unless allowed
        struct options {
            uint32_t max_depth = 512;               // arrays, maps and tags inside one another
            bool allow_duplicate_keys = false;      // then the last one wins
            bool allow_invalid_utf8 = false;        // then the bytes as they are
            size_t max_size = size_t(64) << 20;     // the longest item read from a stream
        };

        // How a value is written: the preferred serialization (§4.1), or
        // with the keys of every map sorted by their encodings, the
        // deterministic one (§4.2.1)
        struct style {
            bool sort_keys = false;
        };

        static const style preferred;
        static const style deterministic;

        // --- making one ---

        // null
        cbor() noexcept = default;

        SGCL_INLINE_HOT cbor(std::nullptr_t) noexcept {
        }

        template<class B>
        requires std::same_as<B, bool>
        SGCL_INLINE_HOT cbor(B b) noexcept
        : _bits(b), _kind(kind::boolean) {
        }

        // An integer of any integral type but bool and the characters
        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>) && (!std::same_as<I, char>) && (!std::same_as<I, wchar_t>)
              && (!std::same_as<I, char8_t>) && (!std::same_as<I, char16_t>) && (!std::same_as<I, char32_t>)
        SGCL_INLINE_HOT cbor(I v) noexcept
        : _kind(kind::integer) {
            if constexpr (std::is_signed_v<I>) {
                if (v < 0) {
                    _bits = ~uint64_t(int64_t(v));   // -1 - v
                    _aux = 1;
                    return;
                }
            }
            _bits = uint64_t(v);
        }

        SGCL_INLINE_HOT cbor(double d) noexcept
        : _bits(std::bit_cast<uint64_t>(d)), _kind(kind::floating) {
        }

        SGCL_INLINE_HOT cbor(float f) noexcept
        : cbor(double(f)) {
        }

        // A text string: its characters shared, not copied
        SGCL_INLINE_HOT cbor(const string& s) noexcept
        : _ptr(s.as_slice().owner()), _kind(kind::text) {
        }

        SGCL_INLINE_HOT cbor(const char* s) noexcept
        : cbor(string(s)) {
        }

        // An integer of any size: within -2^64 to 2^64 - 1 an integer, past
        // them a bignum, tag 2 or 3 over its bytes (§3.4.3)
        cbor(const math::big_integer& v) noexcept;

        static cbor undefined() noexcept {
            cbor c;
            c._kind = kind::undefined;
            return c;
        }

        // A simple value, 0 to 19 or 32 to 255; invalid_argument for 20 to
        // 31 (false, true, null, undefined have their own; 24 to 31 are no
        // simple values)
        static cbor simple(uint8_t value) {
            if (value >= 20 && value < 32) {
                throw invalid_argument("sgcl::encoding::cbor::simple: 20 to 31 are not simple values of their own");
            }
            cbor c;
            c._kind = kind::simple;
            c._bits = value;
            return c;
        }

        // A byte string of a copy of the bytes
        static cbor bytes(const slice<const byte>& b) noexcept {
            cbor c;
            c._kind = kind::bytes;
            c._ptr = string(std::string_view(reinterpret_cast<const char*>(b.data()), b.size())).as_slice().owner();
            return c;
        }

        static cbor array(std::initializer_list<cbor> elements) noexcept;
        static cbor array(const vector<cbor>& elements) noexcept;
        static cbor map(std::initializer_list<member> members) noexcept;
        static cbor map(const vector<member>& members) noexcept;

        // A tag of the number over the content
        static cbor tagged(uint64_t number, const cbor& content) noexcept;

        // Tag 0, the instant as RFC 3339 text in UTC; tag 1, the seconds
        // since 1970 (an integer, a float when there is a part of a second)
        static cbor date_time(const time::datetime& t) noexcept;
        static cbor epoch_time(const time::datetime& t) noexcept;

        // Tag 4, a decimal fraction: mantissa · 10^exponent
        static cbor decimal(const math::big_integer& mantissa, int64_t exponent) noexcept;

        // MessagePack's extension of the type over a copy of the data
        static cbor extension(int8_t type, const slice<const byte>& data) noexcept {
            cbor c = bytes(data);
            c._kind = kind::extension;
            c._bits = uint64_t(int64_t(type));
            return c;
        }

        // --- JSON (RFC 8949 §6) ---

        // The JSON value as CBOR: null, a boolean, an integer where the
        // number is one, else a float, a text, an array, a map of text keys
        static cbor from_json(const json& j) noexcept;

        // CBOR as JSON (§6.1): an integer or a float a number (NaN and the
        // infinities null), a byte string base64url without padding (base64
        // under tag 22, hexadecimal under tag 23), a bignum the number of its
        // digits, another tag its content, undefined and a simple value
        // null; a key that is not text its diagnostic notation
        json to_json() const noexcept;

        // --- reading and writing ---

        // One value of the bytes, and nothing after it
        static expected<cbor, error> parse(const slice<const byte>& bytes) noexcept;
        static expected<cbor, error> parse(const slice<const byte>& bytes, const options& o) noexcept;

        // One item of a stream, and no byte past it (a CBOR sequence, RFC
        // 8742, is read an item at a time): its heads a byte at a time, so a
        // buffered_reader under a socket; an item past max_size is
        // errc::limit_exceeded before it is held. In a task
        // `co_await cbor::async_parse(in)`
        static expected<cbor, error> parse(const io::reader& in);
        static expected<cbor, error> parse(const io::reader& in, const options& o);
        static async::task<expected<cbor, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<cbor, error>> async_parse(io::reader in, options o) noexcept;

        // The encoding: definite lengths, the shortest argument, a float in
        // the shortest of half, single and double that holds it; with
        // deterministic, every map's keys sorted by their encodings.
        // invalid_argument for an extension, which CBOR has not
        vector<byte> to_bytes() const;
        vector<byte> to_bytes(const style& s) const;

        // The diagnostic notation (§8): 1, -1.5, h'0102', "text", [1, 2],
        // {1: "a"}, 1(1700000000), true, null, undefined, simple(16)
        string to_string() const;

        // --- what it is ---

        SGCL_INLINE_HOT kind type() const noexcept {
            return _kind;
        }

        SGCL_INLINE_HOT bool is_null() const noexcept {
            return _kind == kind::null;
        }

        SGCL_INLINE_HOT bool is_bool() const noexcept {
            return _kind == kind::boolean;
        }

        // An integer or a float
        SGCL_INLINE_HOT bool is_number() const noexcept {
            return _kind == kind::integer || _kind == kind::floating;
        }

        SGCL_INLINE_HOT bool is_integer() const noexcept {
            return _kind == kind::integer;
        }

        SGCL_INLINE_HOT bool is_text() const noexcept {
            return _kind == kind::text;
        }

        SGCL_INLINE_HOT bool is_bytes() const noexcept {
            return _kind == kind::bytes;
        }

        SGCL_INLINE_HOT bool is_array() const noexcept {
            return _kind == kind::array;
        }

        SGCL_INLINE_HOT bool is_map() const noexcept {
            return _kind == kind::map;
        }

        SGCL_INLINE_HOT bool is_tag() const noexcept {
            return _kind == kind::tag;
        }

        optional<bool> as_bool() const noexcept {
            if (_kind != kind::boolean) {
                return nullopt;
            }
            return _bits != 0;
        }

        // An integer an int64_t holds
        optional<int64_t> as_int() const noexcept {
            if (_kind != kind::integer || _bits > uint64_t(INT64_MAX)) {
                return nullopt;
            }
            return _aux ? -1 - int64_t(_bits) : int64_t(_bits);
        }

        // A non-negative integer
        optional<uint64_t> as_uint() const noexcept {
            if (_kind != kind::integer || _aux) {
                return nullopt;
            }
            return _bits;
        }

        // An integer, or a bignum (tags 2 and 3 over bytes)
        optional<math::big_integer> as_big_integer() const noexcept;

        // A float, or an integer rounded to the nearest double
        optional<double> as_double() const noexcept {
            if (_kind == kind::floating) {
                return std::bit_cast<double>(_bits);
            }
            if (_kind == kind::integer) {
                return _aux ? -1.0 - double(_bits) : double(_bits);
            }
            return nullopt;
        }

        // A text string
        optional<string> as_string() const noexcept {
            if (_kind != kind::text) {
                return nullopt;
            }
            return _string();
        }

        // A byte string, or an extension's data
        optional<slice<const byte>> as_bytes() const noexcept {
            if (_kind != kind::bytes && _kind != kind::extension) {
                return nullopt;
            }
            return slice<const byte>(_string().as_slice());
        }

        // Tag 0 (RFC 3339 text) or tag 1 (seconds since 1970, an integer or
        // a float), or MessagePack's timestamp (extension -1): the instant,
        // in UTC or the zone of the text's offset
        optional<time::datetime> as_time() const noexcept;

        // Tag 4: the mantissa and the exponent of ten
        optional<pair<math::big_integer, int64_t>> as_decimal() const noexcept;

        // The same with a value for when there is none: c["name"].as_string("?")
        SGCL_INLINE_HOT bool as_bool(bool fallback) const noexcept {
            return as_bool().value_or(fallback);
        }

        SGCL_INLINE_HOT int64_t as_int(int64_t fallback) const noexcept {
            return as_int().value_or(fallback);
        }

        SGCL_INLINE_HOT uint64_t as_uint(uint64_t fallback) const noexcept {
            return as_uint().value_or(fallback);
        }

        SGCL_INLINE_HOT double as_double(double fallback) const noexcept {
            return as_double().value_or(fallback);
        }

        SGCL_INLINE_HOT string as_string(const string& fallback) const noexcept {
            auto s = as_string();
            return s ? *s : fallback;
        }

        optional<uint8_t> as_simple() const noexcept {
            if (_kind != kind::simple) {
                return nullopt;
            }
            return uint8_t(_bits);
        }

        // A tag's number; an extension's type as its unsigned byte; 0 else
        SGCL_INLINE_HOT uint64_t tag() const noexcept {
            return _kind == kind::tag ? _bits : _kind == kind::extension ? uint64_t(uint8_t(_bits)) : 0;
        }

        // A tag's content; null else
        cbor content() const noexcept {
            if (_kind != kind::tag) {
                return cbor();
            }
            return *static_cast<const cbor*>(_ptr.get());
        }

        // --- inside it ---

        // The elements of an array, the members of a map; 0 else
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _kind == kind::array || _kind == kind::map ? size_t(_bits) : 0;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        // An array's element at the index, or a map's value of the integer
        // key; null when there is none
        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>)
        cbor operator[](I index_or_key) const noexcept {
            if (_kind == kind::array) {
                if constexpr (std::is_signed_v<I>) {
                    if (index_or_key < 0) {
                        return cbor();
                    }
                }
                return uint64_t(index_or_key) < _bits ? _elements()[size_t(index_or_key)] : cbor();
            }
            return (*this)[cbor(index_or_key)];
        }

        // A map's value of the text key; null when there is none
        cbor operator[](const string& key) const noexcept {
            return (*this)[cbor(key)];
        }

        template<size_t N>
        cbor operator[](const char (&key)[N]) const noexcept {
            return (*this)[cbor(string(key))];
        }

        // A map's value of the key of any kind; null when there is none
        cbor operator[](const cbor& key) const noexcept;

        bool contains(const cbor& key) const noexcept;

        SGCL_INLINE_HOT slice<const cbor> elements() const noexcept {
            if (_kind != kind::array || _bits == 0) {
                return slice<const cbor>();
            }
            return slice<const cbor>(_ptr, _elements(), size_t(_bits));
        }

        slice<const member> members() const noexcept;

        // A new map with the key set to the value (in its place, or added
        // at the end); on an array, the element at an integer index (one
        // past the end appends). The value itself is unchanged
        cbor set(const cbor& key, const cbor& value) const noexcept;

        // A new map without the key; on an array, without the element at an
        // integer index
        cbor erase(const cbor& key) const noexcept;

        // A new array with the value at its end; an array of the value
        // alone made of a null
        cbor push_back(const cbor& value) const noexcept;

        // Deep equality: values of different kinds are never equal (1 is not
        // 1.0); floats by their bits, every NaN one value; maps as sets of
        // members, in any order
        friend bool operator==(const cbor& a, const cbor& b) noexcept {
            return a._equals(b);
        }

        size_t hash() const noexcept;

    private:
        friend struct detail::CborAccess;
        friend class detail::CborParser;
        friend class detail::CborWriter;

        SGCL_INLINE_HOT string _string() const noexcept {
            return sgcl::detail::StringAccess::over<string>(_ptr);
        }

        SGCL_INLINE_HOT const cbor* _elements() const noexcept {
            return static_cast<const cbor*>(_ptr.get());
        }

        const member* _members() const noexcept;
        size_t _find(const cbor& key) const noexcept;
        bool _equals(const cbor& o) const noexcept;

        tracked_ptr<const void> _ptr;   // a string's object, an array's or a map's buffer, a tag's content
        uint64_t _bits = 0;             // the value, the argument, the count, the tag's number
        kind _kind = kind::null;
        uint8_t _aux = 0;               // an integer's major type, 0 or 1
    };

    static_assert(sizeof(cbor) == 24, "a cbor is a pointer, a word and a kind");

    struct cbor::member {
        cbor key;
        cbor value;
    };

    inline const cbor::style cbor::preferred{};
    inline const cbor::style cbor::deterministic{true};

    namespace detail {
        struct CborAccess {
            // A managed buffer of n T's constructed from [first, first + n)
            template<class T>
            static tracked_ptr<const void> buffer(const T* first, size_t n) noexcept {
                tracked_ptr<const void> owner;
                T* p = json_buffer<T>(n, owner);
                for (size_t i = 0; i < n; ++i) {
                    Maker<T>::construct(p + i, first[i]);
                }
                return owner;
            }

            template<class T>
            static tracked_ptr<const void> buffer_moved(T* first, size_t n) noexcept {
                tracked_ptr<const void> owner;
                T* p = json_buffer<T>(n, owner);
                for (size_t i = 0; i < n; ++i) {
                    Maker<T>::construct(p + i, std::move(first[i]));
                }
                return owner;
            }

            static cbor array_of(tracked_ptr<const void> owner, size_t n) noexcept {
                cbor c;
                c._kind = cbor::kind::array;
                c._bits = n;
                c._ptr = std::move(owner);
                return c;
            }

            static cbor map_of(tracked_ptr<const void> owner, size_t n) noexcept {
                cbor c;
                c._kind = cbor::kind::map;
                c._bits = n;
                c._ptr = std::move(owner);
                return c;
            }

            static cbor integer(uint64_t argument, bool negative) noexcept {
                cbor c;
                c._kind = cbor::kind::integer;
                c._bits = argument;
                c._aux = negative;
                return c;
            }

            static cbor of_string(cbor::kind k, const string& s) noexcept {
                cbor c;
                c._kind = k;
                c._ptr = s.as_slice().owner();
                return c;
            }

            static cbor simple(uint64_t v) noexcept {
                cbor c;
                switch (v) {
                    case 20: return cbor(false);
                    case 21: return cbor(true);
                    case 22: return cbor();
                    case 23: return cbor::undefined();
                    default:
                        c._kind = cbor::kind::simple;
                        c._bits = v;
                        return c;
                }
            }

            static cbor extension_of(int8_t type, const string& data) noexcept {
                cbor c = of_string(cbor::kind::extension, data);
                c._bits = uint64_t(int64_t(type));
                return c;
            }

            // The map of the pairs of values[first...], every key once (an
            // error at start for a key given twice, unless the options let
            // the last one win in the place of the first)
            static expected<cbor, error> map_from(vector<cbor>& values, size_t first, size_t pairs, const cbor::options& o, size_t start) noexcept {
            if (pairs == 0) {
                return CborAccess::map_of(nullptr, 0);
            }
            tracked_ptr<const void> owner;
            cbor::member* ms = json_buffer<cbor::member>(pairs, owner);
            for (size_t i = 0; i < pairs; ++i) {
                Maker<cbor::member>::construct(ms + i, cbor::member{std::move(values[first + 2 * i]), std::move(values[first + 2 * i + 1])});
            }
            if (pairs > 1) {
                // a key given twice: found by a table of the keys' hashes
                // (open addressing, twice the pairs at least)
                size_t cap = std::bit_ceil(pairs * 2);
                uint32_t local[128];
                std::vector<uint32_t> heap;
                uint32_t* slots = local;
                if (cap > 128) {
                    heap.assign(cap, 0);
                    slots = heap.data();
                } else {
                    std::fill_n(local, cap, 0u);
                }
                size_t mask = cap - 1;
                std::vector<bool> dropped;
                for (size_t i = 0; i < pairs; ++i) {
                    size_t sl = ms[i].key.hash() & mask;
                    bool dup = false;
                    while (uint32_t e = slots[sl]) {
                        if (ms[e - 1].key == ms[i].key) {
                            if (!o.allow_duplicate_keys) {
                                return unexpected<error>(error(errc::duplicate_key, start, string("a key given twice in a map: ") + ms[i].key.to_string()));
                            }
                            // the last one wins, in the place of the first
                            ms[e - 1].value = ms[i].value;
                            if (dropped.empty()) {
                                dropped.assign(pairs, false);
                            }
                            dropped[i] = true;
                            dup = true;
                            break;
                        }
                        sl = (sl + 1) & mask;
                    }
                    if (!dup) {
                        slots[sl] = uint32_t(i + 1);
                    }
                }
                if (!dropped.empty()) {
                    vector<cbor::member> kept;
                    for (size_t i = 0; i < pairs; ++i) {
                        if (!dropped[i]) {
                            kept.push_back(ms[i]);
                        }
                    }
                    return CborAccess::map_of(CborAccess::buffer_moved(kept.data(), kept.size()), kept.size());
                }
            }
            return CborAccess::map_of(std::move(owner), pairs);
            }

            SGCL_INLINE_HOT static uint64_t bits(const cbor& c) noexcept {
                return c._bits;
            }

            SGCL_INLINE_HOT static bool negative(const cbor& c) noexcept {
                return c._aux != 0;
            }

            SGCL_INLINE_HOT static string text(const cbor& c) noexcept {
                return c._string();
            }

            SGCL_INLINE_HOT static const cbor* elements(const cbor& c) noexcept {
                return c._elements();
            }

            SGCL_INLINE_HOT static const cbor::member* members(const cbor& c) noexcept {
                return c._members();
            }
        };

        // A double as half precision when it is one exactly: the 16 bits,
        // or false
        inline bool cbor_half(double d, uint16_t& out) noexcept {
            float f = float(d);
            if (double(f) != d && !std::isnan(d)) {
                return false;
            }
            uint32_t x = std::bit_cast<uint32_t>(f);
            uint16_t sign = uint16_t((x >> 16) & 0x8000);
            int exp = int((x >> 23) & 0xFF);
            uint32_t mant = x & 0x7FFFFF;
            if (exp == 0xFF) {
                if (mant) {
                    out = 0x7E00;   // NaN: the one quiet NaN of half precision
                    return true;
                }
                out = uint16_t(sign | 0x7C00);
                return true;
            }
            if (exp == 0 && mant == 0) {
                out = sign;
                return true;
            }
            int e = exp - 127;
            if (e >= -14 && e <= 15) {
                if (mant & 0x1FFF) {
                    return false;
                }
                out = uint16_t(sign | uint16_t((e + 15) << 10) | uint16_t(mant >> 13));
                return true;
            }
            if (e >= -24 && e < -14) {
                // subnormal: the implicit one and the mantissa shifted
                uint32_t full = mant | 0x800000;
                int shift = 13 + (-14 - e);
                if (full & ((1u << shift) - 1)) {
                    return false;
                }
                out = uint16_t(sign | uint16_t(full >> shift));
                return true;
            }
            return false;
        }

        // Half precision as a double (RFC 8949 appendix D)
        inline double cbor_from_half(uint16_t h) noexcept {
            int exp = (h >> 10) & 0x1F;
            int mant = h & 0x3FF;
            double v;
            if (exp == 0) {
                v = std::ldexp(mant, -24);
            } else if (exp != 31) {
                v = std::ldexp(mant + 1024, exp - 25);
            } else {
                v = mant == 0 ? INFINITY : NAN;
            }
            return (h & 0x8000) ? -v : v;
        }

        // The writer of the encoding: an explicit stack, no recursion
        class CborWriter {
        public:
            std::string out;

            void head(uint8_t major, uint64_t arg) noexcept {
                uint8_t m = uint8_t(major << 5);
                if (arg < 24) {
                    out += char(m | arg);
                } else if (arg <= 0xFF) {
                    out += char(m | 24);
                    out += char(arg);
                } else if (arg <= 0xFFFF) {
                    out += char(m | 25);
                    put(arg, 2);
                } else if (arg <= 0xFFFFFFFFull) {
                    out += char(m | 26);
                    put(arg, 4);
                } else {
                    out += char(m | 27);
                    put(arg, 8);
                }
            }

            void put(uint64_t v, int n) noexcept {
                for (int i = n - 1; i >= 0; --i) {
                    out += char(uint8_t(v >> (8 * i)));
                }
            }

            void floating(double d) noexcept {
                uint16_t h;
                if (cbor_half(d, h)) {
                    out += char(0xF9);
                    put(h, 2);
                } else if (double(float(d)) == d) {
                    out += char(0xFA);
                    put(std::bit_cast<uint32_t>(float(d)), 4);
                } else {
                    out += char(0xFB);
                    put(std::bit_cast<uint64_t>(d), 8);
                }
            }

            // One value, its elements after it
            void write(const cbor& top, bool sort_keys);
        };
    }

    // --- making ---

    inline cbor::cbor(const math::big_integer& v) noexcept {
        _kind = kind::integer;
        if (v.sign() >= 0) {
            if (v.bit_length() <= 64) {
                auto b = v.to_bytes();
                uint64_t x = 0;
                for (auto c : b) {
                    x = x << 8 | uint8_t(c);
                }
                _bits = x;
                return;
            }
            *this = tagged(2, bytes(v.to_bytes()));
            return;
        }
        // -1 - v, the argument of major type 1
        math::big_integer n = -v - math::big_integer(1);
        if (n.bit_length() <= 64) {
            auto b = n.to_bytes();
            uint64_t x = 0;
            for (auto c : b) {
                x = x << 8 | uint8_t(c);
            }
            _bits = x;
            _aux = 1;
            return;
        }
        *this = tagged(3, bytes(n.to_bytes()));
    }

    inline cbor cbor::array(std::initializer_list<cbor> elements) noexcept {
        if (elements.size() == 0) {
            return detail::CborAccess::array_of(nullptr, 0);
        }
        return detail::CborAccess::array_of(detail::CborAccess::buffer(elements.begin(), elements.size()), elements.size());
    }

    inline cbor cbor::array(const vector<cbor>& elements) noexcept {
        if (elements.empty()) {
            return detail::CborAccess::array_of(nullptr, 0);
        }
        return detail::CborAccess::array_of(detail::CborAccess::buffer(elements.data(), elements.size()), elements.size());
    }

    inline cbor cbor::map(std::initializer_list<member> members) noexcept {
        if (members.size() == 0) {
            return detail::CborAccess::map_of(nullptr, 0);
        }
        return detail::CborAccess::map_of(detail::CborAccess::buffer(members.begin(), members.size()), members.size());
    }

    inline cbor cbor::map(const vector<member>& members) noexcept {
        if (members.empty()) {
            return detail::CborAccess::map_of(nullptr, 0);
        }
        return detail::CborAccess::map_of(detail::CborAccess::buffer(members.data(), members.size()), members.size());
    }

    inline cbor cbor::tagged(uint64_t number, const cbor& content) noexcept {
        cbor c;
        c._kind = kind::tag;
        c._bits = number;
        c._ptr = detail::CborAccess::buffer(&content, 1);
        return c;
    }

    inline cbor cbor::date_time(const time::datetime& t) noexcept {
        return tagged(0, cbor(t.utc().format(time::rfc3339_nano)));
    }

    inline cbor cbor::epoch_time(const time::datetime& t) noexcept {
        int64_t ns = t.unix_nano();
        if (ns % 1000000000 == 0) {
            return tagged(1, cbor(t.unix()));
        }
        return tagged(1, cbor(double(t.unix()) + double(t.nanosecond()) / 1e9));
    }

    inline cbor cbor::decimal(const math::big_integer& mantissa, int64_t exponent) noexcept {
        return tagged(4, array({cbor(exponent), cbor(mantissa)}));
    }

    inline const cbor::member* cbor::_members() const noexcept {
        return static_cast<const member*>(_ptr.get());
    }

    inline slice<const cbor::member> cbor::members() const noexcept {
        if (_kind != kind::map || _bits == 0) {
            return slice<const member>();
        }
        return slice<const member>(_ptr, _members(), size_t(_bits));
    }

    inline size_t cbor::_find(const cbor& key) const noexcept {
        if (_kind != kind::map) {
            return size_t(-1);
        }
        const member* m = _members();
        for (size_t i = 0; i < _bits; ++i) {
            if (m[i].key == key) {
                return i;
            }
        }
        return size_t(-1);
    }

    inline cbor cbor::operator[](const cbor& key) const noexcept {
        size_t i = _find(key);
        return i == size_t(-1) ? cbor() : _members()[i].value;
    }

    inline bool cbor::contains(const cbor& key) const noexcept {
        return _find(key) != size_t(-1);
    }

    inline cbor cbor::set(const cbor& key, const cbor& value) const noexcept {
        if (_kind == kind::array) {
            auto i = key.as_uint();
            if (!i || *i > _bits) {
                return *this;
            }
            vector<cbor> v(elements().begin(), elements().end());
            if (*i == _bits) {
                v.push_back(value);
            } else {
                v[size_t(*i)] = value;
            }
            return array(v);
        }
        if (_kind != kind::map) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        size_t i = _find(key);
        if (i == size_t(-1)) {
            v.push_back(member{key, value});
        } else {
            v[i].value = value;
        }
        return map(v);
    }

    inline cbor cbor::erase(const cbor& key) const noexcept {
        if (_kind == kind::array) {
            auto i = key.as_uint();
            if (!i || *i >= _bits) {
                return *this;
            }
            vector<cbor> v(elements().begin(), elements().end());
            v.erase(v.begin() + ptrdiff_t(*i));
            return array(v);
        }
        size_t i = _find(key);
        if (i == size_t(-1)) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        v.erase(v.begin() + ptrdiff_t(i));
        return map(v);
    }

    inline cbor cbor::push_back(const cbor& value) const noexcept {
        if (_kind == kind::null) {
            return array({value});
        }
        if (_kind != kind::array) {
            return *this;
        }
        vector<cbor> v(elements().begin(), elements().end());
        v.push_back(value);
        return array(v);
    }

    // --- values ---

    inline optional<math::big_integer> cbor::as_big_integer() const noexcept {
        if (_kind == kind::integer) {
            math::big_integer m(_bits);
            return _aux ? -m - math::big_integer(1) : m;
        }
        if (_kind == kind::tag && (_bits == 2 || _bits == 3)) {
            auto b = content().as_bytes();
            if (!b || content().type() != kind::bytes) {
                return nullopt;
            }
            auto m = math::big_integer::from_bytes(*b);
            return _bits == 2 ? m : -m - math::big_integer(1);
        }
        return nullopt;
    }

    inline optional<time::datetime> cbor::as_time() const noexcept {
        if (_kind == kind::extension && int8_t(_bits) == -1) {
            // MessagePack's timestamp: 32 bits of seconds; 30 of nanoseconds
            // and 34 of seconds; 32 of nanoseconds and 64 of signed seconds
            auto d = _string();
            auto p = reinterpret_cast<const uint8_t*>(d.data());
            auto word = [&](size_t at, size_t n) {
                uint64_t v = 0;
                for (size_t i = 0; i < n; ++i) {
                    v = v << 8 | p[at + i];
                }
                return v;
            };
            int64_t seconds;
            uint64_t ns;
            if (d.size() == 4) {
                seconds = int64_t(word(0, 4));
                ns = 0;
            } else if (d.size() == 8) {
                uint64_t w = word(0, 8);
                ns = w >> 34;
                seconds = int64_t(w & 0x3FFFFFFFFull);
            } else if (d.size() == 12) {
                ns = word(0, 4);
                seconds = int64_t(word(4, 8));
            } else {
                return nullopt;
            }
            if (ns > 999999999) {
                return nullopt;
            }
            if (seconds > -9223372036 && seconds < 9223372036) {
                return time::datetime::from_unix_nano(seconds * 1000000000 + int64_t(ns), time::zone::utc());
            }
            return time::datetime::from_unix(seconds, time::zone::utc());
        }
        if (_kind != kind::tag) {
            return nullopt;
        }
        cbor c = content();
        if (_bits == 0) {
            auto s = c.as_string();
            if (!s) {
                return nullopt;
            }
            auto t = time::datetime::parse(*s, time::rfc3339);
            if (!t) {
                return nullopt;
            }
            return *t;
        }
        if (_bits == 1) {
            if (auto i = c.as_int()) {
                return time::datetime::from_unix(*i, time::zone::utc());
            }
            if (c.type() == kind::floating) {
                double d = *c.as_double();
                if (!std::isfinite(d)) {
                    return nullopt;
                }
                double s = std::floor(d);
                if (s < -9.2e18 || s > 9.2e18) {
                    return time::datetime::from_unix(s < 0 ? INT64_MIN : INT64_MAX, time::zone::utc());
                }
                int64_t ns = int64_t(std::llround((d - s) * 1e9));
                int64_t secs = int64_t(s);
                if (secs > -9223372036 && secs < 9223372036) {
                    return time::datetime::from_unix_nano(secs * 1000000000 + ns, time::zone::utc());
                }
                return time::datetime::from_unix(secs, time::zone::utc());
            }
            if (c.type() == kind::integer) {
                return time::datetime::from_unix(detail::CborAccess::negative(c) ? INT64_MIN : INT64_MAX, time::zone::utc());
            }
        }
        return nullopt;
    }

    inline optional<pair<math::big_integer, int64_t>> cbor::as_decimal() const noexcept {
        if (_kind != kind::tag || _bits != 4) {
            return nullopt;
        }
        cbor c = content();
        if (c.type() != kind::array || c.size() != 2) {
            return nullopt;
        }
        auto e = c[0].as_int();
        auto m = c[1].as_big_integer();
        if (!e || !m || c[0].type() != kind::integer) {
            return nullopt;
        }
        return pair<math::big_integer, int64_t>(*m, *e);
    }

    // --- equality and hash: an explicit stack, no recursion ---

    inline bool cbor::_equals(const cbor& top) const noexcept {
        std::vector<pair<const cbor*, const cbor*>> todo;
        todo.push_back({this, &top});
        while (!todo.empty()) {
            auto [a, b] = todo.back();
            todo.pop_back();
            if (a->_kind != b->_kind) {
                return false;
            }
            switch (a->_kind) {
                case kind::null:
                case kind::undefined:
                    break;
                case kind::boolean:
                case kind::simple:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    break;
                case kind::integer:
                    if (a->_bits != b->_bits || a->_aux != b->_aux) {
                        return false;
                    }
                    break;
                case kind::floating: {
                    double x = std::bit_cast<double>(a->_bits), y = std::bit_cast<double>(b->_bits);
                    if (a->_bits != b->_bits && !(std::isnan(x) && std::isnan(y))) {
                        return false;
                    }
                    break;
                }
                case kind::bytes:
                case kind::text:
                case kind::extension:
                    if (a->_bits != b->_bits || a->_string() != b->_string()) {
                        return false;
                    }
                    break;
                case kind::array:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    for (size_t i = 0; i < a->_bits; ++i) {
                        todo.push_back({a->_elements() + i, b->_elements() + i});
                    }
                    break;
                case kind::map:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    for (size_t i = 0; i < a->_bits; ++i) {
                        const member& m = a->_members()[i];
                        size_t j = b->_find(m.key);
                        if (j == size_t(-1)) {
                            return false;
                        }
                        todo.push_back({&m.value, &b->_members()[j].value});
                    }
                    break;
                case kind::tag:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    todo.push_back({a->_elements(), b->_elements()});
                    break;
            }
        }
        return true;
    }

    inline size_t cbor::hash() const noexcept {
        if (_kind != kind::array && _kind != kind::map && _kind != kind::tag) {
            // a scalar: no stack
            uint64_t x = uint64_t(_kind) * 0x9E3779B97F4A7C15ull;
            if (_kind == kind::floating) {
                double d = std::bit_cast<double>(_bits);
                x ^= std::isnan(d) ? 0x7FF8000000000000ull : _bits;
            } else if (_kind == kind::bytes || _kind == kind::text || _kind == kind::extension) {
                x ^= _string().hash() + _bits;
            } else {
                x ^= _bits + _aux;
            }
            return size_t(detail::mix(x));
        }
        uint64_t h = 0;
        std::vector<const cbor*> todo{this};
        while (!todo.empty()) {
            const cbor* c = todo.back();
            todo.pop_back();
            uint64_t x = uint64_t(c->_kind) * 0x9E3779B97F4A7C15ull;
            switch (c->_kind) {
                case kind::floating: {
                    double d = std::bit_cast<double>(c->_bits);
                    x ^= std::isnan(d) ? 0x7FF8000000000000ull : c->_bits;
                    break;
                }
                case kind::bytes:
                case kind::text:
                case kind::extension:
                    x ^= c->_string().hash() + c->_bits;
                    break;
                case kind::array:
                    x ^= c->_bits;
                    for (size_t i = 0; i < c->_bits; ++i) {
                        todo.push_back(c->_elements() + i);
                    }
                    break;
                case kind::map: {
                    // the members in any order: their hashes summed
                    uint64_t sum = 0;
                    for (size_t i = 0; i < c->_bits; ++i) {
                        const member& m = c->_members()[i];
                        sum += m.key.hash() * 31 + m.value.hash();
                    }
                    x ^= sum + c->_bits;
                    break;
                }
                case kind::tag:
                    x ^= c->_bits;
                    todo.push_back(c->_elements());
                    break;
                default:
                    x ^= c->_bits + c->_aux;
                    break;
            }
            h = detail::mix(h ^ x);
        }
        return size_t(h);
    }

    // --- writing ---

    namespace detail {
        inline void CborWriter::write(const cbor& top, bool sort_keys) {
            // One pass, no recursion. A map whose keys are sorted (§4.2.1) is
            // written in its own order with the places of its members kept;
            // once whole, its members are put in the order of their keys'
            // bytes where they lie, so every byte moves once a sorted map
            // that holds it, and no key is encoded twice
            struct Frame {
                const cbor* items;            // an array's elements, a tag's content
                const cbor::member* members;  // a map's
                size_t n;
                size_t at;
                size_t region;                // where a sorted map's members start in out
                std::vector<size_t> places;   // each member's key, then its value
            };
            std::vector<Frame> stack;
            auto one = [&](const cbor& c) {
                switch (c.type()) {
                    case cbor::kind::null: out += char(0xF6); break;
                    case cbor::kind::undefined: out += char(0xF7); break;
                    case cbor::kind::boolean: out += char(CborAccess::bits(c) ? 0xF5 : 0xF4); break;
                    case cbor::kind::simple: head(7, CborAccess::bits(c)); break;
                    case cbor::kind::integer: head(CborAccess::negative(c) ? 1 : 0, CborAccess::bits(c)); break;
                    case cbor::kind::floating: floating(*c.as_double()); break;
                    case cbor::kind::bytes:
                    case cbor::kind::text: {
                        auto s = CborAccess::text(c);
                        head(c.type() == cbor::kind::bytes ? 2 : 3, s.size());
                        out.append(s.data(), s.size());
                        break;
                    }
                    case cbor::kind::array:
                        head(4, c.size());
                        if (c.size()) {
                            stack.push_back({CborAccess::elements(c), nullptr, c.size(), 0, 0, {}});
                        }
                        break;
                    case cbor::kind::map:
                        head(5, c.size());
                        if (c.size()) {
                            stack.push_back({nullptr, CborAccess::members(c), c.size(), 0, out.size(), {}});
                        }
                        break;
                    case cbor::kind::tag:
                        head(6, c.tag());
                        stack.push_back({CborAccess::elements(c), nullptr, 1, 0, 0, {}});
                        break;
                    case cbor::kind::extension:
                        throw invalid_argument("sgcl::encoding::cbor::to_bytes: a MessagePack extension, which CBOR has not");
                }
            };
            one(top);
            std::string moved;
            while (!stack.empty()) {
                Frame& f = stack.back();
                if (f.items) {
                    if (f.at == f.n) {
                        stack.pop_back();
                        continue;
                    }
                    const cbor& c = f.items[f.at++];
                    one(c);
                    continue;
                }
                if (f.at == 2 * f.n) {
                    if (sort_keys && f.n > 1) {
                        f.places.push_back(out.size());
                        size_t n = f.n;
                        std::vector<size_t> order(n);
                        for (size_t i = 0; i < n; ++i) {
                            order[i] = i;
                        }
                        const char* base = out.data();
                        auto key = [&](size_t i) {
                            return std::string_view(base + f.places[2 * i], f.places[2 * i + 1] - f.places[2 * i]);
                        };
                        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return key(a) < key(b); });
                        moved.assign(out, f.region, out.size() - f.region);
                        size_t w = f.region;
                        for (size_t i : order) {
                            size_t from = f.places[2 * i] - f.region;
                            size_t len = f.places[2 * i + 2] - f.places[2 * i];
                            out.replace(w, len, moved, from, len);
                            w += len;
                        }
                    }
                    stack.pop_back();
                    continue;
                }
                if (sort_keys) {
                    f.places.push_back(out.size());
                }
                const cbor::member& m = f.members[f.at / 2];
                const cbor& c = f.at % 2 == 0 ? m.key : m.value;
                ++f.at;
                one(c);
            }
        }
    }

    inline vector<byte> cbor::to_bytes() const {
        return to_bytes(preferred);
    }

    inline vector<byte> cbor::to_bytes(const style& s) const {
        detail::CborWriter w;
        w.write(*this, s.sort_keys);
        vector<byte> out;
        sgcl::detail::VectorOverwrite::resize(out, w.out.size());
        sgcl::detail::copy_bytes(out.data(), w.out.data(), w.out.size());
        return out;
    }

    // --- reading ---

    namespace detail {
        class CborParser {
        public:
            CborParser(const uint8_t* p, size_t n, const cbor::options& o) noexcept
            : _p(p), _n(n), _o(o) {
            }

            expected<cbor, error> run() noexcept {
                struct Frame {
                    uint8_t major;          // 4 array, 5 map, 6 tag
                    bool indefinite;
                    uint64_t left;          // items still to come (twice the pairs of a map)
                    uint64_t tag;
                    size_t first;           // where its items start in the values
                    size_t start;           // its offset, for the error
                };
                std::vector<Frame> stack;
                vector<cbor> values;        // the items read, waiting for their container
                for (;;) {
                    // the end of the containers that are complete
                    while (!stack.empty()) {
                        Frame& f = stack.back();
                        if (f.indefinite) {
                            if (_at < _n && _p[_at] == 0xFF) {
                                if (f.major == 5 && (values.size() - f.first) % 2) {
                                    return _fail(errc::syntax, _at, "a map of indefinite length ending after a key without its value");
                                }
                                ++_at;
                            } else {
                                break;
                            }
                        } else if (f.left) {
                            break;
                        }
                        Frame done = f;
                        stack.pop_back();
                        auto r = _close(done.major, done.tag, values, done.first, done.start);
                        if (!r) {
                            return unexpected<error>(std::move(r.error()));
                        }
                        values.resize(done.first);
                        values.push_back(std::move(*r));
                        _count(stack);
                    }
                    if (stack.empty() && !values.empty()) {
                        break;
                    }
                    // one item
                    size_t start = _at;
                    if (_at >= _n) {
                        return _fail(errc::unexpected_end, _at, "unexpected end of input");
                    }
                    uint8_t ib = _p[_at++];
                    uint8_t major = ib >> 5, info = ib & 31;
                    uint64_t arg = 0;
                    bool indefinite = false;
                    if (info < 24) {
                        arg = info;
                    } else if (info <= 27) {
                        size_t k = size_t(1) << (info - 24);
                        if (_n - _at < k) {
                            return _fail(errc::unexpected_end, _at, "unexpected end of input inside an argument");
                        }
                        for (size_t i = 0; i < k; ++i) {
                            arg = arg << 8 | _p[_at + i];
                        }
                        _at += k;
                    } else if (info == 31 && major >= 2 && major <= 5) {
                        indefinite = true;
                    } else {
                        return _fail(errc::syntax, start, info == 31 ? "a break where no indefinite length ends" : "the reserved additional information 28 to 30");
                    }
                    switch (major) {
                        case 0:
                        case 1:
                            values.push_back(CborAccess::integer(arg, major == 1));
                            break;
                        case 2:
                        case 3: {
                            if (!indefinite) {
                                if (arg > _n - _at) {
                                    return _fail(errc::unexpected_end, start, "a string longer than the input");
                                }
                                std::string_view v(reinterpret_cast<const char*>(_p + _at), size_t(arg));
                                _at += size_t(arg);
                                if (major == 3 && !_o.allow_invalid_utf8 && !utf8_text_valid(v.data(), v.data() + v.size())) {
                                    return _fail(errc::invalid_utf8, start, "invalid UTF-8 in a text string");
                                }
                                values.push_back(CborAccess::of_string(major == 2 ? cbor::kind::bytes : cbor::kind::text, string(v)));
                                break;
                            }
                            std::string s;
                            if (indefinite) {
                                // chunks of the same major type, definite, until a break
                                for (;;) {
                                    if (_at >= _n) {
                                        return _fail(errc::unexpected_end, _at, "unexpected end of input inside a string of indefinite length");
                                    }
                                    if (_p[_at] == 0xFF) {
                                        ++_at;
                                        break;
                                    }
                                    size_t cs = _at;
                                    uint8_t cb = _p[_at++];
                                    if ((cb >> 5) != major || (cb & 31) == 31) {
                                        return _fail(errc::syntax, cs, "a chunk of a string of indefinite length that is not a definite string of its type");
                                    }
                                    uint64_t len;
                                    if (!_argument(cb & 31, len, cs)) {
                                        return unexpected<error>(std::move(_e));
                                    }
                                    if (len > _n - _at) {
                                        return _fail(errc::unexpected_end, cs, "a string longer than the input");
                                    }
                                    s.append(reinterpret_cast<const char*>(_p + _at), size_t(len));
                                    _at += size_t(len);
                                }
                            } else {
                                if (arg > _n - _at) {
                                    return _fail(errc::unexpected_end, start, "a string longer than the input");
                                }
                                s.assign(reinterpret_cast<const char*>(_p + _at), size_t(arg));
                                _at += size_t(arg);
                            }
                            if (major == 3 && !_o.allow_invalid_utf8 && !utf8_text_valid(s.data(), s.data() + s.size())) {
                                return _fail(errc::invalid_utf8, start, "invalid UTF-8 in a text string");
                            }
                            values.push_back(CborAccess::of_string(major == 2 ? cbor::kind::bytes : cbor::kind::text, string(s)));
                            break;
                        }
                        case 4:
                        case 5:
                        case 6: {
                            if (stack.size() + 1 > _o.max_depth) {
                                return _fail(errc::depth_limit, start, "items nested deeper than max_depth");
                            }
                            uint64_t items = major == 6 ? 1 : major == 5 ? arg * 2 : arg;
                            // each item takes a byte at least: a count past the
                            // input is refused before anything is held
                            if (!indefinite && major != 6 && (arg > _n - _at || (major == 5 && arg > (_n - _at) / 2))) {
                                return _fail(errc::unexpected_end, start, "more items than the input holds");
                            }
                            stack.push_back({major, indefinite, items, arg, values.size(), start});
                            continue;
                        }
                        case 7:
                            if (info < 24) {
                                values.push_back(CborAccess::simple(arg));
                            } else if (info == 24) {
                                if (arg < 32) {
                                    return _fail(errc::syntax, start, "a simple value under 32 in two bytes");
                                }
                                values.push_back(CborAccess::simple(arg));
                            } else if (info == 25) {
                                values.push_back(cbor(cbor_from_half(uint16_t(arg))));
                            } else if (info == 26) {
                                values.push_back(cbor(double(std::bit_cast<float>(uint32_t(arg)))));
                            } else if (info == 27) {
                                values.push_back(cbor(std::bit_cast<double>(arg)));
                            }
                            break;
                    }
                    _count(stack);
                }
                if (_at != _n) {
                    return _fail(errc::syntax, _at, "bytes after the value");
                }
                return std::move(values[0]);
            }

        private:
            const uint8_t* _p;
            size_t _n;
            size_t _at = 0;
            const cbor::options& _o;
            error _e;

            template<class S>
            static void _count(S& stack) noexcept {
                if (!stack.empty() && !stack.back().indefinite) {
                    --stack.back().left;
                }
            }

            bool _argument(uint8_t info, uint64_t& arg, size_t start) noexcept {
                arg = 0;
                if (info < 24) {
                    arg = info;
                    return true;
                }
                if (info > 27) {
                    _e = error(errc::syntax, start, string("the reserved additional information 28 to 30"));
                    return false;
                }
                size_t k = size_t(1) << (info - 24);
                if (_n - _at < k) {
                    _e = error(errc::unexpected_end, _at, string("unexpected end of input inside an argument"));
                    return false;
                }
                for (size_t i = 0; i < k; ++i) {
                    arg = arg << 8 | _p[_at + i];
                }
                _at += k;
                return true;
            }

            unexpected<error> _fail(errc code, size_t at, const char* text) noexcept {
                return unexpected<error>(error(code, at, string(text)));
            }

            // The container of values[first...]
            expected<cbor, error> _close(uint8_t major, uint64_t tag, vector<cbor>& values, size_t first, size_t start) noexcept {
                size_t n = values.size() - first;
                if (major == 6) {
                    return cbor::tagged(tag, values[first]);
                }
                if (major == 4) {
                    if (n == 0) {
                        return CborAccess::array_of(nullptr, 0);
                    }
                    return CborAccess::array_of(CborAccess::buffer_moved(values.data() + first, n), n);
                }
                return CborAccess::map_from(values, first, n / 2, _o, start);
            }
        };
    }

    inline expected<cbor, cbor::error> cbor::parse(const slice<const byte>& bytes) noexcept {
        return parse(bytes, options());
    }

    inline expected<cbor, cbor::error> cbor::parse(const slice<const byte>& bytes, const options& o) noexcept {
        return detail::CborParser(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), o).run();
    }

    inline expected<cbor, cbor::error> cbor::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<cbor, cbor::error> cbor::parse(const io::reader& in, const options& o) {
        auto bytes = detail::read_item(in, detail::CborItemScan(o.max_depth), o.max_size);
        if (!bytes) {
            return unexpected<error>(std::move(bytes.error()));
        }
        return parse(*bytes, o);
    }

    inline async::task<expected<cbor, cbor::error>> cbor::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<cbor, cbor::error>> cbor::async_parse(io::reader in, options o) noexcept {
        auto bytes = co_await detail::async_read_item(std::move(in), detail::CborItemScan(o.max_depth), o.max_size);
        if (!bytes) {
            co_return unexpected<error>(std::move(bytes.error()));
        }
        co_return parse(*bytes, o);
    }

    // --- JSON ---

    inline cbor cbor::from_json(const json& top) noexcept {
        struct Frame {
            const json* src;
            size_t next;
            vector<cbor> items;          // an array's elements, a map's values
        };
        vector<Frame> stack;
        cbor result;
        auto scalar = [](const json& j, bool& container) -> cbor {
            container = false;
            switch (j.type()) {
                case json::kind::null: return cbor();
                case json::kind::boolean: return cbor(*j.as_bool());
                case json::kind::number:
                    if (auto i = j.as_int()) {
                        return cbor(*i);
                    }
                    if (auto u = j.as_uint()) {
                        return cbor(*u);
                    }
                    // an integer past 64 bits kept as its literal: a bignum
                    if (auto t = j.number_text()) {
                        if (auto b = math::big_integer::parse(*t)) {
                            return cbor(*b);
                        }
                    }
                    if (auto d = j.as_double()) {
                        return cbor(*d);
                    }
                    return cbor();
                case json::kind::string: return cbor(*j.as_string());
                default:
                    container = true;
                    return cbor();
            }
        };
        bool container;
        cbor first = scalar(top, container);
        if (!container) {
            return first;
        }
        stack.push_back(Frame{&top, 0, {}});
        while (!stack.empty()) {
            Frame& f = stack.back();
            size_t n = f.src->size();
            if (f.next == n) {
                cbor made;
                if (f.src->type() == json::kind::array) {
                    made = array(f.items);
                } else {
                    vector<member> ms;
                    auto src = f.src->members();
                    for (size_t i = 0; i < n; ++i) {
                        ms.push_back(member{cbor(src[i].key), f.items[i]});
                    }
                    made = map(ms);
                }
                stack.pop_back();
                if (stack.empty()) {
                    result = made;
                } else {
                    stack.back().items.push_back(made);
                }
                continue;
            }
            const json& child = f.src->type() == json::kind::array ? f.src->elements()[f.next] : f.src->members()[f.next].value;
            ++f.next;
            cbor c = scalar(child, container);
            if (container) {
                const json* p = &child;
                stack.push_back(Frame{p, 0, {}});
            } else {
                f.items.push_back(c);
            }
        }
        return result;
    }

    inline json cbor::to_json() const noexcept {
        struct Frame {
            const cbor* src;
            size_t next;
            uint64_t hint;               // the tag 21, 22 or 23 over it
            vector<json> items;
        };
        vector<Frame> stack;
        json result;
        auto scalar = [](const cbor& c, uint64_t hint, bool& container) -> json {
            container = false;
            switch (c.type()) {
                case kind::boolean: return json(*c.as_bool());
                case kind::integer:
                    if (auto i = c.as_int()) {
                        return json(*i);
                    }
                    if (auto u = c.as_uint()) {
                        return json(*u);
                    }
                    return json::parse(c.as_big_integer()->to_string()).value_or(json(*c.as_double()));
                case kind::floating: {
                    double d = *c.as_double();
                    return std::isfinite(d) ? json(d) : json();
                }
                case kind::text: return json(*c.as_string());
                case kind::bytes:
                case kind::extension: {
                    auto b = *c.as_bytes();
                    return json(hint == 23 ? hex::encode(b) : hint == 22 ? base64::standard.encode(b) : base64::raw_url.encode(b));
                }
                case kind::tag:
                    if (c.tag() == 2 || c.tag() == 3) {
                        if (auto big = c.as_big_integer()) {
                            return json::parse(big->to_string()).value_or(json());
                        }
                    }
                    container = true;
                    return json();
                case kind::array:
                case kind::map:
                    container = true;
                    return json();
                default:
                    return json();
            }
        };
        bool container;
        json first = scalar(*this, 0, container);
        if (!container) {
            return first;
        }
        stack.push_back(Frame{this, 0, 0, {}});
        while (!stack.empty()) {
            Frame& f = stack.back();
            const cbor& src = *f.src;
            size_t n = src.type() == kind::tag ? 1 : src.size();
            if (f.next == n) {
                json made;
                if (src.type() == kind::tag) {
                    made = f.items[0];
                } else if (src.type() == kind::array) {
                    made = json::array(f.items);
                } else {
                    json::builder b;
                    auto ms = src.members();
                    for (size_t i = 0; i < n; ++i) {
                        auto key = ms[i].key.as_string();
                        b.set(key ? *key : ms[i].key.to_string(), f.items[i]);
                    }
                    made = n ? b.build() : json::object({});
                }
                stack.pop_back();
                if (stack.empty()) {
                    result = made;
                } else {
                    stack.back().items.push_back(made);
                }
                continue;
            }
            const cbor* child = src.type() == kind::tag ? src._elements() : src.type() == kind::array ? src._elements() + f.next : &src._members()[f.next].value;
            ++f.next;
            uint64_t hint = src.type() == kind::tag && src.tag() >= 21 && src.tag() <= 23 ? src.tag() : f.hint;
            json j = scalar(*child, hint, container);
            if (container) {
                stack.push_back(Frame{child, 0, hint, {}});
            } else {
                f.items.push_back(j);
            }
        }
        return result;
    }

    // --- the diagnostic notation ---

    namespace detail {
        inline void cbor_number(std::string& s, double d) noexcept {
            if (std::isnan(d)) {
                s += "NaN";
                return;
            }
            if (std::isinf(d)) {
                s += d < 0 ? "-Infinity" : "Infinity";
                return;
            }
            char room[NumberTextSize];
            size_t n = number_text(room, d);
            std::string t(room, n);
            // a float always shows itself one: 1.0, 1.0e+300
            size_t e = t.find('e');
            std::string mantissa = e == std::string::npos ? t : t.substr(0, e);
            if (mantissa.find('.') == std::string::npos) {
                mantissa += ".0";
            }
            s += mantissa;
            if (e != std::string::npos) {
                s += t.substr(e);
            }
        }

        inline void cbor_text(std::string& s, std::string_view v) noexcept {
            static constexpr char digits[] = "0123456789abcdef";
            s += '"';
            for (char ch : v) {
                auto c = uint8_t(ch);
                if (c == '"' || c == '\\') {
                    s += '\\';
                    s += ch;
                } else if (c < 0x20) {
                    s += "\\u00";
                    s += digits[c >> 4];
                    s += digits[c & 15];
                } else {
                    s += ch;
                }
            }
            s += '"';
        }
    }

    inline string cbor::to_string() const {
        std::string s;
        struct Frame {
            const cbor* items;
            const member* members;
            size_t n;
            size_t at;
            char close;
            bool is_map;
        };
        std::vector<Frame> stack;
        auto one = [&](const cbor& c) {
            switch (c._kind) {
                case kind::null: s += "null"; break;
                case kind::undefined: s += "undefined"; break;
                case kind::boolean: s += c._bits ? "true" : "false"; break;
                case kind::simple: s += "simple(" + std::to_string(c._bits) + ")"; break;
                case kind::integer:
                    if (c._aux) {
                        if (c._bits == UINT64_MAX) {
                            s += "-18446744073709551616";
                        } else {
                            s += "-" + std::to_string(c._bits + 1);
                        }
                    } else {
                        s += std::to_string(c._bits);
                    }
                    break;
                case kind::floating: detail::cbor_number(s, std::bit_cast<double>(c._bits)); break;
                case kind::bytes:
                case kind::extension: {
                    if (c._kind == kind::extension) {
                        s += "extension(" + std::to_string(int8_t(c._bits)) + ", ";
                    }
                    static constexpr char digits[] = "0123456789abcdef";
                    s += "h'";
                    for (char ch : c._string().view()) {
                        s += digits[uint8_t(ch) >> 4];
                        s += digits[uint8_t(ch) & 15];
                    }
                    s += "'";
                    if (c._kind == kind::extension) {
                        s += ")";
                    }
                    break;
                }
                case kind::text: detail::cbor_text(s, c._string().view()); break;
                case kind::array:
                    s += '[';
                    stack.push_back({c._elements(), nullptr, size_t(c._bits), 0, ']', false});
                    break;
                case kind::map:
                    s += '{';
                    stack.push_back({nullptr, c._members(), size_t(c._bits), 0, '}', true});
                    break;
                case kind::tag:
                    s += std::to_string(c._bits) + "(";
                    stack.push_back({c._elements(), nullptr, 1, 0, ')', false});
                    break;
            }
        };
        one(*this);
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (!f.is_map) {
                if (f.at == f.n) {
                    s += f.close;
                    stack.pop_back();
                    continue;
                }
                if (f.at) {
                    s += ", ";
                }
                const cbor& c = f.items[f.at++];
                one(c);
            } else {
                if (f.at == 2 * f.n) {
                    s += '}';
                    stack.pop_back();
                    continue;
                }
                bool key = f.at % 2 == 0;
                if (key && f.at) {
                    s += ", ";
                } else if (!key) {
                    s += ": ";
                }
                const member& m = f.members[f.at / 2];
                ++f.at;
                one(key ? m.key : m.value);
            }
        }
        return string(s);
    }
}

template<>
struct std::hash<sgcl::encoding::cbor> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::cbor& c) const noexcept {
        return c.hash();
    }
};
