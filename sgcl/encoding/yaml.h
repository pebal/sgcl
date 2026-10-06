//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "json.h"
#include "detail/json_number.h"
#include "detail/files.h"
#include "detail/utf8_check.h"
#include "detail/yaml_scanner.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/map.h"
#include "../core/detail/maker.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;
        struct YamlAccess;
        class YamlParser;
        class YamlWriter;
    }

    // One YAML node (YAML 1.2.2), immutable, as json is one JSON value: a
    // pointer, a word and a kind, 24 bytes, shared by copying. A scalar
    // keeps its text as written and the kind the core schema (§10.3) reads
    // it as; a sequence its elements; a mapping its members, keys of any
    // kind. An alias of a document is the node of its anchor, shared, not
    // copied. Read by parse and parse_all, written by to_string in block
    // style.
    class yaml {
    public:
        using error = encoding::error;

        // The kinds of the core schema, and the two collections
        enum class kind : uint8_t {
            null = 0,
            boolean,
            integer,
            floating,
            string,
            sequence,
            mapping
        };

        struct member;

        // What a parse accepts
        struct options {
            uint32_t max_depth = 512;                    // collections inside one another
            size_t max_alias_nodes = size_t(1) << 20;    // the nodes reached through aliases: the billion laughs bound
            bool allow_duplicate_keys = false;           // then the last one wins
        };

        // --- making one ---

        yaml() noexcept = default;

        SGCL_INLINE_HOT yaml(std::nullptr_t) noexcept {
        }

        template<class B>
        requires std::same_as<B, bool>
        yaml(B b) noexcept
        : yaml(kind::boolean, string(b ? "true" : "false")) {
        }

        template<class I>
        requires std::integral<I> && (!std::same_as<I, bool>) && (!std::same_as<I, char>) && (!std::same_as<I, wchar_t>)
              && (!std::same_as<I, char8_t>) && (!std::same_as<I, char16_t>) && (!std::same_as<I, char32_t>)
        yaml(I v) noexcept
        : yaml(kind::integer, string(std::to_string(v))) {
        }

        // A float: its shortest digits, .inf, -.inf, .nan
        yaml(double d) noexcept;

        // A string: its characters shared, not copied
        SGCL_INLINE_HOT yaml(const string& s) noexcept
        : yaml(kind::string, s) {
        }

        SGCL_INLINE_HOT yaml(const char* s) noexcept
        : yaml(kind::string, string(s)) {
        }

        static yaml sequence(std::initializer_list<yaml> elements) noexcept;
        static yaml sequence(const vector<yaml>& elements) noexcept;
        static yaml mapping(std::initializer_list<member> members) noexcept;
        static yaml mapping(const vector<member>& members) noexcept;

        // The node under an application's tag ("!Ref", "tag:example.com,2026:x");
        // the core schema's tags are no tags of a node: "!!int 1" is an integer
        static yaml tagged(const string& tag, const yaml& value) noexcept;

        // The JSON value as YAML: the same kinds, the keys strings
        static yaml from_json(const json& j) noexcept;

        // --- reading and writing ---

        // Exactly one document of the text: errc::syntax for a second
        static expected<yaml, error> parse(const string& text) noexcept;
        static expected<yaml, error> parse(const string& text, const options& o) noexcept;

        // The text of the stream, to its end; in a task co_await
        // yaml::async_parse(in)
        static expected<yaml, error> parse(const io::reader& in);
        static expected<yaml, error> parse(const io::reader& in, const options& o);
        static async::task<expected<yaml, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<yaml, error>> async_parse(io::reader in, options o) noexcept;

        // Every document of the text
        static expected<vector<yaml>, error> parse_all(const string& text) noexcept;
        static expected<vector<yaml>, error> parse_all(const string& text, const options& o) noexcept;

        // One document in block style, two spaces a level, every line ended
        string to_string() const;

        // --- files: one line each ---

        // One document of a file, read as parse reads it: yaml::load("app.yaml"); a
        // file that does not open is errc::io, io_error() saying why
        static expected<yaml, error> load(const string& path);
        static async::task<expected<yaml, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // The node as JSON: the kinds as they are, a key that is not a string
        // its YAML text, .inf and .nan null
        json to_json() const noexcept;

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

        SGCL_INLINE_HOT bool is_number() const noexcept {
            return _kind == kind::integer || _kind == kind::floating;
        }

        SGCL_INLINE_HOT bool is_integer() const noexcept {
            return _kind == kind::integer;
        }

        SGCL_INLINE_HOT bool is_string() const noexcept {
            return _kind == kind::string;
        }

        SGCL_INLINE_HOT bool is_sequence() const noexcept {
            return _kind == kind::sequence;
        }

        SGCL_INLINE_HOT bool is_mapping() const noexcept {
            return _kind == kind::mapping;
        }

        optional<bool> as_bool() const noexcept;
        optional<int64_t> as_int() const noexcept;
        optional<uint64_t> as_uint() const noexcept;
        optional<double> as_double() const noexcept;     // a float, or an integer rounded

        optional<string> as_string() const noexcept {
            if (_kind != kind::string) {
                return nullopt;
            }
            return _text();
        }

        // The same with a value for when there is none: y["port"].as_int(80)
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

        // A scalar's text as it was written (0o14, 1e3, ~, a string's
        // characters); empty for a collection and for a null made here or
        // left empty in the text
        string text() const noexcept;

        // The application's tag of the node, empty when it has none
        string tag() const noexcept;

        // --- inside it ---

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _kind == kind::sequence || _kind == kind::mapping ? size_t(_bits) : 0;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        // A sequence's element at the index; null past the end
        SGCL_INLINE_HOT yaml operator[](size_t index) const noexcept {
            return is_sequence() && index < _bits ? _elements()[index] : yaml();
        }

        // A mapping's value of the key; null when there is none
        yaml operator[](const string& key) const noexcept {
            return (*this)[yaml(key)];
        }

        template<size_t N>
        yaml operator[](const char (&key)[N]) const noexcept {
            return (*this)[yaml(string(key))];
        }

        yaml operator[](const yaml& key) const noexcept;
        bool contains(const yaml& key) const noexcept;

        slice<const yaml> elements() const noexcept;
        slice<const member> members() const noexcept;

        // New versions: the mapping with the key set (a sequence's element
        // at an integer index, one past the end appending), without it, the
        // sequence with an element at its end (of null, a sequence of one)
        yaml set(const yaml& key, const yaml& value) const noexcept;
        yaml erase(const yaml& key) const noexcept;
        yaml push_back(const yaml& value) const noexcept;

        // Deep: scalars by their kind and the value it reads (0x10 is 16, 1e1
        // is 10.0, every .nan one value), strings by their characters,
        // mappings as sets of members, the tags alike
        friend bool operator==(const yaml& a, const yaml& b) noexcept {
            return a._equals(b);
        }

        size_t hash() const noexcept;

    private:
        friend struct detail::YamlAccess;
        friend class detail::YamlParser;
        friend class detail::YamlWriter;

        static constexpr uint8_t Tagged = 1;

        tracked_ptr<const void> _ptr;   // a scalar's text, a collection's buffer, or a YamlTagged
        uint64_t _bits = 0;             // a collection's count
        kind _kind = kind::null;
        uint8_t _flags = 0;

        SGCL_INLINE_HOT yaml(kind k, const string& text) noexcept
        : _ptr(text.as_slice().owner()), _kind(k) {
        }

        const tracked_ptr<const void>& _payload() const noexcept;

        SGCL_INLINE_HOT string _text() const noexcept {
            return sgcl::detail::StringAccess::over<string>(_payload());
        }

        SGCL_INLINE_HOT const yaml* _elements() const noexcept {
            return static_cast<const yaml*>(_payload().get());
        }

        const member* _members() const noexcept;
        size_t _find(const yaml& key) const noexcept;
        bool _equals(const yaml& o) const noexcept;
    };

    static_assert(sizeof(yaml) == 24, "a yaml is a pointer, a word and a kind");

    struct yaml::member {
        yaml key;
        yaml value;
    };

    namespace detail {
        // A node under an application's tag: the tag and the node's own
        // pointer
        struct YamlTagged {
            string tag;
            tracked_ptr<const void> payload;
        };

        struct YamlAccess {
            template<class T>
            static tracked_ptr<const void> buffer(const T* first, size_t n) noexcept {
                tracked_ptr<const void> owner;
                if (n) {
                    T* p = json_buffer<T>(n, owner);
                    for (size_t i = 0; i < n; ++i) {
                        Maker<T>::construct(p + i, first[i]);
                    }
                }
                return owner;
            }

            static yaml scalar(yaml::kind k, const string& text) noexcept {
                return yaml(k, text);
            }

            static yaml collection(yaml::kind k, tracked_ptr<const void> owner, size_t n) noexcept {
                yaml y;
                y._kind = k;
                y._bits = n;
                y._ptr = std::move(owner);
                return y;
            }

            static yaml with_tag(const yaml& v, const string& tag) noexcept {
                if (tag.empty()) {
                    return v;
                }
                yaml y = v;
                auto node = make_tracked<YamlTagged>();
                node->tag = tag;
                node->payload = v._payload();
                y._ptr = tracked_ptr<const void>(std::move(node));
                y._flags |= yaml::Tagged;
                return y;
            }
        };

        // The core schema (§10.3.2): what a plain scalar's text reads as
        inline bool yaml_digits(std::string_view s, size_t from, bool (*ok)(char)) noexcept {
            if (from >= s.size()) {
                return false;
            }
            for (size_t i = from; i < s.size(); ++i) {
                if (!ok(s[i])) {
                    return false;
                }
            }
            return true;
        }

        inline yaml::kind yaml_resolve(std::string_view s) noexcept {
            if (s.empty() || s == "~" || s == "null" || s == "Null" || s == "NULL") {
                return yaml::kind::null;
            }
            if (s == "true" || s == "True" || s == "TRUE" || s == "false" || s == "False" || s == "FALSE") {
                return yaml::kind::boolean;
            }
            auto dec = [](char c) { return c >= '0' && c <= '9'; };
            size_t sign = (s[0] == '-' || s[0] == '+') ? 1 : 0;
            if (yaml_digits(s, sign, dec)) {
                return yaml::kind::integer;
            }
            if (s.size() > 2 && s[0] == '0' && s[1] == 'o' && yaml_digits(s, 2, [](char c) { return c >= '0' && c <= '7'; })) {
                return yaml::kind::integer;
            }
            if (s.size() > 2 && s[0] == '0' && s[1] == 'x'
                && yaml_digits(s, 2, [](char c) { return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'f'); })) {
                return yaml::kind::integer;
            }
            std::string_view r = s.substr(sign);
            if (r == ".inf" || r == ".Inf" || r == ".INF" || (sign == 0 && (s == ".nan" || s == ".NaN" || s == ".NAN"))) {
                return yaml::kind::floating;
            }
            // [-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?
            size_t i = 0;
            size_t int_digits = 0, frac_digits = 0;
            while (i < r.size() && dec(r[i])) {
                ++i;
                ++int_digits;
            }
            if (i < r.size() && r[i] == '.') {
                ++i;
                while (i < r.size() && dec(r[i])) {
                    ++i;
                    ++frac_digits;
                }
                if (int_digits == 0 && frac_digits == 0) {
                    return yaml::kind::string;
                }
            } else if (int_digits == 0) {
                return yaml::kind::string;
            }
            if (i < r.size() && (r[i] == 'e' || r[i] == 'E')) {
                ++i;
                if (i < r.size() && (r[i] == '-' || r[i] == '+')) {
                    ++i;
                }
                size_t exp = 0;
                while (i < r.size() && dec(r[i])) {
                    ++i;
                    ++exp;
                }
                if (exp == 0) {
                    return yaml::kind::string;
                }
            }
            return i == r.size() ? yaml::kind::floating : yaml::kind::string;
        }

        // An integer's text as its sign and magnitude, false past 64 bits
        inline bool yaml_integer(std::string_view s, bool& negative, uint64_t& magnitude) noexcept {
            negative = false;
            size_t i = 0;
            int base = 10;
            if (s.size() > 2 && s[0] == '0' && (s[1] == 'o' || s[1] == 'x')) {
                base = s[1] == 'o' ? 8 : 16;
                i = 2;
            } else if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
                negative = s[0] == '-';
                i = 1;
            }
            unsigned __int128 v = 0;
            for (; i < s.size(); ++i) {
                char c = s[i];
                int d = c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
                v = v * unsigned(base) + unsigned(d);
                if (v >> 64) {
                    return false;
                }
            }
            magnitude = uint64_t(v);
            return true;
        }

        inline optional<double> yaml_float(std::string_view s) noexcept {
            std::string t;
            size_t i = 0;
            bool negative = false;
            if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
                negative = s[0] == '-';
                i = 1;
            }
            std::string_view r = s.substr(i);
            if (r == ".inf" || r == ".Inf" || r == ".INF") {
                return negative ? -INFINITY : INFINITY;
            }
            if (r == ".nan" || r == ".NaN" || r == ".NAN") {
                return NAN;
            }
            if (!r.empty() && r[0] == '.') {
                t += '0';
            }
            for (size_t k = 0; k < r.size(); ++k) {
                t += r[k];
                if (r[k] == '.' && (k + 1 == r.size() || !(r[k + 1] >= '0' && r[k + 1] <= '9'))) {
                    t += '0';
                }
            }
            double d = 0;
            auto res = std::from_chars(t.data(), t.data() + t.size(), d);
            if (res.ec == std::errc::result_out_of_range) {
                // past a double: the infinity of its sign, or zero
                bool big = t.find_first_of("eE") == std::string::npos || t[t.find_first_of("eE") + 1] != '-';
                d = big ? INFINITY : 0.0;
            } else if (res.ec != std::errc()) {
                return nullopt;
            }
            return negative ? -d : d;
        }

        // The text of a float as the core schema reads it back
        inline std::string yaml_float_text(double d) noexcept {
            if (std::isnan(d)) {
                return ".nan";
            }
            if (std::isinf(d)) {
                return d < 0 ? "-.inf" : ".inf";
            }
            char room[NumberTextSize];
            std::string t(room, number_text(room, d));
            if (t.find_first_of(".e") == std::string::npos) {
                t += ".0";
            }
            return t;
        }
    }

    // --- making ---

    inline yaml::yaml(double d) noexcept
    : yaml(kind::floating, string(detail::yaml_float_text(d))) {
    }

    inline yaml yaml::sequence(std::initializer_list<yaml> elements) noexcept {
        return detail::YamlAccess::collection(kind::sequence, detail::YamlAccess::buffer(elements.begin(), elements.size()), elements.size());
    }

    inline yaml yaml::sequence(const vector<yaml>& elements) noexcept {
        return detail::YamlAccess::collection(kind::sequence, detail::YamlAccess::buffer(elements.data(), elements.size()), elements.size());
    }

    inline yaml yaml::mapping(std::initializer_list<member> members) noexcept {
        return detail::YamlAccess::collection(kind::mapping, detail::YamlAccess::buffer(members.begin(), members.size()), members.size());
    }

    inline yaml yaml::mapping(const vector<member>& members) noexcept {
        return detail::YamlAccess::collection(kind::mapping, detail::YamlAccess::buffer(members.data(), members.size()), members.size());
    }

    inline yaml yaml::tagged(const string& tag, const yaml& value) noexcept {
        yaml plain = value;
        if (plain._flags & Tagged) {
            plain._ptr = value._payload();
            plain._flags &= uint8_t(~Tagged);
        }
        return detail::YamlAccess::with_tag(plain, tag);
    }

    inline const tracked_ptr<const void>& yaml::_payload() const noexcept {
        if (_flags & Tagged) {
            return static_cast<const detail::YamlTagged*>(_ptr.get())->payload;
        }
        return _ptr;
    }

    inline string yaml::tag() const noexcept {
        if (_flags & Tagged) {
            return static_cast<const detail::YamlTagged*>(_ptr.get())->tag;
        }
        return string();
    }

    inline string yaml::text() const noexcept {
        if (_kind == kind::sequence || _kind == kind::mapping) {
            return string();
        }
        return _text();
    }

    inline const yaml::member* yaml::_members() const noexcept {
        return static_cast<const member*>(_payload().get());
    }

    inline slice<const yaml> yaml::elements() const noexcept {
        if (_kind != kind::sequence || _bits == 0) {
            return slice<const yaml>();
        }
        return slice<const yaml>(_payload(), _elements(), size_t(_bits));
    }

    inline slice<const yaml::member> yaml::members() const noexcept {
        if (_kind != kind::mapping || _bits == 0) {
            return slice<const member>();
        }
        return slice<const member>(_payload(), _members(), size_t(_bits));
    }

    inline size_t yaml::_find(const yaml& key) const noexcept {
        if (_kind != kind::mapping) {
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

    inline yaml yaml::operator[](const yaml& key) const noexcept {
        size_t i = _find(key);
        return i == size_t(-1) ? yaml() : _members()[i].value;
    }

    inline bool yaml::contains(const yaml& key) const noexcept {
        return _find(key) != size_t(-1);
    }

    inline yaml yaml::set(const yaml& key, const yaml& value) const noexcept {
        if (_kind == kind::sequence) {
            auto i = key.as_uint();
            if (!i || *i > _bits) {
                return *this;
            }
            vector<yaml> v(elements().begin(), elements().end());
            if (*i == _bits) {
                v.push_back(value);
            } else {
                v[size_t(*i)] = value;
            }
            return detail::YamlAccess::with_tag(sequence(v), tag());
        }
        if (_kind != kind::mapping) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        size_t i = _find(key);
        if (i == size_t(-1)) {
            v.push_back(member{key, value});
        } else {
            v[i].value = value;
        }
        return detail::YamlAccess::with_tag(mapping(v), tag());
    }

    inline yaml yaml::erase(const yaml& key) const noexcept {
        if (_kind == kind::sequence) {
            auto i = key.as_uint();
            if (!i || *i >= _bits) {
                return *this;
            }
            vector<yaml> v(elements().begin(), elements().end());
            v.erase(v.begin() + ptrdiff_t(*i));
            return detail::YamlAccess::with_tag(sequence(v), tag());
        }
        size_t i = _find(key);
        if (i == size_t(-1)) {
            return *this;
        }
        vector<member> v(members().begin(), members().end());
        v.erase(v.begin() + ptrdiff_t(i));
        return detail::YamlAccess::with_tag(mapping(v), tag());
    }

    inline yaml yaml::push_back(const yaml& value) const noexcept {
        if (_kind == kind::null) {
            return sequence({value});
        }
        if (_kind != kind::sequence) {
            return *this;
        }
        vector<yaml> v(elements().begin(), elements().end());
        v.push_back(value);
        return detail::YamlAccess::with_tag(sequence(v), tag());
    }

    // --- values ---

    inline optional<bool> yaml::as_bool() const noexcept {
        if (_kind != kind::boolean) {
            return nullopt;
        }
        auto t = _text();
        return !t.empty() && (t[0] == 't' || t[0] == 'T');
    }

    inline optional<int64_t> yaml::as_int() const noexcept {
        if (_kind != kind::integer) {
            return nullopt;
        }
        bool negative;
        uint64_t m;
        if (!detail::yaml_integer(_text().view(), negative, m)) {
            return nullopt;
        }
        if (negative) {
            if (m > uint64_t(INT64_MAX) + 1) {
                return nullopt;
            }
            return int64_t(0 - m);
        }
        if (m > uint64_t(INT64_MAX)) {
            return nullopt;
        }
        return int64_t(m);
    }

    inline optional<uint64_t> yaml::as_uint() const noexcept {
        if (_kind != kind::integer) {
            return nullopt;
        }
        bool negative;
        uint64_t m;
        if (!detail::yaml_integer(_text().view(), negative, m) || (negative && m != 0)) {
            return nullopt;
        }
        return m;
    }

    inline optional<double> yaml::as_double() const noexcept {
        if (_kind == kind::floating) {
            return detail::yaml_float(_text().view());
        }
        if (_kind == kind::integer) {
            bool negative;
            uint64_t m;
            if (detail::yaml_integer(_text().view(), negative, m)) {
                return negative ? -double(m) : double(m);
            }
            // past 64 bits: the digits as a float
            return detail::yaml_float(_text().view());
        }
        return nullopt;
    }

    // --- equality and hash: without recursion ---

    inline bool yaml::_equals(const yaml& top) const noexcept {
        if (_kind != top._kind) {
            return false;
        }
        if (_kind == kind::string && !((_flags | top._flags) & Tagged)) {
            return _text() == top._text();   // the most common question, a key's: no stack
        }
        std::vector<pair<const yaml*, const yaml*>> todo{{this, &top}};
        while (!todo.empty()) {
            auto [a, b] = todo.back();
            todo.pop_back();
            if (a->_kind != b->_kind || a->tag() != b->tag()) {
                return false;
            }
            switch (a->_kind) {
                case kind::null:
                    break;
                case kind::boolean:
                    if (*a->as_bool() != *b->as_bool()) {
                        return false;
                    }
                    break;
                case kind::integer: {
                    bool na, nb;
                    uint64_t ma, mb;
                    bool fa = detail::yaml_integer(a->_text().view(), na, ma), fb = detail::yaml_integer(b->_text().view(), nb, mb);
                    if (fa != fb) {
                        return false;
                    }
                    if (fa) {
                        if (ma != mb || (na != nb && ma != 0)) {
                            return false;
                        }
                    } else if (a->_text() != b->_text()) {
                        return false;
                    }
                    break;
                }
                case kind::floating: {
                    double x = *a->as_double(), y = *b->as_double();
                    if (!(x == y || (std::isnan(x) && std::isnan(y)))) {
                        return false;
                    }
                    break;
                }
                case kind::string:
                    if (a->_text() != b->_text()) {
                        return false;
                    }
                    break;
                case kind::sequence:
                    if (a->_bits != b->_bits) {
                        return false;
                    }
                    for (size_t i = 0; i < a->_bits; ++i) {
                        todo.push_back({a->_elements() + i, b->_elements() + i});
                    }
                    break;
                case kind::mapping:
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
            }
        }
        return true;
    }

    inline size_t yaml::hash() const noexcept {
        auto scalar = [](const yaml& y) -> uint64_t {
            uint64_t x = uint64_t(y._kind) * 0x9E3779B97F4A7C15ull + y.tag().hash();
            switch (y._kind) {
                case kind::boolean:
                    return x ^ uint64_t(*y.as_bool());
                case kind::integer: {
                    bool n;
                    uint64_t m;
                    if (detail::yaml_integer(y._text().view(), n, m)) {
                        return x ^ (n && m ? ~m + 1 : m);
                    }
                    return x ^ y._text().hash();
                }
                case kind::floating: {
                    double d = *y.as_double();
                    return x ^ (std::isnan(d) ? 0x7FF8000000000000ull : std::bit_cast<uint64_t>(d == 0 ? 0.0 : d));
                }
                case kind::string:
                    return x ^ y._text().hash();
                default:
                    return x;
            }
        };
        if (_kind != kind::sequence && _kind != kind::mapping) {
            return size_t(detail::mix(scalar(*this)));
        }
        uint64_t h = 0;
        std::vector<const yaml*> todo{this};
        while (!todo.empty()) {
            const yaml* y = todo.back();
            todo.pop_back();
            uint64_t x = scalar(*y);
            if (y->_kind == kind::sequence) {
                x ^= y->_bits;
                for (size_t i = 0; i < y->_bits; ++i) {
                    todo.push_back(y->_elements() + i);
                }
            } else if (y->_kind == kind::mapping) {
                uint64_t sum = 0;
                for (size_t i = 0; i < y->_bits; ++i) {
                    const member& m = y->_members()[i];
                    sum += m.key.hash() * 31 + m.value.hash();
                }
                x ^= sum + y->_bits;
            }
            h = detail::mix(h ^ x);
        }
        return size_t(h);
    }

    // --- the parse ---

    namespace detail {
        class YamlParser {
        public:
            YamlParser(std::string_view text, const yaml::options& o) noexcept
            : _text(text), _s(text), _o(o) {
            }

            expected<vector<yaml>, error> all() noexcept {
                vector<yaml> docs;
                const YamlToken* t;
                YamlToken tok;
                if (!_next(tok)) {   // the stream's start
                    return _failure();
                }
                for (;;) {
                    bool directives = false;
                    _handles.clear();
                    for (;;) {
                        if (!_peek(t)) {
                            return _failure();
                        }
                        if (t->type == YamlTok::version_directive) {
                            if (_saw_version) {
                                return _fail(errc::syntax, t->at, "a second %YAML directive in one document");
                            }
                            _saw_version = true;
                        } else if (t->type == YamlTok::tag_directive) {
                            if (_handles.count(t->value)) {
                                return _fail(errc::syntax, t->at, "a %TAG directive of a handle given twice");
                            }
                            _handles[t->value] = t->suffix;
                        } else {
                            break;
                        }
                        directives = true;
                        _next(tok);
                    }
                    _saw_version = false;
                    if (t->type == YamlTok::stream_end) {
                        if (directives) {
                            return _fail(errc::syntax, t->at, "directives without a document after them");
                        }
                        break;
                    }
                    bool explicit_start = false;
                    if (t->type == YamlTok::document_start) {
                        explicit_start = true;
                        _next(tok);
                        if (!_peek(t)) {
                            return _failure();
                        }
                    } else if (directives) {
                        return _fail(errc::syntax, t->at, "directives not followed by '---'");
                    } else if (t->type == YamlTok::document_end) {
                        _next(tok);
                        continue;
                    }
                    yaml node;
                    if (t->type == YamlTok::document_start || t->type == YamlTok::document_end || t->type == YamlTok::stream_end
                        || t->type == YamlTok::version_directive || t->type == YamlTok::tag_directive) {
                        if (!explicit_start) {
                            continue;
                        }
                    } else {
                        size_t size;
                        if (!_node(true, false, 0, node, size)) {
                            return _failure();
                        }
                    }
                    if (!_peek(t)) {
                        return _failure();
                    }
                    if (t->type == YamlTok::document_end) {
                        while (t->type == YamlTok::document_end) {
                            _next(tok);
                            if (!_peek(t)) {
                                return _failure();
                            }
                        }
                    } else if (t->type != YamlTok::document_start && t->type != YamlTok::stream_end) {
                        return _fail(errc::syntax, t->at, "more after the document's node, which ends it");
                    }
                    _anchors.clear();
                    docs.push_back(node);
                }
                return docs;
            }

        private:
            struct Anchored {
                yaml node;
                size_t size;
            };

            std::string_view _text;
            YamlScanner _s;
            const yaml::options& _o;
            std::map<std::string, std::string> _handles;
            map<string, Anchored> _anchors;   // the nodes are handles: a managed map
            size_t _alias_nodes = 0;
            bool _saw_version = false;
            error _e;
            bool _failed = false;

            bool _peek(const YamlToken*& t) noexcept {
                if (_failed) {
                    return false;
                }
                if (!_s.peek(t)) {
                    _e = _s.failure();
                    _failed = true;
                    return false;
                }
                return true;
            }

            bool _next(YamlToken& t) noexcept {
                if (_failed) {
                    return false;
                }
                if (!_s.next(t)) {
                    _e = _s.failure();
                    _failed = true;
                    return false;
                }
                return true;
            }

            unexpected<error> _fail(errc code, size_t at, const char* text) noexcept {
                if (!_failed) {
                    _failed = true;
                    _e = error(code, at, string(text));
                }
                return _failure();
            }

            unexpected<error> _failure() noexcept {
                error e = _e;
                return unexpected<error>(std::move(e.locate(string(_text))));
            }

            bool _error(errc code, size_t at, const char* text) noexcept {
                if (!_failed) {
                    _failed = true;
                    _e = error(code, at, string(text));
                }
                return false;
            }

            static constexpr std::string_view Core = "tag:yaml.org,2002:";

            // The tag of a token, its handle resolved
            bool _resolve_tag(const YamlToken& t, std::string& out) noexcept {
                if (t.value.empty()) {
                    out = t.suffix;   // verbatim
                    return true;
                }
                if (t.value == "!" && t.suffix.empty()) {
                    out = "!";
                    return true;
                }
                auto h = _handles.find(t.value);
                if (h != _handles.end()) {
                    out = h->second + t.suffix;
                } else if (t.value == "!") {
                    out = "!" + t.suffix;
                } else if (t.value == "!!") {
                    out = std::string(Core) + t.suffix;
                } else {
                    return _error(errc::syntax, t.at, "a tag of a handle no %TAG directive gives");
                }
                return true;
            }

            static bool _ends_node(YamlTok t) noexcept {
                return t == YamlTok::key || t == YamlTok::value || t == YamlTok::block_end || t == YamlTok::flow_entry
                    || t == YamlTok::flow_sequence_end || t == YamlTok::flow_mapping_end || t == YamlTok::document_start
                    || t == YamlTok::document_end || t == YamlTok::stream_end || t == YamlTok::block_entry;
            }

            // A scalar of the token and its tag
            bool _scalar(const YamlToken& t, const std::string& tag, bool has_tag, yaml& out) noexcept {
                const std::string& v = t.value;
                yaml::kind k;
                if (!has_tag) {
                    k = t.style == YamlStyle::plain ? yaml_resolve(v) : yaml::kind::string;
                    out = YamlAccess::scalar(k, string(v));
                    return true;
                }
                if (tag == "!") {
                    out = YamlAccess::scalar(yaml::kind::string, string(v));
                    return true;
                }
                if (tag.compare(0, Core.size(), Core) == 0) {
                    std::string_view name = std::string_view(tag).substr(Core.size());
                    yaml::kind resolved = yaml_resolve(v);
                    if (name == "str") {
                        k = yaml::kind::string;
                    } else if (name == "null") {
                        if (resolved != yaml::kind::null) {
                            return _error(errc::type_mismatch, t.at, "a !!null scalar that is not null");
                        }
                        k = resolved;
                    } else if (name == "bool") {
                        if (resolved != yaml::kind::boolean) {
                            return _error(errc::type_mismatch, t.at, "a !!bool scalar that is not true or false");
                        }
                        k = resolved;
                    } else if (name == "int") {
                        if (resolved != yaml::kind::integer) {
                            return _error(errc::type_mismatch, t.at, "an !!int scalar that is not an integer");
                        }
                        k = resolved;
                    } else if (name == "float") {
                        if (resolved != yaml::kind::floating && resolved != yaml::kind::integer) {
                            return _error(errc::type_mismatch, t.at, "a !!float scalar that is not a number");
                        }
                        k = yaml::kind::floating;
                    } else if (name == "seq" || name == "map") {
                        return _error(errc::type_mismatch, t.at, "a !!seq or !!map tag on a scalar");
                    } else {
                        k = t.style == YamlStyle::plain ? yaml_resolve(v) : yaml::kind::string;
                        out = YamlAccess::with_tag(YamlAccess::scalar(k, string(v)), string(tag));
                        return true;
                    }
                    out = YamlAccess::scalar(k, string(v));
                    return true;
                }
                // an application's tag: kept, the plain text read by the core schema all the same
                k = t.style == YamlStyle::plain ? yaml_resolve(v) : yaml::kind::string;
                out = YamlAccess::with_tag(YamlAccess::scalar(k, string(v)), string(tag));
                return true;
            }

            // A collection's tag: the core's checked and dropped, another kept
            bool _collection_tag(yaml& node, const std::string& tag, bool has_tag, size_t at) noexcept {
                if (!has_tag || tag == "!") {
                    return true;
                }
                if (tag.compare(0, Core.size(), Core) == 0) {
                    std::string_view name = std::string_view(tag).substr(Core.size());
                    if ((name == "seq" && node.is_sequence()) || (name == "map" && node.is_mapping())) {
                        return true;
                    }
                    if (name == "seq" || name == "map" || name == "str" || name == "int" || name == "float" || name == "bool" || name == "null") {
                        return _error(errc::type_mismatch, at, "a core tag of another kind on a collection");
                    }
                }
                node = YamlAccess::with_tag(node, string(tag));
                return true;
            }

            static void _add(size_t& size, size_t more) noexcept {
                size = size + more < size ? size_t(-1) : size + more;
            }

            // One node: its properties, then its content; size, the nodes it
            // holds counting through its aliases
            bool _node(bool block, bool indentless, uint32_t depth, yaml& out, size_t& size) noexcept {
                const YamlToken* t;
                YamlToken tok;
                std::string anchor, tag;
                bool has_anchor = false, has_tag = false;
                size_t at = 0;
                for (;;) {
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::anchor && !has_anchor) {
                        _next(tok);
                        anchor = tok.value;
                        has_anchor = true;
                    } else if (t->type == YamlTok::tag && !has_tag) {
                        _next(tok);
                        if (!_resolve_tag(tok, tag)) {
                            return false;
                        }
                        has_tag = true;
                    } else {
                        break;
                    }
                }
                at = t->at;
                size = 1;
                switch (t->type) {
                    case YamlTok::alias: {
                        if (has_anchor || has_tag) {
                            return _error(errc::syntax, t->at, "an alias with properties");
                        }
                        _next(tok);
                        auto a = _anchors.find(string(tok.value));
                        if (a == _anchors.end()) {
                            return _error(errc::syntax, tok.at, "an alias of no anchor before it");
                        }
                        _add(_alias_nodes, a->second.size);
                        if (_alias_nodes > _o.max_alias_nodes) {
                            return _error(errc::limit_exceeded, tok.at, "more nodes reached through aliases than max_alias_nodes");
                        }
                        out = a->second.node;
                        size = a->second.size;
                        return true;
                    }
                    case YamlTok::scalar:
                        _next(tok);
                        if (!_scalar(tok, tag, has_tag, out)) {
                            return false;
                        }
                        break;
                    case YamlTok::flow_sequence_start:
                    case YamlTok::flow_mapping_start:
                    case YamlTok::block_sequence_start:
                    case YamlTok::block_mapping_start:
                    case YamlTok::block_entry: {
                        if (t->type == YamlTok::block_entry && !indentless) {
                            goto empty;
                        }
                        if ((t->type == YamlTok::block_sequence_start || t->type == YamlTok::block_mapping_start) && !block) {
                            return _error(errc::syntax, t->at, "a block collection inside a flow collection");
                        }
                        if (depth + 1 > _o.max_depth) {
                            return _error(errc::depth_limit, t->at, "collections nested deeper than max_depth");
                        }
                        bool ok = t->type == YamlTok::flow_sequence_start ? _flow_sequence(depth + 1, out, size)
                                : t->type == YamlTok::flow_mapping_start ? _flow_mapping(depth + 1, out, size)
                                : t->type == YamlTok::block_sequence_start ? _block_sequence(depth + 1, out, size)
                                : t->type == YamlTok::block_mapping_start ? _block_mapping(depth + 1, out, size)
                                                                          : _indentless_sequence(depth + 1, out, size);
                        if (!ok || !_collection_tag(out, tag, has_tag, at)) {
                            return false;
                        }
                        break;
                    }
                    default:
                    empty:
                        // an empty node: null, or the empty string of its tag
                        if (!has_anchor && !has_tag && !_ends_node(t->type)) {
                            return _error(errc::syntax, t->at, "a token where a node was expected");
                        }
                        {
                            YamlToken e;
                            e.type = YamlTok::scalar;
                            e.at = t->at;
                            if (!_scalar(e, tag, has_tag, out)) {
                                return false;
                            }
                        }
                        break;
                }
                if (has_anchor) {
                    _anchors.insert_or_assign(string(anchor), Anchored{out, size});
                }
                return true;
            }

            // A node, or null when the next token ends one before it begins
            bool _maybe_node(bool block, bool indentless, uint32_t depth, yaml& out, size_t& size) noexcept {
                const YamlToken* t;
                if (!_peek(t)) {
                    return false;
                }
                bool ends = _ends_node(t->type) && !(indentless && t->type == YamlTok::block_entry);
                if (ends) {
                    out = yaml();
                    size = 1;
                    return true;
                }
                return _node(block, indentless, depth, out, size);
            }

            bool _block_sequence(uint32_t depth, yaml& out, size_t& size) noexcept {
                YamlToken tok;
                _next(tok);
                vector<yaml> items;
                for (;;) {
                    const YamlToken* t;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::block_end) {
                        _next(tok);
                        break;
                    }
                    if (t->type != YamlTok::block_entry) {
                        return _error(errc::syntax, t->at, "something other than '- ' in a block sequence");
                    }
                    _next(tok);
                    yaml item;
                    size_t s;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::block_entry || t->type == YamlTok::block_end) {
                        item = yaml();
                        s = 1;
                    } else if (!_node(true, false, depth, item, s)) {
                        return false;
                    }
                    _add(size, s);
                    items.push_back(item);
                }
                out = yaml::sequence(items);
                return true;
            }

            bool _indentless_sequence(uint32_t depth, yaml& out, size_t& size) noexcept {
                YamlToken tok;
                vector<yaml> items;
                for (;;) {
                    const YamlToken* t;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type != YamlTok::block_entry) {
                        break;
                    }
                    _next(tok);
                    if (!_peek(t)) {
                        return false;
                    }
                    yaml item;
                    size_t s = 1;
                    if (t->type == YamlTok::block_entry || t->type == YamlTok::key || t->type == YamlTok::value || t->type == YamlTok::block_end) {
                        item = yaml();
                    } else if (!_node(true, false, depth, item, s)) {
                        return false;
                    }
                    _add(size, s);
                    items.push_back(item);
                }
                out = yaml::sequence(items);
                return true;
            }

            bool _block_mapping(uint32_t depth, yaml& out, size_t& size) noexcept {
                YamlToken tok;
                size_t start = 0;
                {
                    const YamlToken* t;
                    _peek(t);
                    start = t->at;
                }
                _next(tok);
                vector<yaml::member> ms;
                std::vector<size_t> hashes;
                for (;;) {
                    const YamlToken* t;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::block_end) {
                        _next(tok);
                        break;
                    }
                    yaml key, value;
                    size_t ks = 1, vs = 1;
                    size_t key_at = t->at;
                    if (t->type == YamlTok::key) {
                        _next(tok);
                        if (!_peek(t)) {
                            return false;
                        }
                        if (t->type == YamlTok::key || t->type == YamlTok::value || t->type == YamlTok::block_end) {
                            key = yaml();
                        } else if (!_node(true, true, depth, key, ks)) {
                            return false;
                        }
                        if (!_peek(t)) {
                            return false;
                        }
                    } else if (t->type != YamlTok::value) {
                        return _error(errc::syntax, t->at, "something other than a key in a block mapping");
                    }
                    if (t->type == YamlTok::value) {
                        _next(tok);
                        if (!_peek(t)) {
                            return false;
                        }
                        if (t->type == YamlTok::key || t->type == YamlTok::value || t->type == YamlTok::block_end) {
                            value = yaml();
                        } else if (!_node(true, true, depth, value, vs)) {
                            return false;
                        }
                    }
                    _add(size, ks);
                    _add(size, vs);
                    if (!_put(ms, hashes, key, value, key_at)) {
                        return false;
                    }
                }
                (void)start;
                out = yaml::mapping(ms);
                return true;
            }

            // A member added, its key compared with the others' by their
            // hashes first (hashes, beside the members)
            bool _put(vector<yaml::member>& ms, std::vector<size_t>& hashes, const yaml& key, const yaml& value, size_t at) noexcept {
                size_t h = key.hash();
                for (size_t i = 0; i < hashes.size(); ++i) {
                    if (hashes[i] == h && ms[i].key == key) {
                        if (!_o.allow_duplicate_keys) {
                            return _error(errc::duplicate_key, at, "a key given twice in a mapping");
                        }
                        ms[i].value = value;
                        return true;
                    }
                }
                ms.push_back(yaml::member{key, value});
                hashes.push_back(h);
                return true;
            }

            bool _flow_sequence(uint32_t depth, yaml& out, size_t& size) noexcept {
                YamlToken tok;
                _next(tok);
                vector<yaml> items;
                bool first = true;
                for (;;) {
                    const YamlToken* t;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::flow_sequence_end) {
                        _next(tok);
                        break;
                    }
                    if (!first) {
                        if (t->type != YamlTok::flow_entry) {
                            return _error(errc::syntax, t->at, "no ',' or ']' after an entry of a flow sequence");
                        }
                        _next(tok);
                        if (!_peek(t)) {
                            return false;
                        }
                        if (t->type == YamlTok::flow_sequence_end) {
                            _next(tok);
                            break;
                        }
                    }
                    first = false;
                    if (t->type == YamlTok::flow_entry) {
                        return _error(errc::syntax, t->at, "an empty entry in a flow sequence");
                    }
                    if (t->type == YamlTok::key || t->type == YamlTok::value) {
                        // a single pair: a mapping of one member
                        yaml key, value;
                        size_t ks = 1, vs = 1;
                        if (t->type == YamlTok::key) {
                            _next(tok);
                            if (!_peek(t)) {
                                return false;
                            }
                            if (t->type != YamlTok::value && t->type != YamlTok::flow_entry && t->type != YamlTok::flow_sequence_end) {
                                if (!_node(false, false, depth, key, ks)) {
                                    return false;
                                }
                                if (!_peek(t)) {
                                    return false;
                                }
                            }
                        }
                        if (t->type == YamlTok::value) {
                            _next(tok);
                            if (!_maybe_node(false, false, depth, value, vs)) {
                                return false;
                            }
                        }
                        _add(size, ks + vs + 1);
                        items.push_back(yaml::mapping({yaml::member{key, value}}));
                        continue;
                    }
                    yaml item;
                    size_t s;
                    if (!_node(false, false, depth, item, s)) {
                        return false;
                    }
                    _add(size, s);
                    items.push_back(item);
                }
                out = yaml::sequence(items);
                return true;
            }

            bool _flow_mapping(uint32_t depth, yaml& out, size_t& size) noexcept {
                YamlToken tok;
                _next(tok);
                vector<yaml::member> ms;
                std::vector<size_t> hashes;
                bool first = true;
                for (;;) {
                    const YamlToken* t;
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::flow_mapping_end) {
                        _next(tok);
                        break;
                    }
                    if (!first) {
                        if (t->type != YamlTok::flow_entry) {
                            return _error(errc::syntax, t->at, "no ',' or '}' after an entry of a flow mapping");
                        }
                        _next(tok);
                        if (!_peek(t)) {
                            return false;
                        }
                        if (t->type == YamlTok::flow_mapping_end) {
                            _next(tok);
                            break;
                        }
                    }
                    first = false;
                    if (t->type == YamlTok::flow_entry) {
                        return _error(errc::syntax, t->at, "an empty entry in a flow mapping");
                    }
                    yaml key, value;
                    size_t ks = 1, vs = 1;
                    size_t key_at = t->at;
                    if (t->type == YamlTok::key) {
                        _next(tok);
                        if (!_peek(t)) {
                            return false;
                        }
                        if (t->type != YamlTok::value && t->type != YamlTok::flow_entry && t->type != YamlTok::flow_mapping_end) {
                            if (!_node(false, false, depth, key, ks)) {
                                return false;
                            }
                        }
                    } else if (t->type != YamlTok::value) {
                        if (!_node(false, false, depth, key, ks)) {
                            return false;
                        }
                    }
                    if (!_peek(t)) {
                        return false;
                    }
                    if (t->type == YamlTok::value) {
                        _next(tok);
                        if (!_maybe_node(false, false, depth, value, vs)) {
                            return false;
                        }
                    }
                    _add(size, ks);
                    _add(size, vs);
                    if (!_put(ms, hashes, key, value, key_at)) {
                        return false;
                    }
                }
                out = yaml::mapping(ms);
                return true;
            }
        };
    }

    inline expected<vector<yaml>, yaml::error> yaml::parse_all(const string& text) noexcept {
        return parse_all(text, options());
    }

    inline expected<vector<yaml>, yaml::error> yaml::parse_all(const string& text, const options& o) noexcept {
        auto v = text.view();
        if (!detail::utf8_text_valid(v.data(), v.data() + v.size())) {
            size_t at = 0;
            while (at < v.size()) {
                int k = detail::utf8_sequence(v.data() + at, v.data() + v.size());
                if (k <= 0) {
                    break;
                }
                at += size_t(k);
            }
            error e(errc::invalid_utf8, at, string("invalid UTF-8"));
            return unexpected<error>(std::move(e.locate(text)));
        }
        // the characters YAML allows (§5.1): no control character but tab,
        // line feed and carriage return; no C1 control but NEL; no U+FFFE, U+FFFF
        for (size_t i = 0; i < v.size(); ++i) {
            auto b = uint8_t(v[i]);
            bool bad = (b < 0x20 && b != '\t' && b != '\n' && b != '\r') || b == 0x7F
                    || (b == 0xC2 && i + 1 < v.size() && uint8_t(v[i + 1]) < 0xA0 && uint8_t(v[i + 1]) != 0x85)
                    || (b == 0xEF && i + 2 < v.size() && uint8_t(v[i + 1]) == 0xBF && uint8_t(v[i + 2]) >= 0xBE);
            if (bad) {
                error e(errc::invalid_character, i, string("a character YAML does not allow"));
                return unexpected<error>(std::move(e.locate(text)));
            }
        }
        return detail::YamlParser(v, o).all();
    }

    inline expected<yaml, yaml::error> yaml::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<yaml, yaml::error> yaml::parse(const string& text, const options& o) noexcept {
        auto docs = parse_all(text, o);
        if (!docs) {
            return unexpected<error>(std::move(docs.error()));
        }
        if (docs->size() > 1) {
            size_t at = text.view().find("\n---");
            error e(errc::syntax, at == std::string_view::npos ? 0 : at + 1, string("a second document where one was expected"));
            return unexpected<error>(std::move(e.locate(text)));
        }
        return docs->empty() ? yaml() : (*docs)[0];
    }

    inline expected<yaml, yaml::error> yaml::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<yaml, yaml::error> yaml::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<yaml, yaml::error>> yaml::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<yaml, yaml::error>> yaml::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    // --- writing ---

    namespace detail {
        // The bytes of a character YAML does not allow raw at s[i] (a C1
        // control but NEL, U+FFFE, U+FFFF); 0 for any other
        inline size_t yaml_unprintable(std::string_view s, size_t i) noexcept {
            auto b = uint8_t(s[i]);
            if (b == 0xC2 && i + 1 < s.size() && uint8_t(s[i + 1]) < 0xA0 && uint8_t(s[i + 1]) != 0x85) {
                return 2;
            }
            if (b == 0xEF && i + 2 < s.size() && uint8_t(s[i + 1]) == 0xBF && uint8_t(s[i + 2]) >= 0xBE) {
                return 3;
            }
            return 0;
        }

        // Whether a string reads back as itself written plain
        inline bool yaml_plain_safe(std::string_view s, bool key) noexcept {
            if (s.empty() || yaml_resolve(s) != yaml::kind::string) {
                return false;
            }
            char c = s[0];
            if (c == '-' || c == '?' || c == ':' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == '#' || c == '&'
                || c == '*' || c == '!' || c == '|' || c == '>' || c == '\'' || c == '"' || c == '%' || c == '@' || c == '`' || c == ' ') {
                return false;
            }
            if (s.back() == ' ' || s.back() == ':' || s.substr(0, 3) == "---" || s.substr(0, 3) == "...") {
                return false;   // a document marker where a document starts
            }
            for (size_t i = 0; i < s.size(); ++i) {
                auto b = uint8_t(s[i]);
                if (b < 0x20 || b == 0x7F) {
                    return false;
                }
                if (s[i] == ':' && i + 1 < s.size() && s[i + 1] == ' ') {
                    return false;
                }
                if (s[i] == '#' && s[i - 1] == ' ') {
                    return false;
                }
                if ((b == 0xC2 && i + 1 < s.size() && (uint8_t(s[i + 1]) == 0x85 || uint8_t(s[i + 1]) == 0xA0)) || yaml_unprintable(s, i)) {
                    return false;   // NEL, NBSP, a character YAML has only escaped
                }
                if (b == 0xEF && i + 2 < s.size() && uint8_t(s[i + 1]) == 0xBB && uint8_t(s[i + 2]) == 0xBF) {
                    return false;   // a byte order mark
                }
                if (b == 0xE2 && i + 2 < s.size() && uint8_t(s[i + 1]) == 0x80 && (uint8_t(s[i + 2]) == 0xA8 || uint8_t(s[i + 2]) == 0xA9)) {
                    return false;   // LS, PS
                }
            }
            (void)key;
            return true;
        }

        inline void yaml_double_quoted(std::string& out, std::string_view s) noexcept {
            static constexpr char digits[] = "0123456789abcdef";
            out += '"';
            for (size_t i = 0; i < s.size(); ++i) {
                auto b = uint8_t(s[i]);
                switch (s[i]) {
                    case '"': out += "\\\""; continue;
                    case '\\': out += "\\\\"; continue;
                    case '\n': out += "\\n"; continue;
                    case '\t': out += "\\t"; continue;
                    case '\r': out += "\\r"; continue;
                    case '\0': out += "\\0"; continue;
                    default: break;
                }
                if (b < 0x20 || b == 0x7F) {
                    out += "\\x";
                    out += digits[b >> 4];
                    out += digits[b & 15];
                } else if (b == 0xC2 && i + 1 < s.size() && (uint8_t(s[i + 1]) == 0x85 || uint8_t(s[i + 1]) == 0xA0)) {
                    out += uint8_t(s[i + 1]) == 0x85 ? "\\N" : "\\_";
                    ++i;
                } else if (size_t k = yaml_unprintable(s, i)) {
                    if (k == 2) {
                        out += "\\x";
                        out += digits[uint8_t(s[i + 1]) >> 4];
                        out += digits[uint8_t(s[i + 1]) & 15];
                    } else {
                        out += uint8_t(s[i + 2]) == 0xBE ? "\\uFFFE" : "\\uFFFF";
                    }
                    i += k - 1;
                } else if (b == 0xE2 && i + 2 < s.size() && uint8_t(s[i + 1]) == 0x80 && (uint8_t(s[i + 2]) == 0xA8 || uint8_t(s[i + 2]) == 0xA9)) {
                    out += uint8_t(s[i + 2]) == 0xA8 ? "\\L" : "\\P";
                    i += 2;
                } else if (b == 0xEF && i + 2 < s.size() && uint8_t(s[i + 1]) == 0xBB && uint8_t(s[i + 2]) == 0xBF) {
                    out += "\\uFEFF";
                    i += 2;
                } else {
                    out += s[i];
                }
            }
            out += '"';
        }

        // Whether a string is written as a literal block: lines of
        // printable text, and a line with content first
        inline bool yaml_literal(std::string_view s) noexcept {
            if (s.find('\n') == std::string_view::npos || s.find_first_not_of('\n') == std::string_view::npos) {
                return false;
            }
            for (size_t i = 0; i < s.size(); ++i) {
                auto b = uint8_t(s[i]);
                if ((b < 0x20 && s[i] != '\n' && s[i] != '\t') || b == 0x7F) {
                    return false;
                }
                if ((b == 0xC2 && i + 1 < s.size() && uint8_t(s[i + 1]) == 0x85) || yaml_unprintable(s, i)) {
                    return false;
                }
                if (b == 0xE2 && i + 2 < s.size() && uint8_t(s[i + 1]) == 0x80 && (uint8_t(s[i + 2]) == 0xA8 || uint8_t(s[i + 2]) == 0xA9)) {
                    return false;
                }
                if (b == 0xEF && i + 2 < s.size() && uint8_t(s[i + 1]) == 0xBB && uint8_t(s[i + 2]) == 0xBF) {
                    return false;
                }
            }
            // a line of the text that would end the block: --- or ... at the
            // start of a line is fine indented; a leading tab is not
            size_t first = s.find_first_not_of('\n');
            return s[first] != '\t';
        }

        class YamlWriter {
        public:
            std::string out;

            // The scalar written after what the line holds; indent, the
            // column a block scalar's lines start at
            void scalar(const yaml& y, size_t indent, bool key) {
                std::string tag(y.tag().view());
                if (!tag.empty()) {
                    _tag(tag);
                    out += ' ';
                }
                if (y.is_string()) {
                    auto s = y._text();
                    std::string_view v = s.view();
                    if (yaml_plain_safe(v, key)) {
                        out.append(v);
                    } else if (!key && yaml_literal(v)) {
                        size_t trailing = v.size() - (v.find_last_not_of('\n') + 1);
                        out += '|';
                        size_t first = v.find_first_not_of('\n');
                        if (v[first] == ' ') {
                            out += '2';
                        }
                        out += trailing == 0 ? "-" : trailing == 1 ? "" : "+";
                        out += '\n';
                        size_t at = 0;
                        std::string_view body = v.substr(0, v.size() - (trailing ? 1 : 0));
                        while (at <= body.size()) {
                            size_t nl = body.find('\n', at);
                            std::string_view line = body.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
                            if (!line.empty()) {
                                out.append(indent, ' ');
                                out.append(line);
                            }
                            if (nl == std::string_view::npos) {
                                break;
                            }
                            out += '\n';
                            at = nl + 1;
                        }
                        return;   // the caller's line break ends the last line
                    } else {
                        yaml_double_quoted(out, v);
                    }
                    return;
                }
                auto t = y.text();
                if (y.is_null() && t.empty()) {
                    out += "null";
                    return;
                }
                out.append(t.view());
            }

            static bool _uri_char(char c) noexcept {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '#' || c == ';'
                    || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' || c == '$' || c == ','
                    || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')' || c == '['
                    || c == ']';
            }

            // a shorthand's suffix: the characters of a URI but '!' and the flow indicators
            static bool _shorthand(std::string_view s) noexcept {
                if (s.empty()) {
                    return false;
                }
                for (char c : s) {
                    if (!_uri_char(c) || c == '!' || c == ',' || c == '[' || c == ']') {
                        return false;
                    }
                }
                return true;
            }

            // A tag as it reads back: !!name, !name, or !<...> with the
            // characters a URI has not %-escaped
            void _tag(const std::string& tag) {
                std::string_view core = "tag:yaml.org,2002:";
                std::string_view t = tag;
                if (t.substr(0, core.size()) == core && _shorthand(t.substr(core.size()))) {
                    out += "!!";
                    out.append(t.substr(core.size()));
                    return;
                }
                if (t.size() > 1 && t[0] == '!' && _shorthand(t.substr(1))) {
                    out.append(t);
                    return;
                }
                static constexpr char digits[] = "0123456789ABCDEF";
                out += "!<";
                for (char c : t) {
                    if (_uri_char(c)) {
                        out += c;
                    } else {
                        out += '%';
                        out += digits[uint8_t(c) >> 4];
                        out += digits[uint8_t(c) & 15];
                    }
                }
                out += '>';
            }

            static bool _compact(const yaml& y) noexcept {
                return (y.is_sequence() || y.is_mapping()) && !y.empty();
            }

            // A flow form of a node on one line (a key that is a collection,
            // an empty collection)
            void flow(const yaml& top) {
                struct Frame {
                    const yaml* node;
                    size_t at;
                };
                std::vector<Frame> stack;
                auto one = [&](const yaml& y) {
                    std::string tag(y.tag().view());
                    if (!tag.empty()) {
                        _tag(tag);
                        out += ' ';
                    }
                    if (y.is_sequence()) {
                        out += '[';
                        stack.push_back({&y, 0});
                    } else if (y.is_mapping()) {
                        out += '{';
                        stack.push_back({&y, 0});
                    } else if (y.is_string()) {
                        auto s = y._text();
                        if (yaml_plain_safe(s.view(), true) && s.view().find_first_of(",[]{}") == std::string_view::npos) {
                            out.append(s.view());
                        } else {
                            yaml_double_quoted(out, s.view());
                        }
                    } else if (y.is_null() && y.text().empty()) {
                        out += "null";
                    } else {
                        out.append(y.text().view());
                    }
                };
                one(top);
                while (!stack.empty()) {
                    Frame& f = stack.back();
                    const yaml* n = f.node;
                    bool seq = n->is_sequence();
                    size_t count = seq ? n->size() : n->size() * 2;
                    if (f.at == count) {
                        out += seq ? ']' : '}';
                        stack.pop_back();
                        continue;
                    }
                    size_t i = f.at++;
                    if (seq) {
                        if (i) {
                            out += ", ";
                        }
                        one(n->elements()[i]);
                    } else {
                        if (i % 2 == 0) {
                            if (i) {
                                out += ", ";
                            }
                            out += "? ";
                            one(n->members()[i / 2].key);
                        } else {
                            out += ": ";
                            one(n->members()[i / 2].value);
                        }
                    }
                }
            }

            // A document in block style, without recursion
            void document(const yaml& top) {
                struct Frame {
                    const yaml* node;
                    size_t indent;
                    size_t at;
                    bool inline_first;   // the first entry's line has its prefix already
                    bool in_value;       // a complex key written: its value next
                };
                std::vector<Frame> stack;
                auto value_after = [&](const yaml& v, size_t indent, bool after_dash) {
                    // what follows "- " or "key:": a scalar on the line, or a collection
                    std::string tag(v.tag().view());
                    if (_compact(v)) {
                        if (!tag.empty()) {
                            if (!after_dash) {
                                out += ' ';
                            }
                            _tag(tag);
                            out += '\n';
                            stack.push_back({&v, indent, 0, false, false});
                            return;
                        }
                        if (after_dash) {
                            stack.push_back({&v, indent, 0, true, false});
                        } else {
                            out += '\n';
                            stack.push_back({&v, indent, 0, false, false});
                        }
                        return;
                    }
                    if (!after_dash) {
                        out += ' ';
                    }
                    if (v.is_sequence() || v.is_mapping()) {
                        flow(v);
                    } else {
                        scalar(v, indent, false);
                    }
                    out += '\n';
                };
                if (!_compact(top)) {
                    if (top.is_sequence() || top.is_mapping()) {
                        flow(top);
                    } else {
                        scalar(top, 2, false);
                    }
                    out += '\n';
                    return;
                }
                std::string top_tag(top.tag().view());
                if (!top_tag.empty()) {
                    _tag(top_tag);
                    out += '\n';
                }
                stack.push_back({&top, 0, 0, false, false});
                while (!stack.empty()) {
                    Frame& f = stack.back();
                    const yaml* n = f.node;
                    if (f.at == n->size()) {
                        stack.pop_back();
                        continue;
                    }
                    size_t i = f.at;
                    size_t indent = f.indent;
                    bool first_inline = f.inline_first && i == 0 && !f.in_value;
                    if (n->is_sequence()) {
                        ++f.at;
                        if (!first_inline) {
                            out.append(indent, ' ');
                        }
                        out += "- ";
                        value_after(n->elements()[i], indent + 2, true);
                        continue;
                    }
                    const yaml::member& m = n->members()[i];
                    bool complex = (m.key.is_sequence() || m.key.is_mapping()) && !m.key.empty();
                    // an implicit key is at most 1024 characters (§7.4.2): a longer one explicit
                    bool long_key = !complex && !m.key.is_sequence() && !m.key.is_mapping() && m.key.text().size() > 1000;
                    if (f.in_value) {
                        // the value of a complex key
                        f.in_value = false;
                        ++f.at;
                        out.append(indent, ' ');
                        out += ':';
                        value_after(m.value, indent + 2, false);
                        continue;
                    }
                    if (!first_inline) {
                        out.append(indent, ' ');
                    }
                    if (complex) {
                        out += "? ";
                        f.in_value = true;
                        std::string key_tag(m.key.tag().view());
                        if (!key_tag.empty()) {
                            _tag(key_tag);
                            out += '\n';
                            stack.push_back({&m.key, indent + 2, 0, false, false});
                        } else {
                            stack.push_back({&m.key, indent + 2, 0, true, false});
                        }
                        continue;
                    }
                    ++f.at;
                    if (long_key) {
                        out += "? ";
                        scalar(m.key, indent + 2, true);
                        out += '\n';
                        out.append(indent, ' ');
                        out += ':';
                        value_after(m.value, indent + 2, false);
                        continue;
                    }
                    if (m.key.is_sequence() || m.key.is_mapping()) {
                        flow(m.key);
                    } else {
                        scalar(m.key, indent + 2, true);
                    }
                    out += ':';
                    value_after(m.value, indent + 2, false);
                }
            }
        };
    }

    inline string yaml::to_string() const {
        detail::YamlWriter w;
        w.document(*this);
        if (w.out.size() > string::max_size()) {
            throw length_error("sgcl::encoding::yaml: a text longer than a string can hold");
        }
        return string(w.out);
    }

    // --- JSON ---

    inline yaml yaml::from_json(const json& top) noexcept {
        struct Frame {
            const json* src;
            size_t next;
            vector<yaml> items;
        };
        auto scalar = [](const json& j, bool& container) -> yaml {
            container = false;
            switch (j.type()) {
                case json::kind::null: return yaml();
                case json::kind::boolean: return yaml(*j.as_bool());
                case json::kind::number:
                    if (auto i = j.as_int()) {
                        return yaml(*i);
                    }
                    if (auto u = j.as_uint()) {
                        return yaml(*u);
                    }
                    if (auto t = j.number_text()) {
                        auto k = detail::yaml_resolve(t->view());
                        if (k == kind::integer || k == kind::floating) {
                            return detail::YamlAccess::scalar(k, *t);
                        }
                    }
                    return yaml(j.as_double(0.0));
                case json::kind::string: return yaml(*j.as_string());
                default:
                    container = true;
                    return yaml();
            }
        };
        bool container;
        yaml first = scalar(top, container);
        if (!container) {
            return first;
        }
        vector<Frame> stack;
        stack.push_back(Frame{&top, 0, {}});
        yaml result;
        while (!stack.empty()) {
            Frame& f = stack.back();
            size_t n = f.src->size();
            if (f.next == n) {
                yaml made;
                if (f.src->type() == json::kind::array) {
                    made = sequence(f.items);
                } else {
                    vector<member> ms;
                    auto src = f.src->members();
                    for (size_t i = 0; i < n; ++i) {
                        ms.push_back(member{yaml(src[i].key), f.items[i]});
                    }
                    made = mapping(ms);
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
            yaml c = scalar(child, container);
            if (container) {
                stack.push_back(Frame{&child, 0, {}});
            } else {
                f.items.push_back(c);
            }
        }
        return result;
    }

    inline json yaml::to_json() const noexcept {
        struct Frame {
            const yaml* src;
            size_t next;
            vector<json> items;
        };
        auto scalar = [](const yaml& y, bool& container) -> json {
            container = false;
            switch (y.type()) {
                case kind::null: return json();
                case kind::boolean: return json(*y.as_bool());
                case kind::integer:
                    if (auto i = y.as_int()) {
                        return json(*i);
                    }
                    if (auto u = y.as_uint()) {
                        return json(*u);
                    }
                    return json(y.as_double(0.0));
                case kind::floating: {
                    double d = *y.as_double();
                    return std::isfinite(d) ? json(d) : json();
                }
                case kind::string: return json(*y.as_string());
                default:
                    container = true;
                    return json();
            }
        };
        bool container;
        json first = scalar(*this, container);
        if (!container) {
            return first;
        }
        vector<Frame> stack;
        stack.push_back(Frame{this, 0, {}});
        json result;
        while (!stack.empty()) {
            Frame& f = stack.back();
            size_t n = f.src->size();
            if (f.next == n) {
                json made;
                if (f.src->is_sequence()) {
                    made = json::array(f.items);
                } else if (n == 0) {
                    made = json::object({});
                } else {
                    json::builder b;
                    auto ms = f.src->members();
                    for (size_t i = 0; i < n; ++i) {
                        const yaml& k = ms[i].key;
                        string key;
                        if (k.is_string()) {
                            key = *k.as_string();
                        } else if (k.is_sequence() || k.is_mapping()) {
                            detail::YamlWriter w;
                            w.flow(k);
                            key = string(w.out);
                        } else {
                            key = k.text();
                        }
                        b.set(key, f.items[i]);
                    }
                    made = b.build();
                }
                stack.pop_back();
                if (stack.empty()) {
                    result = made;
                } else {
                    stack.back().items.push_back(made);
                }
                continue;
            }
            const yaml* child = f.src->is_sequence() ? f.src->_elements() + f.next : &f.src->_members()[f.next].value;
            ++f.next;
            json j = scalar(*child, container);
            if (container) {
                stack.push_back(Frame{child, 0, {}});
            } else {
                f.items.push_back(j);
            }
        }
        return result;
    }

    namespace detail {
        // A yaml's save in a task: the value copied into the frame
        inline async::task<expected<void, yaml::error>> yaml_save_task(string path, yaml value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<yaml, yaml::error> yaml::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return yaml::parse(in); });
    }

    inline async::task<expected<yaml, yaml::error>> yaml::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return yaml::load(path); });
    }

    inline expected<void, yaml::error> yaml::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, yaml::error>> yaml::async_save(string path) const noexcept {
        return detail::yaml_save_task(std::move(path), *this);
    }
}

template<>
struct std::hash<sgcl::encoding::yaml> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::yaml& y) const noexcept {
        return y.hash();
    }
};
