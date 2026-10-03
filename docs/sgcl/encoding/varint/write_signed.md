[sgcl](../../README.md) › [encoding](../README.md) › [varint](README.md)

# sgcl::encoding::varint::write_signed

```cpp
static size_t write_signed(const slice<byte>& at, int64_t v) noexcept;
```

Writes the varint of a signed number into the front of `at`, zigzagged first, as
[append_signed](append_signed.md) adds it: Go's `binary.PutVarint`. `at` holds the bytes the number takes, a
precondition checked by `assert`; [max_size](README.md#member-objects), 10, always does.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least as many as the number takes |
| `v` | the number |

## Return value

The number of bytes written, 1 to 10.

## Complexity

Linear in the bytes of the number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, encoding::varint::max_size> buffer = {};
    size_t n = encoding::varint::write_signed(buffer, -150);
    println("{} {}", n, encoding::hex::encode(buffer.as_slice(0, n)));
    println("{}", encoding::varint::write_signed(buffer, INT64_MIN));
}
```

Output:

```text
2 ab02
10
```

## See also

- [write](write.md): an unsigned number
- [read_signed, async_read_signed](read_signed.md): the other way
- [sgcl::encoding::varint](README.md)
