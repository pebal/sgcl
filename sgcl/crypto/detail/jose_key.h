//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../constant_time.h"
#include "../ed25519.h"
#include "../error.h"
#include "../hash_id.h"
#include "../hmac.h"
#include "../p256.h"
#include "../p384.h"
#include "../p521.h"
#include "../random.h"
#include "../rsa.h"
#include "../secret.h"
#include "../sha256.h"
#include "../sha512.h"
#include "../x25519.h"
#include "der.h"
#include "jose_text.h"
#include "key_pem.h"
#include "../../encoding/json.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <utility>
#include <variant>

namespace sgcl::crypto::jose {
    // "alg" (RFC 7518 §3.1 and §4.1, RFC 8037 §3.1): the algorithm of a
    // JWS's signature or of a JWE's key management; one list, as the
    // registry has it
    enum class algorithm : uint8_t {
        hs256 = 1,
        hs384,
        hs512,
        rs256,
        rs384,
        rs512,
        ps256,
        ps384,
        ps512,
        es256,
        es384,
        es512,
        eddsa,
        rsa_oaep,
        rsa_oaep_256,
        a128kw,
        a192kw,
        a256kw,
        a128gcmkw,
        a192gcmkw,
        a256gcmkw,
        dir,
        ecdh_es,
        ecdh_es_a128kw,
        ecdh_es_a192kw,
        ecdh_es_a256kw
    };

    // "enc" (RFC 7518 §5.1): the content encryption of a JWE
    enum class encryption : uint8_t {
        a128cbc_hs256 = 1,
        a192cbc_hs384,
        a256cbc_hs512,
        a128gcm,
        a192gcm,
        a256gcm
    };

    // "kty" (RFC 7518 §6.1, RFC 8037 §2)
    enum class key_type : uint8_t {
        ec = 1,
        rsa,
        oct,
        okp
    };

    class jwk;
    class jwk_set;
}

namespace sgcl::crypto::detail {
    using jose::algorithm;
    using jose::encryption;
    using jose::key_type;

    // --- the names of the registries -------------------------------------

    struct AlgName {
        algorithm alg;
        std::string_view name;
    };

    inline constexpr AlgName AlgNames[] = {
        {algorithm::hs256, "HS256"}, {algorithm::hs384, "HS384"}, {algorithm::hs512, "HS512"},
        {algorithm::rs256, "RS256"}, {algorithm::rs384, "RS384"}, {algorithm::rs512, "RS512"},
        {algorithm::ps256, "PS256"}, {algorithm::ps384, "PS384"}, {algorithm::ps512, "PS512"},
        {algorithm::es256, "ES256"}, {algorithm::es384, "ES384"}, {algorithm::es512, "ES512"}, {algorithm::eddsa, "EdDSA"},
        {algorithm::rsa_oaep, "RSA-OAEP"}, {algorithm::rsa_oaep_256, "RSA-OAEP-256"},
        {algorithm::a128kw, "A128KW"}, {algorithm::a192kw, "A192KW"}, {algorithm::a256kw, "A256KW"},
        {algorithm::a128gcmkw, "A128GCMKW"}, {algorithm::a192gcmkw, "A192GCMKW"}, {algorithm::a256gcmkw, "A256GCMKW"},
        {algorithm::dir, "dir"}, {algorithm::ecdh_es, "ECDH-ES"}, {algorithm::ecdh_es_a128kw, "ECDH-ES+A128KW"},
        {algorithm::ecdh_es_a192kw, "ECDH-ES+A192KW"}, {algorithm::ecdh_es_a256kw, "ECDH-ES+A256KW"}};

    struct EncName {
        encryption enc;
        std::string_view name;
    };

    inline constexpr EncName EncNames[] = {
        {encryption::a128cbc_hs256, "A128CBC-HS256"}, {encryption::a192cbc_hs384, "A192CBC-HS384"},
        {encryption::a256cbc_hs512, "A256CBC-HS512"}, {encryption::a128gcm, "A128GCM"},
        {encryption::a192gcm, "A192GCM"}, {encryption::a256gcm, "A256GCM"}};

    // The name of a value of the enumeration; a value cast from a number
    // that is none of the list is a broken contract
    inline std::string_view jose_name(algorithm a) {
        for (const auto& n : AlgNames) {
            if (n.alg == a) {
                return n.name;
            }
        }
        throw invalid_argument("sgcl::crypto::jose: an algorithm of no value of its enumeration");
    }

    inline std::string_view jose_name(encryption e) {
        for (const auto& n : EncNames) {
            if (n.enc == e) {
                return n.name;
            }
        }
        throw invalid_argument("sgcl::crypto::jose: an encryption of no value of its enumeration");
    }

    inline optional<algorithm> jose_alg(std::string_view s) noexcept {
        for (const auto& n : AlgNames) {
            if (n.name == s) {
                return n.alg;
            }
        }
        return nullopt;
    }

    inline optional<encryption> jose_enc(std::string_view s) noexcept {
        for (const auto& n : EncNames) {
            if (n.name == s) {
                return n.enc;
            }
        }
        return nullopt;
    }

    SGCL_INLINE_HOT bool is_signature(algorithm a) noexcept {
        return a >= algorithm::hs256 && a <= algorithm::eddsa;
    }

    SGCL_INLINE_HOT bool is_hmac(algorithm a) noexcept {
        return a >= algorithm::hs256 && a <= algorithm::hs512;
    }

