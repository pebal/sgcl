[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [public_key](../x25519-public_key.md)

# sgcl::crypto::x25519::operator== (sgcl::crypto::x25519::public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Compares the bytes of two keys. A public key is not a secret, so the comparison is an ordinary one, which may stop at
the first byte that differs. Two encodings of one point (a value of p or more and its reduction, or bit 255 set and
clear) are different bytes and so different keys, though they give the same secrets. `!=` is made from it by the
compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys compared |

## Return value

Whether the keys are the same 32 bytes.

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
    // RFC 7748 §6.1: Bob's key, as he computes it and as Alice receives it
    auto bob = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));
    auto received = crypto::x25519::public_key::from_bytes(
        encoding::hex::decode("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"));
    println("{}", bob->public_key() == received.value());
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md): the bytes compared
- [sgcl::crypto::x25519::public_key](../x25519-public_key.md)
