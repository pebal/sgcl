//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Client certificates (mTLS) and session resumption of sgcl/net/tls.h, our
// client and our server over the loopback:
//
//   - resumption: a full handshake whose ticket the next connection
//     resumes (both sides say so, the server's chain kept), the ticket used
//     once and replaced, tickets off on the server, no cache on the
//     client, a ticket of other keys or of a key rotated twice (a full
//     handshake), a key rotated once (still resumed), a lifetime over, the
//     cache's capacity, its key (another port, other protocols);
//   - mTLS: required and given (the chain on the server's state, on both
//     sides' handshakes of every kind of key), required and missing
//     (certificate_required), asked for and missing, a chain of another
//     authority (unknown_ca), a leaf for servers only (bad_certificate), the
//     identity chosen by the authorities the server names, a resumed
//     session that keeps the client's chain, and one without a chain where
//     a chain is now required;
//   - the settings at their edges: the lifetime's bounds, a cache of 0, a
//     moved-from cache and keys.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/net/tls.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace tls = sgcl::net::tls;

namespace {
    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    tls::identity identity_of(const std::string& name) {
        return tls::identity(sgcl::string(slurp(testdata(name + ".pem"))), sgcl::string(slurp(testdata(name + ".key"))));
    }

    crypto::x509::certificate_pool pool_of(const std::string& name) {
        return crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata(name + ".pem"))));
    }

    tls::config server_config(const std::string& leaf = "ecdsa") {
        tls::config c;
        c.identities = {identity_of(leaf)};
        return c;
    }

    tls::config client_config() {
        tls::config c;
        c.roots = pool_of("ca");
        c.server_name = sgcl::string("localhost");
        return c;
    }

    // What the server side of one connection saw (std types only: it is
    // written on the server's thread into the test's stack, where a tracked
    // word of another thread may not be)
    struct Served {
        bool ok = false;                  // the handshake done
        bool resumed = false;
        std::vector<std::string> peers;   // the client's chain: the subjects' common names
        std::string error;                // the handshake's
        std::string line;
    };

    // One connection served on its own thread, on a listener of the
    // test's (one port: the client's cache keys its sessions by it): the
    // handshake (tls::server, so that its failure is seen), a line read,
    // "reply" written, the end read
    struct OneServer {
        net::listener l;
        std::thread t;
        Served served;

        OneServer(const net::listener& on, const tls::config& cfg)
        : l(on) {
            t = std::thread([this, &cfg] {
                auto a = l.accept();
                if (!a) {
                    served.error = "accept";
                    return;
                }
                auto s = tls::server(*a, cfg);
                if (!s) {
                    served.error = std::string(s.error().message().view());
                    return;
                }
                auto st = tls::state_of(*s);
                served.ok = true;
                served.resumed = st->resumed;
                for (const auto& c : st->peer_certificates) {
                    served.peers.emplace_back(c.subject().common_name().view());
                }
                auto line = s->read_line();
                if (line && *line) {
                    served.line = std::string((*line)->view());
                }
                (void)s->write(sgcl::string("reply\n"));
                byte buf[256];
                while (true) {
                    auto n = s->read(buf);
                    if (!n || *n == 0) {
                        break;
                    }
                }
                (void)s->close();
            });
        }

        std::string address() const {
            return "127.0.0.1:" + std::to_string(l.local_endpoint().port());
        }

        void join() {
            t.join();
        }

        ~OneServer() {
            if (t.joinable()) {
                join();
            }
        }
    };

    // What the client side of one connection saw: its state, the reply
    // read, or the error of the handshake or of the read
    struct Talked {
        optional<tls::state> state;
        std::string error;
        std::string reply;
    };

    Talked talk(const std::string& address, const tls::config& cfg) {
        Talked t;
        auto c = tls::connect(sgcl::string(address), cfg);
        if (!c) {
            t.error = std::string(c.error().message().view());
            return t;
        }
        t.state = tls::state_of(*c);
        (void)c->write(sgcl::string("hello\n"));
        auto r = c->read_line();
        if (!r) {
            t.error = std::string(r.error().message().view());
        } else if (*r) {
            t.reply = std::string((*r)->view());
        }
        (void)c->close();
        return t;
    }

    // The test's port
    struct Port {
        net::listener l = *net::tcp::listen("127.0.0.1:0");

        ~Port() {
            (void)l.close();
        }
    };

    // One connection to a server of this config on the port, served and talked to
    pair<Talked, Served> exchange(const Port& port, const tls::config& server, const tls::config& client) {
        OneServer s(port.l, server);
        Talked t = talk(s.address(), client);
        s.join();
        return {t, s.served};
    }

    bool contains(const std::string& text, const std::string& what) {
        return text.find(what) != std::string::npos;
    }
}

