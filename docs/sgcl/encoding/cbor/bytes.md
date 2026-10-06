[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::bytes

```cpp
static cbor bytes(const slice<const byte>& b) noexcept;
```

A byte string of a copy of the bytes (major type 2): a key, a hash, an image — what JSON writes in base64.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the bytes |

## Return value

The value.

## Complexity

Linear in the size of the bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> digest = {byte(0xde), byte(0xad), byte(0xbe), byte(0xef)};
    encoding::cbor c = encoding::cbor::bytes(digest);
    println("{} {}", c.to_string(), encoding::hex::encode(c.to_bytes()));
}
```

Output:

```text
h'deadbeef' 44deadbeef
```

## See also

- [as_bytes](as_bytes.md)
- [sgcl::encoding::cbor](README.md)
