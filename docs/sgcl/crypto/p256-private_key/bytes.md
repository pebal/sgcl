[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::bytes

```cpp
secret<size> bytes() const;
```

The scalar d, `size` bytes big-endian (32 on P-256, 48 on P-384), Go's `PrivateKey.Bytes`: the form
[from_bytes](from_bytes.md) reads. It is the key itself, so it comes as a [secret](../secret/README.md), zeroed when it goes
and never in managed memory.

## Parameters

None.

## Return value

The scalar, a `secret<32>` (`secret<48>` on P-384).

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    auto scalar = key.bytes();
    println("{} bytes", scalar.size);

    auto again = crypto::p256::private_key::from_bytes(scalar);
    println("{}", again->public_key() == key.public_key());
}
```

Output:

```text
32 bytes
true
```

## See also

- [from_bytes](from_bytes.md): the key of a scalar
- [to_pkcs8_der](to_pkcs8_der.md): the scalar with the curve, as other programs read a key
- [sgcl::crypto::p256::private_key](README.md)
