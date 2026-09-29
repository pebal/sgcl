//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../parser.h"
#include "../../headers.h"
#include "../../../../core/aliases.h"

#include <cstdint>
#include <string_view>

// What makes an HTTP/2 request malformed (RFC 9113 §8.1.1, §8.2, §8.3.1),
// pure, for the server (serve.h) and its fuzzing: the fields of the head,
// the trailers, and the body's bytes against its content-length. A
// malformed request is its stream's PROTOCOL_ERROR.
namespace sgcl::net::http::detail::h2 {
    using RequestField = HeadersAccess::Field;

    // The pseudo-fields of a request that passed, pointers into its fields
    struct RequestHead {
        const RequestField* method = nullptr;
        const RequestField* scheme = nullptr;
        const RequestField* authority = nullptr;
        const RequestField* path = nullptr;
        size_t regulars = 0;                  // the fields that are not pseudo-fields
        optional<uint64_t> content_length;
    };

    // §8.2.2: the fields of a connection, never in HTTP/2
    inline bool connection_specific(std::string_view n) noexcept {
        return n == "connection" || n == "keep-alive" || n == "proxy-connection" || n == "transfer-encoding" || n == "upgrade";
    }

    // §8.2.1: a name with an upper-case letter is malformed (HPACK
    // carries names in lower case)
    inline bool has_upper(std::string_view n) noexcept {
        for (char c : n) {
            if (c >= 'A' && c <= 'Z') {
                return true;
            }
        }
        return false;
    }

    inline ErrorCode check_request(const headers& fields, bool end_stream, RequestHead& out) {
        bool regular = false;
        for (auto& f : HeadersAccess::fields(fields)) {
            auto n = f.first.view();
            if (n.empty() || has_upper(n)) {
                return ErrorCode::protocol_error;
            }
            if (n[0] == ':') {
                // §8.3: pseudo-fields first, each once, only the request's
                if (regular) {
                    return ErrorCode::protocol_error;
                }
                const RequestField** slot = n == ":method" ? &out.method : n == ":scheme" ? &out.scheme : n == ":authority" ? &out.authority : n == ":path" ? &out.path : nullptr;
                if (!slot || *slot) {
                    return ErrorCode::protocol_error;
                }
                *slot = &f;
                continue;
            }
            regular = true;
            ++out.regulars;
            if (connection_specific(n)) {
                return ErrorCode::protocol_error;
            }
            if (n == "te" && f.second.view() != "trailers") {
                return ErrorCode::protocol_error;
            }
        }
        if (!out.method) {
            return ErrorCode::protocol_error;
        }
        if (out.method->second.view() == "CONNECT") {
            // §8.5: :authority only
            if (out.scheme || out.path || !out.authority) {
                return ErrorCode::protocol_error;
            }
        } else if (!out.scheme || !out.path || out.path->second.view().empty()) {
            return ErrorCode::protocol_error;
        }
        if (HeadersAccess::count(fields, "content-length")) {
            if (!content_length(fields, out.content_length)) {
                return ErrorCode::protocol_error;
            }
            // §8.1.1: a length no DATA may match
            if (end_stream && out.content_length && *out.content_length > 0) {
                return ErrorCode::protocol_error;
            }
        }
        return ErrorCode::no_error;
    }

    // §8.1: trailers carry no pseudo-field, and the rules of names hold
    inline ErrorCode check_trailers(const headers& fields) {
        for (auto& f : HeadersAccess::fields(fields)) {
            auto n = f.first.view();
            if (n.empty() || n[0] == ':' || has_upper(n) || connection_specific(n)) {
                return ErrorCode::protocol_error;
            }
        }
        return ErrorCode::no_error;
    }

    // §8.1.1: the DATA of a request against its content-length
    struct BodyCount {
        optional<uint64_t> declared;
        uint64_t received = 0;

        // n bytes more (end: the last); false when the request is malformed
        bool data(size_t n, bool end) noexcept {
            received += n;
            if (!declared) {
                return true;
            }
            return received <= *declared && (!end || received == *declared);
        }

        // the trailers end it: the length must be met
        bool ended() const noexcept {
            return !declared || received == *declared;
        }
    };
}
