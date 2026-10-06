//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../io/error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace sgcl::net::amqp {
    // AMQP's reply codes (0-9-1 §1.9, the constants of the specification)
    // by their values: the broker's refusal of a method, its close of a
    // channel or of the connection; past them the client's own failures
    enum class errc {
        content_too_large = 311,
        no_route = 312,
        no_consumers = 313,
        connection_forced = 320,
        invalid_path = 402,
        access_refused = 403,
        not_found = 404,
        resource_locked = 405,
        precondition_failed = 406,
        frame_error = 501,
        syntax_error = 502,
        command_invalid = 503,
        channel_error = 504,
        unexpected_frame = 505,
        resource_error = 506,
        not_allowed = 530,
        not_implemented = 540,
        internal_error = 541,
        malformed = 1000,      // a frame of the broker's that breaks the specification
        nacked                 // a publication the broker nacked (publisher confirms)
    };

    namespace detail {
        inline const char* amqp_code_text(int c) noexcept {
            switch (c) {
                case 311: return "content too large";
                case 312: return "no route";
                case 313: return "no consumers";
                case 320: return "connection forced";
                case 402: return "invalid path";
                case 403: return "access refused";
                case 404: return "not found";
                case 405: return "resource locked";
                case 406: return "precondition failed";
                case 501: return "frame error";
                case 502: return "syntax error";
                case 503: return "command invalid";
                case 504: return "channel error";
                case 505: return "unexpected frame";
                case 506: return "resource error";
                case 530: return "not allowed";
                case 540: return "not implemented";
                case 541: return "internal error";
                case 1000: return "malformed AMQP frame";
                case 1001: return "publication nacked";
            }
            return "unknown AMQP reply";
        }

        class AmqpCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "amqp";
            }

            std::string message(int c) const noexcept override {
                return amqp_code_text(c);
            }
        };
    }

    // The category of errc, named "amqp"
    inline const std::error_category& category() noexcept {
        static const detail::AmqpCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::amqp::errc> : std::true_type {};

namespace sgcl::net::amqp {
    namespace detail {
        // A reply of the broker's (channel.close, connection.close) as an
        // error: its code, the broker's text as the path
        inline io::error amqp_error(int code, const string& op, const string& text) noexcept {
            return io::error(error_code(code, category()), op, text);
        }

        inline io::error amqp_error(errc e, const string& op, const string& what = {}) noexcept {
            return io::error(make_error_code(e), op, what);
        }
    }
}
