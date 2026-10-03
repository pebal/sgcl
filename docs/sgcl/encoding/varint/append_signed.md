[sgcl](../../README.md) › [encoding](../README.md) › [varint](../varint.md)

# sgcl::encoding::varint::append_signed

```cpp
static void append_signed(vector<byte>& out, int64_t v) noexcept;
```

Adds the varint of a signed number at the back of `out`, zigzagged first — 0, -1, 1, -2 to 0, 1, 2, 3 — so that a
small negative number is short too: Go's `binary.AppendVarint`, Protocol Buffers' `sint64`. The vector grows as
`insert` grows it.

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
    for (int64_t v : {0, -1, 1, -2, -64, 64}) {
        encoding::varint::append_signed(out, v);
    }
    println(encoding::hex::encode(out));
}
```

Output:

```text
000102037f8001
```

## See also

- [append](append.md): an unsigned number
- [read_signed, async_read_signed](read_signed.md): the other way
- [sgcl::encoding::varint](../varint.md)
