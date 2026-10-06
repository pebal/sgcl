[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::operator==, operator!= (sgcl::concurrent::count_min_sketch)

```cpp
friend bool operator==(const count_min_sketch& a, const count_min_sketch& b) noexcept;
```

Checks whether two handles stand for the same sketch: one a copy of the other, or both copies of one. The `!=` is
the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same sketch.

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
    concurrent::count_min_sketch a;
    concurrent::count_min_sketch b = a;
    println("{} {}", a == b, a == a.clone());
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md)
- [sgcl::concurrent::count_min_sketch](README.md)
