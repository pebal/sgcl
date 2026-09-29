//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "text.h"
#include "value.h"
#include "../../encoding/json.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The attributes written as the two handlers write them. The text handler
// (slog's TextHandler): ` key=value`, a group's name before its keys with a
// dot (`req.id=5`), a value quoted when it has to be. The JSON handler
// (JSONHandler): `,"key":value`, a group an object. A type described by
// its fields is a group of them; a field that is a container, a variant
// or a json is Go's KindAny: %+v in text ([a b], map[k:v], {f:v}), what
// json.Marshal writes in JSON.
namespace sgcl::slog::detail {
    using encoding::detail::FieldAccess;
    using encoding::detail::FieldInfo;
    using encoding::detail::ValueKind;
    using encoding::field_list;

    // How deep a value of the program is followed (a cycle of pointers):
    // the rest is written as "..." in text and fails in JSON, as a cycle
    // fails json.Marshal
    inline constexpr int MaxDepth = 64;

    // The attributes of the call, where a logger's own kept as a tree
    // have a Splice for them (a record handed to a handler of the program)
    struct Tail {
        const Attr* a = nullptr;
        size_t n = 0;
    };

    // The characters of a string field without a copy (string,
    // std::string), or its text made (a type with to_text), into keep
    inline std::string_view chars_of(const void* p, const ValueOps* ops, string& keep) {
        if (ops == encoding::detail::value_ops<string>()) {
            const string& s = *static_cast<const string*>(p);
            return std::string_view(s.data(), s.size());
        }
        if (ops == encoding::detail::value_ops<std::string>()) {
            const std::string& s = *static_cast<const std::string*>(p);
            return std::string_view(s.data(), s.size());
        }
        keep = ops->get_text(p);
        return std::string_view(keep.data(), keep.size());
    }

    // The name of an enum's value, when the field gives names and the
    // value has one
    inline bool enum_name(const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, std::string_view& name) {
        if (!l || !f || !f->names_count) {
            return false;
        }
        int64_t i = ops->get_int(p);
        if (i < 0 || uint64_t(i) >= f->names_count) {
            return false;
        }
        name = FieldAccess::name(*l, *f, size_t(i));
        return true;
    }

    // The value of a field as an attribute's: the kinds that are data
    // become the value's own, a described type a Record (a group), the
    // rest Any
    inline void field_value(Value& v, const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, string& keep) {
        switch (ops->kind) {
            case ValueKind::boolean:
                v.kind = Kind::Bool;
                v.b = ops->get_bool(p);
                return;
            case ValueKind::signed_integer:
                v.kind = Kind::Int;
                v.i = ops->get_int(p);
                return;
            case ValueKind::unsigned_integer:
                v.kind = Kind::Uint;
                v.u = ops->get_uint(p);
                return;
            case ValueKind::floating:
                v.kind = Kind::Float;
                v.d = ops->get_double(p);
                return;
            case ValueKind::string:
            case ValueKind::text: {
                auto t = chars_of(p, ops, keep);
                set_string(v, t.data(), t.size());
                return;
            }
            case ValueKind::enumeration: {
                std::string_view name;
                if (enum_name(p, ops, l, f, name)) {
                    set_string(v, name.data(), name.size());
                } else {
                    v.kind = Kind::Int;
                    v.i = ops->get_int(p);
                }
                return;
            }
            case ValueKind::optional:
            case ValueKind::pointer:
                if (!ops->has_value(p)) {
                    v.kind = Kind::Null;
                    return;
                }
                field_value(v, ops->value(p), ops->inner(), l, f, keep);
                return;
            case ValueKind::record:
                v.kind = Kind::Record;
                v.obj.p = p;
                v.obj.ops = ops;
                return;
            default:
                v.kind = Kind::Any;
                v.obj.p = p;
                v.obj.ops = ops;
                return;
        }
    }

