//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/utf8.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

// The tokens of YAML 1.2 (YAML 1.2.2, chapters 6 to 9), from the characters:
// the indentation of the block styles turned into tokens that open and close
// the collections, the simple keys found where their ':' comes and the key
// token put back before them, the five scalar styles read into their content
// (folded, escaped, chomped), the properties and the directives. The parse
// (yaml.h) builds the nodes from the tokens.
namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    enum class YamlTok : uint8_t {
        stream_start, stream_end, version_directive, tag_directive, document_start, document_end,
        block_sequence_start, block_mapping_start, block_end, flow_sequence_start, flow_sequence_end,
        flow_mapping_start, flow_mapping_end, block_entry, flow_entry, key, value, alias, anchor, tag, scalar
    };

    enum class YamlStyle : uint8_t { plain, single_quoted, double_quoted, literal, folded };

    struct YamlToken {
        YamlTok type = YamlTok::stream_start;
        YamlStyle style = YamlStyle::plain;
        size_t at = 0;            // where it starts in the text
        std::string value;        // a scalar's content, a name, a tag's handle, a directive's first part
        std::string suffix;       // a tag's suffix, a directive's second part
    };

    class YamlScanner {
    public:
        YamlScanner(std::string_view text) noexcept
        : _p(text.data()), _n(text.size()) {
            _simple_keys.push_back(SimpleKey{});
            if (_n >= 3 && uint8_t(_p[0]) == 0xEF && uint8_t(_p[1]) == 0xBB && uint8_t(_p[2]) == 0xBF) {
                _at = 3;
                _line_start = 3;
            }
        }

        // The next token, false with the error
        bool peek(const YamlToken*& t) noexcept {
            if (!_more()) {
                return false;
            }
            t = &_tokens.front();
            return true;
        }

        bool next(YamlToken& t) noexcept {
            if (!_more()) {
                return false;
            }
            t = std::move(_tokens.front());
            _tokens.pop_front();
            ++_taken;
            return true;
        }

        const error& failure() const noexcept {
            return _error;
        }

    private:
        struct SimpleKey {
            bool possible = false;
            bool required = false;
            size_t token_number = 0;
            size_t at = 0;
            size_t line = 0;
            size_t column = 0;
        };

        const char* _p;
        size_t _n;
        size_t _at = 0;
        size_t _line = 0;
        size_t _line_start = 0;
        std::deque<YamlToken> _tokens;
        size_t _taken = 0;
        std::vector<long> _indents;
        long _indent = -1;
        size_t _flow_level = 0;
        bool _simple_key_allowed = true;
        std::vector<SimpleKey> _simple_keys;
        bool _started = false;
        bool _ended = false;
        bool _adjacent_value = false;   // after a quoted scalar or a flow end: ':' needs no space in flow
        error _error;
        bool _failed = false;

        SGCL_INLINE_HOT size_t _column() const noexcept {
            return _at - _line_start;
        }

        SGCL_INLINE_HOT char _c(size_t k = 0) const noexcept {
            return _at + k < _n ? _p[_at + k] : '\0';
        }

        SGCL_INLINE_HOT bool _eof(size_t k = 0) const noexcept {
            return _at + k >= _n;
        }

        SGCL_INLINE_HOT bool _is_break(size_t k = 0) const noexcept {
            char c = _c(k);
            return !_eof(k) && (c == '\n' || c == '\r');
        }

        SGCL_INLINE_HOT bool _is_space(size_t k = 0) const noexcept {
            char c = _c(k);
            return !_eof(k) && (c == ' ' || c == '\t');
        }

        SGCL_INLINE_HOT bool _is_blank_or_end(size_t k = 0) const noexcept {
            return _eof(k) || _is_space(k) || _is_break(k);
        }

        SGCL_INLINE_HOT static bool _flow_indicator(char c) noexcept {
            return c == ',' || c == '[' || c == ']' || c == '{' || c == '}';
        }

        bool _fail(errc code, size_t at, const char* text) noexcept {
            if (!_failed) {
                _failed = true;
                _error = error(code, at, string(text));
            }
            return false;
        }

        void _skip_break() noexcept {
            if (_c() == '\r' && _c(1) == '\n') {
                _at += 2;
            } else {
                ++_at;
            }
            ++_line;
            _line_start = _at;
        }

        // Tokens enough that the one in front cannot get a key before it
        bool _more() noexcept {
            if (_failed) {
                return false;
            }
            for (;;) {
                bool need = _tokens.empty();
                if (!need) {
                    if (!_stale_simple_keys()) {
                        return false;
                    }
                    for (auto& k : _simple_keys) {
                        if (k.possible && k.token_number == _taken) {
                            need = true;
                            break;
                        }
                    }
                }
                if (!need) {
                    return true;
                }
                if (_ended) {
                    return !_tokens.empty() || _fail(errc::unexpected_end, _n, "the end of the stream was read already");
                }
                if (!_fetch()) {
                    return false;
                }
            }
        }

        void _push(YamlTok type, size_t at) {
            YamlToken t;
            t.type = type;
            t.at = at;
            _tokens.push_back(std::move(t));
        }

        bool _stale_simple_keys() noexcept {
            for (auto& k : _simple_keys) {
                if (k.possible && (k.line != _line || _at - k.at > 1024)) {
                    if (k.required) {
                        return _fail(errc::syntax, k.at, "a simple key without its ':'");
                    }
                    k.possible = false;
                }
            }
            return true;
        }

        bool _save_simple_key() noexcept {
            bool required = _flow_level == 0 && _indent == long(_column());
            if (_simple_key_allowed) {
                if (!_remove_simple_key()) {
                    return false;
                }
                SimpleKey& k = _simple_keys.back();
                k.possible = true;
                k.required = required;
                k.token_number = _taken + _tokens.size();
                k.at = _at;
                k.line = _line;
                k.column = _column();
            }
            return true;
        }

        bool _remove_simple_key() noexcept {
            SimpleKey& k = _simple_keys.back();
            if (k.possible && k.required) {
                return _fail(errc::syntax, k.at, "a simple key without its ':'");
            }
            k.possible = false;
            return true;
        }

        // A collection opened at the column (the block styles): its token at
        // the place given, or at the end
        void _roll_indent(long column, YamlTok type, size_t at, long number) {
            if (_flow_level) {
                return;
            }
            if (_indent < column) {
                _indents.push_back(_indent);
                _indent = column;
                YamlToken t;
                t.type = type;
                t.at = at;
                if (number < 0) {
                    _tokens.push_back(std::move(t));
                } else {
                    _tokens.insert(_tokens.begin() + (number - long(_taken)), std::move(t));
                }
            }
        }

        void _unroll_indent(long column) {
            if (_flow_level) {
                return;
            }
            while (_indent > column) {
                _push(YamlTok::block_end, _at);
                _indent = _indents.back();
                _indents.pop_back();
            }
        }

        bool _scan_to_next_token() noexcept {
            for (;;) {
                while (_c() == ' ' || ((_flow_level || !_simple_key_allowed) && _c() == '\t')) {
                    ++_at;
                }
                // a tab where the indentation of a block is: only before a comment or the end of the line
                if (_c() == '\t' && !_eof()) {
                    size_t k = _at;
                    while (k < _n && (_p[k] == ' ' || _p[k] == '\t')) {
                        ++k;
                    }
                    if (k == _n || _p[k] == '#' || _p[k] == '\n' || _p[k] == '\r') {
                        _at = k;
                    } else {
                        return _fail(errc::syntax, _at, "a tab where the indentation of a block is");
                    }
                }
                if (_c() == '#' && !_eof()) {
                    while (!_eof() && !_is_break()) {
                        ++_at;
                    }
                }
                if (_is_break()) {
                    _skip_break();
                    if (!_flow_level) {
                        _simple_key_allowed = true;
                    }
                    continue;
                }
                return true;
            }
        }

        bool _document_marker() const noexcept {
            return _column() == 0 && _n - _at >= 3 && ((_p[_at] == '-' && _p[_at + 1] == '-' && _p[_at + 2] == '-') || (_p[_at] == '.' && _p[_at + 1] == '.' && _p[_at + 2] == '.')) && _is_blank_or_end(3);
        }

        bool _fetch() {
            if (!_started) {
                _started = true;
                _push(YamlTok::stream_start, _at);
                return true;
            }
            if (!_scan_to_next_token() || !_stale_simple_keys()) {
                return false;
            }
            _unroll_indent(long(_column()));
            if (_eof()) {
                _unroll_indent(-1);
                if (!_remove_simple_key()) {
                    return false;
                }
                _simple_key_allowed = false;
                _push(YamlTok::stream_end, _at);
                _ended = true;
                return true;
            }
            char c = _c();
            bool adjacent = _adjacent_value;
            _adjacent_value = false;
            if (_column() == 0 && c == '%') {
                return _fetch_directive();
            }
            if (_document_marker()) {
                _unroll_indent(-1);
                if (!_remove_simple_key()) {
                    return false;
                }
                _simple_key_allowed = false;
                size_t at = _at;
                YamlTok t = c == '-' ? YamlTok::document_start : YamlTok::document_end;
                _at += 3;
                _push(t, at);
                return true;
            }
            switch (c) {
                case '[':
                case '{':
                    return _fetch_flow_start(c == '[' ? YamlTok::flow_sequence_start : YamlTok::flow_mapping_start);
                case ']':
                case '}':
                    return _fetch_flow_end(c == ']' ? YamlTok::flow_sequence_end : YamlTok::flow_mapping_end);
                case ',':
                    if (!_flow_level) {
                        break;
                    }
                    if (!_remove_simple_key()) {
                        return false;
                    }
                    _simple_key_allowed = true;
                    _push(YamlTok::flow_entry, _at++);
                    return true;
                case '-':
                    if (_is_blank_or_end(1)) {
                        return _fetch_block_entry();
                    }
                    break;
                case '?':
                    if (_is_blank_or_end(1) || (_flow_level && _flow_indicator(_c(1)))) {
                        return _fetch_key();
                    }
                    break;
                case ':':
                    if (_is_blank_or_end(1) || (_flow_level && (_flow_indicator(_c(1)) || adjacent))) {
                        return _fetch_value();
                    }
                    break;
                case '*':
                    return _fetch_anchor(YamlTok::alias);
                case '&':
                    return _fetch_anchor(YamlTok::anchor);
                case '!':
                    return _fetch_tag();
                case '|':
                case '>':
                    if (!_flow_level) {
                        return _fetch_block_scalar(c == '>');
                    }
                    break;
                case '\'':
                case '"':
                    return _fetch_quoted(c == '"');
                default:
                    break;
            }
            // a plain scalar: not an indicator, or - ? : before a character it may hold
            bool indicator = c == '-' || c == '?' || c == ':' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == '#'
                          || c == '&' || c == '*' || c == '!' || c == '|' || c == '>' || c == '\'' || c == '"' || c == '%' || c == '@' || c == '`';
            if (!indicator || ((c == '-' || c == '?' || c == ':') && !_is_blank_or_end(1) && !(_flow_level && _flow_indicator(_c(1))))) {
                return _fetch_plain();
            }
            return _fail(errc::syntax, _at, c == '@' || c == '`' ? "a reserved indicator, '@' or '`'" : c == '\t' ? "a tab where the indentation of a block is" : "a character that starts no token here");
        }

        bool _fetch_flow_start(YamlTok type) {
            if (!_save_simple_key()) {
                return false;
            }
            ++_flow_level;
            _simple_keys.push_back(SimpleKey{});
            _simple_key_allowed = true;
            _push(type, _at++);
            return true;
        }

        bool _fetch_flow_end(YamlTok type) {
            if (!_flow_level) {
                return _fail(errc::syntax, _at, type == YamlTok::flow_sequence_end ? "a ']' that closes no '['" : "a '}' that closes no '{'");
            }
            if (!_remove_simple_key()) {
                return false;
            }
            --_flow_level;
            _simple_keys.pop_back();
            _simple_key_allowed = false;
            _adjacent_value = true;
            _push(type, _at++);
            return true;
        }

        bool _fetch_block_entry() {
            if (_flow_level) {
                return _fail(errc::syntax, _at, "a block sequence entry inside a flow collection");
            }
            if (!_simple_key_allowed) {
                return _fail(errc::syntax, _at, "a block sequence entry where none may begin");
            }
            _roll_indent(long(_column()), YamlTok::block_sequence_start, _at, -1);
            if (!_remove_simple_key()) {
                return false;
            }
            _simple_key_allowed = true;
            _push(YamlTok::block_entry, _at++);
            return true;
        }

        bool _fetch_key() {
            if (!_flow_level) {
                if (!_simple_key_allowed) {
                    return _fail(errc::syntax, _at, "a mapping key where none may begin");
                }
                _roll_indent(long(_column()), YamlTok::block_mapping_start, _at, -1);
            }
            if (!_remove_simple_key()) {
                return false;
            }
            _simple_key_allowed = _flow_level == 0;
            _push(YamlTok::key, _at++);
            return true;
        }

        bool _fetch_value() {
            SimpleKey& k = _simple_keys.back();
            if (k.possible) {
                YamlToken t;
                t.type = YamlTok::key;
                t.at = k.at;
                _tokens.insert(_tokens.begin() + (long(k.token_number) - long(_taken)), std::move(t));
                _roll_indent(long(k.column), YamlTok::block_mapping_start, k.at, long(k.token_number));
                k.possible = false;
                _simple_key_allowed = false;
            } else {
                if (!_flow_level) {
                    if (!_simple_key_allowed) {
                        return _fail(errc::syntax, _at, "a mapping value where none may be");
                    }
                    _roll_indent(long(_column()), YamlTok::block_mapping_start, _at, -1);
                }
                _simple_key_allowed = _flow_level == 0;
            }
            _push(YamlTok::value, _at++);
            return true;
        }

        bool _fetch_anchor(YamlTok type) {
            if (!_save_simple_key()) {
                return false;
            }
            _simple_key_allowed = false;
            size_t start = _at++;
            size_t from = _at;
            while (!_eof() && !_is_space() && !_is_break() && !_flow_indicator(_c())) {
                ++_at;
            }
            if (_at == from) {
                return _fail(errc::syntax, start, type == YamlTok::alias ? "an alias without its name" : "an anchor without its name");
            }
            YamlToken t;
            t.type = type;
            t.at = start;
            t.value.assign(_p + from, _at - from);
            _tokens.push_back(std::move(t));
            return true;
        }

        static bool _uri_char(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '%' || c == '#' || c == ';'
                || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' || c == '$' || c == ',' || c == '_'
                || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')' || c == '[' || c == ']';
        }

        static bool _word_char(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        }

        // %xx escapes of a tag's suffix decoded
        bool _uri(size_t from, size_t to, bool in_flow, std::string& out) {
            for (size_t i = from; i < to; ++i) {
                char c = _p[i];
                if (c == '%') {
                    if (to - i < 3) {
                        return _fail(errc::invalid_escape, i, "a '%' of a tag not followed by two hex digits");
                    }
                    auto hex = [](char h) { return h >= '0' && h <= '9' ? h - '0' : (h | 32) >= 'a' && (h | 32) <= 'f' ? (h | 32) - 'a' + 10 : -1; };
                    int a = hex(_p[i + 1]), b = hex(_p[i + 2]);
                    if (a < 0 || b < 0) {
                        return _fail(errc::invalid_escape, i, "a '%' of a tag not followed by two hex digits");
                    }
                    if (a * 16 + b < 0x20 || a * 16 + b == 0x7F) {
                        return _fail(errc::invalid_escape, i, "a '%' of a tag that stands for a control character");
                    }
                    out += char(a * 16 + b);
                    i += 2;
                } else {
                    (void)in_flow;
                    out += c;
                }
            }
            return true;
        }

        bool _fetch_tag() {
            if (!_save_simple_key()) {
                return false;
            }
            _simple_key_allowed = false;
            size_t start = _at;
            YamlToken t;
            t.type = YamlTok::tag;
            t.at = start;
            if (_c(1) == '<') {
                // !<verbatim>
                _at += 2;
                size_t from = _at;
                while (!_eof() && _c() != '>' && _uri_char(_c())) {
                    ++_at;
                }
                if (_c() != '>' || _at == from) {
                    return _fail(errc::syntax, start, "a verbatim tag without its '>'");
                }
                if (!_uri(from, _at, false, t.suffix)) {
                    return false;
                }
                ++_at;
            } else {
                // ! alone, !suffix, !!suffix, !handle!suffix
                size_t k = _at + 1;
                while (k < _n && _word_char(_p[k])) {
                    ++k;
                }
                if (k < _n && _p[k] == '!') {
                    t.value.assign(_p + _at, k + 1 - _at);
                    _at = k + 1;
                } else {
                    t.value = "!";
                    ++_at;
                }
                size_t from = _at;
                while (!_eof() && _uri_char(_c()) && !(_flow_level && _flow_indicator(_c()))) {
                    ++_at;
                }
                if (!_uri(from, _at, _flow_level != 0, t.suffix)) {
                    return false;
                }
                if (t.value != "!" && t.suffix.empty()) {
                    return _fail(errc::syntax, start, "a tag of a handle without its suffix");
                }
            }
            if (!_is_blank_or_end() && !(_flow_level && _flow_indicator(_c()))) {
                return _fail(errc::syntax, _at, "a tag not followed by a space");
            }
            if (!sgcl::utf8::valid(std::string_view(t.suffix))) {
                return _fail(errc::invalid_utf8, start, "a tag whose escapes are not UTF-8");
            }
            _tokens.push_back(std::move(t));
            return true;
        }

        bool _fetch_directive() {
            _unroll_indent(-1);
            if (!_remove_simple_key()) {
                return false;
            }
            _simple_key_allowed = false;
            size_t start = _at++;
            size_t from = _at;
            while (!_eof() && !_is_space() && !_is_break()) {
                ++_at;
            }
            std::string name(_p + from, _at - from);
            auto word = [&](std::string& out) {
                while (_is_space()) {
                    ++_at;
                }
                size_t f = _at;
                while (!_eof() && !_is_space() && !_is_break()) {
                    ++_at;
                }
                out.assign(_p + f, _at - f);
            };
            YamlToken t;
            t.at = start;
            if (name == "YAML") {
                t.type = YamlTok::version_directive;
                word(t.value);
                if (t.value.size() < 3 || t.value[0] != '1' || t.value[1] != '.' || t.value.find_first_not_of("0123456789", 2) != std::string::npos) {
                    return _fail(errc::syntax, start, "a %YAML directive of a version other than 1.x");
                }
            } else if (name == "TAG") {
                t.type = YamlTok::tag_directive;
                word(t.value);
                word(t.suffix);
                bool uri = !t.suffix.empty();
                for (char c : t.suffix) {
                    uri = uri && _uri_char(c);
                }
                if (t.value.empty() || t.value.front() != '!' || t.value.back() != '!' || !uri) {
                    return _fail(errc::syntax, start, "a %TAG directive that is not a handle and a prefix");
                }
                for (size_t i = 0; i < t.suffix.size(); ++i) {
                    if (t.suffix[i] == '%' && (i + 2 >= t.suffix.size() || !std::isxdigit(uint8_t(t.suffix[i + 1])) || !std::isxdigit(uint8_t(t.suffix[i + 2])))) {
                        return _fail(errc::invalid_escape, start, "a '%' of a tag prefix not followed by two hex digits");
                    }
                }
            } else {
                // a reserved directive: passed over (§6.8.1)
                while (!_eof() && !_is_break()) {
                    ++_at;
                }
                return true;
            }
            while (_is_space()) {
                ++_at;
            }
            if (_c() == '#') {
                while (!_eof() && !_is_break()) {
                    ++_at;
                }
            }
            if (!_eof() && !_is_break()) {
                return _fail(errc::syntax, _at, "more after a directive than its parameters");
            }
            _tokens.push_back(std::move(t));
            return true;
        }

        // The scalars ---------------------------------------------------

        bool _fetch_plain() {
            if (!_save_simple_key()) {
                return false;
            }
            size_t start = _at;
            std::string out;
            std::string whitespace;
            bool leading_blanks = false;
            long min_indent = _indent + 1;
            for (;;) {
                if (_document_marker() || _c() == '#') {
                    break;
                }
                bool stop = false;
                while (!_eof() && !_is_space() && !_is_break()) {
                    char c = _c();
                    if (c == ':' && (_is_blank_or_end(1) || (_flow_level && _flow_indicator(_c(1))))) {
                        stop = true;
                        break;
                    }
                    if (_flow_level && _flow_indicator(c)) {
                        stop = true;
                        break;
                    }
                    if (!whitespace.empty()) {
                        out += whitespace;
                        whitespace.clear();
                    }
                    out += c;
                    ++_at;
                }
                if (stop || !(_is_space() || _is_break())) {
                    break;
                }
                // the blanks and the breaks after the run
                std::string spaces;
                std::string breaks;
                leading_blanks = false;
                while (_is_space() || _is_break()) {
                    if (_is_space()) {
                        if (leading_blanks && long(_column()) < min_indent && _c() == '\t') {
                            return _fail(errc::syntax, _at, "a tab where the indentation of a block is");
                        }
                        if (!leading_blanks) {
                            spaces += _c();
                        }
                        ++_at;
                    } else {
                        _skip_break();
                        if (!leading_blanks) {
                            leading_blanks = true;
                        } else {
                            breaks += '\n';
                        }
                    }
                }
                if (leading_blanks) {
                    whitespace = breaks.empty() ? std::string(" ") : breaks;
                } else {
                    whitespace = spaces;
                }
                if (!_flow_level && leading_blanks && long(_column()) < min_indent) {
                    break;
                }
            }
            YamlToken t;
            t.type = YamlTok::scalar;
            t.style = YamlStyle::plain;
            t.at = start;
            t.value = std::move(out);
            _tokens.push_back(std::move(t));
            _simple_key_allowed = leading_blanks;
            return true;
        }

        bool _hex_escape(size_t digits, std::string& out) {
            uint32_t v = 0;
            for (size_t i = 0; i < digits; ++i) {
                char h = _c(i);
                int d = h >= '0' && h <= '9' ? h - '0' : (h | 32) >= 'a' && (h | 32) <= 'f' ? (h | 32) - 'a' + 10 : -1;
                if (d < 0 || _eof(i)) {
                    return _fail(errc::invalid_escape, _at, "an escape without its hex digits");
                }
                v = v * 16 + uint32_t(d);
            }
            if (!sgcl::utf8::valid(char32_t(v))) {
                return _fail(errc::invalid_escape, _at, "an escape of no Unicode scalar value");
            }
            char b[4];
            out.append(b, sgcl::utf8::encode(char32_t(v), b));
            _at += digits;
            return true;
        }

        bool _fetch_quoted(bool double_quoted) {
            if (!_save_simple_key()) {
                return false;
            }
            _simple_key_allowed = false;
            size_t start = _at++;
            std::string out;
            for (;;) {
                if (_document_marker()) {
                    return _fail(errc::syntax, _at, "a document marker inside a quoted scalar");
                }
                if (_eof()) {
                    return _fail(errc::unexpected_end, start, "a quoted scalar without its closing quote");
                }
                // the characters to the end of the line or the quote
                bool closed = false;
                while (!_eof() && !_is_space() && !_is_break()) {
                    char c = _c();
                    if (!double_quoted && c == '\'') {
                        if (_c(1) == '\'') {
                            out += '\'';
                            _at += 2;
                            continue;
                        }
                        closed = true;
                        break;
                    }
                    if (double_quoted && c == '"') {
                        closed = true;
                        break;
                    }
                    if (double_quoted && c == '\\') {
                        if (_is_break(1)) {
                            // an escaped line break: the break and the next line's leading blanks dropped
                            ++_at;
                            _skip_break();
                            while (_is_space()) {
                                ++_at;
                            }
                            while (_is_break()) {
                                // an empty line after it is still a line feed
                                _skip_break();
                                out += '\n';
                                while (_is_space()) {
                                    ++_at;
                                }
                            }
                            continue;
                        }
                        char e = _c(1);
                        _at += 2;
                        switch (e) {
                            case '0': out += '\0'; break;
                            case 'a': out += '\a'; break;
                            case 'b': out += '\b'; break;
                            case 't':
                            case '\t': out += '\t'; break;
                            case 'n': out += '\n'; break;
                            case 'v': out += '\v'; break;
                            case 'f': out += '\f'; break;
                            case 'r': out += '\r'; break;
                            case 'e': out += '\x1b'; break;
                            case ' ': out += ' '; break;
                            case '"': out += '"'; break;
                            case '/': out += '/'; break;
                            case '\\': out += '\\'; break;
                            case 'N': out += "\xc2\x85"; break;
                            case '_': out += "\xc2\xa0"; break;
                            case 'L': out += "\xe2\x80\xa8"; break;
                            case 'P': out += "\xe2\x80\xa9"; break;
                            case 'x':
                                if (!_hex_escape(2, out)) {
                                    return false;
                                }
                                break;
                            case 'u':
                                if (!_hex_escape(4, out)) {
                                    return false;
                                }
                                break;
                            case 'U':
                                if (!_hex_escape(8, out)) {
                                    return false;
                                }
                                break;
                            default:
                                return _fail(errc::invalid_escape, _at - 2, "an unknown escape in a double-quoted scalar");
                        }
                        continue;
                    }
                    out += c;
                    ++_at;
                }
                if (closed) {
                    ++_at;
                    break;
                }
                // blanks and breaks: folded
                std::string spaces;
                size_t breaks = 0;
                while (_is_space() || _is_break()) {
                    if (_is_space()) {
                        if (breaks == 0) {
                            spaces += _c();
                        }
                        ++_at;
                    } else {
                        _skip_break();
                        ++breaks;
                    }
                }
                if (breaks == 0) {
                    out += spaces;
                } else if (breaks == 1) {
                    out += ' ';
                } else {
                    out.append(breaks - 1, '\n');
                }
                if (!_flow_level && breaks && long(_column()) <= _indent && !_eof()) {
                    return _fail(errc::syntax, _at, "a line of a quoted scalar not indented past its block");
                }
            }
            YamlToken t;
            t.type = YamlTok::scalar;
            t.style = double_quoted ? YamlStyle::double_quoted : YamlStyle::single_quoted;
            t.at = start;
            t.value = std::move(out);
            _tokens.push_back(std::move(t));
            _adjacent_value = true;
            return true;
        }

        bool _fetch_block_scalar(bool folded) {
            if (!_remove_simple_key()) {
                return false;
            }
            _simple_key_allowed = true;
            size_t start = _at++;
            int chomping = 0;   // -1 strip, 0 clip, 1 keep
            long increment = 0;
            for (int i = 0; i < 2; ++i) {
                char c = _c();
                if ((c == '+' || c == '-') && chomping == 0) {
                    chomping = c == '+' ? 1 : -1;
                    ++_at;
                } else if (c >= '1' && c <= '9' && increment == 0) {
                    increment = c - '0';
                    ++_at;
                } else if (c == '0') {
                    return _fail(errc::syntax, _at, "a block scalar's indentation indicator of 0");
                }
            }
            while (_is_space()) {
                ++_at;
            }
            if (_c() == '#') {
                if (_at > start && _p[_at - 1] != ' ' && _p[_at - 1] != '\t') {
                    return _fail(errc::syntax, _at, "a comment not after a space");
                }
                while (!_eof() && !_is_break()) {
                    ++_at;
                }
            }
            if (!_eof() && !_is_break()) {
                return _fail(errc::syntax, _at, "more on a block scalar's line than its indicators");
            }
            if (_is_break()) {
                _skip_break();
            }
            long parent = _indent < 0 ? 0 : _indent;
            long indent = increment ? parent + increment : 0;
            if (_indent < 0 && increment) {
                indent = increment;
            }
            // the leading empty lines, and the indentation when it is detected
            std::string breaks;
            long max_empty = 0;
            {
                for (;;) {
                    while ((indent == 0 || long(_column()) < indent) && _c() == ' ') {
                        ++_at;
                    }
                    if (long(_column()) > max_empty) {
                        max_empty = long(_column());
                    }
                    if ((indent == 0 || long(_column()) < indent) && _c() == '\t' && !_is_break(1)) {
                        // a tab in the indentation of a line with content
                        size_t k = _at;
                        while (k < _n && (_p[k] == ' ' || _p[k] == '\t')) {
                            ++k;
                        }
                        if (k < _n && _p[k] != '\n' && _p[k] != '\r') {
                            return _fail(errc::syntax, _at, "a tab in the indentation of a block scalar");
                        }
                    }
                    if (!_is_break()) {
                        break;
                    }
                    _skip_break();
                    breaks += '\n';
                }
                if (indent == 0) {
                    indent = long(_column());
                    if (indent < max_empty) {
                        indent = max_empty;
                    }
                    if (indent < parent + 1) {
                        indent = parent + 1;
                    }
                    if (_indent < 0 && indent < 1) {
                        indent = 1;
                    }
                    if (long(_column()) < indent && !_eof() && !_is_break() && max_empty > long(_column())) {
                        return _fail(errc::syntax, _at, "a leading empty line of a block scalar more indented than its text");
                    }
                }
            }
            // the lines: each a content (after the indentation) and the
            // empty lines before it
            std::string out;
            bool any = false;
            bool prev_more = false;
            size_t leading_empty = breaks.size();
            size_t trailing = 0;
            bool last_break = false;
            while (long(_column()) == indent && !_eof() && !_document_marker()) {
                bool more = _c() == ' ' || _c() == '\t';
                size_t from = _at;
                while (!_eof() && !_is_break()) {
                    ++_at;
                }
                std::string_view line(_p + from, _at - from);
                size_t empties = any ? trailing : leading_empty;
                if (!any) {
                    out.append(empties, '\n');
                } else if (folded && !prev_more && !more) {
                    if (empties == 0) {
                        out += ' ';
                    } else {
                        out.append(empties, '\n');
                    }
                } else {
                    out.append(1 + empties, '\n');
                }
                out.append(line);
                any = true;
                prev_more = more;
                trailing = 0;
                last_break = false;
                if (_is_break()) {
                    _skip_break();
                    last_break = true;
                }
                // the empty lines after it
                for (;;) {
                    while (long(_column()) < indent && _c() == ' ') {
                        ++_at;
                    }
                    if (_is_break() && long(_column()) <= indent) {
                        if (long(_column()) < indent || true) {
                            _skip_break();
                            ++trailing;
                            continue;
                        }
                    }
                    break;
                }
                if (long(_column()) < indent && !_eof() && !_is_break()) {
                    break;
                }
            }
            if (!any) {
                trailing = leading_empty;
                last_break = false;
            }
            if (chomping == 1) {
                if (any && last_break) {
                    out += '\n';
                }
                out.append(trailing, '\n');
            } else if (chomping == 0 && any && last_break) {
                out += '\n';
            }
            YamlToken t;
            t.type = YamlTok::scalar;
            t.style = folded ? YamlStyle::folded : YamlStyle::literal;
            t.at = start;
            t.value = std::move(out);
            _tokens.push_back(std::move(t));
            return true;
        }
    };
}
