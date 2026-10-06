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
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sgcl::encoding {
    namespace detail {
        class DotenvParser;
    }

    // The entries of a .env file (docker compose's, python-dotenv's, Go's
    // godotenv's): keys and their values in the file's order, immutable, one
    // word shared by copying. Read by parse and load, written by to_string
    // and save, put into the process environment by apply.
    class dotenv {
    public:
        using error = encoding::error;

        // An entry: a key and its value
        struct member {
            string key;
            string value;
        };

        // What a parse does
        struct options {
            bool expand = true;            // $NAME and ${NAME...} in unquoted and double-quoted values
            bool use_environment = true;   // a name not in the file looked up in the process environment
            size_t max_size = size_t(16) << 20;   // the values together, expansions made: limit_exceeded past it
        };

        // No entries
        dotenv() noexcept = default;

        // Of entries made in the program; a key given twice keeps its first
        // place and its last value
        static dotenv from(std::initializer_list<member> members) noexcept;
        static dotenv from(const vector<member>& members) noexcept;

        static expected<dotenv, error> parse(const string& text) noexcept;
        static expected<dotenv, error> parse(const string& text, const options& o) noexcept;
        static expected<dotenv, error> parse(const io::reader& in);
        static expected<dotenv, error> parse(const io::reader& in, const options& o);
        static async::task<expected<dotenv, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<dotenv, error>> async_parse(io::reader in, options o) noexcept;

        // The entries of a file: dotenv::load(".env")
        static expected<dotenv, error> load(const string& path);
        static async::task<expected<dotenv, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // One KEY=value a line: a value plain when it reads back as itself,
        // else in double quotes with \\ \" \$ \n \r \t. invalid_argument for
        // a key a .env cannot hold, or a value with a null character
        string to_string() const;

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _size == 0;
        }

        bool contains(const string& key) const noexcept {
            return _find(key.view()) != nullptr;
        }

        optional<string> get(const string& key) const noexcept {
            const member* m = _find(key.view());
            return m ? optional<string>(m->value) : nullopt;
        }

        string get(const string& key, const string& fallback) const noexcept {
            const member* m = _find(key.view());
            return m ? m->value : fallback;
        }

        // The value read as a decimal integer, a float, a boolean (true,
        // yes, on, 1 and false, no, off, 0 in any case); nullopt or the
        // fallback when there is none or it does not read
        optional<int64_t> get_int(const string& key) const noexcept {
            const member* m = _find(key.view());
            return m ? detail::config_int(m->value.view()) : nullopt;
        }

        int64_t get_int(const string& key, int64_t fallback) const noexcept {
            return get_int(key).value_or(fallback);
        }

        optional<double> get_double(const string& key) const noexcept {
            const member* m = _find(key.view());
            return m ? detail::config_double(m->value.view()) : nullopt;
        }

        double get_double(const string& key, double fallback) const noexcept {
            return get_double(key).value_or(fallback);
        }

        optional<bool> get_bool(const string& key) const noexcept {
            const member* m = _find(key.view());
            return m ? detail::config_bool(m->value.view()) : nullopt;
        }

        bool get_bool(const string& key, bool fallback) const noexcept {
            return get_bool(key).value_or(fallback);
        }

        // In the file's order
        slice<const member> members() const noexcept {
            if (_size == 0) {
                return slice<const member>();
            }
            return slice<const member>(_ptr, _data(), _size);
        }

        // New versions: the key set (in its place, or at the end), the key
        // taken out
        dotenv set(const string& key, const string& value) const noexcept;
        dotenv erase(const string& key) const noexcept;

        // Every entry into the process environment: a variable already set
        // kept, unless overwrite. For the start of a program, before other
        // threads read the environment (getenv and setenv are not safe
        // together). invalid_argument for a key the environment cannot hold
        void apply() const;
        void apply(bool overwrite) const;

        // The same entries in the same order
        friend bool operator==(const dotenv& a, const dotenv& b) noexcept {
            if (a._size != b._size) {
                return false;
            }
            for (size_t i = 0; i < a._size; ++i) {
                if (a._data()[i].key != b._data()[i].key || a._data()[i].value != b._data()[i].value) {
                    return false;
                }
            }
            return true;
        }

    private:
        friend class detail::DotenvParser;

        tracked_ptr<const void> _ptr;
        size_t _size = 0;

        SGCL_INLINE_HOT const member* _data() const noexcept {
            return static_cast<const member*>(_ptr.get());
        }

        const member* _find(std::string_view key) const noexcept {
            const member* d = _data();
            for (size_t i = 0; i < _size; ++i) {
                if (d[i].key.view() == key) {
                    return d + i;
                }
            }
            return nullptr;
        }

        static dotenv _of(const vector<member>& members) noexcept {
            dotenv d;
            d._ptr = detail::config_buffer(members.data(), members.size());
            d._size = members.size();
            return d;
        }
    };

    namespace detail {
        SGCL_INLINE_HOT bool dotenv_key_start(char c) noexcept {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
        }

        SGCL_INLINE_HOT bool dotenv_key_char(char c) noexcept {
            return dotenv_key_start(c) || (c >= '0' && c <= '9') || c == '.';
        }

        SGCL_INLINE_HOT bool dotenv_name_char(char c) noexcept {
            return dotenv_key_start(c) || (c >= '0' && c <= '9');
        }

        inline bool dotenv_valid_key(std::string_view k) noexcept {
            if (k.empty() || !dotenv_key_start(k[0])) {
                return false;
            }
            for (char c : k) {
                if (!dotenv_key_char(c)) {
                    return false;
                }
            }
            return true;
        }

        class DotenvParser {
        public:
            DotenvParser(std::string_view text, const dotenv::options& o) noexcept
            : _p(text.data()), _n(text.size()), _o(o) {
                if (text.substr(0, 3) == "\xEF\xBB\xBF") {
                    _at = 3;   // a byte order mark, passed over
                }
            }

            expected<dotenv, error> run(const string& text) noexcept {
                if (!_document()) {
                    error e = _e;
                    return unexpected<error>(std::move(e.locate(text)));
                }
                return dotenv::_of(_members);
            }

        private:
            const char* _p;
            size_t _n;
            size_t _at = 0;
            const dotenv::options& _o;
            vector<dotenv::member> _members;
            std::unordered_map<std::string, size_t> _index;
            size_t _total = 0;   // the bytes of the values kept
            error _e;

            bool _fail(errc code, size_t at, const std::string& text) noexcept {
                _e = error(code, at, string(text));
                return false;
            }

            SGCL_INLINE_HOT char _c(size_t k = 0) const noexcept {
                return _at + k < _n ? _p[_at + k] : '\0';
            }

            SGCL_INLINE_HOT bool _end() const noexcept {
                return _at >= _n;
            }

            void _blanks() noexcept {
                while (_c() == ' ' || _c() == '\t') {
                    ++_at;
                }
            }

            bool _line_end() const noexcept {
                return _end() || _c() == '\n' || (_c() == '\r' && _c(1) == '\n');
            }

            void _skip_line_end() noexcept {
                _at += _c() == '\r' ? 2 : _end() ? 0 : 1;
            }

            // After a value: blanks, a comment, the line's end
            bool _rest_of_line() noexcept {
                _blanks();
                if (_c() == '#') {
                    while (!_end() && _c() != '\n') {
                        ++_at;
                    }
                }
                if (!_line_end()) {
                    return _fail(errc::syntax, _at, "more on the line after the value");
                }
                _skip_line_end();
                return true;
            }

            bool _document() noexcept {
                for (;;) {
                    _blanks();
                    if (_end()) {
                        return true;
                    }
                    if (_line_end()) {
                        _skip_line_end();
                        continue;
                    }
                    if (_c() == '#') {
                        while (!_end() && _c() != '\n') {
                            ++_at;
                        }
                        continue;
                    }
                    if (std::string_view(_p + _at, std::min<size_t>(7, _n - _at)) == "export " ||
                        std::string_view(_p + _at, std::min<size_t>(7, _n - _at)) == "export\t") {
                        _at += 7;
                        _blanks();
                    }
                    size_t start = _at;
                    if (!dotenv_key_start(_c())) {
                        return _fail(errc::syntax, _at, "a key expected (a letter or '_', then letters, digits, '_' and '.')");
                    }
                    while (dotenv_key_char(_c())) {
                        ++_at;
                    }
                    std::string key(_p + start, _at - start);
                    _blanks();
                    if (_c() != '=') {
                        return _fail(errc::syntax, _at, "no '=' after the key " + key);
                    }
                    ++_at;
                    _blanks();
                    std::string value;
                    if (!_value(value)) {
                        return false;
                    }
                    _put(key, value);
                }
            }

            void _put(const std::string& key, const std::string& value) {
                _total += value.size();
                auto it = _index.find(key);
                if (it != _index.end()) {
                    _total -= _members[it->second].value.size();
                    _members[it->second].value = string(value);
                    return;
                }
                _index.emplace(key, _members.size());
                _members.push_back(dotenv::member{string(key), string(value)});
            }

            bool _value(std::string& out) noexcept {
                char q = _c();
                if (q == '\'') {
                    size_t start = _at++;
                    size_t from = _at;
                    while (!_end() && _c() != '\'') {
                        ++_at;
                    }
                    if (_end()) {
                        return _fail(errc::unexpected_end, start, "a value without its closing quote");
                    }
                    out.assign(_p + from, _at - from);
                    ++_at;
                    return _rest_of_line();
                }
                if (q == '"') {
                    size_t start = _at++;
                    for (;;) {
                        if (_end()) {
                            return _fail(errc::unexpected_end, start, "a value without its closing quote");
                        }
                        char c = _c();
                        if (c == '"') {
                            ++_at;
                            break;
                        }
                        if (c == '\\') {
                            char e = _c(1);
                            switch (e) {
                                case 'n': out += '\n'; break;
                                case 'r': out += '\r'; break;
                                case 't': out += '\t'; break;
                                case '\\': out += '\\'; break;
                                case '"': out += '"'; break;
                                case '$': out += '$'; break;
                                default:
                                    out += '\\';   // kept as written
                                    ++_at;
                                    continue;
                            }
                            _at += 2;
                            continue;
                        }
                        if (c == '$' && _o.expand) {
                            if (!_expand(out, '"')) {
                                return false;
                            }
                            continue;
                        }
                        out += c;
                        ++_at;
                    }
                    return _rest_of_line();
                }
                // unquoted: to the line's end or a comment after a blank, trimmed
                for (;;) {
                    if (_line_end()) {
                        break;
                    }
                    char c = _c();
                    if (c == '#' && (_p[_at - 1] == ' ' || _p[_at - 1] == '\t')) {
                        while (!_end() && _c() != '\n') {
                            ++_at;
                        }
                        break;
                    }
                    if (c == '$' && _o.expand) {
                        if (!_expand(out, 0)) {
                            return false;
                        }
                        continue;
                    }
                    out += c;
                    ++_at;
                }
                while (!out.empty() && (out.back() == ' ' || out.back() == '\t' || out.back() == '\r')) {
                    out.pop_back();
                }
                if (!_line_end()) {
                    return _fail(errc::syntax, _at, "more on the line after the value");
                }
                _skip_line_end();
                return true;
            }

            // Whether n more bytes of a value keep the values within max_size
            bool _room(size_t have, size_t n, size_t at) noexcept {
                if (_total + have + n > _o.max_size) {
                    return _fail(errc::limit_exceeded, at, "the values past max_size, their expansions made");
                }
                return true;
            }

            std::string _lookup(const std::string& name, bool& set) const noexcept {
                auto it = _index.find(name);
                if (it != _index.end()) {
                    set = true;
                    return std::string(_members[it->second].value.view());
                }
                if (_o.use_environment) {
                    if (const char* v = std::getenv(name.c_str())) {
                        set = true;
                        return v;
                    }
                }
                set = false;
                return std::string();
            }

            // $NAME or ${NAME}, ${NAME:-x}, ${NAME-x}, ${NAME:+x}, ${NAME+x},
            // ${NAME:?x}, ${NAME?x} at
            // _at; a '$' of none of them is kept. quote: '"' inside double
            // quotes (the default may hold escapes and ends there), 0 unquoted
            bool _expand(std::string& out, char quote) noexcept {
                size_t start = _at;
                if (_c(1) != '{') {
                    if (!dotenv_key_start(_c(1))) {
                        out += '$';
                        ++_at;
                        return true;
                    }
                    ++_at;
                    size_t from = _at;
                    while (dotenv_name_char(_c())) {
                        ++_at;
                    }
                    bool set;
                    std::string v = _lookup(std::string(_p + from, _at - from), set);
                    if (!_room(out.size(), v.size(), start)) {
                        return false;
                    }
                    out += v;
                    return true;
                }
                _at += 2;
                size_t from = _at;
                while (dotenv_name_char(_c())) {
                    ++_at;
                }
                std::string name(_p + from, _at - from);
                if (name.empty() || !dotenv_key_start(name[0])) {
                    return _fail(errc::syntax, start, "a ${...} without a name");
                }
                bool set;
                std::string value = _lookup(name, set);
                if (_c() == '}') {
                    ++_at;
                    if (!_room(out.size(), value.size(), start)) {
                        return false;
                    }
                    out += value;
                    return true;
                }
                if (_end() || (quote == 0 && _line_end()) || (quote && _c() == quote)) {
                    return _fail(errc::unexpected_end, start, "a ${" + name + " without its '}'");
                }
                bool colon = _c() == ':';
                char op = _c(colon ? 1 : 0);
                if (op != '-' && op != '?' && op != '+') {
                    return _fail(errc::syntax, _at, "a ${" + name + "...} of an operator but :-, -, :+, +, :? and ?");
                }
                _at += colon ? 2 : 1;
                // the word, expanded itself, to the matching '}'
                std::string word;
                for (;;) {
                    if (_end() || (quote == 0 && _line_end()) || (quote && _c() == quote)) {
                        return _fail(errc::unexpected_end, start, "a ${" + name + "...} without its '}'");
                    }
                    char c = _c();
                    if (c == '}') {
                        ++_at;
                        break;
                    }
                    if (c == '$') {
                        if (!_expand(word, quote)) {
                            return false;
                        }
                        if (!_room(out.size(), word.size(), start)) {
                            return false;
                        }
                        continue;
                    }
                    if (c == '\\' && quote && (_c(1) == '"' || _c(1) == '\\' || _c(1) == '$' || _c(1) == '}')) {
                        word += _c(1);
                        _at += 2;
                        continue;
                    }
                    word += c;
                    ++_at;
                }
                bool use_word = colon ? (!set || value.empty()) : !set;
                if (op == '?' && use_word) {
                    return _fail(errc::missing_field, start, name + ": " + (word.empty() ? std::string("required") : word));
                }
                const std::string& piece = op == '+' ? (!use_word ? word : std::string()) : op == '-' && use_word ? word : value;
                if (!_room(out.size(), piece.size(), start)) {
                    return false;
                }
                out += piece;
                return true;
            }
        };
    }

    inline dotenv dotenv::from(std::initializer_list<member> members) noexcept {
        dotenv d;
        for (const member& m : members) {
            d = d.set(m.key, m.value);
        }
        return d;
    }

    inline dotenv dotenv::from(const vector<member>& members) noexcept {
        dotenv d;
        for (const member& m : members) {
            d = d.set(m.key, m.value);
        }
        return d;
    }

    inline dotenv dotenv::set(const string& key, const string& value) const noexcept {
        vector<member> v(members().begin(), members().end());
        for (member& m : v) {
            if (m.key == key) {
                m.value = value;
                return _of(v);
            }
        }
        v.push_back(member{key, value});
        return _of(v);
    }

    inline dotenv dotenv::erase(const string& key) const noexcept {
        if (!contains(key)) {
            return *this;
        }
        vector<member> v;
        for (const member& m : members()) {
            if (m.key != key) {
                v.push_back(m);
            }
        }
        return _of(v);
    }

    inline expected<dotenv, dotenv::error> dotenv::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<dotenv, dotenv::error> dotenv::parse(const string& text, const options& o) noexcept {
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
        return detail::DotenvParser(v, o).run(text);
    }

    inline expected<dotenv, dotenv::error> dotenv::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<dotenv, dotenv::error> dotenv::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<dotenv, dotenv::error>> dotenv::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<dotenv, dotenv::error>> dotenv::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    inline string dotenv::to_string() const {
        std::string out;
        for (const member& m : members()) {
            std::string_view k = m.key.view(), v = m.value.view();
            if (!detail::dotenv_valid_key(k)) {
                throw invalid_argument("sgcl::encoding::dotenv::to_string: a key a .env cannot hold");
            }
            if (v.find('\0') != std::string_view::npos) {
                throw invalid_argument("sgcl::encoding::dotenv::to_string: a value with a null character");
            }
            bool plain = v.empty() || (v.front() != ' ' && v.front() != '\t' && v.back() != ' ' && v.back() != '\t');
            for (char c : v) {
                auto b = uint8_t(c);
                plain = plain && b >= 0x20 && b != 0x7F && c != '"' && c != '\'' && c != '#' && c != '$' && c != '\\';
            }
            out.append(k);
            out += '=';
            if (plain) {
                out.append(v);
            } else {
                out += '"';
                for (char c : v) {
                    switch (c) {
                        case '\\': out += "\\\\"; break;
                        case '"': out += "\\\""; break;
                        case '$': out += "\\$"; break;
                        case '\n': out += "\\n"; break;
                        case '\r': out += "\\r"; break;
                        case '\t': out += "\\t"; break;
                        default: out += c;
                    }
                }
                out += '"';
            }
            out += '\n';
        }
        return string(out);
    }

    inline void dotenv::apply() const {
        apply(false);
    }

    inline void dotenv::apply(bool overwrite) const {
        for (const member& m : members()) {
            std::string k(m.key.view()), v(m.value.view());
            if (k.empty() || k.find('=') != std::string::npos || k.find('\0') != std::string::npos || v.find('\0') != std::string::npos) {
                throw invalid_argument("sgcl::encoding::dotenv::apply: a key or a value the environment cannot hold");
            }
#if defined(_WIN32)
            if (overwrite || !std::getenv(k.c_str())) {
                _putenv_s(k.c_str(), v.c_str());
            }
#else
            ::setenv(k.c_str(), v.c_str(), overwrite ? 1 : 0);
#endif
        }
    }

    namespace detail {
        inline async::task<expected<void, dotenv::error>> dotenv_save_task(string path, dotenv value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<dotenv, dotenv::error> dotenv::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return dotenv::parse(in); });
    }

    inline async::task<expected<dotenv, dotenv::error>> dotenv::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return dotenv::load(path); });
    }

    inline expected<void, dotenv::error> dotenv::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, dotenv::error>> dotenv::async_save(string path) const noexcept {
        return detail::dotenv_save_task(std::move(path), *this);
    }
}
