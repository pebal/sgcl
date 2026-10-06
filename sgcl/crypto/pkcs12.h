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
#include "hash_id.h"
#include "hmac.h"
#include "p256.h"
#include "p384.h"
#include "p521.h"
#include "pbkdf2.h"
#include "random.h"
#include "rsa.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha1.h"
#include "sha256.h"
#include "sha512.h"
#include "x509.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../encoding/asn1.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// PKCS #12 (RFC 7292): a private key and its certificate chain in one file
// sealed by a password, what browsers, Windows, Java and `openssl pkcs12`
// exchange. Read: the PBES2 encryptions of RFC 8018 (PBKDF2 with HMAC-SHA-1
// to SHA-512, AES-128/192/256-CBC), the MAC of RFC 7292 (HMAC of SHA-1 to
// SHA-512 under the key of its Appendix B) or of RFC 9579 (PBMAC1); the
// legacy PBEs of PKCS #12 (3DES, RC2, RC4) are refused, the module having
// none of those ciphers. Written: what OpenSSL 3 writes by default, the key
// and the certificates under PBES2 with PBKDF2-HMAC-SHA-256 and AES-256-CBC,
// a MAC of HMAC-SHA-256.
//
// Secrets: the password is read where it lies; the decrypted key goes from
// the cipher into plain memory (a secret_bytes) and from there into the
// module's typed key, never through managed memory; the MAC is compared in
// constant time and the CBC padding checked in constant time.
namespace sgcl::crypto {
    class pkcs12;

    namespace detail {
        // The key of a file: its PKCS #8 bytes and the typed key read from
        // them, in plain memory, zeroed by their destructors
        struct Pkcs12Key {
            secret_bytes pkcs8;
            x509::key_kind kind = x509::key_kind::none;
            optional<p256::private_key> p256;
            optional<p384::private_key> p384;
            optional<p521::private_key> p521;
            optional<ed25519::private_key> ed25519;
            optional<rsa::private_key> rsa;

            // The typed key of PKCS #8 bytes: false for none the module has
            bool read(const slice<const byte>& der) {
                if (auto k = ed25519::private_key::from_pkcs8_der(der)) {
                    ed25519.emplace(std::move(*k));
                    kind = x509::key_kind::ed25519;
                } else if (auto a = p256::private_key::from_pkcs8_der(der)) {
                    p256.emplace(std::move(*a));
                    kind = x509::key_kind::p256;
                } else if (auto b = p384::private_key::from_pkcs8_der(der)) {
                    p384.emplace(std::move(*b));
                    kind = x509::key_kind::p384;
                } else if (auto c = p521::private_key::from_pkcs8_der(der)) {
                    p521.emplace(std::move(*c));
                    kind = x509::key_kind::p521;
                } else if (auto r = rsa::private_key::from_pkcs8_der(der)) {
                    rsa.emplace(std::move(*r));
                    kind = x509::key_kind::rsa;
                } else {
                    return false;
                }
                pkcs8 = secret_bytes(der.size());
                sgcl::detail::copy_bytes(pkcs8.as_slice().data(), der.data(), der.size());
                return true;
            }

            x509::signing_key signer() const {
                switch (kind) {
                    case x509::key_kind::p256: return x509::signing_key(*p256);
                    case x509::key_kind::p384: return x509::signing_key(*p384);
                    case x509::key_kind::p521: return x509::signing_key(*p521);
                    case x509::key_kind::ed25519: return x509::signing_key(*ed25519);
                    case x509::key_kind::rsa: return x509::signing_key(*rsa);
                    case x509::key_kind::none: break;
                }
                throw logic_error("sgcl::crypto::pkcs12: the file holds no private key");
            }
        };

        struct Pkcs12State {
            x509::chain certificates;
            string friendly_name;
            std::unique_ptr<Pkcs12Key> key;
        };

        namespace p12 {
            using encoding::asn1;
            using oid = asn1::oid;

            inline constexpr oid data{"1.2.840.113549.1.7.1"};
            inline constexpr oid encrypted_data{"1.2.840.113549.1.7.6"};
            inline constexpr oid key_bag{"1.2.840.113549.1.12.10.1.1"};
            inline constexpr oid shrouded_key_bag{"1.2.840.113549.1.12.10.1.2"};
            inline constexpr oid cert_bag{"1.2.840.113549.1.12.10.1.3"};
            inline constexpr oid x509_certificate{"1.2.840.113549.1.9.22.1"};
            inline constexpr oid friendly_name{"1.2.840.113549.1.9.20"};
            inline constexpr oid local_key_id{"1.2.840.113549.1.9.21"};
            inline constexpr oid pbes2{"1.2.840.113549.1.5.13"};
            inline constexpr oid kdf_pbkdf2{"1.2.840.113549.1.5.12"};
            inline constexpr oid pbmac1{"1.2.840.113549.1.5.14"};
            inline constexpr oid aes128_cbc{"2.16.840.1.101.3.4.1.2"};
            inline constexpr oid aes192_cbc{"2.16.840.1.101.3.4.1.22"};
            inline constexpr oid aes256_cbc{"2.16.840.1.101.3.4.1.42"};
            inline constexpr oid d_sha1{"1.3.14.3.2.26"};
            inline constexpr oid d_sha224{"2.16.840.1.101.3.4.2.4"};
            inline constexpr oid d_sha256{"2.16.840.1.101.3.4.2.1"};
            inline constexpr oid d_sha384{"2.16.840.1.101.3.4.2.2"};
            inline constexpr oid d_sha512{"2.16.840.1.101.3.4.2.3"};
            inline constexpr oid hmac_sha1{"1.2.840.113549.2.7"};
            inline constexpr oid hmac_sha224{"1.2.840.113549.2.8"};
            inline constexpr oid hmac_sha256{"1.2.840.113549.2.9"};
            inline constexpr oid hmac_sha384{"1.2.840.113549.2.10"};
            inline constexpr oid hmac_sha512{"1.2.840.113549.2.11"};
            inline constexpr oid legacy_pbe{"1.2.840.113549.1.12.1"};   // pbeWithSHAAnd*: 3DES, RC2, RC4

