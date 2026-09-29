//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "ec_field.h"
#include "../constant_time.h"
#include "../hash_id.h"
#include "../secure_zero.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// The encodings of RSA's messages (RFC 8017): EMSA-PKCS1-v1_5 and EMSA-PSS
// for signatures (§9.2, §9.1), EME-OAEP for encryption (§7.1), with MGF1
// (§B.2.1) over the hash a hash_id names. They work on bytes: what goes
// through the modular arithmetic is their caller's.
//
// What is secret: nothing in the signature encodings (the digest is
// public, the salt is sent). In OAEP decoding the whole encoded message is
// (it comes out of the private operation), and so is which of its checks
// failed — the leading byte, the hash of the label, the separator:
// Manger's attack (CRYPTO 2001) recovers a plaintext from an oracle that
// tells a bad leading byte from the rest. oaep_decode reads every byte and
// folds every check into one mask, with no branch before the end.
namespace sgcl::crypto::detail::rsa_pad {
    inline slice<const byte> view(const unsigned char* p, size_t n) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    // The digest of the parts, in order, into out (digest_size(id) bytes);
    // the hasher's state is zeroed afterwards (what OAEP hashes is secret)
    template<size_t N>
    void hash_parts(hash_id id, unsigned char* out, const slice<const byte> (&parts)[N]) {
        visit_hash(id, [&](auto t) {
            typename decltype(t)::type h;
            for (const auto& p : parts) {
                h.update(p);
            }
            auto d = h.value();
            std::memcpy(out, d.data(), d.size());
            secure_zero(d.data(), d.size());
            secure_zero_object(h);
        });
    }

    // MGF1 (RFC 8017 §B.2.1) of the seed, len bytes of it XORed into out:
    // Hash(seed || C) for the counters C = 0, 1, 2... as four bytes
    inline void mgf1_xor(hash_id id, const unsigned char* seed, size_t seed_len, unsigned char* out, size_t len) {
        visit_hash(id, [&](auto t) {
            using H = typename decltype(t)::type;
            unsigned char counter[4];
            for (uint32_t c = 0, done = 0; done < len; ++c) {
                store_be32(counter, c);
                H h;
                h.update(view(seed, seed_len));
                h.update(view(counter, 4));
                auto d = h.value();
                size_t n = len - done < H::digest_size ? len - done : H::digest_size;
                for (size_t i = 0; i < n; ++i) {
                    out[done + i] ^= static_cast<unsigned char>(d[i]);
                }
                done += uint32_t(n);
                secure_zero(d.data(), d.size());
                secure_zero_object(h);
            }
        });
    }

    // The body of the DER OBJECT IDENTIFIER of a digest: NIST's hash
    // algorithms 2.16.840.1.101.3.4.2.x, SHA-1 1.3.14.3.2.26
    inline size_t digest_oid(hash_id id, unsigned char* out) {
        static constexpr unsigned char nist[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02};
        unsigned char last = 0;
        switch (id) {
            case hash_id::sha1: {
                static constexpr unsigned char sha1[] = {0x2b, 0x0e, 0x03, 0x02, 0x1a};
                std::memcpy(out, sha1, sizeof sha1);
                return sizeof sha1;
            }
            case hash_id::sha256: last = 0x01; break;
            case hash_id::sha384: last = 0x02; break;
            case hash_id::sha512: last = 0x03; break;
            case hash_id::sha224: last = 0x04; break;
            case hash_id::sha512_256: last = 0x06; break;
            case hash_id::sha3_224: last = 0x07; break;
            case hash_id::sha3_256: last = 0x08; break;
            case hash_id::sha3_384: last = 0x09; break;
            case hash_id::sha3_512: last = 0x0a; break;
            default: throw invalid_argument("sgcl::crypto: unknown hash_id");
        }
        std::memcpy(out, nist, sizeof nist);
        out[sizeof nist] = last;
        return sizeof nist + 1;
    }

    // DigestInfo ::= SEQUENCE { SEQUENCE { OID, NULL }, OCTET STRING } of
    // the digest, T of RFC 8017 §9.2 step 2 (every entry of its note 1 has
    // the NULL, and so has every signer met in practice); its length
    inline size_t digest_info(hash_id id, const unsigned char* digest, size_t hlen, unsigned char* out) {
        unsigned char oid[16];
        size_t olen = digest_oid(id, oid);
        size_t alg = 2 + olen + 2;
        size_t i = 0;
        out[i++] = 0x30;
        out[i++] = static_cast<unsigned char>(2 + alg + 2 + hlen);
        out[i++] = 0x30;
        out[i++] = static_cast<unsigned char>(alg);
        out[i++] = 0x06;
        out[i++] = static_cast<unsigned char>(olen);
        std::memcpy(out + i, oid, olen);
        i += olen;
        out[i++] = 0x05;
        out[i++] = 0x00;
        out[i++] = 0x04;
        out[i++] = static_cast<unsigned char>(hlen);
        std::memcpy(out + i, digest, hlen);
        return i + hlen;
    }

