//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/json_number.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/make_tracked.h"
#include "../core/map.h"
#include "../core/ordered_map.h"
#include "../core/ordered_set.h"
#include "../core/set.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/variant.h"
#include "../core/vector.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// A type of the program described by its fields, once for every format of
// the module: a method `void describe(field_list& f)` that names each
// field (f.add("name", name)), read and written by JSON, CSV and XML (and
// later the serialization of object graphs) alike.
//
//     struct user {
//         string name;
//         int age = 0;
//         optional<address> home;
//
//         void describe(field_list& f) {
//             f.add("name", name).required();
//             f.add("age", age).omit_empty();
//             f.add("home", home);
//         }
//     };
//
// A type of another library is described by a free function
// `void describe(field_list& f, lib::point& p)` that argument-dependent
// lookup finds. Two doors for a type fields cannot describe:
// `json to_json() const` with `static optional<T> from_json(const json&)`
// (JSON only), and `string to_text() const` with
// `static optional<T> from_text(const string&)` (one text for every format:
// a JSON string, a CSV field, an XML attribute, a key of a map).
//
// How it works, for the formats: describe() is called on the object
// itself whenever the object is read or written, and it fills the
// field_list with the fields — the name, the address of the member, the
// operations of its type (detail::ValueOps: one constant table per type)
// and the options. A format walks that list and, through the operations,
// the values: every type a field may have is one of the kinds of
// detail::ValueKind, so a format is written once against the kinds and
// never against the types. Nothing is kept between two objects; nothing
// is static but the tables of operations, which hold no tracked pointer.
namespace sgcl::encoding {
    class json;
    class field_list;

    namespace detail {
        using namespace sgcl::detail;

        // What a value of a field is, as the formats see it
        enum class ValueKind : uint8_t {
            boolean,
            signed_integer,     // int8_t ... int64_t, and the enums without names
            unsigned_integer,
            floating,           // float or double (size() says which)
            string,             // sgcl::string, std::string
            text,               // a type with to_text() and from_text()
            enumeration,        // an enum: an integer, or one of the field's names
            optional,           // optional<T>: null or the value
            pointer,            // tracked_ptr<T>: null or a new object
            sequence,           // vector, deque, list, dynamic_array, the immutable ones... : an array
            fixed,              // array<T, N>, std::array, T[N]: an array of exactly N
            set,                // set, sorted_set...: an array
            map,                // map, sorted_map, ordered_map...: an object keyed by the key's text
            tuple,              // pair, tuple: an array of their elements
            record,             // a type with describe(): an object of its fields
            variant,            // variant: an object with a tag (the field's tagged())
            json,               // encoding::json: the value as it is
            custom_json         // a type with to_json() and from_json()
        };

        // A callback of a format, called with a context of its own: what a
        // table of operations hands an element, an entry or a value to
        using ElementFn = bool (*)(void* ctx, const void* element);
        using EntryFn = bool (*)(void* ctx, std::string_view key, const void* value);
        using ReadFn = bool (*)(void* ctx, void* value);
        using MoreFn = bool (*)(void* ctx);
        using KeyFn = bool (*)(void* ctx, std::string_view& key);   // false: no more keys

        // How reading an entry of a map ended
        enum class EntryStatus : uint8_t {
            done,
            failed,          // the format's callback said so
            bad_key          // the key's text is not a key of the map's type
        };

        // The operations of one type, as constant data: the kind and the
        // functions that kind needs (the others are null). A format reads
        // and writes a value through them, knowing nothing else of it.
        struct ValueOps {
            ValueKind kind;
            uint8_t size = 0;                  // the bytes of an integer, a floating or an enum type
            const char* name = "";             // a name for the messages: "int32", "string"...
            // the test of omit_empty(): the default value, an empty text or container, null
            bool (*is_empty)(const void*) = nullptr;
            // boolean
            bool (*get_bool)(const void*) = nullptr;
            void (*set_bool)(void*, bool) = nullptr;
            // integers and enums (an enum by its underlying value); a set
            // out of the type's range fails and leaves the value
            int64_t (*get_int)(const void*) = nullptr;
            bool (*set_int)(void*, int64_t) = nullptr;
            uint64_t (*get_uint)(const void*) = nullptr;
            bool (*set_uint)(void*, uint64_t) = nullptr;
            // floating
            double (*get_double)(const void*) = nullptr;
            void (*set_double)(void*, double) = nullptr;
            // a number set from its decimal literal (JSON's grammar): an
            // integer exactly, a float or a double rounded once. 0: done;
            // 1: not a value of the type (1.5 for an int); 2: out of its range
            int (*set_literal)(void*, std::string_view) = nullptr;
            // the text of a number: a float with a float's shortest digits
            size_t (*number_text)(const void*, char* out) = nullptr;
            // string and text: the value's text; set from a text (false:
            // from_text refused it)
            string (*get_text)(const void*) = nullptr;
            bool (*set_text)(void*, std::string_view) = nullptr;
            // optional, pointer: whether there is a value, the value, a
            // value made (default, or a new object) and given, and null
            bool (*has_value)(const void*) = nullptr;
            const void* (*value)(const void*) = nullptr;
            void* (*emplace)(void*) = nullptr;
            void (*reset)(void*) = nullptr;
            // the element's operations: of an optional, a pointer, a
            // sequence, a fixed array, a set; a map's value's
            const ValueOps* (*inner)() = nullptr;
            // a map's key's (string, an integer, text or an enum)
            const ValueOps* (*key)() = nullptr;
            // sequence, fixed, set, map: the count
            size_t (*count)(const void*) = nullptr;
            // sequence, fixed, set: every element in order, to fn (false stops)
            bool (*for_each)(const void*, void* ctx, ElementFn fn) = nullptr;
            // sequence, set: the container emptied and filled with elements
            // read one at a time while more() says so (read() fills a
            // default element, which is then added); false: a read failed
            bool (*read_elements)(void*, void* ctx, MoreFn more, ReadFn read) = nullptr;
            // fixed and tuple: the element i and its operations
            void* (*element)(void*, size_t i, const ValueOps** ops) = nullptr;
            const void* (*element_of)(const void*, size_t i, const ValueOps** ops) = nullptr;
            size_t fixed_count = 0;            // fixed and tuple: N
            // map: every entry with its key's text, sorted by it when asked
            // (a hash map's order is no order), to fn
            bool (*for_each_entry)(const void*, void* ctx, EntryFn fn, bool sorted) = nullptr;
            // map: emptied, and filled with an entry for every key next()
            // gives, the value read by read()
            EntryStatus (*read_entries)(void*, void* ctx, KeyFn next, ReadFn read) = nullptr;
            bool hashed = false;               // map, set: hashed (no order of its own), so written sorted
            // record: the fields of the object
            void (*describe)(void*, field_list&) = nullptr;
            // variant: the alternative held and its operations, an
            // alternative made (default) and given
            size_t alternatives = 0;
            size_t (*index)(const void*) = nullptr;
            const void* (*alternative)(const void*, const ValueOps** ops) = nullptr;
            void* (*emplace_alternative)(void*, size_t i, const ValueOps** ops) = nullptr;
            // json, custom_json: the value as a json, and set from one
            // (false: from_json refused it)
            json (*to_json)(const void*) = nullptr;
            bool (*from_json)(void*, const json&) = nullptr;
            // the default value of the type put in place (a JSON null read
            // into a field that is not optional)
            void (*clear)(void*) = nullptr;
        };

