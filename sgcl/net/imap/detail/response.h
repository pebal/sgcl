//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "syntax.h"
#include "words.h"
#include "../types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// A client's reading of a server's responses (RFC 9051 §7, §9): a response
// whole (its literals inline) taken apart into its kind, its tag, its
// status and response code, or its data's name and number; then each kind
// of data read into the module's values. Every function returns false
// for bytes of another shape, and the client takes that as a malformed
// response.
namespace sgcl::net::imap::detail {
    inline string str(std::string_view s) {
        return string(s);
    }

    enum class Status : uint8_t { none, ok, no, bad, bye, preauth };

    struct Response {
        enum class Kind : uint8_t { tagged, untagged, continuation };

        Kind kind = Kind::untagged;
        std::string_view tag;          // tagged
        Status status = Status::none;  // a status response
        std::string_view code;         // the response code's atom ("UIDNEXT")
        std::string_view code_data;    // what follows it inside the brackets
        std::string_view text;         // the human-readable text (a continuation's base64)
        std::string_view name;         // a data response's name ("FETCH", "LIST")
        uint32_t number = 0;           // "* 23 EXISTS"
        bool numbered = false;
        size_t data_at = 0;            // where the data after the name begins
    };

    inline Status status_of(std::string_view w) noexcept {
        if (iequal(w, "OK")) {
            return Status::ok;
        }
        if (iequal(w, "NO")) {
            return Status::no;
        }
        if (iequal(w, "BAD")) {
            return Status::bad;
        }
        if (iequal(w, "BYE")) {
            return Status::bye;
        }
        if (iequal(w, "PREAUTH")) {
            return Status::preauth;
        }
        return Status::none;
    }

    // resp-text: ["[" resp-text-code "]" SP] text, the text optional
    // (some servers send none)
    inline bool parse_resp_text(Lexer& x, Response& r) noexcept {
        if (x.peek() == '[') {
            x.eat('[');
            r.code = x.atom();
            if (r.code.empty()) {
                return false;
            }
            if (x.eat(' ')) {
                // to the "]" closing the code, past strings and parentheses
                const size_t start = x.position();
                std::string_view s = x.source();
                size_t p = start;
                int depth = 0;
                bool quoted = false;
                while (p < s.size()) {
                    const char c = s[p];
                    if (quoted) {
                        if (c == '\\') {
                            ++p;
                        } else if (c == '"') {
                            quoted = false;
                        }
                    } else if (c == '"') {
                        quoted = true;
                    } else if (c == '(') {
                        ++depth;
                    } else if (c == ')') {
                        --depth;
                    } else if (c == ']' && depth <= 0) {
                        break;
                    }
                    ++p;
                }
                if (p >= s.size()) {
                    return false;
                }
                r.code_data = s.substr(start, p - start);
                x.seek(p);
            }
            if (!x.eat(']')) {
                return false;
            }
            x.eat(' ');
        }
        r.text = x.rest();
        return true;
    }

    // One response, its literals inline, the last CRLF off
    inline bool parse_response(std::string_view s, Response& r) noexcept {
        r = Response();
        Lexer x(s);
        x.set_lenient(true);
        if (x.eat('+')) {
            r.kind = Response::Kind::continuation;
            x.eat(' ');
            r.text = x.rest();
            return true;
        }
        if (x.eat('*')) {
            r.kind = Response::Kind::untagged;
            if (!x.sp()) {
                return false;
            }
            if (x.peek() >= '0' && x.peek() <= '9') {
                if (!x.number32(r.number)) {
                    return false;
                }
                r.numbered = true;
                if (!x.sp()) {
                    return false;
                }
            }
            r.name = x.atom();
            if (r.name.empty()) {
                return false;
            }
            if (!r.numbered) {
                r.status = status_of(r.name);
                if (r.status != Status::none) {
                    if (!x.eat(' ')) {
                        r.text = {};
                        return x.at_end();
                    }
                    return parse_resp_text(x, r);
                }
            }
            x.eat(' ');
            r.data_at = x.position();
            return true;
        }
        r.kind = Response::Kind::tagged;
        r.tag = x.run(tag_char);
        if (r.tag.empty() || !x.sp()) {
            return false;
        }
        r.status = status_of(x.atom());
        if (r.status != Status::ok && r.status != Status::no && r.status != Status::bad) {
            return false;
        }
        if (!x.eat(' ')) {
            return x.at_end();
        }
        return parse_resp_text(x, r);
    }

