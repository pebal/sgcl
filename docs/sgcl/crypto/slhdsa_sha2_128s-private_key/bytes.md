[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::bytes

```cpp
secret<private_key_size> bytes() const;
```

The key's 4n bytes, SK.seed ‖ SK.prf ‖ PK.seed ‖ PK.root: the one form of an SLH-DSA private key, which [from_bytes](from_bytes.md) takes back.

## Parameters

None.

## Return value

The bytes, as a [secret\<private_key_size\>](../secret/README.md) (64, 96 or 128 bytes by the set): move-only, never in managed memory, zeroed when it goes.

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
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    println("{} bytes", key.bytes().size);
}
```

Output:

```text
64 bytes
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
