[sgcl](../../README.md) › [encoding](../README.md) › [varint](../varint.md)

# sgcl::encoding::varint::append

```cpp
static void append(vector<byte>& out, uint64_t v) noexcept;
```

Adds the varint of `v` at the back of `out`: seven bits a byte, the low ones first, the high bit set on every byte
but the last; 1 byte for a number below 128, [max_size](../varint.md#member-objects), 10, for the largest. Go's
`binary.AppendUvarint`. The vector grows as `insert` grows it.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the vector the bytes are added to |
| `v` | the number |

## Return value

None.

## Complexity

Linear in the bytes of the number, plus the amortized growth of the vector.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> out;
    for (uint64_t v : {uint64_t(1), uint64_t(127), uint64_t(128), uint64_t(300), UINT64_MAX}) {
        encoding::varint::append(out, v);
    }
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
16 017f8001ac02ffffffffffffffffff01
```

## See also

- [append_signed](append_signed.md): a signed number
- [write](write.md): into bytes that are there
- [read, async_read](read.md): the other way
- [sgcl::encoding::varint](../varint.md)
