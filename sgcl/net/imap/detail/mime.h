//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "syntax.h"
#include "words.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// A server's view of a message's structure, by the octets (RFC 9051
// §6.4.5, §7.5.2): the header and the body of the message and of every
// MIME part (RFC 2045, RFC 2046), each part's type and fields, the
// message/rfc822 parts' messages inside; from it the ENVELOPE and the
// BODYSTRUCTURE as the server writes them, and the octets of a section
// (BODY[1.2.HEADER], BODY[HEADER.FIELDS (FROM)]). Offsets into the bytes
// the caller keeps. Malformed input is taken as it can be: a part without
// its closing boundary ends where the bytes do, a field without a colon
// is passed over, the depth of nesting is bounded.
namespace sgcl::net::imap::detail {
    struct Field {
        std::string_view name;
        std::string_view value;   // raw, folded as written
    };

    // The value unfolded (CRLF before a blank removed) and trimmed
    inline std::string unfold(std::string_view v) {
        std::string out;
        out.reserve(v.size());
        for (size_t i = 0; i < v.size(); ++i) {
            const char c = v[i];
            if (c == '\r' || c == '\n') {
                continue;
            }
            out += c;
        }
        size_t a = 0;
        while (a < out.size() && (out[a] == ' ' || out[a] == '\t')) {
            ++a;
        }
        size_t b = out.size();
        while (b > a && (out[b - 1] == ' ' || out[b - 1] == '\t')) {
            --b;
        }
        return out.substr(a, b - a);
    }

