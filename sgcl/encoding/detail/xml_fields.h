//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../fields.h"
#include "../xml.h"

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

// The types of a program in XML, through their describe(field_list&): the
// definitions of xml's members parse<T>, async_parse<T>, as<T>, from,
// stringify, reader::read<T> and writer::value. A type is mapped as Go
// maps a structure:
//
//   - a field is a child element named as the field (a name as written,
//     "dc:title", or "{namespace}local" to match by namespace);
//   - a field marked attribute() is an attribute of the element, one
//     marked text() the element's own text;
//   - a number, a boolean, a string, an enum (by its names() or its value)
//     or a type with to_text/from_text is text: the element's text, or the
//     attribute's value;
//   - a sequence or a set (vector, list, set, the immutable ones, array<T,
//     N>) is the element repeated, once for each value; an optional or a
//     tracked_ptr is an element that may be absent (an attribute too);
//   - a type with describe() is an element holding its fields.
//
// Maps, tuples, variants and json values have no form in XML here: a field
// of one is errc::unsupported_value when written and errc::type_mismatch
// when read, with its path.
//
// The mapping goes through a tree: a document is read into xml nodes, and
// the nodes are mapped onto the type (so an element may come in any order,
// the elements of a sequence among others); a value is made into nodes and
// the nodes written. An error of the mapping has the path of the element
// or attribute ("/catalog/book[2]/@id", indexes from 1 as XPath counts)
// and the offset where the element read began.
namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    inline bool xml_scalar(ValueKind k) noexcept {
        switch (k) {
            case ValueKind::boolean:
            case ValueKind::signed_integer:
            case ValueKind::unsigned_integer:
            case ValueKind::floating:
            case ValueKind::string:
            case ValueKind::text:
            case ValueKind::enumeration:
                return true;
            default:
                return false;
        }
    }

    inline std::string_view xml_trimmed(std::string_view t) noexcept {
        while (!t.empty() && xml_space(t.front())) {
            t.remove_prefix(1);
        }
        while (!t.empty() && xml_space(t.back())) {
            t.remove_suffix(1);
        }
        return t;
    }

    // What a mapping failed with: the code, the words, the path; the place
    // in the document is set by whoever read it
    struct XmlMapFailure {
        errc code = errc::type_mismatch;
        std::string what;
        std::string path;
    };

    // The mapping of values onto nodes and back, over the operations of
    // their types (fields.h): written once against the kinds, never
    // against the types
    class XmlMapper {
    public:
        static constexpr uint32_t MaxDepth = 512;

        optional<XmlMapFailure> failure;

        // --- a value into nodes ---

        // The element `name` for the value; xml() and a failure when it
        // has no form
        // (fields and f: the field the value is of, for an enum's names)
        xml element(const ValueOps* ops, const void* v, std::string_view name, const std::string& path, uint32_t depth,
                    const field_list* of = nullptr, const FieldInfo* field = nullptr) {
            if (depth > MaxDepth) {
                _fail(errc::unsupported_value, "nested deeper than 512 elements: a cycle?", path);
                return xml();
            }
            string n(name);
            if (!xml_qname(name)) {
                _fail(errc::unsupported_value, "'" + std::string(name) + "' is not the name of an element", path);
                return xml();
            }
            if (xml_scalar(ops->kind)) {
                auto t = text(ops, v, of, field, path);
                if (!t) {
                    return xml();
                }
                return xml(n, *t);
            }
            switch (ops->kind) {
                case ValueKind::optional:
                case ValueKind::pointer:
                    return ops->has_value(v) ? element(ops->inner(), ops->value(v), name, path, depth, of, field) : xml();
                case ValueKind::record:
                    break;
                default:
                    _unsupported(ops, path);
                    return xml();
            }
            field_list fields;
            ops->describe(const_cast<void*>(v), fields);
            xml::builder b(n);
            auto& list = FieldAccess::fields(fields);
            for (auto& f : list) {
                if ((f.flags & OmitEmpty) && f.ops->is_empty(f.address)) {
                    continue;
                }
                std::string at = path + "/" + std::string(f.name);
                if (f.flags & (Attribute | Text)) {
                    const ValueOps* ops_f = f.ops;
                    const void* value = f.address;
                    if (ops_f->kind == ValueKind::optional || ops_f->kind == ValueKind::pointer) {
                        if (!ops_f->has_value(value)) {
                            continue;
                        }
                        value = ops_f->value(value);
                        ops_f = ops_f->inner();
                    }
                    if (f.flags & Attribute) {
                        at = path + "/@" + std::string(f.name);
                    }
                    if (!xml_scalar(ops_f->kind)) {
                        _fail(errc::unsupported_value, std::string(f.flags & Attribute ? "an attribute" : "the text of an element") + " holds text, not " + ops_f->name, at);
                        return xml();
                    }
                    auto t = text(ops_f, value, &fields, &f, at);
                    if (!t) {
                        return xml();
                    }
                    if (f.flags & Attribute) {
                        if (!xml_qname(f.name)) {
                            _fail(errc::unsupported_value, "'" + std::string(f.name) + "' is not the name of an attribute", at);
                            return xml();
                        }
                        b.set(string(f.name), *t);
                    } else if (!t->empty()) {
                        b.push_back(xml::text_node(*t));
                    }
                    continue;
                }
                if (!children(b, f, &fields, at, depth)) {
                    return xml();
                }
            }
            return b.build();
        }

        // The elements of a field: one, none (null), or one a value of a
        // sequence
        bool children(xml::builder& b, const FieldInfo& f, const field_list* fields, const std::string& path, uint32_t depth) {
            auto ops = f.ops;
            switch (ops->kind) {
                case ValueKind::sequence:
                case ValueKind::set:
                case ValueKind::fixed: {
                    struct Ctx {
                        XmlMapper* self;
                        xml::builder* b;
                        const FieldInfo* f;
                        const field_list* fields;
                        const ValueOps* inner;
                        const std::string* path;
                        uint32_t depth;
                        size_t index;
                    } ctx{this, &b, &f, fields, ops->inner(), &path, depth, 0};
                    return ops->for_each(f.address, &ctx, [](void* c, const void* e) {
                        auto& x = *static_cast<Ctx*>(c);
                        std::string at = *x.path + "[" + std::to_string(++x.index) + "]";
                        if (x.inner->kind == ValueKind::sequence || x.inner->kind == ValueKind::set || x.inner->kind == ValueKind::fixed) {
                            x.self->_fail(errc::unsupported_value, "a list of lists has no form in XML", at);
                            return false;
                        }
                        auto n = x.self->element(x.inner, e, x.f->name, at, x.depth + 1, x.fields, x.f);
                        if (x.self->failure) {
                            return false;
                        }
                        if (n.exists()) {
                            x.b->push_back(n);
                        }
                        return true;
                    });
                }
                default: {
                    auto n = element(ops, f.address, f.name, path, depth + 1, fields, &f);
                    if (failure) {
                        return false;
                    }
                    if (n.exists()) {
                        b.push_back(n);
                    }
                    return true;
                }
            }
        }

        // The text of a value of a scalar kind
        optional<string> text(const ValueOps* ops, const void* v, const field_list* fields, const FieldInfo* f, const std::string& path) {
            char buf[64];
            switch (ops->kind) {
                case ValueKind::boolean:
                    return string(ops->get_bool(v) ? "true" : "false");
                case ValueKind::floating: {
                    double d = ops->get_double(v);
                    if (std::isnan(d)) {
                        return string("NaN");
                    }
                    if (std::isinf(d)) {
                        return string(d > 0 ? "INF" : "-INF");
                    }
                    return string(std::string_view(buf, ops->number_text(v, buf)));
                }
                case ValueKind::signed_integer:
                case ValueKind::unsigned_integer:
                    return string(std::string_view(buf, ops->number_text(v, buf)));
                case ValueKind::enumeration:
                    if (fields && f && f->names_count) {
                        int64_t i = ops->get_int(v);
                        if (i < 0 || uint64_t(i) >= f->names_count) {
                            _fail(errc::unsupported_value, "the value " + std::to_string(i) + " has no name among the field's names()", path);
                            return nullopt;
                        }
                        return string(FieldAccess::name(*fields, *f, size_t(i)));
                    }
                    return string(std::string_view(buf, ops->number_text(v, buf)));
                case ValueKind::string:
                case ValueKind::text:
                    return ops->get_text(v);
                default:
                    _unsupported(ops, path);
                    return nullopt;
            }
        }

        // --- nodes into a value ---

        // The value from an element (or, for a scalar, from its text)
        bool from_element(const ValueOps* ops, void* v, const xml& node, const std::string& path, uint32_t depth,
                          const field_list* of = nullptr, const FieldInfo* field = nullptr) {
            if (depth > MaxDepth) {
                _fail(errc::depth_limit, "nested deeper than 512 elements", path);
                return false;
            }
            if (xml_scalar(ops->kind)) {
                if (_has_element_children(node)) {
                    _fail(errc::type_mismatch, std::string("expected ") + ops->name + ", found elements", path);
                    return false;
                }
                return from_text(ops, v, node.text().view(), of, field, path);
            }
            switch (ops->kind) {
                case ValueKind::optional:
                case ValueKind::pointer:
                    return from_element(ops->inner(), ops->emplace(v), node, path, depth, of, field);
                case ValueKind::record:
                    break;
                default:
                    _unsupported_read(ops, path);
                    return false;
            }
            field_list fields;
            ops->describe(v, fields);
            auto& list = FieldAccess::fields(fields);
            for (auto& f : list) {
                if (f.flags & Attribute) {
                    std::string at = path + "/@" + std::string(f.name);
                    auto a = node.attribute(string(f.name));
                    if (!a) {
                        if (f.flags & Required) {
                            _fail(errc::missing_field, "the attribute " + std::string(f.name) + " is missing", at);
                            return false;
                        }
                        continue;
                    }
                    if (!_scalar_into(f.ops, f.address, a->view(), &fields, &f, at)) {
                        return false;
                    }
                    continue;
                }
                if (f.flags & Text) {
                    std::string own;
                    bool any = false;
                    for (auto& c : node.children()) {
                        if (c.is_text()) {
                            auto t = c.text();
                            own.append(t.data(), t.size());
                            any = true;
                        }
                    }
                    if (!any) {
                        if (f.flags & Required) {
                            _fail(errc::missing_field, "the element has no text", path + "/text()");
                            return false;
                        }
                        continue;
                    }
                    if (!_scalar_into(f.ops, f.address, own, &fields, &f, path + "/text()")) {
                        return false;
                    }
                    continue;
                }
                if (!_field_from(f, &fields, node, path, depth)) {
                    return false;
                }
            }
            return true;
        }

        // A value of a scalar kind from its text: numbers and booleans with
        // the white space around them left out (XML Schema collapses it),
        // strings as they are
        bool from_text(const ValueOps* ops, void* v, std::string_view t, const field_list* fields, const FieldInfo* f, const std::string& path) {
            switch (ops->kind) {
                case ValueKind::boolean: {
                    auto s = xml_trimmed(t);
                    if (s == "true" || s == "1") {
                        ops->set_bool(v, true);
                    } else if (s == "false" || s == "0") {
                        ops->set_bool(v, false);
                    } else {
                        _fail(errc::type_mismatch, "expected a boolean, found \"" + std::string(s.substr(0, 40)) + "\"", path);
                        return false;
                    }
                    return true;
                }
                case ValueKind::floating: {
                    auto s = xml_trimmed(t);
                    if (s == "NaN" || s == "INF" || s == "-INF" || s == "+INF") {
                        ops->set_double(v, s == "NaN" ? std::nan("") : s[0] == '-' ? -INFINITY : INFINITY);
                        return true;
                    }
                    return _literal(ops, v, s, path);
                }
                case ValueKind::signed_integer:
                case ValueKind::unsigned_integer:
                    return _literal(ops, v, xml_trimmed(t), path);
                case ValueKind::enumeration: {
                    auto s = xml_trimmed(t);
                    if (fields && f && f->names_count) {
                        size_t i = FieldAccess::index_of(*fields, *f, s);
                        if (i == f->names_count) {
                            _fail(errc::type_mismatch, "\"" + std::string(s.substr(0, 40)) + "\" is none of the names of the field", path);
                            return false;
                        }
                        if (!ops->set_int(v, int64_t(i))) {
                            _fail(errc::out_of_range, "the name's index is out of the enum's range", path);
                            return false;
                        }
                        return true;
                    }
                    return _literal(ops, v, s, path);
                }
                case ValueKind::string:
                    ops->set_text(v, t);
                    return true;
                case ValueKind::text:
                    if (!ops->set_text(v, t)) {
                        _fail(errc::type_mismatch, "\"" + std::string(t.substr(0, 40)) + "\" is not a value of the type (from_text refused it)", path);
                        return false;
                    }
                    return true;
                default:
                    _unsupported_read(ops, path);
                    return false;
            }
        }

    private:
        void _fail(errc code, const std::string& what, const std::string& path) {
            if (!failure) {
                failure = XmlMapFailure{code, what, path.empty() ? "/" : path};
            }
        }

        void _unsupported(const ValueOps* ops, const std::string& path) {
            _fail(errc::unsupported_value, std::string(ops->name) + " of this kind (a map, a tuple, a variant, json) has no form in XML", path);
        }

        void _unsupported_read(const ValueOps* ops, const std::string& path) {
            _fail(errc::type_mismatch, std::string(ops->name) + " of this kind (a map, a tuple, a variant, json) has no form in XML", path);
        }

        static bool _has_element_children(const xml& node) noexcept {
            for (auto& c : node.children()) {
                if (c.is_element()) {
                    return true;
                }
            }
            return false;
        }

        bool _literal(const ValueOps* ops, void* v, std::string_view s, const std::string& path) {
            int r = ops->set_literal(v, s);
            if (r == 1) {
                _fail(errc::type_mismatch, std::string("expected ") + ops->name + ", found \"" + std::string(s.substr(0, 40)) + "\"", path);
                return false;
            }
            if (r == 2) {
                _fail(errc::out_of_range, std::string(s.substr(0, 40)) + " is out of the range of the field's type", path);
                return false;
            }
            return true;
        }

        // An attribute's or the text's value into a scalar field, or an
        // optional or pointer to one
        bool _scalar_into(const ValueOps* ops, void* v, std::string_view t, const field_list* fields, const FieldInfo* f, const std::string& path) {
            if (ops->kind == ValueKind::optional || ops->kind == ValueKind::pointer) {
                return _scalar_into(ops->inner(), ops->emplace(v), t, fields, f, path);
            }
            if (!xml_scalar(ops->kind)) {
                _fail(errc::type_mismatch, std::string("an attribute or a text read into ") + ops->name, path);
                return false;
            }
            return from_text(ops, v, t, fields, f, path);
        }

        // A field that is elements: the children of that name
        bool _field_from(const FieldInfo& f, const field_list* fields, const xml& node, const std::string& path, uint32_t depth) {
            auto ops = f.ops;
            std::string at = path + "/" + std::string(f.name);
            string wanted(f.name);
            switch (ops->kind) {
                case ValueKind::sequence:
                case ValueKind::set: {
                    struct Ctx {
                        XmlMapper* self;
                        const xml* node;
                        const string* wanted;
                        const ValueOps* inner;
                        const std::string* at;
                        uint32_t depth;
                        size_t next;     // the child to look from
                        size_t index;    // the elements read
                        xml found;
                        const field_list* fields;
                        const FieldInfo* f;
                    } ctx{this, &node, &wanted, ops->inner(), &at, depth, 0, 0, xml(), fields, &f};
                    auto more = [](void* c) {
                        auto& x = *static_cast<Ctx*>(c);
                        auto kids = x.node->children();
                        for (; x.next < kids.size(); ++x.next) {
                            auto& k = kids[x.next];
                            if (k.is_element() && xml::_matches(k.name().view(), k.local_name(), k.namespace_uri(), x.wanted->view())) {
                                x.found = k;
                                ++x.next;
                                return true;
                            }
                        }
                        return false;
                    };
                    auto read = [](void* c, void* e) {
                        auto& x = *static_cast<Ctx*>(c);
                        std::string path_e = *x.at + "[" + std::to_string(++x.index) + "]";
                        return x.self->from_element(x.inner, e, x.found, path_e, x.depth + 1, x.fields, x.f);
                    };
                    bool ok = ops->read_elements(f.address, &ctx, more, read);
                    if (!ok && !failure) {
                        _fail(errc::type_mismatch, "the elements could not be read", at);
                    }
                    if (ok && ctx.index == 0 && (f.flags & Required)) {
                        _fail(errc::missing_field, "no element " + std::string(f.name), at);
                        return false;
                    }
                    return ok;
                }
                case ValueKind::fixed: {
                    size_t i = 0;
                    for (auto& k : node.children()) {
                        if (!k.is_element() || !xml::_matches(k.name().view(), k.local_name(), k.namespace_uri(), wanted.view())) {
                            continue;
                        }
                        if (i == ops->fixed_count) {
                            _fail(errc::type_mismatch, "more than " + std::to_string(ops->fixed_count) + " elements " + std::string(f.name), at);
                            return false;
                        }
                        const ValueOps* inner = nullptr;
                        void* e = ops->element(f.address, i, &inner);
                        ++i;
                        if (!from_element(inner, e, k, at + "[" + std::to_string(i) + "]", depth + 1, fields, &f)) {
                            return false;
                        }
                    }
                    if (i != ops->fixed_count && (i != 0 || (f.flags & Required))) {
                        _fail(i ? errc::type_mismatch : errc::missing_field,
                              std::to_string(i) + " elements " + std::string(f.name) + " where " + std::to_string(ops->fixed_count) + " are wanted", at);
                        return false;
                    }
                    return true;
                }
                default: {
                    xml k = node.child(wanted);
                    if (!k.exists()) {
                        if (f.flags & Required) {
                            _fail(errc::missing_field, "no element " + std::string(f.name), at);
                            return false;
                        }
                        return true;
                    }
                    return from_element(ops, f.address, k, at, depth + 1, fields, &f);
                }
            }
        }
    };

    inline xml::error xml_map_error(const XmlMapFailure& f, uint64_t offset) {
        xml::error e(f.code, offset, string(f.what));
        e.set_path(string(f.path));
        return e;
    }
}

