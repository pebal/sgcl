//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "fields.h"
#include "detail/json_number.h"
#include "detail/json_out.h"
#include "detail/json_text.h"
#include "detail/position.h"
#include "detail/text_buffer.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/maker.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/unique_ptr.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        // The integer types a json is made of: every one but bool and the
        // character types (a char is a letter, not a number)
        template<class I>
        concept JsonInteger = std::integral<I> && !std::same_as<I, bool> && !std::same_as<I, char> && !std::same_as<I, wchar_t>
            && !std::same_as<I, char8_t> && !std::same_as<I, char16_t> && !std::same_as<I, char32_t>;

        struct JsonAccess;
        class JsonParser;

        template<class T>
        struct JsonValueWriter;
    }

    // One JSON value (RFC 8259): null, a boolean, a number, a string, an
    // array or an object. Immutable, as a string is: a copy is a copy of
    // the handle, a value is shared between threads with no lock, and a
    // "change" (set, erase, push_back, set_path) returns a new value and
    // leaves this one as it was; builder makes one in a loop without a
    // copy per step. Read it with methods: doc["user"]["name"].as_string()
    // — a key that is not there, an index past the end, a member asked of
    // an array all give null, so a chain of lookups never fails half-way.
    //
    // 24 bytes: a tracked pointer to what does not fit in a word (the
    // characters of a string, the elements of an array, the members of an
    // object), a word for a boolean or a number, and the kind. An array is
    // its elements side by side in one managed buffer; an object is its
    // members in the order of the input, side by side, with a hash index
    // (keyed: the hash of sgcl::string) past 16 members.
    //
    // Numbers: an integer literal is an int64 when one holds it, else an
    // uint64, else kept as its text (its digits are never lost, as Go's
    // float64 loses them); any other number is a double, rounded once
    // from the decimal, or its text with options.keep_number_text. A
    // number is written back with the shortest digits that read as the
    // same double, laid out as ECMAScript's JSON.stringify does.
    class json {
    public:
        using error = encoding::error;

        enum class kind : uint8_t {
            null,
            boolean,
            number,
            string,
            array,
            object
        };

        // A member of an object: works with [key, value]
        struct member;

        // What a parse or a reader accepts. The defaults are Go's v2:
        // invalid UTF-8, a lone surrogate and a key given twice in one
        // object are errors; unknown fields of a typed read are skipped.
        struct options {
            uint32_t max_depth = 512;             // arrays and objects inside one another
            bool allow_duplicate_keys = false;    // then the last one wins
            bool allow_invalid_utf8 = false;      // then U+FFFD, for a lone surrogate \uD800 too
            bool keep_number_text = false;        // a number that is not an integer kept as its literal
            bool reject_unknown_fields = false;   // a typed read: a key no field has is an error
            size_t max_token_size = size_t(64) << 20;   // what a reader of a stream holds at once, a token or a value read whole: errc::out_of_range past it
        };

        // How a value is written
        struct style {
            uint8_t indent = 0;                   // 0: compact, no space at all
            bool escape_html = false;             // <, > and & as <, >, &
            bool sort_keys = true;                // the keys of a hash map of a typed value, sorted
        };

        class builder;
        class reader;
        class writer;
        class token;

        static const style compact;
        static const style pretty;                // an indent of 2

        // --- making one ---

        json() noexcept = default;

        json(std::nullptr_t) noexcept {
        }

        json(bool b) noexcept
        : _bits(b), _tag(Tag::boolean) {
        }

        template<class I>
        requires detail::JsonInteger<I>
        json(I v) noexcept {
            if constexpr (std::is_signed_v<I>) {
                _bits = uint64_t(int64_t(v));
                _tag = Tag::int64;
            } else if (uint64_t(v) <= uint64_t(INT64_MAX)) {
                _bits = uint64_t(v);
                _tag = Tag::int64;
            } else {
                _bits = uint64_t(v);
                _tag = Tag::uint64;
            }
        }

        // NaN and the infinities are not JSON numbers: an assertion in a
        // debug build, null in a release one (as JSON.stringify writes them)
        json(double d) noexcept {
            assert(std::isfinite(d) && "NaN and the infinities are not JSON numbers");
            if (std::isfinite(d)) {
                _bits = std::bit_cast<uint64_t>(d);
                _tag = Tag::float64;
            }
        }

        json(float f) noexcept
        : json(double(f)) {
        }

        json(const string& s) noexcept
        : _ptr(s.as_slice().owner()), _tag(Tag::string) {
        }

        json(const char* s)
        : json(string(s)) {
        }

        json(const slice<const char>& s)
        : json(string(s)) {
        }

        static json array(std::initializer_list<json> elements);

        // Any range of values a json is made of
        template<class R>
        requires std::ranges::input_range<const R&> && std::is_convertible_v<std::ranges::range_reference_t<const R&>, json>
        static json array(const R& elements);

        // The members in their order; a key given twice keeps the last value
        static json object(std::initializer_list<member> members);

        // --- reading it in ---

        // The one value of the text, with nothing but white space around
        static expected<json, error> parse(const string& text);
        static expected<json, error> parse(const string& text, const options& o);

        // The one value of the stream, to its end; in a task
        // `co_await json::async_parse(in)`
        static expected<json, error> parse(const io::reader& in);
        static expected<json, error> parse(const io::reader& in, const options& o);
        static async::task<expected<json, error>> async_parse(const io::reader& in);
        static async::task<expected<json, error>> async_parse(io::reader in, options o);

        // --- typed values: a type of fields.h (describe, to_json, to_text) or
        // any the fields may have (a number, a string, a container...) ---

        // The value of the text as a T: its fields by their names, the
        // error with the path of the value that failed ("/users/3/age")
        template<class T>
        static expected<T, error> parse(const string& text);
        template<class T>
        static expected<T, error> parse(const string& text, const options& o);
        template<class T>
        static expected<T, error> parse(const io::reader& in);
        template<class T>
        static expected<T, error> parse(const io::reader& in, const options& o);
        template<class T>
        static async::task<expected<T, error>> async_parse(const io::reader& in);
        template<class T>
        static async::task<expected<T, error>> async_parse(io::reader in, options o);

        // The text of a T; fails on NaN, a value past its names, nesting
        // past 512 (a cycle), a variant's alternative with no describe()
        template<class T>
        static expected<string, error> stringify(const T& value);
        template<class T>
        static expected<string, error> stringify(const T& value, const style& s);

        // The value of a T, as xml::from makes an element of one: what
        // stringify writes, read back as a value (through the text: a
        // value made once, for a tree to go on building or to hand on)
        template<class T>
        static expected<json, error> from(const T& value);

        // This value as a T
        template<class T>
        expected<T, error> as() const;
        template<class T>
        expected<T, error> as(const options& o) const;

        // --- writing it out ---

        // A value always has a text: the strings' invalid UTF-8 is written
        // as �, and a json holds no NaN
        string to_string(const style& s = compact) const;

        // --- the kind ---

        kind type() const noexcept {
            switch (_tag) {
                case Tag::null: return kind::null;
                case Tag::boolean: return kind::boolean;
                case Tag::int64:
                case Tag::uint64:
                case Tag::float64:
                case Tag::number_text: return kind::number;
                case Tag::string: return kind::string;
                case Tag::array: return kind::array;
                case Tag::object:
                case Tag::large_object: return kind::object;
            }
            return kind::null;
        }

        bool is_null() const noexcept {
            return _tag == Tag::null;
        }

        bool is_bool() const noexcept {
            return _tag == Tag::boolean;
        }

        bool is_number() const noexcept {
            return type() == kind::number;
        }

        // A number whose value is an integer an int64 or an uint64 holds
        bool is_integer() const noexcept {
            return as_int() || as_uint();
        }

        bool is_string() const noexcept {
            return _tag == Tag::string;
        }

        bool is_array() const noexcept {
            return _tag == Tag::array;
        }

        bool is_object() const noexcept {
            return _tag == Tag::object || _tag == Tag::large_object;
        }

        // --- the value: nullopt when the kind differs or the value does not fit exactly ---

        optional<bool> as_bool() const noexcept {
            if (_tag != Tag::boolean) {
                return nullopt;
            }
            return _bits != 0;
        }

        // 2.0 is 2; 2.5 and 2^63 are nullopt
        optional<int64_t> as_int() const noexcept;
        optional<uint64_t> as_uint() const noexcept;

        // Any number, rounded to the nearest double; nullopt for a number
        // kept as text that is out of a double's range
        optional<double> as_double() const noexcept;

        optional<string> as_string() const noexcept {
            if (_tag != Tag::string) {
                return nullopt;
            }
            return _string();
        }

        // The literal of a number kept as its text (an integer past uint64,
        // any with keep_number_text); nullopt for any other value
        optional<string> number_text() const {
            if (_tag != Tag::number_text) {
                return nullopt;
            }
            return _string();
        }

        // --- what is inside ---

        // The member's value, or null when there is none (or this is not an object)
        const json& operator[](const string& key) const noexcept {
            return _find(key.view(), key.empty() ? string::hash_of({}) : key.hash());
        }

        template<size_t N>
        const json& operator[](const char (&key)[N]) const noexcept {
            std::string_view k(key, std::char_traits<char>::length(key));
            return _find(k, string::hash_of(k));
        }

        // The element, or null past the end (or when this is not an array)
        const json& operator[](size_t index) const noexcept {
            if (_tag != Tag::array || index >= _bits) {
                return _null();
            }
            return _elements()[index];
        }

        bool contains(const string& key) const noexcept {
            return _find_index(key.view(), key.empty() ? string::hash_of({}) : key.hash()) != NotFound;
        }

        // The elements or the members; 0 for anything else
        size_t size() const noexcept {
            return is_array() || is_object() ? size_t(_bits) : 0;
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        slice<const json> elements() const noexcept {
            if (_tag != Tag::array || _bits == 0) {
                return {};
            }
            return slice<const json>(_ptr, _elements(), size_t(_bits));
        }

        // In the order of the input
        slice<const member> members() const noexcept;

        // The value at a JSON Pointer (RFC 6901): "" is this, "/users/0/name"
        // a member of an element of a member; ~0 is '~' and ~1 is '/'
        optional<json> at_path(const string& pointer) const;

        // --- new versions (this value never changes) ---

        // An object with the member added or its value replaced; on a value
        // that is not an object, an object of one member
        json set(const string& key, const json& value) const;

        // The object without the member (the same value when there is none)
        json erase(const string& key) const;

        // An array with the element replaced; an index past the end is the
        // same value
        json set(size_t index, const json& value) const;

        // An array with the element at the end; on a value that is not an
        // array, an array of one element
        json push_back(const json& value) const;

        // The value with the one at the pointer replaced or added: a member
        // set, an element replaced, "-" or the size appending one; members
        // missing on the way are made as objects (null is replaced by one).
        // A pointer that goes through a number, a string or a boolean, or
        // past the end of an array, gives this value unchanged.
        json set_path(const string& pointer, const json& value) const;

        // Deep. Numbers by value: two integers exactly, an integer and a
        // double as doubles (1 == 1.0, and an integer past 2^53 equals the
        // double it rounds to — the double 2^63 written is the integer
        // 9223372036854775808 read), a number kept as text with another
        // text by its digits and with anything else as the rest compare.
        // Objects as sets of members: the order does not matter, as it
        // does not in JSON.
        friend bool operator==(const json& a, const json& b) {
            return a._equals(b);
        }

        size_t hash() const;

    private:
        friend struct detail::JsonAccess;
        friend class detail::JsonParser;

        enum class Tag : uint8_t {
            null = 0,        // all bits zero: a zeroed json is null
            boolean,
            int64,
            uint64,
            float64,
            number_text,     // the literal, a string's object in _ptr
            string,          // the characters, a string's object in _ptr
            array,           // the first element of a managed buffer in _ptr, the count in _bits
            object,          // the first member likewise
            large_object     // a JsonLargeObject in _ptr: the members and the index
        };

        static constexpr size_t NotFound = size_t(-1);

        string _string() const noexcept {
            return StringAccessOf::over(_ptr);
        }

        struct StringAccessOf {
            static string over(const tracked_ptr<const void>& word) noexcept {
                return sgcl::detail::StringAccess::over<string>(word);
            }
        };

        const json* _elements() const noexcept {
            return static_cast<const json*>(_ptr.get());
        }

        const member* _members() const noexcept;
        size_t _find_index(std::string_view key, size_t hash) const noexcept;
        const json& _find(std::string_view key, size_t hash) const noexcept;
        bool _equals(const json& o) const;

        static const json& _null() noexcept {
            // never constructed and never written: a zeroed json is null
            alignas(8) static const unsigned char bytes[24] = {};
            return *std::launder(reinterpret_cast<const json*>(bytes));
        }

        tracked_ptr<const void> _ptr;
        uint64_t _bits = 0;
        Tag _tag = Tag::null;
    };

    static_assert(sizeof(json) == 24, "a json is a pointer, a word and a tag");

    struct json::member {
        string key;
        json value;
    };

    inline const json::style json::compact {};
    inline const json::style json::pretty {2};

    namespace detail {
        // An object past SmallObject members: the members, and a table of
        // their indexes by the hash of the key (open addressing, a power
        // of two twice the count at least; 0 is empty, i + 1 the member i)
        inline constexpr size_t SmallObject = 16;

        struct JsonLargeObject {
            // the buffers, by their first element (a buffer's element is
            // never addressed by a typed tracked_ptr)
            tracked_ptr<const void> members;
            tracked_ptr<const void> slots;
            uint32_t mask = 0;

            const json::member* member_data() const noexcept {
                return static_cast<const json::member*>(members.get());
            }

            const uint32_t* slot_data() const noexcept {
                return static_cast<const uint32_t*>(slots.get());
            }
        };

        // A managed buffer of n T's, constructed by the caller; the
        // buffer's owner goes to owner
        template<class T>
        T* json_buffer(size_t n, tracked_ptr<const void>& owner) {
            auto u = unique_ptr<T>(Maker<T[]>::make_tracked_data(n));
            T* p = u.get();
            owner = tracked_ptr<const void>(std::move(u));
            return p;
        }

        // What the parser, the builder and the typed walk make values with
        struct JsonAccess {
            static json number_text(const string& literal) {
                json j;
                j._ptr = literal.as_slice().owner();
                j._tag = json::Tag::number_text;
                return j;
            }

            static json uint(uint64_t v) noexcept {
                return json(v);
            }

            // The values [first, first + n) moved into a new array
            static json array_of(json* first, size_t n) {
                json j;
                j._tag = json::Tag::array;
                j._bits = n;
                if (n) {
                    json* p = json_buffer<json>(n, j._ptr);
                    for (size_t i = 0; i < n; ++i) {
                        Maker<json>::construct(p + i, std::move(first[i]));
                    }
                }
                return j;
            }

            // The members moved into a new object; the keys are all
            // different (the caller made sure)
            static json object_of(json::member* first, size_t n) {
                json j;
                j._tag = json::Tag::object;
                j._bits = n;
                if (n == 0) {
                    return j;
                }
                tracked_ptr<const void> owner;
                json::member* p = json_buffer<json::member>(n, owner);
                for (size_t i = 0; i < n; ++i) {
                    Maker<json::member>::construct(p + i, std::move(first[i]));
                }
                if (n <= SmallObject) {
                    j._ptr = std::move(owner);
                    return j;
                }
                size_t cap = std::bit_ceil(n * 2);
                tracked_ptr<const void> slots_owner;
                uint32_t* slots = json_buffer<uint32_t>(cap, slots_owner);
                std::memset(slots, 0, cap * sizeof(uint32_t));
                size_t mask = cap - 1;
                for (size_t i = 0; i < n; ++i) {
                    size_t s = p[i].key.hash() & mask;
                    while (slots[s]) {
                        s = (s + 1) & mask;
                    }
                    slots[s] = uint32_t(i + 1);
                }
                auto node = make_tracked<JsonLargeObject>();
                node->members = std::move(owner);
                node->slots = std::move(slots_owner);
                node->mask = uint32_t(mask);
                j._ptr = tracked_ptr<const void>(std::move(node));
                j._tag = json::Tag::large_object;
                return j;
            }

            // The members with every key given once: an earlier member with
            // the key of a later one is dropped (the last one wins). The
            // index of the first key given twice, or n when every key is
            // different; with keep_last false the members are left as they
            // were and that index is all the caller wants.
            static size_t distinct(vector<json::member>& ms, size_t from, bool keep_last) {
                size_t n = ms.size() - from;
                json::member* m = ms.data() + from;
                size_t first_dup = NotFoundIndex;
                std::vector<bool> dropped;
                auto drop = [&](size_t earlier, size_t later) {
                    if (first_dup == NotFoundIndex || later < first_dup) {
                        first_dup = later;
                    }
                    if (keep_last) {
                        if (dropped.empty()) {
                            dropped.assign(n, false);
                        }
                        dropped[earlier] = true;
                    }
                };
                if (n <= SmallObject) {
                    for (size_t i = 1; i < n; ++i) {
                        auto ki = m[i].key.view();
                        for (size_t k = 0; k < i; ++k) {
                            if (m[k].key.size() == ki.size() && m[k].key.view() == ki && (dropped.empty() || !dropped[k])) {
                                drop(k, i);
                                if (!keep_last) {
                                    return first_dup;
                                }
                                break;
                            }
                        }
                    }
                } else {
                    size_t cap = std::bit_ceil(n * 2);
                    size_t mask = cap - 1;
                    std::vector<uint32_t> slots(cap, 0);
                    for (size_t i = 0; i < n; ++i) {
                        size_t s = m[i].key.hash() & mask;
                        for (;;) {
                            uint32_t e = slots[s];
                            if (!e) {
                                slots[s] = uint32_t(i + 1);
                                break;
                            }
                            if (m[e - 1].key == m[i].key) {
                                drop(e - 1, i);
                                if (!keep_last) {
                                    return first_dup;
                                }
                                slots[s] = uint32_t(i + 1);
                                break;
                            }
                            s = (s + 1) & mask;
                        }
                    }
                }
                if (first_dup != NotFoundIndex && keep_last) {
                    size_t w = 0;
                    for (size_t i = 0; i < n; ++i) {
                        if (!dropped[i]) {
                            if (w != i) {
                                m[w] = std::move(m[i]);
                            }
                            ++w;
                        }
                    }
                    ms.resize(from + w);
                }
                return first_dup == NotFoundIndex ? n : first_dup;
            }

            static constexpr size_t NotFoundIndex = size_t(-1);

            static const tracked_ptr<const void>& pointer(const json& j) noexcept {
                return j._ptr;
            }

            static const json* raw_elements(const json& j) noexcept {
                return j._elements();
            }

            static const json::member* raw_members(const json& j) noexcept {
                return j._members();
            }

            // A number, a string, a boolean or null into the text
            static void write_scalar(JsonOut& out, const json& j) {
                switch (j._tag) {
                    case json::Tag::null:
                        out.null();
                        break;
                    case json::Tag::boolean:
                        out.boolean(j._bits != 0);
                        break;
                    case json::Tag::int64:
                        out.integer(int64_t(j._bits));
                        break;
                    case json::Tag::uint64:
                        out.integer(j._bits);
                        break;
                    case json::Tag::float64:
                        out.floating(std::bit_cast<double>(j._bits));
                        break;
                    case json::Tag::number_text:
                        out.literal(j._string().view());
                        break;
                    case json::Tag::string:
                        out.quoted(j._string().view());
                        break;
                    default:
                        break;
                }
            }
        };

        // --- numbers compared by value ---

        // A number's literal in a normal form: its sign, its significant
        // digits (no leading or trailing zeros) and the power of ten of the
        // first of them; zero has no digits and no sign
        struct NormalDecimal {
            bool negative = false;
            std::string digits;
            int64_t point = 0;

            friend bool operator==(const NormalDecimal&, const NormalDecimal&) = default;
        };

        inline NormalDecimal normal_decimal(std::string_view lit) {
            Decimal d = decimal_of(lit);
            NormalDecimal r;
            std::string all;
            all.reserve(d.integer.size() + d.fraction.size());
            all.append(d.integer);
            all.append(d.fraction);
            size_t lead = 0;
            while (lead < all.size() && all[lead] == '0') {
                ++lead;
            }
            size_t last = all.size();
            while (last > lead && all[last - 1] == '0') {
                --last;
            }
            if (lead == last) {
                return r;   // zero
            }
            r.negative = d.negative;
            r.digits = all.substr(lead, last - lead);
            r.point = int64_t(d.integer.size()) - int64_t(lead) + d.exponent;
            return r;
        }

        inline size_t mix(size_t h) noexcept {
            h ^= h >> 33;
            h *= 0xff51afd7ed558ccdull;
            h ^= h >> 33;
            return h;
        }
    }

    // --- json: the members defined out of the class ---

    inline json json::array(std::initializer_list<json> elements) {
        vector<json> v(elements.begin(), elements.end());
        return detail::JsonAccess::array_of(v.data(), v.size());
    }

    template<class R>
    requires std::ranges::input_range<const R&> && std::is_convertible_v<std::ranges::range_reference_t<const R&>, json>
    json json::array(const R& elements) {
        vector<json> v;
        for (auto&& e : elements) {
            v.push_back(json(e));
        }
        return detail::JsonAccess::array_of(v.data(), v.size());
    }

    inline json json::object(std::initializer_list<member> members) {
        vector<member> v(members.begin(), members.end());
        detail::JsonAccess::distinct(v, 0, true);
        return detail::JsonAccess::object_of(v.data(), v.size());
    }

    inline const json::member* json::_members() const noexcept {
        if (_tag == Tag::large_object) {
            return static_cast<const detail::JsonLargeObject*>(_ptr.get())->member_data();
        }
        return static_cast<const member*>(_ptr.get());
    }

    inline slice<const json::member> json::members() const noexcept {
        if (!is_object() || _bits == 0) {
            return {};
        }
        if (_tag == Tag::large_object) {
            auto node = static_cast<const detail::JsonLargeObject*>(_ptr.get());
            return slice<const member>(node->members, node->member_data(), size_t(_bits));
        }
        return slice<const member>(_ptr, _members(), size_t(_bits));
    }

    inline size_t json::_find_index(std::string_view key, size_t hash) const noexcept {
        if (_tag == Tag::object) {
            // a line, by the hash each key keeps in its string (computed
            // once; the keys of a parse are shared) and then the characters,
            // which measured faster than the characters alone for keys alike
            auto m = _members();
            for (size_t i = 0; i < _bits; ++i) {
                if (m[i].key.hash() == hash && m[i].key.view() == key) {
                    return i;
                }
            }
            return NotFound;
        }
        if (_tag == Tag::large_object) {
            auto node = static_cast<const detail::JsonLargeObject*>(_ptr.get());
            auto m = node->member_data();
            auto slots = node->slot_data();
            size_t s = hash & node->mask;
            while (uint32_t e = slots[s]) {
                auto& k = m[e - 1].key;
                if (k.size() == key.size() && k.view() == key) {
                    return e - 1;
                }
                s = (s + 1) & node->mask;
            }
        }
        return NotFound;
    }

    inline const json& json::_find(std::string_view key, size_t hash) const noexcept {
        size_t i = _find_index(key, hash);
        return i == NotFound ? _null() : _members()[i].value;
    }

    inline optional<int64_t> json::as_int() const noexcept {
        switch (_tag) {
            case Tag::int64:
                return int64_t(_bits);
            case Tag::uint64:
                return nullopt;
            case Tag::float64: {
                double d = std::bit_cast<double>(_bits);
                if (d >= -9223372036854775808.0 && d < 9223372036854775808.0 && d == std::trunc(d)) {
                    return int64_t(d);
                }
                return nullopt;
            }
            case Tag::number_text:
                return detail::exact_integer<int64_t>(_string().view());
            default:
                return nullopt;
        }
    }

    inline optional<uint64_t> json::as_uint() const noexcept {
        switch (_tag) {
            case Tag::int64:
                if (int64_t(_bits) < 0) {
                    return nullopt;
                }
                return _bits;
            case Tag::uint64:
                return _bits;
            case Tag::float64: {
                double d = std::bit_cast<double>(_bits);
                if (d >= 0 && d < 18446744073709551616.0 && d == std::trunc(d)) {
                    return uint64_t(d);
                }
                return nullopt;
            }
            case Tag::number_text:
                return detail::exact_integer<uint64_t>(_string().view());
            default:
                return nullopt;
        }
    }

    inline optional<double> json::as_double() const noexcept {
        switch (_tag) {
            case Tag::int64:
                return double(int64_t(_bits));
            case Tag::uint64:
                return double(_bits);
            case Tag::float64:
                return std::bit_cast<double>(_bits);
            case Tag::number_text:
                return detail::floating_of<double>(_string().view());
            default:
                return nullopt;
        }
    }

    inline bool json::_equals(const json& o) const {
        // an explicit stack of the pairs still to compare: a value built
        // by hand may be deeper than any stack of calls
        std::vector<pair<const json*, const json*>> todo;
        todo.emplace_back(this, &o);
        while (!todo.empty()) {
            auto [a, b] = todo.back();
            todo.pop_back();
            kind ka = a->type(), kb = b->type();
            if (ka != kb) {
                return false;
            }
            switch (ka) {
                case kind::null:
                    break;
                case kind::boolean:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    break;
                case kind::string:
                    if (a->_string() != b->_string()) {
                        return false;
                    }
                    break;
                case kind::number: {
                    Tag ta = a->_tag, tb = b->_tag;
                    if (ta > tb) {
                        std::swap(a, b);
                        std::swap(ta, tb);
                    }
                    // ta <= tb: int64 < uint64 < float64 < number_text
                    bool same;
                    if (tb == Tag::number_text) {
                        if (ta == Tag::number_text) {
                            same = detail::normal_decimal(a->_string().view()) == detail::normal_decimal(b->_string().view());
                        } else if (ta == Tag::float64) {
                            auto d = b->as_double();
                            same = d && *d == std::bit_cast<double>(a->_bits);
                        } else if (ta == Tag::int64) {
                            auto i = b->as_int();
                            same = i && *i == int64_t(a->_bits);
                        } else {
                            auto u = b->as_uint();
                            same = u && *u == a->_bits;
                        }
                    } else if (ta == tb) {
                        same = ta == Tag::float64 ? std::bit_cast<double>(a->_bits) == std::bit_cast<double>(b->_bits) : a->_bits == b->_bits;
                    } else if (tb == Tag::float64) {
                        // an integer and a double compare as doubles: what a
                        // double is written as, read back as an integer, is it
                        double d = std::bit_cast<double>(b->_bits);
                        same = (ta == Tag::int64 ? double(int64_t(a->_bits)) : double(a->_bits)) == d;
                    } else {
                        same = false;   // an int64 and an uint64 never hold the same value: the uint64 is past INT64_MAX
                    }
                    if (!same) {
                        return false;
                    }
                    break;
                }
                case kind::array: {
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    auto ea = a->_elements(), eb = b->_elements();
                    for (size_t i = 0; i < a->_bits; ++i) {
                        todo.emplace_back(ea + i, eb + i);
                    }
                    break;
                }
                case kind::object: {
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    auto ma = a->_members();
                    for (size_t i = 0; i < a->_bits; ++i) {
                        auto& k = ma[i].key;
                        size_t j = b->_find_index(k.view(), k.hash());
                        if (j == NotFound) {
                            return false;
                        }
                        todo.emplace_back(&ma[i].value, &b->_members()[j].value);
                    }
                    break;
                }
            }
        }
        return true;
    }

    inline size_t json::hash() const {
        // numbers hash by their double (equal numbers have equal doubles),
        // an array by its elements in order, an object by the sum of its
        // members' hashes, in any order; iterative, as the comparison is.
        // A container's hash is folded in when its last child is done.
        struct Frame {
            const json* value;
            size_t next;
            size_t h;
        };
        auto scalar = [](const json& j) -> size_t {
            switch (j._tag) {
                case Tag::null:
                    return 0x6e756c6c;
                case Tag::boolean:
                    return detail::mix(j._bits + 0x626f6f6c);
                case Tag::string:
                    return j._string().hash();
                default: {
                    if (auto d = j.as_double()) {
                        return detail::mix(std::bit_cast<uint64_t>(*d == 0 ? 0.0 : *d));
                    }
                    auto n = detail::normal_decimal(j._string().view());
                    return detail::mix(string::hash_of(n.digits) ^ uint64_t(n.point) ^ n.negative);
                }
            }
        };
        std::vector<Frame> stack;
        size_t result = 0;
        const json* v = this;
        for (;;) {
            size_t h;
            if (v->is_array() || v->is_object()) {
                stack.push_back(Frame{v, 0, (v->is_array() ? 0x61727261u : 0x6f626a65u) + v->_bits});
                h = 0;
                v = nullptr;
            } else {
                h = scalar(*v);
            }
            // h is the hash of the value just done (none when a container
            // was opened): fold it into its parent, and close the parents done
            bool pending = v != nullptr;
            for (;;) {
                if (stack.empty()) {
                    return pending ? h : result;
                }
                Frame& f = stack.back();
                if (pending) {
                    if (f.value->is_array()) {
                        f.h = detail::mix(f.h * 31 + h);
                    } else {
                        f.h += detail::mix(f.value->_members()[f.next - 1].key.hash() ^ (h * 0x9E3779B97F4A7C15ull));
                    }
                }
                if (f.next < f.value->_bits) {
                    size_t i = f.next++;
                    v = f.value->is_array() ? &f.value->_elements()[i] : &f.value->_members()[i].value;
                    break;
                }
                h = f.h;
                pending = true;
                stack.pop_back();
                result = h;
            }
        }
    }

    // --- the parser of a text in memory ---

    namespace detail {
        // The error of a text in memory at a byte of it: the detail's words,
        // the offset counted from the start of the whole input
        inline std::string char_name(const char* p, const char* end) {
            if (p == end) {
                return "the end of the input";
            }
            return quoted_byte(uint8_t(*p));
        }

        // A cursor over a text in memory, all of it there: the pieces of
        // the grammar (white space, a string, a number, a word) and the
        // first failure, which stops everything. The parser of a whole
        // value and the typed read walk the text with it.
        class JsonCursor {
        public:
            JsonCursor(const char* begin, const char* end, uint64_t base, const json::options& o) noexcept
            : p(begin), end(end), begin(begin), base(base), options(o) {
            }

            const char* p;
            const char* end;
            const char* begin;
            uint64_t base;
            const json::options& options;
            std::string scratch;
            optional<error> failure;

            bool fail(errc code, const char* at, std::string detail) {
                if (!failure) {
                    failure = error(code, base + uint64_t(at - begin), string(detail));
                }
                return false;
            }

            void space() noexcept {
                p = skip_space(p, end);
            }

            bool at_end() const noexcept {
                return p == end;
            }

            // The end of the input where a value was wanted
            bool fail_end(const char* what) {
                return fail(errc::unexpected_end, end, std::string("unexpected end of input, expected ") + what);
            }

            // A string at p (the quote): its characters, which are the
            // text's own or the scratch's, valid until the next string
            bool string_token(std::string_view& out) {
                const char* start = ++p;
                StringScan state;
                errc code = errc::syntax;
                auto s = scan_string(start, p, end, true, state, scratch, options.allow_invalid_utf8, code);
                if (s != ScanStatus::done) {
                    return fail_string(code, p);
                }
                out = state.decoded ? std::string_view(scratch) : std::string_view(start, size_t(p - 1 - start));
                return true;
            }

            bool fail_string(errc code, const char* at) {
                switch (code) {
                    case errc::unexpected_end:
                        return fail(code, at, "unexpected end of input inside a string");
                    case errc::invalid_escape:
                        return fail(code, at, "invalid escape sequence in a string");
                    case errc::invalid_utf8:
                        return fail(code, at, "invalid UTF-8 in a string");
                    default:
                        return fail(code, at, "invalid character " + char_name(at, end) + " in a string: a control character must be escaped");
                }
            }

            // A number at p: its literal, and whether it is an integer's
            bool number_token(std::string_view& literal, bool& plain) {
                const char* start = p;
                NumberState s = NumberState::start;
                plain = true;
                if (scan_number(p, end, true, s, plain) != ScanStatus::done) {
                    if (p == end) {
                        return fail(errc::unexpected_end, p, "unexpected end of input inside a number");
                    }
                    return fail(errc::syntax, p, "invalid character " + char_name(p, end) + " in a number");
                }
                literal = std::string_view(start, size_t(p - start));
                return true;
            }

            bool word(std::string_view w) {
                if (scan_word(p, end, true, w) != ScanStatus::done) {
                    if (p == end) {
                        return fail(errc::unexpected_end, p, "unexpected end of input inside the literal " + std::string(w));
                    }
                    return fail(errc::syntax, p, "invalid character " + char_name(p, end) + " in the literal " + std::string(w));
                }
                return true;
            }

            // A number's literal out of a double's range
            bool fail_range(const char* at, std::string_view literal) {
                std::string lit(literal.substr(0, 40));
                if (literal.size() > 40) {
                    lit += "...";
                }
                return fail(errc::out_of_range, at, "the number " + lit + " is out of range");
            }
        };

        // A value of the text built as a json: iterative, with an explicit
        // stack of the arrays and objects open (a nesting as deep as
        // max_depth costs no stack of calls), their elements and members
        // gathered on managed stacks and moved into their buffers when
        // they close. The keys of one parse are shared: a key met again
        // (the same field of a thousand objects) is found in a small table
        // of the keys seen last and not made again.
        class JsonParser {
        public:
            explicit JsonParser(const json::options& o) noexcept
            : _options(o) {
            }

            // The value at the start of [begin, end) with nothing but white
            // space after it; offsets in errors count from base
            expected<json, error> parse(const char* begin, const char* end, uint64_t base, uint32_t max_depth) {
                JsonCursor c(begin, end, base, _options);
                json v;
                if (!_value(c, max_depth, v)) {
                    return unexpected<error>(std::move(*c.failure));
                }
                c.space();
                if (!c.at_end()) {
                    c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after the value");
                    return unexpected<error>(std::move(*c.failure));
                }
                return v;
            }

            // One value from the cursor, which is left past it
            bool _value(JsonCursor& c, uint32_t max_depth, json& out) {
                struct Frame {
                    bool object;
                    uint32_t start;   // the first of its elements or members on the stacks
                };
                std::vector<Frame> frames;
                vector<json> values;
                vector<json::member> members;
                std::vector<const char*> key_at;   // where each key of members starts, for an error
                string key;
                json v;
                for (;;) {
                    // --- a value ---
                    c.space();
                    if (c.at_end()) {
                        return c.fail_end("a value");
                    }
                    bool opened = false;
                    switch (*c.p) {
                        case '{':
                        case '[': {
                            bool object = *c.p == '{';
                            if (frames.size() >= max_depth) {
                                return c.fail(errc::depth_limit, c.p, "nesting deeper than " + std::to_string(max_depth));
                            }
                            ++c.p;
                            c.space();
                            if (c.at_end()) {
                                return c.fail_end(object ? "a key or '}'" : "a value or ']'");
                            }
                            if (*c.p == (object ? '}' : ']')) {
                                ++c.p;
                                v = object ? JsonAccess::object_of(nullptr, 0) : JsonAccess::array_of(nullptr, 0);
                                break;
                            }
                            frames.push_back(Frame{object, uint32_t(object ? members.size() : values.size())});
                            opened = true;
                            break;
                        }
                        case '"': {
                            std::string_view s;
                            if (!c.string_token(s)) {
                                return false;
                            }
                            v = json(string(s));
                            break;
                        }
                        case 't':
                            if (!c.word("true")) {
                                return false;
                            }
                            v = json(true);
                            break;
                        case 'f':
                            if (!c.word("false")) {
                                return false;
                            }
                            v = json(false);
                            break;
                        case 'n':
                            if (!c.word("null")) {
                                return false;
                            }
                            v = json();
                            break;
                        default:
                            if (*c.p == '-' || is_digit(*c.p)) {
                                const char* at = c.p;
                                std::string_view lit;
                                bool plain;
                                if (!c.number_token(lit, plain) || !_number(c, at, lit, plain, v)) {
                                    return false;
                                }
                                break;
                            }
                            return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " where a value was expected");
                    }
                    if (opened) {
                        if (frames.back().object) {
                            if (!_key(c, members, key_at)) {
                                return false;
                            }
                        }
                        continue;
                    }
                    // --- the value is v: where it goes, and what may follow ---
                    for (;;) {
                        if (frames.empty()) {
                            out = std::move(v);
                            return true;
                        }
                        Frame& f = frames.back();
                        if (f.object) {
                            members.back().value = std::move(v);
                        } else {
                            values.push_back(std::move(v));
                        }
                        c.space();
                        if (c.at_end()) {
                            return c.fail_end(f.object ? "',' or '}'" : "',' or ']'");
                        }
                        if (*c.p == ',') {
                            ++c.p;
                            if (f.object && !_key(c, members, key_at)) {
                                return false;
                            }
                            break;   // the next value
                        }
                        if (*c.p != (f.object ? '}' : ']')) {
                            return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + (f.object ? " after an object member, expected ',' or '}'" : " after an array element, expected ',' or ']'"));
                        }
                        ++c.p;
                        if (f.object) {
                            size_t n = members.size() - f.start;
                            size_t dup = JsonAccess::distinct(members, f.start, _options.allow_duplicate_keys);
                            if (dup != n && !_options.allow_duplicate_keys) {
                                auto& k = members[f.start + dup].key;
                                return c.fail(errc::duplicate_key, key_at[f.start + dup], "duplicate key \"" + std::string(k.view()) + "\"");
                            }
                            v = JsonAccess::object_of(members.data() + f.start, members.size() - f.start);
                            members.resize(f.start);
                            key_at.resize(f.start);
                        } else {
                            v = JsonAccess::array_of(values.data() + f.start, values.size() - f.start);
                            values.resize(f.start);
                        }
                        frames.pop_back();
                    }
                }
            }

        private:
            // A key and its colon, the member pushed with a null value
            bool _key(JsonCursor& c, vector<json::member>& members, std::vector<const char*>& key_at) {
                c.space();
                if (c.at_end()) {
                    return c.fail_end("a key");
                }
                if (*c.p != '"') {
                    return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " where a key was expected");
                }
                key_at.push_back(c.p);
                std::string_view k;
                if (!c.string_token(k)) {
                    return false;
                }
                members.push_back(json::member{_shared_key(k), json()});
                c.space();
                if (c.at_end()) {
                    return c.fail_end("':'");
                }
                if (*c.p != ':') {
                    return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after an object key, expected ':'");
                }
                ++c.p;
                return true;
            }

            // A key made once per parse: the table of the keys seen last,
            // by a cheap hash of the characters (a collision only makes a
            // string anew)
            string _shared_key(std::string_view k) {
                if (k.size() > 32) {
                    return string(k);
                }
                if (_keys.empty()) {
                    _keys.resize(KeyTable);
                }
                size_t h = k.size();
                for (char ch : k) {
                    h = h * 31 + uint8_t(ch);
                }
                auto& slot = _keys[(h ^ (h >> 7)) & (KeyTable - 1)];
                if (slot.size() == k.size() && slot.view() == k) {
                    return slot;
                }
                slot = string(k);
                return slot;
            }

            bool _number(JsonCursor& c, const char* at, std::string_view lit, bool plain, json& v) {
                if (plain) {
                    auto r = integer_of(lit);
                    if (r.fit == IntegerFit::int64) {
                        // -0 is the double -0.0, which an integer cannot be
                        v = r.i == 0 && lit[0] == '-' ? json(-0.0) : json(r.i);
                    } else if (r.fit == IntegerFit::uint64) {
                        v = json(r.u);
                    } else {
                        v = JsonAccess::number_text(string(lit));
                    }
                    return true;
                }
                if (_options.keep_number_text) {
                    v = JsonAccess::number_text(string(lit));
                    return true;
                }
                auto d = floating_of<double>(lit);
                if (!d) {
                    return c.fail_range(at, lit);
                }
                v = json(*d);
                return true;
            }

            static constexpr size_t KeyTable = 256;
            const json::options& _options;
            vector<string> _keys;
        };
    }

    inline expected<json, json::error> json::parse(const string& text) {
        return parse(text, options());
    }

    inline expected<json, json::error> json::parse(const string& text, const options& o) {
        detail::JsonParser parser(o);
        auto r = parser.parse(text.data(), text.data() + text.size(), 0, o.max_depth);
        if (!r) {
            r.error().locate(text);
        }
        return r;
    }

    // --- writing ---

    namespace detail {
        // A value into the text: iterative, as the comparison is (a value
        // built by hand may be deeper than a stack of calls)
        inline void write_json(JsonOut& out, const json& root) {
            struct Frame {
                const json* value;
                size_t next;
            };
            std::vector<Frame> stack;
            auto emit = [&](const json& v) {
                if (v.is_array() || v.is_object()) {
                    bool object = v.is_object();
                    out.begin(object);
                    if (v.empty()) {
                        out.end(object);
                    } else {
                        stack.push_back(Frame{&v, 0});
                    }
                    return;
                }
                JsonAccess::write_scalar(out, v);
            };
            emit(root);
            while (!stack.empty()) {
                Frame& f = stack.back();
                const json& v = *f.value;
                if (f.next == v.size()) {
                    out.end(v.is_object());
                    stack.pop_back();
                    continue;
                }
                size_t i = f.next++;
                if (v.is_object()) {
                    auto& m = JsonAccess::raw_members(v)[i];
                    out.key(m.key.view());
                    emit(m.value);
                } else {
                    emit(JsonAccess::raw_elements(v)[i]);
                }
            }
        }
    }

    inline string json::to_string(const style& s) const {
        detail::JsonOut out(s.indent, s.escape_html);
        detail::write_json(out, *this);
        return string(out.text().view());
    }

    // --- new versions ---

    namespace detail {
        // The tokens of a JSON Pointer (RFC 6901): "" none, "/a/b" two, ~1
        // and ~0 the '/' and the '~' of a key; nullopt for a text that is
        // not a pointer (no leading '/', a '~' followed by anything else)
        inline optional<std::vector<std::string>> pointer_tokens(std::string_view p) {
            std::vector<std::string> out;
            if (p.empty()) {
                return out;
            }
            if (p[0] != '/') {
                return nullopt;
            }
            size_t i = 1;
            std::string t;
            for (;;) {
                if (i == p.size() || p[i] == '/') {
                    out.push_back(std::move(t));
                    t.clear();
                    if (i == p.size()) {
                        return out;
                    }
                    ++i;
                    continue;
                }
                if (p[i] == '~') {
                    if (i + 1 == p.size() || (p[i + 1] != '0' && p[i + 1] != '1')) {
                        return nullopt;
                    }
                    t += p[i + 1] == '0' ? '~' : '/';
                    i += 2;
                    continue;
                }
                t += p[i++];
            }
        }

        // The index an array token names: digits, no leading zero but in
        // "0" itself; nullopt for anything else ("-" among them)
        inline optional<size_t> pointer_index(std::string_view t) noexcept {
            if (t.empty() || t.size() > 18 || (t.size() > 1 && t[0] == '0')) {
                return nullopt;
            }
            size_t v = 0;
            for (char c : t) {
                if (!is_digit(c)) {
                    return nullopt;
                }
                v = v * 10 + size_t(c - '0');
            }
            return v;
        }
    }

    inline optional<json> json::at_path(const string& pointer) const {
        auto tokens = detail::pointer_tokens(pointer.view());
        if (!tokens) {
            return nullopt;
        }
        const json* v = this;
        for (auto& t : *tokens) {
            if (v->is_object()) {
                size_t i = v->_find_index(t, string::hash_of(t));
                if (i == NotFound) {
                    return nullopt;
                }
                v = &v->_members()[i].value;
            } else if (v->is_array()) {
                auto i = detail::pointer_index(t);
                if (!i || *i >= v->_bits) {
                    return nullopt;
                }
                v = &v->_elements()[*i];
            } else {
                return nullopt;
            }
        }
        return *v;
    }

    inline json json::set(const string& key, const json& value) const {
        if (!is_object()) {
            member m{key, value};
            return detail::JsonAccess::object_of(&m, 1);
        }
        size_t at = _find_index(key.view(), key.empty() ? string::hash_of({}) : key.hash());
        vector<member> ms(_members(), _members() + _bits);
        if (at == NotFound) {
            ms.push_back(member{key, value});
        } else {
            ms[at].value = value;
        }
        return detail::JsonAccess::object_of(ms.data(), ms.size());
    }

    inline json json::erase(const string& key) const {
        size_t at = is_object() ? _find_index(key.view(), key.empty() ? string::hash_of({}) : key.hash()) : NotFound;
        if (at == NotFound) {
            return *this;
        }
        vector<member> ms;
        ms.reserve(_bits - 1);
        for (size_t i = 0; i < _bits; ++i) {
            if (i != at) {
                ms.push_back(_members()[i]);
            }
        }
        return detail::JsonAccess::object_of(ms.data(), ms.size());
    }

    inline json json::set(size_t index, const json& value) const {
        if (!is_array() || index >= _bits) {
            return *this;
        }
        vector<json> es(_elements(), _elements() + _bits);
        es[index] = value;
        return detail::JsonAccess::array_of(es.data(), es.size());
    }

    inline json json::push_back(const json& value) const {
        vector<json> es;
        if (is_array()) {
            es.reserve(_bits + 1);
            es.insert(es.end(), _elements(), _elements() + _bits);
        }
        es.push_back(value);
        return detail::JsonAccess::array_of(es.data(), es.size());
    }

    inline json json::set_path(const string& pointer, const json& value) const {
        auto tokens = detail::pointer_tokens(pointer.view());
        if (!tokens) {
            return *this;
        }
        if (tokens->empty()) {
            return value;
        }
        // the containers on the way, top down; then rebuilt bottom up
        vector<json> chain;
        chain.push_back(*this);
        size_t n = tokens->size();
        for (size_t i = 0; i + 1 < n; ++i) {
            json& cur = chain.back();
            auto& t = (*tokens)[i];
            if (cur.is_null()) {
                cur = detail::JsonAccess::object_of(nullptr, 0);
            }
            json next;
            if (cur.is_object()) {
                size_t at = cur._find_index(t, string::hash_of(t));
                next = at == NotFound ? detail::JsonAccess::object_of(nullptr, 0) : cur._members()[at].value;
            } else if (cur.is_array()) {
                auto at = detail::pointer_index(t);
                if (!at || *at >= cur._bits) {
                    return *this;
                }
                next = cur._elements()[*at];
            } else {
                return *this;
            }
            chain.push_back(std::move(next));
        }
        json result;
        {
            json& cur = chain.back();
            auto& t = tokens->back();
            if (cur.is_null() || cur.is_object()) {
                result = cur.set(string(t), value);
            } else if (cur.is_array()) {
                if (t == "-") {
                    result = cur.push_back(value);
                } else {
                    auto at = detail::pointer_index(t);
                    if (!at || *at > cur._bits) {
                        return *this;
                    }
                    result = *at == cur._bits ? cur.push_back(value) : cur.set(*at, value);
                }
            } else {
                return *this;
            }
        }
        for (size_t i = n - 1; i-- > 0;) {
            json& parent = chain[i];
            auto& t = (*tokens)[i];
            if (parent.is_array()) {
                result = parent.set(*detail::pointer_index(t), result);
            } else {
                result = parent.set(string(t), result);
            }
        }
        return result;
    }

    // Makes an array or an object in a loop without copying it at every
    // step: push_back gathers the elements of an array, set the members
    // of an object (a key set twice keeps its last value); build() hands
    // out the value and leaves the builder empty. An empty builder builds
    // []. Mixing the two in one value is logic_error. A value of its own,
    // for one thread at a time.
    class json::builder {
    public:
        builder() = default;

        builder& push_back(const json& value) {
            if (_mode == Mode::object) {
                throw logic_error("sgcl: json::builder: push_back on a builder of an object");
            }
            _mode = Mode::array;
            _elements.push_back(value);
            return *this;
        }

        builder& set(const string& key, const json& value) {
            if (_mode == Mode::array) {
                throw logic_error("sgcl: json::builder: set on a builder of an array");
            }
            _mode = Mode::object;
            _members.push_back(member{key, value});
            return *this;
        }

        // The elements or the members so far (a key set twice counts twice)
        size_t size() const noexcept {
            return _mode == Mode::object ? _members.size() : _elements.size();
        }

        json build() {
            json out;
            if (_mode == Mode::object) {
                detail::JsonAccess::distinct(_members, 0, true);
                out = detail::JsonAccess::object_of(_members.data(), _members.size());
            } else {
                out = detail::JsonAccess::array_of(_elements.data(), _elements.size());
            }
            _elements.clear();
            _members.clear();
            _mode = Mode::none;
            return out;
        }

    private:
        enum class Mode : uint8_t {
            none,
            array,
            object
        };

        vector<json> _elements;
        vector<member> _members;
        Mode _mode = Mode::none;
    };

    // --- the reader ---

    // A token of json::reader: its kind and its text — a key's or a
    // string's characters with the escapes decoded, a number's literal,
    // "true", "false", "null", or the bracket. The text is a slice of the
    // reader's block: it holds the block, but the reader reuses it, so a
    // text kept past the next call of the reader is copied (string(t.text())).
    class json::token {
    public:
        enum class kind : uint8_t {
            begin_object,
            end_object,
            begin_array,
            end_array,
            key,
            string,
            number,
            boolean,
            null
        };

        token() = default;

        kind type() const noexcept {
            return _kind;
        }

        const slice<const char>& text() const noexcept {
            return _text;
        }

        optional<bool> as_bool() const noexcept {
            if (_kind != kind::boolean) {
                return nullopt;
            }
            return _text.size() == 4;
        }

        // A number whose value is an integer the type holds exactly (1e2 is 100)
        optional<int64_t> as_int() const noexcept {
            if (_kind != kind::number) {
                return nullopt;
            }
            return detail::exact_integer<int64_t>(_text.view());
        }

        optional<uint64_t> as_uint() const noexcept {
            if (_kind != kind::number) {
                return nullopt;
            }
            return detail::exact_integer<uint64_t>(_text.view());
        }

        // A number rounded to the nearest double; nullopt past its range
        optional<double> as_double() const noexcept {
            if (_kind != kind::number) {
                return nullopt;
            }
            return detail::floating_of<double>(_text.view());
        }

        // For the reader, which makes a token in place in its optional
        // (the tag is its own to name): the text's slice built once, not
        // made and moved twice — each slice holds a tracked_ptr, whose
        // making asks the thread's registration
        struct Made {
        private:
            friend class json::reader;
            Made() = default;
        };

        token(Made, kind k, const tracked_ptr<const void>& owner, const char* p, size_t n) noexcept
        : _kind(k), _text(owner, p, n) {
        }

        token(Made, kind k) noexcept
        : _kind(k) {
        }

    private:
        friend class json::reader;

        token(kind k, const slice<const char>& text) noexcept
        : _kind(k), _text(text) {
        }

        kind _kind = kind::null;
        slice<const char> _text;
    };

    namespace detail {
        // The keys met in each object a reader has open, to refuse one
        // given twice: a line of them while an object is small, a table of
        // their indexes by the keyed hash of the characters once it is not
        // (so that a key the input repeats a million times costs the same
        // as one it does not). No tracked pointer: the characters are
        // copied into a byte buffer.
        class KeySeen {
        public:
            void open() {
                _levels.push_back(Level{_entries.size(), _bytes.size(), {}});
            }

            void close() {
                auto& l = _levels.back();
                _entries.resize(l.first);
                _bytes.truncate(l.bytes);
                _levels.pop_back();
            }

            // false: the key was met before in the object open last. The
            // characters compared while the object is small; past that,
            // the keyed hash of each key, and a table
            bool insert(std::string_view key) {
                auto& l = _levels.back();
                size_t count = _entries.size() - l.first;
                size_t h = 0;
                if (count < SmallObject) {
                    for (size_t i = l.first; i < _entries.size(); ++i) {
                        if (_entries[i].size == key.size() && _key(i) == key) {
                            return false;
                        }
                    }
                } else {
                    h = string::hash_of(key);
                    size_t mask = l.table.size() - 1;
                    for (size_t s = h & mask; uint32_t e = l.table[s]; s = (s + 1) & mask) {
                        size_t i = l.first + e - 1;
                        if (_entries[i].hash == h && _key(i) == key) {
                            return false;
                        }
                    }
                }
                _entries.push_back(Entry{_bytes.size(), key.size(), h});
                _bytes.append(key);
                if (count + 1 == SmallObject) {
                    for (size_t i = l.first; i < _entries.size(); ++i) {
                        _entries[i].hash = string::hash_of(_key(i));
                    }
                    _rebuild(l, 64);
                } else if (count + 1 > SmallObject) {
                    if ((count + 1) * 2 > l.table.size()) {
                        _rebuild(l, l.table.size() * 2);
                    } else {
                        _place(l, _entries.size() - 1);
                    }
                }
                return true;
            }

        private:
            struct Entry {
                size_t offset;
                size_t size;
                size_t hash;
            };

            struct Level {
                size_t first;
                size_t bytes;
                std::vector<uint32_t> table;
            };

            std::string_view _key(size_t i) const noexcept {
                return _bytes.view().substr(_entries[i].offset, _entries[i].size);
            }

            void _place(Level& l, size_t i) {
                size_t mask = l.table.size() - 1;
                size_t s = _entries[i].hash & mask;
                while (l.table[s]) {
                    s = (s + 1) & mask;
                }
                l.table[s] = uint32_t(i - l.first + 1);
            }

            void _rebuild(Level& l, size_t size) {
                l.table.assign(size, 0);
                for (size_t i = l.first; i < _entries.size(); ++i) {
                    _place(l, i);
                }
            }

            std::vector<Entry> _entries;
            JsonText _bytes;   // appended to inline, not by a call into the library
            std::vector<Level> _levels;
        };
    }

    // JSON read piece by piece: the tokens one at a time (next), whether
    // the array or the object open has another element (more), the next
    // value whole as a json (read) or skipped (skip). From a text in
    // memory, or from a stream whose input it reads a block at a time —
    // a log of values one after another (NDJSON), an array of a million
    // records read one record at a time. Several values at the top level
    // follow one another, as Go's Decoder reads them.
    //
    // Every method that may reach into the stream does it on the thread
    // that calls it, and has a form for a task with async_ in front:
    // `r.next()`, `co_await r.async_next()` (a reader of a text never
    // waits). The reader keeps the first error and stops: a method
    // returns nullopt (or false) from then on, and
    // last_error() says what and where — its line and column counted
    // across the blocks already let go. A token, a string or a number cut
    // by the end of a block is not a case of its own: the scan stops
    // where the data ends and goes on from there when the next block
    // comes, and a value read whole is gathered in the block first (a
    // block that is too small grows).
    class json::reader {
    public:
        explicit reader(const string& text)
        : reader(text, options()) {
        }

        reader(const string& text, const options& o)
        : _text(text), _options(o), _eof(true) {
            _owner = text.as_slice().owner();
            _d = text.data();
            _n = text.size();
        }

        explicit reader(const io::reader& in)
        : reader(in, options()) {
        }

        reader(const io::reader& in, const options& o)
        : _in(in), _options(o) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // The byte of the input where the next token starts (or its white space)
        uint64_t offset() const noexcept {
            return _base + _pos;
        }

        // The arrays and objects open
        uint32_t depth() const noexcept {
            return uint32_t(_stack.size());
        }

    private:
        enum class Expect : uint8_t {
            top,           // a value, or the end of the input
            value,         // a value (after ',' in an array, after ':')
            first_value,   // a value or ']'
            key,           // a key (after ',' in an object)
            first_key,     // a key or '}'
            colon,
            after_value    // ',' or the end of the array or object
        };

        enum class Step : uint8_t {
            token,
            more,
            end,
            failed
        };

        enum class Pending : uint8_t {
            none,
            string,
            key,
            number,
            word
        };

        // --- the step every method is made of ---

        Step _step() {
            if (_error) {
                return Step::failed;
            }
            for (;;) {
                if (_pending != Pending::none) {
                    return _resume();
                }
                size_t i = size_t(detail::skip_space(_d + _pos, _d + _n) - _d);
                _pos = i;
                if (i == _n) {
                    if (!_eof) {
                        return Step::more;
                    }
                    if (_expect == Expect::top) {
                        return Step::end;
                    }
                    return _fail(errc::unexpected_end, i, "unexpected end of input, expected " + _expected());
                }
                char c = _d[i];
                switch (_expect) {
                    case Expect::after_value: {
                        bool object = _stack.back();
                        if (c == ',') {
                            ++_pos;
                            _expect = object ? Expect::key : Expect::value;
                            continue;
                        }
                        if (c == (object ? '}' : ']')) {
                            return _close(object);
                        }
                        return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + (object ? " after an object member, expected ',' or '}'" : " after an array element, expected ',' or ']'"));
                    }
                    case Expect::colon:
                        if (c == ':') {
                            ++_pos;
                            _expect = Expect::value;
                            continue;
                        }
                        return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " after an object key, expected ':'");
                    case Expect::first_key:
                        if (c == '}') {
                            return _close(true);
                        }
                        [[fallthrough]];
                    case Expect::key:
                        if (c != '"') {
                            return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " where a key was expected");
                        }
                        _begin(Pending::key);
                        continue;
                    case Expect::first_value:
                        if (c == ']') {
                            return _close(false);
                        }
                        [[fallthrough]];
                    case Expect::value:
                    case Expect::top:
                        switch (c) {
                            case '{':
                            case '[': {
                                if (_stack.size() >= _options.max_depth) {
                                    return _fail(errc::depth_limit, i, "nesting deeper than " + std::to_string(_options.max_depth));
                                }
                                bool object = c == '{';
                                _stack.push_back(object);
                                if (object && !_options.allow_duplicate_keys) {
                                    _keys.open();
                                }
                                _expect = object ? Expect::first_key : Expect::first_value;
                                _token(object ? token::kind::begin_object : token::kind::begin_array, i, 1);
                                ++_pos;
                                return Step::token;
                            }
                            case '"':
                                _begin(Pending::string);
                                continue;
                            case 't':
                            case 'f':
                            case 'n':
                                _begin(Pending::word);
                                continue;
                            default:
                                if (c == '-' || detail::is_digit(c)) {
                                    _begin(Pending::number);
                                    continue;
                                }
                                return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " where a value was expected");
                        }
                }
            }
        }

        std::string _expected() const {
            switch (_expect) {
                case Expect::after_value: return _stack.back() ? "',' or '}'" : "',' or ']'";
                case Expect::colon: return "':'";
                case Expect::first_key: return "a key or '}'";
                case Expect::key: return "a key";
                case Expect::first_value: return "a value or ']'";
                default: return "a value";
            }
        }

        void _begin(Pending p) {
            _pending = p;
            _tok_pos = _pos + (p == Pending::string || p == Pending::key ? 1 : 0);
            _sscan = {};
            _nstate = detail::NumberState::start;
            _plain = true;
        }

        void _token(token::kind k, size_t from, size_t n, bool scratch = false) {
            _kind = k;
            _text_from = from;
            _text_size = n;
            _text_scratch = scratch;
        }

        void _value_done() noexcept {
            _expect = _stack.empty() ? Expect::top : Expect::after_value;
        }

        Step _close(bool object) {
            _token(object ? token::kind::end_object : token::kind::end_array, _pos, 1);
            ++_pos;
            _stack.pop_back();
            if (object && !_options.allow_duplicate_keys) {
                _keys.close();
            }
            _value_done();
            return Step::token;
        }

        // A token cut by the end of the data, gone on with
        Step _resume() {
            const char* end = _d + _n;
            switch (_pending) {
                case Pending::string:
                case Pending::key: {
                    const char* start = _d + _pos + 1;
                    const char* p = _d + _tok_pos;
                    errc code = errc::syntax;
                    auto s = detail::scan_string(start, p, end, _eof, _sscan, _scratch, _options.allow_invalid_utf8, code);
                    _tok_pos = size_t(p - _d);
                    if (s == detail::ScanStatus::more) {
                        return Step::more;
                    }
                    if (s == detail::ScanStatus::failed) {
                        _pending = Pending::none;
                        return _fail_string(code, _tok_pos);
                    }
                    bool key = _pending == Pending::key;
                    _pending = Pending::none;
                    size_t from = _pos + 1;
                    size_t n = _tok_pos - 1 - from;
                    if (_sscan.decoded) {
                        _token(key ? token::kind::key : token::kind::string, 0, _scratch.size(), true);
                    } else {
                        _token(key ? token::kind::key : token::kind::string, from, n);
                    }
                    if (key && !_options.allow_duplicate_keys) {
                        std::string_view k = _sscan.decoded ? std::string_view(_scratch.data(), _scratch.size()) : std::string_view(_d + from, n);
                        if (!_keys.insert(k)) {
                            return _fail(errc::duplicate_key, _pos, "duplicate key \"" + std::string(k) + "\"");
                        }
                    }
                    _pos = _tok_pos;
                    if (key) {
                        _expect = Expect::colon;
                    } else {
                        _value_done();
                    }
                    return Step::token;
                }
                case Pending::number: {
                    const char* p = _d + _tok_pos;
                    auto s = detail::scan_number(p, end, _eof, _nstate, _plain);
                    _tok_pos = size_t(p - _d);
                    if (s == detail::ScanStatus::more) {
                        return Step::more;
                    }
                    _pending = Pending::none;
                    if (s == detail::ScanStatus::failed) {
                        if (p == end) {
                            return _fail(errc::unexpected_end, _tok_pos, "unexpected end of input inside a number");
                        }
                        return _fail(errc::syntax, _tok_pos, "invalid character " + detail::quoted_byte(uint8_t(*p)) + " in a number");
                    }
                    _token(token::kind::number, _pos, _tok_pos - _pos);
                    _pos = _tok_pos;
                    _value_done();
                    return Step::token;
                }
                case Pending::word: {
                    char c = _d[_pos];
                    std::string_view word = c == 't' ? "true" : c == 'f' ? "false" : "null";
                    const char* p = _d + _pos;
                    auto s = detail::scan_word(p, end, _eof, word);
                    if (s == detail::ScanStatus::more) {
                        return Step::more;
                    }
                    _pending = Pending::none;
                    size_t at = size_t(p - _d);
                    if (s == detail::ScanStatus::failed) {
                        if (p == end) {
                            return _fail(errc::unexpected_end, at, "unexpected end of input inside the literal " + std::string(word));
                        }
                        return _fail(errc::syntax, at, "invalid character " + detail::quoted_byte(uint8_t(*p)) + " in the literal " + std::string(word));
                    }
                    _token(c == 'n' ? token::kind::null : token::kind::boolean, _pos, word.size());
                    _pos = at;
                    _value_done();
                    return Step::token;
                }
                default:
                    return Step::failed;
            }
        }

        Step _fail_string(errc code, size_t at) {
            switch (code) {
                case errc::unexpected_end:
                    return _fail(code, at, "unexpected end of input inside a string");
                case errc::invalid_escape:
                    return _fail(code, at, "invalid escape sequence in a string");
                case errc::invalid_utf8:
                    return _fail(code, at, "invalid UTF-8 in a string");
                default:
                    return _fail(code, at, "invalid character " + detail::quoted_byte(uint8_t(_d[at])) + " in a string: a control character must be escaped");
            }
        }

        Step _fail(errc code, size_t at, std::string detail) {
            _set_error(error(code, _base + at, string(detail)), at);
            return Step::failed;
        }

        // The error, with the line and the column of the byte at of the
        // data: the lines of the blocks let go were counted when they went
        void _set_error(error e, size_t at) {
            if (_error) {
                return;
            }
            auto p = detail::position_of(std::string_view(_d, _n), at);
            uint32_t line = _lines + p.line;
            uint32_t column = p.line == 1 ? _column + p.column : p.column;
            e.set_position(line, column);
            _error = std::move(e);
        }

        optional<token> _result(Step s) {
            if (s != Step::token) {
                return nullopt;
            }
            if (_text_scratch) {
                if (!_scratch.size()) {
                    return optional<token>(std::in_place, token::Made(), _kind);
                }
                return optional<token>(std::in_place, token::Made(), _kind, _scratch.owner(), _scratch.data(), _scratch.size());
            }
            if (!_text_size) {
                return optional<token>(std::in_place, token::Made(), _kind);
            }
            return optional<token>(std::in_place, token::Made(), _kind, _owner, _d + _text_from, _text_size);
        }

        // --- the input ---

        // Room for more of the stream: the bytes before the one the reader
        // is at are let go (their line endings counted), and a block that
        // is full still grows
        slice<byte> _room() {
            if (_pos > 0) {
                auto gone = std::string_view(_d, _pos);
                auto nl = gone.rfind('\n');
                if (nl == std::string_view::npos) {
                    _column += uint32_t(detail::position_of(gone, gone.size()).column - 1);
                } else {
                    _lines += uint32_t(std::count(gone.begin(), gone.end(), '\n'));
                    _column = uint32_t(detail::position_of(gone.substr(nl + 1), gone.size()).column - 1);
                }
                _block.drop_front(_pos);
                _base += _pos;
                _tok_pos -= _pos;
                _ext_pos -= std::min(_ext_pos, _pos);
                _pos = 0;
            }
            if (_block.size() >= _options.max_token_size) {   // a token or a value of a stream longer than the bound: memory from the network is not unbounded
                _refresh();
                _set_error(error(errc::out_of_range, _base, string("a token longer than options.max_token_size (" + std::to_string(_options.max_token_size) + " bytes)")), 0);
                return slice<byte>();
            }
            if (_block.size() == _block.capacity()) {
                _block.reserve(_block.capacity() + 1);
            }
            _refresh();
            return slice<byte>(_block.owner(), reinterpret_cast<byte*>(_block.data() + _block.size()), _block.capacity() - _block.size());
        }

        void _received(const expected<size_t, io::error>& r) {
            if (_error) {
                return;
            }
            if (!r) {
                _set_error(error(r.error(), _base + _n), _n);
                return;
            }
            if (*r == 0) {
                _eof = true;
            } else {
                _block.resize(_block.size() + *r);
            }
            _refresh();
        }

        void _refresh() noexcept {
            _owner = _block.owner();
            _d = _block.data();
            _n = _block.size();
        }

        bool _fill() {
            auto room = _room();
            _received(_in.read(room));
            return !_error;
        }

        // --- a value whole: its extent in the block, then a parse ---

        // To the start of the next value: ',' and ':' passed over. value:
        // _pos is at it; end: the input ended between values; failed: an
        // end of an array or an object is next, or the text is wrong.
        Step _to_value() {
            if (_error) {
                return Step::failed;
            }
            if (_pending != Pending::none) {
                return _fail(errc::syntax, _pos, "a value read in the middle of a token");
            }
            for (;;) {
                size_t i = size_t(detail::skip_space(_d + _pos, _d + _n) - _d);
                _pos = i;
                if (i == _n) {
                    if (!_eof) {
                        return Step::more;
                    }
                    if (_expect == Expect::top) {
                        return Step::end;
                    }
                    return _fail(errc::unexpected_end, i, "unexpected end of input, expected " + _expected());
                }
                char c = _d[i];
                switch (_expect) {
                    case Expect::after_value:
                        if (c == ',') {
                            ++_pos;
                            _expect = _stack.back() ? Expect::key : Expect::value;
                            continue;
                        }
                        return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " where a value was expected");
                    case Expect::colon:
                        if (c == ':') {
                            ++_pos;
                            _expect = Expect::value;
                            continue;
                        }
                        return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " after an object key, expected ':'");
                    case Expect::first_key:
                    case Expect::key:
                        if (c != '"') {
                            return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " where a key was expected");
                        }
                        _ext_started = false;
                        return Step::token;
                    default:
                        if (c == ']' || c == '}') {
                            return _fail(errc::syntax, i, "invalid character " + detail::quoted_byte(uint8_t(c)) + " where a value was expected");
                        }
                        _ext_started = false;
                        return Step::token;
                }
            }
        }

        // Where the value at _pos ends, found without reading it: the
        // brackets counted outside strings. token: _ext_end is past it
        // (or at the end of the input, when the value is cut short: the
        // parse says so); more: the block ends inside it.
        Step _extent() {
            if (!_ext_started) {
                _ext_started = true;
                char c = _d[_pos];
                _ext_pos = _pos + 1;
                _ext_depth = 0;
                _ext_escape = false;
                _ext_in_string = c == '"';
                _ext_kind = c == '{' || c == '[' ? 2 : c == '"' ? 1 : 0;
                if (_ext_kind == 2) {
                    _ext_depth = 1;
                    if (_stack.size() + 1 > _options.max_depth) {
                        return _fail(errc::depth_limit, _pos, "nesting deeper than " + std::to_string(_options.max_depth));
                    }
                }
            }
            size_t i = _ext_pos;
            if (_ext_kind == 0) {
                while (i < _n) {
                    char c = _d[i];
                    if (detail::is_json_space(c) || c == ',' || c == ']' || c == '}' || c == ':' || c == '[' || c == '{' || c == '"') {
                        break;
                    }
                    ++i;
                }
                if (i == _n && !_eof) {
                    _ext_pos = i;
                    return Step::more;
                }
                _ext_end = i;
                return Step::token;
            }
            while (i < _n) {
                if (_ext_in_string) {
                    if (_ext_escape) {
                        _ext_escape = false;
                        ++i;
                        continue;
                    }
                    const char* q = detail::find_any_of(_d + i, _d + _n, '"', '\\', '"', '\\');
                    i = size_t(q - _d);
                    if (i == _n) {
                        break;
                    }
                    ++i;
                    if (*q == '\\') {
                        _ext_escape = true;
                        continue;
                    }
                    _ext_in_string = false;
                    if (_ext_kind == 1) {
                        _ext_end = i;
                        return Step::token;
                    }
                    continue;
                }
                char c = _d[i++];
                if (c == '"') {
                    _ext_in_string = true;
                } else if (c == '{' || c == '[') {
                    if (_stack.size() + ++_ext_depth > _options.max_depth) {
                        return _fail(errc::depth_limit, i - 1, "nesting deeper than " + std::to_string(_options.max_depth));
                    }
                } else if (c == '}' || c == ']') {
                    if (--_ext_depth == 0) {
                        _ext_end = i;
                        return Step::token;
                    }
                }
            }
            if (_eof) {
                _ext_end = _n;
                return Step::token;
            }
            _ext_pos = i;
            return Step::more;
        }

        // The value [_pos, _ext_end) read as a T, and the reader past it
        template<class T>
        optional<T> _typed_extent();

        // The value [_pos, _ext_end) parsed, and the reader past it
        optional<json> _parse_extent() {
            detail::JsonParser parser(_options);
            bool key = _expect == Expect::key || _expect == Expect::first_key;
            auto r = parser.parse(_d + _pos, _d + _ext_end, _base + _pos, _options.max_depth - uint32_t(_stack.size()));
            if (!r) {
                _set_error(std::move(r.error()), size_t(r.error().offset() - _base));
                return nullopt;
            }
            if (key) {
                if (!_options.allow_duplicate_keys && !_keys.insert(r->as_string()->view())) {
                    _fail(errc::duplicate_key, _pos, "duplicate key \"" + std::string(r->as_string()->view()) + "\"");
                    return nullopt;
                }
                _expect = Expect::colon;
            } else {
                _value_done();
            }
            _pos = _ext_end;
            return std::move(*r);
        }

    public:
        // The next token; nullopt at the end of the input or at an error
        optional<token> next() {
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    if (!_fill()) {
                        return nullopt;
                    }
                    continue;
                }
                return _result(s);
            }
        }

        // In a task: `co_await r.async_next()`
        async::task<optional<token>> async_next() {
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    auto room = _room();
                    _received(co_await _in.async_read(room));
                    if (_error) {
                        co_return nullopt;
                    }
                    continue;
                }
                co_return _result(s);
            }
        }


        // Whether the array or the object open has another element (at the
        // top level: whether another value follows). It only looks: the
        // element is read by what comes next.
        bool more() {
            for (;;) {
                if (auto m = _more_step()) {
                    return *m;
                }
                if (!_fill()) {
                    return false;
                }
            }
        }

        async::task<bool> async_more() {
            for (;;) {
                if (auto m = _more_step()) {
                    co_return *m;
                }
                auto room = _room();
                _received(co_await _in.async_read(room));
                if (_error) {
                    co_return false;
                }
            }
        }

        // The next value, whole, as a json; nullopt at the end of the input
        // or at an error. Where a key is next, the key as a string.
        optional<json> read() {
            for (Step s;;) {
                s = _to_value();
                if (s == Step::more) {
                    if (!_fill()) {
                        return nullopt;
                    }
                    continue;
                }
                if (s != Step::token) {
                    return nullopt;
                }
                break;
            }
            for (;;) {
                Step s = _extent();
                if (s == Step::more) {
                    if (!_fill()) {
                        return nullopt;
                    }
                    continue;
                }
                if (s != Step::token) {
                    return nullopt;
                }
                return _parse_extent();
            }
        }

        async::task<optional<json>> async_read() {
            for (;;) {
                Step s = _to_value();
                if (s == Step::more) {
                    auto room = _room();
                    _received(co_await _in.async_read(room));
                    if (_error) {
                        co_return nullopt;
                    }
                    continue;
                }
                if (s != Step::token) {
                    co_return nullopt;
                }
                break;
            }
            for (;;) {
                Step s = _extent();
                if (s == Step::more) {
                    auto room = _room();
                    _received(co_await _in.async_read(room));
                    if (_error) {
                        co_return nullopt;
                    }
                    continue;
                }
                if (s != Step::token) {
                    co_return nullopt;
                }
                co_return _parse_extent();
            }
        }



        // The next value as a T (fields.h); nullopt at the end of the input
        // or at an error, which last_error() gives with its path
        template<class T>
        optional<T> read();

        template<class T>
        async::task<optional<T>> async_read();

        // The next value checked and passed over (a key and its value where
        // a key is next): false at the end of the input or at an error
        bool skip() {
            size_t depth = 0;
            bool started = false;
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    if (!_fill()) {
                        return false;
                    }
                    continue;
                }
                if (s != Step::token) {
                    return false;
                }
                if (!started) {
                    if (!_skip_start(s, depth)) {
                        return false;
                    }
                    started = true;
                }
                if (_skipped(depth)) {
                    return true;
                }
            }
        }

        async::task<bool> async_skip() {
            size_t depth = 0;
            bool started = false;
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    auto room = _room();
                    _received(co_await _in.async_read(room));
                    if (_error) {
                        co_return false;
                    }
                    continue;
                }
                if (s != Step::token) {
                    co_return false;
                }
                if (!started) {
                    if (!_skip_start(s, depth)) {
                        co_return false;
                    }
                    started = true;
                }
                if (_skipped(depth)) {
                    co_return true;
                }
            }
        }

    private:
        // more: nullopt when the data so far cannot tell
        optional<bool> _more_step() {
            if (_error) {
                return false;
            }
            size_t i = size_t(detail::skip_space(_d + _pos, _d + _n) - _d);
            _pos = i;
            if (i == _n) {
                if (!_eof) {
                    return nullopt;
                }
                return false;
            }
            if (_stack.empty()) {
                return true;
            }
            return _d[i] != ']' && _d[i] != '}';
        }

        // skip: whether the tokens so far make a value whole (a key alone
        // does not, nor an array still open)
        bool _skipped(size_t depth) const noexcept {
            return _stack.size() == depth && _kind != token::kind::key;
        }

        bool _skip_start(Step s, size_t& depth) {
            // the first token of the value: an end of an array or an object is not one
            if (_kind == token::kind::end_array || _kind == token::kind::end_object) {
                _fail(errc::syntax, _pos - 1, "skip() where an array or an object ends");
                return false;
            }
            (void)s;
            depth = _stack.size() - (_kind == token::kind::begin_array || _kind == token::kind::begin_object ? 1 : 0);
            return true;
        }

        // the input: the text, or the block of the stream
        string _text;
        io::reader _in;
        detail::TextBuffer _block;
        tracked_ptr<const void> _owner;
        const char* _d = nullptr;
        size_t _n = 0;
        size_t _pos = 0;           // the next byte to look at
        uint64_t _base = 0;        // the offset of _d[0] in the input
        uint32_t _lines = 0;       // the line endings let go with the blocks
        uint32_t _column = 0;      // the characters after the last of them
        options _options;
        bool _eof = false;
        // the structure
        std::vector<bool> _stack;  // the arrays (false) and objects (true) open
        Expect _expect = Expect::top;
        detail::KeySeen _keys;
        // a token cut by the end of the data
        Pending _pending = Pending::none;
        size_t _tok_pos = 0;
        detail::StringScan _sscan;
        detail::NumberState _nstate = detail::NumberState::start;
        bool _plain = true;
        detail::TextBuffer _scratch;
        // the token found last
        token::kind _kind = token::kind::null;
        size_t _text_from = 0;
        size_t _text_size = 0;
        bool _text_scratch = false;
        // the extent of a value read whole
        size_t _ext_pos = 0;
        size_t _ext_end = 0;
        uint32_t _ext_depth = 0;
        uint8_t _ext_kind = 0;     // 0 a number or a word, 1 a string, 2 an array or an object
        bool _ext_started = false;
        bool _ext_in_string = false;
        bool _ext_escape = false;
        optional<error> _error;
    };

    inline expected<json, json::error> json::parse(const io::reader& in, const options& o) {
        reader r(in, o);
        auto v = r.read();
        if (v && !r.more() && !r.last_error()) {
            return std::move(*v);
        }
        if (r.last_error()) {
            return unexpected<error>(*r.last_error());
        }
        if (!v) {
            return unexpected<error>(error(errc::unexpected_end, r.offset(), "unexpected end of input, expected a value"));
        }
        return unexpected<error>(error(errc::syntax, r.offset(), "a character after the value"));
    }

    inline expected<json, json::error> json::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline async::task<expected<json, json::error>> json::async_parse(io::reader in, options o) {
        reader r(std::move(in), o);
        auto v = co_await r.async_read();
        bool more = v ? co_await r.async_more() : false;
        if (v && !more && !r.last_error()) {
            co_return std::move(*v);
        }
        if (r.last_error()) {
            co_return unexpected<error>(*r.last_error());
        }
        if (!v) {
            co_return unexpected<error>(error(errc::unexpected_end, r.offset(), "unexpected end of input, expected a value"));
        }
        co_return unexpected<error>(error(errc::syntax, r.offset(), "a character after the value"));
    }

    inline async::task<expected<json, json::error>> json::async_parse(const io::reader& in) {
        return async_parse(in, options());
    }

    // --- the writer ---

    // JSON written piece by piece into a stream: begin_object, key, value,
    // end_object... chained, the text gathered in the writer and handed to
    // the stream by flush(). A mistake in the structure (an end with no
    // beginning, a key outside an object, a value where a key belongs, NaN)
    // is kept and reported by flush(), not at every step, and what comes
    // after it is not written. Each value at the top level is followed by
    // a line ending, as Go's Encoder writes it: a stream of values (NDJSON).
    // A long stream of values is flushed in the loop: what is not flushed
    // is held in memory.
    class json::writer {
    public:
        explicit writer(const io::writer& out)
        : writer(out, compact) {
        }

        writer(const io::writer& out, const style& s)
        : _out(s.indent, s.escape_html, true), _sink(out), _style(s) {
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;

        writer& begin_object() {
            _out.begin(true);
            return *this;
        }

        writer& end_object() {
            _out.end(true);
            return *this;
        }

        writer& begin_array() {
            _out.begin(false);
            return *this;
        }

        writer& end_array() {
            _out.end(false);
            return *this;
        }

        writer& key(const string& name) {
            _out.key(name.view());
            return *this;
        }

        template<size_t N>
        writer& key(const char (&name)[N]) {
            _out.key(std::string_view(name, std::char_traits<char>::length(name)));
            return *this;
        }

        writer& value(std::nullptr_t) {
            _out.null();
            return *this;
        }

        writer& value(const char* text) {
            _out.quoted(std::string_view(text));
            return *this;
        }

        // A json, a boolean, a number, a text (string, slice, std::string)
        template<class T>
        writer& value(const T& v);

        // The text so far to the stream: the first mistake in the structure
        // (an io::error of the encoding category), or the stream's failure
        expected<void, io::error> flush() {
            if (auto e = _check()) {
                return io::detail::fail(*e);
            }
            if (!_out.text().empty()) {
                auto w = _sink.write(_pending());
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
                _out.text().clear();
            }
            return {};
        }

        // In a task: `co_await w.async_flush()`
        async::task<expected<void, io::error>> async_flush() {
            if (auto e = _check()) {
                co_return io::detail::fail(*e);
            }
            if (!_out.text().empty()) {
                auto w = co_await _sink.async_write(_pending());
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
                _out.text().clear();
            }
            co_return expected<void, io::error>();
        }

    private:
        optional<io::error> _check() {
            if (_error) {
                return _error;
            }
            if (_out.failed()) {
                _error = io::error(make_error_code(_out.code()), string("json: " + _out.detail()));
                return _error;
            }
            return nullopt;
        }

        slice<const byte> _pending() const noexcept {
            auto& t = const_cast<detail::JsonOut&>(_out).text();
            return slice<const byte>(reinterpret_cast<const byte*>(t.data()), t.size());
        }

        template<class T>
        friend struct detail::JsonValueWriter;

        detail::JsonOut _out;
        io::writer _sink;
        style _style;
        optional<io::error> _error;
    };

    namespace detail {
        // What json::writer::value and stringify write for a value of T:
        // the scalars here; a typed value by its fields (fields.h, the
        // specialization for the rest)
        template<class T>
        struct JsonScalar : std::false_type {};

        template<class T>
        requires std::same_as<T, json> || std::same_as<T, bool> || JsonInteger<T> || std::floating_point<T> || std::same_as<T, string>
            || std::same_as<T, slice<const char>> || std::same_as<T, std::string> || std::same_as<T, std::string_view>
        struct JsonScalar<T> : std::true_type {};

        template<class T>
        void write_scalar_value(JsonOut& out, const T& v) {
            if constexpr (std::is_same_v<T, json>) {
                write_json(out, v);
            } else if constexpr (std::is_same_v<T, bool>) {
                out.boolean(v);
            } else if constexpr (JsonInteger<T>) {
                out.integer(v);
            } else if constexpr (std::is_floating_point_v<T>) {
                if constexpr (std::is_same_v<T, long double>) {
                    out.floating(double(v));
                } else {
                    out.floating(v);
                }
            } else if constexpr (std::is_same_v<T, string> || std::is_same_v<T, slice<const char>>) {
                out.quoted(v.view());
            } else {
                out.quoted(std::string_view(v));
            }
        }

        template<class T>
        bool write_typed(JsonOut& out, const T& v, const json::style& s);

        template<class T>
        struct JsonValueWriter {
            static void write(json::writer& w, const T& v) {
                if constexpr (JsonScalar<T>::value) {
                    write_scalar_value(w._out, v);
                } else {
                    write_typed(w._out, v, w._style);
                }
            }
        };
    }

    template<class T>
    json::writer& json::writer::value(const T& v) {
        detail::JsonValueWriter<T>::write(*this, v);
        return *this;
    }

    // --- typed values ---

    namespace detail {
        template<class T>
        json JsonHooks<T>::to_json(const void* p) {
            if constexpr (std::is_same_v<T, json>) {
                return *static_cast<const json*>(p);
            } else {
                return json(static_cast<const T*>(p)->to_json());
            }
        }

        template<class T>
        bool JsonHooks<T>::from_json(void* p, const json& j) {
            if constexpr (std::is_same_v<T, json>) {
                *static_cast<json*>(p) = j;
                return true;
            } else {
                auto r = T::from_json(j);
                if (!r) {
                    return false;
                }
                *static_cast<T*>(p) = std::move(*r);
                return true;
            }
        }

        // The options of a field that reach its values — an enum's names,
        // a variant's tag, as_string — through an optional, a pointer and
        // the elements of a container, but not into the fields of a record
        struct FieldOptions {
            const field_list* list = nullptr;
            const FieldInfo* info = nullptr;

            bool as_string() const noexcept {
                return info && (info->flags & AsString);
            }

            size_t name_count() const noexcept {
                return info ? info->names_count : 0;
            }

            std::string_view name(size_t i) const noexcept {
                return FieldAccess::name(*list, *info, i);
            }

            size_t index_of(std::string_view n) const noexcept {
                return FieldAccess::index_of(*list, *info, n);
            }

            const char* tag() const noexcept {
                return info ? info->tag : nullptr;
            }
        };

        // The path of a failure, gathered on the way out of the values it
        // is in: a segment a level, the innermost first
        class ErrorPath {
        public:
            void key(std::string_view k) {
                std::string e;
                for (char c : k) {
                    if (c == '~') {
                        e += "~0";
                    } else if (c == '/') {
                        e += "~1";
                    } else {
                        e += c;
                    }
                }
                _segments.push_back(std::move(e));
            }

            void index(size_t i) {
                _segments.push_back(std::to_string(i));
            }

            string text() const {
                std::string t;
                for (auto it = _segments.rbegin(); it != _segments.rend(); ++it) {
                    t += '/';
                    t += *it;
                }
                return string(t);
            }

        private:
            std::vector<std::string> _segments;
        };

        inline const char* found_kind(const char* p, const char* end) noexcept {
            if (p == end) {
                return "the end of the input";
            }
            switch (*p) {
                case '{': return "an object";
                case '[': return "an array";
                case '"': return "a string";
                case 't':
                case 'f': return "a boolean";
                case 'n': return "null";
                default: return "a number";
            }
        }

        // A value of the text checked and passed over: iterative, its keys
        // checked for one given twice as a parse checks them
        inline bool skip_value(JsonCursor& c, uint32_t max_depth) {
            std::vector<bool> stack;   // the objects (true) and arrays open
            KeySeen keys;
            bool unique = !c.options.allow_duplicate_keys;
            for (;;) {
                c.space();
                if (c.at_end()) {
                    return c.fail_end("a value");
                }
                char ch = *c.p;
                if (ch == '{' || ch == '[') {
                    if (stack.size() >= max_depth) {
                        return c.fail(errc::depth_limit, c.p, "nesting deeper than " + std::to_string(max_depth));
                    }
                    bool object = ch == '{';
                    ++c.p;
                    c.space();
                    if (!c.at_end() && *c.p == (object ? '}' : ']')) {
                        ++c.p;
                    } else {
                        stack.push_back(object);
                        if (object) {
                            if (unique) {
                                keys.open();
                            }
                            goto key;
                        }
                        continue;
                    }
                } else if (ch == '"') {
                    std::string_view s;
                    if (!c.string_token(s)) {
                        return false;
                    }
                } else if (ch == 't' || ch == 'f' || ch == 'n') {
                    if (!c.word(ch == 't' ? "true" : ch == 'f' ? "false" : "null")) {
                        return false;
                    }
                } else if (ch == '-' || is_digit(ch)) {
                    std::string_view lit;
                    bool plain;
                    if (!c.number_token(lit, plain)) {
                        return false;
                    }
                } else {
                    return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " where a value was expected");
                }
                // after a value
                for (;;) {
                    if (stack.empty()) {
                        return true;
                    }
                    bool object = stack.back();
                    c.space();
                    if (c.at_end()) {
                        return c.fail_end(object ? "',' or '}'" : "',' or ']'");
                    }
                    if (*c.p == ',') {
                        ++c.p;
                        if (object) {
                            goto key;
                        }
                        break;
                    }
                    if (*c.p != (object ? '}' : ']')) {
                        return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + (object ? " after an object member, expected ',' or '}'" : " after an array element, expected ',' or ']'"));
                    }
                    ++c.p;
                    if (object && unique) {
                        keys.close();
                    }
                    stack.pop_back();
                }
                continue;
            key:
                c.space();
                if (c.at_end()) {
                    return c.fail_end("a key");
                }
                if (*c.p != '"') {
                    return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " where a key was expected");
                }
                {
                    const char* at = c.p;
                    std::string_view k;
                    if (!c.string_token(k)) {
                        return false;
                    }
                    if (unique && !keys.insert(k)) {
                        return c.fail(errc::duplicate_key, at, "duplicate key \"" + std::string(k) + "\"");
                    }
                }
                c.space();
                if (c.at_end()) {
                    return c.fail_end("':'");
                }
                if (*c.p != ':') {
                    return c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after an object key, expected ':'");
                }
                ++c.p;
            }
        }

        // A typed value read from the text through a cursor, the type
        // known by its table of operations: recursive by the type, as
        // deep as the input nests its values, which max_depth bounds
        class JsonTypedReader {
        public:
            JsonTypedReader(JsonCursor& c, uint32_t max_depth) noexcept
            : _c(c), _max(max_depth) {
            }

            ErrorPath path;

            // The recursion goes through value() and the readers of the
            // containers and records alone, each with a small frame: a
            // record 512 deep is read on a thread of 512 KB of stack (and
            // under ASan); what is not a container is read in a frame of
            // its own, left before the next level
            bool value(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                _c.space();
                if (_c.at_end()) {
                    return _c.fail_end("a value");
                }
                // through optionals and pointers without a frame each
                while (ops->kind == ValueKind::optional || ops->kind == ValueKind::pointer) {
                    if (*_c.p == 'n') {
                        return _null(p, ops);
                    }
                    p = ops->emplace(p);
                    ops = ops->inner();
                }
                if (*_c.p == 'n' && ops->kind != ValueKind::json && ops->kind != ValueKind::custom_json) {
                    return _null(p, ops);
                }
                switch (ops->kind) {
                    case ValueKind::sequence:
                    case ValueKind::set:
                        return _sequence(p, ops, opt, depth);
                    case ValueKind::fixed:
                    case ValueKind::tuple:
                        return _fixed(p, ops, opt, depth);
                    case ValueKind::map:
                        return _map(p, ops, opt, depth);
                    case ValueKind::record:
                        return _record(p, ops, depth, {});
                    case ValueKind::variant:
                        return _variant(p, ops, opt, depth);
                    default:
                        return _leaf(p, ops, opt, depth);
                }
            }

            // The fields of a record from an object; the key `ignore` (a
            // variant's tag) passed over. What a record needs across its
            // members — the list of its fields, the ones seen — is in a
            // frame of the reader's kept for its depth, not on the stack
            bool _record(void* p, const ValueOps* ops, uint32_t depth, std::string_view ignore) {
                if (*_c.p != '{') {
                    return _mismatch(ops);
                }
                RecordFrame& r = _frame(depth);
                if (!_record_begin(p, ops, depth, r)) {
                    return false;
                }
                while (!r.done) {
                    int got = _member_key(r, ignore);
                    if (got < 0) {
                        return false;
                    }
                    if (got == 1) {
                        if (!value(r.info().address, r.info().ops, r.options, depth + 1)) {
                            path.key(r.info().name);
                            return false;
                        }
                    } else if (!skip_value(_c, _max > depth ? _max - depth : 0)) {
                        return false;
                    }
                    if (!_member_end(r)) {
                        return false;
                    }
                }
                return _record_end(r);
            }

        private:
            KeySeen _keys;

            struct RecordFrame {
                field_list fields;
                std::vector<bool> seen;
                FieldOptions options;
                size_t field = 0;
                bool unique = true;
                bool done = false;

                const FieldInfo& info() const noexcept {
                    return FieldAccess::fields(fields)[field];
                }
            };

            // The reader's frames of records, one a depth, reused
            std::vector<std::unique_ptr<RecordFrame>> _frames;

            RecordFrame& _frame(uint32_t depth) {
                while (_frames.size() <= depth) {
                    _frames.push_back(std::make_unique<RecordFrame>());
                }
                return *_frames[depth];
            }

            SGCL_NOINLINE bool _record_begin(void* p, const ValueOps* ops, uint32_t depth, RecordFrame& r) {
                if (!_open(depth)) {
                    return false;
                }
                FieldAccess::clear(r.fields);
                ops->describe(p, r.fields);
                r.seen.assign(FieldAccess::fields(r.fields).size(), false);
                r.unique = !_c.options.allow_duplicate_keys;
                if (r.unique) {
                    _keys.open();
                }
                ++_c.p;
                _c.space();
                r.done = !_c.at_end() && *_c.p == '}';
                if (r.done) {
                    ++_c.p;
                }
                return true;
            }

            SGCL_NOINLINE bool _record_end(RecordFrame& r) {
                if (r.unique) {
                    _keys.close();
                }
                auto& list = FieldAccess::fields(r.fields);
                for (size_t f = 0; f < list.size(); ++f) {
                    if (!r.seen[f] && (list[f].flags & Required)) {
                        _c.fail(errc::missing_field, _c.p - 1, "missing field");
                        path.key(list[f].name);
                        return false;
                    }
                }
                return true;
            }

            // A member's key and its colon: 1, a field (r.field); 0, a key
            // no field has (to be skipped); -1, an error (a key given
            // twice, an unknown one refused, the text wrong)
            SGCL_NOINLINE int _member_key(RecordFrame& r, std::string_view ignore) {
                _c.space();
                if (_c.at_end()) {
                    _c.fail_end("a key");
                    return -1;
                }
                if (*_c.p != '"') {
                    _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " where a key was expected");
                    return -1;
                }
                const char* at = _c.p;
                std::string_view k;
                if (!_c.string_token(k)) {
                    return -1;
                }
                if (r.unique && !_keys.insert(k)) {
                    _c.fail(errc::duplicate_key, at, "duplicate key \"" + std::string(k) + "\"");
                    return -1;
                }
                auto& list = FieldAccess::fields(r.fields);
                size_t f = 0;
                while (f < list.size() && list[f].name != k) {
                    ++f;
                }
                bool known = f < list.size();
                if (!known && k != ignore && _c.options.reject_unknown_fields) {
                    _c.fail(errc::unknown_field, at, "unknown field \"" + std::string(k) + "\"");
                    return -1;
                }
                if (!_colon()) {
                    return -1;
                }
                if (known) {
                    r.field = f;
                    r.seen[f] = true;
                    r.options = FieldOptions{&r.fields, &list[f]};
                }
                return known ? 1 : 0;
            }

            // After a member: another member, or the '}' (r.done); false: an error
            SGCL_NOINLINE bool _member_end(RecordFrame& r) {
                _c.space();
                if (_c.at_end()) {
                    return _c.fail_end("',' or '}'");
                }
                if (*_c.p == ',') {
                    ++_c.p;
                    return true;
                }
                if (*_c.p != '}') {
                    return _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " after an object member, expected ',' or '}'");
                }
                ++_c.p;
                r.done = true;
                return true;
            }

            // null into a container, a record or a variant: its default
            // value; into an optional or a pointer: nothing
            SGCL_NOINLINE bool _null(void* p, const ValueOps* ops) {
                if (!_c.word("null")) {
                    return false;
                }
                if (ops->kind == ValueKind::optional || ops->kind == ValueKind::pointer) {
                    ops->reset(p);
                } else {
                    ops->clear(p);
                }
                return true;
            }

            // A value that is not a container: in its own frame
            SGCL_NOINLINE bool _leaf(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                char ch = *_c.p;
                if (ch == 'n' && ops->kind != ValueKind::json && ops->kind != ValueKind::custom_json) {
                    return _null(p, ops);
                }
                switch (ops->kind) {
                    case ValueKind::boolean:
                        return _boolean(p, ops, opt);
                    case ValueKind::signed_integer:
                    case ValueKind::unsigned_integer:
                    case ValueKind::floating:
                        return _number(p, ops, opt);
                    case ValueKind::enumeration:
                        if (opt.name_count()) {
                            return _named(p, ops, opt);
                        }
                        return _number(p, ops, opt);
                    case ValueKind::string:
                    case ValueKind::text: {
                        if (ch != '"') {
                            return _mismatch(ops);
                        }
                        const char* at = _c.p;
                        std::string_view s;
                        if (!_c.string_token(s)) {
                            return false;
                        }
                        if (!ops->set_text(p, s)) {
                            return _c.fail(errc::type_mismatch, at, "\"" + std::string(s.substr(0, 40)) + "\" is not a valid value of the field");
                        }
                        return true;
                    }
                    case ValueKind::json:
                    case ValueKind::custom_json: {
                        const char* at = _c.p;
                        JsonParser parser(_c.options);
                        json j;
                        if (!parser._value(_c, _max > depth ? _max - depth : 0, j)) {
                            return false;
                        }
                        if (!ops->from_json(p, j)) {
                            return _c.fail(errc::type_mismatch, at, "the value is not one the field's from_json takes");
                        }
                        return true;
                    }
                    default:
                        return _mismatch(ops);
                }
            }

            bool _mismatch(const ValueOps* ops) {
                return _c.fail(errc::type_mismatch, _c.p, std::string("expected ") + ops->name + ", found " + found_kind(_c.p, _c.end));
            }

            bool _open(uint32_t depth) {
                if (depth >= _max) {
                    return _c.fail(errc::depth_limit, _c.p, "nesting deeper than " + std::to_string(_max));
                }
                return true;
            }

            bool _colon() {
                _c.space();
                if (_c.at_end()) {
                    return _c.fail_end("':'");
                }
                if (*_c.p != ':') {
                    return _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " after an object key, expected ':'");
                }
                ++_c.p;
                return true;
            }

            bool _boolean(void* p, const ValueOps* ops, const FieldOptions& opt) {
                char ch = *_c.p;
                if (opt.as_string()) {
                    if (ch != '"') {
                        return _mismatch(ops);
                    }
                    const char* at = _c.p;
                    std::string_view s;
                    if (!_c.string_token(s)) {
                        return false;
                    }
                    if (s != "true" && s != "false") {
                        return _c.fail(errc::type_mismatch, at, "expected \"true\" or \"false\"");
                    }
                    ops->set_bool(p, s == "true");
                    return true;
                }
                if (ch != 't' && ch != 'f') {
                    return _mismatch(ops);
                }
                if (!_c.word(ch == 't' ? "true" : "false")) {
                    return false;
                }
                ops->set_bool(p, ch == 't');
                return true;
            }

            bool _number(void* p, const ValueOps* ops, const FieldOptions& opt) {
                const char* at = _c.p;
                std::string_view lit;
                if (opt.as_string()) {
                    if (*_c.p != '"') {
                        return _mismatch(ops);
                    }
                    if (!_c.string_token(lit)) {
                        return false;
                    }
                } else {
                    if (*_c.p != '-' && !is_digit(*_c.p)) {
                        return _mismatch(ops);
                    }
                    bool plain;
                    if (!_c.number_token(lit, plain)) {
                        return false;
                    }
                }
                switch (ops->set_literal(p, lit)) {
                    case 0:
                        return true;
                    case 1:
                        return _c.fail(errc::type_mismatch, at, "expected " + std::string(ops->name) + ", found " + std::string(lit.substr(0, 40)));
                    default:
                        return _c.fail(errc::out_of_range, at, "the number " + std::string(lit.substr(0, 40)) + " is out of the field's range");
                }
            }

            bool _named(void* p, const ValueOps* ops, const FieldOptions& opt) {
                if (*_c.p != '"') {
                    return _c.fail(errc::type_mismatch, _c.p, std::string("expected a name, found ") + found_kind(_c.p, _c.end));
                }
                const char* at = _c.p;
                std::string_view s;
                if (!_c.string_token(s)) {
                    return false;
                }
                size_t i = opt.index_of(s);
                if (i == opt.name_count() || !ops->set_int(p, int64_t(i))) {
                    return _c.fail(errc::type_mismatch, at, "\"" + std::string(s.substr(0, 40)) + "\" is none of the field's names");
                }
                return true;
            }

            struct Elements {
                JsonTypedReader* self;
                const ValueOps* inner;
                const FieldOptions* opt;
                uint32_t depth;
                size_t index = 0;
                bool first = true;
            };

            bool _sequence(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                if (*_c.p != '[') {
                    return _mismatch(ops);
                }
                if (!_open(depth)) {
                    return false;
                }
                ++_c.p;
                Elements e{this, ops->inner(), &opt, depth + 1};
                auto more = [](void* ctx) -> bool {
                    auto& e = *static_cast<Elements*>(ctx);
                    auto& c = e.self->_c;
                    c.space();
                    if (c.at_end()) {
                        c.fail_end(e.first ? "a value or ']'" : "',' or ']'");
                        return false;
                    }
                    if (*c.p == ']') {
                        ++c.p;
                        return false;
                    }
                    if (!e.first) {
                        if (*c.p != ',') {
                            c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after an array element, expected ',' or ']'");
                            return false;
                        }
                        ++c.p;
                    }
                    e.first = false;
                    return true;
                };
                auto read = [](void* ctx, void* v) -> bool {
                    auto& e = *static_cast<Elements*>(ctx);
                    if (!e.self->value(v, e.inner, *e.opt, e.depth)) {
                        e.self->path.index(e.index);
                        return false;
                    }
                    ++e.index;
                    return true;
                };
                return ops->read_elements(p, &e, more, read) && !_c.failure;
            }

            bool _fixed(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                if (*_c.p != '[') {
                    return _mismatch(ops);
                }
                if (!_open(depth)) {
                    return false;
                }
                const char* at = _c.p;
                ++_c.p;
                size_t n = ops->fixed_count;
                auto wrong_count = [&] {
                    return _c.fail(errc::type_mismatch, at, "expected an array of " + std::to_string(n) + " elements");
                };
                for (size_t i = 0; i < n; ++i) {
                    _c.space();
                    if (_c.at_end()) {
                        return _c.fail_end("a value");
                    }
                    if (*_c.p == ']') {
                        return wrong_count();
                    }
                    if (i) {
                        if (*_c.p != ',') {
                            return _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " after an array element, expected ',' or ']'");
                        }
                        ++_c.p;
                    }
                    const ValueOps* eo = nullptr;
                    void* ep = ops->element(p, i, &eo);
                    if (!value(ep, eo, ops->kind == ValueKind::fixed ? opt : FieldOptions{}, depth + 1)) {
                        path.index(i);
                        return false;
                    }
                }
                _c.space();
                if (_c.at_end()) {
                    return _c.fail_end("']'");
                }
                if (*_c.p != ']') {
                    return *_c.p == ',' ? wrong_count() : _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " after an array element, expected ']'");
                }
                ++_c.p;
                return true;
            }

            struct Entries {
                JsonTypedReader* self;
                const ValueOps* inner;
                const FieldOptions* opt;
                uint32_t depth;
                KeySeen keys;
                std::string key;
                const char* key_at = nullptr;
                bool unique;
                bool first = true;
            };

            bool _map(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                if (*_c.p != '{') {
                    return _mismatch(ops);
                }
                if (!_open(depth)) {
                    return false;
                }
                ++_c.p;
                Entries e{this, ops->inner(), &opt, depth + 1, {}, {}, nullptr, !_c.options.allow_duplicate_keys};
                if (e.unique) {
                    e.keys.open();
                }
                auto next = [](void* ctx, std::string_view& key) -> bool {
                    auto& e = *static_cast<Entries*>(ctx);
                    auto& c = e.self->_c;
                    c.space();
                    if (c.at_end()) {
                        c.fail_end(e.first ? "a key or '}'" : "',' or '}'");
                        return false;
                    }
                    if (*c.p == '}') {
                        ++c.p;
                        return false;
                    }
                    if (!e.first) {
                        if (*c.p != ',') {
                            c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after an object member, expected ',' or '}'");
                            return false;
                        }
                        ++c.p;
                        c.space();
                    }
                    e.first = false;
                    if (c.at_end() || *c.p != '"') {
                        c.fail(c.at_end() ? errc::unexpected_end : errc::syntax, c.p, c.at_end() ? "unexpected end of input, expected a key" : "invalid character " + char_name(c.p, c.end) + " where a key was expected");
                        return false;
                    }
                    e.key_at = c.p;
                    std::string_view k;
                    if (!c.string_token(k)) {
                        return false;
                    }
                    if (e.unique && !e.keys.insert(k)) {
                        c.fail(errc::duplicate_key, e.key_at, "duplicate key \"" + std::string(k) + "\"");
                        return false;
                    }
                    e.key.assign(k);
                    if (!e.self->_colon()) {
                        return false;
                    }
                    key = e.key;
                    return true;
                };
                auto read = [](void* ctx, void* v) -> bool {
                    auto& e = *static_cast<Entries*>(ctx);
                    std::string key = e.key;
                    if (!e.self->value(v, e.inner, *e.opt, e.depth)) {
                        e.self->path.key(key);
                        return false;
                    }
                    return true;
                };
                auto status = ops->read_entries(p, &e, next, read);
                if (status == EntryStatus::bad_key) {
                    return _c.fail(errc::type_mismatch, e.key_at, "\"" + e.key.substr(0, 40) + "\" is not a key of the map's type");
                }
                return status == EntryStatus::done && !_c.failure;
            }

            bool _variant(void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                if (!opt.tag()) {
                    return _c.fail(errc::unsupported_value, _c.p, "a variant field without tagged()");
                }
                if (*_c.p != '{') {
                    return _mismatch(ops);
                }
                // the tag first, wherever it is among the members; then the
                // members again, as the alternative's
                const char* start = _c.p;
                std::string_view tag(opt.tag());
                optional<size_t> which;
                ++_c.p;
                _c.space();
                bool first = true;
                while (!_c.at_end() && *_c.p != '}') {
                    if (!first) {
                        if (*_c.p != ',') {
                            return _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " after an object member, expected ',' or '}'");
                        }
                        ++_c.p;
                        _c.space();
                    }
                    first = false;
                    if (_c.at_end() || *_c.p != '"') {
                        return _c.at_end() ? _c.fail_end("a key") : _c.fail(errc::syntax, _c.p, "invalid character " + char_name(_c.p, _c.end) + " where a key was expected");
                    }
                    std::string_view k;
                    if (!_c.string_token(k)) {
                        return false;
                    }
                    bool is_tag = k == tag;
                    if (!_colon()) {
                        return false;
                    }
                    _c.space();
                    if (is_tag) {
                        const char* at = _c.p;
                        std::string_view name;
                        if (_c.at_end() || *_c.p != '"') {
                            return _c.fail(errc::type_mismatch, _c.p, "expected the name of an alternative, found " + std::string(found_kind(_c.p, _c.end)));
                        }
                        if (!_c.string_token(name)) {
                            return false;
                        }
                        size_t i = opt.index_of(name);
                        if (i == opt.name_count() || i >= ops->alternatives) {
                            _c.fail(errc::type_mismatch, at, "\"" + std::string(name.substr(0, 40)) + "\" is none of the alternatives");
                            path.key(tag);
                            return false;
                        }
                        which = i;
                    } else if (!skip_value(_c, _max > depth ? _max - depth : 0)) {
                        return false;
                    }
                    _c.space();
                }
                if (!which) {
                    _c.fail(errc::missing_field, _c.at_end() ? _c.p : _c.p, "missing field");
                    path.key(tag);
                    return false;
                }
                _c.p = start;
                const ValueOps* ao = nullptr;
                void* ap = ops->emplace_alternative(p, *which, &ao);
                if (ao->kind != ValueKind::record) {
                    return _c.fail(errc::unsupported_value, start, "a variant's alternative with no describe()");
                }
                return _record(ap, ao, depth, tag);
            }

            JsonCursor& _c;
            uint32_t _max;
        };

        // A typed value written into the text through its operations
        class JsonTypedWriter {
        public:
            JsonTypedWriter(JsonOut& out, const json::style& s, uint32_t max_depth) noexcept
            : _out(out), _style(s), _max(max_depth) {
            }

            ErrorPath path;

            bool value(const void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                if (_out.failed()) {
                    return false;
                }
                switch (ops->kind) {
                    case ValueKind::boolean: {
                        bool b = ops->get_bool(p);
                        if (opt.as_string()) {
                            _out.quoted(b ? "true" : "false");
                        } else {
                            _out.boolean(b);
                        }
                        return true;
                    }
                    case ValueKind::floating:
                        if (!std::isfinite(ops->get_double(p))) {
                            return _fail(errc::unsupported_value, std::isnan(ops->get_double(p)) ? "NaN is not a JSON number" : "an infinity is not a JSON number");
                        }
                        [[fallthrough]];
                    case ValueKind::signed_integer:
                    case ValueKind::unsigned_integer: {
                        char buf[NumberTextSize];
                        size_t n = ops->number_text(p, buf);
                        if (opt.as_string()) {
                            _out.quoted(std::string_view(buf, n));
                        } else {
                            _out.literal(std::string_view(buf, n));
                        }
                        return true;
                    }
                    case ValueKind::enumeration: {
                        if (opt.name_count()) {
                            int64_t i = ops->get_int(p);
                            if (i < 0 || uint64_t(i) >= opt.name_count()) {
                                return _fail(errc::unsupported_value, "the value " + std::to_string(i) + " has no name");
                            }
                            _out.quoted(opt.name(size_t(i)));
                            return true;
                        }
                        char buf[NumberTextSize];
                        size_t n = ops->number_text(p, buf);
                        _out.literal(std::string_view(buf, n));
                        return true;
                    }
                    case ValueKind::string:
                    case ValueKind::text: {
                        auto t = ops->get_text(p);
                        _out.quoted(t.view());
                        return true;
                    }
                    case ValueKind::optional:
                    case ValueKind::pointer:
                        if (!ops->has_value(p)) {
                            _out.null();
                            return true;
                        }
                        return value(ops->value(p), ops->inner(), opt, depth);
                    case ValueKind::sequence:
                    case ValueKind::set:
                    case ValueKind::fixed: {
                        if (!_open(depth)) {
                            return false;
                        }
                        if (ops->hashed && _style.sort_keys) {
                            return _sorted_set(p, ops, opt, depth);
                        }
                        _out.begin(false);
                        struct Ctx {
                            JsonTypedWriter* self;
                            const ValueOps* inner;
                            const FieldOptions* opt;
                            uint32_t depth;
                            size_t index;
                        } ctx{this, ops->inner(), &opt, depth + 1, 0};
                        bool ok = ops->for_each(p, &ctx, [](void* c, const void* e) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            if (!x.self->value(e, x.inner, *x.opt, x.depth)) {
                                x.self->path.index(x.index);
                                return false;
                            }
                            ++x.index;
                            return true;
                        });
                        if (!ok) {
                            return false;
                        }
                        _out.end(false);
                        return true;
                    }
                    case ValueKind::tuple: {
                        if (!_open(depth)) {
                            return false;
                        }
                        _out.begin(false);
                        for (size_t i = 0; i < ops->fixed_count; ++i) {
                            const ValueOps* eo = nullptr;
                            const void* ep = ops->element_of(p, i, &eo);
                            if (!value(ep, eo, FieldOptions{}, depth + 1)) {
                                path.index(i);
                                return false;
                            }
                        }
                        _out.end(false);
                        return true;
                    }
                    case ValueKind::map: {
                        if (!_open(depth)) {
                            return false;
                        }
                        _out.begin(true);
                        struct Ctx {
                            JsonTypedWriter* self;
                            const ValueOps* inner;
                            const FieldOptions* opt;
                            uint32_t depth;
                        } ctx{this, ops->inner(), &opt, depth + 1};
                        bool ok = ops->for_each_entry(p, &ctx, [](void* c, std::string_view k, const void* v) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            x.self->_out.key(k);
                            if (!x.self->value(v, x.inner, *x.opt, x.depth)) {
                                x.self->path.key(k);
                                return false;
                            }
                            return true;
                        }, ops->hashed && _style.sort_keys);
                        if (!ok) {
                            return false;
                        }
                        _out.end(true);
                        return true;
                    }
                    case ValueKind::record:
                        if (!_open(depth)) {
                            return false;
                        }
                        _out.begin(true);
                        if (!_fields(p, ops, depth)) {
                            return false;
                        }
                        _out.end(true);
                        return true;
                    case ValueKind::variant: {
                        if (!opt.tag()) {
                            return _fail(errc::unsupported_value, "a variant field without tagged()");
                        }
                        size_t i = ops->index(p);
                        if (i >= opt.name_count()) {
                            return _fail(errc::unsupported_value, "the variant's alternative " + std::to_string(i) + " has no name");
                        }
                        const ValueOps* ao = nullptr;
                        const void* ap = ops->alternative(p, &ao);
                        if (!ap || ao->kind != ValueKind::record) {
                            return _fail(errc::unsupported_value, "a variant's alternative with no describe()");
                        }
                        if (!_open(depth)) {
                            return false;
                        }
                        _out.begin(true);
                        _out.key(opt.tag());
                        _out.quoted(opt.name(i));
                        if (!_fields(ap, ao, depth)) {
                            return false;
                        }
                        _out.end(true);
                        return true;
                    }
                    case ValueKind::json:
                    case ValueKind::custom_json: {
                        json j = ops->to_json(p);
                        write_json(_out, j);
                        return !_out.failed();
                    }
                }
                return false;
            }

        private:
            bool _fail(errc code, std::string detail) {
                _out.fail(code, std::move(detail));
                return false;
            }

            bool _open(uint32_t depth) {
                if (depth >= _max) {
                    return _fail(errc::unsupported_value, "nesting deeper than " + std::to_string(_max) + " (a cycle?)");
                }
                return true;
            }

            // A hashed set in the order of its elements' texts: a set has no
            // order of its own, and the text of one must not depend on the
            // key of the hash
            bool _sorted_set(const void* p, const ValueOps* ops, const FieldOptions& opt, uint32_t depth) {
                struct Ctx {
                    std::vector<std::pair<std::string, const void*>> elements;
                    const ValueOps* inner;
                    const FieldOptions* opt;
                    const json::style* style;
                    bool failed = false;
                } ctx{{}, ops->inner(), &opt, &_style};
                ops->for_each(p, &ctx, [](void* c, const void* e) -> bool {
                    auto& x = *static_cast<Ctx*>(c);
                    JsonOut text(0, false);
                    JsonTypedWriter w(text, *x.style, 512);
                    w.value(e, x.inner, *x.opt, 0);
                    x.elements.emplace_back(std::string(text.text().view()), e);
                    return true;
                });
                std::sort(ctx.elements.begin(), ctx.elements.end(), [](auto& a, auto& b) { return a.first < b.first; });
                _out.begin(false);
                for (size_t i = 0; i < ctx.elements.size(); ++i) {
                    if (!value(ctx.elements[i].second, ctx.inner, opt, depth + 1)) {
                        path.index(i);
                        return false;
                    }
                }
                _out.end(false);
                return true;
            }

            bool _fields(const void* p, const ValueOps* ops, uint32_t depth) {
                field_list fields;
                ops->describe(const_cast<void*>(p), fields);
                for (auto& f : FieldAccess::fields(fields)) {
                    if ((f.flags & OmitEmpty) && f.ops->is_empty(f.address)) {
                        continue;
                    }
                    _out.key(f.name);
                    FieldOptions fo{&fields, &f};
                    if (!value(f.address, f.ops, fo, depth + 1)) {
                        path.key(f.name);
                        return false;
                    }
                }
                return true;
            }

            JsonOut& _out;
            const json::style& _style;
            uint32_t _max;
        };

        // A typed value read from a json: the reader of the text over the
        // tree (json::as)
        class JsonDomReader {
        public:
            explicit JsonDomReader(uint32_t max_depth) noexcept
            : _max(max_depth) {
            }

            ErrorPath path;
            optional<error> failure;

            bool value(void* p, const ValueOps* ops, const FieldOptions& opt, const json& j, uint32_t depth) {
                if (j.is_null() && ops->kind != ValueKind::optional && ops->kind != ValueKind::pointer && ops->kind != ValueKind::json && ops->kind != ValueKind::custom_json) {
                    ops->clear(p);
                    return true;
                }
                switch (ops->kind) {
                    case ValueKind::boolean:
                        if (opt.as_string()) {
                            auto s = j.as_string();
                            if (!s || (s->view() != "true" && s->view() != "false")) {
                                return _fail(errc::type_mismatch, "expected \"true\" or \"false\"");
                            }
                            ops->set_bool(p, s->view() == "true");
                            return true;
                        }
                        if (!j.is_bool()) {
                            return _mismatch(ops, j);
                        }
                        ops->set_bool(p, *j.as_bool());
                        return true;
                    case ValueKind::enumeration:
                        if (opt.name_count()) {
                            auto s = j.as_string();
                            if (!s) {
                                return _mismatch(ops, j);
                            }
                            size_t i = opt.index_of(s->view());
                            if (i == opt.name_count() || !ops->set_int(p, int64_t(i))) {
                                return _fail(errc::type_mismatch, "\"" + std::string(s->view().substr(0, 40)) + "\" is none of the field's names");
                            }
                            return true;
                        }
                        [[fallthrough]];
                    case ValueKind::signed_integer:
                    case ValueKind::unsigned_integer:
                    case ValueKind::floating: {
                        std::string lit;
                        if (opt.as_string()) {
                            auto s = j.as_string();
                            if (!s) {
                                return _mismatch(ops, j);
                            }
                            lit.assign(s->view());
                        } else {
                            if (!j.is_number()) {
                                return _mismatch(ops, j);
                            }
                            lit = _literal(j);
                        }
                        switch (ops->set_literal(p, lit)) {
                            case 0:
                                return true;
                            case 1:
                                return _fail(errc::type_mismatch, "expected " + std::string(ops->name) + ", found " + lit.substr(0, 40));
                            default:
                                return _fail(errc::out_of_range, "the number " + lit.substr(0, 40) + " is out of the field's range");
                        }
                    }
                    case ValueKind::string:
                    case ValueKind::text: {
                        auto s = j.as_string();
                        if (!s) {
                            return _mismatch(ops, j);
                        }
                        if (!ops->set_text(p, s->view())) {
                            return _fail(errc::type_mismatch, "\"" + std::string(s->view().substr(0, 40)) + "\" is not a valid value of the field");
                        }
                        return true;
                    }
                    case ValueKind::optional:
                    case ValueKind::pointer:
                        if (j.is_null()) {
                            ops->reset(p);
                            return true;
                        }
                        return value(ops->emplace(p), ops->inner(), opt, j, depth);
                    case ValueKind::sequence:
                    case ValueKind::set: {
                        if (!j.is_array()) {
                            return _mismatch(ops, j);
                        }
                        if (!_open(depth)) {
                            return false;
                        }
                        struct Ctx {
                            JsonDomReader* self;
                            const ValueOps* inner;
                            const FieldOptions* opt;
                            const json* array;
                            uint32_t depth;
                            size_t index;
                        } ctx{this, ops->inner(), &opt, &j, depth + 1, 0};
                        return ops->read_elements(p, &ctx, [](void* c) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            return x.index < x.array->size();
                        }, [](void* c, void* v) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            if (!x.self->value(v, x.inner, *x.opt, (*x.array)[x.index], x.depth)) {
                                x.self->path.index(x.index);
                                return false;
                            }
                            ++x.index;
                            return true;
                        });
                    }
                    case ValueKind::fixed:
                    case ValueKind::tuple: {
                        if (!j.is_array()) {
                            return _mismatch(ops, j);
                        }
                        if (j.size() != ops->fixed_count) {
                            return _fail(errc::type_mismatch, "expected an array of " + std::to_string(ops->fixed_count) + " elements");
                        }
                        if (!_open(depth)) {
                            return false;
                        }
                        for (size_t i = 0; i < ops->fixed_count; ++i) {
                            const ValueOps* eo = nullptr;
                            void* ep = ops->element(p, i, &eo);
                            if (!value(ep, eo, ops->kind == ValueKind::fixed ? opt : FieldOptions{}, j[i], depth + 1)) {
                                path.index(i);
                                return false;
                            }
                        }
                        return true;
                    }
                    case ValueKind::map: {
                        if (!j.is_object()) {
                            return _mismatch(ops, j);
                        }
                        if (!_open(depth)) {
                            return false;
                        }
                        struct Ctx {
                            JsonDomReader* self;
                            const ValueOps* inner;
                            const FieldOptions* opt;
                            const json* object;
                            uint32_t depth;
                            size_t index;
                        } ctx{this, ops->inner(), &opt, &j, depth + 1, 0};
                        auto status = ops->read_entries(p, &ctx, [](void* c, std::string_view& k) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            if (x.index == x.object->size()) {
                                return false;
                            }
                            k = JsonAccess::raw_members(*x.object)[x.index].key.view();
                            return true;
                        }, [](void* c, void* v) -> bool {
                            auto& x = *static_cast<Ctx*>(c);
                            auto& m = JsonAccess::raw_members(*x.object)[x.index];
                            if (!x.self->value(v, x.inner, *x.opt, m.value, x.depth)) {
                                x.self->path.key(m.key.view());
                                return false;
                            }
                            ++x.index;
                            return true;
                        });
                        if (status == EntryStatus::bad_key) {
                            path.key(JsonAccess::raw_members(j)[ctx.index].key.view());
                            return _fail(errc::type_mismatch, "not a key of the map's type");
                        }
                        return status == EntryStatus::done;
                    }
                    case ValueKind::record:
                        if (!j.is_object()) {
                            return _mismatch(ops, j);
                        }
                        return _record(p, ops, j, depth, {});
                    case ValueKind::variant: {
                        if (!opt.tag()) {
                            return _fail(errc::unsupported_value, "a variant field without tagged()");
                        }
                        if (!j.is_object()) {
                            return _mismatch(ops, j);
                        }
                        std::string_view tag(opt.tag());
                        auto& t = j[string(tag)];
                        if (t.is_null()) {
                            path.key(tag);
                            return _fail(errc::missing_field, "missing field");
                        }
                        auto name = t.as_string();
                        size_t i = name ? opt.index_of(name->view()) : opt.name_count();
                        if (i == opt.name_count() || i >= ops->alternatives) {
                            path.key(tag);
                            return _fail(errc::type_mismatch, "none of the alternatives");
                        }
                        const ValueOps* ao = nullptr;
                        void* ap = ops->emplace_alternative(p, i, &ao);
                        if (ao->kind != ValueKind::record) {
                            return _fail(errc::unsupported_value, "a variant's alternative with no describe()");
                        }
                        return _record(ap, ao, j, depth, tag);
                    }
                    case ValueKind::json:
                    case ValueKind::custom_json:
                        if (!ops->from_json(p, j)) {
                            return _fail(errc::type_mismatch, "the value is not one the field's from_json takes");
                        }
                        return true;
                }
                return _mismatch(ops, j);
            }

            const json::options* options = nullptr;

        private:
            bool _fail(errc code, std::string detail) {
                if (!failure) {
                    failure = error(code, 0, string(detail));
                }
                return false;
            }

            bool _mismatch(const ValueOps* ops, const json& j) {
                static constexpr const char* Kinds[] = {"null", "a boolean", "a number", "a string", "an array", "an object"};
                return _fail(errc::type_mismatch, std::string("expected ") + ops->name + ", found " + Kinds[size_t(j.type())]);
            }

            bool _open(uint32_t depth) {
                if (depth >= _max) {
                    return _fail(errc::depth_limit, "nesting deeper than " + std::to_string(_max));
                }
                return true;
            }

            // A number's literal: its own text, or the text it is written as
            static std::string _literal(const json& j) {
                JsonOut out(0, false);
                JsonAccess::write_scalar(out, j);
                return std::string(out.text().view());
            }

            bool _record(void* p, const ValueOps* ops, const json& j, uint32_t depth, std::string_view ignore) {
                if (!_open(depth)) {
                    return false;
                }
                field_list fields;
                ops->describe(p, fields);
                auto& list = FieldAccess::fields(fields);
                std::vector<bool> seen(list.size(), false);
                for (auto& m : j.members()) {
                    auto k = m.key.view();
                    size_t f = 0;
                    while (f < list.size() && list[f].name != k) {
                        ++f;
                    }
                    if (f < list.size()) {
                        seen[f] = true;
                        FieldOptions fo{&fields, &list[f]};
                        if (!value(list[f].address, list[f].ops, fo, m.value, depth + 1)) {
                            path.key(k);
                            return false;
                        }
                    } else if (k != ignore && options && options->reject_unknown_fields) {
                        return _fail(errc::unknown_field, "unknown field \"" + std::string(k) + "\"");
                    }
                }
                for (size_t f = 0; f < list.size(); ++f) {
                    if (!seen[f] && (list[f].flags & Required)) {
                        path.key(list[f].name);
                        return _fail(errc::missing_field, "missing field");
                    }
                }
                return true;
            }

            uint32_t _max;
        };

        // A T read from [begin, end) of a text in memory, all of it
        template<class T>
        expected<T, error> parse_typed(const char* begin, const char* end, uint64_t base, const json::options& o, uint32_t max_depth, bool whole) {
            static_assert(std::is_default_constructible_v<T>, "json::parse<T>: T needs a default constructor");
            JsonCursor c(begin, end, base, o);
            JsonTypedReader r(c, max_depth);
            T value{};
            if (!r.value(std::addressof(value), value_ops<T>(), FieldOptions{}, 0)) {
                c.failure->set_path(r.path.text());
                return unexpected<error>(std::move(*c.failure));
            }
            if (whole) {
                c.space();
                if (!c.at_end()) {
                    c.fail(errc::syntax, c.p, "invalid character " + char_name(c.p, c.end) + " after the value");
                    return unexpected<error>(std::move(*c.failure));
                }
            }
            return value;
        }

        // A typed value into the text; the path of a failure into path
        template<class T>
        bool write_typed(JsonOut& out, const T& v, const json::style& s, string* path) {
            JsonTypedWriter w(out, s, 512);
            if (!w.value(std::addressof(v), value_ops<T>(), FieldOptions{}, 0)) {
                *path = w.path.text();
                return false;
            }
            return true;
        }

        // For json::writer: the path goes into the words of the mistake
        template<class T>
        bool write_typed(JsonOut& out, const T& v, const json::style& s) {
            string path;
            if (!write_typed(out, v, s, &path)) {
                if (!path.empty()) {
                    out.set_detail(std::string(path.view()) + ": " + out.detail());
                }
                return false;
            }
            return true;
        }
    }

    template<class T>
    expected<T, json::error> json::parse(const string& text) {
        return parse<T>(text, options());
    }

    template<class T>
    expected<T, json::error> json::parse(const string& text, const options& o) {
        auto r = detail::parse_typed<T>(text.data(), text.data() + text.size(), 0, o, o.max_depth, true);
        if (!r) {
            r.error().locate(text);
        }
        return r;
    }

    template<class T>
    expected<T, json::error> json::parse(const io::reader& in) {
        return parse<T>(in, options());
    }

    template<class T>
    expected<T, json::error> json::parse(const io::reader& in, const options& o) {
        reader r(in, o);
        auto v = r.read<T>();
        if (v && !r.more() && !r.last_error()) {
            return std::move(*v);
        }
        if (r.last_error()) {
            return unexpected<error>(*r.last_error());
        }
        if (!v) {
            return unexpected<error>(error(errc::unexpected_end, r.offset(), "unexpected end of input, expected a value"));
        }
        return unexpected<error>(error(errc::syntax, r.offset(), "a character after the value"));
    }

    template<class T>
    async::task<expected<T, json::error>> json::async_parse(const io::reader& in) {
        return async_parse<T>(in, options());
    }

    template<class T>
    async::task<expected<T, json::error>> json::async_parse(io::reader in, options o) {
        reader r(std::move(in), o);
        auto v = co_await r.async_read<T>();
        bool more = v ? co_await r.async_more() : false;
        if (v && !more && !r.last_error()) {
            co_return std::move(*v);
        }
        if (r.last_error()) {
            co_return unexpected<error>(*r.last_error());
        }
        if (!v) {
            co_return unexpected<error>(error(errc::unexpected_end, r.offset(), "unexpected end of input, expected a value"));
        }
        co_return unexpected<error>(error(errc::syntax, r.offset(), "a character after the value"));
    }

    template<class T>
    expected<string, json::error> json::stringify(const T& value) {
        return stringify(value, compact);
    }

    template<class T>
    expected<json, json::error> json::from(const T& value) {
        auto text = stringify(value, compact);
        if (!text) {
            return unexpected<error>(std::move(text.error()));
        }
        return parse(*text);
    }

    template<class T>
    expected<string, json::error> json::stringify(const T& value, const style& s) {
        detail::JsonOut out(s.indent, s.escape_html);
        string path;
        if constexpr (detail::JsonScalar<T>::value) {
            detail::write_scalar_value(out, value);
        } else {
            detail::write_typed(out, value, s, &path);
        }
        if (out.failed()) {
            error e(out.code(), 0, string(out.detail()));
            e.set_path(path);
            return unexpected<error>(std::move(e));
        }
        return string(out.text().view());
    }

    template<class T>
    expected<T, json::error> json::as() const {
        return as<T>(options());
    }

    template<class T>
    expected<T, json::error> json::as(const options& o) const {
        static_assert(std::is_default_constructible_v<T>, "json::as<T>: T needs a default constructor");
        detail::JsonDomReader r(o.max_depth);
        r.options = &o;
        T value{};
        if (!r.value(std::addressof(value), detail::value_ops<T>(), detail::FieldOptions{}, *this, 0)) {
            r.failure->set_path(r.path.text());
            return unexpected<error>(std::move(*r.failure));
        }
        return value;
    }

    template<class T>
    optional<T> json::reader::_typed_extent() {
        bool key = _expect == Expect::key || _expect == Expect::first_key;
        auto r = detail::parse_typed<T>(_d + _pos, _d + _ext_end, _base + _pos, _options, _options.max_depth - uint32_t(_stack.size()), true);
        if (!r) {
            _set_error(std::move(r.error()), size_t(r.error().offset() - _base));
            return nullopt;
        }
        if (key) {
            _expect = Expect::colon;
        } else {
            _value_done();
        }
        _pos = _ext_end;
        return std::move(*r);
    }

    template<class T>
    optional<T> json::reader::read() {
        for (;;) {
            Step s = _to_value();
            if (s == Step::more) {
                if (!_fill()) {
                    return nullopt;
                }
                continue;
            }
            if (s != Step::token) {
                return nullopt;
            }
            break;
        }
        for (;;) {
            Step s = _extent();
            if (s == Step::more) {
                if (!_fill()) {
                    return nullopt;
                }
                continue;
            }
            if (s != Step::token) {
                return nullopt;
            }
            return _typed_extent<T>();
        }
    }

    template<class T>
    async::task<optional<T>> json::reader::async_read() {
        for (;;) {
            Step s = _to_value();
            if (s == Step::more) {
                auto room = _room();
                _received(co_await _in.async_read(room));
                if (_error) {
                    co_return nullopt;
                }
                continue;
            }
            if (s != Step::token) {
                co_return nullopt;
            }
            break;
        }
        for (;;) {
            Step s = _extent();
            if (s == Step::more) {
                auto room = _room();
                _received(co_await _in.async_read(room));
                if (_error) {
                    co_return nullopt;
                }
                continue;
            }
            if (s != Step::token) {
                co_return nullopt;
            }
            co_return _typed_extent<T>();
        }
    }
}
