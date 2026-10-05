//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "mime.h"
#include "syntax.h"
#include "words.h"
#include "../types.h"
#include "../../../txt/case.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// A server's SEARCH (RFC 9051 §6.4.4, CONDSTORE's MODSEQ, WITHIN's OLDER
// and YOUNGER, SEARCHRES's "$"): the keys read into a tree, then judged
// against each message (its flags, numbers, size and dates at once, its
// header and body read only for a key that needs them); SORT and THREAD
// (RFC 5256) over the same messages. Strings match as substrings without
// regard to case (Unicode's full case folding), the encoded words of a
// header decoded first, a body's transfer encoding and charset undone.
namespace sgcl::net::imap::detail {
    // The system flags of a message as bits
    enum : uint8_t {
        FlagSeen = 1,
        FlagAnswered = 2,
        FlagFlagged = 4,
        FlagDeleted = 8,
        FlagDraft = 16,
    };

    inline uint8_t system_flag(std::string_view f) noexcept {
        if (iequal(f, "\\Seen")) {
            return FlagSeen;
        }
        if (iequal(f, "\\Answered")) {
            return FlagAnswered;
        }
        if (iequal(f, "\\Flagged")) {
            return FlagFlagged;
        }
        if (iequal(f, "\\Deleted")) {
            return FlagDeleted;
        }
        if (iequal(f, "\\Draft")) {
            return FlagDraft;
        }
        return 0;
    }

    inline const char* system_flag_name(uint8_t bit) noexcept {
        switch (bit) {
            case FlagSeen: return "\\Seen";
            case FlagAnswered: return "\\Answered";
            case FlagFlagged: return "\\Flagged";
            case FlagDeleted: return "\\Deleted";
            case FlagDraft: return "\\Draft";
        }
        return "";
    }

