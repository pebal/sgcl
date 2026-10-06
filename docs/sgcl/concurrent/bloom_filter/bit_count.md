[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::bit_count

```cpp
size_t bit_count() const noexcept;
```

Returns the bits of the filter, *m*: a multiple of 64.

## Parameters

None.

## Return value

The bits.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter f(1000, 0.01);
    println("{}", f.bit_count());
}
```

Output:

```text
9600
```

## See also

- [hash_count](hash_count.md)
- [sgcl::concurrent::bloom_filter](README.md)
