[sgcl](../../README.md) › [encoding](../README.md) › [little_endian](README.md)

# sgcl::encoding::little_endian::read_u32

```cpp
static uint32_t read_u32(const slice<const byte>& at) noexcept;
```

The number of 32 bits at the front of `at`, the low byte first: Go's `binary.LittleEndian.Uint32`. `at` holds at
least four bytes, a precondition checked by `assert`; the bytes after them are not read. A number at an offset is
read from the slice from there, `v.as_slice(4)` of a vector.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 4 |

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
    array<byte, 4> bytes = {byte(0xC0), byte(0xA8), byte(0x00), byte(0x01)};
    println("{}", encoding::little_endian::read_u32(bytes));
    println("{}", encoding::big_endian::read_u32(bytes));
}
```

Output:

```text
16820416
3232235521
```

## See also

- [write_u32](write_u32.md): the other way
- [big_endian::read_u32](../big_endian/read_u32.md): the other order
- [sgcl::encoding::little_endian](README.md)
