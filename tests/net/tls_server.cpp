//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's handshake machine (sgcl/net/tls/detail/server_handshake.h):
//
//   - RFC 8448 §3 (1-RTT), §5 (HelloRetryRequest to P-256, the cookie) and
//     §7 (compatibility mode) as the server, on the trace's randomness: the
//     HelloRetryRequest, the ServerHello and the encrypted flight byte for
//     byte, every traffic secret it installs, the order of the actions (the
//     signature of CertificateVerify is RSA-PSS with a salt the trace does
//     not give: the trace's signature is written in place of the key's,
//     after it is checked to verify over the content the machine made);
//   - §4, 0-RTT refused: the early data skipped, a full handshake;
//   - our client and our server against each other, in memory: every
//     group, every suite, every kind of key, a HelloRetryRequest, ALPN
//     (RFC 7301, no_application_protocol), the identity chosen by SNI and
//     by signature scheme, KeyUpdate both ways;
//   - the refusals: no TLS 1.3, nothing in common, extensions missing,
//     messages out of order, a Finished that does not verify, a second
//     ClientHello that is not what the HelloRetryRequest asked for.
#include "tests/types.h"

#include "sgcl/net/tls/detail/server_handshake.h"
#include "tls_rfc8448.h"
#include "tls_server_identities.h"