namespace sgcl::encoding {
    template<class T>
    expected<T, xml::error> xml::_as(uint64_t offset) const {
        static_assert(std::is_default_constructible_v<T>, "encoding::xml: a type read from XML needs a default constructor");
        if (!exists()) {
            return unexpected<error>(error(errc::missing_field, offset, string("no element to read")));
        }
        T value{};
        detail::XmlMapper m;
        if (!m.from_element(detail::value_ops<T>(), &value, *this, "/" + std::string(name().view()), 0)) {
            return unexpected<error>(detail::xml_map_error(*m.failure, offset));
        }
        return value;
    }

    template<class T>
    expected<T, xml::error> xml::as() const {
        return _as<T>(0);
    }

    template<class T>
    expected<xml, xml::error> xml::from(const string& name, const T& value) {
        detail::XmlMapper m;
        auto n = m.element(detail::value_ops<T>(), &value, name.view(), "/" + std::string(name.view()), 0);
        if (m.failure) {
            return unexpected<error>(detail::xml_map_error(*m.failure, 0));
        }
        if (!n.exists()) {
            return unexpected<error>(error(errc::unsupported_value, 0, string("a null value has no element")));
        }
        return n;
    }

    template<class T>
    expected<string, xml::error> xml::stringify(const string& name, const T& value, const style& s) {
        auto n = from(name, value);
        if (!n) {
            return unexpected<error>(std::move(n.error()));
        }
        return n->to_string(s);
    }

