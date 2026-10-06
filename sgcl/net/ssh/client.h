//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "agent.h"
#include "keys.h"
#include "known_hosts.h"
#include "types.h"
#include "detail/channel_io.h"
#include "detail/conn.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../socks5.h"
#include "../../async/stop_token.h"
#include "../../core/detail/handle_word.h"
#include "../../core/function.h"
#include "../../io/os.h"

#include <mutex>
#include <string>
#include <string_view>

// An SSH client (RFC 4251-4254): a connection to a server, its host key
// checked against known_hosts (or a callback of the program's), the user
// authenticated by keys (given, the user's files, the agent's), a password
// or keyboard-interactive answers; then sessions that run commands, shells
// and subsystems, connections opened through the server (direct-tcpip) and
// a listener on the server whose connections come back here
// (tcpip-forward).
//
//   auto c = net::ssh::client::connect("example.com:22", {.user = "me"});
//   auto r = c->run("uname -a");                     // r->out, r->status.code
//   net::connection db = *c->dial("10.0.0.5:5432");  // through the server
namespace sgcl::net::ssh {
    class client;
    class session;

    namespace detail {
        // The client's side of a connection: the host key's check, the
        // channels the server opens (forwarded connections, the agent's)
        class ClientConn final : public SshConn {
        public:
            ClientConn(const net::connection& transport, const ConnSettings& s) noexcept
            : SshConn(transport, true, s) {
            }

            string address;                          // what known_hosts is asked about
            optional<ssh::known_hosts> known;
            function<expected<void, io::error>(const string&, const ssh::public_key&)> host_key_callback;
            bool ignore_host_key = false;
            bool forward_agent = false;
            string agent_path;                       // forwarded to: $SSH_AUTH_SOCK's when empty
            string user;

