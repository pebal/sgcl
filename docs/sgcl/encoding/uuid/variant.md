[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::variant

```cpp
constexpr variant_kind variant() const noexcept;
```

The [variant](../uuid-variant_kind.md) field, the top bits of the ninth byte: which layout the rest follows.
`rfc9562` for every UUID of the RFC's versions; the others are of before it.

## Parameters

None.

## Return value

The variant.

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
    println(encoding::uuid::v4().variant() == encoding::uuid::variant_kind::rfc9562);
    println(encoding::uuid::nil().variant() == encoding::uuid::variant_kind::ncs);
}
```

Output:

```text
true
true
```

## See also

- [version](version.md)
- [sgcl::encoding::uuid](README.md)
