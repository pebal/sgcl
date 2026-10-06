[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::generate

```cpp
static private_key generate() noexcept;
```

A new key (slh_keygen, FIPS 205 Algorithm 21): SK.seed, SK.prf and PK.seed of [crypto::random](../random/README.md), and PK.root, the root of the top XMSS tree, computed of them.

## Parameters

None.

## Return value

The key.

## Complexity

Constant: one XMSS tree of 2^h' leaves (12 ms for SLH-DSA-SHA2-128s, a fifth of a millisecond for its fast set).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto sig = key.sign("hello");
    println("{} {}", sig.size(), key.public_key().verify("hello", sig));
}
```

Output:

```text
7856 true
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
