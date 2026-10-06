[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::extension

```cpp
static cbor extension(int8_t type, const slice<const byte>& data) noexcept;
```

A MessagePack extension: a type of -128 to 127 and a copy of the data, which [msgpack](../msgpack/README.md)
writes and reads; CBOR has no such kind, so [to_bytes](to_bytes.md) refuses one. [tag](tag.md) gives its type as an
unsigned byte and [as_bytes](as_bytes.md) its data.

## Parameters

| Parameter | Description |
|---|---|
| `type` | the extension's type |
| `data` | its bytes |

## Return value

The value.

## Complexity

Linear in the size of the data.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor point = encoding::cbor::extension(7, vector<byte>{byte(1), byte(2)});
    println("{} {}", point.to_string(), encoding::hex::encode(encoding::msgpack::encode(point)));
}
```

Output:

```text
extension(7, h'0102') d5070102
```

## See also

- [msgpack](../msgpack/README.md)
- [sgcl::encoding::cbor](README.md)
