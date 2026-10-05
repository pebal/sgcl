//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/syntax.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../encoding/email.h"
#include "../../time/datetime.h"
#include "../../time/zone.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sgcl::net::imap {
    // The system flags (RFC 9051 §2.3.2), as a message's flags hold them:
    // `m.has_flag(imap::flag::seen)`, `c.add_flags(uids, {imap::flag::deleted})`
    namespace flag {
        inline constexpr char seen[] = "\\Seen";
        inline constexpr char answered[] = "\\Answered";
        inline constexpr char flagged[] = "\\Flagged";
        inline constexpr char deleted[] = "\\Deleted";
        inline constexpr char draft[] = "\\Draft";
        inline constexpr char recent[] = "\\Recent";   // IMAP4rev1's, read only
    }

    // The attributes of a special-use mailbox (RFC 6154), as a LIST entry
    // holds them and create() takes one
    namespace special_use {
        inline constexpr char all[] = "\\All";
        inline constexpr char archive[] = "\\Archive";
        inline constexpr char drafts[] = "\\Drafts";
        inline constexpr char flagged[] = "\\Flagged";
        inline constexpr char junk[] = "\\Junk";
        inline constexpr char sent[] = "\\Sent";
        inline constexpr char trash[] = "\\Trash";
    }

    class sequence_set;

    namespace detail {
        struct SequenceAccess {
            static bool read(sequence_set& s, Lexer& x);
        };
    }

    // "*" in a sequence set: the largest number in use (the last message,
    // the highest UID)
    inline constexpr uint32_t last = 0;

    // A set of message numbers or UIDs (RFC 9051 §9 sequence-set): ranges,
    // "1:4,7,10:*", kept as written (ranges in their order, "*" as
    // imap::last). A value: a copy is the same set. Made from a number, a
    // range, a list of numbers (search() gives one), or the text.
    class sequence_set {
    public:
        // An empty set
        sequence_set() noexcept = default;

        // One number
        SGCL_INLINE_HOT sequence_set(uint32_t n) noexcept {
            add(n);
        }

        // first:last ("*" as imap::last at either end)
        SGCL_INLINE_HOT sequence_set(uint32_t first, uint32_t last) noexcept {
            add(first, last);
        }

        // Every number of the list, consecutive ones joined into ranges
        sequence_set(const vector<uint32_t>& numbers) noexcept {
            std::vector<uint32_t> v(numbers.begin(), numbers.end());
            std::sort(v.begin(), v.end());
            size_t i = 0;
            while (i < v.size()) {
                size_t j = i;
                while (j + 1 < v.size() && (v[j + 1] == v[j] || v[j + 1] == v[j] + 1)) {
                    ++j;
                }
                add(v[i], v[j]);
                i = j + 1;
            }
        }

        // The text "1:4,7,10:*" (or "$", the saved search result, RFC
        // 5182); invalid_argument for one that is not
        explicit sequence_set(const string& text) {
            auto s = _parse(text.view());
            if (!s) {
                throw std::invalid_argument("imap::sequence_set: not a sequence set: " + std::string(text.view()));
            }
            *this = *s;
        }

        // The text read, or nullopt
        static optional<sequence_set> parse(const string& text) noexcept {
            return _parse(text.view());
        }

        // Every message: "1:*"
        SGCL_INLINE_HOT static sequence_set all() noexcept {
            return sequence_set(1, last);
        }

        // "$": the result a search saved (SEARCHRES, RFC 5182)
        SGCL_INLINE_HOT static sequence_set saved() noexcept {
            sequence_set s;
            s._saved = true;
            return s;
        }

        // A number or a range added at the end
        SGCL_INLINE_HOT void add(uint32_t n) noexcept {
            add(n, n);
        }

        void add(uint32_t first, uint32_t last) noexcept {
            if (first == 0 && last != 0) {
                std::swap(first, last);
            }
            if (first != 0 && last != 0 && last < first) {
                std::swap(first, last);
            }
            _ranges.push_back(Range{first, last});
        }

        // Whether n is in the set, "*" standing for largest
        bool contains(uint32_t n, uint32_t largest = UINT32_MAX) const noexcept {
            for (const auto& r : _ranges) {
                uint32_t a = r.first ? r.first : largest;
                uint32_t b = r.last ? r.last : largest;
                if (a > b) {
                    std::swap(a, b);
                }
                if (n >= a && n <= b) {
                    return true;
                }
            }
            return false;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _ranges.empty() && !_saved;
        }

        // Whether it is "$"
        SGCL_INLINE_HOT bool is_saved() const noexcept {
            return _saved;
        }

        // The numbers, in ascending order without repeats, "*" standing for
        // largest; those past largest left out
        vector<uint32_t> expand(uint32_t largest) const noexcept {
            std::vector<uint32_t> out;
            for (const auto& r : _ranges) {
                uint32_t a = r.first ? r.first : largest;
                uint32_t b = r.last ? r.last : largest;
                if (a > b) {
                    std::swap(a, b);
                }
                if (a == 0) {
                    a = 1;
                }
                b = std::min(b, largest);
                for (uint64_t n = a; n <= b; ++n) {
                    out.push_back(uint32_t(n));
                }
            }
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return vector<uint32_t>(out.begin(), out.end());
        }

        // The text, "1:4,7,10:*"; empty for an empty set
        string to_string() const noexcept {
            if (_saved) {
                return string("$");
            }
            std::string out;
            for (size_t i = 0; i < _ranges.size(); ++i) {
                if (i) {
                    out += ',';
                }
                _put(out, _ranges[i].first);
                if (_ranges[i].last != _ranges[i].first) {
                    out += ':';
                    _put(out, _ranges[i].last);
                }
            }
            return string(out);
        }

        // The ranges as pairs (imap::last for "*"), in their order
        vector<pair<uint32_t, uint32_t>> ranges() const noexcept {
            vector<pair<uint32_t, uint32_t>> out;
            for (const auto& r : _ranges) {
                out.push_back(pair<uint32_t, uint32_t>(r.first, r.last));
            }
            return out;
        }

        friend bool operator==(const sequence_set& a, const sequence_set& b) noexcept {
            if (a._saved != b._saved || a._ranges.size() != b._ranges.size()) {
                return false;
            }
            for (size_t i = 0; i < a._ranges.size(); ++i) {
                if (a._ranges[i].first != b._ranges[i].first || a._ranges[i].last != b._ranges[i].last) {
                    return false;
                }
            }
            return true;
        }

    private:
        friend struct detail::SequenceAccess;

        struct Range {
            uint32_t first;
            uint32_t last;
        };

        static optional<sequence_set> _parse(std::string_view text) noexcept {
            sequence_set out;
            if (text == "$") {
                out._saved = true;
                return out;
            }
            detail::Lexer x(text);
            if (!out._read(x) || !x.at_end()) {
                return nullopt;
            }
            return out;
        }

        static void _put(std::string& out, uint32_t n) {
            if (n == 0) {
                out += '*';
            } else {
                detail::put_number(out, n);
            }
        }

        // The set at the lexer's position: number or "*", ":" and another,
        // "," between them
        bool _read(detail::Lexer& x) {
            std::vector<Range> v;
            for (;;) {
                Range r{};
                if (!_one(x, r.first)) {
                    return false;
                }
                r.last = r.first;
                if (x.eat(':') && !_one(x, r.last)) {
                    return false;
                }
                if (r.first && r.last && r.last < r.first) {
                    std::swap(r.first, r.last);
                }
                v.push_back(r);
                if (!x.eat(',')) {
                    break;
                }
                if (v.size() > 100000) {
                    return false;
                }
            }
            _ranges = vector<Range>(v.begin(), v.end());
            return true;
        }

        static bool _one(detail::Lexer& x, uint32_t& n) {
            if (x.eat('*')) {
                n = 0;
                return true;
            }
            return x.nz_number(n);
        }

        vector<Range> _ranges;
        bool _saved = false;
    };

    namespace detail {
        SGCL_INLINE_HOT bool SequenceAccess::read(sequence_set& s, Lexer& x) {
            return s._read(x);
        }
    }

    // An address of an envelope (RFC 9051 §7.5.2): the display name, the
    // mailbox (the part before "@") and the host. Encoded words in the
    // name (RFC 2047) are decoded by the client.
    struct address {
        string name;
        string mailbox;
        string host;

        // "mailbox@host"; the mailbox alone for one without a host
        string email() const noexcept {
            if (host.empty()) {
                return mailbox;
            }
            return mailbox + "@" + host;
        }
    };

    // The envelope of a message (RFC 9051 §7.5.2): its fields as the
    // server parsed them. The date and the ids are as written; the subject
    // and the names are decoded from RFC 2047 by the client. Groups are
    // flattened into their members.
    struct envelope {
        string date;
        string subject;
        vector<address> from;
        vector<address> sender;
        vector<address> reply_to;
        vector<address> to;
        vector<address> cc;
        vector<address> bcc;
        string in_reply_to;
        string message_id;
    };

    // The MIME structure of a message (BODYSTRUCTURE, RFC 9051 §7.5.2): a
    // part with its type, its parameters and its size, and the parts of a
    // multipart; a message/rfc822 part has the envelope of the message it
    // holds and that message's structure as its one part. part is the
    // section number to fetch it by ("1", "2.1"; empty for the message
    // itself). Types and subtypes in lower case.
    struct body_structure {
        string type;                                // "text", "multipart", "message", "image" ...
        string subtype;                             // "plain", "mixed", "rfc822" ...
        vector<pair<string, string>> parameters;    // the parameters of Content-Type, names in lower case
        string id;                                  // Content-ID
        string description;                         // Content-Description
        string encoding;                            // Content-Transfer-Encoding, lower case ("7bit", "base64")
        uint64_t size = 0;                          // the octets of the body as it is encoded
        uint64_t lines = 0;                         // text/* and message/rfc822: the lines of the body
        string md5;                                 // Content-MD5
        string disposition;                         // Content-Disposition, lower case ("attachment", "inline")
        vector<pair<string, string>> disposition_parameters;
        vector<string> language;                    // Content-Language
        string location;                            // Content-Location
        vector<body_structure> parts;               // a multipart's parts; a message/rfc822's message
        optional<imap::envelope> envelope;          // a message/rfc822's
        string part;                                // the section number: "1.2"; empty for the whole message

        // A parameter of Content-Type ("charset", "boundary", "name"),
        // the name in any case; empty when there is none
        string parameter(const string& name) const noexcept {
            for (const auto& [k, v] : parameters) {
                if (detail::iequal(k.view(), name.view())) {
                    return v;
                }
            }
            return string();
        }

        SGCL_INLINE_HOT bool is_multipart() const noexcept {
            return type == "multipart";
        }

        // The file name of an attachment: the disposition's "filename",
        // else the type's "name"; empty when neither
        string filename() const noexcept {
            for (const auto& [k, v] : disposition_parameters) {
                if (detail::iequal(k.view(), "filename")) {
                    return v;
                }
            }
            return parameter(string("name"));
        }
    };

    // A message as FETCH gave it: the items asked for (the UID always),
    // the rest left at their defaults
    struct message {
        uint32_t seq = 0;                                   // the message sequence number
        uint32_t uid = 0;
        vector<string> flags;                               // "\\Seen", "$Forwarded" ...
        optional<time::datetime> internal_date;             // INTERNALDATE: when the server received it
        uint64_t size = 0;                                  // RFC822.SIZE
        uint64_t modseq = 0;                                // MODSEQ (CONDSTORE)
        optional<imap::envelope> envelope;
        optional<imap::body_structure> body_structure;
        vector<pair<string, string>> sections;              // BODY[section] and BINARY[section]: the section ("", "HEADER", "1.2") and its bytes
        vector<pair<string, uint64_t>> binary_sizes;        // BINARY.SIZE[section]

        // The bytes of a section fetched ("" the whole message, "HEADER",
        // "TEXT", "1.2"); nullopt for one not fetched
        optional<string> section(const string& part) const noexcept {
            for (const auto& [k, v] : sections) {
                if (detail::iequal(k.view(), part.view())) {
                    return v;
                }
            }
            return nullopt;
        }

        // The whole message ("" fetched); empty when it was not
        string text() const noexcept {
            auto s = section(string());
            return s ? *s : string();
        }

        // The whole message ("" fetched) parsed by encoding::email: its
        // header, text, HTML and attachments; a message whose whole was
        // not fetched parses as an empty one, without fields or text
        expected<encoding::email, encoding::error> email() const {
            return encoding::email::parse(text());
        }

        expected<encoding::email, encoding::error> email(const encoding::email::limits& l) const {
            return encoding::email::parse(text(), l);
        }

        bool has_flag(const string& f) const noexcept {
            for (const auto& x : flags) {
                if (detail::iequal(x.view(), f.view())) {
                    return true;
                }
            }
            return false;
        }
    };

    // What STATUS tells of a mailbox (RFC 9051 §6.3.11, CONDSTORE's
    // HIGHESTMODSEQ, RFC 8438's SIZE): the items asked for; the rest 0
    struct status {
        uint32_t messages = 0;
        uint32_t recent = 0;            // IMAP4rev1's
        uint32_t uid_next = 0;
        uint32_t uid_validity = 0;
        uint32_t unseen = 0;
        uint32_t deleted = 0;
        uint64_t size = 0;              // the octets of every message together
        uint64_t highest_modseq = 0;
    };

    // A mailbox as LIST gave it (RFC 9051 §7.3.1): its name in UTF-8, the
    // hierarchy delimiter, the attributes ("\\HasChildren", "\\Noselect",
    // "\\Subscribed", "\\Sent" ...) and, for LIST-STATUS, its status. A
    // backend gives its mailboxes as list entries too.
    struct list_entry {
        string name;
        char delimiter = '/';           // 0 for a flat namespace
        vector<string> attributes;
        optional<imap::status> status;

        bool has_attribute(const string& a) const noexcept {
            for (const auto& x : attributes) {
                if (detail::iequal(x.view(), a.view())) {
                    return true;
                }
            }
            return false;
        }
    };

    // What SELECT or EXAMINE said of the mailbox opened (RFC 9051
    // §6.3.2): the counts, the UIDs, the flags; with QRESYNC (RFC 7162
    // §3.2.5) the UIDs expunged since the state given and the messages
    // changed since
    struct selected {
        string name;
        bool read_only = false;
        uint32_t exists = 0;
        uint32_t recent = 0;            // IMAP4rev1's
        uint32_t uid_validity = 0;
        uint32_t uid_next = 0;
        uint32_t first_unseen = 0;      // IMAP4rev1's UNSEEN: the number of the first unseen message
        uint64_t highest_modseq = 0;    // 0: the mailbox keeps no mod-sequences (NOMODSEQ)
        vector<string> flags;
        vector<string> permanent_flags; // "\\*": new keywords may be made
        sequence_set vanished;          // QRESYNC: the UIDs expunged since
        vector<message> changed;        // QRESYNC: the messages changed since (UID, FLAGS, MODSEQ)
    };

    // What the server answered a COPY or MOVE (UIDPLUS's COPYUID, RFC
    // 4315): the destination's UIDVALIDITY and the UIDs, in pairs; empty
    // when the server gave none
    struct copy_result {
        uint32_t uid_validity = 0;
        vector<uint32_t> source;
        vector<uint32_t> destination;
    };

    // A quota root's resources (RFC 9208): what is used and the limit, the
    // storage in KiB; a limit of 0 is none
    struct quota {
        string root;
        uint64_t storage_used = 0;      // KiB
        uint64_t storage_limit = 0;
        uint64_t messages_used = 0;
        uint64_t messages_limit = 0;
    };

    // One namespace (RFC 2342): its prefix and delimiter
    struct namespace_entry {
        string prefix;
        char delimiter = '/';
    };

    // The namespaces of the server: the user's own, other users', shared
    struct namespaces {
        vector<namespace_entry> personal;
        vector<namespace_entry> other_users;
        vector<namespace_entry> shared;
    };

    // What happened in the selected mailbox while the client was not
    // asking: a message arrived (exists, the new count), one was expunged
    // (expunge: its number, the numbers after it moving down), UIDs vanished
    // (QRESYNC), a message's flags changed (fetch: its number, UID when the
    // server sent it, flags, modseq), the mailbox's flags changed, the
    // server's alert, the server's goodbye
    struct update {
        enum class kind : uint8_t { exists, recent, expunge, vanished, fetch, flags, alert, bye };

        update::kind kind = kind::exists;
        uint32_t number = 0;            // exists, recent: the count; expunge, fetch: the message's number
        uint32_t uid = 0;               // fetch: when sent
        vector<string> flags;           // fetch, flags
        uint64_t modseq = 0;            // fetch: when sent
        sequence_set uids;              // vanished
        string text;                    // alert, bye
    };

    // A thread (THREAD, RFC 5256): a message (its UID; 0 for a missing
    // parent the algorithm put in) and the threads under it
    struct thread {
        uint32_t uid = 0;
        vector<thread> children;
    };

    // The data a backend gives of a message: its UID, flags, mod-sequence,
    // when it was received and its size (the server keeps them while the
    // mailbox is open)
    struct stored_message {
        uint32_t uid = 0;
        vector<string> flags;
        uint64_t modseq = 1;
        time::datetime internal_date;
        uint64_t size = 0;
    };

    // A mailbox as a backend opens it: UIDVALIDITY, the next UID, the
    // highest mod-sequence and the messages in ascending order of UID
    struct mailbox_contents {
        uint32_t uid_validity = 1;
        uint32_t uid_next = 1;
        uint64_t highest_modseq = 1;
        vector<stored_message> messages;
    };

    // The new flags of a message, as the server asks a backend to store
    // them
    struct flag_update {
        uint32_t uid = 0;
        vector<string> flags;
    };
}
