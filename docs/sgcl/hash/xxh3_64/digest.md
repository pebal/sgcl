[sgcl](../../README.md) › [hash](../README.md) › [xxh3_64](../xxh3_64.md)

# sgcl::hash::xxh3_64::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first, as every digest of the module: the form a function written
over any hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256.md) among them.

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
    hash::xxh3_64 h;
    h.update("hello");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
9555e8555c62dcfd
9555e8555c62dcfd
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::xxh3_64](../xxh3_64.md)