    // --- values -------------------------------------------------------------

    // "(" flag *(SP flag) ")", an empty list allowed
    inline bool parse_flag_list(Lexer& x, vector<string>& out) {
        if (!x.eat('(')) {
            return false;
        }
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view f;
            if (!x.flag(f)) {
                return false;
            }
            out.push_back(str(f));
        }
        return true;
    }

    inline bool parse_nstring_into(Lexer& x, string& out, std::string& scratch) {
        std::string_view v;
        bool has;
        if (!x.nstring(v, has, scratch)) {
            return false;
        }
        out = has ? str(v) : string();
        return true;
    }

    // An address: "(" name SP adl SP mailbox SP host ")"
    inline bool parse_address_list(Lexer& x, vector<address>& out, std::string& scratch) {
        if (x.word("NIL")) {
            return true;
        }
        if (!x.eat('(')) {
            return false;
        }
        while (!x.eat(')')) {
            x.eat(' ');
            if (!x.eat('(')) {
                return false;
            }
            std::string_view parts[4];
            bool has[4];
            std::string keep[4];
            for (int i = 0; i < 4; ++i) {
                if (i && !x.sp()) {
                    return false;
                }
                if (!x.nstring(parts[i], has[i], scratch)) {
                    return false;
                }
                keep[i] = std::string(parts[i]);
            }
            if (!x.eat(')')) {
                return false;
            }
            if (!has[3]) {
                continue;   // a group's start (host NIL, mailbox its name) or end: flattened
            }
            address a;
            a.name = has[0] ? str(decode_words(keep[0])) : string();
            a.mailbox = has[2] ? str(keep[2]) : string();
            a.host = str(keep[3]);
            out.push_back(a);
        }
        return true;
    }

    inline bool parse_envelope(Lexer& x, envelope& e, std::string& scratch) {
        if (!x.eat('(')) {
            return false;
        }
        string date, subject;
        if (!parse_nstring_into(x, date, scratch) || !x.sp() || !parse_nstring_into(x, subject, scratch)) {
            return false;
        }
        e.date = date;
        e.subject = str(decode_words(subject.view()));
        vector<address>* lists[6] = {&e.from, &e.sender, &e.reply_to, &e.to, &e.cc, &e.bcc};
        for (auto* l : lists) {
            if (!x.sp() || !parse_address_list(x, *l, scratch)) {
                return false;
            }
        }
        if (!x.sp() || !parse_nstring_into(x, e.in_reply_to, scratch) || !x.sp() || !parse_nstring_into(x, e.message_id, scratch)) {
            return false;
        }
        return x.eat(')');
    }

    // body-fld-param: "(" string SP string *(SP string SP string) ")" / nil
    inline bool parse_params(Lexer& x, vector<pair<string, string>>& out, std::string& scratch) {
        if (x.word("NIL")) {
            return true;
        }
        if (!x.eat('(')) {
            return false;
        }
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view k, v;
            if (!x.string(k, scratch)) {
                return false;
            }
            string key = str(to_lower(k));
            if (!x.sp() || !x.string(v, scratch)) {
                return false;
            }
            out.push_back(pair<string, string>(key, str(decode_words(v))));
        }
        return true;
    }

    inline std::string child_part(const string& prefix, size_t i) {
        std::string p(prefix.view());
        if (!p.empty()) {
            p += '.';
        }
        p += std::to_string(i);
        return p;
    }

    inline bool parse_body(Lexer& x, body_structure& b, const string& prefix, bool top, std::string& scratch, int depth);

    // body-fld-dsp, body-fld-lang, body-fld-loc, body-extension: those
    // there are of the extension data
    inline bool parse_body_ext_tail(Lexer& x, body_structure& b, std::string& scratch) {
        if (!x.eat(' ')) {
            return true;
        }
        // disposition
        if (!x.word("NIL")) {
            if (!x.eat('(')) {
                return false;
            }
            std::string_view d;
            if (!x.string(d, scratch)) {
                return false;
            }
            b.disposition = str(to_lower(d));
            if (!x.sp() || !parse_params(x, b.disposition_parameters, scratch) || !x.eat(')')) {
                return false;
            }
        }
        if (!x.eat(' ')) {
            return true;
        }
        // language
        if (x.peek() == '(') {
            x.eat('(');
            bool first = true;
            while (!x.eat(')')) {
                if (!first && !x.sp()) {
                    return false;
                }
                first = false;
                std::string_view l;
                if (!x.string(l, scratch)) {
                    return false;
                }
                b.language.push_back(str(l));
            }
        } else {
            std::string_view l;
            bool has;
            if (!x.nstring(l, has, scratch)) {
                return false;
            }
            if (has) {
                b.language.push_back(str(l));
            }
        }
        if (!x.eat(' ')) {
            return true;
        }
        if (!parse_nstring_into(x, b.location, scratch)) {
            return false;
        }
        while (x.eat(' ')) {
            if (!x.skip_value()) {
                return false;
            }
        }
        return true;
    }

    inline bool parse_body(Lexer& x, body_structure& b, const string& prefix, bool top, std::string& scratch, int depth) {
        if (depth > 50 || !x.eat('(')) {
            return false;
        }
        if (x.peek() == '(') {
            // multipart: the parts, then the subtype
            b.type = string("multipart");
            b.part = prefix;
            size_t i = 1;
            while (x.peek() == '(') {
                body_structure child;
                if (!parse_body(x, child, str(child_part(prefix, i)), false, scratch, depth + 1)) {
                    return false;
                }
                b.parts.push_back(child);
                ++i;
                x.eat(' ');
            }
            std::string_view sub;
            if (!x.string(sub, scratch)) {
                return false;
            }
            b.subtype = str(to_lower(sub));
            if (x.eat(' ')) {
                if (!parse_params(x, b.parameters, scratch) || !parse_body_ext_tail(x, b, scratch)) {
                    return false;
                }
            }
            return x.eat(')');
        }
        b.part = top ? string("1") : prefix;
        std::string_view t, sub;
        if (!x.string(t, scratch)) {
            return false;
        }
        b.type = str(to_lower(t));
        if (!x.sp() || !x.string(sub, scratch)) {
            return false;
        }
        b.subtype = str(to_lower(sub));
        if (!x.sp() || !parse_params(x, b.parameters, scratch) || !x.sp() || !parse_nstring_into(x, b.id, scratch) || !x.sp()) {
            return false;
        }
        string desc;
        if (!parse_nstring_into(x, desc, scratch) || !x.sp()) {
            return false;
        }
        b.description = str(decode_words(desc.view()));
        std::string_view enc;
        bool has_enc;
        if (!x.nstring(enc, has_enc, scratch) || !x.sp()) {
            return false;
        }
        b.encoding = has_enc ? str(to_lower(enc)) : string("7bit");
        if (!x.number(b.size)) {
            return false;
        }
        const bool message = b.type == "message" && (b.subtype == "rfc822" || b.subtype == "global");
        if (message && x.peek() == ' ') {
            const size_t at = x.position();
            x.eat(' ');
            if (x.peek() == '(') {
                envelope e;
                if (!parse_envelope(x, e, scratch) || !x.sp()) {
                    return false;
                }
                b.envelope = e;
                body_structure inner;
                const string inner_prefix = top ? string("1") : prefix;
                if (!parse_body(x, inner, inner_prefix, false, scratch, depth + 1)) {
                    return false;
                }
                if (!inner.is_multipart()) {
                    inner.part = str(child_part(inner_prefix, 1));
                }
                b.parts.push_back(inner);
                if (!x.sp() || !x.number(b.lines)) {
                    return false;
                }
            } else {
                x.seek(at);
            }
        } else if (b.type == "text") {
            if (!x.sp() || !x.number(b.lines)) {
                return false;
            }
        }
        if (x.eat(' ')) {
            // body-ext-1part: md5, then the rest
            if (!parse_nstring_into(x, b.md5, scratch) || !parse_body_ext_tail(x, b, scratch)) {
                return false;
            }
        }
        return x.eat(')');
    }

    // A section's text inside "[...]": to the "]" closing it, past strings
    // and parentheses (a header list)
    inline bool section_text(Lexer& x, std::string_view& out) noexcept {
        if (!x.eat('[')) {
            return false;
        }
        std::string_view s = x.source();
        size_t p = x.position();
        const size_t start = p;
        int depth = 0;
        bool quoted = false;
        while (p < s.size()) {
            const char c = s[p];
            if (quoted) {
                if (c == '\\') {
                    ++p;
                } else if (c == '"') {
                    quoted = false;
                }
            } else if (c == '"') {
                quoted = true;
            } else if (c == '(') {
                ++depth;
            } else if (c == ')') {
                --depth;
            } else if (c == ']' && depth <= 0) {
                out = s.substr(start, p - start);
                x.seek(p + 1);
                return true;
            } else if (c == '\r' || c == '\n') {
                return false;
            }
            ++p;
        }
        return false;
    }

    // msg-att: "(" item *(SP item) ")"
    inline bool parse_fetch(Lexer& x, message& m, std::string& scratch) {
        if (!x.eat('(')) {
            return false;
        }
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view name = x.run([](unsigned char c) { return atom_char(c) && c != '['; });
            if (name.empty()) {
                return false;
            }
            if (iequal(name, "UID")) {
                if (!x.sp() || !x.nz_number(m.uid)) {
                    return false;
                }
            } else if (iequal(name, "FLAGS")) {
                m.flags = vector<string>();
                if (!x.sp() || !parse_flag_list(x, m.flags)) {
                    return false;
                }
            } else if (iequal(name, "INTERNALDATE")) {
                std::string_view d;
                DateTime t;
                if (!x.sp() || !x.string(d, scratch) || !parse_date_time(d, t)) {
                    return false;
                }
                m.internal_date = time::datetime::from_unix(t.unix, time::zone::fixed(duration(std::chrono::minutes(t.offset))));
            } else if (iequal(name, "RFC822.SIZE")) {
                if (!x.sp() || !x.number(m.size)) {
                    return false;
                }
            } else if (iequal(name, "MODSEQ")) {
                if (!x.sp() || !x.eat('(') || !x.number(m.modseq) || !x.eat(')')) {
                    return false;
                }
            } else if (iequal(name, "ENVELOPE")) {
                envelope e;
                if (!x.sp() || !parse_envelope(x, e, scratch)) {
                    return false;
                }
                m.envelope = e;
            } else if (iequal(name, "BODYSTRUCTURE") || (iequal(name, "BODY") && x.peek() == ' ')) {
                body_structure b;
                if (!x.sp() || !parse_body(x, b, string(), true, scratch, 0)) {
                    return false;
                }
                m.body_structure = b;
            } else if (iequal(name, "BODY") || iequal(name, "BINARY") || iequal(name, "BINARY.SIZE")) {
                std::string_view sec;
                if (!section_text(x, sec)) {
                    return false;
                }
                if (x.eat('<')) {
                    uint64_t origin;
                    if (!x.number(origin) || !x.eat('>')) {
                        return false;
                    }
                }
                if (!x.sp()) {
                    return false;
                }
                if (iequal(name, "BINARY.SIZE")) {
                    uint64_t n;
                    if (!x.number(n)) {
                        return false;
                    }
                    m.binary_sizes.push_back(pair<string, uint64_t>(str(sec), n));
                    continue;
                }
                std::string_view v;
                bool has;
                if (!x.nstring(v, has, scratch)) {
                    return false;
                }
                m.sections.push_back(pair<string, string>(str(sec), has ? str(v) : string()));
            } else if (iequal(name, "RFC822") || iequal(name, "RFC822.HEADER") || iequal(name, "RFC822.TEXT")) {
                std::string_view v;
                bool has;
                if (!x.sp() || !x.nstring(v, has, scratch)) {
                    return false;
                }
                const char* key = iequal(name, "RFC822") ? "" : iequal(name, "RFC822.HEADER") ? "HEADER" : "TEXT";
                m.sections.push_back(pair<string, string>(string(key), has ? str(v) : string()));
            } else {
                // an item of an extension this side does not read
                if (x.peek() == '[') {
                    std::string_view sec;
                    if (!section_text(x, sec)) {
                        return false;
                    }
                }
                if (!x.sp() || !x.skip_value()) {
                    return false;
                }
            }
        }
        return true;
    }

    // A mailbox name of a response: UTF-8, or modified UTF-7 for a
    // connection without UTF-8 names (a name that does not decode kept as
    // it came)
    inline string mailbox_name(std::string_view raw, bool utf8) {
        if (!utf8) {
            std::string out;
            if (utf7_decode(raw, out)) {
                return str(out);
            }
        }
        if (iequal(raw, "INBOX")) {
            return string("INBOX");
        }
        return str(raw);
    }

    // mailbox-list: "(" [mbx-list-flags] ")" SP (DQUOTE QUOTED-CHAR DQUOTE
    // / nil) SP mailbox [SP mbox-list-extended]
    inline bool parse_list(Lexer& x, list_entry& e, bool utf8, std::string& scratch) {
        if (!parse_flag_list(x, e.attributes) || !x.sp()) {
            return false;
        }
        if (x.word("NIL")) {
            e.delimiter = 0;
        } else {
            std::string_view d;
            if (!x.string(d, scratch) || d.size() != 1) {
                return false;
            }
            e.delimiter = d[0];
        }
        std::string_view name;
        if (!x.sp() || !x.astring(name, scratch)) {
            return false;
        }
        e.name = mailbox_name(name, utf8);
        if (x.eat(' ')) {
            // mbox-list-extended: "(" [item *(SP item)] ")": CHILDINFO, OLDNAME
            if (!x.skip_value()) {
                return false;
            }
        }
        return x.at_end();
    }

    // STATUS's attributes: "(" [name SP number *(SP name SP number)] ")"
    inline bool parse_status_items(Lexer& x, status& s) {
        if (!x.eat('(')) {
            return false;
        }
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view k = x.atom();
            uint64_t v;
            if (k.empty() || !x.sp() || !x.number(v)) {
                return false;
            }
            if (iequal(k, "MESSAGES")) {
                s.messages = uint32_t(v);
            } else if (iequal(k, "RECENT")) {
                s.recent = uint32_t(v);
            } else if (iequal(k, "UIDNEXT")) {
                s.uid_next = uint32_t(v);
            } else if (iequal(k, "UIDVALIDITY")) {
                s.uid_validity = uint32_t(v);
            } else if (iequal(k, "UNSEEN")) {
                s.unseen = uint32_t(v);
            } else if (iequal(k, "DELETED")) {
                s.deleted = uint32_t(v);
            } else if (iequal(k, "SIZE")) {
                s.size = v;
            } else if (iequal(k, "HIGHESTMODSEQ")) {
                s.highest_modseq = v;
            }
        }
        return true;
    }

    inline bool parse_status(Lexer& x, string& name, status& s, bool utf8, std::string& scratch) {
        std::string_view n;
        if (!x.astring(n, scratch) || !x.sp()) {
            return false;
        }
        name = mailbox_name(n, utf8);
        return parse_status_items(x, s);
    }

    // SEARCH and SORT: numbers, then "(MODSEQ n)" (CONDSTORE)
    inline bool parse_numbers(Lexer& x, vector<uint32_t>& out, uint64_t& modseq) {
        while (!x.at_end()) {
            if (x.peek() == '(') {
                if (!x.eat('(') || !x.word("MODSEQ") || !x.sp() || !x.number(modseq) || !x.eat(')')) {
                    return false;
                }
                break;
            }
            uint32_t n;
            if (!x.nz_number(n)) {
                return false;
            }
            out.push_back(n);
            if (!x.eat(' ')) {
                break;
            }
            while (x.eat(' ')) {
            }
        }
        return x.at_end();
    }

    struct Esearch {
        string tag;
        bool uid = false;
        uint32_t min = 0, max = 0, count = 0;
        bool has_count = false;
        sequence_set all;
        uint64_t modseq = 0;
    };

    // ESEARCH (RFC 4731): ["(" "TAG" SP string ")"] [SP "UID"] *(SP name SP value)
    inline bool parse_esearch(Lexer& x, Esearch& e, std::string& scratch) {
        if (x.peek() == '(') {
            std::string_view t;
            if (!x.eat('(') || !x.word("TAG") || !x.sp() || !x.string(t, scratch) || !x.eat(')')) {
                return false;
            }
            e.tag = str(t);
            x.eat(' ');
        }
        if (x.word("UID")) {
            e.uid = true;
            x.eat(' ');
        }
        while (!x.at_end()) {
            std::string_view k = x.atom();
            if (k.empty() || !x.sp()) {
                return false;
            }
            if (iequal(k, "MIN")) {
                if (!x.number32(e.min)) {
                    return false;
                }
            } else if (iequal(k, "MAX")) {
                if (!x.number32(e.max)) {
                    return false;
                }
            } else if (iequal(k, "COUNT")) {
                if (!x.number32(e.count)) {
                    return false;
                }
                e.has_count = true;
            } else if (iequal(k, "ALL")) {
                if (!SequenceAccess::read(e.all, x)) {
                    return false;
                }
            } else if (iequal(k, "MODSEQ")) {
                if (!x.number(e.modseq)) {
                    return false;
                }
            } else if (!x.skip_value()) {
                return false;
            }
            if (!x.eat(' ')) {
                break;
            }
        }
        return x.at_end();
    }

    // VANISHED: ["(EARLIER)" SP] known-uids
    inline bool parse_vanished(Lexer& x, bool& earlier, sequence_set& uids) {
        earlier = false;
        if (x.peek() == '(') {
            if (!x.eat('(') || !x.word("EARLIER") || !x.eat(')') || !x.sp()) {
                return false;
            }
            earlier = true;
        }
        return SequenceAccess::read(uids, x) && x.at_end();
    }

    // NAMESPACE: three of nil or "(" 1*( "(" string SP (delimiter / nil)
    // [extension] ")" ) ")"
    inline bool parse_namespace_list(Lexer& x, vector<namespace_entry>& out, std::string& scratch) {
        if (x.word("NIL")) {
            return true;
        }
        if (!x.eat('(')) {
            return false;
        }
        while (!x.eat(')')) {
            if (!x.eat('(')) {
                return false;
            }
            namespace_entry e;
            std::string_view p;
            if (!x.string(p, scratch) || !x.sp()) {
                return false;
            }
            e.prefix = str(p);
            if (x.word("NIL")) {
                e.delimiter = 0;
            } else {
                std::string_view d;
                if (!x.string(d, scratch) || d.size() != 1) {
                    return false;
                }
                e.delimiter = d[0];
            }
            while (x.eat(' ')) {
                if (!x.skip_value()) {
                    return false;
                }
            }
            if (!x.eat(')')) {
                return false;
            }
            out.push_back(e);
        }
        return true;
    }

    inline bool parse_namespace(Lexer& x, namespaces& n, std::string& scratch) {
        return parse_namespace_list(x, n.personal, scratch) && x.sp() && parse_namespace_list(x, n.other_users, scratch) && x.sp()
               && parse_namespace_list(x, n.shared, scratch);
    }

    // QUOTA: root SP "(" [name SP usage SP limit *(SP ...)] ")"
    inline bool parse_quota(Lexer& x, quota& q, std::string& scratch) {
        std::string_view root;
        if (!x.astring(root, scratch) || !x.sp() || !x.eat('(')) {
            return false;
        }
        q.root = str(root);
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view k = x.atom();
            uint64_t used, limit;
            if (k.empty() || !x.sp() || !x.number(used) || !x.sp() || !x.number(limit)) {
                return false;
            }
            if (iequal(k, "STORAGE")) {
                q.storage_used = used;
                q.storage_limit = limit;
            } else if (iequal(k, "MESSAGE")) {
                q.messages_used = used;
                q.messages_limit = limit;
            }
        }
        return true;
    }

    // ID: nil or "(" string SP nstring *(SP string SP nstring) ")"
    inline bool parse_id(Lexer& x, vector<pair<string, string>>& out, std::string& scratch) {
        if (x.word("NIL")) {
            return true;
        }
        if (!x.eat('(')) {
            return false;
        }
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view k;
            if (!x.string(k, scratch)) {
                return false;
            }
            string key = str(k);
            string v;
            if (!x.sp() || !parse_nstring_into(x, v, scratch)) {
                return false;
            }
            out.push_back(pair<string, string>(key, v));
        }
        return true;
    }

    // THREAD (RFC 5256): thread-list *: "(" members [nested] ")", members
    // a chain of numbers, each the parent of the next
    inline bool parse_thread_list(Lexer& x, thread& node, int depth) {
        if (depth > 200 || !x.eat('(')) {
            return false;
        }
        // the chain as a list of numbers, then the nested threads
        std::vector<uint32_t> members;
        while (x.peek() >= '0' && x.peek() <= '9') {
            uint32_t n;
            if (!x.nz_number(n)) {
                return false;
            }
            members.push_back(n);
            x.eat(' ');
        }
        vector<thread> nested;
        while (x.peek() == '(') {
            thread t;
            if (!parse_thread_list(x, t, depth + 1)) {
                return false;
            }
            nested.push_back(t);
            x.eat(' ');
        }
        if (!x.eat(')') || (members.empty() && nested.empty())) {
            return false;
        }
        // build from the end of the chain: the last member holds the nested
        if (members.empty()) {
            node.uid = 0;
            node.children = nested;
            return true;
        }
        thread tail;
        tail.uid = members.back();
        tail.children = nested;
        for (size_t i = members.size() - 1; i-- > 0;) {
            thread parent;
            parent.uid = members[i];
            parent.children.push_back(tail);
            tail = parent;
        }
        node = tail;
        return true;
    }

    inline bool parse_threads(Lexer& x, vector<thread>& out) {
        while (x.peek() == '(') {
            thread t;
            if (!parse_thread_list(x, t, 0)) {
                return false;
            }
            out.push_back(t);
            x.eat(' ');
        }
        return x.at_end();
    }

    // COPYUID (RFC 4315): uidvalidity SP source-set SP destination-set
    inline bool parse_copyuid(std::string_view data, copy_result& r) {
        Lexer x(data);
        sequence_set from, to;
        if (!x.nz_number(r.uid_validity) || !x.sp() || !SequenceAccess::read(from, x) || !x.sp() || !SequenceAccess::read(to, x) || !x.at_end()) {
            return false;
        }
        // the sets as written, in their order (RFC 4315 §3: the pairs in
        // order); ranges expanded
        auto expand = [](const sequence_set& s, vector<uint32_t>& out) {
            for (auto [a, b] : s.ranges()) {
                if (a == 0 || b == 0) {
                    return false;
                }
                if (b - a > 1000000) {
                    return false;
                }
                if (a <= b) {
                    for (uint64_t n = a; n <= b; ++n) {
                        out.push_back(uint32_t(n));
                    }
                } else {
                    for (uint64_t n = a; n >= b; --n) {
                        out.push_back(uint32_t(n));
                    }
                }
            }
            return true;
        };
        if (!expand(from, r.source) || !expand(to, r.destination) || r.source.size() != r.destination.size()) {
            r = copy_result();
            return false;
        }
        return true;
    }

    // APPENDUID: uidvalidity SP uid-set (one UID, or several for MULTIAPPEND)
    inline bool parse_appenduid(std::string_view data, uint32_t& validity, vector<uint32_t>& uids) {
        Lexer x(data);
        sequence_set s;
        if (!x.nz_number(validity) || !x.sp() || !SequenceAccess::read(s, x) || !x.at_end()) {
            return false;
        }
        for (auto [a, b] : s.ranges()) {
            if (a == 0 || b == 0 || (a > b ? a - b : b - a) > 1000000) {
                return false;
            }
            for (uint64_t n = std::min(a, b); n <= std::max(a, b); ++n) {
                uids.push_back(uint32_t(n));
            }
        }
        return true;
    }

    // A capability list: atoms separated by spaces, upper-cased
    inline void parse_capabilities(std::string_view data, std::vector<std::string>& out) {
        out.clear();
        size_t i = 0;
        while (i < data.size()) {
            while (i < data.size() && data[i] == ' ') {
                ++i;
            }
            size_t j = i;
            while (j < data.size() && data[j] != ' ') {
                ++j;
            }
            if (j > i) {
                out.push_back(to_upper(data.substr(i, j - i)));
            }
            i = j;
        }
    }
}
