//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cbc.h"
#include "constant_time.h"
#include "ed25519.h"
#include "error.h"
#include "gcm.h"
#include "hash_id.h"
#include "kw.h"
#include "p256.h"
#include "p384.h"
#include "p521.h"
#include "random.h"
#include "rsa.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha256.h"
#include "sha512.h"
#include "x509.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "../encoding/asn1.h"
#include "../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// CMS, the Cryptographic Message Syntax (RFC 5652): data signed
// (SignedData) and data encrypted to the holders of certificates
// (EnvelopedData, and AuthEnvelopedData of RFC 5083), what S/MIME (RFC
// 8551), signed firmware and packages, PDF and code signatures carry.
//
// Signed: by the module's keys of every kind (RSA PKCS #1 v1.5 with
// SHA-256, ECDSA on P-256, P-384, P-521 with SHA-256, -384, -512, Ed25519
// with SHA-512 per RFC 8419), signed attributes always (content type,
// message digest, signing time); read also RSASSA-PSS, rsaEncryption as the
// signature algorithm (OpenSSL's), and signatures without signed
// attributes. Encrypted: AES-256-GCM (AuthEnvelopedData, S/MIME 4.0's) or
// AES-256-CBC (EnvelopedData, for older readers); the content key to an RSA
// certificate by RSAES-OAEP (SHA-256) and to a P-256, P-384 or P-521
// certificate by ephemeral-static ECDH (RFC 5753) with the X9.63 KDF and
// AES key wrap. Read also AES-128/192 and OAEP of SHA-1 to SHA-512.
// Refused, named in the error: RSA PKCS #1 v1.5 key transport (the
// Bleichenbacher oracle), 3DES, RC2, SHA-1 and MD5 signatures.
//
// Secrets: the content key is made and unwrapped in plain memory; an RSA
// key transport that does not decrypt goes on with a random content key
// (RFC 3218 2.3.2) so that it fails as a wrong tag fails; the GCM tag and
// the CBC padding are checked in constant time.
namespace sgcl::crypto::cms {
    // How a signature is made
    struct sign_options {
        bool detached = false;                      // the content left out of the SignedData (S/MIME's multipart/signed)
        bool include_certificates = true;           // the signer's certificate, and `certificates`, in the SignedData
        x509::chain certificates;                   // more to include: the intermediates of the signer's chain
        optional<time::datetime> signing_time;      // the signingTime attribute; nullopt: now
    };

    // How a signature is verified: the signer's chain under x509's options
    // (roots, intermediates besides the SignedData's, the time, the key
    // usages: empty asks for emailProtection, as OpenSSL's cms -verify)
    struct verify_options {
        x509::verify_options chain;
        x509::chain certificates;                   // signers' certificates the SignedData does not carry
    };

    // What a verification gives: the content, the first signer's verified
    // chain (the signer first), its signing time when it gave one
    struct verified {
        vector<byte> content;
        x509::chain signer;
        optional<time::datetime> signing_time;
    };

    // How data is encrypted
    enum class content_cipher : uint8_t {
        aes256_gcm,                                 // AuthEnvelopedData (RFC 5083): S/MIME 4.0's
        aes256_cbc                                  // EnvelopedData: for readers without AuthEnvelopedData
    };

    struct encrypt_options {
        content_cipher cipher = content_cipher::aes256_gcm;
    };
}

namespace sgcl::crypto::detail::cms {
    using encoding::asn1;
    using oid = asn1::oid;

    inline constexpr oid data{"1.2.840.113549.1.7.1"};
    inline constexpr oid signed_data{"1.2.840.113549.1.7.2"};
    inline constexpr oid enveloped_data{"1.2.840.113549.1.7.3"};
    inline constexpr oid auth_enveloped_data{"1.2.840.113549.1.9.16.1.23"};
    inline constexpr oid attr_content_type{"1.2.840.113549.1.9.3"};
    inline constexpr oid attr_message_digest{"1.2.840.113549.1.9.4"};
    inline constexpr oid attr_signing_time{"1.2.840.113549.1.9.5"};
    inline constexpr oid d_sha1{"1.3.14.3.2.26"};
    inline constexpr oid d_sha224{"2.16.840.1.101.3.4.2.4"};
    inline constexpr oid d_sha256{"2.16.840.1.101.3.4.2.1"};
    inline constexpr oid d_sha384{"2.16.840.1.101.3.4.2.2"};
    inline constexpr oid d_sha512{"2.16.840.1.101.3.4.2.3"};
    inline constexpr oid rsa_encryption{"1.2.840.113549.1.1.1"};
    inline constexpr oid rsaes_oaep{"1.2.840.113549.1.1.7"};
    inline constexpr oid mgf1{"1.2.840.113549.1.1.8"};
    inline constexpr oid rsassa_pss{"1.2.840.113549.1.1.10"};
    inline constexpr oid sha256_rsa{"1.2.840.113549.1.1.11"};
    inline constexpr oid sha384_rsa{"1.2.840.113549.1.1.12"};
    inline constexpr oid sha512_rsa{"1.2.840.113549.1.1.13"};
    inline constexpr oid ecdsa_sha256{"1.2.840.10045.4.3.2"};
    inline constexpr oid ecdsa_sha384{"1.2.840.10045.4.3.3"};
    inline constexpr oid ecdsa_sha512{"1.2.840.10045.4.3.4"};
    inline constexpr oid ed25519_oid{"1.3.101.112"};
    inline constexpr oid ec_public_key{"1.2.840.10045.2.1"};
    inline constexpr oid aes128_cbc{"2.16.840.1.101.3.4.1.2"};
    inline constexpr oid aes192_cbc{"2.16.840.1.101.3.4.1.22"};
    inline constexpr oid aes256_cbc{"2.16.840.1.101.3.4.1.42"};
    inline constexpr oid aes128_gcm{"2.16.840.1.101.3.4.1.6"};
    inline constexpr oid aes192_gcm{"2.16.840.1.101.3.4.1.26"};
    inline constexpr oid aes256_gcm{"2.16.840.1.101.3.4.1.46"};
    inline constexpr oid aes128_wrap{"2.16.840.1.101.3.4.1.5"};
    inline constexpr oid aes192_wrap{"2.16.840.1.101.3.4.1.25"};
    inline constexpr oid aes256_wrap{"2.16.840.1.101.3.4.1.45"};
    inline constexpr oid kdf_sha1{"1.3.133.16.840.63.0.2"};    // dhSinglePass-stdDH-sha1kdf-scheme
    inline constexpr oid kdf_sha224{"1.3.132.1.11.0"};
    inline constexpr oid kdf_sha256{"1.3.132.1.11.1"};
    inline constexpr oid kdf_sha384{"1.3.132.1.11.2"};
    inline constexpr oid kdf_sha512{"1.3.132.1.11.3"};

