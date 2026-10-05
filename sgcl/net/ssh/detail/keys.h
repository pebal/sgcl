//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "wire.h"
#include "../../../crypto/ctr.h"
#include "../../../crypto/ed25519.h"
#include "../../../crypto/gcm.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/p384.h"
#include "../../../crypto/random.h"
#include "../../../crypto/rsa.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../crypto/detail/bcrypt_pbkdf.h"
#include "../../../crypto/detail/der.h"
#include "../../../crypto/detail/key_pem.h"
#include "../../../crypto/detail/rsa_math.h"
#include "../../../encoding/base64.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// The keys of SSH: the public key blobs of RFC 4253 §6.6 (ssh-ed25519 of
// RFC 8709, ecdsa-sha2-nistp256 and -nistp384 of RFC 5656, ssh-rsa with
// the signatures rsa-sha2-256 and rsa-sha2-512 of RFC 8332) and OpenSSH's
// certificates over them (PROTOCOL.certkeys); signatures made and checked;
// and the private key file of OpenSSH (PROTOCOL.key: "openssh-key-v1",
// unencrypted or under bcrypt_pbkdf with AES-CTR or AES-GCM), read and
// written, with the PEM forms crypto reads (PKCS #8, SEC 1, PKCS #1).
//
// An "ssh-rsa" signature (SHA-1) is never made nor taken. RSA keys of 1024
// bits and more are read, as crypto reads them (OpenSSH's own floor).
namespace sgcl::net::ssh::detail {
    enum class KeyKind : uint8_t {
        ed25519,
        ecdsa_p256,
        ecdsa_p384,
        rsa,
    };

    SGCL_INLINE_HOT std::string_view key_name(KeyKind k) noexcept {
        switch (k) {
            case KeyKind::ed25519: return "ssh-ed25519";
            case KeyKind::ecdsa_p256: return "ecdsa-sha2-nistp256";
            case KeyKind::ecdsa_p384: return "ecdsa-sha2-nistp384";
            case KeyKind::rsa: return "ssh-rsa";
        }
        return "";
    }

    SGCL_INLINE_HOT std::string_view cert_name(KeyKind k) noexcept {
        switch (k) {
            case KeyKind::ed25519: return "ssh-ed25519-cert-v01@openssh.com";
            case KeyKind::ecdsa_p256: return "ecdsa-sha2-nistp256-cert-v01@openssh.com";
            case KeyKind::ecdsa_p384: return "ecdsa-sha2-nistp384-cert-v01@openssh.com";
            case KeyKind::rsa: return "ssh-rsa-cert-v01@openssh.com";
        }
        return "";
    }

    SGCL_INLINE_HOT std::string_view curve_name(KeyKind k) noexcept {
        return k == KeyKind::ecdsa_p256 ? "nistp256" : "nistp384";
    }

    // The kind of a key type's name, plain or certificate
    inline bool kind_of_name(std::string_view name, KeyKind& kind, bool& cert) noexcept {
        for (KeyKind k : {KeyKind::ed25519, KeyKind::ecdsa_p256, KeyKind::ecdsa_p384, KeyKind::rsa}) {
            if (name == key_name(k)) {
                kind = k;
                cert = false;
                return true;
            }
            if (name == cert_name(k)) {
                kind = k;
                cert = true;
                return true;
            }
        }
        return false;
    }

    // The algorithms of a signature or of a host key: the name negotiated,
    // the key's kind, whether it is a certificate, the name inside the
    // signature
    struct AlgInfo {
        std::string_view name;
        KeyKind kind;
        bool cert;
        std::string_view signature;
    };

    // In the module's order of preference
    inline constexpr AlgInfo alg_table[] = {
        {"ssh-ed25519-cert-v01@openssh.com", KeyKind::ed25519, true, "ssh-ed25519"},
        {"ecdsa-sha2-nistp256-cert-v01@openssh.com", KeyKind::ecdsa_p256, true, "ecdsa-sha2-nistp256"},
        {"ecdsa-sha2-nistp384-cert-v01@openssh.com", KeyKind::ecdsa_p384, true, "ecdsa-sha2-nistp384"},
        {"rsa-sha2-512-cert-v01@openssh.com", KeyKind::rsa, true, "rsa-sha2-512"},
        {"rsa-sha2-256-cert-v01@openssh.com", KeyKind::rsa, true, "rsa-sha2-256"},
        {"ssh-ed25519", KeyKind::ed25519, false, "ssh-ed25519"},
        {"ecdsa-sha2-nistp256", KeyKind::ecdsa_p256, false, "ecdsa-sha2-nistp256"},
        {"ecdsa-sha2-nistp384", KeyKind::ecdsa_p384, false, "ecdsa-sha2-nistp384"},
        {"rsa-sha2-512", KeyKind::rsa, false, "rsa-sha2-512"},
        {"rsa-sha2-256", KeyKind::rsa, false, "rsa-sha2-256"},
    };

    inline const AlgInfo* find_alg(std::string_view name) noexcept {
        for (const auto& a : alg_table) {
            if (a.name == name) {
                return &a;
            }
        }
        return nullptr;
    }

