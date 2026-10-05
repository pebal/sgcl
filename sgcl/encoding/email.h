//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "base64.h"
#include "quoted_printable.h"
#include "detail/files.h"
#include "detail/mail_text.h"
#include "detail/media_types.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/file.h"
#include "../io/functions.h"
#include "../io/path.h"
#include "../io/stream.h"
#include "../time/datetime.h"
#include "../time/layout.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// A mail message (RFC 5322) with its MIME structure (RFC 2045–2049):
// built, written, parsed. The SMTP of net::smtp sends and receives it.
namespace sgcl::encoding {
    class email;

    namespace detail {
        using namespace sgcl::detail;

        struct MailAccess;

        // A field of a head: the value as it came over the wire (raw: the
        // text after the colon, its folds kept as CRLF), or a program's
        // text, encoded when it is written
        struct MailField {
            string name;
            string value;
            bool raw = false;
        };

        // An entity: a message's head and body, or a part's. A leaf holds
        // its content with the transfer encoding undone; a multipart its
        // parts; a message/rfc822 the message inside it.
        struct MailPart {
            vector<MailField> fields;
            vector<byte> content;
            vector<tracked_ptr<MailPart>> parts;
            tracked_ptr<MailPart> message;
            bool default_rfc822 = false;   // a part of multipart/digest (RFC 2046 §5.1.5)
            bool generated_id = false;     // the root's Message-ID is the constructor's
        };

        inline vector<byte> mail_bytes(std::string_view s) {
            vector<byte> v;
            VectorOverwrite::resize(v, s.size());
            if (!s.empty()) {
                copy_bytes(v.data(), s.data(), s.size());
            }
            return v;
        }

        inline std::string_view mail_view(const vector<byte>& v) noexcept {
            return std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
        }

        inline const MailField* mail_find(const MailPart& p, std::string_view name) noexcept {
            for (auto& f : p.fields) {
                if (mail_iequal(f.name.view(), name)) {
                    return &f;
                }
            }
            return nullptr;
        }

        // A field's value as text: a raw one unfolded, trimmed, its encoded
        // words decoded; a program's as it gave it
        inline std::string mail_field_text(const MailField& f) {
            if (!f.raw) {
                return std::string(f.value.view());
            }
            std::string u = mail_unfold(f.value.view());
            return mail_decode_words(mail_trim(u));
        }

        // A raw field's value without its folds, for a structured reading
        inline std::string mail_field_structured(const MailField& f) {
            if (!f.raw) {
                return std::string(f.value.view());
            }
            return std::string(mail_trim(mail_unfold(f.value.view())));
        }

        inline void mail_check_name(const string& name) {
            if (!mail_field_name_ok(name.view())) {
                throw invalid_argument("sgcl::encoding::email: invalid header name");
            }
        }

        // The value in the place of the first field of the name, the
        // others of the name gone; a field at the end when there was none
        inline void mail_set(MailPart& p, const string& name, const string& value, bool raw) {
            bool found = false;
            size_t kept = 0;
            for (size_t i = 0; i < p.fields.size(); ++i) {
                if (mail_iequal(p.fields[i].name.view(), name.view())) {
                    if (found) {
                        continue;
                    }
                    found = true;
                    p.fields[i].value = value;
                    p.fields[i].raw = raw;
                }
                if (kept != i) {
                    p.fields[kept] = p.fields[i];
                }
                ++kept;
            }
            if (found) {
                p.fields.resize(kept);
            } else {
                p.fields.push_back(MailField{name, value, raw});
            }
        }

        inline void mail_remove(MailPart& p, std::string_view name) {
            size_t kept = 0;
            for (size_t i = 0; i < p.fields.size(); ++i) {
                if (mail_iequal(p.fields[i].name.view(), name)) {
                    continue;
                }
                if (kept != i) {
                    p.fields[kept] = p.fields[i];
                }
                ++kept;
            }
            p.fields.resize(kept);
        }

        // The media type of a part and its parameters
        struct MailType {
            std::string type;   // "text/plain", lower case
            std::vector<MailParam> params;

            const std::string* param(std::string_view name) const noexcept {
                for (auto& p : params) {
                    if (p.name == name) {
                        return &p.value;
                    }
                }
                return nullptr;
            }
        };

        inline MailType mail_type(const MailPart& p) {
            MailType t;
            if (auto f = mail_find(p, "content-type")) {
                t.type = mail_parse_params(mail_field_structured(*f), t.params);
                if (t.type.find('/') == std::string::npos || t.type.front() == '/' || t.type.back() == '/') {
                    // RFC 2045 §5.2: a type that cannot be read is text/plain (its charset kept)
                    t.type = "text/plain";
                }
            } else {
                t.type = p.default_rfc822 ? "message/rfc822" : "text/plain";
            }
            return t;
        }

        inline std::string mail_disposition(const MailPart& p, std::vector<MailParam>* params = nullptr) {
            if (auto f = mail_find(p, "content-disposition")) {
                std::vector<MailParam> local;
                return mail_parse_params(mail_field_structured(*f), params ? *params : local);
            }
            return std::string();
        }

        inline std::string mail_filename(const MailPart& p) {
            std::vector<MailParam> params;
            mail_disposition(p, &params);
            for (auto& q : params) {
                if (q.name == "filename") {
                    return q.value;
                }
            }
            auto t = mail_type(p);
            if (auto n = t.param("name")) {
                return *n;
            }
            return std::string();
        }

        inline bool mail_is_multipart(const MailPart& p) {
            return !p.parts.empty() || mail_type(p).type.starts_with("multipart/");
        }

        // An attachment as a reader of mail takes it: a disposition of
        // "attachment", or a file's name on a part that is not the text
        // of the body, or a part that is not text in a multipart/mixed
        inline bool mail_is_attachment(const MailPart& p) {
            if (!p.parts.empty()) {
                return false;
            }
            std::string d = mail_disposition(p);
            if (d == "attachment") {
                return true;
            }
            if (d == "inline") {
                return false;
            }
            return !mail_filename(p).empty();
        }