            inline error bad(const char* what) {
                return error(errc::malformed, string("sgcl::crypto::pkcs12: ") + string(what));
            }

            inline optional<hash_id> digest_of(const oid& o) noexcept {
                if (o == d_sha1 || o == hmac_sha1) {
                    return hash_id::sha1;
                }
                if (o == d_sha224 || o == hmac_sha224) {
                    return hash_id::sha224;
                }
                if (o == d_sha256 || o == hmac_sha256) {
                    return hash_id::sha256;
                }
                if (o == d_sha384 || o == hmac_sha384) {
                    return hash_id::sha384;
                }
                if (o == d_sha512 || o == hmac_sha512) {
                    return hash_id::sha512;
                }
                return nullopt;
            }

            // The password as RFC 7292 Appendix B.1 has it for its KDF: a
            // BMPString, big-endian UTF-16 and two zero bytes; a password that
            // is not UTF-8 is taken byte by byte, as OpenSSL takes it
            inline secret_bytes bmp_password(const slice<const byte>& password) {
                const auto* p = reinterpret_cast<const uint8_t*>(password.data());
                const size_t n = password.size();
                std::vector<uint32_t> cps;
                bool ok = true;
                for (size_t i = 0; i < n && ok;) {
                    uint32_t c = p[i];
                    size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
                    if (len == 0 || i + len > n) {
                        ok = false;
                        break;
                    }
                    if (len > 1) {
                        c &= 0xFFu >> (len + 1);
                        for (size_t k = 1; k < len; ++k) {
                            if ((p[i + k] >> 6) != 2) {
                                ok = false;
                            }
                            c = c << 6 | (p[i + k] & 0x3F);
                        }
                    }
                    cps.push_back(c);
                    i += len;
                }
                if (!ok) {
                    cps.assign(p, p + n);
                }
                size_t units = 0;
                for (uint32_t c : cps) {
                    units += c >= 0x10000 ? 2 : 1;
                }
                secret_bytes out(2 * units + 2);
                auto* o = reinterpret_cast<uint8_t*>(out.as_slice().data());
                for (uint32_t c : cps) {
                    if (c >= 0x10000) {
                        const uint32_t v = c - 0x10000;
                        const uint32_t hi = 0xD800 | (v >> 10), lo = 0xDC00 | (v & 0x3FF);
                        *o++ = uint8_t(hi >> 8);
                        *o++ = uint8_t(hi);
                        *o++ = uint8_t(lo >> 8);
                        *o++ = uint8_t(lo);
                    } else {
                        *o++ = uint8_t(c >> 8);
                        *o++ = uint8_t(c);
                    }
                }
                *o++ = 0;
                *o = 0;
                secure_zero(cps.data(), cps.size() * sizeof(uint32_t));
                return out;
            }

            // The key derivation of RFC 7292 Appendix B.2 with the hash H:
            // n bytes for the purpose id (1 a key, 2 an IV, 3 a MAC key)
            template<class H>
            inline secret_bytes kdf(uint8_t id, const slice<const byte>& bmp, const slice<const byte>& salt, uint32_t iterations, size_t n) {
                constexpr size_t u = H::digest_size, v = H::block_size;
                auto fill = [&](const slice<const byte>& src) {
                    const size_t len = src.empty() ? 0 : v * ((src.size() + v - 1) / v);
                    std::vector<uint8_t> out(len);
                    for (size_t i = 0; i < len; ++i) {
                        out[i] = uint8_t(src[i % src.size()]);
                    }
                    return out;
                };
                std::vector<uint8_t> s = fill(salt), pw = fill(bmp);
                std::vector<uint8_t> big(s.size() + pw.size());
                if (!s.empty()) {
                    std::memcpy(big.data(), s.data(), s.size());
                }
                if (!pw.empty()) {
                    std::memcpy(big.data() + s.size(), pw.data(), pw.size());
                }
                secure_zero(pw.data(), pw.size());
                uint8_t d[v];
                std::memset(d, id, v);
                secret_bytes out(n);
                auto* o = reinterpret_cast<uint8_t*>(out.as_slice().data());
                for (size_t done = 0; done < n;) {
                    H h;
                    h.update(slice<const byte>(reinterpret_cast<const byte*>(d), v));
                    h.update(slice<const byte>(reinterpret_cast<const byte*>(big.data()), big.size()));
                    auto a = h.digest();
                    for (uint32_t r = 1; r < iterations; ++r) {
                        a = H::of(slice<const byte>(a.data(), a.size()));
                    }
                    const size_t take = n - done < u ? n - done : u;
                    std::memcpy(o + done, a.data(), take);
                    done += take;
                    if (done >= n) {
                        secure_zero(a.data(), a.size());
                        break;
                    }
                    // I_j = (I_j + B + 1) mod 2^(8v), B the digest repeated to v bytes
                    uint8_t b[v];
                    for (size_t i = 0; i < v; ++i) {
                        b[i] = uint8_t(a[i % u]);
                    }
                    for (size_t j = 0; j < big.size(); j += v) {
                        unsigned carry = 1;
                        for (size_t i = v; i-- > 0;) {
                            carry += unsigned(big[j + i]) + b[i];
                            big[j + i] = uint8_t(carry);
                            carry >>= 8;
                        }
                    }
                    secure_zero(b, sizeof b);
                    secure_zero(a.data(), a.size());
                }
                secure_zero(big.data(), big.size());
                return out;
            }

