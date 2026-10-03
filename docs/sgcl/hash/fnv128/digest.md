[sgcl](../../README.md) › [hash](../README.md) › [fnv128](../fnv128.md)

# sgcl::hash::fnv128::digest

```cpp
array<byte, 16> digest() const noexcept;
```

The same sixteen bytes as [value](value.md), the most significant first: Go's `h.Sum(nil)`, and the form a function
written over any hasher ([req::hasher](../req/hasher.md)) takes.

## Parameters

None.

## Return value

The hash as an `array<byte, 16>`, big-endian.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::fnv128 h;
    h.update("foobar");
    println("{}", encoding::hex::encode(h.value()));
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
7896bfea9c3c64bf6dc58353d2c293aa
7896bfea9c3c64bf6dc58353d2c293aa
```

## See also

- [value](value.md): the hash
- [sgcl::hash::fnv128](../fnv128.md)