    // The public material of a key, as views into its blob
    struct KeyMaterial {
        KeyKind kind = KeyKind::ed25519;
        Span ed;      // Ed25519: 32 bytes
        Span point;   // ECDSA: Q
        Span e, n;    // RSA
    };

    // A certificate's fields (PROTOCOL.certkeys), as views into its blob
    struct CertFields {
        Span nonce;
        uint64_t serial = 0;
        uint32_t type = 0;            // 1 user, 2 host
        Span key_id;
        Span principals;              // strings in a string
        uint64_t valid_after = 0;
        uint64_t valid_before = 0;
        Span critical_options;
        Span extensions;
        Span signature_key;
        Span signature;
        size_t signed_size = 0;       // the blob's bytes the signature covers
    };

    struct ParsedKey {
        KeyMaterial key;
        bool cert = false;
        CertFields c;
    };

    // The fields after a plain key's name
    inline bool read_material(Reader& r, KeyKind kind, KeyMaterial& m) noexcept {
        m.kind = kind;
        switch (kind) {
            case KeyKind::ed25519:
                m.ed = r.string();
                return r.ok() && m.ed.n == 32;
            case KeyKind::ecdsa_p256:
            case KeyKind::ecdsa_p384: {
                Span curve = r.string();
                m.point = r.string();
                if (!r.ok() || curve.view() != curve_name(kind)) {
                    return false;
                }
                if (kind == KeyKind::ecdsa_p256) {
                    return m.point.n == 65 && crypto::p256::public_key::from_bytes(m.point.bytes()).has_value();
                }
                return m.point.n == 97 && crypto::p384::public_key::from_bytes(m.point.bytes()).has_value();
            }
            case KeyKind::rsa:
                m.e = r.mpint();
                m.n = r.mpint();
                return r.ok() && m.e.n > 0 && m.e.n <= 8 && m.n.n >= 128 && m.n.n <= 2048;
        }
        return false;
    }

    // A key's blob taken apart, a certificate's fields with it; the
    // certificate's signature is not checked here (cert_signature_ok)
    inline bool parse_key(const uint8_t* p, size_t n, ParsedKey& out) noexcept {
        Reader r(p, n);
        Span name = r.string();
        KeyKind kind;
        bool cert;
        if (!r.ok() || !kind_of_name(name.view(), kind, cert)) {
            return false;
        }
        out.cert = cert;
        if (!cert) {
            return read_material(r, kind, out.key) && r.done();
        }
        out.c.nonce = r.string();
        if (!read_material(r, kind, out.key)) {
            return false;
        }
        out.c.serial = r.u64();
        out.c.type = r.u32();
        out.c.key_id = r.string();
        out.c.principals = r.string();
        out.c.valid_after = r.u64();
        out.c.valid_before = r.u64();
        out.c.critical_options = r.string();
        out.c.extensions = r.string();
        (void)r.string();   // reserved
        out.c.signature_key = r.string();
        out.c.signed_size = size_t(r.at() - p);
        out.c.signature = r.string();
        return r.done() && (out.c.type == 1 || out.c.type == 2);
    }

    // The plain blob of a key's material (a certificate's key without the
    // certificate)
    inline void write_plain(Writer& w, const KeyMaterial& m) {
        w.string(key_name(m.kind));
        switch (m.kind) {
            case KeyKind::ed25519:
                w.string(m.ed);
                break;
            case KeyKind::ecdsa_p256:
            case KeyKind::ecdsa_p384:
                w.string(curve_name(m.kind));
                w.string(m.point);
                break;
            case KeyKind::rsa:
                w.mpint(m.e.p, m.e.n);
                w.mpint(m.n.p, m.n.n);
                break;
        }
    }

    // The fixed width of a number's bytes, zeros in front
    inline void left_pad(uint8_t* out, size_t width, const Span& v) noexcept {
        size_t n = v.n > width ? width : v.n;
        for (size_t i = 0; i < width - n; ++i) {
            out[i] = 0;
        }
        sgcl::detail::copy_bytes(out + width - n, v.p + v.n - n, n);
    }