// --- resumption ---------------------------------------------------------------

TEST(TlsResumption, TheNextConnectionResumes) {
    Port port;
    tls::config server = server_config();
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    auto [first, first_served] = exchange(port, server, client);
    ASSERT_TRUE(first.state) << first.error;
    EXPECT_EQ(first.reply, "reply");
    EXPECT_FALSE(first.state->resumed);
    EXPECT_FALSE(first_served.resumed);
    EXPECT_EQ(client.session_cache->size(), 1u);   // the ticket that came before the reply
    auto [second, second_served] = exchange(port, server, client);
    ASSERT_TRUE(second.state) << second.error;
    EXPECT_EQ(second.reply, "reply");
    EXPECT_EQ(second_served.line, "hello");
    EXPECT_TRUE(second.state->resumed);
    ASSERT_TRUE(second_served.ok);
    EXPECT_TRUE(second_served.resumed);
    // the server's chain of the handshake that made the session
    ASSERT_EQ(second.state->peer_certificates.size(), first.state->peer_certificates.size());
    auto a = second.state->peer_certificates[0].raw();
    auto b = first.state->peer_certificates[0].raw();
    EXPECT_TRUE(a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()));
    EXPECT_EQ(client.session_cache->size(), 1u);   // the ticket used, a new one in its place
    // and the one after resumes again
    auto [third, third_served] = exchange(port, server, client);
    ASSERT_TRUE(third.state) << third.error;
    EXPECT_TRUE(third.state->resumed);
}

TEST(TlsResumption, EveryCipherSuite) {
    Port port;
    for (auto c : {tls::cipher::aes_128_gcm_sha256, tls::cipher::aes_256_gcm_sha384, tls::cipher::chacha20_poly1305_sha256}) {
        SCOPED_TRACE(int(c));
        tls::config server = server_config();
        server.ciphers = {c};
        tls::config client = client_config();
        client.session_cache = tls::session_cache();
        (void)exchange(port, server, client);
        auto [t, s] = exchange(port, server, client);
        ASSERT_TRUE(t.state) << t.error;
        EXPECT_TRUE(t.state->resumed);
        EXPECT_EQ(t.state->cipher, c);
    }
}

// A session of SHA-384 offered to a server that now prefers a suite of
// SHA-256: the hash differs, a full handshake (RFC 8446 §4.2.11)
TEST(TlsResumption, ASuiteOfAnotherHashIsAFullHandshake) {
    Port port;
    tls::config server = server_config();
    server.ciphers = {tls::cipher::aes_256_gcm_sha384};
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    ASSERT_EQ(client.session_cache->size(), 1u);
    server.ciphers = {tls::cipher::aes_128_gcm_sha256};
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_FALSE(t.state->resumed);
    EXPECT_EQ(t.state->cipher, tls::cipher::aes_128_gcm_sha256);
}

// A HelloRetryRequest between: the second ClientHello offers the session
// again with a binder over the new transcript
TEST(TlsResumption, AfterAHelloRetryRequest) {
    Port port;
    tls::config server = server_config();
    server.groups = {tls::group::secp256r1};
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_TRUE(t.state->resumed);
    EXPECT_EQ(t.state->group, tls::group::secp256r1);
}

TEST(TlsResumption, NoTicketsNoCacheNoResumption) {
    Port port;
    // tickets off on the server: nothing to keep
    tls::config server = server_config();
    server.session_tickets = false;
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    EXPECT_EQ(client.session_cache->size(), 0u);
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_FALSE(t.state->resumed);
    // no cache on the client: the tickets passed over
    tls::config plain = client_config();
    server.session_tickets = true;
    (void)exchange(port, server, plain);
    auto [u, v] = exchange(port, server, plain);
    ASSERT_TRUE(u.state) << u.error;
    EXPECT_FALSE(u.state->resumed);
}

