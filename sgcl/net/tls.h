//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"
#include "dns.h"
#include "socket.h"
#include "tls/error.h"
#include "tls/detail/impl.h"
#include "../core/detail/handle_word.h"
#include "../crypto/ed25519.h"
#include "../crypto/p256.h"
#include "../crypto/p384.h"
#include "../crypto/pkcs12.h"
#include "../crypto/rsa.h"
#include "../crypto/x509.h"
#include "../crypto/x509_revocation.h"
#include "../crypto/detail/key_pem.h"
#include "../encoding/pem.h"

#include <memory>
#include <stdexcept>
#include <string_view>

// TLS 1.3 (RFC 8446) over the module's connections: a client that
// connects and completes the handshake before it gives the connection
// out, which is then a net::connection as any other (read, read_line,
// write, deadlines, close, http on top). The client speaks TLS 1.2 too
// (RFC 5246 with RFC 7627's extended master secret, ECDHE and AEAD alone),
// for servers without 1.3, unless config::min_version says otherwise; the
// server speaks 1.3 alone. The groups
// X25519MLKEM768 (post-quantum hybrid, first by default), X25519, P-256,
// P-384, P-521 (when listed); the cipher suites AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM;
// the server's chain verified by crypto::x509 against the system's roots
// or the pool given, for the server's name or address; client
// certificates (mTLS: config::client_auth, the client's identities);
// session resumption by tickets (psk_dhe_ke: the server's ticket_keys,
// the client's session_cache, which resumes 1.2 sessions too, by their
// tickets or session ids), no 0-RTT. The failures are io::errors of
// the tls category (tls/error.h): an alert of this side, of the peer
// ("remote error: tls: …"), a chain that did not verify.
//
//   auto c = net::tls::connect("example.com:443");
//   c->write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
//
// The implementation has not been through an independent cryptographic
// audit.
namespace sgcl::net::tls {
    enum class group : uint16_t {
        x25519_mlkem768 = 0x11EC,
        x25519 = 0x001D,
        secp256r1 = 0x0017,
        secp384r1 = 0x0018,
        secp521r1 = 0x0019,
    };

    enum class cipher : uint16_t {
        aes_128_gcm_sha256 = 0x1301,
        aes_256_gcm_sha384 = 0x1302,
        chacha20_poly1305_sha256 = 0x1303,
        // TLS 1.2, the client's
        ecdhe_ecdsa_aes_128_gcm_sha256 = 0xC02B,
        ecdhe_ecdsa_aes_256_gcm_sha384 = 0xC02C,
        ecdhe_rsa_aes_128_gcm_sha256 = 0xC02F,
        ecdhe_rsa_aes_256_gcm_sha384 = 0xC030,
        ecdhe_rsa_chacha20_poly1305_sha256 = 0xCCA8,
        ecdhe_ecdsa_chacha20_poly1305_sha256 = 0xCCA9,
    };

    // The versions of TLS, by their numbers on the wire
    enum class version : uint16_t {
        tls12 = 0x0303,
        tls13 = 0x0304,
    };

    // Whether a server asks the client for a certificate (RFC 8446 §4.3.2)
    enum class client_auth : uint8_t {
        none,       // not asked for
        request,    // asked for: one sent is verified, none is taken
        require,    // asked for and required: none is certificate_required
    };

    // Whether and how the peer's chain is checked for revocation, after it
    // verified (config::revocation)
    enum class revocation_mode : uint8_t {
        off,            // not checked (Go's default): no status_request sent
        staple_only,    // the leaf's OCSP staple when the peer sends one, Must-Staple enforced, the config's CRLs; nothing online
        soft_fail,      // the staple, the config's CRLs, then OCSP and the CRLs online; a status that cannot be had is taken
        hard_fail,      // the same, and every certificate but the root must be known good
    };

    // Where the status of a connection's peer came from (state::revocation_source)
    enum class revocation_source : uint8_t {
        none,           // not checked, or checked by nothing
        staple,         // the OCSP response the peer stapled
        ocsp,           // an OCSP responder online (or the cache of its answer)
        crl,            // a CRL: the config's, or one of the distribution points (or the cache of it)
    };

    class identity;
    class ech_key;
    class session_cache;
    class ticket_keys;
    class revocation_cache;

    namespace detail {
        // The private key of an identity, in unmanaged memory: one of the
        // kinds CertificateVerify signs with, never copied
        struct IdentityKey {
            optional<crypto::ed25519::private_key> ed25519;
            optional<crypto::p256::private_key> p256;
            optional<crypto::p384::private_key> p384;
            optional<crypto::p521::private_key> p521;
            optional<crypto::rsa::private_key> rsa;
        };

        struct IdentityState {
            crypto::x509::chain certificates;           // the leaf first
            std::unique_ptr<IdentityKey> key;          // unmanaged, zeroed by the keys' own destructors
            StapleHolder staple;                        // the leaf's OCSP response a server staples
        };

        // The response checked against the identity's leaf: successful, a
        // status of the leaf; verified under the issuer when the chain has
        // it. Its DER, thisUpdate and nextUpdate
        inline expected<FetchedStaple, io::error> check_staple(const IdentityState& st, const slice<const byte>& der) noexcept {
            auto fail = [](const string& why) {
                return io::error(crypto::errc::verification, "identity", why);
            };
            auto resp = crypto::x509::ocsp_response::parse(der);
            if (!resp) {
                return unexpected(io::error(resp.error().code(), "identity", resp.error().message()));
            }
            const auto& leaf = st.certificates[0];
            optional<crypto::x509::ocsp_single_response> single;
            if (st.certificates.size() > 1) {
                auto v = resp->verify(leaf, st.certificates[1]);
                if (!v) {
                    return unexpected(fail(v.error().message()));
                }
                single = *v;
            } else {
                if (resp->status() != crypto::x509::ocsp_response_status::successful) {
                    return unexpected(fail(string("an OCSP response that is not successful")));
                }
                for (const auto& r : resp->responses()) {
                    if (r.serial_number == leaf.serial_number()) {
                        single = r;
                        break;
                    }
                }
                if (!single) {
                    return unexpected(fail(string("an OCSP response without a status of the leaf")));
                }
            }
            FetchedStaple f;
            f.der = vector<byte>(der.data(), der.data() + der.size());
            f.this_update = single->this_update.unix();
            f.next_update = single->next_update ? single->next_update->unix() : 0;
            return f;
        }

        struct IdentityAccess {
            static const IdentityState& state(const identity& id) noexcept;
            static tracked_ptr<const void> word(const identity& id) noexcept;
            static StapleHolder& staple(const identity& id) noexcept;
        };

        // The key of a private key's DER under its PEM label: PKCS #8
        // ("PRIVATE KEY") of each kind, SEC 1 ("EC PRIVATE KEY"), PKCS #1
        // ("RSA PRIVATE KEY"); the DER is the caller's secret_bytes, read
        // in place
        inline bool read_key(IdentityKey& k, std::string_view label, const slice<const byte>& der) noexcept {
            if (label == "PRIVATE KEY") {
                if (auto e = crypto::ed25519::private_key::from_pkcs8_der(der)) {
                    k.ed25519.emplace(std::move(*e));
                    return true;
                }
                if (auto p = crypto::p256::private_key::from_pkcs8_der(der)) {
                    k.p256.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p384::private_key::from_pkcs8_der(der)) {
                    k.p384.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p521::private_key::from_pkcs8_der(der)) {
                    k.p521.emplace(std::move(*p));
                    return true;
                }
                if (auto r = crypto::rsa::private_key::from_pkcs8_der(der)) {
                    k.rsa.emplace(std::move(*r));
                    return true;
                }
                return false;
            }
            if (label == "EC PRIVATE KEY") {
                if (auto p = crypto::p256::private_key::from_sec1_der(der)) {
                    k.p256.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p384::private_key::from_sec1_der(der)) {
                    k.p384.emplace(std::move(*p));
                    return true;
                }
                if (auto p = crypto::p521::private_key::from_sec1_der(der)) {
                    k.p521.emplace(std::move(*p));
                    return true;
                }
                return false;
            }
            if (label == "RSA PRIVATE KEY") {
                if (auto r = crypto::rsa::private_key::from_pkcs1_der(der)) {
                    k.rsa.emplace(std::move(*r));
                    return true;
                }
            }
            return false;
        }

