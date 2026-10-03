[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::sign_digest_raw

```cpp
array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest) const;    // (1)
array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest,           // (2)
                                            deterministic_t) const;
array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest,           // (3)
                                            deterministic_t, hash_id id) const;
```

Signs a digest as [sign_digest](sign_digest.md) does, and gives the signature as the fixed-size r ‖ s of IEEE P1363,
each big-endian in `size` bytes: the form of JWS, WebAuthn, PKCS #11 and COSE, which
[verify_digest_raw](../p256-public_key/verify_digest_raw.md) checks. The overloads choose k as `sign_digest`'s do:

1. Hedged: RFC 6979's k with fresh random bytes added; two signatures of one digest differ.
2. RFC 6979's deterministic k, its HMAC the hash known by the digest's length.
3. The deterministic k with the hash named, for SHA-3 and SHA-512/256.

`p384::private_key::sign_digest_raw` gives 96 bytes, r and s of 48 bytes each.

## Parameters

| Parameter | Description |
|---|---|
| `digest` | the digest to sign, one byte or more |
| `id` | the hash that made the digest |

## Return value

The signature r ‖ s, `signature_size` bytes (64 on P-256), in an array.

## Complexity

Constant: one multiplication of the base point and an inverse.

## Exceptions

- (1–2) `std::invalid_argument` when `digest` is empty.
- (3) `std::invalid_argument` when `digest` is not of the length of the hash `id`, or `id` is not a hash the module
  knows.
- `std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the key of RFC 6979, A.2.5, and its signatures of "sample"
    auto key = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721"));

    auto rs = key->sign_digest_raw(crypto::sha256::of("sample"), crypto::deterministic);
    println("r {}", encoding::hex::encode(rs.as_slice(0, 32)));
    println("s {}", encoding::hex::encode(rs.as_slice(32, 32)));

    // a SHA-512 digest is longer than the order: its leftmost 256 bits are signed
    rs = key->sign_digest_raw(crypto::sha512::of("sample"), crypto::deterministic);
    println("r {}", encoding::hex::encode(rs.as_slice(0, 32)));
    println("s {}", encoding::hex::encode(rs.as_slice(32, 32)));
}
```

Output:

```text
r efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716
s f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8
r 8496a60b5e9b47c825488827e0495b0e3fa109ec4568fd3f8d1097678eb97f00
s 2362ab1adbe2b8adf9cb9edab740ea6049c028114f2460f96554f61fae3302fe
```

## See also

- [sign_digest](sign_digest.md): the signature in DER
- [p256::public_key::verify_digest_raw](../p256-public_key/verify_digest_raw.md): checks it
- [ECDSA](../ecdsa.md): the digest, the nonce, the encodings
- [sgcl::crypto::p256::private_key](README.md)