    inline error bad(const char* what) {
        return error(errc::malformed, string("sgcl::crypto::cms: ") + string(what));
    }

    inline error unsupported(const std::string& what) {
        return error(errc::unsupported, string("sgcl::crypto::cms: ") + string(what));
    }

    inline optional<hash_id> digest_of(const oid& o) noexcept {
        if (o == d_sha256) {
            return hash_id::sha256;
        }
        if (o == d_sha384) {
            return hash_id::sha384;
        }
        if (o == d_sha512) {
            return hash_id::sha512;
        }
        if (o == d_sha224) {
            return hash_id::sha224;
        }
        if (o == d_sha1) {
            return hash_id::sha1;
        }
        return nullopt;
    }

    inline oid oid_of(hash_id h) noexcept {
        switch (h) {
            case hash_id::sha384: return d_sha384;
            case hash_id::sha512: return d_sha512;
            case hash_id::sha224: return d_sha224;
            case hash_id::sha1: return d_sha1;
            default: return d_sha256;
        }
    }

    inline vector<byte> digest_of_bytes(hash_id h, const slice<const byte>& data) {
        return visit_hash(h, [&](auto t) {
            auto d = decltype(t)::type::of(data);
            return vector<byte>(d.data(), d.data() + d.size());
        });
    }

    inline asn1 algorithm(const oid& o) {
        return asn1::sequence({asn1::object_identifier(o)});
    }

    // The hash a key signs with, as signing_key signs
    inline hash_id hash_of(x509::key_kind k) noexcept {
        switch (k) {
            case x509::key_kind::p384: return hash_id::sha384;
            case x509::key_kind::p521:
            case x509::key_kind::ed25519: return hash_id::sha512;
            default: return hash_id::sha256;
        }
    }

    inline bool same(const slice<const byte>& a, const slice<const byte>& b) noexcept {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
    }

    // An INTEGER's content without its leading zero bytes
    inline slice<const byte> stripped(const slice<const byte>& b) noexcept {
        size_t i = 0;
        while (i + 1 < b.size() && b[i] == byte(0)) {
            ++i;
        }
        return b.subslice(i, b.size() - i);
    }

    // IssuerAndSerialNumber of a certificate
    inline asn1 issuer_and_serial(const x509::certificate& c) {
        auto issuer = asn1::parse(c.raw_issuer());
        const auto& serial = c.serial_number();
        vector<byte> content;
        if (serial.empty() || (uint8_t(serial[0]) & 0x80)) {
            content.push_back(byte(0));
        }
        for (auto b : serial) {
            content.push_back(b);
        }
        return asn1::sequence({*issuer, asn1::raw(asn1::tag_class::universal, 2, false, content)});
    }

    // Whether a SignerIdentifier, a RecipientIdentifier or a
    // KeyAgreeRecipientIdentifier names the certificate: IssuerAndSerialNumber,
    // or [0] SubjectKeyIdentifier
    inline bool names(const asn1& id, const x509::certificate& c) noexcept {
        if (id.is(asn1::type::sequence) && id.size() == 2 && id[1].is(asn1::type::integer)) {
            const auto ser = stripped(id[1].content());
            vector<byte> mine = c.serial_number();
            return same(id[0].bytes(), c.raw_issuer()) && same(ser, stripped(slice<const byte>(mine.data(), mine.size())));
        }
        if (id.is_context(0) && !id.constructed()) {
            const auto& ski = c.subject_key_id();
            return !ski.empty() && same(id.content(), slice<const byte>(ski.data(), ski.size()));
        }
        return false;
    }

    // --- signatures ---------------------------------------------------------

