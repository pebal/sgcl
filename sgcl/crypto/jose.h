//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aes.h"
#include "gcm.h"
#include "kw.h"
#include "p521.h"
#include "detail/aes_core.h"
#include "detail/jose_key.h"
#include "../core/duration.h"
#include "../core/vector.h"
#include "../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// JOSE (RFC 7515-7519, 7638, 8037): JSON Web Signatures, JSON Web
// Encryption, JSON Web Keys and key sets, JSON Web Tokens — what OAuth 2.0,
// OpenID Connect, ACME and the APIs that sign their requests speak. Every
// algorithm is the module's own (HMAC, RSA, ECDSA, Ed25519, X25519, AES);
// the JSON is encoding::json. A key is verified with an algorithm the key
// allows, never with the one a token names: the key's alg, else its kind's
// family, and "none" does not exist. The private members of a JWK are read
// from the text straight into plain memory and written out only into a
// secret_bytes, never managed memory. README: docs/sgcl/crypto/jose.md
namespace sgcl::crypto::jose {
    class jws;
    class jwe;
    class jwt;
}

namespace sgcl::crypto::detail {
    // The state of a jwk: the key in plain memory (zeroed by the keys' own
    // destructors when the state is collected), its public members as one
    // JSON object, and what is read of them once
    struct JwkState {
        std::unique_ptr<JoseKey> key;
        encoding::json params;
        string kid;
        string use;
        optional<algorithm> alg;
        optional<encryption> dir_enc;   // an alg that names an enc (RFC 7520 5.6): dir with that enc alone
        bool alg_unknown = false;       // an alg the module does not have: the key does nothing
    };

    // The public members of a key made in the program, with the options'
    inline tracked_ptr<JwkState> jwk_made(std::unique_ptr<JoseKey> key, const string& kid, optional<algorithm> alg, const string& use) {
        std::string text = "{";
        text += jwk_key_members(*key);
        auto put = [&](std::string_view name, std::string_view value) {
            text += ",\"";
            text += name;
            text += "\":";
            text.append(encoding::json(string(value)).to_string().view());
        };
        if (!kid.empty()) {
            put("kid", kid.view());
        }
        if (!use.empty()) {
            put("use", use.view());
        }
        if (alg) {
            put("alg", jose_name(*alg));
        }
        text += '}';
        auto s = make_tracked<JwkState>();
        s->params = *encoding::json::parse(string(std::string_view(text)));
        key->check_rsa();
        s->key = std::move(key);
        s->kid = kid;
        s->use = use;
        s->alg = alg;
        return s;
    }

    inline tracked_ptr<JwkState> jwk_read(JwkRead&& r) {
        auto s = make_tracked<JwkState>();
        s->kid = r.params["kid"].as_string(string());
        s->use = r.params["use"].as_string(string());
        if (auto a = r.params["alg"].as_string()) {
            s->alg = jose_alg(a->view());
            if (!s->alg) {
                s->dir_enc = jose_enc(a->view());
                if (s->dir_enc) {
                    s->alg = algorithm::dir;
                }
            }
            s->alg_unknown = !s->alg;
        }
        s->params = std::move(r.params);
        r.key->check_rsa();
        s->key = std::move(r.key);
        return s;
    }

    // Why the key's own members forbid the purpose (use, key_ops, alg), or
    // nullptr
    inline const char* jose_forbidden(const JwkState& s, JosePurpose p, algorithm a) noexcept {
        if (s.alg_unknown) {
            return "the key's alg is one the module does not have";
        }
        if (s.alg && *s.alg != a) {
            return "the key is for another algorithm";
        }
        const bool sig = p == JosePurpose::sign || p == JosePurpose::verify;
        if (!s.use.empty() && s.use.view() != (sig ? "sig" : "enc")) {
            return sig ? "the key's use is not sig" : "the key's use is not enc";
        }
        if (s.params.contains(string("key_ops"))) {
            bool ok = false;
            for (const auto& o : s.params["key_ops"].elements()) {
                auto v = o.as_string(string());
                std::string_view n = v.view();
                switch (p) {
                    case JosePurpose::sign: ok |= n == "sign"; break;
                    case JosePurpose::verify: ok |= n == "verify"; break;
                    case JosePurpose::encrypt: ok |= n == "encrypt" || n == "wrapKey" || n == "deriveKey" || n == "deriveBits"; break;
                    case JosePurpose::decrypt: ok |= n == "decrypt" || n == "unwrapKey" || n == "deriveKey" || n == "deriveBits"; break;
                }
            }
            if (!ok) {
                return "the key's key_ops do not allow it";
            }
        }
        return nullptr;
    }

    // Why the key cannot do the purpose with the algorithm, or nullptr
    inline const char* jose_refusal(const JwkState& s, JosePurpose p, algorithm a) {
        if (auto why = jose_forbidden(s, p, a)) {
            return why;
        }
        return jose_misfit(*s.key, a, p);
    }

    // The text readers over a view of bytes where they lie: the public
    // functions' bodies, and the fuzzers' way to parse straight from their
    // input buffer (ASan sees no read past a managed copy)
    struct JoseTextAccess {
        static expected<jose::jwk, error> jwk_of(std::string_view text) noexcept;
        static expected<jose::jwk_set, error> jwk_set_of(std::string_view text) noexcept;
        static expected<jose::jws, error> jws_of(std::string_view text) noexcept;
        static expected<jose::jwe, error> jwe_of(std::string_view text) noexcept;
        static expected<jose::jwt, error> jwt_unverified(std::string_view token) noexcept;
        template<class K, class O>
        static expected<jose::jwt, error> jwt_verify(std::string_view token, const K& key, const O& o) noexcept;
    };

    struct JwkAccess {
        static const JwkState& state(const jose::jwk& k) noexcept;
        // jwk::from_pem of bytes where they lie (net::acme's account_key)
        static expected<jose::jwk, error> from_pem(const slice<const byte>& pem) noexcept;
    };

    // A header given by the program: null is none, anything but an object a
    // broken contract
    inline void jose_header_check(const encoding::json& h, const char* who) {
        if (!h.is_null() && !h.is_object()) {
            throw invalid_argument(std::string("sgcl::crypto::jose::") + who + ": the header is not a JSON object");
        }
    }