    // The longest DigestInfo: SHA-512's, 19 + 64 bytes
    inline constexpr size_t max_digest_info = 96;

    // EMSA-PKCS1-v1_5 (RFC 8017 §9.2): 00 01 FF...FF 00 || T in em_len
    // bytes; false when T and the eleven bytes around it do not fit
    inline bool pkcs1_encode(hash_id id, const unsigned char* digest, size_t hlen, unsigned char* em, size_t em_len) {
        unsigned char t[max_digest_info];
        size_t tlen = digest_info(id, digest, hlen, t);
        if (em_len < tlen + 11) {
            return false;
        }
        em[0] = 0x00;
        em[1] = 0x01;
        std::memset(em + 2, 0xff, em_len - tlen - 3);
        em[em_len - tlen - 1] = 0x00;
        std::memcpy(em + em_len - tlen, t, tlen);
        return true;
    }

    // EMSA-PSS-ENCODE (RFC 8017 §9.1.1) of the digest with the salt, the
    // message em_bits long (the modulus's bits less one) in
    // ceil(em_bits / 8) bytes; MGF1 over the same hash, as Go and OpenSSL
    // by default. False when the salt and the digest do not fit
    inline bool pss_encode(hash_id id, const unsigned char* digest, size_t hlen, const unsigned char* salt, size_t slen, unsigned char* em, size_t em_bits) {
        size_t em_len = (em_bits + 7) / 8;
        if (em_len < hlen + slen + 2) {
            return false;
        }
        static constexpr unsigned char zeros[8] = {};
        size_t db_len = em_len - hlen - 1;
        unsigned char* h = em + db_len;
        const slice<const byte> parts[] = {view(zeros, 8), view(digest, hlen), view(salt, slen)};
        hash_parts(id, h, parts);
        // DB = PS || 01 || salt, masked in place
        std::memset(em, 0, db_len - slen - 1);
        em[db_len - slen - 1] = 0x01;
        std::memcpy(em + db_len - slen, salt, slen);
        mgf1_xor(id, h, hlen, em, db_len);
        em[0] &= static_cast<unsigned char>(0xff >> (8 * em_len - em_bits));
        em[em_len - 1] = 0xbc;
        return true;
    }

    // The steps of EMSA-PSS-VERIFY (RFC 8017 §9.1.2) before the salt: the
    // trailer BC, the bits above em_bits clear, DB unmasked in place (its
    // length in db_len, H at em + db_len). False when em cannot be one
    inline bool pss_unmask(hash_id id, size_t hlen, unsigned char* em, size_t em_bits, size_t& db_len) {
        size_t em_len = (em_bits + 7) / 8;
        if (em_len < hlen + 2 || em[em_len - 1] != 0xbc) {
            return false;
        }
        db_len = em_len - hlen - 1;
        const unsigned char* h = em + db_len;
        unsigned char top = static_cast<unsigned char>(0xff << (8 - (8 * em_len - em_bits)));
        if (8 * em_len != em_bits && (em[0] & top) != 0) {
            return false;
        }
        mgf1_xor(id, h, hlen, em, db_len);
        em[0] &= static_cast<unsigned char>(0xff >> (8 * em_len - em_bits));
        return true;
    }

    // The last step: H against the hash of 00 x 8 || mHash || salt
    inline bool pss_check(hash_id id, const unsigned char* digest, size_t hlen, const unsigned char* h, const unsigned char* salt, size_t slen) {
        static constexpr unsigned char zeros[8] = {};
        unsigned char want[64];
        const slice<const byte> parts[] = {view(zeros, 8), view(digest, hlen), view(salt, slen)};
        hash_parts(id, want, parts);
        return equal_bytes(want, h, hlen);
    }

    // EMSA-PSS-VERIFY (RFC 8017 §9.1.2) with the salt's length read from
    // the message, as Go's PSSSaltLengthAuto: the first byte of DB that is
    // not zero must be the 01 before the salt, and every length from 0 up
    // is taken. em is em_len = ceil(em_bits / 8) bytes and is unmasked in
    // place. Nothing here is secret
    inline bool pss_verify(hash_id id, const unsigned char* digest, size_t hlen, unsigned char* em, size_t em_bits) {
        size_t db_len = 0;
        if (!pss_unmask(id, hlen, em, em_bits, db_len)) {
            return false;
        }
        size_t i = 0;
        while (i < db_len && em[i] == 0) {
            ++i;
        }
        if (i == db_len || em[i] != 0x01) {
            return false;
        }
        return pss_check(id, digest, hlen, em + db_len, em + i + 1, db_len - i - 1);
    }

