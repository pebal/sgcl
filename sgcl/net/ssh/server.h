//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "agent.h"
#include "keys.h"
#include "types.h"
#include "detail/channel_io.h"
#include "detail/conn.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../../async/blocking.h"
#include "../../async/coroutine.h"
#include "../../async/stop_token.h"
#include "../../async/wait_group.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"

#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

// An SSH server (RFC 4251-4254) in the manner of http::server: host keys,
// the user authenticated by the program's callbacks (a password, a public
// key or certificate, keyboard-interactive answers), and a handler that
// gets each session — an exec's command, a shell, a subsystem — with its
// terminal, its environment and its streams, and decides what it does: no
// system login, no PAM, nothing run by the server on its own. Port
// forwarding both ways when the program's callbacks allow it.
//
//   net::ssh::server srv;
//   srv.host_keys = {net::ssh::private_key::generate()};
//   srv.check_password = [](const string& user, const string& pw) { return pw == "secret"; };
//   srv.handle([](net::ssh::server_session s) { (void)s.output().write("hello\n"); });
//   srv.serve(":2222");
namespace sgcl::net::ssh {
    class server;
    class server_session;

    // What a session runs (RFC 4254 §6.5)
    enum class session_kind : uint8_t {
        exec,        // a command (server_session::command)
        shell,       // the user's shell
        subsystem,   // a subsystem by name (server_session::subsystem, "sftp")
    };

    namespace detail {
        struct SessionHandler {
            function<void(server_session)> plain;
            function<async::task<>(server_session)> awaited;
        };