        template<class T>
        const ValueOps* value_ops() noexcept;

        // The options of a field (field's methods set them)
        enum FieldFlag : uint8_t {
            Required = 1,       // missing when read: missing_field
            OmitEmpty = 2,      // not written when empty
            AsString = 4,       // a number or a boolean written as a string ("12") and read so
            Attribute = 8,      // XML: an attribute of the element
            Text = 16           // XML: the text of the element
        };

        // One field as describe() gave it
        struct FieldInfo {
            std::string_view name;
            void* address = nullptr;
            const ValueOps* ops = nullptr;
            uint8_t flags = 0;
            // an enum's names (names()), or a variant's alternatives'
            // (tagged()): indexes into the list's names
            uint32_t names_from = 0;
            uint32_t names_count = 0;
            const char* tag = nullptr;         // a variant's key of the tag
        };

        // The fields of a field_list: sixteen in the list itself, more on
        // the heap. A type of fields is described once for every value
        // written or read — each record of a JSON array, each row of a CSV
        // file — and a vector allocated and freed its block every time.
        // The inline room is raw bytes: FieldInfo's defaults would be
        // written sixteen times over for every list otherwise.
        class FieldArray {
        public:
            static constexpr size_t Inline = 16;

            FieldArray() noexcept = default;
            FieldArray(const FieldArray&) = delete;
            FieldArray& operator=(const FieldArray&) = delete;

            const FieldInfo* begin() const noexcept {
                return _p;
            }

            const FieldInfo* end() const noexcept {
                return _p + _n;
            }

            size_t size() const noexcept {
                return _n;
            }

            bool empty() const noexcept {
                return !_n;
            }

            const FieldInfo& operator[](size_t i) const noexcept {
                return _p[i];
            }

            FieldInfo& back() noexcept {
                return _p[_n - 1];
            }

            void push_back(const FieldInfo& f) {
                if (_n == _capacity) {
                    _grow();
                }
                std::memcpy(static_cast<void*>(_p + _n), &f, sizeof(FieldInfo));
                ++_n;
            }

            void clear() noexcept {
                _n = 0;
            }

        private:
            void _grow() {
                auto more = std::make_unique_for_overwrite<FieldInfo[]>(_capacity * 2);
                std::memcpy(static_cast<void*>(more.get()), _p, _n * sizeof(FieldInfo));
                _heap = std::move(more);
                _p = _heap.get();
                _capacity *= 2;
            }

            alignas(FieldInfo) std::byte _inline[Inline * sizeof(FieldInfo)];
            std::unique_ptr<FieldInfo[]> _heap;
            FieldInfo* _p = reinterpret_cast<FieldInfo*>(_inline);
            size_t _n = 0;
            size_t _capacity = Inline;
        };

        static_assert(std::is_trivially_copyable_v<FieldInfo>);

        struct FieldAccess;
    }

    // The options of one field, chained after field_list::add
    class field {
    public:
        // Missing from the input: missing_field (by default a missing field
        // keeps its value)
        field& required() noexcept {
            return _set(detail::Required);
        }

        // Not written when it is empty: 0, false, "", an empty container,
        // nullopt, nullptr, a default value of a type of fields
        field& omit_empty() noexcept {
            return _set(detail::OmitEmpty);
        }

        // A number or a boolean written as a string and read from one:
        // "12" (Go's `json:",string"`; quoted, not as_string, which is the
        // name of a json value's string)
        field& quoted() noexcept {
            return _set(detail::AsString);
        }

        // XML: the field is an attribute of the element, not an element
        field& attribute() noexcept {
            return _set(detail::Attribute);
        }

        // XML: the field is the text of the element
        field& text() noexcept {
            return _set(detail::Text);
        }

        // An enum written as one of the names: the name of the value v is
        // names[v] (the values from 0); a value past the names is an
        // error when written, a name not among them when read
        field& names(std::initializer_list<const char*> names);

