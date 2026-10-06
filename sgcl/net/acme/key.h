//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../tls.h"
#include "../../core/detail/handle_word.h"
#include "../../crypto/hmac.h"
#include "../../crypto/jose.h"
#include "../../crypto/sha256.h"
#include "../../crypto/sha512.h"
#include "../../crypto/detail/key_pem.h"
#include "../../encoding/base64.h"
#include "../../encoding/json.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// The key of an ACME account and the JSON Web Signatures made with it
// (RFC 7515, in the flattened JSON form RFC 8555 §6.2 asks for), through
// crypto::jose: ES256 over P-256 (the default, what Let's Encrypt and Go's
// autocert use), ES384 over P-384, EdDSA over Ed25519 (RFC 8037) where the
// CA takes it, RS256 for an existing RSA key. The public key as a JWK (RFC
// 7517) in RFC 7638's canonical text and its thumbprint, which every key
// authorization of a challenge is made of.
namespace sgcl::net::acme {
    // The JWS algorithm of a key (RFC 7518 §3.1, RFC 8037 §3.1)
    enum class key_algorithm : uint8_t {
        es256,   // ECDSA P-256 with SHA-256
        es384,   // ECDSA P-384 with SHA-384
        eddsa,   // Ed25519
        rs256,   // RSA PKCS #1 v1.5 with SHA-256
    };

    class account_key;

    namespace detail {
        using tls::detail::IdentityKey;

        // The key (a jose::jwk: its private half in plain memory, zeroed by
        // its own destructor when the last handle's state is collected) and
        // what is made of its public half once: the JWK in RFC 7638's
        // canonical text, the JWK as a JSON value for the headers, the
        // thumbprint, the algorithm's name
        struct KeyState {
            crypto::jose::jwk key;
            string jwk;
            encoding::json jwk_value;
            string thumbprint;
            const char* alg = "ES256";

            explicit KeyState(const crypto::jose::jwk& k) noexcept
            : key(k) {
            }
        };

        struct KeyAccess {
            static const KeyState& state(const account_key& k) noexcept;
        };

        SGCL_INLINE_HOT string b64(const slice<const byte>& data) {
            return encoding::base64::raw_url.encode(data);
        }

        SGCL_INLINE_HOT string b64(std::string_view text) {
            return encoding::base64::raw_url.encode(slice<const byte>(reinterpret_cast<const byte*>(text.data()), text.size()));
        }

