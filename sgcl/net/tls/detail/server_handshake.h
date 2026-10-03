//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "handshake.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The server's side of the TLS 1.3 handshake (RFC 8446 §2, §4) in the shape
// of the client's (handshake.h): no input or output of its own, feed() takes
// each whole handshake message the assembler gives and answers with a Step,
// the actions the connection carries out in their order.
//
// What it chooses, from the first ClientHello:
//   - the cipher suite: the first of the server's list the client offers;
//   - the group: the first of the server's list the client supports and has
//     sent a share of; else the first of the server's list the client
//     supports, asked for with a HelloRetryRequest (one at most, §4.1.4);
//     none in common is handshake_failure;
//   - the identity: the first whose certificate is for the server name the
//     client sent (SNI), else the first; of it, the first signature scheme
//     of its key the client offers (signature_algorithms); none is
//     handshake_failure;
//   - the protocol (RFC 7301): the first of the server's list the client
//     offers; the client offering some and none of them in common is
//     no_application_protocol; a server with no list answers none.
// v1: no PSK, no 0-RTT (early data refused: the client's is skipped,
// skip_early_data), no CertificateRequest (a client Certificate is
// unexpected_message), no NewSessionTicket sent. A client that sent a
// session id is answered in compatibility mode: the id echoed, a
// change_cipher_spec after the ServerHello or the HelloRetryRequest,
// whichever goes first (§D.4).
//
// The secrets (the key schedule, the transcript, the random, the cookie,
// the secrets the step carries) are in an unmanaged block of the machine's
// own, zeroed stage by stage and when it goes; the identities' private keys
// are not the machine's (it signs through each identity's signer). The
// settings and the result hold managed values (strings, certificates):
// the machine lives in a frame or in a managed object.
namespace sgcl::net::tls::detail {
    // The server's certificate chain and a way to sign with its key: the
    // key itself is not here (tls::identity keeps it, unmanaged, T6)
    struct ServerIdentity {
        std::vector<std::vector<byte>> chain;           // the DER of each certificate, the leaf first
        optional<crypto::x509::certificate> leaf;        // chain[0] read: the names SNI is matched against
        std::vector<uint16_t> schemes;                   // the schemes the key signs, the server's preference first
        void (*sign)(const void* key, uint16_t scheme, const Bytes& content, Builder& out) = nullptr;
        const void* key = nullptr;
    };

    // An identity of a certificate chain (DER, the leaf first) and a key of
    // the module; the key must outlive the identity
    template<class K>
    ServerIdentity identity_of(const std::vector<std::vector<byte>>& chain, const K& key) noexcept {
        ServerIdentity id;
        id.chain = chain;
        if (!chain.empty()) {
            auto leaf = crypto::x509::certificate::parse(bytes_of(chain[0].data(), chain[0].size()));
            if (leaf) {
                id.leaf = std::move(*leaf);
            }
        }
        if constexpr (std::is_same_v<K, crypto::ed25519::private_key>) {
            id.schemes = {uint16_t(SignatureScheme::ed25519)};
        } else if constexpr (std::is_same_v<K, crypto::p256::private_key>) {
            id.schemes = {uint16_t(SignatureScheme::ecdsa_secp256r1_sha256)};
        } else if constexpr (std::is_same_v<K, crypto::p384::private_key>) {
            id.schemes = {uint16_t(SignatureScheme::ecdsa_secp384r1_sha384)};
        } else {
            static_assert(std::is_same_v<K, crypto::rsa::private_key>, "a key of the module: ed25519, p256, p384 or rsa");
            // the schemes whose digest and salt the key's encoding holds
            // (a key of 1024 bits has no room for SHA-512's): one it does
            // not is never chosen, so a client that offers it alone gets
            // handshake_failure, never a signature that cannot be made
            static constexpr SignatureScheme pss[] = {SignatureScheme::rsa_pss_rsae_sha256, SignatureScheme::rsa_pss_rsae_sha384, SignatureScheme::rsa_pss_rsae_sha512};
            static constexpr size_t digest[] = {32, 48, 64};
            for (size_t i = 0; i < 3; ++i) {
                if (rsa_pss_fits(key.bits(), digest[i])) {
                    id.schemes.push_back(uint16_t(pss[i]));
                }
            }
        }
        id.key = &key;
        id.sign = [](const void* k, uint16_t scheme, const Bytes& content, Builder& out) {
            sign(out, scheme, *static_cast<const K*>(k), content);
        };
        return id;
    }

