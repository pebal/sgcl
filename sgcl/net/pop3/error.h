//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../io/error.h"

#include <string>
#include <string_view>
#include <system_error>

namespace sgcl::net::pop3 {
    // The failures of POP3 that neither errno nor io names: what a server
    // answered (-ERR), told apart by the response code it gave (RFC 2449
    // §8, RFC 3206: IN-USE, LOGIN-DELAY, SYS/TEMP, SYS/PERM, AUTH), a reply
    // that breaks RFC 1939, an authentication refused, a STLS the client
    // requires and the server does not offer, a command the server lacks,
    // a message number that is not there
    enum class errc {
        err = 1,                 // a -ERR without a response code of its own
        malformed_response,      // a reply that breaks RFC 1939
        authentication_failed,   // the credentials refused ([AUTH] or none)
        starttls_unavailable,    // TLS required and STLS not offered
        not_supported,           // the server lacks what the call needs (TOP, UIDL, APOP, SASL PLAIN)
        in_use,                  // [IN-USE]: the maildrop is held by another session
        login_delay,             // [LOGIN-DELAY]: too soon after the last login
        sys_temp,                // [SYS/TEMP]: the server failed for now
        sys_perm,                // [SYS/PERM]: the server failed for good
        no_such_message          // a number not in the maildrop, or marked deleted
    };

    namespace detail {
        class Pop3Category
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "pop3";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::err: return "the server refused the command";
                    case errc::malformed_response: return "malformed POP3 response";
                    case errc::authentication_failed: return "authentication failed";
                    case errc::starttls_unavailable: return "the server does not offer STLS";
                    case errc::not_supported: return "the server does not support the command";
                    case errc::in_use: return "the maildrop is in use";
                    case errc::login_delay: return "logged in too soon";
                    case errc::sys_temp: return "temporary server failure";
                    case errc::sys_perm: return "permanent server failure";
                    case errc::no_such_message: return "no such message";
                }
                return "unknown pop3 error";
            }
        };
    }

    // The category of errc, named "pop3"
    inline const std::error_category& category() noexcept {
        static const detail::Pop3Category instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::pop3::errc> : std::true_type {};

namespace sgcl::net::pop3::detail {
    inline io::error pop3_error(errc e, const string& op, const string& what = {}) noexcept {
        return io::error(make_error_code(e), op, what);
    }

    // The code of a -ERR's text: its response code in brackets (RFC 2449
    // §8), else fallback
    inline errc code_of_reply(std::string_view text, errc fallback) noexcept {
        if (text.size() < 2 || text.front() != '[') {
            return fallback;
        }
        size_t end = text.find(']');
        if (end == std::string_view::npos) {
            return fallback;
        }
        std::string code;
        for (char c : text.substr(1, end - 1)) {
            code += char(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
        if (code == "IN-USE") {
            return errc::in_use;
        }
        if (code == "LOGIN-DELAY") {
            return errc::login_delay;
        }
        if (code == "SYS/TEMP") {
            return errc::sys_temp;
        }
        if (code == "SYS/PERM") {
            return errc::sys_perm;
        }
        if (code == "AUTH") {
            return errc::authentication_failed;
        }
        return fallback;
    }
}
