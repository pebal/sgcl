[sgcl](../../README.md) › [hash](../README.md) › [crc32](README.md)

# sgcl::hash::crc32::digest

```cpp
array<byte, 4> digest() const noexcept;
```

`value()` as four bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256/README.md) among them. A gzip member
and a zip entry store the CRC the other way round, the least significant byte first, and PNG this way: a format
writes [value](value.md) in its own order rather than taking `digest()`.

## Parameters

None.

## Return value

The CRC as an `array<byte, 4>`, big-endian.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::crc32 h;
    h.update("123456789");
    println("{:08x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));

    // a gzip member ends with its CRC, the least significant byte first
    vector<byte> trailer;
    encoding::little_endian::append_u32(trailer, h.value());
    println("{}", encoding::hex::encode(trailer));
}
```

Output:

```text
cbf43926
cbf43926
2639f4cb
```

## See also

- [value](value.md): the CRC as a number
- [sgcl::hash::crc32](README.md)