    SGCL_INLINE_HOT string jose_text(const slice<const byte>& b) noexcept {
        return string(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
    }

    SGCL_INLINE_HOT slice<const byte> jose_bytes(const string& s) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    // --- JWS ---------------------------------------------------------------

    struct JwsSignature {
        string protected_b64;
        encoding::json header;       // the protected and the unprotected members together
        vector<byte> signature;
    };

    struct JwsData {
        string payload_b64;
        vector<byte> payload;
        vector<JwsSignature> signatures;
    };

    // A header read: a JSON object whose alg is a string; crit names
    // extensions, none of which the module understands (RFC 7515 4.1.11);
    // b64 other than true is RFC 7797's unencoded payload, not done
    inline expected<void, error> jose_header_ok(const encoding::json& h, const char* what) noexcept {
        if (!h.is_object()) {
            return unexpected(jose_error(errc::malformed, std::string(what) + ": the header is not a JSON object"));
        }
        if (!h["alg"].is_string()) {
            return unexpected(jose_error(errc::malformed, std::string(what) + ": the header has no alg"));
        }
        if (h.contains(string("crit"))) {
            const auto& c = h["crit"];
            if (!c.is_array() || c.empty()) {
                return unexpected(jose_error(errc::malformed, std::string(what) + ": crit is not a list of names"));
            }
            return unexpected(jose_error(errc::unsupported, std::string(what) + ": crit names an extension the module does not understand"));
        }
        if (h.contains(string("b64")) && h["b64"].as_bool() != optional<bool>(true)) {
            return unexpected(jose_error(errc::unsupported, std::string(what) + ": an unencoded payload (RFC 7797)"));
        }
        return {};
    }

    inline expected<encoding::json, error> jose_protected(std::string_view b64, const char* what) noexcept {
        auto raw = b64url_bytes(b64);
        if (!raw) {
            return unexpected(jose_error(errc::malformed, std::string(what) + ": the protected header is not base64url"));
        }
        auto h = encoding::json::parse(jose_text(raw->as_slice()));
        if (!h || !h->is_object()) {
            return unexpected(jose_error(errc::malformed, std::string(what) + ": the protected header is not a JSON object"));
        }
        return *h;
    }

    // A JWS's text where it lies (the fuzzers' buffer, a string's view)
    inline expected<tracked_ptr<JwsData>, error> jws_parse(std::string_view v) noexcept {
        auto d = make_tracked<JwsData>();
        size_t at = 0;
        while (at < v.size() && (v[at] == ' ' || v[at] == '\t' || v[at] == '\r' || v[at] == '\n')) {
            ++at;
        }
        if (at < v.size() && v[at] == '{') {
            // JSON: flattened or general (RFC 7515 7.2)
            auto j = encoding::json::parse(string(v));
            if (!j || !j->is_object()) {
                return unexpected(jose_error(errc::malformed, "JWS: not a JSON object"));
            }
            auto payload = (*j)["payload"].as_string();
            if (!payload) {
                return unexpected(jose_error(errc::malformed, "JWS: no payload"));
            }
            d->payload_b64 = *payload;
            const bool general = j->contains(string("signatures"));
            if (general && (j->contains(string("signature")) || j->contains(string("protected")) || j->contains(string("header")))) {
                return unexpected(jose_error(errc::malformed, "JWS: signatures beside a flattened signature"));
            }
            vector<encoding::json> entries;
            if (general) {
                const auto& sigs = (*j)["signatures"];
                if (!sigs.is_array() || sigs.empty()) {
                    return unexpected(jose_error(errc::malformed, "JWS: signatures is not a list of signatures"));
                }
                for (const auto& s : sigs.elements()) {
                    entries.push_back(s);
                }
            } else {
                entries.push_back(*j);
            }
            for (const auto& e : entries) {
                if (!e.is_object()) {
                    return unexpected(jose_error(errc::malformed, "JWS: a signature is not a JSON object"));
                }
                JwsSignature s;
                auto sig = e["signature"].as_string();
                if (!sig) {
                    return unexpected(jose_error(errc::malformed, "JWS: no signature"));
                }
                auto sb = b64url_bytes(sig->view());
                if (!sb) {
                    return unexpected(jose_error(errc::malformed, "JWS: the signature is not base64url"));
                }
                s.signature = std::move(*sb);
                encoding::json merged = encoding::json::object({});
                if (e.contains(string("protected"))) {
                    auto p = e["protected"].as_string();
                    if (!p) {
                        return unexpected(jose_error(errc::malformed, "JWS: protected is not a string"));
                    }
                    s.protected_b64 = *p;
                    auto h = jose_protected(p->view(), "JWS");
                    if (!h) {
                        return unexpected(h.error());
                    }
                    merged = *h;
                }
                if (e.contains(string("header"))) {
                    const auto& u = e["header"];
                    if (!u.is_object()) {
                        return unexpected(jose_error(errc::malformed, "JWS: header is not a JSON object"));
                    }
                    for (const auto& m : u.members()) {
                        if (merged.contains(m.key)) {
                            return unexpected(jose_error(errc::malformed, "JWS: a member in both the protected and the unprotected header"));
                        }
                        if (m.key.view() == "crit") {
                            return unexpected(jose_error(errc::malformed, "JWS: crit in the unprotected header"));
                        }
                        merged = merged.set(m.key, m.value);
                    }
                }
                if (auto ok = jose_header_ok(merged, "JWS"); !ok) {
                    return unexpected(ok.error());
                }
                s.header = merged;
                d->signatures.push_back(std::move(s));
            }
        } else {
            // compact: three parts
            const size_t a = v.find('.');
            const size_t b = a == std::string_view::npos ? a : v.find('.', a + 1);
            if (b == std::string_view::npos || v.find('.', b + 1) != std::string_view::npos) {
                return unexpected(jose_error(errc::malformed, "JWS: a compact JWS is three parts"));
            }
            JwsSignature s;
            s.protected_b64 = string(v.substr(0, a));
            d->payload_b64 = string(v.substr(a + 1, b - a - 1));
            auto sb = b64url_bytes(v.substr(b + 1));
            if (!sb) {
                return unexpected(jose_error(errc::malformed, "JWS: the signature is not base64url"));
            }
            s.signature = std::move(*sb);
            auto h = jose_protected(s.protected_b64.view(), "JWS");
            if (!h) {
                return unexpected(h.error());
            }
            if (auto ok = jose_header_ok(*h, "JWS"); !ok) {
                return unexpected(ok.error());
            }
            s.header = *h;
            d->signatures.push_back(std::move(s));
        }
        auto p = b64url_bytes(d->payload_b64.view());
        if (!p) {
            return unexpected(jose_error(errc::malformed, "JWS: the payload is not base64url"));
        }
        d->payload = std::move(*p);
        return d;
    }

    // The payload when one signature verifies under the key: the first
    // reason it does not, else
    inline expected<vector<byte>, error> jws_verify(const JwsData& d, const JwkState& key) noexcept {
        optional<error> first;
        for (const auto& s : d.signatures) {
            auto name = s.header["alg"].as_string(string());
            auto a = jose_alg(name.view());
            if (!a || !is_signature(*a)) {
                if (!first) {
                    first = jose_error(errc::verification, std::string("JWS: an algorithm that is not a signature the module verifies: ") + std::string(name.view()));
                }
                continue;
            }
            if (auto why = jose_forbidden(key, JosePurpose::verify, *a)) {
                if (!first) {
                    first = jose_error(errc::invalid_key, std::string("JWS: ") + why);
                }
                continue;
            }
            if (auto why = jose_misfit(*key.key, *a, JosePurpose::verify)) {
                if (!first) {
                    first = jose_error(errc::invalid_key, std::string("JWS: ") + why);
                }
                continue;
            }
            const string input = string::concat(s.protected_b64, ".", d.payload_b64);
            if (jose_verify(*key.key, *a, jose_bytes(input), s.signature.as_slice())) {
                return d.payload;
            }
            if (!first) {
                first = jose_error(errc::verification, "JWS: the signature does not verify");
            }
        }
        return unexpected(first ? *first : jose_error(errc::verification, "JWS: no signature"));
    }

    // The protected header of a signer: alg, then the key's kid unless the
    // program's header names one, then the program's members (an alg of
    // theirs is replaced), then what the caller adds
    inline string jose_header_text(algorithm a, const JwkState& key, const encoding::json& extra, std::initializer_list<encoding::json::member> more) {
        encoding::json h = encoding::json::object({{"alg", string(jose_name(a))}});
        for (const auto& m : more) {
            if (!extra.contains(m.key)) {
                h = h.set(m.key, m.value);
            }
        }
        if (!key.kid.empty() && !extra.contains(string("kid"))) {
            h = h.set(string("kid"), key.kid);
        }
        for (const auto& m : extra.members()) {
            if (m.key.view() != "alg") {
                h = h.set(m.key, m.value);
            }
        }
        return h.to_string();
    }

    // The algorithm a signer uses: the options', else the key's, else the
    // kind's; std::invalid_argument when the key cannot sign with it
    inline algorithm jws_algorithm(const JwkState& key, const optional<algorithm>& asked, const char* who) {
        optional<algorithm> a = asked ? asked : key.alg ? key.alg : jose_default(*key.key, true);
        if (!a) {
            throw invalid_argument(std::string("sgcl::crypto::jose::") + who + ": the key has no signature algorithm (an X25519 key)");
        }
        if (!is_signature(*a)) {
            throw invalid_argument(std::string("sgcl::crypto::jose::") + who + ": " + std::string(jose_name(*a)) + " is not a signature algorithm");
        }
        if (auto why = jose_refusal(key, JosePurpose::sign, *a)) {
            throw invalid_argument(std::string("sgcl::crypto::jose::") + who + ": " + why);
        }
        return *a;
    }

    struct JwsPart {
        string protected_b64;
        string signature_b64;
    };

    inline JwsPart jws_sign_part(const JwkState& key, const string& payload_b64, const optional<algorithm>& asked, const encoding::json& header,
                                 std::initializer_list<encoding::json::member> more, const char* who) {
        jose_header_check(header, who);
        const algorithm a = jws_algorithm(key, asked, who);
        JwsPart p;
        const string h = jose_header_text(a, key, header, more);
        p.protected_b64 = b64url(jose_bytes(h));
        const string input = string::concat(p.protected_b64, ".", payload_b64);
        auto sig = jose_sign(*key.key, a, jose_bytes(input));
        p.signature_b64 = b64url(sig.as_slice());
        return p;
    }

    // --- JWE ---------------------------------------------------------------

    struct JwtData {
        encoding::json header;
        encoding::json claims;
    };

    struct JweData {
        string protected_b64;
        encoding::json header;
        vector<byte> encrypted_key;
        vector<byte> iv;
        vector<byte> ciphertext;
        vector<byte> tag;
    };

    SGCL_INLINE_HOT void put_be32(unsigned char* p, uint32_t v) noexcept {
        p[0] = static_cast<unsigned char>(v >> 24);
        p[1] = static_cast<unsigned char>(v >> 16);
        p[2] = static_cast<unsigned char>(v >> 8);
        p[3] = static_cast<unsigned char>(v);
    }

    // The Concat KDF of RFC 7518 4.6.2 (NIST SP 800-56A 5.8.1, SHA-256):
    // keydatalen bits of the shared secret Z, the algorithm's name and the
    // parties' information, into a secret
    inline secret_bytes concat_kdf(const slice<const byte>& z, std::string_view alg_id, const slice<const byte>& apu, const slice<const byte>& apv, size_t bytes) noexcept {
        secret_bytes out(bytes);
        unsigned char be[4];
        size_t done = 0;
        for (uint32_t counter = 1; done < bytes; ++counter) {
            sha256 h;
            put_be32(be, counter);
            h.update(slice<const byte>(reinterpret_cast<const byte*>(be), 4));
            h.update(z);
            put_be32(be, uint32_t(alg_id.size()));
            h.update(slice<const byte>(reinterpret_cast<const byte*>(be), 4));
            h.update(slice<const byte>(reinterpret_cast<const byte*>(alg_id.data()), alg_id.size()));
            put_be32(be, uint32_t(apu.size()));
            h.update(slice<const byte>(reinterpret_cast<const byte*>(be), 4));
            h.update(apu);
            put_be32(be, uint32_t(apv.size()));
            h.update(slice<const byte>(reinterpret_cast<const byte*>(be), 4));
            h.update(apv);
            put_be32(be, uint32_t(bytes * 8));
            h.update(slice<const byte>(reinterpret_cast<const byte*>(be), 4));
            auto block = h.value();
            const size_t k = bytes - done < block.size() ? bytes - done : block.size();
            sgcl::detail::copy_bytes(out.as_slice().data() + done, block.data(), k);
            secure_zero(block.data(), block.size());
            done += k;
        }
        return out;
    }

    // An AES key schedule for one call, zeroed when it goes
    struct AesOnce {
        AesEncryptKey enc;
        AesDecryptKey dec;

        AesOnce(const unsigned char* key, size_t n, bool decrypt) noexcept {
            aes_setup(enc, key, n);
            if (decrypt) {
                aes_setup_decrypt(dec, enc);
            }
        }

        AesOnce(const AesOnce&) = delete;
        AesOnce& operator=(const AesOnce&) = delete;

        ~AesOnce() {
            secure_zero_object(enc);
            secure_zero_object(dec);
        }
    };

    // The tag of AES-CBC-HMAC-SHA2 (RFC 7518 5.2.2.1): HMAC over the aad,
    // the iv, the ciphertext and the aad's length in bits, its first half
    template<class H>
    inline void cbc_hs_tag(unsigned char* tag, size_t tag_size, const slice<const byte>& mac_key, const slice<const byte>& aad, const slice<const byte>& iv,
                           const slice<const byte>& ct) noexcept {
        hmac<H> m(mac_key);
        m.update(aad);
        m.update(iv);
        m.update(ct);
        unsigned char al[8];
        const uint64_t bits = uint64_t(aad.size()) * 8;
        for (int i = 0; i < 8; ++i) {
            al[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        }
        m.update(slice<const byte>(reinterpret_cast<const byte*>(al), 8));
        auto full = m.value();
        sgcl::detail::copy_bytes(tag, full.data(), tag_size);
        secure_zero(full.data(), full.size());
    }

    inline void cbc_hs_tag_of(encryption e, unsigned char* tag, const slice<const byte>& mac_key, const slice<const byte>& aad, const slice<const byte>& iv,
                              const slice<const byte>& ct) noexcept {
        switch (e) {
            case encryption::a128cbc_hs256: cbc_hs_tag<sha256>(tag, 16, mac_key, aad, iv, ct); break;
            case encryption::a192cbc_hs384: cbc_hs_tag<sha384>(tag, 24, mac_key, aad, iv, ct); break;
            default: cbc_hs_tag<sha512>(tag, 32, mac_key, aad, iv, ct); break;
        }
    }

    SGCL_INLINE_HOT bool is_cbc(encryption e) noexcept {
        return e <= encryption::a256cbc_hs512;
    }

    // The content sealed under the CEK: iv, ciphertext, tag
    inline void jwe_seal(encryption e, const slice<const byte>& cek, const slice<const byte>& plaintext, const slice<const byte>& aad, JweData& out) {
        if (!is_cbc(e)) {
            out.iv = random::bytes(12);
            aes_gcm g(cek);
            auto sealed = g.seal(out.iv.as_slice(), plaintext, aad);
            const size_t n = sealed.size() - 16;
            out.ciphertext = vector<byte>(sealed.data(), sealed.data() + n);
            out.tag = vector<byte>(sealed.data() + n, sealed.data() + sealed.size());
            return;
        }
        const size_t half = cek.size() / 2;
        out.iv = random::bytes(16);
        const size_t pad = 16 - plaintext.size() % 16;
        const size_t n = plaintext.size() + pad;
        out.ciphertext = vector<byte>(n);
        unsigned char* c = reinterpret_cast<unsigned char*>(out.ciphertext.data());
        sgcl::detail::copy_bytes(c, plaintext.data(), plaintext.size());
        sgcl::detail::fill_bytes(c + plaintext.size(), static_cast<unsigned char>(pad), pad);
        AesOnce k(reinterpret_cast<const unsigned char*>(cek.data()) + half, half, false);
        unsigned char chain[16];
        sgcl::detail::copy_bytes(chain, out.iv.data(), 16);
        for (size_t i = 0; i < n; i += 16) {
            for (int j = 0; j < 16; ++j) {
                c[i + j] ^= chain[j];
            }
            aes_encrypt_block(k.enc, c + i, c + i);
            sgcl::detail::copy_bytes(chain, c + i, 16);
        }
        out.tag = vector<byte>(half);
        cbc_hs_tag_of(e, reinterpret_cast<unsigned char*>(out.tag.data()), cek.subslice(0, half), aad, out.iv.as_slice(), out.ciphertext.as_slice());
    }

    inline expected<vector<byte>, error> jwe_open(encryption e, const slice<const byte>& cek, const JweData& d, const slice<const byte>& aad) noexcept {
        auto fail = [] {
            return unexpected(error(errc::authentication, string("sgcl::crypto::jose: JWE: the content does not authenticate")));
        };
        if (!is_cbc(e)) {
            if (d.iv.size() != 12 || d.tag.size() != 16) {
                return fail();
            }
            aes_gcm g(cek);
            vector<byte> sealed(d.ciphertext.size() + 16);
            sgcl::detail::copy_bytes(sealed.data(), d.ciphertext.data(), d.ciphertext.size());
            sgcl::detail::copy_bytes(sealed.data() + d.ciphertext.size(), d.tag.data(), 16);
            auto r = g.open(d.iv.as_slice(), sealed.as_slice(), aad);
            if (!r) {
                return fail();
            }
            return std::move(*r);
        }
        const size_t half = cek.size() / 2;
        if (d.iv.size() != 16 || d.tag.size() != half || d.ciphertext.empty() || d.ciphertext.size() % 16 != 0) {
            return fail();
        }
        unsigned char tag[32];
        cbc_hs_tag_of(e, tag, cek.subslice(0, half), aad, d.iv.as_slice(), d.ciphertext.as_slice());
        const bool ok = constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(tag), half), d.tag.as_slice());
        secure_zero(tag, sizeof tag);
        if (!ok) {
            return fail();
        }
        const size_t n = d.ciphertext.size();
        vector<byte> out(n);
        unsigned char* p = reinterpret_cast<unsigned char*>(out.data());
        AesOnce k(reinterpret_cast<const unsigned char*>(cek.data()) + half, half, true);
        unsigned char iv[16];
        sgcl::detail::copy_bytes(iv, d.iv.data(), 16);
        aes_cbc_decrypt(k.enc, k.dec, iv, reinterpret_cast<const unsigned char*>(d.ciphertext.data()), p, n / 16);
        // PKCS #7 padding, read over the last block without a branch on it
        const unsigned pad = p[n - 1];
        unsigned bad = unsigned(pad - 1) >> 8 | unsigned(16 - pad) >> 8;   // pad outside 1..16
        for (unsigned i = 1; i <= 16; ++i) {
            const unsigned in_pad = unsigned(pad - i) >> 8 ^ 1u;   // 1 when i <= pad
            bad |= in_pad & ((p[n - i] ^ pad) != 0);
        }
        if (bad) {
            return fail();
        }
        out.resize(n - pad);
        return out;
    }

