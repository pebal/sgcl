[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](README.md)

# sgcl::crypto::ed25519::private_key::generate

```cpp
static private_key generate() noexcept;
```

Makes a new key of a seed of 32 bytes from [random](../random/README.md), and computes its scalar, its prefix and its
public key. The seed's copy on the stack is zeroed before the call returns. Go's `ed25519.GenerateKey(nil)`.

## Parameters

None.

## Return value

The key.

## Complexity

Constant: one SHA-512 and one fixed-base multiplication.

## Exceptions

None. A system that gives no random bytes ends the program, with a line on stderr.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    println("{}", encoding::hex::encode(key.public_key().bytes()));
}
```

Sample output:

```text
466f486aec59e2b910d1e326c1f928388dee0eab337a14fd4251d62c6658f1a9
```

## See also

- [from_seed](from_seed.md): the key of a seed
- [random](../random/README.md): where the seed comes from
- [sgcl::crypto::ed25519::private_key](README.md)
