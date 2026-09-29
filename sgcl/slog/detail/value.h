//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../encoding/fields.h"
#include "../../io/error.h"
#include "../../time/datetime.h"
#include "../../txt/format.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sgcl::slog {
    template<class... A>
    class group;
}

// The attributes of a record as the module sees them: a key and a value of
// one of a few kinds, made on the stack of the call from its arguments and
// read by the handlers. Nothing here is managed or allocated for a value
// of the kinds that are data (numbers, texts, a duration, a time, a group,
// a type described by its fields); a type that makes its text as a string
// (to_text(), to_string(), an error's message) costs what that costs.
namespace sgcl::slog::detail {
    using ValueOps = encoding::detail::ValueOps;

    // What a value is. Record and Any are a value of the program read
    // through its operations (encoding's describe): a described type, and
    // any other kind a field may have (a container, a variant, a json);
    // Formatted writes itself into the line (write_text, format_value);
    // Owned is Any made text once, kept in a record's copy (clone());
    // Splice stands where the attributes of the call go in a logger's
    // attributes kept as a tree.
    enum class Kind : uint8_t {
        Null,
        Bool,
        Int,
        Uint,
        Float,
        String,
        Duration,
        Time,
        Group,
        Record,
        Any,
        Formatted,
        Owned,
        Splice
    };

    // The text of a value that writes itself, into out: its whole size,
    // written only when it fits in room (then called again with the room)
    using FormatFn = size_t (*)(const void* object, char* out, size_t room);

    struct Attr;

    struct Value {
        Kind kind = Kind::Null;
        uint32_t count = 0;        // Group: its attributes; Owned: the size of its text form
        int32_t offset = 0;        // Time: seconds east of UTC
        union {
            bool b;
            int64_t i;
            uint64_t u;
            double d;
            int64_t ns;            // Duration, Time
            struct {
                const char* p;
                size_t n;
            } s;                   // String; Owned: its JSON form, the text form after it
            const Attr* g;         // Group
            struct {
                const void* p;
                const ValueOps* ops;
            } obj;                 // Record, Any
            struct {
                const void* p;
                FormatFn fn;
            } fmt;                 // Formatted
        };

        Value() noexcept
        : s{nullptr, 0} {
        }
    };

    struct Attr {
        const char* key = "";
        size_t key_n = 0;
        Value value;
    };

    // An attribute that writes nothing: no key and no value (Go's
    // Any("", nil)), which both handlers skip
    inline bool is_empty_attr(const Attr& a) noexcept {
        return a.key_n == 0 && a.value.kind == Kind::Null;
    }

    //--------------------------------------------------------------------
    // The kinds of the arguments
    //--------------------------------------------------------------------
    template<class T>
    inline constexpr bool IsCharacter = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t>
        || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

    template<class T>
    inline constexpr bool IsGroup = false;

    template<class... A>
    inline constexpr bool IsGroup<slog::group<A...>> = true;

    template<class T>
    inline constexpr bool IsChronoDuration = false;

    template<class R, class P>
    inline constexpr bool IsChronoDuration<std::chrono::duration<R, P>> = true;

    // A key: a literal or a C string
    template<class T>
    inline constexpr bool IsKey = std::is_same_v<std::remove_cv_t<T>, const char*> || std::is_same_v<std::remove_cv_t<T>, char*>
        || (std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>, char>);

    template<class T>
    concept WritesText = requires(const T& v, char* out) {
        { T::MaxText } -> std::convertible_to<size_t>;
        { v.write_text(out) } -> std::convertible_to<size_t>;
    };

    template<class T>
    concept FormatsValue = txt::detail::has_format_value<T>;

    template<class T>
    concept HasToText = requires(const T& v) {
        { v.to_text() } -> std::convertible_to<string>;
    };

    template<class T>
    concept HasToString = requires(const T& v) {
        requires std::convertible_to<decltype(v.to_string()), string> || std::same_as<std::remove_cvref_t<decltype(v.to_string())>, std::string>;
    };

    template<class T>
    concept Described = encoding::detail::HasDescribe<T> || encoding::detail::HasFreeDescribe<T>;

    enum class Category {
        Unsupported,
        Character,
        Null,
        Bool,
        Signed,
        Unsigned,
        Float,
        CString,
        CharArray,
        Text,          // string, std::string, std::string_view, slice<const char>
        Duration,
        Time,
        IoError,
        ErrorCode,
        Exception,
        Optional,
        Group,
        WritesText,
        FormatsValue,
        ToText,
        ToString,
        Described
    };

