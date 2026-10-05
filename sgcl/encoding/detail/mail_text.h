//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../base64.h"
#include "../quoted_printable.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/utf8.h"
#include "../../core/vector.h"
#include "../../txt/encoding.h"
#include "../../txt/idna.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

// The text of a mail's head (RFC 5322, RFC 2045, RFC 2047, RFC 2231, RFC
// 6532): the tokens of a structured field, the encoded words, the
// parameters of Content-Type and Content-Disposition, the folding of a
// line, the choice of a charset's conversion; what email.h builds on.
namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    SGCL_INLINE_HOT constexpr char mail_lower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
    }

    inline constexpr bool mail_iequal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (mail_lower(a[i]) != mail_lower(b[i])) {
                return false;
            }
        }
        return true;
    }

    inline std::string mail_lowered(std::string_view s) {
        std::string out(s);
        for (char& c : out) {
            c = mail_lower(c);
        }
        return out;
    }

    SGCL_INLINE_HOT constexpr bool mail_wsp(char c) noexcept {
        return c == ' ' || c == '\t';
    }

    inline std::string_view mail_trim(std::string_view s) noexcept {
        while (!s.empty() && (mail_wsp(s.front()) || s.front() == '\r' || s.front() == '\n')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (mail_wsp(s.back()) || s.back() == '\r' || s.back() == '\n')) {
            s.remove_suffix(1);
        }
        return s;
    }

    // atext of RFC 5322 §3.2.3, with the bytes of UTF-8 past ASCII that
    // RFC 6532 §3.2 adds
    SGCL_INLINE_HOT constexpr bool mail_atext(uint8_t c) noexcept {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80) {
            return true;
        }
        switch (c) {
            case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-': case '/':
            case '=': case '?': case '^': case '_': case '`': case '{': case '|': case '}': case '~':
                return true;
            default:
                return false;
        }
    }

    // A token's character of RFC 2045 §5.1: printable ASCII but the
    // tspecials
    SGCL_INLINE_HOT constexpr bool mime_token_char(uint8_t c) noexcept {
        if (c <= 32 || c >= 127) {
            return false;
        }
        switch (c) {
            case '(': case ')': case '<': case '>': case '@': case ',': case ';': case ':': case '\\': case '"':
            case '/': case '[': case ']': case '?': case '=':
                return false;
            default:
                return true;
        }
    }

    // ftext of RFC 5322 §3.6.8: the characters of a field's name
    SGCL_INLINE_HOT constexpr bool mail_ftext(uint8_t c) noexcept {
        return c >= 33 && c <= 126 && c != ':';
    }

    inline bool mail_field_name_ok(std::string_view s) noexcept {
        if (s.empty()) {
            return false;
        }
        for (char c : s) {
            if (!mail_ftext(uint8_t(c))) {
                return false;
            }
        }
        return true;
    }

    inline bool mail_ascii(std::string_view s) noexcept {
        for (char c : s) {
            if (uint8_t(c) >= 0x80) {
                return false;
            }
        }
        return true;
    }

    // The folded lines of a field's value joined (RFC 5322 §2.2.3: a CRLF
    // before white space taken out; a bare LF the same)
    inline std::string mail_unfold(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\r' && i + 1 < s.size() && s[i + 1] == '\n') {
                continue;
            }
            if (c == '\n') {
                continue;
            }
            out += c;
        }
        return out;
    }

    // Random letters and digits, for a boundary and a Message-ID: drawn
    // from a generator of the thread seeded once from std::random_device
    // (the module sits below crypto; uniqueness is what is wanted, and
    // the boundary is checked against the content anyway)
    inline void mail_random(std::string& out, size_t n) {
        static thread_local std::mt19937_64 gen{[] {
            std::random_device d;
            return (uint64_t(d()) << 32) ^ d();
        }()};
        static constexpr char Alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        uint64_t bits = 0;
        int left = 0;
        for (size_t i = 0; i < n; ++i) {
            if (left < 6) {
                bits = gen();
                left = 64;
            }
            out += Alphabet[(bits & 63) % 62];
            bits >>= 6;
            left -= 6;
        }
    }

    // Bytes in a charset as UTF-8: the charsets txt converts, UTF-8 and
    // ASCII as they are; false for a charset nobody here knows (the bytes
    // then are the caller's to keep)
    inline bool mail_charset_to_utf8(std::string_view bytes, std::string_view charset, std::string& out) {
        if (charset.empty() || mail_iequal(charset, "utf-8") || mail_iequal(charset, "utf8") || mail_iequal(charset, "us-ascii")
            || mail_iequal(charset, "ascii")) {
            out.assign(bytes);
            return true;
        }
        auto e = txt::encoding_from_name(string(charset));
        if (!e) {
            return false;
        }
        if (mail_ascii(bytes) && *e != txt::encoding::utf16le && *e != txt::encoding::utf16be && *e != txt::encoding::utf32le
            && *e != txt::encoding::utf32be) {
            out.assign(bytes);   // every single byte encoding writes ASCII as itself
            return true;
        }
        auto s = txt::decode(slice<const byte>(reinterpret_cast<const byte*>(bytes.data()), bytes.size()), *e);
        out.assign(s.data(), s.size());
        return true;
    }

    // A base64 text of a mail decoded the way the readers of mail do: the
    // line breaks skipped, and when the text holds anything else (white
    // space, characters outside the alphabet, padding in the middle, a
    // group cut short) only the characters of the alphabet taken, up to
    // the first padding
    inline void mail_base64(std::string_view text, std::string& out) {
        const size_t start = out.size();
        const size_t room = (text.size() / 4 + 1) * 3;
        out.resize(start + room);
        auto r = base64::standard.lenient().decode_to(slice<byte>(reinterpret_cast<byte*>(out.data() + start), room),
                                                      slice<const char>(text.data(), text.size()));
        if (r) {
            out.resize(start + *r);
            return;
        }
        out.resize(start);
        uint32_t acc = 0;
        int bits = 0;
        for (char ch : text) {
            const uint8_t c = uint8_t(ch);
            int v;
            if (c >= 'A' && c <= 'Z') {
                v = c - 'A';
            } else if (c >= 'a' && c <= 'z') {
                v = c - 'a' + 26;
            } else if (c >= '0' && c <= '9') {
                v = c - '0' + 52;
            } else if (c == '+') {
                v = 62;
            } else if (c == '/') {
                v = 63;
            } else if (c == '=') {
                break;
            } else {
                continue;
            }
            acc = (acc << 6) | uint32_t(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out += char((acc >> bits) & 0xFF);
            }
        }
    }

    // RFC 2047 §4.2: the Q encoding's text decoded ('_' a space)
    inline void mail_q_decode(std::string_view text, std::string& out) {
        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (c == '_') {
                out += ' ';
            } else if (c == '=' && i + 2 < text.size() && qp_hex_value(uint8_t(text[i + 1])) >= 0
                       && qp_hex_value(uint8_t(text[i + 2])) >= 0) {
                out += char(qp_hex_value(uint8_t(text[i + 1])) * 16 + qp_hex_value(uint8_t(text[i + 2])));
                i += 2;
            } else {
                out += c;
            }
        }
    }

    // One encoded word at the start of s ("=?charset?Q?text?=", with an
    // RFC 2231 language after a '*'): its charset and its bytes, and how
    // much of s it took; false when s does not begin with one
    struct MailWord {
        std::string_view charset;
        std::string bytes;
        size_t size = 0;
    };

    inline bool mail_word_at(std::string_view s, MailWord& w) {
        if (s.size() < 8 || s[0] != '=' || s[1] != '?') {
            return false;
        }
        size_t q1 = s.find('?', 2);
        if (q1 == std::string_view::npos || q1 == 2 || q1 + 3 >= s.size() || s[q1 + 2] != '?') {
            return false;
        }
        std::string_view cs = s.substr(2, q1 - 2);
        for (char c : cs) {
            if (!mime_token_char(uint8_t(c)) && c != '*') {
                return false;
            }
        }
        if (auto star = cs.find('*'); star != std::string_view::npos) {
            cs = cs.substr(0, star);
        }
        const char enc = mail_lower(s[q1 + 1]);
        if (enc != 'q' && enc != 'b') {
            return false;
        }
        size_t end = s.find("?=", q1 + 3);
        if (end == std::string_view::npos) {
            return false;
        }
        std::string_view text = s.substr(q1 + 3, end - q1 - 3);
        if (text.find_first_of("\r\n") != std::string_view::npos) {
            return false;
        }
        w.charset = cs;
        w.bytes.clear();
        if (enc == 'b') {
            mail_base64(text, w.bytes);
        } else {
            mail_q_decode(text, w.bytes);
        }
        w.size = end + 2;
        return true;
    }

    // Unstructured text with its encoded words decoded (RFC 2047 §6): the
    // white space between two encoded words dropped, the bytes of adjacent
    // words of one charset converted together (a character split between
    // two is whole again), a word of a charset nobody here knows left as
    // it was written. As the readers of mail do, a word is found anywhere,
    // a quoted string's inside included.
    inline std::string mail_decode_words(std::string_view s) {
        if (s.find("=?") == std::string_view::npos) {
            return std::string(s);
        }
        std::string out;
        out.reserve(s.size());
        std::string pending;           // bytes of adjacent words of one charset
        std::string_view pending_cs;
        std::string_view pending_text; // as written, for a charset not known
        size_t gap_from = std::string::npos;   // where white space after a word began in out
        auto flush = [&] {
            if (pending_text.empty()) {
                return;
            }
            std::string conv;
            if (mail_charset_to_utf8(pending, pending_cs, conv)) {
                out += conv;
            } else {
                out += pending_text;
            }
            pending.clear();
            pending_text = {};
        };
        size_t i = 0;
        MailWord w;
        while (i < s.size()) {
            if (s[i] == '=' && mail_word_at(s.substr(i), w)) {
                if (gap_from != std::string::npos) {
                    out.resize(gap_from);   // white space between two words
                }
                if (!pending_text.empty() && mail_iequal(pending_cs, w.charset)) {
                    pending += w.bytes;
                    pending_text = std::string_view(pending_text.data(), size_t(s.data() + i + w.size - pending_text.data()));
                } else {
                    flush();
                    pending = w.bytes;
                    pending_cs = w.charset;
                    pending_text = s.substr(i, w.size);
                }
                i += w.size;
                gap_from = std::string::npos;
                // white space after the word, held to see whether a word follows
                size_t j = i;
                while (j < s.size() && (mail_wsp(s[j]) || s[j] == '\r' || s[j] == '\n')) {
                    ++j;
                }
                if (j > i && j < s.size() && s[j] == '=' && mail_word_at(s.substr(j), w)) {
                    i = j;   // the next word joins this one; the space dropped
                    continue;
                }
                flush();
                continue;
            }
            flush();
            out += s[i];
            ++i;
        }
        flush();
        return out;
    }

    // The tokens of a structured field (RFC 5322 §3.2): comments, quoted
    // strings, atoms, domain literals, the specials. Lenient as the
    // obsolete syntax of §4 is: white space and comments anywhere between
    // tokens, a quoted pair of any character.
    struct MailLexer {
        std::string_view s;
        size_t i = 0;
        std::string last_comment;   // the text of the last comment skipped

        SGCL_INLINE_HOT bool done() const noexcept {
            return i >= s.size();
        }

        SGCL_INLINE_HOT char peek() const noexcept {
            return i < s.size() ? s[i] : '\0';
        }

        // White space and comments; false for a comment never closed
        bool skip_cfws() {
            for (;;) {
                while (i < s.size() && (mail_wsp(s[i]) || s[i] == '\r' || s[i] == '\n')) {
                    ++i;
                }
                if (i >= s.size() || s[i] != '(') {
                    return true;
                }
                std::string text;
                int depth = 0;
                while (i < s.size()) {
                    char c = s[i++];
                    if (c == '\\' && i < s.size()) {
                        text += s[i++];
                        continue;
                    }
                    if (c == '(') {
                        if (depth++ > 0) {
                            text += c;
                        }
                    } else if (c == ')') {
                        if (--depth == 0) {
                            break;
                        }
                        text += c;
                    } else if (c != '\r' && c != '\n') {
                        text += c;
                    }
                }
                if (depth != 0) {
                    last_comment = text;
                    return false;
                }
                last_comment = text;
            }
        }

        // A quoted string's content, the quoted pairs taken; false when
        // it is not closed
        bool quoted(std::string& out) {
            ++i;   // the opening quote
            while (i < s.size()) {
                char c = s[i++];
                if (c == '\\' && i < s.size()) {
                    out += s[i++];
                } else if (c == '"') {
                    return true;
                } else if (c == '\r' || c == '\n') {
                    continue;   // a fold inside the string
                } else {
                    out += c;
                }
            }
            return false;
        }

        // A run of atext; with dots too for a dot-atom (or obs-phrase)
        std::string_view atom(bool dots) noexcept {
            size_t from = i;
            while (i < s.size() && (mail_atext(uint8_t(s[i])) || (dots && s[i] == '.'))) {
                ++i;
            }
            return s.substr(from, i - from);
        }

        // A domain literal, "[...]" as written; false when not closed
        bool literal(std::string& out) {
            size_t from = i;
            ++i;
            while (i < s.size()) {
                char c = s[i++];
                if (c == '\\' && i < s.size()) {
                    ++i;
                } else if (c == ']') {
                    out.append(s.substr(from, i - from));
                    return true;
                }
            }
            return false;
        }
    };

    // A mailbox as parsed: the display name (decoded) and the addr-spec
    struct MailMailbox {
        std::string name;
        std::string addr;
    };

    // A phrase (display name): words of atoms (with dots, obs-phrase) and
    // quoted strings, joined by one space, each word's encoded words
    // decoded. Stops before a special that is not a word.
    inline bool mail_phrase(MailLexer& lx, std::string& out, bool& any) {
        any = false;
        for (;;) {
            if (!lx.skip_cfws()) {
                return false;
            }
            char c = lx.peek();
            std::string word;
            if (c == '"') {
                if (!lx.quoted(word)) {
                    return false;
                }
            } else if (mail_atext(uint8_t(c)) || c == '.') {
                word = std::string(lx.atom(true));
            } else {
                return true;
            }
            if (any) {
                out += ' ';
            }
            out += word;
            any = true;
        }
    }

    // local-part "@" domain, the obsolete forms (words joined by dots,
    // CFWS between them) taken and written back in the plain form
    inline bool mail_addr_spec(MailLexer& lx, std::string& out) {
        std::string local;
        for (bool first = true;; first = false) {
            if (!lx.skip_cfws()) {
                return false;
            }
            if (!first) {
                if (lx.peek() != '.') {
                    break;
                }
                ++lx.i;
                local += '.';
                if (!lx.skip_cfws()) {
                    return false;
                }
            }
            char c = lx.peek();
            if (c == '"') {
                std::string q;
                if (!lx.quoted(q)) {
                    return false;
                }
                // written back quoted when it is not a dot-atom
                bool plain = !q.empty();
                for (char ch : q) {
                    if (!mail_atext(uint8_t(ch)) && ch != '.') {
                        plain = false;
                    }
                }
                if (plain && q.front() != '.' && q.back() != '.' && q.find("..") == std::string::npos) {
                    local += q;
                } else {
                    local += '"';
                    for (char ch : q) {
                        if (ch == '"' || ch == '\\') {
                            local += '\\';
                        }
                        local += ch;
                    }
                    local += '"';
                }
            } else if (mail_atext(uint8_t(c))) {
                local += lx.atom(false);
            } else if (c == '.') {
                continue;   // obs-local-part's empty word: "a..b"
            } else {
                return false;
            }
        }
        if (lx.peek() != '@') {
            return false;
        }
        ++lx.i;
        if (!lx.skip_cfws()) {
            return false;
        }
        std::string domain;
        if (lx.peek() == '[') {
            if (!lx.literal(domain)) {
                return false;
            }
        } else {
            for (bool first = true;; first = false) {
                if (!first) {
                    if (!lx.skip_cfws()) {
                        return false;
                    }
                    if (lx.peek() != '.') {
                        break;
                    }
                    ++lx.i;
                    domain += '.';
                    if (!lx.skip_cfws()) {
                        return false;
                    }
                }
                auto a = lx.atom(false);
                if (a.empty()) {
                    return false;
                }
                domain += a;
            }
        }
        if (local.empty() || domain.empty()) {
            return false;
        }
        out = local + "@" + domain;
        return true;
    }

    // An angle-addr after its '<': an obsolete route ("@a,@b:") skipped,
    // the addr-spec, the '>'
    inline bool mail_angle_addr(MailLexer& lx, std::string& addr) {
        ++lx.i;   // '<'
        if (!lx.skip_cfws()) {
            return false;
        }
        if (lx.peek() == '@' || lx.peek() == ',') {
            while (!lx.done() && lx.peek() != ':' && lx.peek() != '>') {
                ++lx.i;
            }
            if (lx.peek() != ':') {
                return false;
            }
            ++lx.i;
        }
        if (!mail_addr_spec(lx, addr)) {
            return false;
        }
        if (!lx.skip_cfws() || lx.peek() != '>') {
            return false;
        }
        ++lx.i;
        return true;
    }

    // One mailbox: name-addr or addr-spec; an addr-spec's comment is its
    // name when it has no other (Appendix A.5's "(John Doe)")
    inline bool mail_mailbox(MailLexer& lx, MailMailbox& m) {
        size_t start = lx.i;
        lx.last_comment.clear();
        if (!lx.skip_cfws()) {
            return false;
        }
        if (lx.peek() == '<') {
            if (!mail_angle_addr(lx, m.addr)) {
                return false;
            }
            return lx.skip_cfws();
        }
        // try addr-spec first, then a phrase and an angle-addr
        size_t after_ws = lx.i;
        MailLexer probe = lx;
        std::string addr;
        probe.last_comment.clear();
        if (mail_addr_spec(probe, addr)) {
            if (!probe.skip_cfws()) {
                return false;
            }
            char c = probe.peek();
            if (c == '\0' || c == ',' || c == ';') {
                m.addr = addr;
                if (!probe.last_comment.empty()) {
                    m.name = mail_decode_words(mail_trim(probe.last_comment));
                }
                lx = probe;
                return true;
            }
        }
        lx.i = after_ws;
        std::string name;
        bool any = false;
        if (!mail_phrase(lx, name, any)) {
            return false;
        }
        if (lx.peek() != '<') {
            lx.i = start;
            return false;
        }
        if (!mail_angle_addr(lx, m.addr)) {
            return false;
        }
        m.name = mail_decode_words(name);
        return lx.skip_cfws();
    }

    // An address list (RFC 5322 §3.4, §4.4): mailboxes and groups, a
    // group's members in its place, empty elements (",,") skipped; false
    // for text that is not one
    inline bool mail_address_list_in(MailLexer& lx, std::vector<MailMailbox>& out) {
        bool in_group = false;
        for (;;) {
            if (!lx.skip_cfws()) {
                return false;
            }
            if (lx.done()) {
                return !in_group;
            }
            char c = lx.peek();
            if (c == ',') {
                ++lx.i;
                continue;
            }
            if (c == ';' && in_group) {
                ++lx.i;
                in_group = false;
                continue;
            }
            // a group's display name ends with ':'
            if (!in_group) {
                MailLexer probe = lx;
                std::string name;
                bool any = false;
                if (mail_phrase(probe, name, any) && any && probe.peek() == ':') {
                    lx = probe;
                    ++lx.i;
                    in_group = true;
                    continue;
                }
            }
            MailMailbox m;
            if (!mail_mailbox(lx, m)) {
                return false;
            }
            out.push_back(std::move(m));
            if (!lx.skip_cfws()) {
                return false;
            }
            c = lx.peek();
            if (c == ',' ) {
                ++lx.i;
            } else if (c == ';' && in_group) {
                ++lx.i;
                in_group = false;
            } else if (c != '\0') {
                return false;
            }
        }
    }

    // The same over a whole text; fail_at, when given, is where the text
    // stops being a list
    inline bool mail_address_list(std::string_view text, std::vector<MailMailbox>& out, size_t* fail_at) {
        MailLexer lx{text};
        if (!mail_address_list_in(lx, out)) {
            if (fail_at) {
                *fail_at = std::min(lx.i, text.size());
            }
            return false;
        }
        return true;
    }

    // RFC 2047 §5 (3): the characters an encoded word in a phrase may
    // hold as themselves in the Q encoding
    SGCL_INLINE_HOT constexpr bool mail_q_plain(uint8_t c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '!' || c == '*' || c == '+'
               || c == '-' || c == '/';
    }

    // The length of a UTF-8 sequence starting with c (1 for a byte that
    // does not start one: it is a unit of its own)
    SGCL_INLINE_HOT constexpr size_t mail_utf8_len(uint8_t c) noexcept {
        return c < 0xC0 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : c < 0xF8 ? 4 : 1;
    }

    // Text as encoded words of UTF-8 (RFC 2047 §4, §5), each at most 75
    // characters, never a character split between two; Q or B, the
    // shorter. The words are separated by one space.
    inline void mail_encode_words(std::string_view text, std::string& out) {
        size_t q_len = 0;
        for (char c : text) {
            q_len += mail_q_plain(uint8_t(c)) || c == ' ' ? 1 : 3;
        }
        const bool use_q = q_len <= (text.size() + 2) / 3 * 4;
        const size_t Room = 75 - 12;   // "=?utf-8?q?" and "?="
        size_t i = 0;
        bool first = true;
        while (i < text.size()) {
            // the bytes of the next word: whole characters while they fit
            size_t j = i;
            size_t used = 0;
            while (j < text.size()) {
                size_t n = std::min(mail_utf8_len(uint8_t(text[j])), text.size() - j);
                size_t cost = 0;
                if (use_q) {
                    for (size_t k = 0; k < n; ++k) {
                        cost += mail_q_plain(uint8_t(text[j + k])) || text[j + k] == ' ' ? 1 : 3;
                    }
                } else {
                    cost = ((j - i + n + 2) / 3) * 4 - used;
                }
                if (used + cost > Room && j > i) {
                    break;
                }
                used += cost;
                j += n;
            }
            if (!first) {
                out += ' ';
            }
            first = false;
            out += use_q ? "=?utf-8?q?" : "=?utf-8?b?";
            std::string_view piece = text.substr(i, j - i);
            if (use_q) {
                for (char c : piece) {
                    uint8_t u = uint8_t(c);
                    if (c == ' ') {
                        out += '_';
                    } else if (mail_q_plain(u)) {
                        out += c;
                    } else {
                        out += '=';
                        out += QpHex[u >> 4];
                        out += QpHex[u & 15];
                    }
                }
            } else {
                auto b = base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(piece.data()), piece.size()));
                out.append(b.data(), b.size());
            }
            out += "?=";
            i = j;
        }
    }

    // Whether a word of unstructured text must go as an encoded word: a
    // byte past ASCII (without RFC 6532), a control, or what a reader
    // would take for an encoded word
    inline bool mail_word_needs_encoding(std::string_view w, bool utf8) noexcept {
        for (char c : w) {
            uint8_t u = uint8_t(c);
            if ((u >= 0x80 && !utf8) || u < 0x20 || u == 0x7F) {
                return true;
            }
        }
        return w.find("=?") != std::string_view::npos;
    }

    // Unstructured text (a Subject) as a field's value: the words that
    // need it as encoded words, a run of such words together with the
    // spaces between them, the others as they are; line breaks and
    // controls of the text are spaces (a value never ends its line)
    inline std::string mail_encode_unstructured(std::string_view text, bool utf8) {
        std::string clean;
        clean.reserve(text.size());
        for (char c : text) {
            clean += (c == '\r' || c == '\n') ? ' ' : c;
        }
        std::string out;
        size_t i = 0;
        const size_t n = clean.size();
        std::string_view s = clean;
        while (i < n) {
            // white space as it is
            size_t ws = i;
            while (i < n && mail_wsp(s[i])) {
                ++i;
            }
            if (i > ws) {
                out.append(s.substr(ws, i - ws));
                continue;
            }
            size_t w = i;
            while (i < n && !mail_wsp(s[i])) {
                ++i;
            }
            std::string_view word = s.substr(w, i - w);
            if (!mail_word_needs_encoding(word, utf8)) {
                out.append(word);
                continue;
            }
            // the run of words that need encoding, the spaces inside it with them
            size_t run_end = i;
            size_t k = i;
            for (;;) {
                size_t sp = k;
                while (k < n && mail_wsp(s[k])) {
                    ++k;
                }
                if (k == sp || k >= n) {
                    break;
                }
                size_t nw = k;
                while (k < n && !mail_wsp(s[k])) {
                    ++k;
                }
                if (!mail_word_needs_encoding(s.substr(nw, k - nw), utf8)) {
                    break;
                }
                run_end = k;
            }
            mail_encode_words(s.substr(w, run_end - w), out);
            i = run_end;
        }
        return out;
    }

    // A display name as a phrase: atoms as they are, a quoted string when
    // it holds specials, encoded words when it holds what neither can
    inline void mail_write_phrase(std::string_view name, bool utf8, std::string& out) {
        bool atoms = !name.empty() && !mail_wsp(name.front()) && !mail_wsp(name.back());
        bool printable = true;
        bool prev_space = false;
        for (char c : name) {
            uint8_t u = uint8_t(c);
            if (u < 0x20 || u == 0x7F || (u >= 0x80 && !utf8)) {
                printable = false;
                atoms = false;
            } else if (c == ' ') {
                if (prev_space) {
                    atoms = false;
                }
            } else if (!mail_atext(u)) {
                atoms = false;
            }
            prev_space = c == ' ';
        }
        if (atoms && name.find("=?") == std::string_view::npos) {
            out.append(name);
        } else if (printable && name.find("=?") == std::string_view::npos) {
            out += '"';
            for (char c : name) {
                if (c == '"' || c == '\\') {
                    out += '\\';
                }
                out += c;
            }
            out += '"';
        } else {
            std::string clean;
            for (char c : name) {
                clean += (c == '\r' || c == '\n') ? ' ' : c;
            }
            mail_encode_words(clean, out);
        }
    }

    // An addr-spec as written: the local part a dot-atom when it is one
    // (a quoted one written by the parser stays quoted), the domain in
    // ASCII by IDNA when the head may not hold UTF-8
    inline void mail_write_addr(std::string_view addr, bool utf8, std::string& out) {
        size_t at = addr.rfind('@');
        if (at == std::string_view::npos || utf8) {
            out.append(addr);
            return;
        }
        std::string_view domain = addr.substr(at + 1);
        out.append(addr.substr(0, at + 1));
        if (!mail_ascii(domain) && domain.front() != '[') {
            if (auto a = txt::idna::to_ascii(string(domain))) {
                out.append(a->data(), a->size());
                return;
            }
        }
        out.append(domain);
    }

    inline void mail_write_mailbox(std::string_view name, std::string_view addr, bool utf8, std::string& out) {
        if (name.empty()) {
            mail_write_addr(addr, utf8, out);
            return;
        }
        mail_write_phrase(name, utf8, out);
        out += " <";
        mail_write_addr(addr, utf8, out);
        out += '>';
    }

    // A field's line written and folded (RFC 5322 §2.2.3): "Name: value",
    // broken before white space so that a line takes at most 78
    // characters where the value lets it (a word longer than that stays
    // whole, up to the 998 of §2.1.1); white space inside a quoted string
    // is not a place to break when quoted says so
    inline void mail_fold_field(std::string& out, std::string_view name, std::string_view value, bool quoted = true) {
        const size_t line_start = out.size();
        out.append(name);
        out += ": ";
        std::string_view v = value;
        while (!v.empty() && mail_wsp(v.front())) {
            v.remove_prefix(1);
        }
        size_t column = out.size() - line_start;
        if (column + v.size() <= 78) {
            out.append(v);
            out += "\r\n";
            return;
        }
        // the places to break: before a run of white space outside quotes
        size_t i = 0;
        bool in_quote = false;
        while (i < v.size()) {
            // the next word: up to white space outside a quoted string
            size_t start = i;
            while (i < v.size()) {
                char c = v[i];
                if (quoted && c == '\\' && in_quote && i + 1 < v.size()) {
                    i += 2;
                    continue;
                }
                if (quoted && c == '"') {
                    in_quote = !in_quote;
                }
                if (mail_wsp(c) && !in_quote && i > start) {
                    break;
                }
                ++i;
            }
            std::string_view word = v.substr(start, i - start);   // with its leading white space
            if (column + word.size() > 78 && column > name.size() + 2 && mail_wsp(word.front())) {
                out += "\r\n";
                column = 0;
            }
            out.append(word);
            column += word.size();
        }
        out += "\r\n";
    }

    // A parameter of Content-Type or Content-Disposition (RFC 2045 §5.1,
    // RFC 2183, RFC 2231): its name lower case, its value decoded
    struct MailParam {
        std::string name;
        std::string value;
    };

    // "type/subtype; a=b; c*=utf-8''%C5%BC" read: the value before the
    // first ';' (lower case, trimmed), and the parameters with the
    // continuations of RFC 2231 joined and their charset converted, a
    // quoted value's encoded words decoded (what Outlook writes for a
    // file's name). Lenient: a parameter without '=' or a value that is
    // not a token is taken as far as it goes.
    inline std::string mail_parse_params(std::string_view text, std::vector<MailParam>& params) {
        MailLexer lx{text};
        lx.skip_cfws();
        size_t from = lx.i;
        while (!lx.done() && lx.peek() != ';') {
            if (lx.peek() == '(') {
                break;
            }
            ++lx.i;
        }
        std::string value = mail_lowered(mail_trim(text.substr(from, lx.i - from)));
        // a value with white space inside it ("text / plain") closed up
        std::string compact;
        for (char c : value) {
            if (!mail_wsp(c)) {
                compact += c;
            }
        }
        struct Piece {
            std::string name;
            int section = -1;   // -1: not a continuation
            bool encoded = false;
            std::string value;
        };
        std::vector<Piece> pieces;
        while (!lx.done()) {
            if (!lx.skip_cfws()) {
                break;
            }
            if (lx.peek() != ';') {
                ++lx.i;   // garbage between parameters
                continue;
            }
            ++lx.i;
            if (!lx.skip_cfws()) {
                break;
            }
            size_t ns = lx.i;
            while (!lx.done() && lx.peek() != '=' && lx.peek() != ';' && !mail_wsp(lx.peek()) && lx.peek() != '(') {
                ++lx.i;
            }
            std::string name = mail_lowered(text.substr(ns, lx.i - ns));
            if (!lx.skip_cfws()) {
                break;
            }
            if (lx.peek() != '=' || name.empty()) {
                continue;
            }
            ++lx.i;
            if (!lx.skip_cfws()) {
                break;
            }
            std::string v;
            bool was_quoted = false;
            if (lx.peek() == '"') {
                lx.quoted(v);
                was_quoted = true;
            } else {
                size_t vs = lx.i;
                while (!lx.done() && lx.peek() != ';' && lx.peek() != '(') {
                    ++lx.i;
                }
                v = std::string(mail_trim(text.substr(vs, lx.i - vs)));
            }
            Piece p;
            if (!name.empty() && name.back() == '*') {
                p.encoded = true;
                name.pop_back();
            }
            if (auto star = name.find('*'); star != std::string::npos) {
                std::string_view num = std::string_view(name).substr(star + 1);
                int section = 0;
                bool digits = !num.empty() && num.size() <= 4;
                for (char c : num) {
                    if (!digits || c < '0' || c > '9') {
                        digits = false;
                        break;
                    }
                    section = section * 10 + (c - '0');
                }
                if (digits) {
                    p.section = section;
                    name.resize(star);
                }
            }
            if (!p.encoded && was_quoted) {
                v = mail_decode_words(v);
            }
            p.name = std::move(name);
            p.value = std::move(v);
            pieces.push_back(std::move(p));
        }
        auto percent = [](std::string_view s, std::string& out) {
            for (size_t k = 0; k < s.size(); ++k) {
                if (s[k] == '%' && k + 2 < s.size() && qp_hex_value(uint8_t(s[k + 1])) >= 0 && qp_hex_value(uint8_t(s[k + 2])) >= 0) {
                    out += char(qp_hex_value(uint8_t(s[k + 1])) * 16 + qp_hex_value(uint8_t(s[k + 2])));
                    k += 2;
                } else {
                    out += s[k];
                }
            }
        };
        // each name once: its continuations in order of their sections, the
        // charset of section 0 (or of the one form "name*=") applied to the
        // encoded ones; a plain value of the same name kept when there is
        // no RFC 2231 form (which wins when both are there)
        for (size_t a = 0; a < pieces.size(); ++a) {
            const std::string& name = pieces[a].name;
            bool seen = false;
            for (auto& q : params) {
                if (q.name == name) {
                    seen = true;
                    break;
                }
            }
            if (seen) {
                continue;
            }
            const Piece* simple = nullptr;
            const Piece* extended = nullptr;
            std::vector<const Piece*> sections;
            for (size_t b = a; b < pieces.size(); ++b) {
                const Piece& p = pieces[b];
                if (p.name != name) {
                    continue;
                }
                if (p.section >= 0) {
                    if (sections.size() < 1000) {
                        sections.push_back(&p);
                    }
                } else if (p.encoded) {
                    if (!extended) {
                        extended = &p;
                    }
                } else if (!simple) {
                    simple = &p;
                }
            }
            std::string out;
            if (extended || !sections.empty()) {
                std::string charset;
                std::string bytes;
                auto take_charset = [&](std::string_view v) -> std::string_view {
                    size_t q1 = v.find('\'');
                    size_t q2 = q1 == std::string_view::npos ? std::string_view::npos : v.find('\'', q1 + 1);
                    if (q2 == std::string_view::npos) {
                        return v;
                    }
                    charset = std::string(v.substr(0, q1));
                    return v.substr(q2 + 1);
                };
                if (extended) {
                    percent(take_charset(extended->value), bytes);
                } else {
                    std::sort(sections.begin(), sections.end(), [](const Piece* x, const Piece* y) { return x->section < y->section; });
                    int expect = 0;
                    for (const Piece* p : sections) {
                        if (p->section != expect) {
                            if (p->section < expect) {
                                continue;   // a section twice: the first kept
                            }
                            break;          // a gap: what came before it
                        }
                        ++expect;
                        if (p->encoded) {
                            std::string_view v = p->value;
                            if (p->section == 0) {
                                v = take_charset(v);
                            }
                            percent(v, bytes);
                        } else {
                            bytes += p->value;
                        }
                    }
                }
                if (!mail_charset_to_utf8(bytes, charset, out)) {
                    out = bytes;
                }
            } else if (simple) {
                out = simple->value;
            }
            params.push_back(MailParam{name, std::move(out)});
        }
        return compact;
    }

    // A parameter written: a token as it is, printable ASCII as a quoted
    // string, anything else by RFC 2231 in UTF-8 (name*=utf-8''...), in
    // sections of at most 60 characters when long, each on its own line
    inline void mail_write_param(std::string& out, std::string_view name, std::string_view value) {
        bool token = !value.empty();
        bool printable = true;
        for (char c : value) {
            uint8_t u = uint8_t(c);
            if (!mime_token_char(u)) {
                token = false;
            }
            if (u < 0x20 || u >= 0x7F) {
                printable = false;
            }
        }
        if (token && value.size() <= 60 && value.find("=?") == std::string_view::npos) {
            out += "; ";
            out.append(name);
            out += '=';
            out.append(value);
            return;
        }
        if (printable && value.size() <= 900 && value.find("=?") == std::string_view::npos) {   // a reader would decode a word in it
            out += "; ";
            out.append(name);
            out += "=\"";
            for (char c : value) {
                if (c == '"' || c == '\\') {
                    out += '\\';
                }
                out += c;
            }
            out += '"';
            return;
        }
        std::string enc;
        for (char c : value) {
            uint8_t u = uint8_t(c);
            if (mime_token_char(u) && c != '*' && c != '\'' && c != '%') {
                enc += c;
            } else {
                enc += '%';
                enc += QpHex[u >> 4];
                enc += QpHex[u & 15];
            }
        }
        if (enc.size() + name.size() <= 60) {
            out += "; ";
            out.append(name);
            out += "*=utf-8''";
            out += enc;
            return;
        }
        // sections, never a %XX split
        size_t i = 0;
        int section = 0;
        while (i < enc.size()) {
            size_t j = std::min(enc.size(), i + 60);
            if (j < enc.size()) {
                if (enc[j - 1] == '%') {
                    j -= 1;
                } else if (j >= 2 && enc[j - 2] == '%') {
                    j -= 2;
                }
            }
            out += ";\r\n ";
            out.append(name);
            out += '*';
            out += std::to_string(section);
            out += '*';
            out += '=';
            if (section == 0) {
                out += "utf-8''";
            }
            out.append(enc, i, j - i);
            i = j;
            ++section;
        }
    }
}