    // Whether sig_blob (string name, string signature) is a signature of
    // data under the key by the algorithm `alg` ("ssh-ed25519",
    // "ecdsa-sha2-nistp256", "rsa-sha2-512" …: the signature's own name,
    // never "ssh-rsa")
    inline bool verify(const KeyMaterial& k, std::string_view alg, const uint8_t* data, size_t size, const uint8_t* sig_blob, size_t sig_size) noexcept {
        Reader r(sig_blob, sig_size);
        Span name = r.string();
        Span sig = r.string();
        if (!r.done() || name.view() != alg) {
            return false;
        }
        const auto message = bytes_of(data, size);
        switch (k.kind) {
            case KeyKind::ed25519: {
                if (alg != "ssh-ed25519") {
                    return false;
                }
                auto pk = crypto::ed25519::public_key::from_bytes(k.ed.bytes());
                return pk && pk->verify(message, sig.bytes());
            }
            case KeyKind::ecdsa_p256:
            case KeyKind::ecdsa_p384: {
                const bool p256 = k.kind == KeyKind::ecdsa_p256;
                if (alg != (p256 ? "ecdsa-sha2-nistp256" : "ecdsa-sha2-nistp384")) {
                    return false;
                }
                Reader sr(sig);
                Span rr = sr.mpint();
                Span ss = sr.mpint();
                const size_t w = p256 ? 32 : 48;
                if (!sr.done() || rr.n > w || ss.n > w) {
                    return false;
                }
                uint8_t raw[96];
                left_pad(raw, w, rr);
                left_pad(raw + w, w, ss);
                if (p256) {
                    auto pk = crypto::p256::public_key::from_bytes(k.point.bytes());
                    auto d = crypto::sha256::of(message);
                    return pk && pk->verify_digest_raw(slice<const byte>(d.data(), d.size()), bytes_of(raw, 2 * w));
                }
                auto pk = crypto::p384::public_key::from_bytes(k.point.bytes());
                auto d = crypto::sha384::of(message);
                return pk && pk->verify_digest_raw(slice<const byte>(d.data(), d.size()), bytes_of(raw, 2 * w));
            }
            case KeyKind::rsa: {
                crypto::hash_id h;
                if (alg == "rsa-sha2-256") {
                    h = crypto::hash_id::sha256;
                } else if (alg == "rsa-sha2-512") {
                    h = crypto::hash_id::sha512;
                } else {
                    return false;   // "ssh-rsa": SHA-1, refused
                }
                uint64_t e = 0;
                for (size_t i = 0; i < k.e.n; ++i) {
                    e = e << 8 | k.e.p[i];
                }
                auto pk = crypto::rsa::public_key::from_modulus(k.n.bytes(), e);
                if (!pk) {
                    return false;
                }
                const size_t size = pk->size();
                if (sig.n > size) {
                    return false;
                }
                // a signature shorter than the modulus: its leading zeros
                // left out by some signers, put back (RFC 8332 asks for the
                // full length; OpenSSH and Go take the short form)
                std::vector<uint8_t> full(size);
                left_pad(full.data(), size, sig);
                return pk->verify(h, message, bytes_of(full.data(), size));
            }
        }
        return false;
    }

    // Whether a certificate's signature, by the key in its signature_key
    // (a plain key, never a certificate), covers its blob
    inline bool cert_signature_ok(const uint8_t* blob, const ParsedKey& pk) noexcept {
        if (!pk.cert) {
            return false;
        }
        ParsedKey ca;
        if (!parse_key(pk.c.signature_key.p, pk.c.signature_key.n, ca) || ca.cert) {
            return false;
        }
        Reader sr(pk.c.signature);
        Span alg = sr.string();
        if (!sr.ok()) {
            return false;
        }
        return verify(ca.key, alg.view(), blob, pk.c.signed_size, pk.c.signature.p, pk.c.signature.n);
    }

    // The strings of a string (a certificate's principals)
    inline std::vector<std::string_view> strings_of(const Span& s) noexcept {
        std::vector<std::string_view> out;
        Reader r(s);
        while (r.ok() && r.left()) {
            Span x = r.string();
            if (r.ok()) {
                out.push_back(x.view());
            }
        }
        return out;
    }

    // The name and value pairs of a certificate's options or extensions:
    // the value a string in the data's string (empty data: none)
    inline bool options_of(const Span& s, std::vector<std::pair<std::string_view, std::string_view>>& out) noexcept {
        Reader r(s);
        while (r.ok() && r.left()) {
            Span name = r.string();
            Span data = r.string();
            if (!r.ok()) {
                return false;
            }
            std::string_view value;
            if (data.n) {
                Reader vr(data);
                Span v = vr.string();
                value = vr.done() ? v.view() : data.view();
            }
            out.emplace_back(name.view(), value);
        }
        return r.ok();
    }

    // "SHA256:" and the unpadded base64 of the blob's SHA-256, as OpenSSH
    // prints a fingerprint
    inline std::string fingerprint_of(const uint8_t* blob, size_t n) {
        auto d = crypto::sha256::of(bytes_of(blob, n));
        auto t = encoding::base64::raw_standard.encode(slice<const byte>(d.data(), d.size()));
        return "SHA256:" + std::string(t.view());
    }

    // --- the private side ---------------------------------------------------

    // A private key in unmanaged memory, with its public blob and comment
    struct KeyPair {
        KeyKind kind = KeyKind::ed25519;
        optional<crypto::ed25519::private_key> ed25519;
        optional<crypto::p256::private_key> p256;
        optional<crypto::p384::private_key> p384;
        optional<crypto::rsa::private_key> rsa;
        Bytes public_blob;
        std::string comment;

