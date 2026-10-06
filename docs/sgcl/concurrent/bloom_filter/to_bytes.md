[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::to_bytes

```cpp
vector<byte> to_bytes() const;
```

Returns the filter as bytes, for a file or the network: `SGBF`, the version 1, *k* in a byte, two zero bytes, *m*
in eight bytes, then the *m*/64 words, every number little-endian. [from_bytes](from_bytes.md) reads it back, in any
process: the hash is XXH3 with the seed 0 everywhere.

## Parameters

None.

## Return value

The bytes, 16 + *m*/8 of them.

## Complexity

Linear in *m*.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter f(100);
    vector<byte> b = f.to_bytes();
    println("{} bytes, {}{}{}{}", b.size(), char(b[0]), char(b[1]), char(b[2]), char(b[3]));
}
```

Output:

```text
136 bytes, SGBF
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::concurrent::bloom_filter](README.md)
