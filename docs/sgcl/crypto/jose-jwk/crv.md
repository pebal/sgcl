[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::crv

```cpp
string crv() const noexcept;
```

The curve of an EC or OKP key, its `crv`.

## Parameters

None.

## Return value

`P-256`, `P-384`, `P-521`, `Ed25519` or `X25519`; empty for an RSA key and an oct key.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk key(crypto::ed25519::private_key::generate());
    println("{}", key.crv());
}
```

Output:

```text
Ed25519
```

## See also

- [type](type.md)
- [sgcl::crypto::jose::jwk](README.md)
