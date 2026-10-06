//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "html_entities.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The tokenizer of the WHATWG HTML standard (13.2.5), written from its
// states: data, RCDATA, RAWTEXT, script data and its escaped forms,
// PLAINTEXT, tags and attributes, comments, DOCTYPEs, CDATA sections and the
// character references. The tree builder pulls one token at a time and may
// switch the tokenizer's state between them, as the standard has it do.
namespace sgcl::txt::detail::html {
    struct Attr {
        std::string name;
        std::string value;
    };

    enum TokenType : uint8_t { TkDoctype, TkStartTag, TkEndTag, TkComment, TkCharacters, TkEof };

    struct Token {
        uint8_t type = TkEof;
        std::string name;                 // tag name, DOCTYPE name
        std::vector<Attr> attrs;
        bool self_closing = false;
        std::string data;                 // characters, comment
        // DOCTYPE
        bool force_quirks = false;
        bool has_name = false, has_public = false, has_system = false;
        std::string public_id, system_id;
        size_t offset = 0;

        void clear() {
            type = TkEof;
            name.clear();
            attrs.clear();
            self_closing = false;
            data.clear();
            force_quirks = has_name = has_public = has_system = false;
            public_id.clear();
            system_id.clear();
        }

        const Attr* attr(std::string_view n) const noexcept {
            for (const Attr& a : attrs) {
                if (a.name == n) {
                    return &a;
                }
            }
            return nullptr;
        }
    };

    struct ParseError {
        size_t offset;
        const char* code;
    };

