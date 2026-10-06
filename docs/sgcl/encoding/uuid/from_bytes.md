[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::from_bytes

```cpp
static expected<uuid, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

A UUID of sixteen bytes as they are written, the first the top of `time_low`: a UUID read from a binary column or
a packet.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | sixteen bytes |

## Return value

The UUID, or the [error](../error/README.md) `syntax` for a count other than 16.

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
    vector<byte> column = encoding::hex::decode("017f22e279b07cc398c4dc0c0c07398f").value();
    println(*encoding::uuid::from_bytes(column));
    println(encoding::uuid::from_bytes(column.as_slice(0, 10)).error().message());
}
```

Output:

```text
017f22e2-79b0-7cc3-98c4-dc0c0c07398f
offset 10: not a UUID: 10 bytes, not 16
```

## See also

- [bytes](bytes.md): the other way
- [sgcl::encoding::uuid](README.md)
