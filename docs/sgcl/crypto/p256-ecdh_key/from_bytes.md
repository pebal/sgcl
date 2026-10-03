[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::from_bytes

```cpp
static expected<ecdh_key, error> from_bytes(const slice<const byte>& scalar) noexcept;
```

The key of its scalar d, big-endian, Go's `ecdh.P256().NewPrivateKey`: `size` bytes (32 on P-256, 48 on P-384) whose
number is in [1, n − 1]. The public point d·G is computed here, once.

## Parameters

| Parameter | Description |
|---|---|
| `scalar` | the scalar, `size` bytes big-endian |

## Return value

The key, or a [crypto::error](../error.md) with `errc::invalid_key` for a scalar of another length, or of zero, or
not below the order n.

## Complexity

Constant: one multiplication of the base point.

## Exceptions

None.

## Notes

The scalar is a secret: a scalar a program keeps belongs in a [secret](../secret.md) or a
[secret_bytes](../secret_bytes.md), never in a managed `vector` or `string`, which the collector frees without
zeroing.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the scalar dIUT of NIST's first test of ECC CDH on P-256, and its public point
    auto key = crypto::p256::ecdh_key::from_bytes(encoding::hex::decode(
        "7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534"));
    auto point = key->public_key().bytes();
    println("QIUTx {}", encoding::hex::encode(point.as_slice(1, 32)));
    println("QIUTy {}", encoding::hex::encode(point.as_slice(33, 32)));

    auto zero = crypto::p256::ecdh_key::from_bytes(vector<byte>(32));
    println("{}", zero.error().message());
}
```

Output:

```text
QIUTx ead218590119e8876b29146ff89ca61770c4edbbf97d38ce385ed281d8a6b230
QIUTy 28af61281fd35e2fa7002523acc85a429cb06ee6648325389f59edfce1405141
sgcl::crypto::p256: the private scalar is not in [1, n - 1]
```

## See also

- [bytes](bytes.md): the scalar of a key
- [generate](generate.md): a key of a fresh scalar
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