#include <map>
#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;
    using tls::Action;
    using tls::AlertDescription;

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    bytes_t of(const tls::Secret& s) {
        return bytes_t(s.bytes, s.bytes + s.size);
    }

    bytes_t unhex(const char* s) {
        bytes_t v;
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3] = {s[i], s[i + 1], 0};
            v.push_back(uint8_t(std::strtoul(b, nullptr, 16)));
        }
        return v;
    }

    std::vector<sgcl::byte> bytes(const bytes_t& v) {
        auto p = reinterpret_cast<const sgcl::byte*>(v.data());
        return std::vector<sgcl::byte>(p, p + v.size());
    }

    struct Queue {
        bytes_t data;
        size_t at = 0;

        static void fill(void* self, uint8_t* out, size_t n) {
            auto& q = *static_cast<Queue*>(self);
            for (size_t i = 0; i < n; ++i) {
                out[i] = q.at < q.data.size() ? q.data[q.at++] : 0;
            }
        }

        tls::Entropy entropy() {
            return tls::Entropy{&Queue::fill, this};
        }
    };

    // Everything a machine asked for, in order
    struct Log {
        std::vector<bytes_t> sent;
        std::vector<std::string> kinds;
        std::map<std::string, bytes_t> installs;   // "write hs" ... the last of each
        std::vector<AlertDescription> alerts;
        size_t skip = 0;
        bool established = false;

        void take(const tls::Step& step) {
            for (auto& a : step.actions) {
                switch (a.kind) {
                case Action::Kind::send:
                    sent.push_back(of(step.bytes(a)));
                    kinds.push_back("send");
                    break;
                case Action::Kind::change_cipher_spec:
                    kinds.push_back("ccs");
                    break;
                case Action::Kind::install_read:
                case Action::Kind::install_write: {
                    std::string k = std::string(a.kind == Action::Kind::install_read ? "read " : "write ") + (a.epoch == tls::Epoch::handshake ? "hs" : "ap");
                    installs[k] = of(a.secret);
                    kinds.push_back(k);
                    break;
                }
                case Action::Kind::update_read:
                    kinds.push_back("update read");
                    break;
                case Action::Kind::update_write:
                    kinds.push_back("update write");
                    break;
                case Action::Kind::established:
                    kinds.push_back("established");
                    established = true;
                    break;
                case Action::Kind::alert:
                    alerts.push_back(a.alert);
                    kinds.push_back("alert");
                    break;
                case Action::Kind::skip_early_data:
                    skip = a.size;
                    kinds.push_back("skip early data");
                    break;
                case Action::Kind::new_ticket:   // the client's (tls_resumption.cpp)
                    kinds.push_back("new ticket");
                    break;
                }
            }
        }
    };

    const std::vector<rfc8448::Step>& steps() {
        static const std::vector<rfc8448::Step> s = rfc8448::read();
        return s;
    }

    // The messages in the bytes of one send (a flight may hold several)
    std::vector<bytes_t> split(const bytes_t& flight) {
        std::vector<bytes_t> out;
        size_t at = 0;
        while (at + 4 <= flight.size()) {
            size_t n = size_t(flight[at + 1]) << 16 | size_t(flight[at + 2]) << 8 | flight[at + 3];
            out.emplace_back(flight.begin() + long(at), flight.begin() + long(at + 4 + n));
            at += 4 + n;
        }
        return out;
    }

    // --- the identities -----------------------------------------------------------

    struct Keys {
        sgcl::crypto::ed25519::private_key ed25519;
        sgcl::crypto::p256::private_key p256, other;
        sgcl::crypto::p384::private_key p384;
        sgcl::crypto::rsa::private_key rsa, rsa1024;
        std::map<std::string, std::vector<std::vector<sgcl::byte>>> chains;

        static const tls_identities::Identity& find(const char* name) {
            for (auto& i : tls_identities::all) {
                if (std::string(i.name) == name) {
                    return i;
                }
            }
            throw std::logic_error("no such identity");
        }

        static bytes_t key(const char* name) {
            return unhex(find(name).key);
        }

        Keys()
        : ed25519(sgcl::crypto::ed25519::private_key::from_pkcs8_der(view(key("ed25519"))).value())
        , p256(sgcl::crypto::p256::private_key::from_pkcs8_der(view(key("p256"))).value())
        , other(sgcl::crypto::p256::private_key::from_pkcs8_der(view(key("other"))).value())
        , p384(sgcl::crypto::p384::private_key::from_pkcs8_der(view(key("p384"))).value())
        , rsa(sgcl::crypto::rsa::private_key::from_pkcs8_der(view(key("rsa"))).value())
        , rsa1024(sgcl::crypto::rsa::private_key::from_pkcs8_der(view(key("rsa1024"))).value()) {
            for (auto& i : tls_identities::all) {
                chains[i.name] = {bytes(unhex(i.certificate))};
            }
        }

        tls::ServerIdentity identity(const std::string& name) const {
            auto& c = chains.at(name);
            if (name == "ed25519") return tls::identity_of(c, ed25519);
            if (name == "p256") return tls::identity_of(c, p256);
            if (name == "other") return tls::identity_of(c, other);
            if (name == "p384") return tls::identity_of(c, p384);
            if (name == "rsa1024") return tls::identity_of(c, rsa1024);
            return tls::identity_of(c, rsa);
        }
    };

    const Keys& keys() {
        static const Keys k;
        return k;
    }

    // --- RFC 8448 as the server ---------------------------------------------------

    // The signature of the trace, written in place of one the key would
    // make (RSA-PSS draws a salt the trace does not give); it must verify
    // over the content the machine hands the signer
    struct TraceSigner {
        bytes_t signature;
        std::optional<sgcl::crypto::x509::certificate> certificate;
        int verified = 0;
    };

    TraceSigner* trace_signer = nullptr;

    void sign_as_trace(const void*, uint16_t scheme, const tls::Bytes& content, tls::Builder& out) {
        auto& t = *trace_signer;
        if (tls::verify(scheme, t.certificate->public_key(), content, view(t.signature))) {
            ++t.verified;
        }
        out.bytes(t.signature.data(), t.signature.size());
    }

    struct ServerTrace {
        std::vector<rfc8448::Message> client, server;
        tls::ServerSettings settings;
        Queue entropy;
        std::map<std::string, bytes_t> secrets;
        TraceSigner signer;
    };

    void trace_of(ServerTrace& t, int section) {
        for (auto& m : rfc8448::messages(steps())) {
            if (m.section == section) {
                (m.who == "client" ? t.client : t.server).push_back(m);
            }
        }
        bytes_t key;
        for (auto& s : steps()) {
            if (s.section != section) {
                continue;
            }
            if (s.who == "server" && s.title.rfind("create an ephemeral", 0) == 0) {
                key = s.fields.at("private key");
            }
            for (const char* label : {"c hs traffic", "s hs traffic", "c ap traffic", "s ap traffic"}) {
                if (s.title.rfind(std::string("derive secret \"tls13 ") + label + "\"", 0) == 0 && s.fields.count("expanded")) {
                    t.secrets[label] = s.fields.at("expanded");
                }
            }
        }
        auto& st = t.settings;
        st.ciphers = {0x1301};
        for (auto& m : t.server) {
            auto h = tls::read_handshake(view(m.bytes));
            if (m.name == "ServerHello") {
                auto sh = tls::read_server_hello(h->body);
                if (sh->is_retry()) {
                    st.retry_cookie = bytes(of(*tls::read_cookie(*sh->extensions.find(tls::ExtensionType::cookie))));
                    st.groups = {*tls::read_key_share_retry(*sh->extensions.find(tls::ExtensionType::key_share))};
                } else {
                    auto r = of(sh->random);
                    t.entropy.data.insert(t.entropy.data.end(), r.begin(), r.end());
                    if (st.retry_cookie.empty()) {
                        st.groups = {tls::read_key_share_selected(*sh->extensions.find(tls::ExtensionType::key_share))->group};
                    }
                }
            } else if (m.name == "EncryptedExtensions") {
                auto x = tls::read_encrypted_extensions(h->body);
                if (auto g = x->find(tls::ExtensionType::supported_groups)) {
                    auto l = tls::read_groups(*g);
                    st.advertised_groups.assign(l->begin(), l->end());
                }
                if (auto l = x->find(tls::ExtensionType::record_size_limit)) {
                    st.record_size_limit = *tls::read_record_size_limit(*l);
                }
            } else if (m.name == "Certificate" && t.signer.signature.empty()) {
                auto c = tls::read_certificate(h->body);
                auto leaf = *c->begin();
                t.signer.certificate = sgcl::crypto::x509::certificate::parse(leaf.der).value();
                tls::ServerIdentity id;
                id.chain = {bytes(of(leaf.der))};
                id.leaf = *t.signer.certificate;
                id.schemes = {uint16_t(tls::SignatureScheme::rsa_pss_rsae_sha256)};
                id.sign = &sign_as_trace;
                st.identities.push_back(id);
            } else if (m.name == "CertificateVerify" && t.signer.signature.empty()) {
                t.signer.signature = of(tls::read_certificate_verify(h->body)->signature);
            }
        }
        t.entropy.data.insert(t.entropy.data.end(), key.begin(), key.end());
    }

    bytes_t concat(const std::vector<rfc8448::Message>& ms, std::initializer_list<const char*> names, size_t from = 0) {
        bytes_t out;
        for (const char* name : names) {
            for (size_t i = from; i < ms.size(); ++i) {
                if (ms[i].name == name) {
                    out.insert(out.end(), ms[i].bytes.begin(), ms[i].bytes.end());
                    break;
                }
            }
        }
        return out;
    }

    // --- our client and our server --------------------------------------------------

    struct Pair {
        tls::ClientHandshake client;
        tls::ServerHandshake server;
        Log client_log, server_log;

        Pair(const tls::ClientSettings& c, const tls::ServerSettings& s)
        : client(c), server(s) {
        }

        // The client's first flight, then the messages back and forth until
        // neither side has any to send
        void run() {
            std::vector<bytes_t> to_server, to_client;
            auto deliver = [](const tls::Step& step, Log& log, std::vector<bytes_t>& to) {
                size_t before = log.sent.size();
                log.take(step);
                for (size_t i = before; i < log.sent.size(); ++i) {
                    for (auto& m : split(log.sent[i])) {
                        to.push_back(m);
                    }
                }
            };
            deliver(client.start(), client_log, to_server);
            for (int round = 0; round < 10 && (!to_server.empty() || !to_client.empty()); ++round) {
                auto s = std::move(to_server);
                to_server.clear();
                for (auto& m : s) {
                    deliver(server.feed(view(m)), server_log, to_client);
                }
                auto c = std::move(to_client);
                to_client.clear();
                for (auto& m : c) {
                    deliver(client.feed(view(m)), client_log, to_server);
                }
            }
        }
    };

    tls::ClientSettings client_settings() {
        tls::ClientSettings c;
        c.server_name = "example.test";
        c.insecure_skip_verify = true;   // self-signed: CertificateVerify is still checked under the leaf's key
        return c;
    }

    tls::ServerSettings server_settings(std::initializer_list<const char*> names = {"ed25519"}) {
        tls::ServerSettings s;
        for (const char* n : names) {
            s.identities.push_back(keys().identity(n));
        }
        return s;
    }

    // A ClientHello of one's own, its extensions written by `ext`
    template<class F>
    bytes_t client_hello(F&& ext, std::vector<uint16_t> suites = {0x1301}) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        uint8_t random[32] = {1};
        tls::write_client_hello(w, tls::bytes_of(random, 32), tls::Bytes(), suites, std::forward<F>(ext));
        return of(tls::bytes_of(out.data(), out.size()));
    }

    void versions(tls::Builder& w, std::vector<uint16_t> v = {tls::Tls13}) {
        auto e = w.extension(tls::ExtensionType::supported_versions);
        tls::write_versions_offered(w, v);
    }

    void groups(tls::Builder& w, std::vector<uint16_t> g = {0x001D}) {
        auto e = w.extension(tls::ExtensionType::supported_groups);
        tls::write_groups(w, g);
    }

    void schemes(tls::Builder& w, std::vector<uint16_t> s = {0x0807}) {
        auto e = w.extension(tls::ExtensionType::signature_algorithms);
        tls::write_signature_schemes(w, s);
    }

    // A share of a group, a real one (a point on the curve, a key the
    // server can encapsulate to), made once per group
    const tls::ClientShares& real_shares() {
        static tls::ClientShares s;
        if (s.size() == 0) {
            for (uint16_t g : {0x001D, 0x0017, 0x0018, 0x11EC}) {
                s.add(tls::Group(g), tls::Entropy());
            }
        }
        return s;
    }

    void share(tls::Builder& w, uint16_t group = 0x001D) {
        auto e = w.extension(tls::ExtensionType::key_share);
        auto& all = real_shares();
        for (size_t i = 0; i < all.size(); ++i) {
            if (uint16_t(all.group(i)) == group) {
                std::vector<tls::KeyShare> s = {{group, all.public_share(i)}};   // lint-handles: ok slices over unmanaged bytes, no owner
                tls::write_key_shares(w, s);
            }
        }
    }

    AlertDescription first_alert(tls::ServerHandshake& server, const bytes_t& m) {
        Log log;
        log.take(server.feed(view(m)));
        return log.alerts.empty() ? AlertDescription::close_notify : log.alerts[0];
    }
}