        // Whether the key is the leaf's: a signature of the key verified
        // under the certificate's public key
        inline bool key_matches(const IdentityKey& k, const crypto::x509::certificate& leaf) noexcept {
            static constexpr uint8_t probe[] = "sgcl::net::tls identity";
            const auto content = tls::detail::bytes_of(probe, sizeof probe);
            std::vector<byte> sig;
            tls::detail::Builder w(sig);
            uint16_t scheme = 0;
            if (k.ed25519) {
                scheme = uint16_t(tls::detail::SignatureScheme::ed25519);
                tls::detail::sign(w, scheme, *k.ed25519, content);
            } else if (k.p256) {
                scheme = uint16_t(tls::detail::SignatureScheme::ecdsa_secp256r1_sha256);
                tls::detail::sign(w, scheme, *k.p256, content);
            } else if (k.p384) {
                scheme = uint16_t(tls::detail::SignatureScheme::ecdsa_secp384r1_sha384);
                tls::detail::sign(w, scheme, *k.p384, content);
            } else if (k.p521) {
                scheme = uint16_t(tls::detail::SignatureScheme::ecdsa_secp521r1_sha512);
                tls::detail::sign(w, scheme, *k.p521, content);
            } else if (k.rsa) {
                scheme = uint16_t(tls::detail::SignatureScheme::rsa_pss_rsae_sha256);
                tls::detail::sign(w, scheme, *k.rsa, content);
            } else {
                return false;
            }
            return tls::detail::verify(scheme, leaf.public_key(), content, tls::detail::bytes_of(sig.data(), sig.size())).has_value();
        }
    }

    // A certificate chain with its private key, a server's or a client's:
    // a handle of one word whose state is made in the constructor; the key
    // lives in unmanaged memory and is never copied (copies of the handle,
    // and of a config that holds it, share it)
    class identity {
    public:
        // The chain (the leaf first) and the key, each in PEM; the key must
        // be the leaf's. errc::malformed (crypto) for anything else, as an
        // io::error of op "identity" (errc::unsupported for an encrypted
        // key). The key's PEM is bytes, read where they lie: a secret_bytes
        // (crypto::read_secret of a key file), a buffer of the caller's; its
        // DER goes straight into a secret_bytes, never into managed memory
        // (a string converts too, but its bytes are managed: the caller's
        // choice). The key's first block is taken (PRIVATE KEY, EC PRIVATE
        // KEY, RSA PRIVATE KEY; others passed over).
        static expected<identity, io::error> from_pem(const string& certificate_chain_pem, const slice<const byte>& key_pem) noexcept {
            auto s = make_tracked<detail::IdentityState>();
            s->key = std::make_unique<detail::IdentityKey>();
            auto blocks = encoding::pem::parse_all(certificate_chain_pem);
            if (!blocks) {
                return unexpected(_error("the certificate chain is not PEM"));
            }
            for (auto& b : *blocks) {
                if (b.type() != "CERTIFICATE") {
                    continue;
                }
                auto c = crypto::x509::certificate::parse(b.bytes().as_slice());
                if (!c) {
                    return unexpected(_error("a certificate of the chain does not parse"));
                }
                s->certificates.push_back(std::move(*c));
            }
            if (s->certificates.empty()) {
                return unexpected(_error("no certificate in the chain"));
            }
            auto block = crypto::detail::read_key_pem(key_pem);
            if (!block) {
                return unexpected(io::error(block.error().code(), "identity", block.error().message()));
            }
            if (!detail::read_key(*s->key, block->label, block->der)) {
                return unexpected(_error("no private key of a kind TLS 1.3 signs with (Ed25519, P-256, P-384, P-521, RSA)"));
            }
            if (!detail::key_matches(*s->key, s->certificates[0])) {
                return unexpected(_error("the private key is not the leaf certificate's"));
            }
            return identity(std::move(s));
        }

        // The key and the chain of a PKCS #12 file read (crypto::pkcs12): the
        // chain as the file holds it, the leaf first; the key must be the
        // leaf's. crypto::errc::malformed as an io::error of op "identity"
        // for a file without a key or a certificate
        static expected<identity, io::error> from_pkcs12(const crypto::pkcs12& file) noexcept {
            if (file.key_kind() == crypto::x509::key_kind::none || file.certificates().empty()) {
                return unexpected(_error("a PKCS #12 file without a key and its certificate"));
            }
            auto s = make_tracked<detail::IdentityState>();
            s->key = std::make_unique<detail::IdentityKey>();
            s->certificates = file.certificates();
            auto pkcs8 = file.key_pkcs8();
            if (!detail::read_key(*s->key, "PRIVATE KEY", pkcs8.as_slice())) {
                return unexpected(_error("no private key of a kind TLS 1.3 signs with (Ed25519, P-256, P-384, P-521, RSA)"));
            }
            if (!detail::key_matches(*s->key, s->certificates[0])) {
                return unexpected(_error("the private key is not the leaf certificate's"));
            }
            return identity(std::move(s));
        }

        // A PKCS #12 file's bytes and its password in one line: the file
        // read (its errors as an io::error of op "identity"), then as above
        static expected<identity, io::error> from_pkcs12(const slice<const byte>& file, const slice<const byte>& password) noexcept {
            auto p = crypto::pkcs12::parse(file, password);
            if (!p) {
                return unexpected(io::error(p.error().code(), "identity", p.error().message()));
            }
            return from_pkcs12(*p);
        }

        // The same, a broken one thrown (std::invalid_argument)
        SGCL_INLINE_HOT explicit identity(const crypto::pkcs12& file) {
            auto r = from_pkcs12(file);
            if (!r) {
                throw std::invalid_argument(std::string(r.error().message().view()));
            }
            _s = r->_s;
        }

        // The same, a broken one thrown (std::invalid_argument)
        SGCL_INLINE_HOT identity(const string& certificate_chain_pem, const slice<const byte>& key_pem) {
            auto r = from_pem(certificate_chain_pem, key_pem);
            if (!r) {
                throw std::invalid_argument(std::string(r.error().message().view()));
            }
            _s = r->_s;
        }

        SGCL_INLINE_HOT const crypto::x509::chain& certificates() const noexcept {
            return _s->certificates;
        }

        // The OCSP response a server staples for the leaf (RFC 6066, RFC
        // 8446 §4.4.2.1), Go's Certificate.OCSPStaple: checked against the
        // leaf (successful, a status of it, verified under its issuer when
        // the chain has one; crypto::errc::verification or malformed as an
        // io::error of op "identity" otherwise); empty bytes clear it.
        // Copies of the identity share it; a server with
        // config::ocsp_stapling replaces it as it refreshes
        expected<void, io::error> set_ocsp_staple(const slice<const byte>& ocsp_response) const noexcept {
            if (ocsp_response.empty()) {
                _s->staple.clear();
                return {};
            }
            auto f = detail::check_staple(*_s, ocsp_response);
            if (!f) {
                return unexpected(f.error());
            }
            _s->staple.set(std::vector<byte>(f->der.data(), f->der.data() + f->der.size()), f->this_update, f->next_update);
            return {};
        }

        // The response stapled now: empty when there is none, or the one
        // there is past its nextUpdate
        vector<byte> ocsp_staple() const noexcept {
            auto d = _s->staple.current(time::now().unix());
            return vector<byte>(d.data(), d.data() + d.size());
        }

