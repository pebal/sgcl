//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's handshake machine (sgcl/net/tls/detail/handshake.h) against
// the traces of RFC 8448 (~/Programming/oracles/rfc8448/rfc8448.txt or
// $SGCL_ORACLES), on the randomness of the trace given as its entropy:
//
//   - §3, the simple 1-RTT handshake; §5, the HelloRetryRequest (P-256
//     after X25519, the cookie, the second ClientHello padded to 512
//     bytes); §7, compatibility mode (a session id and change_cipher_spec):
//     every ClientHello and the client's Finished byte for byte, every
//     traffic secret the machine installs, the order of the actions, the
//     server's NewSessionTicket passed over;
//   - the server's messages of those traces changed a little: a Finished
//     or a CertificateVerify that does not verify, messages out of order,
//     a suite not offered, a certificate that does not verify against the
//     roots; a KeyUpdate after the handshake; a CertificateRequest (§6)
//     answered with an empty Certificate.
#include "tests/types.h"

#include "sgcl/net/tls/detail/handshake.h"
#include "tls_rfc8448.h"

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

    // A trace: its messages and the settings and entropy its ClientHello
    // was made with (read back from the ClientHello itself)
    struct Trace {
        std::vector<rfc8448::Message> client, server;
        tls::ClientSettings settings;
        Queue entropy;
        std::map<std::string, bytes_t> secrets;   // "c hs traffic" ... as the trace derives them
    };

    Trace trace_of(const std::vector<rfc8448::Step>& steps, int section) {
        Trace t;
        for (auto& m : rfc8448::messages(steps)) {
            if (m.section == section) {
                (m.who == "client" ? t.client : t.server).push_back(m);
            }
        }
        for (auto& s : steps) {
            if (s.section != section) {
                continue;
            }
            if (s.who == "client" && s.title.rfind("create an ephemeral", 0) == 0) {
                t.entropy.data.insert(t.entropy.data.end(), s.fields.at("private key").begin(), s.fields.at("private key").end());
            }
            for (const char* label : {"c hs traffic", "s hs traffic", "c ap traffic", "s ap traffic"}) {
                if (s.title.rfind(std::string("derive secret \"tls13 ") + label + "\"", 0) == 0 && s.fields.count("expanded")) {
                    t.secrets[label] = s.fields.at("expanded");
                }
            }
        }
        // the random and the session id first, then the private keys
        const bytes_t& ch = t.client.at(0).bytes;
        auto body = tls::read_handshake(view(ch));
        auto hello = tls::read_client_hello(body->body);
        bytes_t head = of(hello->random);
        bytes_t session = of(hello->session_id);
        head.insert(head.end(), session.begin(), session.end());
        t.entropy.data.insert(t.entropy.data.begin(), head.begin(), head.end());
        auto& s = t.settings;
        s.compatibility_mode = !session.empty();
        s.insecure_skip_verify = true;
        s.pad_client_hello = true;
        s.ciphers.clear();
        for (uint16_t c : hello->cipher_suites) {
            s.ciphers.push_back(c);
        }
        auto x = hello->extensions;
        auto groups = tls::read_groups(*x.find(tls::ExtensionType::supported_groups));
        s.groups.clear();
        for (uint16_t g : *groups) {
            s.groups.push_back(g);
        }
        auto schemes = tls::read_signature_schemes(*x.find(tls::ExtensionType::signature_algorithms));
        s.schemes.clear();
        for (uint16_t v : *schemes) {
            s.schemes.push_back(v);
        }
        auto shares = tls::read_key_shares(*x.find(tls::ExtensionType::key_share));
        s.key_shares.clear();
        for (auto k : *shares) {
            s.key_shares.push_back(k.group);
        }
        auto name = tls::read_server_name(*x.find(tls::ExtensionType::server_name));
        s.server_name = sgcl::string(std::string_view(reinterpret_cast<const char*>(name->data()), name->size()));
        s.session_ticket_extension = x.find(uint16_t(0x0023)).has_value();
        if (auto l = x.find(tls::ExtensionType::record_size_limit)) {
            s.record_size_limit = *tls::read_record_size_limit(*l);
        }
        return t;
    }

    // Everything the machine asked for, in order
    struct Log {
        std::vector<bytes_t> sent;
        std::vector<std::string> kinds;
        std::vector<std::pair<std::string, bytes_t>> installs;
        std::vector<AlertDescription> alerts;

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
                    installs.push_back({k, of(a.secret)});
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
                    break;
                case Action::Kind::alert:
                    alerts.push_back(a.alert);
                    kinds.push_back("alert");
                    break;
                case Action::Kind::skip_early_data:   // the server's alone
                    kinds.push_back("skip early data");
                    break;
                }
            }
        }
    };

    const std::vector<rfc8448::Step>& steps() {
        static const std::vector<rfc8448::Step> s = rfc8448::read();
        return s;
    }

    // The server's messages of a trace up to (not including) the one named
    std::vector<bytes_t> server_messages(const Trace& t) {
        std::vector<bytes_t> v;
        for (auto& m : t.server) {
            v.push_back(m.bytes);
        }
        return v;
    }

    bytes_t message(uint8_t type, const bytes_t& body) {
        bytes_t m = {type, uint8_t(body.size() >> 16), uint8_t(body.size() >> 8), uint8_t(body.size())};
        m.insert(m.end(), body.begin(), body.end());
        return m;
    }
}

