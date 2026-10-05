//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The reverse proxy's rewriting of heads, on any bytes: a request head as
// the server's parser takes it (or a response head as the client's does),
// its fields rewritten as reverse_proxy passes them on, and RFC 7239's
// Forwarded read and appended. The first byte picks the path and the
// options (X-Forwarded-*, Forwarded, Via, the Host kept, an upgrade).
// What must hold:
//   - no field of one connection goes on: Connection, Keep-Alive,
//     Proxy-Connection, Proxy-Authorization (-Authenticate on a response),
//     Transfer-Encoding, Upgrade (but in an upgrade, with its Connection
//     made "Upgrade"), Trailer (on a request), TE but as "trailers", nor
//     any field the Connection fields name (the proxy's own additions
//     aside); every other field goes on, in its order, as it came;
//   - what goes is writable: every name a token, no value with a control;
//   - X-Forwarded-For ends with the client's address; Forwarded, when on,
//     is a list RFC 7239 reads that ends with this hop's element, and
//     keeps what came only when that was RFC 7239's;
//   - the outgoing URL of an origin-form target joined to a backend's
//     parses, with the backend's origin;
//   - forwarded_valid on any text never fails, and a valid list with an
//     element appended is valid.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/reverse_proxy_fuzz.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
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

    bool clean(std::string_view v) {
        for (unsigned char c : v) {
            if (!(c == '\t' || (c >= 0x20 && c != 0x7F))) {
                return false;
            }
        }
        return true;
    }

    bool added_by_proxy(std::string_view n) {
        return iequal(n, "host") || iequal(n, "te") || iequal(n, "x-forwarded-for") || iequal(n, "x-forwarded-host") || iequal(n, "x-forwarded-proto")
               || iequal(n, "forwarded") || iequal(n, "via") || iequal(n, "connection") || iequal(n, "upgrade");
    }

    void request_side(uint8_t mode, std::string_view bytes) {
        size_t end = find_head_end(bytes.data(), bytes.size());
        if (!end) {
            return;
        }
        string head(bytes.substr(0, end));
        RequestLine line;
        headers h;
        BodyFraming framing;
        if (check_request_head(head, line, h, framing)) {
            return;
        }
        tracked_ptr p = make_tracked<ProxyState>();
        p->x_forwarded = mode & 1;
        p->forwarded = mode & 2;
        p->via = (mode & 4) ? string("fuzz") : string();
        p->preserve_host = mode & 8;
        const bool upgrade = mode & 16;
        tracked_ptr in = make_tracked<RequestImpl>();
        in->head = head;
        in->fields = h;
        in->minor = line.minor;
        in->method = string(head.view().substr(line.method_at, line.method_size));
        in->target = head.as_slice(line.target_at, line.target_size);
        if (auto host = HeadersAccess::find(h, "host")) {
            in->host = head.as_slice(size_t(host->data() - head.data()), host->size());
        }
        in->remote = net::endpoint(*net::ip_address::parse((mode & 32) ? "2001:db8::7" : "192.0.2.9"), 5555);
        headers out = outgoing_fields(*p, *in, upgrade);
        check(!invalid_field(out).has_value());
        // the connection's fields gone, the rest kept in order
        for (auto& f : HeadersAccess::fields(out)) {
            auto n = f.first.view();
            check(!(iequal(n, "keep-alive") || iequal(n, "proxy-connection") || iequal(n, "proxy-authorization") || iequal(n, "transfer-encoding")
                    || iequal(n, "trailer") || iequal(n, "expect")));
            if (iequal(n, "te")) {
                check(f.second.view() == "trailers");
            }
            if (iequal(n, "connection")) {
                check(upgrade && f.second.view() == "Upgrade");
            }
            if (iequal(n, "upgrade")) {
                check(upgrade);
            }
            if (!added_by_proxy(n)) {
                check(!named_by_connection(h, n));
            }
        }
        size_t k = 0;
        auto& got = HeadersAccess::fields(out);
        for (auto& f : HeadersAccess::fields(h)) {
            auto n = f.first.view();
            if (added_by_proxy(n) || hop_by_hop(n, false) || iequal(n, "expect") || named_by_connection(h, n)) {
                continue;
            }
            while (k < got.size() && got[k].first.view() != n) {
                ++k;
            }
            check(k < got.size());
            check(got[k].second.view() == f.second.view());
            ++k;
        }
        if (p->x_forwarded) {
            auto xff = out.get("X-Forwarded-For");
            check(xff.view().ends_with((mode & 32) ? "2001:db8::7" : "192.0.2.9"));
        }
        if (p->forwarded) {
            auto fw = out.get("Forwarded");
            check(forwarded_valid(fw.view()));
            check(fw.view().find((mode & 32) ? "for=\"[2001:db8::7]\"" : "for=192.0.2.9") != std::string_view::npos);
            bool came_valid = true;
            bool came = false;
            for (auto& f : HeadersAccess::fields(h)) {
                if (iequal(f.first.view(), "forwarded")) {
                    came = true;
                    came_valid = came_valid && forwarded_valid(trim_ows(f.second.view()));
                }
            }
            if (came && came_valid) {
                check(fw.view().find(", for=") != std::string_view::npos);
            }
        }
        if (p->preserve_host) {
            if (auto host = HeadersAccess::find(h, "host"); host && !host->empty()) {
                check(out.get("Host").view() == *host);
            }
        } else {
            check(!out.contains("Host"));
        }
        // the outgoing URL of an origin-form target
        string holder;
        std::string_view path, query;
        request_path_query(*in, holder, path, query);
        auto backend = *net::url::parse("http://backend.test:8080/base?k=1");
        string joined = join_target(backend, "http://backend.test:8080", "/base", "k=1", path, query);
        if (in->target.view().starts_with("/")) {
            auto u = net::url::parse(joined);
            check(u.has_value());
            check(u->host() == "backend.test:8080");
        }
    }

    void response_side(uint8_t mode, std::string_view bytes) {
        size_t end = find_head_end(bytes.data(), bytes.size());
        if (!end) {
            return;
        }
        string head(bytes.substr(0, end));
        StatusLine line;
        headers h;
        if (parse_response_head(head, line, h)) {
            return;
        }
        const bool upgrade = mode & 16;
        headers out = end_to_end(h, true, upgrade, [](std::string_view) {
            return false;
        });
        for (auto& f : HeadersAccess::fields(out)) {
            auto n = f.first.view();
            check(!(iequal(n, "keep-alive") || iequal(n, "proxy-connection") || iequal(n, "proxy-authenticate") || iequal(n, "transfer-encoding")
                    || iequal(n, "te")));
            if (iequal(n, "connection")) {
                check(upgrade && f.second.view() == "Upgrade");
            } else if (iequal(n, "upgrade")) {
                check(upgrade);
            } else {
                check(!named_by_connection(h, n));
            }
        }
        // Trailer is passed on in a response
        if (h.contains("Trailer") && !named_by_connection(h, "trailer")) {
            check(out.contains("Trailer"));
        }
    }

    void forwarded_text(std::string_view text) {
        bool valid = forwarded_valid(text);
        headers h;
        h.add("Forwarded", string(text));
        auto e = forwarded_element("192.0.2.1", "a b", "http");
        auto joined = forwarded_append(h, e);
        check(forwarded_valid(joined));
        if (valid && !trim_ows(text).empty()) {
            check(joined == std::string(trim_ows(text)) + ", " + e);
        }
        // a value written as a token or a quoted-string reads back
        std::string quoted;
        forwarded_value(quoted, text);
        check(forwarded_valid("for=" + quoted) == clean(text));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 16384) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode >> 6) {
        case 0:
        case 1: request_side(mode, rest); break;
        case 2: response_side(mode, rest); break;
        case 3: forwarded_text(rest); break;
    }
    return 0;
}
