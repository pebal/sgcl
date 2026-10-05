//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The seeds of resumption and mTLS for the TLS fuzzing harnesses
// (tests/net/fuzz/tls_*_fuzz.cpp), made by our two machines against each
// other on the harnesses' settings (tests/net/fuzz/tls_fuzz_settings.h), so
// that each takes its harness's machine through a whole handshake:
//
//   seeds/tls_server: the client's messages of a resumed handshake (mode 2),
//     of one after a HelloRetryRequest (3), of one with a client
//     certificate (4), and of a session refused where a certificate is
//     required (6);
//   seeds/tls_client: the server's messages of the resumed handshake with
//     its NewSessionTicket, and of a full one with a CertificateRequest
//     (mode 2); a TLS 1.2 server's (tls12_server.h) to the clients of 1.2
//     (modes 4 and 5: a full handshake, one with a CertificateRequest, one
//     of each kind of key), its change_cipher_spec the pseudo-message 0xFF;
//     its abbreviated handshakes to the client of 1.2's resumption (modes 8
//     and 9: by ticket, renewed, by session id) and a session it declines;
//   seeds/tls_ticket: the fixed session's ticket and its content;
//   seeds/tls_messages: a ClientHello with pre_shared_key, a
//     CertificateRequest with certificate_authorities, a NewSessionTicket;
//     TLS 1.2's ServerKeyExchange, CertificateRequest, Certificate,
//     ServerHelloDone and NewSessionTicket.
//
// Disabled: it writes into the tree. Run it after a change of the
// harnesses' settings or of the messages:
//
//   tests_net --gtest_also_run_disabled_tests --gtest_filter=TlsFuzzSeeds.*
#include "tests/types.h"
#include "tests/source_root.h"

#include "tests/net/fuzz/tls_fuzz_settings.h"
#include "tests/net/tls12_server.h"

#include <fstream>
#include <string>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;
    using bytes_t = std::vector<uint8_t>;

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

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

    std::vector<bytes_t> sends(const tls::Step& step) {
        std::vector<bytes_t> out;
        for (auto& a : step.actions) {
            if (a.kind == tls::Action::Kind::send) {
                for (auto& m : split(of(step.bytes(a)))) {
                    out.push_back(m);
                }
            }
        }
        return out;
    }

    // The messages each side sent, in order, the two machines run to the end
    struct Transcript {
        std::vector<bytes_t> client, server;
        bool client_ok = false, server_ok = false;
    };

    Transcript run(tls::ClientHandshake& client, tls::ServerHandshake& server) {
        Transcript t;
        std::vector<bytes_t> to_server = sends(client.start()), to_client;
        for (int round = 0; round < 10 && (!to_server.empty() || !to_client.empty()); ++round) {
            for (auto& m : to_server) {
                t.client.push_back(m);
                for (auto& r : sends(server.feed(tls::bytes_of(m.data(), m.size())))) {
                    to_client.push_back(r);
                }
            }
            to_server.clear();
            for (auto& m : to_client) {
                t.server.push_back(m);
                for (auto& r : sends(client.feed(tls::bytes_of(m.data(), m.size())))) {
                    to_server.push_back(r);
                }
            }
            to_client.clear();
        }
        t.client_ok = client.established();
        t.server_ok = server.established();
        return t;
    }

    // A harness's input: the mode, then each message as its type, a 16-bit
    // length and its body
    bytes_t input(uint8_t mode, const std::vector<bytes_t>& messages) {
        bytes_t out = {mode};
        for (auto& m : messages) {
            out.push_back(m[0]);
            size_t n = m.size() - 4;
            out.push_back(uint8_t(n >> 8));
            out.push_back(uint8_t(n));
            out.insert(out.end(), m.begin() + 4, m.end());
        }
        return out;
    }

    void write(const std::string& dir, const std::string& name, const bytes_t& bytes) {
        std::ofstream f((source_root() / "tests/net/fuzz/seeds" / dir / name).string(), std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        ASSERT_TRUE(f.good()) << dir << "/" << name;
    }

    bytes_t first_of(const std::vector<bytes_t>& ms, tls::HandshakeType type) {
        for (auto& m : ms) {
            if (m[0] == uint8_t(type)) {
                return m;
            }
        }
        return {};
    }
}