    // Whether sig is the signature of `signed_bytes` under the key of c by
    // the SignerInfo's algorithms (h the digest algorithm)
    inline expected<void, error> check_signature(const x509::certificate& c, const asn1& sig_alg, hash_id h, const slice<const byte>& signed_bytes,
                                                 const slice<const byte>& sig) {
        auto a = sig_alg.is(asn1::type::sequence) && sig_alg.size() >= 1 ? sig_alg[0].as_oid() : nullopt;
        if (!a) {
            return unexpected(bad("a signature algorithm that does not read"));
        }
        const auto& key = c.public_key();
        bool ok = false;
        bool matched = true;
        auto d = [&](hash_id id) {
            return digest_of_bytes(id, signed_bytes);
        };
        if (*a == rsa_encryption || *a == sha256_rsa || *a == sha384_rsa || *a == sha512_rsa) {
            const hash_id id = *a == sha384_rsa ? hash_id::sha384 : *a == sha512_rsa ? hash_id::sha512 : *a == sha256_rsa ? hash_id::sha256 : h;
            if ((matched = key.kind() == x509::key_kind::rsa)) {
                auto dg = d(id);
                ok = key.rsa().verify_digest(id, dg, sig);
            }
        } else if (*a == rsassa_pss) {
            // RSASSA-PSS-params: the hash, MGF1 of the same hash, the salt
            const asn1 p = sig_alg.size() == 2 ? sig_alg[1] : asn1();
            hash_id id = hash_id::sha1;
            int64_t salt = 20;
            if (p.is(asn1::type::sequence)) {
                for (const asn1 f : p) {
                    if (f.is_context(0) && f.size() == 1) {
                        auto o = f[0].is(asn1::type::sequence) ? f[0][0].as_oid() : nullopt;
                        auto hh = o ? digest_of(*o) : nullopt;
                        if (!hh) {
                            return unexpected(unsupported("a PSS hash the module does not have"));
                        }
                        id = *hh;
                    } else if (f.is_context(2) && f.size() == 1) {
                        salt = f[0].as_int().value_or(-1);
                    }
                }
            }
            if (id == hash_id::sha1 || salt < 0) {
                return unexpected(error(errc::verification, string("sgcl::crypto::cms: a PSS signature of SHA-1 or of a salt that does not read")));
            }
            if ((matched = key.kind() == x509::key_kind::rsa)) {
                auto dg = d(id);
                ok = key.rsa().verify_digest_pss(id, dg, sig, size_t(salt));
            }
        } else if (*a == ecdsa_sha256 || *a == ecdsa_sha384 || *a == ecdsa_sha512) {
            const hash_id id = *a == ecdsa_sha256 ? hash_id::sha256 : *a == ecdsa_sha384 ? hash_id::sha384 : hash_id::sha512;
            auto dg = d(id);
            if (key.kind() == x509::key_kind::p256) {
                ok = key.p256().verify_digest(dg, sig);
            } else if (key.kind() == x509::key_kind::p384) {
                ok = key.p384().verify_digest(dg, sig);
            } else if (key.kind() == x509::key_kind::p521) {
                ok = key.p521().verify_digest(dg, sig);
            } else {
                matched = false;
            }
        } else if (*a == ed25519_oid) {
            if ((matched = key.kind() == x509::key_kind::ed25519)) {
                ok = key.ed25519().verify(signed_bytes, sig);
            }
        } else {
            return unexpected(unsupported("a signature algorithm the module does not verify: " + std::string(a->to_string().view())));
        }
        if (!matched) {
            return unexpected(error(errc::verification, string("sgcl::crypto::cms: the signature is of another algorithm than the signer's key")));
        }
        if (!ok) {
            return unexpected(error(errc::verification, string("sgcl::crypto::cms: the signature does not verify")));
        }
        return {};
    }

    // --- key transport and agreement -----------------------------------------

    // The KDF of ANSI X9.63 (SEC 1 3.6.1): n bytes of H(Z || counter || info)
    inline secret_bytes x963(hash_id h, const slice<const byte>& z, const slice<const byte>& info, size_t n) {
        return visit_hash(h, [&](auto t) {
            using H = typename decltype(t)::type;
            secret_bytes out(n);
            auto* o = reinterpret_cast<uint8_t*>(out.as_slice().data());
            size_t done = 0;
            for (uint32_t counter = 1; done < n; ++counter) {
                H hh;
                hh.update(z);
                const uint8_t c[4] = {uint8_t(counter >> 24), uint8_t(counter >> 16), uint8_t(counter >> 8), uint8_t(counter)};
                hh.update(slice<const byte>(reinterpret_cast<const byte*>(c), 4));
                hh.update(info);
                auto d = hh.digest();
                const size_t take = n - done < d.size() ? n - done : d.size();
                std::memcpy(o + done, d.data(), take);
                secure_zero(d.data(), d.size());
                done += take;
            }
            return out;
        });
    }

    // ECC-CMS-SharedInfo (RFC 5753 7.2): the wrap algorithm, the UKM, the
    // KEK's length in bits
    inline vector<byte> shared_info(const oid& wrap, size_t kek_bytes, const slice<const byte>& ukm) {
        const uint32_t bits = uint32_t(kek_bytes * 8);
        const uint8_t len[4] = {uint8_t(bits >> 24), uint8_t(bits >> 16), uint8_t(bits >> 8), uint8_t(bits)};
        const asn1 s = asn1::sequence({algorithm(wrap), ukm.empty() ? asn1() : asn1::explicit_tag(0, asn1::octet_string(ukm)),
                                       asn1::explicit_tag(2, asn1::octet_string(slice<const byte>(reinterpret_cast<const byte*>(len), 4)))});
        auto b = s.bytes();
        return vector<byte>(b.data(), b.data() + b.size());
    }

    inline size_t wrap_size(const oid& o) noexcept {
        return o == aes128_wrap ? 16 : o == aes192_wrap ? 24 : o == aes256_wrap ? 32 : 0;
    }

    inline optional<hash_id> kdf_hash(const oid& o) noexcept {
        if (o == kdf_sha256) {
            return hash_id::sha256;
        }
        if (o == kdf_sha384) {
            return hash_id::sha384;
        }
        if (o == kdf_sha512) {
            return hash_id::sha512;
        }
        if (o == kdf_sha224) {
            return hash_id::sha224;
        }
        if (o == kdf_sha1) {
            return hash_id::sha1;
        }
        return nullopt;
    }