    // What the server offers and accepts: the lists are the codes on the
    // wire, the server's preference first (tls::config makes them, T6)
    struct ServerSettings {
        vector<ServerIdentity> identities;
        vector<uint16_t> ciphers = {0x1301, 0x1302, 0x1303};
        vector<uint16_t> groups = {0x11EC, 0x001D, 0x0017, 0x0018};
        vector<string> alpn;
        uint16_t record_size_limit = 0;                  // RFC 8449, answered when the client sent one; 0: not
        vector<uint16_t> advertised_groups;              // supported_groups in EncryptedExtensions (§4.2.7); empty: not sent
        std::vector<byte> retry_cookie;                  // a cookie in the HelloRetryRequest (§4.2.2), for the tests of RFC 8448 §5
        size_t early_data_limit = 1 << 18;               // the bytes of refused early data skipped at most (§4.2.10)
    };

    // What the handshake settled
    struct ServerResult {
        Cipher cipher = Cipher::aes_128_gcm_sha256;
        Group group = Group::x25519;
        string alpn;                                     // empty: none
        string server_name;                              // the client's SNI, empty: none
        size_t identity = 0;                             // the index of the identity chosen
        uint16_t scheme = 0;                             // CertificateVerify's
        bool retried = false;                            // a HelloRetryRequest went
        bool early_data_refused = false;                 // the client offered 0-RTT
    };

    class ServerHandshake {
    public:
        ServerHandshake(const ServerSettings& settings, const Entropy& entropy = Entropy()) noexcept
        : _settings(settings), _entropy(entropy), _s(std::make_unique<Secrets>()) {
        }

        ServerHandshake(const ServerHandshake&) = delete;
        ServerHandshake& operator=(const ServerHandshake&) = delete;

        // A whole handshake message from the client (header included); not
        // noexcept: an RSA signature that does not verify under its own
        // public key (a fault in the computation) is crypto's
        // std::runtime_error (sign, signature.h)
        const Step& feed(const Bytes& message) {
            _s->step.clear();
            if (_state == State::failed) {
                return _s->step;
            }
            auto r = _feed(message);
            if (!r) {
                return _fail(r.error());
            }
            return _s->step;
        }

        // An alert record from the client: the handshake is over
        void on_record_alert(const Alert& a) noexcept {
            _peer_alert = a;
            _state = State::failed;
            _wipe();
        }

        bool established() const noexcept {
            return _state == State::connected;
        }

        bool failed() const noexcept {
            return _state == State::failed;
        }

        const ServerResult& result() const noexcept {
            return _result;
        }

        const optional<Alert>& peer_alert() const noexcept {
            return _peer_alert;
        }

    private:
        enum class State : uint8_t {
            wait_client_hello,
            wait_second_hello,
            wait_finished,
            connected,
            failed,
        };

        struct Secrets {
            optional<Transcript> transcript;
            optional<KeySchedule> schedule;
            uint8_t random[32] = {};
            Step step;

            ~Secrets() {
                crypto::detail::secure_zero(random, sizeof random);
            }
        };

        ServerSettings _settings;
        Entropy _entropy;
        std::unique_ptr<Secrets> _s;
        State _state = State::wait_client_hello;
        Hash _hash = Hash::sha256;
        uint8_t _session_id[32] = {};
        size_t _session_id_size = 0;
        bool _ccs_sent = false;
        ServerResult _result;
        optional<Alert> _peer_alert;

        static Alert _alert(AlertDescription d, const char* what) noexcept {
            return Alert{d, 0, what};
        }

        expected<void, Alert> _feed(const Bytes& message) {
            auto h = read_handshake(message);
            if (!h) {
                return unexpected(h.error());
            }
            const HandshakeType type = HandshakeType(h->type);
            switch (_state) {
            case State::wait_client_hello:
                if (type != HandshakeType::client_hello) {
                    break;
                }
                return _client_hello(message, h->body, false);
            case State::wait_second_hello:
                if (type != HandshakeType::client_hello) {
                    break;
                }
                return _client_hello(message, h->body, true);
            case State::wait_finished:
                if (type != HandshakeType::finished) {
                    break;   // a Certificate: none was asked for (no mTLS in v1)
                }
                return _finished(message, h->body);
            case State::connected:
                return _after(type, h->body);
            case State::failed:
                break;
            }
            return unexpected(_alert(AlertDescription::unexpected_message, "a handshake message out of order"));
        }

