[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::generate

```cpp
static private_key generate() noexcept;
```

Makes a new key of 32 bytes from [random](../random.md), written straight into the key, and computes its public key.
Go's `ecdh.X25519().GenerateKey(rand.Reader)`.

## Parameters

None.

## Return value

The key.

## Complexity

Constant: one fixed-base multiplication.

## Exceptions

None. A system that gives no random bytes ends the program, with a line on stderr.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::x25519::private_key::generate();
    println("{}", encoding::hex::encode(key.public_key().bytes()));
}
```

Sample output:

```text
cc7ff068689003e9b32baf0a3c5d173a31f0cc3811ce8263478e93ced304fc2b
```

## See also

- [from_bytes](from_bytes.md): the key of 32 bytes
- [random](../random.md): where the bytes come from
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