    // The ECDH-ES shared secret Z of the recipient's key and a peer's public
    // key given as a JWK (epk), the same curve
    inline expected<secret_bytes, error> ecdh_z(const JoseKey& priv, const encoding::json& epk) noexcept {
        auto bad = [] {
            return unexpected(jose_error(errc::invalid_key, "JWE: epk is not a public key of the recipient's curve"));
        };
        if (!epk.is_object()) {
            return bad();
        }
        auto x = b64url_bytes(epk["x"].as_string(string()).view());
        if (!x) {
            return bad();
        }
        auto take = [](const auto& s) {
            secret_bytes z(s.size);
            sgcl::detail::copy_bytes(z.as_slice().data(), s.bytes().data(), s.size);
            return z;
        };
        const size_t i = priv.k.index();
        if (i == 10) {
            if (epk["kty"].as_string(string()).view() != "OKP" || epk["crv"].as_string(string()).view() != "X25519" || x->size() != 32) {
                return bad();
            }
            auto p = x25519::public_key::from_bytes(x->as_slice());
            if (!p) {
                return bad();
            }
            auto z = std::get<10>(priv.k).shared_secret(*p);
            if (!z) {
                return unexpected(jose_error(errc::invalid_key, "JWE: epk is a point of small order"));
            }
            return take(*z);
        }
        const size_t n = i == 2 ? 32 : i == 4 ? 48 : 66;
        const std::string_view crv = n == 32 ? "P-256" : n == 48 ? "P-384" : "P-521";
        auto y = b64url_bytes(epk["y"].as_string(string()).view());
        if (!y || epk["kty"].as_string(string()).view() != "EC" || epk["crv"].as_string(string()).view() != crv || x->size() != n || y->size() != n) {
            return bad();
        }
        unsigned char point[1 + 2 * 66];
        point[0] = 0x04;
        sgcl::detail::copy_bytes(point + 1, x->data(), n);
        sgcl::detail::copy_bytes(point + 1 + n, y->data(), n);
        const slice<const byte> pt(reinterpret_cast<const byte*>(point), 1 + 2 * n);
        auto agree = [&](const auto& key, auto curve) -> expected<secret_bytes, error> {
            auto p = decltype(curve)::type::from_bytes(pt);
            if (!p) {
                return bad();
            }
            auto z = key.to_ecdh().shared_secret(*p);
            if (!z) {
                return bad();
            }
            return take(*z);
        };
        if (n == 32) {
            return agree(std::get<2>(priv.k), std::type_identity<p256::public_key>());
        }
        if (n == 48) {
            return agree(std::get<4>(priv.k), std::type_identity<p384::public_key>());
        }
        return agree(std::get<12>(priv.k), std::type_identity<p521::public_key>());
    }