    // Every field of a described value as an attribute, to f (the ones
    // marked omit_empty and empty left out)
    template<class F>
    void for_each_field(const void* p, const ValueOps* ops, F&& f) {
        field_list list;
        ops->describe(const_cast<void*>(p), list);
        for (const FieldInfo& info : FieldAccess::fields(list)) {
            if ((info.flags & encoding::detail::OmitEmpty) && info.ops->is_empty(info.address)) {
                continue;
            }
            Attr a;
            a.key = info.name.data();
            a.key_n = info.name.size();
            string keep;
            field_value(a.value, info.address, info.ops, &list, &info, keep);
            f(a);
        }
    }

    inline bool is_groupish(const Value& v) noexcept {
        return v.kind == Kind::Group || v.kind == Kind::Record;
    }

    // Every attribute of a group, the call's in place of a Splice
    template<class F>
    void for_each_child(const Value& v, const Tail& t, F&& f) {
        if (v.kind == Kind::Record) {
            for_each_field(v.obj.p, v.obj.ops, f);
            return;
        }
        for (uint32_t i = 0; i < v.count; ++i) {
            const Attr& a = v.g[i];
            if (a.value.kind == Kind::Splice) {
                for (size_t k = 0; k < t.n; ++k) {
                    f(t.a[k]);
                }
            } else {
                f(a);
            }
        }
    }

    // A group of no attributes, or of empty groups only: left out, as
    // slog leaves it
    inline bool group_empty(const Value& v, const Tail& t, int depth = 0) {
        if (depth > MaxDepth) {
            return false;
        }
        bool empty = true;
        for_each_child(v, t, [&](const Attr& a) {
            if (empty && !(is_groupish(a.value) && group_empty(a.value, t, depth + 1))) {
                empty = false;
            }
        });
        return empty;
    }

    //--------------------------------------------------------------------
    // Go's %+v of a value of the program (the text of KindAny)
    //--------------------------------------------------------------------
    inline void go_text(Buf& b, const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, int depth);

    struct GoTextCtx {
        Buf* b;
        const ValueOps* ops;
        int depth;
        bool first;
    };

