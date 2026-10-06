[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::merge

```cpp
void merge(const bloom_filter& other);
```

Sets here every bit set in `other`: the union, as if every key added to `other` had been added here. The filters
must have one shape, the same bits and hashes; a filter merged into itself is left as it is. Other threads may add to
either meanwhile.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the filter whose keys to add |

## Return value

None.

## Complexity

Linear in *m*: a word written only where `other` has a bit this one lacks.

## Exceptions

`invalid_argument` for a filter of another shape, nothing changed.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter a(100), b(100);
    a.add("x");
    b.add("y");
    a.merge(b);
    println("{} {}", a.contains("x"), a.contains("y"));
}
```

Output:

```text
true true
```

## See also

- [with_size](with_size.md): a filter of a given shape
- [sgcl::concurrent::bloom_filter](README.md)
