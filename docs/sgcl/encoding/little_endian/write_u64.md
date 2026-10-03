[sgcl](../../README.md) › [encoding](../README.md) › [little_endian](README.md)

# sgcl::encoding::little_endian::write_u64

```cpp
static void write_u64(const slice<byte>& at, uint64_t v) noexcept;
```

Writes the 64 bits of `v` into the front of `at`, the low byte first: Go's `binary.LittleEndian.PutUint64`. `at`
holds at least eight bytes, a precondition checked by `assert`; the bytes after them are left as they were.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes, at least 8 |
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
    array<byte, 10> bytes = {};
    encoding::little_endian::write_u64(bytes.as_slice(1), 0x0123456789ABCDEF);
    println(encoding::hex::encode(bytes));
}
```

Output:

```text
00efcdab896745230100
```

## See also

- [read_u64](read_u64.md): the other way
- [append_u64](append_u64.md): at the back of a vector
- [sgcl::encoding::little_endian](README.md)