        // The content as text in UTF-8: its charset converted (a charset
        // txt does not know: the bytes as they are), its line breaks LF
        inline std::string mail_text_of(const MailPart& p) {
            auto t = mail_type(p);
            const std::string* cs = t.param("charset");
            std::string text;
            if (!mail_charset_to_utf8(mail_view(p.content), cs ? std::string_view(*cs) : std::string_view(), text)) {
                text.assign(mail_view(p.content));
            }
            std::string out;
            out.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '\r') {
                    out += '\n';
                    if (i + 1 < text.size() && text[i + 1] == '\n') {
                        ++i;
                    }
                } else {
                    out += text[i];
                }
            }
            return out;
        }

        // Text in UTF-8 as a part's content: its line breaks CRLF
        inline vector<byte> mail_crlf(std::string_view text) {
            std::string out;
            out.reserve(text.size() + text.size() / 32 + 2);
            for (size_t i = 0; i < text.size(); ++i) {
                char c = text[i];
                if (c == '\r') {
                    out += "\r\n";
                    if (i + 1 < text.size() && text[i + 1] == '\n') {
                        ++i;
                    }
                } else if (c == '\n') {
                    out += "\r\n";
                } else {
                    out += c;
                }
            }
            return mail_bytes(out);
        }

        inline string mail_raw(std::string_view value) {
            std::string v = " ";
            v += value;
            return string(v);
        }

        inline tracked_ptr<MailPart> mail_leaf(std::string_view content_type, vector<byte> content) {
            tracked_ptr p = make_tracked<MailPart>();
            p->fields.push_back(MailField{string("Content-Type"), mail_raw(content_type), true});
            p->content = std::move(content);
            return p;
        }

        inline tracked_ptr<MailPart> mail_text_leaf(std::string_view subtype, std::string_view text) {
            std::string t = "text/";
            t += subtype;
            t += "; charset=utf-8";
            return mail_leaf(t, mail_crlf(text));
        }

        // The value of a Content-Disposition: the type and the file's name
        inline string mail_disposition_value(std::string_view type, std::string_view filename) {
            std::string v(type);
            if (!filename.empty()) {
                mail_write_param(v, "filename", filename);
            }
            return mail_raw(v);
        }

        inline std::string mail_domain_of(const MailPart& root) {
            if (auto f = mail_find(root, "from")) {
                std::vector<MailMailbox> list;
                if (mail_address_list(mail_field_structured(*f), list, nullptr) && !list.empty()) {
                    auto at = list.front().addr.rfind('@');
                    if (at != std::string::npos) {
                        std::string d = list.front().addr.substr(at + 1);
                        if (!d.empty() && d.front() != '[' && mail_ascii(d)) {
                            return d;
                        }
                        if (!d.empty() && d.front() != '[') {
                            if (auto a = txt::idna::to_ascii(string(d))) {
                                return std::string(a->view());
                            }
                        }
                    }
                }
            }
            return "localhost";
        }

        inline string mail_new_id(const MailPart& root) {
            std::string id = "<";
            mail_random(id, 24);
            id += '@';
            id += mail_domain_of(root);
            id += '>';
            return string(id);
        }

        // The body's pieces as a program builds them: the text, the HTML,
        // the parts the HTML shows by Content-ID, the attachments
        struct MailPieces {
            tracked_ptr<MailPart> text;
            tracked_ptr<MailPart> html;
            vector<tracked_ptr<MailPart>> related;
            vector<tracked_ptr<MailPart>> attachments;
        };

        // A leaf with only the content fields of p and its content: the
        // root's body taken out of the root, whose head stays the message's
        inline tracked_ptr<MailPart> mail_content_copy(const MailPart& p) {
            tracked_ptr c = make_tracked<MailPart>();
            for (auto& f : p.fields) {
                if (f.name.size() >= 8 && mail_iequal(f.name.view().substr(0, 8), "content-")) {
                    c->fields.push_back(f);
                }
            }
            c->content = p.content;
            c->parts = p.parts;
            c->message = p.message;
            c->default_rfc822 = p.default_rfc822;
            return c;
        }

        inline void mail_decompose(const tracked_ptr<MailPart>& node, bool root, std::string_view parent, MailPieces& out) {
            auto t = mail_type(*node);
            if (!node->parts.empty()) {
                for (auto& child : node->parts) {
                    mail_decompose(child, false, t.type, out);
                }
                return;
            }
            if (root && node->content.empty() && t.type == "text/plain" && !mail_find(*node, "content-disposition")) {
                return;   // the empty text of a message just made: no text yet
            }
            tracked_ptr<MailPart> leaf = root ? mail_content_copy(*node) : node;
            if (node->message || t.type == "message/rfc822") {
                out.attachments.push_back(leaf);
                return;
            }
            const std::string d = mail_disposition(*node);
            const bool plain = d != "attachment" && mail_filename(*node).empty();
            if (t.type == "text/plain" && plain && !out.text) {
                out.text = leaf;
            } else if (t.type == "text/html" && plain && !out.html) {
                out.html = leaf;
            } else if (mail_find(*node, "content-id") && (d == "inline" || parent == "multipart/related")) {
                out.related.push_back(leaf);
            } else if (t.type.starts_with("multipart/")) {
                return;   // an empty multipart: nothing
            } else {
                out.attachments.push_back(leaf);
            }
        }

        inline tracked_ptr<MailPart> mail_multipart(std::string_view subtype, std::string_view extra = {}) {
            tracked_ptr p = make_tracked<MailPart>();
            std::string t = "multipart/";
            t += subtype;
            t += extra;
            p->fields.push_back(MailField{string("Content-Type"), mail_raw(t), true});
            return p;
        }

        // The pieces back as the shape every reader of mail knows:
        // multipart/mixed of the body and the attachments, the body
        // multipart/alternative of the text and the HTML, the HTML
        // multipart/related with its inline parts; the head of the root
        // kept, its content fields those of the top entity
        inline void mail_recompose(MailPart& root, MailPieces& pc) {
            tracked_ptr<MailPart> html = pc.html;
            if (html && !pc.related.empty()) {
                tracked_ptr rel = mail_multipart("related", "; type=\"text/html\"");
                rel->parts.push_back(html);
                for (auto& r : pc.related) {
                    rel->parts.push_back(r);
                }
                html = rel;
            } else if (!pc.related.empty()) {
                for (auto& r : pc.related) {
                    pc.attachments.push_back(r);
                }
            }
            tracked_ptr<MailPart> body;
            if (pc.text && html) {
                body = mail_multipart("alternative");
                body->parts.push_back(pc.text);
                body->parts.push_back(html);
            } else if (pc.text) {
                body = pc.text;
            } else if (html) {
                body = html;
            }
            tracked_ptr<MailPart> top = body;
            if (!pc.attachments.empty()) {
                top = mail_multipart("mixed");
                if (body) {
                    top->parts.push_back(body);
                }
                for (auto& a : pc.attachments) {
                    top->parts.push_back(a);
                }
            }
            if (!top) {
                top = mail_text_leaf("plain", "");
            }
            size_t kept = 0;
            for (size_t i = 0; i < root.fields.size(); ++i) {
                auto& f = root.fields[i];
                if (f.name.size() >= 8 && mail_iequal(f.name.view().substr(0, 8), "content-")) {
                    continue;
                }
                if (kept != i) {
                    root.fields[kept] = root.fields[i];
                }
                ++kept;
            }
            root.fields.resize(kept);
            for (auto& f : top->fields) {
                if (f.name.size() >= 8 && mail_iequal(f.name.view().substr(0, 8), "content-")) {
                    root.fields.push_back(f);
                }
            }
            root.content = top->content;
            root.parts = top->parts;
            root.message = top->message;
            root.default_rfc822 = false;
        }

        template<class F>
        void mail_restructure(const tracked_ptr<MailPart>& root, F change) {
            MailPieces pc;
            mail_decompose(root, true, std::string_view(), pc);
            change(pc);
            mail_recompose(*root, pc);
        }

        // The first text/plain (or text/html) of the body, as a reader of
        // mail shows it: never an attachment, the first alternative of its
        // kind
        inline const MailPart* mail_find_body(const MailPart& p, std::string_view type) {
            if (!p.parts.empty()) {
                for (auto& c : p.parts) {
                    if (auto f = mail_find_body(*c, type)) {
                        return f;
                    }
                }
                return nullptr;
            }
            if (p.message) {
                return nullptr;
            }
            if (mail_type(p).type == type && mail_disposition(p) != "attachment" && mail_filename(p).empty()) {
                return &p;
            }
            return nullptr;
        }

        inline void mail_collect_attachments(const tracked_ptr<MailPart>& p, bool top, vector<tracked_ptr<MailPart>>& out) {
            if (!p->parts.empty()) {
                for (auto& c : p->parts) {
                    mail_collect_attachments(c, false, out);
                }
                return;
            }
            if (top) {
                return;
            }
            if (p->message || mail_is_attachment(*p)) {
                out.push_back(p);
            }
        }

        inline bool mail_address_field(std::string_view name) noexcept {
            static constexpr std::string_view Names[] = {"from", "sender", "reply-to", "to", "cc", "bcc", "resent-from", "resent-sender",
                                                          "resent-to", "resent-cc", "resent-bcc", "disposition-notification-to",
                                                          "return-receipt-to", "mail-followup-to", "mail-reply-to"};
            for (auto n : Names) {
                if (mail_iequal(name, n)) {
                    return true;
                }
            }
            return false;
        }

        inline bool mail_verbatim_field(std::string_view name) noexcept {
            static constexpr std::string_view Names[] = {"message-id", "in-reply-to", "references", "content-id", "date", "mime-version",
                                                          "content-type", "content-disposition", "content-transfer-encoding",
                                                          "resent-message-id", "resent-date", "received", "return-path", "list-id",
                                                          "dkim-signature", "arc-seal", "arc-message-signature", "authentication-results"};
            for (auto n : Names) {
                if (mail_iequal(name, n)) {
                    return true;
                }
            }
            return false;
        }
    }

    // A mail message: its head and its body, the MIME structure of the
    // body, written as RFC 5322 and RFC 2045–2049 have it and parsed from
    // what the mail of the world is. A handle of one word, as http's
    // request is: a copy is the same message.
    //
    //     encoding::email m("alice@example.com", "bob@example.com", "Hi", "Hello, Bob.");
    //     m.set_html("<p>Hello, <b>Bob</b>.</p>");
    //     m.attach("report.pdf");
    //     string text = m.to_string();
    //
    // The head's fields are set and read by name; the addresses, the
    // subject, the date and the Message-ID have their own calls. Text
    // past ASCII in the head goes out as encoded words (RFC 2047), a
    // file's name as RFC 2231 has it; the body's parts get the transfer
    // encoding their content needs (7bit, quoted-printable, base64, 8bit
    // only where the transport takes it), lines are folded at 78, CRLF
    // everywhere, a boundary drawn at random and checked against the
    // content. The constructors set Date (now) and a Message-ID.
    //
    // Parsing is lenient as the readers of mail are: the obsolete syntax
    // of RFC 5322 §4, UTF-8 in the head (RFC 6532), LF for CRLF, a
    // multipart without its closing delimiter, a malformed encoded word
    // left as it is; only the limits refuse a message.
    class email {
    public:
        using error = encoding::error;

        class address;
        class part;

        // What a parse takes: the bytes of one entity's head, the nesting
        // of multiparts and messages, the parts of the whole message
        struct limits {
            size_t max_header_bytes = 256 * 1024;
            size_t max_depth = 32;
            size_t max_parts = 10000;
        };

        // How a message is written: 8bit content where the transport
        // takes it (8BITMIME), UTF-8 in the head where it takes that
        // (SMTPUTF8, RFC 6532), the Bcc field or not
        struct write_options {
            bool allow_8bit = false;
            bool allow_utf8 = false;
            bool write_bcc = true;
        };

        // An empty message: Date now, a Message-ID, a body of empty text
        email()
        : _root(make_tracked<detail::MailPart>()) {
            _init();
        }

        // A message of a text from one address to others (a list, "a@x,
        // B <b@y>"): the common case in one line. An address that does not
        // parse is invalid_argument.
        email(const string& from, const string& to, const string& subject, const string& text)
        : email() {
            set_from(from);
            add_to(to);
            set_subject(subject);
            set_text(text);
        }

        // ---- the head

        // The first value of the field, as text: unfolded, its encoded
        // words decoded; "" when there is none
        string header(const string& name) const {
            if (auto f = detail::mail_find(*_root, name.view())) {
                return string(detail::mail_field_text(*f));
            }
            return string();
        }

        // Every value of the field, in their order (Received)
        vector<string> header_all(const string& name) const {
            vector<string> out;
            for (auto& f : _root->fields) {
                if (detail::mail_iequal(f.name.view(), name.view())) {
                    out.push_back(string(detail::mail_field_text(f)));
                }
            }
            return out;
        }

        bool has_header(const string& name) const noexcept {
            return detail::mail_find(*_root, name.view()) != nullptr;
        }

        // Every field in its order, names as written, values as header()
        vector<pair<string, string>> headers() const {
            vector<pair<string, string>> out;
            for (auto& f : _root->fields) {
                out.push_back(pair<string, string>(f.name, string(detail::mail_field_text(f))));
            }
            return out;
        }

        // The value as text, in the place of the first field of the name
        // (the others gone), or at the end; encoded when written. A name
        // that is not one (RFC 5322 §3.6.8) is invalid_argument.
        email& set_header(const string& name, const string& value) {
            detail::mail_check_name(name);
            detail::mail_set(*_root, name, value, false);
            if (detail::mail_iequal(name.view(), "message-id")) {
                _root->generated_id = false;
            }
            return *this;
        }

        email& add_header(const string& name, const string& value) {
            detail::mail_check_name(name);
            _root->fields.push_back(detail::MailField{name, value, false});
            return *this;
        }

        email& remove_header(const string& name) {
            detail::mail_remove(*_root, name.view());
            return *this;
        }

        // ---- the addresses

        optional<address> from() const;
        vector<address> to() const;
        vector<address> cc() const;
        vector<address> bcc() const;
        vector<address> reply_to() const;

        email& set_from(const address& a);
        email& set_from(const string& text);
        email& add_to(const address& a);
        email& add_to(const string& list);
        email& add_cc(const address& a);
        email& add_cc(const string& list);
        email& add_bcc(const address& a);
        email& add_bcc(const string& list);
        email& add_reply_to(const address& a);
        email& add_reply_to(const string& list);

        // ---- the subject, the date, the id

        string subject() const {
            return header("Subject");
        }

        email& set_subject(const string& text) {
            detail::mail_set(*_root, string("Subject"), text, false);
            return *this;
        }

        // The Date field as RFC 5322 reads it (the obsolete zones too);
        // nullopt when there is none or it is not a date
        optional<time::datetime> date() const {
            if (auto f = detail::mail_find(*_root, "date")) {
                if (auto t = time::datetime::parse(string(detail::mail_field_structured(*f)), time::email)) {
                    return *t;
                }
            }
            return nullopt;
        }

        email& set_date(const time::datetime& t) {
            detail::mail_set(*_root, string("Date"), t.format(time::email), false);
            return *this;
        }

        // The Message-ID as written, with its angle brackets
        string message_id() const {
            if (auto f = detail::mail_find(*_root, "message-id")) {
                return string(detail::mail_field_structured(*f));
            }
            return string();
        }

        // An id given with or without its angle brackets
        email& set_message_id(const string& id) {
            std::string v(detail::mail_trim(id.view()));
            if (v.empty() || v.front() != '<') {
                v = "<" + v + ">";
            }
            detail::mail_set(*_root, string("Message-ID"), string(v), false);
            _root->generated_id = false;
            return *this;
        }

        // ---- the body

        // The text of the body: its first text/plain that is not an
        // attachment, in UTF-8 with LF line breaks; "" when it has none
        string text() const {
            if (auto p = detail::mail_find_body(*_root, "text/plain")) {
                return string(detail::mail_text_of(*p));
            }
            return string();
        }

        string html() const {
            if (auto p = detail::mail_find_body(*_root, "text/html")) {
                return string(detail::mail_text_of(*p));
            }
            return string();
        }

        // The text of the body, put where a reader of mail looks for it
        // (beside the HTML as multipart/alternative when there is one)
        email& set_text(const string& text) {
            detail::mail_restructure(_root, [&](detail::MailPieces& pc) { pc.text = detail::mail_text_leaf("plain", text.view()); });
            return *this;
        }

        email& set_html(const string& html) {
            detail::mail_restructure(_root, [&](detail::MailPieces& pc) { pc.html = detail::mail_text_leaf("html", html.view()); });
            return *this;
        }

        // The file at path as an attachment: its name the path's last
        // element, its type by its extension; the error of a file that
        // cannot be read, the message unchanged
        expected<void, io::error> attach(const string& path) {
            auto data = io::read_file(path);
            if (!data) {
                return io::detail::fail(data);
            }
            attach(io::path::base(path), *data);
            return {};
        }

        // Bytes as an attachment of the name, of the type given or, when
        // it is empty, the type of the name's extension
        email& attach(const string& filename, const slice<const byte>& data, const string& content_type = string()) {
            auto leaf = _leaf_of(filename, data, content_type);
            leaf->fields.push_back(detail::MailField{string("Content-Disposition"), detail::mail_disposition_value("attachment", filename.view()), true});
            detail::mail_restructure(_root, [&](detail::MailPieces& pc) { pc.attachments.push_back(leaf); });
            return *this;
        }

        // A part the HTML shows by its Content-ID ("cid:..." in an src):
        // the file at path, or bytes of a name; the id it got
        expected<string, io::error> embed(const string& path) {
            auto data = io::read_file(path);
            if (!data) {
                return io::detail::fail(data);
            }
            return embed(io::path::base(path), *data);
        }

        string embed(const string& filename, const slice<const byte>& data, const string& content_type = string()) {
            auto leaf = _leaf_of(filename, data, content_type);
            std::string id;
            detail::mail_random(id, 16);
            id += '@';
            id += detail::mail_domain_of(*_root);
            leaf->fields.push_back(detail::MailField{string("Content-Disposition"), detail::mail_disposition_value("inline", filename.view()), true});
            leaf->fields.push_back(detail::MailField{string("Content-ID"), detail::mail_raw("<" + id + ">"), true});
            detail::mail_restructure(_root, [&](detail::MailPieces& pc) { pc.related.push_back(leaf); });
            return string(id);
        }

        // The attachments, in their order: the parts a reader of mail
        // offers to save (message/rfc822 parts among them)
        vector<part> attachments() const;

        // The body as a part: the root of the MIME tree, whose head is the
        // message's own
        part body() const;

        // A body of a program's own shape (multipart/signed, a calendar's
        // text/calendar): the part's content fields and content become the
        // message's, the message's other fields kept
        email& set_body(const part& p);

        // ---- writing

        // The message as text: CRLF line breaks, the head folded, MIME
        // version 1.0
        string to_string() const;
        string to_string(const write_options& o) const;

        // The same to a writer
        expected<void, io::error> write_to(const io::writer& w) const;

        expected<void, io::error> write_to(const io::writer& w, const write_options& o) const {
            auto r = w.write(to_string(o));
            if (!r) {
                return io::detail::fail(r);
            }
            return {};
        }

        // Into a file (an .eml), made or written over
        expected<void, error> save(const string& path) const;

        expected<void, error> save(const string& path, const write_options& o) const {
            if (auto w = io::write_file(path, to_string(o)); !w) {
                return unexpected(detail::file_error(w.error()));
            }
            return {};
        }

        async::task<expected<void, error>> async_save(string path) const noexcept;
        async::task<expected<void, error>> async_save(string path, write_options o) const noexcept;

        // ---- parsing

        static expected<email, error> parse(const string& text);
        static expected<email, error> parse(const string& text, const limits& l);
        static expected<email, error> parse(const slice<const byte>& data);
        static expected<email, error> parse(const slice<const byte>& data, const limits& l);
        // A literal, a character array, a std::string_view: read where it
        // lies (an exact match, else the conversions to a string and to
        // bytes tie)
        template<sgcl::detail::TextArgument T>
        static expected<email, error> parse(const T& text);
        template<sgcl::detail::TextArgument T>
        static expected<email, error> parse(const T& text, const limits& l);
        static expected<email, error> parse(const io::reader& in);
        static expected<email, error> parse(const io::reader& in, const limits& l);
        static expected<email, error> load(const string& path);
        static expected<email, error> load(const string& path, const limits& l);
        static async::task<expected<email, error>> async_load(string path) noexcept;
        static async::task<expected<email, error>> async_load(string path, limits l) noexcept;

        // The same message: the same object
        friend bool operator==(const email& a, const email& b) noexcept {
            return a._root == b._root;
        }

    private:
        friend struct detail::MailAccess;

        explicit email(tracked_ptr<detail::MailPart> root) noexcept
        : _root(std::move(root)) {
        }

        void _init() noexcept {
            _root->fields.push_back(detail::MailField{string("Date"), time::now().format(time::email), false});
            _root->fields.push_back(detail::MailField{string("Message-ID"), detail::mail_new_id(*_root), false});
            _root->generated_id = true;
            _root->fields.push_back(detail::MailField{string("Content-Type"), detail::mail_raw("text/plain; charset=utf-8"), true});
        }

        // A From set: a Message-ID the constructor made takes its domain
        void _from_changed() {
            if (_root->generated_id) {
                detail::mail_set(*_root, string("Message-ID"), detail::mail_new_id(*_root), false);
            }
        }

        static tracked_ptr<detail::MailPart> _leaf_of(const string& filename, const slice<const byte>& data, const string& content_type) {
            std::string type = content_type.empty() ? std::string(detail::content_type_of(filename.view())) : std::string(content_type.view());
            return detail::mail_leaf(type, detail::mail_bytes(std::string_view(reinterpret_cast<const char*>(data.data()), data.size())));
        }

        email& _add_addresses(const char* name, const string& list);
        email& _add_address(const char* name, const address& a);
        vector<address> _addresses(std::string_view name) const;

        tracked_ptr<detail::MailPart> _root;
    };

    // An address of mail (RFC 5322 §3.4): a display name and an
    // addr-spec, "Alice Doe <alice@example.com>". A value; the text form
    // quotes the name as it needs and writes one past ASCII as encoded
    // words, as Go's mail.Address writes it.
    class email::address {
    public:
        address() noexcept = default;

        // An address read from text, "Alice <alice@example.com>" or
        // "alice@example.com"; one that is not an address is
        // invalid_argument (parse returns the error)
        explicit address(const string& text) {
            auto a = parse(text);
            if (!a) {
                throw invalid_argument("sgcl::encoding::email::address: not an address");
            }
            *this = std::move(*a);
        }

        // The name (any text, "" for none) and the addr-spec as given
        address(const string& name, const string& addr) noexcept
        : _name(name), _addr(addr) {
        }

        // The display name, decoded; "" when there is none
        const string& name() const noexcept {
            return _name;
        }

        // "alice@example.com"
        const string& addr() const noexcept {
            return _addr;
        }

        // The addr-spec's part before the last '@', and after it
        string local_part() const {
            auto at = _addr.view().rfind('@');
            return at == std::string_view::npos ? _addr : string(_addr.view().substr(0, at));
        }

        string domain() const {
            auto at = _addr.view().rfind('@');
            return at == std::string_view::npos ? string() : string(_addr.view().substr(at + 1));
        }

        // The address as a field writes it: "Alice <alice@example.com>",
        // "\"Doe, Alice\" <alice@example.com>", "=?utf-8?q?=C5=81ucja?=
        // <l@example.com>", or the addr-spec alone when there is no name
        string to_string() const {
            std::string out;
            detail::mail_write_mailbox(_name.view(), _addr.view(), false, out);
            return string(out);
        }

        // One address; the error says where the text stops being one
        static expected<address, error> parse(const string& text) {
            std::vector<detail::MailMailbox> list;
            size_t at = 0;
            if (!detail::mail_address_list(text.view(), list, &at) || list.size() != 1) {
                if (list.size() > 1) {
                    return unexpected(error(errc::syntax, 0, "more than one address"));
                }
                const bool blank = detail::mail_trim(text.view()).empty();
                return unexpected(error(errc::syntax, blank ? text.size() : at, blank ? "no address" : "invalid address"));
            }
            return address(string(list.front().name), string(list.front().addr));
        }

        // An address list, groups' members in their place ("Team: a@x,
        // b@y;"), empty groups and elements skipped
        static expected<vector<address>, error> parse_list(const string& text) {
            std::vector<detail::MailMailbox> list;
            size_t at = 0;
            if (!detail::mail_address_list(text.view(), list, &at)) {
                return unexpected(error(errc::syntax, at, "invalid address"));
            }
            vector<address> out;
            for (auto& m : list) {
                out.push_back(address(string(m.name), string(m.addr)));
            }
            return out;
        }

        friend bool operator==(const address& a, const address& b) noexcept {
            return a._name == b._name && a._addr == b._addr;
        }

    private:
        string _name;
        string _addr;
    };

    // A part of a message's MIME tree (RFC 2045, RFC 2046): its head, and
    // its content or its parts (a multipart) or the message it holds
    // (message/rfc822). A handle, as email is: a copy is the same part,
    // and a part taken from a message is that message's.
    class email::part {
    public:
        // A part of text, of the type given ("text/plain", "text/calendar;
        // method=REQUEST"; charset=utf-8 added to a text type that has no
        // charset), or of bytes; a type of multipart ("multipart/mixed")
        // makes an empty multipart, its parts added by add()
        part(const string& content_type, const string& text)
        : _p(_make(content_type, text.view(), true)) {
        }

        part(const string& content_type, const slice<const byte>& data)
        : _p(_make(content_type, std::string_view(reinterpret_cast<const char*>(data.data()), data.size()), false)) {
        }

        explicit part(const string& content_type)
        : _p(_make(content_type, std::string_view(), false)) {
        }

        template<sgcl::detail::TextArgument T>
        part(const string& content_type, const T& text)
        : part(content_type, string(text)) {
        }

        // "text/plain", lower case, without the parameters
        string content_type() const {
            return string(detail::mail_type(*_p).type);
        }

        // A parameter of Content-Type ("charset", "boundary", "name"),
        // decoded; "" when there is none
        string param(const string& name) const {
            auto t = detail::mail_type(*_p);
            if (auto v = t.param(detail::mail_lowered(name.view()))) {
                return string(*v);
            }
            return string();
        }

        string charset() const {
            return param("charset");
        }

        // The name of the file: Content-Disposition's filename (RFC 2231
        // decoded), else Content-Type's name; "" when there is none
        string filename() const {
            return string(detail::mail_filename(*_p));
        }

        // "attachment", "inline", or "" when the part has no disposition
        string disposition() const {
            return string(detail::mail_disposition(*_p));
        }

        // The Content-ID without its angle brackets
        string content_id() const {
            if (auto f = detail::mail_find(*_p, "content-id")) {
                std::string v = detail::mail_field_structured(*f);
                if (v.size() >= 2 && v.front() == '<' && v.back() == '>') {
                    v = v.substr(1, v.size() - 2);
                }
                return string(v);
            }
            return string();
        }

        bool is_multipart() const {
            return detail::mail_is_multipart(*_p);
        }

        bool is_attachment() const {
            return detail::mail_is_attachment(*_p);
        }

        // The parts of a multipart, in their order
        vector<part> parts() const {
            vector<part> out;
            for (auto& c : _p->parts) {
                out.push_back(part(c));
            }
            return out;
        }

        // A part after the others of a multipart
        part& add(const part& p) {
            _p->parts.push_back(p._p);
            return *this;
        }

        // The content, its transfer encoding undone (a message/rfc822's
        // is the message's text)
        vector<byte> content() const;

        // The content as text in UTF-8: its charset converted (bytes in a
        // charset nobody here converts come as they are: charset() names
        // it), line breaks LF
        string text() const {
            return string(detail::mail_text_of(*_p));
        }

        // The message a message/rfc822 part holds
        optional<email> message() const {
            if (_p->message) {
                return email(_p->message);
            }
            return nullopt;
        }

        // ---- the head, as email's

        string header(const string& name) const {
            if (auto f = detail::mail_find(*_p, name.view())) {
                return string(detail::mail_field_text(*f));
            }
            return string();
        }

        vector<string> header_all(const string& name) const {
            vector<string> out;
            for (auto& f : _p->fields) {
                if (detail::mail_iequal(f.name.view(), name.view())) {
                    out.push_back(string(detail::mail_field_text(f)));
                }
            }
            return out;
        }

        bool has_header(const string& name) const noexcept {
            return detail::mail_find(*_p, name.view()) != nullptr;
        }

        vector<pair<string, string>> headers() const {
            vector<pair<string, string>> out;
            for (auto& f : _p->fields) {
                out.push_back(pair<string, string>(f.name, string(detail::mail_field_text(f))));
            }
            return out;
        }

        part& set_header(const string& name, const string& value) {
            detail::mail_check_name(name);
            detail::mail_set(*_p, name, value, false);
            return *this;
        }

        part& add_header(const string& name, const string& value) {
            detail::mail_check_name(name);
            _p->fields.push_back(detail::MailField{name, value, false});
            return *this;
        }

        part& remove_header(const string& name) {
            detail::mail_remove(*_p, name.view());
            return *this;
        }

        // Content-Disposition: attachment with the file's name (RFC 2231
        // when it is not ASCII)
        part& set_filename(const string& name) {
            std::string d = detail::mail_disposition(*_p);
            detail::mail_set(*_p, string("Content-Disposition"), detail::mail_disposition_value(d.empty() ? "attachment" : d, name.view()), true);
            return *this;
        }

        // Content-ID: <id> and Content-Disposition: inline, for a part an
        // HTML shows by "cid:id"
        part& set_content_id(const string& id) {
            std::string v(detail::mail_trim(id.view()));
            if (v.size() >= 2 && v.front() == '<' && v.back() == '>') {
                v = v.substr(1, v.size() - 2);
            }
            detail::mail_set(*_p, string("Content-ID"), detail::mail_raw("<" + v + ">"), true);
            std::string name = detail::mail_filename(*_p);
            detail::mail_set(*_p, string("Content-Disposition"), detail::mail_disposition_value("inline", name), true);
            return *this;
        }

        // The same part: the same object
        friend bool operator==(const part& a, const part& b) noexcept {
            return a._p == b._p;
        }

    private:
        friend class email;
        friend struct detail::MailAccess;

        explicit part(tracked_ptr<detail::MailPart> p) noexcept
        : _p(std::move(p)) {
        }

        static tracked_ptr<detail::MailPart> _make(const string& content_type, std::string_view content, bool text) {
            std::vector<detail::MailParam> params;
            std::string type = detail::mail_parse_params(content_type.view(), params);
            if (type.find('/') == std::string::npos) {
                throw invalid_argument("sgcl::encoding::email::part: not a media type");
            }
            std::string value = type;
            bool charset = false;
            for (auto& p : params) {
                detail::mail_write_param(value, p.name, p.value);
                charset |= p.name == "charset";
            }
            if (type.starts_with("text/") && !charset) {
                value += "; charset=utf-8";
            }
            tracked_ptr p = make_tracked<detail::MailPart>();
            p->fields.push_back(detail::MailField{string("Content-Type"), detail::mail_raw(value), true});
            if (!type.starts_with("multipart/")) {
                p->content = text && type.starts_with("text/") ? detail::mail_crlf(content) : detail::mail_bytes(content);
            }
            return p;
        }

        tracked_ptr<detail::MailPart> _p;
    };
}