        // A variant written as an object with a tag: {"type": "circle",
        // ...the circle's fields}; the names of the alternatives in their
        // order, each alternative a type with describe()
        field& tagged(const char* key, std::initializer_list<const char*> names);

    private:
        friend class field_list;

        field& _set(uint8_t f) noexcept;

        field_list* _list = nullptr;
    };

    // What describe() fills: the fields of an object, in their order
    class field_list {
    public:
        field_list() noexcept {
            _current._list = this;
        }

        field_list(const field_list&) = delete;
        field_list& operator=(const field_list&) = delete;

        // A field of the object: its name in the formats (a literal: it is
        // read after describe returns) and the member itself
        template<class Name, class T>
        requires std::same_as<Name, const char*> || std::same_as<Name, char*>
        field& add(Name name, T& value) {
            return _add(std::string_view(name), value);
        }

        // A literal's length is its type's: no strlen for every value
        // described. An array whose last character before the terminator
        // is a zero too holds a shorter string, and is measured as a
        // pointer is.
        template<size_t N, class T>
        field& add(const char (&name)[N], T& value) {
            const bool literal = N >= 2 && name[N - 1] == 0 && name[N - 2] != 0;
            return _add(literal ? std::string_view(name, N - 1) : std::string_view(name), value);
        }

        size_t size() const noexcept {
            return _fields.size();
        }

    private:
        friend class field;
        friend struct detail::FieldAccess;

        template<class T>
        field& _add(std::string_view name, T& value) {
            detail::FieldInfo f;
            f.name = name;
            f.address = const_cast<void*>(static_cast<const void*>(std::addressof(value)));
            f.ops = detail::value_ops<std::remove_cv_t<T>>();
            _fields.push_back(f);
            return _current;
        }

        detail::FieldArray _fields;
        std::vector<const char*> _names;
        field _current;
    };

    inline field& field::_set(uint8_t f) noexcept {
        _list->_fields.back().flags |= f;
        return *this;
    }

    inline field& field::names(std::initializer_list<const char*> names) {
        auto& info = _list->_fields.back();
        info.names_from = uint32_t(_list->_names.size());
        info.names_count = uint32_t(names.size());
        _list->_names.insert(_list->_names.end(), names.begin(), names.end());
        return *this;
    }

    inline field& field::tagged(const char* key, std::initializer_list<const char*> names) {
        this->names(names);
        _list->_fields.back().tag = key;
        return *this;
    }

    namespace detail {
        // What a format reads of a field_list
        struct FieldAccess {
            static const FieldArray& fields(const field_list& l) noexcept {
                return l._fields;
            }

            // The list emptied, to be filled again by another object
            static void clear(field_list& l) noexcept {
                l._fields.clear();
                l._names.clear();
            }

            // The names given to the field (names() or tagged())
            static std::string_view name(const field_list& l, const FieldInfo& f, size_t i) noexcept {
                return i < f.names_count ? std::string_view(l._names[f.names_from + i]) : std::string_view();
            }

            // The index of a name among the field's, or the count when it is none of them
            static size_t index_of(const field_list& l, const FieldInfo& f, std::string_view n) noexcept {
                for (size_t i = 0; i < f.names_count; ++i) {
                    if (n == l._names[f.names_from + i]) {
                        return i;
                    }
                }
                return f.names_count;
            }
        };

        // --- what a type is ---

        template<class T>
        concept HasDescribe = requires(T& t, field_list& f) { t.describe(f); };

        template<class T>
        concept HasFreeDescribe = requires(T& t, field_list& f) { describe(f, t); };

        template<class T>
        concept HasText = requires(const T& t, const string& s) {
            { t.to_text() } -> std::convertible_to<string>;
            { T::from_text(s) } -> std::convertible_to<optional<T>>;
        };

        template<class T>
        concept HasJson = requires(const T& t, const json& j) {
            t.to_json();
            T::from_json(j);
        };

        template<class T, template<class...> class Of>
        inline constexpr bool IsSpecialization = false;

        template<template<class...> class Of, class... A>
        inline constexpr bool IsSpecialization<Of<A...>, Of> = true;

        template<class T>
        inline constexpr bool IsOptional = false;

        template<class T>
        inline constexpr bool IsOptional<std::optional<T>> = true;

        template<class T>
        inline constexpr bool IsPointer = false;

        template<class T>
        inline constexpr bool IsPointer<tracked_ptr<T>> = true;

        template<class T>
        inline constexpr bool IsVariant = false;

        template<class... A>
        inline constexpr bool IsVariant<sgcl::variant<A...>> = true;

        template<class... A>
        inline constexpr bool IsVariant<std::variant<A...>> = true;

        template<class T>
        inline constexpr bool IsFixed = false;

        template<class T, size_t N>
        inline constexpr bool IsFixed<std::array<T, N>> = true;

        template<class T, size_t N, class S>
        inline constexpr bool IsFixed<sgcl::array<T, N, S>> = true;

        template<class T, size_t N>
        inline constexpr bool IsFixed<T[N]> = true;

        template<class T>
        struct FixedOf;

        template<class T, size_t N>
        struct FixedOf<std::array<T, N>> {
            using element = T;
            static constexpr size_t count = N;
        };

        template<class T, size_t N, class S>
        struct FixedOf<sgcl::array<T, N, S>> {
            using element = T;
            static constexpr size_t count = N;
        };

        template<class T, size_t N>
        struct FixedOf<T[N]> {
            using element = T;
            static constexpr size_t count = N;
        };

        template<class T>
        inline constexpr bool IsTuple = false;

        template<class A, class B>
        inline constexpr bool IsTuple<std::pair<A, B>> = true;

        template<class... A>
        inline constexpr bool IsTuple<std::tuple<A...>> = true;

        // A map: key_type and mapped_type; a set: key_type alone, and a range
        template<class T>
        concept IsMap = requires { typename T::key_type; typename T::mapped_type; } && std::ranges::range<T>;