// --- RFC 8448 --------------------------------------------------------------------

TEST(TlsServer, Rfc8448Traces) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    for (int section : {3, 5, 7}) {
        SCOPED_TRACE(section);
        ServerTrace t;
        trace_of(t, section);
        trace_signer = &t.signer;
        tls::ServerHandshake server(t.settings, t.entropy.entropy());
        Log log;
        size_t ch = 0;
        log.take(server.feed(view(t.client.at(ch++).bytes)));
        ASSERT_TRUE(log.alerts.empty()) << int(log.alerts[0]);
        size_t sh = 0;
        if (section == 5) {
            // the HelloRetryRequest, byte for byte, then the second ClientHello
            ASSERT_EQ(log.sent.size(), 1u);
            EXPECT_EQ(log.sent[0], t.server.at(0).bytes);
            EXPECT_TRUE(server.result().retried);
            log.take(server.feed(view(t.client.at(ch++).bytes)));
            ASSERT_TRUE(log.alerts.empty()) << int(log.alerts[0]);
            sh = 1;
        }
        size_t first = log.sent.size() - 2;
        ASSERT_GE(log.sent.size(), 2u);
        EXPECT_EQ(log.sent[first], t.server.at(sh).bytes);   // ServerHello
        EXPECT_EQ(log.sent[first + 1], concat(t.server, {"EncryptedExtensions", "Certificate", "CertificateVerify", "Finished"}));
        EXPECT_EQ(t.signer.verified, 1);   // the content signed is the trace's
        EXPECT_EQ(log.installs["write hs"], t.secrets["s hs traffic"]);
        EXPECT_EQ(log.installs["read hs"], t.secrets["c hs traffic"]);
        EXPECT_EQ(log.installs["write ap"], t.secrets["s ap traffic"]);
        // the client's Finished (the last of its messages)
        log.take(server.feed(view(t.client.back().bytes)));
        ASSERT_TRUE(log.alerts.empty()) << int(log.alerts[0]);
        EXPECT_TRUE(server.established());
        EXPECT_EQ(log.installs["read ap"], t.secrets["c ap traffic"]);
        EXPECT_EQ(t.entropy.at, t.entropy.data.size());
        std::vector<std::string> expected;
        if (section == 5) {
            expected = {"send"};
        }
        expected.insert(expected.end(), {"send"});
        if (section == 7) {
            expected.push_back("ccs");
        }
        expected.insert(expected.end(), {"write hs", "read hs", "send", "write ap", "read ap", "established"});
        EXPECT_EQ(log.kinds, expected);
        EXPECT_EQ(server.result().server_name, "server");
    }
    trace_signer = nullptr;
}

