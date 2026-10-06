[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::clear

```cpp
void clear() noexcept;
```

Sets every counter and the total to zero, the shape kept.

## Parameters

None.

## Return value

None.

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
    concurrent::count_min_sketch c;
    c.add("x");
    c.clear();
    println("{} {}", c.estimate("x"), c.total());
}
```

Output:

```text
0 0
```

## See also

- [clone](clone.md)
- [sgcl::concurrent::count_min_sketch](README.md)