    inline void go_text(Buf& b, const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, int depth) {
        if (depth > MaxDepth) {
            b.put("...", 3);
            return;
        }
        switch (ops->kind) {
            case ValueKind::boolean:
                b.put(ops->get_bool(p) ? std::string_view("true") : std::string_view("false"));
                return;
            case ValueKind::signed_integer:
                put_int(b, ops->get_int(p));
                return;
            case ValueKind::unsigned_integer:
                put_uint(b, ops->get_uint(p));
                return;
            case ValueKind::floating: {
                char* o = b.reserve(FloatTextSize);
                b.commit(float_text(o, ops->get_double(p)));
                return;
            }
            case ValueKind::string:
            case ValueKind::text: {
                string keep;
                b.put(chars_of(p, ops, keep));
                return;
            }
            case ValueKind::enumeration: {
                std::string_view name;
                if (enum_name(p, ops, l, f, name)) {
                    b.put(name);
                } else {
                    put_int(b, ops->get_int(p));
                }
                return;
            }
            case ValueKind::optional:
            case ValueKind::pointer:
                if (!ops->has_value(p)) {
                    b.put("<nil>", 5);
                } else {
                    go_text(b, ops->value(p), ops->inner(), l, f, depth + 1);
                }
                return;
            case ValueKind::record: {
                field_list list;
                ops->describe(const_cast<void*>(p), list);
                b.put('{');
                bool first = true;
                for (const FieldInfo& info : FieldAccess::fields(list)) {
                    if (!first) {
                        b.put(' ');
                    }
                    first = false;
                    b.put(info.name);
                    b.put(':');
                    go_text(b, info.address, info.ops, &list, &info, depth + 1);
                }
                b.put('}');
                return;
            }
            case ValueKind::sequence:
            case ValueKind::set:
            case ValueKind::fixed: {
                b.put('[');
                GoTextCtx c{&b, ops->inner(), depth + 1, true};
                ops->for_each(p, &c, [](void* ctx, const void* e) {
                    auto& k = *static_cast<GoTextCtx*>(ctx);
                    if (!k.first) {
                        k.b->put(' ');
                    }
                    k.first = false;
                    go_text(*k.b, e, k.ops, nullptr, nullptr, k.depth);
                    return true;
                });
                b.put(']');
                return;
            }
            case ValueKind::tuple: {
                b.put('[');
                for (size_t i = 0; i < ops->fixed_count; ++i) {
                    if (i) {
                        b.put(' ');
                    }
                    const ValueOps* e = nullptr;
                    const void* q = ops->element_of(p, i, &e);
                    go_text(b, q, e, nullptr, nullptr, depth + 1);
                }
                b.put(']');
                return;
            }
            case ValueKind::map: {
                b.put("map[", 4);
                GoTextCtx c{&b, ops->inner(), depth + 1, true};
                ops->for_each_entry(p, &c, [](void* ctx, std::string_view key, const void* v) {
                    auto& k = *static_cast<GoTextCtx*>(ctx);
                    if (!k.first) {
                        k.b->put(' ');
                    }
                    k.first = false;
                    k.b->put(key);
                    k.b->put(':');
                    go_text(*k.b, v, k.ops, nullptr, nullptr, k.depth);
                    return true;
                }, true);
                b.put(']');
                return;
            }
            case ValueKind::variant: {
                const ValueOps* e = nullptr;
                const void* q = ops->alternative(p, &e);
                go_text(b, q, e, nullptr, nullptr, depth + 1);
                return;
            }
            case ValueKind::json:
            case ValueKind::custom_json: {
                auto t = encoding::json::stringify(ops->to_json(p));
                if (t) {
                    b.put(t->data(), t->size());
                }
                return;
            }
        }
    }

    //--------------------------------------------------------------------
    // json.Marshal of a value of the program (the JSON of KindAny)
    //--------------------------------------------------------------------
    struct JsonFail {
        bool failed = false;
        bool cycle = false;
        double value = 0;
    };

    inline bool json_any(Buf& b, const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, int depth, JsonFail& fail);

    struct JsonAnyCtx {
        Buf* b;
        const ValueOps* ops;
        int depth;
        bool first;
        JsonFail* fail;
    };