TEST(TlsResumption, ATicketOfOtherKeysIsAFullHandshake) {
    Port port;
    tls::config server = server_config();
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    ASSERT_EQ(client.session_cache->size(), 1u);
    tls::config other = server_config();   // keys of its own
    auto [t, s] = exchange(port, other, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_FALSE(t.state->resumed);
    EXPECT_FALSE(s.resumed);
    EXPECT_EQ(client.session_cache->size(), 1u);   // the new server's ticket
}

TEST(TlsResumption, RotatedOnceStillResumesTwiceNot) {
    Port port;
    tls::config server = server_config();
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    server.ticket_keys.rotate();
    auto [once, s1] = exchange(port, server, client);
    ASSERT_TRUE(once.state) << once.error;
    EXPECT_TRUE(once.state->resumed);   // the previous key opens it
    // the ticket of that connection is of the current key: two rotations later it is gone
    server.ticket_keys.rotate();
    server.ticket_keys.rotate();
    auto [twice, s2] = exchange(port, server, client);
    ASSERT_TRUE(twice.state) << twice.error;
    EXPECT_FALSE(twice.state->resumed);
}

// A copy of the config shares its keys: a listener of one and a server of
// the other resume each other's tickets
TEST(TlsResumption, CopiesOfAConfigShareTheKeys) {
    Port port;
    tls::config server = server_config();
    tls::config copy = server;
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    auto [t, s] = exchange(port, copy, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_TRUE(t.state->resumed);
}

TEST(TlsResumption, ALifetimeOver) {
    Port port;
    tls::config server = server_config();
    server.ticket_lifetime = sgcl::second;
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    ASSERT_EQ(client.session_cache->size(), 1u);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_FALSE(t.state->resumed);
}

// The cache's key: the server's name, port and protocols. A session of
// another port or of other ALPN protocols is not offered
TEST(TlsResumption, TheKeyOfASession) {
    Port port, other_port;
    tls::config server = server_config();
    server.alpn = {sgcl::string("a"), sgcl::string("b")};
    tls::config client = client_config();
    client.alpn = {sgcl::string("a")};
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    auto [t, s] = exchange(other_port, server, client);   // the same server's keys, another port
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_FALSE(t.state->resumed);
    EXPECT_EQ(client.session_cache->size(), 2u);
    tls::config other = client;
    other.alpn = {sgcl::string("b")};
    auto [u, v] = exchange(port, server, other);   // other protocols
    ASSERT_TRUE(u.state) << u.error;
    EXPECT_FALSE(u.state->resumed);
    EXPECT_EQ(u.state->alpn, "b");
    auto [w, x] = exchange(port, server, client);   // the port's and the protocols' own
    ASSERT_TRUE(w.state) << w.error;
    EXPECT_TRUE(w.state->resumed);
    EXPECT_EQ(w.state->alpn, "a");
    // another server name: the leaf is for localhost and 127.0.0.1 both
    tls::config by_address = client;
    by_address.server_name = sgcl::string("127.0.0.1");
    auto [y, z] = exchange(port, server, by_address);
    ASSERT_TRUE(y.state) << y.error;
    EXPECT_FALSE(y.state->resumed);
}

TEST(TlsResumption, TheCachesBounds) {
    Port port;
    tls::session_cache none(0);
    EXPECT_EQ(none.capacity(), 0u);
    tls::config server = server_config();
    tls::config client = client_config();
    client.session_cache = none;
    (void)exchange(port, server, client);
    EXPECT_EQ(none.size(), 0u);
    auto [t, s] = exchange(port, server, client);
    EXPECT_FALSE(t.state->resumed);
    // a capacity of 2 over three servers: the oldest dropped
    tls::session_cache two(2);
    client.session_cache = two;
    for (int i = 0; i < 3; ++i) {
        Port each;
        (void)exchange(each, server, client);
    }
    EXPECT_EQ(two.size(), 2u);

    two.clear();
    EXPECT_EQ(two.size(), 0u);
    two.clear();
    EXPECT_EQ(two.size(), 0u);
    // the default, and copies and moved-from handles: the same cache
    tls::session_cache d;
    EXPECT_EQ(d.capacity(), 64u);
    tls::session_cache copy = d;
    tls::session_cache moved = std::move(d);
    client.session_cache = copy;
    (void)exchange(port, server, client);
    EXPECT_EQ(moved.size(), 1u);
    EXPECT_EQ(d.size(), 1u);   // NOLINT: a moved-from handle is the same cache
    tls::ticket_keys keys;
    tls::ticket_keys moved_keys = std::move(keys);
    keys.rotate();   // NOLINT: the same keys
    moved_keys.rotate();
}

TEST(TlsResumption, TheLifetimesBounds) {
    Port port;
    tls::config server = server_config();
    for (auto lifetime : {sgcl::duration::zero(), sgcl::millisecond * 999, -sgcl::second, 7 * 24 * sgcl::hour + sgcl::second}) {
        server.ticket_lifetime = lifetime;
        auto l = tls::listen("127.0.0.1:0", server);
        ASSERT_FALSE(l.has_value());
        EXPECT_EQ(l.error().code(), std::errc::invalid_argument);
        auto t = net::tcp::listen("127.0.0.1:0");
        auto c = net::tcp::connect(t->local_endpoint());
        auto s = tls::server(*c, server);
        ASSERT_FALSE(s.has_value());
        EXPECT_EQ(s.error().code(), std::errc::invalid_argument);
        (void)t->close();
    }
    // tickets off: the lifetime not read
    server.session_tickets = false;
    server.ticket_lifetime = sgcl::duration::zero();
    auto l = tls::listen("127.0.0.1:0", server);
    EXPECT_TRUE(l.has_value());
    (void)l->close();
    // the bounds themselves taken
    server.session_tickets = true;
    for (auto lifetime : {sgcl::second, 7 * 24 * sgcl::hour}) {
        server.ticket_lifetime = lifetime;
        tls::config client = client_config();
        client.session_cache = tls::session_cache();
        (void)exchange(port, server, client);
        auto [t, s] = exchange(port, server, client);
        ASSERT_TRUE(t.state) << t.error;
        EXPECT_TRUE(t.state->resumed);
    }
}

// --- mTLS ---------------------------------------------------------------------

TEST(TlsClientAuth, RequiredAndGivenEveryKind) {
    Port port;
    for (const char* kind : {"client_ecdsa", "client_ed25519", "client_rsa"}) {
        SCOPED_TRACE(kind);
        tls::config server = server_config();
        server.client_auth = tls::client_auth::require;
        server.client_roots = pool_of("ca");
        tls::config client = client_config();
        client.identities = {identity_of(kind)};
        auto [t, s] = exchange(port, server, client);
        ASSERT_TRUE(t.state) << t.error;
        EXPECT_EQ(t.reply, "reply") << t.error;
        ASSERT_TRUE(s.ok) << s.error;
        ASSERT_EQ(s.peers.size(), 1u);
        EXPECT_EQ(s.peers[0], std::string("sgcl test client ") + (kind + 7));
    }
}

TEST(TlsClientAuth, RequiredAndMissing) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::require;
    server.client_roots = pool_of("ca");
    auto [t, s] = exchange(port, server, client_config());
    // the client's handshake is done before the server reads its Certificate
    // (TLS 1.3): it reads the alert
    EXPECT_TRUE(contains(t.error, "remote error: tls: certificate required")) << t.error;
    EXPECT_TRUE(contains(s.error, "tls: certificate required")) << s.error;
    EXPECT_FALSE(s.ok);
}

TEST(TlsClientAuth, AskedForAndMissing) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::request;
    server.client_roots = pool_of("ca");
    auto [t, s] = exchange(port, server, client_config());
    ASSERT_TRUE(t.state) << t.error;
    EXPECT_EQ(t.reply, "reply");
    ASSERT_TRUE(s.ok) << s.error;
    EXPECT_TRUE(s.peers.empty());
    // and given, verified
    tls::config client = client_config();
    client.identities = {identity_of("client_ecdsa")};
    auto [u, v] = exchange(port, server, client);
    ASSERT_TRUE(v.ok) << v.error;
    EXPECT_EQ(v.peers.size(), 1u);
}

