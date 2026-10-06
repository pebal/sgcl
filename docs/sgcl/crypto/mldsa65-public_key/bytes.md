[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [public_key](README.md)

# sgcl::crypto::mldsa65::public_key::bytes

```cpp
vector<byte> bytes() const;
```

The key's bytes (pkEncode, FIPS 204 §7.2), the form it is published in, which [from_bytes](from_bytes.md) reads.

## Parameters

None.

## Return value

The bytes: 1952 for ML-DSA-65 (1312, 2592).

## Complexity

Linear in the size of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    println("{}", key.public_key().bytes().size());
}
```

Output:

```text
1952
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::crypto::mldsa65::public_key](README.md)