        template<class A>
        SGCL_INLINE_HOT slice<const byte> bytes_of(const A& a) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(a.data()), a.size());
        }

        // The public JWK of a key, its required members in lexicographic
        // order with no white space: the text RFC 7638 §3 hashes for the
        // thumbprint, the members as crypto::jose writes them
        inline string jwk_of(const crypto::jose::jwk& k) {
            auto p = k.parameters();
            auto m = [&](const char* name) {
                return p[name].as_string(string());
            };
            if (k.type() == crypto::jose::key_type::rsa) {
                return string::concat("{\"e\":\"", m("e"), "\",\"kty\":\"RSA\",\"n\":\"", m("n"), "\"}");
            }
            if (k.type() == crypto::jose::key_type::okp) {
                return string::concat("{\"crv\":\"", m("crv"), "\",\"kty\":\"OKP\",\"x\":\"", m("x"), "\"}");
            }
            return string::concat("{\"crv\":\"", m("crv"), "\",\"kty\":\"EC\",\"x\":\"", m("x"), "\",\"y\":\"", m("y"), "\"}");
        }

        SGCL_INLINE_HOT const char* alg_of(const crypto::jose::jwk& k) noexcept {
            if (k.type() == crypto::jose::key_type::rsa) {
                return "RS256";
            }
            if (k.type() == crypto::jose::key_type::okp) {
                return "EdDSA";
            }
            return k.crv().view() == "P-384" ? "ES384" : "ES256";
        }

        // A JWS in the flattened JSON serialization (RFC 7515 §7.2.2) of a
        // payload (the empty payload of a POST-as-GET, RFC 8555 §6.3, is
        // ""), signed by crypto::jose: the protected header is alg, then the
        // header's members (ECDSA as R || S of fixed size, RFC 7518 §3.4)
        inline string jws(const KeyState& k, const encoding::json& header, const string& payload) {
            return crypto::jose::jws::sign_json(payload, k.key, {.header = header});
        }

        // The protected header of a request (RFC 8555 §6.2) less its alg,
        // which the signer writes: the account's URL as kid, or the JWK
        // where there is no account yet (newAccount, a revocation by the
        // certificate's key); the nonce; the URL
        inline encoding::json header(const KeyState& k, const string& kid, const string& nonce, const string& url) {
            using encoding::json;
            if (kid.empty()) {
                return json::object({{"jwk", k.jwk_value}, {"nonce", nonce}, {"url", url}});
            }
            return json::object({{"kid", kid}, {"nonce", nonce}, {"url", url}});
        }

        // The external account binding of a newAccount (RFC 8555 §7.3.4):
        // a JWS of the account's JWK under the CA's MAC key, HS256, the
        // key's id as kid, the newAccount URL. Made here rather than by
        // crypto::jose, which refuses an HMAC key shorter than its digest
        // (RFC 7518 §3.2): RFC 8555 sets no length, and a CA's key may be
        // shorter
        inline string eab(const KeyState& k, const string& key_id, const slice<const byte>& mac_key, const string& url) {
            using encoding::json;
            string h = b64(json::object({{"alg", "HS256"}, {"kid", key_id}, {"url", url}}).to_string().view());
            string p = b64(k.jwk.view());
            string input = string::concat(h, ".", p);
            auto tag = crypto::hmac<crypto::sha256>::of(bytes_of(input.view()), mac_key);
            return string::concat("{\"protected\":\"", h, "\",\"payload\":\"", p, "\",\"signature\":\"", b64(bytes_of(tag)), "\"}");
        }

        inline tracked_ptr<KeyState> make_key_state(const crypto::jose::jwk& key) {
            auto s = make_tracked<KeyState>(key);
            s->jwk = jwk_of(key);
            s->jwk_value = *encoding::json::parse(s->jwk);
            s->thumbprint = key.thumbprint();
            s->alg = alg_of(key);
            return s;
        }

        // The jose::jwk of an identity's key, a clone of it
        inline crypto::jose::jwk jwk_of_identity(const IdentityKey& k) {
            if (k.p256) {
                return crypto::jose::jwk(k.p256->clone());
            }
            if (k.p384) {
                return crypto::jose::jwk(k.p384->clone());
            }
            if (k.ed25519) {
                return crypto::jose::jwk(k.ed25519->clone());
            }
            return crypto::jose::jwk(k.rsa->clone());
        }

        inline io::error key_error(const char* what) noexcept {
            return io::error(crypto::errc::malformed, "acme key", string(what));
        }

        // The bits of an RSA JWK's modulus (written without leading zeros)
        inline size_t rsa_bits(const crypto::jose::jwk& k) noexcept {
            auto n = encoding::base64::raw_url.decode(k.parameters()["n"].as_string(string()));
            if (!n || n->empty()) {
                return 0;
            }
            size_t bits = n->size() * 8;
            for (unsigned b = unsigned((*n)[0]); b < 0x80 && bits > 0; b <<= 1) {
                --bits;
            }
            return bits;
        }
    }

    // The key of an ACME account: a handle of one word, its copies the same
    // key, safe from many threads; the private key lives in unmanaged
    // memory and is never copied, zeroed by its own destructor when the
    // last handle's state is collected. A new one is P-256 (ES256) unless
    // asked otherwise; one read from PEM keeps its kind
    class account_key {
    public:
        // A new P-256 key
        SGCL_INLINE_HOT account_key()
        : account_key(key_algorithm::es256) {
        }

        // A new key of the algorithm (RS256: 2048 bits)
        explicit account_key(key_algorithm a)
        : _s(detail::make_key_state(_generate(a))) {
        }

        // A key in PEM (PKCS #8 "PRIVATE KEY" of the four kinds, SEC 1 "EC
        // PRIVATE KEY", PKCS #1 "RSA PRIVATE KEY"), the first key block of
        // the text; its bytes read where they lie (a secret_bytes of
        // crypto::read_secret). crypto::errc::malformed for anything else
        static expected<account_key, io::error> from_pem(const slice<const byte>& pem) noexcept {
            auto block = crypto::detail::read_key_pem(pem);
            if (!block) {
                return unexpected(io::error(block.error().code(), "acme key", block.error().message()));
            }
            auto k = crypto::detail::JwkAccess::from_pem(pem);
            if (!k || k->crv().view() == "X25519") {
                return unexpected(detail::key_error("no private key of a kind ACME signs with (P-256, P-384, Ed25519, RSA)"));
            }
            if (k->type() == crypto::jose::key_type::rsa && detail::rsa_bits(*k) < 2048) {
                return unexpected(detail::key_error("an RSA key under 2048 bits"));
            }
            return account_key(detail::make_key_state(*k));
        }

        // The key in PEM, "PRIVATE KEY" over its PKCS #8: a secret_bytes,
        // never managed memory, for io::write_file with permissions 0600
        crypto::secret_bytes to_pem() const {
            return _s->key.to_pem();
        }

        SGCL_INLINE_HOT key_algorithm algorithm() const noexcept {
            const std::string_view a = _s->alg;
            return a == "ES256" ? key_algorithm::es256 : a == "ES384" ? key_algorithm::es384 : a == "EdDSA" ? key_algorithm::eddsa : key_algorithm::rs256;
        }

        // The public key as a JWK (RFC 7517), its members in the order of
        // RFC 7638: {"crv":"P-256","kty":"EC","x":"…","y":"…"}
        SGCL_INLINE_HOT const string& jwk() const noexcept {
            return _s->jwk;
        }

        // The JWK thumbprint (RFC 7638): SHA-256 of jwk(), base64url
        SGCL_INLINE_HOT const string& thumbprint() const noexcept {
            return _s->thumbprint;
        }

        // The key authorization of a challenge's token (RFC 8555 §8.1):
        // token "." thumbprint
        SGCL_INLINE_HOT string key_authorization(const string& token) const {
            return string::concat(token, ".", _s->thumbprint);
        }

        // The same key
        SGCL_INLINE_HOT friend bool operator==(const account_key& a, const account_key& b) noexcept {
            return a._s == b._s || a._s->jwk == b._s->jwk;
        }

    private:
        friend struct detail::KeyAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit account_key(tracked_ptr<detail::KeyState> s) noexcept
        : _s(std::move(s)) {
        }

        static crypto::jose::jwk _generate(key_algorithm a) {
            switch (a) {
                case key_algorithm::es256: return crypto::jose::jwk(crypto::p256::private_key::generate());
                case key_algorithm::es384: return crypto::jose::jwk(crypto::p384::private_key::generate());
                case key_algorithm::eddsa: return crypto::jose::jwk(crypto::ed25519::private_key::generate());
                case key_algorithm::rs256: return crypto::jose::jwk(crypto::rsa::private_key::generate(2048));
            }
            throw std::invalid_argument("sgcl::net::acme::account_key: an algorithm of no value of its enumeration");
        }

        SGCL_INLINE_HOT account_key(sgcl::detail::FromWord, const tracked_ptr<detail::KeyState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::KeyState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::KeyState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::KeyState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const KeyState& KeyAccess::state(const account_key& k) noexcept {
            return *k._s;
        }
    }
}
