[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::public_key

```cpp
slhdsa_sha2_128s::public_key public_key() const;
```

The key's [public_key](../slhdsa_sha2_128s-public_key/README.md), PK.seed ‖ PK.root, the one to publish.

## Parameters

None.

## Return value

The public key, a value.

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
    println("{} bytes", key.public_key().bytes().size());
}
```

Output:

```text
32 bytes
```

## See also

- [slhdsa_sha2_128s::public_key](../slhdsa_sha2_128s-public_key/README.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