    // An ephemeral key of the recipient's curve: Z and the epk member
    inline pair<secret_bytes, encoding::json> ecdh_ephemeral(const JoseKey& to) {
        auto take = [](const auto& s) {
            secret_bytes z(s.size);
            sgcl::detail::copy_bytes(z.as_slice().data(), s.bytes().data(), s.size);
            return z;
        };
        JoseKey eph;
        secret_bytes z;
        switch (to.k.index()) {
            case 2:
            case 3: {
                auto k = p256::private_key::generate();
                p256::public_key peer = to.k.index() == 2 ? std::get<2>(to.k).public_key() : std::get<3>(to.k);
                z = take(*k.to_ecdh().shared_secret(peer));
                eph.k.emplace<p256::public_key>(k.public_key());
                break;
            }
            case 4:
            case 5: {
                auto k = p384::private_key::generate();
                p384::public_key peer = to.k.index() == 4 ? std::get<4>(to.k).public_key() : std::get<5>(to.k);
                z = take(*k.to_ecdh().shared_secret(peer));
                eph.k.emplace<p384::public_key>(k.public_key());
                break;
            }
            case 12:
            case 13: {
                auto k = p521::private_key::generate();
                p521::public_key peer = to.k.index() == 12 ? std::get<12>(to.k).public_key() : std::get<13>(to.k);
                z = take(*k.to_ecdh().shared_secret(peer));
                eph.k.emplace<p521::public_key>(k.public_key());
                break;
            }
            default: {
                auto k = x25519::private_key::generate();
                x25519::public_key peer = to.k.index() == 10 ? std::get<10>(to.k).public_key() : std::get<11>(to.k);
                auto s = k.shared_secret(peer);
                if (!s) {
                    throw invalid_argument("sgcl::crypto::jose::jwe: the recipient's X25519 key is of small order");
                }
                z = take(*s);
                eph.k.emplace<x25519::public_key>(k.public_key());
                break;
            }
        }
        std::string text = "{" + jwk_key_members(eph) + "}";
        return pair<secret_bytes, encoding::json>(std::move(z), *encoding::json::parse(string(std::string_view(text))));
    }

    inline expected<tracked_ptr<JweData>, error> jwe_parse(std::string_view v) noexcept {
        size_t dots[4];
        size_t found = 0;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '.') {
                if (found == 4) {
                    return unexpected(jose_error(errc::malformed, "JWE: a compact JWE is five parts"));
                }
                dots[found++] = i;
            }
        }
        if (found != 4) {
            return unexpected(jose_error(errc::malformed, "JWE: a compact JWE is five parts"));
        }
        auto d = make_tracked<JweData>();
        d->protected_b64 = string(v.substr(0, dots[0]));
        auto h = jose_protected(d->protected_b64.view(), "JWE");
        if (!h) {
            return unexpected(h.error());
        }
        if (auto ok = jose_header_ok(*h, "JWE"); !ok) {
            return unexpected(ok.error());
        }
        if (!(*h)["enc"].is_string()) {
            return unexpected(jose_error(errc::malformed, "JWE: the header has no enc"));
        }
        d->header = *h;
        vector<byte>* parts[4] = {&d->encrypted_key, &d->iv, &d->ciphertext, &d->tag};
        for (int i = 0; i < 4; ++i) {
            const size_t from = dots[i] + 1;
            const size_t to = i == 3 ? v.size() : dots[i + 1];
            auto b = b64url_bytes(v.substr(from, to - from));
            if (!b) {
                return unexpected(jose_error(errc::malformed, "JWE: a part that is not base64url"));
            }
            *parts[i] = std::move(*b);
        }
        return d;
    }

    // The JWE opened under the key: the CEK by the algorithm, then the
    // content. A failure to find the CEK where an attacker could tell it from
    // a wrong tag (RSA-OAEP) goes on with a random CEK (RFC 7516 11.5)
    inline expected<vector<byte>, error> jwe_decrypt(const JweData& d, const JwkState& key) noexcept {
        auto name = d.header["alg"].as_string(string());
        auto a = jose_alg(name.view());
        auto e = jose_enc(d.header["enc"].as_string(string()).view());
        if (!a || is_signature(*a)) {
            return unexpected(jose_error(errc::unsupported, std::string("JWE: an algorithm the module does not have: ") + std::string(name.view())));
        }
        if (!e) {
            return unexpected(jose_error(errc::unsupported, "JWE: an enc the module does not have"));
        }
        if (d.header.contains(string("zip"))) {
            return unexpected(jose_error(errc::unsupported, "JWE: compressed content (zip)"));
        }
        if (auto why = jose_refusal(key, JosePurpose::decrypt, *a)) {
            return unexpected(jose_error(errc::invalid_key, std::string("JWE: ") + why));
        }
        if (key.dir_enc && *key.dir_enc != *e) {
            return unexpected(jose_error(errc::invalid_key, "JWE: the key is for another enc"));
        }
        const size_t cek_size = jose_cek_size(*e);
        const JoseKey& k = *key.key;
        secret_bytes cek;
        switch (*a) {
            case algorithm::dir: {
                const auto& s = std::get<1>(k.k);
                if (!d.encrypted_key.empty()) {
                    return unexpected(jose_error(errc::malformed, "JWE: dir with an encrypted key"));
                }
                if (s.size() != cek_size) {
                    return unexpected(jose_error(errc::invalid_key, "JWE: a dir key of another size than the enc's"));
                }
                cek = s.clone();
                break;
            }
            case algorithm::rsa_oaep:
            case algorithm::rsa_oaep_256: {
                const auto& r = std::get<6>(k.k);
                const hash_id h = *a == algorithm::rsa_oaep ? hash_id::sha1 : hash_id::sha256;
                secret_bytes out(r.public_key().max_oaep_message_size(h));
                auto n = r.decrypt_oaep_to(out, h, d.encrypted_key.as_slice());
                // the size is part of the verdict; both paths copy the same bytes
                const size_t got = n ? *n : 0;
                const unsigned char good = static_cast<unsigned char>(0 - unsigned(n.has_value() & (got == cek_size)));
                auto rnd = random::secret(cek_size);
                cek = secret_bytes(cek_size);
                unsigned char* c = reinterpret_cast<unsigned char*>(cek.as_slice().data());
                const unsigned char* o = reinterpret_cast<const unsigned char*>(out.as_slice().data());
                const unsigned char* q = reinterpret_cast<const unsigned char*>(rnd.as_slice().data());
                const size_t span = out.size() < cek_size ? out.size() : cek_size;
                for (size_t i = 0; i < cek_size; ++i) {
                    const unsigned char from_out = i < span ? o[i] : 0;
                    c[i] = static_cast<unsigned char>((from_out & good) | (q[i] & ~good));
                }
                break;
            }
            case algorithm::a128gcmkw:
            case algorithm::a192gcmkw:
            case algorithm::a256gcmkw: {
                auto iv = b64url_bytes(d.header["iv"].as_string(string()).view());
                auto tag = b64url_bytes(d.header["tag"].as_string(string()).view());
                if (!iv || !tag || iv->size() != 12 || tag->size() != 16) {
                    return unexpected(jose_error(errc::malformed, "JWE: an AES-GCM key wrap without its iv and tag"));
                }
                aes_gcm g(std::get<1>(k.k).as_slice());
                vector<byte> sealed(d.encrypted_key.size() + 16);
                sgcl::detail::copy_bytes(sealed.data(), d.encrypted_key.data(), d.encrypted_key.size());
                sgcl::detail::copy_bytes(sealed.data() + d.encrypted_key.size(), tag->data(), 16);
                if (d.encrypted_key.size() != cek_size) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::jose: JWE: the content does not authenticate")));
                }
                cek = secret_bytes(cek_size);
                if (!g.open_to(cek, iv->as_slice(), sealed.as_slice())) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::jose: JWE: the content does not authenticate")));
                }
                break;
            }
            case algorithm::ecdh_es: {
                if (!d.encrypted_key.empty()) {
                    return unexpected(jose_error(errc::malformed, "JWE: ECDH-ES with an encrypted key"));
                }
                auto z = ecdh_z(k, d.header["epk"]);
                if (!z) {
                    return unexpected(z.error());
                }
                auto apu = b64url_bytes(d.header["apu"].as_string(string()).view());
                auto apv = b64url_bytes(d.header["apv"].as_string(string()).view());
                if (!apu || !apv) {
                    return unexpected(jose_error(errc::malformed, "JWE: apu or apv is not base64url"));
                }
                cek = concat_kdf(*z, jose_name(*e), apu->as_slice(), apv->as_slice(), cek_size);
                break;
            }
            case algorithm::a128kw:
            case algorithm::a192kw:
            case algorithm::a256kw:
            case algorithm::ecdh_es_a128kw:
            case algorithm::ecdh_es_a192kw:
            case algorithm::ecdh_es_a256kw: {
                // the content key wrapped (RFC 3394, RFC 7518 4.4) under the
                // key itself or, for ECDH-ES+A*KW, under the Concat KDF of Z
                // for the alg (4.6.2)
                secret_bytes kek;
                if (*a >= algorithm::ecdh_es_a128kw) {
                    auto z = ecdh_z(k, d.header["epk"]);
                    if (!z) {
                        return unexpected(z.error());
                    }
                    auto apu = b64url_bytes(d.header["apu"].as_string(string()).view());
                    auto apv = b64url_bytes(d.header["apv"].as_string(string()).view());
                    if (!apu || !apv) {
                        return unexpected(jose_error(errc::malformed, "JWE: apu or apv is not base64url"));
                    }
                    kek = concat_kdf(*z, jose_name(*a), apu->as_slice(), apv->as_slice(), jose_wrap_key_size(*a));
                } else {
                    kek = std::get<1>(k.k).clone();
                }
                auto unwrapped = aes_kw(kek.as_slice()).unwrap(d.encrypted_key.as_slice());
                if (!unwrapped) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::jose: JWE: the content key does not unwrap")));
                }
                if (unwrapped->size() != cek_size) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::jose: JWE: a content key of another size than the enc's")));
                }
                cek = std::move(*unwrapped);
                break;
            }
            default:
                return unexpected(jose_error(errc::unsupported, "JWE: not an algorithm of key management"));
        }
        return jwe_open(*e, cek, d, jose_bytes(d.protected_b64));
    }
}

