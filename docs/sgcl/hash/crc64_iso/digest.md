[sgcl](../../README.md) › [hash](../README.md) › [crc64_iso](README.md)

# sgcl::hash::crc64_iso::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first: Go's `h.Sum(nil)`, and the form a function written over any
hasher ([req::hasher](../req/hasher.md)) takes, [crypto::sha256](../../crypto/sha256/README.md) among them. A format that
stores the CRC the other way round, the least significant byte first, writes `value()` in its own order rather than
taking `digest()`.

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
    hash::crc64_iso h;
    h.update("123456789");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));

    // a format that stores the CRC the least significant byte first
    vector<byte> trailer;
    encoding::little_endian::append_u64(trailer, h.value());
    println("{}", encoding::hex::encode(trailer));
}
```

Output:

```text
b90956c775a41001
b90956c775a41001
0110a475c75609b9
```

## See also

- [value](value.md): the CRC as a number
- [sgcl::hash::crc64_iso](README.md)
