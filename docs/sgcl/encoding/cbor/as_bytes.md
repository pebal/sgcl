[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_bytes

```cpp
optional<slice<const byte>> as_bytes() const noexcept;
```

The bytes of a byte string, or of an [extension](extension.md)'s data, a slice of the value; `nullopt` for every other kind.

## Parameters

None.

## Return value

The value, or `nullopt`.

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
    encoding::cbor c = encoding::cbor::bytes(vector<byte>{byte(1), byte(2)});
    println(encoding::hex::encode(*c.as_bytes()));
    println(encoding::cbor("ab").as_bytes().has_value());
}
```

Output:

```text
0102
false
```

## See also

- [bytes](bytes.md)
- [sgcl::encoding::cbor](README.md)
