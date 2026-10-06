[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::clear

```cpp
void clear() noexcept;
```

Unsets every bit: the filter of no keys, its shape kept. A key another thread adds meanwhile may stay.

## Parameters

None.

## Return value

None.

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
    f.add("x");
    f.clear();
    println("{}", f.contains("x"));
}
```

Output:

```text
false
```

## See also

- [clone](clone.md)
- [sgcl::concurrent::bloom_filter](README.md)