// §4: a client offering 0-RTT with a PSK of another server: the PSK
// ignored, the early data skipped, a full handshake
TEST(TlsServer, EarlyDataRefused) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    bytes_t ch;
    for (auto& m : rfc8448::messages(steps())) {
        if (m.section == 4 && m.who == "client" && m.name == "ClientHello") {
            ch = m.bytes;
            break;
        }
    }
    ASSERT_FALSE(ch.empty());
    auto s = server_settings({"rsa"});
    tls::ServerHandshake server(s);
    Log log;
    log.take(server.feed(view(ch)));
    ASSERT_TRUE(log.alerts.empty()) << int(log.alerts[0]);
    ASSERT_FALSE(log.kinds.empty());
    EXPECT_EQ(log.kinds[0], "skip early data");
    EXPECT_EQ(log.skip, s.early_data_limit);
    EXPECT_TRUE(server.result().early_data_refused);
    auto sh = tls::read_server_hello(tls::read_handshake(view(log.sent.at(0)))->body);
    ASSERT_TRUE(sh);
    EXPECT_FALSE(sh->extensions.has(tls::ExtensionType::pre_shared_key));
    // the encrypted flight has a Certificate: a full handshake, not a resumed one
    auto flight = split(log.sent.at(1));
    ASSERT_EQ(flight.size(), 4u);
    EXPECT_EQ(flight[1][0], uint8_t(tls::HandshakeType::certificate));
    auto ee = tls::read_encrypted_extensions(tls::read_handshake(view(flight[0]))->body);
    EXPECT_FALSE(ee->has(tls::ExtensionType::early_data));
}

