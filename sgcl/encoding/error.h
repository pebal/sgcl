//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/position.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../io/error.h"

#include <cstdint>
#include <string>
#include <system_error>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    // What went wrong in the input of a format of the encoding module. One
    // list for every format, so that a program handling the errors of JSON,
    // CSV and base64 learns one set of names; a format uses the codes that
    // mean something for it and no other. The values start at 1: an
    // error_code of 0 is success, and a code of this list travels as an
    // error_code inside an io::error when a decoder is read as a stream.
    enum class errc : uint8_t {
        syntax = 1,             // a character the grammar does not allow here
        unexpected_end,         // the input ends in the middle of a value
        invalid_character,      // a byte outside the alphabet (base64, hex, ...)
        invalid_utf8,
        invalid_escape,         // \x in JSON, &#xD800; in XML
        depth_limit,
        duplicate_key,
        out_of_range,           // 1e400, 300 read into an uint8_t, a varint past 64 bits
        type_mismatch,          // "abc" where the field is a number
        missing_field,          // a field marked required() is absent
        unknown_field,          // only with options.reject_unknown_fields
        unsupported_value,      // NaN when writing, a cycle in a graph
        field_count,            // CSV: a row with more or fewer fields than the first
        mismatched_tag,         // XML: </b> after <a>
        undefined_entity,       // XML: &nbsp; with no DTD to define it
        unsupported_encoding,   // XML: encoding="Shift_JIS"
        io                      // the source or the sink failed: io_error() says how
    };

    namespace detail {
        class EncodingCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "encoding";
            }

            std::string message(int c) const override {
                switch (static_cast<errc>(c)) {
                    case errc::syntax: return "syntax error";
                    case errc::unexpected_end: return "unexpected end of input";
                    case errc::invalid_character: return "invalid character";
                    case errc::invalid_utf8: return "invalid UTF-8";
                    case errc::invalid_escape: return "invalid escape sequence";
                    case errc::depth_limit: return "nesting too deep";
                    case errc::duplicate_key: return "duplicate key";
                    case errc::out_of_range: return "value out of range";
                    case errc::type_mismatch: return "type mismatch";
                    case errc::missing_field: return "missing field";
                    case errc::unknown_field: return "unknown field";
                    case errc::unsupported_value: return "unsupported value";
                    case errc::field_count: return "wrong number of fields";
                    case errc::mismatched_tag: return "mismatched tag";
                    case errc::undefined_entity: return "undefined entity";
                    case errc::unsupported_encoding: return "unsupported encoding";
                    case errc::io: return "input/output error";
                }
                return "unknown encoding error";
            }
        };
    }

    inline const std::error_category& encoding_category() noexcept {
        static const detail::EncodingCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), encoding_category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::encoding::errc> : std::true_type {};

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    // The error of every format of the module, one type under each
    // format's name (base64::error, pem::error, later json::error and
    // csv::error): the code, the byte of the input where it was found,
    // the line and the column when the format has lines, the path inside
    // the structure when it has one, and the error of the stream when the
    // input came from one and that failed. A value: copied, compared,
    // held in an expected.
    //
    // A position costs nothing until something fails. The offset is what
    // a decoder has anyway; the line and the column are counted from the
    // text after the fact, on the path of the error alone (locate), so
    // that a decoding that succeeds never counts a line ending.
    class error {
    public:
        error() = default;

        // The code at the offset; the detail, when given, is what
        // message() says in place of the code's own words ("invalid
        // character '*'", "expected a number, found a string")
        error(errc code, uint64_t offset, const string& detail = {})
        : _code(code), _offset(offset), _detail(detail) {
        }

        // The source or the sink failed at the offset
        error(const io::error& e, uint64_t offset)
        : _code(errc::io), _offset(offset), _io(e) {
        }

        errc code() const noexcept {
            return _code;
        }

        // Bytes from the start of the input
        uint64_t offset() const noexcept {
            return _offset;
        }

        // From 1; 0 when the format has no lines (base64) or the input
        // was not text in memory
        uint32_t line() const noexcept {
            return _line;
        }

        // From 1, in code points: the column a reader counts, not the
        // byte (which offset() gives)
        uint32_t column() const noexcept {
            return _column;
        }

        // Where in the structure: "/users/3/age" (a JSON Pointer),
        // "/catalog/book[2]/@id" (XML); empty where it does not apply
        const string& path() const noexcept {
            return _path;
        }

        const optional<io::error>& io_error() const noexcept {
            return _io;
        }

        // "offset 17: invalid character '*'", "3:14: expected a number",
        // "3:14 /users/3/age: type mismatch"
        string message() const {
            std::string m;
            if (_line) {
                m += std::to_string(_line);
                m += ':';
                m += std::to_string(_column);
            } else {
                m += "offset ";
                m += std::to_string(_offset);
            }
            if (!_path.empty()) {
                m += ' ';
                m.append(_path.data(), _path.size());
            }
            m += ": ";
            if (!_detail.empty()) {
                m.append(_detail.data(), _detail.size());
            } else {
                m += encoding_category().message(static_cast<int>(_code));
            }
            if (_io) {
                m += ": ";
                auto t = _io->message();
                m.append(t.data(), t.size());
            }
            return string(m);
        }

        // The line and the column of offset() in the text the input was:
        // counted now, on the error's path, and never on the good one
        error& locate(const string& text) noexcept {
            auto p = detail::position_of(text.view(), _offset);
            _line = p.line;
            _column = p.column;
            return *this;
        }

        error& set_position(uint32_t line, uint32_t column) noexcept {
            _line = line;
            _column = column;
            return *this;
        }

        error& set_path(const string& path) {
            _path = path;
            return *this;
        }

        // Everything it says: the code, the place, the words and the
        // stream's error (by its code, as io::error compares)
        friend bool operator==(const error& a, const error& b) noexcept {
            return a._code == b._code && a._offset == b._offset && a._line == b._line && a._column == b._column
                && a._path == b._path && a._detail == b._detail && a._io == b._io;
        }

    private:
        errc _code = errc::syntax;
        uint32_t _line = 0;
        uint32_t _column = 0;
        uint64_t _offset = 0;
        string _detail;
        string _path;
        optional<io::error> _io;
    };

    namespace detail {
        // The error as a stream reports it: a decoder read through
        // io::reader fails with an io::error of the encoding category
        // ("decode base64: invalid character"), the one of its source
        // passed on as it was
        inline io::error to_io_error(const error& e, const char* format) {
            if (e.io_error()) {
                return *e.io_error();
            }
            return io::error(make_error_code(e.code()), "decode", format);
        }

        // A byte as a message shows it: 'x' when it is printable ASCII,
        // 0xNN otherwise
        inline std::string quoted_byte(uint8_t b) {
            if (b >= 0x20 && b < 0x7F) {
                return std::string("'") + char(b) + "'";
            }
            static constexpr char Digits[] = "0123456789ABCDEF";
            return std::string("0x") + Digits[b >> 4] + Digits[b & 15];
        }
    }
}
