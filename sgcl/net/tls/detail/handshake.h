//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "key_share.h"
#include "messages.h"
#include "record.h"
#include "schedule.h"
#include "signature.h"
#include "../../../crypto/constant_time.h"
#include "../../../crypto/x509.h"
#include "../../ip.h"
#include "../../../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The client's side of the TLS 1.3 handshake (RFC 8446 §2, §4) as a
// machine without input or output of its own: start() gives the first
// flight, feed() each whole handshake message the assembler gives, and
// each call answers with a Step, the actions the connection carries out
// in their order — bytes to send under an epoch's keys, a compatibility
// change_cipher_spec, keys to install or update in a direction, the
// handshake established, an alert to send before the connection ends.
//
// v1 (the auditor's decisions of 2026-09-27): no PSK and no 0-RTT (a
// NewSessionTicket is read and passed over); a CertificateRequest is
// answered with an empty Certificate (mTLS after v1); ALPN negotiated
// (RFC 7301); the server's chain verified against the roots and the
// server name unless insecure_skip_verify; one HelloRetryRequest at most
// (§4.1.4). The transcript is hashed with SHA-256 and SHA-384 side by side
// until the ServerHello names the suite, the other dropped then.
//
// The machine keeps its secrets (the private keys of its shares, the key
// schedule, the transcripts, the secrets its actions carry) in an
// unmanaged block of its own, zeroed stage by stage and when it goes; what
// outlives the handshake and is managed (the peer's certificates, the
// protocol chosen) are members of the machine itself, which therefore
// lives in a frame or in a managed object, never in unmanaged memory.
namespace sgcl::net::tls::detail {
    inline constexpr uint16_t RenegotiationInfo = 0xFF01;   // RFC 5746, empty: as Go sends it
    inline constexpr uint16_t Padding = 0x0015;             // RFC 7685
    inline constexpr uint16_t SessionTicket = 0x0023;       // RFC 5077, empty

    // The time a certificate's validity is judged at
    struct Clock {
        time::datetime (*now)(void* context) = &Clock::system_now;
        void* context = nullptr;

        time::datetime operator()() const {
            return now(context);
        }

        static time::datetime system_now(void*) {
            return time::now();
        }
    };

    // What the client offers and how it checks the server: the lists are
    // the codes on the wire, in the order they are sent (tls::config makes
    // them, T6)
    struct ClientSettings {
        string server_name;                                   // SNI and the name the certificate must hold
        optional<crypto::x509::certificate_pool> roots;       // nullopt: the system's
        vector<uint16_t> ciphers = {0x1301, 0x1302, 0x1303};
        vector<uint16_t> groups = {0x11EC, 0x001D, 0x0017, 0x0018};
        vector<uint16_t> key_shares = {0x11EC, 0x001D};       // the groups of the first ClientHello's shares
        vector<uint16_t> schemes = {0x0403, 0x0804, 0x0401, 0x0503, 0x0805, 0x0501, 0x0806, 0x0601, 0x0807};
        vector<string> alpn;
        bool insecure_skip_verify = false;
        bool compatibility_mode = true;                       // §D.4: a session id and a change_cipher_spec
        uint16_t record_size_limit = 0;                       // RFC 8449, 0: not sent
        bool pad_client_hello = false;                        // RFC 7685 to 512 bytes, as BoringSSL and NSS
        bool session_ticket_extension = false;                // RFC 5077's, empty (RFC 8448 §3 sends it)
    };

    // One action of a step
    struct Action {
        enum class Kind : uint8_t {
            send,                   // bytes() under the keys of `epoch`
            change_cipher_spec,     // the compatibility record (§D.4), plaintext
            install_read,           // the keys of `secret` for reading, epoch `epoch`
            install_write,          // the same for writing
            update_read,            // the next traffic secret for reading (§7.2)
            update_write,           // the same for writing
            established,            // the handshake is done: result()
            alert,                  // send `alert`, then the connection ends
            skip_early_data,        // the server's: 0-RTT refused, skip up to `size` bytes of the client's early data
                                    // (application_data records before the server's keys are read, records that do
                                    // not open under the handshake keys; RFC 8446 §4.2.10)
        };

        // (a constructor, not an aggregate: the actions name only what
        // they carry, the rest its default)
        explicit Action(Kind k, Epoch e = Epoch::initial, Cipher c = Cipher::aes_128_gcm_sha256) noexcept
        : kind(k), epoch(e), cipher(c) {
        }

