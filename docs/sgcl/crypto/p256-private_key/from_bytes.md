[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::from_bytes

```cpp
static expected<private_key, error> from_bytes(const slice<const byte>& scalar) noexcept;
```

The key of its scalar d, big-endian, Go's `ecdsa.ParseRawPrivateKey`: `size` bytes (32 on P-256, 48 on P-384) whose
number is in [1, n − 1]. The public point d·G is computed here, once.

## Parameters

| Parameter | Description |
|---|---|
| `scalar` | the scalar, `size` bytes big-endian |

## Return value

The key, or a [crypto::error](../error/README.md) with `errc::invalid_key` for a scalar of another length, or of zero, or
not below the order n.

## Complexity

Constant: one multiplication of the base point.

## Exceptions

None.

## Notes

The scalar is a secret: a scalar a program keeps belongs in a [secret](../secret/README.md) or a
[secret_bytes](../secret_bytes/README.md), never in a managed `vector` or `string`, which the collector frees without
zeroing.

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
    println("{}", encoding::hex::encode(key->public_key().bytes_compressed()));

    println("{}", crypto::p256::private_key::from_bytes(vector<byte>(31)).error().message());
    println("{}", crypto::p256::private_key::from_bytes(vector<byte>(32)).error().message());
}
```

Output:

```text
0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
sgcl::crypto::p256: a private key is 32 bytes
sgcl::crypto::p256: the private scalar is not in [1, n - 1]
```

## See also

- [bytes](bytes.md): the scalar of a key
- [generate](generate.md): a key of a fresh scalar
- [sgcl::crypto::p256::private_key](README.md)
