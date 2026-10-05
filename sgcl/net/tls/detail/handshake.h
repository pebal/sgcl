//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "key_share.h"
#include "messages.h"
#include "prf.h"
#include "record.h"
#include "schedule.h"
#include "session.h"
#include "signature.h"
#include "../../../core/detail/bytes.h"
#include "../../../crypto/constant_time.h"
#include "../../../crypto/x509.h"
#include "../../ip.h"
#include "../../../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

// The client's side of the TLS 1.3 handshake (RFC 8446 §2, §4) as a
// machine without input or output of its own: start() gives the first
// flight, feed() each whole handshake message the assembler gives, and
// each call answers with a Step, the actions the connection carries out
// in their order — bytes to send under an epoch's keys, a compatibility
// change_cipher_spec, keys to install or update in a direction, the
// handshake established, an alert to send before the connection ends.
//
// What it does: ALPN negotiated (RFC 7301); the server's chain verified
// against the roots and the server name unless insecure_skip_verify; one
// HelloRetryRequest at most (§4.1.4); a CertificateRequest answered with
// the first of the client's identities whose key signs a scheme the server
// takes and whose chain is issued by one of the authorities it names (when
// it names some), else with an empty Certificate (the server decides);
// resumption (§2.2, psk_dhe_ke only, no 0-RTT): the session given offered
// with its binder and its obfuscated age, the server free to refuse it (a
// full handshake then), and each NewSessionTicket made a session for the
// cache (an action) when the settings keep them. The transcript is hashed
// with SHA-256 and SHA-384 side by side until the ServerHello names the
// suite, the other dropped then.
//
// TLS 1.2 (RFC 5246, for servers without 1.3; the settings say whether it
// is offered): the ClientHello offers 1.3 and 1.2 together, with 1.2's
// extensions (extended_master_secret, ec_point_formats beside
// renegotiation_info); a ServerHello without supported_versions goes on
// in 1.2, refused when its random carries RFC 8446 §4.1.3's downgrade
// sentinel and 1.3 was offered. ECDHE alone (X25519, P-256, P-384), the
// AEAD suites alone, the ServerKeyExchange's signature checked under the
// verified leaf, the extended master secret required (RFC 7627: a server
// without it is handshake_failure), Finished both ways; a
// CertificateRequest answered as in 1.3, the CertificateVerify signed over
// the handshake's messages; no renegotiation (a HelloRequest is passed
// over, as §7.4.1.1 allows). The server's change_cipher_spec, which 1.2
// acts on, comes as change_cipher_spec(): its read keys installed then.
//
// Resumption in TLS 1.2 (RFC 5246 §7.3, RFC 5077), when the settings keep
// sessions: a session of 1.2 offered by its ticket in the SessionTicket
// extension (a fresh random session id beside it, whose echo says the
// server took the ticket, RFC 5077 §3.4) or by its session id; without a
// ticket to offer, the extension empty, asking for one (when the settings
// ask for tickets). A ServerHello echoing the session id sent is the
// abbreviated handshake (§7.3 figure 2): the session's suite (else
// illegal_parameter) and the extended master secret (RFC 7627 §5.3: every
// session is one of it; a server without it is handshake_failure), the
// keys of the session's master secret and the new randoms, the server's
// NewSessionTicket when its ServerHello announced one, its
// change_cipher_spec and Finished, then the client's; the peer's chain is
// the session's. Any other ServerHello is a full handshake, the session
// offered dropped. A NewSessionTicket (after the client's Finished in a
// full handshake, before the server's change_cipher_spec) only where the
// ServerHello announced it; an empty ticket is none. At the end of either
// handshake the session is an action (new_ticket, `tls12`) for the cache:
// the master secret, the ticket (the new one, else the one resumed), the
// server's session id, a lifetime of the ticket's hint capped at seven
// days (RFC 8446's limit) or of a day where none is given (RFC 5246 §F.1.4);
// a session of neither ticket nor session id is not kept.
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
    inline constexpr uint16_t SessionTicket = 0x0023;       // RFC 5077: a 1.2 session's ticket, or empty
    inline constexpr uint32_t MaxLifetime12 = 604800;       // a 1.2 session's lifetime at most (RFC 8446's seven days)
    inline constexpr uint32_t DefaultLifetime12 = 86400;    // where the server gives none (RFC 5246 §F.1.4: a day)

    // The time a certificate's validity is judged at
    struct Clock {
        time::datetime (*now)(void* context) = &Clock::system_now;
        void* context = nullptr;

        SGCL_INLINE_HOT time::datetime operator()() const noexcept {
            return now(context);
        }

        SGCL_INLINE_HOT static time::datetime system_now(void*) noexcept {
            return time::now();
        }
    };

    // The OCSP response a server staples for an identity's leaf (RFC 6066,
    // RFC 8446 §4.4.2.1): set by the program or fetched and refreshed by the
    // server (tls::config::ocsp_stapling), read by each handshake. Inside an
    // identity's state; its bytes are unmanaged (no tracked word), under a
    // lock of their own
    struct StapleHolder {
        mutable std::mutex lock;
        std::vector<byte> der;          // empty: none
        int64_t this_update = 0;        // Unix seconds
        int64_t next_update = 0;        // 0: the response has none
        int64_t refresh_at = 0;         // when the server fetches the next one (ocsp_stapling)
        int64_t retry_at = 0;           // after a fetch that failed, the next try
        bool fetching = false;

        // The response to staple at `now`: none past its nextUpdate
        std::vector<byte> current(int64_t now) const {
            std::lock_guard<std::mutex> g(lock);
            if (der.empty() || (next_update && now > next_update)) {
                return {};
            }
            return der;
        }

        void set(const std::vector<byte>& d, int64_t this_u, int64_t next_u) {
            std::lock_guard<std::mutex> g(lock);
            der = d;
            this_update = this_u;
            next_update = next_u;
            // refreshed halfway through its validity, or after an hour when
            // it has no nextUpdate
            refresh_at = next_u ? this_u + (next_u - this_u) / 2 : this_u + 3600;
            retry_at = 0;
        }

        void clear() noexcept {
            std::lock_guard<std::mutex> g(lock);
            der.clear();
            this_update = next_update = refresh_at = retry_at = 0;
        }
    };

    // A certificate chain and a way to sign with its key, a server's or a
    // client's (mTLS): the key itself is not here (tls::identity keeps it,
    // unmanaged)
    struct ServerIdentity {
        std::vector<std::vector<byte>> chain;           // the DER of each certificate, the leaf first
        optional<crypto::x509::certificate> leaf;        // chain[0] read: the names SNI is matched against
        std::vector<std::vector<byte>> issuers;          // the DER of each certificate's issuer: a CertificateRequest's authorities are matched against them
        std::vector<uint16_t> schemes;                   // the schemes the key signs, the server's preference first
        void (*sign)(const void* key, uint16_t scheme, const Bytes& content, Builder& out) = nullptr;
        const void* key = nullptr;
        const StapleHolder* staple = nullptr;            // the leaf's OCSP response, in the identity's state; null: none
    };

    // An identity of a certificate chain (DER, the leaf first) and a key of
    // the module; the key must outlive the identity
    template<class K>
    ServerIdentity identity_of(const std::vector<std::vector<byte>>& chain, const K& key) noexcept {
        ServerIdentity id;
        id.chain = chain;
        for (size_t i = 0; i < chain.size(); ++i) {
            auto c = crypto::x509::certificate::parse(bytes_of(chain[i].data(), chain[i].size()));
            if (!c) {
                continue;
            }
            auto issuer = c->raw_issuer();
            id.issuers.emplace_back(issuer.data(), issuer.data() + issuer.size());
            if (i == 0) {
                id.leaf = std::move(*c);
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
        vector<ServerIdentity> identities;                    // the client's certificates (mTLS), in order of preference
        tracked_ptr<Session> session;                         // a session to resume: offered once (its PSK zeroed when taken)
        bool resumption = false;                              // NewSessionTickets made sessions (new_ticket actions)
        bool tls13 = true;                                    // TLS 1.3 offered
        bool tls12 = false;                                   // TLS 1.2 offered (the 1.2 suites of `ciphers` with it)
        bool tickets12 = false;                               // TLS 1.2's tickets asked for (RFC 5077), with resumption
        bool status_request = false;                          // RFC 6066 status_request: an OCSP staple asked for
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
            new_ticket,             // the client's: a session to keep, the PSK in `secret`, the ticket the bytes
                                    // at offset and size, `lifetime`, `age_add`, received at `issued_ms`; `tls12`: a
                                    // session of 1.2, the master secret in `secret`, the ticket possibly empty, the
                                    // server's session id in `session_id`
        };

        // (a constructor, not an aggregate: the actions name only what
        // they carry, the rest its default)
        SGCL_INLINE_HOT explicit Action(Kind k, Epoch e = Epoch::initial, Cipher c = Cipher::aes_128_gcm_sha256) noexcept
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
        bool tls12 = false;         // install_*: TLS 1.2's keys, `secret` the AEAD's key and its IV (key_block12);
                                    // new_ticket: a session of 1.2
        uint32_t lifetime = 0;      // new_ticket: seconds
        uint32_t age_add = 0;
        int64_t issued_ms = 0;      // new_ticket: when it came, Unix milliseconds
        uint8_t session_id_size = 0;    // new_ticket of 1.2: the server's session id, the curve of the session
        uint8_t session_id[32] = {};
        uint16_t group = 0;
    };

    // The actions of one call, valid until the next call
    struct Step {
        std::vector<Action> actions;
        std::vector<byte> out;      // the bytes of the send actions

        SGCL_INLINE_HOT Bytes bytes(const Action& a) const noexcept {
            return Bytes(out.data() + a.offset, a.size);
        }

        SGCL_INLINE_HOT void clear() noexcept {
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
        bool resumed = false;                       // the server took the session offered
        bool certificate_sent = false;              // a CertificateRequest answered with a chain
        uint16_t version = Tls13;                   // the version negotiated
        crypto::x509::chain verified_chain;         // the chain verification built, the leaf to a root (empty: not verified)
        std::vector<byte> ocsp_staple;              // the leaf's OCSP response the server stapled (RFC 6066, RFC 8446 §4.4.2.1); empty: none
    };

    // The alert of a chain that did not verify, as Go sends it: an unknown
    // authority unknown_ca, a certificate out of its time
    // certificate_expired, an algorithm not supported
    // unsupported_certificate, the rest bad_certificate (`server`: whose
    // chain, for the diagnostic)
    inline Alert verification_alert(const crypto::error& e, bool server) noexcept {
        using crypto::x509::reason;
        switch (e.reason()) {
        case reason::unknown_authority:
            return Alert{AlertDescription::unknown_ca, 0, server ? "the server's certificate is signed by an unknown authority" : "the client's certificate is signed by an unknown authority"};
        case reason::expired:
        case reason::not_yet_valid:
            return Alert{AlertDescription::certificate_expired, 0, server ? "the server's certificate is not valid at this time" : "the client's certificate is not valid at this time"};
        case reason::unsupported_algorithm:
            return Alert{AlertDescription::unsupported_certificate, 0, server ? "the server's certificate is of an algorithm not supported" : "the client's certificate is of an algorithm not supported"};
        default:
            return Alert{AlertDescription::bad_certificate, 0, server ? "the server's certificate does not verify" : "the client's certificate does not verify"};
        }
    }

    class ClientHandshake {
    public:
        SGCL_INLINE_HOT ClientHandshake(const ClientSettings& settings, const Entropy& entropy = Entropy(), const Clock& clock = Clock()) noexcept
        : _settings(settings), _entropy(entropy), _clock(clock), _s(std::make_unique<Secrets>()) {
        }

        ClientHandshake(const ClientHandshake&) = delete;
        ClientHandshake& operator=(const ClientHandshake&) = delete;

        // The first flight: the ClientHello (not noexcept: settings whose
        // key_shares name a group twice, or more than four, are
        // ClientShares::add's std::logic_error)
        SGCL_INLINE_HOT const Step& start() {
            _s->step.clear();
            if (_state != State::start) {
                return _fail(Alert{AlertDescription::internal_error, 0, "start() twice"});
            }
            if (auto r = _start(); !r) {
                return _fail(r.error());
            }
            return _s->step;
        }

        // A whole handshake message from the server (header included); not
        // noexcept: a chain verified against the system's roots reads them
        // through io::read_file
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

        // An alert record from the server: the handshake is over
        SGCL_INLINE_HOT void on_record_alert(const Alert& a) noexcept {
            _peer_alert = a;
            _state = State::failed;
            _wipe();
        }

        // A failure of the connection's own while the handshake runs (a
        // record that does not frame or open): the handshake is over, and
        // the step is the one a failure of the machine makes, its alert
        // under the keys the server reads with (_fail)
        const Step& fail(const Alert& a) noexcept {
            if (_state == State::failed) {
                _s->step.clear();
                return _s->step;
            }
            return _fail(a);
        }

        // A change_cipher_spec record from the server: TLS 1.2's after the
        // client's Finished, the server's write keys installed for reading;
        // in TLS 1.3 compatibility mode's, nothing; anywhere else in 1.2,
        // unexpected_message
        const Step& change_cipher_spec() noexcept {
            _s->step.clear();
            if (_state == State::wait12_ccs) {
                _push_install12(Action::Kind::install_read, _s->server_keys);
                _s->server_keys.wipe();
                _state = State::wait12_finished;
                return _s->step;
            }
            if (_tls12 && _state != State::failed) {
                return _fail(Alert{AlertDescription::unexpected_message, 0, "a change_cipher_spec out of order"});
            }
            return _s->step;
        }

        SGCL_INLINE_HOT bool established() const noexcept {
            return _state == State::connected;
        }

        // Whether a ServerHello was taken (an alert before it is one of a
        // server that may not speak the versions offered)
        SGCL_INLINE_HOT bool hello_received() const noexcept {
            return _hello_seen;
        }

        SGCL_INLINE_HOT bool failed() const noexcept {
            return _state == State::failed;
        }

        SGCL_INLINE_HOT const ClientResult& result() const noexcept {
            return _result;
        }

        SGCL_INLINE_HOT const optional<Alert>& peer_alert() const noexcept {
            return _peer_alert;
        }

        // Why the server's chain did not verify, when that ended the
        // handshake (reason::none otherwise)
        SGCL_INLINE_HOT crypto::x509::reason verify_reason() const noexcept {
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
            // TLS 1.2 (after `failed`: _fail's range of 1.3's handshake keys is not theirs)
            wait12_certificate,
            wait12_key_exchange,
            wait12_request_or_done,
            wait12_done,
            wait12_ticket,
            wait12_ccs,
            wait12_finished,
            wait12_status_or_key_exchange,   // after the Certificate of a server that answered status_request
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
            Secret psk;                       // the session's, while it may be offered
            Secret resumption_master;         // after the handshake, when NewSessionTickets are kept
            // TLS 1.2
            uint8_t server_random[32] = {};
            SharedSecret pre_master;          // the ECDHE's, from the ServerKeyExchange to the master secret
            Secret master;                    // the extended master secret, to the server's Finished
            Secret client_keys, server_keys;  // each direction's AEAD key and IV, until installed
            std::vector<byte> messages;       // the handshake's messages, for a 1.2 CertificateVerify (not secret)
            std::vector<byte> ticket12;       // a 1.2 NewSessionTicket's ticket, to the end of the handshake
            uint8_t server_session_id[32] = {};   // a full 1.2 handshake's: the session's name for resumption
            size_t server_session_id_size = 0;
            Step step;

            SGCL_INLINE_HOT ~Secrets() {
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
        bool _psk_offered = false;          // the last ClientHello has a pre_shared_key
        bool _tls12 = false;                // the server chose TLS 1.2
        bool _hello_seen = false;           // a ServerHello was taken
        bool _keep_messages = false;        // the messages kept whole (1.2 offered, until its CertificateVerify)
        bool _offer12 = false;              // a 1.2 session offered, its master secret in _s->master
        bool _ticket12_offered = false;     // by its ticket (else by its session id)
        bool _ticket_expected = false;      // the 1.2 ServerHello announced a NewSessionTicket
        uint32_t _ticket_hint = 0;          // its lifetime hint
        bool _status12 = false;             // a 1.2 ServerHello answered status_request: a CertificateStatus may follow
        std::vector<uint8_t> _cert_types;   // a 1.2 CertificateRequest's certificate_types
        Hash _psk_hash = Hash::sha256;
        std::vector<uint16_t> _peer_schemes;                // a CertificateRequest's signature_algorithms
        std::vector<std::vector<byte>> _authorities;        // its certificate_authorities
        uint16_t _retry_group = 0;
        ClientResult _result;
        optional<Alert> _peer_alert;
        crypto::x509::reason _verify_reason = crypto::x509::reason::none;

        // --- the ClientHello ---------------------------------------------------

        expected<void, Alert> _start() {
            if (_settings.server_name.empty() && !_settings.insecure_skip_verify) {
                return unexpected(Alert{AlertDescription::internal_error, 0, "a server name, or insecure_skip_verify, is needed to check the server"});
            }
            if (_settings.ciphers.empty() || _settings.groups.empty() || (_settings.tls13 && _settings.key_shares.empty()) || _settings.schemes.empty()) {
                return unexpected(Alert{AlertDescription::internal_error, 0, "an empty list of ciphers, groups, shares or schemes"});
            }
            if (!_settings.tls13 && !_settings.tls12) {
                return unexpected(Alert{AlertDescription::internal_error, 0, "no version offered"});
            }
            _keep_messages = _settings.tls12;
            _entropy(_s->random, 32);
            if (_settings.compatibility_mode) {
                _entropy(_s->session_id, 32);
                _s->session_id_size = 32;
            }
            for (uint16_t g : _settings.tls13 ? _settings.key_shares : vector<uint16_t>()) {
                if (!supported(Group(g))) {
                    return unexpected(Alert{AlertDescription::internal_error, 0, "a key share of a group v1 does not have"});
                }
                _s->shares.add(Group(g), _entropy);
            }
            _take_session();
            _send_hello();
            _state = State::wait_server_hello;
            return {};
        }

        static bool is_ip_literal(const string& name) noexcept {
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

        void _send_hello() noexcept {
            auto& out = _s->step.out;
            size_t start = out.size();
            // the session offered when its PSK's hash may still be the
            // suite's: before the ServerHello one offered has it, after a
            // HelloRetryRequest the suite chosen must (§4.1.4, §4.2.11)
            _psk_offered = !_s->psk.empty() && (!_hash_known || _hash == _psk_hash);
            const size_t binder_size = hash_size(_psk_hash);
            const size_t psk_extension = _psk_offered ? 4 + 2 + 2 + _settings.session->ticket.size() + 4 + 2 + 1 + binder_size : 0;
            Builder w(out);
            uint64_t offered = 0;
            auto mark = [&](ExtensionType t) noexcept {
                offered |= bit_of(t);
            };
            write_client_hello(w, bytes_of(_s->random, 32), bytes_of(_s->session_id, _s->session_id_size), _settings.ciphers, [&](Builder& w) noexcept {
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
                if (_settings.session_ticket_extension || (_settings.tls12 && _settings.tickets12)) {
                    // RFC 5077 §3.2: the ticket offered, or empty to ask for one
                    auto e = w.extension(SessionTicket);
                    if (_ticket12_offered) {
                        w.bytes(_settings.session->ticket.data(), _settings.session->ticket.size());
                    }
                    offered |= bit_of(SessionTicket);
                }
                if (_settings.tls12) {
                    {
                        auto e = w.extension(ExtensionType::ec_point_formats);
                        auto list = w.block8();
                        w.u8(0);   // uncompressed
                        mark(ExtensionType::ec_point_formats);
                    }
                    auto e = w.extension(ExtensionType::extended_master_secret);
                    mark(ExtensionType::extended_master_secret);
                }
                if (_settings.tls13) {
                    auto e = w.extension(ExtensionType::key_share);
                    auto list = w.block16();
                    for (size_t i = 0; i < _s->shares.size(); ++i) {
                        w.u16(uint16_t(_s->shares.group(i)));
                        auto k = w.block16();
                        w.bytes(_s->shares.public_share(i));
                    }
                    mark(ExtensionType::key_share);
                }
                if (_settings.tls13) {
                    auto e = w.extension(ExtensionType::supported_versions);
                    vector<uint16_t> versions = {Tls13};
                    if (_settings.tls12) {
                        versions.push_back(Tls12);
                    }
                    write_versions_offered(w, versions);
                    mark(ExtensionType::supported_versions);
                }
                {
                    auto e = w.extension(ExtensionType::signature_algorithms);
                    write_signature_schemes(w, _settings.schemes);
                    mark(ExtensionType::signature_algorithms);
                }
                if (_settings.status_request) {
                    auto e = w.extension(ExtensionType::status_request);
                    write_status_request(w);
                    mark(ExtensionType::status_request);
                }
                if (!_s->cookie.empty()) {
                    auto e = w.extension(ExtensionType::cookie);
                    write_cookie(w, bytes_of(_s->cookie.data(), _s->cookie.size()));
                    mark(ExtensionType::cookie);
                }
                if (_settings.tls13) {
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
                    // (and the pre_shared_key after it)
                    size_t length = out.size() - start + psk_extension;
                    if (length >= 256 && length < 512) {
                        size_t pad = 512 - length;
                        pad = pad >= 5 ? pad - 4 : 1;
                        auto e = w.extension(Padding);
                        for (size_t i = 0; i < pad; ++i) {
                            w.u8(0);
                        }
                    }
                }
                if (_psk_offered) {
                    // last (§4.2.11): the ticket, its obfuscated age, a
                    // binder filled in once the message is whole
                    const Session& session = *_settings.session;
                    const int64_t age = _clock().unix_milli() - session.received_ms;
                    auto e = w.extension(ExtensionType::pre_shared_key);
                    {
                        auto ids = w.block16();
                        {
                            auto id = w.block16();
                            w.bytes(session.ticket.data(), session.ticket.size());
                        }
                        w.u32(uint32_t(uint64_t(age) + session.age_add));
                    }
                    auto binders = w.block16();
                    auto one = w.block8();
                    for (size_t i = 0; i < binder_size; ++i) {
                        w.u8(0);
                    }
                    mark(ExtensionType::pre_shared_key);
                }
            });
            if (_psk_offered) {
                // the binder over the transcript so far and the hello up
                // to the binders' list (§4.2.11.2)
                const size_t truncated = out.size() - start - (2 + 1 + binder_size);
                Transcript t = _hash_known ? _transcript() : _psk_hash == Hash::sha384 ? _s->t384 : _s->t256;
                t.update(bytes_of(out.data() + start, truncated));
                uint8_t h[MaxHashSize];
                t.value_to(h);
                KeySchedule(_psk_hash, _s->psk).binder(reinterpret_cast<uint8_t*>(out.data() + out.size() - binder_size), bytes_of(h, binder_size));
            }
            _offered = offered;
            _hash_update(bytes_of(out.data() + start, out.size() - start));
            _push_send(Epoch::initial, start, out.size() - start);
        }

        // The session of the settings, of either version (offered once,
        // whatever comes of it: the session's secret zeroed)
        void _take_session() noexcept {
            Session* session = _settings.session.get();
            if (!session) {
                return;
            }
            if (session->version == Tls12) {
                _take_session12(*session);
            } else if (_settings.tls13) {
                _take_session13(*session);
            }
            session->wipe();
        }

        // A session of 1.3, when it is fresh and of a hash one of the
        // suites offered has: its PSK copied
        void _take_session13(const Session& session) noexcept {
            const bool fresh = known(Cipher(session.cipher)) && session.fresh(_clock().unix_milli());
            const Hash h = hash_of(Cipher(session.cipher));
            bool compatible = false;
            for (uint16_t c : _settings.ciphers) {
                compatible |= known(Cipher(c)) && hash_of(Cipher(c)) == h;
            }
            if (fresh && compatible && session.psk->size == hash_size(h) && session.ticket.size() >= 1 && session.ticket.size() <= MaxTicket) {
                std::memcpy(_s->psk.bytes, session.psk->bytes, sizeof _s->psk.bytes);
                _s->psk.size = session.psk->size;
                _psk_hash = h;
            }
        }

        // A session of 1.2: offered when 1.2 is, fresh, of a suite still
        // offered, its master secret whole; by its ticket when tickets are
        // asked for (a fresh random session id beside it, RFC 5077 §3.4),
        // else by its session id; its master secret copied
        void _take_session12(const Session& session) noexcept {
            const bool usable = _settings.tls12 && known12(Cipher(session.cipher)) && _offers(_settings.ciphers, session.cipher) && session.fresh(_clock().unix_milli())
                             && session.psk->size == 48;
            const bool by_ticket = _settings.tickets12 && !session.ticket.empty() && session.ticket.size() <= MaxTicket;
            if (!usable || (!by_ticket && session.session_id_size == 0) || session.session_id_size > 32) {
                return;
            }
            std::memcpy(_s->master.bytes, session.psk->bytes, sizeof _s->master.bytes);
            _s->master.size = session.psk->size;
            _offer12 = true;
            _ticket12_offered = by_ticket;
            if (by_ticket) {
                if (_s->session_id_size == 0) {
                    _entropy(_s->session_id, 32);
                    _s->session_id_size = 32;
                }
            } else {
                sgcl::detail::copy_bytes(_s->session_id, session.session_id, session.session_id_size);
                _s->session_id_size = session.session_id_size;
            }
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
                return _tls12 ? _after12(type) : _after(type, h->body);
            case State::wait12_certificate:
                if (type != HandshakeType::certificate) {
                    break;
                }
                return _certificate12(message, h->body);
            case State::wait12_status_or_key_exchange:
                if (type == HandshakeType::certificate_status) {
                    return _certificate_status12(message, h->body);
                }
                [[fallthrough]];
            case State::wait12_key_exchange:
                if (type != HandshakeType::server_key_exchange) {
                    break;
                }
                return _server_key_exchange12(message, h->body);
            case State::wait12_request_or_done:
                if (type == HandshakeType::certificate_request) {
                    return _certificate_request12(message, h->body);
                }
                [[fallthrough]];
            case State::wait12_done:
                if (type != HandshakeType::server_hello_done) {
                    break;
                }
                return _server_hello_done12(message, h->body);
            case State::wait12_ticket:
                if (type != HandshakeType::new_session_ticket) {
                    break;
                }
                return _new_session_ticket12(message, h->body);
            case State::wait12_finished:
                if (type != HandshakeType::finished) {
                    break;
                }
                return _finished12(message, h->body);
            case State::wait12_ccs:
            case State::start:
            case State::failed:
                break;
            }
            return unexpected(Alert{AlertDescription::unexpected_message, 0, "a handshake message out of order"});
        }

        expected<void, Alert> _server_hello(const Bytes& message, const Bytes& body) noexcept {
            auto sh = read_server_hello(body);
            if (!sh) {
                return unexpected(sh.error());
            }
            const bool retry = sh->is_retry();
            // supported_versions first: without it the server chose an
            // older version, which v1 does not speak (§4.2.1)
            auto versions = sh->extensions.find(ExtensionType::supported_versions);
            if (!versions) {
                if (!_settings.tls12) {
                    return unexpected(Alert{AlertDescription::protocol_version, 0, "a ServerHello of a version before TLS 1.3"});
                }
                return _server_hello12(message, *sh, retry);
            }
            auto version = read_version_selected(*versions);
            if (!version) {
                return unexpected(version.error());
            }
            if (*version != Tls13 || !_settings.tls13) {
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
            const Cipher cipher = Cipher(sh->cipher_suite);
            if (retry) {
                return _retry(message, *sh, cipher);
            }
            // the session taken (validate_extensions: only when offered)
            bool resumed = false;
            if (auto psk = sh->extensions.find(ExtensionType::pre_shared_key)) {
                auto selected = read_pre_shared_key_selected(*psk);
                if (!selected) {
                    return unexpected(selected.error());
                }
                if (*selected != 0) {
                    return unexpected(Alert{AlertDescription::illegal_parameter, 0, "pre_shared_key selects an identity not offered"});
                }
                if (hash_of(cipher) != _psk_hash) {
                    return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a cipher suite of another hash than the PSK's"});
                }
                resumed = true;
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
            _result.resumed = resumed;
            uint8_t hello_hash[MaxHashSize];
            _transcript().value_to(hello_hash);
            if (resumed) {
                _s->schedule.emplace(_hash, _s->psk);
                _result.peer_certificates = _settings.session->peer_certificates;
            } else {
                _s->schedule.emplace(_hash);
            }
            _s->psk.wipe();   // in the early secret now, or refused
            _keep_messages = false;
            std::vector<byte>().swap(_s->messages);
            _s->schedule->handshake(shared->view(), bytes_of(hello_hash, hash_size(_hash)));
            _push_install(Action::Kind::install_read, Epoch::handshake, _s->schedule->server_handshake_traffic);
            _hello_seen = true;
            _state = State::wait_encrypted_extensions;
            return {};
        }

        expected<void, Alert> _retry(const Bytes& message, const ServerHello& hrr, Cipher cipher) noexcept {
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

        expected<void, Alert> _encrypted_extensions(const Bytes& message, const Bytes& body) noexcept {
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
                if (auto a = _take_alpn(*p); !a) {
                    return a;
                }
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
            // a resumed handshake goes on to the server's Finished: no
            // certificate either way (§4.3.2)
            _state = _result.resumed ? State::wait_finished : State::wait_certificate_or_request;
            return {};
        }

        expected<void, Alert> _certificate_request(const Bytes& message, const Bytes& body) noexcept {
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
            // the schemes the server takes, the authorities it names (§4.3.2)
            auto schemes = read_signature_schemes(*cr->extensions.find(ExtensionType::signature_algorithms));
            if (!schemes) {
                return unexpected(schemes.error());
            }
            _peer_schemes.assign(schemes->begin(), schemes->end());
            if (auto ca = cr->extensions.find(ExtensionType::certificate_authorities)) {
                auto names = read_certificate_authorities(*ca);
                if (!names) {
                    return unexpected(names.error());
                }
                names->each([&](const Bytes& n) noexcept {
                    _authorities.emplace_back(n.data(), n.data() + n.size());
                });
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
                // the leaf's OCSP response (RFC 8446 §4.4.2.1); another
                // entry's is read for its syntax and passed over
                if (auto st = entry.extensions.find(ExtensionType::status_request)) {
                    auto ocsp = read_certificate_status(*st);
                    if (!ocsp) {
                        return unexpected(ocsp.error());
                    }
                    if (chain.empty()) {
                        _result.ocsp_staple.assign(ocsp->data(), ocsp->data() + ocsp->size());
                    }
                }
                chain.push_back(std::move(*cert));
            }
            if (auto v = _verify_chain(chain); !v) {
                return v;
            }
            _result.peer_certificates = chain;
            _hash_update(message);
            _state = State::wait_certificate_verify;
            return {};
        }

        expected<void, Alert> _certificate_verify(const Bytes& message, const Bytes& body) noexcept {
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

        expected<void, Alert> _finished(const Bytes& message, const Bytes& body) noexcept {
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
                // the first identity the request takes, else none (§4.4.2)
                uint16_t scheme = 0;
                const ServerIdentity* id = _client_identity(scheme);
                size_t at = out.size();
                Builder w(out);
                {
                    std::vector<Bytes> ders;   // lint-handles: ok views of unmanaged bytes (bytes_of): no owner, no word
                    if (id) {
                        for (auto& d : id->chain) {
                            ders.push_back(bytes_of(d.data(), d.size()));
                        }
                    }
                    write_certificate(w, Bytes(), ders);
                }
                _hash_update(bytes_of(out.data() + at, out.size() - at));
                if (id) {
                    _transcript().value_to(h);
                    auto content = certificate_verify_content(false, bytes_of(h, n));
                    size_t cv = out.size();
                    {
                        auto m = w.message(HandshakeType::certificate_verify);
                        w.u16(scheme);
                        auto sig = w.block16();
                        id->sign(id->key, scheme, content.view(), w);
                    }
                    _hash_update(bytes_of(out.data() + cv, out.size() - cv));
                    _result.certificate_sent = true;
                }
                _push_send(Epoch::handshake, at, out.size() - at);
                _transcript().value_to(h);
            }
            uint8_t mine[MaxHashSize];
            verify_data(_hash, mine, k.client_handshake_traffic, bytes_of(h, n));
            size_t at = out.size();
            Builder w(out);
            write_finished(w, bytes_of(mine, n));
            crypto::detail::secure_zero(mine, sizeof mine);
            if (_settings.resumption) {
                // resumption_master_secret, over the transcript through this Finished (§7.1)
                _hash_update(bytes_of(out.data() + at, out.size() - at));
                _transcript().value_to(h);
                k.resumption(_s->resumption_master, bytes_of(h, n));
                ZeroingProbe::on_secret(_s->resumption_master);
            }
            _push_send(Epoch::handshake, at, out.size() - at);
            _push_install(Action::Kind::install_write, Epoch::application, k.client_application_traffic);
            _result.server_name = _settings.server_name;
            _s->step.actions.push_back(Action{Action::Kind::established});
            _wipe();
            _state = State::connected;
            return {};
        }

        // After the handshake: a NewSessionTicket made a session (§4.6.1)
        // when they are kept (a lifetime of zero, or a ticket past
        // MaxTicket, passed over), KeyUpdate answered (§4.6.3)
        expected<void, Alert> _after(HandshakeType type, const Bytes& body) noexcept {
            switch (type) {
            case HandshakeType::new_session_ticket: {
                auto t = read_new_session_ticket(body);
                if (!t) {
                    return unexpected(t.error());
                }
                if (auto v = validate_extensions(HandshakeType::new_session_ticket, false, t->extensions, _offered); !v) {
                    return unexpected(v.error());
                }
                if (!_settings.resumption || _s->resumption_master.empty() || t->lifetime == 0 || t->ticket.size() > MaxTicket) {
                    return {};
                }
                Action a{Action::Kind::new_ticket, Epoch::application, _result.cipher};
                resumption_psk(_hash, a.secret, _s->resumption_master, t->nonce);
                ZeroingProbe::on_secret(a.secret);
                auto& out = _s->step.out;
                a.offset = out.size();
                a.size = t->ticket.size();
                out.insert(out.end(), t->ticket.data(), t->ticket.data() + t->ticket.size());
                a.lifetime = t->lifetime;
                a.age_add = t->age_add;
                a.issued_ms = _clock().unix_milli();
                _s->step.actions.push_back(std::move(a));
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

        // The protocol the server chose (RFC 7301): one offered
        expected<void, Alert> _take_alpn(const Bytes& body) noexcept {
            auto one = read_protocol_selected(body);
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
            return {};
        }

        // The server's chain verified against the roots for the server's name
        // or address, unless insecure_skip_verify
        expected<void, Alert> _verify_chain(const crypto::x509::chain& chain) {
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
                    return unexpected(verification_alert(v.error(), true));
                }
                _result.verified_chain = std::move(*v);
            }
            return {};
        }

        // --- TLS 1.2 (RFC 5246, RFC 7627, RFC 8422) ---------------------------------

        // A ServerHello without supported_versions: TLS 1.2. Not after a
        // HelloRetryRequest; the downgrade sentinel refused where 1.3 was
        // offered; a suite of 1.2 offered; every extension one the
        // ClientHello offered; extended_master_secret required. The session
        // id sent echoed: the session offered resumed, of its suite (an
        // echo where none was offered is illegal_parameter)
        expected<void, Alert> _server_hello12(const Bytes& message, const ServerHello& sh, bool retry) noexcept {
            if (retry || _retried) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a TLS 1.2 ServerHello with a HelloRetryRequest"});
            }
            if (_settings.tls13 && std::memcmp(sh.random.data() + 24, Downgrade12, 8) == 0) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a TLS 1.2 ServerHello of a server of TLS 1.3 (the downgrade sentinel, RFC 8446 §4.1.3)"});
            }
            const Cipher cipher = Cipher(sh.cipher_suite);
            if (!_offers(_settings.ciphers, sh.cipher_suite) || !known12(cipher)) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a TLS 1.2 cipher suite not offered"});
            }
            const bool echoed = _s->session_id_size && sh.session_id.size() == _s->session_id_size && std::memcmp(sh.session_id.data(), _s->session_id, _s->session_id_size) == 0;
            if (echoed && !_offer12) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a TLS 1.2 ServerHello resuming a session the client did not offer"});
            }
            if (echoed && sh.cipher_suite != _settings.session->cipher) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a TLS 1.2 session resumed with another cipher suite than its own"});
            }
            bool ems = false;
            for (auto e : sh.extensions) {
                switch (e.type) {
                case RenegotiationInfo:   // RFC 5746 §3.4: renegotiated_connection empty
                    if (e.body.size() != 1 || uint8_t(e.body[0]) != 0) {
                        return unexpected(Alert{AlertDescription::handshake_failure, 0, "renegotiation_info of a renegotiation"});
                    }
                    continue;
                case uint16_t(ExtensionType::extended_master_secret):
                    if (auto x = read_empty(e.body); !x) {
                        return unexpected(x.error());
                    }
                    ems = true;
                    continue;
                case uint16_t(ExtensionType::ec_point_formats): {
                    auto f = read_point_formats(e.body);
                    if (!f) {
                        return unexpected(f.error());
                    }
                    bool uncompressed = false;
                    for (auto b : *f) {
                        uncompressed |= uint8_t(b) == 0;
                    }
                    if (!uncompressed) {
                        return unexpected(Alert{AlertDescription::illegal_parameter, 0, "ec_point_formats without the uncompressed form"});
                    }
                    continue;
                }
                case uint16_t(ExtensionType::application_layer_protocol_negotiation):
                    if (_offered & bit_of(ExtensionType::application_layer_protocol_negotiation)) {
                        if (auto x = _take_alpn(e.body); !x) {
                            return x;
                        }
                        continue;
                    }
                    break;
                case uint16_t(ExtensionType::server_name):
                    if (_offered & bit_of(ExtensionType::server_name)) {
                        if (auto x = read_empty(e.body); !x) {
                            return unexpected(x.error());
                        }
                        continue;
                    }
                    break;
                case uint16_t(ExtensionType::record_size_limit):
                    if (_offered & bit_of(ExtensionType::record_size_limit)) {
                        auto v = read_record_size_limit(e.body);
                        if (!v) {
                            return unexpected(v.error());
                        }
                        _result.record_size_limit = *v;
                        continue;
                    }
                    break;
                case SessionTicket:   // RFC 5077 §3.2: empty, a NewSessionTicket follows
                    if (_offered & bit_of(SessionTicket)) {
                        if (auto x = read_empty(e.body); !x) {
                            return unexpected(x.error());
                        }
                        _ticket_expected = true;
                        continue;
                    }
                    break;
                case uint16_t(ExtensionType::status_request):   // RFC 6066 §8: empty, a CertificateStatus may follow
                    if (_offered & bit_of(ExtensionType::status_request)) {
                        if (auto x = read_empty(e.body); !x) {
                            return unexpected(x.error());
                        }
                        _status12 = true;
                        continue;
                    }
                    break;
                default:
                    break;
                }
                return unexpected(Alert{AlertDescription::unsupported_extension, 0, "an extension that was not offered"});
            }
            if (!ems) {
                return unexpected(Alert{AlertDescription::handshake_failure, 0, "a TLS 1.2 server without the extended master secret (RFC 7627)"});
            }
            _tls12 = true;
            _result.version = Tls12;
            _result.cipher = cipher;
            std::memcpy(_s->server_random, sh.random.data(), 32);
            _s->shares.clear();   // 1.3's
            _s->psk.wipe();
            _choose_hash(hash_of(cipher));
            _hash_update(message);
            _hello_seen = true;
            if (echoed) {
                // the abbreviated handshake (§7.3): the keys of the
                // session's master secret and the new randoms
                _result.resumed = true;
                _result.peer_certificates = _settings.session->peer_certificates;
                if (_settings.session->group) {
                    _result.group = Group(_settings.session->group);   // no key exchange: the session's
                }
                key_block12(cipher, _s->client_keys, _s->server_keys, _s->master, bytes_of(_s->random, 32), bytes_of(_s->server_random, 32));
                _keep_messages = false;
                std::vector<byte>().swap(_s->messages);
                _state = _ticket_expected ? State::wait12_ticket : State::wait12_ccs;
                return {};
            }
            // a full handshake: the session offered, if any, is not taken
            _s->master.wipe();
            sgcl::detail::copy_bytes(_s->server_session_id, sh.session_id.data(), sh.session_id.size());
            _s->server_session_id_size = sh.session_id.size();
            _state = State::wait12_certificate;
            return {};
        }

        // Certificate (§7.4.2): the chain verified as 1.3's, its leaf's key
        // of the suite's kind (RSA for ECDHE_RSA; ECDSA or Ed25519 for
        // ECDHE_ECDSA, RFC 8422)
        expected<void, Alert> _certificate12(const Bytes& message, const Bytes& body) {
            auto c = read_certificate12(body);
            if (!c) {
                return unexpected(c.error());
            }
            if (c->count == 0) {
                return unexpected(Alert{AlertDescription::decode_error, 0, "the server's Certificate is empty"});
            }
            crypto::x509::chain chain;
            bool parsed = true;
            c->each([&](const Bytes& der) noexcept {
                auto cert = crypto::x509::certificate::parse(der);
                if (cert) {
                    chain.push_back(std::move(*cert));
                } else {
                    parsed = false;
                }
            });
            if (!parsed) {
                return unexpected(Alert{AlertDescription::bad_certificate, 0, "a certificate that does not parse"});
            }
            if (auto v = _verify_chain(chain); !v) {
                return v;
            }
            using crypto::x509::key_kind;
            const key_kind k = chain[0].public_key().kind();
            const bool fits = rsa_suite(_result.cipher) ? k == key_kind::rsa : k == key_kind::p256 || k == key_kind::p384 || k == key_kind::ed25519;
            if (!fits) {
                return unexpected(Alert{AlertDescription::unsupported_certificate, 0, "a certificate of another kind of key than the suite's"});
            }
            _result.peer_certificates = chain;
            _hash_update(message);
            _state = _status12 ? State::wait12_status_or_key_exchange : State::wait12_key_exchange;
            return {};
        }

        // CertificateStatus (RFC 6066 §8): the leaf's OCSP response, kept
        // for the connection to verify; a server may leave it out
        expected<void, Alert> _certificate_status12(const Bytes& message, const Bytes& body) noexcept {
            auto ocsp = read_certificate_status(body);
            if (!ocsp) {
                return unexpected(ocsp.error());
            }
            _result.ocsp_staple.assign(ocsp->data(), ocsp->data() + ocsp->size());
            _hash_update(message);
            _state = State::wait12_key_exchange;
            return {};
        }

        // ServerKeyExchange (RFC 8422 §5.4): a curve offered, a point of it,
        // a scheme offered, the signature over both randoms and the
        // parameters under the leaf; the client's share made and the
        // pre-master secret with it
        expected<void, Alert> _server_key_exchange12(const Bytes& message, const Bytes& body) {
            auto ske = read_server_key_exchange12(body);
            if (!ske) {
                return unexpected(ske.error());
            }
            const Group g = Group(ske->group);
            if (!_offers(_settings.groups, ske->group) || (g != Group::x25519 && g != Group::secp256r1 && g != Group::secp384r1)) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a ServerKeyExchange of a curve not offered for TLS 1.2"});
            }
            if (ske->point.size() != share_size(ske->group, true) || (g != Group::x25519 && uint8_t(ske->point[0]) != 4)) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a ServerKeyExchange's point of the wrong size or form"});
            }
            if (!_offers(_settings.schemes, ske->scheme)) {
                return unexpected(Alert{AlertDescription::illegal_parameter, 0, "a ServerKeyExchange signed with a scheme not offered"});
            }
            std::vector<byte> signed_part;
            signed_part.reserve(64 + ske->params.size());
            signed_part.insert(signed_part.end(), reinterpret_cast<const byte*>(_s->random), reinterpret_cast<const byte*>(_s->random) + 32);
            signed_part.insert(signed_part.end(), reinterpret_cast<const byte*>(_s->server_random), reinterpret_cast<const byte*>(_s->server_random) + 32);
            signed_part.insert(signed_part.end(), ske->params.data(), ske->params.data() + ske->params.size());
            if (auto v = verify12(ske->scheme, _result.peer_certificates[0].public_key(), bytes_of(signed_part.data(), signed_part.size()), ske->signature); !v) {
                return unexpected(v.error());
            }
            _s->shares.clear();
            _s->shares.add(g, _entropy);
            auto shared = _s->shares.shared(g, ske->point);
            if (!shared) {
                return unexpected(shared.error());
            }
            _s->pre_master = std::move(*shared);
            _result.group = g;
            _hash_update(message);
            _state = State::wait12_request_or_done;
            return {};
        }

        // CertificateRequest (§7.4.4): the types, the schemes, the authorities
        expected<void, Alert> _certificate_request12(const Bytes& message, const Bytes& body) noexcept {
            auto cr = read_certificate_request12(body);
            if (!cr) {
                return unexpected(cr.error());
            }
            _cert_types.assign(reinterpret_cast<const uint8_t*>(cr->types.data()), reinterpret_cast<const uint8_t*>(cr->types.data()) + cr->types.size());
            _peer_schemes.assign(cr->schemes.begin(), cr->schemes.end());
            cr->authorities.each([&](const Bytes& n) noexcept {
                _authorities.emplace_back(n.data(), n.data() + n.size());
            });
            _certificate_requested = true;
            _hash_update(message);
            _state = State::wait12_done;
            return {};
        }

        // ServerHelloDone: the client's flight. Its Certificate when asked
        // (a chain or none), ClientKeyExchange, the extended master secret
        // over the messages so far and the keys, its CertificateVerify over
        // every message so far, change_cipher_spec, its Finished under the
        // new keys
        expected<void, Alert> _server_hello_done12(const Bytes& message, const Bytes& body) {
            if (auto d = read_server_hello_done(body); !d) {
                return unexpected(d.error());
            }
            _hash_update(message);
            auto& out = _s->step.out;
            const size_t flight = out.size();
            Builder w(out);
            uint16_t scheme = 0;
            const ServerIdentity* id = nullptr;
            if (_certificate_requested) {
                id = _client_identity12(scheme);
                size_t at = out.size();
                std::vector<Bytes> ders;   // lint-handles: ok views of unmanaged bytes (bytes_of): no owner, no word
                if (id) {
                    for (auto& d : id->chain) {
                        ders.push_back(bytes_of(d.data(), d.size()));
                    }
                }
                write_certificate12(w, ders);
                _hash_update(bytes_of(out.data() + at, out.size() - at));
            }
            {
                size_t at = out.size();
                write_client_key_exchange12(w, _s->shares.public_share(0));
                _hash_update(bytes_of(out.data() + at, out.size() - at));
            }
            const size_t n = hash_size(_hash);
            uint8_t h[MaxHashSize];
            _transcript().value_to(h);   // the session hash (RFC 7627 §3)
            extended_master_secret(_hash, _s->master, _s->pre_master.view(), bytes_of(h, n));
            _s->pre_master = SharedSecret();
            _s->shares.clear();
            key_block12(_result.cipher, _s->client_keys, _s->server_keys, _s->master, bytes_of(_s->random, 32), bytes_of(_s->server_random, 32));
            if (id) {
                // over every message so far, hashed by the scheme (§7.4.8)
                size_t at = out.size();
                {
                    auto m = w.message(HandshakeType::certificate_verify);
                    w.u16(scheme);
                    auto sig = w.block16();
                    id->sign(id->key, scheme, bytes_of(_s->messages.data(), _s->messages.size()), w);
                }
                _hash_update(bytes_of(out.data() + at, out.size() - at));
                _result.certificate_sent = true;
            }
            _keep_messages = false;
            std::vector<byte>().swap(_s->messages);
            _push_send(Epoch::initial, flight, out.size() - flight);
            _push_ccs();
            _push_install12(Action::Kind::install_write, _s->client_keys);
            _s->client_keys.wipe();
            _transcript().value_to(h);
            uint8_t mine[12];
            finished12(_hash, mine, _s->master, true, bytes_of(h, n));
            size_t at = out.size();
            write_finished(w, bytes_of(mine, 12));
            crypto::detail::secure_zero(mine, sizeof mine);
            _hash_update(bytes_of(out.data() + at, out.size() - at));
            _push_send(Epoch::application, at, out.size() - at);
            _state = _ticket_expected ? State::wait12_ticket : State::wait12_ccs;
            return {};
        }

        // NewSessionTicket (RFC 5077 §3.3), announced by the ServerHello:
        // the ticket kept to the end of the handshake (one past MaxTicket
        // passed over, an empty one none); in the transcript
        expected<void, Alert> _new_session_ticket12(const Bytes& message, const Bytes& body) noexcept {
            auto t = read_new_session_ticket12(body);
            if (!t) {
                return unexpected(t.error());
            }
            if (t->ticket.size() <= MaxTicket) {
                _s->ticket12.assign(t->ticket.data(), t->ticket.data() + t->ticket.size());
                _ticket_hint = t->lifetime_hint;
            }
            _hash_update(message);
            _state = State::wait12_ccs;
            return {};
        }

        // The server's Finished, after its change_cipher_spec: verify_data
        // over every message through the client's Finished (in an
        // abbreviated handshake, through the server's own messages: the
        // client's change_cipher_spec and Finished follow it); the session
        // for the cache
        expected<void, Alert> _finished12(const Bytes& message, const Bytes& body) noexcept {
            auto f = read_finished(body, 12);
            if (!f) {
                return unexpected(f.error());
            }
            uint8_t h[MaxHashSize], expected_data[12];
            _transcript().value_to(h);
            finished12(_hash, expected_data, _s->master, false, bytes_of(h, hash_size(_hash)));
            const bool ok = crypto::constant_time::equal(bytes_of(expected_data, 12), *f);
            crypto::detail::secure_zero(expected_data, sizeof expected_data);
            if (!ok) {
                return unexpected(Alert{AlertDescription::decrypt_error, 0, "the server's Finished does not verify"});
            }
            _hash_update(message);
            if (_result.resumed) {
                auto& out = _s->step.out;
                Builder w(out);
                _push_ccs();
                _push_install12(Action::Kind::install_write, _s->client_keys);
                _s->client_keys.wipe();
                _transcript().value_to(h);
                uint8_t mine[12];
                finished12(_hash, mine, _s->master, true, bytes_of(h, hash_size(_hash)));
                size_t at = out.size();
                write_finished(w, bytes_of(mine, 12));
                crypto::detail::secure_zero(mine, sizeof mine);
                _hash_update(bytes_of(out.data() + at, out.size() - at));
                _push_send(Epoch::application, at, out.size() - at);
            }
            _result.server_name = _settings.server_name;
            _s->step.actions.push_back(Action{Action::Kind::established});
            if (_settings.resumption) {
                _push_session12();
            }
            _wipe();
            _state = State::connected;
            return {};
        }

        // The session of a 1.2 handshake for the cache: the master secret;
        // the new ticket, else the resumed session's; the session id of the full
        // handshake that made it; the lifetime of the ticket's hint (seven
        // days at most, a day where none is given), a resumed session's own
        // when no new ticket came. Nothing for a session of neither ticket
        // nor session id
        void _push_session12() noexcept {
            const Session* resumed = _result.resumed ? _settings.session.get() : nullptr;
            const bool new_ticket = !_s->ticket12.empty();
            Action a{Action::Kind::new_ticket, Epoch::application, _result.cipher};
            a.tls12 = true;
            auto& out = _s->step.out;
            a.offset = out.size();
            if (new_ticket) {
                out.insert(out.end(), _s->ticket12.begin(), _s->ticket12.end());
            } else if (resumed) {
                out.insert(out.end(), resumed->ticket.begin(), resumed->ticket.end());
            }
            a.size = out.size() - a.offset;
            if (resumed) {
                sgcl::detail::copy_bytes(a.session_id, resumed->session_id, resumed->session_id_size);
                a.session_id_size = resumed->session_id_size;
            } else {
                sgcl::detail::copy_bytes(a.session_id, _s->server_session_id, _s->server_session_id_size);
                a.session_id_size = uint8_t(_s->server_session_id_size);
            }
            if (a.size == 0 && a.session_id_size == 0) {
                out.resize(a.offset);
                return;
            }
            if (resumed && !new_ticket) {
                a.issued_ms = resumed->received_ms;
                a.lifetime = resumed->lifetime;
            } else {
                a.issued_ms = _clock().unix_milli();
                a.lifetime = new_ticket && _ticket_hint ? std::min<uint32_t>(_ticket_hint, MaxLifetime12) : DefaultLifetime12;
            }
            a.group = uint16_t(_result.group);
            std::memcpy(a.secret.bytes, _s->master.bytes, sizeof a.secret.bytes);
            a.secret.size = _s->master.size;
            ZeroingProbe::on_secret(a.secret);
            _s->step.actions.push_back(std::move(a));
        }

        // After a TLS 1.2 handshake: a HelloRequest passed over (no
        // renegotiation, §7.4.1.1), nothing else taken
        expected<void, Alert> _after12(HandshakeType type) noexcept {
            if (type == HandshakeType::hello_request) {
                return {};
            }
            return unexpected(Alert{AlertDescription::unexpected_message, 0, "a handshake message after a TLS 1.2 handshake"});
        }

        // The identity for a 1.2 CertificateRequest: as 1.3's, its key of a
        // certificate type asked for (rsa_sign 1, ecdsa_sign 64: Ed25519's
        // too, RFC 8422), an RSA key signing PKCS #1 v1.5 besides PSS
        const ServerIdentity* _client_identity12(uint16_t& scheme) const noexcept {
            using crypto::x509::key_kind;
            auto has_type = [&](uint8_t t) noexcept {
                for (uint8_t x : _cert_types) {
                    if (x == t) {
                        return true;
                    }
                }
                return false;
            };
            for (const ServerIdentity& id : _settings.identities) {
                const key_kind k = id.leaf ? id.leaf->public_key().kind() : key_kind::none;
                if (!(k == key_kind::rsa ? has_type(1) : has_type(64))) {
                    continue;
                }
                std::vector<uint16_t> candidates(id.schemes.begin(), id.schemes.end());
                if (k == key_kind::rsa) {
                    candidates.insert(candidates.end(), {0x0401, 0x0501, 0x0601});
                }
                uint16_t s = 0;
                for (uint16_t x : candidates) {
                    if (_offers(_peer_schemes, x)) {
                        s = x;
                        break;
                    }
                }
                if (!s) {
                    continue;
                }
                bool issued = _authorities.empty();
                for (const auto& issuer : id.issuers) {
                    for (const auto& a : _authorities) {
                        issued |= issuer.size() == a.size() && std::memcmp(issuer.data(), a.data(), a.size()) == 0;
                    }
                }
                if (issued) {
                    scheme = s;
                    return &id;
                }
            }
            return nullptr;
        }

        // --- helpers ------------------------------------------------------------

        // The client's identity for the server's CertificateRequest: the
        // first whose key signs one of the schemes it takes and, when it
        // names authorities, one of whose certificates is issued by one of
        // them (as Go chooses); the scheme the first of the identity's the
        // server takes
        const ServerIdentity* _client_identity(uint16_t& scheme) const noexcept {
            for (const ServerIdentity& id : _settings.identities) {
                uint16_t s = 0;
                for (uint16_t x : id.schemes) {
                    if (_offers(_peer_schemes, x)) {
                        s = x;
                        break;
                    }
                }
                if (!s) {
                    continue;
                }
                bool issued = _authorities.empty();
                for (const auto& issuer : id.issuers) {
                    for (const auto& a : _authorities) {
                        issued |= issuer.size() == a.size() && std::memcmp(issuer.data(), a.data(), a.size()) == 0;
                    }
                }
                if (issued) {
                    scheme = s;
                    return &id;
                }
            }
            return nullptr;
        }

        template<class R>
        static bool _offers(const R& list, uint16_t v) noexcept {
            for (uint16_t x : list) {
                if (x == v) {
                    return true;
                }
            }
            return false;
        }

        SGCL_INLINE_HOT Transcript& _transcript() noexcept {
            return _hash == Hash::sha384 ? _s->t384 : _s->t256;
        }

        SGCL_INLINE_HOT void _hash_update(const Bytes& m) noexcept {
            if (_keep_messages) {
                _s->messages.insert(_s->messages.end(), m.data(), m.data() + m.size());
            }
            if (_hash_known) {
                _transcript().update(m);
            } else {
                _s->t256.update(m);
                _s->t384.update(m);
            }
        }

        SGCL_INLINE_HOT void _choose_hash(Hash h) noexcept {
            if (!_hash_known) {
                _hash = h;
                _hash_known = true;
                // the other one no longer needed (both held nothing secret)
            }
        }

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

        SGCL_INLINE_HOT void _push_install12(Action::Kind kind, const Secret& s) noexcept {
            Action a{kind, Epoch::application, _result.cipher};
            a.tls12 = true;
            std::memcpy(a.secret.bytes, s.bytes, sizeof s.bytes);
            a.secret.size = s.size;
            _s->step.actions.push_back(std::move(a));
        }

        SGCL_INLINE_HOT void _push_install(Action::Kind kind, Epoch e, const Secret& s) noexcept {
            Action a{kind, e, _result.cipher};
            std::memcpy(a.secret.bytes, s.bytes, sizeof s.bytes);
            a.secret.size = s.size;
            _s->step.actions.push_back(std::move(a));
        }

        // The alert that ends the handshake. From the ServerHello on, the
        // server reads under the handshake keys (RFC 8446 §5.1: no record
        // in the clear after them), so a client that refuses what follows
        // (a chain that does not verify, a Finished) sends its alert under
        // its own handshake keys, a change_cipher_spec first in
        // compatibility mode; in the clear, the server would read only an
        // unexpected_message
        const Step& _fail(const Alert& a) noexcept {
            _s->step.clear();
            if (_s->schedule && _state >= State::wait_encrypted_extensions && _state <= State::wait_finished) {
                if (_settings.compatibility_mode && !_ccs_sent) {
                    _push_ccs();
                }
                _push_install(Action::Kind::install_write, Epoch::handshake, _s->schedule->client_handshake_traffic);
            }
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
        SGCL_INLINE_HOT void _wipe() noexcept {
            _s->shares.clear();
            _s->psk.wipe();
            _s->pre_master = SharedSecret();
            _s->master.wipe();
            _s->client_keys.wipe();
            _s->server_keys.wipe();
            if (_s->schedule) {
                _s->schedule->finish_handshake();
                _s->schedule.reset();
            }
        }
    };
}
