[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_simple

```cpp
optional<uint8_t> as_simple() const noexcept;
```

The number of a simple value; `nullopt` for every other kind, false, true, null and undefined among them.

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
    println(encoding::cbor::simple(16).as_simple());
    println(encoding::cbor(true).as_simple());
}
```

Output:

```text
16
nullopt
```

## See also

- [simple](simple.md)
- [sgcl::encoding::cbor](README.md)
