//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::websocket's opening handshake on any bytes: a server's answer as
// the client checks it, a client's request as the server checks it, and
// Sec-WebSocket-Extensions' permessage-deflate. The first byte picks the
// path (its low two bits) and what was offered. What must hold:
//   - an answer taken has the status 101, Upgrade and Connection, the
//     accept of the key, at most one subprotocol and one of those offered,
//     permessage-deflate only when offered and with no window asked of
//     the client below its own;
//   - a request taken is a GET of HTTP/1.1 with the fields RFC 6455
//     §4.2.1 asks, a key of 16 bytes; a refusal is 400, 405 or 426;
//   - the parameters read of an extension, written as the server's answer
//     and read again, are the same no_context_takeover parameters; the
//     server's choice is never an offer asking it for less than 15 bits.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/websocket_handshake_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http;
    using namespace sgcl::net::http::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string head_of(std::string_view v) {
        size_t end = find_head_end(v.data(), v.size());
        return std::string(v.substr(0, end ? end : v.size()));
    }

    void answer_case(std::string_view input, uint8_t mode) {
        string head(head_of(input));
        StatusLine line;
        headers h;
        if (parse_response_head(head, line, h)) {
            return;
        }
        std::vector<std::string> offered;
        if (mode & 4) {
            offered = {"chat", "superchat"};
        }
        const bool deflate = mode & 8;
        const std::string_view key = "dGhlIHNhbXBsZSBub25jZQ==";
        WsAnswer a;
        if (check_ws_answer(line.status, h, key, offered, deflate, a)) {
            return;
        }
        check(line.status == 101);
        check(HeadersAccess::has_token(h, "upgrade", "websocket") && HeadersAccess::has_token(h, "connection", "upgrade"));
        auto accept = HeadersAccess::find(h, "sec-websocket-accept");
        check(accept && trim_ows(*accept) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
        if (!a.subprotocol.empty()) {
            check(a.subprotocol == "chat" || a.subprotocol == "superchat");
        }
        check(!a.deflate || deflate);
        if (a.deflate) {
            check(!a.deflate->client_max_window_bits_given || a.deflate->client_max_window_bits == 15);
        }
    }

    void request_case(std::string_view input) {
        string head(head_of(input));
        RequestLine line;
        headers h;
        BodyFraming framing;
        if (check_request_head(head, line, h, framing)) {
            return;
        }
        std::string_view method = head.view().substr(line.method_at, line.method_size);
        const char* why = "";
        int status = check_ws_request(method, line.minor, h, why);
        if (status) {
            check(status == 400 || status == 405 || status == 426);
            return;
        }
        check(method == "GET" && line.minor == 1);
        auto key = HeadersAccess::find(h, "sec-websocket-key");
        check(key.has_value());
        auto decoded = encoding::base64::standard.decode(string(trim_ows(*key)));
        check(decoded && decoded->size() == 16);
        check(ws_accept(trim_ows(*key)).size() == 28);
        (void)ws_origin_allowed(h, {"example.com", "*"});
        auto chosen = choose_ws_deflate(ws_tokens(h, "sec-websocket-extensions"));
        if (chosen) {
            check(!chosen->server_max_window_bits_given || chosen->server_max_window_bits == 15);
        }
    }

    void extension_case(std::string_view input) {
        WsDeflateParams p;
        if (!parse_ws_deflate(input, p)) {
            return;
        }
        check(p.server_max_window_bits >= 8 && p.server_max_window_bits <= 15);
        check(p.client_max_window_bits == 0 || (p.client_max_window_bits >= 8 && p.client_max_window_bits <= 15));
        WsDeflateParams q;
        check(parse_ws_deflate(ws_deflate_answer(p), q));
        check(q.server_no_context_takeover == p.server_no_context_takeover && q.client_no_context_takeover == p.client_no_context_takeover);
        auto chosen = choose_ws_deflate({std::string(input)});
        check(chosen.has_value() == !(p.server_max_window_bits_given && p.server_max_window_bits < 15));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode & 3) {
        case 0:
            answer_case(rest, mode);
            break;
        case 1:
            request_case(rest);
            break;
        default:
            extension_case(rest);
            break;
    }
    return 0;
}
