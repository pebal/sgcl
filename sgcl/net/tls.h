//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"
#include "socket.h"
#include "tls/error.h"
#include "tls/detail/impl.h"
#include "../core/detail/handle_word.h"
#include "../crypto/ed25519.h"
#include "../crypto/p256.h"
#include "../crypto/p384.h"
#include "../crypto/rsa.h"
#include "../crypto/x509.h"
#include "../crypto/detail/key_pem.h"
#include "../encoding/pem.h"

#include <memory>
#include <stdexcept>
#include <string_view>

// TLS 1.3 (RFC 8446) over the module's connections: a client that
// connects and completes the handshake before it gives the connection
// out, which is then a net::connection as any other (read, read_line,
// write, deadlines, close, http on top). Only TLS 1.3; the groups
// X25519MLKEM768 (post-quantum hybrid, first by default), X25519, P-256,
// P-384; the cipher suites AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM;
// the server's chain verified by crypto::x509 against the system's roots
// or the pool given, for the server's name or address. The failures are
// io::errors of the tls category (tls/error.h): an alert of this side, of
// the peer ("remote error: tls: …"), a chain that did not verify.
//
//   auto c = net::tls::connect("example.com:443");
//   c->write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
//
// The implementation has not been through an independent cryptographic
// audit.
namespace sgcl::net::tls {
    enum class group : uint16_t {
        x25519_mlkem768 = 0x11EC,
        x25519 = 0x001D,
        secp256r1 = 0x0017,
        secp384r1 = 0x0018,
    };

    enum class cipher : uint16_t {
        aes_128_gcm_sha256 = 0x1301,
        aes_256_gcm_sha384 = 0x1302,
        chacha20_poly1305_sha256 = 0x1303,
    };

    class identity;

    namespace detail {
        // The private key of an identity, in unmanaged memory: one of the
        // kinds CertificateVerify signs with, never copied
        struct IdentityKey {
            optional<crypto::ed25519::private_key> ed25519;
            optional<crypto::p256::private_key> p256;
            optional<crypto::p384::private_key> p384;
            optional<crypto::rsa::private_key> rsa;
        };

        struct IdentityState {
            crypto::x509::chain certificates;           // the leaf first
            std::unique_ptr<IdentityKey> key;          // unmanaged, zeroed by the keys' own destructors
        };

        struct IdentityAccess {
            static const IdentityState& state(const identity& id) noexcept;
            static tracked_ptr<const void> word(const identity& id) noexcept;
        };

        // The key of a private key's DER under its PEM label: PKCS #8
        // ("PRIVATE KEY") of each kind, SEC 1 ("EC PRIVATE KEY"), PKCS #1
        // ("RSA PRIVATE KEY"); the DER is the caller's secret_bytes, read
        // in place
        inline bool read_key(IdentityKey& k, std::string_view label, const slice<const byte>& der) noexcept {
            if (label == "PRIVATE KEY") {
                if (auto e = crypto::ed25519::private_key::from_pkcs8_der(der)) {
                    k.ed25519.emplace(std::move(*e));
                    return true;
                }
                if (auto p = crypto::p256::private_key::from_pkcs8_der(der)) {
                    k.p256.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p384::private_key::from_pkcs8_der(der)) {
                    k.p384.emplace(std::move(*p));
                    return true;
                }
                if (auto r = crypto::rsa::private_key::from_pkcs8_der(der)) {
                    k.rsa.emplace(std::move(*r));
                    return true;
                }
                return false;
            }
            if (label == "EC PRIVATE KEY") {
                if (auto p = crypto::p256::private_key::from_sec1_der(der)) {
                    k.p256.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p384::private_key::from_sec1_der(der)) {
                    k.p384.emplace(std::move(*p));
                    return true;
                }
                return false;
            }
            if (label == "RSA PRIVATE KEY") {
                if (auto r = crypto::rsa::private_key::from_pkcs1_der(der)) {
                    k.rsa.emplace(std::move(*r));
                    return true;
                }
            }
            return false;
        }

