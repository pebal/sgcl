//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The heads of HTTP/1.1 on any bytes, as the server and the client meet
// them: the input up to its empty line (find_head_end, as the server's
// reader cuts it) is a request head for check_request_head when its first
// byte is even and a response head for parse_response_head and
// response_framing when it is odd. What must hold:
//   - a request is refused only with 400, 501 or 505; accepted, its
//     method and target lie inside the head, the method is a token, the
//     target is not empty, there is at most one Host, and the framing is
//     never both a Content-Length and chunked, never until the close;
//   - a response's status is three digits, its reason inside the head;
//   - what is accepted, written back as a head (the line and the fields as
//     parsed) and parsed again, gives the same line, fields and framing:
//     nothing the parser takes is read two ways (request smuggling is a
//     head read one way by one server and another way by the next).
// Built with libFuzzer (tests/fuzz/run.sh http_head) or replayed by the
// library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;
    using sgcl::net::http::headers;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool is_token(std::string_view s) {
        if (s.empty()) {
            return false;
        }
        for (unsigned char c : s) {
            bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      std::string_view("!#$%&'*+-.^_`|~").find(char(c)) != std::string_view::npos;
            if (!ok) {
                return false;
            }
        }
        return true;
    }

    std::string fields_text(const headers& h) {
        std::string out;
        for (auto& f : HeadersAccess::fields(h)) {
            out.append(f.first.view());
            out += ": ";
            out.append(f.second.view());
            out += "\r\n";
        }
        return out;
    }

    struct Request {
        int refused = 0;
        std::string method, target, fields;
        int minor = 0;
        Framing framing = Framing::none;
        uint64_t length = 0;
    };

    Request request(const std::string& text) {
        string head(text);
        RequestLine line;
        headers h;
        BodyFraming framing;
        Request r;
        r.refused = check_request_head(head, line, h, framing);
        if (r.refused) {
            check(r.refused == 400 || r.refused == 501 || r.refused == 505);
            return r;
        }
        check(line.method_at + line.method_size <= head.size());
        check(line.target_at + line.target_size <= head.size());
        r.method = std::string(head.view().substr(line.method_at, line.method_size));
        r.target = std::string(head.view().substr(line.target_at, line.target_size));
        check(is_token(r.method));
        check(!r.target.empty());
        check(HeadersAccess::count(h, "host") <= 1);
        check(framing.kind != Framing::until_close);
        bool te = HeadersAccess::count(h, "transfer-encoding") != 0;
        bool cl = HeadersAccess::count(h, "content-length") != 0;
        check(!(te && cl));
        check(framing.kind != Framing::chunked || te);
        check(framing.kind != Framing::length || cl);
        r.minor = line.minor;
        r.framing = framing.kind;
        r.length = framing.length;
        r.fields = fields_text(h);
        return r;
    }

    void request_head(const std::string& text) {
        Request first = request(text);
        if (first.refused) {
            return;
        }
        std::string again = first.method + " " + first.target + " HTTP/1." + std::to_string(first.minor) + "\r\n" + first.fields + "\r\n";
        Request second = request(again);
        check(second.refused == 0);
        check(second.method == first.method && second.target == first.target && second.minor == first.minor);
        check(second.fields == first.fields);
        check(second.framing == first.framing && second.length == first.length);
    }

    void response_head(const std::string& text, bool head_request) {
        string head(text);
        StatusLine line;
        headers h;
        int refused = parse_response_head(head, line, h);
        if (refused) {
            return;
        }
        check(line.status >= 100 && line.status <= 999);
        check(line.reason_at + line.reason_size <= head.size());
        BodyFraming framing;
        if (!response_framing(h, line.status, head_request, framing)) {
            return;
        }
        bool te = HeadersAccess::count(h, "transfer-encoding") != 0;
        bool cl = HeadersAccess::count(h, "content-length") != 0;
        check(!(te && cl) || framing.kind == Framing::none);
        // written back and parsed again: the same
        std::string again = "HTTP/1." + std::to_string(line.minor) + " " + std::to_string(line.status) + " " +
                            std::string(head.view().substr(line.reason_at, line.reason_size)) + "\r\n" + fields_text(h) + "\r\n";
        string head2(again);
        StatusLine line2;
        headers h2;
        check(parse_response_head(head2, line2, h2) == 0);
        check(line2.status == line.status && line2.minor == line.minor);
        check(fields_text(h2) == fields_text(h));
        BodyFraming framing2;
        check(response_framing(h2, line2.status, head_request, framing2));
        check(framing2.kind == framing.kind && framing2.length == framing.length);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    uint8_t mode = data[0];
    const char* p = reinterpret_cast<const char*>(data + 1);
    size_t n = size - 1;
    // the head as the server's reader cuts it: up to the empty line, or all
    // of it when there is none (the reader would wait, a limit would end it)
    size_t end = find_head_end(p, n);
    std::string text(p, end ? end : n);
    if (mode & 1) {
        response_head(text, (mode & 2) != 0);
    } else {
        request_head(text);
    }
    return 0;
}