// §3, §5 and §7: the whole handshake as the trace has it
TEST(TlsClient, Rfc8448Traces) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    for (int section : {3, 5, 7}) {
        SCOPED_TRACE(section);
        Trace t = trace_of(steps(), section);
        tls::ClientHandshake client(t.settings, t.entropy.entropy());
        Log log;
        log.take(client.start());
        ASSERT_EQ(log.sent.size(), 1u);
        EXPECT_EQ(log.sent[0], t.client.at(0).bytes);
        for (auto& m : t.server) {
            if (m.name == "NewSessionTicket") {
                ASSERT_TRUE(client.established());
                size_t before = log.kinds.size();
                log.take(client.feed(view(m.bytes)));
                EXPECT_EQ(log.kinds.size(), before);   // passed over
                continue;
            }
            log.take(client.feed(view(m.bytes)));
            ASSERT_TRUE(log.alerts.empty()) << m.name << " " << int(log.alerts[0]);
        }
        EXPECT_TRUE(client.established());
        EXPECT_EQ(t.entropy.at, t.entropy.data.size());   // every byte of the trace's randomness used
        // what the client sent: its ClientHellos and its Finished, byte for byte
        std::vector<bytes_t> expected;
        for (auto& m : t.client) {
            expected.push_back(m.bytes);
        }
        EXPECT_EQ(log.sent, expected);
        // the keys, as the trace derives them, in their order
        std::vector<std::pair<std::string, bytes_t>> installs = {
            {"read hs", t.secrets.at("s hs traffic")},
            {"read ap", t.secrets.at("s ap traffic")},
            {"write hs", t.secrets.at("c hs traffic")},
            {"write ap", t.secrets.at("c ap traffic")},
        };
        EXPECT_EQ(log.installs, installs);
        std::vector<std::string> kinds;
        if (section == 5) {
            kinds = {"send", "send", "read hs", "read ap", "write hs", "send", "write ap", "established"};
        } else if (section == 7) {
            kinds = {"send", "read hs", "read ap", "ccs", "write hs", "send", "write ap", "established"};
        } else {
            kinds = {"send", "read hs", "read ap", "write hs", "send", "write ap", "established"};
        }
        EXPECT_EQ(log.kinds, kinds);
        auto& r = client.result();
        EXPECT_EQ(r.cipher, tls::Cipher::aes_128_gcm_sha256);
        EXPECT_EQ(r.group, section == 5 ? tls::Group::secp256r1 : tls::Group::x25519);
        EXPECT_EQ(r.retried, section == 5);
        EXPECT_EQ(r.record_size_limit, 0x4001);
        EXPECT_EQ(r.peer_certificates.size(), 1u);
        EXPECT_EQ(std::string(r.server_name.view()), "server");
        EXPECT_TRUE(r.alpn.empty());
    }
}

namespace {
    // The trace of §3 with the server message `at` (its index) replaced by
    // `changed`: the first alert the client sends, if any
    optional<AlertDescription> run_changed(size_t at, const bytes_t& changed) {
        Trace t = trace_of(steps(), 3);
        tls::ClientHandshake client(t.settings, t.entropy.entropy());
        Log log;
        log.take(client.start());
        auto server = server_messages(t);
        for (size_t i = 0; i < server.size() && log.alerts.empty(); ++i) {
            log.take(client.feed(view(i == at ? changed : server[i])));
        }
        if (log.alerts.empty()) {
            return nullopt;
        }
        EXPECT_TRUE(client.failed());
        return log.alerts[0];
    }
}

