//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/der.h"
#include "detail/keys.h"
#include "detail/key_pem.h"
#include "detail/rsa_keygen.h"
#include "detail/rsa_math.h"
#include "detail/rsa_padding.h"
#include "error.h"
#include "hash_id.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// RSA (RFC 8017), Go's crypto/rsa: signatures in PKCS #1 v1.5 and PSS,
// encryption in OAEP, keys of 2048 bits and more made here, keys of 1024
// bits and more read from PKCS #1, PKCS #8 and SubjectPublicKeyInfo.
//
//   auto key = crypto::rsa::private_key::generate(3072);
//   auto sig = key.sign_digest_pss(crypto::hash_id::sha256, crypto::sha256::of(message));
//   bool ok = key.public_key().verify_digest_pss(crypto::hash_id::sha256, crypto::sha256::of(message), sig);
//
// A private key is move-only and zeroes its words when it goes; a public
// key is a plain value. PKCS #1 v1.5 decryption is not here and will not
// be (Bleichenbacher's attack, CRYPTO 1998, and its descendants): OAEP is
// the one encryption.
//
// What is secret: d, p, q, dP, dQ, qInv, the blinding factor, what the
// private operation takes in and gives out while it is blinded, and in
// decryption the whole encoded message. They go only through the
// constant-time arithmetic of detail/rsa_math.h (Montgomery products of a
// width fixed by the modulus, exponentiation by windows read through
// masks) and are zeroed when their scope ends. The private operation is
// CRT over p and q on a blinded input (c r^e, unblinded by r^-1 after),
// and its result is checked with the public exponent before it leaves (a
// fault in one half of CRT would otherwise give the factors away: Boneh,
// DeMillo and Lipton, 1997). A public key, a signature, a digest, a
// ciphertext and the parse of any encoding are not secret. The
// implementation has not been through an independent cryptographic audit.
namespace sgcl::crypto::detail {
    // rsaEncryption, 1.2.840.113549.1.1.1 (RFC 8017 §A.1), and
    // id-RSASSA-PSS, 1.2.840.113549.1.1.10 (a key only for PSS, which is
    // not read)
    inline constexpr unsigned char oid_rsa_encryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    inline constexpr unsigned char oid_rsassa_pss[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a};

    struct Rsa {
        static constexpr size_t min_bits = 1024;           // read
        static constexpr size_t min_generate_bits = 2048;  // made
        static constexpr size_t max_bits = 16384;
        static constexpr uint64_t max_exponent = (uint64_t(1) << 31) - 1;
        static constexpr size_t der_capacity = 12288;      // a key of max_bits in PKCS #8

        static error key_error(errc code, const char* what) {
            return error(code, string(std::string("sgcl::crypto::rsa: ") + what));
        }

        static error der_error(errc code, size_t offset, const char* what) {
            return error(code, uint64_t(offset), string(std::string("sgcl::crypto::rsa: ") + what));
        }

        // The error of every failed decryption, whatever failed
        static error decryption_error() {
            return error(errc::authentication, string("sgcl::crypto::rsa: decryption error"));
        }

        template<class W>
        static vector<byte> take(const W& w) {
            const byte* p = reinterpret_cast<const byte*>(w.data());
            return vector<byte>(p, p + w.size());
        }

        // An INTEGER of k words
        template<class W>
        static void put_words(W& w, const uint64_t* a, size_t k, unsigned char* tmp) noexcept {
            bn::to_be(tmp, 8 * k, a, k);
            w.put_unsigned(tmp, 8 * k);
            secure_zero(tmp, 8 * k);
        }

        // The AlgorithmIdentifier { rsaEncryption, NULL }
        template<class W>
        static void put_algorithm(W& w) noexcept {
            size_t mark = w.size();
            w.put(0x00);
            w.put(der::null);
            size_t m = w.size();
            w.put(oid_rsa_encryption, sizeof oid_rsa_encryption);
            w.wrap(der::object_identifier, m);
            w.wrap(der::sequence, mark);
        }

        // The AlgorithmIdentifier of an RSA key: rsaEncryption with NULL
        // parameters, as Go reads it (an RSASSA-PSS key, or another
        // algorithm, is unsupported; parameters other than NULL malformed)
        static expected<void, error> read_algorithm(DerReader& in) {
            DerReader alg;
            size_t at = in.offset();
            if (!in.read(der::sequence, alg)) {
                return unexpected<error>(der_error(errc::malformed, at, "DER: an AlgorithmIdentifier is a SEQUENCE"));
            }
            if (alg.read_exact(der::object_identifier, oid_rsassa_pss, sizeof oid_rsassa_pss)) {
                return unexpected<error>(der_error(errc::unsupported, at, "an RSASSA-PSS key (only rsaEncryption keys are read)"));
            }
            if (!alg.read_exact(der::object_identifier, oid_rsa_encryption, sizeof oid_rsa_encryption)) {
                return unexpected<error>(der_error(errc::unsupported, alg.offset(), "not an RSA key"));
            }
            static constexpr unsigned char nothing[1] = {};
            if (!alg.read_exact(der::null, nothing, 0) || !alg.empty()) {
                return unexpected<error>(der_error(errc::malformed, alg.offset(), "DER: an RSA key's parameters are NULL"));
            }
            return {};
        }
    };

    template<class Release>
    class RsaPrivateKey;

    // An RSA public key: the modulus n and the exponent e, checked when it
    // is made (n odd, of 1024 to 16384 bits; e odd, 3 to 2^31 - 1). A plain
    // value: copied and compared freely. It verifies signatures and
    // encrypts
    class RsaPublicKey {
        template<class> friend class RsaPrivateKey;
        friend struct RsaAccess;
        using word = bn::word;

    public:
        // A key from its modulus (big-endian bytes, leading zeros allowed)
        // and exponent, as a JWK or a program's own format gives them.
        // errc::invalid_key for an even modulus or an exponent that is
        // even or below 3; errc::unsupported for a modulus of fewer than
        // 1024 bits or more than 16384, an exponent above 2^31 - 1
        static expected<RsaPublicKey, error> from_modulus(const slice<const byte>& n, uint64_t e) {
            return _make(detail::bytes(n.data()), n.size(), e, 0);
        }