        // Whether the key is the leaf's: a signature of the key verified
        // under the certificate's public key
        inline bool key_matches(const IdentityKey& k, const crypto::x509::certificate& leaf) noexcept {
            static constexpr uint8_t probe[] = "sgcl::net::tls identity";
            const auto content = tls::detail::bytes_of(probe, sizeof probe);
            std::vector<byte> sig;
            tls::detail::Builder w(sig);
            uint16_t scheme = 0;
            if (k.ed25519) {
                scheme = uint16_t(tls::detail::SignatureScheme::ed25519);
                tls::detail::sign(w, scheme, *k.ed25519, content);
            } else if (k.p256) {
                scheme = uint16_t(tls::detail::SignatureScheme::ecdsa_secp256r1_sha256);
                tls::detail::sign(w, scheme, *k.p256, content);
            } else if (k.p384) {
                scheme = uint16_t(tls::detail::SignatureScheme::ecdsa_secp384r1_sha384);
                tls::detail::sign(w, scheme, *k.p384, content);
            } else if (k.rsa) {
                scheme = uint16_t(tls::detail::SignatureScheme::rsa_pss_rsae_sha256);
                tls::detail::sign(w, scheme, *k.rsa, content);
            } else {
                return false;
            }
            return tls::detail::verify(scheme, leaf.public_key(), content, tls::detail::bytes_of(sig.data(), sig.size())).has_value();
        }
    }

    // A server's certificate chain with its private key: a handle of one
    // word whose state is made in the constructor; the key lives in
    // unmanaged memory and is never copied (copies of the handle, and of a
    // config that holds it, share it)
    class identity {
    public:
        // The chain (the leaf first) and the key, each in PEM; the key must
        // be the leaf's. errc::malformed (crypto) for anything else, as an
        // io::error of op "identity" (errc::unsupported for an encrypted
        // key). The key's PEM is bytes, read where they lie: a secret_bytes
        // (crypto::read_secret of a key file), a buffer of the caller's; its
        // DER goes straight into a secret_bytes, never into managed memory
        // (a string converts too, but its bytes are managed: the caller's
        // choice). The key's first block is taken (PRIVATE KEY, EC PRIVATE
        // KEY, RSA PRIVATE KEY; others passed over).
        static expected<identity, io::error> from_pem(const string& certificate_chain_pem, const slice<const byte>& key_pem) noexcept {
            auto s = make_tracked<detail::IdentityState>();
            s->key = std::make_unique<detail::IdentityKey>();
            auto blocks = encoding::pem::parse_all(certificate_chain_pem);
            if (!blocks) {
                return unexpected(_error("the certificate chain is not PEM"));
            }
            for (auto& b : *blocks) {
                if (b.type() != "CERTIFICATE") {
                    continue;
                }
                auto c = crypto::x509::certificate::parse(b.bytes().as_slice());
                if (!c) {
                    return unexpected(_error("a certificate of the chain does not parse"));
                }
                s->certificates.push_back(std::move(*c));
            }
            if (s->certificates.empty()) {
                return unexpected(_error("no certificate in the chain"));
            }
            auto block = crypto::detail::read_key_pem(key_pem);
            if (!block) {
                return unexpected(io::error(block.error().code(), "identity", block.error().message()));
            }
            if (!detail::read_key(*s->key, block->label, block->der)) {
                return unexpected(_error("no private key of a kind TLS 1.3 signs with (Ed25519, P-256, P-384, RSA)"));
            }
            if (!detail::key_matches(*s->key, s->certificates[0])) {
                return unexpected(_error("the private key is not the leaf certificate's"));
            }
            return identity(std::move(s));
        }

        // The same, a broken one thrown (std::invalid_argument)
        SGCL_INLINE_HOT identity(const string& certificate_chain_pem, const slice<const byte>& key_pem) {
            auto r = from_pem(certificate_chain_pem, key_pem);
            if (!r) {
                throw std::invalid_argument(std::string(r.error().message().view()));
            }
            _s = r->_s;
        }

