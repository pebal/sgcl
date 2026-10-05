//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../io/error.h"

#include <string>
#include <system_error>

namespace sgcl::net::imap {
    // The failures of IMAP that neither errno nor io names: what a server
    // answered (a tagged NO or BAD, a BYE), told apart by the response
    // code it gave (RFC 9051 §7.1: NONEXISTENT, ALREADYEXISTS, OVERQUOTA
    // ...), a response that breaks the grammar, an authentication refused,
    // a STARTTLS the client requires and the server does not offer, a
    // capability a call needs and the server lacks. A server's backend
    // reports with the same codes, and the server answers with the
    // response code each stands for.
    enum class errc {
        no = 1,                  // a tagged NO without a code of its own
        bad,                     // a tagged BAD: the command not understood or not allowed now
        bye,                     // the server ended the connection (BYE)
        malformed_response,      // a response that breaks RFC 9051's grammar
        authentication_failed,   // the credentials refused
        starttls_unavailable,    // STARTTLS required and not offered
        not_supported,           // the server lacks the capability the call needs
        nonexistent,             // NONEXISTENT, TRYCREATE: no such mailbox
        already_exists,          // ALREADYEXISTS
        over_quota,              // OVERQUOTA
        too_big,                 // TOOBIG: a message or a literal past the server's limit
        cannot,                  // CANNOT: the operation can never succeed (a name the store refuses)
        limit,                   // LIMIT: past a limit of the server's (a command too long, too many connections)
        in_use,                  // INUSE: the mailbox is held by another
        no_perm,                 // NOPERM: not allowed to the user
        expunged,                // EXPUNGEISSUE: a message asked for was expunged
        unavailable,             // UNAVAILABLE: the server cannot now (the store failed)
        privacy_required,        // PRIVACYREQUIRED, a LOGINDISABLED server: TLS before the credentials
    };

    namespace detail {
        class ImapCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "imap";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::no: return "the server refused the command";
                    case errc::bad: return "the server rejected the command as invalid";
                    case errc::bye: return "the server closed the connection";
                    case errc::malformed_response: return "malformed IMAP response";
                    case errc::authentication_failed: return "authentication failed";
                    case errc::starttls_unavailable: return "the server does not offer STARTTLS";
                    case errc::not_supported: return "the server does not support the operation";
                    case errc::nonexistent: return "no such mailbox";
                    case errc::already_exists: return "the mailbox already exists";
                    case errc::over_quota: return "over quota";
                    case errc::too_big: return "too big";
                    case errc::cannot: return "the operation cannot succeed";
                    case errc::limit: return "past a limit of the server";
                    case errc::in_use: return "the mailbox is in use";
                    case errc::no_perm: return "permission denied";
                    case errc::expunged: return "the message was expunged";
                    case errc::unavailable: return "the server is unavailable";
                    case errc::privacy_required: return "TLS required before the credentials";
                }
                return "unknown imap error";
            }
        };
    }

    // The category of errc, named "imap"
    inline const std::error_category& category() noexcept {
        static const detail::ImapCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::imap::errc> : std::true_type {};

namespace sgcl::net::imap::detail {
    inline io::error imap_error(errc e, const string& op, const string& what = {}) noexcept {
        return io::error(make_error_code(e), op, what);
    }

    // The code a response code stands for (RFC 9051 §7.1, RFC 5530), for
    // a tagged NO; errc::no for none or one of no meaning here
    inline errc code_of(std::string_view rc, bool bad) noexcept {
        auto is = [&](std::string_view s) {
            if (rc.size() != s.size()) {
                return false;
            }
            for (size_t i = 0; i < s.size(); ++i) {
                char c = rc[i];
                if (c >= 'a' && c <= 'z') {
                    c = char(c - 32);
                }
                if (c != s[i]) {
                    return false;
                }
            }
            return true;
        };
        if (is("NONEXISTENT") || is("TRYCREATE")) {
            return errc::nonexistent;
        }
        if (is("ALREADYEXISTS")) {
            return errc::already_exists;
        }
        if (is("OVERQUOTA")) {
            return errc::over_quota;
        }
        if (is("TOOBIG")) {
            return errc::too_big;
        }
        if (is("CANNOT")) {
            return errc::cannot;
        }
        if (is("LIMIT")) {
            return errc::limit;
        }
        if (is("INUSE")) {
            return errc::in_use;
        }
        if (is("NOPERM")) {
            return errc::no_perm;
        }
        if (is("EXPUNGEISSUE")) {
            return errc::expunged;
        }
        if (is("UNAVAILABLE")) {
            return errc::unavailable;
        }
        if (is("AUTHENTICATIONFAILED") || is("AUTHORIZATIONFAILED") || is("EXPIRED")) {
            return errc::authentication_failed;
        }
        if (is("PRIVACYREQUIRED")) {
            return errc::privacy_required;
        }
        return bad ? errc::bad : errc::no;
    }

    // The response code a server answers an error of its backend with,
    // nullptr for one without
    inline const char* response_code_of(const io::error& e) noexcept {
        if (e.code().category() != category()) {
            if (e.code() == std::errc::no_space_on_device || e.code() == std::errc::file_too_large) {
                return "OVERQUOTA";
            }
            if (e.is_not_found()) {
                return "NONEXISTENT";
            }
            if (e.is_exists()) {
                return "ALREADYEXISTS";
            }
            if (e.is_permission()) {
                return "NOPERM";
            }
            return "SERVERBUG";
        }
        switch (static_cast<errc>(e.code().value())) {
            case errc::nonexistent: return "NONEXISTENT";
            case errc::already_exists: return "ALREADYEXISTS";
            case errc::over_quota: return "OVERQUOTA";
            case errc::too_big: return "TOOBIG";
            case errc::cannot: return "CANNOT";
            case errc::limit: return "LIMIT";
            case errc::in_use: return "INUSE";
            case errc::no_perm: return "NOPERM";
            case errc::expunged: return "EXPUNGEISSUE";
            case errc::unavailable: return "UNAVAILABLE";
            case errc::authentication_failed: return "AUTHENTICATIONFAILED";
            case errc::privacy_required: return "PRIVACYREQUIRED";
            default: return nullptr;
        }
    }
}
