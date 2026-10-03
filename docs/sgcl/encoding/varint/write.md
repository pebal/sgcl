[sgcl](../../README.md) › [encoding](../README.md) › [varint](../varint.md)

# sgcl::encoding::varint::write

```cpp
static size_t write(const slice<byte>& at, uint64_t v) noexcept;
```

Writes the varint of `v` into the front of `at`: Go's `binary.PutUvarint`. `at` holds the bytes the number takes,
a precondition checked by `assert`; [max_size](../varint.md#member-objects), 10, always does. The bytes after the
number are left as they were.

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
    size_t n = encoding::varint::write(buffer, 150);
    println("{} {}", n, encoding::hex::encode(buffer.as_slice(0, n)));
}
```

Output:

```text
2 9601
```

## See also

- [write_signed](write_signed.md): a signed number
- [append](append.md): at the back of a vector
- [read, async_read](read.md): the other way
- [sgcl::encoding::varint](../varint.md)