        // The public blob made from the key
        void make_public() {
            public_blob.clear();
            Writer w(public_blob);
            w.string(key_name(kind));
            switch (kind) {
                case KeyKind::ed25519: {
                    auto b = ed25519->public_key().bytes();
                    w.string(b.data(), b.size());
                    break;
                }
                case KeyKind::ecdsa_p256: {
                    auto b = p256->public_key().bytes();
                    w.string(curve_name(kind));
                    w.string(b.data(), b.size());
                    break;
                }
                case KeyKind::ecdsa_p384: {
                    auto b = p384->public_key().bytes();
                    w.string(curve_name(kind));
                    w.string(b.data(), b.size());
                    break;
                }
                case KeyKind::rsa: {
                    auto pub = rsa->public_key();
                    uint64_t e = pub.exponent();
                    uint8_t eb[8];
                    store64(eb, e);
                    w.mpint(eb, 8);
                    auto m = pub.modulus();
                    w.mpint(reinterpret_cast<const uint8_t*>(m.data()), m.size());
                    break;
                }
            }
        }

        // The signature algorithms of the key, the default first
        std::string_view default_alg() const noexcept {
            switch (kind) {
                case KeyKind::ed25519: return "ssh-ed25519";
                case KeyKind::ecdsa_p256: return "ecdsa-sha2-nistp256";
                case KeyKind::ecdsa_p384: return "ecdsa-sha2-nistp384";
                case KeyKind::rsa: return "rsa-sha2-512";
            }
            return "";
        }

        bool can_sign(std::string_view alg) const noexcept {
            if (kind == KeyKind::rsa) {
                return alg == "rsa-sha2-256" || alg == "rsa-sha2-512";
            }
            return alg == default_alg();
        }

        // The signature blob of data by alg (can_sign(alg) holds)
        Bytes sign(std::string_view alg, const uint8_t* data, size_t n) const {
            Bytes out;
            Writer w(out);
            w.string(alg);
            const auto message = bytes_of(data, n);
            switch (kind) {
                case KeyKind::ed25519: {
                    auto s = ed25519->sign(message);
                    w.string(s.data(), s.size());
                    break;
                }
                case KeyKind::ecdsa_p256:
                case KeyKind::ecdsa_p384: {
                    const size_t width = kind == KeyKind::ecdsa_p256 ? 32 : 48;
                    uint8_t raw[96];
                    if (kind == KeyKind::ecdsa_p256) {
                        auto d = crypto::sha256::of(message);
                        auto s = p256->sign_digest_raw(slice<const byte>(d.data(), d.size()));
                        sgcl::detail::copy_bytes(raw, s.data(), 64);
                    } else {
                        auto d = crypto::sha384::of(message);
                        auto s = p384->sign_digest_raw(slice<const byte>(d.data(), d.size()));
                        sgcl::detail::copy_bytes(raw, s.data(), 96);
                    }
                    size_t at = w.begin_string();
                    w.mpint(raw, width);
                    w.mpint(raw + width, width);
                    w.end_string(at);
                    break;
                }
                case KeyKind::rsa: {
                    auto s = rsa->sign(alg == "rsa-sha2-256" ? crypto::hash_id::sha256 : crypto::hash_id::sha512, message);
                    w.string(s.data(), s.size());
                    break;
                }
            }
            return out;
        }
    };

    // --- DER of PKCS #1 for an RSA key from OpenSSH's parts ------------------

    inline void der_length(Bytes& out, size_t n) {
        if (n < 0x80) {
            out.push_back(uint8_t(n));
            return;
        }
        uint8_t tmp[8];
        int k = 0;
        while (n) {
            tmp[k++] = uint8_t(n);
            n >>= 8;
        }
        out.push_back(uint8_t(0x80 | k));
        while (k) {
            out.push_back(tmp[--k]);
        }
    }

    inline void der_integer(Bytes& out, const uint8_t* p, size_t n) {
        while (n > 1 && *p == 0) {
            ++p;
            --n;
        }
        out.push_back(0x02);
        const bool pad = n == 0 || (p[0] & 0x80);
        der_length(out, n + (pad ? 1 : 0));
        if (pad) {
            out.push_back(0);
        }
        out.insert(out.end(), p, p + n);
    }

    // d mod (prime - 1) through crypto's constant-time arithmetic (the
    // reduction it checks keys with): the CRT exponents OpenSSH's format
    // leaves out
    inline void crt_exponent(const Span& d, const Span& prime, SecretBuffer& out) {
        using namespace crypto::detail::bn;
        const size_t kd = words_for_bytes(d.n), k = words_for_bytes(prime.n);
        crypto::detail::bn::SecretWords<OperatorDelete> s(kd + 4 * k);
        word* dw = s.data();
        word* m = dw + kd;
        word* r = m + k;
        word* t = r + k;
        from_be(dw, kd, d.p, d.n);
        from_be(m, k, prime.p, prime.n);
        m[0] ^= 1;   // prime - 1: the prime is odd
        reduce(r, dw, kd, m, k, t);
        out.b.assign(prime.n, 0);
        to_be(out.b.data(), prime.n, r, k);
    }