    template<class T>
    expected<T, xml::error> xml::_typed(reader& r) {
        auto tree = _parse_with(r);
        if (!tree) {
            return unexpected<error>(std::move(tree.error()));
        }
        return tree->template _as<T>(r._node_offset);
    }

    // A document in memory: the error of the mapping has the line and the
    // column of the element it was found under the root of
    template<class T>
    expected<T, xml::error> xml::parse(const string& text) {
        return parse<T>(text, options());
    }

    template<class T>
    expected<T, xml::error> xml::parse(const io::reader& in) {
        return parse<T>(in, options());
    }

    template<class T>
    async::task<expected<T, xml::error>> xml::async_parse(const io::reader& in) {
        return async_parse<T>(in, options());
    }

    template<class T>
    expected<T, xml::error> xml::parse(const string& text, const options& o) {
        reader r(text, o);
        auto v = _typed<T>(r);
        if (!v && v.error().line() == 0) {
            v.error().locate(text);
        }
        return v;
    }

    template<class T>
    expected<T, xml::error> xml::parse(const io::reader& in, const options& o) {
        reader r(in, o);
        return _typed<T>(r);
    }

    template<class T>
    async::task<expected<T, xml::error>> xml::async_parse(io::reader in, options o) {
        reader r(std::move(in), o);
        xml root;
        uint64_t at = 0;
        while (auto n = co_await r.async_read()) {
            if (n->is_element()) {
                root = std::move(*n);
                at = r._node_offset;
            }
        }
        if (r.last_error()) {
            co_return unexpected<error>(*r.last_error());
        }
        co_return root.template _as<T>(at);
    }

    template<class T>
    optional<T> xml::reader::_typed_node(optional<xml> node) {
        if (!node) {
            return nullopt;
        }
        if (!node->is_element()) {
            _typed_error = error(errc::type_mismatch, _node_offset, string("expected an element, found text"));
            return nullopt;
        }
        auto v = node->template _as<T>(_node_offset);
        if (!v) {
            _typed_error = std::move(v.error());
            return nullopt;
        }
        return std::move(*v);
    }

    template<class T>
    optional<T> xml::reader::read() {
        if (last_error()) {
            return nullopt;
        }
        return _typed_node<T>(read());
    }

    template<class T>
    async::task<optional<T>> xml::reader::async_read() {
        if (last_error()) {
            co_return nullopt;
        }
        auto node = co_await async_read();
        co_return _typed_node<T>(std::move(node));
    }

    template<class T>
    xml::writer& xml::writer::value(const string& name, const T& v) {
        if (_core.failure) {
            return *this;
        }
        auto n = xml::from(name, v);
        if (!n) {
            _core.failure = std::move(n.error());
            return *this;
        }
        return node(*n);
    }
}