    // Z of an ephemeral-static ECDH between the recipient's key and the
    // originator's point, of the recipient's curve
    template<class Private, class Public>
    inline expected<secret_bytes, error> ecdh_z(const Private& key, const slice<const byte>& point) {
        auto p = Public::from_bytes(point);
        if (!p) {
            return unexpected(bad("the originator's key is not a point of the recipient's curve"));
        }
        auto z = key.to_ecdh().shared_secret(*p);
        if (!z) {
            return unexpected(bad("the originator's key is not a point of the recipient's curve"));
        }
        secret_bytes out(z->size);
        sgcl::detail::copy_bytes(out.as_slice().data(), z->bytes().data(), z->size);
        return out;
    }

    // The KeyTransRecipientInfo of the content key to an RSA certificate
    inline asn1 ktri(const x509::certificate& c, const slice<const byte>& cek) {
        auto enc = c.public_key().rsa().encrypt_oaep(hash_id::sha256, cek);
        const asn1 sha256_alg = asn1::sequence({asn1::object_identifier(d_sha256)});
        const asn1 params = asn1::sequence({asn1::explicit_tag(0, sha256_alg), asn1::explicit_tag(1, asn1::sequence({asn1::object_identifier(mgf1), sha256_alg}))});
        return asn1::sequence({asn1::integer(0), issuer_and_serial(c), asn1::sequence({asn1::object_identifier(rsaes_oaep), params}), asn1::octet_string(enc)});
    }

    // The KeyAgreeRecipientInfo of the content key to an EC certificate
    template<class Private, class Public>
    inline asn1 kari(const x509::certificate& c, const Public& peer, hash_id h, const oid& kdf, const slice<const byte>& cek) {
        auto eph = Private::generate();
        auto z = eph.to_ecdh().shared_secret(peer);
        if (!z) {
            throw invalid_argument("sgcl::crypto::cms::encrypt: a recipient's key that agrees on nothing");
        }
        auto info = shared_info(aes256_wrap, 32, slice<const byte>());
        auto kek = x963(h, z->bytes(), info, 32);
        auto wrapped = aes_kw(kek.as_slice()).wrap(cek);
        auto point = eph.public_key().bytes();
        const asn1 originator = asn1::explicit_tag(0, asn1::implicit_tag(1, asn1::sequence({algorithm(ec_public_key), asn1::bit_string(point)})));
        const asn1 inner = asn1::sequence({asn1::integer(3), originator, asn1::sequence({asn1::object_identifier(kdf), algorithm(aes256_wrap)}),
                                           asn1::sequence({asn1::sequence({issuer_and_serial(c), asn1::octet_string(wrapped)})})});
        return asn1::implicit_tag(1, inner);
    }