        Kind kind;
        Epoch epoch = Epoch::initial;
        Cipher cipher = Cipher::aes_128_gcm_sha256;
        Secret secret;              // install_*: the traffic secret (zeroed with the step)
        size_t offset = 0;          // send: the bytes in the step's buffer
        size_t size = 0;
        AlertDescription alert = AlertDescription::internal_error;
        const char* what = nullptr; // alert: why (a diagnostic, never sent)
    };

    // The actions of one call, valid until the next call
    struct Step {
        std::vector<Action> actions;
        std::vector<byte> out;      // the bytes of the send actions

        Bytes bytes(const Action& a) const noexcept {
            return Bytes(out.data() + a.offset, a.size);
        }

        void clear() noexcept {
            actions.clear();   // each Secret zeroes itself
            crypto::detail::secure_zero(out.data(), out.size());
            out.clear();
        }
    };

    // What the handshake settled
    struct ClientResult {
        Cipher cipher = Cipher::aes_128_gcm_sha256;
        Group group = Group::x25519;
        string alpn;                                // empty: none
        string server_name;
        crypto::x509::chain peer_certificates;      // the leaf first, as sent
        uint16_t record_size_limit = 0;             // the server's (RFC 8449), 0: none
        bool retried = false;                       // a HelloRetryRequest came
    };

    class ClientHandshake {
    public:
        ClientHandshake(const ClientSettings& settings, const Entropy& entropy = Entropy(), const Clock& clock = Clock())
        : _settings(settings), _entropy(entropy), _clock(clock), _s(std::make_unique<Secrets>()) {
        }

        ClientHandshake(const ClientHandshake&) = delete;
        ClientHandshake& operator=(const ClientHandshake&) = delete;

        // The first flight: the ClientHello
        const Step& start() {
            _s->step.clear();
            if (_state != State::start) {
                return _fail(Alert{AlertDescription::internal_error, 0, "start() twice"});
            }
            if (auto r = _start(); !r) {
                return _fail(r.error());
            }
            return _s->step;
        }

        // A whole handshake message from the server (header included)
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

        // An alert record from the server: the handshake is over
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

        const ClientResult& result() const noexcept {
            return _result;
        }

        const optional<Alert>& peer_alert() const noexcept {
            return _peer_alert;
        }

        // Why the server's chain did not verify, when that ended the
        // handshake (reason::none otherwise)
        crypto::x509::reason verify_reason() const noexcept {
            return _verify_reason;
        }

    private:
        enum class State : uint8_t {
            start,
            wait_server_hello,
            wait_encrypted_extensions,
            wait_certificate_or_request,
            wait_certificate,
            wait_certificate_verify,
            wait_finished,
            connected,
            failed,
        };

        // Everything secret, and the buffers of the handshake's own bytes
        struct Secrets {
            ClientShares shares;
            Transcript t256{Hash::sha256};
            Transcript t384{Hash::sha384};
            optional<KeySchedule> schedule;
            uint8_t random[32] = {};
            uint8_t session_id[32] = {};
            size_t session_id_size = 0;
            std::vector<byte> cookie;
            Step step;

            ~Secrets() {
                crypto::detail::secure_zero(random, sizeof random);
                crypto::detail::secure_zero(cookie.data(), cookie.size());
            }
        };

        ClientSettings _settings;
        Entropy _entropy;
        Clock _clock;
        std::unique_ptr<Secrets> _s;
        State _state = State::start;
        Hash _hash = Hash::sha256;
        bool _hash_known = false;
        uint64_t _offered = 0;              // the extensions of the ClientHello (bits of types below 64)
        bool _retried = false;
        bool _ccs_sent = false;
        bool _certificate_requested = false;
        uint16_t _retry_group = 0;
        ClientResult _result;
        optional<Alert> _peer_alert;
        crypto::x509::reason _verify_reason = crypto::x509::reason::none;

        // --- the ClientHello ---------------------------------------------------