    // The fields of a header (to the blank line or the end)
    inline void parse_fields(std::string_view h, std::vector<Field>& out) {
        size_t i = 0;
        while (i < h.size()) {
            // a line: to LF; continuation lines start with a blank
            size_t eol = h.find('\n', i);
            if (eol == std::string_view::npos) {
                eol = h.size();
            }
            std::string_view line = h.substr(i, eol - i);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            if (line.empty()) {
                break;   // the blank line
            }
            size_t end = eol < h.size() ? eol + 1 : eol;
            while (end < h.size() && (h[end] == ' ' || h[end] == '\t')) {
                size_t e2 = h.find('\n', end);
                end = e2 == std::string_view::npos ? h.size() : e2 + 1;
            }
            const size_t colon = line.find(':');
            if (colon != std::string_view::npos && colon > 0) {
                std::string_view name = line.substr(0, colon);
                while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
                    name.remove_suffix(1);
                }
                bool ok = !name.empty();
                for (unsigned char c : name) {
                    ok &= c > 32 && c < 127;
                }
                if (ok) {
                    std::string_view value = h.substr(i + colon + 1, end - (i + colon + 1));
                    out.push_back(Field{name, value});
                }
            }
            i = end;
        }
    }

    // Content-Type's and Content-Disposition's value: the token before ";"
    // and the parameters (RFC 2045 §5.1, the continuations and charsets
    // of RFC 2231 joined and decoded)
    struct Params {
        std::string value;
        std::vector<std::pair<std::string, std::string>> list;   // names in lower case

        const std::string* find(std::string_view name) const noexcept {
            for (const auto& [k, v] : list) {
                if (iequal(k, name)) {
                    return &v;
                }
            }
            return nullptr;
        }
    };

    inline void skip_cfws(std::string_view s, size_t& i) noexcept {
        for (;;) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
                ++i;
            }
            if (i < s.size() && s[i] == '(') {
                int depth = 0;
                while (i < s.size()) {
                    if (s[i] == '\\') {
                        i = std::min(i + 2, s.size());   // an escape at the end ends the comment
                        continue;
                    }
                    if (s[i] == '(') {
                        ++depth;
                    } else if (s[i] == ')') {
                        if (--depth == 0) {
                            ++i;
                            break;
                        }
                    }
                    ++i;
                }
                continue;
            }
            return;
        }
    }

    // A Content-Type or Content-Disposition read by encoding::email: the
    // value lower case ("text/plain", "attachment"), the parameters with
    // RFC 2231's continuations joined and their charsets converted
    inline Params parse_params_field(std::string_view v) {
        Params p;
        std::vector<encoding::detail::MailParam> list;
        p.value = encoding::detail::mail_parse_params(unfold(v), list);
        p.list.reserve(list.size());
        for (auto& m : list) {
            p.list.emplace_back(std::move(m.name), std::move(m.value));
        }
        return p;
    }

    struct Part {
        size_t header_begin = 0;    // the part's MIME header (for a message, its header)
        size_t body_begin = 0;      // past the blank line
        size_t body_end = 0;
        std::vector<Field> fields;
        std::string type = "text";
        std::string subtype = "plain";
        Params content_type;
        std::string encoding;       // as written, lower case; empty: 7bit
        bool is_message = false;    // the whole of a message (the top, or inside message/rfc822)
        std::vector<std::unique_ptr<Part>> children;   // a multipart's parts; a message/rfc822's message (one)

        const Field* field(std::string_view name) const noexcept {
            for (const auto& f : fields) {
                if (iequal(f.name, name)) {
                    return &f;
                }
            }
            return nullptr;
        }

        bool multipart() const noexcept {
            return type == "multipart";
        }

        bool embedded_message() const noexcept {
            return type == "message" && (subtype == "rfc822" || subtype == "global") && !children.empty();
        }
    };

    // The end of the header: the offset of the body (past the blank line)
    // and of the header's text with the blank line
    inline size_t header_end(std::string_view s, size_t from) noexcept {
        size_t i = from;
        if (i < s.size() && (s[i] == '\n' || (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n'))) {
            return s[i] == '\n' ? i + 1 : i + 2;   // an empty header
        }
        for (;;) {
            const size_t lf = s.find('\n', i);
            if (lf == std::string_view::npos) {
                return s.size();
            }
            i = lf + 1;
            if (i < s.size() && s[i] == '\n') {
                return i + 1;
            }
            if (i + 1 < s.size() && s[i] == '\r' && s[i + 1] == '\n') {
                return i + 2;
            }
            if (i >= s.size()) {
                return s.size();
            }
        }
    }

    inline void parse_part(std::string_view s, Part& p, size_t begin, size_t end, bool digest_child, int depth);

    inline void parse_multipart_body(std::string_view s, Part& p, int depth) {
        const std::string* boundary = p.content_type.find("boundary");
        if (!boundary || boundary->empty() || boundary->size() > 200) {
            return;
        }
        const std::string delim = "--" + *boundary;
        // positions of delimiter lines in [body_begin, body_end)
        size_t i = p.body_begin;
        size_t part_start = std::string_view::npos;
        const bool digest = p.subtype == "digest";
        while (i < p.body_end) {
            // i is at the start of a line
            size_t eol = s.find('\n', i);
            if (eol == std::string_view::npos || eol >= p.body_end) {
                eol = p.body_end;
            }
            std::string_view line = s.substr(i, eol - i);
            if (line.size() >= delim.size() && line.compare(0, delim.size(), delim) == 0) {
                std::string_view tail = line.substr(delim.size());
                bool close = tail.size() >= 2 && tail[0] == '-' && tail[1] == '-';
                std::string_view rest = close ? tail.substr(2) : tail;
                bool blank = true;
                for (char c : rest) {
                    blank &= c == ' ' || c == '\t' || c == '\r';
                }
                if (blank) {
                    if (part_start != std::string_view::npos) {
                        // the part ends before the CRLF in front of this line
                        size_t part_end = i;
                        if (part_end > part_start && s[part_end - 1] == '\n') {
                            --part_end;
                            if (part_end > part_start && s[part_end - 1] == '\r') {
                                --part_end;
                            }
                        }
                        auto child = std::make_unique<Part>();
                        parse_part(s, *child, part_start, part_end, digest, depth + 1);
                        p.children.push_back(std::move(child));
                        if (p.children.size() > 10000) {
                            return;
                        }
                    }
                    if (close) {
                        return;
                    }
                    part_start = eol < p.body_end ? eol + 1 : p.body_end;
                }
            }
            i = eol + 1;
        }
        if (part_start != std::string_view::npos && part_start <= p.body_end) {
            // no closing delimiter: the last part runs to the end
            auto child = std::make_unique<Part>();
            parse_part(s, *child, part_start, p.body_end, digest, depth + 1);
            p.children.push_back(std::move(child));
        }
    }

    inline void parse_part(std::string_view s, Part& p, size_t begin, size_t end, bool digest_child, int depth) {
        p.header_begin = begin;
        std::string_view region = s.substr(0, end);
        p.body_begin = std::min(header_end(region, begin), end);
        p.body_end = end;
        parse_fields(s.substr(begin, p.body_begin - begin), p.fields);
        if (const Field* ct = p.field("content-type")) {
            p.content_type = parse_params_field(ct->value);
            const size_t slash = p.content_type.value.find('/');
            std::string t = p.content_type.value.substr(0, slash);
            std::string sub = slash == std::string::npos ? std::string() : p.content_type.value.substr(slash + 1);
            if (!t.empty() && !sub.empty()) {
                p.type = t;
                p.subtype = sub;
            } else if (digest_child) {
                p.type = "message";
                p.subtype = "rfc822";
            }
        } else if (digest_child) {
            p.type = "message";
            p.subtype = "rfc822";
        }
        if (const Field* cte = p.field("content-transfer-encoding")) {
            p.encoding = to_lower(unfold(cte->value));
        }
        if (depth > 40) {
            return;
        }
        if (p.multipart()) {
            parse_multipart_body(s, p, depth);
        } else if (p.type == "message" && (p.subtype == "rfc822" || p.subtype == "global")) {
            auto inner = std::make_unique<Part>();
            inner->is_message = true;
            parse_part(s, *inner, p.body_begin, p.body_end, false, depth + 1);
            p.children.push_back(std::move(inner));
        }
    }

    // A message's parts, from the bytes the caller keeps
    struct Structure {
        Part root;

        explicit Structure(std::string_view s) {
            root.is_message = true;
            parse_part(s, root, 0, s.size(), false, 0);
        }
    };

    inline uint64_t count_lines(std::string_view s) noexcept {
        uint64_t n = 0;
        for (char c : s) {
            n += c == '\n';
        }
        if (!s.empty() && s.back() != '\n') {
            ++n;
        }
        return n;
    }

    // --- addresses (RFC 5322 §3.4, as an envelope gives them) --------------

    struct Mailbox {
        std::string name;       // display name, as written (encoded words kept)
        std::string adl;
        std::string local;
        std::string host;
        int group = 0;          // 1: a group's start (local is its name), 2: its end
    };

    inline std::string read_phrase_word(std::string_view s, size_t& i) {
        std::string out;
        if (i < s.size() && s[i] == '"') {
            ++i;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    ++i;
                }
                if (s[i] != '\r' && s[i] != '\n') {
                    out += s[i];
                }
                ++i;
            }
            if (i < s.size()) {
                ++i;
            }
            return out;
        }
        while (i < s.size()) {
            const char c = s[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '<' || c == '>' || c == '(' || c == ')' || c == ',' || c == ';' || c == ':'
                || c == '@' || c == '[' || c == ']') {
                break;
            }
            out += c;
            ++i;
        }
        return out;
    }

    // addr-spec at i: local-part "@" domain; false when there is none
    inline bool read_addr_spec(std::string_view s, size_t& i, std::string& local, std::string& host) {
        local.clear();
        host.clear();
        skip_cfws(s, i);
        // local-part: words joined by "."
        for (;;) {
            skip_cfws(s, i);
            std::string w = read_phrase_word(s, i);
            local += w;
            skip_cfws(s, i);
            if (i < s.size() && s[i] == '.') {
                local += '.';
                ++i;
                continue;
            }
            break;
        }
        skip_cfws(s, i);
        if (i < s.size() && s[i] == '@') {
            ++i;
            skip_cfws(s, i);
            if (i < s.size() && s[i] == '[') {
                const size_t close = s.find(']', i);
                const size_t e = close == std::string_view::npos ? s.size() : close + 1;
                host = std::string(s.substr(i, e - i));
                i = e;
            } else {
                for (;;) {
                    skip_cfws(s, i);
                    host += read_phrase_word(s, i);
                    skip_cfws(s, i);
                    if (i < s.size() && s[i] == '.') {
                        host += '.';
                        ++i;
                        continue;
                    }
                    break;
                }
            }
        }
        return !local.empty();
    }

    inline void parse_address_field(std::string_view s, std::vector<Mailbox>& out) {
        size_t i = 0;
        int guard = 0;
        while (i < s.size() && ++guard < 10000) {
            skip_cfws(s, i);
            if (i >= s.size()) {
                break;
            }
            if (s[i] == ',') {
                ++i;
                continue;
            }
            if (s[i] == ';') {
                out.push_back(Mailbox{{}, {}, {}, {}, 2});
                ++i;
                continue;
            }
            // a phrase, then "<", ":" (a group) or an addr-spec
            const size_t start = i;
            std::string phrase;
            size_t j = i;
            bool angle = false, group = false;
            while (j < s.size()) {
                skip_cfws(s, j);
                if (j >= s.size()) {
                    break;
                }
                const char c = s[j];
                if (c == '<') {
                    angle = true;
                    break;
                }
                if (c == ':') {
                    group = true;
                    break;
                }
                if (c == ',' || c == ';' || c == '@' || c == '>') {
                    break;
                }
                const size_t before = j;
                std::string w = read_phrase_word(s, j);
                if (j == before) {
                    ++j;   // a character of no meaning here
                    continue;
                }
                if (!phrase.empty()) {
                    phrase += ' ';
                }
                phrase += w;
                if (j < s.size() && s[j] == '.') {
                    phrase += '.';
                    ++j;
                }
            }
            if (group) {
                out.push_back(Mailbox{{}, {}, phrase, {}, 1});
                i = j + 1;
                continue;
            }
            Mailbox m;
            if (angle) {
                m.name = phrase;
                i = j + 1;
                skip_cfws(s, i);
                if (i < s.size() && s[i] == '@') {
                    // obs-route: @a,@b:
                    const size_t colon = s.find(':', i);
                    if (colon != std::string_view::npos) {
                        m.adl = std::string(s.substr(i, colon - i));
                        i = colon + 1;
                    }
                }
                read_addr_spec(s, i, m.local, m.host);
                skip_cfws(s, i);
                const size_t close = s.find('>', i);
                i = close == std::string_view::npos ? s.size() : close + 1;
            } else {
                i = start;
                if (!read_addr_spec(s, i, m.local, m.host)) {
                    // nothing an address could be made of: to the next comma
                    const size_t comma = s.find(',', i);
                    i = comma == std::string_view::npos ? s.size() : comma + 1;
                    continue;
                }
            }
            if (!m.local.empty() || !m.host.empty()) {
                out.push_back(std::move(m));
            }
            if (out.size() > 10000) {
                break;
            }
        }
    }

    // --- writing the server's ENVELOPE and BODYSTRUCTURE -------------------

    inline void put_field_nstring(std::string& out, const Part& p, std::string_view name, bool utf8) {
        const Field* f = p.field(name);
        if (!f) {
            out += "NIL";
            return;
        }
        put_string(out, unfold(f->value), utf8);
    }

    inline void put_address_list(std::string& out, std::string_view value, bool utf8) {
        std::vector<Mailbox> list;
        parse_address_field(unfold(value), list);
        if (list.empty()) {
            out += "NIL";
            return;
        }
        out += '(';
        for (const auto& m : list) {
            out += '(';
            if (m.group == 2) {
                out += "NIL NIL NIL NIL";
            } else if (m.group == 1) {
                out += "NIL NIL ";
                put_string(out, m.local, utf8);
                out += " NIL";
            } else {
                if (m.name.empty()) {
                    out += "NIL";
                } else {
                    put_string(out, m.name, utf8);
                }
                out += ' ';
                if (m.adl.empty()) {
                    out += "NIL";
                } else {
                    put_string(out, m.adl, utf8);
                }
                out += ' ';
                put_string(out, m.local, utf8);
                out += ' ';
                if (m.host.empty()) {
                    // RFC 9051 §7.5.2: a host of NIL is a group's syntax;
                    // an address without one gets an empty host
                    out += "\"\"";
                } else {
                    put_string(out, m.host, utf8);
                }
            }
            out += ')';
        }
        out += ')';
    }

    inline void put_envelope(std::string& out, const Part& msg, bool utf8) {
        out += '(';
        put_field_nstring(out, msg, "date", utf8);
        out += ' ';
        put_field_nstring(out, msg, "subject", utf8);
        const Field* from = msg.field("from");
        auto addr = [&](std::string_view name, bool default_from) {
            out += ' ';
            const Field* f = msg.field(name);
            if (!f || unfold(f->value).empty()) {
                f = default_from ? from : nullptr;
            }
            if (!f) {
                out += "NIL";
            } else {
                put_address_list(out, f->value, utf8);
            }
        };
        addr("from", false);
        addr("sender", true);
        addr("reply-to", true);
        addr("to", false);
        addr("cc", false);
        addr("bcc", false);
        out += ' ';
        put_field_nstring(out, msg, "in-reply-to", utf8);
        out += ' ';
        put_field_nstring(out, msg, "message-id", utf8);
        out += ')';
    }

    inline void put_params(std::string& out, const std::vector<std::pair<std::string, std::string>>& list, bool utf8) {
        if (list.empty()) {
            out += "NIL";
            return;
        }
        out += '(';
        bool first = true;
        for (const auto& [k, v] : list) {
            if (!first) {
                out += ' ';
            }
            first = false;
            std::string key = to_upper(k);
            put_string(out, key, utf8);
            out += ' ';
            put_string(out, v, utf8);
        }
        out += ')';
    }

    inline void put_body(std::string& out, std::string_view s, const Part& p, bool extended, bool utf8, int depth = 0);

    // body-fld-dsp SP body-fld-lang SP body-fld-loc
    inline void put_ext_tail(std::string& out, const Part& p, bool utf8) {
        out += ' ';
        if (const Field* d = p.field("content-disposition")) {
            Params dp = parse_params_field(d->value);
            if (dp.value.empty()) {
                out += "NIL";
            } else {
                out += '(';
                put_string(out, to_upper(dp.value), utf8);
                out += ' ';
                put_params(out, dp.list, utf8);
                out += ')';
            }
        } else {
            out += "NIL";
        }
        out += ' ';
        if (const Field* l = p.field("content-language")) {
            std::string v = unfold(l->value);
            std::vector<std::string> langs;
            size_t i = 0;
            while (i < v.size()) {
                while (i < v.size() && (v[i] == ' ' || v[i] == ',' || v[i] == '\t')) {
                    ++i;
                }
                size_t j = i;
                while (j < v.size() && v[j] != ',' && v[j] != ' ' && v[j] != '\t') {
                    ++j;
                }
                if (j > i) {
                    langs.push_back(v.substr(i, j - i));
                }
                i = j;
            }
            if (langs.empty()) {
                out += "NIL";
            } else if (langs.size() == 1) {
                put_string(out, langs[0], utf8);
            } else {
                out += '(';
                for (size_t k = 0; k < langs.size(); ++k) {
                    if (k) {
                        out += ' ';
                    }
                    put_string(out, langs[k], utf8);
                }
                out += ')';
            }
        } else {
            out += "NIL";
        }
        out += ' ';
        put_field_nstring(out, p, "content-location", utf8);
    }

    inline void put_body(std::string& out, std::string_view s, const Part& p, bool extended, bool utf8, int depth) {
        out += '(';
        if (p.multipart() && !p.children.empty() && depth < 40) {
            for (const auto& c : p.children) {
                put_body(out, s, *c, extended, utf8, depth + 1);
            }
            out += ' ';
            put_string(out, to_upper(p.subtype), utf8);
            if (extended) {
                out += ' ';
                std::vector<std::pair<std::string, std::string>> params = p.content_type.list;
                put_params(out, params, utf8);
                put_ext_tail(out, p, utf8);
            }
            out += ')';
            return;
        }
        // a multipart without parts is shown as text/plain (RFC 9051 has no
        // form for an empty multipart)
        const bool empty_multipart = p.multipart();
        std::string type = empty_multipart ? "TEXT" : to_upper(p.type);
        std::string subtype = empty_multipart ? "PLAIN" : to_upper(p.subtype);
        put_string(out, type, utf8);
        out += ' ';
        put_string(out, subtype, utf8);
        out += ' ';
        std::vector<std::pair<std::string, std::string>> params = p.content_type.list;
        if (params.empty() && p.type == "text") {
            params.emplace_back("charset", "us-ascii");
        }
        if (empty_multipart) {
            params.clear();
        }
        put_params(out, params, utf8);
        out += ' ';
        put_field_nstring(out, p, "content-id", utf8);
        out += ' ';
        put_field_nstring(out, p, "content-description", utf8);
        out += ' ';
        if (p.encoding.empty()) {
            out += "\"7BIT\"";
        } else {
            put_string(out, to_upper(p.encoding), utf8);
        }
        out += ' ';
        const std::string_view body = s.substr(p.body_begin, p.body_end - p.body_begin);
        put_number(out, body.size());
        if (p.embedded_message() && depth < 40) {
            const Part& inner = *p.children[0];
            out += ' ';
            put_envelope(out, inner, utf8);
            out += ' ';
            put_body(out, s, inner, extended, utf8, depth + 1);
            out += ' ';
            put_number(out, count_lines(body));
        } else if (p.type == "text" || empty_multipart) {
            out += ' ';
            put_number(out, count_lines(body));
        }
        if (extended) {
            out += ' ';
            put_field_nstring(out, p, "content-md5", utf8);
            put_ext_tail(out, p, utf8);
        }
        out += ')';
    }

    // --- sections (RFC 9051 §6.4.5) -----------------------------------------

    struct Section {
        std::vector<uint32_t> path;           // the part numbers
        enum class Text : uint8_t { all, header, header_fields, header_fields_not, text, mime } text = Text::all;
        std::vector<std::string> fields;      // HEADER.FIELDS' names
    };

    // The text inside "[...]"; false for one that is not a section
    inline bool parse_section(std::string_view t, Section& out) {
        Lexer x(t);
        while (x.peek() >= '1' && x.peek() <= '9') {
            uint32_t n;
            if (!x.nz_number(n)) {
                return false;
            }
            out.path.push_back(n);
            if (out.path.size() > 100) {
                return false;
            }
            if (!x.eat('.')) {
                return x.at_end();
            }
        }
        if (x.at_end()) {
            return out.path.empty();
        }
        if (x.word("HEADER.FIELDS.NOT")) {
            out.text = Section::Text::header_fields_not;
        } else if (x.word("HEADER.FIELDS")) {
            out.text = Section::Text::header_fields;
        } else if (x.word("HEADER")) {
            out.text = Section::Text::header;
            return x.at_end();
        } else if (x.word("TEXT")) {
            out.text = Section::Text::text;
            return x.at_end();
        } else if (x.word("MIME")) {
            out.text = Section::Text::mime;
            return !out.path.empty() && x.at_end();
        } else {
            return false;
        }
        if (!x.sp() || !x.eat('(')) {
            return false;
        }
        std::string scratch;
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view f;
            if (!x.astring(f, scratch)) {
                return false;
            }
            out.fields.emplace_back(f);
        }
        return !out.fields.empty() && x.at_end();
    }

    // The part a path names (RFC 9051 §6.4.5): the parts of a message are
    // those of its body, the parts of its multipart or, for a single
    // part, "1" the body itself; a message/rfc822 part's numbers go on into
    // the message it holds. nullptr for a path to nothing
    inline const Part* resolve_part(const Part& msg, const std::vector<uint32_t>& path) noexcept {
        const Part* cur = &msg;
        bool container = true;   // cur is a message, not yet one of its parts
        for (const uint32_t n : path) {
            if (!container && !cur->multipart()) {
                if (!cur->embedded_message()) {
                    return nullptr;
                }
                cur = cur->children[0].get();
                container = true;
            }
            if (cur->multipart()) {
                if (n == 0 || n > cur->children.size()) {
                    return nullptr;
                }
                cur = cur->children[n - 1].get();
            } else if (n != 1 || !container) {
                return nullptr;
            }
            container = false;
        }
        return cur;
    }

    // The octets of a section of the message, appended to out; false for a
    // section of no part (an empty string in the response then, as RFC
    // 9051 §6.4.5 allows for a part that does not exist)
    inline bool section_bytes(std::string_view s, const Part& msg, const Section& sec, std::string& out) {
        const Part* p = resolve_part(msg, sec.path);
        if (!p) {
            return false;
        }
        // the message whose header a HEADER or TEXT of a path names: the
        // part must be a message/rfc822 (its message), or the top
        const Part* m = p;
        if (!sec.path.empty() && sec.text != Section::Text::all && sec.text != Section::Text::mime) {
            if (!p->embedded_message()) {
                return false;
            }
            m = p->children[0].get();
        }
        switch (sec.text) {
            case Section::Text::all:
                if (sec.path.empty()) {
                    out.append(s.data(), s.size());
                } else {
                    out.append(s.data() + p->body_begin, p->body_end - p->body_begin);
                }
                return true;
            case Section::Text::header:
                out.append(s.data() + m->header_begin, m->body_begin - m->header_begin);
                return true;
            case Section::Text::text:
                out.append(s.data() + m->body_begin, m->body_end - m->body_begin);
                return true;
            case Section::Text::mime:
                out.append(s.data() + p->header_begin, p->body_begin - p->header_begin);
                return true;
            case Section::Text::header_fields:
            case Section::Text::header_fields_not: {
                const bool want = sec.text == Section::Text::header_fields;
                // the lines of each field, in their order, then the blank line
                std::string_view h = s.substr(m->header_begin, m->body_begin - m->header_begin);
                size_t i = 0;
                while (i < h.size()) {
                    size_t eol = h.find('\n', i);
                    eol = eol == std::string_view::npos ? h.size() : eol + 1;
                    std::string_view line = h.substr(i, eol - i);
                    if (line == "\r\n" || line == "\n") {
                        break;
                    }
                    size_t end = eol;
                    while (end < h.size() && (h[end] == ' ' || h[end] == '\t')) {
                        size_t e2 = h.find('\n', end);
                        end = e2 == std::string_view::npos ? h.size() : e2 + 1;
                    }
                    const size_t colon = line.find(':');
                    bool match = false;
                    if (colon != std::string_view::npos) {
                        std::string_view name = line.substr(0, colon);
                        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
                            name.remove_suffix(1);
                        }
                        for (const auto& f : sec.fields) {
                            match |= iequal(f, name);
                        }
                    }
                    if (match == want && colon != std::string_view::npos) {
                        out.append(h.data() + i, end - i);
                    }
                    i = end;
                }
                out += "\r\n";
                return true;
            }
        }
        return false;
    }

    // The section's text as the response names it ("1.2.HEADER.FIELDS
    // (FROM TO)"), field names in upper case
    inline std::string section_name(const Section& sec) {
        std::string out;
        for (size_t i = 0; i < sec.path.size(); ++i) {
            if (i) {
                out += '.';
            }
            put_number(out, sec.path[i]);
        }
        const char* t = nullptr;
        switch (sec.text) {
            case Section::Text::all: break;
            case Section::Text::header: t = "HEADER"; break;
            case Section::Text::header_fields: t = "HEADER.FIELDS"; break;
            case Section::Text::header_fields_not: t = "HEADER.FIELDS.NOT"; break;
            case Section::Text::text: t = "TEXT"; break;
            case Section::Text::mime: t = "MIME"; break;
        }
        if (t) {
            if (!out.empty()) {
                out += '.';
            }
            out += t;
        }
        if (!sec.fields.empty()) {
            out += " (";
            for (size_t i = 0; i < sec.fields.size(); ++i) {
                if (i) {
                    out += ' ';
                }
                put_astring(out, to_upper(sec.fields[i]), false);
            }
            out += ')';
        }
        return out;
    }

    // A part's content with its Content-Transfer-Encoding undone (BINARY,
    // RFC 3516); false for an encoding it does not know (UNKNOWN-CTE)
    inline bool decoded_content(std::string_view body, std::string_view encoding, std::string& out) {
        if (encoding.empty() || iequal(encoding, "7bit") || iequal(encoding, "8bit") || iequal(encoding, "binary")) {
            out.append(body.data(), body.size());
            return true;
        }
        if (iequal(encoding, "base64")) {
            encoding::detail::mail_base64(body, out);
            return true;
        }
        if (iequal(encoding, "quoted-printable")) {
            decode_quoted_printable(body, out);
            return true;
        }
        return false;
    }
}
