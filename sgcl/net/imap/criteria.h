//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../../core/duration.h"
#include "../../time/date.h"

#include <string>
#include <string_view>

namespace sgcl::net::imap {
    class criteria;

    namespace detail {
        struct CriteriaAccess {
            static const string& text(const criteria& c) noexcept;
        };

        // A string argument inside the criteria's text: between two NUL
        // bytes (a NUL cannot be searched for, RFC 9051 §4.3), so that the
        // client writes it as its connection allows (quoted, a literal, a
        // literal after CHARSET UTF-8)
        inline void put_argument(std::string& out, std::string_view s) {
            out += '\0';
            for (char c : s) {
                if (c != '\0') {
                    out += c;
                }
            }
            out += '\0';
        }
    }

    // What a search looks for (RFC 9051 §6.4.4): a search key, or keys
    // combined with &&, || and !. A value: made by the static functions,
    // `imap::criteria::unseen() && imap::criteria::from("alice")`; the
    // default is every message. The strings are searched as substrings,
    // in any case, as the server matches them.
    class criteria {
    public:
        // Every message (ALL)
        criteria() noexcept = default;

        // --- flags ---
        SGCL_INLINE_HOT static criteria all() noexcept {
            return criteria();
        }

        SGCL_INLINE_HOT static criteria seen() noexcept {
            return _key("SEEN");
        }

        SGCL_INLINE_HOT static criteria unseen() noexcept {
            return _key("UNSEEN");
        }

        SGCL_INLINE_HOT static criteria answered() noexcept {
            return _key("ANSWERED");
        }

        SGCL_INLINE_HOT static criteria unanswered() noexcept {
            return _key("UNANSWERED");
        }

        SGCL_INLINE_HOT static criteria flagged() noexcept {
            return _key("FLAGGED");
        }

        SGCL_INLINE_HOT static criteria unflagged() noexcept {
            return _key("UNFLAGGED");
        }

        SGCL_INLINE_HOT static criteria deleted() noexcept {
            return _key("DELETED");
        }

        SGCL_INLINE_HOT static criteria undeleted() noexcept {
            return _key("UNDELETED");
        }

        SGCL_INLINE_HOT static criteria draft() noexcept {
            return _key("DRAFT");
        }

        SGCL_INLINE_HOT static criteria undraft() noexcept {
            return _key("UNDRAFT");
        }

        // A keyword set ("$Forwarded"), or not set
        SGCL_INLINE_HOT static criteria keyword(const string& k) noexcept {
            return _key_atom("KEYWORD", k);
        }

        SGCL_INLINE_HOT static criteria unkeyword(const string& k) noexcept {
            return _key_atom("UNKEYWORD", k);
        }

        // --- the header and the text ---
        SGCL_INLINE_HOT static criteria from(const string& s) noexcept {
            return _key_string("FROM", s);
        }

        SGCL_INLINE_HOT static criteria to(const string& s) noexcept {
            return _key_string("TO", s);
        }

        SGCL_INLINE_HOT static criteria cc(const string& s) noexcept {
            return _key_string("CC", s);
        }

        SGCL_INLINE_HOT static criteria bcc(const string& s) noexcept {
            return _key_string("BCC", s);
        }

        SGCL_INLINE_HOT static criteria subject(const string& s) noexcept {
            return _key_string("SUBJECT", s);
        }

        // The body, the text after the header
        SGCL_INLINE_HOT static criteria body(const string& s) noexcept {
            return _key_string("BODY", s);
        }

        // The header or the body
        SGCL_INLINE_HOT static criteria text(const string& s) noexcept {
            return _key_string("TEXT", s);
        }

        // A field of the header holding the value; an empty value: any
        // message with the field
        static criteria header(const string& field, const string& value) noexcept {
            std::string t = "HEADER ";
            detail::put_argument(t, field.view());
            t += ' ';
            detail::put_argument(t, value.view());
            return criteria(string(t), true);
        }

        // --- dates (of the day, the time and zone left out, RFC 9051) ---
        // INTERNALDATE: received before, on, since (on or after) the day
        SGCL_INLINE_HOT static criteria before(const time::date& d) noexcept {
            return _key_date("BEFORE", d);
        }

        SGCL_INLINE_HOT static criteria on(const time::date& d) noexcept {
            return _key_date("ON", d);
        }

        SGCL_INLINE_HOT static criteria since(const time::date& d) noexcept {
            return _key_date("SINCE", d);
        }

        // The Date: field of the header
        SGCL_INLINE_HOT static criteria sent_before(const time::date& d) noexcept {
            return _key_date("SENTBEFORE", d);
        }