    // The content key of the RecipientInfos for the certificate and its key:
    // an RSA transport that does not decrypt gives a random key of the size
    inline expected<secret_bytes, error> unwrap(const asn1& infos, const x509::certificate& me, const x509::signing_key& key, size_t cek_size) {
        const void* k = x509::detail::SignerAccess::key(key);
        bool named = false;
        for (const asn1 ri : infos) {
            if (ri.is(asn1::type::sequence)) {
                // KeyTransRecipientInfo
                if (ri.size() != 4 || !names(ri[1], me)) {
                    continue;
                }
                named = true;
                if (key.kind() != x509::key_kind::rsa) {
                    return unexpected(error(errc::invalid_key, string("sgcl::crypto::cms: a key transport to a key that is not RSA")));
                }
                const asn1 alg = ri[2];
                auto a = alg.is(asn1::type::sequence) && alg.size() >= 1 ? alg[0].as_oid() : nullopt;
                if (a == optional<oid>(rsa_encryption)) {
                    return unexpected(unsupported("RSA PKCS #1 v1.5 key transport (its padding oracle): only RSAES-OAEP"));
                }
                if (a != optional<oid>(rsaes_oaep)) {
                    return unexpected(unsupported("a key transport the module does not have"));
                }
                hash_id h = hash_id::sha1, m = hash_id::sha1;
                if (alg.size() == 2 && alg[1].is(asn1::type::sequence)) {
                    for (const asn1 f : alg[1]) {
                        auto o = f.size() == 1 && f[0].is(asn1::type::sequence) && f[0].size() >= 1 ? f[0][0].as_oid() : nullopt;
                        if (f.is_context(0) && o) {
                            auto hh = digest_of(*o);
                            if (!hh) {
                                return unexpected(unsupported("an OAEP hash the module does not have"));
                            }
                            h = *hh;
                        } else if (f.is_context(1) && o == optional<oid>(mgf1) && f[0].size() == 2) {
                            auto mo = f[0][1].is(asn1::type::sequence) ? f[0][1][0].as_oid() : nullopt;
                            auto mh = mo ? digest_of(*mo) : nullopt;
                            if (!mh) {
                                return unexpected(unsupported("an MGF1 hash the module does not have"));
                            }
                            m = *mh;
                        } else if (f.is_context(2)) {
                            return unexpected(unsupported("an OAEP label (pSource)"));
                        }
                    }
                }
                auto enc = ri[3].as_bytes();
                if (!enc) {
                    return unexpected(bad("a KeyTransRecipientInfo that does not read"));
                }
                const auto* rk = static_cast<const rsa::private_key*>(k);
                secret_bytes buf(rk->public_key().max_oaep_message_size(h));
                auto n = rk->decrypt_oaep_to(buf.as_slice(), h, m, *enc, slice<const byte>());
                secret_bytes cek(cek_size);
                sgcl::detail::copy_bytes(cek.as_slice().data(), buf.as_slice().data(), buf.size() < cek_size ? buf.size() : cek_size);
                // a failure, or a key of another size: a random key, the same path
                secret_bytes rnd = random::secret(cek_size);
                const uint8_t good = uint8_t(0u - uint8_t(n.has_value() && *n == cek_size));
                auto* c = reinterpret_cast<uint8_t*>(cek.as_slice().data());
                const auto* r = reinterpret_cast<const uint8_t*>(rnd.as_slice().data());
                for (size_t i = 0; i < cek_size; ++i) {
                    c[i] = uint8_t((c[i] & good) | (r[i] & ~good));
                }
                return cek;
            }
            if (ri.is_context(1) && ri.constructed()) {
                // KeyAgreeRecipientInfo
                if (ri.size() < 4) {
                    return unexpected(bad("a KeyAgreeRecipientInfo that does not read"));
                }
                const asn1 rek = ri[ri.size() - 1];
                const asn1 kea = ri[ri.size() - 2];
                slice<const byte> wrapped;
                for (const asn1 e : rek) {
                    if (e.is(asn1::type::sequence) && e.size() == 2 && names(e[0], me)) {
                        auto w = e[1].as_bytes();
                        if (!w) {
                            return unexpected(bad("a RecipientEncryptedKey that does not read"));
                        }
                        wrapped = *w;
                        named = true;
                    }
                }
                if (!named) {
                    continue;
                }
                const asn1 orig = ri[1];
                if (!orig.is_context(0) || orig.size() != 1 || !orig[0].is_context(1) || orig[0].size() != 2) {
                    return unexpected(unsupported("an originator that is not an ephemeral key"));
                }
                auto bits = orig[0][1].as_bits();
                if (!bits || bits->length % 8) {
                    return unexpected(bad("the originator's key does not read"));
                }
                slice<const byte> ukm;
                if (ri.size() == 5 && ri[2].is_context(1) && ri[2].size() == 1) {
                    auto u = ri[2][0].as_bytes();
                    if (!u) {
                        return unexpected(bad("a UKM that does not read"));
                    }
                    ukm = *u;
                }
                auto kdf = kea.is(asn1::type::sequence) && kea.size() == 2 ? kea[0].as_oid() : nullopt;
                auto kh = kdf ? kdf_hash(*kdf) : nullopt;
                auto wrap = kea.size() == 2 && kea[1].is(asn1::type::sequence) ? kea[1][0].as_oid() : nullopt;
                const size_t ws = wrap ? wrap_size(*wrap) : 0;
                if (!kh || ws == 0) {
                    return unexpected(unsupported("a key agreement the module does not have (the X9.63 KDF and AES key wrap)"));
                }
                expected<secret_bytes, error> z = unexpected(error(errc::invalid_key, string("sgcl::crypto::cms: a key agreement to a key that is not P-256, P-384 or P-521")));
                switch (key.kind()) {
                    case x509::key_kind::p256: z = ecdh_z<p256::private_key, p256::public_key>(*static_cast<const p256::private_key*>(k), bits->bytes); break;
                    case x509::key_kind::p384: z = ecdh_z<p384::private_key, p384::public_key>(*static_cast<const p384::private_key*>(k), bits->bytes); break;
                    case x509::key_kind::p521: z = ecdh_z<p521::private_key, p521::public_key>(*static_cast<const p521::private_key*>(k), bits->bytes); break;
                    default: break;
                }
                if (!z) {
                    return unexpected(z.error());
                }
                auto info = shared_info(*wrap, ws, ukm);
                auto kek = x963(*kh, z->as_slice(), slice<const byte>(info.data(), info.size()), ws);
                auto cek = aes_kw(kek.as_slice()).unwrap(wrapped);
                if (!cek || cek->size() != cek_size) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::cms: the content key does not unwrap")));
                }
                return std::move(*cek);
            }
        }
        (void)named;
        return unexpected(error(errc::invalid_key, string("sgcl::crypto::cms: no recipient of the certificate given")));
    }
}

namespace sgcl::crypto::cms {
    // A SignedData (ContentInfo, DER) of the content by the signer's key: the
    // certificate's key, of any kind the module signs with (x509::signing_key
    // of the typed keys, pkcs12::signing_key); std::invalid_argument for a
    // certificate of another key
    inline vector<byte> sign(const slice<const byte>& content, const x509::certificate& signer, const x509::signing_key& key, const sign_options& o) {
        using namespace detail::cms;
        auto spki = key.public_key_der();
        if (!same(slice<const byte>(spki.data(), spki.size()), signer.raw_subject_public_key_info())) {
            throw std::invalid_argument("sgcl::crypto::cms::sign: the certificate is not the key's");
        }
        const hash_id h = hash_of(key.kind());
        auto md = digest_of_bytes(h, content);
        const time::datetime when = o.signing_time ? *o.signing_time : time::now();
        const int year = when.year();
        const asn1 t = year >= 1950 && year < 2050 ? asn1::utc_time(when) : asn1::generalized_time(when);
        const asn1 attrs = asn1::set({asn1::sequence({asn1::object_identifier(attr_content_type), asn1::set({asn1::object_identifier(data)})}),
                                      asn1::sequence({asn1::object_identifier(attr_signing_time), asn1::set({t})}),
                                      asn1::sequence({asn1::object_identifier(attr_message_digest), asn1::set({asn1::octet_string(md)})})});
        auto sig = x509::detail::SignerAccess::sign(key, attrs.bytes());
        auto alg_der = x509::detail::SignerAccess::algorithm(key);
        auto sig_alg = asn1::parse(slice<const byte>(reinterpret_cast<const byte*>(alg_der.data()), alg_der.size()));
        const asn1 digest_alg = algorithm(oid_of(h));
        const asn1 signer_info = asn1::sequence({asn1::integer(1), issuer_and_serial(signer), digest_alg, asn1::implicit_tag(0, attrs), *sig_alg, asn1::octet_string(sig)});
        vector<asn1> certs;
        if (o.include_certificates) {
            certs.push_back(*asn1::parse(signer.raw()));
            for (const auto& c : o.certificates) {
                certs.push_back(*asn1::parse(c.raw()));
            }
        }
        const asn1 encap = o.detached ? asn1::sequence({asn1::object_identifier(data)})
                                      : asn1::sequence({asn1::object_identifier(data), asn1::explicit_tag(0, asn1::octet_string(content))});
        const asn1 sd = asn1::sequence({asn1::integer(1), asn1::set({digest_alg}), encap, certs.empty() ? asn1() : asn1::implicit_tag(0, asn1::set(certs)),
                                        asn1::set({signer_info})});
        auto b = asn1::sequence({asn1::object_identifier(signed_data), asn1::explicit_tag(0, sd)}).bytes();
        return vector<byte>(b.data(), b.data() + b.size());
    }