TEST(TlsClientAuth, NotAskedForNothingSent) {
    Port port;
    tls::config client = client_config();
    client.identities = {identity_of("client_ecdsa")};
    auto [t, s] = exchange(port, server_config(), client);
    ASSERT_TRUE(s.ok) << s.error;
    EXPECT_TRUE(s.peers.empty());
    EXPECT_EQ(t.reply, "reply");
}

// A chain of an authority the server does not trust: unknown_ca. (With
// client roots the server names their authorities, and a client with no
// certificate of theirs sends none: asked for, the handshake goes on
// without; the system's roots name none, so the chain is sent and refused)
TEST(TlsClientAuth, AChainOfAnotherAuthority) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::request;   // given, it must verify
    tls::config client = client_config();
    client.identities = {identity_of("other_client")};
    auto [t, s] = exchange(port, server, client);
    EXPECT_TRUE(contains(s.error, "tls: certificate signed by unknown authority")) << s.error;
    EXPECT_TRUE(contains(t.error, "remote error: tls: unknown certificate authority")) << t.error;
    server.client_roots = pool_of("ca");
    auto [u, v] = exchange(port, server, client);
    ASSERT_TRUE(v.ok) << v.error;
    EXPECT_TRUE(v.peers.empty());
}

TEST(TlsClientAuth, ALeafForServersOnly) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::require;
    server.client_roots = pool_of("ca");
    tls::config client = client_config();
    client.identities = {identity_of("ecdsa")};   // extendedKeyUsage serverAuth
    auto [t, s] = exchange(port, server, client);
    EXPECT_TRUE(contains(s.error, "incompatible key usage")) << s.error;
    EXPECT_TRUE(contains(t.error, "remote error: tls: bad certificate")) << t.error;
}

