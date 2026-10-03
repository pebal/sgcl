[sgcl](../../README.md) › [hash](../README.md) › [maphash](../maphash.md)

# sgcl::hash::maphash::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes.

## Parameters

None.

## Return value

The hash as an `array<byte, 8>`, big-endian.

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
    hash::maphash h;
    h.update("hello");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Sample output:

```text
29c3d609186276c7
29c3d609186276c7
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::maphash](../maphash.md)
