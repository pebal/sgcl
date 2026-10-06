[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::use

```cpp
string use() const noexcept;
```

The key's `use`: `sig` for a key that signs and verifies, `enc` for one that encrypts and decrypts. A key whose `use`
is the other is refused for the purpose.

## Parameters

None.

## Return value

The `use`; empty when the key has none, which allows both.

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
    auto key = crypto::jose::jwk(crypto::x25519::private_key::generate(), {.use = "enc"});
    println("{}", key.use());
}
```

Output:

```text
enc
```

## See also

- [options](../jose-jwk-options.md)
- [sgcl::crypto::jose::jwk](README.md)