    // The input stream as the standard preprocesses it: CR LF and CR become
    // LF, bytes that are not UTF-8 become U+FFFD
    inline std::string preprocess(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            unsigned char c = (unsigned char)s[i];
            if (c == '\r') {
                out.push_back('\n');
                i += (i + 1 < s.size() && s[i + 1] == '\n') ? 2 : 1;
                continue;
            }
            if (c < 0x80) {
                out.push_back(char(c));
                ++i;
                continue;
            }
            // the UTF-8 decoder of the Encoding standard: a sequence that breaks
            // off is one U+FFFD for its valid prefix, the breaking byte read again
            size_t need = 0;
            unsigned char lower = 0x80, upper = 0xBF;
            if (c >= 0xC2 && c <= 0xDF) {
                need = 1;
            } else if (c >= 0xE0 && c <= 0xEF) {
                need = 2;
                lower = c == 0xE0 ? 0xA0 : 0x80;
                upper = c == 0xED ? 0x9F : 0xBF;
            } else if (c >= 0xF0 && c <= 0xF4) {
                need = 3;
                lower = c == 0xF0 ? 0x90 : 0x80;
                upper = c == 0xF4 ? 0x8F : 0xBF;
            }
            if (need == 0) {
                out.append("\xEF\xBF\xBD");
                ++i;
                continue;
            }
            size_t j = i + 1, got = 0;
            while (got < need && j < s.size() && (unsigned char)s[j] >= lower && (unsigned char)s[j] <= upper) {
                ++j;
                ++got;
                lower = 0x80;
                upper = 0xBF;
            }
            if (got == need) {
                out.append(s.substr(i, j - i));
            } else {
                out.append("\xEF\xBF\xBD");
            }
            i = j;
        }
        return out;
    }

    inline void append_utf8(std::string& out, char32_t c) {
        if (c < 0x80) {
            out.push_back(char(c));
        } else if (c < 0x800) {
            out.push_back(char(0xC0 | c >> 6));
            out.push_back(char(0x80 | (c & 63)));
        } else if (c < 0x10000) {
            out.push_back(char(0xE0 | c >> 12));
            out.push_back(char(0x80 | (c >> 6 & 63)));
            out.push_back(char(0x80 | (c & 63)));
        } else {
            out.push_back(char(0xF0 | c >> 18));
            out.push_back(char(0x80 | (c >> 12 & 63)));
            out.push_back(char(0x80 | (c >> 6 & 63)));
            out.push_back(char(0x80 | (c & 63)));
        }
    }

    inline bool ascii_alpha(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    inline bool ascii_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    inline bool ascii_alnum(char c) noexcept {
        return ascii_alpha(c) || ascii_digit(c);
    }

    inline bool ascii_hex(char c) noexcept {
        return ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    inline char ascii_lower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
    }

    inline bool html_ws(char c) noexcept {
        return c == '\t' || c == '\n' || c == '\f' || c == ' ' || c == '\r';
    }

    enum TokState : uint8_t {
        SData, SRcdata, SRawtext, SScriptData, SPlaintext, STagOpen, SEndTagOpen, STagName,
        SRcdataLt, SRcdataEndTagOpen, SRcdataEndTagName, SRawtextLt, SRawtextEndTagOpen, SRawtextEndTagName,
        SScriptLt, SScriptEndTagOpen, SScriptEndTagName, SScriptEscapeStart, SScriptEscapeStartDash,
        SScriptEscaped, SScriptEscapedDash, SScriptEscapedDashDash, SScriptEscapedLt, SScriptEscapedEndTagOpen,
        SScriptEscapedEndTagName, SScriptDoubleEscapeStart, SScriptDoubleEscaped, SScriptDoubleEscapedDash,
        SScriptDoubleEscapedDashDash, SScriptDoubleEscapedLt, SScriptDoubleEscapeEnd,
        SBeforeAttrName, SAttrName, SAfterAttrName, SBeforeAttrValue, SAttrValueDq, SAttrValueSq,
        SAttrValueUnq, SAfterAttrValueQuoted, SSelfClosingStartTag, SBogusComment, SMarkupDeclOpen,
        SCommentStart, SCommentStartDash, SComment, SCommentLt, SCommentLtBang, SCommentLtBangDash,
        SCommentLtBangDashDash, SCommentEndDash, SCommentEnd, SCommentEndBang,
        SDoctype, SBeforeDoctypeName, SDoctypeName, SAfterDoctypeName, SAfterDoctypePublicKw,
        SBeforeDoctypePublicId, SDoctypePublicIdDq, SDoctypePublicIdSq, SAfterDoctypePublicId,
        SBetweenDoctypePublicSystem, SAfterDoctypeSystemKw, SBeforeDoctypeSystemId, SDoctypeSystemIdDq,
        SDoctypeSystemIdSq, SAfterDoctypeSystemId, SBogusDoctype, SCdataSection, SCdataSectionBracket,
        SCdataSectionEnd,
    };

    class Tokenizer {
    public:
        explicit Tokenizer(std::string_view input, std::vector<ParseError>* errors) noexcept
        : _s(input)
        , _errors(errors) {
        }

        uint8_t state = SData;
        std::string last_start_tag;
        bool cdata_allowed = false;       // the adjusted current node is not in the HTML namespace

        // The next token into t
        void next(Token& t) {
            t.clear();
            if (_pending_eof) {
                t.type = TkEof;
                t.offset = _s.size();
                return;
            }
            _tok = &t;
            _emitted = false;
            while (!_emitted) {
                step();
            }
        }

        size_t position() const noexcept {
            return _i;
        }

    private:
        std::string_view _s;
        size_t _i = 0;
        std::vector<ParseError>* _errors;
        Token* _tok = nullptr;
        bool _emitted = false;
        bool _pending_eof = false;
        Token _cur;                       // the tag, comment or DOCTYPE being built
        std::string _buf;                 // the temporary buffer
        std::string _chars;               // characters waiting to be emitted
        size_t _chars_at = 0;
        uint8_t _return_state = SData;
        bool _attr_dup = false;

        void error(const char* code) {
            if (_errors) {
                _errors->push_back(ParseError{_i, code});
            }
        }

        bool at_end() const noexcept {
            return _i >= _s.size();
        }

        // the character at the cursor, consumed; '\0' with at_end() at the end
        char consume() noexcept {
            return _i < _s.size() ? _s[_i++] : (++_i, '\0');
        }

        void reconsume() noexcept {
            --_i;
        }

        bool eof_now() const noexcept {
            return _i > _s.size();
        }

        void chars(char c) {
            if (_chars.empty()) {
                _chars_at = _i;
            }
            _chars.push_back(c);
        }

        void chars(std::string_view s) {
            if (_chars.empty()) {
                _chars_at = _i;
            }
            _chars.append(s);
        }

        // the characters gathered, as one token, before anything else is
        bool flush_chars() {
            if (_chars.empty()) {
                return false;
            }
            _tok->type = TkCharacters;
            _tok->data.swap(_chars);
            _chars.clear();
            _tok->offset = _chars_at;
            _emitted = true;
            return true;
        }

        void emit_current() {
            if (_cur.type == TkStartTag || _cur.type == TkEndTag) {
                if (_cur.type == TkStartTag) {
                    last_start_tag = _cur.name;
                } else {
                    if (!_cur.attrs.empty()) {
                        error("end-tag-with-attributes");
                    }
                    if (_cur.self_closing) {
                        error("end-tag-with-trailing-solidus");
                    }
                }
            }
            if (flush_chars()) {
                _pending_tag = true;
                return;
            }
            *_tok = std::move(_cur);
            _cur.clear();
            _emitted = true;
        }

        bool _pending_tag = false;

        void emit_eof() {
            if (flush_chars()) {
                _pending_eof = true;
                return;
            }
            _tok->type = TkEof;
            _tok->offset = _s.size();
            _pending_eof = true;
            _emitted = true;
        }

        bool appropriate_end_tag() const noexcept {
            return !last_start_tag.empty() && _cur.name == last_start_tag;
        }

        void start_tag(uint8_t type) {
            _cur.clear();
            _cur.type = type;
            _cur.offset = _i;
        }

        void start_attr() {
            _cur.attrs.push_back(Attr());
            _attr_dup = false;
        }

        // the attribute's name complete: a duplicate is dropped (13.2.5.33)
        void end_attr_name() {
            if (_cur.attrs.empty()) {
                return;
            }
            const std::string& n = _cur.attrs.back().name;
            for (size_t k = 0; k + 1 < _cur.attrs.size(); ++k) {
                if (_cur.attrs[k].name == n) {
                    error("duplicate-attribute");
                    _attr_dup = true;
                    return;
                }
            }
        }

        void drop_duplicate_attr() {
            if (_attr_dup && !_cur.attrs.empty()) {
                _cur.attrs.pop_back();
                _attr_dup = false;
            }
        }

        bool starts_ci(std::string_view word) const noexcept {
            if (_s.size() - std::min(_i, _s.size()) < word.size()) {
                return false;
            }
            for (size_t k = 0; k < word.size(); ++k) {
                if (ascii_lower(_s[_i + k]) != word[k]) {
                    return false;
                }
            }
            return true;
        }

        bool starts(std::string_view word) const noexcept {
            return _i <= _s.size() && _s.substr(_i).substr(0, word.size()) == word;
        }

        // the character reference after an &, into the attribute or the text
        void char_ref() {
            bool in_attr = _return_state == SAttrValueDq || _return_state == SAttrValueSq
                || _return_state == SAttrValueUnq;
            auto flush = [&](std::string_view s) {
                if (in_attr) {
                    _cur.attrs.back().value.append(s);
                } else {
                    chars(s);
                }
            };
            char c = _i < _s.size() ? _s[_i] : '\0';
            if (at_end() || !(ascii_alnum(c) || c == '#')) {
                flush("&");
                return;
            }
            if (c == '#') {
                ++_i;
                bool hex = _i < _s.size() && (_s[_i] == 'x' || _s[_i] == 'X');
                size_t start = _i;
                if (hex) {
                    ++_i;
                }
                size_t digits_at = _i;
                uint64_t v = 0;
                while (_i < _s.size() && (hex ? ascii_hex(_s[_i]) : ascii_digit(_s[_i]))) {
                    char d = _s[_i++];
                    v = v * (hex ? 16 : 10) + uint64_t(ascii_digit(d) ? d - '0' : (d | 32) - 'a' + 10);
                    if (v > 0x10FFFF) {
                        v = 0x110000;
                    }
                }
                if (_i == digits_at) {
                    error("absence-of-digits-in-numeric-character-reference");
                    _i = start;
                    flush("&#");
                    return;
                }
                if (_i < _s.size() && _s[_i] == ';') {
                    ++_i;
                } else {
                    error("missing-semicolon-after-character-reference");
                }
                char32_t cp = char32_t(v);
                if (cp == 0) {
                    error("null-character-reference");
                    cp = 0xFFFD;
                } else if (cp > 0x10FFFF) {
                    error("character-reference-outside-unicode-range");
                    cp = 0xFFFD;
                } else if (cp >= 0xD800 && cp <= 0xDFFF) {
                    error("surrogate-character-reference");
                    cp = 0xFFFD;
                } else if (cp >= 0x80 && cp <= 0x9F) {
                    static constexpr char32_t c1[32] = {
                        0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                        0x2039, 0x0152, 0x8D, 0x017D, 0x8F, 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013,
                        0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178,
                    };
                    error("control-character-reference");
                    cp = c1[cp - 0x80];
                }
                std::string out;
                append_utf8(out, cp);
                flush(out);
                return;
            }
            // a named reference: the longest name of the table that the input begins with
            size_t end = _i;
            while (end < _s.size() && end - _i < 32 && ascii_alnum(_s[end])) {
                ++end;
            }
            if (end < _s.size() && _s[end] == ';' && end - _i < 32) {
                ++end;
            }
            const NamedReference* found = nullptr;
            size_t len = 0;
            for (size_t n = end - _i; n > 0 && !found; --n) {
                std::string_view cand = _s.substr(_i, n);
                const NamedReference* b = NamedReferences;
                const NamedReference* e = NamedReferences + sizeof NamedReferences / sizeof NamedReferences[0];
                auto it = std::lower_bound(b, e, cand, [](const NamedReference& r, std::string_view k) {
                    return r.name < k;
                });
                if (it != e && it->name == cand) {
                    found = it;
                    len = n;
                }
            }
            if (!found) {
                // an ambiguous ampersand: the alphanumerics as they are
                size_t k = _i;
                while (k < _s.size() && ascii_alnum(_s[k])) {
                    ++k;
                }
                if (k < _s.size() && _s[k] == ';') {
                    error("unknown-named-character-reference");
                }
                flush("&");
                return;
            }
            bool semicolon = found->name.back() == ';';
            char next = _i + len < _s.size() ? _s[_i + len] : '\0';
            if (in_attr && !semicolon && (next == '=' || ascii_alnum(next))) {
                // historical: in an attribute, &amp without its semicolon before = or a letter is text
                flush("&");
                return;
            }
            if (!semicolon) {
                error("missing-semicolon-after-character-reference");
            }
            _i += len;
            std::string out;
            append_utf8(out, found->first);
            if (found->second) {
                append_utf8(out, found->second);
            }
            flush(out);
        }

        void step();
    };

    inline void Tokenizer::step() {
        if (_pending_tag) {
            _pending_tag = false;
            *_tok = std::move(_cur);
            _cur.clear();
            _emitted = true;
            return;
        }
        auto lower = [](char c) { return ascii_lower(c); };
        auto replacement = "\xEF\xBF\xBD";
        switch (state) {
            case SData: {
                // a run up to the next & < or NUL
                size_t b = _i;
                while (_i < _s.size() && _s[_i] != '&' && _s[_i] != '<' && _s[_i] != '\0') {
                    ++_i;
                }
                if (_i > b) {
                    if (_chars.empty()) {
                        _chars_at = b;
                    }
                    _chars.append(_s.substr(b, _i - b));
                }
                if (at_end()) {
                    emit_eof();
                    return;
                }
                char c = consume();
                if (c == '&') {
                    _return_state = SData;
                    char_ref();
                } else if (c == '<') {
                    state = STagOpen;
                } else {
                    error("unexpected-null-character");
                    chars('\0');
                }
                return;
            }
            case SRcdata:
            case SRawtext:
            case SPlaintext:
            case SScriptData: {
                size_t b = _i;
                while (_i < _s.size() && _s[_i] != '<' && _s[_i] != '\0' && !(state == SRcdata && _s[_i] == '&')) {
                    ++_i;
                }
                if (_i > b) {
                    if (_chars.empty()) {
                        _chars_at = b;
                    }
                    _chars.append(_s.substr(b, _i - b));
                }
                if (at_end()) {
                    emit_eof();
                    return;
                }
                char c = consume();
                if (c == '\0') {
                    error("unexpected-null-character");
                    chars(replacement);
                } else if (c == '&') {
                    _return_state = SRcdata;
                    char_ref();
                } else if (state == SPlaintext) {
                    chars('<');
                } else {
                    state = state == SRcdata ? SRcdataLt : state == SRawtext ? SRawtextLt : SScriptLt;
                }
                return;
            }
            case STagOpen: {
                char c = consume();
                if (eof_now()) {
                    error("eof-before-tag-name");
                    chars('<');
                    emit_eof();
                    return;
                }
                if (c == '!') {
                    state = SMarkupDeclOpen;
                } else if (c == '/') {
                    state = SEndTagOpen;
                } else if (ascii_alpha(c)) {
                    start_tag(TkStartTag);
                    reconsume();
                    state = STagName;
                } else if (c == '?') {
                    error("unexpected-question-mark-instead-of-tag-name");
                    _cur.clear();
                    _cur.type = TkComment;
                    reconsume();
                    state = SBogusComment;
                } else {
                    error("invalid-first-character-of-tag-name");
                    chars('<');
                    reconsume();
                    state = SData;
                }
                return;
            }
            case SEndTagOpen: {
                char c = consume();
                if (eof_now()) {
                    error("eof-before-tag-name");
                    chars("</");
                    emit_eof();
                    return;
                }
                if (ascii_alpha(c)) {
                    start_tag(TkEndTag);
                    reconsume();
                    state = STagName;
                } else if (c == '>') {
                    error("missing-end-tag-name");
                    state = SData;
                } else {
                    error("invalid-first-character-of-tag-name");
                    _cur.clear();
                    _cur.type = TkComment;
                    reconsume();
                    state = SBogusComment;
                }
                return;
            }
            case STagName: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-tag");
                        emit_eof();
                        return;
                    }
                    if (html_ws(c)) {
                        state = SBeforeAttrName;
                        return;
                    }
                    if (c == '/') {
                        state = SSelfClosingStartTag;
                        return;
                    }
                    if (c == '>') {
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.name.append(replacement);
                    } else {
                        _cur.name.push_back(lower(c));
                    }
                }
            }
            case SRcdataLt:
            case SRawtextLt: {
                bool rc = state == SRcdataLt;
                char c = consume();
                if (!eof_now() && c == '/') {
                    _buf.clear();
                    state = rc ? SRcdataEndTagOpen : SRawtextEndTagOpen;
                } else {
                    chars('<');
                    reconsume();
                    state = rc ? SRcdata : SRawtext;
                }
                return;
            }
            case SRcdataEndTagOpen:
            case SRawtextEndTagOpen:
            case SScriptEndTagOpen:
            case SScriptEscapedEndTagOpen: {
                uint8_t name_state = state == SRcdataEndTagOpen ? SRcdataEndTagName
                    : state == SRawtextEndTagOpen ? SRawtextEndTagName
                    : state == SScriptEndTagOpen ? SScriptEndTagName : SScriptEscapedEndTagName;
                uint8_t back = state == SRcdataEndTagOpen ? SRcdata : state == SRawtextEndTagOpen ? SRawtext
                    : state == SScriptEndTagOpen ? SScriptData : SScriptEscaped;
                char c = consume();
                if (!eof_now() && ascii_alpha(c)) {
                    start_tag(TkEndTag);
                    reconsume();
                    state = name_state;
                } else {
                    chars("</");
                    reconsume();
                    state = back;
                }
                return;
            }
            case SRcdataEndTagName:
            case SRawtextEndTagName:
            case SScriptEndTagName:
            case SScriptEscapedEndTagName: {
                uint8_t back = state == SRcdataEndTagName ? SRcdata : state == SRawtextEndTagName ? SRawtext
                    : state == SScriptEndTagName ? SScriptData : SScriptEscaped;
                char c = consume();
                bool end = eof_now();
                if (!end && html_ws(c) && appropriate_end_tag()) {
                    state = SBeforeAttrName;
                    return;
                }
                if (!end && c == '/' && appropriate_end_tag()) {
                    state = SSelfClosingStartTag;
                    return;
                }
                if (!end && c == '>' && appropriate_end_tag()) {
                    state = SData;
                    emit_current();
                    return;
                }
                if (!end && ascii_alpha(c)) {
                    _cur.name.push_back(lower(c));
                    _buf.push_back(c);
                    return;
                }
                chars("</");
                chars(_buf);
                _cur.clear();
                reconsume();
                state = back;
                return;
            }
            case SScriptLt: {
                char c = consume();
                if (!eof_now() && c == '/') {
                    _buf.clear();
                    state = SScriptEndTagOpen;
                } else if (!eof_now() && c == '!') {
                    state = SScriptEscapeStart;
                    chars("<!");
                } else {
                    chars('<');
                    reconsume();
                    state = SScriptData;
                }
                return;
            }
            case SScriptEscapeStart:
            case SScriptEscapeStartDash: {
                char c = consume();
                if (!eof_now() && c == '-') {
                    chars('-');
                    state = state == SScriptEscapeStart ? SScriptEscapeStartDash : SScriptEscapedDashDash;
                } else {
                    reconsume();
                    state = SScriptData;
                }
                return;
            }
            case SScriptEscaped:
            case SScriptDoubleEscaped: {
                bool dbl = state == SScriptDoubleEscaped;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-script-html-comment-like-text");
                    emit_eof();
                    return;
                }
                if (c == '-') {
                    chars('-');
                    state = dbl ? SScriptDoubleEscapedDash : SScriptEscapedDash;
                } else if (c == '<') {
                    if (dbl) {
                        chars('<');
                    }
                    state = dbl ? SScriptDoubleEscapedLt : SScriptEscapedLt;
                } else if (c == '\0') {
                    error("unexpected-null-character");
                    chars(replacement);
                } else {
                    chars(c);
                }
                return;
            }
            case SScriptEscapedDash:
            case SScriptDoubleEscapedDash: {
                bool dbl = state == SScriptDoubleEscapedDash;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-script-html-comment-like-text");
                    emit_eof();
                    return;
                }
                if (c == '-') {
                    chars('-');
                    state = dbl ? SScriptDoubleEscapedDashDash : SScriptEscapedDashDash;
                } else if (c == '<') {
                    if (dbl) {
                        chars('<');
                    }
                    state = dbl ? SScriptDoubleEscapedLt : SScriptEscapedLt;
                } else if (c == '\0') {
                    error("unexpected-null-character");
                    chars(replacement);
                    state = dbl ? SScriptDoubleEscaped : SScriptEscaped;
                } else {
                    chars(c);
                    state = dbl ? SScriptDoubleEscaped : SScriptEscaped;
                }
                return;
            }
            case SScriptEscapedDashDash:
            case SScriptDoubleEscapedDashDash: {
                bool dbl = state == SScriptDoubleEscapedDashDash;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-script-html-comment-like-text");
                    emit_eof();
                    return;
                }
                if (c == '-') {
                    chars('-');
                } else if (c == '<') {
                    if (dbl) {
                        chars('<');
                    }
                    state = dbl ? SScriptDoubleEscapedLt : SScriptEscapedLt;
                } else if (c == '>') {
                    chars('>');
                    state = SScriptData;
                } else if (c == '\0') {
                    error("unexpected-null-character");
                    chars(replacement);
                    state = dbl ? SScriptDoubleEscaped : SScriptEscaped;
                } else {
                    chars(c);
                    state = dbl ? SScriptDoubleEscaped : SScriptEscaped;
                }
                return;
            }
            case SScriptEscapedLt: {
                char c = consume();
                if (!eof_now() && c == '/') {
                    _buf.clear();
                    state = SScriptEscapedEndTagOpen;
                } else if (!eof_now() && ascii_alpha(c)) {
                    _buf.clear();
                    chars('<');
                    reconsume();
                    state = SScriptDoubleEscapeStart;
                } else {
                    chars('<');
                    reconsume();
                    state = SScriptEscaped;
                }
                return;
            }
            case SScriptDoubleEscapeStart:
            case SScriptDoubleEscapeEnd: {
                bool start = state == SScriptDoubleEscapeStart;
                char c = consume();
                if (!eof_now() && (html_ws(c) || c == '/' || c == '>')) {
                    chars(c);
                    if (_buf == "script") {
                        state = start ? SScriptDoubleEscaped : SScriptEscaped;
                    } else {
                        state = start ? SScriptEscaped : SScriptDoubleEscaped;
                    }
                } else if (!eof_now() && ascii_alpha(c)) {
                    _buf.push_back(lower(c));
                    chars(c);
                } else {
                    reconsume();
                    state = start ? SScriptEscaped : SScriptDoubleEscaped;
                }
                return;
            }
            case SScriptDoubleEscapedLt: {
                char c = consume();
                if (!eof_now() && c == '/') {
                    _buf.clear();
                    chars('/');
                    state = SScriptDoubleEscapeEnd;
                } else {
                    reconsume();
                    state = SScriptDoubleEscaped;
                }
                return;
            }
            case SBeforeAttrName: {
                char c = consume();
                if (!eof_now() && html_ws(c)) {
                    return;
                }
                if (eof_now() || c == '/' || c == '>') {
                    reconsume();
                    state = SAfterAttrName;
                    return;
                }
                if (c == '=') {
                    error("unexpected-equals-sign-before-attribute-name");
                    start_attr();
                    _cur.attrs.back().name.push_back('=');
                    state = SAttrName;
                    return;
                }
                start_attr();
                reconsume();
                state = SAttrName;
                return;
            }
            case SAttrName: {
                while (true) {
                    char c = consume();
                    if (eof_now() || html_ws(c) || c == '/' || c == '>') {
                        reconsume();
                        end_attr_name();
                        state = SAfterAttrName;
                        return;
                    }
                    if (c == '=') {
                        end_attr_name();
                        state = SBeforeAttrValue;
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.attrs.back().name.append(replacement);
                        continue;
                    }
                    if (c == '"' || c == '\'' || c == '<') {
                        error("unexpected-character-in-attribute-name");
                    }
                    _cur.attrs.back().name.push_back(lower(c));
                }
            }
            case SAfterAttrName: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-tag");
                    emit_eof();
                    return;
                }
                if (html_ws(c)) {
                    return;
                }
                drop_duplicate_attr();
                if (c == '/') {
                    state = SSelfClosingStartTag;
                } else if (c == '=') {
                    state = SBeforeAttrValue;
                    _attr_dup = false;
                    // (a duplicate's value is read and dropped below)
                } else if (c == '>') {
                    state = SData;
                    emit_current();
                } else {
                    start_attr();
                    reconsume();
                    state = SAttrName;
                }
                return;
            }
            case SBeforeAttrValue: {
                char c = consume();
                if (!eof_now() && html_ws(c)) {
                    return;
                }
                if (!eof_now() && c == '"') {
                    state = SAttrValueDq;
                } else if (!eof_now() && c == '\'') {
                    state = SAttrValueSq;
                } else if (!eof_now() && c == '>') {
                    error("missing-attribute-value");
                    drop_duplicate_attr();
                    state = SData;
                    emit_current();
                } else {
                    reconsume();
                    state = SAttrValueUnq;
                }
                return;
            }
            case SAttrValueDq:
            case SAttrValueSq: {
                char quote = state == SAttrValueDq ? '"' : '\'';
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-tag");
                        emit_eof();
                        return;
                    }
                    if (c == quote) {
                        state = SAfterAttrValueQuoted;
                        return;
                    }
                    if (c == '&') {
                        _return_state = state;
                        char_ref();
                        continue;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.attrs.back().value.append(replacement);
                        continue;
                    }
                    _cur.attrs.back().value.push_back(c);
                }
            }
            case SAttrValueUnq: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-tag");
                        emit_eof();
                        return;
                    }
                    if (html_ws(c)) {
                        drop_duplicate_attr();
                        state = SBeforeAttrName;
                        return;
                    }
                    if (c == '&') {
                        _return_state = SAttrValueUnq;
                        char_ref();
                        continue;
                    }
                    if (c == '>') {
                        drop_duplicate_attr();
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.attrs.back().value.append(replacement);
                        continue;
                    }
                    if (c == '"' || c == '\'' || c == '<' || c == '=' || c == '`') {
                        error("unexpected-character-in-unquoted-attribute-value");
                    }
                    _cur.attrs.back().value.push_back(c);
                }
            }
            case SAfterAttrValueQuoted: {
                drop_duplicate_attr();
                char c = consume();
                if (eof_now()) {
                    error("eof-in-tag");
                    emit_eof();
                    return;
                }
                if (html_ws(c)) {
                    state = SBeforeAttrName;
                } else if (c == '/') {
                    state = SSelfClosingStartTag;
                } else if (c == '>') {
                    state = SData;
                    emit_current();
                } else {
                    error("missing-whitespace-between-attributes");
                    reconsume();
                    state = SBeforeAttrName;
                }
                return;
            }
            case SSelfClosingStartTag: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-tag");
                    emit_eof();
                    return;
                }
                if (c == '>') {
                    _cur.self_closing = true;
                    state = SData;
                    emit_current();
                } else {
                    error("unexpected-solidus-in-tag");
                    reconsume();
                    state = SBeforeAttrName;
                }
                return;
            }
            case SBogusComment: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        emit_current();
                        if (!_pending_tag && _emitted) {
                            _pending_eof = false;
                        }
                        state = SData;
                        // the comment, then the end
                        return;
                    }
                    if (c == '>') {
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.data.append(replacement);
                    } else {
                        _cur.data.push_back(c);
                    }
                }
            }
            case SMarkupDeclOpen: {
                if (starts("--")) {
                    _i += 2;
                    _cur.clear();
                    _cur.type = TkComment;
                    _cur.offset = _i;
                    state = SCommentStart;
                } else if (starts_ci("doctype")) {
                    _i += 7;
                    state = SDoctype;
                } else if (starts("[CDATA[")) {
                    _i += 7;
                    if (cdata_allowed) {
                        state = SCdataSection;
                    } else {
                        error("cdata-in-html-content");
                        _cur.clear();
                        _cur.type = TkComment;
                        _cur.data = "[CDATA[";
                        state = SBogusComment;
                    }
                } else {
                    error("incorrectly-opened-comment");
                    _cur.clear();
                    _cur.type = TkComment;
                    state = SBogusComment;
                }
                return;
            }
            case SCommentStart: {
                char c = consume();
                if (!eof_now() && c == '-') {
                    state = SCommentStartDash;
                } else if (!eof_now() && c == '>') {
                    error("abrupt-closing-of-empty-comment");
                    state = SData;
                    emit_current();
                } else {
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SCommentStartDash: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-comment");
                    emit_current();
                    state = SData;
                    return;
                }
                if (c == '-') {
                    state = SCommentEnd;
                } else if (c == '>') {
                    error("abrupt-closing-of-empty-comment");
                    state = SData;
                    emit_current();
                } else {
                    _cur.data.push_back('-');
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SComment: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-comment");
                        emit_current();
                        state = SData;
                        return;
                    }
                    if (c == '<') {
                        _cur.data.push_back('<');
                        state = SCommentLt;
                        return;
                    }
                    if (c == '-') {
                        state = SCommentEndDash;
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.data.append(replacement);
                    } else {
                        _cur.data.push_back(c);
                    }
                }
            }
            case SCommentLt: {
                char c = consume();
                if (!eof_now() && c == '!') {
                    _cur.data.push_back('!');
                    state = SCommentLtBang;
                } else if (!eof_now() && c == '<') {
                    _cur.data.push_back('<');
                } else {
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SCommentLtBang: {
                char c = consume();
                if (!eof_now() && c == '-') {
                    state = SCommentLtBangDash;
                } else {
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SCommentLtBangDash: {
                char c = consume();
                if (!eof_now() && c == '-') {
                    state = SCommentLtBangDashDash;
                } else {
                    reconsume();
                    state = SCommentEndDash;
                }
                return;
            }
            case SCommentLtBangDashDash: {
                char c = consume();
                if (!eof_now() && c != '>') {
                    error("nested-comment");
                }
                reconsume();
                state = SCommentEnd;
                return;
            }
            case SCommentEndDash: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-comment");
                    emit_current();
                    state = SData;
                    return;
                }
                if (c == '-') {
                    state = SCommentEnd;
                } else {
                    _cur.data.push_back('-');
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SCommentEnd: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-comment");
                    emit_current();
                    state = SData;
                    return;
                }
                if (c == '>') {
                    state = SData;
                    emit_current();
                } else if (c == '!') {
                    state = SCommentEndBang;
                } else if (c == '-') {
                    _cur.data.push_back('-');
                } else {
                    _cur.data.append("--");
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SCommentEndBang: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-comment");
                    emit_current();
                    state = SData;
                    return;
                }
                if (c == '-') {
                    _cur.data.append("--!");
                    state = SCommentEndDash;
                } else if (c == '>') {
                    error("incorrectly-closed-comment");
                    state = SData;
                    emit_current();
                } else {
                    _cur.data.append("--!");
                    reconsume();
                    state = SComment;
                }
                return;
            }
            case SDoctype: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.clear();
                    _cur.type = TkDoctype;
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    state = SBeforeDoctypeName;
                } else if (c == '>') {
                    reconsume();
                    state = SBeforeDoctypeName;
                } else {
                    error("missing-whitespace-before-doctype-name");
                    reconsume();
                    state = SBeforeDoctypeName;
                }
                return;
            }
            case SBeforeDoctypeName: {
                char c = consume();
                if (!eof_now() && html_ws(c)) {
                    return;
                }
                _cur.clear();
                _cur.type = TkDoctype;
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (c == '>') {
                    error("missing-doctype-name");
                    _cur.force_quirks = true;
                    state = SData;
                    emit_current();
                    return;
                }
                _cur.has_name = true;
                if (c == '\0') {
                    error("unexpected-null-character");
                    _cur.name.append(replacement);
                } else {
                    _cur.name.push_back(lower(c));
                }
                state = SDoctypeName;
                return;
            }
            case SDoctypeName: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-doctype");
                        _cur.force_quirks = true;
                        emit_current();
                        state = SData;
                        return;
                    }
                    if (html_ws(c)) {
                        state = SAfterDoctypeName;
                        return;
                    }
                    if (c == '>') {
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        _cur.name.append(replacement);
                    } else {
                        _cur.name.push_back(lower(c));
                    }
                }
            }
            case SAfterDoctypeName: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    return;
                }
                if (c == '>') {
                    state = SData;
                    emit_current();
                    return;
                }
                reconsume();
                if (starts_ci("public")) {
                    _i += 6;
                    state = SAfterDoctypePublicKw;
                } else if (starts_ci("system")) {
                    _i += 6;
                    state = SAfterDoctypeSystemKw;
                } else {
                    error("invalid-character-sequence-after-doctype-name");
                    _cur.force_quirks = true;
                    ++_i;
                    state = SBogusDoctype;
                }
                return;
            }
            case SAfterDoctypePublicKw:
            case SAfterDoctypeSystemKw: {
                bool pub = state == SAfterDoctypePublicKw;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    state = pub ? SBeforeDoctypePublicId : SBeforeDoctypeSystemId;
                } else if (c == '"' || c == '\'') {
                    error(pub ? "missing-whitespace-after-doctype-public-keyword"
                              : "missing-whitespace-after-doctype-system-keyword");
                    if (pub) {
                        _cur.has_public = true;
                        state = c == '"' ? SDoctypePublicIdDq : SDoctypePublicIdSq;
                    } else {
                        _cur.has_system = true;
                        state = c == '"' ? SDoctypeSystemIdDq : SDoctypeSystemIdSq;
                    }
                } else if (c == '>') {
                    error(pub ? "missing-doctype-public-identifier" : "missing-doctype-system-identifier");
                    _cur.force_quirks = true;
                    state = SData;
                    emit_current();
                } else {
                    error(pub ? "missing-quote-before-doctype-public-identifier"
                              : "missing-quote-before-doctype-system-identifier");
                    _cur.force_quirks = true;
                    reconsume();
                    state = SBogusDoctype;
                }
                return;
            }
            case SBeforeDoctypePublicId:
            case SBeforeDoctypeSystemId: {
                bool pub = state == SBeforeDoctypePublicId;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    return;
                }
                if (c == '"' || c == '\'') {
                    if (pub) {
                        _cur.has_public = true;
                        state = c == '"' ? SDoctypePublicIdDq : SDoctypePublicIdSq;
                    } else {
                        _cur.has_system = true;
                        state = c == '"' ? SDoctypeSystemIdDq : SDoctypeSystemIdSq;
                    }
                } else if (c == '>') {
                    error(pub ? "missing-doctype-public-identifier" : "missing-doctype-system-identifier");
                    _cur.force_quirks = true;
                    state = SData;
                    emit_current();
                } else {
                    error(pub ? "missing-quote-before-doctype-public-identifier"
                              : "missing-quote-before-doctype-system-identifier");
                    _cur.force_quirks = true;
                    reconsume();
                    state = SBogusDoctype;
                }
                return;
            }
            case SDoctypePublicIdDq:
            case SDoctypePublicIdSq:
            case SDoctypeSystemIdDq:
            case SDoctypeSystemIdSq: {
                bool pub = state == SDoctypePublicIdDq || state == SDoctypePublicIdSq;
                char quote = (state == SDoctypePublicIdDq || state == SDoctypeSystemIdDq) ? '"' : '\'';
                std::string& id = pub ? _cur.public_id : _cur.system_id;
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        error("eof-in-doctype");
                        _cur.force_quirks = true;
                        emit_current();
                        state = SData;
                        return;
                    }
                    if (c == quote) {
                        state = pub ? SAfterDoctypePublicId : SAfterDoctypeSystemId;
                        return;
                    }
                    if (c == '>') {
                        error(pub ? "abrupt-doctype-public-identifier" : "abrupt-doctype-system-identifier");
                        _cur.force_quirks = true;
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                        id.append(replacement);
                    } else {
                        id.push_back(c);
                    }
                }
            }
            case SAfterDoctypePublicId:
            case SBetweenDoctypePublicSystem: {
                bool after = state == SAfterDoctypePublicId;
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    if (after) {
                        state = SBetweenDoctypePublicSystem;
                    }
                    return;
                }
                if (c == '>') {
                    state = SData;
                    emit_current();
                } else if (c == '"' || c == '\'') {
                    if (after) {
                        error("missing-whitespace-between-doctype-public-and-system-identifiers");
                    }
                    _cur.has_system = true;
                    state = c == '"' ? SDoctypeSystemIdDq : SDoctypeSystemIdSq;
                } else {
                    error("missing-quote-before-doctype-system-identifier");
                    _cur.force_quirks = true;
                    reconsume();
                    state = SBogusDoctype;
                }
                return;
            }
            case SAfterDoctypeSystemId: {
                char c = consume();
                if (eof_now()) {
                    error("eof-in-doctype");
                    _cur.force_quirks = true;
                    emit_current();
                    state = SData;
                    return;
                }
                if (html_ws(c)) {
                    return;
                }
                if (c == '>') {
                    state = SData;
                    emit_current();
                } else {
                    error("unexpected-character-after-doctype-system-identifier");
                    reconsume();
                    state = SBogusDoctype;
                }
                return;
            }
            case SBogusDoctype: {
                while (true) {
                    char c = consume();
                    if (eof_now()) {
                        emit_current();
                        state = SData;
                        return;
                    }
                    if (c == '>') {
                        state = SData;
                        emit_current();
                        return;
                    }
                    if (c == '\0') {
                        error("unexpected-null-character");
                    }
                }
            }
            case SCdataSection: {
                size_t e = _s.find("]]>", _i);
                if (e == std::string_view::npos) {
                    chars(_s.substr(_i));
                    _i = _s.size();
                    error("eof-in-cdata");
                    emit_eof();
                    return;
                }
                chars(_s.substr(_i, e - _i));
                _i = e + 3;
                state = SData;
                return;
            }
            default:
                state = SData;
                return;
        }
    }
}