// The server's messages of §3 changed: what the client refuses and how
TEST(TlsClient, ChangedServerMessages) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    Trace t = trace_of(steps(), 3);
    auto server = server_messages(t);   // ServerHello, EncryptedExtensions, Certificate, CertificateVerify, Finished, NewSessionTicket
    ASSERT_EQ(server.size(), 6u);
    // the last byte of the Finished, of the CertificateVerify's signature
    bytes_t fin = server[4];
    fin.back() ^= 1;
    EXPECT_EQ(run_changed(4, fin), AlertDescription::decrypt_error);
    bytes_t cv = server[3];
    cv.back() ^= 1;
    EXPECT_EQ(run_changed(3, cv), AlertDescription::decrypt_error);
    // a scheme not offered in the CertificateVerify (0x0807, Ed25519, is not in §3's list)
    bytes_t cv_scheme = server[3];
    cv_scheme[4] = 0x08;
    cv_scheme[5] = 0x07;
    EXPECT_EQ(run_changed(3, cv_scheme), AlertDescription::illegal_parameter);
    // out of order: EncryptedExtensions first, Finished for Certificate
    EXPECT_EQ(run_changed(0, server[1]), AlertDescription::unexpected_message);
    EXPECT_EQ(run_changed(2, server[4]), AlertDescription::unexpected_message);
    // a cipher suite not offered (0x1304), the session id not echoed
    bytes_t sh = server[0];
    ASSERT_EQ(sh[38], 0);   // the empty legacy_session_id_echo
    bytes_t suite = sh;
    suite[40] = 0x04;
    EXPECT_EQ(run_changed(0, suite), AlertDescription::illegal_parameter);
    // a ServerHello's key share changed: the transcript is another, so the
    // CertificateVerify signed over the trace's does not verify
    bytes_t share = sh;
    const bytes_t head = {0x00, 0x33, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x20};
    auto at = std::search(share.begin(), share.end(), head.begin(), head.end());
    ASSERT_NE(at, share.end());
    at[head.size() + 5] ^= 0x40;
    EXPECT_EQ(run_changed(0, share), AlertDescription::decrypt_error);
    // an empty Certificate
    EXPECT_EQ(run_changed(2, message(11, {0, 0, 0, 0})), AlertDescription::decode_error);
    // a HelloRetryRequest after a HelloRetryRequest is §5's; a second one
    // is refused (the trace's HRR fed twice)
    Trace five = trace_of(steps(), 5);
    tls::ClientHandshake client(five.settings, five.entropy.entropy());
    Log log;
    log.take(client.start());
    log.take(client.feed(view(five.server[0].bytes)));
    ASSERT_TRUE(log.alerts.empty());
    log.take(client.feed(view(five.server[0].bytes)));
    ASSERT_EQ(log.alerts.size(), 1u);
    EXPECT_EQ(log.alerts[0], AlertDescription::unexpected_message);
    // after a failure, nothing more
    EXPECT_TRUE(client.feed(view(five.server[1].bytes)).actions.empty());
}