// --- our client, our server -------------------------------------------------------

TEST(TlsServer, EveryGroupSuiteAndKey) {
    for (const char* key : {"ed25519", "p256", "p384", "rsa"}) {
        for (uint16_t group : {0x11EC, 0x001D, 0x0017, 0x0018}) {
            for (uint16_t cipher : {0x1301, 0x1302, 0x1303}) {
                SCOPED_TRACE(std::string(key) + " " + std::to_string(group) + " " + std::to_string(cipher));
                auto c = client_settings();
                c.groups = {group};
                c.key_shares = {group};
                c.ciphers = {cipher};
                auto s = server_settings({key});
                Pair p(c, s);
                p.run();
                ASSERT_TRUE(p.client_log.alerts.empty()) << int(p.client_log.alerts[0]);
                ASSERT_TRUE(p.server_log.alerts.empty()) << int(p.server_log.alerts[0]);
                EXPECT_TRUE(p.client.established());
                EXPECT_TRUE(p.server.established());
                EXPECT_EQ(uint16_t(p.server.result().group), group);
                EXPECT_EQ(uint16_t(p.client.result().cipher), cipher);
                // the keys agree: what one side writes the other reads
                EXPECT_EQ(p.client_log.installs["write hs"], p.server_log.installs["read hs"]);
                EXPECT_EQ(p.client_log.installs["read hs"], p.server_log.installs["write hs"]);
                EXPECT_EQ(p.client_log.installs["write ap"], p.server_log.installs["read ap"]);
                EXPECT_EQ(p.client_log.installs["read ap"], p.server_log.installs["write ap"]);
                EXPECT_FALSE(p.client_log.installs["write ap"].empty());
                EXPECT_FALSE(p.client.result().retried);
            }
        }
    }
}