    inline vector<byte> sign(const slice<const byte>& content, const x509::certificate& signer, const x509::signing_key& key) {
        return sign(content, signer, key, sign_options());
    }

    namespace detail {
        inline expected<verified, error> verify(const slice<const byte>& der, const slice<const byte>* detached, const verify_options& o) {
            using namespace crypto::detail::cms;
            auto ci = asn1::parse(der, asn1::ber);
            if (!ci || !ci->is(asn1::type::sequence) || ci->size() != 2 || (*ci)[0].as_oid() != optional<oid>(signed_data) || !(*ci)[1].is_context(0) ||
                (*ci)[1].size() != 1) {
                return unexpected(bad("not a ContentInfo of SignedData"));
            }
            const asn1 sd = (*ci)[1][0];
            if (!sd.is(asn1::type::sequence) || sd.size() < 4 || !sd[1].is(asn1::type::set) || !sd[2].is(asn1::type::sequence) || sd[2].size() < 1) {
                return unexpected(bad("a SignedData that does not read"));
            }
            const asn1 encap = sd[2];
            verified out;
            slice<const byte> content;
            if (encap.size() == 2) {
                if (!encap[1].is_context(0) || encap[1].size() != 1) {
                    return unexpected(bad("the encapsulated content does not read"));
                }
                auto c = encap[1][0].as_bytes();
                if (!c) {
                    return unexpected(bad("the encapsulated content is not an OCTET STRING"));
                }
                if (detached) {
                    return unexpected(bad("a detached content given for a SignedData that carries its own"));
                }
                content = *c;
            } else if (detached) {
                content = *detached;
            } else {
                return unexpected(bad("a detached signature without its content"));
            }
            auto ctype = encap[0].as_oid();
            if (!ctype) {
                return unexpected(bad("the encapsulated content type does not read"));
            }
            // the certificates carried, and the caller's
            x509::chain certs;
            size_t at = 3;
            if (at < sd.size() && sd[at].is_context(0)) {
                for (const asn1 c : sd[at]) {
                    if (c.is(asn1::type::sequence)) {
                        if (auto x = x509::certificate::parse(c.bytes())) {
                            certs.push_back(std::move(*x));
                        }
                    }
                }
                ++at;
            }
            if (at < sd.size() && sd[at].is_context(1)) {
                ++at;   // CRLs: not read
            }
            if (at + 1 != sd.size() || !sd[at].is(asn1::type::set) || sd[at].empty()) {
                return unexpected(bad("a SignedData without signers"));
            }
            for (const auto& c : o.certificates) {
                certs.push_back(c);
            }
            bool first = true;
            for (const asn1 si : sd[at]) {
                if (!si.is(asn1::type::sequence) || si.size() < 5) {
                    return unexpected(bad("a SignerInfo that does not read"));
                }
                const x509::certificate* signer = nullptr;
                for (const auto& c : certs) {
                    if (names(si[1], c)) {
                        signer = &c;
                        break;
                    }
                }
                if (!signer) {
                    return unexpected(error(errc::verification, string("sgcl::crypto::cms: the signer's certificate is neither in the SignedData nor given")));
                }
                auto dalg = si[2].is(asn1::type::sequence) && si[2].size() >= 1 ? si[2][0].as_oid() : nullopt;
                auto h = dalg ? digest_of(*dalg) : nullopt;
                if (!h) {
                    return unexpected(unsupported("a digest algorithm the module does not have"));
                }
                if (*h == hash_id::sha1) {
                    return unexpected(error(errc::verification, string("sgcl::crypto::cms: a signature over SHA-1, refused as insecure")));
                }
                size_t i = 3;
                optional<asn1> attrs;
                if (si[i].is_context(0)) {
                    attrs = si[i];
                    ++i;
                }
                if (i + 2 > si.size()) {
                    return unexpected(bad("a SignerInfo that does not read"));
                }
                const asn1 sig_alg = si[i];
                auto sig = si[i + 1].as_bytes();
                if (!sig) {
                    return unexpected(bad("a SignerInfo's signature does not read"));
                }
                optional<time::datetime> when;
                if (attrs) {
                    // the content type and the message digest must be there and agree
                    bool has_type = false, has_digest = false;
                    for (const asn1 a : *attrs) {
                        if (!a.is(asn1::type::sequence) || a.size() != 2 || !a[1].is(asn1::type::set) || a[1].size() != 1) {
                            return unexpected(bad("a signed attribute that does not read"));
                        }
                        auto t = a[0].as_oid();
                        if (t == optional<oid>(attr_content_type)) {
                            has_type = a[1][0].as_oid() == ctype;
                        } else if (t == optional<oid>(attr_message_digest)) {
                            auto md = a[1][0].as_bytes();
                            auto mine = digest_of_bytes(*h, content);
                            has_digest = md && constant_time::equal(*md, mine);
                        } else if (t == optional<oid>(attr_signing_time)) {
                            when = a[1][0].as_time();
                        }
                    }
                    if (!has_type || !has_digest) {
                        return unexpected(error(errc::verification, string("sgcl::crypto::cms: the content type or the message digest does not match the content")));
                    }
                    // signed as a SET OF: the [0] IMPLICIT tag turned back into 0x31
                    vector<byte> der_attrs(attrs->bytes().data(), attrs->bytes().data() + attrs->bytes().size());
                    der_attrs[0] = byte(0x31);
                    if (auto r = check_signature(*signer, sig_alg, *h, der_attrs, *sig); !r) {
                        return unexpected(r.error());
                    }
                } else {
                    if (ctype != optional<oid>(data)) {
                        return unexpected(bad("a content of another type than data without signed attributes"));
                    }
                    if (auto r = check_signature(*signer, sig_alg, *h, content, *sig); !r) {
                        return unexpected(r.error());
                    }
                }
                // the signer's chain
                x509::verify_options vo = o.chain;
                if (vo.key_usages.empty()) {
                    vo.key_usages.push_back(x509::ext_key_usage::email_protection);
                }
                auto pool = x509::certificate_pool();
                for (const auto& c : certs) {
                    pool.add(c);
                }
                for (const auto& c : x509::detail::PoolAccess::data(o.chain.intermediates).certs) {
                    pool.add(c);
                }
                vo.intermediates = pool;   // the time: o.chain's, else now (as OpenSSL), never the signer's own claim
                auto chain = signer->verify(vo);
                if (!chain) {
                    return unexpected(chain.error());
                }
                if (first) {
                    out.signer = *chain;
                    out.signing_time = when;
                    first = false;
                }
            }
            out.content = vector<byte>(content.data(), content.data() + content.size());
            return out;
        }
    }