// The server's certificate against roots: the trace's self-signed
// certificate is no root of an empty pool
TEST(TlsClient, CertificateVerification) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    Trace t = trace_of(steps(), 3);
    auto server = server_messages(t);
    auto body = tls::read_handshake(view(server[2]));
    auto cert = tls::read_certificate(body->body);
    auto leaf = sgcl::crypto::x509::certificate::parse((*cert->begin()).der);
    ASSERT_TRUE(leaf.has_value());
    struct At {
        sgcl::time::datetime when;
        static sgcl::time::datetime now(void* self) {
            return static_cast<At*>(self)->when;
        }
    };
    At inside{leaf->not_before()};
    tls::ClientSettings s = t.settings;
    s.insecure_skip_verify = false;
    s.roots = sgcl::crypto::x509::certificate_pool();
    tls::ClientHandshake client(s, t.entropy.entropy(), tls::Clock{&At::now, &inside});
    Log log;
    log.take(client.start());
    for (size_t i = 0; i < 3; ++i) {
        log.take(client.feed(view(server[i])));
    }
    // the trace's certificate holds no DNS name: not one for "server",
    // bad_certificate as Go sends for a name that does not match
    ASSERT_EQ(log.alerts.size(), 1u);
    EXPECT_TRUE(leaf->dns_names().empty());
    EXPECT_EQ(log.alerts[0], AlertDescription::bad_certificate);
    // past its not_after: certificate_expired
    At after{leaf->not_after() + 24 * sgcl::hour};
    tls::ClientHandshake late(s, t.entropy.entropy(), tls::Clock{&At::now, &after});
    t.entropy.at = 0;
    Log l3;
    l3.take(late.start());
    for (size_t i = 0; i < 3; ++i) {
        l3.take(late.feed(view(server[i])));
    }
    ASSERT_EQ(l3.alerts.size(), 1u);
    EXPECT_EQ(l3.alerts[0], AlertDescription::certificate_expired);
    // no server name and no insecure_skip_verify: refused before anything is sent
    tls::ClientSettings none = t.settings;
    none.insecure_skip_verify = false;
    none.server_name = sgcl::string();
    Queue q = t.entropy;
    tls::ClientHandshake nameless(none, q.entropy());
    Log l2;
    l2.take(nameless.start());
    EXPECT_TRUE(l2.sent.empty());
    ASSERT_EQ(l2.alerts.size(), 1u);
    EXPECT_EQ(l2.alerts[0], AlertDescription::internal_error);
}

// After the handshake: KeyUpdate answered (§4.6.3), other messages refused
TEST(TlsClient, AfterTheHandshake) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    // (the machine holds managed members: it lives in the frame, never in
    // unmanaged memory)
    auto establish = [](tls::ClientHandshake& c, const Trace& t) {
        c.start();
        for (auto& m : t.server) {
            c.feed(view(m.bytes));
        }
        EXPECT_TRUE(c.established());
    };
    {
        Trace t = trace_of(steps(), 3);
        tls::ClientHandshake client(t.settings, t.entropy.entropy());
        establish(client, t);
        auto* c = &client;
        Log log;
        log.take(c->feed(view(message(24, {1}))));
        EXPECT_EQ(log.kinds, (std::vector<std::string>{"update read", "send", "update write"}));
        EXPECT_EQ(log.sent, (std::vector<bytes_t>{{24, 0, 0, 1, 0}}));
        Log l0;
        l0.take(c->feed(view(message(24, {0}))));
        EXPECT_EQ(l0.kinds, (std::vector<std::string>{"update read"}));
        Log l2;
        l2.take(c->feed(view(message(24, {2}))));
        EXPECT_EQ(l2.alerts, (std::vector<AlertDescription>{AlertDescription::illegal_parameter}));
    }
    {
        Trace t = trace_of(steps(), 3);
        tls::ClientHandshake client(t.settings, t.entropy.entropy());
        establish(client, t);
        auto* c = &client;
        Log log;
        log.take(c->feed(view(message(13, {0, 0, 4, 0, 13, 0, 0}))));   // a post-handshake CertificateRequest: not offered
        EXPECT_EQ(log.alerts, (std::vector<AlertDescription>{AlertDescription::unexpected_message}));
    }
}

// §6: the server asks for a certificate; v1 answers with an empty one
TEST(TlsClient, CertificateRequestAnsweredEmpty) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    Trace t = trace_of(steps(), 6);
    tls::ClientHandshake client(t.settings, t.entropy.entropy());
    Log log;
    log.take(client.start());
    ASSERT_EQ(log.sent.size(), 1u);
    EXPECT_EQ(log.sent[0], t.client.at(0).bytes);   // the trace's ClientHello
    for (auto& m : t.server) {
        log.take(client.feed(view(m.bytes)));
        ASSERT_TRUE(log.alerts.empty()) << m.name;
    }
    EXPECT_TRUE(client.established());
    ASSERT_EQ(log.sent.size(), 3u);
    EXPECT_EQ(log.sent[1], (bytes_t{11, 0, 0, 4, 0, 0, 0, 0}));   // Certificate: no context, no entries
    EXPECT_EQ(log.sent[2][0], 20);                                 // then Finished
    EXPECT_EQ(log.installs.at(0).second, t.secrets.at("s hs traffic"));
    EXPECT_EQ(log.installs.at(1).second, t.secrets.at("s ap traffic"));
}
