[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](../big_endian.md)

# sgcl::encoding::big_endian::read_u16

```cpp
static uint16_t read_u16(const slice<const byte>& at) noexcept;
```

The number of 16 bits at the front of `at`, the high byte first: Go's `binary.BigEndian.Uint16`. `at` holds at least
two bytes, a precondition checked by `assert`; the bytes after them are not read. A number at an offset is read from
the slice from there, `v.as_slice(4)` of a vector.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 2 |

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
    array<byte, 2> bytes = {byte(0x01), byte(0xBB)};
    println("{}", encoding::big_endian::read_u16(bytes));
    println("{}", encoding::little_endian::read_u16(bytes));
}
```

Output:

```text
443
47873
```

## See also

- [write_u16](write_u16.md): the other way
- [little_endian::read_u16](../little_endian/read_u16.md): the other order
- [sgcl::encoding::big_endian](../big_endian.md)
