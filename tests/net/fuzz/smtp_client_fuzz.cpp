//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp's client on any bytes from a server: the reply parser alone,
// and the whole client (the greeting, EHLO and its extensions, STARTTLS
// refused, AUTH, a pipelined or plain transaction with DATA or BDAT, QUIT)
// over an in-memory connection whose server side writes the input in
// pieces and then ends. The first byte picks the mode and the credentials.
// What must hold:
//   - the parser takes a line or refuses it, never more than a reply's
//     lines; a reply it finishes has a code of 200 to 599 and an enhanced
//     code of its class or none;
//   - the client ends (the input's end is the connection's), and a failure
//     is one of the codes the module documents.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/smtp_client_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/smtp.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace smtp = sgcl::net::smtp;
    namespace sd = sgcl::net::smtp::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool documented(error_code e) {
        return e == net::errc::smtp_reply || e == net::errc::smtp_auth_failed || e == net::errc::smtp_tls_required
               || e == net::errc::smtp_unsupported || e == net::errc::malformed_smtp_reply || e == io::errc::unexpected_eof
               || e == io::errc::closed || e == std::errc::invalid_argument || e == std::errc::broken_pipe
               || e == std::errc::connection_reset;
    }

    void parse_only(std::string_view in) {
        sd::ReplyParser p;
        size_t at = 0;
        while (at < in.size() && !p.done) {
            size_t nl = in.find('\n', at);
            std::string_view line = in.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
            at = nl == std::string_view::npos ? in.size() : nl + 1;
            if (!p.feed(line)) {
                check(!p.error.empty());
                return;
            }
            check(p.lines <= sd::ReplyParser::MaxLines);
        }
        if (p.done) {
            check(p.r.code >= 200 && p.r.code <= 599);
            check(p.r.enhanced.empty() || p.r.enhanced.view()[0] - '0' == p.r.code / 100);
        }
    }

    async::task<> server_side(net::connection c, std::string input, size_t piece) {
        for (size_t i = 0; i < input.size(); i += piece) {
            if (!co_await c.async_write(string(std::string_view(input).substr(i, piece)))) {
                break;
            }
        }
        (void)c.close_write();
    }

    async::task<> drain(net::connection c) {
        array<byte, 4096> buf;
        for (;;) {
            auto r = co_await c.async_read(slice<byte>(buf.data(), buf.size()));
            if (!r || *r == 0) {
                break;
            }
        }
        (void)co_await c.async_close();
    }

    async::task<int> client_side(net::connection c, uint8_t mode) {
        tracked_ptr s = make_tracked<sd::ClientState>();
        s->server = string("memory");
        s->server_name = string("memory");
        s->o.hostname = string("fuzz.test");
        s->o.timeout = std::chrono::seconds(2);
        if (mode & 1) {
            s->o.username = string("alice");
            s->o.password = string("secret");
            s->o.allow_insecure_auth = true;
        }
        if (mode & 2) {
            s->o.require_tls = true;
        }
        s->o.starttls = false;
        auto opened = co_await sd::start(s, c);
        if (!opened) {
            check(documented(opened.error().code()));
            (void)c.close();
            co_return 0;
        }
        smtp::envelope e;
        e.from = string("a@example.com");
        e.to.push_back(string("b@example.com"));
        e.to.push_back(string("c@example.com"));
        if (mode & 4) {
            e.ret = string("HDRS");
            e.notify.push_back(string("NEVER"));
        }
        auto r = co_await sd::transact(s, e, string("Subject: x\r\n\r\n.line\r\nbody\r\n"));
        if (!r) {
            check(documented(r.error().code()));
        } else {
            check(r->reply.positive());
            check(r->rejected.size() < 2);
        }
        (void)co_await sd::quit(s);
        co_return 1;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t mode = data[0];
    const size_t piece = size_t(data[1] % 32) + 1;
    std::string_view in(reinterpret_cast<const char*>(data + 2), size - 2);
    if (mode & 8) {
        parse_only(in);
        return 0;
    }
    auto [client, server] = net::connection::in_memory();
    async::go(server_side(server, std::string(in), piece));
    async::go(drain(server));
    (void)async::spawn(client_side(client, mode)).wait();
    (void)server.close();
    return 0;
}
