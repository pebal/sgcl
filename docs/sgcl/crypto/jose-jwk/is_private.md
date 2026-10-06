[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::is_private

```cpp
bool is_private() const noexcept;
```

Whether the key has its private half: a private key, or a symmetric key, which is all private.

## Parameters

None.

## Return value

`true` for a private key and an oct key, `false` for a public key.

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
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa);
    println("{} {}", key.is_private(), key.public_key().is_private());
}
```

Output:

```text
true false
```

## See also

- [public_key](public_key.md): the public half
- [sgcl::crypto::jose::jwk](README.md)
