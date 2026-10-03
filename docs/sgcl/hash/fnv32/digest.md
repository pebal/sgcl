[sgcl](../../README.md) › [hash](../README.md) › [fnv32](../fnv32.md)

# sgcl::hash::fnv32::digest

```cpp
array<byte, 4> digest() const noexcept;
```

`value()` as four bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256.md) among them.

## Parameters

None.

## Return value

The hash as an `array<byte, 4>`, big-endian.

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
    hash::fnv32 h;
    h.update("foobar");
    println("{:08x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
31f0b262
31f0b262
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::fnv32](../fnv32.md)
