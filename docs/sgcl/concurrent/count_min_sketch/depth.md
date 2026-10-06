[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::depth

```cpp
size_t depth() const noexcept;
```

Returns the rows, *d*.

## Parameters

None.

## Return value

The depth.

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
    concurrent::count_min_sketch c;
    println("{}", c.depth());
}
```

Output:

```text
5
```

## See also

- [width](width.md)
- [sgcl::concurrent::count_min_sketch](README.md)