    // The digest of a signature algorithm (EdDSA's is its own)
    SGCL_INLINE_HOT hash_id jose_hash(algorithm a) noexcept {
        switch (a) {
            case algorithm::hs384:
            case algorithm::rs384:
            case algorithm::ps384:
            case algorithm::es384: return hash_id::sha384;
            case algorithm::hs512:
            case algorithm::rs512:
            case algorithm::ps512:
            case algorithm::es512: return hash_id::sha512;
            default: return hash_id::sha256;
        }
    }

    // The key of an AES key wrap or AES-GCM key wrap, in bytes; 0 for any
    // other algorithm
    SGCL_INLINE_HOT size_t jose_wrap_key_size(algorithm a) noexcept {
        switch (a) {
            case algorithm::a128kw:
            case algorithm::a128gcmkw:
            case algorithm::ecdh_es_a128kw: return 16;
            case algorithm::a192kw:
            case algorithm::a192gcmkw:
            case algorithm::ecdh_es_a192kw: return 24;
            case algorithm::a256kw:
            case algorithm::a256gcmkw:
            case algorithm::ecdh_es_a256kw: return 32;
            default: return 0;
        }
    }

    // The content key of an encryption, in bytes
    SGCL_INLINE_HOT size_t jose_cek_size(encryption e) noexcept {
        switch (e) {
            case encryption::a128cbc_hs256: return 32;
            case encryption::a192cbc_hs384: return 48;
            case encryption::a256cbc_hs512: return 64;
            case encryption::a128gcm: return 16;
            case encryption::a192gcm: return 24;
            case encryption::a256gcm: return 32;
        }
        return 0;
    }

    // --- the key ----------------------------------------------------------

    // The odd primes to 1427 under which 65537 does not generate the whole
    // multiplicative group, with its order there: only these can tell a
    // modulus of ROCA's form from another. Made when the program is
    // compiled, the most telling first (the smallest share of the group)
    struct RocaPrime {
        uint16_t p;
        uint16_t order;
    };

