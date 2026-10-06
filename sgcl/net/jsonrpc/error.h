//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../encoding/json.h"
#include "../../io/error.h"

#include <string>
#include <string_view>
#include <system_error>

namespace sgcl::net::jsonrpc {
    using encoding::json;

    // The error codes of JSON-RPC 2.0 (§5.1) by their values, and LSP's
    // request_cancelled; a code a handler gives of its own is kept as it
    // is in the error's value. malformed is the client's: a response that
    // breaks the specification.
    enum class errc {
        parse_error = -32700,
        invalid_request = -32600,
        method_not_found = -32601,
        invalid_params = -32602,
        internal_error = -32603,
        request_cancelled = -32800,
        malformed = 1
    };

    namespace detail {
        class JsonrpcCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "jsonrpc";
            }

            std::string message(int c) const noexcept override {
                switch (c) {
                    case -32700: return "parse error";
                    case -32600: return "invalid request";
                    case -32601: return "method not found";
                    case -32602: return "invalid params";
                    case -32603: return "internal error";
                    case -32800: return "request cancelled";
                    case 1: return "malformed JSON-RPC message";
                }
                if (c <= -32000 && c >= -32099) {
                    return "server error";
                }
                return "JSON-RPC error";
            }
        };
    }

    // The category of errc, named "jsonrpc"
    inline const std::error_category& category() noexcept {
        static const detail::JsonrpcCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }

    // An error object (§5.1): what a handler returns to refuse a call, and
    // what error_of reads back from a call's error
    struct error {
        int code = 0;
        string message;
        optional<json> data;    // none: no data member

        error() = default;

        error(int code, const string& message, optional<json> data = nullopt)
        : code(code), message(message), data(std::move(data)) {
        }

        error(errc code, const string& message, optional<json> data = nullopt)
        : code(int(code)), message(message), data(std::move(data)) {
        }
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::jsonrpc::errc> : std::true_type {};

namespace sgcl::net::jsonrpc {
    namespace detail {
        // An error object as an io::error: its code the value, the path its
        // message, then a line of its data's JSON when it has data
        inline io::error rpc_error(const error& e, const string& op) noexcept {
            std::string p(e.message.view());
            if (e.data) {
                p += '\n';
                p.append(e.data->to_string().view());
            }
            return io::error(error_code(e.code, category()), op, string(p));
        }

        inline io::error rpc_error(errc e, const string& op, const string& what = {}) noexcept {
            return io::error(make_error_code(e), op, what);
        }
    }

    // The error object a call's error carries: the code, the message, the
    // data; none for an error of another category, or the client's own
    // (errc::malformed)
    inline optional<error> error_of(const io::error& e) {
        if (e.code().category() != category() || e.code().value() == int(errc::malformed)) {
            return nullopt;
        }
        error r;
        r.code = e.code().value();
        std::string_view p = e.path().view();
        size_t nl = p.find('\n');
        r.message = string(p.substr(0, nl));
        if (nl != std::string_view::npos) {
            auto d = json::parse(string(p.substr(nl + 1)));
            if (d) {
                r.data = *d;
            }
        }
        return r;
    }
}
