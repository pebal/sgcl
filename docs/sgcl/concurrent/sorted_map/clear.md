[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::clear

```cpp
void clear() noexcept;
```

Erases every element there is at the time of the walk: a walk over the bottom list that erases each node it
reaches, as [erase](erase.md) erases the element an iterator addresses.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements.

## Exceptions

None.

## Notes

`clear` is a walk of erasures, each lock-free and linearizable, not one step: under concurrent insertions an
element inserted behind the walk is left, and the map may hold elements when `clear` returns. The erased elements
are destroyed by the collector, as an erased element always is.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> names = {{1, "Ada"}, {2, "Grace"}, {3, "Linus"}};
    names.clear();
    println("{} {}", names.empty(), names.size());
}
```

Output:

```text
true 0
```

## See also

- [erase](erase.md): erases one element
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