    inline bool json_any(Buf& b, const void* p, const ValueOps* ops, const field_list* l, const FieldInfo* f, int depth, JsonFail& fail) {
        if (depth > MaxDepth) {
            fail.failed = true;
            fail.cycle = true;
            return false;
        }
        switch (ops->kind) {
            case ValueKind::boolean:
                b.put(ops->get_bool(p) ? std::string_view("true") : std::string_view("false"));
                return true;
            case ValueKind::signed_integer:
                put_int(b, ops->get_int(p));
                return true;
            case ValueKind::unsigned_integer:
                put_uint(b, ops->get_uint(p));
                return true;
            case ValueKind::floating: {
                double d = ops->get_double(p);
                if (!std::isfinite(d)) {
                    fail.failed = true;
                    fail.value = d;
                    return false;
                }
                char* o = b.reserve(FloatTextSize);
                b.commit(float_json(o, d));
                return true;
            }
            case ValueKind::string:
            case ValueKind::text: {
                string keep;
                auto t = chars_of(p, ops, keep);
                json_string(b, t.data(), t.size(), true);
                return true;
            }
            case ValueKind::enumeration: {
                std::string_view name;
                if (enum_name(p, ops, l, f, name)) {
                    json_string(b, name.data(), name.size(), true);
                } else {
                    put_int(b, ops->get_int(p));
                }
                return true;
            }
            case ValueKind::optional:
            case ValueKind::pointer:
                if (!ops->has_value(p)) {
                    b.put("null", 4);
                    return true;
                }
                return json_any(b, ops->value(p), ops->inner(), l, f, depth + 1, fail);
            case ValueKind::record: {
                field_list list;
                ops->describe(const_cast<void*>(p), list);
                b.put('{');
                bool first = true;
                for (const FieldInfo& info : FieldAccess::fields(list)) {
                    if ((info.flags & encoding::detail::OmitEmpty) && info.ops->is_empty(info.address)) {
                        continue;
                    }
                    if (!first) {
                        b.put(',');
                    }
                    first = false;
                    json_string(b, info.name.data(), info.name.size(), true);
                    b.put(':');
                    if (!json_any(b, info.address, info.ops, &list, &info, depth + 1, fail)) {
                        return false;
                    }
                }
                b.put('}');
                return true;
            }
            case ValueKind::sequence:
            case ValueKind::set:
            case ValueKind::fixed: {
                b.put('[');
                JsonAnyCtx c{&b, ops->inner(), depth + 1, true, &fail};
                ops->for_each(p, &c, [](void* ctx, const void* e) {
                    auto& k = *static_cast<JsonAnyCtx*>(ctx);
                    if (!k.first) {
                        k.b->put(',');
                    }
                    k.first = false;
                    return json_any(*k.b, e, k.ops, nullptr, nullptr, k.depth, *k.fail);
                });
                if (fail.failed) {
                    return false;
                }
                b.put(']');
                return true;
            }
            case ValueKind::tuple: {
                b.put('[');
                for (size_t i = 0; i < ops->fixed_count; ++i) {
                    if (i) {
                        b.put(',');
                    }
                    const ValueOps* e = nullptr;
                    const void* q = ops->element_of(p, i, &e);
                    if (!json_any(b, q, e, nullptr, nullptr, depth + 1, fail)) {
                        return false;
                    }
                }
                b.put(']');
                return true;
            }
            case ValueKind::map: {
                b.put('{');
                JsonAnyCtx c{&b, ops->inner(), depth + 1, true, &fail};
                ops->for_each_entry(p, &c, [](void* ctx, std::string_view key, const void* v) {
                    auto& k = *static_cast<JsonAnyCtx*>(ctx);
                    if (!k.first) {
                        k.b->put(',');
                    }
                    k.first = false;
                    json_string(*k.b, key.data(), key.size(), true);
                    k.b->put(':');
                    return json_any(*k.b, v, k.ops, nullptr, nullptr, k.depth, *k.fail);
                }, ops->hashed);
                if (fail.failed) {
                    return false;
                }
                b.put('}');
                return true;
            }
            case ValueKind::variant: {
                const ValueOps* e = nullptr;
                const void* q = ops->alternative(p, &e);
                return json_any(b, q, e, nullptr, nullptr, depth + 1, fail);
            }
            case ValueKind::json:
            case ValueKind::custom_json: {
                auto t = encoding::json::stringify(ops->to_json(p));
                if (t) {
                    b.put(t->data(), t->size());
                } else {
                    b.put("null", 4);
                }
                return true;
            }
        }
        return true;
    }

    // What slog writes in place of a value json.Marshal refused
    inline void json_error(Buf& b, const JsonFail& fail) {
        Buf& out = b;
        char text[64];
        size_t n;
        if (fail.cycle) {
            static constexpr std::string_view Cycle = "!ERROR:json: unsupported value: encountered a cycle";
            json_string(out, Cycle.data(), Cycle.size());
            return;
        }
        static constexpr std::string_view Head = "!ERROR:json: unsupported value: ";
        sgcl::detail::copy_bytes(text, Head.data(), Head.size());
        n = Head.size() + float_text(text + Head.size(), fail.value);
        json_string(out, text, n);
    }