        template<class R>
        static bool _has(const R& list, uint16_t v) noexcept {
            for (uint16_t x : list) {
                if (x == v) {
                    return true;
                }
            }
            return false;
        }

        // --- the ClientHello -------------------------------------------------------

        expected<void, Alert> _client_hello(const Bytes& message, const Bytes& body, bool second) {
            auto ch = read_client_hello(body);
            if (!ch) {
                return unexpected(ch.error());
            }
            const Extensions& x = ch->extensions;
            if (auto v = validate_extensions(HandshakeType::client_hello, false, x, 0); !v) {
                return unexpected(v.error());
            }
            // TLS 1.3 or nothing (§4.2.1): a client without supported_versions
            // speaks an older version
            auto versions_body = x.find(ExtensionType::supported_versions);
            if (!versions_body) {
                return unexpected(_alert(AlertDescription::protocol_version, "a ClientHello of a version before TLS 1.3"));
            }
            auto versions = read_versions_offered(*versions_body);
            if (!versions) {
                return unexpected(versions.error());
            }
            if (!versions->contains(Tls13)) {
                return unexpected(_alert(AlertDescription::protocol_version, "a client without TLS 1.3"));
            }
            auto groups_body = x.find(ExtensionType::supported_groups);
            auto shares_body = x.find(ExtensionType::key_share);
            auto schemes_body = x.find(ExtensionType::signature_algorithms);
            if (!groups_body || !shares_body) {
                return unexpected(_alert(AlertDescription::missing_extension, "a ClientHello without supported_groups or key_share (no PSK in v1)"));
            }
            if (!schemes_body) {
                return unexpected(_alert(AlertDescription::missing_extension, "a ClientHello without signature_algorithms"));
            }
            auto groups = read_groups(*groups_body);
            if (!groups) {
                return unexpected(groups.error());
            }
            auto shares = read_key_shares(*shares_body);
            if (!shares) {
                return unexpected(shares.error());
            }
            auto schemes = read_signature_schemes(*schemes_body);
            if (!schemes) {
                return unexpected(schemes.error());
            }
            for (auto s : *shares) {   // §4.2.8: a share of a group not in supported_groups
                if (!groups->contains(s.group)) {
                    return unexpected(_alert(AlertDescription::illegal_parameter, "a key share of a group not in supported_groups"));
                }
            }
            if (second) {
                return _second_hello(message, *ch, *shares);
            }
            if (ch->session_id.size()) {
                std::memcpy(_session_id, ch->session_id.data(), ch->session_id.size());
                _session_id_size = ch->session_id.size();
            }
            // the cipher suite
            optional<uint16_t> cipher;
            for (uint16_t c : _settings.ciphers) {
                if (known(Cipher(c)) && ch->cipher_suites.contains(c)) {
                    cipher = c;
                    break;
                }
            }
            if (!cipher) {
                return unexpected(_alert(AlertDescription::handshake_failure, "no cipher suite in common"));
            }
            _result.cipher = Cipher(*cipher);
            _hash = hash_of(_result.cipher);
            // the group: one with a share first, else one to ask for
            optional<uint16_t> group;
            bool shared = false;
            for (uint16_t g : _settings.groups) {
                if (supported(Group(g)) && groups->contains(g) && shares->find(g)) {
                    group = g;
                    shared = true;
                    break;
                }
            }
            if (!group) {
                for (uint16_t g : _settings.groups) {
                    if (supported(Group(g)) && groups->contains(g)) {
                        group = g;
                        break;
                    }
                }
            }
            if (!group) {
                return unexpected(_alert(AlertDescription::handshake_failure, "no group in common"));
            }
            _result.group = Group(*group);
            // the identity and its scheme
            string sni;
            if (auto sn = x.find(ExtensionType::server_name)) {
                auto host = read_server_name(*sn);
                if (!host) {
                    return unexpected(host.error());
                }
                sni = string(std::string_view(reinterpret_cast<const char*>(host->data()), host->size()));
            }
            _result.server_name = sni;
            if (auto chosen = _choose_identity(sni, *schemes); !chosen) {
                return unexpected(chosen.error());
            }
            // the protocol
            if (auto p = x.find(ExtensionType::application_layer_protocol_negotiation)) {
                auto offered = read_protocols(*p);
                if (!offered) {
                    return unexpected(offered.error());
                }
                if (!_settings.alpn.empty()) {
                    bool found = false;
                    for (const string& mine : _settings.alpn) {
                        auto v = mine.view();
                        for (auto theirs : *offered) {
                            if (theirs.size() == v.size() && std::memcmp(theirs.data(), v.data(), v.size()) == 0) {
                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            _result.alpn = mine;
                            break;
                        }
                    }
                    if (!found) {
                        return unexpected(_alert(AlertDescription::no_application_protocol, "no protocol in common (RFC 7301)"));
                    }
                }
            }
            _client_record_size_limit = 0;
            if (auto l = x.find(ExtensionType::record_size_limit)) {
                auto v = read_record_size_limit(*l);
                if (!v) {
                    return unexpected(v.error());
                }
                _client_record_size_limit = *v;
            }
            // 0-RTT refused: the client's early data skipped (§4.2.10)
            if (x.has(ExtensionType::early_data)) {
                if (auto e = read_empty(*x.find(ExtensionType::early_data)); !e) {
                    return unexpected(e.error());
                }
                _result.early_data_refused = true;
                Action a{Action::Kind::skip_early_data};
                a.size = _settings.early_data_limit;
                _s->step.actions.push_back(std::move(a));
            }
            _s->transcript.emplace(_hash);
            _s->transcript->update(message);
            if (!shared) {
                return _retry();
            }
            return _server_flight(*shares->find(*group));
        }

        expected<void, Alert> _choose_identity(const string& sni, const U16List& offered) noexcept {
            if (_settings.identities.empty()) {
                return unexpected(_alert(AlertDescription::internal_error, "a server without an identity"));
            }
            auto scheme_of = [&](const ServerIdentity& id) noexcept -> uint16_t {
                for (uint16_t s : id.schemes) {
                    if (offered.contains(s)) {
                        return s;
                    }
                }
                return 0;
            };
            // the first identity for the name, of a scheme the client takes; then any of such a scheme
            for (int pass = 0; pass < 2; ++pass) {
                for (size_t i = 0; i < _settings.identities.size(); ++i) {
                    const ServerIdentity& id = _settings.identities[i];
                    if (pass == 0 && (sni.empty() || !id.leaf || !id.leaf->verify_hostname(sni))) {
                        continue;
                    }
                    if (uint16_t s = scheme_of(id)) {
                        _result.identity = i;
                        _result.scheme = s;
                        return {};
                    }
                }
            }
            return unexpected(_alert(AlertDescription::handshake_failure, "no certificate of a signature scheme the client takes"));
        }

        // A HelloRetryRequest for the group chosen (§4.1.4): the first
        // ClientHello becomes message_hash of it in the transcript (§4.4.1)
        expected<void, Alert> _retry() noexcept {
            uint8_t ch1[MaxHashSize];
            _s->transcript->value_to(ch1);
            Transcript fresh(_hash);
            std::vector<byte> m;
            Builder mh(m);
            write_message_hash(mh, bytes_of(ch1, hash_size(_hash)));
            fresh.update(bytes_of(m.data(), m.size()));
            *_s->transcript = std::move(fresh);
            auto& out = _s->step.out;
            size_t at = out.size();
            Builder w(out);
            write_server_hello(w, bytes_of(HelloRetryRandom, 32), bytes_of(_session_id, _session_id_size), uint16_t(_result.cipher), [&](Builder& w) noexcept {
                {
                    auto e = w.extension(ExtensionType::key_share);
                    write_key_share_retry(w, uint16_t(_result.group));
                }
                if (!_settings.retry_cookie.empty()) {
                    auto e = w.extension(ExtensionType::cookie);
                    write_cookie(w, bytes_of(_settings.retry_cookie.data(), _settings.retry_cookie.size()));
                }
                auto e = w.extension(ExtensionType::supported_versions);
                write_version_selected(w, Tls13);
            });
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            _push_send(Epoch::initial, at, out.size() - at);
            if (_session_id_size) {
                _push_ccs();
            }
            _result.retried = true;
            _state = State::wait_second_hello;
            return {};
        }

        // The second ClientHello: the share asked for, the suite still
        // offered, the cookie echoed, no early data (§4.1.2)
        expected<void, Alert> _second_hello(const Bytes& message, const ClientHello& ch, const KeyShareList& shares) {
            if (!ch.cipher_suites.contains(uint16_t(_result.cipher))) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "a second ClientHello without the suite chosen"));
            }
            if (ch.session_id.size() != _session_id_size || std::memcmp(ch.session_id.data(), _session_id, _session_id_size) != 0) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "a second ClientHello of another session id"));
            }
            if (ch.extensions.has(ExtensionType::early_data)) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "early data in a second ClientHello"));
            }
            auto share = shares.find(uint16_t(_result.group));
            size_t count = 0;
            for (auto s : shares) {
                (void)s;
                ++count;
            }
            if (!share || count != 1) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "a second ClientHello without the one share asked for"));
            }
            if (!_settings.retry_cookie.empty()) {
                auto c = ch.extensions.find(ExtensionType::cookie);
                auto cookie = c ? read_cookie(*c) : expected<Bytes, Alert>(unexpected(_alert(AlertDescription::illegal_parameter, "no cookie")));
                if (!cookie || cookie->size() != _settings.retry_cookie.size() || std::memcmp(cookie->data(), _settings.retry_cookie.data(), cookie->size()) != 0) {
                    return unexpected(_alert(AlertDescription::illegal_parameter, "a second ClientHello without the cookie sent"));
                }
            }
            _s->transcript->update(message);
            return _server_flight(*share);
        }

        // --- the server's flight ----------------------------------------------------

        expected<void, Alert> _server_flight(const Bytes& client_share) {
            _entropy(_s->random, 32);
            auto share = server_share(_result.group, client_share, _entropy);
            if (!share) {
                return unexpected(share.error());
            }
            const size_t n = hash_size(_hash);
            auto& out = _s->step.out;
            // ServerHello
            size_t at = out.size();
            Builder w(out);
            write_server_hello(w, bytes_of(_s->random, 32), bytes_of(_session_id, _session_id_size), uint16_t(_result.cipher), [&](Builder& w) noexcept {
                {
                    auto e = w.extension(ExtensionType::key_share);
                    write_key_share_selected(w, KeyShare{uint16_t(_result.group), bytes_of(share->public_share.data(), share->public_share.size())});
                }
                auto e = w.extension(ExtensionType::supported_versions);
                write_version_selected(w, Tls13);
            });
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            _push_send(Epoch::initial, at, out.size() - at);
            if (_session_id_size && !_ccs_sent) {
                _push_ccs();
            }
            uint8_t h[MaxHashSize];
            _s->transcript->value_to(h);
            _s->schedule.emplace(_hash);
            KeySchedule& k = *_s->schedule;
            k.handshake(share->secret.view(), bytes_of(h, n));
            _push_install(Action::Kind::install_write, Epoch::handshake, k.server_handshake_traffic);
            _push_install(Action::Kind::install_read, Epoch::handshake, k.client_handshake_traffic);
            // EncryptedExtensions, Certificate, CertificateVerify, Finished
            at = out.size();
            write_encrypted_extensions(w, [&](Builder& w) noexcept {
                if (!_settings.advertised_groups.empty()) {
                    auto e = w.extension(ExtensionType::supported_groups);
                    write_groups(w, _settings.advertised_groups);
                }
                if (_client_record_size_limit && _settings.record_size_limit) {
                    auto e = w.extension(ExtensionType::record_size_limit);
                    w.u16(_settings.record_size_limit);
                }
                if (!_result.server_name.empty()) {
                    auto e = w.extension(ExtensionType::server_name);   // empty: the name was used (RFC 6066 §3)
                }
                if (!_result.alpn.empty()) {
                    auto e = w.extension(ExtensionType::application_layer_protocol_negotiation);
                    auto list = w.block16();
                    auto v = _result.alpn.view();
                    auto one = w.block8();
                    w.bytes(v.data(), v.size());
                }
            });
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            const ServerIdentity& id = _settings.identities[_result.identity];
            at = out.size();
            {
                std::vector<Bytes> ders;   // lint-handles: ok views of unmanaged bytes (bytes_of): no owner, no word
                for (auto& d : id.chain) {
                    ders.push_back(bytes_of(d.data(), d.size()));
                }
                write_certificate(w, Bytes(), ders);
            }
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            _s->transcript->value_to(h);
            at = out.size();
            {
                auto content = certificate_verify_content(true, bytes_of(h, n));
                auto m = w.message(HandshakeType::certificate_verify);
                w.u16(_result.scheme);
                auto sig = w.block16();
                id.sign(id.key, _result.scheme, content.view(), w);
            }
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            _s->transcript->value_to(h);
            uint8_t mine[MaxHashSize];
            verify_data(_hash, mine, k.server_handshake_traffic, bytes_of(h, n));
            at = out.size();
            write_finished(w, bytes_of(mine, n));
            crypto::detail::secure_zero(mine, sizeof mine);
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            // everything after the ServerHello under the handshake keys, one send
            size_t flight = _s->step.actions.back().kind == Action::Kind::send ? 0 : 0;
            (void)flight;
            _push_send(Epoch::handshake, _flight_start(), out.size() - _flight_start());
            // the application secrets, from the transcript through the server's Finished
            _s->transcript->value_to(h);
            k.application(bytes_of(h, n));
            _push_install(Action::Kind::install_write, Epoch::application, k.server_application_traffic);
            _state = State::wait_finished;
            return {};
        }

        // Where the encrypted part of the flight begins in the step's
        // buffer: right after the last send (the ServerHello)
        size_t _flight_start() const noexcept {
            size_t end = 0;
            for (auto& a : _s->step.actions) {
                if (a.kind == Action::Kind::send) {
                    end = a.offset + a.size;
                }
            }
            return end;
        }

        // --- the client's Finished ------------------------------------------------------

        expected<void, Alert> _finished(const Bytes& message, const Bytes& body) noexcept {
            const size_t n = hash_size(_hash);
            auto f = read_finished(body, n);
            if (!f) {
                return unexpected(f.error());
            }
            uint8_t h[MaxHashSize], expected_data[MaxHashSize];
            _s->transcript->value_to(h);
            verify_data(_hash, expected_data, _s->schedule->client_handshake_traffic, bytes_of(h, n));
            const bool ok = crypto::constant_time::equal(bytes_of(expected_data, n), *f);
            crypto::detail::secure_zero(expected_data, sizeof expected_data);
            if (!ok) {
                return unexpected(_alert(AlertDescription::decrypt_error, "the client's Finished does not verify"));
            }
            _s->transcript->update(message);
            _push_install(Action::Kind::install_read, Epoch::application, _s->schedule->client_application_traffic);
            _s->step.actions.push_back(Action{Action::Kind::established});
            _wipe();
            _state = State::connected;
            return {};
        }

        // After the handshake: KeyUpdate answered (§4.6.3); nothing else a
        // client sends then is taken in v1
        expected<void, Alert> _after(HandshakeType type, const Bytes& body) noexcept {
            if (type != HandshakeType::key_update) {
                return unexpected(_alert(AlertDescription::unexpected_message, "a handshake message after the handshake that v1 does not take"));
            }
            auto requested = read_key_update(body);
            if (!requested) {
                return unexpected(requested.error());
            }
            _s->step.actions.push_back(Action{Action::Kind::update_read, Epoch::application});
            if (*requested) {
                auto& out = _s->step.out;
                size_t at = out.size();
                Builder w(out);
                write_key_update(w, false);
                _push_send(Epoch::application, at, out.size() - at);
                _s->step.actions.push_back(Action{Action::Kind::update_write, Epoch::application});
            }
            return {};
        }

        // --- helpers ----------------------------------------------------------------

        uint16_t _client_record_size_limit = 0;

        void _push_send(Epoch e, size_t offset, size_t size) noexcept {
            Action a{Action::Kind::send, e};
            a.offset = offset;
            a.size = size;
            _s->step.actions.push_back(std::move(a));
        }

        void _push_ccs() noexcept {
            _s->step.actions.push_back(Action{Action::Kind::change_cipher_spec, Epoch::initial});
            _ccs_sent = true;
        }

        void _push_install(Action::Kind kind, Epoch e, const Secret& s) noexcept {
            Action a{kind, e, _result.cipher};
            std::memcpy(a.secret.bytes, s.bytes, sizeof s.bytes);
            a.secret.size = s.size;
            _s->step.actions.push_back(std::move(a));
        }

        const Step& _fail(const Alert& a) noexcept {
            _s->step.clear();
            Action x{Action::Kind::alert};
            x.alert = a.description;
            x.what = a.what;
            _s->step.actions.push_back(std::move(x));
            _state = State::failed;
            _wipe();
            return _s->step;
        }

        void _wipe() noexcept {
            if (_s->schedule) {
                _s->schedule->finish_handshake();
                _s->schedule.reset();
            }
            _s->transcript.reset();
        }
    };
}
