[sgcl](../../README.md) › [hash](../README.md) › [crc32c](README.md)

# sgcl::hash::crc32c::digest

```cpp
array<byte, 4> digest() const noexcept;
```

`value()` as four bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256/README.md) among them. A format that
stores the CRC the other way round, the least significant byte first, writes `value()` in its own order rather than
taking `digest()`.

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
    hash::crc32c h;
    h.update("123456789");
    println("{:08x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));

    // a format that stores the CRC the least significant byte first
    vector<byte> trailer;
    encoding::little_endian::append_u32(trailer, h.value());
    println("{}", encoding::hex::encode(trailer));
}
```

Output:

```text
e3069283
e3069283
839206e3
```

## See also

- [value](value.md): the CRC as a number
- [sgcl::hash::crc32c](README.md)
