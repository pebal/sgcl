[sgcl](../../README.md) › [hash](../README.md) › [adler32](README.md)

# sgcl::hash::adler32::digest

```cpp
array<byte, 4> digest() const noexcept;
```

`value()` as four bytes, the most significant first: what zlib writes at the end of a stream, Go's `h.Sum(nil)`,
and the form a function written over any hasher ([req::hasher](../req/hasher.md)) takes,
[crypto::sha256](../../crypto/sha256/README.md) among them.

## Parameters

None.

## Return value

The checksum as an `array<byte, 4>`, big-endian.

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
    hash::adler32 h;
    h.update("Wikipedia");
    println("{:08x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));
}
```

Output:

```text
11e60398
11e60398
```

## See also

- [value](value.md): the checksum as a number
- [sgcl::hash::adler32](README.md)
