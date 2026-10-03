[sgcl](../../README.md) › [hash](../README.md) › [siphash](README.md)

# sgcl::hash::siphash::digest

```cpp
array<byte, 8> digest() const noexcept;
```

`value()` as eight bytes, the most significant first, as every digest of the module, and the form a function
written over any hasher ([req::hasher](../req/hasher.md)) takes. The reference implementation and Go's
`dchest/siphash` (`h.Sum(nil)`) write the same number the least significant byte first: to compare with their
bytes, compare [value](value.md).

## Parameters

None.

## Return value

The hash as an `array<byte, 8>`, big-endian.

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
    array<byte, 16> key = {};
    hash::siphash h(key);
    h.update("hello");
    println("{:016x}", h.value());
    println("{}", encoding::hex::encode(h.digest()));

    // the bytes the reference implementation writes
    vector<byte> reference;
    encoding::little_endian::append_u64(reference, h.value());
    println("{}", encoding::hex::encode(reference));
}
```

Output:

```text
8cc15d5db2f752b9
8cc15d5db2f752b9
b952f7b25d5dc18c
```

## See also

- [value](value.md): the hash as a number
- [sgcl::hash::siphash](README.md)
