[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](README.md)

# sgcl::crypto::x509::public_key::public_key

```cpp
public_key() = default;
```

Makes no key: `key_kind::none` and an empty algorithm. A key is read from a certificate.

## Parameters

None.

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
    crypto::x509::public_key nothing;
    println("{} {}", nothing.has_value(), nothing.kind() == crypto::x509::key_kind::none);
}
```

Output:

```text
false true
```

## See also

- [kind](kind.md)
- [sgcl::crypto::x509::public_key](README.md)