    //--------------------------------------------------------------------
    // The handlers' attributes
    //--------------------------------------------------------------------
    // The line and the buffers the values are made in on the way to it
    struct Lines {
        Buf line;       // the record
        Buf tmp;        // a value's text before it is quoted
        Buf prefix;     // the text handler's group prefix ("req.")
    };

    // A value that writes itself, into tmp
    inline std::string_view formatted(Buf& tmp, const Value& v) {
        tmp.clear();
        constexpr size_t First = 128;
        char* at = tmp.reserve(First);
        size_t n = v.fmt.fn(v.fmt.p, at, First);
        if (n > First) {
            at = tmp.reserve(n);
            n = v.fmt.fn(v.fmt.p, at, n);
        }
        tmp.commit(n);
        return tmp.view();
    }

    inline void text_value(Lines& w, const Value& v) {
        Buf& b = w.line;
        switch (v.kind) {
            case Kind::Null:
                b.put("<nil>", 5);
                return;
            case Kind::Bool:
                b.put(v.b ? std::string_view("true") : std::string_view("false"));
                return;
            case Kind::Int:
                put_int(b, v.i);
                return;
            case Kind::Uint:
                put_uint(b, v.u);
                return;
            case Kind::Float: {
                char* o = b.reserve(FloatTextSize);
                b.commit(float_text(o, v.d));
                return;
            }
            case Kind::String:
                text_string(b, v.s.p, v.s.n);
                return;
            case Kind::Duration: {
                char* o = b.reserve(DurationTextSize);
                b.commit(duration_text(o, v.ns));
                return;
            }
            case Kind::Time: {
                char* o = b.reserve(TimeTextSize);
                b.commit(time_text(o, v.ns, v.offset));
                return;
            }
            case Kind::Formatted: {
                auto t = formatted(w.tmp, v);
                text_string(b, t.data(), t.size());
                return;
            }
            case Kind::Any: {
                w.tmp.clear();
                go_text(w.tmp, v.obj.p, v.obj.ops, nullptr, nullptr, 0);
                text_string(b, w.tmp.data(), w.tmp.size());
                return;
            }
            case Kind::Owned:
                text_string(b, v.s.p + v.s.n, v.count);
                return;
            case Kind::Group:
            case Kind::Record:
            case Kind::Splice:
                return;
        }
    }

    // prefix + key, quoted as one text when either part needs it
    inline void text_key(Buf& b, const Buf& prefix, const char* key, size_t n) {
        const bool quote = (prefix.size() == 0 && n == 0) || text_needs_quoting(key, n)
                        || (prefix.size() && text_needs_quoting(prefix.data(), prefix.size()));
        if (!quote) {
            b.put(prefix.data(), prefix.size());
            b.put(key, n);
            return;
        }
        b.put('"');
        text_escaped(b, prefix.data(), prefix.size());
        text_escaped(b, key, n);
        b.put('"');
    }

    inline bool text_attr(Lines& w, const Attr& a, const Tail& t, int depth = 0);

    inline bool text_attrs(Lines& w, const Attr* a, size_t n, const Tail& t = Tail{}) {
        bool any = false;
        for (size_t i = 0; i < n; ++i) {
            if (a[i].value.kind == Kind::Splice) {
                for (size_t k = 0; k < t.n; ++k) {
                    any |= text_attr(w, t.a[k], t);
                }
            } else {
                any |= text_attr(w, a[i], t);
            }
        }
        return any;
    }

    // One attribute, ` key=value`, or a group's attributes under its
    // name; false when nothing was written
    inline bool text_attr(Lines& w, const Attr& a, const Tail& t, int depth) {
        if (is_empty_attr(a)) {
            return false;
        }
        if (is_groupish(a.value)) {
            if (depth > MaxDepth || group_empty(a.value, t)) {
                return false;
            }
            size_t mark = w.prefix.size();
            if (a.key_n) {
                w.prefix.put(a.key, a.key_n);
                w.prefix.put('.');
            }
            bool any = false;
            for_each_child(a.value, t, [&](const Attr& c) {
                any |= text_attr(w, c, t, depth + 1);
            });
            w.prefix.resize_down(mark);
            return any;
        }
        w.line.put(' ');
        text_key(w.line, w.prefix, a.key, a.key_n);
        w.line.put('=');
        text_value(w, a.value);
        return true;
    }