    // RFC 5322 §3.3's date-time, leniently (the day name optional, a year
    // of two digits, a zone of letters): the instant; false for none
    inline bool parse_mail_date(std::string_view s, DateTime& out) noexcept {
        size_t i = 0;
        auto blank = [&] {
            for (;;) {
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n' || s[i] == ',')) {
                    ++i;
                }
                if (i < s.size() && s[i] == '(') {
                    skip_cfws(s, i);
                    continue;
                }
                return;
            }
        };
        auto word = [&] {
            blank();
            const size_t a = i;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != ',' && s[i] != '(' && s[i] != '\r' && s[i] != '\n') {
                ++i;
            }
            return s.substr(a, i - a);
        };
        auto number = [](std::string_view w, int& v) {
            if (w.empty() || w.size() > 4) {
                return false;
            }
            v = 0;
            for (char c : w) {
                if (c < '0' || c > '9') {
                    return false;
                }
                v = v * 10 + (c - '0');
            }
            return true;
        };
        std::string_view w = word();
        int day;
        if (!number(w, day)) {
            w = word();   // the day of the week
            if (!number(w, day)) {
                return false;
            }
        }
        const int month = month_of(word());
        int year;
        std::string_view yw = word();
        if (!month || !number(yw, year)) {
            return false;
        }
        if (yw.size() <= 2) {
            year += year < 50 ? 2000 : 1900;
        } else if (yw.size() == 3) {
            year += 1900;
        }
        int hh = 0, mm = 0, ss = 0;
        std::string_view t = word();
        if (!t.empty()) {
            int parts[3] = {0, 0, 0};
            int k = 0;
            size_t j = 0;
            while (j <= t.size() && k < 3) {
                size_t e = t.find(':', j);
                if (e == std::string_view::npos) {
                    e = t.size();
                }
                if (!number(t.substr(j, e - j), parts[k])) {
                    return false;
                }
                ++k;
                j = e + 1;
            }
            if (k < 2) {
                return false;
            }
            hh = parts[0];
            mm = parts[1];
            ss = parts[2];
        }
        int offset = 0;
        std::string_view z = word();
        if (z.size() == 5 && (z[0] == '+' || z[0] == '-')) {
            int v;
            if (number(z.substr(1), v)) {
                offset = (v / 100 * 60 + v % 100) * (z[0] == '-' ? -1 : 1);
            }
        } else if (iequal(z, "EST") || iequal(z, "CDT")) {
            offset = -300;
        } else if (iequal(z, "EDT")) {
            offset = -240;
        } else if (iequal(z, "CST") || iequal(z, "MDT")) {
            offset = iequal(z, "CST") ? -360 : -360;
        } else if (iequal(z, "MST") || iequal(z, "PDT")) {
            offset = -420;
        } else if (iequal(z, "PST")) {
            offset = -480;
        }
        if (day < 1 || day > 31 || hh > 23 || mm > 59 || ss > 60) {
            return false;
        }
        out.offset = offset;
        out.unix = days_from_civil(year, unsigned(month), unsigned(day)) * 86400 + hh * 3600 + mm * 60 + (ss > 59 ? 59 : ss) - int64_t(offset) * 60;
        return true;
    }

    // The days since 1970 of an instant in its own zone (SEARCH compares
    // dates "disregarding time and timezone")
    SGCL_INLINE_HOT int64_t local_days(DateTime t) noexcept {
        int64_t l = t.unix + int64_t(t.offset) * 60;
        return l >= 0 ? l / 86400 : (l - 86399) / 86400;
    }

    // --- matching ----------------------------------------------------------

    inline bool is_ascii(std::string_view s) noexcept {
        for (unsigned char c : s) {
            if (c >= 0x80) {
                return false;
            }
        }
        return true;
    }

    // A pattern folded once; texts matched against it
    struct Needle {
        std::string folded;
        bool ascii = true;

        explicit Needle(std::string_view s) {
            ascii = is_ascii(s);
            if (ascii) {
                folded = to_lower(s);
            } else {
                string f = txt::fold_case(string(s));
                folded = std::string(f.view());
            }
        }

        bool in(std::string_view text) const {
            if (folded.empty()) {
                return true;
            }
            if (is_ascii(text)) {
                if (!ascii) {
                    return false;
                }
                const size_t n = folded.size();
                if (text.size() < n) {
                    return false;
                }
                const char first = folded[0];
                for (size_t i = 0; i + n <= text.size(); ++i) {
                    if (lower(text[i]) != first) {
                        continue;
                    }
                    size_t k = 1;
                    while (k < n && lower(text[i + k]) == folded[k]) {
                        ++k;
                    }
                    if (k == n) {
                        return true;
                    }
                }
                return false;
            }
            std::string_view valid = text;
            std::string fixed;
            if (!valid_utf8(text)) {
                fixed = to_utf8(text, "utf-8");
                valid = fixed;
            }
            string f = txt::fold_case(string(valid));
            return f.view().find(folded) != std::string_view::npos;
        }
    };

    // --- the keys --------------------------------------------------------------

    struct SearchKey {
        enum class Op : uint8_t {
            all, and_, or_, not_, flags_set, flags_unset, keyword, unkeyword, numbers, uids, saved,
            larger, smaller, before, on, since, sent_before, sent_on, sent_since, older, younger,
            header, body, text, modseq, recent, new_, old, none
        };

        Op op = Op::all;
        uint8_t flags = 0;
        std::string arg;            // keyword, header name
        std::string value;          // the string looked for
        uint64_t number = 0;        // size, modseq, seconds, days
        std::vector<std::pair<uint32_t, uint32_t>> set;   // a sequence set's ranges, 0 for "*" (plain memory: a key lives in unique_ptrs)

        bool in_set(uint32_t n, uint32_t largest) const noexcept {
            for (auto [a, b] : set) {
                uint32_t lo = a ? a : largest, hi = b ? b : largest;
                if (lo > hi) {
                    std::swap(lo, hi);
                }
                if (n >= lo && n <= hi) {
                    return true;
                }
            }
            return false;
        }

        bool read_set(Lexer& x) {
            sequence_set s;
            if (!SequenceAccess::read(s, x)) {
                return false;
            }
            for (auto [a, b] : s.ranges()) {
                set.emplace_back(a, b);
            }
            return true;
        }
        std::vector<std::unique_ptr<SearchKey>> children;
        std::unique_ptr<Needle> needle;
    };

    struct SearchParse {
        bool uses_modseq = false;
        bool uses_saved = false;
        bool needs_content = false;
        std::string charset;        // to convert the strings from; empty: UTF-8
    };

    inline bool parse_search_key(Lexer& x, SearchKey& k, SearchParse& sp, std::string& scratch, int depth);

    // The strings of a search in its charset, as UTF-8
    inline std::string search_string(std::string_view s, const SearchParse& sp) {
        if (sp.charset.empty()) {
            return std::string(s);
        }
        return to_utf8(s, sp.charset);
    }

    inline bool parse_search_keys(Lexer& x, SearchKey& k, SearchParse& sp, std::string& scratch, int depth) {
        // keys separated by spaces, to the end or a ")"
        k.op = SearchKey::Op::and_;
        for (;;) {
            auto child = std::make_unique<SearchKey>();
            if (!parse_search_key(x, *child, sp, scratch, depth + 1)) {
                return false;
            }
            k.children.push_back(std::move(child));
            if (k.children.size() > 10000) {
                return false;
            }
            if (x.at_end() || x.peek() == ')') {
                break;
            }
            if (!x.sp()) {
                return false;
            }
        }
        return true;
    }

    inline bool parse_search_key(Lexer& x, SearchKey& k, SearchParse& sp, std::string& scratch, int depth) {
        if (depth > 100) {
            return false;
        }
        using Op = SearchKey::Op;
        auto string_arg = [&](std::string& out) {
            std::string_view v;
            if (!x.sp() || !x.astring(v, scratch)) {
                return false;
            }
            out = search_string(v, sp);
            return true;
        };
        auto date_arg = [&](uint64_t& out) {
            std::string_view v;
            if (!x.sp()) {
                return false;
            }
            if (x.peek() == '"') {
                if (!x.string(v, scratch)) {
                    return false;
                }
            } else {
                v = x.atom();
            }
            int64_t days;
            if (!parse_date(v, days)) {
                return false;
            }
            out = uint64_t(days);
            return true;
        };
        auto text_key = [&](Op op, const char* header) {
            k.op = op;
            if (header) {
                k.arg = header;
            }
            if (!string_arg(k.value)) {
                return false;
            }
            k.needle = std::make_unique<Needle>(k.value);
            sp.needs_content = true;
            return true;
        };
        const char c = x.peek();
        if (c == '(') {
            x.eat('(');
            if (!parse_search_keys(x, k, sp, scratch, depth)) {
                return false;
            }
            return x.eat(')');
        }
        if ((c >= '0' && c <= '9') || c == '*' || c == '$') {
            if (x.eat('$')) {
                k.op = Op::saved;
                sp.uses_saved = true;
                return true;
            }
            k.op = Op::numbers;
            return k.read_set(x);
        }
        std::string_view w = x.atom();
        if (w.empty()) {
            return false;
        }
        std::string u = to_upper(w);
        if (u == "ALL") {
            k.op = Op::all;
        } else if (u == "ANSWERED" || u == "DELETED" || u == "DRAFT" || u == "FLAGGED" || u == "SEEN") {
            k.op = Op::flags_set;
            k.flags = system_flag("\\" + u);
        } else if (u == "UNANSWERED" || u == "UNDELETED" || u == "UNDRAFT" || u == "UNFLAGGED" || u == "UNSEEN") {
            k.op = Op::flags_unset;
            k.flags = system_flag("\\" + u.substr(2));
        } else if (u == "KEYWORD" || u == "UNKEYWORD") {
            std::string_view f;
            if (!x.sp() || !x.flag(f)) {
                return false;
            }
            const uint8_t sys = system_flag(f);
            if (sys) {
                k.op = u == "KEYWORD" ? Op::flags_set : Op::flags_unset;
                k.flags = sys;
            } else {
                k.op = u == "KEYWORD" ? Op::keyword : Op::unkeyword;
                k.arg = std::string(f);
            }
        } else if (u == "BCC" || u == "CC" || u == "FROM" || u == "TO" || u == "SUBJECT") {
            return text_key(Op::header, u == "BCC" ? "bcc" : u == "CC" ? "cc" : u == "FROM" ? "from" : u == "TO" ? "to" : "subject");
        } else if (u == "HEADER") {
            std::string_view f;
            if (!x.sp() || !x.astring(f, scratch)) {
                return false;
            }
            k.arg = to_lower(f);
            return text_key(Op::header, nullptr);
        } else if (u == "BODY") {
            return text_key(Op::body, nullptr);
        } else if (u == "TEXT") {
            return text_key(Op::text, nullptr);
        } else if (u == "BEFORE" || u == "ON" || u == "SINCE") {
            k.op = u == "BEFORE" ? Op::before : u == "ON" ? Op::on : Op::since;
            return date_arg(k.number);
        } else if (u == "SENTBEFORE" || u == "SENTON" || u == "SENTSINCE") {
            k.op = u == "SENTBEFORE" ? Op::sent_before : u == "SENTON" ? Op::sent_on : Op::sent_since;
            sp.needs_content = true;
            return date_arg(k.number);
        } else if (u == "LARGER" || u == "SMALLER") {
            k.op = u == "LARGER" ? Op::larger : Op::smaller;
            return x.sp() && x.number(k.number);
        } else if (u == "OLDER" || u == "YOUNGER") {
            k.op = u == "OLDER" ? Op::older : Op::younger;
            return x.sp() && x.number(k.number) && k.number > 0;
        } else if (u == "UID") {
            k.op = Op::uids;
            if (!x.sp()) {
                return false;
            }
            if (x.eat('$')) {
                k.op = Op::saved;
                k.flags = 1;   // the saved set as UIDs
                sp.uses_saved = true;
                return true;
            }
            return k.read_set(x);
        } else if (u == "NOT") {
            k.op = Op::not_;
            auto child = std::make_unique<SearchKey>();
            if (!x.sp() || !parse_search_key(x, *child, sp, scratch, depth + 1)) {
                return false;
            }
            k.children.push_back(std::move(child));
        } else if (u == "OR") {
            k.op = Op::or_;
            for (int i = 0; i < 2; ++i) {
                auto child = std::make_unique<SearchKey>();
                if (!x.sp() || !parse_search_key(x, *child, sp, scratch, depth + 1)) {
                    return false;
                }
                k.children.push_back(std::move(child));
            }
        } else if (u == "MODSEQ") {
            k.op = Op::modseq;
            sp.uses_modseq = true;
            if (!x.sp()) {
                return false;
            }
            if (x.peek() == '"') {
                // entry-name entry-type-req: "/flags/\\Seen" all|shared|priv
                std::string_view name;
                if (!x.string(name, scratch) || !x.sp() || x.atom().empty() || !x.sp()) {
                    return false;
                }
            }
            return x.number(k.number);
        } else if (u == "NEW") {
            k.op = Op::new_;
        } else if (u == "OLD") {
            k.op = Op::old;
        } else if (u == "RECENT") {
            k.op = Op::recent;
        } else {
            return false;
        }
        return true;
    }

    // What a search program is judged against, for one message
    struct SearchMessage {
        uint32_t seq = 0;
        uint32_t uid = 0;
        uint8_t flags = 0;
        const std::vector<std::string>* keywords = nullptr;
        uint64_t size = 0;
        DateTime internal_date;
        uint64_t modseq = 0;
        bool recent = false;
        uint32_t largest_seq = 0;
        uint32_t largest_uid = 0;
        int64_t now = 0;
        const std::vector<uint32_t>* saved = nullptr;   // the UIDs SAVE kept
        // the content, read the first time a key needs it
        std::function<const std::string*()> content;
    };

    // A message's text for searching: the header fields' values decoded,
    // the text parts decoded into UTF-8
    struct SearchText {
        std::unique_ptr<Structure> structure;
        std::string_view bytes;
        bool body_done = false;
        std::string body;

        void prepare(const std::string& s) {
            if (!structure) {
                bytes = s;
                structure = std::make_unique<Structure>(bytes);
            }
        }

        void collect(const Part& p, int depth) {
            if (depth > 40) {
                return;
            }
            if (p.multipart()) {
                for (const auto& c : p.children) {
                    collect(*c, depth + 1);
                }
                return;
            }
            if (p.embedded_message()) {
                const Part& inner = *p.children[0];
                for (const auto& f : inner.fields) {
                    body += decode_words(unfold(f.value));
                    body += '\n';
                }
                collect(inner, depth + 1);
                return;
            }
            if (p.type != "text" && !(p.type == "message")) {
                return;
            }
            std::string raw;
            decoded_content(bytes.substr(p.body_begin, p.body_end - p.body_begin), p.encoding, raw);
            const std::string* cs = p.content_type.find("charset");
            body += to_utf8(raw, cs ? std::string_view(*cs) : std::string_view());
            body += '\n';
        }

        const std::string& body_text() {
            if (!body_done) {
                body_done = true;
                collect(structure->root, 0);
            }
            return body;
        }
    };

    inline bool match_header(SearchText& t, const SearchKey& k) {
        const Part& root = t.structure->root;
        bool any = false;
        for (const auto& f : root.fields) {
            if (!iequal(f.name, k.arg)) {
                continue;
            }
            any = true;
            if (k.value.empty()) {
                return true;
            }
            if (k.needle->in(decode_words(unfold(f.value)))) {
                return true;
            }
        }
        (void)any;
        return false;
    }

    inline bool judge(const SearchKey& k, SearchMessage& m, std::unique_ptr<SearchText>& text) {
        using Op = SearchKey::Op;
        auto content = [&]() -> SearchText* {
            if (!text) {
                const std::string* s = m.content ? m.content() : nullptr;
                if (!s) {
                    return nullptr;
                }
                text = std::make_unique<SearchText>();
                text->prepare(*s);
            }
            return text.get();
        };
        switch (k.op) {
            case Op::all: return true;
            case Op::none: return false;
            case Op::and_:
                for (const auto& c : k.children) {
                    if (!judge(*c, m, text)) {
                        return false;
                    }
                }
                return true;
            case Op::or_: return judge(*k.children[0], m, text) || judge(*k.children[1], m, text);
            case Op::not_: return !judge(*k.children[0], m, text);
            case Op::flags_set: return (m.flags & k.flags) == k.flags;
            case Op::flags_unset: return (m.flags & k.flags) == 0;
            case Op::keyword:
            case Op::unkeyword: {
                bool has = false;
                if (m.keywords) {
                    for (const auto& kw : *m.keywords) {
                        has |= iequal(kw, k.arg);
                    }
                }
                return k.op == Op::keyword ? has : !has;
            }
            case Op::numbers: return k.in_set(m.seq, m.largest_seq);
            case Op::uids: return k.in_set(m.uid, m.largest_uid);
            case Op::saved:
                if (!m.saved) {
                    return false;
                }
                return std::binary_search(m.saved->begin(), m.saved->end(), m.uid);
            case Op::larger: return m.size > k.number;
            case Op::smaller: return m.size < k.number;
            case Op::before: return local_days(m.internal_date) < int64_t(k.number);
            case Op::on: return local_days(m.internal_date) == int64_t(k.number);
            case Op::since: return local_days(m.internal_date) >= int64_t(k.number);
            case Op::older: return m.now - m.internal_date.unix >= int64_t(k.number);
            case Op::younger: return m.now - m.internal_date.unix <= int64_t(k.number);
            case Op::modseq: return m.modseq >= k.number;
            case Op::recent: return m.recent;
            case Op::new_: return m.recent && !(m.flags & FlagSeen);
            case Op::old: return !m.recent;
            case Op::sent_before:
            case Op::sent_on:
            case Op::sent_since: {
                SearchText* t = content();
                if (!t) {
                    return false;
                }
                const Field* f = t->structure->root.field("date");
                DateTime d;
                if (!f || !parse_mail_date(unfold(f->value), d)) {
                    return false;
                }
                const int64_t days = local_days(d);
                return k.op == Op::sent_before ? days < int64_t(k.number) : k.op == Op::sent_on ? days == int64_t(k.number) : days >= int64_t(k.number);
            }
            case Op::header: {
                SearchText* t = content();
                return t && match_header(*t, k);
            }
            case Op::body: {
                SearchText* t = content();
                return t && k.needle->in(t->body_text());
            }
            case Op::text: {
                SearchText* t = content();
                if (!t) {
                    return false;
                }
                for (const auto& f : t->structure->root.fields) {
                    if (k.needle->in(decode_words(unfold(f.value))) || k.needle->in(f.name)) {
                        return true;
                    }
                }
                return k.needle->in(t->body_text());
            }
        }
        return false;
    }

    // --- SORT and THREAD (RFC 5256) ---------------------------------------------

    // The base subject (RFC 5256 §2.1): encoded words decoded, blanks
    // collapsed, the "(fwd)" trailers, the "Re:"/"Fw:"/"Fwd:" leaders with
    // their "[blob]"s and the "[Fwd: ...]" wrappers taken off; whether a
    // leader or trailer of a reply or a forward was there
    inline std::string base_subject(std::string_view raw, bool* was_reply = nullptr) {
        std::string s = decode_words(unfold(raw));
        // collapse blanks
        std::string c;
        bool blank = false;
        for (char ch : s) {
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                blank = true;
                continue;
            }
            if (blank && !c.empty()) {
                c += ' ';
            }
            blank = false;
            c += ch;
        }
        bool reply = false;
        for (int guard = 0; guard < 100; ++guard) {
            bool changed = false;
            // trailers: "(fwd)" and blanks
            for (;;) {
                while (!c.empty() && c.back() == ' ') {
                    c.pop_back();
                }
                if (c.size() >= 5 && iequal(std::string_view(c).substr(c.size() - 5), "(fwd)")) {
                    c.resize(c.size() - 5);
                    reply = true;
                    continue;
                }
                break;
            }
            // leaders: blob* ("re" / "fw" / "fwd") blob? ":" and blobs
            for (;;) {
                bool step = false;
                size_t i = 0;
                while (i < c.size() && c[i] == ' ') {
                    ++i;
                }
                // subj-blob: "[" *BLOBCHAR "]" *WSP
                size_t j = i;
                auto skip_blob = [&](size_t& p) {
                    if (p < c.size() && c[p] == '[') {
                        const size_t e = c.find(']', p);
                        if (e != std::string::npos && c.find('[', p + 1) > e) {
                            p = e + 1;
                            while (p < c.size() && c[p] == ' ') {
                                ++p;
                            }
                            return true;
                        }
                    }
                    return false;
                };
                size_t k = j;
                while (skip_blob(k)) {
                }
                std::string_view rest = std::string_view(c).substr(k);
                size_t lead = 0;
                if (istarts(rest, "fwd")) {
                    lead = 3;
                } else if (istarts(rest, "fw") || istarts(rest, "re")) {
                    lead = 2;
                }
                if (lead) {
                    size_t p = k + lead;
                    while (p < c.size() && c[p] == ' ') {
                        ++p;
                    }
                    skip_blob(p);
                    if (p < c.size() && c[p] == ':') {
                        c.erase(0, p + 1);
                        reply = true;
                        step = true;
                    }
                }
                if (!step && k > i) {
                    // a leading blob, when what is left is not empty
                    std::string tail = c.substr(k);
                    bool empty = true;
                    for (char ch : tail) {
                        empty &= ch == ' ';
                    }
                    if (!empty) {
                        c = tail;
                        step = true;
                    }
                }
                if (!step) {
                    break;
                }
                changed = true;
            }
            while (!c.empty() && c.front() == ' ') {
                c.erase(0, 1);
            }
            // "[fwd:" subject "]"
            if (c.size() >= 6 && istarts(c, "[fwd:") && c.back() == ']') {
                c = c.substr(5, c.size() - 6);
                reply = true;
                changed = true;
            }
            if (!changed) {
                break;
            }
        }
        while (!c.empty() && c.front() == ' ') {
            c.erase(0, 1);
        }
        if (was_reply) {
            *was_reply = reply;
        }
        return c;
    }

    // What SORT and THREAD read of a message, once
    struct SortInfo {
        DateTime sent;              // the Date: field, else the internal date
        std::string subject;        // the base subject, folded
        bool reply = false;
        std::string from, to, cc;   // the first address's mailbox, folded
        std::string display_from, display_to;
        std::string message_id;
        std::vector<std::string> references;
    };

    inline std::string folded(std::string_view s) {
        if (is_ascii(s)) {
            return to_lower(s);
        }
        std::string v = valid_utf8(s) ? std::string(s) : to_utf8(s, "utf-8");
        string f = txt::fold_case(string(v));
        return std::string(f.view());
    }

    // The message ids of a field: "<...>" each
    inline void message_ids(std::string_view v, std::vector<std::string>& out) {
        size_t i = 0;
        while (i < v.size()) {
            const size_t a = v.find('<', i);
            if (a == std::string_view::npos) {
                break;
            }
            const size_t b = v.find('>', a);
            if (b == std::string_view::npos) {
                break;
            }
            std::string id;
            for (size_t k = a + 1; k < b; ++k) {
                if (v[k] != ' ' && v[k] != '\t' && v[k] != '\r' && v[k] != '\n') {
                    id += v[k];
                }
            }
            if (!id.empty()) {
                out.push_back(id);
            }
            i = b + 1;
            if (out.size() > 1000) {
                break;
            }
        }
    }

    inline SortInfo sort_info(std::string_view bytes, DateTime internal) {
        SortInfo s;
        Structure st(bytes);
        const Part& r = st.root;
        DateTime d;
        if (const Field* f = r.field("date"); f && parse_mail_date(unfold(f->value), d)) {
            s.sent = d;
        } else {
            s.sent = internal;
        }
        if (const Field* f = r.field("subject")) {
            s.subject = folded(base_subject(f->value, &s.reply));
        }
        auto first = [&](std::string_view name, std::string& mailbox, std::string* display) {
            if (const Field* f = r.field(name)) {
                std::vector<Mailbox> list;
                parse_address_field(unfold(f->value), list);
                for (const auto& m : list) {
                    if (m.group) {
                        continue;
                    }
                    mailbox = folded(m.local);
                    if (display) {
                        const std::string name_text = decode_words(m.name);
                        *display = folded(name_text.empty() ? m.local + "@" + m.host : name_text);
                    }
                    break;
                }
            }
        };
        first("from", s.from, &s.display_from);
        first("to", s.to, &s.display_to);
        first("cc", s.cc, nullptr);
        if (const Field* f = r.field("message-id")) {
            std::vector<std::string> ids;
            message_ids(f->value, ids);
            if (!ids.empty()) {
                s.message_id = ids[0];
            }
        }
        if (const Field* f = r.field("references")) {
            message_ids(f->value, s.references);
        }
        if (s.references.empty()) {
            if (const Field* f = r.field("in-reply-to")) {
                std::vector<std::string> ids;
                message_ids(f->value, ids);
                if (!ids.empty()) {
                    s.references.push_back(ids[0]);
                }
            }
        }
        return s;
    }

    // A SORT criterion
    struct SortKey {
        enum class Kind : uint8_t { arrival, cc, date, from, size, subject, to, display_from, display_to };
        Kind kind;
        bool reverse;
    };

    inline bool parse_sort_keys(Lexer& x, std::vector<SortKey>& out) {
        if (!x.eat('(')) {
            return false;
        }
        bool reverse = false;
        bool first = true;
        while (!x.eat(')')) {
            if (!first && !x.sp()) {
                return false;
            }
            first = false;
            std::string_view w = x.atom();
            if (iequal(w, "REVERSE")) {
                if (reverse) {
                    return false;
                }
                reverse = true;
                first = true;
                if (!x.sp()) {
                    return false;
                }
                continue;
            }
            SortKey k{};
            k.reverse = reverse;
            reverse = false;
            if (iequal(w, "ARRIVAL")) {
                k.kind = SortKey::Kind::arrival;
            } else if (iequal(w, "CC")) {
                k.kind = SortKey::Kind::cc;
            } else if (iequal(w, "DATE")) {
                k.kind = SortKey::Kind::date;
            } else if (iequal(w, "FROM")) {
                k.kind = SortKey::Kind::from;
            } else if (iequal(w, "SIZE")) {
                k.kind = SortKey::Kind::size;
            } else if (iequal(w, "SUBJECT")) {
                k.kind = SortKey::Kind::subject;
            } else if (iequal(w, "TO")) {
                k.kind = SortKey::Kind::to;
            } else if (iequal(w, "DISPLAYFROM")) {
                k.kind = SortKey::Kind::display_from;
            } else if (iequal(w, "DISPLAYTO")) {
                k.kind = SortKey::Kind::display_to;
            } else {
                return false;
            }
            out.push_back(k);
        }
        return !out.empty() && !reverse;
    }

    // One message as SORT and THREAD see it
    struct SortItem {
        uint32_t seq;
        uint32_t uid;
        uint64_t size;
        DateTime internal;
        const SortInfo* info;
    };

    inline void sort_items(std::vector<SortItem>& items, const std::vector<SortKey>& keys) {
        std::stable_sort(items.begin(), items.end(), [&](const SortItem& a, const SortItem& b) {
            for (const auto& k : keys) {
                int c = 0;
                switch (k.kind) {
                    case SortKey::Kind::arrival: c = a.internal.unix < b.internal.unix ? -1 : a.internal.unix > b.internal.unix ? 1 : 0; break;
                    case SortKey::Kind::date: c = a.info->sent.unix < b.info->sent.unix ? -1 : a.info->sent.unix > b.info->sent.unix ? 1 : 0; break;
                    case SortKey::Kind::size: c = a.size < b.size ? -1 : a.size > b.size ? 1 : 0; break;
                    case SortKey::Kind::cc: c = a.info->cc.compare(b.info->cc); break;
                    case SortKey::Kind::from: c = a.info->from.compare(b.info->from); break;
                    case SortKey::Kind::to: c = a.info->to.compare(b.info->to); break;
                    case SortKey::Kind::subject: c = a.info->subject.compare(b.info->subject); break;
                    case SortKey::Kind::display_from: c = a.info->display_from.compare(b.info->display_from); break;
                    case SortKey::Kind::display_to: c = a.info->display_to.compare(b.info->display_to); break;
                }
                if (c) {
                    return k.reverse ? c > 0 : c < 0;
                }
            }
            return a.seq < b.seq;
        });
    }

    // A node of a thread as the server builds it: a message (its index in
    // the items) or a dummy (-1), and its children
    struct ThreadNode {
        int item = -1;
        std::vector<std::unique_ptr<ThreadNode>> children;
    };

    inline const DateTime& thread_date(const ThreadNode& n, const std::vector<SortItem>& items) {
        // a dummy's date is its first child's (RFC 5256 §3, (B))
        const ThreadNode* p = &n;
        int guard = 0;
        while (p->item < 0 && !p->children.empty() && ++guard < 10000) {
            p = p->children[0].get();
        }
        static const DateTime none{};
        return p->item < 0 ? none : items[size_t(p->item)].info->sent;
    }

    inline void sort_thread_children(ThreadNode& n, const std::vector<SortItem>& items, int depth = 0) {
        if (depth > 5000) {
            return;
        }
        for (auto& c : n.children) {
            sort_thread_children(*c, items, depth + 1);
        }
        std::stable_sort(n.children.begin(), n.children.end(), [&](const std::unique_ptr<ThreadNode>& a, const std::unique_ptr<ThreadNode>& b) {
            const auto& da = thread_date(*a, items);
            const auto& db = thread_date(*b, items);
            if (da.unix != db.unix) {
                return da.unix < db.unix;
            }
            const int ia = a->item >= 0 ? int(items[size_t(a->item)].seq) : 0;
            const int ib = b->item >= 0 ? int(items[size_t(b->item)].seq) : 0;
            return ia < ib;
        });
    }

    // THREAD=ORDEREDSUBJECT (RFC 5256 §3)
    inline std::vector<std::unique_ptr<ThreadNode>> thread_ordered_subject(std::vector<SortItem>& items) {
        std::vector<size_t> order(items.size());
        for (size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            int c = items[a].info->subject.compare(items[b].info->subject);
            if (c) {
                return c < 0;
            }
            if (items[a].info->sent.unix != items[b].info->sent.unix) {
                return items[a].info->sent.unix < items[b].info->sent.unix;
            }
            return items[a].seq < items[b].seq;
        });
        std::vector<std::unique_ptr<ThreadNode>> roots;
        for (size_t i = 0; i < order.size();) {
            auto root = std::make_unique<ThreadNode>();
            root->item = int(order[i]);
            size_t j = i + 1;
            while (j < order.size() && items[order[j]].info->subject == items[order[i]].info->subject) {
                auto child = std::make_unique<ThreadNode>();
                child->item = int(order[j]);
                root->children.push_back(std::move(child));
                ++j;
            }
            roots.push_back(std::move(root));
            i = j;
        }
        std::stable_sort(roots.begin(), roots.end(), [&](const std::unique_ptr<ThreadNode>& a, const std::unique_ptr<ThreadNode>& b) {
            const auto& da = items[size_t(a->item)].info->sent;
            const auto& db = items[size_t(b->item)].info->sent;
            if (da.unix != db.unix) {
                return da.unix < db.unix;
            }
            return items[size_t(a->item)].seq < items[size_t(b->item)].seq;
        });
        return roots;
    }

    // THREAD=REFERENCES (RFC 5256 §3, after Jamie Zawinski's algorithm)
    inline std::vector<std::unique_ptr<ThreadNode>> thread_references(std::vector<SortItem>& items) {
        struct Container {
            int item = -1;
            Container* parent = nullptr;
            std::vector<Container*> children;
        };
        std::vector<std::unique_ptr<Container>> pool;
        std::map<std::string, Container*> by_id;
        auto get = [&](const std::string& id) {
            auto it = by_id.find(id);
            if (it != by_id.end()) {
                return it->second;
            }
            pool.push_back(std::make_unique<Container>());
            Container* c = pool.back().get();
            by_id.emplace(id, c);
            return c;
        };
        auto reachable = [](Container* from, Container* to) {
            // whether `to` is `from` or under it
            std::vector<Container*> stack{from};
            size_t guard = 0;
            while (!stack.empty() && ++guard < 1000000) {
                Container* c = stack.back();
                stack.pop_back();
                if (c == to) {
                    return true;
                }
                for (auto* k : c->children) {
                    stack.push_back(k);
                }
            }
            return false;
        };
        auto unlink = [](Container* c) {
            if (c->parent) {
                auto& sib = c->parent->children;
                sib.erase(std::remove(sib.begin(), sib.end(), c), sib.end());
                c->parent = nullptr;
            }
        };
        auto link = [&](Container* parent, Container* child) {
            if (child->parent == parent || reachable(child, parent)) {
                return;
            }
            unlink(child);
            child->parent = parent;
            parent->children.push_back(child);
        };
        size_t unique = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const SortInfo& info = *items[i].info;
            std::string id = info.message_id;
            Container* c = id.empty() ? nullptr : get(id);
            if (!c || c->item >= 0) {
                // no id, or one already taken: a unique one
                c = get("\x01unique-" + std::to_string(unique++));
            }
            c->item = int(i);
            Container* prev = nullptr;
            for (const auto& ref : info.references) {
                Container* r = get(ref);
                if (prev && !r->parent) {
                    link(prev, r);
                }
                prev = r;
            }
            if (prev && prev != c) {
                if (c->parent) {
                    unlink(c);
                }
                link(prev, c);
            } else if (c->parent) {
                unlink(c);
            }
        }
        // the root set
        std::vector<Container*> roots;
        for (auto& c : pool) {
            if (!c->parent) {
                roots.push_back(c.get());
            }
        }
        // prune dummies (RFC 5256 §3 (C))
        std::function<void(Container*, int)> prune = [&](Container* c, int depth) {
            if (depth > 5000) {
                return;
            }
            std::vector<Container*> kids = c->children;
            for (auto* k : kids) {
                prune(k, depth + 1);
            }
            std::vector<Container*> next;
            for (auto* k : c->children) {
                if (k->item < 0 && k->children.empty()) {
                    continue;   // an empty dummy: gone
                }
                if (k->item < 0) {
                    // a dummy with children: they take its place
                    for (auto* g : k->children) {
                        g->parent = c;
                        next.push_back(g);
                    }
                    k->children.clear();
                    continue;
                }
                next.push_back(k);
            }
            c->children = next;
        };
        std::vector<Container*> pruned;
        for (auto* r : roots) {
            prune(r, 0);
            if (r->item < 0) {
                if (r->children.empty()) {
                    continue;
                }
                if (r->children.size() == 1) {
                    r->children[0]->parent = nullptr;
                    pruned.push_back(r->children[0]);
                    continue;
                }
            }
            pruned.push_back(r);
        }
        // to ThreadNodes
        std::function<std::unique_ptr<ThreadNode>(Container*, int)> build = [&](Container* c, int depth) {
            auto n = std::make_unique<ThreadNode>();
            n->item = c->item;
            if (depth < 5000) {
                for (auto* k : c->children) {
                    n->children.push_back(build(k, depth + 1));
                }
            }
            return n;
        };
        std::vector<std::unique_ptr<ThreadNode>> out;
        for (auto* r : pruned) {
            out.push_back(build(r, 0));
        }
        // sort the root set by date (D), then group by base subject (E)
        auto subject_of = [&](const ThreadNode& n) -> const SortItem* {
            if (n.item >= 0) {
                return &items[size_t(n.item)];
            }
            return n.children.empty() || n.children[0]->item < 0 ? nullptr : &items[size_t(n.children[0]->item)];
        };
        std::stable_sort(out.begin(), out.end(), [&](const std::unique_ptr<ThreadNode>& a, const std::unique_ptr<ThreadNode>& b) {
            return thread_date(*a, items).unix < thread_date(*b, items).unix;
        });
        std::map<std::string, size_t> by_subject;
        for (size_t i = 0; i < out.size(); ++i) {
            const SortItem* s = subject_of(*out[i]);
            if (!s || s->info->subject.empty()) {
                continue;
            }
            auto it = by_subject.find(s->info->subject);
            if (it == by_subject.end()) {
                by_subject.emplace(s->info->subject, i);
                continue;
            }
            // prefer a dummy, or one that is not a reply, as the holder
            const SortItem* old = subject_of(*out[it->second]);
            if ((out[i]->item < 0 && out[it->second]->item >= 0) || (old && old->info->reply && !s->info->reply)) {
                it->second = i;
            }
        }
        std::vector<std::unique_ptr<ThreadNode>> merged;
        std::vector<bool> taken(out.size(), false);
        std::vector<long> position(out.size(), -1);
        for (size_t i = 0; i < out.size(); ++i) {
            const SortItem* s = subject_of(*out[i]);
            if (s && !s->info->subject.empty()) {
                auto it = by_subject.find(s->info->subject);
                size_t holder = it->second;
                if (holder != i) {
                    if (position[holder] < 0) {
                        // the holder not yet placed: place it now, here
                        position[holder] = long(merged.size());
                        merged.push_back(std::move(out[holder]));
                        taken[holder] = true;
                    }
                    ThreadNode& h = *merged[size_t(position[holder])];
                    ThreadNode* t = out[i].get();
                    if (h.item < 0 && t->item < 0) {
                        for (auto& k : t->children) {
                            h.children.push_back(std::move(k));
                        }
                    } else if (h.item < 0) {
                        h.children.push_back(std::move(out[i]));
                    } else if (t->item >= 0 && items[size_t(t->item)].info->reply && !items[size_t(h.item)].info->reply) {
                        h.children.push_back(std::move(out[i]));
                    } else {
                        // a new dummy holding both
                        auto d = std::make_unique<ThreadNode>();
                        d->children.push_back(std::move(merged[size_t(position[holder])]));
                        d->children.push_back(std::move(out[i]));
                        merged[size_t(position[holder])] = std::move(d);
                    }
                    taken[i] = true;
                    continue;
                }
            }
            if (!taken[i]) {
                position[i] = long(merged.size());
                merged.push_back(std::move(out[i]));
                taken[i] = true;
            }
        }
        for (auto& n : merged) {
            sort_thread_children(*n, items);
        }
        std::stable_sort(merged.begin(), merged.end(), [&](const std::unique_ptr<ThreadNode>& a, const std::unique_ptr<ThreadNode>& b) {
            return thread_date(*a, items).unix < thread_date(*b, items).unix;
        });
        return merged;
    }

    // A thread as THREAD writes it: "(3 6 (4 23)(44 7 96))"
    inline void put_thread(std::string& out, const ThreadNode& n, const std::vector<SortItem>& items, bool uid, int depth = 0) {
        out += '(';
        const ThreadNode* cur = &n;
        bool wrote = false;
        while (depth < 5000) {
            if (cur->item >= 0) {
                if (wrote) {
                    out += ' ';
                }
                put_number(out, uid ? items[size_t(cur->item)].uid : items[size_t(cur->item)].seq);
                wrote = true;
            }
            if (cur->children.size() == 1 && cur->item >= 0) {
                cur = cur->children[0].get();
                ++depth;
                continue;
            }
            if (!cur->children.empty()) {
                if (wrote) {
                    out += ' ';
                }
                for (const auto& c : cur->children) {
                    put_thread(out, *c, items, uid, depth + 1);
                }
            }
            break;
        }
        out += ')';
    }
}
