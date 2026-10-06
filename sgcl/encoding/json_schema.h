//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "json.h"
#include "detail/files.h"
#include "detail/json_number.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/map.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/utf8.h"
#include "../core/vector.h"
#include "../txt/regex.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        struct JsonSchemaData;
        class JsonSchemaValidator;
    }

    // A JSON Schema of draft 2020-12, compiled: its resources, anchors and
    // references indexed and its patterns compiled once, then any number of
    // instances validated, from any number of threads (it never changes).
    // One word shared by copying. valid answers yes or no; validate gives
    // every failed keyword with where it failed in the instance and in the
    // schema, in the specification's "basic" output.
    class json_schema {
    public:
        using error = encoding::error;

        // A keyword an instance failed
        struct violation {
            string instance_location;   // a JSON Pointer into the instance: "/items/3/name"
            string keyword_location;    // the path through the schema, $refs included: "/properties/items/items/$ref/required"
            string absolute_location;   // the schema resource's URI and the keyword's pointer in it
            string message;
        };

        // What compile does
        struct options {
            bool format_assertion = false;   // "format" asserts (2020-12: an annotation unless asked)
            uint32_t max_depth = 512;        // schemas inside one another while validating, $refs counted
        };

        // The schema true: every instance valid
        json_schema() noexcept;

        // compile(schema).value(): a schema written in the program
        explicit json_schema(const json& schema);

        // The schema, and the other documents its $refs reach (each by its
        // $id). An error for what is no schema: a keyword of the wrong type,
        // a $ref no resource has, a pattern txt::regex refuses (a
        // backreference or a lookaround: unsupported_value), a $schema of
        // another draft
        static expected<json_schema, error> compile(const json& schema) noexcept;
        static expected<json_schema, error> compile(const json& schema, const options& o) noexcept;
        static expected<json_schema, error> compile(const json& schema, const vector<json>& resources) noexcept;
        static expected<json_schema, error> compile(const json& schema, const vector<json>& resources, const options& o) noexcept;

        // json::parse of the text, then compile
        static expected<json_schema, error> parse(const string& text) noexcept;

        // The schema of a file
        static expected<json_schema, error> load(const string& path);

        // Whether the instance is valid: the first failure ends the walk
        bool valid(const json& instance) const noexcept;

        // Every keyword the instance fails, in the schema's order; empty
        // when it is valid
        vector<violation> validate(const json& instance) const;

        // The root's $id resolved, or the base URI given to a schema
        // without one (https://schema.invalid/root.json)
        string id() const noexcept;

    private:
        friend class detail::JsonSchemaValidator;

        tracked_ptr<const detail::JsonSchemaData> _data;
    };

    namespace detail {
        // --- URIs (RFC 3986 §5.2) ---

        struct UriParts {
            std::string scheme, authority, path, query, fragment;
            bool has_scheme = false, has_authority = false, has_query = false, has_fragment = false;
        };

        inline UriParts uri_split(std::string_view s) {
            UriParts u;
            size_t i = 0;
            size_t colon = s.find(':');
            size_t stop = s.find_first_of("/?#");
            if (colon != std::string_view::npos && colon > 0 && (stop == std::string_view::npos || colon < stop)) {
                bool ok = (s[0] | 32) >= 'a' && (s[0] | 32) <= 'z';
                for (size_t k = 1; k < colon && ok; ++k) {
                    char c = s[k];
                    ok = ((c | 32) >= 'a' && (c | 32) <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
                }
                if (ok) {
                    u.scheme.assign(s.substr(0, colon));
                    u.has_scheme = true;
                    i = colon + 1;
                }
            }
            if (s.substr(i, 2) == "//") {
                size_t end = s.find_first_of("/?#", i + 2);
                if (end == std::string_view::npos) {
                    end = s.size();
                }
                u.authority.assign(s.substr(i + 2, end - i - 2));
                u.has_authority = true;
                i = end;
            }
            size_t end = s.find_first_of("?#", i);
            if (end == std::string_view::npos) {
                end = s.size();
            }
            u.path.assign(s.substr(i, end - i));
            i = end;
            if (i < s.size() && s[i] == '?') {
                end = s.find('#', i);
                if (end == std::string_view::npos) {
                    end = s.size();
                }
                u.query.assign(s.substr(i + 1, end - i - 1));
                u.has_query = true;
                i = end;
            }
            if (i < s.size() && s[i] == '#') {
                u.fragment.assign(s.substr(i + 1));
                u.has_fragment = true;
            }
            return u;
        }

        inline std::string uri_remove_dots(std::string_view in) {
            std::string out;
            std::string s(in);
            while (!s.empty()) {
                if (s.compare(0, 3, "../") == 0) {
                    s.erase(0, 3);
                } else if (s.compare(0, 2, "./") == 0) {
                    s.erase(0, 2);
                } else if (s.compare(0, 3, "/./") == 0) {
                    s.replace(0, 3, "/");
                } else if (s == "/.") {
                    s = "/";
                } else if (s.compare(0, 4, "/../") == 0 || s == "/..") {
                    s = s.size() == 3 ? std::string("/") : s.substr(3);
                    size_t last = out.rfind('/');
                    out.erase(last == std::string::npos ? 0 : last);
                } else if (s == "." || s == "..") {
                    s.clear();
                } else {
                    size_t start = s[0] == '/' ? 1 : 0;
                    size_t next = s.find('/', start);
                    if (next == std::string::npos) {
                        next = s.size();
                    }
                    out.append(s, 0, next);
                    s.erase(0, next);
                }
            }
            return out;
        }

        inline std::string uri_join(const UriParts& u) {
            std::string r;
            if (u.has_scheme) {
                r += u.scheme + ':';
            }
            if (u.has_authority) {
                r += "//" + u.authority;
            }
            r += u.path;
            if (u.has_query) {
                r += '?' + u.query;
            }
            if (u.has_fragment) {
                r += '#' + u.fragment;
            }
            return r;
        }

        // The reference resolved against the base
        inline std::string uri_resolve(std::string_view base, std::string_view ref) {
            UriParts b = uri_split(base), r = uri_split(ref), t;
            if (r.has_scheme) {
                t = r;
                t.path = uri_remove_dots(r.path);
            } else {
                if (r.has_authority) {
                    t.authority = r.authority;
                    t.has_authority = true;
                    t.path = uri_remove_dots(r.path);
                    t.query = r.query;
                    t.has_query = r.has_query;
                } else {
                    if (r.path.empty()) {
                        t.path = b.path;
                        t.query = r.has_query ? r.query : b.query;
                        t.has_query = r.has_query || b.has_query;
                    } else {
                        if (r.path[0] == '/') {
                            t.path = uri_remove_dots(r.path);
                        } else {
                            std::string merged;
                            if (b.has_authority && b.path.empty()) {
                                merged = "/" + r.path;
                            } else {
                                size_t last = b.path.rfind('/');
                                merged = (last == std::string::npos ? std::string() : b.path.substr(0, last + 1)) + r.path;
                            }
                            t.path = uri_remove_dots(merged);
                        }
                        t.query = r.query;
                        t.has_query = r.has_query;
                    }
                    t.authority = b.authority;
                    t.has_authority = b.has_authority;
                }
                t.scheme = b.scheme;
                t.has_scheme = b.has_scheme;
            }
            t.fragment = r.fragment;
            t.has_fragment = r.has_fragment;
            return uri_join(t);
        }

        inline std::string uri_without_fragment(std::string_view u) {
            size_t h = u.find('#');
            return std::string(h == std::string_view::npos ? u : u.substr(0, h));
        }

        // %xx in a fragment read
        inline std::string uri_unpercent(std::string_view s) {
            std::string out;
            for (size_t i = 0; i < s.size(); ++i) {
                auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; };
                if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
                    out += char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
                    i += 2;
                } else {
                    out += s[i];
                }
            }
            return out;
        }

        inline std::string pointer_token(std::string_view key) {
            std::string out;
            for (char c : key) {
                if (c == '~') {
                    out += "~0";
                } else if (c == '/') {
                    out += "~1";
                } else {
                    out += c;
                }
            }
            return out;
        }

        struct SchemaPlace {
            json node;
            string base;       // the base URI in force at the node
            string resource;   // the URI of the resource holding it (no fragment)
            string pointer;    // the node's pointer inside that resource
        };

        // A schema object's keywords, found once at compile: by index, no
        // lookup of a name while validating
        enum SchemaKw : uint8_t {
            K_d_dynamicRef,
            K_d_id,
            K_d_ref,
            K_additionalProperties,
            K_allOf,
            K_anyOf,
            K_const,
            K_contains,
            K_dependentRequired,
            K_dependentSchemas,
            K_else,
            K_enum,
            K_exclusiveMaximum,
            K_exclusiveMinimum,
            K_format,
            K_if,
            K_items,
            K_maxContains,
            K_maxItems,
            K_maxLength,
            K_maxProperties,
            K_maximum,
            K_minContains,
            K_minItems,
            K_minLength,
            K_minProperties,
            K_minimum,
            K_multipleOf,
            K_not,
            K_oneOf,
            K_pattern,
            K_patternProperties,
            K_prefixItems,
            K_properties,
            K_propertyNames,
            K_required,
            K_then,
            K_type,
            K_unevaluatedItems,
            K_unevaluatedProperties,
            K_uniqueItems,
            K_count
        };

        inline constexpr const char* SchemaKwNames[] = {"$dynamicRef", "$id", "$ref", "additionalProperties", "allOf", "anyOf", "const", "contains", "dependentRequired", "dependentSchemas", "else", "enum", "exclusiveMaximum", "exclusiveMinimum", "format", "if", "items", "maxContains", "maxItems", "maxLength", "maxProperties", "maximum", "minContains", "minItems", "minLength", "minProperties", "minimum", "multipleOf", "not", "oneOf", "pattern", "patternProperties", "prefixItems", "properties", "propertyNames", "required", "then", "type", "unevaluatedItems", "unevaluatedProperties", "uniqueItems"};

        struct SchemaNode {
            uint64_t has = 0;
            json kw[K_count];
            optional<txt::regex> pattern;   // the compiled "pattern", when there is one

            SGCL_INLINE_HOT bool contains(SchemaKw k) const noexcept {
                return has >> k & 1;
            }
        };

        struct JsonSchemaData {
            json root;
            string base;
            json_schema::options o;
            bool annotations = false;            // an unevaluated* keyword somewhere: annotations tracked
            map<string, SchemaPlace> resources;  // a resource's URI: its root
            map<string, SchemaPlace> anchors;    // "uri#name": $anchor and $dynamicAnchor
            map<string, bool> dynamic;           // "uri#name" of a $dynamicAnchor
            map<string, txt::regex> regexes;     // every pattern and patternProperties key
            // the target of each $ref, by its text's characters (one per $ref of the
            // schema) and the base it resolves against
            struct RefTarget {
                std::string base;
                size_t index;
            };
            std::unordered_map<const char*, RefTarget> ref_cache;
            vector<SchemaPlace> ref_places;
            // every schema object's keywords, by its members' address
            std::unordered_map<const void*, uint32_t> node_index;
            vector<SchemaNode> nodes;   // nodes[0]: an object of no keywords
        };

        inline const char* const SchemaDraft = "https://json-schema.org/draft/2020-12/schema";

        // --- the formats (format_assertion) ---

        inline bool fmt_digits(std::string_view s, size_t at, size_t n) noexcept {
            if (at + n > s.size()) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                if (s[at + i] < '0' || s[at + i] > '9') {
                    return false;
                }
            }
            return true;
        }

        inline int fmt_num(std::string_view s, size_t at, size_t n) noexcept {
            int v = 0;
            for (size_t i = 0; i < n; ++i) {
                v = v * 10 + (s[at + i] - '0');
            }
            return v;
        }

        inline bool fmt_date(std::string_view s) noexcept {
            if (s.size() != 10 || !fmt_digits(s, 0, 4) || s[4] != '-' || !fmt_digits(s, 5, 2) || s[7] != '-' || !fmt_digits(s, 8, 2)) {
                return false;
            }
            int y = fmt_num(s, 0, 4), m = fmt_num(s, 5, 2), d = fmt_num(s, 8, 2);
            static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
            return m >= 1 && m <= 12 && d >= 1 && d <= days[m - 1] + (m == 2 && leap);
        }

        // RFC 3339's full-time: HH:MM:SS[.frac](Z|±HH:MM); a leap second only at 23:59:60 UTC
        inline bool fmt_time(std::string_view s) noexcept {
            if (s.size() < 9 || !fmt_digits(s, 0, 2) || s[2] != ':' || !fmt_digits(s, 3, 2) || s[5] != ':' || !fmt_digits(s, 6, 2)) {
                return false;
            }
            int h = fmt_num(s, 0, 2), m = fmt_num(s, 3, 2), sec = fmt_num(s, 6, 2);
            size_t i = 8;
            if (i < s.size() && s[i] == '.') {
                size_t f = ++i;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    ++i;
                }
                if (i == f) {
                    return false;
                }
            }
            if (i >= s.size()) {
                return false;
            }
            int off = 0;
            if ((s[i] | 32) == 'z') {
                ++i;
            } else if ((s[i] == '+' || s[i] == '-') && fmt_digits(s, i + 1, 2) && i + 6 == s.size() && s[i + 3] == ':' && fmt_digits(s, i + 4, 2)) {
                int oh = fmt_num(s, i + 1, 2), om = fmt_num(s, i + 4, 2);
                if (oh > 23 || om > 59) {
                    return false;
                }
                off = (s[i] == '-' ? -1 : 1) * (oh * 60 + om);
                i += 6;
            } else {
                return false;
            }
            if (i != s.size() || h > 23 || m > 59 || sec > 60) {
                return false;
            }
            if (sec == 60) {
                int utc = ((h * 60 + m - off) % 1440 + 1440) % 1440;
                return utc == 23 * 60 + 59;
            }
            return true;
        }

        inline bool fmt_datetime(std::string_view s) noexcept {
            return s.size() > 11 && (s[10] == 'T' || s[10] == 't') && fmt_date(s.substr(0, 10)) && fmt_time(s.substr(11));
        }

        // ISO 8601 durations as RFC 3339 appendix A writes them
        inline bool fmt_duration(std::string_view s) noexcept {
            if (s.size() < 3 || s[0] != 'P') {
                return false;
            }
            size_t i = 1;
            auto num = [&]() {
                size_t f = i;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    ++i;
                }
                return i > f;
            };
            if (s[i] != 'T') {
                // weeks alone, or Y M D in order
                const char order[] = {'Y', 'M', 'D'};
                int next = 0;
                bool any = false;
                while (i < s.size() && s[i] != 'T') {
                    if (!num() || i >= s.size()) {
                        return false;
                    }
                    if (s[i] == 'W') {
                        return !any && i + 1 == s.size();
                    }
                    int k = -1;
                    for (int j = next; j < 3; ++j) {
                        if (s[i] == order[j]) {
                            k = j;
                        }
                    }
                    if (k < 0) {
                        return false;
                    }
                    next = k + 1;
                    any = true;
                    ++i;
                }
                if (i == s.size()) {
                    return any;
                }
            }
            ++i;   // T
            const char order[] = {'H', 'M', 'S'};
            int next = 0;
            bool any = false;
            while (i < s.size()) {
                if (!num() || i >= s.size()) {
                    return false;
                }
                int k = -1;
                for (int j = next; j < 3; ++j) {
                    if (s[i] == order[j]) {
                        k = j;
                    }
                }
                if (k < 0) {
                    return false;
                }
                next = k + 1;
                any = true;
                ++i;
            }
            return any;
        }

        inline bool fmt_ipv4(std::string_view s) noexcept {
            int parts = 0;
            size_t i = 0;
            while (parts < 4) {
                size_t f = i;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9' && i - f < 3) {
                    ++i;
                }
                if (i == f || (i - f > 1 && s[f] == '0') || fmt_num(s, f, i - f) > 255) {
                    return false;
                }
                ++parts;
                if (parts < 4) {
                    if (i >= s.size() || s[i] != '.') {
                        return false;
                    }
                    ++i;
                }
            }
            return i == s.size();
        }

        inline bool fmt_ipv6(std::string_view s) noexcept {
            if (s.empty()) {
                return false;
            }
            int groups = 0;
            bool gap = false;
            size_t i = 0;
            if (s.substr(0, 2) == "::") {
                gap = true;
                i = 2;
                if (i == s.size()) {
                    return true;
                }
            } else if (s[0] == ':') {
                return false;
            }
            while (i < s.size()) {
                size_t f = i;
                while (i < s.size() && std::isxdigit(uint8_t(s[i])) && i - f < 4) {
                    ++i;
                }
                if (i < s.size() && s[i] == '.') {
                    // an IPv4 address in the last 32 bits
                    if (!fmt_ipv4(s.substr(f))) {
                        return false;
                    }
                    groups += 2;
                    i = s.size();
                    break;
                }
                if (i == f) {
                    return false;
                }
                ++groups;
                if (i == s.size()) {
                    break;
                }
                if (s[i] != ':') {
                    return false;
                }
                ++i;
                if (i < s.size() && s[i] == ':') {
                    if (gap) {
                        return false;
                    }
                    gap = true;
                    ++i;
                    if (i == s.size()) {
                        break;
                    }
                } else if (i == s.size()) {
                    return false;
                }
            }
            return gap ? groups < 8 : groups == 8;
        }

        inline bool fmt_hostname(std::string_view s) noexcept {
            if (s.empty() || s.size() > 253) {
                return false;
            }
            size_t i = 0;
            while (i <= s.size()) {
                size_t end = s.find('.', i);
                if (end == std::string_view::npos) {
                    end = s.size();
                }
                std::string_view label = s.substr(i, end - i);
                if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') {
                    return false;
                }
                for (char c : label) {
                    if (!(std::isalnum(uint8_t(c)) || c == '-')) {
                        return false;
                    }
                }
                i = end + 1;
                if (end == s.size()) {
                    break;
                }
            }
            return true;
        }

        inline bool fmt_email(std::string_view s) noexcept {
            size_t at = s.rfind('@');
            if (at == std::string_view::npos || at == 0 || at + 1 >= s.size()) {
                return false;
            }
            std::string_view local = s.substr(0, at), domain = s.substr(at + 1);
            if (local.front() == '.' || local.back() == '.' || local.find("..") != std::string_view::npos) {
                return false;
            }
            for (char c : local) {
                if (!(std::isalnum(uint8_t(c)) || std::string_view("!#$%&'*+/=?^_`{|}~-.").find(c) != std::string_view::npos)) {
                    return false;
                }
            }
            if (domain.front() == '[' && domain.back() == ']') {
                std::string_view lit = domain.substr(1, domain.size() - 2);
                return lit.substr(0, 5) == "IPv6:" ? fmt_ipv6(lit.substr(5)) : fmt_ipv4(lit);
            }
            return fmt_hostname(domain);
        }

        inline bool fmt_uri_chars(std::string_view s) noexcept {
            for (size_t i = 0; i < s.size(); ++i) {
                auto c = uint8_t(s[i]);
                if (c == '%') {
                    if (i + 2 >= s.size() || !std::isxdigit(uint8_t(s[i + 1])) || !std::isxdigit(uint8_t(s[i + 2]))) {
                        return false;
                    }
                    continue;
                }
                if (c <= 0x20 || c >= 0x7F || std::string_view("\"<>\\^`{|}").find(char(c)) != std::string_view::npos) {
                    return false;
                }
            }
            return true;
        }

        inline bool fmt_uri_reference(std::string_view s) noexcept {
            if (!fmt_uri_chars(s)) {
                return false;
            }
            UriParts u = uri_split(s);
            return u.fragment.find('#') == std::string::npos;
        }

        inline bool fmt_uri(std::string_view s) noexcept {
            return fmt_uri_reference(s) && uri_split(s).has_scheme;
        }

        inline bool fmt_uuid(std::string_view s) noexcept {
            if (s.size() != 36) {
                return false;
            }
            for (size_t i = 0; i < 36; ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23 ? s[i] != '-' : !std::isxdigit(uint8_t(s[i]))) {
                    return false;
                }
            }
            return true;
        }

        inline bool fmt_json_pointer(std::string_view s) noexcept {
            if (!s.empty() && s[0] != '/') {
                return false;
            }
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '~' && (i + 1 >= s.size() || (s[i + 1] != '0' && s[i + 1] != '1'))) {
                    return false;
                }
            }
            return true;
        }

        inline bool fmt_relative_json_pointer(std::string_view s) noexcept {
            size_t i = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                ++i;
            }
            if (i == 0 || (i > 1 && s[0] == '0')) {
                return false;
            }
            return (i + 1 == s.size() && s[i] == '#') || fmt_json_pointer(s.substr(i));
        }

        // An unknown format passes: formats are open (2020-12 §7.2.3)
        inline bool fmt_check(std::string_view format, std::string_view s) noexcept {
            if (format == "date-time") return fmt_datetime(s);
            if (format == "date") return fmt_date(s);
            if (format == "time") return fmt_time(s);
            if (format == "duration") return fmt_duration(s);
            if (format == "email" || format == "idn-email") return fmt_email(s);
            if (format == "hostname" || format == "idn-hostname") return fmt_hostname(s);
            if (format == "ipv4") return fmt_ipv4(s);
            if (format == "ipv6") return fmt_ipv6(s);
            if (format == "uri" || format == "iri") return fmt_uri(s);
            if (format == "uri-reference" || format == "iri-reference") return fmt_uri_reference(s);
            if (format == "uuid") return fmt_uuid(s);
            if (format == "json-pointer") return fmt_json_pointer(s);
            if (format == "relative-json-pointer") return fmt_relative_json_pointer(s);
            if (format == "regex") return bool(txt::regex::compile(string(std::string(s))));
            return true;
        }

        // --- numbers ---

        // A number's decimal digits and exponent: value = digits * 10^exp
        inline bool schema_decimal(const json& n, __int128& digits, int& exp) noexcept {
            std::string text;
            if (auto t = n.number_text()) {
                text.assign(t->view());
            } else if (auto i = n.as_int()) {
                text = std::to_string(*i);
            } else if (auto u = n.as_uint()) {
                text = std::to_string(*u);
            } else {
                char b[64];
                auto r = std::to_chars(b, b + sizeof b, n.as_double(0.0));
                text.assign(b, r.ptr);
            }
            bool neg = !text.empty() && text[0] == '-';
            size_t i = neg ? 1 : 0;
            digits = 0;
            exp = 0;
            int ndig = 0;
            bool frac = false;
            for (; i < text.size(); ++i) {
                char c = text[i];
                if (c == '.') {
                    frac = true;
                } else if (c >= '0' && c <= '9') {
                    if (ndig > 36) {
                        return false;
                    }
                    digits = digits * 10 + (c - '0');
                    if (digits != 0) {
                        ++ndig;
                    }
                    if (frac) {
                        --exp;
                    }
                } else if (c == 'e' || c == 'E') {
                    int e = 0;
                    auto r = std::from_chars(text.data() + i + 1 + (text[i + 1] == '+'), text.data() + text.size(), e);
                    if (r.ec != std::errc()) {
                        return false;
                    }
                    exp += e;
                    break;
                } else {
                    return false;
                }
            }
            if (neg) {
                digits = -digits;
            }
            return true;
        }

        // Whether a is a multiple of b: exactly by the decimal digits when
        // they fit 128 bits, else by the doubles
        inline bool schema_multiple(const json& a, const json& b) noexcept {
            __int128 da, db;
            int ea, eb;
            if (schema_decimal(a, da, ea) && schema_decimal(b, db, eb) && db != 0) {
                if (da == 0) {
                    return true;
                }
                // scale to the smaller exponent
                bool fits = true;
                auto scale = [&](__int128& d, int by) {
                    for (int k = 0; k < by; ++k) {
                        if (d > (__int128(1) << 120) / 10 || d < -((__int128(1) << 120) / 10)) {
                            fits = false;
                            return;
                        }
                        d *= 10;
                    }
                };
                if (ea > eb) {
                    if (ea - eb > 40) {
                        fits = false;
                    } else {
                        scale(da, ea - eb);
                    }
                } else if (eb > ea) {
                    if (eb - ea > 40) {
                        // b is far larger than a: a multiple only of 0
                        return false;
                    }
                    scale(db, eb - ea);
                }
                if (fits) {
                    return da % db == 0;
                }
            }
            double x = a.as_double(0.0), m = b.as_double(0.0);
            if (m == 0) {
                return false;
            }
            double q = x / m;
            if (!std::isfinite(q)) {
                return false;
            }
            return std::fabs(q - std::nearbyint(q)) <= 1e-9 * std::max(1.0, std::fabs(q));
        }

        // A number of no fraction: "integer" (1.0 is one)
        inline bool schema_integer(const json& n) noexcept {
            if (!n.is_number()) {
                return false;
            }
            if (n.as_int() || n.as_uint()) {
                return true;
            }
            __int128 d;
            int e;
            if (schema_decimal(n, d, e)) {
                if (e >= 0) {
                    return true;
                }
                for (; e < 0; ++e) {
                    if (d % 10 != 0) {
                        return false;
                    }
                    d /= 10;
                }
                return true;
            }
            double v = n.as_double(0.5);
            return std::isfinite(v) && std::trunc(v) == v;
        }

        inline int schema_compare(const json& a, const json& b) noexcept {
            __int128 da, db;
            int ea, eb;
            if (schema_decimal(a, da, ea) && schema_decimal(b, db, eb) && std::abs(ea - eb) <= 30) {
                // the same exponent, when it fits
                bool ok = true;
                auto up = [&](__int128& d, int by) {
                    for (int k = 0; k < by; ++k) {
                        if (d > (__int128(1) << 120) / 10 || d < -((__int128(1) << 120) / 10)) {
                            ok = false;
                            return;
                        }
                        d *= 10;
                    }
                };
                if (ea > eb) {
                    up(da, ea - eb);
                } else {
                    up(db, eb - ea);
                }
                if (ok) {
                    return da < db ? -1 : da > db ? 1 : 0;
                }
            }
            double x = a.as_double(0.0), y = b.as_double(0.0);
            return x < y ? -1 : x > y ? 1 : 0;
        }

        // --- compiling ---

        class JsonSchemaCompiler {
        public:
            JsonSchemaCompiler(JsonSchemaData& d) noexcept
            : _d(d) {
            }

            bool add_document(const json& doc, const std::string& base, error& e) {
                if (!_scan(doc, base, base, "", 0, e)) {
                    return false;
                }
                return true;
            }

            // Every $ref and $dynamicRef resolves
            bool check_refs(error& e);

        private:
            JsonSchemaData& _d;
            std::vector<std::pair<std::string, std::string>> _refs;   // (resolved URI, where)
            struct RefNode {
                const char* text;
                std::string base;
                std::string uri;
            };
            std::vector<RefNode> _ref_nodes;   // text: the $ref's characters in the schema, which the schema keeps

            bool _fail(errc code, const std::string& where, const std::string& why, error& e) {
                e = error(code, 0, string(why + (where.empty() ? std::string() : " (at " + where + ")")));
                ErrorAccess::without_place(e);
                return false;
            }

            static bool _is_schema(const json& j) noexcept {
                return j.is_object() || j.is_bool();
            }

            bool _scan(const json& s, std::string base, const std::string& resource_in, const std::string& pointer, uint32_t depth, error& e) {
                if (depth > 10000) {
                    return _fail(errc::depth_limit, pointer, "a schema nested deeper than 10000", e);
                }
                if (s.is_bool()) {
                    if (pointer.empty()) {
                        _d.resources.insert_or_assign(string(resource_in), SchemaPlace{s, string(base), string(resource_in), string()});
                    }
                    return true;
                }
                if (!s.is_object()) {
                    return _fail(errc::type_mismatch, pointer, "a schema that is no object and no boolean", e);
                }
                _index_node(s);
                std::string resource = resource_in;
                std::string ptr = pointer;
                if (s.contains("$schema")) {
                    auto v = s["$schema"].as_string();
                    if (!v) {
                        return _fail(errc::type_mismatch, pointer, "$schema that is no string", e);
                    }
                    std::string u(v->view());
                    if (uri_without_fragment(u) != SchemaDraft || (u.find('#') != std::string::npos && u.back() != '#')) {
                        return _fail(errc::unsupported_value, pointer, "$schema of another draft than 2020-12: " + u, e);
                    }
                }
                if (s.contains("$id")) {
                    auto v = s["$id"].as_string();
                    if (!v) {
                        return _fail(errc::type_mismatch, pointer, "$id that is no string", e);
                    }
                    std::string id = uri_resolve(base, v->view());
                    size_t hash = id.find('#');
                    if (hash != std::string::npos) {
                        if (hash + 1 != id.size()) {
                            return _fail(errc::syntax, pointer, "$id with a fragment", e);
                        }
                        id.erase(hash);
                    }
                    base = id;
                    resource = id;
                    ptr.clear();
                }
                if (!_d.resources.contains(string(resource)) || ptr.empty()) {
                    if (ptr.empty()) {
                        _d.resources.insert_or_assign(string(resource), SchemaPlace{s, string(base), string(resource), string()});
                    }
                }
                for (const char* kw : {"$anchor", "$dynamicAnchor"}) {
                    if (s.contains(kw)) {
                        auto v = s[kw].as_string();
                        if (!v) {
                            return _fail(errc::type_mismatch, pointer, std::string(kw) + " that is no string", e);
                        }
                        std::string name(v->view());
                        bool ok = !name.empty() && (std::isalpha(uint8_t(name[0])) || name[0] == '_');
                        for (char c : name) {
                            ok = ok && (std::isalnum(uint8_t(c)) || c == '-' || c == '_' || c == '.');
                        }
                        if (!ok) {
                            return _fail(errc::syntax, pointer, std::string(kw) + " that is no plain name: " + name, e);
                        }
                        string key(base + "#" + name);
                        _d.anchors.insert_or_assign(key, SchemaPlace{s, string(base), string(resource), string(ptr)});
                        if (std::string_view(kw) == "$dynamicAnchor") {
                            _d.dynamic.insert_or_assign(key, true);
                        }
                    }
                }
                for (const char* kw : {"$ref", "$dynamicRef"}) {
                    if (s.contains(kw)) {
                        auto v = s[kw].as_string();
                        if (!v) {
                            return _fail(errc::type_mismatch, pointer, std::string(kw) + " that is no string", e);
                        }
                        _refs.push_back({uri_resolve(base, v->view()), pointer + "/" + kw});
                        if (std::string_view(kw) == "$ref") {
                            _ref_nodes.push_back({v->data(), base, _refs.back().first});
                        }
                    }
                }
                if (s.contains("unevaluatedProperties") || s.contains("unevaluatedItems")) {
                    _d.annotations = true;
                }
                for (const char* kw : {"pattern"}) {
                    if (s.contains(kw)) {
                        auto v = s[kw].as_string();
                        if (!v) {
                            return _fail(errc::type_mismatch, pointer, "pattern that is no string", e);
                        }
                        if (!_regex(*v, pointer + "/pattern", e)) {
                            return false;
                        }
                    }
                }
                // the type of each keyword's value, and the subschemas inside
                for (const auto& m : s.members()) {
                    std::string_view k = m.key.view();
                    const json& v = m.value;
                    std::string at = ptr + "/" + pointer_token(k);
                    std::string where = pointer + "/" + pointer_token(k);
                    if (k == "additionalProperties" || k == "propertyNames" || k == "items" || k == "contains" || k == "not" || k == "if" || k == "then" ||
                        k == "else" || k == "unevaluatedItems" || k == "unevaluatedProperties") {
                        if (!_is_schema(v)) {
                            return _fail(errc::type_mismatch, where, std::string(k) + " that is no schema", e);
                        }
                        if (!_scan_at(v, base, resource, at, where, depth, e)) {
                            return false;
                        }
                    } else if (k == "allOf" || k == "anyOf" || k == "oneOf" || k == "prefixItems") {
                        if (!v.is_array() || (v.empty() && k != "prefixItems")) {
                            return _fail(errc::type_mismatch, where, std::string(k) + " that is no non-empty array of schemas", e);
                        }
                        for (size_t i = 0; i < v.size(); ++i) {
                            if (!_is_schema(v[i])) {
                                return _fail(errc::type_mismatch, where, std::string(k) + " with an element that is no schema", e);
                            }
                            if (!_scan_at(v[i], base, resource, at + "/" + std::to_string(i), where + "/" + std::to_string(i), depth, e)) {
                                return false;
                            }
                        }
                    } else if (k == "properties" || k == "patternProperties" || k == "dependentSchemas" || k == "$defs" || k == "definitions") {
                        if (!v.is_object()) {
                            return _fail(errc::type_mismatch, where, std::string(k) + " that is no object", e);
                        }
                        for (const auto& p : v.members()) {
                            if (!_is_schema(p.value)) {
                                return _fail(errc::type_mismatch, where, std::string(k) + " with a member that is no schema", e);
                            }
                            if (k == "patternProperties" && !_regex(p.key, where, e)) {
                                return false;
                            }
                            std::string tok = "/" + pointer_token(p.key.view());
                            if (!_scan_at(p.value, base, resource, at + tok, where + tok, depth, e)) {
                                return false;
                            }
                        }
                    } else if (k == "type") {
                        auto ok_type = [](const json& t) {
                            auto n = t.as_string();
                            if (!n) {
                                return false;
                            }
                            std::string_view x = n->view();
                            return x == "null" || x == "boolean" || x == "object" || x == "array" || x == "number" || x == "string" || x == "integer";
                        };
                        bool ok = v.is_array() ? !v.empty() : ok_type(v);
                        if (v.is_array()) {
                            for (const auto& t : v.elements()) {
                                ok = ok && ok_type(t);
                            }
                        }
                        if (!ok) {
                            return _fail(errc::type_mismatch, where, "type that is none of the seven, or no array of them", e);
                        }
                    } else if (k == "enum") {
                        if (!v.is_array()) {
                            return _fail(errc::type_mismatch, where, "enum that is no array", e);
                        }
                    } else if (k == "multipleOf") {
                        if (!v.is_number() || v.as_double(0.0) <= 0) {
                            return _fail(errc::type_mismatch, where, "multipleOf that is no positive number", e);
                        }
                    } else if (k == "maximum" || k == "minimum" || k == "exclusiveMaximum" || k == "exclusiveMinimum") {
                        if (!v.is_number()) {
                            return _fail(errc::type_mismatch, where, std::string(k) + " that is no number", e);
                        }
                    } else if (k == "maxLength" || k == "minLength" || k == "maxItems" || k == "minItems" || k == "maxContains" || k == "minContains" ||
                               k == "maxProperties" || k == "minProperties") {
                        if (!schema_integer(v) || v.as_double(-1.0) < 0) {
                            return _fail(errc::type_mismatch, where, std::string(k) + " that is no non-negative integer", e);
                        }
                    } else if (k == "uniqueItems") {
                        if (!v.is_bool()) {
                            return _fail(errc::type_mismatch, where, "uniqueItems that is no boolean", e);
                        }
                    } else if (k == "required") {
                        bool ok = v.is_array();
                        if (ok) {
                            for (const auto& r : v.elements()) {
                                ok = ok && r.is_string();
                            }
                        }
                        if (!ok) {
                            return _fail(errc::type_mismatch, where, "required that is no array of strings", e);
                        }
                    } else if (k == "dependentRequired") {
                        bool ok = v.is_object();
                        if (ok) {
                            for (const auto& p : v.members()) {
                                ok = ok && p.value.is_array();
                                if (ok) {
                                    for (const auto& r : p.value.elements()) {
                                        ok = ok && r.is_string();
                                    }
                                }
                            }
                        }
                        if (!ok) {
                            return _fail(errc::type_mismatch, where, "dependentRequired that is no object of arrays of strings", e);
                        }
                    } else if (k == "format") {
                        if (!v.is_string()) {
                            return _fail(errc::type_mismatch, where, "format that is no string", e);
                        }
                    }
                }
                return true;
            }

            // A subschema at its pointer in the resource (where: in the document, for errors)
            bool _scan_at(const json& v, const std::string& base, const std::string& resource, const std::string& at, const std::string& where, uint32_t depth,
                          error& e) {
                (void)where;
                return _scan(v, base, resource, at, depth + 1, e);
            }

            void _index_node(const json& s) {
                if (_d.nodes.empty()) {
                    _d.nodes.push_back(SchemaNode{});
                }
                auto ms = s.members();
                if (ms.empty() || _d.node_index.count(ms.data())) {
                    return;
                }
                SchemaNode n;
                for (const auto& m : ms) {
                    for (uint8_t k = 0; k < K_count; ++k) {
                        if (m.key.view() == SchemaKwNames[k]) {
                            n.has |= uint64_t(1) << k;
                            n.kw[k] = m.value;
                            if (k == K_pattern && m.value.is_string()) {
                                if (auto r = txt::regex::compile(*m.value.as_string())) {
                                    n.pattern = *r;
                                }
                            }
                            break;
                        }
                    }
                }
                _d.nodes.push_back(n);
                _d.node_index[ms.data()] = uint32_t(_d.nodes.size() - 1);
            }

            bool _regex(const string& pattern, const std::string& where, error& e) {
                if (_d.regexes.contains(pattern)) {
                    return true;
                }
                auto r = txt::regex::compile(pattern);
                if (!r) {
                    return _fail(errc::unsupported_value, where, "a pattern txt::regex does not take: " + std::string(r.error().message().view()), e);
                }
                _d.regexes.insert_or_assign(pattern, *r);
                return true;
            }
        };

        // The node a URI names: a resource and a fragment (a JSON Pointer or
        // an anchor's name); nullptr for none. place gets its place
        inline bool schema_lookup(const JsonSchemaData& d, const std::string& uri, SchemaPlace& place) noexcept {
            std::string res = uri_without_fragment(uri);
            size_t hash = uri.find('#');
            std::string frag = hash == std::string::npos ? std::string() : uri_unpercent(std::string_view(uri).substr(hash + 1));
            if (!frag.empty() && frag[0] != '/') {
                auto a = d.anchors.find(string(res + "#" + frag));
                if (a == d.anchors.end()) {
                    return false;
                }
                place = a->second;
                return true;
            }
            auto r = d.resources.find(string(res));
            if (r == d.resources.end()) {
                return false;
            }
            place = r->second;
            if (frag.empty()) {
                return true;
            }
            auto node = place.node.at_path(string(frag));
            if (!node) {
                return false;
            }
            // the base at the node: the $ids on the way
            std::string base(place.base.view());
            json at = place.node;
            size_t i = 1;
            while (i <= frag.size()) {
                size_t end = frag.find('/', i);
                if (end == std::string::npos) {
                    end = frag.size();
                }
                std::string token = frag.substr(i, end - i);
                std::string key;
                for (size_t k = 0; k < token.size(); ++k) {
                    if (token[k] == '~' && k + 1 < token.size()) {
                        key += token[k + 1] == '1' ? '/' : '~';
                        ++k;
                    } else {
                        key += token[k];
                    }
                }
                if (at.is_object()) {
                    at = at[string(key)];
                } else if (at.is_array()) {
                    size_t idx = 0;
                    std::from_chars(key.data(), key.data() + key.size(), idx);
                    at = at[idx];
                }
                if (at.is_object() && at.contains("$id")) {
                    if (auto id = at["$id"].as_string()) {
                        base = uri_without_fragment(uri_resolve(base, id->view()));
                    }
                }
                i = end + 1;
            }
            place.node = *node;
            place.base = string(base);
            place.pointer = string(frag);
            return true;
        }

        inline bool JsonSchemaCompiler::check_refs(error& e) {
            for (auto& [uri, where] : _refs) {
                SchemaPlace p;
                if (!schema_lookup(_d, uri, p)) {
                    return _fail(errc::missing_field, where, "a reference no resource has: " + uri, e);
                }
            }
            for (auto& r : _ref_nodes) {
                SchemaPlace p;
                if (schema_lookup(_d, r.uri, p)) {
                    _d.ref_places.push_back(p);
                    _d.ref_cache[r.text] = JsonSchemaData::RefTarget{r.base, _d.ref_places.size() - 1};
                }
            }
            return true;
        }

        // --- validating ---

