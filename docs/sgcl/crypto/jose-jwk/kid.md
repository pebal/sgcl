[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::kid

```cpp
string kid() const noexcept;
```

The key's `kid`: given in its [options](../jose-jwk-options.md) or read. A signature or an encryption made with the
key names it in its header, and a [key set](../jose-jwk_set/README.md) finds the key by it.

## Parameters

None.

## Return value

The `kid`; empty when the key has none.

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
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "2026-10"});
    println("{}", key.kid());
}
```

Output:

```text
2026-10
```

## See also

- [jwk_set::find](../jose-jwk_set/find.md)
- [sgcl::crypto::jose::jwk](README.md)
