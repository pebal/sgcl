[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::clone

```cpp
hyperloglog clone() const;
```

Returns a sketch of its own with the same precision and registers: what a copy of the handle is not.

## Parameters

None.

## Return value

The new sketch.

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
    concurrent::hyperloglog h;
    h.add("x");
    auto copy = h.clone();
    h.clear();
    println("{:.0f} {}", copy.estimate(), copy == h);
}
```

Output:

```text
1 false
```

## See also

- [merge](merge.md)
- [sgcl::concurrent::hyperloglog](README.md)
