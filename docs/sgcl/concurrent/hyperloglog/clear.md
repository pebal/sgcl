[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::clear

```cpp
void clear() noexcept;
```

Sets every register to zero: the sketch of no keys, its precision kept.

## Parameters

None.

## Return value

None.

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
    h.clear();
    println("{}", h.estimate());
}
```

Output:

```text
0
```

## See also

- [clone](clone.md)
- [sgcl::concurrent::hyperloglog](README.md)