    inline void json_value(Lines& w, const Value& v) {
        Buf& b = w.line;
        switch (v.kind) {
            case Kind::Null:
                b.put("null", 4);
                return;
            case Kind::Bool:
                b.put(v.b ? std::string_view("true") : std::string_view("false"));
                return;
            case Kind::Int:
                put_int(b, v.i);
                return;
            case Kind::Uint:
                put_uint(b, v.u);
                return;
            case Kind::Float: {
                if (!std::isfinite(v.d)) {
                    JsonFail f;
                    f.failed = true;
                    f.value = v.d;
                    json_error(b, f);
                    return;
                }
                char* o = b.reserve(FloatTextSize);
                b.commit(float_json(o, v.d));
                return;
            }
            case Kind::String:
                json_string(b, v.s.p, v.s.n);
                return;
            case Kind::Duration:
                put_int(b, v.ns);
                return;
            case Kind::Time: {
                char* o = b.reserve(TimeTextSize + 2);
                o[0] = '"';
                size_t n = time_json(o + 1, v.ns, v.offset);
                o[n + 1] = '"';
                b.commit(n + 2);
                return;
            }
            case Kind::Formatted: {
                auto t = formatted(w.tmp, v);
                json_string(b, t.data(), t.size());
                return;
            }
            case Kind::Any: {
                size_t mark = b.size();
                JsonFail fail;
                if (!json_any(b, v.obj.p, v.obj.ops, nullptr, nullptr, 0, fail)) {
                    b.resize_down(mark);
                    json_error(b, fail);
                }
                return;
            }
            case Kind::Owned:
                b.put(v.s.p, v.s.n);
                return;
            case Kind::Group:
            case Kind::Record:
            case Kind::Splice:
                return;
        }
    }

    inline bool json_attr(Lines& w, const Attr& a, bool& first, const Tail& t, int depth = 0);

    inline bool json_attrs(Lines& w, const Attr* a, size_t n, bool& first, const Tail& t = Tail{}) {
        bool any = false;
        for (size_t i = 0; i < n; ++i) {
            if (a[i].value.kind == Kind::Splice) {
                for (size_t k = 0; k < t.n; ++k) {
                    any |= json_attr(w, t.a[k], first, t);
                }
            } else {
                any |= json_attr(w, a[i], first, t);
            }
        }
        return any;
    }

    // One attribute, `"key":value` after a comma unless first, or a
    // group as an object (a group without a name: its attributes inline)
    inline bool json_attr(Lines& w, const Attr& a, bool& first, const Tail& t, int depth) {
        if (is_empty_attr(a)) {
            return false;
        }
        Buf& b = w.line;
        if (is_groupish(a.value)) {
            if (depth > MaxDepth || group_empty(a.value, t)) {
                return false;
            }
            if (!a.key_n) {
                bool any = false;
                for_each_child(a.value, t, [&](const Attr& c) {
                    any |= json_attr(w, c, first, t, depth + 1);
                });
                return any;
            }
            if (!first) {
                b.put(',');
            }
            first = false;
            json_string(b, a.key, a.key_n);
            b.put(":{", 2);
            bool inner = true;
            for_each_child(a.value, t, [&](const Attr& c) {
                json_attr(w, c, inner, t, depth + 1);
            });
            b.put('}');
            return true;
        }
        if (!first) {
            b.put(',');
        }
        first = false;
        json_string(b, a.key, a.key_n);
        b.put(':');
        json_value(w, a.value);
        return true;
    }
}
