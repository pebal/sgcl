[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](README.md)

# sgcl::encoding::big_endian::write_u16

```cpp
static void write_u16(const slice<byte>& at, uint16_t v) noexcept;
```

Writes the 16 bits of `v` into the front of `at`, the high byte first: Go's `binary.BigEndian.PutUint16`. `at` holds
at least two bytes, a precondition checked by `assert`; the bytes after them are left as they were.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 2 |
| `v` | the number |

## Return value

None.

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
    array<byte, 4> bytes = {};
    encoding::big_endian::write_u16(bytes.as_slice(1), 0xCAFE);
    println(encoding::hex::encode(bytes));
}
```

Output:

```text
00cafe00
```

## See also

- [read_u16](read_u16.md): the other way
- [append_u16](append_u16.md): at the back of a vector
- [sgcl::encoding::big_endian](README.md)