            inline secret_bytes kdf_of(hash_id h, uint8_t id, const slice<const byte>& bmp, const slice<const byte>& salt, uint32_t iterations, size_t n) {
                return visit_hash(h, [&](auto t) { return kdf<typename decltype(t)::type>(id, bmp, salt, iterations, n); });
            }

            inline secret_bytes hmac_of(hash_id h, const slice<const byte>& key, const slice<const byte>& data) {
                return visit_hash(h, [&](auto t) {
                    auto tag = hmac<typename decltype(t)::type>::of(data, key);
                    secret_bytes out(tag.size());
                    std::memcpy(out.as_slice().data(), tag.data(), tag.size());
                    return out;
                });
            }

            inline secret_bytes pbkdf2_of(hash_id h, const slice<const byte>& password, const slice<const byte>& salt, uint32_t iterations, size_t n) {
                return visit_hash(h, [&](auto t) { return crypto::pbkdf2<typename decltype(t)::type>::derive(password, salt, iterations, n); });
            }

            // PBKDF2-params (RFC 8018 A.2): the salt, the iterations, the key
            // length if given and the PRF (HMAC-SHA-1 by default)
            struct Pbkdf2 {
                slice<const byte> salt;
                uint32_t iterations = 0;
                size_t key_length = 0;
                hash_id prf = hash_id::sha1;
            };

            inline expected<Pbkdf2, error> read_pbkdf2(const asn1& alg, uint32_t max_iterations) {
                if (!alg.is(asn1::type::sequence) || alg.size() < 2 || alg[0].as_oid() != optional<oid>(kdf_pbkdf2)) {
                    return unexpected(bad("a key derivation that is not PBKDF2"));
                }
                const asn1 p = alg[1];
                if (!p.is(asn1::type::sequence) || p.size() < 2 || p.size() > 4) {
                    return unexpected(bad("PBKDF2's parameters do not read"));
                }
                Pbkdf2 out;
                auto salt = p[0].as_bytes();
                auto iter = p[1].as_int();
                if (!salt || !p[0].is(asn1::type::octet_string) || !iter || *iter < 1) {
                    return unexpected(bad("PBKDF2's salt or iteration count does not read"));
                }
                if (uint64_t(*iter) > max_iterations) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: more iterations than options::max_iterations")));
                }
                out.salt = *salt;
                out.iterations = uint32_t(*iter);
                size_t at = 2;
                if (at < p.size() && p[at].is(asn1::type::integer)) {
                    auto kl = p[at].as_int();
                    if (!kl || *kl < 1 || *kl > 64) {
                        return unexpected(bad("PBKDF2's key length does not read"));
                    }
                    out.key_length = size_t(*kl);
                    ++at;
                }
                if (at < p.size()) {
                    const asn1 prf = p[at];
                    auto h = prf.is(asn1::type::sequence) && prf.size() >= 1 ? prf[0].as_oid() : nullopt;
                    auto d = h ? digest_of(*h) : nullopt;
                    if (!d || *h == d_sha1 || *h == d_sha256 || *h == d_sha224 || *h == d_sha384 || *h == d_sha512) {
                        return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a PBKDF2 PRF the module does not have")));
                    }
                    out.prf = *d;
                    ++at;
                }
                if (at != p.size()) {
                    return unexpected(bad("PBKDF2's parameters do not read"));
                }
                return out;
            }

