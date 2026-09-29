//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HTTP past the head's parser, on any bytes: what a program hands the
// library from outside (a Set-Cookie value, a Cookie field, a method, a
// URL, fields and a body) and what the library writes of it. The first
// byte picks the path, the rest is split at NUL bytes. What must hold:
//   - a cookie parsed and written is a Set-Cookie value that parses to a
//     cookie with the same name and attributes, and written and parsed
//     once more, to the same cookie and text: to_string∘parse is
//     idempotent from its second application;
//   - request_cookie finds a name only as a pair's name, its value inside
//     the field;
//   - a request the client would send is one whose method is a token,
//     whose target and host are visible ASCII, whose names are tokens and
//     whose values hold no control: anything else is refused before a
//     byte is written (request splitting); what it writes (the method,
//     the URL's target, Host, the fields, the framing of its body) is read
//     by the server's parser as that one request: the same method, target
//     and fields, the body exactly the rest (or refused for a Host of the
//     program's the server will not take, or CONNECT's origin form);
//   - a response a handler builds (a Location through redirect, a
//     Set-Cookie through add_cookie, fields of its own) is refused by the
//     writer exactly when a name is no token or a value holds a control,
//     and otherwise read back by the client's parser field for field.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/http_fields_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;
    using sgcl::net::http::cookie;
    using sgcl::net::http::headers;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void same(const cookie& a, const cookie& b) {
        check(a.name == b.name);
        check(a.value == b.value);
        check(a.path == b.path);
        check(a.domain == b.domain);
        check(a.expires == b.expires);
        check(a.max_age == b.max_age);
        check(a.secure == b.secure && a.http_only == b.http_only && a.partitioned == b.partitioned);
        check(a.same_site == b.same_site);
    }

    void written_cookie(const std::vector<std::string_view>& parts) {
        auto c = cookie::parse(string(parts[0]));
        if (!c) {
            return;
        }
        // the first writing may still drop what the reading then trims (a
        // path's byte past ASCII dropped before a space, as Go does); from
        // the second on, the text and the cookie stay
        string once = c->to_string();
        auto again = cookie::parse(once);
        check(again.has_value());
        string twice = again->to_string();
        auto third = cookie::parse(twice);
        check(third.has_value());
        same(*again, *third);
        check(third->to_string() == twice);
        // what to_string keeps of the first: the name and the attributes
        check(again->name == c->name);
        check(again->secure == c->secure && again->http_only == c->http_only && again->partitioned == c->partitioned);
        check(again->same_site == c->same_site);
        check(again->max_age.has_value() == c->max_age.has_value());
        // Expires to the second (a date past datetime's range is its end,
        // which carries a fraction the text does not)
        check(again->expires.has_value() == c->expires.has_value());
        check(!c->expires || again->expires->unix() == c->expires->unix());
    }

    void request_cookie_of(const std::vector<std::string_view>& parts) {
        headers h;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            h.add("Cookie", string(parts[i]));
        }
        auto name = parts.back();
        auto v = request_cookie(h, name);
        if (!v) {
            return;
        }
        check(!name.empty());
        bool inside = false;
        for (auto& f : HeadersAccess::fields(h)) {
            inside = inside || f.second.view().find(name) != std::string_view::npos;
        }
        check(inside);
    }

    bool clean_value(std::string_view v) {
        for (unsigned char c : v) {
            if (!field_value_char(c)) {
                return false;
            }
        }
        return true;
    }

    bool visible(std::string_view v) {
        for (unsigned char c : v) {
            if (c <= 0x20 || c >= 0x7F) {
                return false;
            }
        }
        return !v.empty();
    }

    // The server's side: a response as a handler builds it through the
    // writer, the fields the library adds among them (Location through
    // redirect, Set-Cookie through add_cookie) beside the handler's own.
    // What must hold: the writer refuses the head (fields_writable, the
    // first error) exactly when a name is not a token or a value holds a
    // byte a field may not — a cookie's text is made safe by to_string,
    // a Location is the program's as given — and a head it writes is read
    // back by the client's parser with the same fields
    void written_response(const std::vector<std::string_view>& parts) {
        // the parts are split at NUL, so a NUL inside one is written 0x01
        std::vector<std::string> own;
        for (auto p : parts) {
            std::string t(p);
            for (auto& ch : t) {
                ch = ch == '\x01' ? '\0' : ch;
            }
            own.push_back(std::move(t));
        }
        auto part = [&](size_t i) { return i < own.size() ? std::string_view(own[i]) : std::string_view(); };
        tracked_ptr w = make_tracked<WriterImpl>();
        auto writer = WriterAccess::make(w);
        bool writable = clean_value(part(0));
        writer.redirect(string(part(0)));
        net::http::cookie c(string(part(1)), string(part(2)));
        c.path = string(part(3));
        try {
            writer.add_cookie(c);
            check(is_token(part(1)));
        } catch (const std::invalid_argument&) {
            check(!is_token(part(1)));   // a name that is no token: the program's broken contract
        }
        for (size_t i = 4; i + 1 < own.size(); i += 2) {
            writer.add_header(string(part(i)), string(part(i + 1)));
            writable = writable && is_token(part(i)) && clean_value(part(i + 1));
        }
        check(w->fields_writable() == writable);
        if (!writable) {
            check(w->failed.has_value() && w->failed->code() == std::errc::invalid_argument);
            return;
        }
        std::string head;
        w->head_to(head, uint64_t(0));
        size_t end = find_head_end(head.data(), head.size());
        check(end == head.size());
        string text(head);
        StatusLine line;
        headers h;
        check(parse_response_head(text, line, h) == 0);
        check(line.status == 302);
        auto& read = HeadersAccess::fields(h);
        size_t k = 0;
        for (auto& f : HeadersAccess::fields(w->fields)) {
            auto n = f.first.view();
            if (iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "connection")) {
                continue;
            }
            check(k < read.size());
            check(read[k].first.view() == n);
            check(read[k].second.view() == trim_ows(f.second.view()));
            ++k;
        }
    }

    void written_request(uint8_t mode, const std::vector<std::string_view>& parts) {
        string method(parts[0]);
        auto target = net::url::parse(string(parts.size() > 1 ? parts[1] : std::string_view()));
        if (!target || target->scheme() != "http") {
            target = net::url::parse("http://example.com/a?b");
        }
        Outgoing o;
        o.method = method;
        o.target = *target;
        // what the client may write, as the model has it: the method a
        // token, the target and the host visible ASCII, every name a
        // token, no value with a control; anything else is refused before
        // a byte is written (unsendable)
        bool sendable = is_token(method.view()) && visible(target->request_target().view());
        // and what the server then takes: CONNECT wants an authority for
        // its target, which the client (no tunnels) does not write; a Host
        // of the program's is the server's to judge
        bool taken = method.view() != "CONNECT";
        bool user_host = false;
        for (size_t i = 2; i + 1 < parts.size(); i += 2) {
            o.fields.add(string(parts[i]), string(parts[i + 1]));
            sendable = sendable && is_token(parts[i]) && clean_value(parts[i + 1]);
            if (iequal(parts[i], "host")) {
                taken = taken && !user_host && valid_host(trim_ows(parts[i + 1]));
                user_host = true;
            }
        }
        if (!user_host) {
            sendable = sendable && visible(target->host().view());
            taken = taken && valid_host(target->host().view());
        }
        auto refused_now = unsendable(o);
        check(refused_now.has_value() == !sendable);
        if (!sendable) {
            return;
        }
        std::string_view body = parts.size() % 2 == 1 && parts.size() > 2 ? parts.back() : std::string_view();
        if (mode & 2) {
            o.body_kind = net::http::detail::RequestImpl::BodyKind::text;
            o.text = string(body);
        }
        bool chunked = false;
        std::string bytes = request_bytes(o, chunked);
        check(!chunked);
        size_t end = find_head_end(bytes.data(), bytes.size());
        check(end != 0);
        string head(std::string_view(bytes).substr(0, end));
        RequestLine line;
        headers h;
        BodyFraming framing;
        int refused = check_request_head(head, line, h, framing);
        if (refused) {
            check(!taken);
            return;
        }
        // read as the request written: the method, the target, the fields
        check(head.view().substr(line.method_at, line.method_size) == method.view());
        check(head.view().substr(line.target_at, line.target_size) == target->request_target().view());
        size_t i = 0;
        auto& fields = HeadersAccess::fields(h);
        if (!user_host) {
            check(i < fields.size() && iequal(fields[i].first.view(), "host"));
            check(fields[i].second.view() == target->host().view());
            ++i;
        }
        for (auto& f : HeadersAccess::fields(o.fields)) {
            if (iequal(f.first.view(), "content-length") || iequal(f.first.view(), "transfer-encoding")) {
                continue;
            }
            check(i < fields.size());
            check(fields[i].first.view() == f.first.view());
            check(fields[i].second.view() == trim_ows(f.second.view()));
            ++i;
        }
        // and the framing: the body the rest, exactly
        uint64_t length = framing.kind == Framing::length ? framing.length : 0;
        check(framing.kind == Framing::length || framing.kind == Framing::none);
        check(end + length == bytes.size());
        check(std::string_view(bytes).substr(end) == o.text.view());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 8192) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    std::vector<std::string_view> parts;
    size_t at = 0;
    for (;;) {
        size_t nul = rest.find('\0', at);
        if (nul == std::string_view::npos || parts.size() == 15) {
            break;
        }
        parts.push_back(rest.substr(at, nul - at));
        at = nul + 1;
    }
    parts.push_back(rest.substr(at));
    switch (mode % 3) {
        case 0: written_cookie(parts); break;
        case 1: request_cookie_of(parts); break;
        case 2:
            if ((mode >> 5) & 1) {
                written_response(parts);
            } else {
                written_request(mode >> 2, parts);
            }
            break;
    }
    return 0;
}
