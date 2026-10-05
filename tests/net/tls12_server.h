//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A TLS 1.2 server written from RFC 5246, RFC 7627, RFC 8422 and RFC 5077
// for the tests of the client's 1.2 (tls12_client.cpp) and the seeds of its
// fuzzing harness (tls_fuzz_seeds.cpp): at the level of handshake messages,
// the identities of tls_server_identities.h, sessions resumed by ticket or
// session id from a Store the test keeps (the abbreviated handshake of
// RFC 5246 §7.3), knobs for what it does wrong. Not a server of the library
// (the library's server speaks 1.3 alone).
#pragma once

#include "sgcl/net/tls/detail/handshake.h"
#include "tls_server_identities.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace tls12_server {
    namespace tls = sgcl::net::tls::detail;
    using bytes_t = std::vector<uint8_t>;

    inline bytes_t unhex(const char* s) {
        bytes_t v;
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3] = {s[i], s[i + 1], 0};
            v.push_back(uint8_t(std::strtoul(b, nullptr, 16)));
        }
        return v;
    }

    inline sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    inline bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    inline bytes_t of(const std::vector<sgcl::byte>& v) {
        auto p = reinterpret_cast<const uint8_t*>(v.data());
        return bytes_t(p, p + v.size());
    }

    template<class C>
    bytes_t raw(const C& c) {
        auto p = reinterpret_cast<const uint8_t*>(c.data());
        return bytes_t(p, p + c.size());
    }

    // The keys of the identities (tls_server_identities.h)
    struct Keys {
        sgcl::crypto::ed25519::private_key ed25519;
        sgcl::crypto::p256::private_key p256;
        sgcl::crypto::p384::private_key p384;
        sgcl::crypto::rsa::private_key rsa;

        static const tls_identities::Identity& find(const char* name) {
            for (auto& i : tls_identities::all) {
                if (std::string(i.name) == name) {
                    return i;
                }
            }
            throw std::logic_error("no such identity");
        }

        static std::vector<std::vector<sgcl::byte>> chain(const char* name) {
            bytes_t der = unhex(find(name).certificate);
            auto p = reinterpret_cast<const sgcl::byte*>(der.data());
            return {std::vector<sgcl::byte>(p, p + der.size())};
        }

        Keys()
        : ed25519(sgcl::crypto::ed25519::private_key::from_pkcs8_der(view(unhex(find("ed25519").key))).value())
        , p256(sgcl::crypto::p256::private_key::from_pkcs8_der(view(unhex(find("p256").key))).value())
        , p384(sgcl::crypto::p384::private_key::from_pkcs8_der(view(unhex(find("p384").key))).value())
        , rsa(sgcl::crypto::rsa::private_key::from_pkcs8_der(view(unhex(find("rsa").key))).value()) {
        }

        tls::ServerIdentity identity(const std::string& name) const {
            if (name == "ed25519") return tls::identity_of(chain("ed25519"), ed25519);
            if (name == "p256") return tls::identity_of(chain("p256"), p256);
            if (name == "p384") return tls::identity_of(chain("p384"), p384);
            return tls::identity_of(chain("rsa"), rsa);
        }
    };

    inline const Keys& keys() {
        static const Keys k;
        return k;
    }

    // The sessions a server of these tests resumes: the master secret and
    // the suite, by ticket and by session id
    struct Saved {
        bytes_t master;
        uint16_t cipher = 0;
    };

    struct Store {
        std::map<bytes_t, Saved> tickets, ids;
        uint8_t next = 1;   // what makes each ticket and session id its own

        bytes_t fresh(size_t n, uint8_t tag) {
            bytes_t v(n);
            for (size_t i = 0; i < n; ++i) {
                v[i] = uint8_t(tag ^ (next * 7 + i));
            }
            ++next;
            return v;
        }
    };

    // A TLS 1.2 server from the RFCs, at the level of messages, with knobs
    // for what it does wrong
    struct Server12 {
        // what it chooses and sends
        uint16_t cipher = 0xC02B;
        uint16_t group = 0x001D;
        uint16_t scheme = 0x0403;
        uint16_t declared_scheme = 0;   // the scheme written in the ServerKeyExchange, when not the one signed with
        std::string leaf = "p256";
        bool ems = true;
        bool reneg = true;
        bytes_t reneg_body = {0};
        bool point_formats = true;
        bytes_t formats = {0};
        bool downgrade = false;
        bool echo_session = false;
        uint16_t extra_extension = 0;
        std::string alpn;
        uint16_t version = 0x0303;
        bool break_signature = false;
        bool break_finished = false;
        bool request_certificate = false;
        bytes_t cert_types = {1, 64};
        std::vector<uint16_t> request_schemes = {0x0403, 0x0804, 0x0401, 0x0807};
        // sessions (RFC 5077, RFC 5246 §7.3), with a store
        Store* store = nullptr;             // the sessions kept and resumed; none: no session id of its own
        bool tickets = false;               // a ClientHello's SessionTicket answered: a NewSessionTicket issued
        uint32_t ticket_hint = 7200;
        bool empty_ticket = false;          // the NewSessionTicket's ticket empty
        bool big_ticket = false;            // a ticket past the client's MaxTicket
        bool session_ids = true;            // a session id of its own sent and kept (with a store)
        bool decline = false;               // no session resumed
        uint16_t resume_cipher = 0;         // a resumed ServerHello's suite, when not the session's
        bool renew = false;                 // a NewSessionTicket in the abbreviated handshake

        // what it learned
        bool resumed = false;                 // the handshake is an abbreviated one
        bool ticket_offered = false;          // the ClientHello's SessionTicket extension
        bytes_t offered_ticket, client_session_id, session_id, issued_ticket;
        std::vector<bytes_t> before_ccs;      // a full handshake's NewSessionTicket, sent before the change_cipher_spec
        bytes_t client_random, server_random, messages;
        tls::ClientShares share;
        tls::Secret master, client_keys, server_keys;
        bool client_certificate = false, client_verify_ok = false;
        std::string error;

        tls::Hash hash() const {
            return tls::hash_of(tls::Cipher(cipher));
        }

        void add(const bytes_t& m) {
            messages.insert(messages.end(), m.begin(), m.end());
        }

        bytes_t transcript_hash() const {
            tls::Transcript t(hash());
            t.update(view(messages));
            bytes_t h(t.size());
            t.value_to(h.data());
            return h;
        }

        // The ClientHello: ServerHello, Certificate, ServerKeyExchange,
        // CertificateRequest when asked, ServerHelloDone
        std::vector<bytes_t> hello(const bytes_t& ch) {
            add(ch);
            auto h = tls::read_client_hello(tls::read_handshake(view(ch))->body);
            client_random = of(h->random);
            client_session_id = of(h->session_id);
            if (auto t = h->extensions.find(tls::SessionTicket)) {
                ticket_offered = true;
                offered_ticket = of(*t);
            }
            server_random = bytes_t(32);
            for (size_t i = 0; i < 32; ++i) {
                server_random[i] = uint8_t(0xA0 + i);
            }
            if (downgrade) {
                std::copy(std::begin(tls::Downgrade12), std::end(tls::Downgrade12), server_random.begin() + 24);
            }
            if (store && !decline) {
                const Saved* saved = nullptr;
                if (!offered_ticket.empty() && store->tickets.count(offered_ticket)) {
                    saved = &store->tickets[offered_ticket];
                } else if (!client_session_id.empty() && store->ids.count(client_session_id)) {
                    saved = &store->ids[client_session_id];
                }
                if (saved) {
                    return resume(*saved);
                }
            }
            if (echo_session) {
                session_id = client_session_id;
            } else if (store) {
                session_id = session_ids ? store->fresh(32, 0x5A) : bytes_t();   // none: resumed by its ticket alone
            } else {
                session_id = bytes_t(32, 0x55);
            }
            std::vector<bytes_t> out;
            std::vector<sgcl::byte> buf;
            tls::Builder w(buf);
            {
                auto m = w.message(tls::HandshakeType::server_hello);
                w.u16(version);
                w.bytes(server_random.data(), 32);
                {
                    auto sid = w.block8();
                    w.bytes(session_id.data(), session_id.size());
                }
                w.u16(cipher);
                w.u8(0);
                auto x = w.block16();
                if (reneg) {
                    auto e = w.extension(uint16_t(0xFF01));
                    w.bytes(reneg_body.data(), reneg_body.size());
                }
                if (ems) {
                    auto e = w.extension(tls::ExtensionType::extended_master_secret);
                }
                if (point_formats) {
                    auto e = w.extension(tls::ExtensionType::ec_point_formats);
                    auto l = w.block8();
                    w.bytes(formats.data(), formats.size());
                }
                if (!alpn.empty()) {
                    auto e = w.extension(tls::ExtensionType::application_layer_protocol_negotiation);
                    auto l = w.block16();
                    auto one = w.block8();
                    w.bytes(alpn.data(), alpn.size());
                }
                if (tickets && ticket_offered) {
                    auto e = w.extension(tls::SessionTicket);
                }
                if (extra_extension) {
                    auto e = w.extension(extra_extension);
                }
            }
            out.push_back(of(buf));
            buf.clear();
            tls::ServerIdentity id = keys().identity(leaf);
            {
                std::vector<tls::Bytes> ders;   // lint-handles: ok views of unmanaged bytes (bytes_of): no owner, no word
                for (auto& d : id.chain) {
                    ders.push_back(tls::bytes_of(d.data(), d.size()));
                }
                tls::write_certificate12(w, ders);
            }
            out.push_back(of(buf));
            buf.clear();
            if (tls::Group(group) != tls::Group::x25519_mlkem768) {
                share.add(tls::Group(group), tls::Entropy());
            }
            {
                bytes_t params = {3, uint8_t(group >> 8), uint8_t(group)};
                bytes_t point = share.size() ? of(share.public_share(0)) : bytes_t(32, 1);
                params.push_back(uint8_t(point.size()));
                params.insert(params.end(), point.begin(), point.end());
                bytes_t signed_part = client_random;
                signed_part.insert(signed_part.end(), server_random.begin(), server_random.end());
                signed_part.insert(signed_part.end(), params.begin(), params.end());
                auto m = w.message(tls::HandshakeType::server_key_exchange);
                w.bytes(params.data(), params.size());
                w.u16(declared_scheme ? declared_scheme : scheme);
                auto sig = w.block16();
                const size_t at = buf.size();
                tls::Builder sw(buf);
                if ((leaf == "p256" && scheme == 0x0503) || (leaf == "p384" && scheme == 0x0403)) {
                    // ECDSA with the hash of the other curve's scheme: 1.2 allows it
                    auto digest = scheme == 0x0503 ? raw(sgcl::crypto::sha384::of(view(signed_part))) : raw(sgcl::crypto::sha256::of(view(signed_part)));
                    auto sig = leaf == "p256" ? raw(keys().p256.sign_digest(view(digest))) : raw(keys().p384.sign_digest(view(digest)));
                    sw.bytes(sig.data(), sig.size());
                } else {
                    tls::ServerIdentity signer = keys().identity(leaf);
                    signer.sign(signer.key, scheme, view(signed_part), sw);
                }
                if (break_signature) {
                    buf[at + 7] ^= sgcl::byte(1);
                }
            }
            out.push_back(of(buf));
            buf.clear();
            if (request_certificate) {
                auto m = w.message(tls::HandshakeType::certificate_request);
                {
                    auto t = w.block8();
                    w.bytes(cert_types.data(), cert_types.size());
                }
                tls::write_signature_schemes(w, request_schemes);
                auto a = w.block16();
            }
            if (!buf.empty()) {
                out.push_back(of(buf));
                buf.clear();
            }
            {
                auto m = w.message(tls::HandshakeType::server_hello_done);
            }
            out.push_back(of(buf));
            for (auto& m : out) {
                add(m);
            }
            return out;
        }

        // The client's flight ([Certificate], ClientKeyExchange,
        // [CertificateVerify], Finished): the keys, the client's Finished and
        // CertificateVerify checked; the server's Finished
        bytes_t flight(const std::vector<bytes_t>& msgs) {
            sgcl::optional<sgcl::crypto::x509::certificate> client_leaf;
            for (auto& m : msgs) {
                auto h = tls::read_handshake(view(m));
                switch (tls::HandshakeType(h->type)) {
                case tls::HandshakeType::certificate: {
                    auto c = tls::read_certificate12(h->body);
                    if (c->count) {
                        client_certificate = true;
                        c->each([&](const tls::Bytes& der) {
                            if (!client_leaf) {
                                client_leaf = sgcl::crypto::x509::certificate::parse(der).value();
                            }
                        });
                    }
                    add(m);
                    break;
                }
                case tls::HandshakeType::client_key_exchange: {
                    tls::Reader r(h->body);
                    tls::Bytes point;
                    r.vec8(point, 1, 255);
                    auto pre = share.shared(tls::Group(group), point);
                    if (!pre) {
                        error = "the client's point";
                        return {};
                    }
                    add(m);
                    bytes_t session_hash = transcript_hash();
                    tls::extended_master_secret(hash(), master, pre->view(), view(session_hash));
                    tls::key_block12(tls::Cipher(cipher), client_keys, server_keys, master, view(client_random), view(server_random));
                    break;
                }
                case tls::HandshakeType::certificate_verify: {
                    auto cv = tls::read_certificate_verify(h->body);
                    client_verify_ok = client_leaf && tls::verify12(cv->scheme, client_leaf->public_key(), view(messages), cv->signature).has_value();
                    add(m);
                    break;
                }
                case tls::HandshakeType::finished: {
                    bytes_t th = transcript_hash();
                    bytes_t expected(12);
                    tls::finished12(hash(), expected.data(), master, true, view(th));
                    if (of(h->body) != expected) {
                        error = "the client's Finished";
                        return {};
                    }
                    add(m);
                    break;
                }
                default:
                    error = "a message of the client out of order";
                    return {};
                }
            }
            if (tickets && ticket_offered) {
                before_ccs.push_back(new_session_ticket());
                add(before_ccs.back());
            }
            if (store) {
                Saved saved{bytes_t(master.bytes, master.bytes + master.size), cipher};
                if (!session_id.empty()) {
                    store->ids[session_id] = saved;
                }
                if (!issued_ticket.empty()) {
                    store->tickets[issued_ticket] = saved;
                }
            }
            return server_finished();
        }

        // The server's Finished over the messages so far (after its
        // change_cipher_spec)
        bytes_t server_finished() {
            bytes_t th = transcript_hash();
            bytes_t mine(12);
            tls::finished12(hash(), mine.data(), master, false, view(th));
            if (break_finished) {
                mine[0] ^= 1;
            }
            std::vector<sgcl::byte> buf;
            tls::Builder w(buf);
            tls::write_finished(w, view(mine));
            bytes_t m = of(buf);
            if (resumed) {
                add(m);   // the client's Finished follows it (§7.3)
            }
            return m;
        }

        // The client's Finished of an abbreviated handshake, checked
        bool client_finished(const bytes_t& m) {
            auto h = tls::read_handshake(view(m));
            if (!h || tls::HandshakeType(h->type) != tls::HandshakeType::finished) {
                error = "no Finished of the client";
                return false;
            }
            bytes_t th = transcript_hash();
            bytes_t expected(12);
            tls::finished12(hash(), expected.data(), master, true, view(th));
            if (of(h->body) != expected) {
                error = "the client's Finished";
                return false;
            }
            add(m);
            return true;
        }

        // A NewSessionTicket of a ticket of the store's (RFC 5077 §3.3)
        bytes_t new_session_ticket() {
            issued_ticket = empty_ticket ? bytes_t() : store ? store->fresh(big_ticket ? tls::MaxTicket + 1 : 48, 0x7C) : bytes_t(48, 0x7C);
            std::vector<sgcl::byte> buf;
            tls::Builder w(buf);
            tls::write_new_session_ticket12(w, ticket_hint, view(issued_ticket));
            return of(buf);
        }

        // The abbreviated handshake (§7.3): ServerHello echoing the client's
        // session id, a NewSessionTicket when renewing, the keys of the
        // session's master secret; the change_cipher_spec and Finished follow
        // (server_finished)
        std::vector<bytes_t> resume(const Saved& saved) {
            resumed = true;
            const uint16_t session_cipher = saved.cipher;
            cipher = resume_cipher ? resume_cipher : session_cipher;
            std::copy(saved.master.begin(), saved.master.end(), master.bytes);
            master.size = uint8_t(saved.master.size());
            session_id = client_session_id;
            std::vector<bytes_t> out;
            std::vector<sgcl::byte> buf;
            tls::Builder w(buf);
            {
                auto m = w.message(tls::HandshakeType::server_hello);
                w.u16(version);
                w.bytes(server_random.data(), 32);
                {
                    auto sid = w.block8();
                    w.bytes(client_session_id.data(), client_session_id.size());
                }
                w.u16(cipher);
                w.u8(0);
                auto x = w.block16();
                if (reneg) {
                    auto e = w.extension(uint16_t(0xFF01));
                    w.bytes(reneg_body.data(), reneg_body.size());
                }
                if (ems) {
                    auto e = w.extension(tls::ExtensionType::extended_master_secret);
                }
                if (!alpn.empty()) {
                    auto e = w.extension(tls::ExtensionType::application_layer_protocol_negotiation);
                    auto l = w.block16();
                    auto one = w.block8();
                    w.bytes(alpn.data(), alpn.size());
                }
                if (renew && ticket_offered) {
                    auto e = w.extension(tls::SessionTicket);
                }
                if (extra_extension) {
                    auto e = w.extension(extra_extension);
                }
            }
            out.push_back(of(buf));
            if (renew && ticket_offered) {
                out.push_back(new_session_ticket());
                if (store && !issued_ticket.empty()) {
                    store->tickets[issued_ticket] = saved;
                }
            }
            for (auto& m : out) {
                add(m);
            }
            tls::key_block12(tls::Cipher(cipher), client_keys, server_keys, master, view(client_random), view(server_random));
            return out;
        }
    };

}
