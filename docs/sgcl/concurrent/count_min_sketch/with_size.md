[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::with_size

```cpp
static count_min_sketch with_size(size_t width, size_t depth);
```

A sketch of `depth` rows of `width` counters: for a shape computed elsewhere, or that of another sketch to merge
with ([merge](merge.md) takes sketches of one shape).

## Parameters

| Parameter | Description |
|---|---|
| `width` | the counters a row, from 1 to 2^40 / depth |
| `depth` | the rows, from 1 to 64 |

## Return value

The sketch, every counter zero.

## Complexity

Linear in the counters: allocated, zero.

## Exceptions

`invalid_argument` for a width or a depth out of its range.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto c = concurrent::count_min_sketch::with_size(1000, 4);
    println("{} {}", c.width(), c.depth());
}
```

Output:

```text
1000 4
```

## See also

- [(constructor)](count_min_sketch.md): the shape for an error and a probability
- [sgcl::concurrent::count_min_sketch](README.md)
