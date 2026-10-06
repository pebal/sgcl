[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::with_size

```cpp
static bloom_filter with_size(size_t bits, unsigned hashes);
```

A filter of `bits` bits, rounded up to whole words of 64 (at least one), and `hashes` positions a key: for a shape
computed elsewhere, or the shape of another filter to merge with ([merge](merge.md) takes filters of one shape).

## Parameters

| Parameter | Description |
|---|---|
| `bits` | the bits, *m* |
| `hashes` | the positions a key, *k*, from 1 to 64 |

## Return value

The filter, every bit unset.

## Complexity

Linear in the bits: allocated, zero.

## Exceptions

`invalid_argument` for `hashes` outside 1 to 64.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto f = concurrent::bloom_filter::with_size(1000, 4);
    println("{} {}", f.bit_count(), f.hash_count());
}
```

Output:

```text
1024 4
```

## See also

- [(constructor)](bloom_filter.md): the optimal shape for a count and a rate
- [sgcl::concurrent::bloom_filter](README.md)
