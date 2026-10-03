[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [name](../x509-name.md)

# sgcl::crypto::x509::name::name

```cpp
name() = default;
```

Makes an empty name, with no attribute: what a certificate's empty subject is read as. A name with attributes is read
from a certificate.

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
    crypto::x509::name nobody;
    println("{} [{}]", nobody.empty(), nobody.to_string());
}
```

Output:

```text
true []
```

## See also

- [empty](empty.md)
- [sgcl::crypto::x509::name](../x509-name.md)