TEST(TlsServer, HelloRetryRequest) {
    // the client shares X25519 alone and supports the hybrid, the server
    // has the hybrid alone: one retry (a server with X25519 too would take
    // the share it has, as Go's does, rather than ask)
    auto c = client_settings();
    c.key_shares = {0x001D};
    auto s = server_settings();
    s.groups = {0x11EC};
    for (bool compatibility : {false, true}) {
        c.compatibility_mode = compatibility;
        Pair p(c, s);
        p.run();
        ASSERT_TRUE(p.server_log.alerts.empty()) << int(p.server_log.alerts[0]);
        ASSERT_TRUE(p.client_log.alerts.empty()) << int(p.client_log.alerts[0]);
        EXPECT_TRUE(p.server.established());
        EXPECT_TRUE(p.server.result().retried);
        EXPECT_TRUE(p.client.result().retried);
        EXPECT_EQ(p.server.result().group, tls::Group::x25519_mlkem768);
        EXPECT_EQ(p.client_log.installs["write ap"], p.server_log.installs["read ap"]);
        // compatibility mode: one change_cipher_spec, after the retry
        size_t ccs = 0;
        for (auto& k : p.server_log.kinds) {
            ccs += k == "ccs";
        }
        EXPECT_EQ(ccs, compatibility ? 1u : 0u);
        if (compatibility) {
            EXPECT_EQ(p.server_log.kinds[1], "ccs");
        }
    }
    // a server of both groups takes the share it has: no retry
    s.groups = {0x11EC, 0x001D};
    Pair q(c, s);
    q.run();
    EXPECT_TRUE(q.server.established());
    EXPECT_FALSE(q.server.result().retried);
    EXPECT_EQ(q.server.result().group, tls::Group::x25519);
}

TEST(TlsServer, Alpn) {
    auto c = client_settings();
    c.alpn = {"h2", "http/1.1"};
    auto s = server_settings();
    s.alpn = {"http/1.1", "h2"};
    {
        Pair p(c, s);   // the server's preference
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().alpn, "http/1.1");
        EXPECT_EQ(p.client.result().alpn, "http/1.1");
    }
    {
        s.alpn = {"spdy/3"};   // none in common
        Pair p(c, s);
        p.run();
        ASSERT_EQ(p.server_log.alerts.size(), 1u);
        EXPECT_EQ(p.server_log.alerts[0], AlertDescription::no_application_protocol);
        EXPECT_FALSE(p.client.established());
    }
    {
        s.alpn.clear();   // a server without a list: none chosen
        Pair p(c, s);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.client.result().alpn, "");
    }
    {
        c.alpn.clear();   // a client without one: none either
        s.alpn = {"h2"};
        Pair p(c, s);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().alpn, "");
    }
}

TEST(TlsServer, TheIdentityBySniAndScheme) {
    auto s = server_settings({"p256", "other", "ed25519"});
    auto c = client_settings();
    {
        c.server_name = "other.test";
        Pair p(c, s);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().identity, 1u);
        EXPECT_EQ(p.server.result().server_name, "other.test");
    }
    {
        c.server_name = "unknown.test";   // no certificate for it: the first
        Pair p(c, s);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().identity, 0u);
    }
    {
        c.server_name = "example.test";
        c.schemes = {0x0807};   // Ed25519 alone: the identity that has it
        Pair p(c, s);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().identity, 2u);
        EXPECT_EQ(p.server.result().scheme, 0x0807);
    }
    {
        auto r = server_settings({"rsa"});
        c.schemes = {0x0401, 0x0501};   // PKCS #1 alone: no CertificateVerify an RSA key may make
        Pair p(c, r);
        p.run();
        ASSERT_EQ(p.server_log.alerts.size(), 1u);
        EXPECT_EQ(p.server_log.alerts[0], AlertDescription::handshake_failure);
    }
    {
        auto r = server_settings({"rsa"});
        c.schemes = {0x0806};
        Pair p(c, r);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_EQ(p.server.result().scheme, 0x0806);
    }
}