namespace sgcl::encoding::detail {
    struct MailAccess {
        SGCL_INLINE_HOT static const tracked_ptr<MailPart>& root(const email& m) noexcept {
            return m._root;
        }

        SGCL_INLINE_HOT static email make(tracked_ptr<MailPart> root) noexcept {
            return email(std::move(root));
        }

        SGCL_INLINE_HOT static const tracked_ptr<MailPart>& node(const email::part& p) noexcept {
            return p._p;
        }

        SGCL_INLINE_HOT static email::part part(tracked_ptr<MailPart> p) noexcept {
            return email::part(std::move(p));
        }
    };

    // ---- writing

    enum class MailCte : uint8_t { seven, eight, qp, base64 };

    struct MailWriter {
        std::string& out;
        const email::write_options& o;

        void field(const MailField& f) {
            const std::string_view name = f.name.view();
            if (f.raw) {
                out.append(name);
                out += ':';
                out.append(f.value.view());
                out += "\r\n";
                return;
            }
            std::string_view v = f.value.view();
            if (mail_address_field(name)) {
                std::vector<MailMailbox> list;
                if (mail_address_list(v, list, nullptr) && !list.empty()) {
                    std::string value;
                    for (size_t i = 0; i < list.size(); ++i) {
                        if (i) {
                            value += ", ";
                        }
                        mail_write_mailbox(list[i].name, list[i].addr, o.allow_utf8, value);
                    }
                    mail_fold_field(out, name, value);
                    return;
                }
            }
            if (mail_verbatim_field(name)) {
                std::string clean;
                for (char c : v) {
                    clean += (c == '\r' || c == '\n') ? ' ' : c;
                }
                mail_fold_field(out, name, clean, false);
                return;
            }
            mail_fold_field(out, name, mail_encode_unstructured(v, o.allow_utf8), false);
        }