    // An RSA private key from n, e, d, iqmp, p, q: its PKCS #1 DER built
    // in unmanaged memory (zeroed) and read by crypto, which checks the
    // parts agree
    inline optional<crypto::rsa::private_key> rsa_from_parts(const Span& n, const Span& e, const Span& d, const Span& iqmp, const Span& p, const Span& q) {
        if (n.n == 0 || e.n == 0 || d.n == 0 || p.n == 0 || q.n == 0 || iqmp.n == 0 || !(p.p[p.n - 1] & 1) || !(q.p[q.n - 1] & 1)) {
            return nullopt;
        }
        SecretBuffer dp, dq, body, der;
        crt_exponent(d, p, dp);
        crt_exponent(d, q, dq);
        uint8_t zero = 0;
        der_integer(body.b, &zero, 1);
        der_integer(body.b, n.p, n.n);
        der_integer(body.b, e.p, e.n);
        der_integer(body.b, d.p, d.n);
        der_integer(body.b, p.p, p.n);
        der_integer(body.b, q.p, q.n);
        der_integer(body.b, dp.b.data(), dp.b.size());
        der_integer(body.b, dq.b.data(), dq.b.size());
        der_integer(body.b, iqmp.p, iqmp.n);
        der.b.push_back(0x30);
        der_length(der.b, body.b.size());
        der.b.insert(der.b.end(), body.b.begin(), body.b.end());
        auto k = crypto::rsa::private_key::from_pkcs1_der(bytes_of(der.b.data(), der.b.size()));
        if (!k) {
            return nullopt;
        }
        return std::move(*k);
    }

    // The parts of an RSA key as OpenSSH writes them: n, e, d, iqmp, p, q
    // (from crypto's PKCS #1 DER, read in unmanaged memory)
    inline bool rsa_parts(const crypto::rsa::private_key& k, Writer& w) {
        auto der = k.to_pkcs1_der();
        auto s = der.as_slice();
        crypto::detail::DerReader in(reinterpret_cast<const unsigned char*>(s.data()), s.size());
        crypto::detail::DerReader seq;
        if (!in.read(crypto::detail::der::sequence, seq)) {
            return false;
        }
        const unsigned char* p[9];
        size_t n[9];
        for (int i = 0; i < 9; ++i) {
            if (!seq.read_unsigned_bytes(p[i], n[i])) {
                return false;
            }
        }
        // version, n, e, d, p, q, dP, dQ, qInv
        w.mpint(p[1], n[1]);
        w.mpint(p[2], n[2]);
        w.mpint(p[3], n[3]);
        w.mpint(p[8], n[8]);
        w.mpint(p[4], n[4]);
        w.mpint(p[5], n[5]);
        return true;
    }

    // --- OpenSSH's private key file -------------------------------------------

    inline constexpr std::string_view OpensshBegin = "-----BEGIN OPENSSH PRIVATE KEY-----";
    inline constexpr std::string_view OpensshEnd = "-----END OPENSSH PRIVATE KEY-----";
    inline constexpr char OpensshMagic[] = "openssh-key-v1";   // with its NUL: 15 bytes

    // Why a key file could not be read
    enum class KeyFileError : uint8_t {
        none,
        malformed,     // not a key of a form read here
        unsupported,   // a cipher, KDF or key type not read here
        passphrase,    // encrypted, and no passphrase or a wrong one
    };

    // The private section's key of a type (the fields after the type's
    // name), and its comment
    inline bool read_private(Reader& r, KeyPair& k) {
        Span type = r.string();
        KeyKind kind;
        bool cert;
        if (!r.ok() || !kind_of_name(type.view(), kind, cert) || cert) {
            return false;
        }
        k.kind = kind;
        switch (kind) {
            case KeyKind::ed25519: {
                Span pk = r.string();
                Span sk = r.string();
                if (!r.ok() || pk.n != 32 || sk.n != 64) {
                    return false;
                }
                auto key = crypto::ed25519::private_key::from_seed(bytes_of(sk.p, 32));
                if (!key || std::memcmp(key->public_key().bytes().data(), pk.p, 32) != 0 || std::memcmp(sk.p + 32, pk.p, 32) != 0) {
                    return false;
                }
                k.ed25519.emplace(std::move(*key));
                break;
            }
            case KeyKind::ecdsa_p256:
            case KeyKind::ecdsa_p384: {
                Span curve = r.string();
                Span q = r.string();
                Span d = r.mpint();
                const size_t w = kind == KeyKind::ecdsa_p256 ? 32 : 48;
                if (!r.ok() || curve.view() != curve_name(kind) || d.n > w) {
                    return false;
                }
                uint8_t scalar[48];
                left_pad(scalar, w, d);
                bool ok = false;
                if (kind == KeyKind::ecdsa_p256) {
                    auto key = crypto::p256::private_key::from_bytes(bytes_of(scalar, w));
                    if (key && q.n == 65 && std::memcmp(key->public_key().bytes().data(), q.p, 65) == 0) {
                        k.p256.emplace(std::move(*key));
                        ok = true;
                    }
                } else {
                    auto key = crypto::p384::private_key::from_bytes(bytes_of(scalar, w));
                    if (key && q.n == 97 && std::memcmp(key->public_key().bytes().data(), q.p, 97) == 0) {
                        k.p384.emplace(std::move(*key));
                        ok = true;
                    }
                }
                crypto::detail::secure_zero(scalar, sizeof scalar);
                if (!ok) {
                    return false;
                }
                break;
            }
            case KeyKind::rsa: {
                Span n = r.mpint(), e = r.mpint(), d = r.mpint(), iqmp = r.mpint(), p = r.mpint(), q = r.mpint();
                if (!r.ok()) {
                    return false;
                }
                auto key = rsa_from_parts(n, e, d, iqmp, p, q);
                if (!key) {
                    return false;
                }
                k.rsa.emplace(std::move(*key));
                break;
            }
        }
        Span comment = r.string();
        if (!r.ok()) {
            return false;
        }
        k.comment.assign(comment.view());
        k.make_public();
        return true;
    }

