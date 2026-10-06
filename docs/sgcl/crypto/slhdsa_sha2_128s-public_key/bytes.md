[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [public_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::public_key::bytes

```cpp
vector<byte> bytes() const;
```

The key's bytes, PK.seed ‖ PK.root, the form it is published in, which [from_bytes](from_bytes.md) reads.

## Parameters

None.

## Return value

The bytes: 32 for SLH-DSA-SHA2-128s (48, 64 for the 192 and 256 sets).

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
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    println("{}", key.public_key().bytes().size());
}
```

Output:

```text
32
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::public_key](README.md)
