[sgcl](../../README.md) › [hash](../README.md) › [xxh64](README.md)

# sgcl::hash::xxh64::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first, as every digest of the module and as xxhsum writes it: the form a
function written over any hasher ([req::hasher](../req/hasher.md)) takes. A format that stores XXH64 in its own
order (LZ4 and zstd store it little-endian) writes `value()` itself.

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
    hash::xxh64 h;
    h.update("hello");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
26c7827d889f6da3
26c7827d889f6da3
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::xxh64](README.md)
