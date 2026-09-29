//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>

// The errors of HTTP/2 (RFC 9113 §5.4, §7), shared by the frames, HPACK and
// the connection machine: what broke is the connection (stream 0: GOAWAY
// with the code) or one stream (RST_STREAM on it with the code).
namespace sgcl::net::http::detail::h2 {
    // §7
    enum class ErrorCode : uint32_t {
        no_error = 0x0,
        protocol_error = 0x1,
        internal_error = 0x2,
        flow_control_error = 0x3,
        settings_timeout = 0x4,
        stream_closed = 0x5,
        frame_size_error = 0x6,
        refused_stream = 0x7,
        cancel = 0x8,
        compression_error = 0x9,
        connect_error = 0xa,
        enhance_your_calm = 0xb,
        inadequate_security = 0xc,
        http_1_1_required = 0xd,
    };

    // What broke: the connection (stream 0: GOAWAY) or a stream (RST_STREAM
    // on it); `what` a constant text, a diagnostic never sent to the peer
    struct Error {
        ErrorCode code = ErrorCode::protocol_error;
        uint32_t stream = 0;
        const char* what = nullptr;

        bool connection() const noexcept {
            return stream == 0;
        }
    };

    inline Error connection_error(ErrorCode code, const char* what) noexcept {
        return Error{code, 0, what};
    }

    inline Error stream_error(uint32_t stream, ErrorCode code, const char* what) noexcept {
        return Error{code, stream, what};
    }
}
