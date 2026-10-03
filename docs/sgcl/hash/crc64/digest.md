[sgcl](../../README.md) › [hash](../README.md) › [crc64](README.md)

# sgcl::hash::crc64::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256/README.md) among them. A block of xz
stores the CRC the other way round, the least significant byte first: a format writes [value](value.md) in its own
order rather than taking `digest()`.

## Parameters

None.

## Return value

The CRC as an `array<byte, 8>`, big-endian.

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
    hash::crc64 h;
    h.update("123456789");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));

    // a block of xz ends with its CRC, the least significant byte first
    vector<byte> trailer;
    encoding::little_endian::append_u64(trailer, h.value());
    println("{}", encoding::hex::encode(trailer));
}
```

Output:

```text
995dc9bbdf1939fa
995dc9bbdf1939fa
fa3919dfbbc95d99
```

## See also

- [value](value.md): the CRC as a number
- [sgcl::hash::crc64](README.md)
