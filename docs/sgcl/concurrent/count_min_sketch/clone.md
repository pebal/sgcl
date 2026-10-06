[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::clone

```cpp
count_min_sketch clone() const;
```

Returns a sketch of its own with the same shape and counters: what a copy of the handle is not.

## Parameters

None.

## Return value

The new sketch.

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
    c.add("x", 4);
    auto copy = c.clone();
    c.clear();
    println("{} {}", copy.estimate("x"), copy == c);
}
```

Output:

```text
4 false
```

## See also

- [merge](merge.md)
- [sgcl::concurrent::count_min_sketch](README.md)
