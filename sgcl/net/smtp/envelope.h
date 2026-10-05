//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../ip.h"
#include "../tls.h"
#include "../dns.h"
#include "../../async/stop_token.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstdint>
#include <string>
#include <string_view>

// What the client and the server of SMTP share: a reply, an envelope, what
// a send gives, the settings of a client
namespace sgcl::net::smtp {
    // A reply of an SMTP server (RFC 5321 §4.2): the code, the enhanced
    // status code of RFC 3463 when the server gives one ("5.1.1"), and the
    // text of its lines (each without the code and the enhanced code),
    // joined by "\n". A reply of code 0 stands for none: a handler's or
    // a callback's "go on".
    struct reply {
        int code = 0;
        string enhanced;
        string text;

        // "550 5.1.1 No such user", lines after the first joined by "; "
        string to_string() const {
            std::string out = std::to_string(code);
            if (!enhanced.empty()) {
                out += ' ';
                out += enhanced.view();
            }
            if (!text.empty()) {
                out += ' ';
                for (char c : text.view()) {
                    if (c == '\n') {
                        out += "; ";
                    } else {
                        out += c;
                    }
                }
            }
            return string(out);
        }

        bool positive() const noexcept {
            return code >= 200 && code < 400;
        }

        friend bool operator==(const reply&, const reply&) noexcept = default;
    };

    // The envelope of a message (RFC 5321 §2.3.1): the reverse path, the
    // recipients and the parameters of MAIL and RCPT. A client sends it
    // (from, to and the DSN fields of RFC 3461); a server fills all of it
    // for its handler, with what it learned of the session. One type on
    // both sides, so that what a server received a client can pass on.
    struct envelope {
        string from;              // MAIL FROM's address; "" for the null path <> (a bounce)
        vector<string> to;        // RCPT TO's addresses, in their order
        string ret;               // DSN: "FULL" or "HDRS"; "" for none
        string envid;             // DSN: the envelope's id; "" for none
        vector<string> notify;    // DSN: per recipient of to ("SUCCESS,FAILURE,DELAY", "NEVER"); one value for all of them
        vector<string> orcpt;     // DSN: per recipient of to, the original recipient ("rfc822;bob@example.org")
        // what a server fills
        uint64_t size = 0;        // MAIL's SIZE (RFC 1870); 0 when none was declared
        string body;              // MAIL's BODY: "7BIT", "8BITMIME", "BINARYMIME", or ""
        bool smtputf8 = false;    // MAIL's SMTPUTF8 (RFC 6531)
        endpoint client;          // the client's address
        string helo;              // the name of its EHLO or HELO
        bool tls = false;         // whether the session runs over TLS
        string user;              // the user AUTH authenticated; "" for none
    };

    // A recipient the server refused, with its reply
    struct rejection {
        string recipient;
        smtp::reply reply;
    };

    // What a message's sending gives: the server's reply to its end
    // ("250 2.0.0 Ok: queued as 4ZCq1x"), and the recipients it refused
    // while it took the others
    struct receipt {
        smtp::reply reply;
        vector<rejection> rejected;
    };

    // How a client talks to its server. The credentials come from here or
    // from the URL's user and password; XOAUTH2 takes oauth_token in place
    // of the password. TLS: smtps:// is TLS from the first byte (RFC 8314),
    // smtp:// upgrades by STARTTLS when the server offers it (RFC 3207),
    // and refuses to go on in clear text when require_tls is set; AUTH is
    // never sent in clear text unless allow_insecure_auth says so. The
    // waits are those of RFC 5321 §4.5.3.2 (5 minutes for the greeting,
    // MAIL and RCPT, 2 for DATA, 3 for each block of data, 10 for the
    // end of it) unless timeout gives one for all of them.
    struct options {
        string username;
        string password;
        string oauth_token;                    // XOAUTH2 (RFC 7628's bearer token), with username
        string auth;                           // the mechanism to use ("PLAIN", "LOGIN", "XOAUTH2"); empty: the best one offered
        string hostname;                       // the name EHLO gives; empty: this host's name, or its address as a literal
        tls::config tls;                       // the roots, the client certificate, the session cache; the server's name from the URL
        bool starttls = true;                  // upgrade a plain connection when the server offers STARTTLS
        bool require_tls = false;              // fail (smtp_tls_required) rather than go on in clear text
        bool allow_insecure_auth = false;      // AUTH over a connection without TLS
        duration connect_timeout = 30 * second;
        duration timeout = duration::zero();   // every wait for the server; zero: RFC 5321's own
        uint16_t port = 0;                     // deliver's port of the exchangers; zero: 25
        net::dns::options dns;                 // deliver's resolver of MX records
        async::stop_token stop;
    };

    // The reply an error carries, for an error of the codes that come from
    // a reply (smtp_reply, smtp_auth_failed); nullopt for any other error
    inline optional<reply> reply_of(const io::error& e) noexcept {
        if (e.code() != errc::smtp_reply && e.code() != errc::smtp_auth_failed) {
            return nullopt;
        }
        std::string_view p = e.path().view();
        if (p.size() < 3 || p[0] < '2' || p[0] > '5' || p[1] < '0' || p[1] > '9' || p[2] < '0' || p[2] > '9') {
            return nullopt;
        }
        reply r;
        r.code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
        p.remove_prefix(3);
        if (!p.empty() && p.front() == ' ') {
            p.remove_prefix(1);
        }
        // "5.1.1 " when it is there
        size_t sp = p.find(' ');
        std::string_view first = p.substr(0, sp);
        auto digits = [](std::string_view s) {
            if (s.empty() || s.size() > 3) {
                return false;
            }
            for (char c : s) {
                if (c < '0' || c > '9') {
                    return false;
                }
            }
            return true;
        };
        size_t d1 = first.find('.');
        size_t d2 = d1 == std::string_view::npos ? std::string_view::npos : first.find('.', d1 + 1);
        if (d2 != std::string_view::npos && first.size() >= 5 && first[0] == p[0] && first[0] - '0' == r.code / 100 && digits(first.substr(0, d1))
            && digits(first.substr(d1 + 1, d2 - d1 - 1)) && digits(first.substr(d2 + 1))) {
            r.enhanced = string(first);
            p = sp == std::string_view::npos ? std::string_view() : p.substr(sp + 1);
        }
        std::string text;
        for (size_t i = 0; i < p.size(); ++i) {
            if (p[i] == ';' && i + 1 < p.size() && p[i + 1] == ' ') {
                text += '\n';
                ++i;
            } else {
                text += p[i];
            }
        }
        r.text = string(text);
        return r;
    }
}
