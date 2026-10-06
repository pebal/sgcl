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
//   - a client certificate (§4.3.2, §4.4.2): asked for when the settings
//     say so (CertificateRequest with signature_algorithms and the
//     subjects of the client roots as certificate_authorities), its chain
//     verified against those roots for client authentication, its
//     CertificateVerify checked; none sent where one is required is
//     certificate_required;
//   - resumption (§2.2, §4.2.11, psk_dhe_ke only): the first identity of
//     the client's pre_shared_key that is a ticket of the server's keys,
//     opens, is of a suite of the hash chosen, within its lifetime, and
//     carries a client chain where one is required, is taken once its
//     binder verifies (a binder that does not is decrypt_error); anything
//     else is a full handshake. A resumed handshake sends no certificate
//     and asks for none. After the client's Finished, one NewSessionTicket
//     when tickets are on (§4.6.1).
// No 0-RTT: early data refused, the client's skipped (skip_early_data). A
// client that sent a session id is answered in compatibility mode: the id
// echoed, a change_cipher_spec after the ServerHello or the
// HelloRetryRequest, whichever goes first (§D.4).
//
// The secrets (the key schedule, the transcript, the random, the cookie,
// the secrets the step carries) are in an unmanaged block of the machine's
// own, zeroed stage by stage and when it goes; the identities' private keys
// are not the machine's (it signs through each identity's signer). The
// settings and the result hold managed values (strings, certificates):
// the machine lives in a frame or in a managed object.
namespace sgcl::net::tls::detail {
    // What the server offers and accepts: the lists are the codes on the
    // wire, the server's preference first (tls::config makes them, T6)
    // A key of ECH's (RFC 9849) as the server opens with it: the ECHConfig,
    // its id, KEM and suites, the HPKE private key (held by tls::ech_key's
    // state, which the connection keeps)
    struct EchServerKey {
        std::vector<byte> config;
        uint8_t id = 0;
        uint16_t kem = 0;
        std::vector<uint32_t> suites;     // kdf << 16 | aead
        const crypto::hpke::private_key* key = nullptr;
    };

    struct ServerSettings {
        vector<ServerIdentity> identities;
        vector<uint16_t> ciphers = {0x1301, 0x1302, 0x1303};
        vector<uint16_t> groups = {0x11EC, 0x001D, 0x0017, 0x0018};
        vector<string> alpn;
        uint16_t record_size_limit = 0;                  // RFC 8449, answered when the client sent one; 0: not
        vector<uint16_t> advertised_groups;              // supported_groups in EncryptedExtensions (§4.2.7); empty: not sent
        std::vector<byte> retry_cookie;                  // a cookie in the HelloRetryRequest (§4.2.2), for the tests of RFC 8448 §5
        size_t early_data_limit = 1 << 18;               // the bytes of refused early data skipped at most (§4.2.10)
        uint8_t client_auth = 0;                         // a client certificate: 0 not asked for, 1 asked for, 2 required
        optional<crypto::x509::certificate_pool> client_roots;   // what a client's chain must lead to; nullopt: the system's
        TicketKeys* tickets = nullptr;                   // tickets issued and taken; null: neither
        uint32_t ticket_lifetime = 86400;                // seconds, at most 604800
        std::vector<EchServerKey> ech_keys;              // RFC 9849: what a ClientHelloOuter is opened with; empty: no ECH
        std::vector<byte> ech_retry_configs;             // the ECHConfigList sent when ECH is rejected; empty: none
    };

    // The identities of a client's pre_shared_key tried at most (§4.2.11)
    inline constexpr size_t MaxPskIdentities = 5;

