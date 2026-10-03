[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::rsa

```cpp
#include "sgcl/crypto/rsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::rsa {
    using public_key = /* unspecified */;
    using private_key = /* unspecified */;
}
```

`sgcl::crypto::rsa` is RSA (RFC 8017, PKCS #1 v2.2), Go's `crypto/rsa`: the signatures of most certificates and of
JWS's RS256 and PS256, the key transport of CMS and of JWE's RSA-OAEP. A [private_key](rsa-private_key.md) signs
digests in PKCS #1 v1.5 and in PSS and decrypts OAEP; its [public_key](rsa-public_key.md) verifies and encrypts. Keys
of 2048 to 16384 bits are made here, keys of 1024 bits and more are read from PKCS #1, PKCS #8 and
SubjectPublicKeyInfo.

Where Go's `rsa.PublicKey` and `rsa.PrivateKey` are structs whose numbers any code may change, the two types here are
made only by their factories and read through methods: a public key is a plain value, checked once when it is made,
and a private key is move-only, its numbers a secret in memory of its own that is zeroed when the key goes. The
functions take a digest and the [hash_id](hash_id.md) of the hash that made it, as Go's `SignPKCS1v15` takes a
`crypto.Hash`.

Written from RFC 8017 and FIPS 186-5; tested against known answers (a key of OpenSSL's; signatures, a PSS salt and
an OAEP seed given, made by Go and checked by OpenSSL), against OpenSSL on keys of 2048, 3072 and 4096 bits (PKCS
#1 v1.5 signatures byte for byte for every [hash_id](hash_id.md), PSS and OAEP both ways, PKCS #1, PKCS #8 and SPKI
byte for byte, keys made here passing OpenSSL's full check), with its arithmetic held to OpenSSL's BIGNUM, and
fuzzed. **The implementation has not been through an independent cryptographic audit.**

## Rules

- **A message, or a digest and the hash that made it**: `sign(hash_id::sha256, message)` hashes the message
  itself, and is `sign_digest(hash_id::sha256, sha256::of(message))`; `sign_pss`, `verify` and `verify_pss` are the
  same to `sign_digest_pss`, `verify_digest` and `verify_digest_pss`. The [hash_id](hash_id.md) is part of the
  signature (PKCS #1 v1.5 writes its identifier into the signed block; PSS hashes the digest again with it and masks
  with MGF1 over it), so it is given with every call. A digest of another
  length than the hash's is `invalid_argument` when signing (the program's own data) and `false` when verifying,
  since a certificate names its hash itself and a verification must not be a way for one to stop the program. An
  unknown `hash_id` is `invalid_argument` everywhere. Any `hash_id` works; SHA-1 is broken for signatures and here
  only for old signatures that must still be read.
- **PKCS #1 v1.5 or PSS.** `sign`, `sign_digest`, `verify` and `verify_digest` are PKCS #1 v1.5 (RFC 8017 §8.2,
  JWS's RS256): deterministic, what most certificates and TLS 1.2 use. The names with `_pss` are PSS (§8.1,
  PS256; TLS 1.3 signs with it): MGF1 over the digest's own hash, a fresh random salt as long as the digest.
- **Verification never throws on data**: a digest of another length than its hash's, a signature of another length
  than the modulus's, one not below n, one that does not verify is `false`. Every `verify*` is
  `[[nodiscard]]`: a check whose result is dropped was never made.
- **OAEP, and only OAEP.** `encrypt_oaep` and `decrypt_oaep` are RSAES-OAEP (§7.1). **PKCS #1 v1.5 encryption is
  not here, and will not be**: its decryption is Bleichenbacher's padding oracle (CRYPTO 1998), found again in TLS
  stacks every few years (ROBOT, 2017). A program that must read such messages needs another library.
- **One error for every failed decryption**: whatever failed, `errc::authentication`, "sgcl::crypto::rsa:
  decryption error", in the same time ([decrypt_oaep](rsa-private_key/decrypt_oaep.md)). Do not add to what a
  failure tells: answer every failure alike.
- **The private key is a secret**: d, p, q, dP, dQ and qInv live in one block of words the key allocates, zeroed
  with stores the compiler cannot drop when the key goes; so is every word of scratch an operation uses. The key is
  move-only (`clone()` makes a second one by name), a move leaves the source empty, and a call on an empty key is
  `logic_error`. Its encodings are a [secret_bytes](secret_bytes.md), never managed memory. A public key, a
  signature, a digest, a ciphertext and the parse of any encoding are not secret.
- **Constant time** where a secret is: the private operation is Montgomery arithmetic of a width fixed by the
  modulus (never by a value), exponentiation four bits at a time with each window's power read by scanning the
  whole table through masks, CRT over p and q. It runs on a **blinded** input (c·rᵉ with a fresh random r,
  unblinded by r⁻¹ after), and its result is **checked with the public exponent** before it leaves: a fault in one
  half of CRT would otherwise give the factors of n to whoever sees the signature (Boneh, DeMillo and Lipton, 1997).
  A failed check is `runtime_error` from signing ("a fault in the computation") and the one decryption error from
  decryption. The inverse of the blinding factor is computed in variable time, on its product with a second random
  number. The tests measure the claim with dudect (the exponentiation by a secret exponent, a whole signature,
  OAEP's decoding and the whole decryption, each against a control that leaks on purpose).
- **Sizes**: [generate](rsa-private_key/generate.md) makes keys of 2048 to 16384 bits; the readers take a modulus of
  1024 to 16384 bits and a public exponent of 3 to 2³¹ − 1, odd.
- **Formats**: PKCS #1 (`RSA PUBLIC KEY`, `RSA PRIVATE KEY` in PEM), PKCS #8 (`PRIVATE KEY`) and SubjectPublicKeyInfo
  (`PUBLIC KEY`) are read and written as Go's `x509.MarshalPKCS1PrivateKey`, `MarshalPKCS8PrivateKey` and
  `MarshalPKIXPublicKey` write them, and as OpenSSL does, byte for byte. A private key's PEM is read and written by
  the key; a public key's PEM is [encoding::pem](../encoding/pem.md)'s.

## Member types

| Type | Definition |
|---|---|
| [public_key](rsa-public_key.md) | the modulus and the public exponent: verifies signatures, encrypts; a plain value |
| [private_key](rsa-private_key.md) | the key with its secret numbers: signs digests, decrypts; move-only |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

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

- [hash_id](hash_id.md): the hash named when the program runs; [sha256](sha256.md), [sha512](sha512.md)
- [random](random.md): the salts, the seeds, the blinding and the primes
- [secure_zero](secure_zero.md), [secret_bytes](secret_bytes.md): the secrets
- [error](error.md): what reading a key and decrypting return
- [x509::public_key](x509-public_key.md): the key of a certificate
- [p256](p256.md), [ecdsa](ecdsa.md): the signatures of the curves, of smaller keys
- [The module](README.md)
