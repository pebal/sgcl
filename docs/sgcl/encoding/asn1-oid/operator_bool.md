[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the identifier has arcs: `false` for `oid()` alone.

## Parameters

None.

## Return value

`true` for an identifier.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", bool(encoding::asn1::oid()), bool(encoding::asn1::oid("1.2")));
}
```

Output:

```text
false true
```

## See also

- [sgcl::encoding::asn1::oid](README.md)
