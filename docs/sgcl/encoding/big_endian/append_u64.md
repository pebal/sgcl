[sgcl](../../README.md) › [encoding](../README.md) › [big_endian](README.md)

# sgcl::encoding::big_endian::append_u64

```cpp
static void append_u64(vector<byte>& out, uint64_t v) noexcept;
```

Adds the 64 bits of `v` at the back of `out`, eight bytes, the high byte first: Go's
`binary.BigEndian.AppendUint64`. The vector grows as `insert` grows it.

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
    encoding::big_endian::append_u64(out, 7);
    encoding::big_endian::append_u64(out, UINT64_MAX);
    println("{} {}", out.size(), encoding::hex::encode(out));
}
```

Output:

```text
16 0000000000000007ffffffffffffffff
```

## See also

- [write_u64](write_u64.md): into bytes that are there
- [varint::append](../varint/append.md): a number in as few bytes as it needs
- [sgcl::encoding::big_endian](README.md)
