//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's TLS 1.2 below the connection:
//
//   - the PRF (prf.h) against OpenSSL's TLS1-PRF on random inputs, P_SHA256
//     and P_SHA384, with the client's labels (tls12_prf_vectors.h,
//     tools/tls12_prf_oracle.cpp), and the key block and verify_data made
//     of it;
//   - the record layer's 1.2 AEAD (record.h): every suite sealed and opened,
//     in place and out of place, AES-GCM's explicit nonce, the additional
//     data (a byte of the header changed does not open), the limits, no
//     KeyUpdate, change_cipher_spec reported as taken;
//   - the 1.2 messages read (messages.h): ServerKeyExchange,
//     CertificateRequest, Certificate, ServerHelloDone, every truncation a
//     decode_error;
//   - the client's machine (handshake.h) against a TLS 1.2 server written
//     here from RFC 5246, RFC 7627 and RFC 8422 at the level of messages:
//     every suite × group × kind of key and the signature schemes of 1.2
//     (PKCS #1 v1.5, PSS, ECDSA with a hash of another curve's, Ed25519),
//     the keys of both directions as the server makes them, ALPN, a client
//     certificate (ECDSA, Ed25519, RSA by PKCS #1 v1.5 and by PSS, none of a
//     type not asked for), and every refusal: the downgrade sentinel, no
//     extended master secret, renegotiation_info of a renegotiation, a
//     session id echoed, an extension not offered, point formats, a suite
//     or a curve not offered, a signature that does not verify or of a
//     scheme not offered, a leaf of the other kind, a Finished that does
//     not verify, change_cipher_spec and Finished out of order, a version
//     before 1.2, 1.2 not offered, a 1.2 ServerHello after a
//     HelloRetryRequest, and what comes after the handshake (HelloRequest
//     passed over, KeyUpdate and NewSessionTicket refused);
//   - resumption (RFC 5246 §7.3, RFC 5077): the session of a full handshake
//     (its ticket, its session id, the lifetime of the ticket's hint),
//     offered by ticket (a random session id beside it) or by session id,
//     the abbreviated handshake (the keys of the session's master secret,
//     the server's change_cipher_spec and Finished first, the peer's chain
//     the session's, the session kept again, a renewed ticket), and every
//     refusal and boundary: a session declined (a full handshake), of
//     another suite, without the extended master secret, expired, of a
//     suite no longer offered, of the other version; a NewSessionTicket not
//     announced, announced and missing, empty, past MaxTicket, malformed;
//     a SessionTicket extension with a body.
#include "tests/types.h"

#include "sgcl/net/tls/detail/handshake.h"
#include "tls12_prf_vectors.h"
#include "tls12_server.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;
    using tls12_server::Server12;
    using tls12_server::keys;
    using tls::Action;
    using tls::AlertDescription;

    bytes_t unhex(const char* s) {
        bytes_t v;
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3] = {s[i], s[i + 1], 0};
            v.push_back(uint8_t(std::strtoul(b, nullptr, 16)));
        }
        return v;
    }

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    bytes_t of(const std::vector<sgcl::byte>& v) {
        auto p = reinterpret_cast<const uint8_t*>(v.data());
        return bytes_t(p, p + v.size());
    }

    template<class C>
    bytes_t raw(const C& c) {
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        return bytes_t(p, p + c.size());
    }

    bytes_t of(const tls::Secret& s) {
        return bytes_t(s.bytes, s.bytes + s.size);
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
}

// --- the PRF ------------------------------------------------------------------------

TEST(Tls12Prf, OpenSslsTls1Prf) {
    size_t n = 0;
    for (auto& v : tls12_vectors::prfs) {
        SCOPED_TRACE(std::string(v.label) + " " + std::to_string(v.hash));
        bytes_t secret = unhex(v.secret), seed1 = unhex(v.seed1), seed2 = unhex(v.seed2), expected = unhex(v.out);
        bytes_t out(expected.size());
        tls::prf12(v.hash == 256 ? tls::Hash::sha256 : tls::Hash::sha384, tls::room_of(out.data(), out.size()), view(secret), v.label, view(seed1), view(seed2));
        EXPECT_EQ(out, expected);
        // the same seed in one part
        bytes_t joined = seed1;
        joined.insert(joined.end(), seed2.begin(), seed2.end());
        bytes_t again(expected.size());
        tls::prf12(v.hash == 256 ? tls::Hash::sha256 : tls::Hash::sha384, tls::room_of(again.data(), again.size()), view(secret), v.label, view(joined));
        EXPECT_EQ(again, expected);
        ++n;
    }
    EXPECT_EQ(n, 10u);
}

// The extended master secret, the key block and verify_data as the PRF of
// their labels (RFC 7627 §4, RFC 5246 §6.3, §7.4.9)
TEST(Tls12Prf, TheSecretsOfTheHandshake) {
    for (auto& v : tls12_vectors::prfs) {
        const tls::Hash h = v.hash == 256 ? tls::Hash::sha256 : tls::Hash::sha384;
        bytes_t secret = unhex(v.secret), seed1 = unhex(v.seed1), seed2 = unhex(v.seed2), expected = unhex(v.out);
        if (std::string(v.label) == "extended master secret") {
            tls::Secret master;
            tls::extended_master_secret(h, master, view(secret), view(seed1));
            EXPECT_EQ(of(master), expected);
        } else if (std::string(v.label) == "client finished" || std::string(v.label) == "server finished") {
            tls::Secret master;
            std::memcpy(master.bytes, secret.data(), secret.size());
            master.size = uint8_t(secret.size());
            bytes_t out(12);
            tls::finished12(h, out.data(), master, std::string(v.label) == "client finished", view(seed1));
            EXPECT_EQ(out, expected);
        } else if (std::string(v.label) == "key expansion") {
            // seed1 the server's random, seed2 the client's (§6.3)
            tls::Secret master;
            std::memcpy(master.bytes, secret.data(), secret.size());
            master.size = uint8_t(secret.size());
            for (auto c : {tls::Cipher::ecdhe_rsa_aes_128_gcm_sha256, tls::Cipher::ecdhe_ecdsa_aes_256_gcm_sha384, tls::Cipher::ecdhe_rsa_chacha20_poly1305_sha256}) {
                if (tls::hash_of(c) != h) {
                    continue;
                }
                tls::Secret client, server;
                tls::key_block12(c, client, server, master, view(seed2), view(seed1));
                const size_t k = tls::key_size(c), iv = tls::iv_size12(c);
                ASSERT_LE(2 * k + 2 * iv, expected.size());
                bytes_t ck(expected.begin(), expected.begin() + long(k)), sk(expected.begin() + long(k), expected.begin() + long(2 * k));
                bytes_t civ(expected.begin() + long(2 * k), expected.begin() + long(2 * k + iv)), siv(expected.begin() + long(2 * k + iv), expected.begin() + long(2 * k + 2 * iv));
                ck.insert(ck.end(), civ.begin(), civ.end());
                sk.insert(sk.end(), siv.begin(), siv.end());
                EXPECT_EQ(of(client), ck);
                EXPECT_EQ(of(server), sk);
            }
        }
    }
}