// The authorities the server names choose the identity: the first of the
// client's issued by one of them; a server that names none (the system's
// roots) gets the first
TEST(TlsClientAuth, TheIdentityByTheAuthoritiesNamed) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::require;
    server.client_roots = pool_of("ca");
    tls::config client = client_config();
    client.identities = {identity_of("other_client"), identity_of("client_ed25519")};
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(s.ok) << s.error;
    EXPECT_EQ(s.peers[0], "sgcl test client ed25519");
    // both authorities named: the first
    crypto::x509::certificate_pool both = pool_of("ca");
    both.append_pem(sgcl::string(slurp(testdata("other_ca.pem"))));
    server.client_roots = both;
    auto [u, v] = exchange(port, server, client);
    ASSERT_TRUE(v.ok) << v.error;
    EXPECT_EQ(v.peers[0], "sgcl other test client");
    // none named (the system's roots): the first, refused
    server.client_roots = nullopt;
    auto [w, x] = exchange(port, server, client);
    EXPECT_TRUE(contains(x.error, "unknown authority")) << x.error;
}

TEST(TlsClientAuth, AResumedSessionKeepsTheClientsChain) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::require;
    server.client_roots = pool_of("ca");
    tls::config client = client_config();
    client.identities = {identity_of("client_ecdsa")};
    client.session_cache = tls::session_cache();
    auto [t, s] = exchange(port, server, client);
    ASSERT_TRUE(s.ok) << s.error;
    EXPECT_FALSE(s.resumed);
    auto [u, v] = exchange(port, server, client);
    ASSERT_TRUE(v.ok) << v.error;
    EXPECT_TRUE(v.resumed);
    ASSERT_EQ(v.peers.size(), 1u);
    EXPECT_EQ(v.peers[0], "sgcl test client ecdsa");
}

// A session made without a client certificate is not resumed where one is
// now required: a full handshake, which asks for it
TEST(TlsClientAuth, ASessionWithoutAChainWhereOneIsRequired) {
    Port port;
    tls::config server = server_config();
    server.client_auth = tls::client_auth::request;
    server.client_roots = pool_of("ca");
    tls::config client = client_config();
    client.session_cache = tls::session_cache();
    (void)exchange(port, server, client);
    ASSERT_EQ(client.session_cache->size(), 1u);
    tls::config strict = server;   // the same keys
    strict.client_auth = tls::client_auth::require;
    auto [t, s] = exchange(port, strict, client);
    EXPECT_TRUE(contains(s.error, "certificate required")) << s.error;
    // given a certificate now, the full handshake goes through
    client.identities = {identity_of("client_ecdsa")};
    auto [u, v] = exchange(port, strict, client);
    ASSERT_TRUE(v.ok) << v.error;
    EXPECT_FALSE(v.resumed);
}

TEST(TlsClientAuth, TheListenersHandshakeDropsAClientWithout) {
    tls::config server = server_config();
    server.client_auth = tls::client_auth::require;
    server.client_roots = pool_of("ca");
    auto l = tls::listen("127.0.0.1:0", server);
    ASSERT_TRUE(l.has_value());
    const std::string address = "127.0.0.1:" + std::to_string(l->local_endpoint().port());
    std::string seen;
    std::thread accepting([&] {
        auto c = l->accept();
        if (c) {
            auto st = tls::state_of(*c);
            seen = st && !st->peer_certificates.empty() ? std::string(st->peer_certificates[0].subject().common_name().view()) : "none";
            (void)c->write(sgcl::string("reply\n"));
            byte buf[64];
            (void)c->read(buf);
            (void)c->close();
        }
    });
    Talked without = talk(address, client_config());
    EXPECT_TRUE(contains(without.error, "certificate required")) << without.error;
    tls::config client = client_config();
    client.identities = {identity_of("client_ecdsa")};
    Talked with = talk(address, client);
    accepting.join();
    (void)l->close();
    EXPECT_EQ(with.reply, "reply") << with.error;
    EXPECT_EQ(seen, "sgcl test client ecdsa");
}

TEST(TlsClientAuth, AClientAuthOfNoValue) {
    tls::config server = server_config();
    server.client_auth = tls::client_auth(7);
    auto l = tls::listen("127.0.0.1:0", server);
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error().code(), std::errc::invalid_argument);
}