        expected<void, Alert> _start() {
            if (_settings.server_name.empty() && !_settings.insecure_skip_verify) {
                return unexpected(Alert{AlertDescription::internal_error, 0, "a server name, or insecure_skip_verify, is needed to check the server"});
            }
            if (_settings.ciphers.empty() || _settings.groups.empty() || _settings.key_shares.empty() || _settings.schemes.empty()) {
                return unexpected(Alert{AlertDescription::internal_error, 0, "an empty list of ciphers, groups, shares or schemes"});
            }
            _entropy(_s->random, 32);
            if (_settings.compatibility_mode) {
                _entropy(_s->session_id, 32);
                _s->session_id_size = 32;
            }
            for (uint16_t g : _settings.key_shares) {
                if (!supported(Group(g))) {
                    return unexpected(Alert{AlertDescription::internal_error, 0, "a key share of a group v1 does not have"});
                }
                _s->shares.add(Group(g), _entropy);
            }
            _send_hello();
            _state = State::wait_server_hello;
            return {};
        }

        static bool is_ip_literal(const string& name) {
            auto v = name.view();
            if (v.find(':') != std::string_view::npos) {
                return true;
            }
            for (char c : v) {
                if (c != '.' && (c < '0' || c > '9')) {
                    return false;
                }
            }
            return !v.empty();
        }

        void _send_hello() {
            auto& out = _s->step.out;
            size_t start = out.size();
            Builder w(out);
            uint64_t offered = 0;
            auto mark = [&](ExtensionType t) {
                offered |= bit_of(t);
            };
            write_client_hello(w, bytes_of(_s->random, 32), bytes_of(_s->session_id, _s->session_id_size), _settings.ciphers, [&](Builder& w) {
                if (!_settings.server_name.empty() && !is_ip_literal(_settings.server_name)) {
                    auto e = w.extension(ExtensionType::server_name);
                    auto v = _settings.server_name.view();
                    write_server_name(w, bytes_of(v.data(), v.size()));
                    mark(ExtensionType::server_name);
                }
                {
                    auto e = w.extension(RenegotiationInfo);
                    w.u8(0);
                }
                {
                    auto e = w.extension(ExtensionType::supported_groups);
                    write_groups(w, _settings.groups);
                    mark(ExtensionType::supported_groups);
                }
                if (_settings.session_ticket_extension) {
                    auto e = w.extension(SessionTicket);
                }
                {
                    auto e = w.extension(ExtensionType::key_share);
                    auto list = w.block16();
                    for (size_t i = 0; i < _s->shares.size(); ++i) {
                        w.u16(uint16_t(_s->shares.group(i)));
                        auto k = w.block16();
                        w.bytes(_s->shares.public_share(i));
                    }
                    mark(ExtensionType::key_share);
                }
                {
                    auto e = w.extension(ExtensionType::supported_versions);
                    const uint16_t versions[] = {Tls13};
                    write_versions_offered(w, versions);
                    mark(ExtensionType::supported_versions);
                }
                {
                    auto e = w.extension(ExtensionType::signature_algorithms);
                    write_signature_schemes(w, _settings.schemes);
                    mark(ExtensionType::signature_algorithms);
                }
                if (!_s->cookie.empty()) {
                    auto e = w.extension(ExtensionType::cookie);
                    write_cookie(w, bytes_of(_s->cookie.data(), _s->cookie.size()));
                    mark(ExtensionType::cookie);
                }
                {
                    auto e = w.extension(ExtensionType::psk_key_exchange_modes);
                    const uint8_t dhe = 1;
                    write_psk_modes(w, bytes_of(&dhe, 1));
                    mark(ExtensionType::psk_key_exchange_modes);
                }
                if (_settings.record_size_limit) {
                    auto e = w.extension(ExtensionType::record_size_limit);
                    w.u16(_settings.record_size_limit);
                    mark(ExtensionType::record_size_limit);
                }
                if (!_settings.alpn.empty()) {
                    auto e = w.extension(ExtensionType::application_layer_protocol_negotiation);
                    auto list = w.block16();
                    for (const string& p : _settings.alpn) {
                        auto v = p.view();
                        auto one = w.block8();
                        w.bytes(v.data(), v.size());
                    }
                    mark(ExtensionType::application_layer_protocol_negotiation);
                }
                if (_settings.pad_client_hello) {
                    // RFC 7685 as BoringSSL: a hello of 256 to 511 bytes
                    // made 512, the padding extension's header counted
                    size_t length = out.size() - start;
                    if (length >= 256 && length < 512) {
                        size_t pad = 512 - length;
                        pad = pad >= 5 ? pad - 4 : 1;
                        auto e = w.extension(Padding);
                        for (size_t i = 0; i < pad; ++i) {
                            w.u8(0);
                        }
                    }
                }
            });
            _offered = offered;
            _hash_update(bytes_of(out.data() + start, out.size() - start));
            _push_send(Epoch::initial, start, out.size() - start);
        }

