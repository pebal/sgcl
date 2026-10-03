[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](README.md)

# sgcl::encoding::big_endian::append_u32

```cpp
static void append_u32(vector<byte>& out, uint32_t v) noexcept;
```

Adds the 32 bits of `v` at the back of `out`, four bytes, the high byte first: Go's `binary.BigEndian.AppendUint32`.
The vector grows as `insert` grows it.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the vector the bytes are added to |
| `v` | the number |

## Return value

None.

## Complexity

Amortized constant: the growth of the vector.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> out;
    encoding::big_endian::append_u32(out, 7);
    encoding::big_endian::append_u32(out, 0xDEADBEEF);
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
8 00000007deadbeef
```

## See also

- [write_u32](write_u32.md): into bytes that are there
- [varint::append](../varint/append.md): a number in as few bytes as it needs
- [sgcl::encoding::big_endian](README.md)
