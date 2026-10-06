//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the TLS machine harnesses (tls_client_fuzz.cpp, tls_server_fuzz.cpp)
// and the program that writes their seeds (tests/net/tls_fuzz_seeds.cpp)
// share, so that a seed made by one of our machines against the other
// takes the harness's machine through the same handshake: the entropy, a
// clock that stands still, the identities (a server's Ed25519, a client's
// ECDSA P-256 of the test CA), ticket keys of a fixed key, and a session
// whose ticket those keys open (a resumption with no handshake before it).
#pragma once

#include "sgcl/net/tls/detail/server_handshake.h"
#include "tests/net/tls_client_identities.h"
#include "tests/net/tls_server_identities.h"

#include <cstring>
#include <memory>
#include <vector>

namespace tls_fuzz {
    namespace tls = sgcl::net::tls::detail;

    inline std::vector<sgcl::byte> unhex(const char* s) {
        std::vector<sgcl::byte> v;
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3] = {s[i], s[i + 1], 0};
            v.push_back(sgcl::byte(std::strtoul(b, nullptr, 16)));
        }
        return v;
    }

    // The server's entropy: a counter's bytes
    struct ServerEntropy {
        size_t at = 0;

        static void fill(void* self, uint8_t* out, size_t n) {
            auto& f = *static_cast<ServerEntropy*>(self);
            for (size_t i = 0; i < n; ++i) {
                out[i] = uint8_t(f.at++ * 131 + 17);
            }
        }

        tls::Entropy entropy() {
            return tls::Entropy{&ServerEntropy::fill, this};
        }
    };

    // The client's (of its default mode): the bytes given, then a pattern
    // of each call's own
    struct ClientEntropy {
        const uint8_t* data = nullptr;
        size_t size = 0;
        size_t at = 0;

        static void fill(void* self, uint8_t* out, size_t n) {
            auto& f = *static_cast<ClientEntropy*>(self);
            for (size_t i = 0; i < n; ++i) {
                out[i] = f.at < f.size ? f.data[f.at++] : uint8_t(i * 29 + 7);
            }
        }

        tls::Entropy entropy() {
            return tls::Entropy{&ClientEntropy::fill, this};
        }
    };

    // The time, standing still (within the certificates' validity)
    inline constexpr int64_t Now = 1'800'000'000'000;

    inline sgcl::time::datetime now(void*) noexcept {
        return sgcl::time::datetime::from_unix_milli(Now, sgcl::time::zone::utc());
    }

    inline tls::Clock clock() {
        return tls::Clock{&now, nullptr};
    }

    inline const sgcl::crypto::ed25519::private_key& server_key() {
        static const auto k = [] {
            auto der = unhex(tls_identities::all[0].key);   // "ed25519"
            return sgcl::crypto::ed25519::private_key::from_pkcs8_der(tls::bytes_of(der.data(), der.size())).value();
        }();
        return k;
    }

    inline const sgcl::crypto::p256::private_key& client_key() {
        static const auto k = [] {
            auto der = unhex(tls_client_identities::client_key);
            return sgcl::crypto::p256::private_key::from_pkcs8_der(tls::bytes_of(der.data(), der.size())).value();
        }();
        return k;
    }

    inline tls::TicketKeys& ticket_keys() {
        static const uint8_t name[8] = {'f', 'u', 'z', 'z', 'k', 'e', 'y', '1'};
        static const uint8_t key[32] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
        static tls::TicketKeys keys(name, key);
        return keys;
    }

    inline sgcl::crypto::x509::certificate_pool ca() {
        auto der = unhex(tls_client_identities::ca);
        sgcl::crypto::x509::certificate_pool p;
        p.add(sgcl::crypto::x509::certificate::parse(tls::bytes_of(der.data(), der.size())).value());
        return p;
    }

    // The server of a mode: bit 0, P-256 alone (a HelloRetryRequest for
    // most clients), a cookie and ALPN; bit 1, tickets issued and taken
    // (the fixed keys); bit 2, a client certificate required (the test CA)
    inline tls::ServerSettings server_settings(uint8_t mode) {
        tls::ServerSettings s;
        s.identities.push_back(tls::identity_of({unhex(tls_identities::all[0].certificate)}, server_key()));
        if (mode & 1) {
            s.groups = {0x0017};
            s.retry_cookie = unhex("0102030405");
            s.alpn = {sgcl::string("h2")};
        }
        if (mode & 2) {
            s.tickets = &ticket_keys();
        }
        if (mode & 4) {
            s.client_auth = 2;
            s.client_roots = ca();
        }
        return s;
    }

    // A session whose ticket the fixed keys open: suite 0x1301, issued a
    // second before Now for a day, a PSK of 0x42s, no client chain
    inline sgcl::tracked_ptr<tls::Session> session() {
        sgcl::tracked_ptr<tls::Session> s = sgcl::make_tracked<tls::Session>();
        s->cipher = 0x1301;
        s->psk = std::make_unique<tls::Secret>();
        s->psk->size = 32;
        std::memset(s->psk->bytes, 0x42, 32);
        s->received_ms = Now - 1000;
        s->lifetime = 86400;
        s->age_add = 0x01020304;
        std::vector<sgcl::byte> content, ticket;
        tls::Builder w(content);
        tls::write_ticket_content(w, s->cipher, s->received_ms, s->lifetime, s->age_add, *s->psk, sgcl::crypto::x509::chain());
        ServerEntropy e;
        ticket_keys().seal(ticket, tls::bytes_of(content.data(), content.size()), Now, int64_t(s->lifetime) * 1000, e.entropy());
        s->ticket.assign(ticket.data(), ticket.data() + ticket.size());
        return s;
    }

    // The client of the resumption and mTLS mode: the session offered, its
    // tickets kept, the client certificate, ALPN
    inline tls::ClientSettings auth_client_settings() {
        tls::ClientSettings s;
        s.server_name = sgcl::string("example.test");
        s.insecure_skip_verify = true;
        s.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        s.identities.push_back(tls::identity_of({unhex(tls_client_identities::client_certificate)}, client_key()));
        s.session = session();
        s.resumption = true;
        return s;
    }

    // The client of TLS 1.2 (modes 4 and 5): 1.3 and 1.2 offered, or 1.2
    // alone, the suites of both, X25519 and the NIST curves
    inline tls::ClientSettings tls12_client_settings(bool tls13) {
        tls::ClientSettings s;
        s.server_name = sgcl::string("example.test");
        s.insecure_skip_verify = true;
        s.tls13 = tls13;
        s.tls12 = true;
        s.ciphers = {0x1301, 0x1302, 0x1303, 0xC02B, 0xC02F, 0xCCA9, 0xCCA8, 0xC02C, 0xC030};
        s.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        s.identities.push_back(tls::identity_of({unhex(tls_client_identities::client_certificate)}, client_key()));
        return s;
    }

    // A TLS 1.2 session the seeds' server (tls12_server.h) knows: suite
    // 0xC02B, a master secret of 0x42s, a ticket of 48 bytes and a session
    // id of 32, received a second before Now for a day, no chain
    // (insecure_skip_verify)
    inline constexpr uint16_t Session12Cipher = 0xC02B;

    inline std::vector<uint8_t> session12_ticket() {
        std::vector<uint8_t> t(48);
        for (size_t i = 0; i < t.size(); ++i) {
            t[i] = uint8_t(0x7C ^ i);
        }
        return t;
    }

    inline std::vector<uint8_t> session12_id() {
        std::vector<uint8_t> id(32);
        for (size_t i = 0; i < id.size(); ++i) {
            id[i] = uint8_t(0x5A ^ (i * 3));
        }
        return id;
    }

    inline sgcl::tracked_ptr<tls::Session> session12() {
        sgcl::tracked_ptr<tls::Session> s = sgcl::make_tracked<tls::Session>();
        s->version = tls::Tls12;
        s->cipher = Session12Cipher;
        s->psk = std::make_unique<tls::Secret>();
        s->psk->size = 48;
        std::memset(s->psk->bytes, 0x42, 48);
        s->received_ms = Now - 1000;
        s->lifetime = 86400;
        auto t = session12_ticket();
        auto p = reinterpret_cast<const sgcl::byte*>(t.data());
        s->ticket.assign(p, p + t.size());
        auto id = session12_id();
        std::memcpy(s->session_id, id.data(), id.size());
        s->session_id_size = uint8_t(id.size());
        return s;
    }

    // The client of TLS 1.2's resumption (modes 8 and 9): the session
    // above offered by its ticket (1.3 and 1.2 offered), or by its session
    // id (tickets not asked for, 1.2 alone); its sessions kept
    inline tls::ClientSettings tls12_resume_settings(bool by_ticket) {
        tls::ClientSettings s = tls12_client_settings(by_ticket);
        s.session = session12();
        s.resumption = true;
        s.tickets12 = by_ticket;
        return s;
    }

    // The harnesses' pseudo-message of a change_cipher_spec record (a type no
    // handshake message has): the client's change_cipher_spec() in place of
    // a feed
    inline constexpr uint8_t ChangeCipherSpec = 0xFF;

    // Encrypted Client Hello (tls_ech_fuzz.cpp): the server's key, an
    // X25519 key derived from a fixed seed, and its ECHConfig (id 7, the
    // public name "public.test", HKDF-SHA256 with AES-128-GCM and
    // ChaCha20-Poly1305, names padded to 32)
    inline const sgcl::crypto::hpke::private_key& ech_private_key() {
        static const auto k = [] {
            uint8_t ikm[32];
            for (size_t i = 0; i < sizeof ikm; ++i) {
                ikm[i] = uint8_t(0xEC ^ i);
            }
            return sgcl::crypto::hpke::private_key::derive(sgcl::crypto::hpke::kem::dhkem_x25519, tls::bytes_of(ikm, sizeof ikm));
        }();
        return k;
    }

    inline std::vector<sgcl::byte> ech_config() {
        auto pub = ech_private_key().public_key().bytes();
        return tls::write_ech_config(7, 0x0020, tls::bytes_of(pub.data(), pub.size()), {0x00010001, 0x00010003}, 32, "public.test");
    }

    // The ECHConfigList of the config
    inline std::vector<sgcl::byte> ech_config_list() {
        auto c = ech_config();
        std::vector<sgcl::byte> l = {sgcl::byte(c.size() >> 8), sgcl::byte(c.size())};
        l.insert(l.end(), c.begin(), c.end());
        return l;
    }

    // The server of a mode (server_settings) with the key, its config sent
    // as retry_configs
    inline tls::ServerSettings ech_server_settings(uint8_t mode) {
        tls::ServerSettings s = server_settings(mode);
        tls::EchServerKey k;
        k.config = ech_config();
        k.id = 7;
        k.kem = 0x0020;
        k.suites = {0x00010001, 0x00010003};
        k.key = &ech_private_key();
        s.ech_keys.push_back(std::move(k));
        s.ech_retry_configs = ech_config_list();
        return s;
    }

    // A client of the config: "example.test" sealed, ALPN; `list` the
    // ECHConfigList it was given (another key's to be rejected)
    inline tls::ClientSettings ech_client_settings(const std::vector<sgcl::byte>& list) {
        tls::ClientSettings s;
        s.server_name = sgcl::string("example.test");
        s.insecure_skip_verify = true;
        s.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        s.ech = tls::choose_ech(tls::bytes_of(list.data(), list.size()));
        return s;
    }
}
