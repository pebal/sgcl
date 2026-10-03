[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::sign_digest

```cpp
/*(1)*/ vector<byte> sign_digest(const slice<const byte>& digest) const;
/*(2)*/ vector<byte> sign_digest(const slice<const byte>& digest, deterministic_t) const;
/*(3)*/ vector<byte> sign_digest(const slice<const byte>& digest,
                                 deterministic_t, hash_id id) const;
```

Signs a digest the program made (`sha256::of(message)`, or any other), and gives the signature as an ECDSA-Sig-Value
in DER, a SEQUENCE of the two INTEGERs r and s in their shortest form (RFC 5480 §2.2), as Go's `ecdsa.SignASN1` gives
it: the form of X.509 and TLS, which [verify_digest](../p256-public_key/verify_digest.md) checks. The digest's
leftmost bits are used, as many as the order n has, and its length is not checked against any hash: a digest of any
size is signed as FIPS 186-5 §6.4.1 truncates it ([ECDSA](../ecdsa.md#rules)).

1. k is RFC 6979's with random bytes added (hedged, §3.6): an HMAC-DRBG over SHA-256 (SHA-384 on P-384) of the key,
   the digest and fresh random bytes. Deterministic in the key and the digest if the system's random bytes were bad,
   fresh for every signature when they are good: two signatures of one digest differ.
2. RFC 6979's deterministic signature, without the random bytes: the same key and digest give the same signature,
   byte for byte OpenSSL's. The HMAC is the hash that made the digest, known by the digest's length alone: SHA-1,
   SHA-224, SHA-256, SHA-384 or SHA-512 for 20, 28, 32, 48 or 64 bytes, the curve's own hash for any other length.
3. The same with the hash named: needed for SHA-3 and SHA-512/256, whose lengths (2) reads as SHA-2's. With them (2)
   gives a valid signature, but not the one every other implementation gives.

- (2–3) The deterministic signature tells whoever sees two signatures whether the digests were equal, and a fault of
  the hardware while signing is not hidden by fresh random bytes as (1) hides it: it is for test vectors and for
  protocols that ask for it, and (1) stays the default.

`p384::private_key::sign_digest` signs with a key of P-384: its signatures are at most `max_signature_size`, 104
bytes, where P-256's are at most 72.

## Parameters

| Parameter | Description |
|---|---|
| `digest` | the digest to sign, one byte or more |
| `id` | the hash that made the digest |

## Return value

The signature in DER, at most `max_signature_size` bytes (72 on P-256).

## Complexity

Constant: one multiplication of the base point and an inverse; a candidate k out of range, drawn again, is rarer than
one in 2³².

## Exceptions

- (1–2) `std::invalid_argument` when `digest` is empty: the signature of nothing is a program's mistake (the
  arguments swapped, a digest never made), as Go refuses it.
- (3) `std::invalid_argument` when `digest` is not of the length of the hash `id`, or `id` is not a hash the module
  knows.
- `std::logic_error` when the key was moved from.

## Notes

Everything that touches d, k and k⁻¹ runs in constant time, and the generator's state and k are zeroed before the
function returns. Signing a digest of a message someone else chose is safe: the digest is not secret.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the key of RFC 6979, A.2.5; a program makes its own with generate()
    auto key = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721"));
    auto digest = crypto::sha256::of("sample");

    auto hedged = key->sign_digest(digest);
    println("{}", key->public_key().verify_digest(digest, hedged));
    println("{}", hedged == key->sign_digest(digest));

    // RFC 6979's signature of SHA-256("sample")
    auto fixed = key->sign_digest(digest, crypto::deterministic);
    println("{}", encoding::hex::encode(fixed));

    // a SHA3-256 digest has SHA-256's length: the hash is named
    auto sha3 = crypto::sha3_256::of("sample");
    auto named = key->sign_digest(sha3, crypto::deterministic, crypto::hash_id::sha3_256);
    println("{}", named == key->sign_digest(sha3, crypto::deterministic));

    try {
        key->sign_digest(vector<byte>());
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
false
3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8
false
sgcl::crypto::ecdsa: an empty digest
```

## See also

- [sign_digest_raw](sign_digest_raw.md): the signature as r ‖ s
- [p256::public_key::verify_digest](../p256-public_key/verify_digest.md): checks it
- [ECDSA](../ecdsa.md): the digest, the nonce, the encodings
- [hash_id](../hash_id.md): a hash named when the program runs
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
