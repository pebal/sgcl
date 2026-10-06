[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::empty

```cpp
bool empty() const noexcept;
```

Whether there is no element inside: a primitive element, an empty SEQUENCE, [asn1()](asn1.md).

## Parameters

None.

## Return value

`true` when there is none inside.

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
    println("{} {} {}", encoding::asn1::sequence({}).empty(),
            encoding::asn1::sequence({encoding::asn1::null()}).empty(), encoding::asn1::null().empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): how many
- [sgcl::encoding::asn1](README.md)