    private:
        friend struct detail::IdentityAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit identity(tracked_ptr<detail::IdentityState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT identity(sgcl::detail::FromWord, const tracked_ptr<detail::IdentityState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::IdentityState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::IdentityState>& _handle_word() const noexcept {
            return _s;
        }

        static io::error _error(const char* what) noexcept {
            return io::error(crypto::errc::malformed, "identity", string(what));
        }

        tracked_ptr<detail::IdentityState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const IdentityState& IdentityAccess::state(const identity& id) noexcept {
            return *id._s;
        }

        SGCL_INLINE_HOT tracked_ptr<const void> IdentityAccess::word(const identity& id) noexcept {
            return tracked_ptr<const void>(id._s);
        }

        SGCL_INLINE_HOT StapleHolder& IdentityAccess::staple(const identity& id) noexcept {
            return id._s->staple;
        }

        // The state of an ech_key: the ECHConfig, read once, and the HPKE
        // private key in unmanaged memory, zeroed when the state is collected
        struct EchKeyState {
            EchConfig config;
            std::unique_ptr<crypto::hpke::private_key> key;
            bool retry = true;
        };

        struct EchKeyAccess {
            static const EchKeyState& state(const ech_key& k) noexcept;
            static tracked_ptr<const void> word(const ech_key& k) noexcept;
        };

        struct SessionCacheAccess {
            static const tracked_ptr<SessionCacheState>& state(const session_cache& c) noexcept;
        };

        struct RevocationCacheAccess {
            static const tracked_ptr<RevocationCacheState>& state(const revocation_cache& c) noexcept;
        };

        // The ticket keys inside a handle: unmanaged, zeroed when the
        // handle's state is collected
        struct TicketKeyState {
            std::unique_ptr<TicketKeys> keys;
        };

        struct TicketKeysAccess {
            static TicketKeys& keys(const ticket_keys& k) noexcept;
            static tracked_ptr<const void> word(const ticket_keys& k) noexcept;
        };
    }

    // A server's key of Encrypted Client Hello (RFC 9849): an ECHConfig —
    // its id, its KEM's public key, the HPKE suites it takes, the public
    // name the outer hello names — and the HPKE private key that opens the
    // hellos sealed to it. A handle of one word whose state is made in the
    // constructor; the private key lives in unmanaged memory, never copied,
    // zeroed when the last handle's state is collected. What a server
    // publishes is ech_config_list of its keys (DNS's HTTPS record, RFC 9848)
    class ech_key {
    public:
        // What a new key is made of
        struct options {
            optional<uint8_t> config_id;                              // nullopt: random
            crypto::hpke::kem kem = crypto::hpke::kem::dhkem_x25519;
            vector<crypto::hpke::suite> suites;                       // empty: HKDF-SHA256 with AES-128-GCM, AES-256-GCM and ChaCha20-Poly1305
            uint8_t max_name_length = 0;                              // the longest server name of the hellos (RFC 9849 §6.1.3); 0: none named
            bool retry = true;                                        // sent in retry_configs when ECH is rejected
        };

        // A new key of the public name: a fresh HPKE key and its ECHConfig.
        // A public name that is no DNS name (an address, a label of other
        // characters than letters, digits and '-') is std::invalid_argument
        static ech_key generate(const string& public_name) {
            return generate(public_name, options());
        }

        static ech_key generate(const string& public_name, const options& o) {
            if (!detail::ech_public_name_ok(public_name.view())) {
                throw std::invalid_argument("sgcl::net::tls::ech_key::generate: a public name that is no DNS name");
            }
            auto key = crypto::hpke::private_key::generate(o.kem);
            uint8_t id = 0;
            if (o.config_id) {
                id = *o.config_id;
            } else {
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(&id), 1));
            }
            std::vector<uint32_t> suites;
            if (o.suites.empty()) {
                suites = {0x00010001, 0x00010002, 0x00010003};
            }
            for (const auto& x : o.suites) {
                if (x.aead == crypto::hpke::aead::export_only) {
                    throw std::invalid_argument("sgcl::net::tls::ech_key::generate: an export_only suite seals no hello");
                }
                suites.push_back(uint32_t(x.kdf) << 16 | uint16_t(x.aead));
            }
            auto pub = key.public_key().bytes();
            auto config = detail::write_ech_config(id, uint16_t(o.kem), pub.as_slice(), suites, o.max_name_length, public_name.view());
            auto k = from_bytes(slice<const byte>(config.data(), config.size()), key.bytes(), o.retry);
            return *k;
        }

        // A key of an ECHConfig (version 0xfe0d) and its private key, as
        // Go's EncryptedClientHelloKey has them: crypto::errc::malformed for
        // a config that does not read, crypto::errc::invalid_key for a key
        // that is not the config's, as an io::error of op "ech key"
        static expected<ech_key, io::error> from_bytes(const slice<const byte>& config, const slice<const byte>& private_key, bool retry = true) noexcept {
            std::vector<byte> list;
            list.reserve(config.size() + 2);
            list.push_back(byte(config.size() >> 8));
            list.push_back(byte(config.size()));
            list.insert(list.end(), config.data(), config.data() + config.size());
            auto configs = detail::read_ech_configs(detail::bytes_of(list.data(), list.size()));
            if (!configs || configs->size() != 1 || !detail::ech_kem_known(configs->front().kem)) {
                return unexpected(io::error(crypto::errc::malformed, "ech key", string("not one ECHConfig of version 0xfe0d and a KEM of the module")));
            }
            auto s = make_tracked<detail::EchKeyState>();
            s->config = std::move(configs->front());
            auto key = crypto::hpke::private_key::from_bytes(crypto::hpke::kem(s->config.kem), private_key);
            if (!key) {
                return unexpected(io::error(crypto::errc::invalid_key, "ech key", string("a private key that does not read")));
            }
            auto pub = key->public_key().bytes();
            if (pub.size() != s->config.public_key.size() || !std::equal(pub.begin(), pub.end(), s->config.public_key.begin())) {
                return unexpected(io::error(crypto::errc::invalid_key, "ech key", string("a private key that is not the ECHConfig's")));
            }
            s->key = std::make_unique<crypto::hpke::private_key>(std::move(*key));
            s->retry = retry;
            return ech_key(std::move(s));
        }

        // The ECHConfig
        vector<byte> config() const {
            return vector<byte>(_s->config.raw.data(), _s->config.raw.data() + _s->config.raw.size());
        }

        // The HPKE private key (SerializePrivateKey), in plain memory
        crypto::secret_bytes private_key() const {
            return _s->key->bytes();
        }

        uint8_t config_id() const noexcept {
            return _s->config.id;
        }

        string public_name() const {
            return string(std::string_view(_s->config.public_name));
        }

        // Whether the key's config is sent in retry_configs
        bool retry() const noexcept {
            return _s->retry;
        }

