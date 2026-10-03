[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](README.md)

# sgcl::crypto::p256::operator== (sgcl::crypto::p256::public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Compares two keys by their points, Go's `PublicKey.Equal`. A key read from the compressed form equals the same key
read from the uncompressed one: the point is what is kept. `!=` is its negation, written by the language. The key is
public, so the comparison is not in constant time.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys to compare |

## Return value

`true` when the keys are the same point, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the private key of RFC 6979, A.2.5, and its public key as the RFC gives it
    auto signer = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721"));
    auto published = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"));

    println("{}", signer->public_key() == published);
    println("{}", crypto::p256::private_key::generate().public_key() != published);
}
```

Output:

```text
true
true
```

## See also

- [bytes](bytes.md): the point compared
- [sgcl::crypto::p256::public_key](README.md)