// A key of 1024 bits has no room for RSA-PSS under SHA-512 (its digest and
// salt, 130 bytes, past the 128 of the encoding): a client that offers that
// scheme alone gets handshake_failure, as one with no scheme in common, and
// the server's feed throws nothing; the schemes that fit still serve
TEST(TlsServer, AnRsaKeyTooSmallForTheScheme) {
    auto r = server_settings({"rsa1024"});
    auto c = client_settings();
    {
        c.schemes = {0x0806};   // rsa_pss_rsae_sha512 alone
        Pair p(c, r);
        p.run();
        EXPECT_FALSE(p.client.established());
        ASSERT_EQ(p.server_log.alerts.size(), 1u);
        EXPECT_EQ(p.server_log.alerts[0], AlertDescription::handshake_failure);
    }
    for (uint16_t scheme : {uint16_t(0x0804), uint16_t(0x0805)}) {
        c.schemes = {0x0806, scheme};
        Pair p(c, r);
        p.run();
        EXPECT_TRUE(p.client.established()) << scheme;
        EXPECT_EQ(p.server.result().scheme, scheme);
    }
}

TEST(TlsServer, KeyUpdateBothWays) {
    Pair p(client_settings(), server_settings());
    p.run();
    ASSERT_TRUE(p.server.established());
    std::vector<sgcl::byte> m;
    tls::Builder w(m);
    tls::write_key_update(w, true);
    Log log;
    log.take(p.server.feed(tls::bytes_of(m.data(), m.size())));
    EXPECT_EQ(log.kinds, (std::vector<std::string>{"update read", "send", "update write"}));
    ASSERT_EQ(log.sent.size(), 1u);
    EXPECT_EQ(log.sent[0], unhex("1800000100"));   // KeyUpdate, update_not_requested
    Log again;
    p.client_log = Log();
    p.client_log.take(p.client.feed(view(log.sent[0])));
    EXPECT_EQ(p.client_log.kinds, (std::vector<std::string>{"update read"}));
    // a client's NewSessionTicket, a second KeyUpdate of 2
    std::vector<sgcl::byte> bad;
    tls::Builder b(bad);
    {
        auto x = b.message(tls::HandshakeType::key_update);
        b.u8(2);
    }
    again.take(p.server.feed(tls::bytes_of(bad.data(), bad.size())));
    ASSERT_EQ(again.alerts.size(), 1u);
    EXPECT_EQ(again.alerts[0], AlertDescription::illegal_parameter);
}

// --- refusals ---------------------------------------------------------------------

TEST(TlsServer, ClientHelloRefusals) {
    auto s = server_settings();
    auto check = [&](const bytes_t& ch, AlertDescription want, const char* what) {
        tls::ServerHandshake server(s);
        EXPECT_EQ(first_alert(server, ch), want) << what;
    };
    auto all = [](tls::Builder& w) {
        groups(w);
        share(w);
        versions(w);
        schemes(w);
    };
    {
        tls::ServerHandshake server(s);
        Log log;
        log.take(server.feed(view(client_hello(all))));
        EXPECT_TRUE(log.alerts.empty());   // ours reads
    }
    check(client_hello([](tls::Builder& w) { groups(w); share(w); schemes(w); }), AlertDescription::protocol_version, "no supported_versions");
    check(client_hello([](tls::Builder& w) { groups(w); share(w); versions(w, {0x0303}); schemes(w); }), AlertDescription::protocol_version, "TLS 1.2 alone");
    check(client_hello(all, {0x1304}), AlertDescription::handshake_failure, "no suite in common");
    check(client_hello([](tls::Builder& w) { groups(w, {0x0019}); versions(w); schemes(w); { auto e = w.extension(tls::ExtensionType::key_share); w.u16(0); } }), AlertDescription::handshake_failure, "no group in common");
    check(client_hello([](tls::Builder& w) { groups(w); share(w); versions(w); }), AlertDescription::missing_extension, "no signature_algorithms");
    check(client_hello([](tls::Builder& w) { share(w); versions(w); schemes(w); }), AlertDescription::missing_extension, "no supported_groups");
    check(client_hello([](tls::Builder& w) { groups(w, {0x001D}); share(w, 0x0017); versions(w); schemes(w); }), AlertDescription::illegal_parameter, "a share of a group not supported");
    check(client_hello([](tls::Builder& w) { groups(w); share(w); versions(w); schemes(w, {0x0401}); }), AlertDescription::handshake_failure, "no scheme the key makes");
    // a Finished before the ClientHello; a Certificate from the client
    tls::ServerHandshake server(s);
    EXPECT_EQ(first_alert(server, unhex("1400000100")), AlertDescription::unexpected_message);
}

