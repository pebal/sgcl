//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../key.h"
#include "../../../crypto/constant_time.h"
#include "../../../crypto/ed25519.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/p384.h"
#include "../../../crypto/rsa.h"
#include "../../../encoding/json.h"

#include <string>
#include <string_view>

// A JWS read and verified, the server's side of RFC 8555 §6.2 (the test
// server's, and the tests' oracle of the client): the flattened JSON form,
// its protected header (alg, nonce, url, kid or jwk), the payload, a
// signature checked under a JWK of the four kinds the client signs with
// (RFC 7518 §3.4: ECDSA as R || S) or under an HMAC key (HS256, an
// external account binding).
namespace sgcl::net::acme::detail {
    // A public key of a JWK: one of the kinds, its canonical JWK (RFC 7638's
    // members in their order) and its thumbprint
    struct JwkKey {
        optional<crypto::p256::public_key> p256;
        optional<crypto::p384::public_key> p384;
        optional<crypto::ed25519::public_key> ed25519;
        optional<crypto::rsa::public_key> rsa;
        string canonical;
        string thumbprint;

        SGCL_INLINE_HOT const char* alg() const noexcept {
            return p256 ? "ES256" : p384 ? "ES384" : ed25519 ? "EdDSA" : "RS256";
        }
    };

    inline optional<vector<byte>> b64_decode(const string& text) noexcept {
        auto r = encoding::base64::raw_url.decode(text);
        if (!r) {
            return nullopt;
        }
        return std::move(*r);
    }

    // The key of a JWK object: EC of P-256 or P-384, OKP of Ed25519, RSA of
    // 2048 bits or more; nullopt for anything else
    inline optional<JwkKey> jwk_key(const encoding::json& jwk) noexcept {
        if (!jwk.is_object()) {
            return nullopt;
        }
        auto kty = jwk["kty"].as_string();
        if (!kty) {
            return nullopt;
        }
        JwkKey k;
        if (*kty == "EC") {
            auto crv = jwk["crv"].as_string();
            auto x = jwk["x"].as_string();
            auto y = jwk["y"].as_string();
            if (!crv || !x || !y) {
                return nullopt;
            }
            auto xb = b64_decode(*x);
            auto yb = b64_decode(*y);
            size_t n = *crv == "P-256" ? 32 : *crv == "P-384" ? 48 : 0;
            if (!n || !xb || !yb || xb->size() != n || yb->size() != n) {
                return nullopt;
            }
            vector<byte> point;
            point.push_back(byte(0x04));
            for (auto b : *xb) {
                point.push_back(b);
            }
            for (auto b : *yb) {
                point.push_back(b);
            }
            if (n == 32) {
                auto p = crypto::p256::public_key::from_bytes(point.as_slice());
                if (!p) {
                    return nullopt;
                }
                k.p256.emplace(*p);
            } else {
                auto p = crypto::p384::public_key::from_bytes(point.as_slice());
                if (!p) {
                    return nullopt;
                }
                k.p384.emplace(*p);
            }
            k.canonical = string::concat("{\"crv\":\"", *crv, "\",\"kty\":\"EC\",\"x\":\"", *x, "\",\"y\":\"", *y, "\"}");
        } else if (*kty == "OKP") {
            auto crv = jwk["crv"].as_string();
            auto x = jwk["x"].as_string();
            if (!crv || *crv != "Ed25519" || !x) {
                return nullopt;
            }
            auto xb = b64_decode(*x);
            if (!xb) {
                return nullopt;
            }
            auto p = crypto::ed25519::public_key::from_bytes(xb->as_slice());
            if (!p) {
                return nullopt;
            }
            k.ed25519.emplace(*p);
            k.canonical = string::concat("{\"crv\":\"Ed25519\",\"kty\":\"OKP\",\"x\":\"", *x, "\"}");
        } else if (*kty == "RSA") {
            auto n = jwk["n"].as_string();
            auto e = jwk["e"].as_string();
            if (!n || !e) {
                return nullopt;
            }
            auto nb = b64_decode(*n);
            auto eb = b64_decode(*e);
            if (!nb || !eb || eb->empty() || eb->size() > 8 || nb->empty() || (*nb)[0] == byte(0) || (*eb)[0] == byte(0)) {
                return nullopt;   // RFC 7518 §6.3.1: the shortest form, no leading zero
            }
            uint64_t ev = 0;
            for (auto b : *eb) {
                ev = ev << 8 | uint64_t(b);
            }
            auto p = crypto::rsa::public_key::from_modulus(nb->as_slice(), ev);
            if (!p || p->bits() < 2048) {
                return nullopt;
            }
            k.rsa.emplace(std::move(*p));
            k.canonical = string::concat("{\"e\":\"", *e, "\",\"kty\":\"RSA\",\"n\":\"", *n, "\"}");
        } else {
            return nullopt;
        }
        auto d = crypto::sha256::of(bytes_of(k.canonical.view()));
        k.thumbprint = b64(bytes_of(d));
        return k;
    }