// the paths of a violation, made only when violations are collected
#define SGCL_JS_PATH(e) (_collect ? std::string(e) : std::string())

        class JsonSchemaValidator {
        public:
            JsonSchemaValidator(const JsonSchemaData& d, bool collect) noexcept
            : _d(d), _collect(collect) {
            }

            vector<json_schema::violation> errors;

            static json_schema make(tracked_ptr<JsonSchemaData> d) noexcept {
                json_schema s;
                s._data = std::move(d);
                return s;
            }

            // What an evaluation saw: valid or not, and what it evaluated
            // (for unevaluatedItems and unevaluatedProperties)
            struct Result {
                bool ok = true;
                std::unordered_set<std::string> props;
                size_t items = 0;          // the first items evaluated
                bool all_items = false;    // every item
                std::unordered_set<size_t> item_set;   // by contains

                void merge(const Result& r) {
                    props.insert(r.props.begin(), r.props.end());
                    items = std::max(items, r.items);
                    all_items = all_items || r.all_items;
                    item_set.insert(r.item_set.begin(), r.item_set.end());
                }
            };

            Result run(const json& instance) {
                SchemaPlace root;
                schema_lookup(_d, std::string(_d.base.view()), root);
                std::string ipath, kpath;
                return _eval(root.node, instance, std::string(root.base.view()), std::string(root.resource.view()), "", ipath, kpath, 0, true);
            }

        private:
            const JsonSchemaData& _d;
            bool _collect;
            std::vector<const std::string*> _scope;   // the dynamic scope: the resources entered, outermost first (strings of the frames)

            void _report(const std::string& ipath, const std::string& kpath, const std::string& resource, const std::string& sptr, const std::string& message) {
                if (_collect) {
                    errors.push_back(json_schema::violation{string(ipath), string(kpath), string(resource + "#" + sptr), string(message)});
                }
            }

            static std::string _text(const json& j) {
                std::string t(j.to_string().view());
                return t.size() > 80 ? t.substr(0, 77) + "..." : t;
            }

            static std::string _type_of(const json& j) {
                switch (j.type()) {
                    case json::kind::null: return "null";
                    case json::kind::boolean: return "boolean";
                    case json::kind::number: return schema_integer(j) ? "integer" : "number";
                    case json::kind::string: return "string";
                    case json::kind::array: return "array";
                    default: return "object";
                }
            }

            static bool _type_ok(const json& j, std::string_view t) {
                if (t == "integer") {
                    return schema_integer(j);
                }
                if (t == "number") {
                    return j.is_number();
                }
                return _type_of(j) == t || (t == "number" && j.is_number());
            }

            // own_base: base_in is the node's own (a place looked up), its $id already in it
            Result _eval(const json& s, const json& inst, const std::string& base_in, const std::string& resource, const std::string& sptr, const std::string& ipath,
                         const std::string& kpath, uint32_t depth, bool own_base = false) {
                Result res;
                if (s.is_bool()) {
                    if (!*s.as_bool()) {
                        res.ok = false;
                        _report(ipath, kpath, resource, sptr, "the schema false accepts nothing");
                    }
                    return res;
                }
                if (depth > _d.o.max_depth) {
                    res.ok = false;
                    _report(ipath, kpath, resource, sptr, "schemas nested deeper than max_depth (a $ref cycle that reads nothing?)");
                    return res;
                }
                const SchemaNode& nd = _node(s);
                // the base, the resource and the pointer: the caller's, or this node's own
                std::string own_base_text, empty;
                const std::string* base_p = &base_in;
                const std::string* res_p = &resource;
                const std::string* ptr_p = &sptr;
                bool pushed = false;
                if (nd.contains(K_d_id)) {
                    if (!own_base) {
                        own_base_text = uri_without_fragment(uri_resolve(base_in, nd.kw[K_d_id].as_string("").view()));
                        base_p = &own_base_text;
                    }
                    res_p = base_p;
                    ptr_p = &empty;
                }
                const std::string& base = *base_p;
                const std::string& res_uri = *res_p;
                const std::string& ptr = *ptr_p;
                if (_scope.empty() || *_scope.back() != res_uri) {
                    _scope.push_back(&res_uri);
                    pushed = true;
                }
                bool fast = !_collect && !_d.annotations;
                auto fail = [&](const std::string& kw, const std::string& message) {
                    res.ok = false;
                    _report(ipath, kpath + "/" + kw, res_uri, ptr + "/" + kw, message);
                };
                auto sub = [&](const json& schema, const json& instance, const std::string& kw_rel, const std::string& ipath2) {
                    return _eval(schema, instance, base, res_uri, SGCL_JS_PATH(ptr + "/" + kw_rel), ipath2, SGCL_JS_PATH(kpath + "/" + kw_rel), depth + 1);
                };
                auto done = [&]() -> bool { return !res.ok && !_collect; };
                // --- references ---
                if (nd.contains(K_d_ref)) {
                    string ref = nd.kw[K_d_ref].as_string("");
                    SchemaPlace p;
                    bool found = false;
                    std::string uri;
                    if (auto c = _d.ref_cache.find(ref.data()); c != _d.ref_cache.end() && c->second.base == base) {
                        p = _d.ref_places[c->second.index];
                        found = true;
                    } else {
                        uri = uri_resolve(base, ref.view());
                        found = schema_lookup(_d, uri, p);
                    }
                    if (found) {
                        Result r = _eval(p.node, inst, std::string(p.base.view()), std::string(p.resource.view()), std::string(p.pointer.view()), ipath,
                                         kpath + "/$ref", depth + 1, true);
                        if (r.ok) {
                            res.merge(r);
                        } else {
                            res.ok = false;
                        }
                    } else {
                        fail("$ref", "a reference no resource has: " + uri);
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                if (nd.contains(K_d_dynamicRef)) {
                    std::string uri = uri_resolve(base, nd.kw[K_d_dynamicRef].as_string("").view());
                    SchemaPlace p;
                    if (schema_lookup(_d, uri, p)) {
                        // the target names a $dynamicAnchor of the fragment: the
                        // outermost resource of the scope with one of the name wins
                        size_t hash = uri.find('#');
                        std::string name = hash == std::string::npos ? std::string() : uri.substr(hash + 1);
                        if (!name.empty() && name[0] != '/' && _d.dynamic.contains(string(uri_without_fragment(uri) + "#" + name))) {
                            for (const std::string* scope : _scope) {
                                string key(*scope + "#" + name);
                                if (_d.dynamic.contains(key)) {
                                    schema_lookup(_d, *scope + "#" + name, p);
                                    break;
                                }
                            }
                        }
                        Result r = _eval(p.node, inst, std::string(p.base.view()), std::string(p.resource.view()), std::string(p.pointer.view()), ipath,
                                         kpath + "/$dynamicRef", depth + 1, true);
                        if (r.ok) {
                            res.merge(r);
                        } else {
                            res.ok = false;
                        }
                    } else {
                        fail("$dynamicRef", "a reference no resource has: " + uri);
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                // --- assertions of any type ---
                if (nd.contains(K_type)) {
                    const json& t = nd.kw[K_type];
                    bool ok = false;
                    if (t.is_array()) {
                        for (const auto& x : t.elements()) {
                            ok = ok || _type_ok(inst, x.as_string("").view());
                        }
                    } else {
                        ok = _type_ok(inst, t.as_string("").view());
                    }
                    if (!ok) {
                        std::string what = _type_of(inst);
                        fail("type", (std::string("aeiou").find(what[0]) == std::string::npos ? "a " : "an ") + what + " where the type is " + _text(t));
                        if (done()) {
                            return _leave(res, pushed);
                        }
                    }
                }
                if (nd.contains(K_enum)) {
                    bool ok = false;
                    for (const auto& x : nd.kw[K_enum].elements()) {
                        ok = ok || x == inst;
                    }
                    if (!ok) {
                        fail("enum", "a value none of the enum's: " + _text(inst));
                    }
                }
                if (nd.contains(K_const) && !(nd.kw[K_const] == inst)) {
                    fail("const", "a value other than the const " + _text(nd.kw[K_const]));
                }
                if (done()) {
                    return _leave(res, pushed);
                }
                // --- numbers ---
                if (inst.is_number()) {
                    if (nd.contains(K_multipleOf) && !schema_multiple(inst, nd.kw[K_multipleOf])) {
                        fail("multipleOf", _text(inst) + " is no multiple of " + _text(nd.kw[K_multipleOf]));
                    }
                    if (nd.contains(K_maximum) && schema_compare(inst, nd.kw[K_maximum]) > 0) {
                        fail("maximum", _text(inst) + " is greater than the maximum " + _text(nd.kw[K_maximum]));
                    }
                    if (nd.contains(K_exclusiveMaximum) && schema_compare(inst, nd.kw[K_exclusiveMaximum]) >= 0) {
                        fail("exclusiveMaximum", _text(inst) + " is not less than " + _text(nd.kw[K_exclusiveMaximum]));
                    }
                    if (nd.contains(K_minimum) && schema_compare(inst, nd.kw[K_minimum]) < 0) {
                        fail("minimum", _text(inst) + " is less than the minimum " + _text(nd.kw[K_minimum]));
                    }
                    if (nd.contains(K_exclusiveMinimum) && schema_compare(inst, nd.kw[K_exclusiveMinimum]) <= 0) {
                        fail("exclusiveMinimum", _text(inst) + " is not greater than " + _text(nd.kw[K_exclusiveMinimum]));
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                // --- strings ---
                if (inst.is_string()) {
                    string str = *inst.as_string();
                    size_t cps = 0;
                    if (nd.contains(K_maxLength) || nd.contains(K_minLength)) {
                        for (unsigned char c : str.view()) {
                            cps += (c & 0xC0) != 0x80;
                        }
                    }
                    if (nd.contains(K_maxLength) && double(cps) > nd.kw[K_maxLength].as_double(0.0)) {
                        fail("maxLength", "a string of " + std::to_string(cps) + " characters, more than " + _text(nd.kw[K_maxLength]));
                    }
                    if (nd.contains(K_minLength) && double(cps) < nd.kw[K_minLength].as_double(0.0)) {
                        fail("minLength", "a string of " + std::to_string(cps) + " characters, fewer than " + _text(nd.kw[K_minLength]));
                    }
                    if (nd.contains(K_pattern)) {
                        if (nd.pattern && !nd.pattern->contains(str)) {
                            fail("pattern", "a string the pattern " + _text(nd.kw[K_pattern]) + " does not match");
                        }
                    }
                    if (_d.o.format_assertion && nd.contains(K_format)) {
                        std::string f(nd.kw[K_format].as_string("").view());
                        if (!fmt_check(f, str.view())) {
                            fail("format", "a string that is no " + f);
                        }
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                // --- in-place applicators ---
                if (nd.contains(K_allOf)) {
                    const json& v = nd.kw[K_allOf];
                    for (size_t i = 0; i < v.size(); ++i) {
                        Result r = sub(v[i], inst, SGCL_JS_PATH("allOf/" + std::to_string(i)), SGCL_JS_PATH(ipath));
                        if (r.ok) {
                            res.merge(r);
                        } else {
                            res.ok = false;
                            if (!_collect) {
                                return _leave(res, pushed);
                            }
                        }
                    }
                }
                if (nd.contains(K_anyOf)) {
                    const json& v = nd.kw[K_anyOf];
                    size_t mark = errors.size();
                    bool any = false;
                    for (size_t i = 0; i < v.size(); ++i) {
                        Result r = sub(v[i], inst, SGCL_JS_PATH("anyOf/" + std::to_string(i)), SGCL_JS_PATH(ipath));
                        if (r.ok) {
                            any = true;
                            res.merge(r);
                            if (fast) {
                                break;
                            }
                        }
                    }
                    if (any) {
                        errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                    } else {
                        fail("anyOf", "no subschema of anyOf matches");
                        if (done()) {
                            return _leave(res, pushed);
                        }
                    }
                }
                if (nd.contains(K_oneOf)) {
                    const json& v = nd.kw[K_oneOf];
                    size_t mark = errors.size();
                    std::vector<size_t> matched;
                    Result kept;
                    for (size_t i = 0; i < v.size(); ++i) {
                        Result r = sub(v[i], inst, SGCL_JS_PATH("oneOf/" + std::to_string(i)), SGCL_JS_PATH(ipath));
                        if (r.ok) {
                            matched.push_back(i);
                            kept.merge(r);
                        }
                    }
                    if (matched.size() == 1) {
                        errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                        res.merge(kept);
                    } else if (matched.empty()) {
                        fail("oneOf", "no subschema of oneOf matches");
                    } else {
                        errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                        fail("oneOf", "subschemas " + std::to_string(matched[0]) + " and " + std::to_string(matched[1]) + " of oneOf both match");
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                if (nd.contains(K_not)) {
                    size_t mark = errors.size();
                    Result r = sub(nd.kw[K_not], inst, SGCL_JS_PATH("not"), SGCL_JS_PATH(ipath));
                    errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                    if (r.ok) {
                        fail("not", "the subschema of not matches");
                        if (done()) {
                            return _leave(res, pushed);
                        }
                    }
                }
                if (nd.contains(K_if)) {
                    size_t mark = errors.size();
                    Result r = sub(nd.kw[K_if], inst, SGCL_JS_PATH("if"), SGCL_JS_PATH(ipath));
                    errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                    if (r.ok) {
                        res.merge(r);
                        if (nd.contains(K_then)) {
                            Result t = sub(nd.kw[K_then], inst, SGCL_JS_PATH("then"), SGCL_JS_PATH(ipath));
                            if (t.ok) {
                                res.merge(t);
                            } else {
                                res.ok = false;
                            }
                        }
                    } else if (nd.contains(K_else)) {
                        Result t = sub(nd.kw[K_else], inst, SGCL_JS_PATH("else"), SGCL_JS_PATH(ipath));
                        if (t.ok) {
                            res.merge(t);
                        } else {
                            res.ok = false;
                        }
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                // --- arrays ---
                if (inst.is_array()) {
                    size_t n = inst.size();
                    if (nd.contains(K_maxItems) && double(n) > nd.kw[K_maxItems].as_double(0.0)) {
                        fail("maxItems", "an array of " + std::to_string(n) + " items, more than " + _text(nd.kw[K_maxItems]));
                    }
                    if (nd.contains(K_minItems) && double(n) < nd.kw[K_minItems].as_double(0.0)) {
                        fail("minItems", "an array of " + std::to_string(n) + " items, fewer than " + _text(nd.kw[K_minItems]));
                    }
                    if (nd.contains(K_uniqueItems) && nd.kw[K_uniqueItems].as_bool(false)) {
                        std::unordered_multimap<size_t, size_t> seen;
                        for (size_t i = 0; i < n && res.ok; ++i) {
                            size_t h = inst[i].hash();
                            auto range = seen.equal_range(h);
                            for (auto it = range.first; it != range.second; ++it) {
                                if (inst[it->second] == inst[i]) {
                                    fail("uniqueItems", "items " + std::to_string(it->second) + " and " + std::to_string(i) + " are equal");
                                    break;
                                }
                            }
                            seen.emplace(h, i);
                        }
                    }
                    size_t prefix = 0;
                    if (nd.contains(K_prefixItems)) {
                        const json& v = nd.kw[K_prefixItems];
                        prefix = std::min(n, v.size());
                        for (size_t i = 0; i < prefix; ++i) {
                            if (!sub(v[i], inst[i], SGCL_JS_PATH("prefixItems/" + std::to_string(i)), SGCL_JS_PATH(ipath + "/" + std::to_string(i))).ok) {
                                res.ok = false;
                                if (!_collect) {
                                    return _leave(res, pushed);
                                }
                            }
                        }
                        res.items = std::max(res.items, prefix);
                    }
                    if (nd.contains(K_items)) {
                        for (size_t i = prefix; i < n; ++i) {
                            if (!sub(nd.kw[K_items], inst[i], SGCL_JS_PATH("items"), SGCL_JS_PATH(ipath + "/" + std::to_string(i))).ok) {
                                res.ok = false;
                                if (!_collect) {
                                    return _leave(res, pushed);
                                }
                            }
                        }
                        res.all_items = true;
                    }
                    if (nd.contains(K_contains)) {
                        size_t mark = errors.size();
                        size_t count = 0;
                        for (size_t i = 0; i < n; ++i) {
                            if (sub(nd.kw[K_contains], inst[i], SGCL_JS_PATH("contains"), SGCL_JS_PATH(ipath + "/" + std::to_string(i))).ok) {
                                ++count;
                                res.item_set.insert(i);
                            }
                        }
                        errors.erase(errors.begin() + ptrdiff_t(mark), errors.end());
                        double lo = nd.contains(K_minContains) ? nd.kw[K_minContains].as_double(1.0) : 1.0;
                        if (double(count) < lo) {
                            fail("contains", "an array with " + std::to_string(count) + " items the contains subschema matches, fewer than " +
                                                 (nd.contains(K_minContains) ? _text(nd.kw[K_minContains]) : std::string("1")));
                        }
                        if (nd.contains(K_maxContains) && double(count) > nd.kw[K_maxContains].as_double(0.0)) {
                            fail("maxContains", "an array with " + std::to_string(count) + " items the contains subschema matches, more than " +
                                                    _text(nd.kw[K_maxContains]));
                        }
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                }
                // --- objects ---
                if (inst.is_object()) {
                    size_t n = inst.size();
                    if (nd.contains(K_maxProperties) && double(n) > nd.kw[K_maxProperties].as_double(0.0)) {
                        fail("maxProperties", "an object of " + std::to_string(n) + " properties, more than " + _text(nd.kw[K_maxProperties]));
                    }
                    if (nd.contains(K_minProperties) && double(n) < nd.kw[K_minProperties].as_double(0.0)) {
                        fail("minProperties", "an object of " + std::to_string(n) + " properties, fewer than " + _text(nd.kw[K_minProperties]));
                    }
                    if (nd.contains(K_required)) {
                        for (const auto& r : nd.kw[K_required].elements()) {
                            if (!inst.contains(*r.as_string())) {
                                fail("required", "a required property is missing: " + std::string(r.as_string("").view()));
                            }
                        }
                    }
                    if (nd.contains(K_dependentRequired)) {
                        for (const auto& d : nd.kw[K_dependentRequired].members()) {
                            if (inst.contains(d.key)) {
                                for (const auto& r : d.value.elements()) {
                                    if (!inst.contains(*r.as_string())) {
                                        fail("dependentRequired", "the property " + std::string(r.as_string("").view()) + " is required with " +
                                                                      std::string(d.key.view()));
                                    }
                                }
                            }
                        }
                    }
                    if (done()) {
                        return _leave(res, pushed);
                    }
                    std::unordered_set<std::string> local;   // properties and patternProperties matched here
                    if (nd.contains(K_properties)) {
                        const json& props = nd.kw[K_properties];
                        for (const auto& m : inst.members()) {
                            if (props.contains(m.key)) {
                                std::string k(m.key.view());
                                local.insert(k);
                                if (!sub(props[m.key], m.value, SGCL_JS_PATH("properties/" + pointer_token(k)), SGCL_JS_PATH(ipath + "/" + pointer_token(k))).ok) {
                                    res.ok = false;
                                    if (!_collect) {
                                        return _leave(res, pushed);
                                    }
                                }
                            }
                        }
                    }
                    if (nd.contains(K_patternProperties)) {
                        for (const auto& pp : nd.kw[K_patternProperties].members()) {
                            auto re = _d.regexes.find(pp.key);
                            if (re == _d.regexes.end()) {
                                continue;
                            }
                            for (const auto& m : inst.members()) {
                                if (re->second.contains(m.key)) {
                                    std::string k(m.key.view());
                                    local.insert(k);
                                    if (!sub(pp.value, m.value, SGCL_JS_PATH("patternProperties/" + pointer_token(pp.key.view())), SGCL_JS_PATH(ipath + "/" + pointer_token(k))).ok) {
                                        res.ok = false;
                                        if (!_collect) {
                                            return _leave(res, pushed);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (nd.contains(K_additionalProperties)) {
                        for (const auto& m : inst.members()) {
                            std::string k(m.key.view());
                            if (local.count(k)) {
                                continue;
                            }
                            res.props.insert(k);
                            if (!sub(nd.kw[K_additionalProperties], m.value, SGCL_JS_PATH("additionalProperties"), SGCL_JS_PATH(ipath + "/" + pointer_token(k))).ok) {
                                res.ok = false;
                                if (!_collect) {
                                    return _leave(res, pushed);
                                }
                            }
                        }
                    }
                    res.props.insert(local.begin(), local.end());
                    if (nd.contains(K_propertyNames)) {
                        for (const auto& m : inst.members()) {
                            if (!sub(nd.kw[K_propertyNames], json(m.key), SGCL_JS_PATH("propertyNames"), SGCL_JS_PATH(ipath + "/" + pointer_token(m.key.view()))).ok) {
                                res.ok = false;
                                if (!_collect) {
                                    return _leave(res, pushed);
                                }
                            }
                        }
                    }
                    if (nd.contains(K_dependentSchemas)) {
                        for (const auto& d : nd.kw[K_dependentSchemas].members()) {
                            if (inst.contains(d.key)) {
                                Result r = sub(d.value, inst, SGCL_JS_PATH("dependentSchemas/" + pointer_token(d.key.view())), SGCL_JS_PATH(ipath));
                                if (r.ok) {
                                    res.merge(r);
                                } else {
                                    res.ok = false;
                                    if (!_collect) {
                                        return _leave(res, pushed);
                                    }
                                }
                            }
                        }
                    }
                }
                // --- unevaluated: after every other keyword of this schema ---
                if (inst.is_array() && nd.contains(K_unevaluatedItems) && !res.all_items) {
                    for (size_t i = res.items; i < inst.size(); ++i) {
                        if (res.item_set.count(i)) {
                            continue;
                        }
                        if (!sub(nd.kw[K_unevaluatedItems], inst[i], SGCL_JS_PATH("unevaluatedItems"), SGCL_JS_PATH(ipath + "/" + std::to_string(i))).ok) {
                            res.ok = false;
                            if (!_collect) {
                                return _leave(res, pushed);
                            }
                        }
                    }
                    res.all_items = true;
                }
                if (inst.is_object() && nd.contains(K_unevaluatedProperties)) {
                    for (const auto& m : inst.members()) {
                        std::string k(m.key.view());
                        if (res.props.count(k)) {
                            continue;
                        }
                        if (!sub(nd.kw[K_unevaluatedProperties], m.value, SGCL_JS_PATH("unevaluatedProperties"), SGCL_JS_PATH(ipath + "/" + pointer_token(k))).ok) {
                            res.ok = false;
                            if (!_collect) {
                                return _leave(res, pushed);
                            }
                        }
                        res.props.insert(k);
                    }
                }
                return _leave(res, pushed);
            }

            const SchemaNode& _node(const json& s) const noexcept {
                auto ms = s.members();
                auto it = ms.empty() ? _d.node_index.end() : _d.node_index.find(ms.data());
                return it == _d.node_index.end() ? _d.nodes[0] : _d.nodes[it->second];
            }

            Result _leave(Result& r, bool pushed) {
                if (pushed) {
                    _scope.pop_back();
                }
                if (!r.ok) {
                    // a failed schema gives no annotations
                    r.props.clear();
                    r.items = 0;
                    r.all_items = false;
                    r.item_set.clear();
                }
                return std::move(r);
            }
        };
    }

#undef SGCL_JS_PATH

    inline json_schema::json_schema() noexcept
    : _data([] {
          auto d = make_tracked<detail::JsonSchemaData>();
          d->root = json(true);
          d->base = string("https://schema.invalid/root.json");
          d->resources.insert_or_assign(d->base, detail::SchemaPlace{d->root, d->base, d->base, string()});
          return d;
      }()) {
    }

    inline json_schema::json_schema(const json& schema)
    : json_schema(compile(schema).value()) {
    }

    inline expected<json_schema, json_schema::error> json_schema::compile(const json& schema) noexcept {
        return compile(schema, vector<json>(), options());
    }

    inline expected<json_schema, json_schema::error> json_schema::compile(const json& schema, const options& o) noexcept {
        return compile(schema, vector<json>(), o);
    }

    inline expected<json_schema, json_schema::error> json_schema::compile(const json& schema, const vector<json>& resources) noexcept {
        return compile(schema, resources, options());
    }

    inline expected<json_schema, json_schema::error> json_schema::compile(const json& schema, const vector<json>& resources, const options& o) noexcept {
        auto d = make_tracked<detail::JsonSchemaData>();
        d->o = o;
        d->root = schema;
        error e;
        detail::JsonSchemaCompiler c(*d);
        const std::string fallback = "https://schema.invalid/root.json";
        if (!schema.is_object() && !schema.is_bool()) {
            e = error(errc::type_mismatch, 0, string("a schema that is no object and no boolean"));
            detail::ErrorAccess::without_place(e);
            return unexpected<error>(std::move(e));
        }
        std::string base = fallback;
        if (schema.is_object() && schema.contains("$id")) {
            if (auto id = schema["$id"].as_string()) {
                base = detail::uri_without_fragment(detail::uri_resolve(fallback, id->view()));
            }
        }
        d->base = string(base);
        for (size_t i = 0; i < resources.size(); ++i) {
            std::string rb = "https://schema.invalid/resource" + std::to_string(i) + ".json";
            if (!resources[i].is_object() || !resources[i].contains("$id")) {
                e = error(errc::missing_field, 0, string("a resource without $id"));
                detail::ErrorAccess::without_place(e);
                return unexpected<error>(std::move(e));
            }
            if (!c.add_document(resources[i], rb, e)) {
                return unexpected<error>(std::move(e));
            }
        }
        if (!c.add_document(schema, fallback, e)) {
            return unexpected<error>(std::move(e));
        }
        if (!c.check_refs(e)) {
            return unexpected<error>(std::move(e));
        }
        return detail::JsonSchemaValidator::make(std::move(d));
    }

    inline expected<json_schema, json_schema::error> json_schema::parse(const string& text) noexcept {
        auto j = json::parse(text);
        if (!j) {
            return unexpected<error>(std::move(j.error()));
        }
        return compile(*j);
    }

    inline expected<json_schema, json_schema::error> json_schema::load(const string& path) {
        auto j = json::load(path);
        if (!j) {
            return unexpected<error>(std::move(j.error()));
        }
        return compile(*j);
    }

    inline bool json_schema::valid(const json& instance) const noexcept {
        detail::JsonSchemaValidator v(*_data, false);
        return v.run(instance).ok;
    }

    inline vector<json_schema::violation> json_schema::validate(const json& instance) const {
        detail::JsonSchemaValidator v(*_data, true);
        auto r = v.run(instance);
        (void)r;
        return std::move(v.errors);
    }

    inline string json_schema::id() const noexcept {
        return _data->base;
    }
}