        SGCL_INLINE_HOT static criteria sent_on(const time::date& d) noexcept {
            return _key_date("SENTON", d);
        }

        SGCL_INLINE_HOT static criteria sent_since(const time::date& d) noexcept {
            return _key_date("SENTSINCE", d);
        }

        // Received more (less) than the time ago (WITHIN, RFC 5032)
        SGCL_INLINE_HOT static criteria older(duration d) noexcept {
            return _key_number("OLDER", uint64_t(d.seconds() > 0 ? d.seconds() : 1));
        }

        SGCL_INLINE_HOT static criteria younger(duration d) noexcept {
            return _key_number("YOUNGER", uint64_t(d.seconds() > 0 ? d.seconds() : 1));
        }

        // --- size, numbers ---
        // RFC822.SIZE over (under) the octets
        SGCL_INLINE_HOT static criteria larger(uint64_t octets) noexcept {
            return _key_number("LARGER", octets);
        }

        SGCL_INLINE_HOT static criteria smaller(uint64_t octets) noexcept {
            return _key_number("SMALLER", octets);
        }

        // The UIDs of the set
        SGCL_INLINE_HOT static criteria uid(const sequence_set& s) noexcept {
            return criteria(string("UID " + std::string(s.to_string().view())), true);
        }

        // The message numbers of the set
        SGCL_INLINE_HOT static criteria numbers(const sequence_set& s) noexcept {
            return criteria(s.to_string(), true);
        }

        // A mod-sequence at or past n (CONDSTORE, RFC 7162)
        SGCL_INLINE_HOT static criteria modseq(uint64_t n) noexcept {
            return _key_number("MODSEQ", n);
        }

        // --- combinations ---
        // Both
        friend criteria operator&&(const criteria& a, const criteria& b) noexcept {
            if (a._text.empty()) {
                return b;
            }
            if (b._text.empty()) {
                return a;
            }
            return criteria(a._text + " " + b._text, false);
        }

        // Either
        friend criteria operator||(const criteria& a, const criteria& b) noexcept {
            return criteria("OR " + a._single_text() + " " + b._single_text(), true);
        }

        // Not
        friend criteria operator!(const criteria& a) noexcept {
            return criteria("NOT " + a._single_text(), true);
        }

        // The search keys as the command writes them, strings quoted
        // ("UNSEEN FROM \"alice\"")
        string to_string() const noexcept {
            if (_text.empty()) {
                return string("ALL");
            }
            std::string out;
            std::string_view t = _text.view();
            size_t i = 0;
            while (i < t.size()) {
                if (t[i] == '\0') {
                    const size_t end = t.find('\0', i + 1);
                    detail::put_quoted(out, t.substr(i + 1, end - i - 1));
                    i = end + 1;
                } else {
                    out += t[i++];
                }
            }
            return string(out);
        }

    private:
        friend struct detail::CriteriaAccess;

        SGCL_INLINE_HOT criteria(const string& text, bool single) noexcept
        : _text(text)
        , _single(single) {
        }

        SGCL_INLINE_HOT static criteria _key(const char* k) noexcept {
            return criteria(string(k), true);
        }

        static criteria _key_string(const char* k, const string& s) noexcept {
            std::string t = k;
            t += ' ';
            detail::put_argument(t, s.view());
            return criteria(string(t), true);
        }

        static criteria _key_atom(const char* k, const string& s) noexcept {
            std::string t = k;
            t += ' ';
            bool atom = !s.empty();
            for (unsigned char c : s.view()) {
                atom &= detail::atom_char(c);
            }
            if (atom) {
                t += s.view();
            } else {
                detail::put_argument(t, s.view());
            }
            return criteria(string(t), true);
        }

        static criteria _key_number(const char* k, uint64_t n) noexcept {
            std::string t = k;
            t += ' ';
            detail::put_number(t, n);
            return criteria(string(t), true);
        }

        static criteria _key_date(const char* k, const time::date& d) noexcept {
            std::string t = k;
            t += ' ';
            t += detail::format_date(detail::days_from_civil(d.year(), unsigned(int(d.month())), unsigned(d.day())));
            return criteria(string(t), true);
        }

        string _single_text() const noexcept {
            if (_text.empty()) {
                return string("ALL");
            }
            if (_single) {
                return _text;
            }
            return "(" + _text + ")";
        }

        string _text;          // empty: ALL
        bool _single = true;   // one search key (NOT and OR take it as it is)
    };

    namespace detail {
        SGCL_INLINE_HOT const string& CriteriaAccess::text(const criteria& c) noexcept {
            return c._text;
        }
    }
}