        // --- the server's messages ----------------------------------------------

        expected<void, Alert> _feed(const Bytes& message) {
            auto h = read_handshake(message);
            if (!h) {
                return unexpected(h.error());
            }
            const HandshakeType type = HandshakeType(h->type);
            switch (_state) {
            case State::wait_server_hello:
                if (type != HandshakeType::server_hello) {
                    break;
                }
                return _server_hello(message, h->body);
            case State::wait_encrypted_extensions:
                if (type != HandshakeType::encrypted_extensions) {
                    break;
                }
                return _encrypted_extensions(message, h->body);
            case State::wait_certificate_or_request:
                if (type == HandshakeType::certificate_request) {
                    return _certificate_request(message, h->body);
                }
                [[fallthrough]];
            case State::wait_certificate:
                if (type != HandshakeType::certificate) {
                    break;
                }
                return _certificate(message, h->body);
            case State::wait_certificate_verify:
                if (type != HandshakeType::certificate_verify) {
                    break;
                }
                return _certificate_verify(message, h->body);
            case State::wait_finished:
                if (type != HandshakeType::finished) {
                    break;
                }
                return _finished(message, h->body);
            case State::connected:
                return _after(type, h->body);
            case State::start:
            case State::failed:
                break;
            }
            return unexpected(Alert{AlertDescription::unexpected_message, 0, "a handshake message out of order"});
        }

