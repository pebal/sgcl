[sgcl](../../README.md) › [encoding](../README.md) › [little_endian](README.md)

# sgcl::encoding::little_endian::append_u32

```cpp
static void append_u32(vector<byte>& out, uint32_t v) noexcept;
```

Adds the 32 bits of `v` at the back of `out`, four bytes, the low byte first: Go's
`binary.LittleEndian.AppendUint32`. The vector grows as `insert` grows it.

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
    encoding::little_endian::append_u32(out, 7);
    encoding::little_endian::append_u32(out, 0xDEADBEEF);
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
8 07000000efbeadde
```

## See also

- [write_u32](write_u32.md): into bytes that are there
- [varint::append](../varint/append.md): a number in as few bytes as it needs
- [sgcl::encoding::little_endian](README.md)
