//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "fields.h"
#include "detail/json_number.h"
#include "detail/text_buffer.h"
#include "detail/text_scan.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/generator.h"
#include "../core/make_tracked.h"
#include "../core/map.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        template<class T>
        struct CsvRecordWriter;
    }

    // CSV (RFC 4180, with Go's extensions): records of fields, a record a
    // line, a field in quotes when it holds the separator, a quote or a
    // line ending, "" for a quote inside. csv::reader reads the records
    // (csv::row), by index or by the name of the header's column, and a
    // record as a type of the program (fields.h); csv::writer writes them.
    // What Go's encoding/csv is, with the header and the types on top.
    class csv {
    public:
        using error = encoding::error;

        struct options {
            char separator = ',';
            char comment = 0;                  // 0: none; '#': a line starting with it is skipped
            bool trim_leading_space = false;   // the white space at a field's start dropped
            bool lazy_quotes = false;          // a quote in an unquoted field kept, as Go's LazyQuotes
            bool same_field_count = true;      // every record as long as the first (Go: FieldsPerRecord 0)
            size_t max_record_size = size_t(16) << 20;   // what a reader of a stream holds at once, a record: errc::out_of_range past it
        };

        class row;
        class reader;
        class writer;

    private:
        csv() = delete;
    };

    namespace detail {
        // The names of a header's columns, shared by its rows
        struct CsvHeader {
            vector<string> names;
            sgcl::map<string, uint32_t> index;
        };
    }

    // A record: its fields as text, their places in the input, and the
    // header's names when the reader read one. The fields are slices of
    // one string of the row's own, so a row kept is valid on its own,
    // after the reader has gone on.
    class csv::row {
    public:
        row() = default;

        size_t size() const noexcept {
            return _meta.size() / 3;
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        // The field; index < size(), as a vector's operator[] asks (a debug
        // build asserts it); at() is the form that checks and throws
        slice<const char> operator[](size_t index) const {
            assert(index < size() && "csv::row: the index past the fields; at() checks");
            uint32_t from = index ? _meta[(index - 1) * 3] : 0;
            uint32_t to = _meta[index * 3];
            return _text.as_slice(from, to - from);
        }

        // The field, or out_of_range past the fields
        slice<const char> at(size_t index) const {
            if (index >= size()) {
                throw out_of_range("sgcl::encoding::csv::row::at");
            }
            return (*this)[index];
        }

        // The field of the header's column; nullopt when there is no such
        // column (or no header) or the row is shorter
        optional<slice<const char>> operator[](const string& column) const {
            if (!_header) {
                return nullopt;
            }
            auto it = _header->index.find(column);
            if (it == _header->index.end() || it->second >= size()) {
                return nullopt;
            }
            return (*this)[it->second];
        }

        template<size_t N>
        optional<slice<const char>> operator[](const char (&column)[N]) const {
            return (*this)[string(column)];
        }

        // The line the record starts on (a quoted field may span several)
        uint32_t line() const noexcept {
            return _meta.empty() ? 0 : _meta[1];
        }

        // The line and the column (in code points) where the field starts
        // (Go's FieldPos)
        pair<uint32_t, uint32_t> position(size_t index) const noexcept {
            if (index >= size()) {
                return {0, 0};
            }
            return {_meta[index * 3 + 1], _meta[index * 3 + 2]};
        }

        // The fields in order: for (auto field : r)
        class iterator {
        public:
            using value_type = slice<const char>;
            using difference_type = ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            iterator() = default;

            slice<const char> operator*() const {
                return (*_row)[_i];
            }

            iterator& operator++() noexcept {
                ++_i;
                return *this;
            }

            iterator operator++(int) noexcept {
                auto t = *this;
                ++_i;
                return t;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._i == b._i;
            }

        private:
            friend class row;

            iterator(const row* r, size_t i) noexcept
            : _row(r), _i(i) {
            }

            const row* _row = nullptr;
            size_t _i = 0;
        };

        iterator begin() const noexcept {
            return iterator(this, 0);
        }

        iterator end() const noexcept {
            return iterator(this, size());
        }

    private:
        friend class csv::reader;

        string _text;                // the fields, one after another
        vector<uint32_t> _meta;      // for each field: its end in _text, its line, its column
        tracked_ptr<const detail::CsvHeader> _header;
    };

    namespace detail {
        // A byte of white space that trim_leading_space drops at a field's
        // start, as Go's unicode.IsSpace: the ASCII ones here, the others
        // (U+0085, U+00A0, U+1680, U+2000...) by their UTF-8 below
        inline bool csv_ascii_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\v' || c == '\f';
        }

        // The length of a Unicode space other than ASCII's at p, 0 when
        // there is none, -1 when the data ends inside one that may be
        inline int csv_unicode_space(const char* p, const char* end) noexcept {
            auto b = uint8_t(*p);
            if (b != 0xC2 && b != 0xE1 && b != 0xE2 && b != 0xE3) {
                return 0;
            }
            auto at = [&](int k) -> int { return p + k < end ? uint8_t(p[k]) : -1; };
            int b1 = at(1);
            if (b1 < 0) {
                return -1;
            }
            if (b == 0xC2) {
                return b1 == 0x85 || b1 == 0xA0 ? 2 : 0;
            }
            int b2 = at(2);
            if (b2 < 0) {
                return -1;
            }
            if (b == 0xE1) {
                return b1 == 0x9A && b2 == 0x80 ? 3 : 0;   // U+1680
            }
            if (b == 0xE3) {
                return b1 == 0x80 && b2 == 0x80 ? 3 : 0;   // U+3000
            }
            // 0xE2: U+2000..U+200A, U+2028, U+2029, U+202F, U+205F
            if (b1 == 0x80 && ((b2 >= 0x80 && b2 <= 0x8A) || b2 == 0xA8 || b2 == 0xA9 || b2 == 0xAF)) {
                return 3;
            }
            if (b1 == 0x81 && b2 == 0x9F) {
                return 3;
            }
            return 0;
        }

        // Characters that grow, unmanaged (no tracked pointer in them): the
        // fields of the record being read, appended inline
        class CsvChars {
        public:
            void clear() noexcept {
                _size = 0;
            }

            size_t size() const noexcept {
                return _size;
            }

            const char* data() const noexcept {
                return _data.get();
            }

            std::string_view view() const noexcept {
                return std::string_view(_data.get(), _size);
            }

            void push_back(char c) {
                if (_size == _capacity) {
                    _grow(_size + 1);
                }
                _data[_size++] = c;
            }

            void append(const char* p, size_t n) {
                if (n > _capacity - _size) {
                    _grow(_size + n);
                }
                copy_bytes(_data.get() + _size, p, n);
                _size += n;
            }

        private:
            void _grow(size_t need) {
                size_t capacity = std::max<size_t>({need, _capacity * 2, 256});
                auto d = std::make_unique<char[]>(capacity);
                if (_size) {
                    std::memcpy(d.get(), _data.get(), _size);
                }
                _data = std::move(d);
                _capacity = capacity;
            }

            std::unique_ptr<char[]> _data;
            size_t _size = 0;
            size_t _capacity = 0;
        };

        // The code points of [p, end): the bytes that do not continue a
        // UTF-8 sequence, eight at a time
        inline uint32_t code_points(const char* p, const char* end) noexcept {
            uint32_t n = 0;
            while (end - p >= 8) {
                uint64_t w = load_word(p);
                uint64_t continuation = w & ~(w << 1) & Highs;   // 10xxxxxx
                n += 8 - uint32_t(std::popcount(continuation));
                p += 8;
            }
            for (; p != end; ++p) {
                n += (uint8_t(*p) & 0xC0) != 0x80;
            }
            return n;
        }
    }

    // CSV read a record at a time from a text or a stream. A record ends
    // at a line ending ('\n', or "\r\n", whose '\r' is dropped); a field
    // in quotes may hold the separator, "" for a quote, and line endings
    // (a "\r\n" inside is read as '\n', as Go reads it). An empty line is
    // skipped, and with options.comment a line starting with it. The
    // first mistake stops the reader: next() returns nullopt, and
    // last_error() says what and where — a quote in an unquoted field, a
    // character after a closing quote, a quoted field not closed, a record
    // of another length than the first — with its line and column (in
    // code points). A record cut by the end of a block goes on where it
    // stopped: its fields gathered so far are kept, nothing is read twice.
    //
    // Each method reads on the thread that calls it; a task uses the
    // async_ forms: `co_await r.async_next()`.
    class csv::reader {
    public:
        explicit reader(const string& text)
        : reader(text, options()) {
        }

        reader(const string& text, const options& o)
        : _text(text), _options(o), _eof(true) {
            _check(o);
            _owner = text.as_slice().owner();
            _d = text.data();
            _n = text.size();
        }

        explicit reader(const io::reader& in)
        : reader(in, options()) {
        }

        reader(const io::reader& in, const options& o)
        : _in(in), _options(o) {
            _check(o);
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;

        // The next record, which becomes the header: its fields are the
        // names of the columns, which the rows after it are asked by
        // (row[name]) and read<T> fills its fields by
        optional<row> read_header() {
            auto r = next();
            _take_header(r);
            return r;
        }

        // The next record; nullopt at the end of the input or at an error
        optional<row> next() {
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    auto room = _room();
                    _received(_in.read(room));
                    continue;
                }
                return _result(s);
            }
        }

        // The records to the end, in a range-for: for (auto r : reader.rows())
        generator<row> rows() {
            while (auto r = next()) {
                co_yield std::move(*r);
            }
        }

        // The next record as a T: the fields of T by the header's column
        // names (the first record is taken for the header when
        // read_header was not called); a field whose column is not there
        // keeps its value (required(): missing_field), a column no field
        // has is skipped. The fields are what one text holds — a number,
        // a boolean, a string, an enum with names, a type with to_text,
        // an optional of one (an empty field is nullopt).
        template<class T>
        optional<T> read();

        // In a task: `co_await r.async_next()`
        async::task<optional<row>> async_next() {
            for (;;) {
                Step s = _step();
                if (s == Step::more) {
                    auto room = _room();
                    _received(co_await _in.async_read(room));
                    continue;
                }
                co_return _result(s);
            }
        }

        async::task<optional<row>> async_read_header() {
            auto r = co_await async_next();
            _take_header(r);
            co_return r;
        }

        template<class T>
        async::task<optional<T>> async_read();

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // The header read, empty when none was
        slice<const string> header() const noexcept {
            if (!_header) {
                return {};
            }
            return _header->names.as_slice();
        }

        // The line the next record starts on
        uint32_t line() const noexcept {
            return _line;
        }

    private:
        enum class Step : uint8_t {
            record,
            more,
            end,
            failed
        };

        enum class State : uint8_t {
            line_start,     // a record's first byte: a comment, an empty line, or a field
            field_start,    // a field's first byte (after the separator)
            unquoted,
            quoted,
            quote,          // a quote in a quoted field: "" or its end
            after_cr,       // a '\r' outside quotes: a line ending when '\n' follows
            quoted_cr,      // a '\r' inside quotes
            comment         // a comment line, to its end
        };

        static void _check(const options& o) {
            char s = o.separator;
            if (s == '"' || s == '\r' || s == '\n' || s == 0 || uint8_t(s) >= 0x80) {
                throw invalid_argument("sgcl::encoding::csv: the separator is a quote, a line ending, NUL or not ASCII");
            }
            if (o.comment == s || o.comment == '"' || o.comment == '\r' || o.comment == '\n' || uint8_t(o.comment) >= 0x80) {
                throw invalid_argument("sgcl::encoding::csv: the comment character is the separator, a quote, a line ending or not ASCII");
            }
        }

        void _take_header(const optional<row>& r) {
            if (!r) {
                return;
            }
            auto h = make_tracked<detail::CsvHeader>();
            for (size_t i = 0; i < r->size(); ++i) {
                string name((*r)[i]);
                h->names.push_back(name);
                h->index.try_emplace(name, uint32_t(i));
            }
            _header = tracked_ptr<const detail::CsvHeader>(std::move(h));
            _columns_of = nullptr;
        }

        // --- the record, a byte at a time through the states, the runs of
        // plain bytes found by the search of text_scan.h ---

        void _begin_field() {
            _field_line = _line;
            _field_column = _column;
        }

        void _end_field() {
            _meta.push_back(uint32_t(_text_out.size()));
            _meta.push_back(_field_line);
            _meta.push_back(_field_column);
        }

        // consumes [_pos, q) of the data, counting the columns
        void _advance(size_t q) noexcept {
            if (q != _pos) {
                _ended_line = false;
            }
            _column += detail::code_points(_d + _pos, _d + q);
            _pos = q;
        }

        // A line ending consumed: the column past it kept, for the place
        // of a quoted field the input ends in
        void _newline() noexcept {
            _last_line_end = _column;
            ++_line;
            _column = 1;
            _ended_line = true;
        }

        // The error at the byte the reader is at, or `back` bytes before
        // it on the same line
        Step _fail(errc code, std::string detail, uint32_t back = 0) {
            error e(code, _base + _pos - back, string(detail));
            e.set_position(_line, _column - back);
            _error = std::move(e);
            return Step::failed;
        }

        Step _step() {
            if (_error) {
                return Step::failed;
            }
            const char sep = _options.separator;
            for (;;) {
                if (_pos == _n) {
                    if (!_eof) {
                        return Step::more;
                    }
                    return _at_end();
                }
                char c = _d[_pos];
                if (c == '\r' && _pos + 1 == _n) {
                    // a '\r' that ends the input is dropped, as Go drops
                    // it: one that ends the data so far waits for the next
                    if (!_eof) {
                        return Step::more;
                    }
                    _pos = _n;
                    continue;
                }
                switch (_state) {
                    case State::line_start:
                        if (c == '\n') {
                            _advance(_pos + 1);
                            _newline();
                            continue;
                        }
                        if (c == '\r' && _d[_pos + 1] == '\n') {
                            // an empty line ended with "\r\n"
                            _advance(_pos + 2);
                            _newline();
                            continue;
                        }
                        if (_options.comment && c == _options.comment) {
                            _state = State::comment;
                            continue;
                        }
                        _record_line = _line;
                        _text_out.clear();
                        _meta.clear();
                        _state = State::field_start;
                        [[fallthrough]];
                    case State::field_start:
                        if (_options.trim_leading_space) {
                            if (detail::csv_ascii_space(c)) {
                                _advance(_pos + 1);
                                continue;
                            }
                            if (c == '\r' && _d[_pos + 1] != '\n') {
                                // a space to Go too, unless it is a line ending's
                                _advance(_pos + 1);
                                continue;
                            }
                            int u = detail::csv_unicode_space(_d + _pos, _d + _n);
                            if (u < 0 && !_eof) {
                                return Step::more;
                            }
                            if (u > 0) {
                                _advance(_pos + size_t(u));
                                continue;
                            }
                        }
                        _begin_field();
                        if (c == '"') {
                            _advance(_pos + 1);
                            _state = State::quoted;
                            continue;
                        }
                        _state = State::unquoted;
                        continue;
                    case State::unquoted: {
                        const char* q = detail::find_any_of(_d + _pos, _d + _n, sep, '"', '\n', '\r');
                        size_t at = size_t(q - _d);
                        _text_out.append(_d + _pos, at - _pos);
                        _advance(at);
                        if (at == _n) {
                            continue;
                        }
                        if (*q == sep) {
                            _end_field();
                            _advance(at + 1);
                            _state = State::field_start;
                            continue;
                        }
                        if (*q == '"') {
                            if (!_options.lazy_quotes) {
                                return _fail(errc::syntax, "bare \" in a field without quotes");
                            }
                            _text_out.push_back('"');
                            _advance(at + 1);
                            continue;
                        }
                        if (*q == '\r') {
                            if (at + 1 == _n) {
                                continue;   // the last byte so far: the loop's start decides
                            }
                            _state = State::after_cr;
                            _advance(at + 1);
                            continue;
                        }
                        // '\n': the record ends
                        _end_field();
                        _advance(at + 1);
                        _newline();
                        _state = State::line_start;
                        return Step::record;
                    }
                    case State::after_cr:
                        // "\r\n" ends the record; a '\r' alone is a byte of the field
                        if (c == '\n') {
                            _end_field();
                            _advance(_pos + 1);
                            _newline();
                            _state = State::line_start;
                            return Step::record;
                        }
                        _text_out.push_back('\r');
                        _state = State::unquoted;
                        continue;
                    case State::quoted: {
                        const char* q = detail::find_any_of(_d + _pos, _d + _n, '"', '\n', '\r', '"');
                        size_t at = size_t(q - _d);
                        _text_out.append(_d + _pos, at - _pos);
                        _advance(at);
                        if (at == _n) {
                            continue;
                        }
                        if (*q == '"') {
                            _advance(at + 1);
                            _state = State::quote;
                            continue;
                        }
                        if (*q == '\r') {
                            if (at + 1 == _n) {
                                continue;
                            }
                            _advance(at + 1);
                            _state = State::quoted_cr;
                            continue;
                        }
                        _text_out.push_back('\n');
                        _advance(at + 1);
                        _newline();
                        continue;
                    }
                    case State::quoted_cr:
                        // "\r\n" inside quotes is read as '\n'; a '\r' alone stays
                        if (c == '\n') {
                            _text_out.push_back('\n');
                            _advance(_pos + 1);
                            _newline();
                            --_last_line_end;   // Go's line has the "\r\n" as one '\n'
                        } else {
                            _text_out.push_back('\r');
                        }
                        _state = State::quoted;
                        continue;
                    case State::quote:
                        if (c == '"') {
                            _text_out.push_back('"');
                            _advance(_pos + 1);
                            _state = State::quoted;
                            continue;
                        }
                        if (c == sep) {
                            _end_field();
                            _advance(_pos + 1);
                            _state = State::field_start;
                            continue;
                        }
                        if (c == '\n') {
                            _end_field();
                            _advance(_pos + 1);
                            _newline();
                            _state = State::line_start;
                            return Step::record;
                        }
                        if (c == '\r' && _d[_pos + 1] == '\n') {
                            _end_field();
                            _advance(_pos + 2);
                            _newline();
                            _state = State::line_start;
                            return Step::record;
                        }
                        if (_options.lazy_quotes) {
                            // the quote was a byte of the field
                            _text_out.push_back('"');
                            _state = State::quoted;
                            continue;
                        }
                        // at the quote, which is where Go says it is
                        return _fail(errc::syntax, "extraneous or missing \" in a quoted field", 1);
                    case State::comment: {
                        const char* q = static_cast<const char*>(std::memchr(_d + _pos, '\n', _n - _pos));
                        if (!q) {
                            _advance(_n);
                            continue;
                        }
                        _advance(size_t(q - _d) + 1);
                        _newline();
                        _state = State::line_start;
                        continue;
                    }
                }
            }
        }

        // The end of the input: the record it cuts ends there, a quoted
        // field is not closed
        Step _at_end() {
            switch (_state) {
                case State::line_start:
                case State::comment:
                    return Step::end;
                case State::quoted:
                case State::quoted_cr:
                    if (!_options.lazy_quotes) {
                        // Go's place: past the last line read, the line
                        // ending counted, when the input ends with one
                        error e(errc::unexpected_end, _base + _pos, string("extraneous or missing \" in a quoted field"));
                        if (_ended_line) {
                            e.set_position(_line - 1, _last_line_end);
                        } else {
                            e.set_position(_line, _column);
                        }
                        _error = std::move(e);
                        return Step::failed;
                    }
                    if (_state == State::quoted_cr) {
                        _text_out.push_back('\r');   // a '\r' no '\n' followed: a byte of the field
                    }
                    break;
                case State::after_cr:
                    _text_out.push_back('\r');
                    break;
                default:
                    break;
            }
            if (_state == State::field_start) {
                _begin_field();
            }
            _end_field();
            _state = State::line_start;
            return Step::record;
        }

        // The record read has the count of fields the first one had
        bool _counted() {
            size_t fields = _meta.size() / 3;
            if (_options.same_field_count) {
                if (_fields == 0) {
                    _fields = fields;
                } else if (fields != _fields) {
                    error e(errc::field_count, _base + _pos, string("wrong number of fields: " + std::to_string(fields) + ", the first record has " + std::to_string(_fields)));
                    e.set_position(_record_line, 1);
                    _error = std::move(e);
                    return false;
                }
            }
            return true;
        }

        optional<row> _result(Step s) {
            if (s != Step::record || !_counted()) {
                return nullopt;
            }
            row r;
            r._text = string(_text_out.view());
            r._meta = vector<uint32_t>(_meta.begin(), _meta.end());
            r._header = _header;
            return r;
        }

        // --- the input ---

        slice<byte> _room() {
            if (_pos > 0) {
                _block.drop_front(_pos);
                _base += _pos;
                _pos = 0;
            }
            if (_block.size() + _text_out.size() >= _options.max_record_size) {   // a record of a stream longer than the bound: the part assembled and the part not yet read
                _refresh();
                error e(errc::out_of_range, _base, string("a record longer than options.max_record_size (" + std::to_string(_options.max_record_size) + " bytes)"));
                e.set_position(_line, _column);
                _error = std::move(e);
                return slice<byte>();
            }
            if (_block.size() == _block.capacity()) {
                _block.reserve(_block.capacity() + 1);
            }
            _refresh();
            return slice<byte>(_block.owner(), reinterpret_cast<byte*>(_block.data() + _block.size()), _block.capacity() - _block.size());
        }

        void _received(const expected<size_t, io::error>& r) {
            if (_error) {
                return;
            }
            if (!r) {
                error e(r.error(), _base + _n);
                e.set_position(_line, _column);
                _error = std::move(e);
                return;
            }
            if (*r == 0) {
                _eof = true;
            } else {
                _block.resize(_block.size() + *r);
            }
            _refresh();
        }

        void _refresh() noexcept {
            _owner = _block.owner();
            _d = _block.data();
            _n = _block.size();
        }

        template<class T>
        optional<T> _typed();

        string _text;
        io::reader _in;
        detail::TextBuffer _block;
        tracked_ptr<const void> _owner;
        const char* _d = nullptr;
        size_t _n = 0;
        size_t _pos = 0;
        uint64_t _base = 0;
        options _options;
        bool _eof = false;
        // the record being read
        State _state = State::line_start;
        detail::CsvChars _text_out;
        std::vector<uint32_t> _meta;
        uint32_t _line = 1;
        uint32_t _column = 1;
        uint32_t _last_line_end = 1;   // the column past the last line ending
        bool _ended_line = false;      // nothing consumed since the last line ending
        uint32_t _record_line = 1;
        uint32_t _field_line = 1;
        uint32_t _field_column = 1;
        size_t _fields = 0;
        tracked_ptr<const detail::CsvHeader> _header;
        // read<T>: the column of each field of the type read last, and its fields
        field_list _record_fields;
        const detail::ValueOps* _columns_of = nullptr;
        std::vector<uint32_t> _columns;
        optional<error> _error;
    };

    // CSV written a record at a time: the fields quoted where they must be
    // (they hold the separator, a quote, a line ending, or start with a
    // space; a field `\.` too, as Go quotes it for Postgres), a quote
    // doubled, each record ended with '\n' (use_crlf: "\r\n", which RFC 4180
    // wants). The text gathers in the writer, flush() hands it to the
    // stream. A record written as a type of the program (fields.h) writes
    // the header of its field names first.
    class csv::writer {
    public:
        explicit writer(const io::writer& out)
        : writer(out, options()) {
        }

        writer(const io::writer& out, const options& o)
        : _out(out), _options(o) {
            if (o.separator == '"' || o.separator == '\r' || o.separator == '\n' || o.separator == 0 || uint8_t(o.separator) >= 0x80) {
                throw invalid_argument("sgcl::encoding::csv: the separator is a quote, a line ending, NUL or not ASCII");
            }
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;

        writer& use_crlf() noexcept {
            _crlf = true;
            return *this;
        }

        writer& write(std::initializer_list<string> fields) {
            bool first = true;
            for (auto& f : fields) {
                _field(f.view(), first);
                first = false;
            }
            _end_record();
            return *this;
        }

        writer& write(const row& r) {
            bool first = true;
            for (auto f : r) {
                _field(f.view(), first);
                first = false;
            }
            _end_record();
            return *this;
        }

        // Any range of texts (string, slice, std::string, const char*)
        template<class R>
        requires std::ranges::input_range<const R&> && (std::is_convertible_v<std::ranges::range_reference_t<const R&>, std::string_view> || std::is_convertible_v<std::ranges::range_reference_t<const R&>, string> || std::is_same_v<std::remove_cvref_t<std::ranges::range_reference_t<const R&>>, slice<const char>>)
        writer& write(const R& fields) {
            bool first = true;
            for (auto&& f : fields) {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(f)>, string> || std::is_same_v<std::remove_cvref_t<decltype(f)>, slice<const char>>) {
                    _field(f.view(), first);
                } else {
                    _field(std::string_view(f), first);
                }
                first = false;
            }
            _end_record();
            return *this;
        }

        // A record of a type of the program: its fields as the columns, the
        // header of their names written before the first
        template<class T>
        requires detail::HasDescribe<T> || detail::HasFreeDescribe<T>
        writer& write(const T& record);

        // The text so far to the stream; the first mistake (a field of a
        // kind CSV has no text for) or the stream's failure
        expected<void, io::error> flush() {
            if (_error) {
                return io::detail::fail(*_error);
            }
            if (!_text.empty()) {
                auto w = _out.write(slice<const byte>(reinterpret_cast<const byte*>(_text.data()), _text.size()));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
                _text.clear();
            }
            return {};
        }

        async::task<expected<void, io::error>> async_flush() {
            if (_error) {
                co_return io::detail::fail(*_error);
            }
            if (!_text.empty()) {
                auto w = co_await _out.async_write(slice<const byte>(reinterpret_cast<const byte*>(_text.data()), _text.size()));
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
                _text.clear();
            }
            co_return expected<void, io::error>();
        }

    private:
        bool _needs_quotes(std::string_view f) const noexcept {
            if (f.empty()) {
                return false;
            }
            if (f == "\\.") {
                return true;
            }
            for (char c : f) {
                if (c == _options.separator || c == '"' || c == '\r' || c == '\n') {
                    return true;
                }
            }
            if (detail::csv_ascii_space(f[0]) || f[0] == '\r' || f[0] == '\n') {
                return true;
            }
            return detail::csv_unicode_space(f.data(), f.data() + f.size()) > 0;
        }

        void _field(std::string_view f, bool first) {
            if (!first) {
                _text += _options.separator;
            }
            if (!_needs_quotes(f)) {
                _text.append(f);
                return;
            }
            _text += '"';
            for (char c : f) {
                switch (c) {
                    case '"':
                        _text += "\"\"";
                        break;
                    case '\r':
                        if (!_crlf) {
                            _text += '\r';
                        }
                        break;
                    case '\n':
                        _text += _crlf ? "\r\n" : "\n";
                        break;
                    default:
                        _text += c;
                }
            }
            _text += '"';
        }

        void _end_record() {
            _text += _crlf ? "\r\n" : "\n";
        }

        template<class T>
        friend struct detail::CsvRecordWriter;

        io::writer _out;
        options _options;
        std::string _text;
        optional<io::error> _error;
        bool _crlf = false;
        bool _header_written = false;
    };

    // --- records as types ---

    namespace detail {
        // The text of a field of a type for a CSV field, or false when CSV
        // has none for its kind (a container, a record): unsupported_value
        inline bool csv_field_text(const void* p, const ValueOps* ops, const field_list& list, const FieldInfo& f, std::string& out) {
            switch (ops->kind) {
                case ValueKind::boolean:
                    out = ops->get_bool(p) ? "true" : "false";
                    return true;
                case ValueKind::signed_integer:
                case ValueKind::unsigned_integer:
                case ValueKind::floating: {
                    if (ops->kind == ValueKind::floating && !std::isfinite(ops->get_double(p))) {
                        double d = ops->get_double(p);
                        out = std::isnan(d) ? "NaN" : d > 0 ? "+Inf" : "-Inf";
                        return true;
                    }
                    char buf[NumberTextSize];
                    out.assign(buf, ops->number_text(p, buf));
                    return true;
                }
                case ValueKind::enumeration:
                    if (f.names_count) {
                        int64_t i = ops->get_int(p);
                        if (i < 0 || uint64_t(i) >= f.names_count) {
                            return false;
                        }
                        out.assign(FieldAccess::name(list, f, size_t(i)));
                        return true;
                    } else {
                        char buf[NumberTextSize];
                        out.assign(buf, ops->number_text(p, buf));
                        return true;
                    }
                case ValueKind::string:
                case ValueKind::text:
                    out.assign(ops->get_text(p).view());
                    return true;
                case ValueKind::optional:
                case ValueKind::pointer:
                    if (!ops->has_value(p)) {
                        out.clear();
                        return true;
                    }
                    return csv_field_text(ops->value(p), ops->inner(), list, f, out);
                default:
                    return false;
            }
        }

        // A field of a type set from a CSV field: 0 done, 1 not a value of
        // the field's type, 2 out of its range, 3 a kind CSV has no text for
        inline int csv_set_field(void* p, const ValueOps* ops, const field_list& list, const FieldInfo& f, std::string_view t) {
            switch (ops->kind) {
                case ValueKind::boolean:
                    // strconv.ParseBool's words
                    if (t == "1" || t == "t" || t == "T" || t == "TRUE" || t == "true" || t == "True") {
                        ops->set_bool(p, true);
                        return 0;
                    }
                    if (t == "0" || t == "f" || t == "F" || t == "FALSE" || t == "false" || t == "False") {
                        ops->set_bool(p, false);
                        return 0;
                    }
                    return 1;
                case ValueKind::signed_integer:
                case ValueKind::unsigned_integer:
                case ValueKind::floating:
                    return ops->set_literal(p, t);
                case ValueKind::enumeration:
                    if (f.names_count) {
                        size_t i = FieldAccess::index_of(list, f, t);
                        return i < f.names_count && ops->set_int(p, int64_t(i)) ? 0 : 1;
                    }
                    return ops->set_literal(p, t);
                case ValueKind::string:
                case ValueKind::text:
                    return ops->set_text(p, t) ? 0 : 1;
                case ValueKind::optional:
                case ValueKind::pointer:
                    if (t.empty()) {
                        ops->reset(p);
                        return 0;
                    }
                    return csv_set_field(ops->emplace(p), ops->inner(), list, f, t);
                default:
                    return 3;
            }
        }

        template<class T>
        struct CsvRecordWriter {
            static void write(csv::writer& w, const T& record) {
                if (w._error) {
                    return;
                }
                field_list fields;
                describe_of(const_cast<T&>(record), fields);
                auto& list = FieldAccess::fields(fields);
                if (!w._header_written) {
                    w._header_written = true;
                    bool first = true;
                    for (auto& f : list) {
                        w._field(f.name, first);
                        first = false;
                    }
                    w._end_record();
                }
                std::string text;
                bool first = true;
                for (auto& f : list) {
                    if (!csv_field_text(f.address, f.ops, fields, f, text)) {
                        w._error = io::error(make_error_code(errc::unsupported_value), string("csv: /" + std::string(f.name) + ": a value CSV has no text for"));
                        return;
                    }
                    w._field(text, first);
                    first = false;
                }
                w._end_record();
            }
        };
    }

    template<class T>
    requires detail::HasDescribe<T> || detail::HasFreeDescribe<T>
    csv::writer& csv::writer::write(const T& record) {
        detail::CsvRecordWriter<T>::write(*this, record);
        return *this;
    }

    // A record as a T, from the fields of the record just read (no row is
    // made): the columns of T's fields found once for the type and kept
    template<class T>
    optional<T> csv::reader::_typed() {
        static_assert(std::is_default_constructible_v<T>, "csv::reader::read<T>: T needs a default constructor");
        if (!_counted()) {
            return nullopt;
        }
        optional<T> out(std::in_place);   // the record made where it is returned: no move of its strings
        T& value = *out;
        field_list& fields = _record_fields;   // kept from record to record: no allocation a record
        detail::FieldAccess::clear(fields);
        detail::describe_of(value, fields);
        auto& list = detail::FieldAccess::fields(fields);
        const detail::ValueOps* type = detail::value_ops<T>();
        if (_columns_of != type || _columns.size() != list.size()) {
            _columns.clear();
            for (auto& f : list) {
                auto it = _header->index.find(string(f.name));
                _columns.push_back(it == _header->index.end() ? UINT32_MAX : it->second);
            }
            _columns_of = type;
        }
        size_t count = _meta.size() / 3;
        for (size_t k = 0; k < list.size(); ++k) {
            auto& f = list[k];
            uint32_t c = _columns[k];
            if (c == UINT32_MAX || c >= count) {
                if (f.flags & detail::Required) {
                    error e(errc::missing_field, _base + _pos, string("missing column"));
                    e.set_position(_record_line, 1);
                    e.set_path(string("/" + std::string(f.name)));
                    _error = std::move(e);
                    out.reset();
                    return out;
                }
                continue;
            }
            uint32_t from = c ? _meta[(c - 1) * 3] : 0;
            std::string_view field(_text_out.data() + from, _meta[c * 3] - from);
            int s = detail::csv_set_field(f.address, f.ops, fields, f, field);
            if (s) {
                errc code = s == 2 ? errc::out_of_range : s == 3 ? errc::unsupported_value : errc::type_mismatch;
                std::string what = s == 3 ? "a field CSV has no text for" : s == 2 ? "\"" + std::string(field.substr(0, 40)) + "\" is out of the field's range" : "\"" + std::string(field.substr(0, 40)) + "\" is not " + f.ops->name;
                error e(code, _base + _pos, string(what));
                e.set_position(_meta[c * 3 + 1], _meta[c * 3 + 2]);
                e.set_path(string("/" + std::string(f.name)));
                _error = std::move(e);
                out.reset();
                return out;
            }
        }
        return out;
    }

    template<class T>
    optional<T> csv::reader::read() {
        if (!_header) {
            read_header();
            if (!_header) {
                return nullopt;
            }
        }
        for (;;) {
            Step s = _step();
            if (s == Step::more) {
                auto room = _room();
                _received(_in.read(room));
                continue;
            }
            if (s != Step::record) {
                return nullopt;
            }
            return _typed<T>();
        }
    }

    template<class T>
    async::task<optional<T>> csv::reader::async_read() {
        if (!_header) {
            co_await async_read_header();
            if (!_header) {
                co_return nullopt;
            }
        }
        for (;;) {
            Step s = _step();
            if (s == Step::more) {
                auto room = _room();
                _received(co_await _in.async_read(room));
                continue;
            }
            if (s != Step::record) {
                co_return nullopt;
            }
            co_return _typed<T>();
        }
    }
}