    inline constexpr auto RocaPrimes = [] {
        std::array<RocaPrime, 230> t{};
        size_t n = 0;
        for (uint32_t p = 3; p <= 1427; p += 2) {
            bool prime = true;
            for (uint32_t q = 3; q * q <= p; q += 2) {
                prime &= p % q != 0;
            }
            if (!prime) {
                continue;
            }
            const uint32_t g = 65537 % p;
            uint32_t x = g;
            uint32_t order = 1;
            while (x != 1) {
                x = x * g % p;
                ++order;
            }
            if (order < p - 1) {
                t[n++] = RocaPrime{uint16_t(p), uint16_t(order)};
            }
        }
        // by order / (p - 1), ascending
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (uint32_t(t[j].order) * (t[i].p - 1u) < uint32_t(t[i].order) * (t[j].p - 1u)) {
                    RocaPrime x = t[i];
                    t[i] = t[j];
                    t[j] = x;
                }
            }
        }
        return std::pair<std::array<RocaPrime, 230>, size_t>(t, n);
    }();

    // Whether an RSA modulus has the fingerprint of the Infineon keys of
    // CVE-2017-15361 (ROCA, Nemec et al., CCS 2017): their primes are
    // k M + (65537^a mod M), M the product of the first 126 primes for keys
    // of 1984 to 3936 bits (225 for 3968 to 4096), so n mod p lies in the
    // subgroup 65537 generates mod every odd prime p of M: r^order is 1. A
    // modulus that passes for every one is refused; one at random does with
    // odds of 2^-167 (2^-281), and is told apart after a few primes. Sizes
    // the generator never made are not checked. The modulus is public
    inline bool rsa_roca(const vector<byte>& n, size_t bits) noexcept {
        const uint32_t limit = bits >= 1984 && bits <= 3936 ? 701 : bits >= 3968 && bits <= 4096 ? 1427 : 0;
        if (limit == 0) {
            return false;
        }
        const auto& [table, count] = RocaPrimes;
        for (size_t i = 0; i < count; ++i) {
            const uint32_t p = table[i].p;
            if (p > limit) {
                continue;
            }
            // n mod p, four bytes at a time
            uint64_t r = 0;
            size_t k = 0;
            const size_t head = n.size() % 4;
            for (; k < head; ++k) {
                r = (r << 8 | uint64_t(n[k])) % p;
            }
            for (; k < n.size(); k += 4) {
                const uint64_t w = uint64_t(n[k]) << 24 | uint64_t(n[k + 1]) << 16 | uint64_t(n[k + 2]) << 8 | uint64_t(n[k + 3]);
                r = (r << 32 | w) % p;
            }
            // r^order mod p
            uint64_t acc = 1;
            uint64_t base = r;
            for (uint32_t e = table[i].order; e != 0; e >>= 1) {
                if (e & 1) {
                    acc = acc * base % p;
                }
                base = base * base % p;
            }
            if (r == 0 || acc != 1) {
                return false;
            }
        }
        return true;
    }

    // One key of the kinds a JWK has, in plain memory: never copied, each
    // alternative zeroing its own secret when it goes
    struct JoseKey {
        bool roca = false;   // an RSA modulus of CVE-2017-15361, found when the key is made or read

        std::variant<std::monostate, secret_bytes, p256::private_key, p256::public_key, p384::private_key, p384::public_key,
                     rsa::private_key, rsa::public_key, ed25519::private_key, ed25519::public_key, x25519::private_key,
                     x25519::public_key, p521::private_key, p521::public_key>
            k;

        template<class T>
        SGCL_INLINE_HOT const T* get() const noexcept {
            return std::get_if<T>(&k);
        }

        // The ROCA flag set from the key's modulus
        void check_rsa() {
            optional<rsa::public_key> tmp;
            if (auto p = rsa_public(tmp)) {
                roca = rsa_roca(p->modulus(), p->bits());
            }
        }

        key_type type() const noexcept {
            switch (k.index()) {
                case 1: return key_type::oct;
                case 2:
                case 3:
                case 4:
                case 5:
                case 12:
                case 13: return key_type::ec;
                case 6:
                case 7: return key_type::rsa;
                default: return key_type::okp;
            }
        }

        bool is_private() const noexcept {
            const size_t i = k.index();
            return i == 1 || i == 2 || i == 4 || i == 6 || i == 8 || i == 10 || i == 12;
        }

        std::string_view crv() const noexcept {
            switch (k.index()) {
                case 2:
                case 3: return "P-256";
                case 4:
                case 5: return "P-384";
                case 12:
                case 13: return "P-521";
                case 8:
                case 9: return "Ed25519";
                case 10:
                case 11: return "X25519";
                default: return {};
            }
        }

        // The public key of an RSA key, private or public
        const rsa::public_key* rsa_public(optional<rsa::public_key>& tmp) const {
            if (auto p = get<rsa::public_key>()) {
                return p;
            }
            if (auto s = get<rsa::private_key>()) {
                tmp.emplace(s->public_key());
                return &*tmp;
            }
            return nullptr;
        }

        size_t rsa_bits() const {
            optional<rsa::public_key> tmp;
            auto p = rsa_public(tmp);
            return p ? p->bits() : 0;
        }

        // The key with its private half left behind
        std::unique_ptr<JoseKey> public_half() const {
            auto out = std::make_unique<JoseKey>();
            out->roca = roca;
            switch (k.index()) {
                case 2: out->k.emplace<p256::public_key>(std::get<2>(k).public_key()); break;
                case 3: out->k.emplace<p256::public_key>(std::get<3>(k)); break;
                case 4: out->k.emplace<p384::public_key>(std::get<4>(k).public_key()); break;
                case 5: out->k.emplace<p384::public_key>(std::get<5>(k)); break;
                case 6: out->k.emplace<rsa::public_key>(std::get<6>(k).public_key()); break;
                case 7: out->k.emplace<rsa::public_key>(std::get<7>(k)); break;
                case 8: out->k.emplace<ed25519::public_key>(std::get<8>(k).public_key()); break;
                case 9: out->k.emplace<ed25519::public_key>(std::get<9>(k)); break;
                case 10: out->k.emplace<x25519::public_key>(std::get<10>(k).public_key()); break;
                case 11: out->k.emplace<x25519::public_key>(std::get<11>(k)); break;
                case 12: out->k.emplace<p521::public_key>(std::get<12>(k).public_key()); break;
                case 13: out->k.emplace<p521::public_key>(std::get<13>(k)); break;
                default: throw logic_error("sgcl::crypto::jose::jwk: a symmetric key has no public half");
            }
            return out;
        }
    };

    // --- the members of a JWK --------------------------------------------

    // The public members of the key itself, in RFC 7517's order (kty, crv,
    // x, y; kty, n, e; kty, crv, x; kty), as text
    inline std::string jwk_key_members(const JoseKey& key) {
        std::string s = "\"kty\":\"";
        auto put_b64 = [&](const slice<const byte>& b) {
            s.append(b64url(b).view());
        };
        auto ec = [&](std::string_view crv, const auto& point, size_t n) {
            s += "EC\",\"crv\":\"";
            s += crv;
            s += "\",\"x\":\"";
            put_b64(slice<const byte>(point.data() + 1, n));
            s += "\",\"y\":\"";
            put_b64(slice<const byte>(point.data() + 1 + n, n));
            s += '"';
        };
        switch (key.k.index()) {
            case 1: s += "oct\""; break;
            case 2: ec("P-256", std::get<2>(key.k).public_key().bytes(), 32); break;
            case 3: ec("P-256", std::get<3>(key.k).bytes(), 32); break;
            case 4: ec("P-384", std::get<4>(key.k).public_key().bytes(), 48); break;
            case 5: ec("P-384", std::get<5>(key.k).bytes(), 48); break;
            case 12: ec("P-521", std::get<12>(key.k).public_key().bytes(), 66); break;
            case 13: ec("P-521", std::get<13>(key.k).bytes(), 66); break;
            case 6:
            case 7: {
                optional<rsa::public_key> tmp;
                auto p = key.rsa_public(tmp);
                auto n = p->modulus();
                uint64_t e = p->exponent();
                unsigned char eb[8];
                size_t m = 0;
                for (int i = 7; i >= 0; --i) {
                    unsigned char b = static_cast<unsigned char>(e >> (8 * i));
                    if (m || b) {
                        eb[m++] = b;
                    }
                }
                s += "RSA\",\"n\":\"";
                size_t skip = 0;
                while (skip + 1 < n.size() && n[skip] == byte(0)) {
                    ++skip;
                }
                put_b64(slice<const byte>(n.data() + skip, n.size() - skip));
                s += "\",\"e\":\"";
                put_b64(slice<const byte>(reinterpret_cast<const byte*>(eb), m));
                s += '"';
                break;
            }
            case 8:
            case 9:
            case 10:
            case 11: {
                s += "OKP\",\"crv\":\"";
                s += key.crv();
                s += "\",\"x\":\"";
                switch (key.k.index()) {
                    case 8: put_b64(std::get<8>(key.k).public_key().bytes()); break;
                    case 9: put_b64(std::get<9>(key.k).bytes()); break;
                    case 10: put_b64(std::get<10>(key.k).public_key().bytes()); break;
                    default: put_b64(std::get<11>(key.k).bytes()); break;
                }
                s += '"';
                break;
            }
            default: break;
        }
        return s;
    }

    // The private members of a private key, ",\"d\":\"…\"" and the rest,
    // into a secret text: the scalar, the seed, RSA's numbers read back from
    // its PKCS #1 DER (a secret_bytes), the octets of a symmetric key
    inline void jwk_private_members(const JoseKey& key, SecretText& out) {
        switch (key.k.index()) {
            case 1:
                out.put(",\"k\":\"");
                out.put_b64(std::get<1>(key.k).as_slice());
                out.put("\"");
                break;
            case 2: {
                auto d = std::get<2>(key.k).bytes();
                out.put(",\"d\":\"");
                out.put_b64(d.bytes());
                out.put("\"");
                break;
            }
            case 4: {
                auto d = std::get<4>(key.k).bytes();
                out.put(",\"d\":\"");
                out.put_b64(d.bytes());
                out.put("\"");
                break;
            }
            case 12: {
                auto d = std::get<12>(key.k).bytes();
                out.put(",\"d\":\"");
                out.put_b64(d.bytes());
                out.put("\"");
                break;
            }
            case 6: {
                auto der = std::get<6>(key.k).to_pkcs1_der();
                DerReader in(reinterpret_cast<const unsigned char*>(der.as_slice().data()), der.size());
                DerReader seq;
                in.read(der::sequence, seq);
                const unsigned char* p;
                size_t n;
                static constexpr const char* names[9] = {nullptr, nullptr, nullptr, "d", "p", "q", "dp", "dq", "qi"};
                for (int i = 0; i < 9; ++i) {
                    seq.read_unsigned_bytes(p, n);
                    if (names[i]) {
                        out.put(",\"");
                        out.put(names[i]);
                        out.put("\":\"");
                        out.put_b64(slice<const byte>(reinterpret_cast<const byte*>(p), n));
                        out.put("\"");
                    }
                }
                break;
            }
            case 8: {
                auto seed = std::get<8>(key.k).seed();
                out.put(",\"d\":\"");
                out.put_b64(seed.bytes());
                out.put("\"");
                break;
            }
            case 10: {
                auto d = std::get<10>(key.k).bytes();
                out.put(",\"d\":\"");
                out.put_b64(d.bytes());
                out.put("\"");
                break;
            }
            default: break;
        }
    }

    // RFC 7638 §3: the required members of the key in lexicographic order,
    // no white space; hashed. For an oct key the text holds the key, so it
    // is made in a secret text
    inline vector<byte> jwk_thumbprint(const JoseKey& key, hash_id h) {
        SecretText t;
        auto b64 = [&](const slice<const byte>& b) {
            t.put("\"");
            t.put_b64(b);
            t.put("\"");
        };
        auto ec = [&](std::string_view crv, const auto& point, size_t n) {
            t.put("{\"crv\":\"");
            t.put(crv);
            t.put("\",\"kty\":\"EC\",\"x\":");
            b64(slice<const byte>(point.data() + 1, n));
            t.put(",\"y\":");
            b64(slice<const byte>(point.data() + 1 + n, n));
            t.put("}");
        };
        switch (key.k.index()) {
            case 1:
                t.put("{\"k\":");
                b64(std::get<1>(key.k).as_slice());
                t.put(",\"kty\":\"oct\"}");
                break;
            case 2: ec("P-256", std::get<2>(key.k).public_key().bytes(), 32); break;
            case 3: ec("P-256", std::get<3>(key.k).bytes(), 32); break;
            case 4: ec("P-384", std::get<4>(key.k).public_key().bytes(), 48); break;
            case 5: ec("P-384", std::get<5>(key.k).bytes(), 48); break;
            case 12: ec("P-521", std::get<12>(key.k).public_key().bytes(), 66); break;
            case 13: ec("P-521", std::get<13>(key.k).bytes(), 66); break;
            case 6:
            case 7: {
                // the members as jwk_key_members writes them: "n" and "e"
                std::string m = jwk_key_members(key);
                auto n_at = m.find("\"n\":");
                auto e_at = m.find(",\"e\":");
                t.put("{\"e\":");
                t.put(std::string_view(m).substr(e_at + 5));
                t.put(",\"kty\":\"RSA\",\"n\":");
                t.put(std::string_view(m).substr(n_at + 4, e_at - n_at - 4));
                t.put("}");
                break;
            }
            case 8:
            case 9:
            case 10:
            case 11: {
                t.put("{\"crv\":\"");
                t.put(key.crv());
                t.put("\",\"kty\":\"OKP\",\"x\":");
                switch (key.k.index()) {
                    case 8: b64(std::get<8>(key.k).public_key().bytes()); break;
                    case 9: b64(std::get<9>(key.k).bytes()); break;
                    case 10: b64(std::get<10>(key.k).public_key().bytes()); break;
                    default: b64(std::get<11>(key.k).bytes()); break;
                }
                t.put("}");
                break;
            }
            default: break;
        }
        auto text = t.take();
        return digest(h, text.as_slice());
    }

    // --- a JWK read --------------------------------------------------------

    SGCL_INLINE_HOT error jose_error(errc code, std::string_view what) noexcept {
        std::string m = "sgcl::crypto::jose: ";
        m += what;
        return error(code, string(m));
    }

    // The private members every kind may carry, never handed to encoding::json
    SGCL_INLINE_HOT bool jwk_private_member(std::string_view k) noexcept {
        return k == "d" || k == "p" || k == "q" || k == "dp" || k == "dq" || k == "qi" || k == "k" || k == "oth" || k == "r" || k == "t";
    }

    // What a JWK's text gives: the key in plain memory and the public
    // members as one JSON object
    struct JwkRead {
        std::unique_ptr<JoseKey> key;
        encoding::json params;
    };

    // An EC key of a curve: the point checked on the curve, d checked to
    // be its scalar
    template<class Pub, class Priv>
    expected<void, error> jwk_ec(JoseKey& key, const slice<const byte>& point, const optional<secret_bytes>& d, size_t n) noexcept {
        auto p = Pub::from_bytes(point);
        if (!p) {
            return unexpected(jose_error(errc::invalid_key, "JWK: x and y are not a point of the curve"));
        }
        if (!d) {
            key.k.template emplace<Pub>(*p);
            return {};
        }
        if (d->size() != n) {
            return unexpected(jose_error(errc::invalid_key, "JWK: d is not of the curve's size"));
        }
        auto s = Priv::from_bytes(d->as_slice());
        if (!s) {
            return unexpected(jose_error(errc::invalid_key, "JWK: d is not a scalar of the curve"));
        }
        if (!(s->public_key() == *p)) {
            return unexpected(jose_error(errc::invalid_key, "JWK: d is not the private key of x and y"));
        }
        key.k.template emplace<Priv>(std::move(*s));
        return {};
    }

    // An OKP key (RFC 8037): Ed25519's d is the seed, X25519's the scalar
    template<class Pub, class Priv>
    expected<void, error> jwk_okp(JoseKey& key, const slice<const byte>& x, const optional<secret_bytes>& d) noexcept {
        auto p = Pub::from_bytes(x);
        if (!p) {
            return unexpected(jose_error(errc::invalid_key, "JWK: x is not a public key of the curve"));
        }
        if (!d) {
            key.k.template emplace<Pub>(*p);
            return {};
        }
        expected<Priv, error> s = [&] {
            if constexpr (std::is_same_v<Priv, ed25519::private_key>) {
                return Priv::from_seed(d->as_slice());
            } else {
                return Priv::from_bytes(d->as_slice());
            }
        }();
        if (!s) {
            return unexpected(jose_error(errc::invalid_key, "JWK: d is not a private key of the curve"));
        }
        if (!(s->public_key() == *p)) {
            return unexpected(jose_error(errc::invalid_key, "JWK: d is not the private key of x"));
        }
        key.k.template emplace<Priv>(std::move(*s));
        return {};
    }

    // An RSA private key of its numbers: written as PKCS #1 DER in a writer
    // on the stack (zeroed when it goes) and read by rsa's own reader, which
    // checks that the numbers agree
    inline expected<void, error> jwk_rsa_private(JoseKey& key, const slice<const byte>& n, const slice<const byte>& e, const slice<const byte>& d,
                                                 const optional<secret_bytes> (&parts)[5]) noexcept {
        if (n.size() > 2048 || d.size() > 2048 || e.size() > 9) {
            return unexpected(jose_error(errc::unsupported, "JWK: an RSA key past 16384 bits"));
        }
        for (const auto& p : parts) {
            if (p->size() > 2048) {
                return unexpected(jose_error(errc::unsupported, "JWK: an RSA key past 16384 bits"));
            }
        }
        auto w = std::make_unique<DerWriter<2048 * 7 + 64>>();
        auto put = [&](const slice<const byte>& b) {
            static constexpr unsigned char zero = 0;
            if (b.size() == 0) {
                w->put_unsigned(&zero, 1);
            } else {
                w->put_unsigned(reinterpret_cast<const unsigned char*>(b.data()), b.size());
            }
        };
        size_t mark = w->size();
        for (int i = 4; i >= 0; --i) {
            put(parts[i]->as_slice());
        }
        put(d);
        put(e);
        put(n);
        static constexpr unsigned char version[] = {der::integer, 0x01, 0x00};
        w->put(version, sizeof version);
        w->wrap(der::sequence, mark);
        auto k = rsa::private_key::from_pkcs1_der(slice<const byte>(reinterpret_cast<const byte*>(w->data()), w->size()));
        if (!k) {
            return unexpected(jose_error(k.error().code(), std::string("JWK: ") + std::string(k.error().message().view())));
        }
        key.k.emplace<rsa::private_key>(std::move(*k));
        return {};
    }

    inline expected<JwkRead, error> read_jwk(std::string_view text) noexcept {
        std::vector<JsonPlace> places;
        if (!JsonPlaces::read(text, places)) {
            return unexpected(jose_error(errc::malformed, "JWK: not one JSON object"));
        }
        // the public members, as written, to encoding::json; the private ones kept where they lie
        std::string pub = "{";
        const JsonPlace* priv[7] = {};   // d, p, q, dp, dq, qi, k
        static constexpr std::string_view priv_names[7] = {"d", "p", "q", "dp", "dq", "qi", "k"};
        bool other_primes = false;
        for (const auto& m : places) {
            if (jwk_private_member(m.key)) {
                if (m.key == "oth" || m.key == "r" || m.key == "t") {
                    other_primes = true;
                    continue;
                }
                for (int i = 0; i < 7; ++i) {
                    if (m.key == priv_names[i]) {
                        priv[i] = &m;
                    }
                }
                continue;
            }
            if (pub.size() > 1) {
                pub += ',';
            }
            pub.append(m.key_text);
            pub += ':';
            pub.append(m.value);
        }
        pub += '}';
        auto params = encoding::json::parse(string(std::string_view(pub)));
        if (!params || !params->is_object()) {
            return unexpected(jose_error(errc::malformed, "JWK: a public member that is not JSON"));
        }
        // the members the reader needs, found in one pass over the object;
        // each that is there must be a string, key_ops a list of them
        static constexpr std::string_view known[9] = {"kty", "kid", "use", "alg", "crv", "x", "y", "n", "e"};
        const encoding::json* found[9] = {};
        for (const auto& m : params->members()) {
            const std::string_view k = m.key.view();
            for (int i = 0; i < 9; ++i) {
                if (k == known[i]) {
                    if (!m.value.is_string()) {
                        return unexpected(jose_error(errc::malformed, std::string("JWK: ") + std::string(k) + " is not a string"));
                    }
                    found[i] = &m.value;
                }
            }
            if (k == "key_ops") {
                bool ok = m.value.is_array();
                for (const auto& o : m.value.elements()) {
                    ok &= o.is_string();
                }
                if (!ok) {
                    return unexpected(jose_error(errc::malformed, "JWK: key_ops is not a list of strings"));
                }
            }
        }
        auto str = [&](int i) -> string {
            return found[i] ? found[i]->as_string(string()) : string();
        };
        const string kty_text = str(0);
        if (kty_text.empty()) {
            return unexpected(jose_error(errc::malformed, "JWK: no kty"));
        }
        const optional<string> kty = kty_text;
        // a private member: a string of base64url without escapes, into plain memory
        auto secret_of = [&](int i) -> expected<optional<secret_bytes>, error> {
            if (!priv[i]) {
                return optional<secret_bytes>();
            }
            auto inner = JsonPlaces::plain_string(priv[i]->value);
            if (!inner) {
                return unexpected(jose_error(errc::malformed, std::string("JWK: ") + std::string(priv_names[i]) + " is not a base64url string"));
            }
            auto b = b64url_secret(*inner);
            if (!b) {
                return unexpected(jose_error(errc::malformed, std::string("JWK: ") + std::string(priv_names[i]) + " is not base64url"));
            }
            return optional<secret_bytes>(std::move(*b));
        };
        auto pub_bytes = [&](const char* name) -> optional<vector<byte>> {
            for (int i = 5; i < 9; ++i) {
                if (known[i] == name) {
                    if (!found[i]) {
                        return nullopt;
                    }
                    return b64url_bytes(found[i]->as_string(string()).view());
                }
            }
            return nullopt;
        };
        JwkRead out;
        out.key = std::make_unique<JoseKey>();
        auto& key = *out.key;
        const string crv = str(4);
        if ((*kty == "EC" || *kty == "OKP") && crv.empty()) {
            return unexpected(jose_error(errc::malformed, "JWK: an EC or OKP key without crv"));
        }
        auto d = secret_of(0);
        if (!d) {
            return unexpected(d.error());
        }
        if (*kty == "oct") {
            auto k = secret_of(6);
            if (!k) {
                return unexpected(k.error());
            }
            if (!*k) {
                return unexpected(jose_error(errc::malformed, "JWK: an oct key without k"));
            }
            key.k.emplace<secret_bytes>(std::move(**k));
        } else if (*kty == "EC") {
            size_t n = crv == "P-256" ? 32 : crv == "P-384" ? 48 : crv == "P-521" ? 66 : 0;
            if (n == 0) {
                return unexpected(jose_error(errc::unsupported, "JWK: an EC key of a curve the module does not have"));
            }
            auto x = pub_bytes("x");
            auto y = pub_bytes("y");
            if (!x || !y) {
                return unexpected(jose_error(errc::malformed, "JWK: an EC key without base64url x and y"));
            }
            if (x->size() != n || y->size() != n) {
                return unexpected(jose_error(errc::invalid_key, "JWK: x and y are not of the curve's size"));
            }
            unsigned char point[1 + 2 * 66];
            point[0] = 0x04;
            sgcl::detail::copy_bytes(point + 1, x->data(), n);
            sgcl::detail::copy_bytes(point + 1 + n, y->data(), n);
            const slice<const byte> pt(reinterpret_cast<const byte*>(point), 1 + 2 * n);
            auto r = n == 32   ? jwk_ec<p256::public_key, p256::private_key>(key, pt, *d, n)
                     : n == 48 ? jwk_ec<p384::public_key, p384::private_key>(key, pt, *d, n)
                               : jwk_ec<p521::public_key, p521::private_key>(key, pt, *d, n);
            if (!r) {
                return unexpected(r.error());
            }
        } else if (*kty == "OKP") {
            const bool ed = crv == "Ed25519";
            if (!ed && crv != "X25519") {
                return unexpected(jose_error(errc::unsupported, "JWK: an OKP key of a curve the module does not have"));
            }
            auto x = pub_bytes("x");
            if (!x) {
                return unexpected(jose_error(errc::malformed, "JWK: an OKP key without base64url x"));
            }
            if (x->size() != 32 || (*d && (*d)->size() != 32)) {
                return unexpected(jose_error(errc::invalid_key, "JWK: x or d is not of 32 bytes"));
            }
            if (ed) {
                auto r = jwk_okp<ed25519::public_key, ed25519::private_key>(key, x->as_slice(), *d);
                if (!r) {
                    return unexpected(r.error());
                }
            } else {
                auto r = jwk_okp<x25519::public_key, x25519::private_key>(key, x->as_slice(), *d);
                if (!r) {
                    return unexpected(r.error());
                }
            }
        } else if (*kty == "RSA") {
            if (other_primes) {
                return unexpected(jose_error(errc::unsupported, "JWK: a multi-prime RSA key"));
            }
            auto n = pub_bytes("n");
            auto e = pub_bytes("e");
            if (!n || !e || n->empty() || e->empty()) {
                return unexpected(jose_error(errc::malformed, "JWK: an RSA key without base64url n and e"));
            }
            size_t ez = 0;
            while (ez < e->size() && (*e)[ez] == byte(0)) {
                ++ez;
            }
            if (e->size() - ez > 8) {
                return unexpected(jose_error(errc::unsupported, "JWK: an RSA exponent past 64 bits"));
            }
            uint64_t ev = 0;
            for (size_t i = ez; i < e->size(); ++i) {
                ev = ev << 8 | uint64_t((*e)[i]);
            }
            if (!*d) {
                for (int i = 1; i < 6; ++i) {
                    if (priv[i]) {
                        return unexpected(jose_error(errc::malformed, "JWK: an RSA key with primes but no d"));
                    }
                }
                auto p = rsa::public_key::from_modulus(n->as_slice(), ev);
                if (!p) {
                    return unexpected(jose_error(p.error().code(), std::string("JWK: ") + std::string(p.error().message().view())));
                }
                key.k.emplace<rsa::public_key>(std::move(*p));
            } else {
                optional<secret_bytes> parts[5];
                for (int i = 1; i < 6; ++i) {
                    auto s = secret_of(i);
                    if (!s) {
                        return unexpected(s.error());
                    }
                    if (!*s) {
                        return unexpected(jose_error(errc::unsupported, "JWK: an RSA private key without its primes (p, q, dp, dq, qi)"));
                    }
                    parts[i - 1] = std::move(**s);
                }
                auto r = jwk_rsa_private(key, n->as_slice(), e->as_slice(), (*d)->as_slice(), parts);
                if (!r) {
                    return unexpected(r.error());
                }
            }
        } else {
            return unexpected(jose_error(errc::unsupported, "JWK: a kty the module does not have"));
        }
        if (*d && (*kty == "oct")) {
            return unexpected(jose_error(errc::malformed, "JWK: an oct key with d"));
        }
        if (priv[6] && *kty != "oct") {
            return unexpected(jose_error(errc::malformed, "JWK: k in a key that is not oct"));
        }
        out.params = std::move(*params);
        return out;
    }

    // --- what a key may do -------------------------------------------------

    enum class JosePurpose : uint8_t {
        sign,
        verify,
        encrypt,
        decrypt
    };

    // Whether the key's kind and size fit the algorithm (RFC 7518: an HMAC
    // key at least as long as the digest, RSA of 2048 bits or more, the
    // curve of ES256 P-256, of ES384 P-384, of ES512 P-521), the private half asked for
    // where the purpose needs it. The reason when not
    inline const char* jose_misfit(const JoseKey& key, algorithm a, JosePurpose p) {
        const bool priv = p == JosePurpose::sign || p == JosePurpose::decrypt;
        if (priv && !key.is_private()) {
            return "a public key cannot sign or decrypt";
        }
        const size_t i = key.k.index();
        switch (a) {
            case algorithm::hs256:
            case algorithm::hs384:
            case algorithm::hs512: {
                if (i != 1) {
                    return "an HMAC algorithm takes an oct key";
                }
                return std::get<1>(key.k).size() < digest_size(jose_hash(a)) ? "an HMAC key shorter than the digest (RFC 7518 3.2)" : nullptr;
            }
            case algorithm::rs256:
            case algorithm::rs384:
            case algorithm::rs512:
            case algorithm::ps256:
            case algorithm::ps384:
            case algorithm::ps512:
            case algorithm::rsa_oaep:
            case algorithm::rsa_oaep_256:
                if (i != 6 && i != 7) {
                    return "an RSA algorithm takes an RSA key";
                }
                if (key.rsa_bits() < 2048) {
                    return "an RSA key under 2048 bits (RFC 7518 3.3, 4.3)";
                }
                return key.roca ? "an RSA key of the ROCA weakness (CVE-2017-15361)" : nullptr;
            case algorithm::es256: return i == 2 || i == 3 ? nullptr : "ES256 takes a P-256 key";
            case algorithm::es384: return i == 4 || i == 5 ? nullptr : "ES384 takes a P-384 key";
            case algorithm::es512: return i == 12 || i == 13 ? nullptr : "ES512 takes a P-521 key";
            case algorithm::eddsa: return i == 8 || i == 9 ? nullptr : "EdDSA takes an Ed25519 key";
            case algorithm::a128kw:
            case algorithm::a192kw:
            case algorithm::a256kw:
            case algorithm::a128gcmkw:
            case algorithm::a192gcmkw:
            case algorithm::a256gcmkw:
                if (i != 1) {
                    return "an AES key wrap takes an oct key";
                }
                return std::get<1>(key.k).size() != jose_wrap_key_size(a) ? "an AES key wrap key of another size than the algorithm's" : nullptr;
            case algorithm::dir: return i == 1 ? nullptr : "dir takes an oct key";
            case algorithm::ecdh_es:
            case algorithm::ecdh_es_a128kw:
            case algorithm::ecdh_es_a192kw:
            case algorithm::ecdh_es_a256kw:
                return (i >= 2 && i <= 5) || (i >= 10 && i <= 13) ? nullptr : "ECDH-ES takes a P-256, P-384, P-521 or X25519 key";
        }
        return "an algorithm of no value of its enumeration";
    }

    // The algorithm a key signs or encrypts with when none is named: its
    // own alg, else its kind's
    inline optional<algorithm> jose_default(const JoseKey& key, bool sign) noexcept {
        switch (key.k.index()) {
            case 1: {
                if (sign) {
                    return algorithm::hs256;
                }
                const size_t n = std::get<1>(key.k).size();
                return n == 16 ? algorithm::a128kw : n == 24 ? algorithm::a192kw : n == 32 ? algorithm::a256kw : algorithm::dir;
            }
            case 2:
            case 3: return sign ? algorithm::es256 : algorithm::ecdh_es;
            case 4:
            case 5: return sign ? algorithm::es384 : algorithm::ecdh_es;
            case 12:
            case 13: return sign ? algorithm::es512 : algorithm::ecdh_es;
            case 6:
            case 7: return sign ? algorithm::rs256 : algorithm::rsa_oaep_256;
            case 8:
            case 9: return sign ? optional<algorithm>(algorithm::eddsa) : nullopt;
            case 10:
            case 11: return sign ? nullopt : optional<algorithm>(algorithm::ecdh_es);
            default: return nullopt;
        }
    }

    // --- signatures ---------------------------------------------------------

    // The signature of the signing input; the key fits (jose_misfit)
    inline vector<byte> jose_sign(const JoseKey& key, algorithm a, const slice<const byte>& input) {
        auto bytes_of = [](const auto& arr) {
            return vector<byte>(arr.data(), arr.data() + arr.size());
        };
        switch (a) {
            case algorithm::hs256: return bytes_of(hmac<sha256>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::hs384: return bytes_of(hmac<sha384>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::hs512: return bytes_of(hmac<sha512>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::rs256:
            case algorithm::rs384:
            case algorithm::rs512: return std::get<6>(key.k).sign(jose_hash(a), input);
            case algorithm::ps256:
            case algorithm::ps384:
            case algorithm::ps512: return std::get<6>(key.k).sign_pss(jose_hash(a), input);
            case algorithm::es256: {
                auto d = sha256::of(input);
                return bytes_of(std::get<2>(key.k).sign_digest_raw(slice<const byte>(d.data(), d.size())));
            }
            case algorithm::es384: {
                auto d = sha384::of(input);
                return bytes_of(std::get<4>(key.k).sign_digest_raw(slice<const byte>(d.data(), d.size())));
            }
            case algorithm::es512: {
                auto d = sha512::of(input);
                return bytes_of(std::get<12>(key.k).sign_digest_raw(slice<const byte>(d.data(), d.size())));
            }
            case algorithm::eddsa: return bytes_of(std::get<8>(key.k).sign(input));
            default: throw invalid_argument("sgcl::crypto::jose: not a signature algorithm");
        }
    }

    // Whether sig is the signature of input under the key (which fits a)
    inline bool jose_verify(const JoseKey& key, algorithm a, const slice<const byte>& input, const slice<const byte>& sig) noexcept {
        auto mac = [&](auto tag) {
            return sig.size() == tag.size() && constant_time::equal(slice<const byte>(tag.data(), tag.size()), sig);
        };
        switch (a) {
            case algorithm::hs256: return mac(hmac<sha256>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::hs384: return mac(hmac<sha384>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::hs512: return mac(hmac<sha512>::of(input, std::get<1>(key.k).as_slice()));
            case algorithm::rs256:
            case algorithm::rs384:
            case algorithm::rs512:
            case algorithm::ps256:
            case algorithm::ps384:
            case algorithm::ps512: {
                optional<rsa::public_key> tmp;
                auto p = key.rsa_public(tmp);
                const bool pss = a >= algorithm::ps256;
                const hash_id h = jose_hash(a);
                return visit_hash(h, [&](auto t) {
                    auto d = decltype(t)::type::of(input);
                    const slice<const byte> ds(d.data(), d.size());
                    // PSS: the salt as long as the digest (RFC 7518 3.5)
                    return pss ? p->verify_digest_pss(h, ds, sig, d.size()) : p->verify_digest(h, ds, sig);
                });
            }
            case algorithm::es256: {
                auto d = sha256::of(input);
                const slice<const byte> ds(d.data(), d.size());
                return key.k.index() == 2 ? std::get<2>(key.k).public_key().verify_digest_raw(ds, sig) : std::get<3>(key.k).verify_digest_raw(ds, sig);
            }
            case algorithm::es384: {
                auto d = sha384::of(input);
                const slice<const byte> ds(d.data(), d.size());
                return key.k.index() == 4 ? std::get<4>(key.k).public_key().verify_digest_raw(ds, sig) : std::get<5>(key.k).verify_digest_raw(ds, sig);
            }
            case algorithm::es512: {
                auto d = sha512::of(input);
                const slice<const byte> ds(d.data(), d.size());
                return key.k.index() == 12 ? std::get<12>(key.k).public_key().verify_digest_raw(ds, sig) : std::get<13>(key.k).verify_digest_raw(ds, sig);
            }
            case algorithm::eddsa:
                return key.k.index() == 8 ? std::get<8>(key.k).public_key().verify(input, sig) : std::get<9>(key.k).verify(input, sig);
            default: return false;
        }
    }
}