            optional<io::error> check_host_key(const Bytes& blob, const AlgInfo& alg, const Bytes& h, const Span& signature) override {
                ParsedKey pk;
                if (!parse_key(blob.data(), blob.size(), pk) || pk.key.kind != alg.kind || pk.cert != alg.cert) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": a host key not of the algorithm negotiated");
                }
                if (!verify(pk.key, alg.signature, h.data(), h.size(), signature.p, signature.n)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": the host key's signature does not verify");
                }
                if (pk.cert && !cert_signature_ok(blob.data(), pk)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": the host certificate's signature does not verify");
                }
                if (ignore_host_key) {
                    return nullopt;
                }
                ssh::public_key key = PublicKeyAccess::make(blob.data(), blob.size());
                if (host_key_callback) {
                    auto r = host_key_callback(address, key);
                    if (!r) {
                        return r.error();
                    }
                    return nullopt;
                }
                if (!known) {
                    auto k = ssh::known_hosts::load();
                    if (!k) {
                        return k.error();
                    }
                    known = *k;
                }
                auto r = known->check(address, key);
                if (!r) {
                    return r.error();
                }
                return nullopt;
            }

            void on_channel_open(std::string_view type, uint32_t sender, uint32_t window, uint32_t max_packet, Reader& r) override {
                if (type == "forwarded-tcpip") {
                    Span addr = r.string();
                    uint32_t port = r.u32();
                    Span orig = r.string();
                    uint32_t orig_port = r.u32();
                    if (!r.ok()) {
                        post_open_failure(sender, OpenConnectFailed, "malformed");
                        return;
                    }
                    tracked_ptr<ForwardListener> l = _listener_of(addr.view(), port);
                    if (!l) {
                        post_open_failure(sender, OpenAdministrativelyProhibited, "no such forwarding");
                        return;
                    }
                    auto c = _accept_channel(type, sender, window, max_packet);
                    if (!c) {
                        post_open_failure(sender, OpenResourceShortage, "too many channels");
                        return;
                    }
                    string what("ssh " + describe() + " forwarded " + std::string(printable(orig.view())) + ":" + std::to_string(orig_port));
                    net::connection conn = connection_of(c, l->local_endpoint(), endpoint_of(orig.view(), orig_port), what);
                    if (!l->offer(conn)) {
                        close_channel(c);
                    }
                    return;
                }
                if (type == "auth-agent@openssh.com" && forward_agent) {
                    async::go(_open_agent(tracked_ptr<ClientConn>(this), sender, window, max_packet));
                    return;
                }
                post_open_failure(sender, OpenAdministrativelyProhibited, "refused");
            }

            void on_closed() override {
                vector<tracked_ptr<ForwardListener>> all;
                {
                    std::lock_guard<std::mutex> g(_lm);
                    all = _listeners;
                    _listeners.clear();
                }
                for (auto& l : all) {
                    (void)l->close();
                }
            }

            void add_listener(const tracked_ptr<ForwardListener>& l) {
                std::lock_guard<std::mutex> g(_lm);
                _listeners.push_back(l);
            }

            void remove_listener(ForwardListener* l) {
                std::lock_guard<std::mutex> g(_lm);
                for (size_t i = 0; i < _listeners.size(); ++i) {
                    if (_listeners[i].get() == l) {
                        _listeners.erase(_listeners.begin() + ptrdiff_t(i));
                        break;
                    }
                }
            }

            // A channel the server opened, taken: its ids and windows, the
            // confirmation posted
            tracked_ptr<ChannelImpl> _accept_channel(std::string_view type, uint32_t sender, uint32_t window, uint32_t max_packet) {
                tracked_ptr<SshConn> self(this);
                auto c = add_channel([&](uint32_t id) {
                    return tracked_ptr<ChannelImpl>(make_tracked<ChannelImpl>(self, id, type, settings.window, settings.max_packet));
                });
                if (!c) {
                    return c;
                }
                {
                    std::lock_guard<std::mutex> g(c->m);
                    c->remote_id = sender;
                    c->remote_window = window;
                    c->remote_max_packet = std::max<uint32_t>(1, std::min<uint32_t>(max_packet, 256 * 1024 - 1024));
                    c->opened = true;
                }
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelOpenConfirmation).u32(sender).u32(c->local_id).u32(c->local_window_size).u32(c->local_max_packet);
                post(std::move(b));
                return c;
            }

        private:
            tracked_ptr<ForwardListener> _listener_of(std::string_view address, uint32_t port) {
                std::lock_guard<std::mutex> g(_lm);
                for (auto& l : _listeners) {
                    if (l->port == port && (l->address == address || l->address.empty() || address.empty())) {
                        return l;
                    }
                }
                for (auto& l : _listeners) {   // a server that names the address its own way
                    if (l->port == port) {
                        return l;
                    }
                }
                return tracked_ptr<ForwardListener>();
            }

            static async::task<void> _open_agent(tracked_ptr<ClientConn> self, uint32_t sender, uint32_t window, uint32_t max_packet) noexcept {
                auto s = co_await co_agent_connect(self->agent_path);
                if (!s) {
                    self->post_open_failure(sender, OpenConnectFailed, "no agent");
                    co_return;
                }
                auto c = self->_accept_channel("auth-agent@openssh.com", sender, window, max_packet);
                if (!c) {
                    (void)(*s)->c.close();
                    self->post_open_failure(sender, OpenResourceShortage, "too many channels");
                    co_return;
                }
                net::connection ch = connection_of(c, endpoint(), endpoint(), string("ssh agent forwarding"));
                co_await co_pipe(ch, (*s)->c);
            }

            std::mutex _lm;
            vector<tracked_ptr<ForwardListener>> _listeners;
        };

        // A session of the client's: a channel of the type "session"
        class SessionChannel final : public ChannelImpl {
        public:
            using ChannelImpl::ChannelImpl;
            bool agent_requested = false;
        };

        // --- the authentication (RFC 4252) ---------------------------------------

        struct AuthKey {
            ssh::public_key key;                       // a key's or a certificate's blob
            optional<ssh::private_key> priv;           // a key of the program's
            bool from_agent = false;
        };

        struct ClientAuth {
            function<void(const string&)> banner;
            string user;
            vector<AuthKey> keys;
            optional<ssh::agent> agent;
            string password;
            function<vector<string>(const string&, const string&, const vector<prompt>&)> keyboard_interactive;
        };

        inline async::task<expected<string, io::error>> co_auth_next(tracked_ptr<SshConn> c, function<void(const string&)> banner = {}) noexcept {
            for (;;) {
                auto m = co_await c->auth_inbox.receive();
                if (!m) {
                    co_return fail(c->current_error());
                }
                if (!m->empty() && uint8_t(m->data()[0]) == MsgUserauthBanner) {
                    // the server's text for a person (RFC 4252 §5.4), to the
                    // program's callback when it has one
                    Reader r(reinterpret_cast<const uint8_t*>(m->data()) + 1, m->size() - 1);
                    Span text = r.string();
                    if (r.ok() && banner) {
                        banner(string(text.view()));
                    }
                    continue;
                }
                co_return std::move(*m);
            }
        }

        // The methods a failure names, and whether it was a partial success
        inline std::vector<std::string> auth_methods(const string& m, bool& partial) {
            Reader r(reinterpret_cast<const uint8_t*>(m.data()) + 1, m.size() - 1);
            std::vector<std::string> out;
            for (auto n : r.name_list()) {
                out.emplace_back(n);
            }
            partial = r.boolean();
            return out;
        }

        // The algorithm a key signs the authentication with: the server's
        // choice for RSA (rsa-sha2-512 or -256 by server-sig-algs, RFC 8332
        // §3.3; rsa-sha2-512 when the server sent none)
        inline std::string_view auth_alg(const SshConn& c, const ssh::public_key& key) {
            ParsedKey pk;
            const string& blob = PublicKeyAccess::blob(key);
            parse_key(reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), pk);
            if (pk.key.kind == KeyKind::rsa) {
                const bool has512 = contains_name(c.server_sig_algs, "rsa-sha2-512");
                const bool has256 = contains_name(c.server_sig_algs, "rsa-sha2-256");
                if (!has512 && has256) {
                    return pk.cert ? "rsa-sha2-256-cert-v01@openssh.com" : "rsa-sha2-256";
                }
                return pk.cert ? "rsa-sha2-512-cert-v01@openssh.com" : "rsa-sha2-512";
            }
            return pk.cert ? cert_name(pk.key.kind) : key_name(pk.key.kind);
        }

        // The data a public key's signature covers (RFC 4252 §7)
        inline Bytes auth_signed_data(const Bytes& session_id, std::string_view user, std::string_view alg, const string& blob) {
            Bytes d;
            Writer w(d);
            w.string(session_id);
            w.u8(MsgUserauthRequest).string(user).string("ssh-connection").string("publickey").boolean(true).string(alg).string(blob.view());
            return d;
        }

        // ssh-userauth asked for, then the methods the server lists tried
        // in the order publickey, keyboard-interactive, password: success,
        // or ssh_auth_failed naming the methods
        inline async::task<expected<void, io::error>> co_client_auth(tracked_ptr<SshConn> c, ClientAuth a) noexcept {
            {
                Bytes b;
                Writer w(b);
                w.u8(MsgServiceRequest).string("ssh-userauth");
                auto s = co_await SshConn::co_send(c, std::move(b));
                if (!s) {
                    co_return fail(s);
                }
                auto m = co_await co_auth_next(c, a.banner);
                if (!m) {
                    co_return fail(m);
                }
                if (m->empty() || uint8_t(m->data()[0]) != MsgServiceAccept) {
                    co_return fail(c->protocol_error("ssh-userauth was not accepted"));
                }
            }
            const std::string user(a.user.view());
            auto request = [&](std::string_view method) {
                Bytes b;
                Writer w(b);
                w.u8(MsgUserauthRequest).string(user).string("ssh-connection").string(method);
                return b;
            };
            // none: the methods the server takes
            std::vector<std::string> methods;
            {
                auto s = co_await SshConn::co_send(c, request("none"));
                if (!s) {
                    co_return fail(s);
                }
                auto m = co_await co_auth_next(c, a.banner);
                if (!m) {
                    co_return fail(m);
                }
                const uint8_t t = m->empty() ? 0 : uint8_t(m->data()[0]);
                if (t == MsgUserauthSuccess) {
                    co_return expected<void, io::error>();
                }
                if (t != MsgUserauthFailure) {
                    co_return fail(c->protocol_error("an unexpected authentication answer"));
                }
                bool partial;
                methods = auth_methods(*m, partial);
            }
            std::string tried;
            auto offered = [&](std::string_view m) { return contains_name(methods, m); };
            // the answer of one attempt: true success, false failure (the
            // methods taken from it), or the connection's error
            auto outcome = [&](const string& m, bool& success) -> optional<io::error> {
                const uint8_t t = m.empty() ? 0 : uint8_t(m.data()[0]);
                if (t == MsgUserauthSuccess) {
                    success = true;
                    return nullopt;
                }
                if (t == MsgUserauthFailure) {
                    bool partial;
                    methods = auth_methods(m, partial);
                    success = false;
                    return nullopt;
                }
                return c->protocol_error("an unexpected authentication answer");
            };
            // publickey: each key signed and sent (no query first)
            for (const auto& k : a.keys) {
                if (!offered("publickey")) {
                    break;
                }
                const std::string_view alg = auth_alg(*c, k.key);
                const string& blob = PublicKeyAccess::blob(k.key);
                Bytes data = auth_signed_data(c->session_id, user, alg, blob);
                Bytes sig;
                const std::string_view sig_alg = find_alg(alg) ? find_alg(alg)->signature : alg;
                if (k.priv) {
                    const KeyPair& kp = PrivateKeyAccess::key(*k.priv);
                    if (!kp.can_sign(sig_alg)) {
                        continue;
                    }
                    sig = kp.sign(sig_alg, data.data(), data.size());
                } else if (k.from_agent && a.agent) {
                    uint32_t flags = sig_alg == "rsa-sha2-256" ? AgentRsaSha256 : sig_alg == "rsa-sha2-512" ? AgentRsaSha512 : 0;
                    auto s = co_await co_agent_sign(AgentAccess::state(*a.agent), blob, data, flags);
                    if (!s) {
                        continue;
                    }
                    sig = std::move(*s);
                } else {
                    continue;
                }
                Bytes b = request("publickey");
                Writer w(b);
                w.boolean(true).string(alg).string(blob.view()).string(sig);
                auto s = co_await SshConn::co_send(c, std::move(b));
                if (!s) {
                    co_return fail(s);
                }
                auto m = co_await co_auth_next(c, a.banner);
                if (!m) {
                    co_return fail(m);
                }
                bool success;
                if (auto e = outcome(*m, success)) {
                    co_return fail(*e);
                }
                if (success) {
                    co_return expected<void, io::error>();
                }
                tried = "publickey";
            }
            // keyboard-interactive (RFC 4256): the questions answered by the
            // program's callback
            if (a.keyboard_interactive && offered("keyboard-interactive")) {
                Bytes b = request("keyboard-interactive");
                Writer w(b);
                w.string("").string("");
                auto s = co_await SshConn::co_send(c, std::move(b));
                if (!s) {
                    co_return fail(s);
                }
                for (;;) {
                    auto m = co_await co_auth_next(c, a.banner);
                    if (!m) {
                        co_return fail(m);
                    }
                    const uint8_t t = m->empty() ? 0 : uint8_t(m->data()[0]);
                    if (t != MsgUserauth60) {
                        bool success;
                        if (auto e = outcome(*m, success)) {
                            co_return fail(*e);
                        }
                        if (success) {
                            co_return expected<void, io::error>();
                        }
                        tried += tried.empty() ? "keyboard-interactive" : ", keyboard-interactive";
                        break;
                    }
                    Reader r(reinterpret_cast<const uint8_t*>(m->data()) + 1, m->size() - 1);
                    Span name = r.string();
                    Span instruction = r.string();
                    (void)r.string();
                    uint32_t count = r.u32();
                    if (count > 64) {
                        co_return fail(c->protocol_error("too many keyboard-interactive prompts"));
                    }
                    vector<prompt> prompts;
                    for (uint32_t i = 0; i < count && r.ok(); ++i) {
                        Span text = r.string();
                        bool echo = r.boolean();
                        prompts.push_back(prompt{string(text.view()), echo});
                    }
                    if (!r.ok()) {
                        co_return fail(c->protocol_error("a malformed keyboard-interactive request"));
                    }
                    vector<string> answers = a.keyboard_interactive(string(name.view()), string(instruction.view()), prompts);
                    Bytes resp;
                    Writer rw(resp);
                    rw.u8(MsgUserauthInfoResponse).u32(uint32_t(answers.size()));
                    for (const auto& x : answers) {
                        rw.string(x.view());
                    }
                    auto s2 = co_await SshConn::co_send(c, std::move(resp));
                    if (!s2) {
                        co_return fail(s2);
                    }
                }
            }
            // password
            if (!a.password.empty() && offered("password")) {
                Bytes b = request("password");
                Writer w(b);
                w.boolean(false).string(a.password.view());
                auto s = co_await SshConn::co_send(c, std::move(b));
                crypto::detail::secure_zero(b.data(), b.size());
                if (!s) {
                    co_return fail(s);
                }
                auto m = co_await co_auth_next(c, a.banner);
                if (!m) {
                    co_return fail(m);
                }
                bool success = false;
                if (!m->empty() && uint8_t(m->data()[0]) == MsgUserauth60) {
                    success = false;   // a password change asked for: not done here
                } else if (auto e = outcome(*m, success)) {
                    co_return fail(*e);
                }
                if (success) {
                    co_return expected<void, io::error>();
                }
                tried += tried.empty() ? "password" : ", password";
            }
            std::string offered_list;
            for (const auto& m : methods) {
                offered_list += offered_list.empty() ? m : "," + m;
            }
            co_return fail(ssh_error(net::errc::ssh_auth_failed, c->describe() + ": no method succeeded (tried: " + (tried.empty() ? std::string("none") : tried) +
                                                                     "; the server takes: " + offered_list + ")"));
        }

        struct ClientAccess;
        struct SessionAccess;
    }

    // A session (RFC 4254 §6): a remote program, a shell or a subsystem,
    // with its standard streams. A handle of one word, its copies the same
    // session. The requests come before the program starts (set_env,
    // request_pty), then one of exec, shell and subsystem; the streams are
    // read and written while it runs, and wait() gives how it ended
    class session {
    public:
        session() noexcept = default;

        // An environment variable for the program (the server may refuse:
        // OpenSSH takes only those its AcceptEnv names)
        // `set_env(...)` on this thread, `co_await async_set_env(...)` in a task
        expected<void, io::error> set_env(const string& name, const string& value) const {
            return async_set_env(name, value).wait();
        }

        async::task<expected<void, io::error>> async_set_env(const string& name, const string& value) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(name.view()).string(value.view());
            return _request("env", std::move(b));
        }

        // A pseudo-terminal for the program
        // `request_pty(...)` on this thread, `co_await async_request_pty(...)` in a task
        expected<void, io::error> request_pty(const ssh::pty& p = {}) const {
            return async_request_pty(p).wait();
        }

        async::task<expected<void, io::error>> async_request_pty(const ssh::pty& p = {}) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(p.term.view()).u32(p.columns).u32(p.rows).u32(p.width_pixels).u32(p.height_pixels);
            size_t at = w.begin_string();
            for (const auto& m : p.modes) {
                w.u8(m.first).u32(m.second);
            }
            w.u8(0);   // TTY_OP_END
            w.end_string(at);
            return _request("pty-req", std::move(b));
        }

        // A command started (the user's shell runs it on the server)
        // `exec(...)` on this thread, `co_await async_exec(...)` in a task
        expected<void, io::error> exec(const string& command) const {
            return async_exec(command).wait();
        }

        async::task<expected<void, io::error>> async_exec(const string& command) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(command.view());
            return _start("exec", std::move(b));
        }

        // The user's login shell started
        // `shell(...)` on this thread, `co_await async_shell(...)` in a task
        expected<void, io::error> shell() const {
            return async_shell().wait();
        }

        async::task<expected<void, io::error>> async_shell() const noexcept {
            return _start("shell", detail::Bytes());
        }

        // A subsystem started ("sftp")
        // `subsystem(...)` on this thread, `co_await async_subsystem(...)` in a task
        expected<void, io::error> subsystem(const string& name) const {
            return async_subsystem(name).wait();
        }

        async::task<expected<void, io::error>> async_subsystem(const string& name) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(name.view());
            return _start("subsystem", std::move(b));
        }

        // The terminal's new size (no answer is asked for)
        // `window_change(...)` on this thread, `co_await async_window_change(...)` in a task
        expected<void, io::error> window_change(uint32_t columns, uint32_t rows) const {
            return async_window_change(columns, rows).wait();
        }

        async::task<expected<void, io::error>> async_window_change(uint32_t columns, uint32_t rows) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.u32(columns).u32(rows).u32(0).u32(0);
            return detail::SshConn::co_channel_request(_c->conn, tracked_ptr<detail::ChannelImpl>(_c), "window-change", false, std::move(b));
        }

        // A signal to the program: "TERM", "INT", "KILL", "HUP" … without
        // "SIG" (no answer is asked for; OpenSSH delivers them since 7.9)
        // `signal(...)` on this thread, `co_await async_signal(...)` in a task
        expected<void, io::error> signal(const string& name) const {
            return async_signal(name).wait();
        }

        async::task<expected<void, io::error>> async_signal(const string& name) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(name.view());
            return detail::SshConn::co_channel_request(_c->conn, tracked_ptr<detail::ChannelImpl>(_c), "signal", false, std::move(b));
        }

        // The program's standard input: what is written goes to it; close()
        // sends the end (EOF)
        io::writer input() const {
            return detail::writer_of(tracked_ptr<detail::ChannelImpl>(_c), 0);
        }

        // The program's standard output and error, read as they come
        io::reader output() const {
            return detail::reader_of(tracked_ptr<detail::ChannelImpl>(_c), 0);
        }

        io::reader error_output() const {
            return detail::reader_of(tracked_ptr<detail::ChannelImpl>(_c), 1);
        }

        // The end of the program's input (EOF): input().close()
        // `close_input(...)` on this thread, `co_await async_close_input(...)` in a task
        expected<void, io::error> close_input() const {
            return async_close_input().wait();
        }

        async::task<expected<void, io::error>> async_close_input() const noexcept {
            return detail::SshConn::co_channel_eof(_c->conn, tracked_ptr<detail::ChannelImpl>(_c));
        }

        // How the program ended, once the server closed the session: its
        // exit code or signal. What the program writes from here on and
        // what is not read yet of its output is dropped (a session waited
        // for never holds the server up on a full window)
        // `wait(...)` on this thread, `co_await async_wait(...)` in a task
        expected<exit_status, io::error> wait() const {
            return async_wait().wait();
        }

        async::task<expected<exit_status, io::error>> async_wait() const noexcept {
            return _co_wait(_c, true);
        }

        // The session closed at once (the program may go on; the server
        // decides)
        expected<void, io::error> close() const noexcept {
            _c->conn->close_channel(tracked_ptr<detail::ChannelImpl>(_c));
            return {};
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_c;
        }

    private:
        friend class client;
        friend struct detail::SessionAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit session(tracked_ptr<detail::SessionChannel> c) noexcept
        : _c(std::move(c)) {
        }

        SGCL_INLINE_HOT session(sgcl::detail::FromWord, const tracked_ptr<detail::SessionChannel>& w) noexcept
        : _c(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::SessionChannel>& _handle_word() noexcept {
            return _c;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::SessionChannel>& _handle_word() const noexcept {
            return _c;
        }

        async::task<expected<void, io::error>> _request(const char* name, detail::Bytes extra) const noexcept {
            return detail::SshConn::co_channel_request(_c->conn, tracked_ptr<detail::ChannelImpl>(_c), name, true, std::move(extra));
        }

        // exec, shell or subsystem: the agent's forwarding asked for first
        // when the connection forwards it
        async::task<expected<void, io::error>> _start(const char* name, detail::Bytes extra) const noexcept {
            return _co_start(_c, name, std::move(extra));
        }

        static async::task<expected<void, io::error>> _co_start(tracked_ptr<detail::SessionChannel> c, std::string name, detail::Bytes extra) noexcept {
            auto* cc = static_cast<detail::ClientConn*>(c->conn.get());
            if (cc->forward_agent && !c->agent_requested) {
                c->agent_requested = true;
                (void)co_await detail::SshConn::co_channel_request(c->conn, tracked_ptr<detail::ChannelImpl>(c), "auth-agent-req@openssh.com", true, detail::Bytes());
            }
            co_return co_await detail::SshConn::co_channel_request(c->conn, tracked_ptr<detail::ChannelImpl>(c), name, true, std::move(extra));
        }

        static async::task<expected<exit_status, io::error>> _co_wait(tracked_ptr<detail::SessionChannel> c, bool discard) noexcept {
            {
                std::lock_guard<std::mutex> g(c->m);
                if (discard && !c->discard) {
                    c->discard = true;
                    // what waits unread is dropped, its window given back
                    // (posted under the channel's lock: never after our CLOSE)
                    uint32_t adjust = 0;
                    for (int s = 0; s < 2; ++s) {
                        adjust += uint32_t(c->in[s].size() - c->in_pos[s]);
                        c->in[s].clear();
                        c->in_pos[s] = 0;
                    }
                    adjust += c->consumed;
                    c->consumed = 0;
                    if (adjust && !c->close_out && !c->close_in) {
                        detail::Bytes b;
                        detail::Writer w(b);
                        w.u8(detail::MsgChannelWindowAdjust).u32(c->remote_id).u32(adjust);
                        c->conn->post(std::move(b));
                    }
                }
            }
            for (;;) {
                tracked_ptr<async::detail::ChannelState<void>> r;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    if (c->close_in) {
                        exit_status s;
                        if (c->exit_status) {
                            s.code = int(*c->exit_status);
                        }
                        s.signal = string(c->exit_signal);
                        s.core_dumped = c->core_dumped;
                        s.message = string(c->exit_message);
                        co_return s;
                    }
                    if (c->conn_gone) {
                        co_return unexpected(c->conn->current_error());
                    }
                    r = c->rearm_locked();
                }
                co_await r->receive();
            }
        }

        tracked_ptr<detail::SessionChannel> _c;
    };

    // An SSH client connection: a handle of one word, its copies the same
    // connection, safe from many tasks and threads (sessions, forwarded
    // connections and requests share it). close() ends it, and with it
    // every session and forwarded connection
    class client {
    public:
        // How a connection is made and authenticated
        struct options {
            string user;                                    // empty: $USER
            vector<ssh::private_key> keys;                  // tried in order; none: ~/.ssh/id_ed25519, id_ecdsa, id_rsa (those without a passphrase)
            vector<ssh::public_key> certificates;           // offered before the key each certifies
            bool agent = true;                              // the keys of the agent at $SSH_AUTH_SOCK after those
            string password;                                // tried after the keys when not empty
            // keyboard-interactive (RFC 4256): the answers to the server's
            // questions (name, instruction, prompts); none: not tried
            function<vector<string>(const string&, const string&, const vector<ssh::prompt>&)> keyboard_interactive;
            optional<ssh::known_hosts> known_hosts;         // none: known_hosts::load() of the user's file
            // the host key decided by the program instead of known_hosts:
            // the address ("host:port") and the key; an error refuses it
            function<expected<void, io::error>(const string&, const ssh::public_key&)> host_key_callback;
            bool insecure_ignore_host_key = false;          // any host key taken unchecked (tests only)
            duration timeout = 30 * second;                 // the dial, the handshake and the authentication together
            async::stop_token stop;                         // the connect cancelled
            // How the connection is made; tcp::connect by default
            function<async::task<expected<net::connection, io::error>>(const string&, async::stop_token)> dial;
            vector<string> kex;                             // the algorithms, in order of preference; empty: the module's
            vector<string> host_key_algorithms;
            vector<string> ciphers;
            vector<string> macs;
            bool compression = false;                       // zlib@openssh.com offered first
            uint64_t rekey_bytes = uint64_t(1) << 30;       // a new key exchange after this many bytes either way
            duration rekey_interval = 3600 * second;        // or after this long
            duration keepalive_interval = duration::zero(); // keepalive@openssh.com when the server is silent this long; zero: none
            bool forward_agent = false;                     // the agent forwarded to the sessions' servers
            function<void(const string&)> banner;           // the server's banner before the authentication (RFC 4252 §5.4); none: dropped
        };

        client() noexcept = default;

        // A connection to "host:port", authenticated
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& address) {
            return async_connect(address, options()).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string address) noexcept {
            return _co_connect(std::move(address), net::connection(), options());
        }

        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& address, const options& o) {
            return async_connect(address, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string address, options o) noexcept {
            return _co_connect(std::move(address), net::connection(), std::move(o));
        }

        // The same over a connection there is (one through a proxy, a
        // test's pipe in memory); known_hosts is asked about its remote
        // endpoint. The transport is closed when it fails
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            string address = transport.remote_endpoint().to_string();
            return _co_connect(std::move(address), std::move(transport), std::move(o));
        }

        // A command run in a session of its own, its output and error read
        // whole, how it ended (a code other than 0 is not an error: it is
        // in the result's status)
        // `run(...)` on this thread, `co_await async_run(...)` in a task
        expected<run_result, io::error> run(const string& command) const {
            return async_run(command).wait();
        }

        async::task<expected<run_result, io::error>> async_run(const string& command) const noexcept {
            return _co_run(_c, command);
        }

        // A new session
        // `open_session(...)` on this thread, `co_await async_open_session(...)` in a task
        expected<session, io::error> open_session() const {
            return async_open_session().wait();
        }

        async::task<expected<session, io::error>> async_open_session() const noexcept {
            return _co_session(_c);
        }

        // A connection to "host:port" made by the server (direct-tcpip, RFC
        // 4254 §7.2): ssh -L's way
        // `dial(...)` on this thread, `co_await async_dial(...)` in a task
        expected<net::connection, io::error> dial(const string& address) const {
            return async_dial(address).wait();
        }

        async::task<expected<net::connection, io::error>> async_dial(const string& address) const noexcept {
            return _co_dial(_c, address);
        }

        // A listener on the server at "host:port" (tcpip-forward, RFC 4254
        // §7.1: ssh -R's way); its connections come here. ":0" lets the
        // server choose the port, which local_endpoint() then holds; an
        // empty host is every address of the server's, "localhost" its
        // loopback. Its close cancels the forwarding
        // `listen(...)` on this thread, `co_await async_listen(...)` in a task
        expected<net::listener, io::error> listen(const string& address) const {
            return async_listen(address).wait();
        }

        async::task<expected<net::listener, io::error>> async_listen(const string& address) const noexcept {
            return _co_listen(_c, address);
        }

        // A SOCKS5 proxy here whose connections the server makes (ssh -D,
        // OpenSSH's DynamicForward): each CONNECT a direct-tcpip channel to
        // its target, the name resolved by the server. Serves the address
        // ("127.0.0.1:1080") or the listener until this connection ends
        // (then nothing for its close, its error otherwise) or the
        // listener is closed (net::errc::server_closed). BIND and UDP
        // ASSOCIATE are refused (SSH has no way for them); the options'
        // authentication and rules, a socks5::server's, are taken as given
        // `serve_socks5(...)` on this thread, `co_await async_serve_socks5(...)` in a task
        expected<void, io::error> serve_socks5(const string& address) const {
            return async_serve_socks5(address).wait();
        }

        async::task<expected<void, io::error>> async_serve_socks5(string address) const noexcept {
            return _co_socks5(_c, std::move(address), net::listener(), net::socks5::server());
        }

        expected<void, io::error> serve_socks5(const net::listener& l, const net::socks5::server& proxy = {}) const {
            return async_serve_socks5(l, proxy).wait();
        }

        async::task<expected<void, io::error>> async_serve_socks5(net::listener l, net::socks5::server proxy = {}) const noexcept {
            return _co_socks5(_c, string(), std::move(l), std::move(proxy));
        }

        // keepalive@openssh.com sent and its answer awaited: whether the
        // server still answers
        // `keepalive(...)` on this thread, `co_await async_keepalive(...)` in a task
        expected<void, io::error> keepalive() const {
            return async_keepalive().wait();
        }

        async::task<expected<void, io::error>> async_keepalive() const noexcept {
            return _co_keepalive(_c);
        }

        // Until the connection ends (the server's disconnect, a failure,
        // close()): its error, ssh_disconnected for a plain end
        // `wait(...)` on this thread, `co_await async_wait(...)` in a task
        expected<void, io::error> wait() const {
            return async_wait().wait();
        }

        async::task<expected<void, io::error>> async_wait() const noexcept {
            return _co_wait(_c);
        }

        // The connection ended now: every session, forwarded connection and
        // listener ends with it
        expected<void, io::error> close() const noexcept {
            _c->fail_with(io::error(io::errc::closed, "ssh", string(_c->describe())));
            return {};
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _c->failed.load();
        }

        // The server's host key (a certificate's blob when it sent one)
        ssh::public_key host_key() const noexcept {
            return detail::PublicKeyAccess::make(_c->host_key_blob.data(), _c->host_key_blob.size());
        }

        // The server's version line ("SSH-2.0-OpenSSH_10.2")
        string server_version() const noexcept {
            return string(_c->peer_version);
        }

        // The user authenticated
        string user() const noexcept {
            return _c->user;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_c;
        }

    private:
        friend struct detail::ClientAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit client(tracked_ptr<detail::ClientConn> c) noexcept
        : _c(std::move(c)) {
        }

        SGCL_INLINE_HOT client(sgcl::detail::FromWord, const tracked_ptr<detail::ClientConn>& w) noexcept
        : _c(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ClientConn>& _handle_word() noexcept {
            return _c;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ClientConn>& _handle_word() const noexcept {
            return _c;
        }

        static detail::ConnSettings _settings(const options& o) {
            detail::ConnSettings s;
            auto take = [](const vector<string>& from, std::vector<std::string>& to) {
                if (!from.empty()) {
                    to.clear();
                    for (const auto& x : from) {
                        to.emplace_back(x.view());
                    }
                }
            };
            take(o.kex, s.prefs.kex);
            take(o.host_key_algorithms, s.prefs.host_key);
            take(o.ciphers, s.prefs.ciphers);
            take(o.macs, s.prefs.macs);
            if (o.compression) {
                s.prefs.compression = {"zlib@openssh.com", "none"};
            }
            s.rekey_bytes = o.rekey_bytes ? o.rekey_bytes : uint64_t(1) << 30;
            s.rekey_interval = o.rekey_interval;
            s.keepalive_interval = o.keepalive_interval;
            return s;
        }

        // The keys the authentication offers: the program's (each after the
        // certificates of it), or the user's files; then the agent's
        static async::task<detail::ClientAuth> _auth_of(options o) noexcept {
            detail::ClientAuth a;
            a.user = o.user;
            if (a.user.empty()) {
                auto u = io::getenv(string("USER"));
                a.user = u ? *u : string();
            }
            vector<ssh::private_key> keys = o.keys;
            if (keys.empty()) {
                auto home = io::getenv(string("HOME"));
                if (home) {
                    for (const char* name : {"id_ed25519", "id_ecdsa", "id_rsa"}) {
                        auto k = ssh::private_key::load(string(std::string(home->view()) + "/.ssh/" + name));
                        if (k) {
                            keys.push_back(*k);
                        }
                    }
                }
            }
            for (const auto& k : keys) {
                ssh::public_key pub = k.public_key();
                for (const auto& cert : o.certificates) {
                    auto c = cert.certificate();
                    if (c && c->key == pub) {
                        a.keys.push_back(detail::AuthKey{cert, k, false});
                    }
                }
                a.keys.push_back(detail::AuthKey{pub, k, false});
            }
            if (o.agent && io::getenv(string("SSH_AUTH_SOCK"))) {
                auto ag = co_await ssh::agent::async_connect();
                if (ag) {
                    auto listed = co_await ag->async_list();
                    if (listed) {
                        for (const auto& k : *listed) {
                            bool dup = false;
                            for (const auto& x : a.keys) {
                                dup |= x.key == k;
                            }
                            if (!dup) {
                                a.keys.push_back(detail::AuthKey{k, nullopt, true});
                            }
                        }
                        a.agent = *ag;
                    }
                }
            }
            a.password = o.password;
            a.keyboard_interactive = o.keyboard_interactive;
            a.banner = o.banner;
            co_return a;
        }

        static async::task<expected<client, io::error>> _co_connect(string address, net::connection transport, options o) noexcept {
            const time_point deadline = o.timeout > duration::zero() ? sgcl::clock::now() + o.timeout : time_point();
            if (o.stop.stop_requested()) {
                if (transport) {
                    (void)transport.close();
                }
                co_return unexpected(net::detail::system_error(ECANCELED, "ssh", address));
            }
            if (!transport) {
                if (o.dial) {
                    auto c = co_await o.dial(address, o.stop);
                    if (!c) {
                        co_return unexpected(c.error());
                    }
                    transport = *c;
                } else {
                    auto c = co_await net::detail::dial(address, o.stop, deadline);
                    if (!c) {
                        co_return unexpected(c.error());
                    }
                    transport = *c;
                }
            }
            tracked_ptr conn = make_tracked<detail::ClientConn>(transport, _settings(o));
            conn->address = address;
            conn->known = o.known_hosts;
            conn->host_key_callback = o.host_key_callback;
            conn->ignore_host_key = o.insecure_ignore_host_key;
            conn->forward_agent = o.forward_agent;
            transport.set_deadline(deadline);
            tracked_ptr done = make_tracked<async::detail::ChannelState<void>>();
            if (o.stop.stop_possible()) {
                async::go(_watch_stop(conn, o.stop, done));
            }
            auto r = co_await _co_handshake(conn, std::move(o));
            done->close();
            if (!r) {
                // the first error is the connection's: a stop's, a timeout's,
                // before the transport's close that followed it
                conn->fail_with(r.error());
                co_return unexpected(conn->current_error());
            }
            transport.set_deadline(time_point());
            co_return client(conn);
        }

        static async::task<expected<void, io::error>> _co_handshake(tracked_ptr<detail::ClientConn> conn, options o) noexcept {
            auto v = co_await detail::SshConn::co_version(conn);
            if (!v) {
                co_return v;
            }
            auto k = co_await detail::SshConn::co_first_kex(conn);
            if (!k) {
                co_return k;
            }
            detail::SshConn::start(conn);
            detail::ClientAuth a = co_await _auth_of(std::move(o));
            conn->user = a.user;
            auto r = co_await detail::co_client_auth(conn, std::move(a));
            if (!r) {
                co_return r;
            }
            conn->authenticated.store(true);
            co_return expected<void, io::error>();
        }

        static async::task<void> _watch_stop(tracked_ptr<detail::ClientConn> conn, async::stop_token stop, tracked_ptr<async::detail::ChannelState<void>> done) noexcept {
            bool stopped = false;
            co_await sgcl::async::select(stop.on_stop([&] { stopped = true; }), done->on_receive([] {}));
            if (stopped && !done->closed()) {
                conn->fail_with(net::detail::system_error(ECANCELED, "ssh", conn->address));
            }
        }

        static async::task<expected<session, io::error>> _co_session(tracked_ptr<detail::ClientConn> conn) noexcept {
            tracked_ptr<detail::SshConn> base(conn);
            auto c = conn->add_channel([&](uint32_t id) {
                return tracked_ptr<detail::ChannelImpl>(make_tracked<detail::SessionChannel>(base, id, "session", conn->settings.window, conn->settings.max_packet));
            });
            if (!c) {
                co_return unexpected(conn->failed.load() ? conn->current_error() : detail::ssh_error(net::errc::ssh_channel_refused, conn->describe() + ": too many channels"));
            }
            auto r = co_await detail::SshConn::co_open_channel(base, c, detail::Bytes());
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return session(tracked_ptr<detail::SessionChannel>(c.as<detail::SessionChannel>()));
        }

        static async::task<expected<string, io::error>> _co_read_all(io::reader r) noexcept {
            co_return co_await r.async_read_all_text();
        }

        static async::task<expected<run_result, io::error>> _co_run(tracked_ptr<detail::ClientConn> conn, string command) noexcept {
            auto s = co_await _co_session(conn);
            if (!s) {
                co_return unexpected(s.error());
            }
            // the command and the end of its input sent together (the agent's
            // forwarding asked for first when the connection forwards it)
            tracked_ptr<detail::SessionChannel> sc = s->_c;
            if (conn->forward_agent) {
                sc->agent_requested = true;
                (void)co_await detail::SshConn::co_channel_request(tracked_ptr<detail::SshConn>(conn), tracked_ptr<detail::ChannelImpl>(sc), "auth-agent-req@openssh.com", true,
                                                                   detail::Bytes());
            }
            detail::Bytes cmd;
            detail::Writer cw(cmd);
            cw.string(command.view());
            auto reply = co_await detail::SshConn::co_start_request_then_eof(tracked_ptr<detail::SshConn>(conn), tracked_ptr<detail::ChannelImpl>(sc), "exec", std::move(cmd));
            if (!reply) {
                (void)s->close();
                co_return unexpected(reply.error());
            }
            bool answered = false;
            // both streams drained as they come, in this task, to the
            // session's close
            tracked_ptr<detail::SessionChannel> c = s->_c;
            std::string out, err;
            for (;;) {
                tracked_ptr<async::detail::ChannelState<void>> wait;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    uint32_t took = 0;
                    for (int k = 0; k < 2; ++k) {
                        detail::Bytes& b = c->in[k];
                        size_t at = c->in_pos[k];
                        (k == 0 ? out : err).append(reinterpret_cast<const char*>(b.data()) + at, b.size() - at);
                        took += uint32_t(b.size() - at);
                        b.clear();
                        c->in_pos[k] = 0;
                    }
                    c->consumed += took;
                    if (c->consumed >= c->local_window_size / 2 && !c->close_in && !c->close_out && !c->eof_in) {
                        c->local_window += c->consumed;
                        detail::Bytes a;
                        detail::Writer w(a);
                        w.u8(detail::MsgChannelWindowAdjust).u32(c->remote_id).u32(c->consumed);
                        c->conn->post(std::move(a));
                        c->consumed = 0;
                    }
                    if (!answered) {
                        // the command's answer, which comes before its output
                        if (auto ok = (*reply)->try_receive()) {
                            if (!*ok) {
                                conn->close_channel(tracked_ptr<detail::ChannelImpl>(c));
                                co_return unexpected(detail::ssh_error(net::errc::ssh_request_refused, conn->describe() + ": exec"));
                            }
                            answered = true;
                        } else if ((*reply)->closed()) {
                            co_return unexpected(c->conn_gone ? conn->current_error() : detail::ssh_error(net::errc::ssh_request_refused, conn->describe() + ": exec (the channel closed)"));
                        }
                    }
                    if (c->close_in && answered) {
                        break;
                    }
                    if (c->conn_gone) {
                        co_return unexpected(conn->current_error());
                    }
                    wait = c->rearm_locked();
                }
                co_await wait->receive();
            }
            auto status = co_await s->async_wait();
            if (!status) {
                co_return unexpected(status.error());
            }
            run_result r;
            r.out = string(out);
            r.err = string(err);
            r.status = std::move(*status);
            co_return r;
        }

        static async::task<expected<void, io::error>> _co_socks5(tracked_ptr<detail::ClientConn> conn, string address, net::listener l, net::socks5::server proxy) noexcept {
            if (!l) {
                auto made = net::tcp::listen(address);
                if (!made) {
                    co_return unexpected(made.error());
                }
                l = *made;
            }
            proxy.dial = [conn](string target, async::stop_token) -> async::task<expected<net::connection, io::error>> {
                return _co_dial(conn, std::move(target));
            };
            proxy.bind = false;
            proxy.udp = false;
            // the proxy ends with this connection: a task waits for the end and closes it
            auto ender = async::spawn([](tracked_ptr<detail::ClientConn> conn, net::socks5::server proxy) -> async::task<> {
                (void)co_await _co_wait(conn);
                proxy.close();
            }(conn, proxy));
            auto served = co_await proxy.async_serve(l);
            proxy.close();
            if (!conn->failed.load()) {
                co_return served;   // the listener closed by the program: server_closed
            }
            auto end = co_await _co_wait(conn);
            co_await ender;
            if (!end && end.error().code() != io::errc::closed && end.error().code() != net::errc::ssh_disconnected) {
                co_return end;
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<net::connection, io::error>> _co_dial(tracked_ptr<detail::ClientConn> conn, string address) noexcept {
            auto hp = net::detail::split_host_port(address.view());
            auto port = hp ? net::detail::parse_port(hp->port) : nullopt;
            if (!hp || !port || hp->host.empty()) {
                co_return unexpected(net::detail::net_error(net::errc::invalid_address, "ssh dial", address));
            }
            tracked_ptr<detail::SshConn> base(conn);
            auto c = conn->add_channel([&](uint32_t id) {
                return tracked_ptr<detail::ChannelImpl>(make_tracked<detail::ChannelImpl>(base, id, "direct-tcpip", conn->settings.window, conn->settings.max_packet));
            });
            if (!c) {
                co_return unexpected(conn->failed.load() ? conn->current_error() : detail::ssh_error(net::errc::ssh_channel_refused, conn->describe() + ": too many channels"));
            }
            detail::Bytes extra;
            detail::Writer w(extra);
            w.string(hp->host).u32(*port).string("127.0.0.1").u32(0);
            auto r = co_await detail::SshConn::co_open_channel(base, c, std::move(extra));
            if (!r) {
                co_return unexpected(r.error());
            }
            string what("ssh " + conn->describe() + "->" + std::string(address.view()));
            co_return detail::connection_of(c, endpoint(), detail::endpoint_of(hp->host, *port), what);
        }

        static async::task<expected<net::listener, io::error>> _co_listen(tracked_ptr<detail::ClientConn> conn, string address) noexcept {
            auto hp = net::detail::split_host_port(address.view());
            auto port = hp ? net::detail::parse_port(hp->port) : nullopt;
            if (!hp || !port) {
                co_return unexpected(net::detail::net_error(net::errc::invalid_address, "ssh listen", address));
            }
            std::string host(hp->host);
            detail::Bytes b;
            detail::Writer w(b);
            w.u8(detail::MsgGlobalRequest).string("tcpip-forward").boolean(true).string(host).u32(*port);
            auto r = co_await detail::SshConn::co_global_request(conn, std::move(b));
            if (!r) {
                co_return unexpected(r.error());
            }
            if (!r->first) {
                co_return unexpected(detail::ssh_error(net::errc::ssh_request_refused, conn->describe() + ": tcpip-forward " + std::string(address.view())));
            }
            uint32_t bound = *port;
            if (bound == 0) {
                detail::Reader rd(reinterpret_cast<const uint8_t*>(r->second.data()), r->second.size());
                bound = rd.u32();
                if (!rd.ok() || bound == 0 || bound > 65535) {
                    co_return unexpected(conn->protocol_error("tcpip-forward answered without the port"));
                }
            }
            tracked_ptr<detail::SshConn> base(conn);
            tracked_ptr l = make_tracked<detail::ForwardListener>(base, host, bound, detail::endpoint_of(host.empty() ? std::string_view("0.0.0.0") : std::string_view(host), bound));
            detail::ForwardListener* raw = l.get();
            l->on_close = [conn, host, bound, raw] {
                conn->remove_listener(raw);
                if (!conn->failed.load()) {
                    detail::Bytes c;
                    detail::Writer cw(c);
                    cw.u8(detail::MsgGlobalRequest).string("cancel-tcpip-forward").boolean(false).string(host).u32(bound);
                    conn->post(std::move(c));
                }
            };
            conn->add_listener(l);
            co_return net::detail::ListenerAccess::make(tracked_ptr<net::detail::ListenerImpl>(l));
        }

        static async::task<expected<void, io::error>> _co_keepalive(tracked_ptr<detail::ClientConn> conn) noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.u8(detail::MsgGlobalRequest).string("keepalive@openssh.com").boolean(true);
            auto r = co_await detail::SshConn::co_global_request(conn, std::move(b));
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();   // a failure is an answer too, as OpenSSH answers it
        }

        static async::task<expected<void, io::error>> _co_wait(tracked_ptr<detail::ClientConn> conn) noexcept {
            co_await conn->ended->receive();
            co_return unexpected(conn->current_error());
        }

        tracked_ptr<detail::ClientConn> _c;
    };

    namespace detail {
        // A session's channel, for the modules over it (sftp: a deadline on
        // its reads while it waits for the server's VERSION)
        struct SessionAccess {
            SGCL_INLINE_HOT static void set_read_deadline(const session& s, time_point t) noexcept {
                {
                    std::lock_guard<std::mutex> g(s._c->m);
                    s._c->deadlines[0] = t;
                }
                s._c->wake();
            }
        };

        struct ClientAccess {
            SGCL_INLINE_HOT static const tracked_ptr<ClientConn>& conn(const client& c) noexcept {
                return c._c;
            }
        };
    }
}
