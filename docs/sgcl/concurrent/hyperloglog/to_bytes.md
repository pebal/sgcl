[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::to_bytes

```cpp
vector<byte> to_bytes() const;
```

Returns the sketch as bytes, for a file or the network: `SGHL`, the version 1, *p* in a byte, two zero bytes, then
the 2^*p* registers, a byte each. [from_bytes](from_bytes.md) reads it back in any process.

## Parameters

None.

## Return value

The bytes, 8 + 2^*p* of them.

## Complexity

Linear in 2^*p*.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog h(10);
    println("{} bytes", h.to_bytes().size());
}
```

Output:

```text
1032 bytes
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::concurrent::hyperloglog](README.md)
