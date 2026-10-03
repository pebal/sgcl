[sgcl](../../README.md) › [hash](../README.md) › [xxh3_128](README.md)

# sgcl::hash::xxh3_128::digest

```cpp
array<byte, 16> digest() const noexcept;
```

The same sixteen bytes as [value](value.md), the high half first: the form a function written over any hasher
([req::hasher](../req/hasher.md)) takes.

## Parameters

None.

## Return value

The hash as an `array<byte, 16>`, big-endian.

## Complexity

Constant: as [value](value.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::xxh3_128 h;
    h.update("hello");
    println("{}", encoding::hex::encode(h.digest()));
    println("{}", h.digest() == h.value());
}
```

Output:

```text
b5e9c1ad071b3e7fc779cfaa5e523818
true
```

## See also

- [value](value.md): the hash
- [sgcl::hash::xxh3_128](README.md)
