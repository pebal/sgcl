[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::alg

```cpp
optional<algorithm> alg() const noexcept;
```

The one algorithm the key works with, its `alg`: a key that has one signs, verifies, encrypts and decrypts with it
alone. An `alg` that names a content encryption (RFC 7520 §5.6) is `dir` with that encryption alone.

## Parameters

None.

## Return value

The algorithm; `nullopt` for a key without `alg`, which works with any algorithm of its kind, and for an `alg` the
module does not have, which leaves a key that does nothing ([parameters](parameters.md) holds its text).

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
    auto any = crypto::jose::jwk(crypto::p256::private_key::generate());
    auto pinned = crypto::jose::jwk::generate(crypto::jose::algorithm::es256);
    println("{} {}", any.alg().has_value(), pinned.alg() == crypto::jose::algorithm::es256);
}
```

Output:

```text
false true
```

## See also

- [algorithm](../jose-algorithm.md)
- [sgcl::crypto::jose::jwk](README.md)