            // The plaintext of a PBES2 encryption (RFC 8018 6.2), in plain
            // memory: errc::authentication for a padding that is not PKCS #7's
            inline expected<secret_bytes, error> pbes2_decrypt(const asn1& alg, const slice<const byte>& ciphertext, const slice<const byte>& password,
                                                               uint32_t max_iterations) {
                auto id = alg.is(asn1::type::sequence) && alg.size() >= 1 ? alg[0].as_oid() : nullopt;
                if (!id) {
                    return unexpected(bad("an encryption algorithm that does not read"));
                }
                if (id->starts_with(legacy_pbe)) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a legacy PBE of PKCS #12 (3DES, RC2, RC4) the module does not have: ") +
                                                                   id->to_string()));
                }
                if (*id != pbes2 || alg.size() != 2 || !alg[1].is(asn1::type::sequence) || alg[1].size() != 2) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: an encryption the module does not have: ") + id->to_string()));
                }
                auto kdf = read_pbkdf2(alg[1][0], max_iterations);
                if (!kdf) {
                    return unexpected(kdf.error());
                }
                const asn1 scheme = alg[1][1];
                auto cipher = scheme.is(asn1::type::sequence) && scheme.size() == 2 ? scheme[0].as_oid() : nullopt;
                const size_t key_size = !cipher ? 0 : *cipher == aes128_cbc ? 16 : *cipher == aes192_cbc ? 24 : *cipher == aes256_cbc ? 32 : 0;
                if (key_size == 0) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a cipher the module does not have (AES-CBC only)")));
                }
                auto iv = scheme[1].as_bytes();
                if (!iv || iv->size() != 16 || (kdf->key_length && kdf->key_length != key_size)) {
                    return unexpected(bad("the cipher's IV or key length does not read"));
                }
                if (ciphertext.empty() || ciphertext.size() % 16 != 0) {
                    return unexpected(bad("a ciphertext of no whole blocks"));
                }
                auto key = pbkdf2_of(kdf->prf, password, kdf->salt, kdf->iterations, key_size);
                aes_cbc c(key.as_slice(), *iv);
                secret_bytes out(ciphertext.size());
                c.decrypt_blocks(out.as_slice(), ciphertext);
                // PKCS #7: 1 to 16, the last `pad` bytes equal to it, checked whole
                const auto* last = reinterpret_cast<const uint8_t*>(out.as_slice().data()) + out.size() - 16;
                const uint32_t pad = last[15];
                uint32_t badpad = (((pad | (0u - pad)) >> 31) ^ 1u) | (uint32_t(16 - pad) >> 31);
                for (uint32_t i = 0; i < 16; ++i) {
                    const uint32_t in_pad = uint32_t(i - pad) >> 31;
                    const uint32_t diff = uint32_t(last[15 - i] ^ pad);
                    badpad |= in_pad & ((diff | (0u - diff)) >> 31);
                }
                if (badpad) {
                    return unexpected(error(errc::authentication, string("sgcl::crypto::pkcs12: the content does not decrypt (a wrong password?)")));
                }
                secret_bytes plain(out.size() - pad);
                sgcl::detail::copy_bytes(plain.as_slice().data(), out.as_slice().data(), plain.size());
                return plain;
            }

            // A PBES2 AlgorithmIdentifier and the ciphertext of plaintext
            // (PBKDF2-HMAC-SHA-256, AES-256-CBC), salt and IV random
            inline pair<asn1, vector<byte>> pbes2_encrypt(const slice<const byte>& plaintext, const slice<const byte>& password, uint32_t iterations) {
                auto salt = random::bytes(16);
                auto iv = random::bytes(16);
                auto key = crypto::pbkdf2<crypto::sha256>::derive(password, salt.as_slice(), iterations, 32);
                aes_cbc c(key.as_slice(), iv.as_slice());
                vector<byte> ct = c.encrypt(plaintext);
                asn1 alg = asn1::sequence({asn1::object_identifier(pbes2),
                                           asn1::sequence({asn1::sequence({asn1::object_identifier(kdf_pbkdf2),
                                                                           asn1::sequence({asn1::octet_string(salt.as_slice()), asn1::integer(iterations),
                                                                                           asn1::sequence({asn1::object_identifier(hmac_sha256), asn1::null()})})}),
                                                           asn1::sequence({asn1::object_identifier(aes256_cbc), asn1::octet_string(iv.as_slice())})})});
                return pair<asn1, vector<byte>>(alg, std::move(ct));
            }

            // A bag's attributes: friendlyName and localKeyId, copied out of
            // the bytes they were read from (a decrypted part, freed after it)
            struct Attributes {
                string friendly_name;
                std::vector<uint8_t> local_key_id;
            };

            inline Attributes attributes_of(const asn1& bag) {
                Attributes a;
                if (bag.size() < 3 || !bag[2].is(asn1::type::set)) {
                    return a;
                }
                for (const asn1 attr : bag[2]) {
                    if (!attr.is(asn1::type::sequence) || attr.size() != 2 || !attr[1].is(asn1::type::set) || attr[1].empty()) {
                        continue;
                    }
                    auto t = attr[0].as_oid();
                    if (t == optional<oid>(friendly_name)) {
                        if (auto s = attr[1][0].as_string()) {
                            a.friendly_name = *s;
                        }
                    } else if (t == optional<oid>(local_key_id)) {
                        if (auto b = attr[1][0].as_bytes()) {
                            const auto* p = reinterpret_cast<const uint8_t*>(b->data());
                            a.local_key_id.assign(p, p + b->size());
                        }
                    }
                }
                return a;
            }

            // What a file's bags gave: the key (decrypted), the certificates
            struct Found {
                std::unique_ptr<Pkcs12Key> key;
                Attributes key_attributes;
                vector<pair<x509::certificate, Attributes>> certificates;
            };

            inline expected<void, error> read_bags(const asn1& safe, const slice<const byte>& password, uint32_t max_iterations, Found& f) {
                if (!safe.is(asn1::type::sequence)) {
                    return unexpected(bad("SafeContents that are not a SEQUENCE"));
                }
                for (const asn1 bag : safe) {
                    if (!bag.is(asn1::type::sequence) || bag.size() < 2 || bag.size() > 3 || !bag[1].is_context(0) || bag[1].size() != 1) {
                        return unexpected(bad("a SafeBag that does not read"));
                    }
                    auto id = bag[0].as_oid();
                    const asn1 value = bag[1][0];
                    if (id == optional<oid>(cert_bag)) {
                        if (!value.is(asn1::type::sequence) || value.size() != 2 || !value[1].is_context(0) || value[1].size() != 1) {
                            return unexpected(bad("a CertBag that does not read"));
                        }
                        if (value[0].as_oid() != optional<oid>(x509_certificate)) {
                            continue;   // an SDSI certificate: passed over
                        }
                        auto der = value[1][0].as_bytes();
                        if (!der || !value[1][0].is(asn1::type::octet_string)) {
                            return unexpected(bad("a CertBag that does not read"));
                        }
                        auto c = x509::certificate::parse(*der);
                        if (!c) {
                            return unexpected(error(errc::malformed, string("sgcl::crypto::pkcs12: a certificate that does not parse: ") + c.error().message()));
                        }
                        f.certificates.emplace_back(std::move(*c), attributes_of(bag));
                    } else if (id == optional<oid>(shrouded_key_bag) || id == optional<oid>(key_bag)) {
                        if (f.key) {
                            continue;   // the first key is the file's
                        }
                        auto key = std::make_unique<Pkcs12Key>();
                        if (id == optional<oid>(key_bag)) {
                            if (!key->read(value.bytes())) {
                                return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a private key of a kind the module does not have")));
                            }
                        } else {
                            if (!value.is(asn1::type::sequence) || value.size() != 2 || !value[1].is(asn1::type::octet_string)) {
                                return unexpected(bad("an EncryptedPrivateKeyInfo that does not read"));
                            }
                            auto plain = pbes2_decrypt(value[0], *value[1].as_bytes(), password, max_iterations);
                            if (!plain) {
                                return unexpected(plain.error());
                            }
                            if (!key->read(plain->as_slice())) {
                                return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a private key of a kind the module does not have")));
                            }
                        }
                        f.key = std::move(key);
                        f.key_attributes = attributes_of(bag);
                    }
                    // CRL, secret and nested bags: passed over
                }
                return {};
            }

            // The certificates in the chain's order: the leaf (the key's by
            // its localKeyId, else by its public key, else the first), then
            // each one's issuer the file holds, then the rest as they came
            inline x509::chain order(Found& f) {
                x509::chain out;
                const size_t n = f.certificates.size();
                if (n == 0) {
                    return out;
                }
                std::vector<bool> used(n, false);
                size_t leaf = n;
                if (f.key && !f.key_attributes.local_key_id.empty()) {
                    for (size_t i = 0; i < n && leaf == n; ++i) {
                        const auto& id = f.certificates[i].second.local_key_id;
                        if (id.size() == f.key_attributes.local_key_id.size() &&
                            std::memcmp(id.data(), f.key_attributes.local_key_id.data(), id.size()) == 0) {
                            leaf = i;
                        }
                    }
                }
                if (f.key && leaf == n) {
                    auto spki = f.key->signer().public_key_der();
                    for (size_t i = 0; i < n && leaf == n; ++i) {
                        auto c = f.certificates[i].first.raw_subject_public_key_info();
                        if (c.size() == spki.size() && std::memcmp(c.data(), spki.data(), spki.size()) == 0) {
                            leaf = i;
                        }
                    }
                }
                if (leaf == n) {
                    leaf = 0;
                }
                out.push_back(f.certificates[leaf].first);
                used[leaf] = true;
                for (size_t step = 1; step < n; ++step) {
                    const auto issuer = out.back().raw_issuer();
                    size_t next = n;
                    for (size_t i = 0; i < n && next == n; ++i) {
                        if (used[i]) {
                            continue;
                        }
                        auto subject = f.certificates[i].first.raw_subject();
                        if (subject.size() == issuer.size() && std::memcmp(subject.data(), issuer.data(), issuer.size()) == 0) {
                            next = i;
                        }
                    }
                    if (next == n) {
                        break;
                    }
                    out.push_back(f.certificates[next].first);
                    used[next] = true;
                }
                for (size_t i = 0; i < n; ++i) {
                    if (!used[i]) {
                        out.push_back(f.certificates[i].first);
                    }
                }
                return out;
            }
        }

        struct Pkcs12Access {
            static pkcs12 make(tracked_ptr<Pkcs12State> s) noexcept;
        };
    }

    // A PKCS #12 file read: its private key, the certificate chain (the
    // leaf first) and the leaf's friendly name. A handle of one word; the
    // key lies in plain memory of its own, shared by copies, zeroed when the
    // last one's state is collected
    class pkcs12 {
    public:
        // What a file is written and read with
        struct options {
            string friendly_name;                   // encode: the leaf's and the key's friendlyName; empty: none
            uint32_t iterations = 2048;             // encode: PBKDF2's and the MAC's iterations, OpenSSL 3's default
            uint32_t max_iterations = 1000000;      // parse: more in any of the file's derivations is errc::unsupported
        };

        // The contents of a file (DER or BER) under its password: the MAC
        // checked first (errc::authentication for a wrong password), then
        // every bag decrypted. errc::malformed for bytes that do not read,
        // errc::unsupported for what the module does not have (a legacy PBE,
        // a public-key integrity mode, a key of another kind), named in the
        // error. A file of certificates alone has no key
        static expected<pkcs12, error> parse(const slice<const byte>& file, const slice<const byte>& password) noexcept {
            return parse(file, password, options());
        }

        static expected<pkcs12, error> parse(const slice<const byte>& file, const slice<const byte>& password, const options& o) noexcept {
            try {
                return _parse(file, password, o);
            } catch (const std::bad_alloc&) {
                throw;
            } catch (...) {
                return unexpected(detail::p12::bad("bytes that do not read"));
            }
        }

        // A file of the key and its chain (the leaf first, the key's;
        // std::invalid_argument for a leaf of another key), written as
        // OpenSSL 3 writes one: the certificates and the key under PBES2
        // (PBKDF2-HMAC-SHA-256, AES-256-CBC), a MAC of HMAC-SHA-256, the
        // leaf and the key tied by a localKeyId. The key is the module's of
        // any kind it signs with (p256/p384/p521::private_key,
        // ed25519::private_key, rsa::private_key, through x509::signing_key)
        static vector<byte> encode(const x509::signing_key& key, const x509::chain& chain, const slice<const byte>& password) {
            return encode(key, chain, password, options());
        }

        static vector<byte> encode(const x509::signing_key& key, const x509::chain& chain, const slice<const byte>& password, const options& o);

        // The chain, the leaf first; empty for a file without certificates
        const x509::chain& certificates() const noexcept {
            return _s->certificates;
        }

        // The friendlyName of the key's bag, else of the leaf's; empty when none
        string friendly_name() const noexcept {
            return _s->friendly_name;
        }

        // The kind of the key; key_kind::none for a file without one
        x509::key_kind key_kind() const noexcept {
            return _s->key ? _s->key->kind : x509::key_kind::none;
        }

        // The key as the module signs with it (x509::create_certificate,
        // cms::sign): a view, valid while this handle or a copy lives.
        // std::logic_error for a file without a key
        x509::signing_key signing_key() const {
            if (!_s->key) {
                throw logic_error("sgcl::crypto::pkcs12: the file holds no private key");
            }
            return _s->key->signer();
        }

        // The key's PKCS #8 PrivateKeyInfo, in plain memory: what each kind's
        // from_pkcs8_der reads (p256::private_key::from_pkcs8_der(p.key_pkcs8())).
        // std::logic_error for a file without a key
        secret_bytes key_pkcs8() const {
            if (!_s->key) {
                throw logic_error("sgcl::crypto::pkcs12: the file holds no private key");
            }
            return _s->key->pkcs8.clone();
        }

    private:
        friend struct detail::Pkcs12Access;
        tracked_ptr<detail::Pkcs12State> _s;

        explicit pkcs12(tracked_ptr<detail::Pkcs12State> s) noexcept
        : _s(std::move(s)) {
        }

        static expected<pkcs12, error> _parse(const slice<const byte>& file, const slice<const byte>& password, const options& o);
    };

    namespace detail {
        inline pkcs12 Pkcs12Access::make(tracked_ptr<Pkcs12State> s) noexcept {
            return pkcs12(std::move(s));
        }
    }

    inline expected<pkcs12, error> pkcs12::_parse(const slice<const byte>& file, const slice<const byte>& password, const options& o) {
        using namespace detail::p12;
        auto pfx_r = asn1::parse(file, asn1::ber);
        if (!pfx_r) {
            return unexpected(bad("bytes that are not BER"));
        }
        const asn1 pfx = *pfx_r;
        if (!pfx.is(asn1::type::sequence) || pfx.size() < 2 || pfx.size() > 3 || pfx[0].as_int() != optional<int64_t>(3)) {
            return unexpected(bad("not a PFX of version 3"));
        }
        const asn1 auth = pfx[1];
        if (!auth.is(asn1::type::sequence) || auth.size() != 2 || !auth[1].is_context(0) || auth[1].size() != 1) {
            return unexpected(bad("the authSafe does not read"));
        }
        if (auth[0].as_oid() != optional<oid>(data)) {
            return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: an authSafe that is not data (the public-key integrity mode)")));
        }
        auto auth_bytes = auth[1][0].as_bytes();
        if (!auth_bytes || !auth[1][0].is(asn1::type::octet_string)) {
            return unexpected(bad("the authSafe's content is not an OCTET STRING"));
        }
        // the MAC, over the authSafe's content
        if (pfx.size() == 3) {
            const asn1 mac = pfx[2];
            if (!mac.is(asn1::type::sequence) || mac.size() < 2 || mac.size() > 3 || !mac[0].is(asn1::type::sequence) || mac[0].size() != 2 ||
                !mac[0][0].is(asn1::type::sequence) || mac[0][0].size() < 1) {
                return unexpected(bad("the MacData does not read"));
            }
            auto digest_alg = mac[0][0][0].as_oid();
            auto given = mac[0][1].as_bytes();
            auto salt = mac[1].as_bytes();
            int64_t iterations = 1;
            if (mac.size() == 3) {
                auto it = mac[2].as_int();
                if (!it || *it < 1) {
                    return unexpected(bad("the MAC's iteration count does not read"));
                }
                iterations = *it;
            }
            if (!digest_alg || !given || !salt) {
                return unexpected(bad("the MacData does not read"));
            }
            secret_bytes tag;
            if (*digest_alg == pbmac1) {
                // RFC 9579: PBKDF2 of the password (UTF-8) keys an HMAC; the
                // MacData's salt and count are not used
                const asn1 params = mac[0][0].size() == 2 ? mac[0][0][1] : asn1();
                if (!params.is(asn1::type::sequence) || params.size() != 2) {
                    return unexpected(bad("PBMAC1's parameters do not read"));
                }
                auto kdf = read_pbkdf2(params[0], o.max_iterations);
                if (!kdf) {
                    return unexpected(kdf.error());
                }
                auto scheme = params[1].is(asn1::type::sequence) && params[1].size() >= 1 ? params[1][0].as_oid() : nullopt;
                auto h = scheme ? digest_of(*scheme) : nullopt;
                if (!h || *scheme == d_sha1 || *scheme == d_sha256 || *scheme == d_sha224 || *scheme == d_sha384 || *scheme == d_sha512 || kdf->key_length == 0) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a PBMAC1 scheme the module does not have")));
                }
                auto key = pbkdf2_of(kdf->prf, password, kdf->salt, kdf->iterations, kdf->key_length);
                tag = hmac_of(*h, key.as_slice(), *auth_bytes);
            } else {
                auto h = digest_of(*digest_alg);
                if (!h || *digest_alg != (*h == hash_id::sha1 ? d_sha1 : *h == hash_id::sha224 ? d_sha224 : *h == hash_id::sha256 ? d_sha256 : *h == hash_id::sha384 ? d_sha384 : d_sha512)) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a MAC digest the module does not have: ") + digest_alg->to_string()));
                }
                if (uint64_t(iterations) > o.max_iterations) {
                    return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: more iterations than options::max_iterations")));
                }
                auto bmp = bmp_password(password);
                auto key = kdf_of(*h, 3, bmp.as_slice(), *salt, uint32_t(iterations), digest_size(*h));
                tag = hmac_of(*h, key.as_slice(), *auth_bytes);
            }
            if (tag.size() != given->size() || !constant_time::equal(tag.as_slice(), *given)) {
                return unexpected(error(errc::authentication, string("sgcl::crypto::pkcs12: the MAC does not verify (a wrong password?)")));
            }
        }
        auto safe_r = asn1::parse(*auth_bytes, asn1::ber);
        if (!safe_r || !safe_r->is(asn1::type::sequence)) {
            return unexpected(bad("the AuthenticatedSafe does not read"));
        }
        Found f;
        for (const asn1 ci : *safe_r) {
            if (!ci.is(asn1::type::sequence) || ci.size() != 2 || !ci[1].is_context(0) || ci[1].size() != 1) {
                return unexpected(bad("a ContentInfo that does not read"));
            }
            auto type = ci[0].as_oid();
            if (type == optional<oid>(data)) {
                auto content = ci[1][0].as_bytes();
                if (!content) {
                    return unexpected(bad("a data ContentInfo that does not read"));
                }
                auto safe = asn1::parse(*content, asn1::ber);
                if (!safe) {
                    return unexpected(bad("SafeContents that do not read"));
                }
                if (auto r = read_bags(*safe, password, o.max_iterations, f); !r) {
                    return unexpected(r.error());
                }
            } else if (type == optional<oid>(encrypted_data)) {
                const asn1 ed = ci[1][0];
                if (!ed.is(asn1::type::sequence) || ed.size() < 2 || !ed[1].is(asn1::type::sequence) || ed[1].size() != 3) {
                    return unexpected(bad("an EncryptedData that does not read"));
                }
                const asn1 eci = ed[1];
                auto ct = eci[2].is_context(0) ? eci[2].as_bytes() : nullopt;
                if (!ct) {
                    return unexpected(bad("an EncryptedContentInfo without its content"));
                }
                auto plain = pbes2_decrypt(eci[1], *ct, password, o.max_iterations);
                if (!plain) {
                    return unexpected(plain.error());
                }
                // certificates mostly; a key bag here is read from the
                // decrypted bytes where they lie (DER) and copied to plain memory
                auto safe = asn1::parse(plain->as_slice(), asn1::ber);
                if (!safe) {
                    return unexpected(bad("decrypted SafeContents that do not read"));
                }
                if (auto r = read_bags(*safe, password, o.max_iterations, f); !r) {
                    return unexpected(r.error());
                }
            } else {
                return unexpected(error(errc::unsupported, string("sgcl::crypto::pkcs12: a ContentInfo of a type the module does not read (enveloped data)")));
            }
        }
        auto s = make_tracked<detail::Pkcs12State>();
        s->certificates = order(f);
        s->friendly_name = f.key_attributes.friendly_name;
        if (s->friendly_name.empty()) {
            for (const auto& c : f.certificates) {
                const auto x = c.first.raw();
                const auto y = s->certificates.empty() ? slice<const byte>() : s->certificates[0].raw();
                if (!y.empty() && x.size() == y.size() && std::memcmp(x.data(), y.data(), x.size()) == 0) {
                    s->friendly_name = c.second.friendly_name;
                }
            }
        }
        s->key = std::move(f.key);
        return pkcs12(std::move(s));
    }

    inline vector<byte> pkcs12::encode(const x509::signing_key& key, const x509::chain& chain, const slice<const byte>& password, const options& o) {
        using namespace detail::p12;
        if (o.iterations == 0) {
            throw std::invalid_argument("sgcl::crypto::pkcs12::encode: iterations of 0");
        }
        if (key.kind() == x509::key_kind::none) {
            throw std::invalid_argument("sgcl::crypto::pkcs12::encode: no key");
        }
        if (!chain.empty()) {
            auto spki = key.public_key_der();
            auto leaf = chain[0].raw_subject_public_key_info();
            if (spki.size() != leaf.size() || std::memcmp(spki.data(), leaf.data(), spki.size()) != 0) {
                throw std::invalid_argument("sgcl::crypto::pkcs12::encode: the leaf is not the key's certificate");
            }
        }
        // the leaf and the key tied by a localKeyId, the SHA-1 of the leaf as OpenSSL makes it
        vector<asn1> attrs;
        if (!chain.empty()) {
            auto id = crypto::sha1::of(chain[0].raw());
            attrs.push_back(asn1::sequence({asn1::object_identifier(local_key_id), asn1::set({asn1::octet_string(slice<const byte>(id.data(), id.size()))})}));
        }
        if (!o.friendly_name.empty()) {
            attrs.push_back(asn1::sequence({asn1::object_identifier(detail::p12::friendly_name), asn1::set({asn1::bmp_string(o.friendly_name)})}));
        }
        const asn1 attr_set = attrs.empty() ? asn1() : asn1::set(attrs);
        // the certificates, encrypted
        vector<asn1> cert_bags;
        for (size_t i = 0; i < chain.size(); ++i) {
            cert_bags.push_back(asn1::sequence({asn1::object_identifier(cert_bag),
                                                asn1::explicit_tag(0, asn1::sequence({asn1::object_identifier(x509_certificate),
                                                                                      asn1::explicit_tag(0, asn1::octet_string(chain[i].raw()))})),
                                                i == 0 ? attr_set : asn1()}));
        }
        vector<asn1> contents;
        if (!cert_bags.empty()) {
            const asn1 certs = asn1::sequence(cert_bags);
            auto [alg, ct] = pbes2_encrypt(certs.bytes(), password, o.iterations);
            contents.push_back(asn1::sequence(
                {asn1::object_identifier(encrypted_data),
                 asn1::explicit_tag(0, asn1::sequence({asn1::integer(0), asn1::sequence({asn1::object_identifier(data), alg, asn1::implicit_tag(0, asn1::octet_string(ct))})}))}));
        }
        // the key, shrouded
        {
            auto pkcs8 = x509::detail::SignerAccess::pkcs8(key);
            auto [alg, ct] = pbes2_encrypt(pkcs8.as_slice(), password, o.iterations);
            const asn1 bag = asn1::sequence({asn1::object_identifier(shrouded_key_bag), asn1::explicit_tag(0, asn1::sequence({alg, asn1::octet_string(ct)})), attr_set});
            contents.push_back(asn1::sequence({asn1::object_identifier(data), asn1::explicit_tag(0, asn1::octet_string(asn1::sequence({bag}).bytes()))}));
        }
        const asn1 auth_safe = asn1::sequence(contents);
        const auto auth_bytes = auth_safe.bytes();
        // the MAC: HMAC-SHA-256 under the key of Appendix B (id 3)
        auto salt = random::bytes(8);
        auto bmp = bmp_password(password);
        auto mac_key = kdf<crypto::sha256>(3, bmp.as_slice(), salt.as_slice(), o.iterations, 32);
        auto tag = hmac<crypto::sha256>::of(auth_bytes, mac_key.as_slice());
        const asn1 pfx = asn1::sequence(
            {asn1::integer(3), asn1::sequence({asn1::object_identifier(data), asn1::explicit_tag(0, asn1::octet_string(auth_bytes))}),
             asn1::sequence({asn1::sequence({asn1::sequence({asn1::object_identifier(d_sha256), asn1::null()}), asn1::octet_string(slice<const byte>(tag.data(), tag.size()))}),
                             asn1::octet_string(salt.as_slice()), asn1::integer(o.iterations)})});
        auto b = pfx.bytes();
        return vector<byte>(b.data(), b.data() + b.size());
    }
}