TEST(TlsFuzzSeeds, DISABLED_Write) {
    // the server harness: the client's messages
    struct Case {
        uint8_t mode;
        const char* name;
        bool session, identity;
    };
    for (Case c : {Case{2, "resumed", true, false}, Case{3, "resumed_retry", true, false}, Case{4, "client_certificate", false, true}, Case{6, "session_refused_certificate_required", true, true}}) {
        SCOPED_TRACE(c.name);
        tls::ClientSettings cs = tls_fuzz::auth_client_settings();
        if (!c.session) {
            cs.session = nullptr;
        }
        if (!c.identity) {
            cs.identities.clear();
        }
        tls_fuzz::ClientEntropy ce;
        tls::ClientHandshake client(cs, ce.entropy(), tls_fuzz::clock());
        tls_fuzz::ServerEntropy se;
        tls::ServerHandshake server(tls_fuzz::server_settings(c.mode), se.entropy(), tls_fuzz::clock());
        Transcript t = run(client, server);
        ASSERT_TRUE(t.client_ok && t.server_ok);
        EXPECT_EQ(server.result().resumed, c.mode == 2 || c.mode == 3);
        write("tls_server", c.name, input(c.mode, t.client));
        if (c.mode == 2) {
            // the messages harness: a ClientHello with pre_shared_key
            write("tls_messages", "auth-ClientHello-psk", first_of(t.client, tls::HandshakeType::client_hello));
            write("tls_messages", "auth-NewSessionTicket", first_of(t.server, tls::HandshakeType::new_session_ticket));
        }
        if (c.mode == 4) {
            write("tls_messages", "auth-CertificateRequest", first_of(t.server, tls::HandshakeType::certificate_request));
            write("tls_messages", "auth-client-Certificate", first_of(t.client, tls::HandshakeType::certificate));
        }
    }
    // the client harness (mode 2): the server's messages
    for (uint8_t server_mode : {uint8_t(2), uint8_t(4)}) {
        tls_fuzz::ClientEntropy ce;
        tls::ClientHandshake client(tls_fuzz::auth_client_settings(), ce.entropy(), tls_fuzz::clock());
        tls_fuzz::ServerEntropy se;
        tls::ServerHandshake server(tls_fuzz::server_settings(server_mode), se.entropy(), tls_fuzz::clock());
        Transcript t = run(client, server);
        ASSERT_TRUE(t.client_ok && t.server_ok);
        EXPECT_EQ(client.result().resumed, server_mode == 2);
        write("tls_client", server_mode == 2 ? "resumed_with_ticket" : "certificate_request", input(2, t.server));
    }
    // the client harness, TLS 1.2 (modes 4 and 5): a 1.2 server's messages,
    // its change_cipher_spec before its Finished
    struct Case12 {
        uint8_t mode;
        const char* name;
        const char* leaf;
        uint16_t cipher, group, scheme;
        bool request;
    };
    for (Case12 c : {Case12{4, "tls12_ecdsa", "p256", 0xC02B, 0x001D, 0x0403, false}, Case12{5, "tls12_alone_rsa", "rsa", 0xC030, 0x0017, 0x0401, false},
                     Case12{4, "tls12_ed25519_chacha", "ed25519", 0xCCA9, 0x0018, 0x0807, false}, Case12{4, "tls12_certificate_request", "p256", 0xC02C, 0x001D, 0x0403, true}}) {
        SCOPED_TRACE(c.name);
        tls12_server::Server12 server;
        server.leaf = c.leaf;
        server.cipher = c.cipher;
        server.group = c.group;
        server.scheme = c.scheme;
        server.request_certificate = c.request;
        server.alpn = "h2";
        tls_fuzz::ClientEntropy ce;
        tls::ClientHandshake client(tls_fuzz::tls12_client_settings(c.mode == 4), ce.entropy(), tls_fuzz::clock());
        std::vector<bytes_t> hello = sends(client.start());
        ASSERT_EQ(hello.size(), 1u);
        std::vector<bytes_t> from_server = server.hello(hello[0]), flight;
        for (auto& m : from_server) {
            for (auto& r : sends(client.feed(tls::bytes_of(m.data(), m.size())))) {
                flight.push_back(r);
            }
        }
        bytes_t fin = server.flight(flight);
        ASSERT_FALSE(fin.empty()) << server.error;
        (void)client.change_cipher_spec();
        (void)client.feed(tls::bytes_of(fin.data(), fin.size()));
        ASSERT_TRUE(client.established());
        from_server.push_back(bytes_t{tls_fuzz::ChangeCipherSpec, 0, 0, 0});
        from_server.push_back(fin);
        from_server.push_back(bytes_t{0, 0, 0, 0});   // a HelloRequest after: passed over
        write("tls_client", c.name, input(c.mode, from_server));
        if (c.request) {
            write("tls_messages", "tls12-ServerKeyExchange", from_server[2]);
            write("tls_messages", "tls12-CertificateRequest", from_server[3]);
            write("tls_messages", "tls12-Certificate", from_server[1]);
            write("tls_messages", "tls12-ServerHelloDone", from_server[4]);
        }
    }
    // the client harness, TLS 1.2's resumption (modes 8 and 9): the
    // abbreviated handshake by ticket, renewed, by session id, and a
    // session declined (a full handshake with a NewSessionTicket)
    struct Resume12 {
        uint8_t mode;
        const char* name;
        bool renew, decline;
    };
    for (Resume12 c : {Resume12{8, "tls12_resumed_ticket", false, false}, Resume12{8, "tls12_resumed_renewed", true, false}, Resume12{9, "tls12_resumed_session_id", false, false},
                       Resume12{8, "tls12_declined_new_ticket", false, true}}) {
        SCOPED_TRACE(c.name);
        tls12_server::Store store;
        tls12_server::Saved saved{bytes_t(48, 0x42), tls_fuzz::Session12Cipher};
        store.tickets[tls_fuzz::session12_ticket()] = saved;
        store.ids[tls_fuzz::session12_id()] = saved;
        tls12_server::Server12 server;
        server.store = &store;
        server.tickets = true;
        server.renew = c.renew;
        server.decline = c.decline;
        server.alpn = "h2";
        tls_fuzz::ClientEntropy ce;
        tls::ClientHandshake client(tls_fuzz::tls12_resume_settings(c.mode == 8), ce.entropy(), tls_fuzz::clock());
        std::vector<bytes_t> hello = sends(client.start());
        ASSERT_EQ(hello.size(), 1u);
        std::vector<bytes_t> from_server = server.hello(hello[0]), flight;
        for (auto& m : from_server) {
            for (auto& r : sends(client.feed(tls::bytes_of(m.data(), m.size())))) {
                flight.push_back(r);
            }
        }
        ASSERT_EQ(server.resumed, !c.decline);
        bytes_t fin;
        if (server.resumed) {
            (void)client.change_cipher_spec();
            fin = server.server_finished();
            auto mine = sends(client.feed(tls::bytes_of(fin.data(), fin.size())));
            ASSERT_EQ(mine.size(), 1u);
            ASSERT_TRUE(server.client_finished(mine[0])) << server.error;
        } else {
            fin = server.flight(flight);
            ASSERT_FALSE(fin.empty()) << server.error;
            for (auto& m : server.before_ccs) {
                (void)client.feed(tls::bytes_of(m.data(), m.size()));
                from_server.push_back(m);
            }
            (void)client.change_cipher_spec();
            (void)client.feed(tls::bytes_of(fin.data(), fin.size()));
        }
        ASSERT_TRUE(client.established());
        EXPECT_EQ(client.result().resumed, !c.decline);
        from_server.push_back(bytes_t{tls_fuzz::ChangeCipherSpec, 0, 0, 0});
        from_server.push_back(fin);
        write("tls_client", c.name, input(c.mode, from_server));
        if (c.renew) {
            write("tls_messages", "tls12-NewSessionTicket", from_server[1]);
        }
    }
    // the ticket harness: the ticket (mode 0) and its content (mode 1)
    auto s = tls_fuzz::session();
    bytes_t ticket = {0};
    ticket.insert(ticket.end(), reinterpret_cast<const uint8_t*>(s->ticket.data()), reinterpret_cast<const uint8_t*>(s->ticket.data()) + s->ticket.size());
    write("tls_ticket", "session_ticket", ticket);
    std::vector<sgcl::byte> content;
    tls::Builder w(content);
    tls::write_ticket_content(w, s->cipher, s->received_ms, s->lifetime, s->age_add, *s->psk, sgcl::crypto::x509::chain());
    bytes_t c = {1};
    c.insert(c.end(), reinterpret_cast<const uint8_t*>(content.data()), reinterpret_cast<const uint8_t*>(content.data()) + content.size());
    write("tls_ticket", "session_content", c);
    // a content with a client's chain
    sgcl::crypto::x509::chain chain;
    auto der = tls_fuzz::unhex(tls_client_identities::client_certificate);
    chain.push_back(sgcl::crypto::x509::certificate::parse(tls::bytes_of(der.data(), der.size())).value());
    std::vector<sgcl::byte> with_chain;
    tls::Builder w2(with_chain);
    tls::write_ticket_content(w2, 0x1302, tls_fuzz::Now, 3600, 7, *s->psk, chain);
    bytes_t c2 = {1};
    c2.insert(c2.end(), reinterpret_cast<const uint8_t*>(with_chain.data()), reinterpret_cast<const uint8_t*>(with_chain.data()) + with_chain.size());
    write("tls_ticket", "content_with_chain", c2);
}
