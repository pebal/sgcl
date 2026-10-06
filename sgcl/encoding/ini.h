//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/config_values.h"
#include "detail/files.h"
#include "detail/utf8_check.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sgcl::encoding {
    namespace detail {
        class IniParser;
    }

    // The sections of an INI file and their keys and values, in the file's
    // order, immutable, one word shared by copying: the INI of Python's
    // configparser (interpolation off, keys in their case, strict), keys
    // before the first section in the section "". Read by parse and load,
    // written by to_string and save.
    class ini {
    public:
        using error = encoding::error;

        // An entry of a section: a key and its value
        struct member {
            string key;
            string value;
        };

        // A section: its name ("" for the keys before the first header) and
        // its entries
        struct section {
            string name;
            slice<const member> members;
        };

        // What a parse accepts
        struct options {
            bool allow_duplicates = false;   // a section or a key twice: merged, the last value winning
            bool allow_no_value = false;     // a line of a key alone: the key with an empty value
        };

        // No sections
        ini() noexcept = default;

        static expected<ini, error> parse(const string& text) noexcept;
        static expected<ini, error> parse(const string& text, const options& o) noexcept;
        static expected<ini, error> parse(const io::reader& in);
        static expected<ini, error> parse(const io::reader& in, const options& o);
        static async::task<expected<ini, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<ini, error>> async_parse(io::reader in, options o) noexcept;

        // The sections of a file: ini::load("app.ini")
        static expected<ini, error> load(const string& path);
        static async::task<expected<ini, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // The section "" first, without a header, then every section as
        // [name] and its key = value lines, a blank line between them; a
        // value of several lines on indented lines after its key.
        // invalid_argument for what would read back otherwise
        string to_string() const;

        // The sections
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _size == 0;
        }

        slice<const section> sections() const noexcept {
            if (_size == 0) {
                return slice<const section>();
            }
            return slice<const section>(_ptr, _data(), _size);
        }

        bool contains(const string& name) const noexcept {
            return _section(name.view()) != nullptr;
        }

        bool contains(const string& name, const string& key) const noexcept {
            return _find(name.view(), key.view()) != nullptr;
        }

        optional<string> get(const string& name, const string& key) const noexcept {
            const member* m = _find(name.view(), key.view());
            return m ? optional<string>(m->value) : nullopt;
        }

        string get(const string& name, const string& key, const string& fallback) const noexcept {
            const member* m = _find(name.view(), key.view());
            return m ? m->value : fallback;
        }

        // The value read as a decimal integer, a float, a boolean (true,
        // yes, on, 1 and false, no, off, 0 in any case); nullopt or the
        // fallback when there is none or it does not read
        optional<int64_t> get_int(const string& name, const string& key) const noexcept {
            const member* m = _find(name.view(), key.view());
            return m ? detail::config_int(m->value.view()) : nullopt;
        }

        int64_t get_int(const string& name, const string& key, int64_t fallback) const noexcept {
            return get_int(name, key).value_or(fallback);
        }

        optional<double> get_double(const string& name, const string& key) const noexcept {
            const member* m = _find(name.view(), key.view());
            return m ? detail::config_double(m->value.view()) : nullopt;
        }

        double get_double(const string& name, const string& key, double fallback) const noexcept {
            return get_double(name, key).value_or(fallback);
        }

        optional<bool> get_bool(const string& name, const string& key) const noexcept {
            const member* m = _find(name.view(), key.view());
            return m ? detail::config_bool(m->value.view()) : nullopt;
        }

        bool get_bool(const string& name, const string& key, bool fallback) const noexcept {
            return get_bool(name, key).value_or(fallback);
        }

        // New versions: the key set in the section (in its place, or at the
        // end; the section made at the end when there is none), the key
        // taken out, the section taken out
        ini set(const string& name, const string& key, const string& value) const noexcept;
        ini erase(const string& name, const string& key) const noexcept;
        ini erase(const string& name) const noexcept;

        // The same sections with the same entries, in the same order
        friend bool operator==(const ini& a, const ini& b) noexcept {
            if (a._size != b._size) {
                return false;
            }
            for (size_t i = 0; i < a._size; ++i) {
                const section& x = a._data()[i];
                const section& y = b._data()[i];
                if (x.name != y.name || x.members.size() != y.members.size()) {
                    return false;
                }
                for (size_t k = 0; k < x.members.size(); ++k) {
                    if (x.members[k].key != y.members[k].key || x.members[k].value != y.members[k].value) {
                        return false;
                    }
                }
            }
            return true;
        }

    private:
        friend class detail::IniParser;

        tracked_ptr<const void> _ptr;
        size_t _size = 0;

        SGCL_INLINE_HOT const section* _data() const noexcept {
            return static_cast<const section*>(_ptr.get());
        }

        const section* _section(std::string_view name) const noexcept {
            const section* d = _data();
            for (size_t i = 0; i < _size; ++i) {
                if (d[i].name.view() == name) {
                    return d + i;
                }
            }
            return nullptr;
        }

        const member* _find(std::string_view name, std::string_view key) const noexcept {
            const section* s = _section(name);
            if (!s) {
                return nullptr;
            }
            for (const member& m : s->members) {
                if (m.key.view() == key) {
                    return &m;
                }
            }
            return nullptr;
        }

        static ini _of(const vector<section>& sections) noexcept {
            ini r;
            r._ptr = detail::config_buffer(sections.data(), sections.size());
            r._size = sections.size();
            return r;
        }

        static slice<const member> _members(const vector<member>& v) noexcept {
            if (v.empty()) {
                return slice<const member>();
            }
            tracked_ptr<const void> owner = detail::config_buffer(v.data(), v.size());
            return slice<const member>(owner, static_cast<const member*>(owner.get()), v.size());
        }
    };

    namespace detail {
        // configparser's white space at a line's ends and before its text,
        // Python's ASCII white space: space, tab, carriage return, form
        // feed, vertical tab, and the separators 0x1C to 0x1F
        SGCL_INLINE_HOT bool ini_blank(char c) noexcept {
            return c == ' ' || (c >= '\t' && c <= '\r' && c != '\n') || (c >= '\x1c' && c <= '\x1f');
        }

        SGCL_INLINE_HOT std::string_view ini_trim(std::string_view s) noexcept {
            while (!s.empty() && ini_blank(s.front())) {
                s.remove_prefix(1);
            }
            while (!s.empty() && ini_blank(s.back())) {
                s.remove_suffix(1);
            }
            return s;
        }

        class IniParser {
        public:
            IniParser(std::string_view text, const ini::options& o) noexcept
            : _p(text.data()), _n(text.size()), _o(o) {
                if (text.substr(0, 3) == "\xEF\xBB\xBF") {
                    _at = 3;   // a byte order mark, passed over
                }
            }

            expected<ini, error> run(const string& text) noexcept {
                if (!_document()) {
                    error e = _e;
                    return unexpected<error>(std::move(e.locate(text)));
                }
                vector<ini::section> sections;
                for (Section& s : _sections) {
                    vector<ini::member> ms;
                    for (auto& [k, v] : s.members) {
                        ms.push_back(ini::member{string(k), string(v)});
                    }
                    sections.push_back(ini::section{string(s.name), ini::_members(ms)});
                }
                return ini::_of(sections);
            }

        private:
            struct Section {
                std::string name;
                std::vector<pair<std::string, std::string>> members;
                std::unordered_map<std::string, size_t> index;
            };

            const char* _p;
            size_t _n;
            size_t _at = 0;
            const ini::options& _o;
            std::vector<Section> _sections;
            std::unordered_map<std::string, size_t> _index;
            error _e;

            bool _fail(errc code, size_t at, const std::string& text) noexcept {
                _e = error(code, at, string(text));
                return false;
            }

            size_t _section(const std::string& name, size_t at, bool header) noexcept {
                auto it = _index.find(name);
                if (it != _index.end()) {
                    if (header && !_o.allow_duplicates) {
                        _fail(errc::duplicate_key, at, "the section [" + name + "] given twice");
                        return size_t(-1);
                    }
                    return it->second;
                }
                _index.emplace(name, _sections.size());
                _sections.push_back(Section{name, {}, {}});
                return _sections.size() - 1;
            }

            bool _document() noexcept {
                size_t current = size_t(-1);
                size_t value_of = size_t(-1);   // the entry a deeper line continues
                size_t key_indent = 0;
                while (_at < _n) {
                    size_t start = _at;
                    size_t end = _at;
                    while (end < _n && _p[end] != '\n') {
                        ++end;
                    }
                    _at = end < _n ? end + 1 : end;
                    std::string_view line(_p + start, end - start);
                    if (!line.empty() && line.back() == '\r') {
                        line.remove_suffix(1);
                    }
                    size_t indent = 0;
                    while (indent < line.size() && ini_blank(line[indent])) {
                        ++indent;
                    }
                    std::string_view text = ini_trim(line);
                    if (text.empty() || text.front() == ';' || text.front() == '#') {
                        value_of = size_t(-1);   // a blank line or a comment ends a value
                        continue;
                    }
                    if (value_of != size_t(-1) && indent > key_indent) {
                        auto& v = _sections[current].members[value_of].second;
                        v += '\n';
                        v.append(text);
                        continue;
                    }
                    value_of = size_t(-1);
                    key_indent = indent;
                    size_t place = start + indent;
                    if (text.front() == '[') {
                        size_t close = text.rfind(']');
                        if (close != std::string_view::npos && close > 1) {
                            std::string_view after = ini_trim(text.substr(close + 1));
                            if (!after.empty() && after.front() != ';' && after.front() != '#') {
                                return _fail(errc::syntax, size_t(after.data() - _p), "more on the line after a section's ']'");
                            }
                            current = _section(std::string(text.substr(1, close - 1)), place, true);
                            if (current == size_t(-1)) {
                                return false;
                            }
                            continue;
                        }
                    }
                    if (current == size_t(-1)) {
                        current = _section(std::string(), place, false);
                    }
                    size_t d = text.find_first_of("=:");
                    std::string key, value;
                    if (d == std::string_view::npos) {
                        if (!_o.allow_no_value) {
                            return _fail(errc::syntax, place, "a line that is no section, no key = value and no comment");
                        }
                        key = std::string(text);
                    } else {
                        key = std::string(ini_trim(text.substr(0, d)));
                        value = std::string(ini_trim(text.substr(d + 1)));
                    }
                    if (key.empty()) {
                        return _fail(errc::syntax, place, "a value without its key");
                    }
                    Section& s = _sections[current];
                    auto it = s.index.find(key);
                    if (it != s.index.end()) {
                        if (!_o.allow_duplicates) {
                            return _fail(errc::duplicate_key, place, "the key " + key + " given twice in [" + s.name + "]");
                        }
                        s.members[it->second].second = value;
                        value_of = it->second;
                        continue;
                    }
                    s.index.emplace(key, s.members.size());
                    s.members.push_back({key, value});
                    value_of = s.members.size() - 1;
                }
                return true;
            }
        };
    }

    inline ini ini::set(const string& name, const string& key, const string& value) const noexcept {
        vector<section> v(sections().begin(), sections().end());
        for (section& s : v) {
            if (s.name == name) {
                vector<member> ms(s.members.begin(), s.members.end());
                bool found = false;
                for (member& m : ms) {
                    if (m.key == key) {
                        m.value = value;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ms.push_back(member{key, value});
                }
                s.members = _members(ms);
                return _of(v);
            }
        }
        vector<member> ms;
        ms.push_back(member{key, value});
        if (name.empty()) {
            v.insert(v.begin(), section{name, _members(ms)});   // the section "" is the file's first
        } else {
            v.push_back(section{name, _members(ms)});
        }
        return _of(v);
    }

    inline ini ini::erase(const string& name, const string& key) const noexcept {
        if (!contains(name, key)) {
            return *this;
        }
        vector<section> v(sections().begin(), sections().end());
        for (section& s : v) {
            if (s.name == name) {
                vector<member> ms;
                for (const member& m : s.members) {
                    if (m.key != key) {
                        ms.push_back(m);
                    }
                }
                s.members = _members(ms);
            }
        }
        return _of(v);
    }

    inline ini ini::erase(const string& name) const noexcept {
        if (!contains(name)) {
            return *this;
        }
        vector<section> v;
        for (const section& s : sections()) {
            if (s.name != name) {
                v.push_back(s);
            }
        }
        return _of(v);
    }

    inline expected<ini, ini::error> ini::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<ini, ini::error> ini::parse(const string& text, const options& o) noexcept {
        auto v = text.view();
        size_t bad = v.find('\0');
        if (bad != std::string_view::npos) {
            error e(errc::invalid_character, bad, string("a null character"));
            return unexpected<error>(std::move(e.locate(text)));
        }
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
        return detail::IniParser(v, o).run(text);
    }

    inline expected<ini, ini::error> ini::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<ini, ini::error> ini::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<ini, ini::error>> ini::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<ini, ini::error>> ini::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    namespace detail {
        SGCL_INLINE_HOT bool ini_has_break(std::string_view s) noexcept {
            return s.find('\n') != std::string_view::npos;
        }
    }

    inline string ini::to_string() const {
        auto refuse = [](const char* what) {
            throw invalid_argument(std::string("sgcl::encoding::ini::to_string: ") + what);
        };
        // the section "" first: its keys read as no section's after a header
        std::vector<const section*> order;
        for (const section& s : sections()) {
            if (s.name.empty()) {
                order.insert(order.begin(), &s);
            } else {
                order.push_back(&s);
            }
        }
        std::string out;
        for (const section* s : order) {
            std::string_view name = s->name.view();
            if (!name.empty()) {
                if (detail::ini_has_break(name) || name.find('\0') != std::string_view::npos) {
                    refuse("a section's name with a line break");
                }
                if (!out.empty()) {
                    out += '\n';
                }
                out += '[';
                out.append(name);
                out += "]\n";
            } else if (s->members.empty()) {
                continue;
            }
            for (const member& m : s->members) {
                std::string_view k = m.key.view(), v = m.value.view();
                if (k.find('\0') != std::string_view::npos || v.find('\0') != std::string_view::npos) {
                    refuse("a null character");
                }
                if (k.empty() || detail::ini_blank(k.front()) || detail::ini_blank(k.back()) || detail::ini_has_break(k) ||
                    k.find_first_of("=:") != std::string_view::npos || k.front() == ';' || k.front() == '#') {
                    refuse("a key that does not read back (empty, blanks around it, '=' or ':' in it, a line break, ';' or '#' first)");
                }
                if (out.empty() && k.substr(0, 3) == "\xEF\xBB\xBF") {
                    refuse("a key of a byte order mark first at the start of the text (the reading passes over it)");
                }
                if (k.front() == '[') {
                    // its line would read as a header: a ']' past the second character
                    std::string_view first_line = v.substr(0, v.find('\n'));
                    if (k.rfind(']') > 1 && k.rfind(']') != std::string_view::npos) {
                        refuse("a key of '[' first on a line with a ']' (a section's header)");
                    }
                    if (first_line.find(']') != std::string_view::npos) {
                        refuse("a key of '[' first on a line with a ']' (a section's header)");
                    }
                }
                out.append(k);
                out += " =";
                // the value's lines: each trimmed when read, none empty
                size_t at = 0;
                bool first = true;
                for (;;) {
                    size_t nl = v.find('\n', at);
                    std::string_view line = v.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
                    if (detail::ini_trim(line) != line || (line.empty() && !first) ||
                        (!first && (line.front() == ';' || line.front() == '#'))) {
                        refuse("a value that does not read back (white space at the ends of a line, an empty line after the first, a comment's ';' or '#' first on a line after the first)");
                    }
                    if (first) {
                        if (!line.empty()) {
                            out += ' ';
                            out.append(line);
                        }
                    } else {
                        out += "\n    ";
                        out.append(line);
                    }
                    first = false;
                    if (nl == std::string_view::npos) {
                        break;
                    }
                    at = nl + 1;
                }
                out += '\n';
            }
        }
        return string(out);
    }

    namespace detail {
        inline async::task<expected<void, ini::error>> ini_save_task(string path, ini value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<ini, ini::error> ini::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return ini::parse(in); });
    }

    inline async::task<expected<ini, ini::error>> ini::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return ini::load(path); });
    }

    inline expected<void, ini::error> ini::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, ini::error>> ini::async_save(string path) const noexcept {
        return detail::ini_save_task(std::move(path), *this);
    }
}