namespace sgcl::crypto::jose {
    // A JSON Web Key (RFC 7517): one key of any kind the module has, public
    // or private, and its JWK members. A handle of one word: copies share
    // the key, safe from many threads; a move is a copy. The key lives in
    // plain memory and is zeroed by its own destructor when the last
    // handle's state is collected
    class jwk {
    public:
        // kid, alg and use of a key made by the program
        struct options {
            string kid;                  // "kid"; empty: none
            optional<algorithm> alg;     // "alg": the one algorithm the key is for; nullopt: any of its kind
            string use;                  // "use": "sig" or "enc"; empty: none
        };

        explicit jwk(p256::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(p256::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const p256::public_key& key) : jwk(key, options()) {}
        explicit jwk(const p256::public_key& key, const options& o) : jwk(_of(key), o) {}
        explicit jwk(p384::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(p384::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const p384::public_key& key) : jwk(key, options()) {}
        explicit jwk(const p384::public_key& key, const options& o) : jwk(_of(key), o) {}
        explicit jwk(p521::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(p521::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const p521::public_key& key) : jwk(key, options()) {}
        explicit jwk(const p521::public_key& key, const options& o) : jwk(_of(key), o) {}
        explicit jwk(rsa::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(rsa::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const rsa::public_key& key) : jwk(key, options()) {}
        explicit jwk(const rsa::public_key& key, const options& o) : jwk(_of(key), o) {}
        explicit jwk(ed25519::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(ed25519::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const ed25519::public_key& key) : jwk(key, options()) {}
        explicit jwk(const ed25519::public_key& key, const options& o) : jwk(_of(key), o) {}
        explicit jwk(x25519::private_key key) : jwk(std::move(key), options()) {}
        explicit jwk(x25519::private_key key, const options& o) : jwk(_of(std::move(key)), o) {}
        explicit jwk(const x25519::public_key& key) : jwk(key, options()) {}
        explicit jwk(const x25519::public_key& key, const options& o) : jwk(_of(key), o) {}

        jwk(const jwk&) noexcept = default;
        jwk& operator=(const jwk&) noexcept = default;

        // A symmetric key ("oct"), its bytes copied into plain memory; an
        // empty key is std::invalid_argument
        static jwk symmetric(const slice<const byte>& key) {
            return symmetric(key, options());
        }

        static jwk symmetric(const slice<const byte>& key, const options& o) {
            if (key.size() == 0) {
                throw invalid_argument("sgcl::crypto::jose::jwk: an empty symmetric key");
            }
            auto k = std::make_unique<detail::JoseKey>();
            secret_bytes s(key.size());
            sgcl::detail::copy_bytes(s.as_slice().data(), key.data(), key.size());
            k->k.emplace<secret_bytes>(std::move(s));
            return jwk(std::move(k), o);
        }

        // A new key fit for a, which becomes its alg unless o names one
        static jwk generate(algorithm a) {
            return generate(a, options());
        }

        static jwk generate(algorithm a, const options& o) {
            options with = o;
            if (!with.alg) {
                with.alg = a;
            }
            switch (a) {
                case algorithm::hs256:
                case algorithm::hs384:
                case algorithm::hs512: {
                    auto s = random::secret(digest_size(detail::jose_hash(a)));
                    return symmetric(s, with);
                }
                case algorithm::rs256:
                case algorithm::rs384:
                case algorithm::rs512:
                case algorithm::ps256:
                case algorithm::ps384:
                case algorithm::ps512:
                case algorithm::rsa_oaep:
                case algorithm::rsa_oaep_256: return jwk(rsa::private_key::generate(2048), with);
                case algorithm::es256:
                case algorithm::ecdh_es:
                case algorithm::ecdh_es_a128kw:
                case algorithm::ecdh_es_a192kw:
                case algorithm::ecdh_es_a256kw: return jwk(p256::private_key::generate(), with);
                case algorithm::es384: return jwk(p384::private_key::generate(), with);
                case algorithm::es512: return jwk(p521::private_key::generate(), with);
                case algorithm::eddsa: return jwk(ed25519::private_key::generate(), with);
                case algorithm::dir: throw invalid_argument("sgcl::crypto::jose::jwk::generate: dir has no key of its own (symmetric of the enc's size)");
                default: {
                    auto s = random::secret(detail::jose_wrap_key_size(a));
                    if (s.size() == 0) {
                        throw invalid_argument("sgcl::crypto::jose::jwk::generate: an algorithm of no value of its enumeration");
                    }
                    return symmetric(s, with);
                }
            }
        }

        // A JWK's text, public or private: the private members decoded
        // straight into plain memory, unknown members kept in parameters().
        // A private key's text read by read_secret is taken where it lies
        static expected<jwk, error> parse(const string& text) noexcept {
            return _parse(text.view());
        }

        static expected<jwk, error> parse(const secret_bytes& text) noexcept {
            return _parse(std::string_view(reinterpret_cast<const char*>(text.as_slice().data()), text.size()));
        }

        // The key a literal of the program spells: parse's value, or
        // bad_expected_access<crypto::error> with parse's message (DESIGN 234)
        explicit jwk(const string& text)
        : jwk(parse(text).value()) {
        }

        // A private key in PEM, as the keys' from_pem read it (PKCS #8 of
        // each kind, SEC 1, PKCS #1); one read by read_secret where it lies
        static expected<jwk, error> from_pem(const string& pem) noexcept {
            return _from_pem(slice<const byte>(reinterpret_cast<const byte*>(pem.data()), pem.size()));
        }

        static expected<jwk, error> from_pem(const secret_bytes& pem) noexcept {
            return _from_pem(pem.as_slice());
        }

        key_type type() const noexcept {
            return _s->key->type();
        }

        // A private key, or an oct key
        bool is_private() const noexcept {
            return _s->key->is_private();
        }

        string kid() const noexcept {
            return _s->kid;
        }

        // The alg the key is for; nullopt when it has none, or one the
        // module does not have (parameters() holds it, and the key does
        // nothing)
        optional<algorithm> alg() const noexcept {
            return _s->alg;
        }

        string use() const noexcept {
            return _s->use;
        }

        // "P-256", "P-384", "Ed25519", "X25519"; "" for RSA and oct
        string crv() const noexcept {
            return string(_s->key->crv());
        }

        // Every public member, as read or made: kty, crv, x, y, n, e, kid,
        // use, alg, key_ops, x5c and any other
        encoding::json parameters() const noexcept {
            return _s->params;
        }

        // The public half, with the same members; an oct key has none:
        // std::logic_error
        jwk public_key() const {
            auto k = _s->key->public_half();
            auto s = make_tracked<detail::JwkState>();
            s->key = std::move(k);
            s->params = _s->params;
            s->kid = _s->kid;
            s->use = _s->use;
            s->alg = _s->alg;
            s->dir_enc = _s->dir_enc;
            s->alg_unknown = _s->alg_unknown;
            return jwk(std::move(s));
        }

        // The public JWK, compact; an oct key has none: std::logic_error
        string to_json() const {
            if (_s->key->type() == key_type::oct) {
                throw logic_error("sgcl::crypto::jose::jwk::to_json: a symmetric key has no public form (to_private_json)");
            }
            return _s->params.to_string();
        }

        // The JWK with its private members, compact, in plain memory
        secret_bytes to_private_json() const {
            detail::SecretText t;
            string pub = _s->params.to_string();
            std::string_view v = pub.view();
            t.put(v.substr(0, v.size() - 1));
            detail::jwk_private_members(*_s->key, t);
            t.put("}");
            return t.take();
        }

        // A private key's PKCS #8 in PEM ("PRIVATE KEY"); a public key or an
        // oct key: std::logic_error
        secret_bytes to_pem() const {
            const auto& k = _s->key->k;
            switch (k.index()) {
                case 2: return std::get<2>(k).to_pem();
                case 4: return std::get<4>(k).to_pem();
                case 6: return detail::write_key_pem("PRIVATE KEY", std::get<6>(k).to_pkcs8_der());
                case 8: return std::get<8>(k).to_pem();
                case 10: return detail::write_key_pem("PRIVATE KEY", std::get<10>(k).to_pkcs8_der());
                case 12: return std::get<12>(k).to_pem();
                default: throw logic_error("sgcl::crypto::jose::jwk::to_pem: only a private key of an asymmetric kind has a PEM");
            }
        }

        // RFC 7638: the required members in order, hashed, base64url
        string thumbprint() const {
            return thumbprint(hash_id::sha256);
        }

        string thumbprint(hash_id h) const {
            auto d = detail::jwk_thumbprint(*_s->key, h);
            return detail::b64url(d.as_slice());
        }

        // The same key: the same members of the key (the private ones
        // compared in constant time) and the same public members
        friend bool operator==(const jwk& a, const jwk& b) noexcept {
            if (a._s == b._s) {
                return true;
            }
            if (a.is_private() != b.is_private() || a.type() != b.type() || !(a._s->params == b._s->params)) {
                return false;
            }
            if (!a.is_private()) {
                return true;
            }
            auto x = a.to_private_json();
            auto y = b.to_private_json();
            return x == y;
        }

    private:
        friend struct detail::JwkAccess;
        friend struct detail::JoseTextAccess;
        friend class jwk_set;
        tracked_ptr<detail::JwkState> _s;

        static expected<jwk, error> _parse(std::string_view text) noexcept {
            auto r = detail::read_jwk(text);
            if (!r) {
                return unexpected(r.error());
            }
            return jwk(detail::jwk_read(std::move(*r)));
        }

        static expected<jwk, error> _from_pem(const slice<const byte>& pem) noexcept {
            auto block = detail::read_key_pem(pem, "sgcl::crypto::jose::jwk: ");
            if (!block) {
                return unexpected(block.error());
            }
            auto k = std::make_unique<detail::JoseKey>();
            const slice<const byte> der = block->der;
            if (block->label == "PRIVATE KEY") {
                if (auto e = ed25519::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<ed25519::private_key>(std::move(*e));
                } else if (auto x = x25519::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<x25519::private_key>(std::move(*x));
                } else if (auto p = p256::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<p256::private_key>(std::move(*p));
                } else if (auto q = p384::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<p384::private_key>(std::move(*q));
                } else if (auto u = p521::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<p521::private_key>(std::move(*u));
                } else if (auto r = rsa::private_key::from_pkcs8_der(der)) {
                    k->k.emplace<rsa::private_key>(std::move(*r));
                }
            } else if (block->label == "EC PRIVATE KEY") {
                if (auto p = p256::private_key::from_sec1_der(der)) {
                    k->k.emplace<p256::private_key>(std::move(*p));
                } else if (auto q = p384::private_key::from_sec1_der(der)) {
                    k->k.emplace<p384::private_key>(std::move(*q));
                } else if (auto u = p521::private_key::from_sec1_der(der)) {
                    k->k.emplace<p521::private_key>(std::move(*u));
                }
            } else if (auto r = rsa::private_key::from_pkcs1_der(der)) {
                k->k.emplace<rsa::private_key>(std::move(*r));
            }
            if (k->k.index() == 0) {
                return unexpected(detail::jose_error(errc::unsupported, "jwk: PEM: no key of a kind the module has, or one that does not read"));
            }
            return jwk(std::move(k), options());
        }

        explicit jwk(tracked_ptr<detail::JwkState> s) noexcept
        : _s(std::move(s)) {
        }

        jwk(std::unique_ptr<detail::JoseKey> k, const options& o)
        : _s(detail::jwk_made(std::move(k), o.kid, o.alg, o.use)) {
        }

        template<class K>
        static std::unique_ptr<detail::JoseKey> _of(K&& key) {
            auto k = std::make_unique<detail::JoseKey>();
            k->k.emplace<std::remove_cvref_t<K>>(std::forward<K>(key));
            return k;
        }
    };

    // A JWK Set (RFC 7517 5): the keys of an issuer, an OpenID provider's
    // jwks_uri. A value: a copy holds the same keys
    class jwk_set {
    public:
        jwk_set() noexcept = default;

        jwk_set(std::initializer_list<jwk> keys) {
            for (const auto& k : keys) {
                _keys.push_back(k);
            }
        }

        // {"keys":[…]}; a key of a kty or a curve the module does not have is
        // skipped, as RFC 7517 5 asks; a key that is malformed fails the set
        static expected<jwk_set, error> parse(const string& text) noexcept {
            return _parse(text.view());
        }

        static expected<jwk_set, error> parse(const secret_bytes& text) noexcept {
            return _parse(std::string_view(reinterpret_cast<const char*>(text.as_slice().data()), text.size()));
        }

        // The set a literal of the program spells: parse's value, or
        // bad_expected_access<crypto::error> with parse's message
        explicit jwk_set(const string& text)
        : jwk_set(parse(text).value()) {
        }

        size_t size() const noexcept {
            return _keys.size();
        }

        bool empty() const noexcept {
            return _keys.empty();
        }

        // The key at i; i past the end is a broken contract (an assertion)
        const jwk& operator[](size_t i) const noexcept {
            return _keys[i];
        }

        slice<const jwk> keys() const noexcept {
            return _keys.as_slice();
        }

        // The first key of the kid
        optional<jwk> find(const string& kid) const noexcept {
            for (const auto& k : _keys) {
                if (k.kid() == kid) {
                    return k;
                }
            }
            return nullopt;
        }

        void push_back(const jwk& key) {
            _keys.push_back(key);
        }

        // {"keys":[…]} of the public keys; oct keys left out
        string to_json() const {
            std::string s = "{\"keys\":[";
            bool first = true;
            for (const auto& k : _keys) {
                if (k.type() == key_type::oct) {
                    continue;
                }
                if (!first) {
                    s += ',';
                }
                first = false;
                s.append(k.to_json().view());
            }
            s += "]}";
            return string(std::string_view(s));
        }

        // {"keys":[…]} with the private members, in plain memory
        secret_bytes to_private_json() const {
            detail::SecretText t;
            t.put("{\"keys\":[");
            bool first = true;
            for (const auto& k : _keys) {
                if (!first) {
                    t.put(",");
                }
                first = false;
                auto one = k.to_private_json();
                t.put(std::string_view(reinterpret_cast<const char*>(one.as_slice().data()), one.size()));
            }
            t.put("]}");
            return t.take();
        }

    private:
        friend struct detail::JoseTextAccess;
        vector<jwk> _keys;

        static expected<jwk_set, error> _parse(std::string_view v) noexcept {

            std::vector<detail::JsonPlace> places;
            if (!detail::JsonPlaces::read(v, places)) {
                return unexpected(detail::jose_error(errc::malformed, "JWK Set: not one JSON object"));
            }
            const detail::JsonPlace* keys = nullptr;
            for (const auto& p : places) {
                if (p.key == "keys") {
                    keys = &p;
                }
            }
            if (!keys || keys->kind != '[') {
                return unexpected(detail::jose_error(errc::malformed, "JWK Set: no keys array"));
            }
            std::vector<std::string_view> items;
            if (!detail::JsonPlaces::elements(keys->value, items)) {
                return unexpected(detail::jose_error(errc::malformed, "JWK Set: keys is not an array"));
            }
            jwk_set out;
            for (auto item : items) {
                auto k = jwk::_parse(item);
                if (!k) {
                    if (k.error().code() == errc::unsupported) {
                        continue;
                    }
                    return unexpected(k.error());
                }
                out._keys.push_back(*k);
            }
            return out;
        }
    };

    // What a signer may set: the algorithm, and more members of the
    // protected header
    struct sign_options {
        optional<algorithm> alg;     // nullopt: the key's alg, else its kind's
        encoding::json header;       // an object of more members (typ, cty, kid, nonce, url…); alg is the library's
    };

    // A JSON Web Signature (RFC 7515), read: compact, flattened or general
    // JSON. A handle of one word; nothing in it is trusted until verify
    class jws {
    public:
        // The compact serialization of the payload signed by the key
        static string sign(const slice<const byte>& payload, const jwk& key) {
            return sign(payload, key, sign_options());
        }

        static string sign(const slice<const byte>& payload, const jwk& key, const sign_options& o) {
            const string p = detail::b64url(payload);
            auto part = detail::jws_sign_part(detail::JwkAccess::state(key), p, o.alg, o.header, {}, "jws::sign");
            return string::concat(part.protected_b64, ".", p, ".", part.signature_b64);
        }

        // The flattened JSON serialization (one key)
        static string sign_json(const slice<const byte>& payload, const jwk& key) {
            return sign_json(payload, key, sign_options());
        }

        static string sign_json(const slice<const byte>& payload, const jwk& key, const sign_options& o) {
            const string p = detail::b64url(payload);
            auto part = detail::jws_sign_part(detail::JwkAccess::state(key), p, o.alg, o.header, {}, "jws::sign_json");
            return string::concat("{\"payload\":\"", p, "\",\"protected\":\"", part.protected_b64, "\",\"signature\":\"", part.signature_b64, "\"}");
        }

        // The general JSON serialization, a signature by every key of the
        // set (o.alg, when given, for every key); an empty set is
        // std::invalid_argument
        static string sign_json(const slice<const byte>& payload, const jwk_set& keys) {
            return sign_json(payload, keys, sign_options());
        }

        static string sign_json(const slice<const byte>& payload, const jwk_set& keys, const sign_options& o) {
            if (keys.empty()) {
                throw invalid_argument("sgcl::crypto::jose::jws::sign_json: an empty key set");
            }
            const string p = detail::b64url(payload);
            std::string s = "{\"payload\":\"";
            s.append(p.view());
            s += "\",\"signatures\":[";
            for (size_t i = 0; i < keys.size(); ++i) {
                auto part = detail::jws_sign_part(detail::JwkAccess::state(keys[i]), p, o.alg, o.header, {}, "jws::sign_json");
                if (i) {
                    s += ',';
                }
                s += "{\"protected\":\"";
                s.append(part.protected_b64.view());
                s += "\",\"signature\":\"";
                s.append(part.signature_b64.view());
                s += "\"}";
            }
            s += "]}";
            return string(std::string_view(s));
        }

        // Read, nothing verified: compact, flattened or general JSON
        static expected<jws, error> parse(const string& text) noexcept {
            auto d = detail::jws_parse(text.view());
            if (!d) {
                return unexpected(d.error());
            }
            return jws(std::move(*d));
        }

        // The signature a literal of the program spells: parse's value, or
        // bad_expected_access<crypto::error> with parse's message
        explicit jws(const string& text)
        : jws(parse(text).value()) {
        }

        // The payload of text when a signature of it verifies under the key
        static expected<vector<byte>, error> verify(const string& text, const jwk& key) noexcept {
            auto j = parse(text);
            if (!j) {
                return unexpected(j.error());
            }
            return j->verify(key);
        }

        static expected<vector<byte>, error> verify(const string& text, const jwk_set& keys) noexcept {
            auto j = parse(text);
            if (!j) {
                return unexpected(j.error());
            }
            return j->verify(keys);
        }

        // The payload when a signature verifies under the key
        expected<vector<byte>, error> verify(const jwk& key) const noexcept {
            return detail::jws_verify(*_d, detail::JwkAccess::state(key));
        }

        // The payload when a signature verifies under a key of the set: the
        // keys of its kid when it names one (two of one kid refused), else
        // every key. A set that mixes oct keys with public keys is refused
        expected<vector<byte>, error> verify(const jwk_set& keys) const noexcept {
            bool oct = false;
            bool other = false;
            for (const auto& k : keys.keys()) {
                (k.type() == key_type::oct ? oct : other) = true;
            }
            if (oct && other) {
                return unexpected(detail::jose_error(errc::invalid_key, "JWS: a key set that mixes symmetric and public keys"));
            }
            optional<error> first;
            for (size_t i = 0; i < _d->signatures.size(); ++i) {
                const string kid = this->kid(i);
                size_t matches = 0;
                for (const auto& k : keys.keys()) {
                    matches += !kid.empty() && k.kid() == kid;
                }
                if (matches > 1) {
                    return unexpected(detail::jose_error(errc::invalid_key, "JWS: two keys of the signature's kid"));
                }
                for (const auto& k : keys.keys()) {
                    if (!kid.empty() && k.kid() != kid) {
                        continue;
                    }
                    detail::JwsData one;
                    one.payload_b64 = _d->payload_b64;
                    one.payload = _d->payload;
                    one.signatures.push_back(_d->signatures[i]);
                    auto r = detail::jws_verify(one, detail::JwkAccess::state(k));
                    if (r) {
                        return r;
                    }
                    if (!first) {
                        first = r.error();
                    }
                }
            }
            return unexpected(first ? *first : detail::jose_error(errc::verification, "JWS: no key of the set for the signature"));
        }

        size_t signature_count() const noexcept {
            return _d->signatures.size();
        }

        // The protected and the unprotected header of signature i together;
        // null past the last
        encoding::json header(size_t i = 0) const noexcept {
            return i < _d->signatures.size() ? _d->signatures[i].header : encoding::json();
        }

        optional<algorithm> alg(size_t i = 0) const noexcept {
            return detail::jose_alg(header(i)["alg"].as_string(string()).view());
        }

        string kid(size_t i = 0) const noexcept {
            return header(i)["kid"].as_string(string());
        }

        // The payload as it stands, before any verification
        vector<byte> unverified_payload() const {
            return _d->payload;
        }

    private:
        friend struct detail::JoseTextAccess;
        tracked_ptr<detail::JwsData> _d;

        explicit jws(tracked_ptr<detail::JwsData> d) noexcept
        : _d(std::move(d)) {
        }
    };

    // What an encrypter may set
    struct encrypt_options {
        optional<algorithm> alg;                  // nullopt: the key's alg, else its kind's
        encryption enc = encryption::a256gcm;
        encoding::json header;                    // an object of more members (typ, cty…)
    };

    // A JSON Web Encryption (RFC 7516), compact serialization
    class jwe {
    public:
        static string encrypt(const slice<const byte>& plaintext, const jwk& key) {
            return encrypt(plaintext, key, encrypt_options());
        }

        static string encrypt(const slice<const byte>& plaintext, const jwk& key, const encrypt_options& o) {
            const auto& s = detail::JwkAccess::state(key);
            detail::jose_header_check(o.header, "jwe::encrypt");
            optional<algorithm> a = o.alg ? o.alg : s.alg ? s.alg : detail::jose_default(*s.key, false);
            if (!a || detail::is_signature(*a)) {
                throw invalid_argument("sgcl::crypto::jose::jwe::encrypt: the algorithm is not one of key management");
            }
            if (auto why = detail::jose_refusal(s, detail::JosePurpose::encrypt, *a)) {
                throw invalid_argument(std::string("sgcl::crypto::jose::jwe::encrypt: ") + why);
            }
            if (s.dir_enc && *s.dir_enc != o.enc) {
                throw invalid_argument("sgcl::crypto::jose::jwe::encrypt: the key is for another enc");
            }
            const size_t cek_size = detail::jose_cek_size(o.enc);
            if (cek_size == 0) {
                throw invalid_argument("sgcl::crypto::jose::jwe::encrypt: an encryption of no value of its enumeration");
            }
            const auto& k = *s.key;
            secret_bytes cek;
            vector<byte> encrypted_key;
            encoding::json h = encoding::json::object({{"alg", string(detail::jose_name(*a))}, {"enc", string(detail::jose_name(o.enc))}});
            if (!s.kid.empty() && !o.header.contains(string("kid"))) {
                h = h.set(string("kid"), s.kid);
            }
            switch (*a) {
                case algorithm::dir: {
                    const auto& key_bytes = std::get<1>(k.k);
                    if (key_bytes.size() != cek_size) {
                        throw invalid_argument("sgcl::crypto::jose::jwe::encrypt: a dir key of another size than the enc's");
                    }
                    cek = key_bytes.clone();
                    break;
                }
                case algorithm::rsa_oaep:
                case algorithm::rsa_oaep_256: {
                    cek = random::secret(cek_size);
                    optional<rsa::public_key> tmp;
                    auto p = k.rsa_public(tmp);
                    encrypted_key = p->encrypt_oaep(*a == algorithm::rsa_oaep ? hash_id::sha1 : hash_id::sha256, cek);
                    break;
                }
                case algorithm::a128gcmkw:
                case algorithm::a192gcmkw:
                case algorithm::a256gcmkw: {
                    cek = random::secret(cek_size);
                    auto iv = random::bytes(12);
                    aes_gcm g(std::get<1>(k.k).as_slice());
                    auto sealed = g.seal(iv.as_slice(), cek);
                    encrypted_key = vector<byte>(sealed.data(), sealed.data() + cek_size);
                    h = h.set(string("iv"), detail::b64url(iv.as_slice()));
                    h = h.set(string("tag"), detail::b64url(slice<const byte>(sealed.data() + cek_size, 16)));
                    break;
                }
                case algorithm::ecdh_es: {
                    auto [z, epk] = detail::ecdh_ephemeral(k);
                    h = h.set(string("epk"), epk);
                    cek = detail::concat_kdf(z, detail::jose_name(o.enc), {}, {}, cek_size);
                    break;
                }
                case algorithm::a128kw:
                case algorithm::a192kw:
                case algorithm::a256kw:
                case algorithm::ecdh_es_a128kw:
                case algorithm::ecdh_es_a192kw:
                case algorithm::ecdh_es_a256kw: {
                    secret_bytes kek;
                    if (*a >= algorithm::ecdh_es_a128kw) {
                        auto [z, epk] = detail::ecdh_ephemeral(k);
                        h = h.set(string("epk"), epk);
                        kek = detail::concat_kdf(z, detail::jose_name(*a), {}, {}, detail::jose_wrap_key_size(*a));
                    } else {
                        kek = std::get<1>(k.k).clone();
                    }
                    cek = random::secret(cek_size);
                    encrypted_key = aes_kw(kek.as_slice()).wrap(cek.as_slice());
                    break;
                }
                default: throw invalid_argument("sgcl::crypto::jose::jwe::encrypt: an algorithm of no value of its enumeration");
            }
            for (const auto& m : o.header.members()) {
                if (m.key.view() != "alg" && m.key.view() != "enc") {
                    h = h.set(m.key, m.value);
                }
            }
            detail::JweData d;
            d.protected_b64 = detail::b64url(detail::jose_bytes(h.to_string()));
            detail::jwe_seal(o.enc, cek, plaintext, detail::jose_bytes(d.protected_b64), d);
            return string::concat(d.protected_b64, ".", detail::b64url(encrypted_key.as_slice()), ".", detail::b64url(d.iv.as_slice()), ".",
                                  detail::b64url(d.ciphertext.as_slice()), ".", detail::b64url(d.tag.as_slice()));
        }

        // The plaintext of a compact JWE, decrypted under the key
        static expected<vector<byte>, error> decrypt(const string& compact, const jwk& key) noexcept {
            auto j = parse(compact);
            if (!j) {
                return unexpected(j.error());
            }
            return j->decrypt(key);
        }

        // Under the key of its kid, or every key that can when it names none
        static expected<vector<byte>, error> decrypt(const string& compact, const jwk_set& keys) noexcept {
            auto j = parse(compact);
            if (!j) {
                return unexpected(j.error());
            }
            const string kid = j->kid();
            optional<error> first;
            for (const auto& k : keys.keys()) {
                if (!kid.empty() && k.kid() != kid) {
                    continue;
                }
                auto r = j->decrypt(k);
                if (r) {
                    return r;
                }
                if (!first) {
                    first = r.error();
                }
            }
            return unexpected(first ? *first : detail::jose_error(errc::invalid_key, "JWE: no key of the set for it"));
        }

        static expected<jwe, error> parse(const string& compact) noexcept {
            auto d = detail::jwe_parse(compact.view());
            if (!d) {
                return unexpected(d.error());
            }
            return jwe(std::move(*d));
        }

        // The encryption a literal of the program spells: parse's value, or
        // bad_expected_access<crypto::error> with parse's message
        explicit jwe(const string& compact)
        : jwe(parse(compact).value()) {
        }

        expected<vector<byte>, error> decrypt(const jwk& key) const noexcept {
            return detail::jwe_decrypt(*_d, detail::JwkAccess::state(key));
        }

        // The protected header
        encoding::json header() const noexcept {
            return _d->header;
        }

        optional<algorithm> alg() const noexcept {
            return detail::jose_alg(_d->header["alg"].as_string(string()).view());
        }

        optional<encryption> enc() const noexcept {
            return detail::jose_enc(_d->header["enc"].as_string(string()).view());
        }

        string kid() const noexcept {
            return _d->header["kid"].as_string(string());
        }

    private:
        friend struct detail::JoseTextAccess;
        tracked_ptr<detail::JweData> _d;

        explicit jwe(tracked_ptr<detail::JweData> d) noexcept
        : _d(std::move(d)) {
        }
    };

    // A JSON Web Token (RFC 7519): a compact JWS whose payload is a JSON
    // object of claims. A handle of one word
    class jwt {
    public:
        // How the claims of a JWT are checked
        struct verify_options {
            string issuer;                  // iss must be this; empty: not checked
            string audience;                // aud must hold this; empty: a token with an aud is refused (RFC 7519 4.1.3)
            duration leeway = minute;       // the clocks' skew allowed for exp, nbf and iat
            optional<time::datetime> at;    // the instant checked against; nullopt: time::now()
            bool require_expiration = true; // a token without exp is refused
        };

        // The claims signed by the key, compact; typ "JWT" unless the
        // header names one. Claims that are not an object:
        // std::invalid_argument
        static string sign(const encoding::json& claims, const jwk& key) {
            return sign(claims, key, sign_options());
        }

        static string sign(const encoding::json& claims, const jwk& key, const sign_options& o) {
            if (!claims.is_object()) {
                throw invalid_argument("sgcl::crypto::jose::jwt::sign: the claims are not a JSON object");
            }
            const string text = claims.to_string();
            const string p = detail::b64url(detail::jose_bytes(text));
            auto part = detail::jws_sign_part(detail::JwkAccess::state(key), p, o.alg, o.header, {{"typ", "JWT"}}, "jwt::sign");
            return string::concat(part.protected_b64, ".", p, ".", part.signature_b64);
        }

        // The token verified under the key and its claims checked
        static expected<jwt, error> verify(const string& token, const jwk& key) noexcept {
            return verify(token, key, verify_options());
        }

        static expected<jwt, error> verify(const string& token, const jwk& key, const verify_options& o) noexcept {
            return _verify(token.view(), key, o);
        }

    private:
        friend struct detail::JoseTextAccess;

        template<class K>
        static expected<jwt, error> _verify(std::string_view token, const K& key, const verify_options& o) noexcept {
            auto j = _compact(token);
            if (!j) {
                return unexpected(j.error());
            }
            auto payload = j->verify(key);
            if (!payload) {
                return unexpected(payload.error());
            }
            return _checked(*j, *payload, o);
        }

        static expected<jwt, error> _parse_unverified(std::string_view token) noexcept {
            auto j = _compact(token);
            if (!j) {
                return unexpected(j.error());
            }
            auto c = _claims(j->unverified_payload());
            if (!c) {
                return unexpected(c.error());
            }
            return jwt(j->header(), *c);
        }

    public:
        static expected<jwt, error> verify(const string& token, const jwk_set& keys) noexcept {
            return verify(token, keys, verify_options());
        }

        static expected<jwt, error> verify(const string& token, const jwk_set& keys, const verify_options& o) noexcept {
            return _verify(token.view(), keys, o);
        }

        // Read without verifying the signature or a claim: to look at the
        // issuer before the key is chosen. Nothing in it is to be trusted
        static expected<jwt, error> parse_unverified(const string& token) noexcept {
            return _parse_unverified(token.view());
        }

        encoding::json claims() const noexcept {
            return _d->claims;
        }

        encoding::json header() const noexcept {
            return _d->header;
        }

        string issuer() const noexcept {
            return _d->claims["iss"].as_string(string());
        }

        string subject() const noexcept {
            return _d->claims["sub"].as_string(string());
        }

        string id() const noexcept {
            return _d->claims["jti"].as_string(string());
        }

        // aud: one string or a list of them
        vector<string> audience() const {
            vector<string> out;
            const auto& a = _d->claims["aud"];
            if (auto s = a.as_string()) {
                out.push_back(*s);
            }
            for (const auto& e : a.elements()) {
                if (auto s = e.as_string()) {
                    out.push_back(*s);
                }
            }
            return out;
        }

        optional<time::datetime> expires_at() const noexcept {
            return _time("exp");
        }

        optional<time::datetime> not_before() const noexcept {
            return _time("nbf");
        }

        optional<time::datetime> issued_at() const noexcept {
            return _time("iat");
        }

    private:
        tracked_ptr<detail::JwtData> _d;

        jwt(const encoding::json& header, const encoding::json& claims) {
            _d = make_tracked<detail::JwtData>();
            _d->header = header;
            _d->claims = claims;
        }

        static expected<jws, error> _compact(std::string_view v) noexcept {
            if (v.empty() || v.front() == '{' || v.front() == ' ') {
                return unexpected(detail::jose_error(errc::malformed, "JWT: a token is a compact JWS"));
            }
            return detail::JoseTextAccess::jws_of(v);
        }

        static expected<encoding::json, error> _claims(const vector<byte>& payload) noexcept {
            auto c = encoding::json::parse(detail::jose_text(payload.as_slice()));
            if (!c || !c->is_object()) {
                return unexpected(detail::jose_error(errc::malformed, "JWT: the claims are not a JSON object"));
            }
            return *c;
        }

        // A NumericDate (RFC 7519 2): seconds, a fraction allowed
        static optional<double> _numeric(const encoding::json& v) noexcept {
            if (!v.is_number()) {
                return nullopt;
            }
            return v.as_double();
        }

        optional<time::datetime> _time(const char* name) const noexcept {
            auto t = _numeric(_d->claims[string(name)]);
            if (!t || !(*t > -1e15 && *t < 1e15)) {
                return nullopt;
            }
            return time::datetime::from_unix(int64_t(*t), time::zone::utc());
        }

        static expected<jwt, error> _checked(const jws& j, const vector<byte>& payload, const verify_options& o) noexcept {
            using detail::jose_error;
            auto c = _claims(payload);
            if (!c) {
                return unexpected(c.error());
            }
            const auto& claims = *c;
            const double now = double(o.at ? o.at->unix() : time::now().unix());
            const double leeway = o.leeway.seconds();
            auto date = [&](const char* name) -> expected<optional<double>, error> {
                if (!claims.contains(string(name))) {
                    return optional<double>();
                }
                auto t = _numeric(claims[string(name)]);
                if (!t) {
                    return unexpected(jose_error(errc::malformed, std::string("JWT: ") + name + " is not a number"));
                }
                return t;
            };
            auto exp = date("exp");
            auto nbf = date("nbf");
            auto iat = date("iat");
            for (const auto* d : {&exp, &nbf, &iat}) {
                if (!*d) {
                    return unexpected(d->error());
                }
            }
            if (*exp) {
                if (now >= **exp + leeway) {
                    return unexpected(jose_error(errc::expired, "JWT: the token has expired (exp)"));
                }
            } else if (o.require_expiration) {
                return unexpected(jose_error(errc::verification, "JWT: the token has no exp"));
            }
            if (*nbf && now + leeway < **nbf) {
                return unexpected(jose_error(errc::not_yet_valid, "JWT: the token is not valid yet (nbf)"));
            }
            if (*iat && **iat > now + leeway) {
                return unexpected(jose_error(errc::not_yet_valid, "JWT: the token was issued in the future (iat)"));
            }
            if (!o.issuer.empty() && claims["iss"].as_string() != optional<string>(o.issuer)) {
                return unexpected(jose_error(errc::verification, "JWT: the issuer (iss) is not the one expected"));
            }
            const auto& aud = claims["aud"];
            if (claims.contains(string("aud"))) {
                if (!aud.is_string() && !aud.is_array()) {
                    return unexpected(jose_error(errc::malformed, "JWT: aud is neither a string nor a list"));
                }
                bool found = false;
                if (aud.is_string()) {
                    found = !o.audience.empty() && aud.as_string() == optional<string>(o.audience);
                }
                for (const auto& e : aud.elements()) {
                    if (!e.is_string()) {
                        return unexpected(jose_error(errc::malformed, "JWT: aud holds a value that is not a string"));
                    }
                    found |= !o.audience.empty() && e.as_string() == optional<string>(o.audience);
                }
                if (!found) {
                    return unexpected(jose_error(errc::verification, o.audience.empty() ? "JWT: the token names an audience (aud) and none was expected"
                                                                                       : "JWT: the audience (aud) does not hold the one expected"));
                }
            } else if (!o.audience.empty()) {
                return unexpected(jose_error(errc::verification, "JWT: the token names no audience (aud)"));
            }
            return jwt(j.header(), claims);
        }
    };
}

namespace sgcl::crypto::detail {
    inline expected<jose::jwk, error> JoseTextAccess::jwk_of(std::string_view text) noexcept {
        return jose::jwk::_parse(text);
    }

    inline expected<jose::jwk_set, error> JoseTextAccess::jwk_set_of(std::string_view text) noexcept {
        return jose::jwk_set::_parse(text);
    }

    inline expected<jose::jws, error> JoseTextAccess::jws_of(std::string_view text) noexcept {
        auto d = jws_parse(text);
        if (!d) {
            return unexpected(d.error());
        }
        return jose::jws(std::move(*d));
    }

    inline expected<jose::jwe, error> JoseTextAccess::jwe_of(std::string_view text) noexcept {
        auto d = jwe_parse(text);
        if (!d) {
            return unexpected(d.error());
        }
        return jose::jwe(std::move(*d));
    }

    inline expected<jose::jwt, error> JoseTextAccess::jwt_unverified(std::string_view token) noexcept {
        return jose::jwt::_parse_unverified(token);
    }

    template<class K, class O>
    inline expected<jose::jwt, error> JoseTextAccess::jwt_verify(std::string_view token, const K& key, const O& o) noexcept {
        return jose::jwt::_verify(token, key, o);
    }

    inline expected<jose::jwk, error> JwkAccess::from_pem(const slice<const byte>& pem) noexcept {
        return jose::jwk::_from_pem(pem);
    }

    SGCL_INLINE_HOT const JwkState& JwkAccess::state(const jose::jwk& k) noexcept {
        return *k._s;
    }
}