        template<class T>
        concept IsSet = requires { typename T::key_type; } && !IsMap<T> && std::ranges::range<T>;

        template<class T>
        concept IsSequence = std::ranges::range<T> && requires { typename T::value_type; } && !IsMap<T> && !IsSet<T>
            && !std::is_same_v<T, string> && !std::is_same_v<T, std::string> && !IsFixed<T>;

        template<class T>
        inline constexpr bool IsHashed = requires { typename T::hasher; };

        template<class K, class V, class H, class E>
        inline constexpr bool IsHashed<sgcl::map<K, V, H, E>> = true;

        template<class K, class H, class E>
        inline constexpr bool IsHashed<sgcl::set<K, H, E>> = true;

        // in the order of their insertion, which is an order
        template<class K, class V, class H, class E>
        inline constexpr bool IsHashed<sgcl::ordered_map<K, V, H, E>> = false;

        template<class K, class H, class E>
        inline constexpr bool IsHashed<sgcl::ordered_set<K, H, E>> = false;

        template<class T>
        inline constexpr bool IsCharacter = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t>
            || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

        // A container of std is taken only for values that hold no tracked
        // pointer (its memory is not the collector's): numbers, enums,
        // std::string, and std's own containers of them
        template<class T>
        struct IsPlain : std::bool_constant<std::is_arithmetic_v<T> || std::is_enum_v<T>> {};

        template<>
        struct IsPlain<std::string> : std::true_type {};

        template<class T>
        struct IsPlain<std::optional<T>> : IsPlain<T> {};

        template<class A, class B>
        struct IsPlain<std::pair<A, B>> : std::bool_constant<IsPlain<A>::value && IsPlain<B>::value> {};

        template<class... A>
        struct IsPlain<std::tuple<A...>> : std::bool_constant<(IsPlain<A>::value && ...)> {};

        template<class T, size_t N>
        struct IsPlain<std::array<T, N>> : IsPlain<T> {};

        template<class T, class A>
        struct IsPlain<std::vector<T, A>> : IsPlain<T> {};

        template<class T>
        inline constexpr bool IsStdContainer = IsSpecialization<T, std::vector> || IsSpecialization<T, std::deque> || IsSpecialization<T, std::list>
            || IsSpecialization<T, std::map> || IsSpecialization<T, std::unordered_map> || IsSpecialization<T, std::set>
            || IsSpecialization<T, std::unordered_set> || IsSpecialization<T, std::multimap> || IsSpecialization<T, std::multiset>;

        template<class T>
        struct ElementsPlain : std::false_type {};

        template<class T>
        requires IsMap<T>
        struct ElementsPlain<T> : std::bool_constant<IsPlain<typename T::key_type>::value && IsPlain<typename T::mapped_type>::value> {};

        template<class T>
        requires (!IsMap<T>) && requires { typename T::value_type; }
        struct ElementsPlain<T> : IsPlain<typename T::value_type> {};

        template<class T>
        void describe_of(T& value, field_list& f) {
            if constexpr (HasDescribe<T>) {
                value.describe(f);
            } else {
                describe(f, value);
            }
        }

        // The text of a map's key and the key from its text
        template<class K>
        concept TextKey = std::is_same_v<K, string> || std::is_same_v<K, std::string> || (std::is_integral_v<K> && !std::is_same_v<K, bool> && !IsCharacter<K>) || HasText<K>;

        template<class K>
        std::string key_text(const K& k) {
            if constexpr (std::is_same_v<K, string>) {
                return std::string(k.view());
            } else if constexpr (std::is_same_v<K, std::string>) {
                return k;
            } else if constexpr (std::is_integral_v<K>) {
                char buf[24];
                auto r = std::to_chars(buf, buf + sizeof buf, k);
                return std::string(buf, r.ptr);
            } else {
                auto t = k.to_text();
                return std::string(t.view());
            }
        }

        template<class K>
        optional<K> key_of(std::string_view t) {
            if constexpr (std::is_same_v<K, string>) {
                return string(t);
            } else if constexpr (std::is_same_v<K, std::string>) {
                return std::string(t);
            } else if constexpr (std::is_integral_v<K>) {
                K v{};
                auto r = std::from_chars(t.data(), t.data() + t.size(), v);
                if (r.ec != std::errc() || r.ptr != t.data() + t.size() || t.empty() || t[0] == '+') {
                    return nullopt;
                }
                return v;
            } else {
                auto k = K::from_text(string(t));
                if (!k) {
                    return nullopt;
                }
                return std::move(*k);
            }
        }

        template<class T>
        constexpr const char* type_name() noexcept {
            if constexpr (std::is_same_v<T, bool>) {
                return "a boolean";
            } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
                return "an integer";
            } else if constexpr (std::is_floating_point_v<T>) {
                return "a number";
            } else if constexpr (std::is_same_v<T, string> || std::is_same_v<T, std::string> || HasText<T>) {
                return "a string";
            } else {
                return "a value";
            }
        }

        // --- the tables ---

        template<class T>
        struct Ops {
            static bool is_empty(const void* p) {
                const T& v = *static_cast<const T*>(p);
                if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
                    return v == T{};
                } else if constexpr (std::is_same_v<T, string> || std::is_same_v<T, std::string>) {
                    return v.empty();
                } else if constexpr (IsOptional<T> || IsPointer<T>) {
                    return !v;
                } else if constexpr (IsFixed<T>) {
                    return FixedOf<T>::count == 0;
                } else if constexpr (requires { v.empty(); }) {
                    return v.empty();
                } else if constexpr (requires { v == T{}; }) {
                    return bool(v == T{});
                } else {
                    return false;
                }
            }

