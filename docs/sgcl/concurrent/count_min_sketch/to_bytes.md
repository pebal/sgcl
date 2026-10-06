[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::to_bytes

```cpp
vector<byte> to_bytes() const;
```

Returns the sketch as bytes, for a file or the network: `SGCM`, the version 1, three zero bytes, the width, the depth
and the total in eight bytes each, then the counters row by row, every number little-endian. [from_bytes](from_bytes.md)
reads it back in any process.

## Parameters

None.

## Return value

The bytes, 32 + 8·*w*·*d* of them.

## Complexity

Linear in the counters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto c = concurrent::count_min_sketch::with_size(10, 2);
    println("{} bytes", c.to_bytes().size());
}
```

Output:

```text
192 bytes
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::concurrent::count_min_sketch](README.md)