    // Whether sig is the JWS signature of input under the key, by alg
    inline bool jws_verify(const JwkKey& k, std::string_view alg, const slice<const byte>& input, const slice<const byte>& sig) noexcept {
        if (alg == "ES256" && k.p256) {
            auto d = crypto::sha256::of(input);
            return sig.size() == 64 && k.p256->verify_digest_raw(bytes_of(d), sig);
        }
        if (alg == "ES384" && k.p384) {
            auto d = crypto::sha384::of(input);
            return sig.size() == 96 && k.p384->verify_digest_raw(bytes_of(d), sig);
        }
        if (alg == "EdDSA" && k.ed25519) {
            return k.ed25519->verify(input, sig);
        }
        if (alg == "RS256" && k.rsa) {
            auto d = crypto::sha256::of(input);
            return k.rsa->verify_digest(crypto::hash_id::sha256, bytes_of(d), sig);
        }
        return false;
    }

    // A JWS in the flattened JSON form taken apart: the header and the
    // payload decoded, the signing input, the signature's bytes
    struct ParsedJws {
        encoding::json header;
        string header_b64;
        string payload_b64;
        string payload;                  // decoded; "" for a POST-as-GET
        vector<byte> signature;
        string alg, nonce, url, kid;
        encoding::json jwk;              // null when the header has a kid

        SGCL_INLINE_HOT string signing_input() const {
            return string::concat(header_b64, ".", payload_b64);
        }
    };

    // The flattened JSON form of a JWS read (RFC 7515 §7.2.2), its protected
    // header a JSON object with alg; nullopt and why for anything else
    inline expected<ParsedJws, string> parse_jws(const string& text) noexcept {
        auto j = encoding::json::parse(text);
        if (!j || !j->is_object()) {
            return unexpected(string("the body is not a JSON object"));
        }
        ParsedJws p;
        auto h = (*j)["protected"].as_string();
        auto pl = (*j)["payload"].as_string();
        auto sig = (*j)["signature"].as_string();
        if (!h || !pl || !sig) {
            return unexpected(string("a JWS without protected, payload or signature"));
        }
        if (!(*j)["header"].is_null() || !(*j)["signatures"].is_null()) {
            return unexpected(string("a JWS with an unprotected header or several signatures"));
        }
        p.header_b64 = *h;
        p.payload_b64 = *pl;
        auto hb = b64_decode(*h);
        auto pb = b64_decode(*pl);
        auto sb = b64_decode(*sig);
        if (!hb || !pb || !sb) {
            return unexpected(string("a JWS part that is not base64url"));
        }
        p.signature = std::move(*sb);
        p.payload = string(std::string_view(reinterpret_cast<const char*>(pb->data()), pb->size()));
        auto hj = encoding::json::parse(string(std::string_view(reinterpret_cast<const char*>(hb->data()), hb->size())));
        if (!hj || !hj->is_object()) {
            return unexpected(string("a protected header that is not a JSON object"));
        }
        p.header = *hj;
        p.alg = (*hj)["alg"].as_string(string());
        p.nonce = (*hj)["nonce"].as_string(string());
        p.url = (*hj)["url"].as_string(string());
        p.kid = (*hj)["kid"].as_string(string());
        p.jwk = (*hj)["jwk"];
        if (p.alg.empty()) {
            return unexpected(string("a protected header without alg"));
        }
        return p;
    }
}
