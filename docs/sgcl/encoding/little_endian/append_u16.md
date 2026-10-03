[sgcl](../../README.md) › [encoding](../README.md) › [little_endian](../little_endian.md)

# sgcl::encoding::little_endian::append_u16

```cpp
static void append_u16(vector<byte>& out, uint16_t v) noexcept;
```

Adds the 16 bits of `v` at the back of `out`, two bytes, the low byte first: Go's
`binary.LittleEndian.AppendUint16`. The vector grows as `insert` grows it.

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
    encoding::little_endian::append_u16(out, 7);
    encoding::little_endian::append_u16(out, 0xBEEF);
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
4 0700efbe
```

## See also

- [write_u16](write_u16.md): into bytes that are there
- [varint::append](../varint/append.md): a number in as few bytes as it needs
- [sgcl::encoding::little_endian](../little_endian.md)
