//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "level.h"
#include "detail/owned.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace sgcl::slog {
    class attr;
    class attrs;
    class record;

    // The message of a record and where in the code it was written: a
    // string literal, a const char*, a string, a std::string or a text
    // slice, and the std::source_location of the call, which the
    // constructor's default argument takes where the call is (a pack of
    // attributes after the message leaves no place for a default
    // argument of the verb itself). Refers to the text, as the call does.
    class message {
    public:
        template<class C>
        requires std::same_as<C, const char*> || std::same_as<C, char*>
        SGCL_INLINE_HOT message(C text, std::source_location where = std::source_location::current()) noexcept
        : _p(text ? text : ""), _n(text ? std::strlen(text) : 0), _where(where) {
        }

        template<size_t N>
        SGCL_INLINE_HOT message(const char (&text)[N], std::source_location where = std::source_location::current()) noexcept
        : _p(text), _n(detail::array_length(text)), _where(where) {
        }

        SGCL_INLINE_HOT message(const string& text, std::source_location where = std::source_location::current()) noexcept
        : _p(text.data()), _n(text.size()), _where(where) {
        }

        SGCL_INLINE_HOT message(const std::string& text, std::source_location where = std::source_location::current()) noexcept
        : _p(text.data()), _n(text.size()), _where(where) {
        }

        SGCL_INLINE_HOT message(const slice<const char>& text, std::source_location where = std::source_location::current()) noexcept
        : _p(text.data()), _n(text.size()), _where(where) {
        }

        SGCL_INLINE_HOT slice<const char> text() const noexcept {
            return slice<const char>(_p, _n);
        }

        SGCL_INLINE_HOT const std::source_location& where() const noexcept {
            return _where;
        }

    private:
        friend struct detail::Access;

        const char* _p;
        size_t _n;
        std::source_location _where;
    };

    namespace detail {
        // What a record is: its time, level, message and place, and its
        // attributes, the logger's (a list with a Splice where the call's
        // go) and the call's (the tail), or the call's alone
        struct RecordData {
            int64_t ns = 0;
            int32_t offset = 0;
            bool utc = false;
            bool has_source = false;
            slog::level lvl = slog::level::info;
            const char* msg = "";
            size_t msg_n = 0;
            std::source_location where;
            const Attr* attrs = nullptr;
            size_t n = 0;
            Tail tail;
        };

        // A record's copy: its data and the memory it points into
        struct RecordCopy {
            Arena arena;
            RecordData data;
        };
    }

    // A value of an attribute, as a handler reads it (slog.Value): a
    // view of the record's, valid while the record is. type() says the
    // kind; as_...() gives the value of that kind (another kind: a
    // logic_error); text() and json() write it as the two handlers do.
    // A type described by its fields is a group of them; a container,
    // a variant or a json of the program is `any`, read through text()
    // and json().
    class value {
    public:
        enum class kind : uint8_t {
            null,
            boolean,
            int64,
            uint64,
            float64,
            string,
            duration,
            time,
            group,
            any
        };

        value() noexcept = default;

        kind type() const noexcept {
            switch (_v->kind) {
                case detail::Kind::Null: return kind::null;
                case detail::Kind::Bool: return kind::boolean;
                case detail::Kind::Int: return kind::int64;
                case detail::Kind::Uint: return kind::uint64;
                case detail::Kind::Float: return kind::float64;
                case detail::Kind::String:
                case detail::Kind::Formatted: return kind::string;
                case detail::Kind::Duration: return kind::duration;
                case detail::Kind::Time: return kind::time;
                case detail::Kind::Group:
                case detail::Kind::Record: return kind::group;
                case detail::Kind::Any:
                case detail::Kind::Owned: return kind::any;
                case detail::Kind::Splice: return kind::null;
            }
            return kind::null;
        }

        SGCL_INLINE_HOT bool as_bool() const {
            _want(kind::boolean, "as_bool");
            return _v->b;
        }

        SGCL_INLINE_HOT int64_t as_int() const {
            _want(kind::int64, "as_int");
            return _v->i;
        }

        SGCL_INLINE_HOT uint64_t as_uint() const {
            _want(kind::uint64, "as_uint");
            return _v->u;
        }

        SGCL_INLINE_HOT double as_double() const {
            _want(kind::float64, "as_double");
            return _v->d;
        }

        SGCL_INLINE_HOT sgcl::duration as_duration() const {
            _want(kind::duration, "as_duration");
            return sgcl::duration(std::chrono::nanoseconds(_v->ns));
        }

        // The time, in a zone of its offset (UTC for none)
        SGCL_INLINE_HOT time::datetime as_time() const {
            _want(kind::time, "as_time");
            return _v->offset ? time::datetime::from_unix_nano(_v->ns, time::zone::fixed(std::chrono::seconds(_v->offset)))
                              : time::datetime::from_unix_nano(_v->ns, time::zone::utc());
        }

        // The text of a string (a value that writes itself, written)
        SGCL_INLINE_HOT string as_string() const {
            _want(kind::string, "as_string");
            if (_v->kind == detail::Kind::Formatted) {
                detail::Buf tmp;
                auto t = detail::formatted(tmp, *_v);
                return string(t.data(), t.size());
            }
            return string(_v->s.p, _v->s.n);
        }

        // The attributes of a group, in order
        slog::attrs as_group() const;

        // The value as the text handler writes it, not quoted: 5, 1.5s,
        // [a b]; a group [k=v k2=v2]
        string text() const;

        // The value as the JSON handler writes it: "a", 5, {"k":1}
        string json() const;

    private:
        friend class attr;
        friend class attrs;
        friend struct detail::Access;

        SGCL_INLINE_HOT value(const detail::Value* v, const detail::Tail& t, const tracked_ptr<const void>& owner) noexcept
        : _v(v), _tail(t), _owner(owner) {
        }

        SGCL_INLINE_HOT void _want(kind k, const char* what) const {
            if (type() != k) {
                throw std::logic_error(std::string("sgcl::slog::value::") + what + ": a value of another kind");
            }
        }

        static inline const detail::Value Null{};

        const detail::Value* _v = &Null;
        detail::Tail _tail;
        tracked_ptr<const void> _owner;   // what keeps a group made of a described type
    };

    // An attribute of a record (slog.Attr): its key and value
    class attr {
    public:
        SGCL_INLINE_HOT slice<const char> key() const noexcept {
            return slice<const char>(_a->key, _a->key_n);
        }

        SGCL_INLINE_HOT slog::value value() const noexcept {
            return slog::value(&_a->value, _tail, _owner);
        }

    private:
        friend class attrs;
        friend struct detail::Access;

        SGCL_INLINE_HOT attr(const detail::Attr* a, const detail::Tail& t, const tracked_ptr<const void>& owner) noexcept
        : _a(a), _tail(t), _owner(owner) {
        }

        const detail::Attr* _a;
        detail::Tail _tail;
        tracked_ptr<const void> _owner;
    };

    // The attributes of a group or of a record, in order (the []Attr of
    // slog's Value.Group): a range of attr, the call's attributes in
    // place of the Splice that stands for them in the logger's list. A
    // view of the record's, valid while the record is; the attributes of
    // a type described by its fields are a copy of their own, which the
    // range keeps.
    class attrs {
    public:
        // An input iterator whose * is an attr, by value
        class iterator {
        public:
            using value_type = attr;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::input_iterator_tag;

            iterator() noexcept = default;

            SGCL_INLINE_HOT attr operator*() const noexcept {
                return attr(_at, _tail, _owner);
            }

            SGCL_INLINE_HOT iterator& operator++() noexcept {
                ++_at;
                _settle();
                return *this;
            }

            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator i = *this;
                ++*this;
                return i;
            }

            SGCL_INLINE_HOT friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._at == b._at;
            }

        private:
            friend class attrs;

            SGCL_INLINE_HOT iterator(const detail::Attr* at, const detail::Attr* end, const detail::Tail& tail, bool in_tail,
                     const tracked_ptr<const void>& owner) noexcept
            : _at(at), _end(end), _tail(tail), _in_tail(in_tail), _owner(owner) {
                _settle();
            }

            // Past a Splice into the call's attributes, and at their
            // end, to the one end all iterators compare with
            SGCL_INLINE_HOT void _settle() noexcept {
                if (!_in_tail && _at != _end && _at->value.kind == detail::Kind::Splice) {
                    _in_tail = true;
                    _at = _tail.a;
                    _end = _tail.a + _tail.n;
                }
                if (_at == _end) {
                    _at = nullptr;
                }
            }

            const detail::Attr* _at = nullptr;
            const detail::Attr* _end = nullptr;
            detail::Tail _tail;
            bool _in_tail = false;
            tracked_ptr<const void> _owner;   // what keeps a copy's attributes
        };

        // No attributes
        attrs() noexcept = default;

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return iterator(_a, _a + _n, _tail, false, _owner);
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return iterator();
        }

        // Walked and counted: the call's attributes stand in the list
        // in place of one entry
        size_t size() const noexcept {
            size_t k = 0;
            for (auto i = begin(); i != end(); ++i) {
                ++k;
            }
            return k;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return begin() == end();
        }

    private:
        friend class value;
        friend class record;

        SGCL_INLINE_HOT attrs(const detail::Attr* a, size_t n, const detail::Tail& tail,
              const tracked_ptr<const void>& owner) noexcept
        : _a(a), _n(n), _tail(tail), _owner(owner) {
        }

        // A group's attributes, a described type's made into a copy of
        // their own first, which the range keeps
        static attrs _group(const detail::Value& v, const detail::Tail& t, const tracked_ptr<const void>& owner);

        const detail::Attr* _a = nullptr;
        size_t _n = 0;
        detail::Tail _tail;
        tracked_ptr<const void> _owner;
    };

    namespace detail {
        struct Access {
            static const RecordData& data(const record& r) noexcept;
            static record make(const RecordData& d) noexcept;

            SGCL_INLINE_HOT static const tracked_ptr<LevelVarState>& state(const level_var& v) noexcept {
                return v._s;
            }

            SGCL_INLINE_HOT static const Attr* of(const attr& a) noexcept {
                return a._a;
            }

            SGCL_INLINE_HOT static const char* text(const message& m) noexcept {
                return m._p;
            }

            SGCL_INLINE_HOT static size_t size(const message& m) noexcept {
                return m._n;
            }
        };

    }

    inline attrs attrs::_group(const detail::Value& v, const detail::Tail& t, const tracked_ptr<const void>& owner) {
        if (v.kind == detail::Kind::Group) {
            return attrs(v.g, v.count, t, owner);
        }
        if (v.kind == detail::Kind::Record) {
            tracked_ptr<detail::RecordCopy> copy = make_tracked<detail::RecordCopy>();
            detail::Lines w;
            detail::Value g;
            detail::copy_children(copy->arena, w, v, g, t, 0);
            return attrs(g.g, g.count, detail::Tail{}, tracked_ptr<const void>(copy));
        }
        return attrs();
    }

    SGCL_INLINE_HOT attrs value::as_group() const {
        _want(kind::group, "as_group");
        return attrs::_group(*_v, _tail, _owner);
    }

    inline string value::text() const {
        detail::Lines w;
        detail::Buf& b = w.line;
        switch (_v->kind) {
            case detail::Kind::String:
                b.put(_v->s.p, _v->s.n);
                break;
            case detail::Kind::Formatted: {
                auto t = detail::formatted(w.tmp, *_v);
                b.put(t);
                break;
            }
            case detail::Kind::Any:
                detail::go_text(b, _v->obj.p, _v->obj.ops, nullptr, nullptr, 0);
                break;
            case detail::Kind::Owned:
                b.put(_v->s.p + _v->s.n, _v->count);
                break;
            case detail::Kind::Group:
            case detail::Kind::Record: {
                b.put('[');
                bool first = true;
                for (attr a : as_group()) {
                    if (!first) {
                        b.put(' ');
                    }
                    first = false;
                    b.put(a.key().data(), a.key().size());
                    b.put('=');
                    string t = a.value().text();
                    b.put(t.data(), t.size());
                }
                b.put(']');
                break;
            }
            default:
                detail::text_value(w, *_v);
        }
        return string(b.data(), b.size());
    }

    inline string value::json() const {
        detail::Lines w;
        detail::Buf& b = w.line;
        if (_v->kind == detail::Kind::Group || _v->kind == detail::Kind::Record) {
            b.put('{');
            bool first = true;
            detail::for_each_child(*_v, _tail, [&](const detail::Attr& c) {
                detail::json_attr(w, c, first, _tail, detail::child_depth(*_v, 0));
            });
            b.put('}');
        } else {
            detail::json_value(w, *_v);
        }
        return string(b.data(), b.size());
    }

    // A record as a handler of the program gets it (slog.Record): the
    // time, the level, the message, where it was written, and the
    // attributes — the logger's (from with(), inside its groups) and the
    // call's, as a tree in which a logger's group() is an attribute of
    // kind group that holds what came after it. A view of the stack of
    // the call, valid for the call to handle(); clone() is a copy that
    // owns all it holds, to keep (slog::memory keeps those).
    class record {
    public:
        record() noexcept = default;

        // The time, in the logger's zone (local, or UTC with options::utc)
        SGCL_INLINE_HOT time::datetime time() const noexcept {
            return _d.utc ? time::datetime::from_unix_nano(_d.ns, time::zone::utc()) : time::datetime::from_unix_nano(_d.ns);
        }

        SGCL_INLINE_HOT slog::level level() const noexcept {
            return _d.lvl;
        }

        SGCL_INLINE_HOT slice<const char> message() const noexcept {
            return slice<const char>(_d.msg, _d.msg_n);
        }

        // Where the call is; the logger's .source() makes the handlers
        // write it
        SGCL_INLINE_HOT const std::source_location& source() const noexcept {
            return _d.where;
        }

        SGCL_INLINE_HOT bool has_source() const noexcept {
            return _d.has_source;
        }

        SGCL_INLINE_HOT attrs::iterator begin() const noexcept {
            return _range().begin();
        }

        SGCL_INLINE_HOT attrs::iterator end() const noexcept {
            return attrs::iterator();
        }

        // The attributes at the top of the tree
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _range().size();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _range().empty();
        }

        // A copy that owns everything: the texts, the attributes, the
        // values of the program made data (a described type a group, a
        // container its two texts)
        record clone() const {
            tracked_ptr<detail::RecordCopy> copy = make_tracked<detail::RecordCopy>();
            detail::Lines w;
            detail::RecordData& d = copy->data;
            d = _d;
            d.msg = copy->arena.copy(_d.msg, _d.msg_n);
            size_t n = 0;
            for (auto i = begin(); i != end(); ++i) {
                ++n;
            }
            detail::Attr* a = copy->arena.attrs(n);
            size_t k = 0;
            for (auto i = begin(); i != end() && k < n; ++i, ++k) {
                detail::copy_attr(copy->arena, w, *detail::Access::of(*i), a[k], _d.tail, 0);
            }
            d.attrs = a;
            d.n = n;
            d.tail = detail::Tail{};
            record r;
            r._d = d;
            r._owner = tracked_ptr<const void>(copy);
            return r;
        }

    private:
        friend struct detail::Access;

        SGCL_INLINE_HOT attrs _range() const noexcept {
            return attrs(_d.attrs, _d.n, _d.tail, _owner);
        }

        detail::RecordData _d;
        tracked_ptr<const void> _owner;   // a clone's copy
    };

    namespace detail {
        SGCL_INLINE_HOT const RecordData& Access::data(const record& r) noexcept {
            return r._d;
        }

        SGCL_INLINE_HOT record Access::make(const RecordData& d) noexcept {
            record r;
            r._d = d;
            return r;
        }
    }
}