        // The transfer encoding of a leaf: the one it came in when that
        // is quoted-printable or base64, else the lightest its content and
        // the transport allow
        MailCte choose(const MailPart& p, const MailType& t) {
            std::string orig;
            if (auto f = mail_find(p, "content-transfer-encoding")) {
                orig = mail_lowered(mail_field_structured(*f));
            }
            if (orig == "base64") {
                return MailCte::base64;
            }
            if (orig == "quoted-printable") {
                return MailCte::qp;
            }
            const bool text = t.type.starts_with("text/") || (t.type.starts_with("message/") && !p.message);
            if (!text && orig.empty() && !p.content.empty() && !mail_find(p, "content-transfer-encoding")) {
                // a program's binary part: scanned only for the 7bit of a small ASCII file
                if (p.content.size() > 4096) {
                    return MailCte::base64;
                }
            }
            size_t high = 0, controls = 0, bare = 0, line = 0, longest = 0, escapes = 0;
            const auto* d = reinterpret_cast<const uint8_t*>(p.content.data());
            const size_t n = p.content.size();
            for (size_t i = 0; i < n; ++i) {
                uint8_t c = d[i];
                if (c == '\r') {
                    if (i + 1 < n && d[i + 1] == '\n') {
                        ++i;
                    } else {
                        ++bare;
                    }
                    longest = std::max(longest, line);
                    line = 0;
                    continue;
                }
                if (c == '\n') {
                    ++bare;
                    longest = std::max(longest, line);
                    line = 0;
                    continue;
                }
                ++line;
                if (c >= 0x80) {
                    ++high;
                    ++escapes;
                } else if ((c < 0x20 && c != '\t') || c == 0x7F) {
                    ++controls;
                    ++escapes;
                } else if (c == '=') {
                    ++escapes;
                }
            }
            longest = std::max(longest, line);
            const bool lines_ok = longest <= 998 && controls == 0 && (text || bare == 0);
            if (high == 0 && lines_ok) {
                return MailCte::seven;
            }
            if (o.allow_8bit && lines_ok) {
                return MailCte::eight;
            }
            if (!text) {
                return MailCte::base64;
            }
            return escapes * 6 <= n ? MailCte::qp : MailCte::base64;
        }

