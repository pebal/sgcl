//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../io/error.h"

#include <cerrno>
#include <netdb.h>
#include <string>
#include <system_error>

namespace sgcl::net {
    // The failures of net that neither errno nor io names: an address
    // that cannot be taken apart ("host:port" without a port, a port past
    // 65535, a path too long for a unix socket), a name the resolver does
    // not know, a name that resolved to nothing a socket of the kind asked
    // for can use. They form the net category beside the system one and
    // io's; everything the module reports is an io::error, whose code is
    // in one of those, or in the lookup category below. The rest are
    // HTTP's: a URL that is not one or of a scheme the client cannot
    // speak (https before TLS), a response that breaks RFC 9112, a head
    // or a body past its limit, a chain of redirects too long, a server
    // asked to serve after its shutdown, a Set-Cookie value that holds no
    // cookie. Then the DNS resolver's (dns::lookup_mx and the other
    // records): a name with no record of the type asked (NODATA, RFC 2308),
    // a server's SERVFAIL, and any other refusal of a server or an answer
    // that cannot be read. Then a proxy's (SOCKS5, an HTTP proxy's
    // CONNECT): a failure the proxy reports without a cause errno names, a
    // connection it refused, a request it cannot carry out, credentials it
    // wants or refused, an answer that breaks its protocol; then a
    // multipart body's: one that breaks RFC 2046, one of more parts than
    // allowed, a request whose Content-Type is not multipart; then a
    // WebSocket's: an opening handshake refused or answered wrong, a frame
    // or a message that breaks RFC 6455, a connection the peer closed;
    // then SSH's: a handshake that failed (no common algorithm, a host
    // key's signature that does not verify), a host key known_hosts does
    // not have, one it has another key for, one it revokes, an
    // authentication refused, a message that breaks the protocol, a
    // connection the peer ended, a channel the peer would not open, a
    // request it refused;
    // and SMTP's: a reply refusing a command (the reply in the error's
    // path, smtp::reply_of reads it back), an authentication refused, TLS
    // required where the server offers none, a need the server's
    // extensions do not meet (SMTPUTF8, a size past SIZE, a mechanism of
    // AUTH), and a reply that breaks RFC 5321; then SFTP's: a server's
    // failure that names no errno (SSH_FX_FAILURE, its message in the
    // error's path), and a packet that breaks the protocol; then mail
    // authentication's: a message DKIM cannot sign (no head, no From) or
    // an Authentication-Results field that does not parse, and a DMARC
    // record or aggregate report that breaks RFC 7489.
    enum class errc {
        invalid_address = 1,
        host_not_found,
        no_suitable_address,
        invalid_url,
        unsupported_scheme,
        malformed_response,
        header_too_large,
        body_too_large,
        too_many_redirects,
        server_closed,
        invalid_cookie,
        http_status,
        no_data,
        server_failure,
        server_misbehaving,
        proxy_failure,
        proxy_refused,
        proxy_unsupported,
        proxy_auth_required,
        malformed_proxy_response,
        malformed_multipart,
        too_many_parts,
        not_multipart,
        websocket_handshake,
        websocket_protocol,
        websocket_closed,
        ssh_handshake,
        ssh_host_key_unknown,
        ssh_host_key_mismatch,
        ssh_host_key_revoked,
        ssh_auth_failed,
        ssh_protocol,
        ssh_disconnected,
        ssh_channel_refused,
        ssh_request_refused,
        smtp_reply,
        smtp_auth_failed,
        smtp_tls_required,
        smtp_unsupported,
        malformed_smtp_reply,
        sftp_failure,
        sftp_protocol,
        malformed_message,
        malformed_dmarc
    };

