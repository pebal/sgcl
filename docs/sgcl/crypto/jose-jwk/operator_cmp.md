[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::operator==

```cpp
friend bool operator==(const jwk& a, const jwk& b) noexcept;
```

Whether two handles are the same key: two copies of one handle, or two keys of the same kind, both private or both
public, of the same public members and, for private keys, the same private members, compared in constant time. A
private key and its public half are not equal; their [thumbprints](thumbprint.md) are.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same key.

## Complexity

Linear in the size of the keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256);
    auto again = crypto::jose::jwk::parse(key.to_private_json());
    println("{} {}", *again == key, key.public_key() == key);
}
```

Output:

```text
true false
```

## See also

- [thumbprint](thumbprint.md)
- [sgcl::crypto::jose::jwk](README.md)
