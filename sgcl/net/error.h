//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
    // cookie.
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
        invalid_cookie
    };

    namespace detail {
        class NetCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "net";
            }

            std::string message(int c) const override {
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

            std::string message(int c) const override {
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
    inline io::error system_error(int e, const string& op, const string& path = {}) {
        return io::error(error_code(e, std::system_category()), op, path);
    }

    inline io::error net_error(errc e, const string& op, const string& path = {}) {
        return io::error(make_error_code(e), op, path);
    }

    // A code of getaddrinfo or getnameinfo as an error: EAI_NONAME (and
    // the EAI_NODATA of the systems that still have it) is the host not
    // found, EAI_SYSTEM is errno, anything else the code itself
    inline io::error lookup_error(int eai, int saved_errno, const string& op, const string& path) {
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