    // The content of a SignedData (DER or BER) whose every signature
    // verifies under its signer's certificate (carried, or among
    // o.certificates) and whose signer's chain verifies under o.chain:
    // errc::verification for a signature, a digest or a chain that does not
    // verify, errc::malformed for bytes that do not read, errc::unsupported
    // for an algorithm the module does not have
    inline expected<verified, error> verify(const slice<const byte>& der, const verify_options& o) noexcept {
        try {
            return detail::verify(der, nullptr, o);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return unexpected(crypto::detail::cms::bad("bytes that do not read"));
        }
    }

    inline expected<verified, error> verify(const slice<const byte>& der) noexcept {
        return verify(der, verify_options());
    }

    // A detached SignedData over the content given
    inline expected<verified, error> verify_detached(const slice<const byte>& der, const slice<const byte>& content, const verify_options& o) noexcept {
        try {
            return detail::verify(der, &content, o);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return unexpected(crypto::detail::cms::bad("bytes that do not read"));
        }
    }

    inline expected<verified, error> verify_detached(const slice<const byte>& der, const slice<const byte>& content) noexcept {
        return verify_detached(der, content, verify_options());
    }

    // The content encrypted to the certificates' holders (ContentInfo, DER):
    // an RSA certificate by RSAES-OAEP with SHA-256, a P-256, P-384 or P-521
    // one by ephemeral-static ECDH; std::invalid_argument for no recipient
    // or a certificate of another key (Ed25519 encrypts nothing)
    inline vector<byte> encrypt(const slice<const byte>& content, const x509::chain& recipients, const encrypt_options& o) {
        using namespace crypto::detail::cms;
        if (recipients.empty()) {
            throw std::invalid_argument("sgcl::crypto::cms::encrypt: no recipient");
        }
        auto cek = random::secret(32);
        vector<asn1> infos;
        for (const auto& c : recipients) {
            const auto& k = c.public_key();
            switch (k.kind()) {
                case x509::key_kind::rsa: infos.push_back(ktri(c, cek.as_slice())); break;
                case x509::key_kind::p256: infos.push_back(kari<p256::private_key>(c, k.p256(), hash_id::sha256, kdf_sha256, cek.as_slice())); break;
                case x509::key_kind::p384: infos.push_back(kari<p384::private_key>(c, k.p384(), hash_id::sha384, kdf_sha384, cek.as_slice())); break;
                case x509::key_kind::p521: infos.push_back(kari<p521::private_key>(c, k.p521(), hash_id::sha512, kdf_sha512, cek.as_slice())); break;
                default: throw std::invalid_argument("sgcl::crypto::cms::encrypt: a recipient whose key encrypts nothing (Ed25519, an unknown kind)");
            }
        }
        bool any_kari = false;
        for (const auto& c : recipients) {
            any_kari |= c.public_key().kind() != x509::key_kind::rsa;
        }
        asn1 ci;
        if (o.cipher == content_cipher::aes256_gcm) {
            auto nonce = random::bytes(12);
            auto sealed = aes_gcm(cek.as_slice()).seal(nonce.as_slice(), content);
            const size_t n = sealed.size() - 16;
            const asn1 eci = asn1::sequence({asn1::object_identifier(data),
                                             asn1::sequence({asn1::object_identifier(aes256_gcm), asn1::sequence({asn1::octet_string(nonce.as_slice()), asn1::integer(16)})}),
                                             asn1::implicit_tag(0, asn1::octet_string(slice<const byte>(sealed.data(), n)))});
            const asn1 aed = asn1::sequence({asn1::integer(0), asn1::set(infos), eci, asn1::octet_string(slice<const byte>(sealed.data() + n, 16))});
            ci = asn1::sequence({asn1::object_identifier(auth_enveloped_data), asn1::explicit_tag(0, aed)});
        } else {
            auto iv = random::bytes(16);
            auto ct = aes_cbc(cek.as_slice(), iv.as_slice()).encrypt(content);
            const asn1 eci = asn1::sequence({asn1::object_identifier(data), asn1::sequence({asn1::object_identifier(aes256_cbc), asn1::octet_string(iv.as_slice())}),
                                             asn1::implicit_tag(0, asn1::octet_string(ct))});
            const asn1 ed = asn1::sequence({asn1::integer(any_kari ? 2 : 0), asn1::set(infos), eci});
            ci = asn1::sequence({asn1::object_identifier(enveloped_data), asn1::explicit_tag(0, ed)});
        }
        auto b = ci.bytes();
        return vector<byte>(b.data(), b.data() + b.size());
    }