// --- the record layer ---------------------------------------------------------------

namespace {
    tls::Secret key_iv(tls::Cipher c, uint8_t fill) {
        tls::Secret s;
        s.size = uint8_t(tls::key_size(c) + tls::iv_size12(c));
        for (size_t i = 0; i < s.size; ++i) {
            s.bytes[i] = uint8_t(fill + i);
        }
        return s;
    }

    bytes_t seal12(tls::RecordProtection& w, tls::ContentType type, const bytes_t& content) {
        bytes_t out(w.sealed_size(type, content.size()));
        size_t n = w.seal(type, view(content), out.data());
        EXPECT_EQ(n, out.size());
        return out;
    }

    constexpr tls::Cipher suites12[] = {tls::Cipher::ecdhe_ecdsa_aes_128_gcm_sha256, tls::Cipher::ecdhe_ecdsa_aes_256_gcm_sha384, tls::Cipher::ecdhe_rsa_aes_128_gcm_sha256,
                                        tls::Cipher::ecdhe_rsa_aes_256_gcm_sha384, tls::Cipher::ecdhe_rsa_chacha20_poly1305_sha256, tls::Cipher::ecdhe_ecdsa_chacha20_poly1305_sha256};
}

TEST(Tls12Record, SealedAndOpenedEverySuite) {
    for (auto c : suites12) {
        SCOPED_TRACE(int(c));
        tls::RecordProtection w, r;
        w.install12(c, key_iv(c, 3));
        r.install12(c, key_iv(c, 3));
        EXPECT_FALSE(w.needs_update());
        const size_t e = tls::explicit_nonce12(c);
        for (size_t n : {size_t(0), size_t(1), size_t(100), tls::MaxPlaintext}) {
            bytes_t content(n);
            for (size_t i = 0; i < n; ++i) {
                content[i] = uint8_t(i * 7);
            }
            const uint64_t seq = w.sequence();
            bytes_t rec = seal12(w, tls::ContentType::application_data, content);
            ASSERT_EQ(rec.size(), 5 + e + n + 16);
            EXPECT_EQ(rec[0], 23);
            EXPECT_EQ(rec[1], 3);
            EXPECT_EQ(rec[2], 3);
            if (e) {
                // the explicit nonce: the sequence number (RFC 5288 §3)
                uint64_t explicit_part = 0;
                for (size_t i = 0; i < 8; ++i) {
                    explicit_part = explicit_part << 8 | rec[5 + i];
                }
                EXPECT_EQ(explicit_part, seq);
            }
            // out of place, then in place
            bytes_t plain(n + 32);
            bytes_t copy = rec;
            auto o = r.open(copy.data(), copy.size(), plain.data());
            ASSERT_TRUE(o.has_value()) << o.error().what;
            EXPECT_EQ(o->type, tls::ContentType::application_data);
            EXPECT_EQ(of(o->fragment), content);
            bytes_t again = seal12(w, tls::ContentType::handshake, n ? content : bytes_t{1});
            auto p = r.open(again.data(), again.size());
            ASSERT_TRUE(p.has_value()) << p.error().what;
            EXPECT_EQ(p->type, tls::ContentType::handshake);
            EXPECT_EQ(of(p->fragment), n ? content : bytes_t{1});
        }
        EXPECT_EQ(w.sequence(), 8u);
        EXPECT_EQ(r.sequence(), 8u);
        // a byte changed: the type (in the additional data), the explicit
        // nonce, the ciphertext, the tag; and another sequence number
        tls::RecordProtection w2;
        w2.install12(c, key_iv(c, 3));
        bytes_t rec = seal12(w2, tls::ContentType::alert, {1, 0});
        for (size_t i : {size_t(0), size_t(5), size_t(5 + e), rec.size() - 1}) {
            tls::RecordProtection rr;
            rr.install12(c, key_iv(c, 3));
            bytes_t bad = rec;
            if (i == 0) {
                bad[0] = 22;   // handshake in place of alert
            } else {
                bad[i] ^= 0x40;
            }
            auto x = rr.open(bad.data(), bad.size());
            ASSERT_FALSE(x.has_value()) << i;
            EXPECT_EQ(x.error().description, AlertDescription::bad_record_mac) << i;
            bytes_t good = rec;
            EXPECT_TRUE(rr.open(good.data(), good.size()).has_value()) << i;   // a failed open leaves the sequence
        }
        tls::RecordProtection late;
        late.install12(c, key_iv(c, 3));
        bytes_t first = rec;
        ASSERT_TRUE(late.open(first.data(), first.size()).has_value());
        bytes_t replayed = rec;
        EXPECT_FALSE(late.open(replayed.data(), replayed.size()).has_value());   // sequence 1 now
    }
}

TEST(Tls12Record, TheLimitsAndTheTypes) {
    const auto c = tls::Cipher::ecdhe_rsa_aes_128_gcm_sha256;
    tls::RecordProtection w, r;
    w.install12(c, key_iv(c, 9));
    r.install12(c, key_iv(c, 9));
    // shorter than the explicit nonce and the tag: bad_record_mac
    bytes_t tiny = {23, 3, 3, 0, 23};
    tiny.resize(5 + 23);
    auto t = r.open(tiny.data(), tiny.size());
    ASSERT_FALSE(t.has_value());
    EXPECT_EQ(t.error().description, AlertDescription::bad_record_mac);
    // over 2^14 + 256: record_overflow, before anything is opened
    bytes_t big(5 + tls::MaxCiphertext + 1);
    big[0] = 23;
    big[1] = big[2] = 3;
    big[3] = uint8_t((big.size() - 5) >> 8);
    big[4] = uint8_t(big.size() - 5);
    auto b = r.open(big.data(), big.size());
    ASSERT_FALSE(b.has_value());
    EXPECT_EQ(b.error().description, AlertDescription::record_overflow);
    // an empty handshake record refused; an empty application data one taken
    bytes_t empty_hs(w.sealed_size(tls::ContentType::handshake, 0));
    w.seal(tls::ContentType::handshake, sgcl::slice<const sgcl::byte>(), empty_hs.data());
    auto eh = r.open(empty_hs.data(), empty_hs.size());
    ASSERT_FALSE(eh.has_value());
    EXPECT_EQ(eh.error().description, AlertDescription::unexpected_message);
    // a change_cipher_spec under the keys: taken while accepted, refused after
    bytes_t ccs = {20, 3, 3, 0, 1, 1};
    r.accept_ccs(true);
    auto cc = r.open(bytes_t(ccs).data(), ccs.size());
    ASSERT_TRUE(cc.has_value());
    EXPECT_EQ(cc->type, tls::ContentType::change_cipher_spec);
    r.accept_ccs(false);
    auto cd = r.open(bytes_t(ccs).data(), ccs.size());
    EXPECT_FALSE(cd.has_value());
    // padding is TLS 1.3's alone
    bytes_t out(100);
    EXPECT_THROW(w.seal(tls::ContentType::application_data, view(bytes_t(3)), out.data(), 1), std::logic_error);
    // an unknown type: unexpected_message
    bytes_t odd = seal12(w, tls::ContentType::application_data, {1, 2, 3});
    odd[0] = 25;
    auto u = r.open(odd.data(), odd.size());
    ASSERT_FALSE(u.has_value());
    EXPECT_EQ(u.error().description, AlertDescription::unexpected_message);
    // a KeyUpdate is never asked for in 1.2
    EXPECT_FALSE(w.needs_update());
}