        // Text with its line breaks CRLF, as 7bit and 8bit must have them
        void lines(std::string_view s) {
            size_t from = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                char c = s[i];
                if (c == '\r') {
                    if (i + 1 < s.size() && s[i + 1] == '\n') {
                        ++i;
                        continue;
                    }
                    out.append(s.substr(from, i - from));
                    out += "\r\n";
                    from = i + 1;
                } else if (c == '\n') {
                    out.append(s.substr(from, i - from));
                    out += "\r\n";
                    from = i + 1;
                }
            }
            out.append(s.substr(from));
        }

        // Base64 in lines of 76, each ended by CRLF
        void base64_lines(std::string_view s) {
            const size_t whole = s.size() / 57;
            const size_t rest = s.size() % 57;
            const size_t chars = whole * 78 + (rest ? (rest + 2) / 3 * 4 + 2 : 0);
            size_t at = out.size();
            out.resize(at + chars);
            char* o = out.data() + at;
            const char* p = s.data();
            for (size_t k = 0; k < whole; ++k) {
                base64::standard.encode_to(slice<char>(o, 76), slice<const byte>(reinterpret_cast<const byte*>(p), 57));
                o[76] = '\r';
                o[77] = '\n';
                o += 78;
                p += 57;
            }
            if (rest) {
                size_t m = base64::standard.encode_to(slice<char>(o, (rest + 2) / 3 * 4), slice<const byte>(reinterpret_cast<const byte*>(p), rest));
                o[m] = '\r';
                o[m + 1] = '\n';
            }
        }

