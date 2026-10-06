[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::hash_count

```cpp
unsigned hash_count() const noexcept;
```

Returns the positions a key sets, *k*.

## Parameters

None.

## Return value

The hashes a key.

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
    println("{}", f.hash_count());
}
```

Output:

```text
7
```

## See also

- [bit_count](bit_count.md)
- [sgcl::concurrent::bloom_filter](README.md)