    // The key and IV bytes of the private section's ciphers
    struct FileCipher {
        std::string_view name;
        size_t key_size;
        size_t iv_size;
        bool gcm;
    };

    inline constexpr FileCipher file_ciphers[] = {
        {"aes256-ctr", 32, 16, false},
        {"aes192-ctr", 24, 16, false},
        {"aes128-ctr", 16, 16, false},
        {"aes256-gcm@openssh.com", 32, 12, true},
        {"aes128-gcm@openssh.com", 16, 12, true},
    };

    // The base64 between OpenSSH's lines, decoded into a secret buffer;
    // false when the text has none
    inline bool openssh_body(std::string_view v, SecretBuffer& out, bool& found) {
        found = false;
        size_t b = v.find(OpensshBegin);
        if (b == std::string_view::npos) {
            return false;
        }
        found = true;
        size_t from = b + OpensshBegin.size();
        size_t e = v.find(OpensshEnd, from);
        if (e == std::string_view::npos) {
            return false;
        }
        std::string_view body = v.substr(from, e - from);
        // spaces and line ends between the base64's lines
        std::string clean;
        clean.reserve(body.size());
        for (char c : body) {
            if (c != '\n' && c != '\r' && c != ' ' && c != '\t') {
                clean.push_back(c);
            }
        }
        const auto& codec = encoding::base64::standard;
        out.b.assign(codec.max_decoded_size(clean.size()), 0);
        auto n = codec.decode_to(mutable_bytes_of(out.b.data(), out.b.size()), slice<const char>(clean.data(), clean.size()));
        crypto::detail::secure_zero(clean.data(), clean.size());
        if (!n) {
            return false;
        }
        out.b.resize(*n);
        return true;
    }