    private:
        friend struct detail::EchKeyAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit ech_key(tracked_ptr<detail::EchKeyState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT ech_key(sgcl::detail::FromWord, const tracked_ptr<detail::EchKeyState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::EchKeyState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::EchKeyState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::EchKeyState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const EchKeyState& EchKeyAccess::state(const ech_key& k) noexcept {
            return *k._s;
        }

        SGCL_INLINE_HOT tracked_ptr<const void> EchKeyAccess::word(const ech_key& k) noexcept {
            return tracked_ptr<const void>(k._s);
        }

        // The ECHConfigList of the keys' configs (those of `retry` alone when asked)
        inline std::vector<byte> ech_list_of(const vector<tls::ech_key>& keys, bool retry_only) noexcept {
            std::vector<byte> out(2);
            for (const auto& k : keys) {
                const auto& st = EchKeyAccess::state(k);
                if (retry_only && !st.retry) {
                    continue;
                }
                out.insert(out.end(), st.config.raw.begin(), st.config.raw.end());
            }
            out[0] = byte((out.size() - 2) >> 8);
            out[1] = byte(out.size() - 2);
            return out.size() == 2 ? std::vector<byte>() : out;
        }
    }

    // The ECHConfigList of the keys (RFC 9849 §4): what a server publishes
    // in its DNS HTTPS record's "ech" (RFC 9848) and what a client's
    // config::ech_config_list takes; empty for no keys
    inline vector<byte> ech_config_list(const vector<ech_key>& keys) {
        auto l = detail::ech_list_of(keys, false);
        return vector<byte>(l.data(), l.data() + l.size());
    }

    namespace detail {
        // The marker of a rejecting server's retry_configs in an
        // ech_required error's text: RFC 9460's presentation form of the
        // same bytes in a DNS HTTPS record
        inline constexpr std::string_view EchRetryMarker = " ech=";
    }

    // The retry_configs of a server that rejected a client's Encrypted
    // Client Hello (Go's ECHRejectionError.RetryConfigList): an ECHConfigList
    // for config::ech_config_list, of an alert::ech_required error of
    // client() or connect(); empty for any other error and for a server that
    // sent none
    inline vector<byte> ech_retry_configs(const io::error& e) noexcept {
        if (e.code() != make_error_code(alert::ech_required)) {
            return {};
        }
        const std::string_view p = e.path().view();
        const size_t at = p.rfind(detail::EchRetryMarker);
        if (at == std::string_view::npos) {
            return {};
        }
        auto b = encoding::base64::standard.decode(string(p.substr(at + detail::EchRetryMarker.size())));
        return b ? *b : vector<byte>();
    }

    // The sessions a client may resume (RFC 8446 §2.2; TLS 1.2's, RFC 5246
    // §7.3 and RFC 5077, too): what the NewSessionTickets of its servers
    // gave it (a 1.2 server's ticket or session id), by the server's name,
    // port and ALPN protocols; a handle of one word, its copies the same
    // cache, safe from many threads. A session is offered once (a 1.2
    // session the server resumed is kept again); a server holds at most
    // four, the cache at most `capacity`, the oldest dropped first.
    // Their secrets live in unmanaged memory, zeroed when a session is
    // taken, dropped or cleared
    class session_cache {
    public:
        // A cache of at most `capacity` sessions (0: none kept)
        SGCL_INLINE_HOT explicit session_cache(size_t capacity = 64) noexcept
        : _s(make_tracked<detail::SessionCacheState>(capacity)) {
        }

        // The sessions held now
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _s->size();
        }

        SGCL_INLINE_HOT size_t capacity() const noexcept {
            return _s->capacity;
        }

        // Every session dropped, its secret zeroed
        SGCL_INLINE_HOT void clear() const noexcept {
            _s->clear();
        }

    private:
        friend struct detail::SessionCacheAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT session_cache(sgcl::detail::FromWord, const tracked_ptr<detail::SessionCacheState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::SessionCacheState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::SessionCacheState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::SessionCacheState> _s;
    };

    // The keys a server seals its session tickets under (AES-256-GCM): a
    // handle of one word, its copies (and the copies of a config that holds
    // it) the same keys, safe from many threads. Two are kept, the current
    // one that seals and the previous one that still opens; the current one
    // becomes the previous once it is older than the tickets' lifetime, so
    // a ticket resumes for its lifetime and no longer. The keys live in
    // unmanaged memory, zeroed when replaced and when the handle's state is
    // collected
    class ticket_keys {
    public:
        // A key made at random
        SGCL_INLINE_HOT ticket_keys() noexcept
        : _s(make_tracked<detail::TicketKeyState>()) {
            _s->keys = std::make_unique<detail::TicketKeys>();
        }

        // A new current key: the current one becomes the previous, and the
        // tickets sealed under the previous one no longer resume
        SGCL_INLINE_HOT void rotate() const noexcept {
            _s->keys->rotate();
        }

    private:
        friend struct detail::TicketKeysAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT ticket_keys(sgcl::detail::FromWord, const tracked_ptr<detail::TicketKeyState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::TicketKeyState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::TicketKeyState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::TicketKeyState> _s;
    };

    // What the online revocation checks fetched (config::revocation
    // soft_fail and hard_fail): OCSP answers by certificate and CRLs by
    // URL, each kept until its nextUpdate; a handle of one word, its copies
    // the same cache, safe from many threads. A config without one shares
    // the process's
    class revocation_cache {
    public:
        // A cache of at most `capacity` answers and lists (0: none kept)
        SGCL_INLINE_HOT explicit revocation_cache(size_t capacity = 256) noexcept
        : _s(make_tracked<detail::RevocationCacheState>(capacity)) {
        }

        // What is held now
        SGCL_INLINE_HOT size_t size() const noexcept {
            return _s->size();
        }

        SGCL_INLINE_HOT size_t capacity() const noexcept {
            return _s->capacity;
        }

        // Everything dropped: the next checks fetch again
        SGCL_INLINE_HOT void clear() const noexcept {
            _s->clear();
        }

    private:
        friend struct detail::RevocationCacheAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT revocation_cache(sgcl::detail::FromWord, const tracked_ptr<detail::RevocationCacheState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::RevocationCacheState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::RevocationCacheState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::RevocationCacheState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const tracked_ptr<RevocationCacheState>& RevocationCacheAccess::state(const revocation_cache& c) noexcept {
            return c._s;
        }

        SGCL_INLINE_HOT const tracked_ptr<SessionCacheState>& SessionCacheAccess::state(const session_cache& c) noexcept {
            return c._s;
        }

        SGCL_INLINE_HOT TicketKeys& TicketKeysAccess::keys(const ticket_keys& k) noexcept {
            return *k._s->keys;
        }

        SGCL_INLINE_HOT tracked_ptr<const void> TicketKeysAccess::word(const ticket_keys& k) noexcept {
            return tracked_ptr<const void>(k._s);
        }
    }

    // What a client's hello asks a server for: the name it connects to
    // (SNI) and the protocols it offers (ALPN), as Go's ClientHelloInfo
    // gives them to GetCertificate
    struct client_hello {
        string server_name;                                     // empty: the client sent none
        vector<string> alpn;                                    // in the client's order; empty: none offered
    };

    // A server's identity for a hello, chosen per connection (Go's
    // GetCertificate): an identity, or an error that ends the handshake
    using identity_function = function<async::task<expected<tls::identity, io::error>>(const client_hello&)>;

    // The settings of a connection: a value, copied freely (the copies
    // share the identities, the cache and the ticket keys)
    struct config {
        string server_name;                                     // SNI and the name checked; empty: the address's host
        optional<crypto::x509::certificate_pool> roots;         // none: the system's
        vector<tls::identity> identities;                       // a server's; a client's, sent when the server asks
        vector<tls::group> groups = {group::x25519_mlkem768, group::x25519, group::secp256r1, group::secp384r1};
        vector<tls::cipher> ciphers = {cipher::aes_128_gcm_sha256, cipher::chacha20_poly1305_sha256, cipher::aes_256_gcm_sha384,
                                       cipher::ecdhe_ecdsa_aes_128_gcm_sha256, cipher::ecdhe_rsa_aes_128_gcm_sha256,
                                       cipher::ecdhe_ecdsa_chacha20_poly1305_sha256, cipher::ecdhe_rsa_chacha20_poly1305_sha256,
                                       cipher::ecdhe_ecdsa_aes_256_gcm_sha384, cipher::ecdhe_rsa_aes_256_gcm_sha384};
        tls::version min_version = version::tls12;              // the oldest a client offers (a server: 1.3 alone)
        tls::version max_version = version::tls13;              // the newest
        vector<string> alpn;                                    // offered, in order of preference
        bool insecure_skip_verify = false;                      // the server's chain taken unchecked (tests only)
        duration handshake_timeout = 10 * second;               // the connect and the handshake together
        tls::client_auth client_auth = client_auth::none;       // a server's: whether it asks for a client certificate
        optional<crypto::x509::certificate_pool> client_roots;  // a server's: what a client's chain must lead to; none: the system's
        optional<tls::session_cache> session_cache;             // a client's: the sessions it resumes; none: no resumption
        bool session_tickets = true;                            // a server's: tickets issued and resumed; a client's: TLS 1.2's asked for
        duration ticket_lifetime = 24 * hour;                   // a server's: how long a ticket resumes, at most 7 days
        tls::ticket_keys ticket_keys;                           // a server's: what its tickets are sealed under
        tls::revocation_mode revocation = revocation_mode::off; // the peer's chain checked for revocation, and how
        duration revocation_timeout = 5 * second;               // the online checks of one handshake, together
        vector<crypto::x509::revocation_list> crls;             // CRLs of the program's, checked first
        bool fetch_crls = true;                                 // soft_fail, hard_fail: the CRLs of the distribution points fetched
        optional<tls::revocation_cache> revocation_cache;       // what the online checks fetched; none: the process's
        bool ocsp_stapling = false;                             // a server's: its identities' OCSP responses fetched, stapled, refreshed
        identity_function identity_for;                         // a server's: its identity for each hello, in place of `identities`; empty: none
        vector<byte> ech_config_list;                           // a client's: the server's ECHConfigList (RFC 9849, DNS's HTTPS "ech"); empty: no ECH
        vector<tls::ech_key> ech_keys;                          // a server's: the keys of Encrypted Client Hello; empty: no ECH
        bool ech_from_dns = false;                              // a client's: with ech_config_list empty, the list of the server name's DNS HTTPS record; none: no ECH
        net::dns::options ech_dns;                              // a client's: how that record is looked up; empty: /etc/resolv.conf's servers
    };

    // What the handshake settled
    struct state {
        tls::version version = version::tls13;
        tls::cipher cipher = cipher::aes_128_gcm_sha256;
        tls::group group = group::x25519;
        string server_name;
        string alpn;                                            // empty: none
        crypto::x509::chain peer_certificates;                  // the leaf first, as sent (a client's: empty without mTLS)
        bool resumed = false;                                   // a session resumed, no certificate exchanged
        optional<crypto::x509::revocation_status> revocation;   // the peer's chain's as checked; none: not checked
        tls::revocation_source revocation_source = revocation_source::none;   // where the leaf's came from
        bool ech_accepted = false;                              // the hello answered was the encrypted ClientHelloInner (RFC 9849)
    };

    namespace detail {
        // The client's settings of a config: the shares of the first group
        // (and of X25519 beside the hybrid, as Go and the browsers send
        // them), compatibility mode on (§D.4), no record_size_limit
        inline ClientSettings client_settings(const config& c) noexcept {
            ClientSettings s;
            s.server_name = c.server_name;
            s.roots = c.roots;
            s.insecure_skip_verify = c.insecure_skip_verify;
            // the versions offered: those of min_version..max_version with
            // a suite of their own in the list; the suites of those
            bool has13 = false, has12 = false;
            for (auto x : c.ciphers) {
                has13 |= known(Cipher(x));
                has12 |= known12(Cipher(x));
            }
            s.tls13 = has13 && c.max_version >= version::tls13 && c.min_version <= version::tls13;
            s.tls12 = has12 && c.min_version <= version::tls12;
            s.ciphers.clear();
            for (auto x : c.ciphers) {
                if ((s.tls13 && known(Cipher(x))) || (s.tls12 && known12(Cipher(x)))) {
                    s.ciphers.push_back(uint16_t(x));
                }
            }
            s.groups.clear();
            s.key_shares.clear();
            bool x25519 = false;
            for (auto g : c.groups) {
                s.groups.push_back(uint16_t(g));
                x25519 |= g == group::x25519;
            }
            if (!c.groups.empty()) {
                s.key_shares.push_back(uint16_t(c.groups[0]));
                if (c.groups[0] == group::x25519_mlkem768 && x25519) {
                    s.key_shares.push_back(uint16_t(group::x25519));
                }
            }
            s.alpn = c.alpn;
            s.status_request = c.revocation != revocation_mode::off && !c.insecure_skip_verify;
            if (!c.ech_config_list.empty()) {
                // ECH (RFC 9849): the first config the module can use, TLS 1.3 alone
                s.ech = choose_ech(bytes_of(c.ech_config_list.data(), c.ech_config_list.size()));
                s.tls12 = false;
                if (s.tls13) {
                    s.ciphers.clear();
                    for (auto x : c.ciphers) {
                        if (known(Cipher(x))) {
                            s.ciphers.push_back(uint16_t(x));
                        }
                    }
                }
            }
            return s;
        }

        inline io::error config_error(const char* what) noexcept {
            return io::error(std::make_error_code(std::errc::invalid_argument), "tls", string(what));
        }

        // The identities of a config as the machines sign with them: their
        // chains as DER, their keys in place (`keep` holds the states the
        // keys live in)
        inline void identities_of(const vector<tls::identity>& ids, vector<ServerIdentity>& out, vector<tracked_ptr<const void>>& keep) noexcept {
            for (const auto& id : ids) {
                const IdentityState& st = IdentityAccess::state(id);
                std::vector<std::vector<byte>> chain;
                for (const auto& cert : st.certificates) {
                    auto der = cert.raw();
                    chain.emplace_back(der.data(), der.data() + der.size());
                }
                const IdentityKey& k = *st.key;
                if (k.ed25519) {
                    out.push_back(identity_of(chain, *k.ed25519));
                } else if (k.p256) {
                    out.push_back(identity_of(chain, *k.p256));
                } else if (k.p384) {
                    out.push_back(identity_of(chain, *k.p384));
                } else if (k.p521) {
                    out.push_back(identity_of(chain, *k.p521));
                } else {
                    out.push_back(identity_of(chain, *k.rsa));
                }
                out.back().staple = &st.staple;
                keep.push_back(IdentityAccess::word(id));
            }
        }

        // The check of the peer's chain the config asks for after a
        // handshake (none for revocation_mode::off, for a chain not
        // verified — insecure_skip_verify — and for a resumed session,
        // whose certificates were checked when it was made)
        // The fields of a config the check reads, apart: a server keeps them
        // for each handshake without a copy of the whole config
        struct RevocationConfig {
            tls::revocation_mode revocation = revocation_mode::off;
            duration revocation_timeout = 5 * second;
            vector<crypto::x509::revocation_list> crls;
            bool fetch_crls = true;
            optional<tls::revocation_cache> revocation_cache;
        };

        inline RevocationConfig revocation_config(const config& c) noexcept {
            RevocationConfig r;
            r.revocation = c.revocation;
            r.revocation_timeout = c.revocation_timeout;
            r.crls = c.crls;
            r.fetch_crls = c.fetch_crls;
            r.revocation_cache = c.revocation_cache;
            return r;
        }

        template<class C>
        RevocationCheck revocation_check(const C& c, const crypto::x509::chain& verified, const std::vector<byte>& staple, time_point deadline) {
            RevocationCheck in;
            in.chain = verified;
            in.staple = vector<byte>(staple.data(), staple.data() + staple.size());
            in.mode = RevocationMode(uint8_t(c.revocation));
            in.crls = c.crls;
            in.fetch_crls = c.fetch_crls;
            in.cache = c.revocation_cache ? RevocationCacheAccess::state(*c.revocation_cache) : process_revocation_cache();
            in.deadline = std::min(deadline, sgcl::clock::now() + std::chrono::nanoseconds(c.revocation_timeout));
            return in;
        }

        // What the check found, applied: the status kept for state_of, or
        // the connection ended with the check's alert and its error a
        // certificate failure ("tls: certificate is revoked")
        inline expected<void, io::error> apply_revocation(TlsImpl& impl, const RevocationOutcome& o) noexcept {
            if (!o.ok) {
                impl.abort(o.alert);
                return unexpected(certificate_error(o.reason, "handshake", impl.describe()));
            }
            impl.set_revocation(o.status, uint8_t(o.source));
            return {};
        }

        SGCL_INLINE_HOT bool client_checks_revocation(const config& c, const TlsImpl& impl) noexcept {
            return c.revocation != revocation_mode::off && !impl.result().resumed && !impl.result().verified_chain.empty();
        }

        // Everything a client's handshake over this transport needs: the
        // settings with the identities and the session to offer (taken from
        // the cache, of the server's name, the transport's port and the
        // ALPN list), what keeps the keys, the cache and its key
        struct ClientSetup {
            ClientSettings settings;
            vector<tracked_ptr<const void>> keep;
            tracked_ptr<SessionCacheState> cache;
            string key;
        };

        inline ClientSetup client_setup(const config& c, const net::connection& transport) noexcept {
            ClientSetup s;
            s.settings = client_settings(c);
            identities_of(c.identities, s.settings.identities, s.keep);
            if (c.session_cache && c.ech_config_list.empty()) {
                s.cache = SessionCacheAccess::state(*c.session_cache);
                s.key = session_key(c.server_name, transport.remote_endpoint().port(), c.alpn);
                s.settings.session = s.cache->take(s.key, time::now().unix_milli());
                s.settings.resumption = true;
                s.settings.tickets12 = c.session_tickets;
            }
            return s;
        }

        SGCL_INLINE_HOT bool valid(tls::version v) noexcept {
            return v == version::tls12 || v == version::tls13;
        }

        inline optional<io::error> check_client(const config& c) noexcept {
            if (c.ciphers.empty() || c.groups.empty()) {
                return config_error("a config without cipher suites or groups");
            }
            if (!valid(c.min_version) || !valid(c.max_version) || c.min_version > c.max_version) {
                return config_error("a config whose min_version and max_version are no range of TLS 1.2 and 1.3");
            }
            const ClientSettings s = client_settings(c);
            if (!s.tls13 && !s.tls12) {
                return config_error("a config with no version of min_version to max_version that has a cipher suite of its list");
            }
            if (!s.tls13) {
                bool curve = false;
                for (auto g : c.groups) {
                    curve |= g == group::x25519 || g == group::secp256r1 || g == group::secp384r1 || g == group::secp521r1;
                }
                if (!curve) {
                    return config_error("a config of TLS 1.2 alone without a group of it (X25519, P-256, P-384, P-521)");
                }
            }
            if (c.server_name.empty() && !c.insecure_skip_verify) {
                return config_error("a config without a server name to verify");
            }
            if (!c.ech_config_list.empty()) {
                if (!s.tls13) {
                    return config_error("an ECH config list with a config that does not offer TLS 1.3");
                }
                if (!s.ech) {
                    // the list's own fault, as identity::from_pem reports a PEM's
                    const bool reads = bool(read_ech_configs(bytes_of(c.ech_config_list.data(), c.ech_config_list.size())));
                    return io::error(reads ? crypto::errc::unsupported : crypto::errc::malformed, "tls",
                                     string(reads ? "an ECH config list with no config of a KEM, a suite and a public name the module takes"
                                                  : "an ECH config list that does not read"));
                }
            }
            if (uint8_t(c.revocation) > uint8_t(revocation_mode::hard_fail)) {
                return config_error("a revocation of no value of its enumeration");
            }
            for (auto& p : c.alpn) {
                if (p.empty() || p.size() > 255) {
                    return config_error("an ALPN protocol of 0 or more than 255 bytes");
                }
            }
            return nullopt;
        }

        // An ech_required error with the server's retry_configs, when it
        // sent some (net::tls::ech_retry_configs reads them back)
        inline io::error ech_rejection(const io::error& e, const std::vector<byte>& retry) {
            if (retry.empty() || e.code() != make_error_code(alert::ech_required)) {
                return e;
            }
            string b64 = encoding::base64::standard.encode(bytes_of(retry.data(), retry.size()));
            return io::error(e.code(), e.op(), e.path() + string(EchRetryMarker) + b64, e.count());
        }

        // config::ech_from_dns: the ECHConfigList of the server name's HTTPS
        // records (RFC 9460, RFC 9848), looked up when the config asks for it,
        // has no list of its own and offers TLS 1.3, for a name that is no
        // address. The list is the first ServiceMode record's, in their
        // order, whose "ech" holds a config the module can use and whose
        // mandatory keys are all keys dns reads into fields (1..7); a lookup
        // that fails or finds none gives none, and the handshake goes on
        // without ECH, as without a record (RFC 9849 §6.1)
        SGCL_INLINE_HOT bool wants_dns_ech(const config& c) noexcept {
            return c.ech_from_dns && c.ech_config_list.empty() && !c.server_name.empty() && client_settings(c).tls13
                && !net::detail::IpText::parse(net::detail::IpText::view(c.server_name));
        }

        inline vector<byte> ech_of_records(const expected<vector<net::dns::svcb>, io::error>& found) noexcept {
            if (!found) {
                return {};
            }
            for (const auto& r : *found) {
                if (r.priority == 0 || r.ech.empty()) {
                    continue;
                }
                bool known = true;
                for (uint16_t k : r.mandatory) {
                    known &= k >= 1 && k <= 7;
                }
                if (known && choose_ech(bytes_of(r.ech.data(), r.ech.size()))) {
                    return r.ech;
                }
            }
            return {};
        }

        // The name asked (RFC 9460 §9.1): the server name for port 443,
        // "_port._https.name" for another; absolute, so that no search
        // domain makes it another host's
        inline string ech_query_name(const string& server_name, uint16_t port) noexcept {
            std::string q;
            if (port != 443) {
                q = "_" + std::to_string(port) + "._https.";
            }
            q += server_name.view();
            if (q.back() != '.') {
                q += '.';
            }
            return string(std::string_view(q));
        }

        // The lookup, given up at half the time left to the handshake's
        // deadline: a DNS server that does not answer costs ECH, never the
        // connection
        inline async::task<vector<byte>> co_ech_from_dns(string server_name, uint16_t port, net::dns::options o, time_point deadline) noexcept {
            async::stop_source bound;
            const auto left = deadline - sgcl::clock::now();
            bound.stop_after(left > decltype(left)::zero() ? duration(left / 2) : duration());
            auto found = co_await net::dns::async_lookup_https(ech_query_name(server_name, port), o, bound.token());
            co_return ech_of_records(found);
        }

        inline expected<net::connection, io::error> block_client(const net::connection& transport, const config& given, time_point deadline) {
            if (auto e = check_client(given)) {
                return unexpected(*e);
            }
            optional<config> looked;
            if (wants_dns_ech(given)) {
                looked = given;
                looked->ech_config_list = co_ech_from_dns(given.server_name, transport.remote_endpoint().port(), given.ech_dns, deadline).wait();
            }
            const config& c = looked ? *looked : given;
            auto setup = client_setup(c, transport);
            auto impl = make_tracked<TlsImpl>(transport, setup.settings, setup.keep, setup.cache, setup.key);
            auto r = impl->handshake(deadline);
            if (!r) {
                (void)transport.close();
                if (impl->result().ech_rejected) {
                    return unexpected(ech_rejection(r.error(), impl->result().ech_retry_configs));
                }
                return unexpected(r.error());
            }
            if (client_checks_revocation(c, *impl)) {
                auto o = co_check_revocation(revocation_check(c, impl->result().verified_chain, impl->result().ocsp_staple, deadline)).wait();
                if (auto a = apply_revocation(*impl, o); !a) {
                    return unexpected(a.error());
                }
            }
            return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        inline async::task<expected<net::connection, io::error>> co_client(net::connection transport, config c, time_point deadline) noexcept {
            if (auto e = check_client(c)) {
                co_return unexpected(*e);
            }
            if (wants_dns_ech(c)) {
                c.ech_config_list = co_await co_ech_from_dns(c.server_name, transport.remote_endpoint().port(), c.ech_dns, deadline);
            }
            auto setup = client_setup(c, transport);
            auto impl = make_tracked<TlsImpl>(transport, setup.settings, setup.keep, setup.cache, setup.key);
            auto r = co_await impl->async_handshake(deadline);
            if (!r) {
                (void)transport.close();
                if (impl->result().ech_rejected) {
                    co_return unexpected(ech_rejection(r.error(), impl->result().ech_retry_configs));
                }
                co_return unexpected(r.error());
            }
            if (client_checks_revocation(c, *impl)) {
                auto o = co_await co_check_revocation(revocation_check(c, impl->result().verified_chain, impl->result().ocsp_staple, deadline));
                if (auto a = apply_revocation(*impl, o); !a) {
                    co_return unexpected(a.error());
                }
            }
            co_return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        // The config with the address's host for a server name when it has none
        SGCL_INLINE_HOT expected<config, io::error> for_address(const string& address, const config& c) noexcept {
            config out = c;
            if (out.server_name.empty()) {
                auto t = net::detail::parse_target(address, "dial tls");
                if (!t) {
                    return unexpected(t.error());
                }
                out.server_name = t->host;
            }
            return out;
        }

        inline async::task<expected<net::connection, io::error>> co_connect(string address, config c) noexcept {
            auto cfg = for_address(address, c);
            if (!cfg) {
                co_return unexpected(cfg.error());
            }
            const time_point deadline = sgcl::clock::now() + c.handshake_timeout;
            // ECH from DNS: the HTTPS record asked beside the TCP connect
            async::task<vector<byte>> ech;
            const bool dns_ech = wants_dns_ech(*cfg);
            if (dns_ech) {
                auto target = net::detail::parse_target(address, "dial tls");
                ech = async::spawn(co_ech_from_dns(cfg->server_name, target ? target->port : uint16_t(443), cfg->ech_dns, deadline));
            }
            auto t = co_await net::tcp::async_connect(address, c.handshake_timeout);
            if (!t) {
                co_return unexpected(t.error());   // the lookup, dropped, ends at its own bound
            }
            if (dns_ech) {
                cfg->ech_config_list = co_await ech;
                cfg->ech_from_dns = false;   // asked once: co_client does not ask again
            }
            auto r = co_await co_client(*t, *cfg, deadline);
            vector<byte> retry = r ? vector<byte>() : ech_retry_configs(r.error());
            if (r || retry.empty()) {
                co_return r;
            }
            // ECH rejected with retry_configs from a server authenticated for
            // the public name: once more with them (RFC 9849 §6.1.6)
            cfg->ech_config_list = retry;
            auto again = co_await net::tcp::async_connect(address, c.handshake_timeout);
            if (!again) {
                co_return unexpected(again.error());
            }
            co_return co_await co_client(*again, *cfg, deadline);
        }
    }

    // A new TCP connection to "host:port" and the handshake over it, both
    // within config::handshake_timeout
    // `connect(...)` on this thread, `co_await async_connect(...)` in a task
    SGCL_INLINE_HOT expected<net::connection, io::error> connect(const string& address, const config& c = {}) {
        return detail::co_connect(address, c).wait();
    }

    SGCL_INLINE_HOT async::task<expected<net::connection, io::error>> async_connect(string address, config c = {}) noexcept {
        return detail::co_connect(std::move(address), std::move(c));
    }

    // The client's handshake over a connection there is (the transport's
    // deadlines are replaced by the handshake's and then removed); the
    // transport is closed when it fails
    // `client(...)` on this thread, `co_await async_client(...)` in a task
    SGCL_INLINE_HOT expected<net::connection, io::error> client(const net::connection& transport, const config& c) {
        return detail::block_client(transport, c, sgcl::clock::now() + c.handshake_timeout);
    }

    SGCL_INLINE_HOT async::task<expected<net::connection, io::error>> async_client(net::connection transport, config c) noexcept {
        const time_point deadline = sgcl::clock::now() + c.handshake_timeout;
        return detail::co_client(std::move(transport), std::move(c), deadline);
    }

    // --- the server ----------------------------------------------------------

    namespace detail {
        // A server's settings: the identities (their chains as DER, their
        // keys signing in place: `keep` holds the states the keys live in,
        // and the ticket keys'), the server's preferences, the client
        // certificate asked for, the tickets, no record_size_limit answered
        struct ServerSetup {
            ServerSettings settings;
            vector<tracked_ptr<const void>> keep;
            RevocationConfig revocation;             // the config's fields of revocation, for a client's chain
            vector<tls::identity> stapled;           // the identities whose OCSP responses the server fetches (ocsp_stapling)
            tracked_ptr<const IdentitySelector> select;          // config::identity_for, when set
        };

        // The OCSP response of an identity's leaf fetched and kept (a task
        // of its own, never the handshake's): the responders of the leaf's
        // AIA, the response verified under the issuer the chain holds; one
        // that fails tried again a minute later, the old staple kept until
        // its nextUpdate
        inline async::task<void> co_refresh_staple(tls::identity id) noexcept {
            const IdentityState& st = IdentityAccess::state(id);
            StapleHolder& holder = IdentityAccess::staple(id);
            auto f = co_await co_fetch_staple(st.certificates[0], st.certificates[1], sgcl::clock::now() + 10 * second);
            if (f) {
                holder.set(std::vector<byte>(f->der.data(), f->der.data() + f->der.size()), f->this_update, f->next_update);
            }
            std::lock_guard<std::mutex> g(holder.lock);
            holder.fetching = false;
            if (!f) {
                holder.retry_at = time::now().unix() + 60;
            }
        }

        // A fetch started for each identity whose staple is due (none yet,
        // or past halfway through its validity) and is not being fetched;
        // an identity without its issuer in the chain, or without an OCSP
        // responder, staples only what the program sets
        inline void refresh_staples(const vector<tls::identity>& ids) noexcept {
            const int64_t now = time::now().unix();
            for (const auto& id : ids) {
                const IdentityState& st = IdentityAccess::state(id);
                if (st.certificates.size() < 2 || st.certificates[0].ocsp_servers().empty()) {
                    continue;
                }
                StapleHolder& holder = IdentityAccess::staple(id);
                {
                    std::lock_guard<std::mutex> g(holder.lock);
                    if (holder.fetching || now < holder.refresh_at || now < holder.retry_at) {
                        continue;
                    }
                    holder.fetching = true;
                }
                async::go(co_refresh_staple(id));
            }
        }

        // The revocation of a client's chain, when the server's config asks
        // for it and the client sent one that verified
        SGCL_INLINE_HOT bool server_checks_revocation(const RevocationConfig& c, const TlsImpl& impl) noexcept {
            return c.revocation != revocation_mode::off && !impl.server_result().resumed && !impl.server_result().verified_chain.empty();
        }

        // config::identity_for as the connection's choice: the identity it
        // gives as the machine signs with it
        inline async::task<expected<SelectedIdentities, io::error>> co_select(identity_function f, HelloInfo info) noexcept {
            client_hello hello;
            hello.server_name = std::move(info.server_name);
            hello.alpn = std::move(info.alpn);
            expected<tls::identity, io::error> id = unexpected(config_error("an identity_for that threw"));
            try {
                id = co_await f(hello);
            } catch (...) {
            }
            if (!id) {
                co_return unexpected(id.error());
            }
            SelectedIdentities out;
            identities_of(vector<tls::identity>{*id}, out.identities, out.keep);
            co_return out;
        }

        inline ServerSetup server_setup(const config& c) noexcept {
            ServerSetup s;
            s.settings.ciphers.clear();
            for (auto x : c.ciphers) {
                s.settings.ciphers.push_back(uint16_t(x));
            }
            s.settings.groups.clear();
            for (auto g : c.groups) {
                s.settings.groups.push_back(uint16_t(g));
            }
            s.settings.alpn = c.alpn;
            identities_of(c.identities, s.settings.identities, s.keep);
            if (c.identity_for) {
                s.select = make_tracked<IdentitySelector>([f = c.identity_for](HelloInfo info) {
                    return co_select(f, std::move(info));
                });
            }
            s.settings.client_auth = uint8_t(c.client_auth);
            s.settings.client_roots = c.client_roots;
            if (c.session_tickets) {
                s.settings.tickets = &TicketKeysAccess::keys(c.ticket_keys);
                s.settings.ticket_lifetime = uint32_t(c.ticket_lifetime.seconds());
                s.keep.push_back(TicketKeysAccess::word(c.ticket_keys));
            }
            if (c.revocation != revocation_mode::off) {
                s.revocation = revocation_config(c);
            }
            if (c.ocsp_stapling) {
                s.stapled = c.identities;
            }
            for (const auto& k : c.ech_keys) {
                const auto& st = EchKeyAccess::state(k);
                EchServerKey e;
                e.config = st.config.raw;
                e.id = st.config.id;
                e.kem = st.config.kem;
                e.suites = st.config.suites;
                e.key = st.key.get();
                s.settings.ech_keys.push_back(std::move(e));
                s.keep.push_back(EchKeyAccess::word(k));
            }
            if (!c.ech_keys.empty()) {
                s.settings.ech_retry_configs = ech_list_of(c.ech_keys, true);
            }
            return s;
        }

        inline optional<io::error> check_server(const config& c) noexcept {
            bool has13 = false;
            for (auto x : c.ciphers) {
                has13 |= known(Cipher(x));
            }
            if (!has13 || c.groups.empty()) {
                return config_error("a server's config without TLS 1.3 cipher suites or groups");
            }
            if (!valid(c.min_version) || !valid(c.max_version) || c.min_version > c.max_version || c.max_version != version::tls13) {
                return config_error("a server's config without TLS 1.3 in min_version to max_version (the server speaks 1.3 alone)");
            }
            if (c.identities.empty() && !c.identity_for) {
                return config_error("a server's config without an identity or an identity_for");
            }
            if (c.session_tickets && (c.ticket_lifetime < second || c.ticket_lifetime > 7 * 24 * hour)) {
                return config_error("a ticket lifetime under a second or past seven days");
            }
            if (c.client_auth != client_auth::none && c.client_auth != client_auth::request && c.client_auth != client_auth::require) {
                return config_error("a client_auth of no value of its enumeration");
            }
            if (uint8_t(c.revocation) > uint8_t(revocation_mode::hard_fail)) {
                return config_error("a revocation of no value of its enumeration");
            }
            for (auto& p : c.alpn) {
                if (p.empty() || p.size() > 255) {
                    return config_error("an ALPN protocol of 0 or more than 255 bytes");
                }
            }
            return nullopt;
        }

        inline async::task<expected<net::connection, io::error>> co_server(net::connection transport, ServerSetup setup, time_point deadline) noexcept {
            refresh_staples(setup.stapled);
            auto impl = make_tracked<TlsImpl>(transport, setup.settings, setup.keep, setup.select);
            auto r = co_await impl->async_handshake(deadline);
            if (!r) {
                (void)transport.close();
                co_return unexpected(r.error());
            }
            if (server_checks_revocation(setup.revocation, *impl)) {
                auto o = co_await co_check_revocation(revocation_check(setup.revocation, impl->server_result().verified_chain, std::vector<byte>(), deadline));
                if (auto a = apply_revocation(*impl, o); !a) {
                    co_return unexpected(a.error());
                }
            }
            co_return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
        }

        // A listener whose accept gives connections after their handshake:
        // an accept loop over the listener inside, each connection's
        // handshake in a task of its own within handshake_timeout (a slow
        // client holds no other up), the ready ones through a channel; a
        // handshake that fails is dropped (its connection closed, counted)
        class TlsListenerImpl final : public net::detail::ListenerImpl {
        public:
            SGCL_INLINE_HOT TlsListenerImpl(const net::listener& inner, ServerSetup setup, duration timeout) noexcept
            : _inner(inner)
            , _setup(std::move(setup))
            , _timeout(timeout)
            , _ready(16) {
            }

            // The accept loop, started by listen() once the object is made
            SGCL_INLINE_HOT static void start(const tracked_ptr<TlsListenerImpl>& self) {
                async::go(_loop(self));
            }

            expected<net::connection, io::error> _block_accept() override {
                auto c = _ready.receive().wait();
                if (!c) {
                    return unexpected(net::detail::closed_error("accept", describe()));
                }
                return *c;
            }

            async::task<expected<net::connection, io::error>> _co_accept() noexcept override {
                auto c = co_await _ready.receive();
                if (!c) {
                    co_return unexpected(net::detail::closed_error("accept", describe()));
                }
                co_return *c;
            }

            expected<void, io::error> close() noexcept override {
                auto r = _inner.close();
                _ready.close();
                // the connections ready and not taken: what the closed
                // channel still holds, taken without a wait
                while (auto c = _ready.try_receive()) {
                    (void)c->close();
                }
                return r;
            }

            bool is_closed() const noexcept override {
                return _inner.is_closed();
            }

            endpoint local_endpoint() const noexcept override {
                return _inner.local_endpoint();
            }

            string path() const noexcept override {
                return _inner.path();
            }

            string describe() const noexcept override {
                return string("tls ") + _inner.local_endpoint().to_string();
            }

            // The handshakes that failed (timed out, refused, broken)
            SGCL_INLINE_HOT uint64_t failed() const noexcept {
                return _failed.load(std::memory_order_relaxed);
            }

        private:
            static async::task<void> _loop(tracked_ptr<TlsListenerImpl> self) noexcept {
                for (;;) {
                    auto c = co_await self->_inner.async_accept();
                    if (!c) {
                        if (self->_inner.is_closed()) {
                            self->_ready.close();
                            co_return;
                        }
                        if (async::detail::runtime_exiting()) {
                            co_return;   // the end of the program (async/scheduler.h: runtime_exit): an accept tried again would end the same way at once
                        }
                        continue;   // a connection that failed on its way in
                    }
                    async::go(_handshake(self, *c));
                }
            }

            static async::task<void> _handshake(tracked_ptr<TlsListenerImpl> self, net::connection c) noexcept {
                auto r = co_await co_server(c, self->_setup, sgcl::clock::now() + self->_timeout);
                if (!r) {
                    self->_failed.fetch_add(1, std::memory_order_relaxed);
                    co_return;   // dropped: co_server closed the transport
                }
                if (!co_await self->_ready.send(*r)) {
                    (void)r->close();   // the listener closed meanwhile
                }
            }

            net::listener _inner;
            ServerSetup _setup;
            duration _timeout;
            async::channel<net::connection> _ready;
            std::atomic<uint64_t> _failed = 0;
        };

        SGCL_INLINE_HOT expected<net::listener, io::error> make_listener(const net::listener& inner, const config& c) {
            tracked_ptr<TlsListenerImpl> impl = make_tracked<TlsListenerImpl>(inner, server_setup(c), c.handshake_timeout);
            if (c.ocsp_stapling) {
                refresh_staples(c.identities);   // fetched now, for the first connections
            }
            TlsListenerImpl::start(impl);
            return net::detail::ListenerAccess::make(tracked_ptr<net::detail::ListenerImpl>(std::move(impl)));
        }
    }

    // A TCP listener whose accept() gives connections whose handshake is
    // done: each handshake runs in a task of its own, bounded by
    // config::handshake_timeout, and one that fails is dropped
    // `listen(...)` on this thread, `co_await async_listen(...)` in a task
    SGCL_INLINE_HOT expected<net::listener, io::error> listen(const string& address, const config& c) {
        if (auto e = detail::check_server(c)) {
            return unexpected(*e);
        }
        auto l = net::tcp::listen(address);
        if (!l) {
            return unexpected(l.error());
        }
        return detail::make_listener(*l, c);
    }

    inline async::task<expected<net::listener, io::error>> async_listen(string address, config c) noexcept {
        if (auto e = detail::check_server(c)) {
            co_return unexpected(*e);
        }
        auto l = co_await net::tcp::async_listen(address);
        if (!l) {
            co_return unexpected(l.error());
        }
        co_return detail::make_listener(*l, c);
    }



    // The server's handshake over a connection there is (one accepted);
    // the transport is closed when it fails
    // `server(...)` on this thread, `co_await async_server(...)` in a task
    inline expected<net::connection, io::error> server(const net::connection& transport, const config& c) {
        if (auto e = detail::check_server(c)) {
            return unexpected(*e);
        }
        auto setup = detail::server_setup(c);
        detail::refresh_staples(setup.stapled);
        const time_point deadline = sgcl::clock::now() + c.handshake_timeout;
        auto impl = make_tracked<detail::TlsImpl>(transport, setup.settings, setup.keep, setup.select);
        auto r = impl->handshake(deadline);
        if (!r) {
            (void)transport.close();
            return unexpected(r.error());
        }
        if (detail::server_checks_revocation(setup.revocation, *impl)) {
            auto o = detail::co_check_revocation(detail::revocation_check(setup.revocation, impl->server_result().verified_chain, std::vector<byte>(), deadline)).wait();
            if (auto a = detail::apply_revocation(*impl, o); !a) {
                return unexpected(a.error());
            }
        }
        return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(std::move(impl)));
    }

    inline async::task<expected<net::connection, io::error>> async_server(net::connection transport, config c) noexcept {
        if (auto e = detail::check_server(c)) {
            co_return unexpected(*e);
        }
        co_return co_await detail::co_server(std::move(transport), detail::server_setup(c), sgcl::clock::now() + c.handshake_timeout);
    }

    // What the handshake of a TLS connection settled; nullopt for a
    // connection without TLS
    inline optional<state> state_of(const net::connection& c) noexcept {
        if (!c) {
            return nullopt;
        }
        auto* t = dynamic_cast<const detail::TlsImpl*>(&net::detail::ConnectionAccess::impl(c));
        if (!t) {
            return nullopt;
        }
        state s;
        if (t->is_server()) {
            const auto& r = t->server_result();
            s.cipher = tls::cipher(uint16_t(r.cipher));
            s.group = tls::group(uint16_t(r.group));
            s.server_name = r.server_name;
            s.alpn = r.alpn;
            s.peer_certificates = r.peer_certificates;
            s.resumed = r.resumed;
            s.revocation = t->revocation();
            s.revocation_source = tls::revocation_source(t->revocation_source());
            s.ech_accepted = r.ech_accepted;
            return s;
        }
        const auto& r = t->result();
        s.version = tls::version(r.version);
        s.cipher = tls::cipher(uint16_t(r.cipher));
        s.group = tls::group(uint16_t(r.group));
        s.server_name = r.server_name;
        s.alpn = r.alpn;
        s.peer_certificates = r.peer_certificates;
        s.resumed = r.resumed;
        s.revocation = t->revocation();
        s.revocation_source = tls::revocation_source(t->revocation_source());
        s.ech_accepted = r.ech_accepted;
        return s;
    }
}

#include "detail/dns_tls.h"   // DNS over TLS for net::dns: the resolver's transport of a "tls://" server
