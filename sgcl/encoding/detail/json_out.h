//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "json_number.h"
#include "json_text.h"
#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // The characters JsonOut writes: a block that doubles, every append
    // one comparison and a copy the compiler writes in place. A
    // std::string took a call into the library for each of the few
    // characters a value is made of (a quote, a comma, a colon) and
    // zeroed what it grew by; stringify of ten thousand records spent a
    // fifth of its time there.
    class JsonText {
    public:
        const char* data() const noexcept {
            return _data.get();
        }

        size_t size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return !_size;
        }

        void clear() noexcept {
            _size = 0;
        }

        // Back to the first n characters (n <= size())
        void truncate(size_t n) noexcept {
            _size = n;
        }

        std::string_view view() const noexcept {
            return std::string_view(_data.get(), _size);
        }

        void push_back(char c) {
            if (_size == _capacity) [[unlikely]] {
                _grow(1);
            }
            _data[_size++] = c;
        }

        // A few bytes (a number, a key, a word of a string) copied by words
        // that overlap (copy_bytes), not by a call of memcpy
        void append(const char* p, size_t n) {
            if (n > _capacity - _size) [[unlikely]] {
                _grow(n);
            }
            copy_bytes(_data.get() + _size, p, n);
            _size += n;
        }

        // A string of up to 64 bytes quoted in one pass when none of its
        // bytes is to be escaped (nor, with escape_html, is < > or &):
        // copied and tested together, with no branch per byte; false, and
        // nothing written, otherwise — write_string's loop takes it then.
        // The keys and most values of a record are such strings.
        bool quoted_plain(std::string_view s, bool escape_html) {
            const size_t n = s.size();
            if (n > 64) {
                return false;
            }
            if (n + 2 > _capacity - _size) [[unlikely]] {
                _grow(n + 2);
            }
            char* d = _data.get() + _size;
            const auto* p = reinterpret_cast<const unsigned char*>(s.data());
            uint8_t odd = 0;   // a byte and not a bool: a bool's | keeps the loop from being vectorized
            if (escape_html) {
                for (size_t i = 0; i < n; ++i) {
                    unsigned char c = p[i];
                    d[i + 1] = char(c);
                    odd |= uint8_t(c == '"') | uint8_t(c == '\\') | uint8_t(c < 0x20) | uint8_t(c >> 7) | uint8_t(c == '<')
                        | uint8_t(c == '>') | uint8_t(c == '&');
                }
            } else {
                for (size_t i = 0; i < n; ++i) {
                    unsigned char c = p[i];
                    d[i + 1] = char(c);
                    odd |= uint8_t(c == '"') | uint8_t(c == '\\') | uint8_t(c < 0x20) | uint8_t(c >> 7);
                }
            }
            if (odd) {
                return false;
            }
            d[0] = '"';
            d[n + 1] = '"';
            _size += n + 2;
            return true;
        }

        void append(std::string_view s) {
            append(s.data(), s.size());
        }

        void append(size_t n, char c) {
            if (n > _capacity - _size) [[unlikely]] {
                _grow(n);
            }
            std::memset(_data.get() + _size, c, n);
            _size += n;
        }

        JsonText& operator+=(char c) {
            push_back(c);
            return *this;
        }

        JsonText& operator+=(std::string_view s) {
            append(s);
            return *this;
        }

    private:
        void _grow(size_t n) {
            size_t capacity = std::max(_capacity * 2, std::max(_size + n, size_t(256)));
            auto more = std::make_unique_for_overwrite<char[]>(capacity);
            if (_size) {
                std::memcpy(more.get(), _data.get(), _size);
            }
            _data = std::move(more);
            _capacity = capacity;
        }

        std::unique_ptr<char[]> _data;
        size_t _size = 0;
        size_t _capacity = 0;
    };

    // The text of JSON as it is written, piece by piece: the punctuation,
    // the indentation and the commas follow from the structure, which is
    // checked as it goes (an end without a beginning, a key outside an
    // object, a value where a key belongs). A text with nothing to write
    // into but itself: json::to_string, stringify and json::writer (which
    // hands the text to its stream) are all this. The first mistake is
    // kept and the rest of the calls do nothing.
    //
    // The layout is Go's MarshalIndent with an empty prefix: compact has
    // no space at all; with an indent, every element and member on a line
    // of its own, indented by its depth, ": " after a key, and an empty
    // array or object as [] and {}.
    class JsonOut {
    public:
        JsonOut(uint8_t indent, bool escape_html, bool newline_after_top = false)
        : _indent(indent), _escape_html(escape_html), _newline_after_top(newline_after_top) {
        }

        JsonText& text() noexcept {
            return _text;
        }

        bool failed() const noexcept {
            return _failed;
        }

        errc code() const noexcept {
            return _code;
        }

        const std::string& detail() const noexcept {
            return _detail;
        }

        size_t depth() const noexcept {
            return _stack.size();
        }

        // Whether a whole value was written and nothing is open
        bool complete() const noexcept {
            return _stack.empty() && _values > 0;
        }

        void fail(errc code, std::string detail) {
            if (!_failed) {
                _failed = true;
                _code = code;
                _detail = std::move(detail);
            }
        }

        void set_detail(std::string detail) {
            _detail = std::move(detail);
        }

        void begin(bool object) {
            if (!_before_value()) {
                return;
            }
            _text += object ? '{' : '[';
            _stack.push_back(Level{object, false, false});
        }

        void end(bool object) {
            if (_failed) {
                return;
            }
            if (_stack.empty() || _stack.back().object != object) {
                fail(errc::syntax, object ? "end_object without an open object" : "end_array without an open array");
                return;
            }
            if (object && _stack.back().after_key) {
                fail(errc::syntax, "end_object after a key with no value");
                return;
            }
            bool had = _stack.back().has_elements;
            _stack.pop_back();
            if (had && _indent) {
                _newline();
            }
            _text += object ? '}' : ']';
            _value_done();
        }

        void key(std::string_view name) {
            if (_failed) {
                return;
            }
            if (_stack.empty() || !_stack.back().object || _stack.back().after_key) {
                fail(errc::syntax, _stack.empty() || !_stack.back().object ? "a key outside an object" : "two keys in a row");
                return;
            }
            auto& level = _stack.back();
            if (level.has_elements) {
                _text += ',';
            }
            level.has_elements = true;
            if (_indent) {
                _newline();
            }
            if (!_text.quoted_plain(name, _escape_html)) {
                write_string(_text, name, _escape_html);
            }
            _text += ':';
            if (_indent) {
                _text += ' ';
            }
            level.after_key = true;
        }

        void null() {
            if (_before_value()) {
                _text += "null";
                _value_done();
            }
        }

        void boolean(bool b) {
            if (_before_value()) {
                _text += b ? "true" : "false";
                _value_done();
            }
        }

        template<class I>
        void integer(I v) {
            if (_before_value()) {
                char buf[NumberTextSize];
                _text.append(buf, integer_text(buf, v));
                _value_done();
            }
        }

        // A double or a float, each with its own shortest digits; NaN and
        // the infinities are not JSON: unsupported_value
        template<class F>
        void floating(F v) {
            if (!std::isfinite(v)) {
                fail(errc::unsupported_value, std::isnan(v) ? "NaN is not a JSON number" : "an infinity is not a JSON number");
                return;
            }
            if (_before_value()) {
                char buf[NumberTextSize];
                _text.append(buf, number_text(buf, v));
                _value_done();
            }
        }

        // A number written as its literal, which the caller vouches for
        // (a literal read before)
        void literal(std::string_view text) {
            if (_before_value()) {
                _text.append(text);
                _value_done();
            }
        }

        void quoted(std::string_view s) {
            if (_before_value()) {
                if (!_text.quoted_plain(s, _escape_html)) {
                    write_string(_text, s, _escape_html);
                }
                _value_done();
            }
        }

    private:
        struct Level {
            bool object;
            bool has_elements;
            bool after_key;
        };

        void _newline() {
            _text += '\n';
            _text.append(_stack.size() * _indent, ' ');
        }

        bool _before_value() {
            if (_failed) {
                return false;
            }
            if (_stack.empty()) {
                if (_values > 0) {
                    if (!_newline_after_top) {
                        fail(errc::syntax, "a second value at the top level");
                        return false;
                    }
                }
                return true;
            }
            auto& level = _stack.back();
            if (level.object) {
                if (!level.after_key) {
                    fail(errc::syntax, "a value in an object where a key belongs");
                    return false;
                }
                level.after_key = false;
                return true;
            }
            if (level.has_elements) {
                _text += ',';
            }
            level.has_elements = true;
            if (_indent) {
                _newline();
            }
            return true;
        }

        void _value_done() {
            if (_stack.empty()) {
                ++_values;
                if (_newline_after_top) {
                    _text += '\n';
                }
            }
        }

        JsonText _text;
        std::vector<Level> _stack;
        size_t _values = 0;
        std::string _detail;
        uint8_t _indent;
        bool _escape_html;
        bool _newline_after_top;
        bool _failed = false;
        errc _code = errc::syntax;
    };
}
