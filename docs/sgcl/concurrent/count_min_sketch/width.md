[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::width

```cpp
size_t width() const noexcept;
```

Returns the counters a row, *w*.

## Parameters

None.

## Return value

The width.

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
    println("{}", c.width());
}
```

Output:

```text
2719
```

## See also

- [depth](depth.md)
- [sgcl::concurrent::count_min_sketch](README.md)
