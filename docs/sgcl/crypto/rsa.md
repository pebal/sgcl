# sgcl::crypto::rsa

```cpp
#include "sgcl/crypto/rsa.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto::rsa {
    class public_key;     // n and e: verifies signatures, encrypts
    class private_key;    // signs digests, decrypts; move-only
}
```

RSA (RFC 8017, PKCS #1 v2.2), Go's `crypto/rsa`: the signatures of most certificates and of JWS's RS256 and PS256, the key transport of CMS and of JWE's RSA-OAEP. A `private_key` signs digests in PKCS #1 v1.5 and in PSS and decrypts OAEP; its `public_key` verifies and encrypts. Keys of 2048 to 16384 bits are made here, keys of 1024 bits and more are read from PKCS #1, PKCS #8 and SubjectPublicKeyInfo. Written from RFC 8017 and FIPS 186-5; tested against known answers (a key of OpenSSL's; signatures, a PSS salt and an OAEP seed given, made by Go and checked by OpenSSL), against OpenSSL on keys of 2048, 3072 and 4096 bits (PKCS #1 v1.5 signatures byte for byte for every [`hash_id`](hash_id.md), PSS and OAEP both ways, PKCS #1, PKCS #8 and SPKI byte for byte, keys made here passing OpenSSL's full check), with its arithmetic held to OpenSSL's BIGNUM, and fuzzed.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A digest, and the hash that made it**: `sign_digest(hash_id::sha256, sha256::of(message))`. The [`hash_id`](hash_id.md) is part of the signature (PKCS #1 v1.5 writes its identifier into the signed block; PSS hashes the digest again with it and masks with MGF1 over it), so it is given with every call. A digest of another length than the hash's is `std::invalid_argument` when signing (the program's own data) and `false` when verifying, since a certificate names its hash itself and a verification must not be a way for one to stop the program. Any `hash_id` works; SHA-1 is broken for signatures and here only for old signatures that must still be read.
- **PKCS #1 v1.5 or PSS.** `sign_digest` and `verify_digest` are PKCS #1 v1.5 (RFC 8017 §8.2, JWS's RS256): deterministic, what most certificates and TLS 1.2 use. `sign_digest_pss` and `verify_digest_pss` are PSS (§8.1, PS256; TLS 1.3 signs with it): MGF1 over the digest's own hash, a fresh random salt as long as the digest when signing (Go's `PSSSaltLengthEqualsHash`, what FIPS 186-5 allows at most), and a salt of any length when verifying (Go's `PSSSaltLengthAuto`: the length is read from the signature), or of exactly the length given to the overload with `salt_length` (RFC 8017 §9.1.2 as written, Go's `PSSSaltLengthEqualsHash` when it is the digest's length; what a certificate's RSASSA-PSS parameters ask, and what `x509` verifies with). A v1.5 signature is verified by building the one encoding the digest has and comparing the whole block, never by parsing it: no DigestInfo without its NULL, no bytes after the digest, no short padding gets through (the lax parsers behind Bleichenbacher's forgeries of 2006 and BERserk).
- **Verification never throws on data**: a digest of another length than its hash's, a signature of another length than the modulus's, one not below n, one that does not verify is `false`. Both `verify_digest*` are `[[nodiscard]]`: a check whose result is dropped was never made.
- **OAEP, and only OAEP.** `encrypt_oaep` and `decrypt_oaep` are RSAES-OAEP (§7.1): the label's hash and MGF1 by the `hash_id` given, or MGF1 over another hash with the four-argument forms (Java's `OAEPWithSHA-256AndMGF1Padding` is SHA-256 with MGF1 over SHA-1), a label or none. A message is at most `max_oaep_message_size(id)` bytes (the modulus's bytes less twice the digest's less 2: 190 for a 2048-bit key and SHA-256); a longer one is `std::invalid_argument`. **PKCS #1 v1.5 encryption is not here, and will not be**: its decryption is Bleichenbacher's padding oracle (1998), found again in TLS stacks every few years (ROBOT, 2017). A program that must read such messages needs another library.
- **The plaintext into the program's buffer.** `decrypt_oaep` returns a managed `vector<byte>`, which nobody zeroes; for a key unwrapped or a password, `decrypt_oaep_to(out, …)` writes into the program's own buffer, which it clears with [`secure_zero`](secure_zero.md), as an AEAD's `open_to`. `out` holds at least `max_oaep_message_size(id)` bytes, else `std::length_error`, decided from the key and the hash before anything is decrypted; it returns the message's length, and a failure leaves `out` as it was.
- **One error for every failed decryption.** A ciphertext of another length (exactly the modulus's bytes, as RFC 8017 §7.1.2 has it, where Go and OpenSSL take a shorter one), not below n, an encoding that is not OAEP's, another label, another hash — every failure is the same `crypto::error`: `errc::authentication`, "sgcl::crypto::rsa: decryption error", offset 0, and it takes the same time: every check is done on every byte and folded into one mask before the one branch. An attacker who can tell a bad leading byte from a bad label hash decrypts any message (Manger, 2001). Do not add to what a failure tells: answer every failure alike.
- **The private key is a secret**: d, p, q, dP, dQ and qInv live in one block of words the key allocates, zeroed with stores the compiler cannot drop when the key goes; so is every word of scratch an operation uses. The key is move-only (`clone()` makes a second one by name), a move leaves the source empty, and a call on an empty key is `std::logic_error`. Keep it on the stack or in a `unique_ptr`. `to_pkcs1_der()`, `to_pkcs8_der()` and `to_pem()` give the secret in a [`secret_bytes`](secret.md#secret_bytes), and so does `decrypt_oaep` the message: never in managed memory, zeroed when they go. A public key is a plain value: copied and compared.
- **Constant time** where a secret is: the private operation is Montgomery arithmetic of a width fixed by the modulus (never by a value), exponentiation four bits at a time with each window's power read by scanning the whole table through masks, CRT over p and q. It runs on a **blinded** input (c·rᵉ with a fresh random r, unblinded by r⁻¹ after), and its result is **checked with the public exponent** before it leaves: a fault in one half of CRT would otherwise give the factors of n to whoever sees the signature (Boneh, DeMillo and Lipton, 1997). A failed check is `std::runtime_error` from signing ("a fault in the computation") and the one decryption error from `decrypt_oaep`. The inverse of the blinding factor is computed in variable time, on its product with a second random number. The tests measure the claim with dudect (the exponentiation by a secret exponent, a whole signature, OAEP's decoding and the whole decryption, each against a control that leaks on purpose).
- **Keys made here**: `generate(bits)` makes two primes of half the bits each (fresh random candidates with their top two bits set, trial division by the primes below 2048, sixteen rounds of Miller–Rabin), e = 65537, |p − q| above 2^(bits/2 − 100) as FIPS 186-5 asks, and d = e⁻¹ mod λ(n) = lcm(p − 1, q − 1), the smallest d, as FIPS 186-5 (B.3.1) and SP 800-56B ask, so that the keys pass a FIPS validator (OpenSSL's FIPS provider, an HSM) as well as any other; the gcd of p − 1 and q − 1 and the quotient that gives λ are computed in constant time. Fewer than 2048 bits, or more than 16384, is `std::invalid_argument`. It takes tens of milliseconds for 2048 bits and grows quickly with the size (a candidate's test costs the cube of its length, and the primes are sparser), and varies from key to key as the primes are found.
- **Keys read**: `from_pkcs1_der` (`RSA PRIVATE KEY` in PEM), `from_pkcs8_der` (`PRIVATE KEY`), `public_key::from_pkix_der` (`PUBLIC KEY`), `from_pkcs1_der` (`RSA PUBLIC KEY`) and `from_modulus(n, e)` (a JWK's `n` and `e`) return `expected`: `errc::malformed` with the byte's offset for DER that is not strict DER; `errc::unsupported` for a modulus of fewer than 1024 bits or more than 16384, an exponent above 2³¹ − 1, a multi-prime key, an RSASSA-PSS key (`id-RSASSA-PSS`: read the key as `rsaEncryption`), another algorithm; `errc::invalid_key` for an even modulus, an exponent even or below 3, and a private key whose numbers do not agree — n = p·q, qInv·q = 1 mod p, dP = d mod (p − 1) and e·dP = 1 mod (p − 1), the same for q, all checked when the key is read (in constant time, since they are secrets). A private key's PEM is read by `from_pem` (`PRIVATE KEY` or `RSA PRIVATE KEY`, straight into a `secret_bytes`; for a key file `from_pem(crypto::read_secret(path))`) and written by `to_pem` (`PRIVATE KEY`, as OpenSSL writes it); a public key's PEM is [`encoding::pem`](../encoding/pem.md)'s.
- **Formats written**: PKCS #1, PKCS #8 and SPKI as Go's `x509.MarshalPKCS1PrivateKey`, `MarshalPKCS8PrivateKey` and `MarshalPKIXPublicKey` write them, and as OpenSSL does, byte for byte.

## Members

```cpp
class public_key {                                   // a plain value: copied, compared
public:
    static expected<public_key, error> from_modulus(const slice<const byte>& n, uint64_t e);
    static expected<public_key, error> from_pkcs1_der(const slice<const byte>& der);   // RSAPublicKey
    static expected<public_key, error> from_pkix_der(const slice<const byte>& der);    // SubjectPublicKeyInfo

    size_t bits() const noexcept;                    // 2048
    size_t size() const noexcept;                    // 256: a signature's and a ciphertext's bytes
    vector<byte> modulus() const;                    // n, size() bytes
    uint64_t exponent() const noexcept;              // e
    vector<byte> to_pkcs1_der() const;
    vector<byte> to_pkix_der() const;

    [[nodiscard]] bool verify_digest(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature) const;      // PKCS #1 v1.5
    [[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature) const;  // PSS, any salt
    [[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest, const slice<const byte>& signature,
                                         size_t salt_length) const;                                                              // PSS, that salt only

    // message and label: bytes or text, which a slice of bytes takes both
    vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message) const;
    vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message, const slice<const byte>& label) const;
    vector<byte> encrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& message, const slice<const byte>& label) const;
    size_t max_oaep_message_size(hash_id id) const;

    friend bool operator==(const public_key&, const public_key&) noexcept;
};

class private_key {                                  // move-only
public:
    static private_key generate(size_t bits);        // 2048 to 16384; e = 65537
    static expected<private_key, error> from_pkcs1_der(const slice<const byte>& der);  // RSAPrivateKey
    static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der);  // PrivateKeyInfo
    static expected<private_key, error> from_pem(const slice<const byte>& text);       // "PRIVATE KEY" or "RSA PRIVATE KEY"

    private_key(private_key&&) noexcept;             // the source left empty
    private_key& operator=(private_key&&) noexcept;
    ~private_key();                                  // every word zeroed
    private_key clone() const;

    public_key public_key() const;
    size_t bits() const noexcept;
    size_t size() const noexcept;

    vector<byte> sign_digest(hash_id id, const slice<const byte>& digest) const;       // PKCS #1 v1.5
    vector<byte> sign_digest_pss(hash_id id, const slice<const byte>& digest) const;   // PSS, salt of the digest's length

    expected<vector<byte>, error> decrypt_oaep(hash_id id, const slice<const byte>& ciphertext) const;   // the message: the user's data
    expected<vector<byte>, error> decrypt_oaep(hash_id id, const slice<const byte>& ciphertext, const slice<const byte>& label) const;
    expected<vector<byte>, error> decrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& ciphertext, const slice<const byte>& label) const;
    expected<size_t, error> decrypt_oaep_to(const slice<byte>& out, hash_id id, const slice<const byte>& ciphertext) const;   // and with the label, and mgf1

    secret_bytes to_pkcs1_der() const;
    secret_bytes to_pkcs8_der() const;
    secret_bytes to_pem() const;                                                        // "PRIVATE KEY"
};
```

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    // the signer keeps its key as PKCS #8 and publishes the public key as SPKI
    auto stored = crypto::rsa::private_key::generate(2048).to_pkcs8_der();
    auto key = crypto::rsa::private_key::from_pkcs8_der(stored);
    if (!key) {
        eprintln(key.error().message());
        return 1;
    }
    auto published = key->public_key().to_pkix_der();

    // PS256: a PSS signature of a SHA-256 digest
    auto digest = crypto::sha256::of("{\"sub\":\"1234567890\"}");
    auto sig = key->sign_digest_pss(crypto::hash_id::sha256, digest);

    auto pub = crypto::rsa::public_key::from_pkix_der(published);
    println(pub && pub->verify_digest_pss(crypto::hash_id::sha256, digest, sig)
                ? "valid" : "forged");
    sig[10] ^= byte(1);
    println(pub && pub->verify_digest_pss(crypto::hash_id::sha256, digest, sig)
                ? "valid" : "forged");

    // OAEP: a key for a symmetric cipher sent to the key's owner
    auto session = crypto::random::secret(32);
    auto sealed = pub->encrypt_oaep(crypto::hash_id::sha256, session, "session key");
    // a vector<byte>; a key to keep goes to decrypt_oaep_to
    auto opened = key->decrypt_oaep(crypto::hash_id::sha256, sealed, "session key");
    println(opened && crypto::constant_time::equal(opened, session) ? "the key arrived" : "lost");

    // another label, or a ciphertext changed: the one error
    auto wrong = key->decrypt_oaep(crypto::hash_id::sha256, sealed, "another label");
    println(wrong ? "opened" : wrong.error().message());
}
```

Output:

```text
valid
forged
the key arrived
sgcl::crypto::rsa: decryption error
```

## See also

[`hash_id`](hash_id.md) (the digest named at run time), [`sha256`](sha256.md), [`sha512`](sha512.md), [`random`](random.md), [`secure_zero`](secure_zero.md), [`error`](error.md), [`p256`](p256.md) and [`ecdsa`](ecdsa.md) (the curves' signatures, smaller and faster).