        expected<void, Alert> _server_hello(const Bytes& message, const Bytes& body) {
            auto sh = read_server_hello(body);
            if (!sh) {
                return unexpected(sh.error());
            }
            const bool retry = sh->is_retry();
            // supported_versions first: without it the server chose an
            // older version, which v1 does not speak (§4.2.1)
            auto versions = sh->extensions.find(ExtensionType::supported_versions);
            if (!versions) {
                return unexpected(Alert{AlertDescription::protocol_version, 0, "a ServerHello of a version before TLS 1.3"});
            }
            auto version = read_version_selected(*versions);
            if (!version) {
                return unexpected(version.error());
            }
            if (*version != Tls13) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "supported_versions selects a version not offered"});
            }
            if (sh->session_id.size() != _s->session_id_size || std::memcmp(sh->session_id.data(), _s->session_id, _s->session_id_size) != 0) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "legacy_session_id_echo is not the session id sent"});
            }
            if (!_offers(_settings.ciphers, sh->cipher_suite) || !known(Cipher(sh->cipher_suite))) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a cipher suite not offered"});
            }
            if (auto v = validate_extensions(HandshakeType::server_hello, retry, sh->extensions, _offered); !v) {
                return unexpected(v.error());
            }
            if (sh->extensions.has(ExtensionType::pre_shared_key)) {
                return unexpected(Alert{AlertDescription::unsupported_extension, 0, "pre_shared_key where none was offered"});
            }
            const Cipher cipher = Cipher(sh->cipher_suite);
            if (retry) {
                return _retry(message, *sh, cipher);
            }
            if (_retried && cipher != _result.cipher) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a ServerHello with another cipher suite than the HelloRetryRequest's"});
            }
            auto ks = sh->extensions.find(ExtensionType::key_share);
            if (!ks) {
                return unexpected(Alert{AlertDescription::missing_extension, 0, "a ServerHello without key_share"});
            }
            auto share = read_key_share_selected(*ks);
            if (!share) {
                return unexpected(share.error());
            }
            if (_retried && _retry_group != 0 && share->group != _retry_group) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a ServerHello's group is not the HelloRetryRequest's"});
            }
            if (!_s->shares.has(Group(share->group))) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a key share of a group the client did not send"});
            }
            auto shared = _s->shares.shared(Group(share->group), share->key);
            if (!shared) {
                return unexpected(shared.error());
            }
            _s->shares.clear();
            _choose_hash(hash_of(cipher));
            _hash_update(message);
            _result.cipher = cipher;
            _result.group = Group(share->group);
            _result.retried = _retried;
            uint8_t hello_hash[MaxHashSize];
            _transcript().value_to(hello_hash);
            _s->schedule.emplace(_hash);
            _s->schedule->handshake(shared->view(), bytes_of(hello_hash, hash_size(_hash)));
            _push_install(Action::Kind::install_read, Epoch::handshake, _s->schedule->server_handshake_traffic);
            _state = State::wait_encrypted_extensions;
            return {};
        }

        expected<void, Alert> _retry(const Bytes& message, const ServerHello& hrr, Cipher cipher) {
            if (_retried) {
                return unexpected(Alert{AlertDescription::unexpected_message, 0, "a second HelloRetryRequest"});
            }
            _retried = true;
            _result.cipher = cipher;
            auto ks = hrr.extensions.find(ExtensionType::key_share);
            auto cookie = hrr.extensions.find(ExtensionType::cookie);
            if (!ks && !cookie) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a HelloRetryRequest that changes nothing"});
            }
            if (ks) {
                auto g = read_key_share_retry(*ks);
                if (!g) {
                    return unexpected(g.error());
                }
                if (!_offers(_settings.groups, *g) || !supported(Group(*g)) || _s->shares.has(Group(*g))) {
                    return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a HelloRetryRequest for a group not offered, or already shared"});
                }
                _retry_group = *g;
            }
            if (cookie) {
                auto c = read_cookie(*cookie);
                if (!c) {
                    return unexpected(c.error());
                }
                auto p = reinterpret_cast<const byte*>(c->data());
                _s->cookie.assign(p, p + c->size());
            }
            // §4.4.1: the first ClientHello becomes message_hash of it
            _choose_hash(hash_of(cipher));
            uint8_t ch1[MaxHashSize];
            _transcript().value_to(ch1);
            Transcript fresh(_hash);
            std::vector<byte> m;
            Builder w(m);
            write_message_hash(w, bytes_of(ch1, hash_size(_hash)));
            fresh.update(bytes_of(m.data(), m.size()));
            fresh.update(message);
            _transcript() = std::move(fresh);
            // the second ClientHello: the share asked for (or the same
            // shares when only a cookie is asked for), the cookie
            if (ks) {
                _s->shares.clear();
                _s->shares.add(Group(_retry_group), _entropy);
            } else {
                _retry_group = 0;
            }
            if (_settings.compatibility_mode) {
                _push_ccs();
            }
            _send_hello();
            _state = State::wait_server_hello;
            return {};
        }

        expected<void, Alert> _encrypted_extensions(const Bytes& message, const Bytes& body) {
            auto x = read_encrypted_extensions(body);
            if (!x) {
                return unexpected(x.error());
            }
            if (auto v = validate_extensions(HandshakeType::encrypted_extensions, false, *x, _offered); !v) {
                return unexpected(v.error());
            }
            if (auto sn = x->find(ExtensionType::server_name)) {
                if (auto e = read_empty(*sn); !e) {
                    return unexpected(e.error());
                }
            }
            if (auto p = x->find(ExtensionType::application_layer_protocol_negotiation)) {
                auto one = read_protocol_selected(*p);
                if (!one) {
                    return unexpected(one.error());
                }
                bool ours = false;
                for (const string& mine : _settings.alpn) {
                    auto v = mine.view();
                    ours |= v.size() == one->size() && std::memcmp(v.data(), one->data(), v.size()) == 0;
                }
                if (!ours) {
                    return unexpected(Alert{AlertDescription::illegal_parameter, 0, "ALPN selects a protocol not offered"});
                }
                _result.alpn = string(std::string_view(reinterpret_cast<const char*>(one->data()), one->size()));
            }
            if (auto l = x->find(ExtensionType::record_size_limit)) {
                auto v = read_record_size_limit(*l);
                if (!v) {
                    return unexpected(v.error());
                }
                _result.record_size_limit = *v;
            }
            if (auto g = x->find(ExtensionType::supported_groups)) {
                if (auto r = read_groups(*g); !r) {   // the server's preference: read for its syntax
                    return unexpected(r.error());
                }
            }
            _hash_update(message);
            _state = State::wait_certificate_or_request;
            return {};
        }

        expected<void, Alert> _certificate_request(const Bytes& message, const Bytes& body) {
            auto cr = read_certificate_request(body);
            if (!cr) {
                return unexpected(cr.error());
            }
            if (!cr->context.empty()) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a CertificateRequest of the handshake with a context"});
            }
            if (auto v = validate_extensions(HandshakeType::certificate_request, false, cr->extensions, _offered); !v) {
                return unexpected(v.error());
            }
            _certificate_requested = true;
            _hash_update(message);
            _state = State::wait_certificate;
            return {};
        }

        expected<void, Alert> _certificate(const Bytes& message, const Bytes& body) {
            auto c = read_certificate(body);
            if (!c) {
                return unexpected(c.error());
            }
            if (!c->context.empty()) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "the server's Certificate with a context"});
            }
            if (c->count == 0) {
                return unexpected(Alert{AlertDescription::decode_error, 0, "the server's Certificate is empty"});
            }
            crypto::x509::chain chain;
            for (auto entry : *c) {
                if (auto v = validate_extensions(HandshakeType::certificate, false, entry.extensions, _offered); !v) {
                    return unexpected(v.error());
                }
                auto cert = crypto::x509::certificate::parse(entry.der);
                if (!cert) {
                    return unexpected(Alert{AlertDescription::bad_certificate, 0, "a certificate that does not parse"});
                }
                chain.push_back(std::move(*cert));
            }
            if (!_settings.insecure_skip_verify) {
                crypto::x509::verify_options o;
                if (_settings.roots) {
                    o.roots = *_settings.roots;
                }
                for (size_t i = 1; i < chain.size(); ++i) {
                    o.intermediates.add(chain[i]);
                }
                // an address is verified by its bytes (the certificate's
                // IP addresses), a name by the DNS names
                array<uint8_t, 16> address{};
                if (is_ip_literal(_settings.server_name)) {
                    auto ip = ip_address::parse(_settings.server_name);
                    if (!ip) {
                        return unexpected(Alert{AlertDescription::internal_error, 0, "a server name that is no address nor host name"});
                    }
                    address = ip->bytes();
                    const bool v4 = ip->is_v4();
                    o.ip = slice<const byte>(reinterpret_cast<const byte*>(address.data()) + (v4 ? 12 : 0), v4 ? 4 : 16);
                } else {
                    o.dns_name = _settings.server_name;
                }
                o.time = _clock();
                auto v = chain[0].verify(o);
                if (!v) {
                    _verify_reason = v.error().reason();
                    return unexpected(_verification_alert(v.error()));
                }
            }
            _result.peer_certificates = chain;
            _hash_update(message);
            _state = State::wait_certificate_verify;
            return {};
        }

        // As Go: an unknown authority unknown_ca, an expired certificate
        // certificate_expired, the rest bad_certificate
        static Alert _verification_alert(const crypto::error& e) {
            using crypto::x509::reason;
            switch (e.reason()) {
            case reason::unknown_authority:
                return Alert{AlertDescription::unknown_ca, 0, "the server's certificate is signed by an unknown authority"};
            case reason::expired:
            case reason::not_yet_valid:
                return Alert{AlertDescription::certificate_expired, 0, "the server's certificate is not valid at this time"};
            case reason::unsupported_algorithm:
                return Alert{AlertDescription::unsupported_certificate, 0, "the server's certificate is of an algorithm not supported"};
            default:
                return Alert{AlertDescription::bad_certificate, 0, "the server's certificate does not verify"};
            }
        }

        expected<void, Alert> _certificate_verify(const Bytes& message, const Bytes& body) {
            auto cv = read_certificate_verify(body);
            if (!cv) {
                return unexpected(cv.error());
            }
            if (!_offers(_settings.schemes, cv->scheme)) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "CertificateVerify with a scheme not offered"});
            }
            uint8_t h[MaxHashSize];
            _transcript().value_to(h);
            auto content = certificate_verify_content(true, bytes_of(h, hash_size(_hash)));
            if (auto v = verify(cv->scheme, _result.peer_certificates[0].public_key(), content.view(), cv->signature); !v) {
                return unexpected(v.error());
            }
            _hash_update(message);
            _state = State::wait_finished;
            return {};
        }

        expected<void, Alert> _finished(const Bytes& message, const Bytes& body) {
            const size_t n = hash_size(_hash);
            auto f = read_finished(body, n);
            if (!f) {
                return unexpected(f.error());
            }
            uint8_t h[MaxHashSize], expected_data[MaxHashSize];
            _transcript().value_to(h);
            verify_data(_hash, expected_data, _s->schedule->server_handshake_traffic, bytes_of(h, n));
            const bool ok = crypto::constant_time::equal(bytes_of(expected_data, n), *f);
            crypto::detail::secure_zero(expected_data, sizeof expected_data);
            if (!ok) {
                return unexpected(Alert{AlertDescription::decrypt_error, 0, "the server's Finished does not verify"});
            }
            _hash_update(message);
            _transcript().value_to(h);
            KeySchedule& k = *_s->schedule;
            k.application(bytes_of(h, n));
            _push_install(Action::Kind::install_read, Epoch::application, k.server_application_traffic);
            // the client's second flight
            if (_settings.compatibility_mode && !_ccs_sent) {
                _push_ccs();
            }
            _push_install(Action::Kind::install_write, Epoch::handshake, k.client_handshake_traffic);
            auto& out = _s->step.out;
            if (_certificate_requested) {
                size_t at = out.size();
                Builder w(out);
                write_certificate(w, Bytes(), std::vector<Bytes>());   // none (mTLS after v1)
                _hash_update(bytes_of(out.data() + at, out.size() - at));
                _push_send(Epoch::handshake, at, out.size() - at);
                _transcript().value_to(h);
            }
            uint8_t mine[MaxHashSize];
            verify_data(_hash, mine, k.client_handshake_traffic, bytes_of(h, n));
            size_t at = out.size();
            Builder w(out);
            write_finished(w, bytes_of(mine, n));
            crypto::detail::secure_zero(mine, sizeof mine);
            _push_send(Epoch::handshake, at, out.size() - at);
            _push_install(Action::Kind::install_write, Epoch::application, k.client_application_traffic);
            _result.server_name = _settings.server_name;
            _s->step.actions.push_back(Action{Action::Kind::established});
            _wipe();
            _state = State::connected;
            return {};
        }

        // After the handshake: NewSessionTicket passed over, KeyUpdate
        // answered (§4.6)
        expected<void, Alert> _after(HandshakeType type, const Bytes& body) {
            switch (type) {
            case HandshakeType::new_session_ticket: {
                auto t = read_new_session_ticket(body);
                if (!t) {
                    return unexpected(t.error());
                }
                if (auto v = validate_extensions(HandshakeType::new_session_ticket, false, t->extensions, _offered); !v) {
                    return unexpected(v.error());
                }
                return {};
            }
            case HandshakeType::key_update: {
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
            default:
                return unexpected(Alert{AlertDescription::unexpected_message, 0, "a handshake message after the handshake that v1 does not take"});
            }
        }

        // --- helpers ------------------------------------------------------------

        template<class R>
        static bool _offers(const R& list, uint16_t v) {
            for (uint16_t x : list) {
                if (x == v) {
                    return true;
                }
            }
            return false;
        }

        Transcript& _transcript() noexcept {
            return _hash == Hash::sha384 ? _s->t384 : _s->t256;
        }

        void _hash_update(const Bytes& m) {
            if (_hash_known) {
                _transcript().update(m);
            } else {
                _s->t256.update(m);
                _s->t384.update(m);
            }
        }

        void _choose_hash(Hash h) noexcept {
            if (!_hash_known) {
                _hash = h;
                _hash_known = true;
                // the other one no longer needed (both held nothing secret)
            }
        }

        void _push_send(Epoch e, size_t offset, size_t size) {
            Action a{Action::Kind::send, e};
            a.offset = offset;
            a.size = size;
            _s->step.actions.push_back(std::move(a));
        }

        void _push_ccs() {
            _s->step.actions.push_back(Action{Action::Kind::change_cipher_spec, Epoch::initial});
            _ccs_sent = true;
        }

        void _push_install(Action::Kind kind, Epoch e, const Secret& s) {
            Action a{kind, e, _result.cipher};
            std::memcpy(a.secret.bytes, s.bytes, sizeof s.bytes);
            a.secret.size = s.size;
            _s->step.actions.push_back(std::move(a));
        }

        const Step& _fail(const Alert& a) {
            _s->step.clear();
            Action x{Action::Kind::alert};
            x.alert = a.description;
            x.what = a.what;
            _s->step.actions.push_back(std::move(x));
            _state = State::failed;
            _wipe();
            return _s->step;
        }

        // The secrets the handshake no longer needs: the private keys and
        // the schedule's (the traffic secrets the step carries stay until
        // the next call)
        void _wipe() noexcept {
            _s->shares.clear();
            if (_s->schedule) {
                _s->schedule->finish_handshake();
                _s->schedule.reset();
            }
        }
    };
}
