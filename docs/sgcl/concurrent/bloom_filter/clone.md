[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::clone

```cpp
bloom_filter clone() const;
```

Returns a filter of its own with the same shape and the same bits: what a copy of the handle is not.

## Parameters

None.

## Return value

The new filter.

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
    auto copy = f.clone();
    f.clear();
    println("{} {}", copy.contains("x"), copy == f);
}
```

Output:

```text
true false
```

## See also

- [(constructor)](bloom_filter.md): a copy of the handle
- [sgcl::concurrent::bloom_filter](README.md)
