[sgcl](../../README.md) › [hash](../README.md) › [xxh32](README.md)

# sgcl::hash::xxh32::digest

```cpp
array<byte, 4> digest() const noexcept;
```

`value()` as four bytes, the most significant first, as every digest of the module and as xxhsum writes it: the form a
function written over any hasher ([req::hasher](../req/hasher.md)) takes. A format that stores XXH32 in its own
order (LZ4 and zstd store it little-endian) writes `value()` itself.

## Parameters

None.

## Return value

The hash as an `array<byte, 4>`, big-endian.

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
    hash::xxh32 h;
    h.update("hello");
    println("{:08x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
fb0077f9
fb0077f9
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::xxh32](README.md)
