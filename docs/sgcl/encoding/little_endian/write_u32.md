[sgcl](../../README.md) › [encoding](../README.md) › [little_endian](README.md)

# sgcl::encoding::little_endian::write_u32

```cpp
static void write_u32(const slice<byte>& at, uint32_t v) noexcept;
```

Writes the 32 bits of `v` into the front of `at`, the low byte first: Go's `binary.LittleEndian.PutUint32`. `at`
holds at least four bytes, a precondition checked by `assert`; the bytes after them are left as they were.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 4 |
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
    array<byte, 6> bytes = {};
    encoding::little_endian::write_u32(bytes.as_slice(1), 0xCAFEBABE);
    println(encoding::hex::encode(bytes));
}
```

Output:

```text
00bebafeca00
```

## See also

- [read_u32](read_u32.md): the other way
- [append_u32](append_u32.md): at the back of a vector
- [sgcl::encoding::little_endian](README.md)