        // The head of an entity, without the fields the writer sets
        // itself; whether a Content-Transfer-Encoding field was seen (its
        // place is where the writer's goes)
        void head(const MailPart& p, bool top, bool skip_type, const char* cte) {
            bool cte_written = false;
            bool mime_version = !top || mail_find(p, "mime-version");
            for (auto& f : p.fields) {
                const std::string_view n = f.name.view();
                if (!mime_version && n.size() >= 8 && mail_iequal(n.substr(0, 8), "content-")) {
                    out += "MIME-Version: 1.0\r\n";   // before the content fields
                    mime_version = true;
                }
                if (mail_iequal(n, "content-transfer-encoding")) {
                    if (cte && !cte_written) {
                        out += "Content-Transfer-Encoding: ";
                        out += cte;
                        out += "\r\n";
                        cte_written = true;
                    }
                    continue;
                }
                if (skip_type && mail_iequal(n, "content-type")) {
                    continue;
                }
                if (!o.write_bcc && mail_iequal(n, "bcc")) {
                    continue;
                }
                field(f);
            }
            if (!mime_version) {
                out += "MIME-Version: 1.0\r\n";
            }
            if (cte && !cte_written) {
                out += "Content-Transfer-Encoding: ";
                out += cte;
                out += "\r\n";
            }
        }

        // How often a delimiter of the boundary starts a line in
        // out[from, end)
        size_t delimiters(size_t from, std::string_view b) const {
            std::string needle = "\n--";
            needle += b;
            size_t count = 0;
            std::string_view region(out.data() + from, out.size() - from);
            if (region.substr(0, 2 + b.size()) == needle.substr(1)) {
                ++count;
            }
            size_t at = 0;
            while ((at = region.find(needle, at)) != std::string_view::npos) {
                ++count;
                at += needle.size();
            }
            return count;
        }

        void entity(const MailPart& p, bool top, int depth) {
            MailType t = mail_type(p);
            if (!p.parts.empty() && depth < 200) {
                multipart(p, t, top, depth);
                return;
            }
            if (p.message && depth < 200) {
                head(p, top, false, nullptr);
                out += "\r\n";
                entity(*p.message, false, depth + 1);
                return;
            }
            MailCte cte = choose(p, t);
            static constexpr const char* Names[] = {"7bit", "8bit", "quoted-printable", "base64"};
            const bool text = t.type.starts_with("text/") || t.type.starts_with("message/");
            // 7bit, the default, is not written for a program's part (RFC 2045 §6.1)
            const char* name = Names[int(cte)];
            if (cte == MailCte::seven && !mail_find(p, "content-transfer-encoding")) {
                name = nullptr;
            }
            head(p, top, false, name);
            out += "\r\n";
            std::string_view content = mail_view(p.content);
            switch (cte) {
                case MailCte::seven:
                case MailCte::eight:
                    lines(content);
                    break;
                case MailCte::qp: {
                    QpEncoder e;
                    e.binary = !text;
                    e.feed(reinterpret_cast<const uint8_t*>(content.data()), content.size(), out);
                    e.finish(out);
                    break;
                }
                case MailCte::base64:
                    base64_lines(content);
                    break;
            }
        }

