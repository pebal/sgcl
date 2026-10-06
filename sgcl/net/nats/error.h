//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../io/error.h"

#include <string>
#include <system_error>

namespace sgcl::net::nats {
    // The failures of NATS that neither errno nor io names: the server's
    // -ERR (its text the error's path), a request nobody answers, the
    // limits of the protocol, a line that breaks it
    enum class errc {
        server_error = 1,          // -ERR of another kind
        authorization_violation,   // -ERR 'Authorization Violation', 'Authentication Timeout', 'User Authentication Expired'
        permissions_violation,     // -ERR 'Permissions Violation for Publish/Subscription to ...'
        no_responders,             // a request's status 503: no subscriber of its subject
        max_payload,               // a message past the server's max_payload
        slow_consumer,             // a subscription's queue full: messages dropped
        invalid_subject,           // a subject or a queue group that may not be used
        malformed                  // a line of the server's that breaks the protocol
    };

    namespace detail {
        class NatsCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "nats";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::server_error: return "NATS server error";
                    case errc::authorization_violation: return "authorization violation";
                    case errc::permissions_violation: return "permissions violation";
                    case errc::no_responders: return "no responders";
                    case errc::max_payload: return "maximum payload exceeded";
                    case errc::slow_consumer: return "slow consumer";
                    case errc::invalid_subject: return "invalid subject";
                    case errc::malformed: return "malformed NATS protocol line";
                }
                return "unknown nats error";
            }
        };
    }

    // The category of errc, named "nats"
    inline const std::error_category& category() noexcept {
        static const detail::NatsCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::nats::errc> : std::true_type {};

namespace sgcl::net::nats::detail {
    inline io::error nats_error(errc e, const string& op, const string& what = {}) noexcept {
        return io::error(make_error_code(e), op, what);
    }
}