// --- the messages -------------------------------------------------------------------

TEST(Tls12Messages, ReadAndRefused) {
    // ServerKeyExchange: a named curve, its point, a signature
    bytes_t ske = {3, 0x00, 0x1D, 4, 1, 2, 3, 4, 0x04, 0x03, 0, 2, 9, 9};
    auto s = tls::read_server_key_exchange12(view(ske));
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->group, 0x001Du);
    EXPECT_EQ(of(s->point), (bytes_t{1, 2, 3, 4}));
    EXPECT_EQ(of(s->params), (bytes_t{3, 0x00, 0x1D, 4, 1, 2, 3, 4}));
    EXPECT_EQ(s->scheme, 0x0403u);
    EXPECT_EQ(of(s->signature), (bytes_t{9, 9}));
    for (size_t n = 0; n < ske.size(); ++n) {
        auto cut = tls::read_server_key_exchange12(view(bytes_t(ske.begin(), ske.begin() + long(n))));
        ASSERT_FALSE(cut.has_value()) << n;
        EXPECT_EQ(cut.error().description, AlertDescription::decode_error) << n;
    }
    bytes_t explicit_curve = ske;
    explicit_curve[0] = 1;
    EXPECT_EQ(tls::read_server_key_exchange12(view(explicit_curve)).error().description, AlertDescription::illegal_parameter);
    bytes_t empty_point = {3, 0, 0x1D, 0, 4, 3, 0, 0};
    EXPECT_EQ(tls::read_server_key_exchange12(view(empty_point)).error().description, AlertDescription::decode_error);
    // CertificateRequest: types, schemes, authorities
    bytes_t cr = {2, 1, 64, 0, 4, 0x04, 0x03, 0x04, 0x01, 0, 5, 0, 3, 'a', 'b', 'c'};
    auto r = tls::read_certificate_request12(view(cr));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(of(r->types), (bytes_t{1, 64}));
    EXPECT_EQ(r->schemes.size(), 2u);
    EXPECT_EQ(r->authorities.count, 1u);
    for (size_t n = 0; n < cr.size(); ++n) {
        EXPECT_FALSE(tls::read_certificate_request12(view(bytes_t(cr.begin(), cr.begin() + long(n))))) << n;
    }
    bytes_t no_authorities = {1, 64, 0, 2, 0x04, 0x03, 0, 0};
    EXPECT_TRUE(tls::read_certificate_request12(view(no_authorities)).has_value());
    bytes_t odd_schemes = {1, 64, 0, 3, 0x04, 0x03, 0x01, 0, 0};
    EXPECT_FALSE(tls::read_certificate_request12(view(odd_schemes)).has_value());
    // Certificate: a list of DERs, an empty one read (the machine refuses it)
    bytes_t cert = {0, 0, 8, 0, 0, 2, 0xAA, 0xBB, 0, 0, 0};
    EXPECT_FALSE(tls::read_certificate12(view(cert)).has_value());   // an entry of length 0
    bytes_t cert2 = {0, 0, 5, 0, 0, 2, 0xAA, 0xBB};
    auto c2 = tls::read_certificate12(view(cert2));
    ASSERT_TRUE(c2.has_value());
    EXPECT_EQ(c2->count, 1u);
    EXPECT_EQ(tls::read_certificate12(view(bytes_t{0, 0, 0}))->count, 0u);
    // ServerHelloDone: empty
    EXPECT_TRUE(tls::read_server_hello_done(view(bytes_t())).has_value());
    EXPECT_EQ(tls::read_server_hello_done(view(bytes_t{0})).error().description, AlertDescription::decode_error);
    // ec_point_formats
    EXPECT_EQ(of(*tls::read_point_formats(view(bytes_t{2, 0, 1}))), (bytes_t{0, 1}));
    EXPECT_FALSE(tls::read_point_formats(view(bytes_t{0})).has_value());
    // a ServerHello of TLS 1.1: protocol_version
    bytes_t sh(2 + 32 + 1 + 2 + 1);
    sh[0] = 3;
    sh[1] = 2;
    EXPECT_EQ(tls::read_server_hello(view(sh)).error().description, AlertDescription::protocol_version);
    sh[1] = 3;
    auto bare = tls::read_server_hello(view(sh));   // no extensions at all: TLS 1.2's form
    ASSERT_TRUE(bare.has_value());
    EXPECT_EQ(bare->extensions.count, 0u);
}

// --- the machine against a TLS 1.2 server of the test's --------------------------------

namespace {
    // A client's machine and its log: the messages it sent, its installs
    struct Driven {
        tls::ClientHandshake client;
        std::vector<bytes_t> sent;              // messages
        std::vector<std::string> kinds;
        std::map<std::string, bytes_t> installs;
        std::vector<AlertDescription> alerts;
        sgcl::tracked_ptr<tls::Session> kept;   // the last session the machine gave
        size_t sessions = 0;

        explicit Driven(const tls::ClientSettings& s)
        : client(s) {
        }

        void take(const tls::Step& step) {
            for (auto& a : step.actions) {
                switch (a.kind) {
                case Action::Kind::send:
                    for (auto& m : split(of(step.bytes(a)))) {
                        sent.push_back(m);
                    }
                    kinds.push_back("send");
                    break;
                case Action::Kind::change_cipher_spec:
                    kinds.push_back("ccs");
                    break;
                case Action::Kind::install_read:
                case Action::Kind::install_write: {
                    std::string k = std::string(a.kind == Action::Kind::install_read ? "read" : "write") + (a.tls12 ? "12" : "13");
                    installs[k] = of(a.secret);
                    kinds.push_back(k);
                    break;
                }
                case Action::Kind::established:
                    kinds.push_back("established");
                    break;
                case Action::Kind::alert:
                    alerts.push_back(a.alert);
                    kinds.push_back("alert");
                    break;
                case Action::Kind::new_ticket: {
                    // as the connection keeps it (impl.h, _keep_session)
                    kept = sgcl::make_tracked<tls::Session>();
                    kept->version = a.tls12 ? tls::Tls12 : tls::Tls13;
                    kept->cipher = uint16_t(a.cipher);
                    auto t = step.bytes(a);
                    kept->ticket.assign(t.data(), t.data() + t.size());
                    kept->psk = std::make_unique<tls::Secret>();
                    std::memcpy(kept->psk->bytes, a.secret.bytes, sizeof a.secret.bytes);
                    kept->psk->size = a.secret.size;
                    kept->received_ms = a.issued_ms;
                    kept->lifetime = a.lifetime;
                    std::memcpy(kept->session_id, a.session_id, a.session_id_size);
                    kept->session_id_size = a.session_id_size;
                    kept->group = a.group;
                    kept->peer_certificates = client.result().peer_certificates;
                    ++sessions;
                    kinds.push_back("session");
                    break;
                }
                default:
                    kinds.push_back("other");
                    break;
                }
            }
        }