    // The schemes a CertificateRequest takes (§4.2.3): CertificateVerify's,
    // and PKCS #1 v1.5 for the certificates of the chain
    inline constexpr uint16_t RequestSchemes[] = {0x0403, 0x0804, 0x0503, 0x0805, 0x0806, 0x0807, 0x0401, 0x0501, 0x0601};

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
        bool resumed = false;                            // a session of a ticket taken
        crypto::x509::chain peer_certificates;           // the client's, the leaf first (mTLS; a resumed session's from its ticket)
        crypto::x509::chain verified_chain;              // the client's chain as verification built it, the leaf to a root
        bool staple_sent = false;                        // an OCSP response stapled to the leaf (the client asked, the identity had one)
        bool ech_accepted = false;                       // the ClientHelloInner was decrypted and answered (RFC 9849)
    };

    class ServerHandshake {
    public:
        SGCL_INLINE_HOT ServerHandshake(const ServerSettings& settings, const Entropy& entropy = Entropy(), const Clock& clock = Clock()) noexcept
        : _settings(settings), _entropy(entropy), _clock(clock), _s(std::make_unique<Secrets>()) {
        }

        ServerHandshake(const ServerHandshake&) = delete;
        ServerHandshake& operator=(const ServerHandshake&) = delete;

        // A whole handshake message from the client (header included); not
        // noexcept: an RSA signature that does not verify under its own
        // public key (a fault in the computation) is crypto's
        // std::runtime_error (sign, signature.h)
        SGCL_INLINE_HOT const Step& feed(const Bytes& message) {
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

        // The identities of this connection, chosen once its first
        // ClientHello was seen (tls::config::identity_for), before that
        // hello is fed
        SGCL_INLINE_HOT void set_identities(vector<ServerIdentity> identities) noexcept {
            _settings.identities = std::move(identities);
        }

        // The ClientHello the handshake answers, of the first one received:
        // its ClientHelloInner when it carries ECH's outer extension and one
        // of the keys opens it (RFC 9849 §7.1), else the hello itself; for
        // the choice of the identity (tls::config::identity_for) before the
        // hello is fed. The decryption is made once
        Bytes hello_answered(const Bytes& message) noexcept {
            if (_state != State::wait_client_hello) {
                return message;
            }
            if (!_ech_tried) {
                _ech_tried = true;
                auto r = _ech_open_first(message);
                if (!r) {
                    _ech_error = r.error();
                }
            }
            return _ech == 1 ? bytes_of(_s->inner_hello.data(), _s->inner_hello.size()) : message;
        }

        // An alert record from the client: the handshake is over
        SGCL_INLINE_HOT void on_record_alert(const Alert& a) noexcept {
            _peer_alert = a;
            _state = State::failed;
            _wipe();
        }

        SGCL_INLINE_HOT bool established() const noexcept {
            return _state == State::connected;
        }

        SGCL_INLINE_HOT bool failed() const noexcept {
            return _state == State::failed;
        }

        SGCL_INLINE_HOT const ServerResult& result() const noexcept {
            return _result;
        }

        SGCL_INLINE_HOT const optional<Alert>& peer_alert() const noexcept {
            return _peer_alert;
        }

        // Why the client's chain did not verify, when that ended the
        // handshake (reason::none otherwise)
        SGCL_INLINE_HOT crypto::x509::reason verify_reason() const noexcept {
            return _verify_reason;
        }

    private:
        enum class State : uint8_t {
            wait_client_hello,
            wait_second_hello,
            wait_certificate,
            wait_certificate_verify,
            wait_finished,
            connected,
            failed,
        };

        struct Secrets {
            optional<Transcript> transcript;
            optional<KeySchedule> schedule;
            uint8_t random[32] = {};
            Secret psk;                  // a ticket's, once taken, until the schedule has it
            Step step;
            // ECH (RFC 9849): the context the inner hellos open under, the
            // inner hello decoded, its random
            optional<crypto::hpke::recipient> ech;
            std::vector<byte> inner_hello;
            uint8_t inner_random[32] = {};

            SGCL_INLINE_HOT ~Secrets() {
                crypto::detail::secure_zero(random, sizeof random);
                crypto::detail::secure_zero(inner_random, sizeof inner_random);
            }
        };

        ServerSettings _settings;
        Entropy _entropy;
        Clock _clock;
        std::unique_ptr<Secrets> _s;
        State _state = State::wait_client_hello;
        Hash _hash = Hash::sha256;
        uint8_t _session_id[32] = {};
        size_t _session_id_size = 0;
        bool _ccs_sent = false;
        bool _status_requested = false;     // the client's status_request (RFC 6066) asked for an OCSP staple
        ServerResult _result;
        optional<Alert> _peer_alert;
        uint16_t _psk_identity = 0;
        crypto::x509::reason _verify_reason = crypto::x509::reason::none;
        uint8_t _ech = 0;                   // ECH: 0 none, 1 accepted, 2 offered and rejected (retry_configs sent)
        bool _ech_tried = false;            // the first hello's decryption made
        optional<Alert> _ech_error;         // an inner hello that decrypted and does not decode
        uint8_t _ech_config = 0;            // the accepted hello's config id and suite, which the second must keep
        uint16_t _ech_kdf = 0;
        uint16_t _ech_aead = 0;

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
            case State::wait_client_hello: {
                if (type != HandshakeType::client_hello) {
                    break;
                }
                const Bytes answered = hello_answered(message);
                if (_ech_error) {
                    return unexpected(*_ech_error);
                }
                if (_ech == 1) {
                    auto inner = read_handshake(answered);
                    return _client_hello(answered, inner->body, false);
                }
                return _client_hello(message, h->body, false);
            }
            case State::wait_second_hello:
                if (type != HandshakeType::client_hello) {
                    break;
                }
                if (_ech == 1) {
                    // RFC 9849 §7.1.1: the second outer hello opened under the
                    // first's context, enc empty, the same config and suite
                    auto inner = _ech_open_second(message);
                    if (!inner) {
                        return unexpected(inner.error());
                    }
                    auto ih = read_handshake(bytes_of(_s->inner_hello.data(), _s->inner_hello.size()));
                    return _client_hello(bytes_of(_s->inner_hello.data(), _s->inner_hello.size()), ih->body, true);
                }
                return _client_hello(message, h->body, true);
            case State::wait_certificate:
                if (type != HandshakeType::certificate) {
                    break;
                }
                return _client_certificate(message, h->body);
            case State::wait_certificate_verify:
                if (type != HandshakeType::certificate_verify) {
                    break;
                }
                return _client_certificate_verify(message, h->body);
            case State::wait_finished:
                if (type != HandshakeType::finished) {
                    break;   // a Certificate where none was asked for, or a resumed handshake
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

        // --- ECH (RFC 9849 §7) ------------------------------------------------------

        // The outer extension of a hello and where its payload lies in the
        // message; nullopt for a hello without one (or with one of type inner)
        struct EchFound {
            EchOuter outer;
            size_t payload_at = 0;      // in the message
        };

        static expected<optional<EchFound>, Alert> _ech_find(const Bytes& message, const ClientHello& ch) noexcept {
            auto e = ch.extensions.find(EchExtension);
            if (!e) {
                return optional<EchFound>();
            }
            auto type = ech_type(*e);
            if (!type || *type != 0) {
                return optional<EchFound>();
            }
            auto outer = read_ech_outer(*e);
            if (!outer) {
                return unexpected(outer.error());
            }
            EchFound f;
            f.outer = *outer;
            f.payload_at = size_t(e->data() - message.data()) + outer->payload_at;
            return optional<EchFound>(f);
        }

        // The payload opened over the hello's body with the payload zeroed
        // (ClientHelloOuterAAD, §5.2) and decoded into the inner hello
        expected<bool, Alert> _ech_decode(const Bytes& message, const ClientHello& ch, const EchFound& f) {
            std::vector<byte> aad(message.data() + 4, message.data() + message.size());
            crypto::detail::secure_zero(aad.data() + (f.payload_at - 4), f.outer.payload.size());
            auto plain = _s->ech->open(f.outer.payload, bytes_of(aad.data(), aad.size()));
            if (!plain) {
                return false;
            }
            auto inner = decode_inner(plain->as_slice(), ch);
            if (!inner) {
                return unexpected(inner.error());
            }
            _s->inner_hello = std::move(*inner);
            std::memcpy(_s->inner_random, _s->inner_hello.data() + 6, 32);
            return true;
        }

        // The first hello: a key of its config id, a suite it has, the
        // context set up and the payload opened. A hello that does not open
        // is answered as the outer hello, with retry_configs (§7.1); one that
        // opens and does not decode is illegal_parameter
        expected<void, Alert> _ech_open_first(const Bytes& message) {
            if (_settings.ech_keys.empty()) {
                return {};
            }
            auto h = read_handshake(message);
            if (!h) {
                return {};
            }
            auto ch = read_client_hello(h->body);
            if (!ch) {
                return {};
            }
            auto found = _ech_find(message, *ch);
            if (!found) {
                return unexpected(found.error());
            }
            if (!*found) {
                return {};
            }
            const EchFound& f = **found;
            _ech = 2;
            const uint32_t suite = uint32_t(f.outer.kdf) << 16 | f.outer.aead;
            for (const auto& k : _settings.ech_keys) {
                bool has = false;
                for (uint32_t x : k.suites) {
                    has |= x == suite;
                }
                if (k.id != f.outer.config_id || !has || !ech_suite_known(suite)) {
                    continue;
                }
                const auto info = ech_info(k.config);
                auto r = crypto::hpke::recipient::setup(f.outer.enc, *k.key, crypto::hpke::suite{crypto::hpke::kdf(f.outer.kdf), crypto::hpke::aead(f.outer.aead)},
                                                        bytes_of(info.data(), info.size()));
                if (!r) {
                    continue;
                }
                _s->ech.emplace(std::move(*r));
                auto d = _ech_decode(message, *ch, f);
                if (!d) {
                    return unexpected(d.error());
                }
                if (*d) {
                    _ech = 1;
                    _ech_config = f.outer.config_id;
                    _ech_kdf = f.outer.kdf;
                    _ech_aead = f.outer.aead;
                    _result.ech_accepted = true;
                    return {};
                }
                _s->ech.reset();
            }
            return {};
        }

        expected<bool, Alert> _ech_open_second(const Bytes& message) {
            auto fail = [](const char* what) {
                return unexpected(Alert{AlertDescription::decrypt_error, 0, what});
            };
            auto h = read_handshake(message);
            if (!h) {
                return unexpected(h.error());
            }
            auto ch = read_client_hello(h->body);
            if (!ch) {
                return unexpected(ch.error());
            }
            auto found = _ech_find(message, *ch);
            if (!found) {
                return unexpected(found.error());
            }
            if (!*found) {
                return unexpected(Alert{AlertDescription::missing_extension, 0, "a second ClientHello without encrypted_client_hello after ECH was accepted"});
            }
            const EchFound& f = **found;
            if (!f.outer.enc.empty() || f.outer.config_id != _ech_config || f.outer.kdf != _ech_kdf || f.outer.aead != _ech_aead) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a second ClientHelloOuter with enc, or another config or suite"});
            }
            auto d = _ech_decode(message, *ch, f);
            if (!d) {
                return unexpected(d.error());
            }
            if (!*d) {
                return fail("a second ClientHelloInner that does not open");
            }
            return true;
        }

        // The 8 bytes of a confirmation written into the message at `at`
        // (zeros there now), over the transcript so far (§7.2)
        void _ech_confirm(size_t message_at, size_t at, const char* label, const Transcript& t) noexcept {
            auto& out = _s->step.out;
            uint8_t confirmation[8];
            ech_confirmation(_hash, confirmation, _s->inner_random, t, bytes_of(out.data() + message_at, out.size() - message_at), label);
            sgcl::detail::copy_bytes(out.data() + at, confirmation, 8);
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
                return unexpected(_alert(AlertDescription::missing_extension, "a ClientHello without supported_groups or key_share (psk_dhe_ke alone is taken)"));
            }
            if (x.has(ExtensionType::pre_shared_key) && !x.has(ExtensionType::psk_key_exchange_modes)) {
                return unexpected(_alert(AlertDescription::missing_extension, "pre_shared_key without psk_key_exchange_modes (§4.2.9)"));
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
            if (auto st = x.find(ExtensionType::status_request)) {
                auto ocsp = read_status_request(*st);
                if (!ocsp) {
                    return unexpected(ocsp.error());
                }
                _status_requested = *ocsp;
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
            if (shared) {   // a session for this hello's ServerHello (after a HelloRetryRequest, the second's)
                if (auto p = _try_psk(message, x, nullptr); !p) {
                    return p;
                }
            }
            _s->transcript.emplace(_hash);
            _s->transcript->update(message);
            if (!shared) {
                return _retry();
            }
            return _server_flight(*shares->find(*group));
        }

        // A session of the client's pre_shared_key (§4.2.11): the first
        // identity that is a ticket of the server's keys and holds; its
        // binder checked over `prior` (the transcript before this hello,
        // none for the first) and the hello truncated. Nothing taken is no
        // error: a full handshake
        expected<void, Alert> _try_psk(const Bytes& message, const Extensions& x, const Transcript* prior) {
            auto body = x.find(ExtensionType::pre_shared_key);
            if (!body) {
                return {};
            }
            auto modes = read_psk_modes(*x.find(ExtensionType::psk_key_exchange_modes));
            if (!modes) {
                return unexpected(modes.error());
            }
            auto keys = read_pre_shared_keys(*body);
            if (!keys) {
                return unexpected(keys.error());
            }
            bool dhe = false;
            for (auto m : *modes) {
                dhe |= uint8_t(m) == 1;   // psk_dhe_ke: psk_ke alone is not taken
            }
            if (!_settings.tickets || !dhe) {
                return {};
            }
            const size_t n = hash_size(_hash);
            const time::datetime now = _clock();
            const int64_t now_ms = now.unix_milli();
            std::vector<byte> plain;   // the content opened: unmanaged, zeroed
            auto zero = [&]() noexcept {
                crypto::detail::secure_zero(plain.data(), plain.size());
            };
            // the identities and their binders side by side, the first
            // MaxPskIdentities of them (as Go: the work of a hello bounded)
            Reader identities(keys->identities), binders(keys->binders);
            for (size_t i = 0; i < keys->count && i < MaxPskIdentities; ++i) {
                PskIdentity id;
                Bytes theirs;
                (void)identities.vec16(id.identity, 1, 0xFFFF);   // read once already: cannot fail
                (void)identities.u32(id.obfuscated_age);
                (void)binders.vec8(theirs, 32, 255);
                if (id.identity.size() < TicketKeys::Overhead || id.identity.size() > MaxTicket) {
                    continue;
                }
                zero();
                plain.resize(id.identity.size() - TicketKeys::Overhead);
                auto opened = _settings.tickets->open(id.identity, reinterpret_cast<uint8_t*>(plain.data()));
                if (!opened) {
                    continue;
                }
                auto t = read_ticket_content(bytes_of(plain.data(), *opened));
                if (!t || !known(Cipher(t->cipher)) || hash_of(Cipher(t->cipher)) != _hash || t->psk.size() != n) {
                    continue;   // of a suite of another hash: a full handshake (§4.2.11)
                }
                if (now_ms < t->issued_ms || now_ms - t->issued_ms > int64_t(t->lifetime) * 1000) {
                    continue;   // past its lifetime
                }
                crypto::x509::chain certificates;
                bool chain_ok = true;
                Reader list(t->certificates);
                while (!list.empty()) {
                    Bytes der;
                    (void)list.vec24(der, 1, 0xFFFFFF);
                    auto c = crypto::x509::certificate::parse(der);
                    if (!c) {
                        chain_ok = false;
                        break;
                    }
                    certificates.push_back(std::move(*c));
                }
                if (!chain_ok || (_settings.client_auth == 2 && certificates.empty())) {
                    continue;   // a session without the client certificate now required
                }
                if (!certificates.empty() && (now < certificates[0].not_before() || now > certificates[0].not_after())) {
                    continue;   // the client's certificate out of its time
                }
                // the binder: one that does not verify ends the handshake (§4.2.11.2)
                Secret psk;
                std::memcpy(psk.bytes, t->psk.data(), n);
                psk.size = uint8_t(n);
                zero();
                Transcript tr = prior ? *prior : Transcript(_hash);
                tr.update(bytes_of(message.data(), keys->truncated_size(message)));
                uint8_t h[MaxHashSize], mine[MaxHashSize];
                tr.value_to(h);
                KeySchedule(_hash, psk).binder(mine, bytes_of(h, n));
                const bool ok = theirs.size() == n && crypto::constant_time::equal(bytes_of(mine, n), theirs);
                crypto::detail::secure_zero(mine, sizeof mine);
                if (!ok) {
                    return unexpected(_alert(AlertDescription::decrypt_error, "a PSK binder that does not verify"));
                }
                _s->psk = std::move(psk);
                _psk_identity = uint16_t(i);
                _result.resumed = true;
                _result.peer_certificates = certificates;
                return {};
            }
            zero();
            return {};
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
                {
                    auto e = w.extension(ExtensionType::supported_versions);
                    write_version_selected(w, Tls13);
                }
                if (_ech == 1) {
                    // the HelloRetryRequest's acceptance (§7.2.1), last, zeros for now
                    auto e = w.extension(EchExtension);
                    for (int i = 0; i < 8; ++i) {
                        w.u8(0);
                    }
                }
            });
            if (_ech == 1) {
                _ech_confirm(at, out.size() - 8, EchHrrAccept, *_s->transcript);
            }
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
            if (auto p = _try_psk(message, ch.extensions, &*_s->transcript); !p) {
                return p;
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
                {
                    auto e = w.extension(ExtensionType::supported_versions);
                    write_version_selected(w, Tls13);
                }
                if (_result.resumed) {
                    auto e = w.extension(ExtensionType::pre_shared_key);
                    write_pre_shared_key_selected(w, _psk_identity);
                }
            });
            if (_ech == 1) {
                // the acceptance in the last 8 bytes of the random (§7.2)
                crypto::detail::secure_zero(out.data() + at + 4 + 2 + 24, 8);
                _ech_confirm(at, at + 4 + 2 + 24, EchAccept, *_s->transcript);
            }
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            _push_send(Epoch::initial, at, out.size() - at);
            if (_session_id_size && !_ccs_sent) {
                _push_ccs();
            }
            uint8_t h[MaxHashSize];
            _s->transcript->value_to(h);
            if (_result.resumed) {
                _s->schedule.emplace(_hash, _s->psk);
                _s->psk.wipe();
            } else {
                _s->schedule.emplace(_hash);
            }
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
                if (_ech == 2 && !_settings.ech_retry_configs.empty()) {
                    // retry_configs (RFC 9849 §7.1): the ECHConfigList to try next
                    auto e = w.extension(EchExtension);
                    w.bytes(bytes_of(_settings.ech_retry_configs.data(), _settings.ech_retry_configs.size()));
                }
            });
            _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
            // a resumed handshake authenticates by the PSK: no certificate
            // either way (§4.3.2)
            if (!_result.resumed) {
                if (_settings.client_auth) {
                    at = out.size();
                    _write_certificate_request(w);
                    _s->transcript->update(bytes_of(out.data() + at, out.size() - at));
                }
                const ServerIdentity& id = _settings.identities[_result.identity];
                at = out.size();
                {
                    std::vector<Bytes> ders;   // lint-handles: ok views of unmanaged bytes (bytes_of): no owner, no word
                    for (auto& d : id.chain) {
                        ders.push_back(bytes_of(d.data(), d.size()));
                    }
                    std::vector<byte> staple;
                    if (_status_requested && id.staple) {
                        staple = id.staple->current(_clock().unix());
                    }
                    write_certificate(w, Bytes(), ders, bytes_of(staple.data(), staple.size()));
                    _result.staple_sent = !staple.empty();
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
            }
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
            _state = _settings.client_auth && !_result.resumed ? State::wait_certificate : State::wait_finished;
            return {};
        }

        // CertificateRequest (§4.3.2): an empty context, the schemes taken,
        // the subjects of the client roots as certificate_authorities (as
        // many as the extension holds) when the roots are given
        void _write_certificate_request(Builder& w) noexcept {
            write_certificate_request(w, Bytes(), [&](Builder& w) noexcept {
                {
                    auto e = w.extension(ExtensionType::signature_algorithms);
                    write_signature_schemes(w, RequestSchemes);
                }
                if (_settings.client_roots && !_settings.client_roots->empty()) {
                    auto e = w.extension(ExtensionType::certificate_authorities);
                    auto list = w.block16();
                    size_t used = 0;
                    for (const auto& c : _settings.client_roots->certificates()) {
                        auto subject = c.raw_subject();
                        if (subject.empty() || subject.size() > 0xFFFF || used + 2 + subject.size() > 0xFFFF) {
                            continue;
                        }
                        used += 2 + subject.size();
                        auto one = w.block16();
                        w.bytes(subject);
                    }
                }
            });
        }

        // --- the client's certificate ------------------------------------------------

        expected<void, Alert> _client_certificate(const Bytes& message, const Bytes& body) {
            auto c = read_certificate(body);
            if (!c) {
                return unexpected(c.error());
            }
            if (!c->context.empty()) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "the client's Certificate with a context"));
            }
            if (c->count == 0) {
                if (_settings.client_auth == 2) {
                    return unexpected(_alert(AlertDescription::certificate_required, "no client certificate where one is required"));
                }
                _s->transcript->update(message);
                _state = State::wait_finished;
                return {};
            }
            crypto::x509::chain chain;
            for (auto entry : *c) {
                if (auto v = validate_extensions(HandshakeType::certificate, false, entry.extensions, 0); !v) {
                    return unexpected(v.error());
                }
                auto cert = crypto::x509::certificate::parse(entry.der);
                if (!cert) {
                    return unexpected(_alert(AlertDescription::bad_certificate, "a client certificate that does not parse"));
                }
                chain.push_back(std::move(*cert));
            }
            crypto::x509::verify_options o;
            if (_settings.client_roots) {
                o.roots = *_settings.client_roots;
            }
            for (size_t i = 1; i < chain.size(); ++i) {
                o.intermediates.add(chain[i]);
            }
            o.key_usages = {crypto::x509::ext_key_usage::client_auth};
            o.time = _clock();
            auto v = chain[0].verify(o);
            if (!v) {
                _verify_reason = v.error().reason();
                return unexpected(verification_alert(v.error(), false));
            }
            _result.verified_chain = std::move(*v);
            _result.peer_certificates = chain;
            _s->transcript->update(message);
            _state = State::wait_certificate_verify;
            return {};
        }

        expected<void, Alert> _client_certificate_verify(const Bytes& message, const Bytes& body) noexcept {
            auto cv = read_certificate_verify(body);
            if (!cv) {
                return unexpected(cv.error());
            }
            if (!_has(RequestSchemes, cv->scheme)) {
                return unexpected(_alert(AlertDescription::illegal_parameter, "the client's CertificateVerify with a scheme not asked for"));
            }
            const size_t n = hash_size(_hash);
            uint8_t h[MaxHashSize];
            _s->transcript->value_to(h);
            auto content = certificate_verify_content(false, bytes_of(h, n));
            if (auto v = verify(cv->scheme, _result.peer_certificates[0].public_key(), content.view(), cv->signature); !v) {
                return unexpected(v.error());
            }
            _s->transcript->update(message);
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
            if (_settings.tickets) {
                _issue_ticket();
            }
            _wipe();
            _state = State::connected;
            return {};
        }

        // One NewSessionTicket (§4.6.1), under the application keys: the
        // PSK of the resumption master secret and the nonce 0, sealed with
        // what the session needs into the ticket; none when the client's
        // chain makes it larger than MaxTicket
        void _issue_ticket() noexcept {
            const size_t n = hash_size(_hash);
            uint8_t h[MaxHashSize];
            _s->transcript->value_to(h);
            Secret master, psk;
            _s->schedule->resumption(master, bytes_of(h, n));
            ZeroingProbe::on_secret(master);
            const uint8_t nonce[1] = {0};
            resumption_psk(_hash, psk, master, bytes_of(nonce, 1));
            master.wipe();
            ZeroingProbe::on_secret(psk);
            uint8_t add[4];
            _entropy(add, 4);
            const uint32_t age_add = uint32_t(add[0]) << 24 | uint32_t(add[1]) << 16 | uint32_t(add[2]) << 8 | add[3];
            const int64_t now_ms = _clock().unix_milli();
            std::vector<byte> content;
            {
                Builder c(content);
                write_ticket_content(c, uint16_t(_result.cipher), now_ms, _settings.ticket_lifetime, age_add, psk, _result.peer_certificates);
            }
            psk.wipe();
            if (content.size() + TicketKeys::Overhead > MaxTicket) {
                crypto::detail::secure_zero(content.data(), content.size());
                return;
            }
            std::vector<byte> ticket;
            _settings.tickets->seal(ticket, bytes_of(content.data(), content.size()), now_ms, int64_t(_settings.ticket_lifetime) * 1000, _entropy);
            crypto::detail::secure_zero(content.data(), content.size());
            auto& out = _s->step.out;
            const size_t at = out.size();
            Builder w(out);
            {
                auto m = w.message(HandshakeType::new_session_ticket);
                w.u32(_settings.ticket_lifetime);
                w.u32(age_add);
                {
                    auto b = w.block8();
                    w.bytes(nonce, 1);
                }
                {
                    auto t = w.block16();
                    w.bytes(ticket.data(), ticket.size());
                }
                auto x = w.block16(0xFFFE);
            }
            _push_send(Epoch::application, at, out.size() - at);
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

        SGCL_INLINE_HOT void _push_send(Epoch e, size_t offset, size_t size) noexcept {
            Action a{Action::Kind::send, e};
            a.offset = offset;
            a.size = size;
            _s->step.actions.push_back(std::move(a));
        }

        SGCL_INLINE_HOT void _push_ccs() noexcept {
            _s->step.actions.push_back(Action{Action::Kind::change_cipher_spec, Epoch::initial});
            _ccs_sent = true;
        }

        SGCL_INLINE_HOT void _push_install(Action::Kind kind, Epoch e, const Secret& s) noexcept {
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

        SGCL_INLINE_HOT void _wipe() noexcept {
            _s->psk.wipe();
            if (_s->schedule) {
                _s->schedule->finish_handshake();
                _s->schedule.reset();
            }
            _s->transcript.reset();
        }
    };
}