        // An RSAPublicKey of PKCS #1 (RFC 8017 §A.1.1, "RSA PUBLIC KEY" in
        // PEM): SEQUENCE { modulus, publicExponent }
        static expected<RsaPublicKey, error> from_pkcs1_der(const slice<const byte>& der) {
            DerReader in(detail::bytes(der.data()), der.size());
            auto k = _read_pkcs1(in);
            if (k && !in.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, in.offset(), "DER: data after the key"));
            }
            return k;
        }

        // A SubjectPublicKeyInfo (RFC 5280 §4.1.2.7, RFC 3279 §2.3.1,
        // "PUBLIC KEY" in PEM) of an rsaEncryption key
        static expected<RsaPublicKey, error> from_pkix_der(const slice<const byte>& der) {
            DerReader in(detail::bytes(der.data()), der.size());
            DerReader seq;
            if (!in.read(der::sequence, seq) || !in.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, in.offset(), "DER: a SubjectPublicKeyInfo is one SEQUENCE"));
            }
            if (auto a = Rsa::read_algorithm(seq); !a) {
                return unexpected<error>(a.error());
            }
            DerReader bits;
            size_t at = seq.offset();
            if (!seq.read(der::bit_string, bits) || !seq.empty() || bits.size() < 1 || bits.data()[0] != 0) {
                return unexpected<error>(Rsa::der_error(errc::malformed, at, "DER: the key is a BIT STRING of whole bytes"));
            }
            DerReader body(bits.data() + 1, bits.size() - 1, bits.offset() + 1);
            auto k = _read_pkcs1(body);
            if (k && !body.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, body.offset(), "DER: data after the RSAPublicKey"));
            }
            return k;
        }

        // The modulus's bits: 2048 for a 2048-bit key
        size_t bits() const noexcept {
            return _bits;
        }

        // The modulus's bytes: the length of a signature and of a
        // ciphertext
        size_t size() const noexcept {
            return (_bits + 7) / 8;
        }

        // n, big-endian, size() bytes
        vector<byte> modulus() const {
            _check();
            vector<byte> out(size());
            bn::to_be(detail::bytes(out.data()), size(), _n.data(), _n.size());
            return out;
        }

        uint64_t exponent() const noexcept {
            return _e;
        }

        // The RSAPublicKey of PKCS #1, as Go's x509.MarshalPKCS1PublicKey
        // writes it
        vector<byte> to_pkcs1_der() const {
            _check();
            auto w = std::make_unique<DerWriter<Rsa::der_capacity>>();
            _write_pkcs1(*w);
            return Rsa::take(*w);
        }

        // The SubjectPublicKeyInfo, as Go's x509.MarshalPKIXPublicKey
        // writes it
        vector<byte> to_pkix_der() const {
            _check();
            auto w = std::make_unique<DerWriter<Rsa::der_capacity>>();
            _write_pkcs1(*w);
            w->put(0x00);
            w->wrap(der::bit_string, 0);
            Rsa::put_algorithm(*w);
            w->wrap(der::sequence, 0);
            return Rsa::take(*w);
        }

        // Whether signature is the PKCS #1 v1.5 signature (RFC 8017 §8.2)
        // of digest, made by the hash id names, under this key: the
        // signature is exactly size() bytes, below n, and the message it
        // opens to is, byte for byte, the one encoding of the digest
        // (the whole encoded message is compared, never parsed: no
        // parameters or trailing bytes a lax parser would let through).
        // A digest of another length than id's is false too: a
        // verification never throws, since in X.509 the hash comes with
        // the data (the signature's AlgorithmIdentifier), and an exception
        // there would be a certificate's way to stop a program.
        // [[nodiscard]]: a check whose result is dropped was never made
        [[nodiscard]] bool verify_digest(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature) const {
            _check();
            if (digest.size() != digest_size(id)) {
                return false;
            }
            size_t k = size();
            std::vector<unsigned char> em(k);
            if (!_open(signature, em.data())) {
                return false;
            }
            std::vector<unsigned char> want(k);
            if (!rsa_pad::pkcs1_encode(id, detail::bytes(digest.data()), digest.size(), want.data(), k)) {
                return false;
            }
            return equal_bytes(em.data(), want.data(), k);
        }

        // Whether signature is a PSS signature (RFC 8017 §8.1) of digest
        // under this key, MGF1 over the same hash as the digest, the salt
        // of any length (as Go's PSSSaltLengthAuto: what sign_digest_pss
        // makes, a salt as long as the digest, and every other length
        // verify). A digest of another length than id's is false, as for
        // verify_digest
        [[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature) const {
            return _verify_pss(id, digest, signature, nullptr);
        }

        // The same with the salt's length fixed (RFC 8017 §9.1.2 as
        // written, Go's PSSSaltLengthEqualsHash when salt_length is the
        // digest's): a signature with a salt of any other length is false,
        // as is a length the key has no room for. What a certificate's
        // RSASSA-PSS-params ask
        [[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature, size_t salt_length) const {
            return _verify_pss(id, digest, signature, &salt_length);
        }

        // message encrypted with RSAES-OAEP (RFC 8017 §7.1): the label's
        // hash and MGF1 both by id (Go's EncryptOAEP), an empty label.
        // size() bytes. The message is at most max_oaep_message_size(id)
        // bytes (size() - 2 hLen - 2: 190 for a 2048-bit key and SHA-256);
        // a longer one is std::invalid_argument. The message and the label
        // are bytes or text, which a slice of bytes takes both
        vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message) const {
            return _encrypt(id, id, message, slice<const byte>());
        }

        // The same with a label, which the decryption must be given
        vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message, const slice<const byte>& label) const {
            return _encrypt(id, id, message, label);
        }

        // The same with MGF1 over another hash than the label's (Java's
        // "OAEPWithSHA-256AndMGF1Padding" is SHA-256 with MGF1 over SHA-1)
        vector<byte> encrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& message, const slice<const byte>& label) const {
            return _encrypt(id, mgf1, message, label);
        }

        // The longest message encrypt_oaep takes under id: size() - 2 hLen
        // - 2, or 0 when the key is too small for the hash at all
        size_t max_oaep_message_size(hash_id id) const {
            size_t h = digest_size(id);
            return size() >= 2 * h + 2 ? size() - 2 * h - 2 : 0;
        }

        friend bool operator==(const RsaPublicKey& a, const RsaPublicKey& b) noexcept {
            return a._e == b._e && a._n == b._n;
        }

    private:
        std::vector<word> _n;    // kn words
        std::vector<word> _rr;   // R^2 mod n
        word _m0inv = 0;
        uint64_t _e = 0;
        size_t _bits = 0;

        RsaPublicKey() = default;

        void _check() const {
            if (_n.empty()) {
                moved_from("sgcl::crypto::rsa::public_key");
            }
        }

        bn::Modulus _mod() const noexcept {
            return bn::Modulus{_n.data(), _rr.data(), nullptr, _m0inv, _n.size()};
        }

        void _check_digest(hash_id id, const slice<const byte>& digest) const {
            _check();
            if (digest.size() != digest_size(id)) {
                throw invalid_argument("sgcl::crypto::rsa: the digest is not of the hash's length");
            }
        }

        static expected<RsaPublicKey, error> _make(const unsigned char* n, size_t len, uint64_t e, size_t offset) {
            while (len != 0 && *n == 0) {
                ++n;
                --len;
            }
            if (len > Rsa::max_bits / 8) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, offset, "a modulus of more than 16384 bits"));
            }
            size_t kn = bn::words_for_bytes(len);
            RsaPublicKey k;
            k._n.assign(kn, 0);
            if (kn != 0) {
                bn::from_be(k._n.data(), kn, n, len);
            }
            k._bits = bn::bit_length(k._n.data(), kn);
            if (k._bits < Rsa::min_bits) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, offset, "a modulus of fewer than 1024 bits"));
            }
            if (k._bits > Rsa::max_bits) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, offset, "a modulus of more than 16384 bits"));
            }
            if ((k._n[0] & 1) == 0) {
                return unexpected<error>(Rsa::der_error(errc::invalid_key, offset, "the modulus is even"));
            }
            if (e < 3 || (e & 1) == 0) {
                return unexpected<error>(Rsa::der_error(errc::invalid_key, offset, "the public exponent is not an odd number of at least 3"));
            }
            if (e > Rsa::max_exponent) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, offset, "a public exponent above 2^31 - 1"));
            }
            k._e = e;
            k._m0inv = bn::mont_m0inv(k._n[0]);
            k._rr.assign(kn, 0);
            std::vector<word> rrr(kn), scratch(bn::mul_scratch(kn) + kn);
            bn::mont_constants(k._rr.data(), rrr.data(), k._n.data(), kn, k._m0inv, scratch.data());
            return k;
        }

        // SEQUENCE { INTEGER n, INTEGER e } at the reader
        static expected<RsaPublicKey, error> _read_pkcs1(DerReader& in) {
            DerReader seq;
            size_t at = in.offset();
            if (!in.read(der::sequence, seq)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, at, "DER: an RSAPublicKey is a SEQUENCE"));
            }
            const unsigned char* n;
            size_t nlen;
            const unsigned char* e;
            size_t elen;
            size_t nat = seq.offset();
            if (!seq.read_unsigned_bytes(n, nlen)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, nat, "DER: the modulus is a non-negative INTEGER"));
            }
            size_t eat = seq.offset();
            if (!seq.read_unsigned_bytes(e, elen)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, eat, "DER: the public exponent is a non-negative INTEGER"));
            }
            if (!seq.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, seq.offset(), "DER: data after an RSAPublicKey's fields"));
            }
            if (elen > 8) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, eat, "a public exponent above 2^31 - 1"));
            }
            uint64_t ev = 0;
            for (size_t i = 0; i < elen; ++i) {
                ev = ev << 8 | e[i];
            }
            return _make(n, nlen, ev, nat);
        }

        // INTEGER n, INTEGER e, as the writer takes them (last first)
        template<class W>
        void _write_numbers(W& w) const {
            unsigned char eb[8];
            for (int i = 0; i < 8; ++i) {
                eb[i] = static_cast<unsigned char>(_e >> (56 - 8 * i));
            }
            w.put_unsigned(eb, 8);
            std::vector<unsigned char> tmp(8 * _n.size());
            Rsa::put_words(w, _n.data(), _n.size(), tmp.data());
        }

        template<class W>
        void _write_pkcs1(W& w) const {
            size_t mark = w.size();
            _write_numbers(w);
            w.wrap(der::sequence, mark);
        }

        // s^e mod n for s below n, into out (kn words). The words it works
        // in are zeroed when freed: under encrypt_oaep, s is the encoded
        // message, and m·R mod n would give it back from freed memory
        void _power(word* out, const word* s) const {
            size_t kn = _n.size();
            bn::Modulus mod = _mod();
            bn::SecretWords<> m(kn), scratch(bn::mul_scratch(kn) + 2 * kn);
            bn::to_mont(m.data(), s, mod, scratch.data());
            bn::mont_pow_public(m.data(), m.data(), _e, mod, scratch.data());
            bn::from_mont(out, m.data(), mod, scratch.data());
        }

        // The signature (size() bytes, below n) raised to e, as size()
        // bytes into em; false for a signature of another length or not
        // below n (RFC 8017 §8.2.2 step 1, §5.2.2 step 1)
        bool _open(const slice<const byte>& signature, unsigned char* em) const {
            size_t k = size();
            size_t kn = _n.size();
            if (signature.size() != k) {
                return false;
            }
            std::vector<word> s(kn), m(kn);
            bn::from_be(s.data(), kn, detail::bytes(signature.data()), k);
            if (bn::less_mask(s.data(), _n.data(), kn) == 0) {
                return false;
            }
            _power(m.data(), s.data());
            bn::to_be(em, k, m.data(), kn);
            return true;
        }

        // verify_digest_pss: the salt's length read from the signature
        // (salt_length null) or the one given
        bool _verify_pss(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature, const size_t* salt_length) const {
            _check();
            if (digest.size() != digest_size(id)) {
                return false;
            }
            size_t k = size();
            std::vector<unsigned char> em(k);
            if (!_open(signature, em.data())) {
                return false;
            }
            size_t em_bits = _bits - 1;
            size_t em_len = (em_bits + 7) / 8;
            // em_len is k or k - 1; in the second case the top byte is 0
            if (em_len < k && em[0] != 0) {
                return false;
            }
            const unsigned char* d = detail::bytes(digest.data());
            unsigned char* e = em.data() + (k - em_len);
            return salt_length ? rsa_pad::pss_verify(id, d, digest.size(), e, em_bits, *salt_length) : rsa_pad::pss_verify(id, d, digest.size(), e, em_bits);
        }

        vector<byte> _encrypt(hash_id id, hash_id mgf, const slice<const byte>& message, const slice<const byte>& label) const {
            unsigned char seed[64];
            random::fill(slice<byte>(reinterpret_cast<byte*>(seed), digest_size(id)));
            vector<byte> out = _encrypt(id, mgf, message, label, seed);
            secure_zero(seed, sizeof seed);
            return out;
        }

        // with the seed given (hLen bytes): the tests' known answers
        vector<byte> _encrypt(hash_id id, hash_id mgf, const slice<const byte>& message, const slice<const byte>& label, const unsigned char* seed) const {
            _check();
            size_t k = size();
            size_t h = digest_size(id);
            (void)digest_size(mgf);   // an unknown id is std::invalid_argument before anything else
            if (k < 2 * h + 2 || message.size() > k - 2 * h - 2) {
                throw invalid_argument("sgcl::crypto::rsa: a message too long for OAEP under this key and hash");
            }
            size_t kn = _n.size();
            bn::SecretWords<> buf(bn::words_for_bytes(k) + kn);
            unsigned char* em = reinterpret_cast<unsigned char*>(buf.data());
            word* m = buf.data() + bn::words_for_bytes(k);
            rsa_pad::oaep_encode(id, mgf, detail::bytes(message.data()), message.size(), detail::bytes(label.data()), label.size(), seed, em, k);
            bn::from_be(m, kn, em, k);   // below n: its top byte is 0
            std::vector<word> c(kn);
            _power(c.data(), m);
            vector<byte> out(k);
            bn::to_be(detail::bytes(out.data()), k, c.data(), kn);
            return out;
        }
    };

    // An RSA private key: the public key and d, p, q, dP, dQ, qInv, with
    // the Montgomery constants of p and q, in one block of words zeroed
    // when the key goes. Move-only: clone() copies by name, a move leaves
    // the source empty (a call on it is std::logic_error). Release is how
    // the zeroed words go back (a test's probe; the key of the API is
    // RsaPrivateKey<bn::OperatorDelete>)
    template<class Release>
    class RsaPrivateKey {
        using word = bn::word;
        using Words = bn::SecretWords<Release>;

    public:
        // A new key of bits bits (2048 to 16384; another number is
        // std::invalid_argument), e = 65537, from crypto::random. Two
        // primes of half the bits each; d = e^-1 mod (p - 1)(q - 1)
        static RsaPrivateKey generate(size_t bits) {
            if (bits < Rsa::min_generate_bits || bits > Rsa::max_bits) {
                throw invalid_argument("sgcl::crypto::rsa: a key of " + std::to_string(bits) + " bits (2048 to 16384 are made)");
            }
            for (;;) {
                auto k = _generate(bits);
                if (k) {
                    return std::move(*k);
                }
            }
        }

        // An RSAPrivateKey of PKCS #1 (RFC 8017 §A.1.2, "RSA PRIVATE KEY"
        // in PEM), two primes (version 0; a multi-prime key is
        // unsupported). The key is checked whole: n = p q, qInv q = 1 mod
        // p, dP = d mod (p - 1) and e dP = 1 mod (p - 1), and the same for
        // q; a key that fails is errc::invalid_key
        static expected<RsaPrivateKey, error> from_pkcs1_der(const slice<const byte>& der) {
            DerReader in(detail::bytes(der.data()), der.size());
            auto k = _read_pkcs1(in);
            if (k && !in.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, in.offset(), "DER: data after the key"));
            }
            return k;
        }

        // A PKCS #8 PrivateKeyInfo ("PRIVATE KEY" in PEM) of an
        // rsaEncryption key, or its version 2 (RFC 5958) with the public
        // key after it, which is skipped
        static expected<RsaPrivateKey, error> from_pkcs8_der(const slice<const byte>& der) {
            DerReader in(detail::bytes(der.data()), der.size());
            DerReader seq;
            if (!in.read(der::sequence, seq) || !in.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, in.offset(), "DER: a PKCS#8 key is one SEQUENCE"));
            }
            static constexpr unsigned char v0[] = {0x00};
            static constexpr unsigned char v1[] = {0x01};
            if (!seq.read_exact(der::integer, v0, 1) && !seq.read_exact(der::integer, v1, 1)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, seq.offset(), "DER: a PKCS#8 version is 0 or 1"));
            }
            if (auto a = Rsa::read_algorithm(seq); !a) {
                return unexpected<error>(a.error());
            }
            DerReader inner;
            size_t at = seq.offset();
            if (!seq.read(der::octet_string, inner)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, at, "DER: a PKCS#8 privateKey is an OCTET STRING"));
            }
            auto k = _read_pkcs1(inner);
            if (!k) {
                return k;
            }
            if (!inner.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, inner.offset(), "DER: data after the RSAPrivateKey"));
            }
            DerReader skip;
            if (seq.peek(der::context0)) {
                seq.read(der::context0, skip);
            }
            if (seq.peek(der::implicit1)) {
                seq.read(der::implicit1, skip);
            }
            if (!seq.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, seq.offset(), "DER: data after a PKCS#8 key's fields"));
            }
            return k;
        }

        RsaPrivateKey(RsaPrivateKey&&) noexcept = default;
        RsaPrivateKey& operator=(RsaPrivateKey&&) noexcept = default;
        RsaPrivateKey(const RsaPrivateKey&) = delete;
        RsaPrivateKey& operator=(const RsaPrivateKey&) = delete;
        ~RsaPrivateKey() = default;

        RsaPrivateKey clone() const {
            _check();
            Words s(_s.size());
            bn::copy(s.data(), _s.data(), _s.size());
            return RsaPrivateKey(RsaPublicKey(_pub), std::move(s), _k);
        }

        RsaPublicKey public_key() const {
            _check();
            return _pub;
        }

        size_t bits() const noexcept {
            return _pub._bits;
        }

        size_t size() const noexcept {
            return _pub.size();
        }

        // The PKCS #1 v1.5 signature (RFC 8017 §8.2.1) of a digest the
        // program made by the hash id names (sha256::of(message) with
        // hash_id::sha256: RS256), size() bytes; deterministic, as the
        // scheme is. A digest of another length than id's is
        // std::invalid_argument, and so is a key too small for the hash's
        // encoding (never at 1024 bits and more)
        vector<byte> sign_digest(hash_id id, const slice<const byte>& digest) const {
            _check();
            _pub._check_digest(id, digest);
            size_t k = size();
            std::vector<unsigned char> em(k);
            if (!rsa_pad::pkcs1_encode(id, detail::bytes(digest.data()), digest.size(), em.data(), k)) {
                throw invalid_argument("sgcl::crypto::rsa: the key is too small for the hash");
            }
            return _sign(em.data(), k);
        }

        // The PSS signature (RFC 8017 §8.1.1) of the digest: MGF1 over the
        // same hash, a random salt as long as the digest (Go's
        // PSSSaltLengthEqualsHash; FIPS 186-5 asks for no more), so a
        // new signature every time. A digest of another length than id's
        // is std::invalid_argument, and so is a key too small for the
        // hash and the salt (a 1024-bit key with SHA-512)
        vector<byte> sign_digest_pss(hash_id id, const slice<const byte>& digest) const {
            _check();
            _pub._check_digest(id, digest);
            size_t h = digest.size();
            unsigned char salt[64];
            random::fill(slice<byte>(reinterpret_cast<byte*>(salt), h));
            vector<byte> sig = _sign_pss(id, digest, salt, h);
            return sig;
        }

        // The message of an OAEP ciphertext (RFC 8017 §7.1.2), the label's
        // hash and MGF1 both by id, an empty label. Every failure — a
        // ciphertext of another length than size() or not below n, an
        // encoding that is not one, another label — is the one
        // errc::authentication "decryption error", in the same time: which
        // check failed is what an attacker who can ask for decryptions
        // needs (Manger, CRYPTO 2001). The message is the user's data, a
        // vector<byte> (decrypt_oaep_to, into the caller's buffer, for a key
        // unwrapped)
        [[nodiscard]] expected<vector<byte>, error> decrypt_oaep(hash_id id, const slice<const byte>& ciphertext) const {
            return _decrypt(id, id, ciphertext, slice<const byte>());
        }

        // The same with the label (bytes or text) the encryption was given
        [[nodiscard]] expected<vector<byte>, error> decrypt_oaep(hash_id id, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            return _decrypt(id, id, ciphertext, label);
        }

        // The same with MGF1 over another hash than the label's
        [[nodiscard]] expected<vector<byte>, error> decrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            return _decrypt(id, mgf1, ciphertext, label);
        }

        // The message into out, the program's own buffer, which it clears
        // with secure_zero when done (a key unwrapped, a password: data
        // that must not stay in managed memory, as an AEAD's open_to):
        // the number of bytes written, or the one errc::authentication, in
        // which case out is not touched. out holds at least
        // max_oaep_message_size(id) bytes, else std::length_error, decided
        // before the decryption from the key and the hash alone
        [[nodiscard]] expected<size_t, error> decrypt_oaep_to(const slice<byte>& out, hash_id id, const slice<const byte>& ciphertext) const {
            return _decrypt_to(out, id, id, ciphertext, slice<const byte>());
        }

        [[nodiscard]] expected<size_t, error> decrypt_oaep_to(const slice<byte>& out, hash_id id, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            return _decrypt_to(out, id, id, ciphertext, label);
        }

        [[nodiscard]] expected<size_t, error> decrypt_oaep_to(const slice<byte>& out, hash_id id, hash_id mgf1, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            return _decrypt_to(out, id, mgf1, ciphertext, label);
        }

        // The RSAPrivateKey of PKCS #1, as Go's x509.MarshalPKCS1PrivateKey
        // and OpenSSL write it; the bytes hold the secret: a secret_bytes,
        // never managed memory
        secret_bytes to_pkcs1_der() const {
            _check();
            auto w = std::make_unique<DerWriter<Rsa::der_capacity>>();
            _write_pkcs1(*w);
            return detail::take_secret(*w);
        }

        // PKCS #8 PrivateKeyInfo, as Go's x509.MarshalPKCS8PrivateKey and
        // OpenSSL write it; the bytes hold the secret: a secret_bytes
        secret_bytes to_pkcs8_der() const {
            _check();
            auto w = std::make_unique<DerWriter<Rsa::der_capacity>>();
            _write_pkcs1(*w);
            w->wrap(der::octet_string, 0);
            Rsa::put_algorithm(*w);
            static constexpr unsigned char version[] = {der::integer, 0x01, 0x00};
            w->put(version, sizeof version);
            w->wrap(der::sequence, 0);
            return detail::take_secret(*w);
        }

        // The key from PEM text (a file's bytes, read_secret's or the
        // program's): the first private key block, "PRIVATE KEY" or "RSA PRIVATE KEY",
        // its base64 decoded straight into a secret_bytes (encoding::pem
        // would put the DER in managed memory). Text around the block is
        // passed over; an encrypted key is errc::unsupported
        static expected<RsaPrivateKey, error> from_pem(const slice<const byte>& text) {
            auto p = detail::read_key_pem(text);
            if (!p) {
                return unexpected<error>(p.error());
            }
            if (p->label == "PRIVATE KEY") {
                return from_pkcs8_der(p->der);
            }
            if (p->label == "RSA PRIVATE KEY") {
                return from_pkcs1_der(p->der);
            }
            return unexpected<error>(error(errc::malformed, string("PEM: a block of another key's type")));
        }

        // The key as PEM, "PRIVATE KEY" over its PKCS #8, as Go's
        // pem.Encode of x509.MarshalPKCS8PrivateKey and OpenSSL's genpkey
        // write it: a secret_bytes, never managed memory
        secret_bytes to_pem() const {
            return detail::write_key_pem("PRIVATE KEY", to_pkcs8_der());
        }


    private:
        friend struct RsaAccess;

        RsaPublicKey _pub;
        Words _s;        // d (kn), p, q, dP, dQ, qInv, p's R^2, R^3, q's R^2, R^3 (k each), p's and q's m0inv
        size_t _k = 0;   // words of p and of q: the larger's

        RsaPrivateKey(RsaPublicKey&& pub, Words&& s, size_t k) noexcept
        : _pub(std::move(pub)), _s(std::move(s)), _k(k) {
        }

        static size_t _layout(size_t kn, size_t k) noexcept {
            return kn + 9 * k + 2;
        }

        // the parts of the block
        word* _d() const noexcept { return const_cast<word*>(_s.data()); }
        word* _p() const noexcept { return _d() + _pub._n.size(); }
        word* _q() const noexcept { return _p() + _k; }
        word* _dp() const noexcept { return _p() + 2 * _k; }
        word* _dq() const noexcept { return _p() + 3 * _k; }
        word* _qinv() const noexcept { return _p() + 4 * _k; }
        word* _p_rr() const noexcept { return _p() + 5 * _k; }
        word* _p_rrr() const noexcept { return _p() + 6 * _k; }
        word* _q_rr() const noexcept { return _p() + 7 * _k; }
        word* _q_rrr() const noexcept { return _p() + 8 * _k; }
        word* _m0() const noexcept { return _p() + 9 * _k; }

        bn::Modulus _pmod() const noexcept {
            return bn::Modulus{_p(), _p_rr(), _p_rrr(), _m0()[0], _k};
        }

        bn::Modulus _qmod() const noexcept {
            return bn::Modulus{_q(), _q_rr(), _q_rrr(), _m0()[1], _k};
        }

        void _check() const {
            if (!_s) {
                moved_from("sgcl::crypto::rsa::private_key");
            }
        }

        // The Montgomery constants of p and q (both odd, above 1)
        void _constants() {
            size_t k = _k;
            Words scratch(bn::mul_scratch(k) + k);
            _m0()[0] = bn::mont_m0inv(_p()[0]);
            _m0()[1] = bn::mont_m0inv(_q()[0]);
            bn::mont_constants(_p_rr(), _p_rrr(), _p(), k, _m0()[0], scratch.data());
            bn::mont_constants(_q_rr(), _q_rrr(), _q(), k, _m0()[1], scratch.data());
        }

        // Whether the key's parts agree (see from_pkcs1_der), every check
        // folded into one mask before the answer, which is public. p and q
        // are odd and above 1 already
        bool _consistent() const {
            size_t k = _k;
            size_t kn = _pub._n.size();
            Words s(8 * k + 2 + kn + bn::reduce_scratch(k));
            word* pq = s.data();         // 2k
            word* r = pq + 2 * k;        // k
            word* m = r + k;             // k
            word* t = m + k;             // k + 1
            word* u = t + k + 1;         // k + 1
            word* extra = u + k + 1;     // kn + reduce_scratch
            uint64_t ok = ~uint64_t(0);
            // n = p q
            bn::mul(pq, _p(), k, _q(), k);
            ok &= bn::equal_mask(pq, _pub._n.data(), kn);
            if (2 * k > kn) {
                ok &= bn::zero_mask(pq + kn, 2 * k - kn);
            }
            // qInv < p, qInv q = 1 mod p
            ok &= bn::less_mask(_qinv(), _p(), k);
            bn::Modulus pm = _pmod();
            bn::reduce_to_mont(r, _q(), k, pm, extra);
            bn::mont_mul(r, r, _qinv(), pm, extra);
            ok &= bn::one_mask(r, k);
            // dP = d mod (p - 1), e dP = 1 mod (p - 1); the same for q
            for (int i = 0; i < 2; ++i) {
                const word* prime = i == 0 ? _p() : _q();
                const word* dx = i == 0 ? _dp() : _dq();
                bn::copy(m, prime, k);
                m[0] ^= 1;
                bn::reduce(r, _d(), kn, m, k, u);
                ok &= bn::equal_mask(r, dx, k);
                bn::mul_word(t, dx, k, _pub._e);
                bn::reduce(r, t, k + 1, m, k, u);
                ok &= bn::one_mask(r, k);
            }
            return ct_barrier(ok) != 0;
        }

        // A key from the parts' big-endian bytes, checked whole
        struct Parts {
            const unsigned char* p[8];   // n, e, d, p, q, dP, dQ, qInv
            size_t n[8];
            size_t at[8];
        };

        static expected<RsaPrivateKey, error> _from_parts(const Parts& parts) {
            uint64_t ev = 0;
            if (parts.n[1] > 8) {
                return unexpected<error>(Rsa::der_error(errc::unsupported, parts.at[1], "a public exponent above 2^31 - 1"));
            }
            for (size_t i = 0; i < parts.n[1]; ++i) {
                ev = ev << 8 | parts.p[1][i];
            }
            auto pub = RsaPublicKey::_make(parts.p[0], parts.n[0], ev, parts.at[0]);
            if (!pub) {
                return unexpected<error>(pub.error());
            }
            size_t kn = pub->_n.size();
            size_t k = bn::words_for_bytes(parts.n[3] > parts.n[4] ? parts.n[3] : parts.n[4]);
            // the lengths are public (the encoding shows them)
            auto too_long = [&](int i, size_t words) {
                return parts.n[i] > 8 * words;
            };
            if (k == 0 || k > kn || 2 * k < kn || too_long(2, kn) || too_long(5, k) || too_long(6, k) || too_long(7, k)) {
                return unexpected<error>(Rsa::der_error(errc::invalid_key, parts.at[0], "the key's numbers do not fit together"));
            }
            Words s(_layout(kn, k));
            RsaPrivateKey key(std::move(*pub), std::move(s), k);
            bn::from_be(key._d(), kn, parts.p[2], parts.n[2]);
            bn::from_be(key._p(), k, parts.p[3], parts.n[3]);
            bn::from_be(key._q(), k, parts.p[4], parts.n[4]);
            bn::from_be(key._dp(), k, parts.p[5], parts.n[5]);
            bn::from_be(key._dq(), k, parts.p[6], parts.n[6]);
            bn::from_be(key._qinv(), k, parts.p[7], parts.n[7]);
            // p and q odd and above 1 before any Montgomery constant; the
            // answer is public
            auto odd_above_one = [&](const word* x) {
                word high = x[0] >> 1;
                for (size_t i = 1; i < k; ++i) {
                    high |= x[i];
                }
                return (x[0] & 1) != 0 && high != 0;
            };
            if (!odd_above_one(key._p()) || !odd_above_one(key._q())) {
                return unexpected<error>(Rsa::der_error(errc::invalid_key, parts.at[3], "a prime is even or 1"));
            }
            key._constants();
            if (!key._consistent()) {
                return unexpected<error>(Rsa::der_error(errc::invalid_key, parts.at[0], "the key's numbers do not agree (n = p q, d, dP, dQ, qInv)"));
            }
            return key;
        }

        static expected<RsaPrivateKey, error> _read_pkcs1(DerReader& in) {
            DerReader seq;
            size_t at = in.offset();
            if (!in.read(der::sequence, seq)) {
                return unexpected<error>(Rsa::der_error(errc::malformed, at, "DER: an RSAPrivateKey is a SEQUENCE"));
            }
            static constexpr unsigned char v0[] = {0x00};
            static constexpr unsigned char v1[] = {0x01};
            size_t vat = seq.offset();
            if (!seq.read_exact(der::integer, v0, 1)) {
                if (seq.read_exact(der::integer, v1, 1)) {
                    return unexpected<error>(Rsa::der_error(errc::unsupported, vat, "a multi-prime key"));
                }
                return unexpected<error>(Rsa::der_error(errc::malformed, vat, "DER: an RSAPrivateKey's version is 0"));
            }
            static constexpr const char* names[8] = {
                "DER: the modulus is a non-negative INTEGER",
                "DER: the public exponent is a non-negative INTEGER",
                "DER: the private exponent is a non-negative INTEGER",
                "DER: prime1 is a non-negative INTEGER",
                "DER: prime2 is a non-negative INTEGER",
                "DER: exponent1 is a non-negative INTEGER",
                "DER: exponent2 is a non-negative INTEGER",
                "DER: coefficient is a non-negative INTEGER"};
            Parts parts{};
            for (int i = 0; i < 8; ++i) {
                parts.at[i] = seq.offset();
                if (!seq.read_unsigned_bytes(parts.p[i], parts.n[i])) {
                    return unexpected<error>(Rsa::der_error(errc::malformed, parts.at[i], names[i]));
                }
            }
            if (!seq.empty()) {
                return unexpected<error>(Rsa::der_error(errc::malformed, seq.offset(), "DER: data after an RSAPrivateKey's fields"));
            }
            return _from_parts(parts);
        }

        template<class W>
        void _write_pkcs1(W& w) const {
            size_t k = _k;
            size_t kn = _pub._n.size();
            Words tmp(kn > k ? kn : k);
            unsigned char* b = reinterpret_cast<unsigned char*>(tmp.data());
            size_t mark = w.size();
            Rsa::put_words(w, _qinv(), k, b);
            Rsa::put_words(w, _dq(), k, b);
            Rsa::put_words(w, _dp(), k, b);
            Rsa::put_words(w, _q(), k, b);
            Rsa::put_words(w, _p(), k, b);
            Rsa::put_words(w, _d(), kn, b);
            _pub._write_numbers(w);
            static constexpr unsigned char version[] = {der::integer, 0x01, 0x00};
            w.put(version, sizeof version);
            w.wrap(der::sequence, mark);
        }

        // --- the private operation ------------------------------------------

        // out = c^d mod n for c below n (kn words each): blinded, CRT,
        // checked with e. False when the check fails (a fault)
        bool _private(word* out, const word* c) const {
            const size_t k = _k;
            const size_t kn = _pub._n.size();
            const word* n = _pub._n.data();
            bn::Modulus nm = _pub._mod();
            bn::Modulus pm = _pmod();
            bn::Modulus qm = _qmod();
            // kn <= 2k: n = p q
            size_t scratch = std::max({bn::pow_scratch(k), bn::reduce_scratch(k), bn::inverse_scratch(kn), bn::mul_scratch(kn) + kn});
            Words s(10 * kn + 6 * k + scratch);
            word* r = s.data();
            word* b = r + kn;
            word* a = b + kn;
            word* ainv = a + kn;
            word* rinv = ainv + kn;       // r^-1 R mod n
            word* re = rinv + kn;         // r^e R mod n
            word* cb = re + kn;           // c r^e mod n
            word* mb = cb + kn;           // (c r^e)^d = m r mod n
            word* v = mb + kn;
            word* bm = v + kn;
            word* cp = bm + kn;           // k: c mod p, then m1
            word* cq = cp + k;            // k: c mod q, then m2
            word* m2 = cq + k;            // k
            word* h = m2 + k;             // k
            word* hq = h + k;             // 2k
            word* t = hq + 2 * k;
            // the blinding factor r and its inverse, through the product
            // with a second random b: only r b is inverted in variable time
            for (;;) {
                _random_below_n(r);
                _random_below_n(b);
                bn::to_mont(bm, b, nm, t);
                bn::mont_mul(a, r, bm, nm, t);          // r b
                if (bn::zero_mask(a, kn) != 0) {
                    continue;
                }
                if (bn::inverse_vartime(ainv, a, n, kn, t)) {
                    break;
                }
            }
            bn::mont_mul(v, bm, nm.rr, nm, t);          // b R^2
            bn::mont_mul(rinv, ainv, v, nm, t);        // (r b)^-1 b R = r^-1 R
            bn::to_mont(re, r, nm, t);
            bn::mont_pow_public(re, re, _pub._e, nm, t);   // r^e R
            bn::mont_mul(cb, c, re, nm, t);            // c r^e
            // CRT: m1 = cb^dP mod p, m2 = cb^dQ mod q
            bn::reduce_to_mont(cp, cb, kn, pm, t);
            bn::mont_pow(cp, cp, _dp(), 64 * k, pm, t);
            bn::reduce_to_mont(cq, cb, kn, qm, t);
            bn::mont_pow(cq, cq, _dq(), 64 * k, qm, t);
            bn::from_mont(m2, cq, qm, t);
            // h = qInv (m1 - m2) mod p, in p's form: m2 reduced mod p
            bn::reduce_to_mont(h, m2, k, pm, t);
            bn::mod_sub(h, cp, h, _p(), k);
            bn::mont_mul(h, h, _qinv(), pm, t);         // plain
            // mb = m2 + h q
            bn::mul(hq, h, k, _q(), k);
            word carry = bn::add(hq, hq, m2, k);
            for (size_t i = k; i < 2 * k; ++i) {
                bn::wide x = bn::wide(hq[i]) + carry;
                hq[i] = word(x);
                carry = word(x >> 64);
            }
            bn::copy(mb, hq, kn);
            // unblind: m = mb r^-1
            bn::mont_mul(out, mb, rinv, nm, t);
            // the check: out^e = c
            bn::to_mont(v, out, nm, t);
            bn::mont_pow_public(v, v, _pub._e, nm, t);
            bn::from_mont(v, v, nm, t);
            return ct_barrier(bn::equal_mask(v, c, kn)) != 0;
        }

        // A random number in [0, n) into r (kn words); 0 is refused by the
        // caller through the product
        void _random_below_n(word* r) const {
            size_t kn = _pub._n.size();
            size_t bits = _pub._bits;
            for (;;) {
                random::fill(slice<byte>(reinterpret_cast<byte*>(r), kn * sizeof(word)));
                if (bits % 64 != 0) {
                    r[kn - 1] &= (word(1) << (bits % 64)) - 1;
                }
                if (bn::less_mask(r, _pub._n.data(), kn) != 0) {
                    return;
                }
            }
        }

        // The signature of the encoded message em (k bytes, below n)
        vector<byte> _sign(const unsigned char* em, size_t len) const {
            size_t kn = _pub._n.size();
            size_t k = size();
            Words w(2 * kn);
            bn::from_be(w.data(), kn, em, len);
            if (!_private(w.data() + kn, w.data())) {
                throw runtime_error("sgcl::crypto::rsa: the signature did not verify with the public key (a fault in the computation)");
            }
            vector<byte> out(k);
            bn::to_be(detail::bytes(out.data()), k, w.data() + kn, kn);
            return out;
        }

        vector<byte> _sign_pss(hash_id id, const slice<const byte>& digest, const unsigned char* salt, size_t slen) const {
            size_t em_bits = _pub._bits - 1;
            size_t em_len = (em_bits + 7) / 8;
            std::vector<unsigned char> em(em_len);
            if (!rsa_pad::pss_encode(id, detail::bytes(digest.data()), digest.size(), salt, slen, em.data(), em_bits)) {
                throw invalid_argument("sgcl::crypto::rsa: the key is too small for the hash and its salt");
            }
            return _sign(em.data(), em_len);
        }

        expected<vector<byte>, error> _decrypt(hash_id id, hash_id mgf, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            Words w;
            auto offset = _decode(w, id, mgf, ciphertext, label);
            if (!offset) {
                return unexpected<error>(offset.error());
            }
            const byte* p = reinterpret_cast<const byte*>(w.data() + 2 * _pub._n.size());
            vector<byte> out(size() - *offset);
            if (out.size()) {
                std::memcpy(out.data(), p + *offset, out.size());
            }
            return out;
        }

        expected<size_t, error> _decrypt_to(const slice<byte>& out, hash_id id, hash_id mgf, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            _check();
            if (out.size() < _pub.max_oaep_message_size(id)) {
                throw length_error("sgcl::crypto::rsa::decrypt_oaep_to: the output holds fewer bytes than max_oaep_message_size");
            }
            Words w;
            auto offset = _decode(w, id, mgf, ciphertext, label);
            if (!offset) {
                return unexpected<error>(offset.error());
            }
            const unsigned char* em = reinterpret_cast<const unsigned char*>(w.data() + 2 * _pub._n.size());
            size_t n = size() - *offset;
            std::memcpy(out.data(), em + *offset, n);
            return n;
        }

        // The decryption and the OAEP decoding into w (c, m, then the
        // encoded message): the offset of the message in the encoded
        // message, or the one decryption error
        expected<size_t, error> _decode(Words& w, hash_id id, hash_id mgf, const slice<const byte>& ciphertext, const slice<const byte>& label) const {
            _check();
            size_t k = size();
            size_t h = digest_size(id);
            (void)digest_size(mgf);
            size_t kn = _pub._n.size();
            if (ciphertext.size() != k || k < 2 * h + 2) {
                return unexpected<error>(Rsa::decryption_error());
            }
            w = Words(2 * kn + bn::words_for_bytes(k));
            word* c = w.data();
            word* m = c + kn;
            unsigned char* em = reinterpret_cast<unsigned char*>(m + kn);
            bn::from_be(c, kn, detail::bytes(ciphertext.data()), k);
            if (bn::less_mask(c, _pub._n.data(), kn) == 0) {
                return unexpected<error>(Rsa::decryption_error());
            }
            bool ok = _private(m, c);
            bn::to_be(em, k, m, kn);
            size_t offset = 0;
            uint64_t good = rsa_pad::oaep_decode(id, mgf, em, k, detail::bytes(label.data()), label.size(), &offset);
            if (!ok || good == 0) {
                return unexpected<error>(Rsa::decryption_error());
            }
            return offset;
        }

        // --- generation ------------------------------------------------------

        static optional<RsaPrivateKey> _generate(size_t bits) {
            using namespace bn;
            const uint64_t e = 65537;
            size_t pbits = (bits + 1) / 2;
            size_t qbits = bits / 2;
            size_t k = (pbits + 63) / 64;
            size_t kn = (bits + 63) / 64;
            rsa_keygen::Sieve sieve(k, e);
            Words s(6 * k + 2);
            word* p = s.data();
            word* q = p + k;
            word* scratch = q + k;
            uint64_t pe = 0, qe = 0;
            rsa_keygen::random_prime<Release>(p, pbits, sieve, &pe);
            rsa_keygen::random_prime<Release>(q, qbits, sieve, &qe);
            // |p - q| above 2^(bits/2 - 100), as FIPS 186-5 asks; an
            // answer that is public: a new pair
            {
                Words d(2 * k);
                word borrow = sub(d.data(), p, q, k);
                sub(d.data() + k, q, p, k);
                select(d.data(), ct_bit_mask(borrow), d.data() + k, d.data(), k);
                size_t floor_bit = qbits - 100;
                word high = 0;
                for (size_t i = 0; i < k; ++i) {
                    word mask = 64 * i >= floor_bit ? ~word(0) : 64 * (i + 1) <= floor_bit ? 0 : ~word(0) << (floor_bit - 64 * i);
                    high |= d.data()[i] & mask;
                }
                if (ct_zero_mask(high) != 0) {
                    return nullopt;
                }
            }
            // n = p q, of exactly bits bits (the top two bits of each)
            std::vector<word> n(2 * k);
            mul(n.data(), p, k, q, k);
            RsaPublicKey pub;
            {
                std::vector<unsigned char> nb(8 * kn);
                to_be(nb.data(), nb.size(), n.data(), kn);
                auto made = RsaPublicKey::_make(nb.data(), nb.size(), e, 0);
                if (!made || made->_bits != bits) {
                    return nullopt;
                }
                pub = std::move(*made);
            }
            Words ks(_layout(kn, k));
            RsaPrivateKey key(std::move(pub), std::move(ks), k);
            copy(key._p(), p, k);
            copy(key._q(), q, k);
            key._constants();
            const rsa_keygen::SmallModulus& em = sieve.e();
            uint64_t rp = pe - 1;   // (p - 1) mod e, not 0
            uint64_t rq = qe - 1;
            // dP, dQ
            word* m = scratch;          // 2k: p - 1, q - 1, then (p - 1)(q - 1)
            word* m2 = m + k;
            word* t = m2 + k;           // 2k + 2
            copy(m, p, k);
            m[0] ^= 1;
            rsa_keygen::inverse_of_e(key._dp(), m, k, rp, em, t);
            copy(m2, q, k);
            m2[0] ^= 1;
            rsa_keygen::inverse_of_e(key._dq(), m2, k, rq, em, t);
            // d = e^-1 mod λ(n) = lcm(p - 1, q - 1), the smallest d, as
            // FIPS 186-5 (B.3.1) and SP 800-56B ask: (p - 1)(q - 1) over
            // gcd(p - 1, q - 1), both in constant time (the gcd and the
            // quotient are of secrets); e divides neither p - 1 nor q - 1,
            // so it does not divide λ either
            {
                Words phi(2 * k), g(k), lambda(2 * k), tt(2 * k + 2), d(2 * k);
                mul(phi.data(), m, k, m2, k);
                rsa_keygen::gcd_consttime(g.data(), m, m2, k, tt.data());
                rsa_keygen::div_consttime(lambda.data(), phi.data(), 2 * k, g.data(), k, tt.data());
                uint64_t rl = rsa_keygen::mod_small(lambda.data(), 2 * k, em);
                rsa_keygen::inverse_of_e(d.data(), lambda.data(), 2 * k, rl, em, tt.data());
                copy(key._d(), d.data(), kn);
            }
            // qInv = q^(p - 2) mod p
            {
                bn::Modulus pm = key._pmod();
                Words x(k), ex(k), sc(bn::pow_scratch(k) + bn::reduce_scratch(k));
                reduce_to_mont(x.data(), q, k, pm, sc.data());
                copy(ex.data(), p, k);
                Words two(k);
                two.data()[0] = 2;
                sub(ex.data(), ex.data(), two.data(), k);   // p - 2
                mont_pow(x.data(), x.data(), ex.data(), 64 * k, pm, sc.data());
                from_mont(key._qinv(), x.data(), pm, sc.data());
            }
            if (!key._consistent()) {
                return nullopt;   // never, but a key is never given out unchecked
            }
            return optional<RsaPrivateKey>(std::move(key));
        }
    };

    // The tests' way in: the known answers of PSS and OAEP (a salt and a
    // seed given, not drawn), the private operation alone, and a key's
    // words changed after its checks (a fault)
    struct RsaAccess {
        template<class R>
        static vector<byte> sign_pss(const RsaPrivateKey<R>& key, hash_id id, const slice<const byte>& digest, const slice<const byte>& salt) {
            key._check();
            return key._sign_pss(id, digest, detail::bytes(salt.data()), salt.size());
        }

        static vector<byte> encrypt_oaep(const RsaPublicKey& key, hash_id id, hash_id mgf, const slice<const byte>& message, const slice<const byte>& label, const slice<const byte>& seed) {
            return key._encrypt(id, mgf, message, label, detail::bytes(seed.data()));
        }

        // out = c^d mod n, both size() bytes big-endian; false on a fault
        template<class R>
        static bool private_op(const RsaPrivateKey<R>& key, unsigned char* out, const unsigned char* c) {
            size_t kn = key._pub._n.size();
            size_t k = key.size();
            std::vector<uint64_t> w(2 * kn);
            bn::from_be(w.data(), kn, c, k);
            bool ok = key._private(w.data() + kn, w.data());
            bn::to_be(out, k, w.data() + kn, kn);
            return ok;
        }

        // The bit of dP flipped
        template<class R>
        static void break_dp(RsaPrivateKey<R>& key, unsigned bit) {
            key._dp()[bit / 64] ^= uint64_t(1) << (bit % 64);
        }
    };
}

namespace sgcl::crypto::rsa {
    using public_key = detail::RsaPublicKey;
    using private_key = detail::RsaPrivateKey<detail::bn::OperatorDelete>;
}
