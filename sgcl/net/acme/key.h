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
// (RFC 7515, in the flattened JSON form RFC 8555 §6.2 asks for): ES256 over
// P-256 (the default, what Let's Encrypt and Go's autocert use), ES384 over
// P-384, EdDSA over Ed25519 (RFC 8037) where the CA takes it, RS256 for an
// existing RSA key. The public key as a JWK (RFC 7517) and its thumbprint
// (RFC 7638), which every key authorization of a challenge is made of.
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

        // The key in unmanaged memory and what is made of its public half
        // once: the JWK (RFC 7638's members in their order, which is its
        // canonical text), the JWK as a JSON value for the headers, the
        // thumbprint, the algorithm's name
        struct KeyState {
            std::unique_ptr<IdentityKey> key;
            string jwk;
            encoding::json jwk_value;
            string thumbprint;
            const char* alg = "ES256";
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

        // The public JWK of a key, its members in lexicographic order with no
        // white space: the text RFC 7638 §3 hashes for the thumbprint
        inline string jwk_of(const IdentityKey& k) {
            if (k.p256) {
                auto p = k.p256->public_key().bytes();   // 04 || X || Y
                return string::concat("{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"", b64(slice<const byte>(p.data() + 1, 32)), "\",\"y\":\"",
                                      b64(slice<const byte>(p.data() + 33, 32)), "\"}");
            }
            if (k.p384) {
                auto p = k.p384->public_key().bytes();
                return string::concat("{\"crv\":\"P-384\",\"kty\":\"EC\",\"x\":\"", b64(slice<const byte>(p.data() + 1, 48)), "\",\"y\":\"",
                                      b64(slice<const byte>(p.data() + 49, 48)), "\"}");
            }
            if (k.ed25519) {
                auto p = k.ed25519->public_key().bytes();
                return string::concat("{\"crv\":\"Ed25519\",\"kty\":\"OKP\",\"x\":\"", b64(slice<const byte>(p.data(), 32)), "\"}");
            }
            auto pub = k.rsa->public_key();
            uint64_t e = pub.exponent();
            unsigned char eb[8];
            size_t n = 0;
            for (int i = 7; i >= 0; --i) {
                unsigned char b = static_cast<unsigned char>(e >> (8 * i));
                if (n || b) {
                    eb[n++] = b;
                }
            }
            auto mod = pub.modulus();
            return string::concat("{\"e\":\"", b64(slice<const byte>(reinterpret_cast<const byte*>(eb), n)), "\",\"kty\":\"RSA\",\"n\":\"", b64(mod.as_slice()), "\"}");
        }

        SGCL_INLINE_HOT const char* alg_of(const IdentityKey& k) noexcept {
            return k.p256 ? "ES256" : k.p384 ? "ES384" : k.ed25519 ? "EdDSA" : "RS256";
        }

        // The JWS signature of a signing input (RFC 7515 §5.1): ECDSA as R
        // || S of fixed size (RFC 7518 §3.4), never DER
        inline vector<byte> jws_signature(const IdentityKey& k, const slice<const byte>& input) {
            if (k.p256) {
                auto d = crypto::sha256::of(input);
                auto s = k.p256->sign_digest_raw(bytes_of(d));
                return vector<byte>(s.data(), s.data() + s.size());
            }
            if (k.p384) {
                auto d = crypto::sha384::of(input);
                auto s = k.p384->sign_digest_raw(bytes_of(d));
                return vector<byte>(s.data(), s.data() + s.size());
            }
            if (k.ed25519) {
                auto s = k.ed25519->sign(input);
                return vector<byte>(s.data(), s.data() + s.size());
            }
            auto d = crypto::sha256::of(input);
            return k.rsa->sign_digest(crypto::hash_id::sha256, bytes_of(d));
        }

        // A JWS in the flattened JSON serialization (RFC 7515 §7.2.2) of a
        // protected header and a payload (the empty payload of a
        // POST-as-GET, RFC 8555 §6.3, is "")
        inline string jws(const IdentityKey& k, const string& header, const string& payload) {
            string h = b64(header.view());
            string p = b64(payload.view());
            string input = string::concat(h, ".", p);
            auto sig = jws_signature(k, bytes_of(input.view()));
            return string::concat("{\"protected\":\"", h, "\",\"payload\":\"", p, "\",\"signature\":\"", b64(sig.as_slice()), "\"}");
        }

        // The protected header of a request (RFC 8555 §6.2): the algorithm,
        // the account's URL as kid, or the JWK where there is no account
        // yet (newAccount, a revocation by the certificate's key); the
        // nonce; the URL
        inline string header(const KeyState& k, const string& kid, const string& nonce, const string& url) {
            using encoding::json;
            if (kid.empty()) {
                return json::object({{"alg", k.alg}, {"jwk", k.jwk_value}, {"nonce", nonce}, {"url", url}}).to_string();
            }
            return json::object({{"alg", k.alg}, {"kid", kid}, {"nonce", nonce}, {"url", url}}).to_string();
        }

        // The external account binding of a newAccount (RFC 8555 §7.3.4):
        // a JWS of the account's JWK under the CA's MAC key, HS256, the
        // key's id as kid, the newAccount URL
        inline string eab(const KeyState& k, const string& key_id, const slice<const byte>& mac_key, const string& url) {
            using encoding::json;
            string h = b64(json::object({{"alg", "HS256"}, {"kid", key_id}, {"url", url}}).to_string().view());
            string p = b64(k.jwk.view());
            string input = string::concat(h, ".", p);
            auto tag = crypto::hmac<crypto::sha256>::of(bytes_of(input.view()), mac_key);
            return string::concat("{\"protected\":\"", h, "\",\"payload\":\"", p, "\",\"signature\":\"", b64(bytes_of(tag)), "\"}");
        }

        inline tracked_ptr<KeyState> make_key_state(std::unique_ptr<IdentityKey> key) {
            auto s = make_tracked<KeyState>();
            s->jwk = jwk_of(*key);
            s->jwk_value = *encoding::json::parse(s->jwk);
            auto d = crypto::sha256::of(bytes_of(s->jwk.view()));
            s->thumbprint = b64(bytes_of(d));
            s->alg = alg_of(*key);
            s->key = std::move(key);
            return s;
        }

        inline io::error key_error(const char* what) noexcept {
            return io::error(crypto::errc::malformed, "acme key", string(what));
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
        explicit account_key(key_algorithm a) {
            auto k = std::make_unique<detail::IdentityKey>();
            switch (a) {
                case key_algorithm::es256: k->p256.emplace(crypto::p256::private_key::generate()); break;
                case key_algorithm::es384: k->p384.emplace(crypto::p384::private_key::generate()); break;
                case key_algorithm::eddsa: k->ed25519.emplace(crypto::ed25519::private_key::generate()); break;
                case key_algorithm::rs256: k->rsa.emplace(crypto::rsa::private_key::generate(2048)); break;
                default: throw std::invalid_argument("sgcl::net::acme::account_key: an algorithm of no value of its enumeration");
            }
            _s = detail::make_key_state(std::move(k));
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
            auto k = std::make_unique<detail::IdentityKey>();
            if (!tls::detail::read_key(*k, block->label, block->der)) {
                return unexpected(detail::key_error("no private key of a kind ACME signs with (P-256, P-384, Ed25519, RSA)"));
            }
            if (k->rsa && k->rsa->public_key().bits() < 2048) {
                return unexpected(detail::key_error("an RSA key under 2048 bits"));
            }
            return account_key(detail::make_key_state(std::move(k)));
        }

        // The key in PEM, "PRIVATE KEY" over its PKCS #8: a secret_bytes,
        // never managed memory, for io::write_file with permissions 0600
        crypto::secret_bytes to_pem() const {
            const auto& k = *_s->key;
            if (k.p256) {
                return k.p256->to_pem();
            }
            if (k.p384) {
                return k.p384->to_pem();
            }
            if (k.ed25519) {
                return k.ed25519->to_pem();
            }
            return k.rsa->to_pem();
        }

        SGCL_INLINE_HOT key_algorithm algorithm() const noexcept {
            const auto& k = *_s->key;
            return k.p256 ? key_algorithm::es256 : k.p384 ? key_algorithm::es384 : k.ed25519 ? key_algorithm::eddsa : key_algorithm::rs256;
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