        SGCL_INLINE_HOT const crypto::x509::chain& certificates() const noexcept {
            return _s->certificates;
        }

    private:
        friend struct detail::IdentityAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit identity(tracked_ptr<detail::IdentityState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT identity(sgcl::detail::FromWord, const tracked_ptr<detail::IdentityState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::IdentityState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::IdentityState>& _handle_word() const noexcept {
            return _s;
        }

        static io::error _error(const char* what) noexcept {
            return io::error(crypto::errc::malformed, "identity", string(what));
        }

        tracked_ptr<detail::IdentityState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const IdentityState& IdentityAccess::state(const identity& id) noexcept {
            return *id._s;
        }

        SGCL_INLINE_HOT tracked_ptr<const void> IdentityAccess::word(const identity& id) noexcept {
            return tracked_ptr<const void>(id._s);
        }
    }

    // The settings of a connection: a value, copied freely
    struct config {
        string server_name;                                     // SNI and the name checked; empty: the address's host
        optional<crypto::x509::certificate_pool> roots;         // none: the system's
        vector<tls::identity> identities;                       // a server's
        vector<tls::group> groups = {group::x25519_mlkem768, group::x25519, group::secp256r1, group::secp384r1};
        vector<tls::cipher> ciphers = {cipher::aes_128_gcm_sha256, cipher::chacha20_poly1305_sha256, cipher::aes_256_gcm_sha384};
        vector<string> alpn;                                    // offered, in order of preference
        bool insecure_skip_verify = false;                      // the server's chain taken unchecked (tests only)
        duration handshake_timeout = 10 * second;               // the connect and the handshake together
    };

    // What the handshake settled
    struct state {
        tls::cipher cipher = cipher::aes_128_gcm_sha256;
        tls::group group = group::x25519;
        string server_name;
        string alpn;                                            // empty: none
        crypto::x509::chain peer_certificates;                  // the leaf first, as sent
    };

    namespace detail {
        // The client's settings of a config: the shares of the first group
        // (and of X25519 beside the hybrid, as Go and the browsers send
        // them), compatibility mode on (§D.4), no record_size_limit
        inline ClientSettings client_settings(const config& c) noexcept {
            ClientSettings s;
            s.server_name = c.server_name;
            s.roots = c.roots;
            s.insecure_skip_verify = c.insecure_skip_verify;
            s.ciphers.clear();
            for (auto x : c.ciphers) {
                s.ciphers.push_back(uint16_t(x));
            }
            s.groups.clear();
            s.key_shares.clear();
            bool x25519 = false;
            for (auto g : c.groups) {
                s.groups.push_back(uint16_t(g));
                x25519 |= g == group::x25519;
            }
            if (!c.groups.empty()) {
                s.key_shares.push_back(uint16_t(c.groups[0]));
                if (c.groups[0] == group::x25519_mlkem768 && x25519) {
                    s.key_shares.push_back(uint16_t(group::x25519));
                }
            }
            s.alpn = c.alpn;
            return s;
        }

        inline io::error config_error(const char* what) noexcept {
            return io::error(std::make_error_code(std::errc::invalid_argument), "tls", string(what));
        }

        inline optional<io::error> check_client(const config& c) noexcept {
            if (c.ciphers.empty() || c.groups.empty()) {
                return config_error("a config without cipher suites or groups");
            }
            if (c.server_name.empty() && !c.insecure_skip_verify) {
                return config_error("a config without a server name to verify");
            }
            for (auto& p : c.alpn) {
                if (p.empty() || p.size() > 255) {
                    return config_error("an ALPN protocol of 0 or more than 255 bytes");
                }
            }
            return nullopt;
        }

        inline expected<net::connection, io::error> block_client(const net::connection& transport, const config& c, time_point deadline) {
            if (auto e = check_client(c)) {
                return unexpected(*e);
            }
            auto impl = make_tracked<TlsImpl>(transport, client_settings(c));
            auto r = impl->handshake(deadline);
            if (!r) {
                (void)transport.close();
                return unexpected(r.error());
            }
            return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        inline async::task<expected<net::connection, io::error>> co_client(net::connection transport, config c, time_point deadline) noexcept {
            if (auto e = check_client(c)) {
                co_return unexpected(*e);
            }
            auto impl = make_tracked<TlsImpl>(transport, client_settings(c));
            auto r = co_await impl->async_handshake(deadline);
            if (!r) {
                (void)transport.close();
                co_return unexpected(r.error());
            }
            co_return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        // The config with the address's host for a server name when it has none
        SGCL_INLINE_HOT expected<config, io::error> for_address(const string& address, const config& c) noexcept {
            config out = c;
            if (out.server_name.empty()) {
                auto t = net::detail::parse_target(address, "dial tls");
                if (!t) {
                    return unexpected(t.error());
                }
                out.server_name = t->host;
            }
            return out;
        }

        inline async::task<expected<net::connection, io::error>> co_connect(string address, config c) noexcept {
            auto cfg = for_address(address, c);
            if (!cfg) {
                co_return unexpected(cfg.error());
            }
            const time_point deadline = sgcl::clock::now() + c.handshake_timeout;
            auto t = co_await net::tcp::async_connect(address, c.handshake_timeout);
            if (!t) {
                co_return unexpected(t.error());
            }
            co_return co_await co_client(*t, *cfg, deadline);
        }
    }

    // A new TCP connection to "host:port" and the handshake over it, both
    // within config::handshake_timeout
    // `connect(...)` on this thread, `co_await async_connect(...)` in a task
    SGCL_INLINE_HOT expected<net::connection, io::error> connect(const string& address, const config& c = {}) {
        return detail::co_connect(address, c).wait();
    }

    SGCL_INLINE_HOT async::task<expected<net::connection, io::error>> async_connect(string address, config c = {}) noexcept {
        return detail::co_connect(std::move(address), std::move(c));
    }

    // The client's handshake over a connection there is (the transport's
    // deadlines are replaced by the handshake's and then removed); the
    // transport is closed when it fails
    // `client(...)` on this thread, `co_await async_client(...)` in a task
    SGCL_INLINE_HOT expected<net::connection, io::error> client(const net::connection& transport, const config& c) {
        return detail::block_client(transport, c, sgcl::clock::now() + c.handshake_timeout);
    }

    SGCL_INLINE_HOT async::task<expected<net::connection, io::error>> async_client(net::connection transport, config c) noexcept {
        const time_point deadline = sgcl::clock::now() + c.handshake_timeout;
        return detail::co_client(std::move(transport), std::move(c), deadline);
    }

    // --- the server ----------------------------------------------------------

    namespace detail {
        // A server's settings: the identities (their chains as DER, their
        // keys signing in place: `keep` holds the states the keys live in),
        // the server's preferences, no record_size_limit answered
        struct ServerSetup {
            ServerSettings settings;
            vector<tracked_ptr<const void>> keep;
        };

        inline ServerSetup server_setup(const config& c) noexcept {
            ServerSetup s;
            s.settings.ciphers.clear();
            for (auto x : c.ciphers) {
                s.settings.ciphers.push_back(uint16_t(x));
            }
            s.settings.groups.clear();
            for (auto g : c.groups) {
                s.settings.groups.push_back(uint16_t(g));
            }
            s.settings.alpn = c.alpn;
            for (const auto& id : c.identities) {
                const IdentityState& st = IdentityAccess::state(id);
                std::vector<std::vector<byte>> chain;
                for (const auto& cert : st.certificates) {
                    auto der = cert.raw();
                    chain.emplace_back(der.data(), der.data() + der.size());
                }
                const IdentityKey& k = *st.key;
                if (k.ed25519) {
                    s.settings.identities.push_back(identity_of(chain, *k.ed25519));
                } else if (k.p256) {
                    s.settings.identities.push_back(identity_of(chain, *k.p256));
                } else if (k.p384) {
                    s.settings.identities.push_back(identity_of(chain, *k.p384));
                } else {
                    s.settings.identities.push_back(identity_of(chain, *k.rsa));
                }
                s.keep.push_back(IdentityAccess::word(id));
            }
            return s;
        }

        inline optional<io::error> check_server(const config& c) noexcept {
            if (c.ciphers.empty() || c.groups.empty()) {
                return config_error("a config without cipher suites or groups");
            }
            if (c.identities.empty()) {
                return config_error("a server's config without an identity");
            }
            for (auto& p : c.alpn) {
                if (p.empty() || p.size() > 255) {
                    return config_error("an ALPN protocol of 0 or more than 255 bytes");
                }
            }
            return nullopt;
        }

        inline async::task<expected<net::connection, io::error>> co_server(net::connection transport, ServerSetup setup, time_point deadline) noexcept {
            auto impl = make_tracked<TlsImpl>(transport, setup.settings, setup.keep);
            auto r = co_await impl->async_handshake(deadline);
            if (!r) {
                (void)transport.close();
                co_return unexpected(r.error());
            }
            co_return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        // A listener whose accept gives connections after their handshake:
        // an accept loop over the listener inside, each connection's
        // handshake in a task of its own within handshake_timeout (a slow
        // client holds no other up), the ready ones through a channel; a
        // handshake that fails is dropped (its connection closed, counted)
        class TlsListenerImpl final : public net::detail::ListenerImpl {
        public:
            SGCL_INLINE_HOT TlsListenerImpl(const net::listener& inner, ServerSetup setup, duration timeout) noexcept
            : _inner(inner)
            , _setup(std::move(setup))
            , _timeout(timeout)
            , _ready(16) {
            }

            // The accept loop, started by listen() once the object is made
            SGCL_INLINE_HOT static void start(const tracked_ptr<TlsListenerImpl>& self) {
                async::go(_loop(self));
            }

            expected<net::connection, io::error> _block_accept() override {
                auto c = _ready.receive().wait();
                if (!c) {
                    return unexpected(net::detail::closed_error("accept", describe()));
                }
                return *c;
            }

            async::task<expected<net::connection, io::error>> _co_accept() noexcept override {
                auto c = co_await _ready.receive();
                if (!c) {
                    co_return unexpected(net::detail::closed_error("accept", describe()));
                }
                co_return *c;
            }

            expected<void, io::error> close() noexcept override {
                auto r = _inner.close();
                _ready.close();
                // the connections ready and not taken: what the closed
                // channel still holds, taken without a wait
                while (auto c = _ready.try_receive()) {
                    (void)c->close();
                }
                return r;
            }

            bool is_closed() const noexcept override {
                return _inner.is_closed();
            }

            endpoint local_endpoint() const noexcept override {
                return _inner.local_endpoint();
            }

            string path() const noexcept override {
                return _inner.path();
            }

            string describe() const noexcept override {
                return string("tls ") + _inner.local_endpoint().to_string();
            }

            // The handshakes that failed (timed out, refused, broken)
            SGCL_INLINE_HOT uint64_t failed() const noexcept {
                return _failed.load(std::memory_order_relaxed);
            }

        private:
            static async::task<void> _loop(tracked_ptr<TlsListenerImpl> self) noexcept {
                for (;;) {
                    auto c = co_await self->_inner.async_accept();
                    if (!c) {
                        if (self->_inner.is_closed()) {
                            self->_ready.close();
                            co_return;
                        }
                        continue;   // a connection that failed on its way in
                    }
                    async::go(_handshake(self, *c));
                }
            }

            static async::task<void> _handshake(tracked_ptr<TlsListenerImpl> self, net::connection c) noexcept {
                auto r = co_await co_server(c, self->_setup, sgcl::clock::now() + self->_timeout);
                if (!r) {
                    self->_failed.fetch_add(1, std::memory_order_relaxed);
                    co_return;   // dropped: co_server closed the transport
                }
                if (!co_await self->_ready.send(*r)) {
                    (void)r->close();   // the listener closed meanwhile
                }
            }

            net::listener _inner;
            ServerSetup _setup;
            duration _timeout;
            async::channel<net::connection> _ready;
            std::atomic<uint64_t> _failed = 0;
        };

        SGCL_INLINE_HOT expected<net::listener, io::error> make_listener(const net::listener& inner, const config& c) {
            tracked_ptr<TlsListenerImpl> impl = make_tracked<TlsListenerImpl>(inner, server_setup(c), c.handshake_timeout);
            TlsListenerImpl::start(impl);
            return net::detail::ListenerAccess::make(tracked_ptr<net::detail::ListenerImpl>(std::move(impl)));
        }
    }

    // A TCP listener whose accept() gives connections whose handshake is
    // done: each handshake runs in a task of its own, bounded by
    // config::handshake_timeout, and one that fails is dropped
    // `listen(...)` on this thread, `co_await async_listen(...)` in a task
    SGCL_INLINE_HOT expected<net::listener, io::error> listen(const string& address, const config& c) {
        if (auto e = detail::check_server(c)) {
            return unexpected(*e);
        }
        auto l = net::tcp::listen(address);
        if (!l) {
            return unexpected(l.error());
        }
        return detail::make_listener(*l, c);
    }

    inline async::task<expected<net::listener, io::error>> async_listen(string address, config c) noexcept {
        if (auto e = detail::check_server(c)) {
            co_return unexpected(*e);
        }
        auto l = co_await net::tcp::async_listen(address);
        if (!l) {
            co_return unexpected(l.error());
        }
        co_return detail::make_listener(*l, c);
    }



    // The server's handshake over a connection there is (one accepted);
    // the transport is closed when it fails
    // `server(...)` on this thread, `co_await async_server(...)` in a task
    inline expected<net::connection, io::error> server(const net::connection& transport, const config& c) {
        if (auto e = detail::check_server(c)) {
            return unexpected(*e);
        }
        auto setup = detail::server_setup(c);
        auto impl = make_tracked<detail::TlsImpl>(transport, setup.settings, setup.keep);
        auto r = impl->handshake(sgcl::clock::now() + c.handshake_timeout);
        if (!r) {
            (void)transport.close();
            return unexpected(r.error());
        }
        return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
    }

    inline async::task<expected<net::connection, io::error>> async_server(net::connection transport, config c) noexcept {
        if (auto e = detail::check_server(c)) {
            co_return unexpected(*e);
        }
        co_return co_await detail::co_server(std::move(transport), detail::server_setup(c), sgcl::clock::now() + c.handshake_timeout);
    }

    // What the handshake of a TLS connection settled; nullopt for a
    // connection without TLS
    inline optional<state> state_of(const net::connection& c) noexcept {
        if (!c) {
            return nullopt;
        }
        auto* t = dynamic_cast<const detail::TlsImpl*>(&net::detail::ConnectionAccess::impl(c));
        if (!t) {
            return nullopt;
        }
        state s;
        if (t->is_server()) {
            const auto& r = t->server_result();
            s.cipher = tls::cipher(uint16_t(r.cipher));
            s.group = tls::group(uint16_t(r.group));
            s.server_name = r.server_name;
            s.alpn = r.alpn;
            return s;
        }
        const auto& r = t->result();
        s.cipher = tls::cipher(uint16_t(r.cipher));
        s.group = tls::group(uint16_t(r.group));
        s.server_name = r.server_name;
        s.alpn = r.alpn;
        s.peer_certificates = r.peer_certificates;
        return s;
    }
}