    // EMSA-PSS-VERIFY as RFC 8017 §9.1.2 writes it, the salt's length
    // given (sLen): DB must be emLen - hLen - sLen - 2 zeros, then 01, then
    // a salt of exactly salt_len bytes; a length that leaves no room is
    // false. What a certificate's RSASSA-PSS-params name (Go's
    // PSSSaltLengthEqualsHash, OpenSSL's check of the parameters)
    inline bool pss_verify(hash_id id, const unsigned char* digest, size_t hlen, unsigned char* em, size_t em_bits, size_t salt_len) {
        size_t em_len = (em_bits + 7) / 8;
        if (salt_len > em_len || em_len < hlen + salt_len + 2) {
            return false;
        }
        size_t db_len = 0;
        if (!pss_unmask(id, hlen, em, em_bits, db_len)) {
            return false;
        }
        size_t ps = db_len - salt_len - 1;
        for (size_t i = 0; i < ps; ++i) {
            if (em[i] != 0) {
                return false;
            }
        }
        if (em[ps] != 0x01) {
            return false;
        }
        return pss_check(id, digest, hlen, em + db_len, em + ps + 1, salt_len);
    }

    // EME-OAEP encoding (RFC 8017 §7.1.1 step 2) of the message into k
    // bytes: 00 || maskedSeed || maskedDB, DB = lHash || PS || 01 || M.
    // The label's hash and the seed's length are id's, the masks MGF1 over
    // mgf. The message must fit (m_len <= k - 2 hLen - 2)
    inline void oaep_encode(hash_id id, hash_id mgf, const unsigned char* msg, size_t m_len, const unsigned char* label, size_t l_len, const unsigned char* seed, unsigned char* em, size_t k) {
        size_t hlen = digest_size(id);
        unsigned char* s = em + 1;
        unsigned char* db = em + 1 + hlen;
        size_t db_len = k - hlen - 1;
        em[0] = 0x00;
        const slice<const byte> parts[] = {view(label, l_len)};
        hash_parts(id, db, parts);
        std::memset(db + hlen, 0, db_len - hlen - m_len - 1);
        db[db_len - m_len - 1] = 0x01;
        std::memcpy(db + db_len - m_len, msg, m_len);
        std::memcpy(s, seed, hlen);
        mgf1_xor(mgf, s, hlen, db, db_len);
        mgf1_xor(mgf, db, db_len, s, hlen);
    }

    // EME-OAEP decoding (RFC 8017 §7.1.2 step 3) of the k bytes of em, in
    // place, in constant time: every byte is read and every check — the
    // leading 00, the label's hash, the zeros and the 01 after it — is
    // folded into one mask with no branch on any of them. All ones when em
    // is an encoding, the message then at em + *offset to the end; the
    // offset is set either way (the caller branches on the mask alone). k
    // must be at least 2 hLen + 2
    inline uint64_t oaep_decode(hash_id id, hash_id mgf, unsigned char* em, size_t k, const unsigned char* label, size_t l_len, size_t* offset) {
        size_t hlen = digest_size(id);
        unsigned char* s = em + 1;
        unsigned char* db = em + 1 + hlen;
        size_t db_len = k - hlen - 1;
        unsigned char lhash[64];
        const slice<const byte> parts[] = {view(label, l_len)};
        hash_parts(id, lhash, parts);
        mgf1_xor(mgf, db, db_len, s, hlen);
        mgf1_xor(mgf, s, hlen, db, db_len);
        uint64_t good = ct_zero_mask(em[0]);
        good &= ct_bit_mask(uint64_t(equal_bytes(lhash, db, hlen)));
        // the first 01 after the zeros: looking stays all ones through the
        // zeros; a byte other than 00 and 01 while looking is invalid
        uint64_t looking = ~uint64_t(0);
        uint64_t index = 0;
        uint64_t invalid = 0;
        for (size_t i = hlen; i < db_len; ++i) {
            uint64_t is1 = ct_eq_mask(db[i], 1);
            uint64_t is0 = ct_zero_mask(db[i]);
            index = (index & ~(looking & is1)) | (uint64_t(i) & looking & is1);
            invalid |= looking & ~is1 & ~is0;
            looking &= ~is1;
        }
        good &= ~invalid & ~looking;
        *offset = size_t(1 + hlen + index + 1);
        secure_zero(lhash, sizeof lhash);
        return ct_barrier(good);
    }
}