    // A key from OpenSSH's file format
    inline KeyFileError read_openssh(std::string_view text, std::string_view passphrase, KeyPair& k) {
        SecretBuffer raw;
        bool found;
        if (!openssh_body(text, raw, found)) {
            return KeyFileError::malformed;
        }
        const Bytes& b = raw.b;
        if (b.size() < sizeof OpensshMagic || std::memcmp(b.data(), OpensshMagic, sizeof OpensshMagic) != 0) {
            return KeyFileError::malformed;
        }
        Reader r(b.data() + sizeof OpensshMagic, b.size() - sizeof OpensshMagic);
        Span cipher = r.string();
        Span kdf = r.string();
        Span kdf_options = r.string();
        uint32_t count = r.u32();
        if (!r.ok()) {
            return KeyFileError::malformed;
        }
        if (count != 1) {
            return KeyFileError::unsupported;
        }
        Span pub = r.string();
        Span sealed = r.string();
        if (!r.ok()) {
            return KeyFileError::malformed;
        }
        SecretBuffer clear;
        if (cipher.view() == "none") {
            if (kdf.view() != "none" || !r.done()) {
                return KeyFileError::malformed;
            }
            clear.b.assign(sealed.p, sealed.p + sealed.n);
        } else {
            const FileCipher* fc = nullptr;
            for (const auto& c : file_ciphers) {
                if (c.name == cipher.view()) {
                    fc = &c;
                }
            }
            if (!fc || kdf.view() != "bcrypt") {
                return KeyFileError::unsupported;
            }
            Reader kr(kdf_options);
            Span salt = kr.string();
            uint32_t rounds = kr.u32();
            if (!kr.done() || salt.n == 0 || rounds == 0) {
                return KeyFileError::malformed;
            }
            Span tag;
            if (fc->gcm) {
                tag = r.raw(16);
            }
            if (!r.done() || (sealed.n % 16) != 0) {
                return KeyFileError::malformed;
            }
            if (passphrase.empty()) {
                return KeyFileError::passphrase;
            }
            if (rounds > 1u << 16) {   // a key file that would take hours: refused
                return KeyFileError::unsupported;
            }
            uint8_t kiv[48];
            if (!crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>(passphrase.data()), passphrase.size(), salt.p, salt.n, rounds, kiv,
                                              fc->key_size + fc->iv_size)) {
                return KeyFileError::malformed;
            }
            clear.b.assign(sealed.n, 0);
            bool ok = true;
            if (fc->gcm) {
                crypto::aes_gcm g(bytes_of(kiv, fc->key_size));
                Bytes joined(sealed.p, sealed.p + sealed.n);
                joined.insert(joined.end(), tag.p, tag.p + 16);
                auto o = g.open_to(mutable_bytes_of(clear.b.data(), clear.b.size()), bytes_of(kiv + fc->key_size, 12), bytes_of(joined.data(), joined.size()));
                ok = o.has_value();
            } else {
                crypto::aes_ctr c(bytes_of(kiv, fc->key_size), bytes_of(kiv + fc->key_size, 16));
                c.xor_key_stream(mutable_bytes_of(clear.b.data(), clear.b.size()), bytes_of(sealed.p, sealed.n));
            }
            crypto::detail::secure_zero(kiv, sizeof kiv);
            if (!ok) {
                return KeyFileError::passphrase;
            }
        }
        Reader pr(clear.b.data(), clear.b.size());
        uint32_t check1 = pr.u32();
        uint32_t check2 = pr.u32();
        if (!pr.ok()) {
            return KeyFileError::malformed;
        }
        if (check1 != check2) {
            return cipher.view() == "none" ? KeyFileError::malformed : KeyFileError::passphrase;
        }
        if (!read_private(pr, k)) {
            return KeyFileError::malformed;
        }
        // the padding: 1, 2, 3 … to the block's end
        size_t pad = pr.left();
        Span tail = pr.raw(pad);
        for (size_t i = 0; i < pad; ++i) {
            if (tail.p[i] != uint8_t(i + 1)) {
                return KeyFileError::malformed;
            }
        }
        if (pub.n != k.public_blob.size() || std::memcmp(pub.p, k.public_blob.data(), pub.n) != 0) {
            return KeyFileError::malformed;
        }
        return KeyFileError::none;
    }

    // A private key's type and fields as OpenSSH's private section and the
    // agent protocol's add request write them (without the comment)
    inline void write_key_fields(Writer& w, const KeyPair& k) {
        w.string(key_name(k.kind));
        switch (k.kind) {
            case KeyKind::ed25519: {
                auto pk = k.ed25519->public_key().bytes();
                auto seed = k.ed25519->seed();
                w.string(pk.data(), pk.size());
                size_t at = w.begin_string();
                w.raw(seed.bytes().data(), 32);
                w.raw(pk.data(), 32);
                w.end_string(at);
                break;
            }
            case KeyKind::ecdsa_p256: {
                auto q = k.p256->public_key().bytes();
                auto d = k.p256->bytes();
                w.string(curve_name(k.kind));
                w.string(q.data(), q.size());
                w.mpint(reinterpret_cast<const uint8_t*>(d.bytes().data()), d.bytes().size());
                break;
            }
            case KeyKind::ecdsa_p384: {
                auto q = k.p384->public_key().bytes();
                auto d = k.p384->bytes();
                w.string(curve_name(k.kind));
                w.string(q.data(), q.size());
                w.mpint(reinterpret_cast<const uint8_t*>(d.bytes().data()), d.bytes().size());
                break;
            }
            case KeyKind::rsa:
                rsa_parts(*k.rsa, w);
                break;
        }
    }

    // The private section of a key: the check words, the key, its comment
    inline void write_private(Writer& w, const KeyPair& k, uint32_t check) {
        w.u32(check);
        w.u32(check);
        write_key_fields(w, k);
        w.string(k.comment);
    }

    // The key in OpenSSH's file format, lines of 70 characters as
    // ssh-keygen writes them: unencrypted, or under bcrypt_pbkdf (16
    // rounds, a 16-byte salt) with aes256-ctr, ssh-keygen's default
    inline void write_openssh(const KeyPair& k, std::string_view passphrase, SecretBuffer& text, uint32_t rounds = 16) {
        SecretBuffer priv, blob;
        uint32_t check;
        crypto::random::fill(mutable_bytes_of(&check, 4));
        {
            Writer w(priv.b);
            write_private(w, k, check);
            const size_t block = passphrase.empty() ? 8 : 16;
            for (uint8_t i = 1; priv.b.size() % block != 0; ++i) {
                w.u8(i);
            }
        }
        Writer w(blob.b);
        w.raw(OpensshMagic, sizeof OpensshMagic);
        if (passphrase.empty()) {
            w.string("none");
            w.string("none");
            w.string("");
        } else {
            uint8_t salt[16];
            crypto::random::fill(mutable_bytes_of(salt, 16));
            w.string("aes256-ctr");
            w.string("bcrypt");
            size_t at = w.begin_string();
            w.string(salt, 16);
            w.u32(rounds);
            w.end_string(at);
            uint8_t kiv[48];
            crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>(passphrase.data()), passphrase.size(), salt, 16, rounds, kiv, 48);
            crypto::aes_ctr c(bytes_of(kiv, 32), bytes_of(kiv + 32, 16));
            c.xor_key_stream(mutable_bytes_of(priv.b.data(), priv.b.size()), bytes_of(priv.b.data(), priv.b.size()));
            crypto::detail::secure_zero(kiv, sizeof kiv);
        }
        w.u32(1);
        w.string(k.public_blob);
        w.string(priv.b);
        const auto& codec = encoding::base64::standard;
        const size_t chars = codec.encoded_size(blob.b.size());
        SecretBuffer b64;
        b64.b.assign(chars, 0);
        codec.encode_to(slice<char>(reinterpret_cast<char*>(b64.b.data()), chars), bytes_of(blob.b.data(), blob.b.size()));
        text.b.clear();
        text.b.reserve(chars + chars / 70 + 80);
        text.b.insert(text.b.end(), OpensshBegin.begin(), OpensshBegin.end());
        text.b.push_back('\n');
        for (size_t i = 0; i < chars; i += 70) {
            size_t n = chars - i < 70 ? chars - i : 70;
            text.b.insert(text.b.end(), b64.b.begin() + ptrdiff_t(i), b64.b.begin() + ptrdiff_t(i + n));
            text.b.push_back('\n');
        }
        text.b.insert(text.b.end(), OpensshEnd.begin(), OpensshEnd.end());
        text.b.push_back('\n');
    }

    // A key of PEM's forms (PKCS #8, SEC 1, PKCS #1), its DER in a
    // secret_bytes; false for none of the kinds SSH signs with
    inline KeyFileError read_pem(const slice<const byte>& text, KeyPair& k) {
        auto block = crypto::detail::read_key_pem(text);
        if (!block) {
            return block.error().code() == crypto::errc::unsupported ? KeyFileError::unsupported : KeyFileError::malformed;
        }
        const auto der = block->der.as_slice();
        if (block->label == "PRIVATE KEY") {
            if (auto e = crypto::ed25519::private_key::from_pkcs8_der(der)) {
                k.kind = KeyKind::ed25519;
                k.ed25519.emplace(std::move(*e));
            } else if (auto p = crypto::p256::private_key::from_pkcs8_der(der)) {
                k.kind = KeyKind::ecdsa_p256;
                k.p256.emplace(std::move(*p));
            } else if (auto p3 = crypto::p384::private_key::from_pkcs8_der(der)) {
                k.kind = KeyKind::ecdsa_p384;
                k.p384.emplace(std::move(*p3));
            } else if (auto r = crypto::rsa::private_key::from_pkcs8_der(der)) {
                k.kind = KeyKind::rsa;
                k.rsa.emplace(std::move(*r));
            } else {
                return KeyFileError::unsupported;
            }
        } else if (block->label == "EC PRIVATE KEY") {
            if (auto p = crypto::p256::private_key::from_sec1_der(der)) {
                k.kind = KeyKind::ecdsa_p256;
                k.p256.emplace(std::move(*p));
            } else if (auto p3 = crypto::p384::private_key::from_sec1_der(der)) {
                k.kind = KeyKind::ecdsa_p384;
                k.p384.emplace(std::move(*p3));
            } else {
                return KeyFileError::unsupported;
            }
        } else {
            auto r = crypto::rsa::private_key::from_pkcs1_der(der);
            if (!r) {
                return KeyFileError::malformed;
            }
            k.kind = KeyKind::rsa;
            k.rsa.emplace(std::move(*r));
        }
        k.make_public();
        return KeyFileError::none;
    }

    // A key of any form read here: OpenSSH's when its BEGIN line is there,
    // PEM's otherwise
    inline KeyFileError read_key_file(const slice<const byte>& text, std::string_view passphrase, KeyPair& k) {
        std::string_view v(reinterpret_cast<const char*>(text.data()), text.size());
        if (v.find(OpensshBegin) != std::string_view::npos) {
            return read_openssh(v, passphrase, k);
        }
        return read_pem(text, k);
    }

    // A public key line ("type base64 [comment]", authorized_keys and .pub):
    // the blob decoded, its type checked against the line's, the comment
    inline bool parse_public_line(std::string_view line, Bytes& blob, std::string& comment, std::string_view& type) {
        auto skip = [&](size_t i) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            return i;
        };
        size_t i = skip(0);
        size_t t = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        type = line.substr(t, i - t);
        i = skip(i);
        size_t b = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') {
            ++i;
        }
        std::string_view b64 = line.substr(b, i - b);
        i = skip(i);
        std::string_view rest = line.substr(i);
        while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\r' || rest.back() == ' ' || rest.back() == '\t')) {
            rest.remove_suffix(1);
        }
        KeyKind kind;
        bool cert;
        if (type.empty() || b64.empty() || !kind_of_name(type, kind, cert)) {
            return false;
        }
        const auto& codec = encoding::base64::standard;
        blob.assign(codec.max_decoded_size(b64.size()), 0);
        auto n = codec.decode_to(mutable_bytes_of(blob.data(), blob.size()), slice<const char>(b64.data(), b64.size()));
        if (!n) {
            return false;
        }
        blob.resize(*n);
        ParsedKey pk;
        if (!parse_key(blob.data(), blob.size(), pk)) {
            return false;
        }
        Reader r(blob.data(), blob.size());
        if (r.string().view() != type) {
            return false;
        }
        comment.assign(rest);
        return true;
    }
}
