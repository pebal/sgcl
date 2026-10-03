[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](README.md)

# sgcl::encoding::big_endian::read_u64

```cpp
static uint64_t read_u64(const slice<const byte>& at) noexcept;
```

The number of 64 bits at the front of `at`, the high byte first: Go's `binary.BigEndian.Uint64`. `at` holds at least
eight bytes, a precondition checked by `assert`; the bytes after them are not read. A number at an offset is read
from the slice from there, `v.as_slice(4)` of a vector.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 8 |

## Return value

The number.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 8> bytes = {byte(0x00), byte(0x00), byte(0x00), byte(0x01),
                            byte(0x00), byte(0x00), byte(0x00), byte(0x00)};
    println("{}", encoding::big_endian::read_u64(bytes));
    println("{}", encoding::little_endian::read_u64(bytes));
}
```

Output:

```text
4294967296
16777216
```

## See also

- [write_u64](write_u64.md): the other way
- [little_endian::read_u64](../little_endian/read_u64.md): the other order
- [sgcl::encoding::big_endian](README.md)