        void multipart(const MailPart& p, MailType& t, bool top, int depth) {
            const size_t start = out.size();
            const std::string* given = t.param("boundary");
            std::string boundary;
            bool rebuild = true;
            if (given && !given->empty() && given->size() <= 70) {
                boundary = *given;
                rebuild = false;
                if (auto f = mail_find(p, "content-type"); f && !f->raw) {
                    rebuild = true;
                }
            }
            for (int attempt = 0;; ++attempt) {
                if (boundary.empty() || attempt > 0) {
                    boundary = "=_";
                    mail_random(boundary, 28);
                    rebuild = true;
                }
                head(p, top, rebuild, nullptr);
                if (rebuild) {
                    std::string value = t.type;
                    for (auto& q : t.params) {
                        if (q.name != "boundary") {
                            mail_write_param(value, q.name, q.value);
                        }
                    }
                    value += "; boundary=\"";
                    value += boundary;
                    value += '"';
                    mail_fold_field(out, "Content-Type", value);
                }
                out += "\r\n";
                const size_t body = out.size();
                for (auto& c : p.parts) {
                    out += "--";
                    out += boundary;
                    out += "\r\n";
                    entity(*c, false, depth + 1);
                    out += "\r\n";
                }
                out += "--";
                out += boundary;
                out += "--\r\n";
                if (delimiters(body, boundary) == p.parts.size() + 1 || attempt >= 8) {
                    return;
                }
                out.resize(start);   // the boundary is in the content: another
            }
        }
    };

    // ---- parsing

    struct MailParser {
        const email::limits& l;
        size_t parts = 0;
        optional<error> failed;

        // One entity of data (which starts at offset of the whole input)
        tracked_ptr<MailPart> entity(std::string_view data, uint64_t offset, size_t depth, bool digest_child) {
            tracked_ptr p = make_tracked<MailPart>();
            p->default_rfc822 = digest_child;
            const std::string_view head_room = data.substr(0, std::min(data.size(), l.max_header_bytes + 1));
            size_t i = 0;
            if (depth == 0 && head_room.starts_with("From ") && head_room.find_first_not_of(" \t", 4) != std::string_view::npos
                && head_room[head_room.find_first_not_of(" \t", 4)] != ':') {
                // an mbox's separator line before the head
                size_t nl = head_room.find('\n');
                i = nl == std::string_view::npos ? head_room.size() : nl + 1;
            }
            size_t body = std::string_view::npos;
            size_t last = SIZE_MAX;
            while (i < head_room.size()) {
                size_t nl = head_room.find('\n', i);
                if (nl == std::string_view::npos) {
                    if (head_room.size() > l.max_header_bytes) {
                        failed.emplace(errc::limit_exceeded, offset + i, "header block too large");
                        return p;
                    }
                    nl = head_room.size();   // the last line, without its break
                }
                std::string_view line = head_room.substr(i, nl - i);
                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }
                if (line.empty()) {
                    body = std::min(nl + 1, data.size());
                    break;
                }
                if (mail_wsp(line.front())) {
                    if (last != SIZE_MAX) {
                        std::string v(p->fields[last].value.view());
                        v += "\r\n";
                        v += line;
                        p->fields[last].value = string(v);
                    }
                    i = nl + 1;
                    continue;
                }
                size_t colon = line.find(':');
                std::string_view name = colon == std::string_view::npos ? std::string_view() : line.substr(0, colon);
                while (!name.empty() && mail_wsp(name.back())) {
                    name.remove_suffix(1);   // obs-optional: "Subject :"
                }
                if (!mail_field_name_ok(name)) {
                    body = i;   // a line that is not a field: the body starts here
                    break;
                }
                p->fields.push_back(MailField{string(name), string(line.substr(colon + 1)), true});
                last = p->fields.size() - 1;
                i = nl + 1;
            }
            if (body == std::string_view::npos) {
                if (head_room.size() > l.max_header_bytes) {
                    failed.emplace(errc::limit_exceeded, offset + std::min(i, head_room.size()), "header block too large");
                    return p;
                }
                body = data.size();
            }
            std::string_view content = data.substr(std::min(body, data.size()));
            const uint64_t content_at = offset + body;
            MailType t = mail_type(*p);
            if (t.type.starts_with("multipart/")) {
                const std::string* b = t.param("boundary");
                if (b && !b->empty() && split(*p, content, content_at, *b, depth, t.type == "multipart/digest")) {
                    return p;
                }
                if (failed) {
                    return p;
                }
            }
            std::string cte;
            if (auto f = mail_find(*p, "content-transfer-encoding")) {
                cte = mail_lowered(mail_field_structured(*f));
            }
            if ((t.type == "message/rfc822" || t.type == "message/global") && cte != "base64" && cte != "quoted-printable") {
                if (depth + 1 > l.max_depth) {
                    failed.emplace(errc::depth_limit, content_at, "messages nested too deep");
                    return p;
                }
                p->message = entity(content, content_at, depth + 1, false);
                p->content = mail_bytes(content);
                return p;
            }
            if (cte == "base64") {
                std::string out;
                mail_base64(content, out);
                p->content = mail_bytes(out);
            } else if (cte == "quoted-printable") {
                QpDecoder d;
                d.lenient = true;
                std::string out;
                d.feed(reinterpret_cast<const uint8_t*>(content.data()), content.size(), out);
                d.finish(out);
                p->content = mail_bytes(out);
            } else {
                p->content = mail_bytes(content);
            }
            return p;
        }

        // A delimiter line at s[at]: "--boundary" then "--" for the close,
        // then white space to the line's end; the end of the line, or npos
        static size_t delimiter_end(std::string_view s, size_t at, std::string_view b, bool& close) noexcept {
            size_t k = at + 2 + b.size();
            close = false;
            if (s.substr(k, 2) == "--") {
                close = true;
                k += 2;
            }
            while (k < s.size() && mail_wsp(s[k])) {
                ++k;
            }
            if (k == s.size()) {
                return k;
            }
            if (s[k] == '\n') {
                return k + 1;
            }
            if (s[k] == '\r' && k + 1 < s.size() && s[k + 1] == '\n') {
                return k + 2;
            }
            if (s[k] == '\r' && k + 1 == s.size()) {
                return s.size();
            }
            return std::string_view::npos;
        }

        // The parts of a multipart body; false when it holds no delimiter
        bool split(MailPart& p, std::string_view s, uint64_t offset, std::string_view b, size_t depth, bool digest) {
            std::string needle = "--";
            needle += b;
            // the first delimiter: at the start or after a line break
            size_t at = std::string_view::npos;
            size_t line_end = 0;
            bool close = false;
            for (size_t from = 0;;) {
                size_t k = s.find(needle, from);
                if (k == std::string_view::npos) {
                    return false;
                }
                if (k == 0 || s[k - 1] == '\n') {
                    line_end = delimiter_end(s, k, b, close);
                    if (line_end != std::string_view::npos) {
                        at = k;
                        break;
                    }
                }
                from = k + 1;
            }
            (void)at;
            while (!close) {
                if (depth + 1 > l.max_depth) {
                    failed.emplace(errc::depth_limit, offset + line_end, "parts nested too deep");
                    return true;
                }
                if (++parts > l.max_parts) {
                    failed.emplace(errc::limit_exceeded, offset + line_end, "too many parts");
                    return true;
                }
                // the next delimiter; the line break before it is its own
                size_t start = line_end;
                size_t next = std::string_view::npos;
                size_t next_end = 0;
                bool next_close = false;
                for (size_t from = start;;) {
                    size_t k = s.find(needle, from);
                    if (k == std::string_view::npos) {
                        break;
                    }
                    if (k > 0 && s[k - 1] == '\n') {
                        size_t e = delimiter_end(s, k, b, next_close);
                        if (e != std::string_view::npos) {
                            next = k;
                            next_end = e;
                            break;
                        }
                    }
                    from = k + 1;
                }
                size_t stop = next == std::string_view::npos ? s.size() : next - 1;
                if (next != std::string_view::npos && stop > start && s[stop - 1] == '\r') {
                    --stop;
                }
                if (stop < start) {
                    stop = start;
                }
                auto child = entity(s.substr(start, stop - start), offset + start, depth + 1, digest);
                p.parts.push_back(child);
                if (failed) {
                    return true;
                }
                if (next == std::string_view::npos) {
                    break;   // no closing delimiter: the last part runs to the end
                }
                line_end = next_end;
                close = next_close;
            }
            return true;
        }
    };

    inline expected<email, error> mail_parse(std::string_view data, const email::limits& l) {
        MailParser parser{l};
        auto root = parser.entity(data, 0, 0, false);
        if (parser.failed) {
            return unexpected(*parser.failed);
        }
        return MailAccess::make(root);
    }

    inline async::task<expected<void, error>> mail_save_task(string path, email m, email::write_options o) noexcept {
        co_return co_await async::spawn_blocking([path, m, o] { return m.save(path, o); });
    }
}