    // The kind of an argument, first match wins: the exact types, then
    // what a type says of itself (a text of its own into a buffer, a
    // format_value, to_text, to_string), then fields. What matches none is
    // an error of the build, not a %v
    template<class T>
    constexpr Category category_of() {
        using U = std::remove_cv_t<T>;
        if constexpr (IsCharacter<U>) {
            return Category::Character;
        } else if constexpr (std::is_same_v<U, bool>) {
            return Category::Bool;
        } else if constexpr (std::is_same_v<U, std::nullptr_t>) {
            return Category::Null;
        } else if constexpr (std::is_integral_v<U>) {
            return std::is_signed_v<U> ? Category::Signed : Category::Unsigned;
        } else if constexpr (std::is_floating_point_v<U>) {
            return Category::Float;
        } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
            return Category::CString;
        } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>) {
            return Category::CharArray;
        } else if constexpr (std::is_same_v<U, string> || std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                             || std::is_same_v<U, slice<const char>> || std::is_same_v<U, slice<char>>) {
            return Category::Text;
        } else if constexpr (std::is_same_v<U, sgcl::duration> || IsChronoDuration<U>) {
            return Category::Duration;
        } else if constexpr (std::is_same_v<U, time::datetime>) {
            return Category::Time;
        } else if constexpr (std::is_same_v<U, io::error>) {
            return Category::IoError;
        } else if constexpr (std::is_same_v<U, std::error_code>) {
            return Category::ErrorCode;
        } else if constexpr (std::is_same_v<U, std::exception_ptr>) {
            return Category::Exception;
        } else if constexpr (encoding::detail::IsOptional<U>) {
            return Category::Optional;
        } else if constexpr (IsGroup<U>) {
            return Category::Group;
        } else if constexpr (WritesText<U>) {
            return Category::WritesText;
        } else if constexpr (FormatsValue<U>) {
            return Category::FormatsValue;
        } else if constexpr (HasToText<U>) {
            return Category::ToText;
        } else if constexpr (HasToString<U>) {
            return Category::ToString;
        } else if constexpr (Described<U>) {
            return Category::Described;
        } else {
            return Category::Unsupported;
        }
    }

    // The length of a literal or a character array: to its first NUL, never
    // past its end
    template<size_t N>
    constexpr size_t array_length(const char (&a)[N]) noexcept {
        size_t n = 0;
        while (n < N && a[n]) {
            ++n;
        }
        return n;
    }

    // The text an exception says of itself, as on_unhandled writes it:
    // its type and what()
    inline std::string exception_text(const std::exception_ptr& e) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            char type[256];
            async::detail::type_name(typeid(x), type, sizeof(type));
            std::string t(type);
            t += ": ";
            t += x.what();
            return t;
        } catch (...) {
            return "unknown exception";
        }
    }

    // io::error's message() without the managed string it makes
    inline std::string io_error_text(const io::error& e) {
        std::string m(e.op().data(), e.op().size());
        if (!e.path().empty()) {
            m += ' ';
            m.append(e.path().data(), e.path().size());
        }
        if (!m.empty()) {
            m += ": ";
        }
        m += e.code().message();
        return m;
    }

    template<class T>
    size_t write_text_of(const void* p, char* out, size_t room) {
        if (room < T::MaxText) {
            return T::MaxText;
        }
        return static_cast<const T*>(p)->write_text(out);
    }

    template<class T>
    size_t format_value_of(const void* p, char* out, size_t room) {
        txt::format_sink sink(out, room);
        format_value(sink, *static_cast<const T*>(p), txt::format_spec{});
        return sink.size();
    }

    template<class T, Category C = category_of<T>()>
    struct Holder;

    template<class T, Category C>
    struct Holder {
        static_assert(C != Category::Character, "slog: a character as a value; give a string, or an integer for its code");
        static_assert(C != Category::Unsupported,
                      "slog: a value of a kind a record does not take: a number, bool, a string, a duration, a datetime, "
                      "an error (io::error, std::error_code, std::exception_ptr), optional, a type with write_text, "
                      "format_value, to_text() or to_string(), or a type with describe()");
    };

    inline void set_string(Value& v, const char* p, size_t n) noexcept {
        v.kind = Kind::String;
        v.s.p = p;
        v.s.n = n;
    }

    template<class T>
    struct Holder<T, Category::Null> {
        Holder(const T&) noexcept {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Null;
        }
    };

    template<class T>
    struct Holder<T, Category::Bool> {
        Holder(const T& x) noexcept
        : value(x) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Bool;
            v.b = value;
        }

        bool value;
    };

    template<class T>
    struct Holder<T, Category::Signed> {
        Holder(const T& x) noexcept
        : value(int64_t(x)) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Int;
            v.i = value;
        }

        int64_t value;
    };

    template<class T>
    struct Holder<T, Category::Unsigned> {
        Holder(const T& x) noexcept
        : value(uint64_t(x)) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Uint;
            v.u = value;
        }

        uint64_t value;
    };

    template<class T>
    struct Holder<T, Category::Float> {
        Holder(const T& x) noexcept
        : value(double(x)) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Float;
            v.d = value;
        }

        double value;
    };

    template<class T>
    struct Holder<T, Category::CString> {
        Holder(const T& x) noexcept
        : p(x ? x : ""), n(x ? std::strlen(x) : 0) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, p, n);
        }

        const char* p;
        size_t n;
    };

    template<class T>
    struct Holder<T, Category::CharArray> {
        Holder(const T& x) noexcept
        : p(x), n(array_length(x)) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, p, n);
        }

        const char* p;
        size_t n;
    };

    template<class T>
    struct Holder<T, Category::Text> {
        Holder(const T& x) noexcept
        : p(x.data()), n(x.size()) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, p, n);
        }

        const char* p;
        size_t n;
    };

    template<class T>
    struct Holder<T, Category::Duration> {
        Holder(const T& x) noexcept
        : ns(sgcl::duration(x).nanoseconds()) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Duration;
            v.ns = ns;
        }

        int64_t ns;
    };

    template<class T>
    struct Holder<T, Category::Time> {
        Holder(const T& x) noexcept
        : ns(x.unix_nano()), offset(int32_t(x.offset().nanoseconds() / 1000000000)) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Time;
            v.ns = ns;
            v.offset = offset;
        }

        int64_t ns;
        int32_t offset;
    };

    template<class T>
    struct Holder<T, Category::IoError> {
        Holder(const T& x)
        : text(io_error_text(x)) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, text.data(), text.size());
        }

        std::string text;
    };

    template<class T>
    struct Holder<T, Category::ErrorCode> {
        Holder(const T& x)
        : text(x.message()) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, text.data(), text.size());
        }

        std::string text;
    };

    template<class T>
    struct Holder<T, Category::Exception> {
        Holder(const T& x)
        : null(!x), text(x ? exception_text(x) : std::string()) {
        }

        void fill(Value& v) const noexcept {
            if (null) {
                v.kind = Kind::Null;
            } else {
                set_string(v, text.data(), text.size());
            }
        }

        bool null;
        std::string text;
    };

    template<class T>
    struct Holder<T, Category::Optional> {
        using Inner = typename T::value_type;

        Holder(const T& x) {
            if (x) {
                inner.emplace(*x);
            }
        }

        void fill(Value& v) const {
            if (inner) {
                inner->fill(v);
            } else {
                v.kind = Kind::Null;
            }
        }

        std::optional<Holder<Inner>> inner;
    };

    template<class T>
    struct Holder<T, Category::WritesText> {
        Holder(const T& x) noexcept
        : p(&x) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Formatted;
            v.fmt.p = p;
            v.fmt.fn = &write_text_of<T>;
        }

        const T* p;
    };

    template<class T>
    struct Holder<T, Category::FormatsValue> {
        Holder(const T& x) noexcept
        : p(&x) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Formatted;
            v.fmt.p = p;
            v.fmt.fn = &format_value_of<T>;
        }

        const T* p;
    };

    template<class T>
    struct Holder<T, Category::ToText> {
        Holder(const T& x)
        : text(x.to_text()) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, text.data(), text.size());
        }

        string text;
    };

    template<class T>
    struct Holder<T, Category::ToString> {
        Holder(const T& x)
        : text(x.to_string()) {
        }

        void fill(Value& v) const noexcept {
            set_string(v, text.data(), text.size());
        }

        std::conditional_t<std::is_same_v<std::remove_cvref_t<decltype(std::declval<const T&>().to_string())>, std::string>, std::string, string> text;
    };

    template<class T>
    struct Holder<T, Category::Described> {
        Holder(const T& x) noexcept
        : p(&x) {
        }

        void fill(Value& v) const noexcept {
            v.kind = Kind::Record;
            v.obj.p = p;
            v.obj.ops = encoding::detail::value_ops<std::remove_cv_t<T>>();
        }

        const T* p;
    };

    //--------------------------------------------------------------------
    // The arguments of a call: keys, values and groups
    //--------------------------------------------------------------------
    enum class Role : uint8_t {
        Key,
        Value,
        Group
    };

    template<size_t N>
    struct Layout {
        Role role[N ? N : 1] = {};
        size_t index[N ? N : 1] = {};   // the attribute each argument goes to
        size_t count = 0;               // attributes
        int error = 0;                  // 1: a key without a value; 2: not a key where one is due; 3: a group given as a value
    };

    template<class... A>
    constexpr Layout<sizeof...(A)> layout_of() {
        constexpr size_t N = sizeof...(A);
        Layout<N> l;
        constexpr bool keys[N ? N : 1] = {IsKey<A>...};
        constexpr bool groups[N ? N : 1] = {IsGroup<std::remove_cv_t<A>>...};
        bool want_value = false;
        for (size_t i = 0; i < N; ++i) {
            if (want_value) {
                if (groups[i]) {
                    l.error = 3;
                    return l;
                }
                l.role[i] = Role::Value;
                l.index[i] = l.count - 1;
                want_value = false;
            } else if (groups[i]) {
                l.role[i] = Role::Group;
                l.index[i] = l.count++;
            } else if (keys[i]) {
                l.role[i] = Role::Key;
                l.index[i] = l.count++;
                want_value = true;
            } else {
                l.error = 2;
                return l;
            }
        }
        if (want_value) {
            l.error = 1;
        }
        return l;
    }

    // The holders of a call's arguments and the attributes made of them,
    // on the stack of the call. Neither copied nor moved: an attribute of
    // a group points into its holder
    template<class... A>
    struct Pack {
        static constexpr Layout<sizeof...(A)> L = layout_of<A...>();
        static_assert(L.error != 1, "slog: a key without a value: the attributes are pairs, key then value");
        static_assert(L.error != 2, "slog: a key must be a literal (or a const char*), or the argument a slog::group; the attributes are pairs, key then value");
        static_assert(L.error != 3, "slog: a group stands on its own, with its name inside: slog::group(\"req\", \"id\", id), not a value after a key");
        static constexpr size_t Count = L.count;

        explicit Pack(const A&... a)
        : holders(a...) {
            _fill(std::index_sequence_for<A...>{});
        }

        template<class Tuple>
        explicit Pack(const Tuple& t, int)
        : Pack(t, std::index_sequence_for<A...>{}) {
        }

        Pack(const Pack&) = delete;
        Pack& operator=(const Pack&) = delete;

        std::tuple<Holder<A>...> holders;
        Attr attrs[Count ? Count : 1];

    private:
        template<class Tuple, size_t... I>
        Pack(const Tuple& t, std::index_sequence<I...>)
        : holders(std::get<I>(t)...) {
            _fill(std::index_sequence_for<A...>{});
        }

        template<size_t... I>
        void _fill(std::index_sequence<I...>) {
            (_one<I>(), ...);
        }

        template<size_t I>
        void _one() {
            using T = std::tuple_element_t<I, std::tuple<A...>>;
            Attr& a = attrs[L.index[I]];
            const auto& h = std::get<I>(holders);
            if constexpr (L.role[I] == Role::Key) {
                if constexpr (IsKey<T>) {
                    a.key = h.p;
                    a.key_n = h.n;
                }
            } else if constexpr (L.role[I] == Role::Group) {
                if constexpr (IsGroup<std::remove_cv_t<T>>) {
                    h.fill_group(a);
                }
            } else {
                h.fill(a.value);
            }
        }
    };

    // A group's own holders, made from the references it keeps
    template<class... B>
    struct Holder<slog::group<B...>, Category::Group> {
        Holder(const slog::group<B...>& g);

        void fill_group(Attr& a) const noexcept {
            a.key = name;
            a.key_n = name_n;
            a.value.kind = Kind::Group;
            a.value.g = pack.attrs;
            a.value.count = uint32_t(Pack<B...>::Count);
        }

        void fill(Value&) const noexcept {
        }

        const char* name;
        size_t name_n;
        Pack<B...> pack;
    };
}

namespace sgcl::slog {
    // Attributes in a group of their own (slog.Group): one argument of a
    // record or of with(), standing where a key would, with its name
    // inside: log.info("request", slog::group("req", "id", id, "path",
    // path)) writes `req.id=5 req.path=/a` as text, "req":{"id":5,...} as
    // JSON. A group of no attributes, or of empty groups only, is left out.
    // It keeps its arguments by reference, as the call does: it is made
    // in the call that logs it, never kept.
    template<class... A>
    class group {
    public:
        group(const char* name, const A&... kv) noexcept
        : _name(name ? name : ""), _name_n(name ? std::strlen(name) : 0), _args(kv...) {
        }

    private:
        template<class T, detail::Category C>
        friend struct detail::Holder;

        const char* _name;
        size_t _name_n;
        std::tuple<const A&...> _args;
    };

    template<class... A>
    group(const char*, const A&...) -> group<A...>;
}

namespace sgcl::slog::detail {
    template<class... B>
    Holder<slog::group<B...>, Category::Group>::Holder(const slog::group<B...>& g)
    : name(g._name), name_n(g._name_n), pack(g._args, 0) {
    }
}