        // The whole handshake with the server; false when either refused
        bool handshake(Server12& server) {
            take(client.start());
            if (sent.empty()) {
                return false;
            }
            for (auto& m : server.hello(sent[0])) {
                take(client.feed(view(m)));
                if (!alerts.empty()) {
                    return false;
                }
            }
            if (server.resumed) {
                // the abbreviated handshake: the server's change_cipher_spec
                // and Finished, then the client's
                take(client.change_cipher_spec());
                take(client.feed(view(server.server_finished())));
                return alerts.empty() && client.established() && sent.size() == 2 && server.client_finished(sent[1]);
            }
            std::vector<bytes_t> flight(sent.begin() + 1, sent.end());
            bytes_t fin = server.flight(flight);
            if (fin.empty()) {
                return false;
            }
            for (auto& m : server.before_ccs) {
                take(client.feed(view(m)));
            }
            take(client.change_cipher_spec());
            take(client.feed(view(fin)));
            return alerts.empty() && client.established();
        }

        AlertDescription alert() const {
            return alerts.empty() ? AlertDescription::close_notify : alerts[0];
        }
    };

    tls::ClientSettings settings12(bool tls13 = true) {
        tls::ClientSettings s;
        s.server_name = "example.test";
        s.insecure_skip_verify = true;   // self-signed: the signatures are still checked under the leaf's key
        s.tls13 = tls13;
        s.tls12 = true;
        s.ciphers = {0x1301, 0x1302, 0x1303, 0xC02B, 0xC02F, 0xCCA9, 0xCCA8, 0xC02C, 0xC030};
        return s;
    }
}

TEST(Tls12Client, EverySuiteGroupAndKey) {
    struct Leaf {
        const char* name;
        std::vector<uint16_t> schemes;
    };
    const Leaf leaves[] = {{"p256", {0x0403, 0x0503}}, {"p384", {0x0503, 0x0403}}, {"ed25519", {0x0807}}, {"rsa", {0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601}}};
    size_t n = 0;
    for (auto& leaf : leaves) {
        for (uint16_t cipher : {0xC02B, 0xC02C, 0xCCA9, 0xC02F, 0xC030, 0xCCA8}) {
            const bool rsa = cipher == 0xC02F || cipher == 0xC030 || cipher == 0xCCA8;
            if (rsa != (std::string(leaf.name) == "rsa")) {
                continue;
            }
            for (uint16_t group : {0x001D, 0x0017, 0x0018}) {
                for (uint16_t scheme : leaf.schemes) {
                    SCOPED_TRACE(std::string(leaf.name) + " " + std::to_string(cipher) + " " + std::to_string(group) + " " + std::to_string(scheme));
                    Server12 server;
                    server.cipher = cipher;
                    server.group = group;
                    server.leaf = leaf.name;
                    server.scheme = scheme;
                    Driven run(settings12());
                    ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
                    EXPECT_EQ(run.client.result().version, tls::Tls12);
                    EXPECT_EQ(uint16_t(run.client.result().cipher), cipher);
                    EXPECT_EQ(uint16_t(run.client.result().group), group);
                    EXPECT_EQ(run.installs["write12"], of(server.client_keys));
                    EXPECT_EQ(run.installs["read12"], of(server.server_keys));
                    EXPECT_EQ(run.kinds, (std::vector<std::string>{"send", "send", "ccs", "write12", "send", "read12", "established"}));
                    ++n;
                }
            }
        }
    }
    EXPECT_EQ(n, 3u * 3 * (2 + 2 + 1) + 3u * 3 * 6);
}

TEST(Tls12Client, AlpnAndOneVersionAlone) {
    Server12 server;
    server.alpn = "h2";
    tls::ClientSettings s = settings12();
    s.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
    Driven run(s);
    ASSERT_TRUE(run.handshake(server)) << int(run.alert());
    EXPECT_EQ(run.client.result().alpn, "h2");
    // a protocol not offered
    Server12 other;
    other.alpn = "spdy/3";
    Driven refused(s);
    EXPECT_FALSE(refused.handshake(other));
    EXPECT_EQ(refused.alert(), AlertDescription::illegal_parameter);
    // ALPN where none was offered
    Server12 unasked;
    unasked.alpn = "h2";
    Driven none(settings12());
    EXPECT_FALSE(none.handshake(unasked));
    EXPECT_EQ(none.alert(), AlertDescription::unsupported_extension);
    // 1.2 alone: no supported_versions, no key_share; the sentinel is no matter
    Server12 sentinel;
    sentinel.downgrade = true;
    Driven alone(settings12(false));
    ASSERT_TRUE(alone.handshake(sentinel)) << int(alone.alert());
    auto ch = tls::read_client_hello(tls::read_handshake(view(alone.sent[0]))->body);
    EXPECT_FALSE(ch->extensions.has(tls::ExtensionType::supported_versions));
    EXPECT_FALSE(ch->extensions.has(tls::ExtensionType::key_share));
    EXPECT_TRUE(ch->extensions.has(tls::ExtensionType::extended_master_secret));
    EXPECT_TRUE(ch->extensions.has(tls::ExtensionType::ec_point_formats));
}

TEST(Tls12Client, TheClientHelloOfBothVersions) {
    Driven run(settings12());
    run.take(run.client.start());
    auto ch = tls::read_client_hello(tls::read_handshake(view(run.sent.at(0)))->body);
    ASSERT_TRUE(ch.has_value());
    auto versions = tls::read_versions_offered(*ch->extensions.find(tls::ExtensionType::supported_versions));
    EXPECT_TRUE(versions->contains(tls::Tls13));
    EXPECT_TRUE(versions->contains(tls::Tls12));
    EXPECT_TRUE(ch->extensions.has(tls::ExtensionType::key_share));
    EXPECT_TRUE(ch->extensions.find(uint16_t(0xFF01)).has_value());
    EXPECT_TRUE(ch->cipher_suites.contains(0xC02B));
    EXPECT_TRUE(ch->cipher_suites.contains(0x1301));
    // 1.3 alone, the default of the machine: none of 1.2's
    tls::ClientSettings only13;
    only13.server_name = "example.test";
    only13.insecure_skip_verify = true;
    Driven r13(only13);
    r13.take(r13.client.start());
    auto c13 = tls::read_client_hello(tls::read_handshake(view(r13.sent.at(0)))->body);
    EXPECT_FALSE(c13->extensions.has(tls::ExtensionType::extended_master_secret));
    EXPECT_FALSE(tls::read_versions_offered(*c13->extensions.find(tls::ExtensionType::supported_versions))->contains(tls::Tls12));
}

