[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::merge

```cpp
void merge(const count_min_sketch& other);
```

Adds the other's counters to these, and its total: as if every count added there had been added here. The sketches
must have one shape; a sketch merged into itself counts everything twice.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the sketch whose counts to add |

## Return value

None.

## Complexity

Linear in the counters.

## Exceptions

`invalid_argument` for a sketch of another shape, nothing changed.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = concurrent::count_min_sketch::with_size(100, 3);
    auto b = concurrent::count_min_sketch::with_size(100, 3);
    a.add("x", 2);
    b.add("x", 5);
    a.merge(b);
    println("{} {}", a.estimate("x"), a.total());
}
```

Output:

```text
7 7
```

## See also

- [with_size](with_size.md): a sketch of a given shape
- [sgcl::concurrent::count_min_sketch](README.md)
