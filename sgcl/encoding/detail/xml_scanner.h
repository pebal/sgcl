//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "xml_names.h"
#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/dynamic_array.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/set.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/utf8.h"
#include "../../core/vector.h"
#include "../../io/error.h"
#include "../../txt/encoding.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    // An element open in the document: its names, for the end tag to be
    // checked against and to carry, and how many namespace bindings it
    // brought, to be dropped with it
    struct XmlOpen {
        string name;
        string local;
        string uri;
        uint32_t bindings = 0;
    };

    // A prefix bound to a namespace by an xmlns attribute, and the binding
    // of the same prefix it hides (-1: none)
    struct XmlBinding {
        string prefix;
        string uri;
        int32_t previous = -1;
    };

    // The reader of XML under xml::reader and xml::parse: bytes in, tokens
    // out, one at a time, and nothing held of the document past the token
    // being read. A template over the public class only so that this header
    // stands on its own while the token it fills is a member of xml.
    //
    // How it goes. The input is UTF-8 in a buffer — the caller's string
    // as it is, or a managed buffer a stream is read into; a document in
    // UTF-16 or in a single byte encoding is turned into UTF-8 on the way
    // in, block by block. A token is read from the buffer in one pass once
    // all of it is there. When the buffer ends inside a token, the reader
    // says so (more) and, while the stream brings the rest, a small finder
    // follows the new bytes looking for where the token ends (a '<' after
    // text, a '>' outside quotes after a tag, --> after a comment, ...);
    // the token is read again only when that end is there. So a token that
    // arrives a byte at a time costs a pass over its bytes and one reading,
    // not a reading per byte: the quadratic time a stream parser falls into
    // on a large token fed slowly is not there.
    //
    // What is refused, by construction: no DTD is interpreted — the
    // DOCTYPE declaration is checked for its shape and passed on as a
    // token, its internal subset skipped — so no entity but the five
    // predefined ones exists, an external entity is never loaded, and an
    // entity expanding into more entities ("billion laughs") has nothing to
    // expand. A reference to any other entity is errc::undefined_entity.
    // The limits for input from outside: the depth of elements
    // (options::max_depth) and the size of one token held in the buffer
    // (options::max_token_size).
    template<class Xml>
    class XmlScanner {
    public:
        using attribute = typename Xml::attribute_type;
        using token = typename Xml::token;
        using options = typename Xml::options;
        using token_kind = typename token::kind;

        enum class Step : uint8_t {
            token,      // a token was read
            more,       // the input ends inside a token: give room() bytes
            end,        // the document ended, well formed
            failed      // last_error() says what went wrong
        };

        // A document in memory, read where it lies
        XmlScanner(const string& text, const options& o)
        : _options(o), _text(text), _memory(true) {
            _init();
            _base = _text.data();
            _end = _text.size();
            _eof = true;
        }

        // A document a stream brings: room() and received() feed it
        explicit XmlScanner(const options& o)
        : _options(o) {
            _init();
            _buffer.resize(config::io_buffer_size);
            _base = _buffer.data();
        }

        Step step(token& out) {
            for (;;) {
                if (_error) {
                    return Step::failed;
                }
                if (_ended) {
                    return Step::end;
                }
                if (_close_pending) {
                    _close_pending = false;
                    _end_element(out);
                    return Step::token;
                }
                if (!_sniffed) {
                    if (!_sniff()) {
                        return _error ? Step::failed : Step::more;
                    }
                    continue;
                }
                if (_waiting) {
                    if (!_eof && !(_end > _waited_end && _find_end())) {
                        return _more();
                    }
                    _waiting = false;
                }
                if (_pos == _end) {
                    if (!_eof) {
                        return _more();
                    }
                    if (_deferred) {
                        _take_deferred();
                        return Step::failed;
                    }
                    if (!_open.empty()) {
                        _fail(errc::unexpected_end, _end, "the document ends inside <" + std::string(_open.back().name.view()) + ">");
                        return Step::failed;
                    }
                    if (!_root_done) {
                        _fail(errc::unexpected_end, _end, "no root element");
                        return Step::failed;
                    }
                    _ended = true;
                    return Step::end;
                }
                auto r = _token(out);
                if (r == Parse::done) {
                    _first = false;
                    return Step::token;
                }
                if (r == Parse::failed) {
                    return Step::failed;
                }
                if (r == Parse::more) {
                    _waiting = true;
                    _waited_end = _end;
                    _find_state = Find::classify;
                    _find_at = _pos;
                    _find_end();
                    return _more();
                }
                // Parse::again: the input changed under the token (the
                // encoding the declaration named); read it once more
            }
        }

        // Where the next bytes of the stream go: free room at the end of
        // the buffer, or the block a document in another encoding is read
        // into before it becomes UTF-8
        slice<byte> room() {
            if (_transcode != Transcode::none) {
                if (!_raw) {
                    _raw = make_tracked<array<byte, config::io_buffer_size>>();
                }
                return slice<byte>(_raw, _raw->data(), _raw->size());
            }
            _reserve(config::io_buffer_size / 2);
            return as_writable_bytes(_buffer.as_slice(_end, _buffer.size() - _end));
        }

        // What a read into room() gave: n bytes, 0 at the end of the
        // stream, or the stream's error, which is reported once the tokens
        // before it are read
        void received(const expected<size_t, io::error>& r) {
            if (!r) {
                _deferred = error(r.error(), _original(_end));
                _eof = true;
                return;
            }
            size_t n = *r;
            if (n == 0) {
                _eof = true;
                if (_transcode == Transcode::utf16le || _transcode == Transcode::utf16be) {
                    if (_carry_size && !_deferred) {
                        _deferred = error(errc::unexpected_end, _raw_count, "the document ends inside a UTF-16 character");
                    }
                }
                return;
            }
            if (_transcode == Transcode::none) {
                _end += n;
            } else {
                _decode(reinterpret_cast<const uint8_t*>(_raw->data()), n);
            }
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // The byte of the input where the next token starts
        uint64_t offset() const noexcept {
            return _original(_pos);
        }

        uint32_t depth() const noexcept {
            return uint32_t(_open.size());
        }

        // "/catalog/book": the elements open, for a message
        string path() const {
            std::string p;
            for (auto& o : _open) {
                p += '/';
                p.append(o.name.data(), o.name.size());
            }
            return string(p);
        }

    private:
        enum class Parse : uint8_t {
            done,
            more,
            failed,
            again
        };

        enum class Transcode : uint8_t {
            none,
            utf16le,
            utf16be,
            single
        };

        // What the finder is inside of, while a token cut by the end of
        // the buffer waits for its end
        enum class Find : uint8_t {
            classify,
            text,
            tag,
            tag_quote,
            comment,
            cdata,
            instruction,
            doctype,
            doctype_quote,
            subset,
            subset_quote,
            subset_comment,
            subset_instruction,
            found
        };

        // Positions a parsing step returns in place of an index
        static constexpr size_t Short = size_t(-2);    // the data ended; more is coming
        static constexpr size_t Failed = size_t(-1);   // _error is set

        void _init() {
            _cache = dynamic_array<string>(CacheSize);
        }

        // --- the input -------------------------------------------------

        Step _more() {
            if (!_memory && _end - _pos > _options.max_token_size) {
                _fail(errc::out_of_range, _pos, "a token longer than options.max_token_size ("
                    + std::to_string(_options.max_token_size) + " bytes)");
                return Step::failed;
            }
            return Step::more;
        }

        // At least n bytes of room past the end: the bytes already read
        // moved to the front first, the buffer grown only when a token
        // fills it
        void _reserve(size_t n) {
            if (_buffer.size() - _end >= n) {
                return;
            }
            if (_pos > 0) {
                _drop();
            }
            if (_buffer.size() - _end < n) {
                size_t size = std::max(_buffer.size() * 2, _end + n);
                _buffer.resize(size);
                _base = _buffer.data();
            }
        }

        // The bytes before the token being read are gone for good: what a
        // position needs of them (lines, the column, the characters an
        // offset in another encoding counts) is counted now
        void _drop() {
            const char* d = _buffer.data();
            size_t n = _pos;
            _lines += uint64_t(std::count(d, d + n, '\n'));
            const char* last = d + n;
            while (last != d && last[-1] != '\n') {
                --last;
            }
            uint64_t tail = _code_points(last, d + n);
            _column = last == d ? _column + tail : tail;
            if (_transcode != Transcode::none) {
                _dropped_points += _code_points(d, d + n);
                _dropped_supplementary += _supplementary(d, d + n);
            }
            std::memmove(_buffer.data(), d + n, _end - n);
            _dropped += n;
            _end -= n;
            _pos = 0;
            if (_waiting) {
                _waited_end -= n;
                _find_at -= n;
            }
        }

        static uint64_t _code_points(const char* p, const char* e) noexcept {
            uint64_t n = 0;
            for (; p != e; ++p) {
                n += (uint8_t(*p) & 0xC0) != 0x80;
            }
            return n;
        }

        static uint64_t _supplementary(const char* p, const char* e) noexcept {
            uint64_t n = 0;
            for (; p != e; ++p) {
                n += uint8_t(*p) >= 0xF0;
            }
            return n;
        }

        // The byte of the input a position of the buffer stands for: the
        // same byte for UTF-8, and counted back through the characters for
        // a document that was turned into UTF-8
        uint64_t _original(size_t at) const noexcept {
            uint64_t utf8 = _dropped + at;
            if (_transcode == Transcode::none) {
                return utf8;
            }
            uint64_t points = _dropped_points + _code_points(_base, _base + at) - _origin_points;
            if (_transcode == Transcode::single) {
                return _origin_bytes + points;
            }
            uint64_t supplementary = _dropped_supplementary + _supplementary(_base, _base + at) - _origin_supplementary;
            return _origin_bytes + 2 * points + 2 * supplementary;
        }

        // --- errors ----------------------------------------------------

        // The error at a position of the buffer, with its line and column
        // counted now, on the error's path alone
        void _fail(errc code, size_t at, const std::string& what) {
            if (_error) {
                return;
            }
            at = std::min(at, _end);
            error e(code, _original(at), string(what));
            auto [line, column] = _position(at);
            e.set_position(line, column);
            e.set_path(path());
            _error = std::move(e);
        }

        // The line and the column of a position of the buffer, both from 1
        pair<uint32_t, uint32_t> _position(size_t at) const noexcept {
            const char* d = _base;
            uint64_t lines = _lines + uint64_t(std::count(d, d + at, '\n'));
            const char* last = d + at;
            while (last != d && last[-1] != '\n') {
                --last;
            }
            uint64_t column = _code_points(last, d + at) + (last == d ? _column : 0);
            return {uint32_t(std::min<uint64_t>(lines + 1, UINT32_MAX)), uint32_t(std::min<uint64_t>(column + 1, UINT32_MAX))};
        }

        // The failure of the input itself — the stream's error, a UTF-16
        // unit without its pair — found when the data read before it ran
        // out: with the line, the column and the path of that place
        void _take_deferred() {
            error e = std::move(*_deferred);
            _deferred.reset();
            auto [line, column] = _position(_end);
            e.set_position(line, column);
            e.set_path(path());
            _error = std::move(e);
        }

        size_t _failed(errc code, size_t at, const std::string& what) {
            _fail(code, at, what);
            return Failed;
        }

        // The data ended at `at`: more is coming, or it never will and the
        // token is cut
        size_t _short(size_t at, const char* inside) {
            if (!_eof) {
                return Short;
            }
            if (_deferred) {
                _take_deferred();
                return Failed;
            }
            return _failed(errc::unexpected_end, at, std::string("the document ends inside ") + inside);
        }

        static std::string _shown(char32_t c) {
            static constexpr char Digits[] = "0123456789ABCDEF";
            std::string s = "U+";
            int shift = c > 0xFFFF ? 20 : 12;
            for (; shift >= 0; shift -= 4) {
                s += Digits[(c >> shift) & 15];
            }
            if (c >= 0x21 && c < 0x7F) {
                s += " '";
                s += char(c);
                s += '\'';
            }
            return s;
        }

        // A byte that is not ASCII, or a control: the character it begins,
        // checked to be one XML holds. The width, Short, or Failed.
        size_t _character(size_t i) {
            uint8_t b = uint8_t(_base[i]);
            if (b < 0x80) {
                if (b < 0x20 && b != '\t' && b != '\n' && b != '\r') {
                    return _failed(errc::invalid_character, i, "a character XML does not allow: " + _shown(b));
                }
                return 1;
            }
            auto r = xml_rune(_base + i, _base + _end);
            if (!r.width) {
                if (_end - i < 4 && !_eof) {
                    return Short;
                }
                return _failed(errc::invalid_utf8, i, "invalid UTF-8");
            }
            if (!xml_char(r.c)) {
                return _failed(errc::invalid_character, i, "a character XML does not allow: " + _shown(r.c));
            }
            return r.width;
        }

        // --- strings ---------------------------------------------------

        // A string of the document's bytes. A short one — the names of
        // elements and attributes, the white space between elements, the
        // small values that come back again and again — is looked up in a
        // small table of the strings made last and shared when it is
        // there, so that a thousand <book> elements hold one "book".
        static constexpr size_t CacheSize = 256;
        static constexpr size_t CachedLength = 24;

        string _string(const char* p, size_t n) {
            if (n == 0) {
                return string();
            }
            if (n > CachedLength) {
                return string(std::string_view(p, n));
            }
            uint32_t h = 2166136261u;
            for (size_t k = 0; k < n; ++k) {
                h = (h ^ uint8_t(p[k])) * 16777619u;
            }
            auto& slot = _cache[(h ^ (h >> 16)) & (CacheSize - 1)];
            if (slot.size() == n && std::memcmp(slot.data(), p, n) == 0) {
                return slot;
            }
            slot = string(std::string_view(p, n));
            return slot;
        }

        string _string(const std::string& s) {
            return _string(s.data(), s.size());
        }

        // Bytes with their line endings as XML 1.0 2.11 has them: CR LF
        // and a CR alone are each LF
        string _line_ends(const char* p, size_t n) {
            if (!std::memchr(p, '\r', n)) {
                return string(std::string_view(p, n));
            }
            std::string& t = _scratch;
            t.clear();
            for (size_t k = 0; k < n; ++k) {
                if (p[k] == '\r') {
                    t += '\n';
                    k += k + 1 < n && p[k + 1] == '\n';
                } else {
                    t += p[k];
                }
            }
            return string(t);
        }

        // --- where the document starts ---------------------------------

        // The encoding from the first bytes (XML 1.0, appendix F): a byte
        // order mark, or UTF-16 without one (the '<?' of a declaration in
        // two bytes a character); everything else is read as UTF-8 until a
        // declaration says otherwise. false: more bytes are needed, or it
        // failed.
        bool _sniff() {
            if (_end < 4 && !_eof) {
                return false;
            }
            _sniffed = true;
            auto at = [&](size_t i) -> int { return i < _end ? uint8_t(_base[i]) : -1; };
            int b0 = at(0), b1 = at(1), b2 = at(2), b3 = at(3);
            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
                _pos = 3;
                _utf8_bom = true;
                _start = 3;
                return true;
            }
            if ((b0 == 0 && b1 == 0 && ((b2 == 0xFE && b3 == 0xFF) || (b2 == 0 && b3 == 0x3C)))
                || (b0 == 0xFF && b1 == 0xFE && b2 == 0 && b3 == 0) || (b0 == 0x3C && b1 == 0 && b2 == 0 && b3 == 0)) {
                _fail(errc::unsupported_encoding, 0, "a document in UTF-32, which this reader does not read");
                return false;
            }
            if (b0 == 0x4C && b1 == 0x6F && b2 == 0xA7 && b3 == 0x94) {
                _fail(errc::unsupported_encoding, 0, "a document in EBCDIC, which this reader does not read");
                return false;
            }
            if ((b0 == 0xFE && b1 == 0xFF) || (b0 == 0xFF && b1 == 0xFE)) {
                _begin_transcoding(b0 == 0xFE ? Transcode::utf16be : Transcode::utf16le, 2);
                return !_error;
            }
            if ((b0 == 0 && b1 == 0x3C && b2 == 0 && b3 == 0x3F) || (b0 == 0x3C && b1 == 0 && b2 == 0x3F && b3 == 0)) {
                _begin_transcoding(b0 == 0 ? Transcode::utf16be : Transcode::utf16le, 0);
                return !_error;
            }
            return true;
        }

        // From here on the input is in another encoding: the bytes from
        // `from` that were taken as they are become UTF-8, and what a
        // stream brings later goes through the block
        void _begin_transcoding(Transcode t, size_t from) {
            // A single byte encoding starts past the declaration, whose
            // ASCII was read as UTF-8 and stays; UTF-16 starts at the front
            // (past its byte order mark), and nothing before it is text
            std::string raw(_base + from, _end - from);
            size_t keep = t == Transcode::single ? from : 0;
            _origin_bytes = _original(from);
            _origin_points = _dropped_points + _code_points(_base, _base + keep);
            _origin_supplementary = _dropped_supplementary + _supplementary(_base, _base + keep);
            _raw_count = _origin_bytes;
            if (_memory) {
                _memory = false;
                _buffer.resize(std::max<size_t>(keep + raw.size() * 3 + 16, config::io_buffer_size));
                copy_bytes(_buffer.data(), _base, keep);
                _base = _buffer.data();
            }
            _end = keep;
            if (t != Transcode::single) {
                _pos = 0;
                _start = 0;
            }
            _transcode = t;
            _decode(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
            if (_eof && _carry_size && !_deferred) {
                _deferred = error(errc::unexpected_end, _raw_count, "the document ends inside a UTF-16 character");
            }
        }

        // Bytes of the input's encoding appended as UTF-8
        void _decode(const uint8_t* p, size_t n) {
            if (_transcode == Transcode::single) {
                auto text = txt::decode(slice<const byte>(reinterpret_cast<const byte*>(p), n), _single);
                _reserve(text.size());
                copy_bytes(_buffer.data() + _end, text.data(), text.size());
                _end += text.size();
                _raw_count += n;
                return;
            }
            _reserve(n * 2 + 8);
            char* o = _buffer.data() + _end;
            bool big = _transcode == Transcode::utf16be;
            auto unit = [big](const uint8_t* q) { return big ? uint32_t(q[0]) << 8 | q[1] : uint32_t(q[1]) << 8 | q[0]; };
            size_t i = 0;
            // the carry: an odd byte, or a high surrogate waiting for its pair
            while (_carry_size && i < n) {
                _carry[_carry_size++] = p[i++];
                if (_carry_size == 2) {
                    uint32_t u = unit(_carry);
                    if (u >= 0xD800 && u < 0xDC00) {
                        continue;
                    }
                    if (u >= 0xDC00 && u < 0xE000) {
                        _lone_surrogate(o, _raw_count);
                        return;
                    }
                    o += utf8::encode(char32_t(u), o);
                    _carry_size = 0;
                    _raw_count += 2;
                } else if (_carry_size == 4) {
                    uint32_t high = unit(_carry), low = unit(_carry + 2);
                    if (low < 0xDC00 || low >= 0xE000) {
                        _lone_surrogate(o, _raw_count);
                        return;
                    }
                    o += utf8::encode(char32_t(0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)), o);
                    _carry_size = 0;
                    _raw_count += 4;
                }
            }
            while (i + 1 < n) {
                uint32_t u = unit(p + i);
                if (u < 0xD800 || u >= 0xE000) {
                    o += utf8::encode(char32_t(u), o);
                    i += 2;
                    _raw_count += 2;
                    continue;
                }
                if (u >= 0xDC00) {
                    _lone_surrogate(o, _raw_count);
                    return;
                }
                if (i + 3 >= n) {
                    break;
                }
                uint32_t low = unit(p + i + 2);
                if (low < 0xDC00 || low >= 0xE000) {
                    _lone_surrogate(o, _raw_count);
                    return;
                }
                o += utf8::encode(char32_t(0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00)), o);
                i += 4;
                _raw_count += 4;
            }
            while (i < n) {
                _carry[_carry_size++] = p[i++];
            }
            _end = size_t(o - _buffer.data());
        }

        // A surrogate of UTF-16 without its pair: the text up to it is
        // read, and then the reader stops there
        void _lone_surrogate(char* o, uint64_t at) {
            _end = size_t(o - _buffer.data());
            if (!_deferred) {
                _deferred = error(errc::invalid_character, at, "a UTF-16 surrogate without its pair");
            }
            _eof = true;
            _carry_size = 0;
        }

        // --- the finder: where a cut token ends ------------------------

        // Follows the bytes from _find_at to the end of the buffer; true
        // once the token that starts at _pos has its end there. Mirrors the
        // shape the reading of the token expects, so that it says "there"
        // when the reading will not want more; for a token that is not
        // well formed it may say so early, and the reading then fails.
        bool _find_end() {
            const char* d = _base;
            size_t e = _end;
            size_t i = _find_at;
            auto search = [&](char c) {
                auto p = static_cast<const char*>(std::memchr(d + i, c, e - i));
                i = p ? size_t(p - d) : e;
                return p != nullptr;
            };
            // a terminator of three characters (-->, ]]>) or two (?>): the
            // search goes on from two bytes back, so one cut in the middle
            // is found when the rest comes
            auto search_end = [&](const char* t, size_t n, size_t floor) {
                i = std::max(i >= n - 1 ? i - (n - 1) : 0, floor);
                for (; i + n <= e; ++i) {
                    if (std::memcmp(d + i, t, n) == 0) {
                        i += n;
                        return true;
                    }
                }
                i = std::max(i, e >= n - 1 ? e - (n - 1) : 0);
                return false;
            };
            for (;;) {
                switch (_find_state) {
                    case Find::classify: {
                        size_t n = e - _pos;
                        if (n < 1) {
                            _find_at = _pos;
                            return false;
                        }
                        if (d[_pos] != '<') {
                            _find_state = Find::text;
                            i = _pos + 1;
                            break;
                        }
                        if (n < 2) {
                            return false;
                        }
                        char c = d[_pos + 1];
                        if (c == '?') {
                            _find_state = Find::instruction;
                            i = _pos + 2;
                        } else if (c == '!') {
                            if (n < 4) {
                                return false;
                            }
                            if (d[_pos + 2] == '-' && d[_pos + 3] == '-') {
                                _find_state = Find::comment;
                                i = _pos + 4;
                            } else if (d[_pos + 2] == '[') {
                                _find_state = Find::cdata;
                                i = _pos + 3;
                            } else if (d[_pos + 2] == 'D') {
                                _find_state = Find::doctype;
                                i = _pos + 2;
                            } else {
                                _find_state = Find::found;
                            }
                        } else {
                            _find_state = Find::tag;
                            i = _pos + 1;
                        }
                        break;
                    }
                    case Find::text:
                        if (!search('<')) {
                            _find_at = i;
                            return false;
                        }
                        _find_state = Find::found;
                        break;
                    case Find::tag:
                        for (; i < e && d[i] != '>' && d[i] != '"' && d[i] != '\''; ++i) {
                        }
                        if (i == e) {
                            _find_at = i;
                            return false;
                        }
                        if (d[i] == '>') {
                            _find_state = Find::found;
                        } else {
                            _quote = d[i++];
                            _find_state = Find::tag_quote;
                        }
                        break;
                    case Find::tag_quote:
                    case Find::doctype_quote:
                    case Find::subset_quote:
                        if (!search(_quote)) {
                            _find_at = i;
                            return false;
                        }
                        ++i;
                        _find_state = _find_state == Find::tag_quote ? Find::tag
                                    : _find_state == Find::doctype_quote ? Find::doctype : Find::subset;
                        break;
                    case Find::comment:
                    case Find::subset_comment: {
                        size_t floor = _find_state == Find::comment ? _pos + 4 : 0;
                        if (!search_end("-->", 3, floor)) {
                            _find_at = i;
                            return false;
                        }
                        _find_state = _find_state == Find::comment ? Find::found : Find::subset;
                        break;
                    }
                    case Find::cdata:
                        if (!search_end("]]>", 3, _pos + 3)) {
                            _find_at = i;
                            return false;
                        }
                        _find_state = Find::found;
                        break;
                    case Find::instruction:
                    case Find::subset_instruction: {
                        size_t floor = _find_state == Find::instruction ? _pos + 2 : 0;
                        if (!search_end("?>", 2, floor)) {
                            _find_at = i;
                            return false;
                        }
                        _find_state = _find_state == Find::instruction ? Find::found : Find::subset;
                        break;
                    }
                    case Find::doctype:
                        for (; i < e && d[i] != '>' && d[i] != '"' && d[i] != '\'' && d[i] != '['; ++i) {
                        }
                        if (i == e) {
                            _find_at = i;
                            return false;
                        }
                        if (d[i] == '>') {
                            _find_state = Find::found;
                        } else if (d[i] == '[') {
                            ++i;
                            _find_state = Find::subset;
                        } else {
                            _quote = d[i++];
                            _find_state = Find::doctype_quote;
                        }
                        break;
                    case Find::subset:
                        for (; i < e && d[i] != ']' && d[i] != '"' && d[i] != '\'' && d[i] != '<'; ++i) {
                        }
                        if (i == e) {
                            _find_at = i;
                            return false;
                        }
                        if (d[i] == ']') {
                            ++i;
                            _find_state = Find::doctype;
                        } else if (d[i] == '<') {
                            if (e - i < 4) {
                                _find_at = i;
                                return false;
                            }
                            if (d[i + 1] == '!' && d[i + 2] == '-' && d[i + 3] == '-') {
                                i += 4;
                                _find_state = Find::subset_comment;
                            } else if (d[i + 1] == '?') {
                                i += 2;
                                _find_state = Find::subset_instruction;
                            } else {
                                ++i;
                            }
                        } else {
                            _quote = d[i++];
                            _find_state = Find::subset_quote;
                        }
                        break;
                    case Find::found:
                        _find_at = i;
                        return true;
                }
            }
        }

        // --- tokens ----------------------------------------------------

        Parse _token(token& out) {
            const char* d = _base;
            if (d[_pos] != '<') {
                return _character_data(out);
            }
            if (_pos + 1 >= _end) {
                return _parse_short(_pos + 1, "a tag");
            }
            char c = d[_pos + 1];
            if (c == '/') {
                return _end_tag(out);
            }
            if (c == '?') {
                return _instruction(out);
            }
            if (c == '!') {
                if (_pos + 4 > _end && !(_eof && _end - _pos >= 2)) {
                    return _parse_short(_end, "a declaration");
                }
                if (_starts(_pos, "<!--")) {
                    return _comment(out);
                }
                if (_pos + 2 < _end && d[_pos + 2] == '[') {
                    if (_pos + 9 > _end && !_eof) {
                        return Parse::more;
                    }
                    if (!_starts(_pos, "<![CDATA[")) {
                        return _parse_fail(errc::syntax, _pos, "'<![' that begins no CDATA section");
                    }
                    return _cdata(out);
                }
                if (_pos + 9 > _end && !_eof && std::string_view("<!DOCTYPE").starts_with(std::string_view(d + _pos, _end - _pos))) {
                    return Parse::more;
                }
                if (_starts(_pos, "<!DOCTYPE")) {
                    return _doctype(out);
                }
                return _parse_fail(errc::syntax, _pos, "'<!' that begins no comment, CDATA section or DOCTYPE");
            }
            return _start_tag(out);
        }

        bool _starts(size_t i, std::string_view s) const noexcept {
            return _end - i >= s.size() && std::memcmp(_base + i, s.data(), s.size()) == 0;
        }

        Parse _parse_short(size_t at, const char* inside) {
            return _short(at, inside) == Short ? Parse::more : Parse::failed;
        }

        Parse _parse_fail(errc code, size_t at, const std::string& what) {
            _fail(code, at, what);
            return Parse::failed;
        }

        static Parse _status(size_t r) {
            return r == Short ? Parse::more : Parse::failed;
        }

        static bool _bad(size_t r) {
            return r >= Short;
        }

        // White space from i
        size_t _spaces(size_t i) const noexcept {
            while (i < _end && xml_space(_base[i])) {
                ++i;
            }
            return i;
        }

        // [5] Name from i: its end; i itself when no name starts there;
        // Short when the data ends inside it; Failed for invalid UTF-8
        size_t _name(size_t i) {
            const char* d = _base;
            size_t j = i;
            if (j >= _end) {
                return _short(j, "a name");
            }
            if (uint8_t(d[j]) < 0x80) {
                if (!(XmlBytes[uint8_t(d[j])] & XmlNameStart)) {
                    return i;
                }
                ++j;
            } else {
                auto r = xml_rune(d + j, d + _end);
                if (!r.width) {
                    if (_end - j < 4 && !_eof) {
                        return Short;
                    }
                    return _failed(errc::invalid_utf8, j, "invalid UTF-8");
                }
                if (!xml_name_start(r.c)) {
                    return i;
                }
                j += r.width;
            }
            for (;;) {
                while (j < _end && (XmlBytes[uint8_t(d[j])] & XmlNameChar)) {
                    ++j;
                }
                if (j >= _end) {
                    return _eof ? j : Short;
                }
                if (uint8_t(d[j]) < 0x80) {
                    return j;
                }
                auto r = xml_rune(d + j, d + _end);
                if (!r.width) {
                    if (_end - j < 4 && !_eof) {
                        return Short;
                    }
                    return _failed(errc::invalid_utf8, j, "invalid UTF-8");
                }
                if (!xml_name_char(r.c)) {
                    return j;
                }
                j += r.width;
            }
        }

        // A reference from i (the '&'), its character appended to out: the
        // index past its ';', Short or Failed
        size_t _reference(size_t i, std::string& out) {
            const char* d = _base;
            size_t j = i + 1;
            if (j >= _end) {
                return _short(j, "a reference");
            }
            if (d[j] == '#') {
                ++j;
                if (j >= _end) {
                    return _short(j, "a reference");
                }
                bool hex = d[j] == 'x';
                j += hex;
                uint32_t v = 0;
                size_t digits = 0;
                for (;; ++j, ++digits) {
                    if (j >= _end) {
                        return _short(j, "a reference");
                    }
                    char c = d[j];
                    uint32_t k;
                    if (c >= '0' && c <= '9') {
                        k = uint32_t(c - '0');
                    } else if (hex && (c | 0x20) >= 'a' && (c | 0x20) <= 'f') {
                        k = uint32_t((c | 0x20) - 'a' + 10);
                    } else {
                        break;
                    }
                    v = std::min<uint32_t>(v * (hex ? 16 : 10) + k, 0x110000);
                }
                if (!digits || d[j] != ';') {
                    return _failed(errc::invalid_escape, i, "a character reference without its digits or its ';'");
                }
                if (!xml_char(char32_t(v))) {
                    return _failed(errc::invalid_escape, i, "a reference to a character XML does not allow: "
                        + std::string(d + i, j + 1 - i));
                }
                char buf[4];
                out.append(buf, utf8::encode(char32_t(v), buf));
                return j + 1;
            }
            size_t n = _name(j);
            if (_bad(n)) {
                return n;
            }
            if (n == j) {
                return _failed(errc::syntax, i, "a '&' that begins no reference (write &amp;)");
            }
            if (n >= _end) {
                return _short(n, "a reference");
            }
            if (d[n] != ';') {
                return _failed(errc::syntax, i, "an entity reference without its ';'");
            }
            std::string_view name(d + j, n - j);
            char c = name == "lt" ? '<' : name == "gt" ? '>' : name == "amp" ? '&' : name == "apos" ? '\'' : name == "quot" ? '"' : 0;
            if (!c) {
                return _failed(errc::undefined_entity, i, "undefined entity &" + std::string(name) + "; (only the five of XML are known: no DTD is read)");
            }
            out += c;
            return n + 1;
        }

        // Character data up to the next '<'
        Parse _character_data(token& out) {
            const char* d = _base;
            size_t i = _pos;
            std::string& s = _scratch;
            bool plain = true;     // nothing changed yet: the text is the bytes [_pos, i)
            auto change = [&] {
                if (plain) {
                    s.assign(d + _pos, i - _pos);
                    plain = false;
                }
            };
            bool outside = _open.empty();
            for (;;) {
                size_t run = i;
                while (run < _end && (XmlBytes[uint8_t(d[run])] & XmlText)) {
                    ++run;
                }
                if (!plain) {
                    s.append(d + i, run - i);
                }
                i = run;
                if (i >= _end) {
                    if (!_eof) {
                        return Parse::more;
                    }
                    if (_deferred) {
                        _take_deferred();
                        return Parse::failed;
                    }
                    break;
                }
                char b = d[i];
                if (b == '<') {
                    break;
                }
                if (b == '&') {
                    if (outside) {
                        return _parse_fail(errc::syntax, i, "a reference outside the root element");
                    }
                    change();
                    size_t r = _reference(i, s);
                    if (_bad(r)) {
                        return _status(r);
                    }
                    i = r;
                    continue;
                }
                if (b == '\r') {
                    // a CR LF cut between two reads needs no care: text is
                    // read again whole once the '<' after it is there
                    change();
                    s += '\n';
                    i += (i + 1 < _end && d[i + 1] == '\n') ? 2 : 1;
                    continue;
                }
                if (b == ']') {
                    if (i + 2 >= _end && !_eof) {
                        return Parse::more;
                    }
                    if (i + 2 < _end && d[i + 1] == ']' && d[i + 2] == '>') {
                        return _parse_fail(errc::syntax, i, "']]>' in character data");
                    }
                    if (!plain) {
                        s += ']';
                    }
                    ++i;
                    continue;
                }
                size_t w = _character(i);
                if (_bad(w)) {
                    return _status(w);
                }
                if (!plain) {
                    s.append(d + i, w);
                }
                i += w;
            }
            if (outside) {
                for (size_t k = _pos; k < i; ++k) {
                    if (!xml_space(d[k])) {
                        return _parse_fail(errc::syntax, k, _root_done ? "text after the root element" : "text before the root element");
                    }
                }
            }
            out = token();
            out._kind = token_kind::text;
            out._text = plain ? _string(d + _pos, i - _pos) : _string(s);
            _pos = i;
            return Parse::done;
        }

        // The data of a comment, a CDATA section or an instruction, to the
        // terminator; the index past the terminator, Short or Failed. In a
        // comment, "--" before the end is an error.
        size_t _until(size_t i, std::string_view terminator, bool comment, std::string& s, const char* inside) {
            const char* d = _base;
            s.clear();
            for (;;) {
                size_t run = i;
                while (run < _end && (XmlBytes[uint8_t(d[run])] & XmlPlain)) {
                    ++run;
                }
                s.append(d + i, run - i);
                i = run;
                if (i >= _end) {
                    return _short(i, inside);
                }
                char b = d[i];
                if (b == terminator[0]) {
                    if (i + terminator.size() > _end) {
                        return _short(_end, inside);
                    }
                    if (std::memcmp(d + i, terminator.data(), terminator.size()) == 0) {
                        return i + terminator.size();
                    }
                    if (comment && d[i + 1] == '-') {
                        return _failed(errc::syntax, i, "'--' inside a comment");
                    }
                    s += b;
                    ++i;
                    continue;
                }
                if (b == '-' || b == '?' || b == ']') {
                    s += b;
                    ++i;
                    continue;
                }
                if (b == '\r') {
                    s += '\n';
                    i += (i + 1 < _end && d[i + 1] == '\n') ? 2 : 1;
                    continue;
                }
                size_t w = _character(i);
                if (_bad(w)) {
                    return w;
                }
                s.append(d + i, w);
                i += w;
            }
        }

        Parse _comment(token& out) {
            size_t r = _until(_pos + 4, "-->", true, _scratch, "a comment");
            if (_bad(r)) {
                return _status(r);
            }
            out = token();
            out._kind = token_kind::comment;
            out._text = _string(_scratch);
            _pos = r;
            return Parse::done;
        }

        Parse _cdata(token& out) {
            if (_open.empty()) {
                return _parse_fail(errc::syntax, _pos, "a CDATA section outside the root element");
            }
            size_t r = _until(_pos + 9, "]]>", false, _scratch, "a CDATA section");
            if (_bad(r)) {
                return _status(r);
            }
            out = token();
            out._kind = token_kind::text;
            out._text = _string(_scratch);
            _pos = r;
            return Parse::done;
        }

        // [16] PI: '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
        Parse _instruction(token& out) {
            const char* d = _base;
            size_t i = _pos + 2;
            size_t n = _name(i);
            if (_bad(n)) {
                return _status(n);
            }
            if (n == i) {
                return _parse_fail(errc::syntax, i, "a processing instruction without a target");
            }
            std::string_view target(d + i, n - i);
            if (target.size() == 3 && (target[0] | 0x20) == 'x' && (target[1] | 0x20) == 'm' && (target[2] | 0x20) == 'l') {
                if (target == "xml" && _first && _pos == _start) {
                    return _declaration(out, n);
                }
                return _parse_fail(errc::syntax, _pos, target == "xml" ? "an XML declaration that is not at the very start of the document"
                                                                          : "the target '" + std::string(target) + "' is reserved");
            }
            if (target.find(':') != std::string_view::npos) {
                return _parse_fail(errc::syntax, i, "a colon in the target of an instruction (Namespaces in XML)");
            }
            string name = _string(target.data(), target.size());
            size_t k = n;
            if (k >= _end) {
                return _parse_short(k, "an instruction");
            }
            if (d[k] != '?' && !xml_space(d[k])) {
                return _parse_fail(errc::syntax, k, "no white space after the target of an instruction");
            }
            k = _spaces(k);
            size_t r = _until(k, "?>", false, _scratch, "an instruction");
            if (_bad(r)) {
                return _status(r);
            }
            out = token();
            out._kind = token_kind::instruction;
            out._name = name;
            out._text = _string(_scratch);
            _pos = r;
            return Parse::done;
        }

        // [23] XMLDecl: '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>'
        Parse _declaration(token& out, size_t i) {
            const char* d = _base;
            // read only once the whole of it is there: a value cut by the
            // end of the data must not look like a declaration without it
            if (std::string_view(d + i, _end - i).find("?>") == std::string_view::npos) {
                return _parse_short(_end, "the XML declaration");
            }
            size_t content = _spaces(i);
            auto pseudo = [&](size_t& k, std::string_view name, std::string_view& value) -> size_t {
                // S name S? = S? quoted; returns 1 if there, 0 if absent
                size_t s = _spaces(k);
                if (s >= _end) {
                    return _short(s, "the XML declaration");
                }
                if (!_starts(s, name)) {
                    return 0;
                }
                if (s == k) {
                    return _failed(errc::syntax, s, "no white space before '" + std::string(name) + "' in the XML declaration");
                }
                size_t j = _spaces(s + name.size());
                if (j >= _end) {
                    return _short(j, "the XML declaration");
                }
                if (d[j] != '=') {
                    return _failed(errc::syntax, j, "'=' expected after '" + std::string(name) + "' in the XML declaration");
                }
                j = _spaces(j + 1);
                if (j >= _end) {
                    return _short(j, "the XML declaration");
                }
                char q = d[j];
                if (q != '"' && q != '\'') {
                    return _failed(errc::syntax, j, "a value in quotes expected in the XML declaration");
                }
                auto close = static_cast<const char*>(std::memchr(d + j + 1, q, _end - j - 1));
                if (!close) {
                    return _short(_end, "the XML declaration");
                }
                value = std::string_view(d + j + 1, size_t(close - d) - j - 1);
                k = size_t(close - d) + 1;
                return 1;
            };
            size_t k = i;
            std::string_view version, encoding, standalone;
            size_t r = pseudo(k, "version", version);
            if (_bad(r)) {
                return _status(r);
            }
            if (!r) {
                return _parse_fail(errc::syntax, _spaces(k), "the XML declaration without its version");
            }
            bool digits = version.size() > 2 && std::all_of(version.begin() + 2, version.end(), [](char c) { return c >= '0' && c <= '9'; });
            if (!version.starts_with("1.") || !digits) {
                return _parse_fail(errc::syntax, i, "a version that is not 1.x: '" + std::string(version) + "'");
            }
            r = pseudo(k, "encoding", encoding);
            if (_bad(r)) {
                return _status(r);
            }
            if (r) {
                bool valid = !encoding.empty() && ((encoding[0] | 0x20) >= 'a' && (encoding[0] | 0x20) <= 'z');
                for (char c : encoding) {
                    valid = valid && (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-');
                }
                if (!valid) {
                    return _parse_fail(errc::syntax, k - 1 - encoding.size(), "not the name of an encoding: '" + std::string(encoding) + "'");
                }
            }
            r = pseudo(k, "standalone", standalone);
            if (_bad(r)) {
                return _status(r);
            }
            if (r && standalone != "yes" && standalone != "no") {
                return _parse_fail(errc::syntax, k - 1 - standalone.size(), "standalone is 'yes' or 'no', not '" + std::string(standalone) + "'");
            }
            size_t e = _spaces(k);
            if (e + 2 > _end) {
                return _parse_short(_end, "the XML declaration");
            }
            if (d[e] != '?' || d[e + 1] != '>') {
                return _parse_fail(errc::syntax, e, "'?>' expected to end the XML declaration");
            }
            out = token();
            out._kind = token_kind::instruction;
            out._name = string("xml");
            out._text = _line_ends(d + content, e - content);
            size_t after = e + 2;
            if (!encoding.empty()) {
                auto switched = _declared(encoding, _pos, after);
                if (switched == Parse::failed) {
                    return switched;
                }
            }
            _pos = after;
            return Parse::done;
        }

        // The encoding a declaration names, against the one the document
        // was found in: UTF-8 and UTF-16 as they are, a single byte
        // encoding of txt (27 of them) from the byte after the declaration
        // on, anything else unsupported_encoding
        Parse _declared(std::string_view name, size_t at, size_t after) {
            auto lower = [](std::string_view s) {
                std::string l(s);
                for (auto& c : l) {
                    c = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
                }
                return l;
            };
            std::string n = lower(name);
            bool utf16 = n == "utf-16" || n == "utf-16le" || n == "utf-16be" || n == "ucs-2" || n == "iso-10646-ucs-2";
            bool utf8 = n == "utf-8" || n == "utf8";
            if (_transcode == Transcode::utf16le || _transcode == Transcode::utf16be) {
                if (!utf16) {
                    return _parse_fail(errc::unsupported_encoding, at, "the declaration names " + std::string(name) + " for a document in UTF-16");
                }
                return Parse::done;
            }
            if (utf8) {
                return Parse::done;
            }
            if (utf16) {
                return _parse_fail(errc::unsupported_encoding, at, "a document declared UTF-16 without the byte order mark UTF-16 needs");
            }
            if (_utf8_bom) {
                return _parse_fail(errc::unsupported_encoding, at, "the declaration names " + std::string(name) + " for a document with the byte order mark of UTF-8");
            }
            auto e = txt::encoding_from_name(string(name));
            bool single = e && (txt::detail::single_byte(*e) || *e == txt::encoding::ascii || *e == txt::encoding::latin1);
            if (!single) {
                return _parse_fail(errc::unsupported_encoding, at, "an encoding this reader does not read: " + std::string(name));
            }
            _single = *e;
            _begin_transcoding(Transcode::single, after);
            return _error ? Parse::failed : Parse::done;
        }

        // [40] STag, [44] EmptyElemTag
        Parse _start_tag(token& out) {
            const char* d = _base;
            size_t i = _pos + 1;
            size_t n = _name(i);
            if (_bad(n)) {
                return _status(n);
            }
            if (n == i) {
                return _parse_fail(errc::syntax, i, "a '<' that begins no tag (write &lt;)");
            }
            if (_open.empty() && _root_done) {
                return _parse_fail(errc::syntax, _pos, "a second root element");
            }
            size_t name_end = n;
            _attributes.clear();
            _attribute_at.clear();
            bool self = false;
            i = n;
            for (;;) {
                size_t s = _spaces(i);
                if (s >= _end) {
                    return _parse_short(s, "a tag");
                }
                char c = d[s];
                if (c == '>') {
                    i = s + 1;
                    break;
                }
                if (c == '/') {
                    if (s + 1 >= _end) {
                        return _parse_short(s + 1, "a tag");
                    }
                    if (d[s + 1] != '>') {
                        return _parse_fail(errc::syntax, s, "'/' not followed by '>' in a tag");
                    }
                    self = true;
                    i = s + 2;
                    break;
                }
                size_t a = _name(s);
                if (_bad(a)) {
                    return _status(a);
                }
                if (a == s) {
                    return _parse_fail(errc::syntax, s, "an attribute's name expected in the tag");
                }
                if (s == i) {
                    return _parse_fail(errc::syntax, s, "no white space before an attribute");
                }
                size_t j = _spaces(a);
                if (j >= _end) {
                    return _parse_short(j, "a tag");
                }
                if (d[j] != '=') {
                    return _parse_fail(errc::syntax, j, "an attribute without '=' and its value");
                }
                j = _spaces(j + 1);
                if (j >= _end) {
                    return _parse_short(j, "a tag");
                }
                char q = d[j];
                if (q != '"' && q != '\'') {
                    return _parse_fail(errc::syntax, j, "an attribute's value without quotes");
                }
                size_t v = _value(j + 1, q);
                if (_bad(v)) {
                    return _status(v);
                }
                _attributes.push_back(attribute{_string(d + s, a - s), _string(_scratch), string()});
                _attribute_at.push_back(s);
                i = v;
            }
            if (_open.size() >= _options.max_depth) {
                return _parse_fail(errc::depth_limit, _pos, "elements nested deeper than options.max_depth ("
                    + std::to_string(_options.max_depth) + ")");
            }
            if (!_namespaces(_pos + 1, name_end)) {
                return Parse::failed;
            }
            out = token();
            out._kind = token_kind::start_element;
            auto& top = _open.back();
            out._name = top.name;
            out._local = top.local;
            out._uri = top.uri;
            if (!_attributes.empty()) {
                out._attributes = dynamic_array<attribute>(_attributes.begin(), _attributes.end());
            }
            _close_pending = self;
            _pos = i;
            return Parse::done;
        }

        // [10] AttValue from i (past the quote q), normalized (3.3.3) into
        // _scratch: the index past the closing quote, Short or Failed
        size_t _value(size_t i, char q) {
            const char* d = _base;
            std::string& s = _scratch;
            s.clear();
            for (;;) {
                size_t run = i;
                while (run < _end && (XmlBytes[uint8_t(d[run])] & XmlValue)) {
                    ++run;
                }
                s.append(d + i, run - i);
                i = run;
                if (i >= _end) {
                    return _short(i, "an attribute's value");
                }
                char b = d[i];
                if (b == q) {
                    return i + 1;
                }
                if (b == '"' || b == '\'') {
                    s += b;
                    ++i;
                    continue;
                }
                if (b == '<') {
                    return _failed(errc::syntax, i, "'<' in an attribute's value (write &lt;)");
                }
                if (b == '&') {
                    size_t r = _reference(i, s);
                    if (_bad(r)) {
                        return r;
                    }
                    i = r;
                    continue;
                }
                if (b == '\t' || b == '\n') {
                    s += ' ';
                    ++i;
                    continue;
                }
                if (b == '\r') {
                    s += ' ';
                    i += (i + 1 < _end && d[i + 1] == '\n') ? 2 : 1;
                    continue;
                }
                size_t w = _character(i);
                if (_bad(w)) {
                    return w;
                }
                s.append(d + i, w);
                i += w;
            }
        }

        // The prefix of a QName, or an empty view
        static std::string_view _prefix(std::string_view qname) noexcept {
            auto c = qname.find(':');
            return c == std::string_view::npos ? std::string_view() : qname.substr(0, c);
        }

        // The namespace a prefix is bound to here; nullopt when none
        optional<string> _lookup(std::string_view prefix) {
            if (prefix.empty()) {
                return _default < 0 ? string() : _bindings[size_t(_default)].uri;
            }
            if (prefix == "xml") {
                return string(XmlNamespace);
            }
            if (prefix == "xmlns") {
                return string(XmlnsNamespace);
            }
            if (_prefixes.empty()) {
                return nullopt;
            }
            auto p = _prefixes.find(_string(prefix.data(), prefix.size()));
            if (p == _prefixes.end()) {
                return nullopt;
            }
            return _bindings[size_t(p->second)].uri;
        }

        // Namespaces in XML 1.0 over the tag just read: the declarations
        // among its attributes bound, the names checked as QNames, the
        // element and its attributes given their namespaces, attributes
        // unique by name and by namespace and local name; the element
        // opened
        bool _namespaces(size_t name_at, size_t name_end) {
            std::string_view qname(_base + name_at, name_end - name_at);
            if (!xml_qname_of_name(qname)) {
                _fail(errc::syntax, name_at, "'" + std::string(qname) + "' is not a qualified name (Namespaces in XML)");
                return false;
            }
            if (_prefix(qname) == "xmlns") {
                _fail(errc::syntax, name_at, "an element with the prefix xmlns");
                return false;
            }
            size_t count = _attributes.size();
            // unique by name (XML 1.0, 3.1)
            if (count > 1 && !_unique(false)) {
                return false;
            }
            uint32_t declared = 0;
            for (size_t k = 0; k < count; ++k) {
                auto& a = _attributes[k];
                std::string_view n = a.name.view();
                if (!xml_qname_of_name(n)) {
                    _fail(errc::syntax, _attribute_at[k], "'" + std::string(n) + "' is not a qualified name (Namespaces in XML)");
                    return false;
                }
                bool default_declaration = n == "xmlns";
                if (!default_declaration && _prefix(n) != "xmlns") {
                    continue;
                }
                std::string_view prefix = default_declaration ? std::string_view() : n.substr(6);
                std::string_view uri = a.value.view();
                const char* wrong = nullptr;
                if (prefix == "xmlns") {
                    wrong = "the prefix xmlns cannot be declared";
                } else if (prefix == "xml" && uri != XmlNamespace) {
                    wrong = "the prefix xml is bound to its own namespace alone";
                } else if (prefix != "xml" && uri == XmlNamespace) {
                    wrong = "only the prefix xml is bound to the XML namespace";
                } else if (uri == XmlnsNamespace) {
                    wrong = "nothing is bound to the xmlns namespace";
                } else if (!default_declaration && uri.empty()) {
                    wrong = "a prefix cannot be undeclared in XML 1.0 (xmlns:p=\"\")";
                }
                if (wrong) {
                    _fail(errc::syntax, _attribute_at[k], wrong);
                    return false;
                }
                XmlBinding b;
                b.prefix = default_declaration ? string() : _string(prefix.data(), prefix.size());
                b.uri = a.value;
                int32_t index = int32_t(_bindings.size());
                if (default_declaration) {
                    b.previous = _default;
                    _default = index;
                } else {
                    auto p = _prefixes.find(b.prefix);
                    b.previous = p == _prefixes.end() ? -1 : p->second;
                    _prefixes.insert_or_assign(b.prefix, index);
                }
                _bindings.push_back(std::move(b));
                ++declared;
            }
            // the element is opened once its tag is found good: an error
            // in the tag has the path of the element around it
            XmlOpen open;
            open.bindings = declared;
            open.name = _string(qname.data(), qname.size());
            auto prefix = _prefix(qname);
            open.local = prefix.empty() ? open.name : _string(qname.data() + prefix.size() + 1, qname.size() - prefix.size() - 1);
            auto uri = _lookup(prefix);
            if (!uri) {
                _fail(errc::syntax, name_at, "the prefix '" + std::string(prefix) + "' is not declared");
                return false;
            }
            open.uri = *uri;
            bool prefixed = false;
            for (size_t k = 0; k < count; ++k) {
                auto& a = _attributes[k];
                std::string_view n = a.name.view();
                auto p = _prefix(n);
                if (n == "xmlns") {
                    a.namespace_uri = string(XmlnsNamespace);
                } else if (!p.empty()) {
                    auto u = _lookup(p);
                    if (!u) {
                        _fail(errc::syntax, _attribute_at[k], "the prefix '" + std::string(p) + "' is not declared");
                        return false;
                    }
                    a.namespace_uri = *u;
                    prefixed = prefixed || p != "xmlns";
                }
            }
            // unique by namespace and local name (Namespaces in XML, 6.3)
            if (prefixed && !_unique(true)) {
                return false;
            }
            _open.push_back(std::move(open));
            return true;
        }

        // No two attributes of the tag alike: by name, or by namespace and
        // local name. Pairwise for a few, through a set for more, so that
        // a tag of a hundred thousand attributes is not 10^10 comparisons.
        bool _unique(bool expanded) {
            size_t count = _attributes.size();
            auto key = [&](size_t k) -> std::string {
                auto& a = _attributes[k];
                if (!expanded) {
                    return std::string(a.name.view());
                }
                auto n = a.name.view();
                auto c = n.find(':');
                std::string s(a.namespace_uri.view());
                s += '\0';
                s += c == std::string_view::npos ? n : n.substr(c + 1);
                return s;
            };
            auto duplicate = [&](size_t k) {
                _fail(errc::duplicate_key, _attribute_at[k], expanded
                    ? "two attributes of one namespace and local name: " + std::string(_attributes[k].name.view())
                    : "the attribute " + std::string(_attributes[k].name.view()) + " twice in one tag");
                return false;
            };
            if (count <= 16) {
                for (size_t k = 1; k < count; ++k) {
                    for (size_t m = 0; m < k; ++m) {
                        if (expanded && (_attributes[k].namespace_uri.empty() || _attributes[m].namespace_uri.empty())) {
                            continue;
                        }
                        if (key(k) == key(m)) {
                            return duplicate(k);
                        }
                    }
                }
                return true;
            }
            sgcl::set<string> seen;
            for (size_t k = 0; k < count; ++k) {
                if (expanded && _attributes[k].namespace_uri.empty()) {
                    continue;
                }
                if (!seen.insert(string(key(k))).second) {
                    return duplicate(k);
                }
            }
            return true;
        }

        // [42] ETag
        Parse _end_tag(token& out) {
            const char* d = _base;
            size_t i = _pos + 2;
            size_t n = _name(i);
            if (_bad(n)) {
                return _status(n);
            }
            if (n == i) {
                return _parse_fail(errc::syntax, i, "'</' that begins no end tag");
            }
            size_t s = _spaces(n);
            if (s >= _end) {
                return _parse_short(s, "an end tag");
            }
            if (d[s] != '>') {
                return _parse_fail(errc::syntax, s, "an end tag with more than its name");
            }
            std::string_view name(d + i, n - i);
            if (_open.empty()) {
                return _parse_fail(errc::mismatched_tag, _pos, "</" + std::string(name) + "> with no element open");
            }
            if (_open.back().name.view() != name) {
                return _parse_fail(errc::mismatched_tag, _pos, "</" + std::string(name) + "> closes <" + std::string(_open.back().name.view()) + ">");
            }
            _pos = s + 1;
            _end_element(out);
            return Parse::done;
        }

        // The end of the element on the top: its token, its bindings gone
        void _end_element(token& out) {
            auto& top = _open.back();
            out = token();
            out._kind = token_kind::end_element;
            out._name = top.name;
            out._local = top.local;
            out._uri = top.uri;
            for (uint32_t k = 0; k < top.bindings; ++k) {
                auto& b = _bindings.back();
                if (b.prefix.empty()) {
                    _default = b.previous;
                } else if (b.previous < 0) {
                    _prefixes.erase(b.prefix);
                } else {
                    _prefixes.insert_or_assign(b.prefix, b.previous);
                }
                _bindings.pop_back();
            }
            _open.pop_back();
            if (_open.empty()) {
                _root_done = true;
            }
        }

        // [28] doctypedecl: its shape checked, its internal subset skipped
        // over (never interpreted), the whole passed on as a token
        Parse _doctype(token& out) {
            const char* d = _base;
            if (_doctype_seen || _root_done || !_open.empty()) {
                return _parse_fail(errc::syntax, _pos, _doctype_seen ? "a second DOCTYPE declaration" : "a DOCTYPE declaration after the root element's start");
            }
            size_t i = _pos + 9;
            size_t s = _spaces(i);
            if (s >= _end) {
                return _parse_short(s, "the DOCTYPE declaration");
            }
            if (s == i) {
                return _parse_fail(errc::syntax, i, "no white space after <!DOCTYPE");
            }
            size_t content = s;
            size_t n = _name(s);
            if (_bad(n)) {
                return _status(n);
            }
            if (n == s) {
                return _parse_fail(errc::syntax, s, "the DOCTYPE declaration without the root element's name");
            }
            if (!xml_qname_of_name(std::string_view(d + s, n - s))) {
                return _parse_fail(errc::syntax, s, "the DOCTYPE's name is not a qualified name (Namespaces in XML)");
            }
            string name = _string(d + s, n - s);
            i = _spaces(n);
            if (i >= _end) {
                return _parse_short(i, "the DOCTYPE declaration");
            }
            if (i > n && (_starts(i, "SYSTEM") || _starts(i, "PUBLIC"))) {
                size_t r = _external_id(i);
                if (_bad(r)) {
                    return _status(r);
                }
                i = _spaces(r);
            } else if (i + 6 > _end && !_eof && (std::string_view("SYSTEM").starts_with(std::string_view(d + i, _end - i))
                                                || std::string_view("PUBLIC").starts_with(std::string_view(d + i, _end - i)))) {
                return Parse::more;
            }
            if (i >= _end) {
                return _parse_short(i, "the DOCTYPE declaration");
            }
            if (d[i] == '[') {
                size_t r = _subset(i + 1);
                if (_bad(r)) {
                    return _status(r);
                }
                i = _spaces(r);
                if (i >= _end) {
                    return _parse_short(i, "the DOCTYPE declaration");
                }
            }
            if (d[i] != '>') {
                return _parse_fail(errc::syntax, i, "'>' expected to end the DOCTYPE declaration");
            }
            size_t last = i;
            while (last > content && xml_space(d[last - 1])) {
                --last;
            }
            out = token();
            out._kind = token_kind::doctype;
            out._name = name;
            out._text = _line_ends(d + content, last - content);
            _doctype_seen = true;
            _pos = i + 1;
            return Parse::done;
        }

        // A quoted literal from i: the index past it. A public identifier's
        // characters are the few [13] allows.
        size_t _literal(size_t i, bool pubid) {
            const char* d = _base;
            if (i >= _end) {
                return _short(i, "a literal");
            }
            char q = d[i];
            if (q != '"' && q != '\'') {
                return _failed(errc::syntax, i, "a literal in quotes expected");
            }
            for (size_t k = i + 1;;) {
                if (k >= _end) {
                    return _short(k, "a literal");
                }
                char c = d[k];
                if (c == q) {
                    return k + 1;
                }
                if (pubid) {
                    bool ok = c == ' ' || c == '\r' || c == '\n' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                           || std::string_view("-'()+,./:=?;!*#@$_%").find(c) != std::string_view::npos;
                    if (!ok) {
                        return _failed(errc::syntax, k, "a character a public identifier cannot hold");
                    }
                    ++k;
                    continue;
                }
                size_t w = _character(k);
                if (_bad(w)) {
                    return w;
                }
                k += w;
            }
        }

        // [75] ExternalID: SYSTEM "literal" | PUBLIC "pubid" "literal"
        size_t _external_id(size_t i) {
            bool pub = _starts(i, "PUBLIC");
            size_t k = i + 6;
            size_t s = _spaces(k);
            if (s >= _end) {
                return _short(s, "the DOCTYPE declaration");
            }
            if (s == k) {
                return _failed(errc::syntax, k, "no white space after SYSTEM or PUBLIC");
            }
            k = s;
            if (pub) {
                k = _literal(k, true);
                if (_bad(k)) {
                    return k;
                }
                s = _spaces(k);
                if (s >= _end) {
                    return _short(s, "the DOCTYPE declaration");
                }
                if (s == k) {
                    return _failed(errc::syntax, k, "no white space between the public and the system identifier");
                }
                k = s;
            }
            return _literal(k, false);
        }

        // The internal subset from i: markup declarations, parameter
        // entity references, comments and instructions, each skipped by
        // its shape — a declaration to its '>' outside quotes — to the ']'
        // that closes it. Nothing is interpreted.
        size_t _subset(size_t i) {
            const char* d = _base;
            for (;;) {
                i = _spaces(i);
                if (i >= _end) {
                    return _short(i, "the DOCTYPE's internal subset");
                }
                char c = d[i];
                if (c == ']') {
                    return i + 1;
                }
                if (c == '%') {
                    size_t n = _name(i + 1);
                    if (_bad(n)) {
                        return n;
                    }
                    if (n == i + 1) {
                        return _failed(errc::syntax, i, "'%' that begins no parameter entity reference");
                    }
                    if (n >= _end) {
                        return _short(n, "the DOCTYPE's internal subset");
                    }
                    if (d[n] != ';') {
                        return _failed(errc::syntax, n, "a parameter entity reference without its ';'");
                    }
                    i = n + 1;
                    continue;
                }
                if (c != '<') {
                    return _failed(errc::syntax, i, "a character the DOCTYPE's internal subset cannot hold here");
                }
                if (i + 4 > _end) {
                    return _short(_end, "the DOCTYPE's internal subset");
                }
                if (_starts(i, "<!--")) {
                    size_t r = _until(i + 4, "-->", true, _scratch, "a comment");
                    if (_bad(r)) {
                        return r;
                    }
                    i = r;
                    continue;
                }
                if (d[i + 1] == '?') {
                    size_t n = _name(i + 2);
                    if (_bad(n)) {
                        return n;
                    }
                    if (n == i + 2) {
                        return _failed(errc::syntax, i + 2, "a processing instruction without a target");
                    }
                    std::string_view target(d + i + 2, n - i - 2);
                    if (target.size() == 3 && (target[0] | 0x20) == 'x' && (target[1] | 0x20) == 'm' && (target[2] | 0x20) == 'l') {
                        return _failed(errc::syntax, i, "the target '" + std::string(target) + "' is reserved");
                    }
                    if (n < _end && d[n] != '?' && !xml_space(d[n])) {
                        return _failed(errc::syntax, n, "no white space after the target of an instruction");
                    }
                    size_t r = _until(n, "?>", false, _scratch, "an instruction");
                    if (_bad(r)) {
                        return r;
                    }
                    i = r;
                    continue;
                }
                if (d[i + 1] != '!') {
                    return _failed(errc::syntax, i, "a markup declaration expected in the DOCTYPE's internal subset");
                }
                size_t n = _name(i + 2);
                if (_bad(n)) {
                    return n;
                }
                std::string_view keyword(d + i + 2, n - i - 2);
                if (keyword != "ELEMENT" && keyword != "ATTLIST" && keyword != "ENTITY" && keyword != "NOTATION") {
                    return _failed(errc::syntax, i, "'<!" + std::string(keyword) + "' is no markup declaration");
                }
                if (n >= _end) {
                    return _short(n, "a markup declaration");
                }
                if (!xml_space(d[n])) {
                    return _failed(errc::syntax, n, "no white space after <!" + std::string(keyword));
                }
                size_t k = n;
                for (;;) {
                    if (k >= _end) {
                        return _short(k, "a markup declaration");
                    }
                    char b = d[k];
                    if (b == '>') {
                        break;
                    }
                    if (b == '"' || b == '\'') {
                        k = _literal(k, false);
                        if (_bad(k)) {
                            return k;
                        }
                        continue;
                    }
                    if (b == '<') {
                        return _failed(errc::syntax, k, "'<' inside a markup declaration");
                    }
                    size_t w = _character(k);
                    if (_bad(w)) {
                        return w;
                    }
                    k += w;
                }
                i = k + 1;
            }
        }

        // --- state -----------------------------------------------------

        options _options;
        string _text;                        // the document in memory, read where it lies
        vector<char> _buffer;                // or the buffer a stream is read into
        const char* _base = nullptr;         // the one of the two being read
        size_t _pos = 0;                     // where the next token starts
        size_t _end = 0;                     // the end of the data
        size_t _start = 0;                   // where the document starts (past a byte order mark)
        bool _memory = false;
        bool _eof = false;
        bool _sniffed = false;
        bool _utf8_bom = false;
        bool _first = true;                  // nothing read yet: an XML declaration may come
        bool _doctype_seen = false;
        bool _root_done = false;
        bool _ended = false;
        bool _close_pending = false;         // <a/> was read: its end comes next

        optional<error> _error;
        optional<error> _deferred;           // the stream's failure, reported where the data ends

        // positions of what was dropped from the front of the buffer
        uint64_t _dropped = 0;
        uint64_t _lines = 0;
        uint64_t _column = 0;

        // another encoding turned into UTF-8
        Transcode _transcode = Transcode::none;
        txt::encoding _single = txt::encoding::utf8;
        tracked_ptr<array<byte, config::io_buffer_size>> _raw;
        uint8_t _carry[4] {};
        uint8_t _carry_size = 0;
        uint64_t _raw_count = 0;             // bytes of the input decoded
        uint64_t _origin_bytes = 0;          // the input's byte where the other encoding starts
        uint64_t _origin_points = 0;         // the code points of UTF-8 before it
        uint64_t _origin_supplementary = 0;
        uint64_t _dropped_points = 0;
        uint64_t _dropped_supplementary = 0;

        // the finder
        bool _waiting = false;
        size_t _waited_end = 0;
        Find _find_state = Find::classify;
        size_t _find_at = 0;
        char _quote = 0;

        // the elements open and the namespaces in scope
        vector<XmlOpen> _open;
        vector<XmlBinding> _bindings;
        map<string, int32_t> _prefixes;
        int32_t _default = -1;

        // scratch of one token
        std::string _scratch;
        vector<attribute> _attributes;
        std::vector<size_t> _attribute_at;
        dynamic_array<string> _cache;
    };
}
