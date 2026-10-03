[sgcl](../../README.md) › [hash](../README.md) › [fnv128a](README.md)

# sgcl::hash::fnv128a::digest

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
    hash::fnv128a h;
    h.update("foobar");
    println("{}", encoding::hex::encode(h.value()));
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
343e1662793c64bf6f0d3597ba446f18
343e1662793c64bf6f0d3597ba446f18
```

## See also

- [value](value.md): the hash
- [sgcl::hash::fnv128a](README.md)
