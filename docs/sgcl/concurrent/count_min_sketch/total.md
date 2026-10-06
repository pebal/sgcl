[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::total

```cpp
uint64_t total() const noexcept;
```

Returns every count added, *N*: the scale of the error ε·*N*.

## Parameters

None.

## Return value

The total.

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
    c.add("a", 3);
    c.add("b");
    println("{}", c.total());
}
```

Output:

```text
4
```

## See also

- [estimate](estimate.md)
- [sgcl::concurrent::count_min_sketch](README.md)