            static void clear(void* p) {
                if constexpr (std::is_array_v<T>) {
                    for (auto& e : *static_cast<T*>(p)) {
                        Ops<std::remove_extent_t<T>>::clear(&e);
                    }
                } else {
                    *static_cast<T*>(p) = T{};
                }
            }
        };

        // Whether a literal's value is an integer, of any size: its
        // significant digits end at or before the point
        inline bool literal_is_integral(std::string_view lit) noexcept {
            Decimal d = decimal_of(lit);
            std::string_view b = d.fraction;
            while (!b.empty() && b.back() == '0') {
                b.remove_suffix(1);
            }
            int64_t shift = d.exponent - int64_t(b.size());
            if (shift >= 0) {
                return true;
            }
            // the digits a b must end with -shift zeros
            std::string_view a = d.integer;
            size_t zeros = 0;
            if (b.empty()) {
                for (size_t i = a.size(); i > 0 && a[i - 1] == '0'; --i) {
                    ++zeros;
                }
                bool all_zero = zeros == a.size();
                return all_zero || int64_t(zeros) >= -shift;
            }
            return false;
        }

        template<class T>
        struct ScalarOps {
            static bool get_bool(const void* p) {
                return *static_cast<const T*>(p);
            }

            static void set_bool(void* p, bool b) {
                *static_cast<T*>(p) = b;
            }

            static int64_t get_int(const void* p) {
                if constexpr (std::is_enum_v<T>) {
                    return int64_t(std::underlying_type_t<T>(*static_cast<const T*>(p)));
                } else {
                    return int64_t(*static_cast<const T*>(p));
                }
            }

            static uint64_t get_uint(const void* p) {
                if constexpr (std::is_enum_v<T>) {
                    return uint64_t(std::underlying_type_t<T>(*static_cast<const T*>(p)));
                } else {
                    return uint64_t(*static_cast<const T*>(p));
                }
            }

            using Integer = typename std::conditional_t<std::is_enum_v<T>, std::underlying_type<T>, std::type_identity<T>>::type;

            static bool set_int(void* p, int64_t v) {
                if constexpr (std::is_signed_v<Integer>) {
                    if (v < int64_t(std::numeric_limits<Integer>::min()) || v > int64_t(std::numeric_limits<Integer>::max())) {
                        return false;
                    }
                } else {
                    if (v < 0 || uint64_t(v) > uint64_t(std::numeric_limits<Integer>::max())) {
                        return false;
                    }
                }
                *static_cast<T*>(p) = T(Integer(v));
                return true;
            }

            static bool set_uint(void* p, uint64_t v) {
                if (v > uint64_t(std::numeric_limits<Integer>::max())) {
                    return false;
                }
                *static_cast<T*>(p) = T(Integer(v));
                return true;
            }

            static double get_double(const void* p) {
                return double(*static_cast<const T*>(p));
            }

            static void set_double(void* p, double d) {
                *static_cast<T*>(p) = T(d);
            }

            // A number from its literal in JSON's grammar (the whole text,
            // checked): an integer exactly, a float or a double rounded once
            static int set_literal(void* p, std::string_view lit) {
                const char* q = lit.data();
                NumberState st = NumberState::start;
                bool plain = true;
                if (lit.empty() || scan_number(q, lit.data() + lit.size(), true, st, plain) != ScanStatus::done || q != lit.data() + lit.size()) {
                    return 1;
                }
                if constexpr (std::is_floating_point_v<T>) {
                    auto v = floating_of<T>(lit);
                    if (!v) {
                        return 2;
                    }
                    *static_cast<T*>(p) = *v;
                    return 0;
                } else {
                    if (plain) {
                        // an integer's literal: its digits, at once
                        auto r = integer_of(lit);
                        if (r.fit == IntegerFit::int64) {
                            return set_int(p, r.i) ? 0 : 2;
                        }
                        if (r.fit == IntegerFit::uint64) {
                            return set_uint(p, r.u) ? 0 : 2;
                        }
                        return 2;
                    }
                    auto v = exact_integer<Integer>(lit);
                    if (!v) {
                        // an integer too large for the type, or no integer at all
                        return literal_is_integral(lit) ? 2 : 1;
                    }
                    *static_cast<T*>(p) = T(*v);
                    return 0;
                }
            }

            static size_t number_text(const void* p, char* out) {
                if constexpr (std::is_floating_point_v<T>) {
                    return detail::number_text(out, *static_cast<const T*>(p));
                } else {
                    return integer_text(out, Integer(*static_cast<const T*>(p)));
                }
            }
        };

        template<class T>
        struct TextOps {
            static string get_text(const void* p) {
                const T& v = *static_cast<const T*>(p);
                if constexpr (std::is_same_v<T, string>) {
                    return v;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return string(v);
                } else {
                    return v.to_text();
                }
            }

            static bool set_text(void* p, std::string_view t) {
                T& v = *static_cast<T*>(p);
                if constexpr (std::is_same_v<T, string>) {
                    v = string(t);
                    return true;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    v.assign(t);
                    return true;
                } else {
                    auto r = T::from_text(string(t));
                    if (!r) {
                        return false;
                    }
                    v = std::move(*r);
                    return true;
                }
            }
        };

        template<class T>
        struct OptionalOps {
            using U = typename T::value_type;

            static bool has_value(const void* p) {
                return static_cast<const T*>(p)->has_value();
            }

            static const void* value(const void* p) {
                return std::addressof(**static_cast<const T*>(p));
            }

            static void* emplace(void* p) {
                return std::addressof(static_cast<T*>(p)->emplace());
            }

            static void reset(void* p) {
                static_cast<T*>(p)->reset();
            }

            static const ValueOps* inner() {
                return value_ops<U>();
            }
        };

        template<class T>
        struct PointerOps {
            using U = typename T::element_type;

            static bool has_value(const void* p) {
                return bool(*static_cast<const T*>(p));
            }

            static const void* value(const void* p) {
                return static_cast<const T*>(p)->get();
            }

            // a new object: a pointer read is never shared with another
            static void* emplace(void* p) {
                auto& t = *static_cast<T*>(p);
                tracked_ptr<U> made = make_tracked<U>();
                t = made;
                return made.get();
            }

            static void reset(void* p) {
                *static_cast<T*>(p) = nullptr;
            }

            static const ValueOps* inner() {
                return value_ops<std::remove_cv_t<U>>();
            }
        };

        // Adding an element to a sequence or a set: in place, or through the
        // new version an immutable container returns
        template<class C, class E>
        concept VoidPush = requires(C& c, E&& e) { { c.push_back(std::move(e)) } -> std::same_as<void>; };

        template<class C, class E>
        concept ValuePush = requires(const C& c, E&& e) { { c.push_back(std::move(e)) } -> std::same_as<C>; };

        template<class C, class E>
        concept ValueInsert = requires(const C& c, E&& e) { { c.insert(std::move(e)) } -> std::same_as<C>; };

        template<class C, class E>
        concept PlainInsert = !ValueInsert<C, E> && requires(C& c, E&& e) { c.insert(std::move(e)); };

        template<class C, class E>
        concept Addable = VoidPush<C, E> || ValuePush<C, E> || ValueInsert<C, E> || PlainInsert<C, E>;

        template<class C, class E>
        void add_element(C& c, E&& e) {
            if constexpr (VoidPush<C, E>) {
                c.push_back(std::move(e));
            } else if constexpr (ValuePush<C, E>) {
                c = c.push_back(std::move(e));
            } else if constexpr (ValueInsert<C, E>) {
                c = c.insert(std::move(e));
            } else {
                c.insert(std::move(e));
            }
        }

        template<class C>
        concept VoidClear = requires(C& c) { { c.clear() } -> std::same_as<void>; };

        template<class T>
        struct RangeOps {
            using E = std::ranges::range_value_t<T>;

            static size_t count(const void* p) {
                const T& c = *static_cast<const T*>(p);
                if constexpr (requires { c.size(); }) {
                    return size_t(c.size());
                } else {
                    return size_t(std::ranges::distance(c));
                }
            }

            static bool for_each(const void* p, void* ctx, ElementFn fn) {
                for (auto& e : *static_cast<const T*>(p)) {
                    if (!fn(ctx, std::addressof(e))) {
                        return false;
                    }
                }
                return true;
            }

            // A sequence built front to back: an immutable list's
            // push_front builds it back to front, and a dynamic_array has a
            // size once — both from a vector gathered first
            static bool read_elements(void* p, void* ctx, MoreFn more, ReadFn read) {
                T& c = *static_cast<T*>(p);
                if constexpr (Addable<T, E>) {
                    T fresh{};
                    T& into = [&]() -> T& {
                        if constexpr (VoidClear<T>) {
                            c.clear();
                            return c;
                        } else {
                            return fresh;
                        }
                    }();
                    while (more(ctx)) {
                        E e{};
                        if (!read(ctx, std::addressof(e))) {
                            return false;
                        }
                        add_element(into, std::move(e));
                    }
                    if constexpr (!VoidClear<T>) {
                        c = std::move(fresh);
                    }
                    return true;
                } else {
                    vector<E> gathered;
                    while (more(ctx)) {
                        E e{};
                        if (!read(ctx, std::addressof(e))) {
                            return false;
                        }
                        gathered.push_back(std::move(e));
                    }
                    if constexpr (requires(const T& t, E&& e) { { t.push_front(std::move(e)) } -> std::same_as<T>; }) {
                        T built{};
                        for (auto it = gathered.rbegin(); it != gathered.rend(); ++it) {
                            built = built.push_front(std::move(*it));
                        }
                        c = std::move(built);
                    } else {
                        c = T(std::make_move_iterator(gathered.begin()), std::make_move_iterator(gathered.end()));
                    }
                    return true;
                }
            }

            static const ValueOps* inner() {
                return value_ops<std::remove_cv_t<E>>();
            }
        };

        template<class T>
        struct FixedOps {
            using E = typename FixedOf<T>::element;
            static constexpr size_t N = FixedOf<T>::count;

            static size_t count(const void*) {
                return N;
            }

            static E* data(void* p) {
                if constexpr (std::is_array_v<T>) {
                    return *static_cast<T*>(p);
                } else {
                    return static_cast<T*>(p)->data();
                }
            }

            static const E* data(const void* p) {
                if constexpr (std::is_array_v<T>) {
                    return *static_cast<const T*>(p);
                } else {
                    return static_cast<const T*>(p)->data();
                }
            }

            static bool for_each(const void* p, void* ctx, ElementFn fn) {
                const E* d = data(p);
                for (size_t i = 0; i < N; ++i) {
                    if (!fn(ctx, d + i)) {
                        return false;
                    }
                }
                return true;
            }

            static void* element(void* p, size_t i, const ValueOps** ops) {
                *ops = value_ops<E>();
                return data(p) + i;
            }

            static const void* element_of(const void* p, size_t i, const ValueOps** ops) {
                *ops = value_ops<E>();
                return data(p) + i;
            }

            static const ValueOps* inner() {
                return value_ops<E>();
            }
        };

        template<class T>
        struct TupleOps {
            static constexpr size_t N = std::tuple_size_v<T>;

            template<size_t I = 0>
            static void* at(T& t, size_t i, const ValueOps** ops) {
                if constexpr (I == N) {
                    return nullptr;
                } else {
                    if (i == I) {
                        *ops = value_ops<std::remove_cv_t<std::tuple_element_t<I, T>>>();
                        return std::addressof(std::get<I>(t));
                    }
                    return at<I + 1>(t, i, ops);
                }
            }

            static void* element(void* p, size_t i, const ValueOps** ops) {
                return at(*static_cast<T*>(p), i, ops);
            }

            static const void* element_of(const void* p, size_t i, const ValueOps** ops) {
                return at(*const_cast<T*>(static_cast<const T*>(p)), i, ops);
            }
        };

        template<class T>
        struct MapOps {
            using K = typename T::key_type;
            using V = typename T::mapped_type;

            static size_t count(const void* p) {
                return size_t(static_cast<const T*>(p)->size());
            }

            static bool for_each_entry(const void* p, void* ctx, EntryFn fn, bool sorted) {
                const T& m = *static_cast<const T*>(p);
                if (sorted) {
                    // the keys' texts and the values, in order of the texts
                    std::vector<std::pair<std::string, const V*>> entries;
                    entries.reserve(size_t(m.size()));
                    for (auto& [k, v] : m) {
                        entries.emplace_back(key_text(k), std::addressof(v));
                    }
                    std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) { return a.first < b.first; });
                    for (auto& [k, v] : entries) {
                        if (!fn(ctx, k, v)) {
                            return false;
                        }
                    }
                    return true;
                }
                for (auto& [k, v] : m) {
                    auto t = key_text(k);
                    if (!fn(ctx, t, std::addressof(v))) {
                        return false;
                    }
                }
                return true;
            }

            static EntryStatus read_entries(void* p, void* ctx, KeyFn next, ReadFn read) {
                T& m = *static_cast<T*>(p);
                T fresh{};
                std::string_view t;
                while (next(ctx, t)) {
                    auto k = key_of<K>(t);
                    if (!k) {
                        return EntryStatus::bad_key;
                    }
                    V v{};
                    if (!read(ctx, std::addressof(v))) {
                        return EntryStatus::failed;
                    }
                    if constexpr (requires { fresh.insert_or_assign(std::move(*k), std::move(v)); }) {
                        fresh.insert_or_assign(std::move(*k), std::move(v));
                    } else if constexpr (requires { { fresh.insert(std::move(*k), std::move(v)) } -> std::convertible_to<T>; }) {
                        fresh = fresh.insert(std::move(*k), std::move(v));
                    } else if constexpr (requires { { fresh.set(std::move(*k), std::move(v)) } -> std::convertible_to<T>; }) {
                        fresh = fresh.set(std::move(*k), std::move(v));
                    } else {
                        fresh[std::move(*k)] = std::move(v);
                    }
                }
                m = std::move(fresh);
                return EntryStatus::done;
            }

            static const ValueOps* inner() {
                return value_ops<std::remove_cv_t<V>>();
            }

            static const ValueOps* key() {
                return value_ops<std::remove_cv_t<K>>();
            }
        };

        template<class T>
        struct RecordOps {
            static void describe(void* p, field_list& f) {
                describe_of(*static_cast<T*>(p), f);
            }
        };

        template<class T>
        struct VariantOps;

        template<template<class...> class Var, class... A>
        struct VariantOps<Var<A...>> {
            using T = Var<A...>;
            static constexpr size_t N = sizeof...(A);

            static size_t index(const void* p) {
                return static_cast<const T*>(p)->index();
            }

            template<size_t I = 0>
            static const void* held(const T& v, const ValueOps** ops) {
                if constexpr (I == N) {
                    return nullptr;
                } else {
                    if (v.index() == I) {
                        using U = std::tuple_element_t<I, std::tuple<A...>>;
                        *ops = value_ops<U>();
                        if constexpr (IsSpecialization<T, std::variant>) {
                            return std::addressof(std::get<I>(v));
                        } else {
                            return std::addressof(sgcl::get<I>(v));
                        }
                    }
                    return held<I + 1>(v, ops);
                }
            }

            static const void* alternative(const void* p, const ValueOps** ops) {
                return held(*static_cast<const T*>(p), ops);
            }

            template<size_t I = 0>
            static void* make(T& v, size_t i, const ValueOps** ops) {
                if constexpr (I == N) {
                    return nullptr;
                } else {
                    if (i == I) {
                        using U = std::tuple_element_t<I, std::tuple<A...>>;
                        *ops = value_ops<U>();
                        return std::addressof(v.template emplace<I>());
                    }
                    return make<I + 1>(v, i, ops);
                }
            }

            static void* emplace_alternative(void* p, size_t i, const ValueOps** ops) {
                return make(*static_cast<T*>(p), i, ops);
            }
        };

        template<class T>
        struct JsonHooks {
            static json to_json(const void* p);           // json.h
            static bool from_json(void* p, const json& j);
        };

        template<class T>
        constexpr ValueOps make_value_ops() {
            ValueOps o{};
            o.is_empty = &Ops<T>::is_empty;
            if constexpr (!std::is_array_v<T>) {
                o.clear = &Ops<T>::clear;
            } else {
                o.clear = &Ops<T>::clear;
            }
            if constexpr (std::is_same_v<T, json>) {
                o.kind = ValueKind::json;
                o.name = "json";
                o.to_json = &JsonHooks<T>::to_json;
                o.from_json = &JsonHooks<T>::from_json;
            } else if constexpr (HasJson<T>) {
                o.kind = ValueKind::custom_json;
                o.name = "a value";
                o.to_json = &JsonHooks<T>::to_json;
                o.from_json = &JsonHooks<T>::from_json;
            } else if constexpr (HasText<T>) {
                o.kind = ValueKind::text;
                o.name = "a string";
                o.get_text = &TextOps<T>::get_text;
                o.set_text = &TextOps<T>::set_text;
            } else if constexpr (HasDescribe<T> || HasFreeDescribe<T>) {
                static_assert(std::is_default_constructible_v<T>, "encoding: describe() needs a default constructor to read the type; or give to_json/from_json");
                o.kind = ValueKind::record;
                o.name = "an object";
                o.describe = &RecordOps<T>::describe;
            } else if constexpr (std::is_same_v<T, bool>) {
                o.kind = ValueKind::boolean;
                o.name = "a boolean";
                o.size = 1;
                o.get_bool = &ScalarOps<T>::get_bool;
                o.set_bool = &ScalarOps<T>::set_bool;
            } else if constexpr (IsCharacter<T>) {
                static_assert(sizeof(T) == 0, "encoding: a character field; use a string, or an integer type for a number");
            } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
                using I = typename ScalarOps<T>::Integer;
                o.kind = std::is_enum_v<T> ? ValueKind::enumeration : std::is_signed_v<I> ? ValueKind::signed_integer : ValueKind::unsigned_integer;
                o.name = "an integer";
                o.size = uint8_t(sizeof(T));
                o.get_int = &ScalarOps<T>::get_int;
                o.set_int = &ScalarOps<T>::set_int;
                o.get_uint = &ScalarOps<T>::get_uint;
                o.set_uint = &ScalarOps<T>::set_uint;
                o.set_literal = &ScalarOps<T>::set_literal;
                o.number_text = &ScalarOps<T>::number_text;
            } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                o.kind = ValueKind::floating;
                o.name = "a number";
                o.size = uint8_t(sizeof(T));
                o.get_double = &ScalarOps<T>::get_double;
                o.set_double = &ScalarOps<T>::set_double;
                o.set_literal = &ScalarOps<T>::set_literal;
                o.number_text = &ScalarOps<T>::number_text;
            } else if constexpr (std::is_same_v<T, string> || std::is_same_v<T, std::string>) {
                o.kind = ValueKind::string;
                o.name = "a string";
                o.get_text = &TextOps<T>::get_text;
                o.set_text = &TextOps<T>::set_text;
            } else if constexpr (IsOptional<T>) {
                o.kind = ValueKind::optional;
                o.name = "a value";
                o.has_value = &OptionalOps<T>::has_value;
                o.value = &OptionalOps<T>::value;
                o.emplace = &OptionalOps<T>::emplace;
                o.reset = &OptionalOps<T>::reset;
                o.inner = &OptionalOps<T>::inner;
            } else if constexpr (IsPointer<T>) {
                static_assert(std::is_default_constructible_v<typename T::element_type>, "encoding: a tracked_ptr field is read into a new object: its type needs a default constructor");
                o.kind = ValueKind::pointer;
                o.name = "a value";
                o.has_value = &PointerOps<T>::has_value;
                o.value = &PointerOps<T>::value;
                o.emplace = &PointerOps<T>::emplace;
                o.reset = &PointerOps<T>::reset;
                o.inner = &PointerOps<T>::inner;
            } else if constexpr (IsVariant<T>) {
                o.kind = ValueKind::variant;
                o.name = "an object";
                o.alternatives = VariantOps<T>::N;
                o.index = &VariantOps<T>::index;
                o.alternative = &VariantOps<T>::alternative;
                o.emplace_alternative = &VariantOps<T>::emplace_alternative;
            } else if constexpr (IsFixed<T>) {
                o.kind = ValueKind::fixed;
                o.name = "an array";
                o.fixed_count = FixedOps<T>::N;
                o.count = &FixedOps<T>::count;
                o.for_each = &FixedOps<T>::for_each;
                o.element = &FixedOps<T>::element;
                o.element_of = &FixedOps<T>::element_of;
                o.inner = &FixedOps<T>::inner;
            } else if constexpr (IsTuple<T>) {
                o.kind = ValueKind::tuple;
                o.name = "an array";
                o.fixed_count = TupleOps<T>::N;
                o.element = &TupleOps<T>::element;
                o.element_of = &TupleOps<T>::element_of;
            } else if constexpr (IsMap<T>) {
                static_assert(TextKey<typename T::key_type>, "encoding: a map's key is a string, an integer or a type with to_text/from_text");
                static_assert(!IsStdContainer<T> || ElementsPlain<T>::value, "encoding: a container of std holds no managed values (a string, a tracked_ptr...): use the library's");
                o.kind = ValueKind::map;
                o.name = "an object";
                o.hashed = IsHashed<T>;
                o.count = &MapOps<T>::count;
                o.for_each_entry = &MapOps<T>::for_each_entry;
                o.read_entries = &MapOps<T>::read_entries;
                o.inner = &MapOps<T>::inner;
                o.key = &MapOps<T>::key;
            } else if constexpr (IsSet<T> || IsSequence<T>) {
                static_assert(!IsStdContainer<T> || ElementsPlain<T>::value, "encoding: a container of std holds no managed values (a string, a tracked_ptr...): use the library's");
                o.kind = IsSet<T> ? ValueKind::set : ValueKind::sequence;
                o.name = "an array";
                o.hashed = IsHashed<T>;
                o.count = &RangeOps<T>::count;
                o.for_each = &RangeOps<T>::for_each;
                o.read_elements = &RangeOps<T>::read_elements;
                o.inner = &RangeOps<T>::inner;
            } else {
                static_assert(sizeof(T) == 0, "encoding: a field of a type with no describe(), no to_text/from_text, no to_json/from_json, and no kind the formats know");
            }
            return o;
        }

        template<class T>
        struct OpsTable {
            static constexpr ValueOps value = make_value_ops<T>();
        };

        template<class T>
        const ValueOps* value_ops() noexcept {
            return &OpsTable<T>::value;
        }
    }
}
