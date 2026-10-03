[sgcl](../README.md) › [crypto](README.md) › [p256](p256.md)

# sgcl::crypto::p256::public_key

```cpp
#include "sgcl/crypto/p256.h"   // or "sgcl/crypto.h"
#include "sgcl/crypto/p384.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::p256 {
    using public_key = /* unspecified */;
}

namespace sgcl::crypto::p384 {
    using public_key = /* unspecified */;
}
```

`sgcl::crypto::p256::public_key` is a point of the curve P-256 other than the identity, Go's `ecdsa.PublicKey` and
`ecdh.PublicKey` in one: it [verifies](p256-public_key/verify_digest.md) the [ECDSA](ecdsa.md) signatures of its
[private_key](p256-private_key.md), and it is the peer an [ecdh_key](p256-ecdh_key.md) agrees with. It is read from
SEC 1's point encoding by [from_bytes](p256-public_key/from_bytes.md) or from a SubjectPublicKeyInfo by
[from_pkix_der](p256-public_key/from_pkix_der.md), and taken from a private key by its `public_key()`; there is no
other way to make one.

`p384::public_key` is the same class on P-384, with the sizes of that curve: a point of 97 bytes uncompressed and 49
compressed, and signatures r ‖ s of 96 bytes ([p384](p384.md)). The two are different types.

## Rules

- **A plain value**: nothing in a public key is a secret. It is copied and compared freely, holds the point and
  nothing else (65 bytes, 97 on P-384), allocates nothing and lives anywhere.
- **Always a valid point**: a key is checked when it is made from bytes, so it is on the curve and not the identity.
  ECDH with it cannot be led into a small subgroup: the order of the curve is prime.
- **Not in constant time**: decompressing a point, checking it is on the curve and verifying a signature work on
  public data and may take time that depends on it.

## Member objects

| Constant | P-256 | P-384 | Description |
|---|---|---|---|
| `size` | 65 | 97 | the uncompressed point, 04 ‖ X ‖ Y, `static constexpr size_t` |
| `compressed_size` | 33 | 49 | the compressed point, 02 or 03 (the parity of Y) ‖ X, `static constexpr size_t` |
| `signature_size` | 64 | 96 | a signature r ‖ s, as `verify_digest_raw` takes it, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the copy and the move; a key is made by `from_bytes`, `from_pkix_der` or a private key's `public_key()` |

#### Creation

| Function | Description |
|---|---|
| [from_bytes](p256-public_key/from_bytes.md) | reads a point in SEC 1's encoding, checked (static) |
| [from_pkix_der](p256-public_key/from_pkix_der.md) | reads a SubjectPublicKeyInfo (static) |

#### Verification

| Function | Description |
|---|---|
| [verify_digest](p256-public_key/verify_digest.md) | checks a DER signature of a digest |
| [verify_digest_raw](p256-public_key/verify_digest_raw.md) | checks a signature r ‖ s of a digest |

#### Encodings

| Function | Description |
|---|---|
| [bytes](p256-public_key/bytes.md) | the uncompressed point, 04 ‖ X ‖ Y |
| [bytes_compressed](p256-public_key/bytes_compressed.md) | the compressed point, 02 or 03 ‖ X |
| [to_pkix_der](p256-public_key/to_pkix_der.md) | the SubjectPublicKeyInfo |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](p256-public_key/operator_cmp.md) | compares two points |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the public key of RFC 6979, A.2.5, and its signature of SHA-256("sample")
    auto key = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299"));
    auto signature = encoding::hex::decode(
        "efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
        "f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");

    println("{}", key->verify_digest_raw(crypto::sha256::of("sample"), signature));
    println("{}", key->verify_digest_raw(crypto::sha256::of("simple"), signature));
    println("{}", encoding::hex::encode(key->bytes_compressed()));
}
```

Output:

```text
true
false
0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
```

## See also

- [p256::private_key](p256-private_key.md): the key that signs
- [p256::ecdh_key](p256-ecdh_key.md): the key that agrees with this one
- [ECDSA](ecdsa.md): how signatures are made and checked
- [p256](p256.md), [p384](p384.md): the curves and their rules
