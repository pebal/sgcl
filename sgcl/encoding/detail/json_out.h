//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "json_number.h"
#include "json_text.h"
#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"

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
    //
    // A limit on the length (json::to_string and stringify: a string's)
    // is weighed where the block grows, the only place the text can pass
    // it: a growth past it lets the text go and stops the writing
    // (stopped(), over()), so the appends themselves weigh nothing. The
    // writing's own mistakes stop it through the same flag (JsonOut::fail).
    class JsonText {
    public:
        SGCL_INLINE_HOT const char* data() const noexcept {
            return _data.get();
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return !_size;
        }

        SGCL_INLINE_HOT void clear() noexcept {
            _size = 0;
        }

        // Back to the first n characters (n <= size())
        SGCL_INLINE_HOT void truncate(size_t n) noexcept {
            _size = n;
        }

        SGCL_INLINE_HOT std::string_view view() const noexcept {
            return std::string_view(_data.get(), _size);
        }

        SGCL_INLINE_HOT void push_back(char c) noexcept {
            if (_size == _capacity) [[unlikely]] {
                _grow(1);
            }
            _data[_size++] = c;
        }

        // A few bytes (a number, a key, a word of a string) copied by words
        // that overlap (copy_bytes), not by a call of memcpy
        SGCL_INLINE_HOT void append(const char* p, size_t n) noexcept {
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
        bool quoted_plain(std::string_view s, bool escape_html) noexcept {
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

        SGCL_INLINE_HOT void append(std::string_view s) noexcept {
            append(s.data(), s.size());
        }

        SGCL_INLINE_HOT void append(size_t n, char c) noexcept {
            if (n > _capacity - _size) [[unlikely]] {
                _grow(n);
            }
            // memset, not fill_bytes (notes 384, 393): through fill_bytes
            // json::pretty wrote the three nativejson corpora 1.7-12.6%
            // faster, but an indentation of 128-256 bytes 4-7% slower with
            // every block aligned alike: fill_bytes's own path lost to libc there
            // (before 444; the remaining memsets wait for DESIGN 442)
            std::memset(_data.get() + _size, c, n);
            _size += n;
        }

        SGCL_INLINE_HOT JsonText& operator+=(char c) noexcept {
            push_back(c);
            return *this;
        }

        // A block lent to the text (JsonLent), empty; and the block given
        // back out of it, with its capacity
        SGCL_INLINE_HOT void lend(std::unique_ptr<char[]> block, size_t capacity) noexcept {
            _data = std::move(block);
            _capacity = capacity;
            _size = 0;
        }

        SGCL_INLINE_HOT std::unique_ptr<char[]> give_back(size_t& capacity) noexcept {
            capacity = _capacity;
            _capacity = 0;
            _size = 0;
            return std::move(_data);
        }

        // The longest the text may grow; past it the text is let go and
        // the writing stops
        SGCL_INLINE_HOT void limit(size_t n) noexcept {
            _limit = n;
        }

        // Whether the writing stopped: at a mistake of its own (stop()) or
        // at a growth past the limit (then over() too)
        SGCL_INLINE_HOT bool stopped() const noexcept {
            return _stopped;
        }

        SGCL_INLINE_HOT void stop() noexcept {
            _stopped = true;
        }

        SGCL_INLINE_HOT bool over() const noexcept {
            return _over;
        }

        SGCL_INLINE_HOT JsonText& operator+=(std::string_view s) noexcept {
            append(s);
            return *this;
        }

    private:
        void _grow(size_t n) noexcept {
            if (n > _limit - _size) [[unlikely]] {
                if (_past_limit(n)) {
                    return;
                }
            }
            // never past the limit, so that no append within the block
            // passes it unweighed
            size_t capacity = std::max(std::min(std::max(_capacity * 2, size_t(256)), _limit), _size + n);
            auto more = std::make_unique_for_overwrite<char[]>(capacity);
            if (_size) {
                // memcpy, not copy_bytes (note 313): a growth, once per
                // doubling, and copy_bytes inlined here grew _grow by 148
                // bytes, which moved the writer's hot functions after it
                // and cost stringify 0.6%
                std::memcpy(more.get(), _data.get(), _size);
            }
            _data = std::move(more);
            _capacity = capacity;
        }

        // A write of n past the limit: the text is let go and the writing
        // stops; true when the block holds the write in hand from its start
        SGCL_COLD bool _past_limit(size_t n) noexcept {
            _over = true;
            _stopped = true;
            _size = 0;
            return n <= _capacity;
        }

        std::unique_ptr<char[]> _data;
        size_t _size = 0;
        size_t _capacity = 0;
        size_t _limit = SIZE_MAX;
        bool _stopped = false;
        bool _over = false;
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
        friend class JsonLent;

    public:
        SGCL_INLINE_HOT JsonOut(uint8_t indent, bool escape_html, bool newline_after_top = false) noexcept
        : _indent(indent), _escape_html(escape_html), _newline_after_top(newline_after_top) {
        }

        SGCL_INLINE_HOT JsonText& text() noexcept {
            return _text;
        }

        SGCL_INLINE_HOT bool failed() const noexcept {
            return _text.stopped();
        }

        SGCL_INLINE_HOT errc code() const noexcept {
            return _code;
        }

        SGCL_INLINE_HOT const std::string& detail() const noexcept {
            return _detail;
        }

        SGCL_INLINE_HOT size_t depth() const noexcept {
            return _stack.size();
        }

        // Whether a whole value was written and nothing is open
        SGCL_INLINE_HOT bool complete() const noexcept {
            return _stack.empty() && _values > 0;
        }

        void fail(errc code, std::string detail) noexcept {
            if (!_text.stopped()) {
                _text.stop();
                _code = code;
                _detail = std::move(detail);
            }
        }

        SGCL_INLINE_HOT void set_detail(std::string detail) noexcept {
            _detail = std::move(detail);
        }

        // The longest text the writing may make: a string's, for
        // json::to_string and stringify. An indented text grows with the
        // square of the depth (a value 100000 deep indented by 255 is
        // 1.3e12 characters), so the writing stops once the text would
        // grow past it, as at a mistake (weighed where the block grows,
        // JsonText); too_long() then, with failed() and no code of its own
        SGCL_INLINE_HOT void limit(size_t n) noexcept {
            _text.limit(n);
        }

        SGCL_INLINE_HOT bool too_long() const noexcept {
            return _text.over();
        }

        SGCL_INLINE_HOT void begin(bool object) noexcept {
            if (!_before_value()) {
                return;
            }
            _text += object ? '{' : '[';
            _stack.push_back(Level{object, false, false});
        }

        void end(bool object) noexcept {
            if (_text.stopped()) {
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

        void key(std::string_view name) noexcept {
            if (_text.stopped()) {
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

        SGCL_INLINE_HOT void null() noexcept {
            if (_before_value()) {
                _text += "null";
                _value_done();
            }
        }

        SGCL_INLINE_HOT void boolean(bool b) noexcept {
            if (_before_value()) {
                _text += b ? "true" : "false";
                _value_done();
            }
        }

        template<class I>
        SGCL_INLINE_HOT void integer(I v) noexcept {
            if (_before_value()) {
                char buf[NumberTextSize];
                _text.append(buf, integer_text(buf, v));
                _value_done();
            }
        }

        // A double or a float, each with its own shortest digits; NaN and
        // the infinities are not JSON: unsupported_value
        template<class F>
        SGCL_INLINE_HOT void floating(F v) noexcept {
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
        SGCL_INLINE_HOT void literal(std::string_view text) noexcept {
            if (_before_value()) {
                _text.append(text);
                _value_done();
            }
        }

        SGCL_INLINE_HOT void quoted(std::string_view s) noexcept {
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

        SGCL_INLINE_HOT void _newline() noexcept {
            _text += '\n';
            _text.append(_stack.size() * _indent, ' ');
        }

        bool _before_value() noexcept {
            if (_text.stopped()) {
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

        SGCL_INLINE_HOT void _value_done() noexcept {
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
        errc _code = errc::syntax;
    };

    // The text block and the stack of levels a one-shot writing works in
    // (json::to_string, stringify): lent by the thread for the call and
    // given back, emptied, at its end, so that the calls after the first
    // allocate nothing but the string they return. The text of a call is
    // copied into that string before the lending ends (the return value is
    // made before the locals go). A few of each are kept, for a writing
    // inside a writing; one past what is kept, or a block past KeepBytes,
    // is let go (a thread that once wrote a text of megabytes does not keep
    // its room). Plain memory: nothing in a block or a stack is a tracked
    // word.
    class JsonLent {
    public:
        explicit JsonLent(JsonOut& out) noexcept
        : _out(out) {
            auto& pool = _pool();
            if (pool.count) {
                auto& kept = pool.slots[--pool.count];
                _out._text.lend(std::move(kept.block), kept.capacity);
                _out._stack = std::move(kept.stack);
                kept.capacity = 0;
            }
        }

        JsonLent(const JsonLent&) = delete;
        JsonLent& operator=(const JsonLent&) = delete;

        ~JsonLent() {
            auto& pool = _pool();
            size_t capacity = 0;
            auto block = _out._text.give_back(capacity);
            if (block && capacity <= KeepBytes && pool.count < KeepSlots) {
                auto& kept = pool.slots[pool.count++];
                kept.block = std::move(block);
                kept.capacity = capacity;
                _out._stack.clear();
                kept.stack = std::move(_out._stack);
            }
        }

    private:
        static constexpr size_t KeepBytes = size_t(64) << 10;
        static constexpr size_t KeepSlots = 4;

        struct Kept {
            std::unique_ptr<char[]> block;
            size_t capacity = 0;
            std::vector<JsonOut::Level> stack;
        };

        struct Pool {
            Kept slots[KeepSlots];
            size_t count = 0;
        };

        static Pool& _pool() noexcept {
            thread_local Pool pool;
            return pool;
        }

        JsonOut& _out;
    };
}