TEST(Tls12Client, TheServerHelloRefused) {
    struct Case {
        const char* what;
        std::function<void(Server12&)> set;
        AlertDescription alert;
        bool tls13 = true;
    };
    const Case cases[] = {
        {"the downgrade sentinel", [](Server12& s) { s.downgrade = true; }, AlertDescription::illegal_parameter},
        {"no extended master secret", [](Server12& s) { s.ems = false; }, AlertDescription::handshake_failure},
        {"renegotiation_info of a renegotiation", [](Server12& s) { s.reneg_body = {12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; }, AlertDescription::handshake_failure},
        {"the session id echoed", [](Server12& s) { s.echo_session = true; }, AlertDescription::illegal_parameter},
        {"an extension not offered", [](Server12& s) { s.extra_extension = 5; }, AlertDescription::unsupported_extension},
        {"an unknown extension", [](Server12& s) { s.extra_extension = 0x7777; }, AlertDescription::unsupported_extension},
        {"point formats without uncompressed", [](Server12& s) { s.formats = {1, 2}; }, AlertDescription::illegal_parameter},
        {"a CBC suite", [](Server12& s) { s.cipher = 0xC013; }, AlertDescription::illegal_parameter},
        {"a 1.3 suite without supported_versions", [](Server12& s) { s.cipher = 0x1301; }, AlertDescription::illegal_parameter},
        {"TLS 1.1", [](Server12& s) { s.version = 0x0302; }, AlertDescription::protocol_version},
    };
    for (auto& c : cases) {
        SCOPED_TRACE(c.what);
        Server12 server;
        c.set(server);
        Driven run(settings12(c.tls13));
        EXPECT_FALSE(run.handshake(server));
        EXPECT_EQ(run.alert(), c.alert);
        EXPECT_TRUE(run.client.failed());
    }
    // what is taken: no renegotiation_info at all, no point formats
    for (int k = 0; k < 2; ++k) {
        Server12 server;
        (k == 0 ? server.reneg : server.point_formats) = false;
        Driven run(settings12());
        EXPECT_TRUE(run.handshake(server)) << k << " " << int(run.alert());
    }
    // 1.2 not offered: a 1.2 ServerHello is protocol_version
    tls::ClientSettings only13 = settings12();
    only13.tls12 = false;
    Server12 server;
    Driven run(only13);
    EXPECT_FALSE(run.handshake(server));
    EXPECT_EQ(run.alert(), AlertDescription::protocol_version);
}

TEST(Tls12Client, TheServersKeyExchangeAndFinishedRefused) {
    struct Case {
        const char* what;
        std::function<void(Server12&)> set;
        AlertDescription alert;
    };
    const Case cases[] = {
        {"a signature that does not verify", [](Server12& s) { s.break_signature = true; }, AlertDescription::decrypt_error},
        {"a scheme not offered", [](Server12& s) { s.declared_scheme = 0x0203; }, AlertDescription::illegal_parameter},
        {"an RSA leaf for an ECDSA suite", [](Server12& s) { s.leaf = "rsa"; s.scheme = 0x0804; }, AlertDescription::unsupported_certificate},
        {"an ECDSA leaf for an RSA suite", [](Server12& s) { s.cipher = 0xC02F; }, AlertDescription::unsupported_certificate},
        {"a PSS scheme on an ECDSA key", [](Server12& s) { s.declared_scheme = 0x0804; }, AlertDescription::illegal_parameter},
        {"a curve not of 1.2", [](Server12& s) { s.group = 0x11EC; }, AlertDescription::illegal_parameter},
        {"a Finished that does not verify", [](Server12& s) { s.break_finished = true; }, AlertDescription::decrypt_error},
    };
    for (auto& c : cases) {
        SCOPED_TRACE(c.what);
        Server12 server;
        c.set(server);
        Driven run(settings12());
        EXPECT_FALSE(run.handshake(server));
        EXPECT_EQ(run.alert(), c.alert);
    }
    // a curve not offered: P-384 to a client that offers X25519 alone
    tls::ClientSettings s = settings12();
    s.groups = {0x001D};
    Server12 server;
    server.group = 0x0018;
    Driven run(s);
    EXPECT_FALSE(run.handshake(server));
    EXPECT_EQ(run.alert(), AlertDescription::illegal_parameter);
}

TEST(Tls12Client, OutOfOrder) {
    // change_cipher_spec before ServerHelloDone
    {
        Server12 server;
        Driven run(settings12());
        run.take(run.client.start());
        auto msgs = server.hello(run.sent[0]);
        run.take(run.client.feed(view(msgs[0])));
        run.take(run.client.change_cipher_spec());
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // the server's Finished before its change_cipher_spec
    {
        Server12 server;
        Driven run(settings12());
        run.take(run.client.start());
        for (auto& m : server.hello(run.sent[0])) {
            run.take(run.client.feed(view(m)));
        }
        bytes_t fin = server.flight(std::vector<bytes_t>(run.sent.begin() + 1, run.sent.end()));
        ASSERT_FALSE(fin.empty()) << server.error;
        run.take(run.client.feed(view(fin)));
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // a ServerKeyExchange in place of the Certificate
    {
        Server12 server;
        Driven run(settings12());
        run.take(run.client.start());
        auto msgs = server.hello(run.sent[0]);
        run.take(run.client.feed(view(msgs[0])));
        run.take(run.client.feed(view(msgs[2])));
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // a compatibility change_cipher_spec before the ServerHello: nothing
    {
        Driven run(settings12());
        run.take(run.client.start());
        run.take(run.client.change_cipher_spec());
        EXPECT_TRUE(run.alerts.empty());
    }
}

TEST(Tls12Client, AfterTheHandshake) {
    Server12 server;
    Driven run(settings12());
    ASSERT_TRUE(run.handshake(server));
    // a HelloRequest: passed over
    bytes_t hello_request = {0, 0, 0, 0};
    run.take(run.client.feed(view(hello_request)));
    EXPECT_TRUE(run.alerts.empty());
    // a KeyUpdate: TLS 1.3's
    bytes_t key_update = {24, 0, 0, 1, 0};
    run.take(run.client.feed(view(key_update)));
    EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    // a NewSessionTicket: none was offered
    Server12 again;
    Driven other(settings12());
    ASSERT_TRUE(other.handshake(again));
    bytes_t nst = {4, 0, 0, 6, 0, 0, 0, 0, 0, 1};
    other.take(other.client.feed(view(nst)));
    EXPECT_EQ(other.alert(), AlertDescription::unexpected_message);
}

TEST(Tls12Client, ClientCertificates) {
    struct Case {
        const char* identity;
        std::vector<uint16_t> asked;
        bytes_t types;
        bool sent;
    };
    const Case cases[] = {
        {"p256", {0x0403}, {64}, true},
        {"p384", {0x0503, 0x0403}, {1, 64}, true},
        {"ed25519", {0x0807}, {64}, true},
        {"rsa", {0x0401}, {1}, true},           // PKCS #1 v1.5
        {"rsa", {0x0804, 0x0401}, {1}, true},   // PSS first
        {"p256", {0x0403}, {1}, false},         // an ECDSA key where only RSA is asked for
        {"rsa", {0x0403}, {1, 64}, false},      // no scheme of the key
    };
    for (auto& c : cases) {
        SCOPED_TRACE(std::string(c.identity) + " " + std::to_string(c.asked[0]));
        Server12 server;
        server.request_certificate = true;
        server.request_schemes = c.asked;
        server.cert_types = c.types;
        tls::ClientSettings s = settings12();
        s.identities.push_back(keys().identity(c.identity));
        Driven run(s);
        ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_EQ(server.client_certificate, c.sent);
        EXPECT_EQ(server.client_verify_ok, c.sent);
        EXPECT_EQ(run.client.result().certificate_sent, c.sent);
    }
}

// A HelloRetryRequest of a TLS 1.3 server, then a ServerHello of 1.2: refused
TEST(Tls12Client, A12ServerHelloAfterARetry) {
    tls::ClientSettings s = settings12();
    s.key_shares = {0x001D};
    Driven run(s);
    run.take(run.client.start());
    std::vector<sgcl::byte> buf;
    tls::Builder w(buf);
    auto ch = tls::read_client_hello(tls::read_handshake(view(run.sent[0]))->body);
    tls::write_server_hello(w, tls::bytes_of(tls::HelloRetryRandom, 32), ch->session_id, 0x1301, [&](tls::Builder& w) {
        {
            auto e = w.extension(tls::ExtensionType::key_share);
            tls::write_key_share_retry(w, 0x0017);
        }
        auto e = w.extension(tls::ExtensionType::supported_versions);
        tls::write_version_selected(w, tls::Tls13);
    });
    run.take(run.client.feed(view(of(buf))));
    ASSERT_TRUE(run.alerts.empty());
    Server12 server;
    auto msgs = server.hello(run.sent.back());
    run.take(run.client.feed(view(msgs[0])));
    EXPECT_EQ(run.alert(), AlertDescription::illegal_parameter);
}

// --- resumption (RFC 5246 §7.3, RFC 5077) ------------------------------------------------

namespace {
    using tls12_server::Store;

    tls::ClientSettings resuming(bool tickets = true) {
        tls::ClientSettings s = settings12();
        s.resumption = true;
        s.tickets12 = tickets;
        return s;
    }

    tls::ClientHello hello_of(const bytes_t& m) {
        return tls::read_client_hello(tls::read_handshake(view(m))->body).value();
    }

    // A full handshake with a server of the store; the session it gave
    sgcl::tracked_ptr<tls::Session> first_session(Store& store, bool tickets, bool ids = true, uint16_t cipher = 0xC02B, const char* leaf = "p256") {
        Server12 server;
        server.store = &store;
        server.tickets = tickets;
        server.session_ids = ids;
        server.cipher = cipher;
        server.leaf = leaf;
        server.group = 0x0018;   // P-384: the resumed state's group, not the default's
        if (std::string(leaf) == "rsa") {
            server.scheme = 0x0804;
        } else if (std::string(leaf) == "ed25519") {
            server.scheme = 0x0807;
        }
        Driven run(resuming(tickets));
        EXPECT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_FALSE(run.client.result().resumed);
        return run.kept;
    }
}

TEST(Tls12Resume, TheSessionOfAFullHandshake) {
    // a ticket and a session id: both kept, the ticket's hint its lifetime
    {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.ticket_hint = 3600;
        Driven run(resuming());
        ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_EQ(run.kinds, (std::vector<std::string>{"send", "send", "ccs", "write12", "send", "read12", "established", "session"}));
        // the ClientHello asked for a ticket: an empty SessionTicket
        ASSERT_TRUE(server.ticket_offered);
        EXPECT_TRUE(server.offered_ticket.empty());
        ASSERT_TRUE(run.kept);
        EXPECT_EQ(run.sessions, 1u);
        EXPECT_EQ(run.kept->version, tls::Tls12);
        EXPECT_EQ(run.kept->cipher, 0xC02Bu);
        EXPECT_EQ(raw(run.kept->ticket), server.issued_ticket);
        EXPECT_EQ(bytes_t(run.kept->session_id, run.kept->session_id + run.kept->session_id_size), server.session_id);
        EXPECT_EQ(run.kept->session_id_size, 32u);
        EXPECT_EQ(bytes_t(run.kept->psk->bytes, run.kept->psk->bytes + run.kept->psk->size), of(server.master));
        EXPECT_EQ(run.kept->lifetime, 3600u);
        EXPECT_EQ(run.kept->peer_certificates.size(), 1u);
        EXPECT_TRUE(run.kept->fresh(sgcl::time::now().unix_milli()));
    }
    // the lifetime: none given a day, past seven days seven days
    for (uint32_t hint : {0u, 604801u, 1u}) {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.ticket_hint = hint;
        Driven run(resuming());
        ASSERT_TRUE(run.handshake(server));
        ASSERT_TRUE(run.kept);
        EXPECT_EQ(run.kept->lifetime, hint == 0 ? 86400u : hint > 604800 ? 604800u : hint) << hint;
    }
    // a session id alone: a day
    {
        Store store;
        Server12 server;
        server.store = &store;
        Driven run(resuming(false));
        ASSERT_TRUE(run.handshake(server));
        EXPECT_FALSE(server.ticket_offered);   // no tickets asked for
        ASSERT_TRUE(run.kept);
        EXPECT_TRUE(run.kept->ticket.empty());
        EXPECT_EQ(run.kept->session_id_size, 32u);
        EXPECT_EQ(run.kept->lifetime, 86400u);
    }
    // an empty ticket is none: the session id alone; with no session id either, no session
    for (bool ids : {true, false}) {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.empty_ticket = true;
        server.session_ids = ids;
        Driven run(resuming());
        ASSERT_TRUE(run.handshake(server)) << int(run.alert());
        EXPECT_EQ(bool(run.kept), ids);
        if (run.kept) {
            EXPECT_TRUE(run.kept->ticket.empty());
            EXPECT_EQ(run.kept->lifetime, 86400u);
        }
    }
    // a ticket past MaxTicket passed over: the session id alone
    {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.big_ticket = true;
        Driven run(resuming());
        ASSERT_TRUE(run.handshake(server)) << int(run.alert());
        ASSERT_TRUE(run.kept);
        EXPECT_TRUE(run.kept->ticket.empty());
    }
    // no resumption in the settings: no SessionTicket, no session
    {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        Driven run(settings12());
        ASSERT_TRUE(run.handshake(server));
        EXPECT_FALSE(server.ticket_offered);
        EXPECT_FALSE(run.kept);
    }
}

TEST(Tls12Resume, ByTicket) {
    for (auto [cipher, leaf] : {std::pair<uint16_t, const char*>{0xC02B, "p256"}, {0xC030, "rsa"}, {0xCCA9, "ed25519"}}) {
        SCOPED_TRACE(cipher);
        Store store;
        auto session = first_session(store, true, true, cipher, leaf);
        ASSERT_TRUE(session);
        const bytes_t ticket = raw(session->ticket);
        const auto peer = session->peer_certificates;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.cipher = cipher;
        server.leaf = leaf;
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_TRUE(server.resumed);
        EXPECT_TRUE(run.client.result().resumed);
        EXPECT_EQ(run.client.result().version, tls::Tls12);
        EXPECT_EQ(uint16_t(run.client.result().cipher), cipher);
        EXPECT_EQ(uint16_t(run.client.result().group), 0x0018u);   // the session's: no key exchange
        EXPECT_EQ(run.kinds, (std::vector<std::string>{"send", "read12", "ccs", "write12", "send", "established", "session"}));
        EXPECT_EQ(run.installs["write12"], of(server.client_keys));
        EXPECT_EQ(run.installs["read12"], of(server.server_keys));
        // the ticket offered with a fresh random session id beside it (RFC 5077 §3.4)
        EXPECT_EQ(server.offered_ticket, ticket);
        EXPECT_EQ(server.client_session_id.size(), 32u);
        EXPECT_NE(server.client_session_id, bytes_t(session->session_id, session->session_id + session->session_id_size));
        // the peer's chain is the session's
        ASSERT_EQ(run.client.result().peer_certificates.size(), peer.size());
        EXPECT_TRUE(run.client.result().peer_certificates[0].raw() == peer[0].raw());
        // offered once: the session given is zeroed
        EXPECT_FALSE(session->fresh(sgcl::time::now().unix_milli()));
        // the session kept again: the same ticket, its time and lifetime
        ASSERT_TRUE(run.kept);
        EXPECT_EQ(raw(run.kept->ticket), ticket);
        EXPECT_EQ(run.kept->received_ms, session->received_ms);
        EXPECT_EQ(run.kept->lifetime, session->lifetime);
        EXPECT_EQ(run.kept->session_id_size, session->session_id_size);
        EXPECT_EQ(run.kept->group, 0x0018u);
        EXPECT_EQ(bytes_t(run.kept->psk->bytes, run.kept->psk->bytes + run.kept->psk->size), of(server.master));
        EXPECT_EQ(run.kept->peer_certificates.size(), peer.size());
        // and resumed again from it
        Server12 third;
        third.store = &store;
        third.tickets = true;
        third.cipher = cipher;
        third.leaf = leaf;
        tls::ClientSettings s3 = resuming();
        s3.session = run.kept;
        Driven again(s3);
        ASSERT_TRUE(again.handshake(third)) << int(again.alert()) << " " << third.error;
        EXPECT_TRUE(again.client.result().resumed);
    }
}

TEST(Tls12Resume, BySessionId) {
    Store store;
    auto session = first_session(store, false);
    ASSERT_TRUE(session);
    const bytes_t id(session->session_id, session->session_id + session->session_id_size);
    for (bool tls13 : {true, false}) {
        SCOPED_TRACE(tls13);
        Server12 server;
        server.store = &store;
        tls::ClientSettings s = resuming(false);
        s.tls13 = tls13;
        sgcl::tracked_ptr<tls::Session> copy = sgcl::make_tracked<tls::Session>();
        copy->version = tls::Tls12;
        copy->cipher = session->cipher;
        copy->psk = std::make_unique<tls::Secret>();
        std::memcpy(copy->psk->bytes, session->psk->bytes, sizeof copy->psk->bytes);
        copy->psk->size = session->psk->size;
        copy->received_ms = session->received_ms;
        copy->lifetime = session->lifetime;
        std::memcpy(copy->session_id, session->session_id, 32);
        copy->session_id_size = 32;
        copy->peer_certificates = session->peer_certificates;
        s.session = copy;
        Driven run(s);
        ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_TRUE(run.client.result().resumed);
        EXPECT_EQ(server.client_session_id, id);   // the session id sent, also as 1.3's legacy_session_id
        EXPECT_FALSE(server.ticket_offered);
        ASSERT_TRUE(run.kept);
        EXPECT_TRUE(run.kept->ticket.empty());
        EXPECT_EQ(bytes_t(run.kept->session_id, run.kept->session_id + run.kept->session_id_size), id);
        EXPECT_EQ(run.kept->received_ms, session->received_ms);
    }
    // a session of a ticket and a session id, tickets not asked for: by its session id
    Store both;
    auto ticketed = first_session(both, true);
    ASSERT_TRUE(ticketed && !ticketed->ticket.empty());
    Server12 server;
    server.store = &both;
    tls::ClientSettings s = resuming(false);
    s.session = ticketed;
    Driven run(s);
    ASSERT_TRUE(run.handshake(server)) << int(run.alert());
    EXPECT_TRUE(run.client.result().resumed);
    EXPECT_FALSE(server.ticket_offered);
    // kept with its ticket still
    ASSERT_TRUE(run.kept);
    EXPECT_FALSE(run.kept->ticket.empty());
}

TEST(Tls12Resume, ARenewedTicket) {
    Store store;
    auto session = first_session(store, true);
    ASSERT_TRUE(session);
    const bytes_t old_ticket = raw(session->ticket);
    Server12 server;
    server.store = &store;
    server.tickets = true;
    server.renew = true;
    server.ticket_hint = 1800;
    tls::ClientSettings s = resuming();
    s.session = session;
    Driven run(s);
    ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
    EXPECT_TRUE(run.client.result().resumed);
    ASSERT_TRUE(run.kept);
    EXPECT_EQ(raw(run.kept->ticket), server.issued_ticket);
    EXPECT_NE(raw(run.kept->ticket), old_ticket);
    EXPECT_EQ(run.kept->lifetime, 1800u);
    EXPECT_GE(run.kept->received_ms, session->received_ms);
    // renewed with an empty ticket: the old one kept
    Store store2;
    auto session2 = first_session(store2, true);
    Server12 empty;
    empty.store = &store2;
    empty.tickets = true;
    empty.renew = true;
    empty.empty_ticket = true;
    tls::ClientSettings s2 = resuming();
    s2.session = session2;
    Driven run2(s2);
    ASSERT_TRUE(run2.handshake(empty)) << int(run2.alert()) << " " << empty.error;
    ASSERT_TRUE(run2.kept);
    EXPECT_EQ(run2.kept->ticket, session2->ticket);
}

TEST(Tls12Resume, DeclinedIsAFullHandshake) {
    for (bool tickets : {true, false}) {
        SCOPED_TRACE(tickets);
        Store store;
        auto session = first_session(store, tickets);
        ASSERT_TRUE(session);
        Server12 server;
        server.store = &store;
        server.tickets = tickets;
        server.decline = true;
        tls::ClientSettings s = resuming(tickets);
        s.session = session;
        Driven run(s);
        ASSERT_TRUE(run.handshake(server)) << int(run.alert()) << " " << server.error;
        EXPECT_FALSE(server.resumed);
        EXPECT_FALSE(run.client.result().resumed);
        EXPECT_EQ(run.client.result().peer_certificates.size(), 1u);
        // the new session in its place
        ASSERT_TRUE(run.kept);
        EXPECT_EQ(bytes_t(run.kept->session_id, run.kept->session_id + run.kept->session_id_size), server.session_id);
        EXPECT_NE(bytes_t(run.kept->session_id, run.kept->session_id + 32), bytes_t(session->session_id, session->session_id + 32));
        EXPECT_FALSE(session->fresh(sgcl::time::now().unix_milli()));   // the old one zeroed
    }
}

TEST(Tls12Resume, Refused) {
    // the session's suite not the ServerHello's: illegal_parameter
    {
        Store store;
        auto session = first_session(store, true);
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.resume_cipher = 0xC02C;   // offered, but not the session's
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        EXPECT_FALSE(run.handshake(server));
        EXPECT_EQ(run.alert(), AlertDescription::illegal_parameter);
    }
    // no extended master secret in the abbreviated handshake (RFC 7627 §5.3)
    {
        Store store;
        auto session = first_session(store, true);
        Server12 server;
        server.store = &store;
        server.tickets = true;
        server.ems = false;
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        EXPECT_FALSE(run.handshake(server));
        EXPECT_EQ(run.alert(), AlertDescription::handshake_failure);
    }
    // a Finished of the server that does not verify
    {
        Store store;
        auto session = first_session(store, true);
        Server12 server;
        server.store = &store;
        server.break_finished = true;
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        EXPECT_FALSE(run.handshake(server));
        EXPECT_EQ(run.alert(), AlertDescription::decrypt_error);
        EXPECT_EQ(run.sessions, 0u);
    }
    // the server's Finished before its change_cipher_spec
    {
        Store store;
        auto session = first_session(store, true);
        Server12 server;
        server.store = &store;
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        run.take(run.client.start());
        for (auto& m : server.hello(run.sent[0])) {
            run.take(run.client.feed(view(m)));
        }
        ASSERT_TRUE(server.resumed);
        run.take(run.client.feed(view(server.server_finished())));
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // a Certificate after a ServerHello that resumed
    {
        Store store;
        auto session = first_session(store, true);
        Server12 server;
        server.store = &store;
        tls::ClientSettings s = resuming();
        s.session = session;
        Driven run(s);
        run.take(run.client.start());
        auto msgs = server.hello(run.sent[0]);
        run.take(run.client.feed(view(msgs[0])));
        Server12 full;
        auto full_msgs = full.hello(run.sent[0]);
        run.take(run.client.feed(view(full_msgs[1])));
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
}

TEST(Tls12Resume, NotOffered) {
    auto offered = [](const tls::ClientSettings& s) {
        Driven run(s);
        run.take(run.client.start());
        auto ch = hello_of(run.sent.at(0));
        auto t = ch.extensions.find(tls::SessionTicket);
        return std::pair<bool, bytes_t>{t && !t->empty(), of(ch.session_id)};
    };
    Store store;
    // expired
    {
        auto session = first_session(store, true);
        session->received_ms -= int64_t(session->lifetime) * 1000 + 1;
        tls::ClientSettings s = resuming();
        s.session = session;
        auto [ticket, id] = offered(s);
        EXPECT_FALSE(ticket);
        EXPECT_NE(id, bytes_t(session->session_id, session->session_id + 32));
    }
    // a time before it came
    {
        auto session = first_session(store, true);
        session->received_ms += 3600 * 1000;
        tls::ClientSettings s = resuming();
        s.session = session;
        EXPECT_FALSE(offered(s).first);
    }
    // a suite no longer offered
    {
        auto session = first_session(store, true);
        tls::ClientSettings s = resuming();
        s.ciphers = {0x1301, 0xC02C};
        s.session = session;
        EXPECT_FALSE(offered(s).first);
    }
    // 1.2 not offered: neither the ticket nor the extension
    {
        auto session = first_session(store, true);
        tls::ClientSettings s = resuming();
        s.tls12 = false;
        s.session = session;
        Driven run(s);
        run.take(run.client.start());
        EXPECT_FALSE(hello_of(run.sent.at(0)).extensions.find(tls::SessionTicket).has_value());
        EXPECT_FALSE(session->fresh(sgcl::time::now().unix_milli()));   // taken all the same
    }
    // a 1.3 session where 1.3 is not offered
    {
        auto session = first_session(store, true);
        session->version = tls::Tls13;
        tls::ClientSettings s = resuming();
        s.tls13 = false;
        s.session = session;
        auto ch = offered(s);
        EXPECT_FALSE(ch.first);
    }
    // a master secret of the wrong size, a session of neither ticket nor id
    {
        auto session = first_session(store, true);
        session->psk->size = 32;
        tls::ClientSettings s = resuming();
        s.session = session;
        EXPECT_FALSE(offered(s).first);
        auto bare = first_session(store, false);
        bare->session_id_size = 0;
        tls::ClientSettings b = resuming(false);
        b.session = bare;
        EXPECT_NE(offered(b).second.size(), 0u);   // compatibility mode's random id: a full handshake
    }
}

TEST(Tls12Resume, TheNewSessionTicketOutOfPlace) {
    // not announced (no SessionTicket in the ServerHello): unexpected_message
    {
        Store store;
        Server12 server;
        server.store = &store;
        Driven run(resuming());
        run.take(run.client.start());
        for (auto& m : server.hello(run.sent[0])) {
            run.take(run.client.feed(view(m)));
        }
        server.flight(std::vector<bytes_t>(run.sent.begin() + 1, run.sent.end()));
        run.take(run.client.feed(view(server.new_session_ticket())));
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // announced, and the change_cipher_spec first: unexpected_message
    {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        Driven run(resuming());
        run.take(run.client.start());
        for (auto& m : server.hello(run.sent[0])) {
            run.take(run.client.feed(view(m)));
        }
        server.flight(std::vector<bytes_t>(run.sent.begin() + 1, run.sent.end()));
        run.take(run.client.change_cipher_spec());
        EXPECT_EQ(run.alert(), AlertDescription::unexpected_message);
    }
    // malformed: decode_error
    for (bytes_t body : {bytes_t{0, 0, 1}, bytes_t{0, 0, 0, 1, 0, 2, 7}, bytes_t{0, 0, 0, 1, 0, 1, 7, 9}}) {
        Store store;
        Server12 server;
        server.store = &store;
        server.tickets = true;
        Driven run(resuming());
        run.take(run.client.start());
        for (auto& m : server.hello(run.sent[0])) {
            run.take(run.client.feed(view(m)));
        }
        server.flight(std::vector<bytes_t>(run.sent.begin() + 1, run.sent.end()));
        bytes_t m = {4, 0, 0, uint8_t(body.size())};
        m.insert(m.end(), body.begin(), body.end());
        run.take(run.client.feed(view(m)));
        EXPECT_EQ(run.alert(), AlertDescription::decode_error);
    }
    // a SessionTicket in the ServerHello with a body: decode_error; one not asked for: unsupported_extension
    {
        Server12 unasked;
        unasked.extra_extension = tls::SessionTicket;
        Driven r(settings12());
        EXPECT_FALSE(r.handshake(unasked));
        EXPECT_EQ(r.alert(), AlertDescription::unsupported_extension);
    }
    {
        Driven run(resuming());
        run.take(run.client.start());
        Server12 server;
        auto msgs = server.hello(run.sent[0]);
        bytes_t sh = msgs[0];
        // the ServerHello with SessionTicket {1}: appended to its extensions
        auto h = tls::read_server_hello(tls::read_handshake(view(sh))->body).value();
        std::vector<sgcl::byte> buf;
        tls::Builder w(buf);
        tls::write_server_hello(w, h.random, h.session_id, h.cipher_suite, [&](tls::Builder& w) {
            for (auto e : h.extensions) {
                auto x = w.extension(e.type);
                w.bytes(e.body);
            }
            auto x = w.extension(tls::SessionTicket);
            w.u8(1);
        });
        // write_server_hello writes 1.3's legacy_version 0x0303: the same as 1.2's
        run.take(run.client.feed(view(of(buf))));
        EXPECT_EQ(run.alert(), AlertDescription::decode_error);
    }
}