    namespace detail {
        class NetCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "net";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::invalid_address: return "invalid address";
                    case errc::host_not_found: return "no such host";
                    case errc::no_suitable_address: return "no suitable address found";
                    case errc::invalid_url: return "invalid URL";
                    case errc::unsupported_scheme: return "unsupported protocol scheme";
                    case errc::malformed_response: return "malformed HTTP response";
                    case errc::header_too_large: return "header too large";
                    case errc::body_too_large: return "body too large";
                    case errc::too_many_redirects: return "stopped after too many redirects";
                    case errc::server_closed: return "server closed";
                    case errc::invalid_cookie: return "invalid cookie";
                    case errc::http_status: return "the response's status is not 2xx";
                    case errc::no_data: return "no DNS record of the type asked";
                    case errc::server_failure: return "DNS server failure";
                    case errc::server_misbehaving: return "DNS server misbehaving";
                    case errc::proxy_failure: return "proxy failure";
                    case errc::proxy_refused: return "the proxy refused the connection";
                    case errc::proxy_unsupported: return "the proxy does not support the request";
                    case errc::proxy_auth_required: return "proxy authentication required";
                    case errc::malformed_proxy_response: return "malformed proxy response";
                    case errc::malformed_multipart: return "malformed multipart body";
                    case errc::too_many_parts: return "too many parts";
                    case errc::not_multipart: return "the body is not multipart";
                    case errc::websocket_handshake: return "WebSocket handshake failed";
                    case errc::websocket_protocol: return "WebSocket protocol violation";
                    case errc::websocket_closed: return "WebSocket closed by the peer";
                    case errc::ssh_handshake: return "SSH handshake failed";
                    case errc::ssh_host_key_unknown: return "SSH host key unknown";
                    case errc::ssh_host_key_mismatch: return "SSH host key mismatch";
                    case errc::ssh_host_key_revoked: return "SSH host key revoked";
                    case errc::ssh_auth_failed: return "SSH authentication failed";
                    case errc::ssh_protocol: return "SSH protocol error";
                    case errc::ssh_disconnected: return "SSH connection closed by the peer";
                    case errc::ssh_channel_refused: return "SSH channel refused";
                    case errc::ssh_request_refused: return "SSH request refused";
                    case errc::smtp_reply: return "the SMTP server refused";
                    case errc::smtp_auth_failed: return "SMTP authentication failed";
                    case errc::smtp_tls_required: return "TLS required but not available";
                    case errc::smtp_unsupported: return "the SMTP server does not support what the message needs";
                    case errc::malformed_smtp_reply: return "malformed SMTP reply";
                    case errc::sftp_failure: return "SFTP failure";
                    case errc::sftp_protocol: return "SFTP protocol error";
                    case errc::malformed_message: return "malformed mail message";
                    case errc::malformed_dmarc: return "malformed DMARC record";
                }
                return "unknown net error";
            }
        };

        // The codes getaddrinfo and getnameinfo return (EAI_*), which are
        // not errno values and would be misread in the system category:
        // EAI_AGAIN is 2 on macOS, and 2 there is ENOENT
        class LookupCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "lookup";
            }

            std::string message(int c) const noexcept override {
                const char* m = ::gai_strerror(c);
                return m ? std::string(m) : std::string("unknown lookup error");
            }
        };
    }

    inline const std::error_category& category() noexcept {
        static const detail::NetCategory instance;
        return instance;
    }

    // The category of the EAI_* codes, gai_strerror's text for each
    inline const std::error_category& lookup_category() noexcept {
        static const detail::LookupCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::errc> : std::true_type {};

namespace sgcl::net::detail {
    using namespace sgcl::detail;
    // The errors the module makes of its own: an errno value in the
    // system category, as io::last_error has it
    inline io::error system_error(int e, const string& op, const string& path = {}) noexcept {
        return io::error(error_code(e, std::system_category()), op, path);
    }

    inline io::error net_error(errc e, const string& op, const string& path = {}) noexcept {
        return io::error(make_error_code(e), op, path);
    }

    // A code of getaddrinfo or getnameinfo as an error: EAI_NONAME (and
    // the EAI_NODATA of the systems that still have it) is the host not
    // found, EAI_SYSTEM is errno, anything else the code itself
    inline io::error lookup_error(int eai, int saved_errno, const string& op, const string& path) noexcept {
        if (eai == EAI_NONAME
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
            || eai == EAI_NODATA
#endif
        ) {
            return net_error(errc::host_not_found, op, path);
        }
        if (eai == EAI_SYSTEM) {
            return system_error(saved_errno, op, path);
        }
        return io::error(error_code(eai, lookup_category()), op, path);
    }
}