TEST(TlsServer, TheClientsSecondFlightRefused) {
    // a Finished that does not verify
    {
        Pair p(client_settings(), server_settings());
        std::vector<bytes_t> to_server;
        Log log;
        p.client_log.take(p.client.start());
        p.server_log.take(p.server.feed(view(p.client_log.sent[0])));
        std::vector<sgcl::byte> f;
        tls::Builder w(f);
        bytes_t zeros(32, 0);
        tls::write_finished(w, view(zeros));
        log.take(p.server.feed(tls::bytes_of(f.data(), f.size())));
        ASSERT_EQ(log.alerts.size(), 1u);
        EXPECT_EQ(log.alerts[0], AlertDescription::decrypt_error);
        EXPECT_TRUE(p.server.failed());
        Log after;
        after.take(p.server.feed(tls::bytes_of(f.data(), f.size())));
        EXPECT_TRUE(after.kinds.empty());   // failed: nothing more
    }
    // a Certificate where none was asked for
    {
        Pair p(client_settings(), server_settings());
        p.client_log.take(p.client.start());
        p.server_log.take(p.server.feed(view(p.client_log.sent[0])));
        std::vector<sgcl::byte> c;
        tls::Builder w(c);
        tls::write_certificate(w, tls::Bytes(), std::vector<tls::Bytes>());   // lint-handles: ok slices over unmanaged bytes, no owner
        Log log;
        log.take(p.server.feed(tls::bytes_of(c.data(), c.size())));
        ASSERT_EQ(log.alerts.size(), 1u);
        EXPECT_EQ(log.alerts[0], AlertDescription::unexpected_message);
    }
}

TEST(TlsServer, TheSecondClientHelloRefused) {
    auto s = server_settings();
    s.groups = {0x0017};   // P-256 asked for
    auto first = client_hello([](tls::Builder& w) { groups(w, {0x001D, 0x0017}); share(w); versions(w); schemes(w); });
    auto run = [&](const bytes_t& second) {
        tls::ServerHandshake server(s);
        Log log;
        log.take(server.feed(view(first)));
        EXPECT_TRUE(server.result().retried);
        return first_alert(server, second);
    };
    auto good = client_hello([](tls::Builder& w) { groups(w, {0x001D, 0x0017}); share(w, 0x0017); versions(w); schemes(w); });
    EXPECT_EQ(run(good), AlertDescription::close_notify);
    EXPECT_EQ(run(first), AlertDescription::illegal_parameter);   // the same share again
    EXPECT_EQ(run(client_hello([](tls::Builder& w) { groups(w, {0x001D, 0x0017}); share(w, 0x0017); versions(w); schemes(w); }, {0x1302})), AlertDescription::illegal_parameter);   // the suite gone
    EXPECT_EQ(run(client_hello([](tls::Builder& w) {
        groups(w, {0x001D, 0x0017});
        share(w, 0x0017);
        versions(w);
        schemes(w);
        auto e = w.extension(tls::ExtensionType::early_data);
    })), AlertDescription::illegal_parameter);   // early data after a retry
    // with a cookie: the second must echo it
    s.retry_cookie = bytes(unhex("c0ffee"));
    EXPECT_EQ(run(good), AlertDescription::illegal_parameter);
}
