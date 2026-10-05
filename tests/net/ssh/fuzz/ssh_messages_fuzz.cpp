//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's messages after the packets are opened, on any bytes: a
// sequence of payloads (each a 2-byte length and its bytes) fed to a
// server's or a client's connection as its read loop feeds them (channel
// opens, requests, data, windows, EOF, CLOSE, global requests and their
// answers, key exchange messages), or to the authentication of either side
// (RFC 4252 and 4256: the server's checks of requests, the client's reading
// of answers, keyboard-interactive's questions). The transport swallows
// what is written. What must hold:
//   - every payload is acted on or ends the connection with an error, and
//     nothing after an error is fed;
//   - the authentication ends, with success or an error, when its messages
//     run out; the server's success only for a password, an answer or a key
//     its callbacks take.
#include "sgcl/net/ssh/client.h"
#include "sgcl/net/ssh/server.h"

#include <cstdint>
#include <cstring>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::ssh::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // A transport that takes every write and has nothing to read
    class NullConn final : public net::detail::ConnImpl {
    public:
        expected<size_t, io::error> raw_read(const slice<byte>&) override {
            return size_t(0);
        }

        async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte>) noexcept override {
            co_return size_t(0);
        }

        expected<size_t, io::error> raw_write(const slice<const byte>& data) override {
            return data.size();
        }

        async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept override {
            co_return data.size();
        }

        expected<void, io::error> close() noexcept override {
            _closed = true;
            return {};
        }

        bool is_closed() const noexcept override {
            return _closed;
        }

        expected<void, io::error> close_write() override {
            return {};
        }

        void set_deadline(int, time_point) noexcept override {
        }

        time_point deadline(int) const noexcept override {
            return time_point();
        }

        string describe() const noexcept override {
            return string("null");
        }

    private:
        std::atomic<bool> _closed{false};
    };

    net::connection null_connection() {
        return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<NullConn>()));
    }

    // The payloads of the input
    template<class F>
    void payloads(const uint8_t* p, size_t n, F f) {
        while (n >= 2) {
            size_t len = size_t(p[0]) << 8 | p[1];
            p += 2;
            n -= 2;
            if (len > n) {
                len = n;
            }
            if (!f(p, len)) {
                return;
            }
            p += len;
            n -= len;
        }
    }

    tracked_ptr<d::ServerSettings> server_settings() {
        tracked_ptr cfg = make_tracked<d::ServerSettings>();
        cfg->host_keys = {net::ssh::private_key::generate()};
        cfg->check_password = [](const string& user, const string& pw) { return user == "u" && pw == "pw"; };
        cfg->check_public_key = [](const string&, const net::ssh::public_key&) { return false; };
        cfg->check_keyboard_interactive = [](const string&, const vector<string>& a) { return a.size() == 1 && a[0] == "ok"; };
        cfg->prompts = {net::ssh::prompt{string("Q: "), false}};
        cfg->max_auth_tries = 3;
        cfg->max_sessions = 4;
        cfg->allow_direct_tcpip = [](const string&, const string&, uint16_t) { return false; };
        cfg->allow_tcpip_forward = [](const string&, const string&, uint16_t) { return false; };
        cfg->handler.plain = function<void(net::ssh::server_session)>([](net::ssh::server_session) {});
        cfg->on_error = [](const string&) {};
        return cfg;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const uint8_t mode = data[0] & 3;
    const uint8_t* p = data + 1;
    const size_t n = size - 1;
    if (mode == 0 || mode == 1) {
        tracked_ptr<d::SshConn> conn;
        if (mode == 0) {
            conn = tracked_ptr<d::SshConn>(make_tracked<d::ServerConn>(null_connection(), server_settings()));
        } else {
            tracked_ptr c = make_tracked<d::ClientConn>(null_connection(), d::ConnSettings());
            conn = tracked_ptr<d::SshConn>(c);
        }
        conn->authenticated.store((data[0] & 4) == 0);
        payloads(p, n, [&](const uint8_t* q, size_t len) {
            auto e = conn->dispatch(q, len);
            if (e) {
                check(!e->message().empty());
                conn->fail_with(*e);
                return false;
            }
            return true;
        });
        conn->fail_with(io::error(io::errc::closed, "fuzz"));
        return 0;
    }
    if (mode == 2) {
        tracked_ptr c = make_tracked<d::ServerConn>(null_connection(), server_settings());
        c->session_id = d::Bytes(32, 7);
        bool good_password = false;
        payloads(p, n, [&](const uint8_t* q, size_t len) {
            std::string_view v(reinterpret_cast<const char*>(q), len);
            good_password |= v.find("pw") != std::string_view::npos || v.find("ok") != std::string_view::npos;
            return c->auth_inbox.try_send(string(v));
        });
        c->auth_inbox.close();
        auto r = d::co_server_auth(c).wait();
        if (r) {
            check(good_password);
        }
        c->fail_with(io::error(io::errc::closed, "fuzz"));
        return 0;
    }
    tracked_ptr c = make_tracked<d::ClientConn>(null_connection(), d::ConnSettings());
    c->session_id = d::Bytes(32, 7);
    payloads(p, n, [&](const uint8_t* q, size_t len) { return c->auth_inbox.try_send(string(std::string_view(reinterpret_cast<const char*>(q), len))); });
    c->auth_inbox.close();
    d::ClientAuth a;
    a.user = "u";
    a.password = "pw";
    a.keyboard_interactive = [](const string&, const string&, const vector<net::ssh::prompt>& prompts) {
        vector<string> out;
        for (size_t i = 0; i < prompts.size(); ++i) {
            out.push_back(string("x"));
        }
        return out;
    };
    a.keys.push_back(d::AuthKey{net::ssh::private_key::generate().public_key(), net::ssh::private_key::generate(), false});
    (void)d::co_client_auth(c, std::move(a)).wait();
    c->fail_with(io::error(io::errc::closed, "fuzz"));
    return 0;
}