        // What a serve() runs with: the server's fields when it was called
        struct ServerSettings {
            ConnSettings conn;
            vector<ssh::private_key> host_keys;
            vector<ssh::public_key> host_certificates;
            function<bool(const string&, const string&)> check_password;
            function<bool(const string&, const ssh::public_key&)> check_public_key;
            function<bool(const string&, const vector<string>&)> check_keyboard_interactive;
            vector<prompt> prompts;
            bool no_client_auth = false;
            int max_auth_tries = 6;
            duration login_grace_time;
            uint32_t max_sessions = 10;
            function<bool(const string&, const string&, uint16_t)> allow_direct_tcpip;
            function<bool(const string&, const string&, uint16_t)> allow_tcpip_forward;
            function<void(const string&)> on_error;
            string banner;
            SessionHandler handler;

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "ssh: " << what.view() << '\n';
                }
            }
        };

        class ServerConn;

        // A session of the server's: what the client asked before it
        // started, and how it ends
        class ServerSessionChannel final : public ChannelImpl {
        public:
            using ChannelImpl::ChannelImpl;

            bool started = false;
            session_kind kind = session_kind::shell;
            std::string command;     // exec's; subsystem's name
            optional<pty> terminal;
            vector<pair<string, string>> env;
            std::string last_signal;
            bool exit_sent = false;
            bool agent_forwarding = false;   // the client asked for it (auth-agent-req@openssh.com)
            vector<tracked_ptr<ChannelImpl>> agents;   // the agent channels opened for it, closed with it
            async::stop_source stop;

            void on_request(std::string_view name, bool want_reply, Reader& r) override;

            // The client closed the session: its stop stopped; one that never
            // started gives its room back now (a started one when its handler
            // returns)
            void on_peer_close() override;
        };

        // A listener of a client's tcpip-forward
        struct ServerForward {
            string address;
            uint32_t port = 0;       // as asked
            uint32_t bound = 0;      // as listened on
            net::listener l;
        };

        class ServerConn final : public SshConn {
        public:
            ServerConn(const net::connection& transport, const tracked_ptr<ServerSettings>& s) noexcept
            : SshConn(transport, false, s->conn)
            , cfg(s) {
            }

            tracked_ptr<ServerSettings> cfg;
            string user;

            bool host_key_of(const AlgInfo& alg, Bytes& blob) override {
                const KeyPair* k = _key_of(alg, blob);
                return k != nullptr;
            }

            bool sign_host(const AlgInfo& alg, const Bytes& h, Bytes& signature) override {
                Bytes blob;
                const KeyPair* k = _key_of(alg, blob);
                if (!k || !k->can_sign(alg.signature)) {
                    return false;
                }
                signature = k->sign(alg.signature, h.data(), h.size());
                return true;
            }

            void on_channel_open(std::string_view type, uint32_t sender, uint32_t window, uint32_t max_packet, Reader& r) override {
                if (type == "session") {
                    if (_sessions() >= cfg->max_sessions) {
                        post_open_failure(sender, OpenResourceShortage, "too many sessions");
                        return;
                    }
                    tracked_ptr<SshConn> self(this);
                    auto c = add_channel([&](uint32_t id) {
                        return tracked_ptr<ChannelImpl>(make_tracked<ServerSessionChannel>(self, id, "session", settings.window, settings.max_packet));
                    });
                    if (!c) {
                        post_open_failure(sender, OpenResourceShortage, "too many channels");
                        return;
                    }
                    _confirm(c, sender, window, max_packet);
                    return;
                }
                if (type == "direct-tcpip") {
                    Span host = r.string();
                    uint32_t port = r.u32();
                    Span orig = r.string();
                    uint32_t orig_port = r.u32();
                    if (!r.ok() || port > 65535) {
                        post_open_failure(sender, OpenConnectFailed, "malformed");
                        return;
                    }
                    if (!cfg->allow_direct_tcpip || !cfg->allow_direct_tcpip(user, string(host.view()), uint16_t(port))) {
                        post_open_failure(sender, OpenAdministrativelyProhibited, "forwarding refused");
                        return;
                    }
                    async::go(_direct(tracked_ptr<ServerConn>(this), std::string(host.view()), port, std::string(printable(orig.view())), orig_port, sender, window, max_packet));
                    return;
                }
                post_open_failure(sender, OpenUnknownChannelType, "unknown channel type");
            }

            void on_global_request(std::string_view name, Reader& r, const tracked_ptr<GlobalSlot>& slot) override {
                if (name == "tcpip-forward" || name == "cancel-tcpip-forward") {
                    Span addr = r.string();
                    uint32_t port = r.u32();
                    if (!r.ok() || port > 65535) {
                        if (slot) {
                            answer_global(slot, false, Bytes());
                        }
                        return;
                    }
                    if (name == "cancel-tcpip-forward") {
                        bool found = _cancel(addr.view(), port);
                        if (slot) {
                            answer_global(slot, found, Bytes());
                        }
                        return;
                    }
                    if (!cfg->allow_tcpip_forward || !cfg->allow_tcpip_forward(user, string(addr.view()), uint16_t(port))) {
                        if (slot) {
                            answer_global(slot, false, Bytes());
                        }
                        return;
                    }
                    async::go(_forward(tracked_ptr<ServerConn>(this), std::string(addr.view()), port, slot));
                    return;
                }
                if (slot) {
                    answer_global(slot, false, Bytes());
                }
            }

            void on_closed() override {
                vector<net::listener> ls;
                {
                    std::lock_guard<std::mutex> g(_fm);
                    for (auto& f : _forwards) {
                        ls.push_back(f.l);
                    }
                }
                for (auto& l : ls) {
                    (void)l.close();
                }
                vector<tracked_ptr<ServerSessionChannel>> sessions;
                {
                    std::lock_guard<std::mutex> g(_fm);
                    sessions = _live;
                }
                for (auto& s : sessions) {
                    s->stop.request_stop();
                }
            }

            void track(const tracked_ptr<ServerSessionChannel>& s) {
                std::lock_guard<std::mutex> g(_fm);
                _live.push_back(s);
            }

            void untrack(ServerSessionChannel* s) {
                std::lock_guard<std::mutex> g(_fm);
                for (size_t i = 0; i < _live.size(); ++i) {
                    if (_live[i].get() == s) {
                        _live.erase(_live.begin() + ptrdiff_t(i));
                        break;
                    }
                }
            }

        private:
            const KeyPair* _key_of(const AlgInfo& alg, Bytes& blob) {
                for (const auto& hk : cfg->host_keys) {
                    const KeyPair& k = PrivateKeyAccess::key(hk);
                    if (k.kind != alg.kind) {
                        continue;
                    }
                    if (!alg.cert) {
                        blob = k.public_blob;
                        return &k;
                    }
                    for (const auto& c : cfg->host_certificates) {
                        auto cert = c.certificate();
                        if (cert && cert->type == certificate_type::host && PublicKeyAccess::blob(cert->key).view() ==
                                                                               std::string_view(reinterpret_cast<const char*>(k.public_blob.data()), k.public_blob.size())) {
                            const string& b = PublicKeyAccess::blob(c);
                            blob.assign(reinterpret_cast<const uint8_t*>(b.data()), reinterpret_cast<const uint8_t*>(b.data()) + b.size());
                            return &k;
                        }
                    }
                }
                return nullptr;
            }

            size_t _sessions() {
                std::lock_guard<std::mutex> g(_fm);
                return _live.size();
            }

            void _confirm(const tracked_ptr<ChannelImpl>& c, uint32_t sender, uint32_t window, uint32_t max_packet) {
                {
                    std::lock_guard<std::mutex> g(c->m);
                    c->remote_id = sender;
                    c->remote_window = window;
                    c->remote_max_packet = std::max<uint32_t>(1, std::min<uint32_t>(max_packet, 256 * 1024 - 1024));
                    c->opened = true;
                }
                if (auto s = c.as<ServerSessionChannel>()) {
                    track(tracked_ptr<ServerSessionChannel>(s));
                }
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelOpenConfirmation).u32(sender).u32(c->local_id).u32(c->local_window_size).u32(c->local_max_packet);
                post(std::move(b));
            }

            bool _cancel(std::string_view address, uint32_t port) {
                net::listener l;
                {
                    std::lock_guard<std::mutex> g(_fm);
                    for (size_t i = 0; i < _forwards.size(); ++i) {
                        if (_forwards[i].address.view() == address && (_forwards[i].port == port || _forwards[i].bound == port)) {
                            l = _forwards[i].l;
                            _forwards.erase(_forwards.begin() + ptrdiff_t(i));
                            break;
                        }
                    }
                }
                if (!l) {
                    return false;
                }
                (void)l.close();
                return true;
            }

            // The host a client's forwarding names, as the server listens on
            // it: "" and "*" every address, "localhost" the loopback
            static std::string _bind_host(std::string_view a) {
                if (a.empty() || a == "*" || a == "0.0.0.0") {
                    return "0.0.0.0";
                }
                if (a == "localhost") {
                    return "127.0.0.1";
                }
                if (a.find(':') != std::string_view::npos) {
                    return "[" + std::string(a) + "]";
                }
                return std::string(a);
            }

            static async::task<void> _direct(tracked_ptr<ServerConn> self, std::string host, uint32_t port, std::string orig, uint32_t orig_port, uint32_t sender, uint32_t window,
                                             uint32_t max_packet) noexcept {
                std::string target = host.find(':') != std::string::npos ? "[" + host + "]:" + std::to_string(port) : host + ":" + std::to_string(port);
                auto sock = co_await net::tcp::async_connect(string(target), 10 * second);
                if (!sock) {
                    self->post_open_failure(sender, OpenConnectFailed, "connect failed");
                    co_return;
                }
                tracked_ptr<SshConn> base(self);
                auto c = self->add_channel([&](uint32_t id) {
                    return tracked_ptr<ChannelImpl>(make_tracked<ChannelImpl>(base, id, "direct-tcpip", self->settings.window, self->settings.max_packet));
                });
                if (!c) {
                    (void)sock->close();
                    self->post_open_failure(sender, OpenResourceShortage, "too many channels");
                    co_return;
                }
                self->_confirm(c, sender, window, max_packet);
                string what("ssh " + self->describe() + " direct-tcpip from " + orig + ":" + std::to_string(orig_port));
                co_await co_pipe(connection_of(c, endpoint(), endpoint_of(host, port), what), *sock);
            }

            static async::task<void> _forward(tracked_ptr<ServerConn> self, std::string address, uint32_t port, tracked_ptr<GlobalSlot> slot) noexcept {
                auto l = co_await net::tcp::async_listen(string(_bind_host(address) + ":" + std::to_string(port)));
                if (!l) {
                    if (slot) {
                        self->answer_global(slot, false, Bytes());
                    }
                    co_return;
                }
                const uint32_t bound = l->local_endpoint().port();
                {
                    std::lock_guard<std::mutex> g(self->_fm);
                    self->_forwards.push_back(ServerForward{string(address), port, bound, *l});
                }
                if (slot) {
                    Bytes extra;
                    if (port == 0) {
                        Writer w(extra);
                        w.u32(bound);
                    }
                    self->answer_global(slot, true, std::move(extra));
                }
                for (;;) {
                    auto c = co_await l->async_accept();
                    if (!c) {
                        break;
                    }
                    async::go(_forwarded(self, address, bound, *c));
                }
            }

            // A connection to a forwarded port: a forwarded-tcpip channel
            // opened to the client and the two piped
            static async::task<void> _forwarded(tracked_ptr<ServerConn> self, std::string address, uint32_t port, net::connection sock) noexcept {
                tracked_ptr<SshConn> base(self);
                auto c = self->add_channel([&](uint32_t id) {
                    return tracked_ptr<ChannelImpl>(make_tracked<ChannelImpl>(base, id, "forwarded-tcpip", self->settings.window, self->settings.max_packet));
                });
                if (!c) {
                    (void)sock.close();
                    co_return;
                }
                endpoint from = sock.remote_endpoint();
                Bytes extra;
                Writer w(extra);
                w.string(address).u32(port).string(std::string_view(from.address().to_string().view())).u32(from.port());
                auto r = co_await SshConn::co_open_channel(base, c, std::move(extra));
                if (!r) {
                    (void)sock.close();
                    co_return;
                }
                string what("ssh " + self->describe() + " forwarded-tcpip");
                co_await co_pipe(connection_of(c, sock.local_endpoint(), from, what), sock);
            }

            std::mutex _fm;
            vector<ServerForward> _forwards;
            vector<tracked_ptr<ServerSessionChannel>> _live;
        };

        struct ServerImpl {
            std::mutex lock;
            vector<net::listener> listeners;
            vector<tracked_ptr<ServerConn>> conns;
            async::wait_group running;
            std::atomic<bool> shutting_down{false};
            SessionHandler handler;

            void add(const tracked_ptr<ServerConn>& c) {
                std::lock_guard<std::mutex> g(lock);
                conns.push_back(c);
            }

            void remove(ServerConn* c) {
                std::lock_guard<std::mutex> g(lock);
                for (size_t i = 0; i < conns.size(); ++i) {
                    if (conns[i].get() == c) {
                        conns.erase(conns.begin() + ptrdiff_t(i));
                        break;
                    }
                }
            }

            vector<tracked_ptr<ServerConn>> snapshot() {
                std::lock_guard<std::mutex> g(lock);
                return conns;
            }

            void close_listeners() {
                vector<net::listener> ls;
                {
                    std::lock_guard<std::mutex> g(lock);
                    ls = listeners;
                }
                for (auto& l : ls) {
                    (void)l.close();
                }
            }
        };

        struct ServerSessionAccess;
    }

    // A session as the server's handler gets it: what the client asked for
    // and the streams of the program the handler is. A handle of one word,
    // its copies the same session. The session ends when the handler
    // returns: the exit status (0 unless exit() said otherwise), the end of
    // the output, the close
    class server_session {
    public:
        server_session() noexcept = default;

        // The user authenticated
        string user() const noexcept {
            return static_cast<detail::ServerConn*>(_c->conn.get())->user;
        }

        SGCL_INLINE_HOT session_kind kind() const noexcept {
            return _c->kind;
        }

        // exec's command; empty for the other kinds
        string command() const noexcept {
            return _c->kind == session_kind::exec ? string(_c->command) : string();
        }

        // subsystem's name; empty for the other kinds
        string subsystem() const noexcept {
            return _c->kind == session_kind::subsystem ? string(_c->command) : string();
        }

        // The terminal asked for, with its size as window-change last set
        // it; nullopt when none was
        optional<ssh::pty> pty() const noexcept {
            std::lock_guard<std::mutex> g(_c->m);
            return _c->terminal;
        }

        // The variables the client set, in their order
        vector<pair<string, string>> env() const noexcept {
            std::lock_guard<std::mutex> g(_c->m);
            return _c->env;
        }

        // The last signal the client sent ("INT", "TERM" …); empty when none
        string last_signal() const noexcept {
            std::lock_guard<std::mutex> g(_c->m);
            return string(_c->last_signal);
        }

        // Stopped when the client closes the session or the connection ends
        async::stop_token stop() const noexcept {
            return _c->stop.token();
        }

        // The client's address
        endpoint remote_endpoint() const noexcept {
            return _c->conn->transport.remote_endpoint();
        }

        // What the client writes to the program; the end of it is the
        // client's EOF
        io::reader input() const {
            return detail::reader_of(tracked_ptr<detail::ChannelImpl>(_c), 0);
        }

        // The program's standard output and error, to the client
        io::writer output() const {
            return detail::writer_of(tracked_ptr<detail::ChannelImpl>(_c), 0);
        }

        io::writer error_output() const {
            return detail::writer_of(tracked_ptr<detail::ChannelImpl>(_c), 1);
        }

        // The exit status sent (RFC 4254 §6.10); once: a second is ignored
        // `exit(...)` on this thread, `co_await async_exit(...)` in a task
        expected<void, io::error> exit(int code) const {
            return async_exit(code).wait();
        }

        async::task<expected<void, io::error>> async_exit(int code) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.u32(uint32_t(code));
            return _exit(_c, "exit-status", std::move(b));
        }

        // The program's end by a signal ("TERM", "KILL" … without "SIG")
        // sent instead of a status; once
        // `exit_signal(...)` on this thread, `co_await async_exit_signal(...)` in a task
        expected<void, io::error> exit_signal(const string& name, bool core_dumped = false, const string& message = {}) const {
            return async_exit_signal(name, core_dumped, message).wait();
        }

        async::task<expected<void, io::error>> async_exit_signal(const string& name, bool core_dumped = false, const string& message = {}) const noexcept {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(name.view()).boolean(core_dumped).string(message.view()).string("");
            return _exit(_c, "exit-signal", std::move(b));
        }

        // The client's agent, when it asked for its forwarding (ssh -A,
        // client::options::forward_agent): an auth-agent@openssh.com channel
        // opened to the client, as an agent of this side's; ssh_request_refused
        // when it did not ask, ssh_channel_refused when it refuses the channel
        // `agent(...)` on this thread, `co_await async_agent(...)` in a task
        expected<ssh::agent, io::error> agent() const {
            return async_agent().wait();
        }

        async::task<expected<ssh::agent, io::error>> async_agent() const noexcept {
            return _co_agent(_c);
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_c;
        }

    private:
        static async::task<expected<ssh::agent, io::error>> _co_agent(tracked_ptr<detail::ServerSessionChannel> c) noexcept {
            {
                std::lock_guard<std::mutex> g(c->m);
                if (!c->agent_forwarding) {
                    co_return unexpected(detail::ssh_error(net::errc::ssh_request_refused, c->conn->describe() + ": the client did not ask for its agent's forwarding"));
                }
            }
            tracked_ptr<detail::SshConn> conn = c->conn;
            auto ch = conn->add_channel([&](uint32_t id) {
                return tracked_ptr<detail::ChannelImpl>(make_tracked<detail::ChannelImpl>(conn, id, "auth-agent@openssh.com", conn->settings.window, conn->settings.max_packet));
            });
            if (!ch) {
                co_return unexpected(detail::ssh_error(net::errc::ssh_channel_refused, conn->describe() + ": too many channels"));
            }
            auto r = co_await detail::SshConn::co_open_channel(conn, ch, detail::Bytes());
            if (!r) {
                co_return unexpected(r.error());
            }
            {
                std::lock_guard<std::mutex> g(c->m);
                c->agents.push_back(ch);
            }
            tracked_ptr s = make_tracked<detail::AgentState>();
            s->c = detail::connection_of(ch, endpoint(), endpoint(), string("ssh " + conn->describe() + " agent"));
            s->path = string("ssh agent forwarding");
            co_return detail::AgentAccess::make(s);
        }

        friend struct detail::ServerSessionAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit server_session(tracked_ptr<detail::ServerSessionChannel> c) noexcept
        : _c(std::move(c)) {
        }

        SGCL_INLINE_HOT server_session(sgcl::detail::FromWord, const tracked_ptr<detail::ServerSessionChannel>& w) noexcept
        : _c(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ServerSessionChannel>& _handle_word() noexcept {
            return _c;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ServerSessionChannel>& _handle_word() const noexcept {
            return _c;
        }

        static async::task<expected<void, io::error>> _exit(tracked_ptr<detail::ServerSessionChannel> c, std::string name, detail::Bytes extra) noexcept {
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->exit_sent) {
                    co_return expected<void, io::error>();
                }
                c->exit_sent = true;
            }
            // after the output written: requests go out under the channel's write lock
            co_return co_await detail::SshConn::co_channel_request(c->conn, tracked_ptr<detail::ChannelImpl>(c), name, false, std::move(extra));
        }

        tracked_ptr<detail::ServerSessionChannel> _c;
    };

    namespace detail {
        struct ServerSessionAccess {
            SGCL_INLINE_HOT static server_session make(const tracked_ptr<ServerSessionChannel>& c) noexcept {
                return server_session(c);
            }
        };

        // The handler of a session, run in a task of its own once exec,
        // shell or subsystem is answered; then the status (0 unless sent),
        // EOF and the close
        inline async::task<void> run_session(tracked_ptr<ServerSessionChannel> c) noexcept {
            auto* sc = static_cast<ServerConn*>(c->conn.get());
            tracked_ptr<ServerSettings> cfg = sc->cfg;
            server_session s = ServerSessionAccess::make(c);
            bool threw = false;
            if (cfg->handler.awaited) {
                try {
                    co_await cfg->handler.awaited(s);
                } catch (const std::exception& e) {
                    threw = true;
                    cfg->report(string(std::string("a session handler threw: ") + e.what()));
                } catch (...) {
                    threw = true;
                    cfg->report(string("a session handler threw"));
                }
            } else if (cfg->handler.plain) {
                // a handler of the blocking forms: on a thread of the blocking
                // pool, where a write may wait
                threw = co_await async::spawn_blocking([cfg, s] {
                    try {
                        cfg->handler.plain(s);
                        return false;
                    } catch (const std::exception& e) {
                        cfg->report(string(std::string("a session handler threw: ") + e.what()));
                    } catch (...) {
                        cfg->report(string("a session handler threw"));
                    }
                    return true;
                });
            }
            // the status (0 unless exit() or exit_signal() sent one), EOF and
            // CLOSE in one write; untracked first: a client that opens the
            // next session at once finds the room
            std::string request;
            Bytes extra;
            {
                std::lock_guard<std::mutex> g(c->m);
                if (!c->exit_sent) {
                    c->exit_sent = true;
                    request = "exit-status";
                    Writer w(extra);
                    w.u32(threw ? 1 : 0);
                }
            }
            sc->untrack(c.get());
            vector<tracked_ptr<ChannelImpl>> agents;
            {
                std::lock_guard<std::mutex> g(c->m);
                agents = c->agents;
                c->agents.clear();
            }
            for (auto& a : agents) {   // the session's agent forwarding ends with it, as sshd ends it
                c->conn->close_channel(a);
            }
            co_await SshConn::co_finish_channel(c->conn, tracked_ptr<ChannelImpl>(c), std::move(request), std::move(extra));
        }

        inline void ServerSessionChannel::on_peer_close() {
            bool idle;
            {
                std::lock_guard<std::mutex> g(m);
                idle = !started;
            }
            stop.request_stop();
            if (idle) {
                static_cast<ServerConn*>(conn.get())->untrack(this);
            }
        }

        inline void ServerSessionChannel::on_request(std::string_view name, bool want_reply, Reader& r) {
            bool ok = false;
            bool start = false;
            {
                std::lock_guard<std::mutex> g(m);
                if (name == "pty-req" && !started) {
                    Span term = r.string();
                    pty p;
                    p.term = string(printable(term.view()));
                    p.columns = r.u32();
                    p.rows = r.u32();
                    p.width_pixels = r.u32();
                    p.height_pixels = r.u32();
                    Span modes = r.string();
                    if (r.ok()) {
                        Reader mr(modes);
                        while (mr.ok() && mr.left()) {
                            uint8_t op = mr.u8();
                            if (op == 0 || op >= 160) {   // TTY_OP_END; 160 and up take no value of 4 bytes we know
                                break;
                            }
                            uint32_t v = mr.u32();
                            if (mr.ok()) {
                                p.modes.push_back(pair<uint8_t, uint32_t>(op, v));
                            }
                        }
                        terminal = std::move(p);
                        ok = true;
                    }
                } else if (name == "env" && !started) {
                    Span n = r.string();
                    Span v = r.string();
                    if (r.ok() && env.size() < 256) {
                        env.push_back(pair<string, string>(string(n.view()), string(v.view())));
                        ok = true;
                    }
                } else if ((name == "exec" || name == "shell" || name == "subsystem") && !started) {
                    if (name == "shell") {
                        kind = session_kind::shell;
                        ok = true;
                    } else {
                        Span v = r.string();
                        if (r.ok()) {
                            kind = name == "exec" ? session_kind::exec : session_kind::subsystem;
                            command.assign(v.view());
                            ok = true;
                        }
                    }
                    started = ok;
                    start = ok;
                } else if (name == "window-change") {
                    uint32_t cols = r.u32(), rows = r.u32(), wp = r.u32(), hp = r.u32();
                    if (r.ok() && terminal) {
                        terminal->columns = cols;
                        terminal->rows = rows;
                        terminal->width_pixels = wp;
                        terminal->height_pixels = hp;
                    }
                    ok = r.ok();
                } else if (name == "auth-agent-req@openssh.com") {
                    agent_forwarding = true;
                    ok = true;
                } else if (name == "signal") {
                    Span s = r.string();
                    if (r.ok()) {
                        last_signal.assign(printable(s.view()));
                        ok = true;
                    }
                }
            }
            if (want_reply) {
                Bytes b;
                Writer w(b);
                w.u8(ok ? MsgChannelSuccess : MsgChannelFailure).u32(remote_id);
                conn->post(std::move(b));
            }
            if (start) {
                async::go(run_session(tracked_ptr<ServerSessionChannel>(this)));
            }
        }

        // The user authenticated by the server's callbacks (RFC 4252 and
        // RFC 4256), within max_auth_tries failures
        inline async::task<expected<void, io::error>> co_server_auth(tracked_ptr<ServerConn> c) noexcept {
            tracked_ptr<ServerSettings> cfg = c->cfg;
            auto next = [c]() { return c->auth_inbox.receive(); };
            {
                auto m = co_await next();
                if (!m) {
                    co_return fail(c->current_error());
                }
                Reader r(reinterpret_cast<const uint8_t*>(m->data()), m->size());
                if (r.u8() != MsgServiceRequest || r.string().view() != "ssh-userauth") {
                    co_return fail(c->protocol_error("ssh-userauth was not asked for"));
                }
                Bytes b;
                Writer w(b);
                w.u8(MsgServiceAccept).string("ssh-userauth");
                auto s = co_await SshConn::co_send(c, std::move(b));
                if (!s) {
                    co_return fail(s);
                }
                if (!cfg->banner.empty()) {
                    Bytes bb;
                    Writer bw(bb);
                    bw.u8(MsgUserauthBanner).string(cfg->banner.view()).string("");
                    auto sb = co_await SshConn::co_send(c, std::move(bb));
                    if (!sb) {
                        co_return fail(sb);
                    }
                }
            }
            std::vector<std::string_view> methods;
            if (cfg->check_public_key) {
                methods.push_back("publickey");
            }
            if (cfg->check_keyboard_interactive) {
                methods.push_back("keyboard-interactive");
            }
            if (cfg->check_password) {
                methods.push_back("password");
            }
            auto failure = [&]() {
                Bytes b;
                Writer w(b);
                w.u8(MsgUserauthFailure).name_list(methods).boolean(false);
                return b;
            };
            int failures = 0;
            for (;;) {
                auto m = co_await next();
                if (!m) {
                    co_return fail(c->current_error());
                }
                Reader r(reinterpret_cast<const uint8_t*>(m->data()), m->size());
                if (r.u8() != MsgUserauthRequest) {
                    co_return fail(c->protocol_error("an authentication request was expected"));
                }
                Span user = r.string();
                Span service = r.string();
                Span method = r.string();
                if (!r.ok()) {
                    co_return fail(c->protocol_error("a malformed authentication request"));
                }
                if (service.view() != "ssh-connection") {
                    c->disconnect(DisconnectServiceNotAvailable, "service not available", ssh_error(net::errc::ssh_protocol, c->describe() + ": a service not offered"));
                    co_return fail(c->current_error());
                }
                const string uname(user.view());
                bool ok = false;
                bool counted = true;
                if (method.view() == "none") {
                    ok = cfg->no_client_auth;
                    counted = false;
                } else if (method.view() == "publickey" && cfg->check_public_key) {
                    bool has_sig = r.boolean();
                    Span alg = r.string();
                    Span blob = r.string();
                    Span sig;
                    if (has_sig) {
                        sig = r.string();
                    }
                    const AlgInfo* ai = find_alg(alg.view());
                    ParsedKey pk;
                    bool usable = r.ok() && ai && parse_key(blob.p, blob.n, pk) && pk.key.kind == ai->kind && pk.cert == ai->cert;
                    ssh::public_key key = usable ? PublicKeyAccess::make(blob.p, blob.n) : ssh::public_key();
                    if (usable && pk.cert) {
                        // a user's certificate: signed (verified by
                        // certificate()), of the user type, valid now, for
                        // the user; the program's callback decides on its
                        // authority (authorized_keys::allows does it)
                        auto cert = key.certificate();
                        usable = cert && certificate_holds(*cert, certificate_type::user, uname.view(), uint64_t(time::now().unix()));
                    }
                    if (usable && !has_sig) {
                        // a query: PK_OK when the key would do
                        if (cfg->check_public_key(uname, key)) {
                            Bytes b;
                            Writer w(b);
                            w.u8(MsgUserauth60).string(alg).string(blob);
                            auto s = co_await SshConn::co_send(c, std::move(b));
                            if (!s) {
                                co_return fail(s);
                            }
                            continue;
                        }
                    } else if (usable) {
                        Bytes data;
                        Writer dw(data);
                        dw.string(c->session_id);
                        dw.u8(MsgUserauthRequest).string(user).string(service).string("publickey").boolean(true).string(alg).string(blob);
                        ok = verify(pk.key, ai->signature, data.data(), data.size(), sig.p, sig.n) && cfg->check_public_key(uname, key);
                    }
                } else if (method.view() == "password" && cfg->check_password) {
                    bool change = r.boolean();
                    Span pw = r.string();
                    if (r.ok() && !change) {
                        ok = cfg->check_password(uname, string(pw.view()));
                    }
                } else if (method.view() == "keyboard-interactive" && cfg->check_keyboard_interactive) {
                    Bytes b;
                    Writer w(b);
                    w.u8(MsgUserauth60).string("").string("").string("").u32(uint32_t(cfg->prompts.size()));
                    for (const auto& p : cfg->prompts) {
                        w.string(p.text.view()).boolean(p.echo);
                    }
                    auto s = co_await SshConn::co_send(c, std::move(b));
                    if (!s) {
                        co_return fail(s);
                    }
                    auto ans = co_await next();
                    if (!ans) {
                        co_return fail(c->current_error());
                    }
                    Reader ar(reinterpret_cast<const uint8_t*>(ans->data()), ans->size());
                    if (ar.u8() != MsgUserauthInfoResponse) {
                        co_return fail(c->protocol_error("a keyboard-interactive response was expected"));
                    }
                    uint32_t n = ar.u32();
                    vector<string> answers;
                    for (uint32_t i = 0; i < n && ar.ok() && i < 64; ++i) {
                        answers.push_back(string(ar.string().view()));
                    }
                    if (ar.done() && n == cfg->prompts.size()) {
                        ok = cfg->check_keyboard_interactive(uname, answers);
                    }
                }
                if (ok) {
                    c->user = uname;
                    c->authenticated.store(true);   // before the success: the client's channels may follow it at once
                    c->start_compression_in();
                    Bytes b{MsgUserauthSuccess};
                    auto s = co_await SshConn::co_send(c, std::move(b));
                    if (!s) {
                        co_return fail(s);
                    }
                    {
                        auto g = co_await c->write_lock.scoped_lock();
                        c->start_compression_out();
                    }
                    co_return expected<void, io::error>();
                }
                if (counted && ++failures >= cfg->max_auth_tries) {
                    auto e = ssh_error(net::errc::ssh_auth_failed, c->describe() + ": too many authentication failures");
                    c->disconnect(DisconnectNoMoreAuthMethods, "too many authentication failures", e);
                    co_return fail(e);
                }
                auto s = co_await SshConn::co_send(c, failure());
                if (!s) {
                    co_return fail(s);
                }
            }
        }

        // One connection: the version, the first key exchange and the
        // authentication within login_grace_time, then the read loop serves
        // it until it ends
        inline async::task<void> serve_ssh(tracked_ptr<ServerImpl> s, tracked_ptr<ServerSettings> cfg, net::connection transport) noexcept {
            tracked_ptr conn = make_tracked<ServerConn>(transport, cfg);
            s->add(conn);
            const time_point grace = cfg->login_grace_time > duration::zero() ? sgcl::clock::now() + cfg->login_grace_time : time_point();
            transport.set_deadline(grace);
            auto v = co_await SshConn::co_version(conn);
            expected<void, io::error> r = v;
            if (r) {
                r = co_await SshConn::co_first_kex(conn);
            }
            if (r) {
                SshConn::start(conn);
                r = co_await co_server_auth(conn);
            }
            if (!r) {
                if (conn->disconnecting.load()) {
                    co_await conn->ended->receive();   // the DISCONNECT sent, then the end
                } else if (!conn->failed.load()) {
                    conn->fail_with(r.error());
                }
            } else {
                transport.set_deadline(time_point());
                conn->authenticated.store(true);
                co_await conn->ended->receive();
            }
            s->remove(conn.get());
            s->running.done();
        }

        inline ConnSettings conn_settings(const vector<string>& kex, const vector<string>& host_key, const vector<string>& ciphers, const vector<string>& macs, bool compression,
                                          uint64_t rekey_bytes, duration rekey_interval, duration keepalive, uint32_t max_packet) {
            ConnSettings s;
            auto take = [](const vector<string>& from, std::vector<std::string>& to) {
                if (!from.empty()) {
                    to.clear();
                    for (const auto& x : from) {
                        to.emplace_back(x.view());
                    }
                }
            };
            take(kex, s.prefs.kex);
            take(host_key, s.prefs.host_key);
            take(ciphers, s.prefs.ciphers);
            take(macs, s.prefs.macs);
            if (compression) {
                s.prefs.compression = {"zlib@openssh.com", "none"};
            }
            s.rekey_bytes = rekey_bytes ? rekey_bytes : uint64_t(1) << 30;
            s.rekey_interval = rekey_interval;
            s.keepalive_interval = keepalive;
            s.max_packet = std::max<uint32_t>(1024, std::min<uint32_t>(max_packet, 256 * 1024 - 1024));
            return s;
        }
    }

    // An SSH server: a handle of one word with its settings beside it (as
    // http::server has them): copies share the connections and the
    // handler, the fields are read when serve() is called
    class server {
    public:
        server() noexcept
        : _impl(make_tracked<detail::ServerImpl>()) {
        }

        server(const server&) = default;
        server& operator=(const server&) = default;

        // The handler of the sessions: a function of a server_session,
        // returning void, run on a thread of the blocking pool where it uses
        // the blocking forms (s.output().write(...)), or async::task<>, run
        // as a task that awaits the async_ forms; it runs once the client
        // started an exec, a shell or a subsystem, and the session ends when
        // it returns
        template<class Handler>
        server& handle(Handler h) {
            using R = std::invoke_result_t<Handler&, server_session>;
            detail::SessionHandler out;
            if constexpr (std::is_void_v<R>) {
                out.plain = function<void(server_session)>(std::move(h));
            } else {
                static_assert(std::is_same_v<R, async::task<>>, "a session handler returns void or async::task<>");
                out.awaited = function<async::task<>(server_session)>(std::move(h));
            }
            std::lock_guard<std::mutex> g(_impl->lock);
            _impl->handler = std::move(out);
            return *this;
        }

        // Listens on the address (":2222") and serves until shutdown() or
        // close(): then net::errc::server_closed
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // The connections of a listener the program made
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return _co_serve(_impl, _settings(), l);
        }

        // The listeners closed, then the connections waited for until they
        // end (a limit: `co_await async::with_timeout(s.async_shutdown(), 10s)`,
        // then close())
        // `shutdown(...)` on this thread, `co_await async_shutdown(...)` in a task
        void shutdown() const {
            async_shutdown().wait();
        }

        async::task<> async_shutdown() const noexcept {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed, every session's
        // stop() stopped
        void close() const {
            _impl->shutting_down.store(true);
            _impl->close_listeners();
            for (auto& c : _impl->snapshot()) {
                c->fail_with(io::error(io::errc::closed, "ssh", string(c->describe())));
            }
        }

        vector<ssh::private_key> host_keys;                 // one per kind at most is used; none: serve() fails
        vector<ssh::public_key> host_certificates;          // host certificates of those keys, offered as such
        // The authentication: a callback per method, the method offered when
        // its callback is set
        function<bool(const string& user, const string& password)> check_password;
        function<bool(const string& user, const ssh::public_key& key)> check_public_key;   // a certificate's key too, once it holds for the user
        function<bool(const string& user, const vector<string>& answers)> check_keyboard_interactive;
        vector<ssh::prompt> prompts = {ssh::prompt{string("Password: "), false}};   // keyboard-interactive's questions
        bool no_client_auth = false;                        // "none" lets anyone in
        int max_auth_tries = 6;                             // failures before the connection is ended
        duration login_grace_time = 120 * second;           // the handshake and the authentication; zero: none
        uint32_t max_sessions = 10;                         // a connection's sessions at once
        uint32_t max_packet = 32768;                        // a channel's largest data packet
        // Port forwarding: the user, the host and port asked for; refused
        // when unset. direct-tcpip is dialed by the server, tcpip-forward
        // listened on by it
        function<bool(const string& user, const string& host, uint16_t port)> allow_direct_tcpip;
        function<bool(const string& user, const string& host, uint16_t port)> allow_tcpip_forward;
        vector<string> kex;                                 // the algorithms, in order of preference; empty: the module's
        vector<string> host_key_algorithms;
        vector<string> ciphers;
        vector<string> macs;
        bool compression = false;                           // zlib@openssh.com taken when a client offers it first
        uint64_t rekey_bytes = uint64_t(1) << 30;
        duration rekey_interval = 3600 * second;
        duration keepalive_interval = duration::zero();     // keepalive@openssh.com when the client is silent this long
        string banner;                                      // sent before the authentication (RFC 4252 §5.4); empty: none
        function<void(const string&)> on_error;             // a handler's exception, an accept's failure; a line on stderr by default

    private:
        tracked_ptr<detail::ServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::ServerSettings>();
            cfg->conn = detail::conn_settings(kex, host_key_algorithms, ciphers, macs, compression, rekey_bytes, rekey_interval, keepalive_interval, max_packet);
            // the host key algorithms of the keys there are, in the module's order
            std::vector<std::string> hk;
            for (const auto& name : cfg->conn.prefs.host_key) {
                const detail::AlgInfo* a = detail::find_alg(name);
                if (!a) {
                    continue;
                }
                bool have = false;
                for (const auto& k : host_keys) {
                    if (detail::PrivateKeyAccess::key(k).kind != a->kind) {
                        continue;
                    }
                    if (!a->cert) {
                        have = true;
                        break;
                    }
                    for (const auto& c : host_certificates) {
                        auto cert = c.certificate();
                        have |= cert && cert->key == k.public_key();
                    }
                }
                if (have) {
                    hk.push_back(name);
                }
            }
            cfg->conn.prefs.host_key = hk;
            cfg->host_keys = host_keys;
            cfg->host_certificates = host_certificates;
            cfg->check_password = check_password;
            cfg->check_public_key = check_public_key;
            cfg->check_keyboard_interactive = check_keyboard_interactive;
            cfg->prompts = prompts;
            cfg->no_client_auth = no_client_auth;
            cfg->max_auth_tries = max_auth_tries > 0 ? max_auth_tries : 1;
            cfg->login_grace_time = login_grace_time;
            cfg->max_sessions = max_sessions;
            cfg->allow_direct_tcpip = allow_direct_tcpip;
            cfg->allow_tcpip_forward = allow_tcpip_forward;
            cfg->on_error = on_error;
            cfg->banner = banner;
            {
                std::lock_guard<std::mutex> g(_impl->lock);
                cfg->handler = _impl->handler;
            }
            return cfg;
        }

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, net::listener l) noexcept {
            if (cfg->conn.prefs.host_key.empty()) {
                (void)l.close();
                co_return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "ssh serve", string("a server without a host key")));
            }
            if (impl->shutting_down.load()) {
                (void)l.close();
                co_return unexpected(net::detail::net_error(net::errc::server_closed, "ssh serve", l.local_endpoint().to_string()));
            }
            {
                std::lock_guard<std::mutex> g(impl->lock);
                impl->listeners.push_back(l);
            }
            if (impl->shutting_down.load()) {
                (void)l.close();
            }
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    if (impl->shutting_down.load()) {
                        co_return unexpected(net::detail::net_error(net::errc::server_closed, "ssh serve", l.local_endpoint().to_string()));
                    }
                    if (!async::detail::runtime_exiting()) {   // the end of the program (async/scheduler.h: runtime_exit) is no failure to report
                        cfg->report(string("accept: ") + c.error().message());
                    }
                    co_return unexpected(c.error());
                }
                impl->running.add();
                async::go(detail::serve_ssh(impl, cfg, *c));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, string address) noexcept {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return unexpected(l.error());
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::ServerImpl> impl) noexcept {
            impl->shutting_down.store(true);
            impl->close_listeners();
            co_await impl->running;
        }

        tracked_ptr<detail::ServerImpl> _impl;
    };
}
