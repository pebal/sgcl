[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::private_key

```cpp
private_key(private_key&& other) noexcept;
```

The key of `other`, moved in; `other` is left zeroed. A key is not copied: [clone](clone.md) is the copy by name. The
keys are made by [generate](generate.md), [derive](derive.md) and [from_bytes](from_bytes.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key moved from |

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
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto pub = key.public_key();
    crypto::hpke::private_key moved(std::move(key));
    println("{}", moved.public_key() == pub);
}
```

Output:

```text
true
```

## See also

- [clone](clone.md)
- [sgcl::crypto::hpke::private_key](README.md)