namespace sgcl::encoding {
    inline optional<email::address> email::from() const {
        auto list = _addresses("from");
        if (list.empty()) {
            return nullopt;
        }
        return list.front();
    }

    inline vector<email::address> email::to() const {
        return _addresses("to");
    }

    inline vector<email::address> email::cc() const {
        return _addresses("cc");
    }

    inline vector<email::address> email::bcc() const {
        return _addresses("bcc");
    }

    inline vector<email::address> email::reply_to() const {
        return _addresses("reply-to");
    }

    inline vector<email::address> email::_addresses(std::string_view name) const {
        vector<address> out;
        for (auto& f : _root->fields) {
            if (!detail::mail_iequal(f.name.view(), name)) {
                continue;
            }
            std::vector<detail::MailMailbox> list;
            if (detail::mail_address_list(detail::mail_field_structured(f), list, nullptr)) {
                for (auto& m : list) {
                    out.push_back(address(string(m.name), string(m.addr)));
                }
            }
        }
        return out;
    }

    inline email& email::_add_address(const char* name, const address& a) {
        std::string text;
        detail::mail_write_mailbox(a.name().view(), a.addr().view(), true, text);
        if (auto f = detail::mail_find(*_root, name)) {
            std::string v = detail::mail_field_structured(*f);
            if (!v.empty()) {
                v += ", ";
            }
            v += text;
            detail::mail_set(*_root, string(name), string(v), false);
        } else {
            _root->fields.push_back(detail::MailField{string(name), string(text), false});
        }
        return *this;
    }

    inline email& email::_add_addresses(const char* name, const string& list) {
        auto parsed = address::parse_list(list);
        if (!parsed) {
            throw invalid_argument("sgcl::encoding::email: not an address list");
        }
        for (auto& a : *parsed) {
            _add_address(name, a);
        }
        return *this;
    }

    inline email& email::set_from(const address& a) {
        std::string text;
        detail::mail_write_mailbox(a.name().view(), a.addr().view(), true, text);
        detail::mail_set(*_root, string("From"), string(text), false);
        _from_changed();
        return *this;
    }

    inline email& email::set_from(const string& text) {
        return set_from(address(text));
    }

    inline email& email::add_to(const address& a) {
        return _add_address("To", a);
    }

    inline email& email::add_to(const string& list) {
        return _add_addresses("To", list);
    }

    inline email& email::add_cc(const address& a) {
        return _add_address("Cc", a);
    }

    inline email& email::add_cc(const string& list) {
        return _add_addresses("Cc", list);
    }

    inline email& email::add_bcc(const address& a) {
        return _add_address("Bcc", a);
    }

    inline email& email::add_bcc(const string& list) {
        return _add_addresses("Bcc", list);
    }

    inline email& email::add_reply_to(const address& a) {
        return _add_address("Reply-To", a);
    }

    inline email& email::add_reply_to(const string& list) {
        return _add_addresses("Reply-To", list);
    }

    inline vector<email::part> email::attachments() const {
        vector<tracked_ptr<detail::MailPart>> found;
        detail::mail_collect_attachments(_root, true, found);
        vector<part> out;
        for (auto& p : found) {
            out.push_back(part(p));
        }
        return out;
    }

    inline email::part email::body() const {
        return part(_root);
    }

    inline email& email::set_body(const part& p) {
        detail::MailPieces none;
        tracked_ptr<detail::MailPart> top = p._p;
        size_t kept = 0;
        for (size_t i = 0; i < _root->fields.size(); ++i) {
            auto& f = _root->fields[i];
            if (f.name.size() >= 8 && detail::mail_iequal(f.name.view().substr(0, 8), "content-")) {
                continue;
            }
            if (kept != i) {
                _root->fields[kept] = _root->fields[i];
            }
            ++kept;
        }
        _root->fields.resize(kept);
        if (top == _root) {
            return *this;
        }
        for (auto& f : top->fields) {
            _root->fields.push_back(f);
        }
        _root->content = top->content;
        _root->parts = top->parts;
        _root->message = top->message;
        return *this;
    }

    inline string email::to_string(const write_options& o) const {
        std::string out;
        size_t estimate = 512;
        for (auto& p : _root->parts) {
            estimate += p->content.size() * 4 / 3 + 256;
        }
        estimate += _root->content.size() * 4 / 3;
        out.reserve(estimate);
        detail::MailWriter w{out, o};
        w.entity(*_root, true, 0);
        return string(out);
    }

    inline async::task<expected<void, email::error>> email::async_save(string path, write_options o) const noexcept {
        return detail::mail_save_task(std::move(path), *this, o);
    }

    inline vector<byte> email::part::content() const {
        return _p->content;
    }

    inline expected<email, email::error> email::parse(const string& text, const limits& l) {
        return detail::mail_parse(text.view(), l);
    }

    inline expected<email, email::error> email::parse(const slice<const byte>& data, const limits& l) {
        return detail::mail_parse(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()), l);
    }

    inline expected<email, email::error> email::parse(const io::reader& in, const limits& l) {
        auto all = io::read_all(in);
        if (!all) {
            return unexpected(error(all.error(), 0));
        }
        return detail::mail_parse(detail::mail_view(*all), l);
    }

    inline expected<email, email::error> email::load(const string& path, const limits& l) {
        auto all = io::read_file(path);
        if (!all) {
            return unexpected(detail::file_error(all.error()));
        }
        return detail::mail_parse(detail::mail_view(*all), l);
    }

    inline async::task<expected<email, email::error>> email::async_load(string path, limits l) noexcept {
        co_return co_await async::spawn_blocking([path, l] { return email::load(path, l); });
    }

    inline string email::to_string() const {
        return to_string(write_options());
    }

    inline expected<void, io::error> email::write_to(const io::writer& w) const {
        return write_to(w, write_options());
    }

    inline expected<void, email::error> email::save(const string& path) const {
        return save(path, write_options());
    }

    inline async::task<expected<void, email::error>> email::async_save(string path) const noexcept {
        return detail::mail_save_task(std::move(path), *this, write_options());
    }

    inline expected<email, email::error> email::parse(const string& text) {
        return detail::mail_parse(text.view(), limits());
    }

    inline expected<email, email::error> email::parse(const slice<const byte>& data) {
        return parse(data, limits());
    }

    inline expected<email, email::error> email::parse(const io::reader& in) {
        return parse(in, limits());
    }

    inline expected<email, email::error> email::load(const string& path) {
        return load(path, limits());
    }

    inline async::task<expected<email, email::error>> email::async_load(string path) noexcept {
        return async_load(std::move(path), limits());
    }

    template<sgcl::detail::TextArgument T>
    expected<email, email::error> email::parse(const T& text) {
        const std::string_view v(text);
        return detail::mail_parse(v, limits());
    }

    template<sgcl::detail::TextArgument T>
    expected<email, email::error> email::parse(const T& text, const limits& l) {
        const std::string_view v(text);
        return detail::mail_parse(v, l);
    }
}