    inline vector<byte> encrypt(const slice<const byte>& content, const x509::chain& recipients) {
        return encrypt(content, recipients, encrypt_options());
    }

    namespace detail {
        inline expected<vector<byte>, error> decrypt(const slice<const byte>& der, const x509::certificate& me, const x509::signing_key& key) {
            using namespace crypto::detail::cms;
            auto ci = asn1::parse(der, asn1::ber);
            if (!ci || !ci->is(asn1::type::sequence) || ci->size() != 2 || !(*ci)[1].is_context(0) || (*ci)[1].size() != 1) {
                return unexpected(bad("not a ContentInfo"));
            }
            auto type = (*ci)[0].as_oid();
            const bool auth = type == optional<oid>(auth_enveloped_data);
            if (!auth && type != optional<oid>(enveloped_data)) {
                return unexpected(bad("not a ContentInfo of EnvelopedData or AuthEnvelopedData"));
            }
            const asn1 ed = (*ci)[1][0];
            if (!ed.is(asn1::type::sequence) || ed.size() < 3) {
                return unexpected(bad("an EnvelopedData that does not read"));
            }
            size_t at = 1;
            if (ed[at].is_context(0)) {
                ++at;   // OriginatorInfo: not read
            }
            if (at + 1 >= ed.size() || !ed[at].is(asn1::type::set)) {
                return unexpected(bad("an EnvelopedData without recipients"));
            }
            const asn1 infos = ed[at];
            const asn1 eci = ed[at + 1];
            if (!eci.is(asn1::type::sequence) || eci.size() != 3 || !eci[1].is(asn1::type::sequence) || eci[1].size() != 2) {
                return unexpected(bad("the encrypted content does not read"));
            }
            auto calg = eci[1][0].as_oid();
            auto ct = eci[2].is_context(0) ? eci[2].as_bytes() : nullopt;
            if (!calg || !ct) {
                return unexpected(bad("the encrypted content does not read"));
            }
            if (auth) {
                const size_t ks = *calg == aes128_gcm ? 16 : *calg == aes192_gcm ? 24 : *calg == aes256_gcm ? 32 : 0;
                if (ks == 0) {
                    return unexpected(unsupported("a content encryption the module does not have (AES-GCM)"));
                }
                const asn1 p = eci[1][1];
                auto nonce = p.is(asn1::type::sequence) && p.size() >= 1 ? p[0].as_bytes() : nullopt;
                const int64_t icv = p.size() == 2 ? p[1].as_int().value_or(0) : 12;
                if (!nonce || nonce->size() != 12 || icv != 16) {
                    return unexpected(unsupported("a GCM nonce of other than 12 bytes or a tag of other than 16"));
                }
                // authAttrs [1] (the AAD), then the MAC
                size_t m = at + 2;
                vector<byte> aad;
                if (m < ed.size() && ed[m].is_context(1)) {
                    auto b = ed[m].bytes();
                    aad.assign(b.data(), b.data() + b.size());
                    aad[0] = byte(0x31);
                    ++m;
                }
                auto tag = m < ed.size() ? ed[m].as_bytes() : nullopt;
                if (!tag || tag->size() != 16) {
                    return unexpected(bad("the AuthEnvelopedData's MAC does not read"));
                }
                auto cek = unwrap(infos, me, key, ks);
                if (!cek) {
                    return unexpected(cek.error());
                }
                vector<byte> sealed(ct->data(), ct->data() + ct->size());
                for (auto b : *tag) {
                    sealed.push_back(b);
                }
                auto pt = aes_gcm(cek->as_slice()).open(*nonce, sealed, aad);
                if (!pt) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::cms: the content does not authenticate")));
                }
                return std::move(*pt);
            }
            const size_t ks = *calg == aes128_cbc ? 16 : *calg == aes192_cbc ? 24 : *calg == aes256_cbc ? 32 : 0;
            if (ks == 0) {
                return unexpected(unsupported("a content encryption the module does not have (AES-CBC, AES-GCM): " + std::string(calg->to_string().view())));
            }
            auto iv = eci[1][1].as_bytes();
            if (!iv || iv->size() != 16 || ct->empty() || ct->size() % 16) {
                return unexpected(bad("the CBC IV or ciphertext does not read"));
            }
            auto cek = unwrap(infos, me, key, ks);
            if (!cek) {
                return unexpected(cek.error());
            }
            auto pt = aes_cbc(cek->as_slice(), *iv).decrypt(*ct);
            if (!pt) {
                return unexpected(error(errc::authentication, string("sgcl::crypto::cms: the content does not decrypt")));
            }
            return std::move(*pt);
        }
    }

    // The content of an EnvelopedData or AuthEnvelopedData (DER or BER) for
    // the recipient's certificate and key (of the typed keys,
    // pkcs12::signing_key): errc::invalid_key for no recipient of the
    // certificate, errc::authentication for a content that does not decrypt
    // or authenticate, errc::unsupported (named) for what the module does
    // not have, errc::malformed for bytes that do not read
    inline expected<vector<byte>, error> decrypt(const slice<const byte>& der, const x509::certificate& recipient, const x509::signing_key& key) noexcept {
        try {
            return detail::decrypt(der, recipient, key);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return unexpected(crypto::detail::cms::bad("bytes that do not read"));
        }
    }
}
