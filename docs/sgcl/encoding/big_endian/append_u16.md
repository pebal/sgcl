[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](../big_endian.md)

# sgcl::encoding::big_endian::append_u16

```cpp
static void append_u16(vector<byte>& out, uint16_t v) noexcept;
```

Adds the 16 bits of `v` at the back of `out`, two bytes, the high byte first: Go's `binary.BigEndian.AppendUint16`.
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
    encoding::big_endian::append_u16(out, 7);
    encoding::big_endian::append_u16(out, 0xBEEF);
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
4 0007beef
```

## See also

- [write_u16](write_u16.md): into bytes that are there
- [varint::append](../varint/append.md): a number in as few bytes as it needs
- [sgcl::encoding::big_endian](../big_endian.md)
