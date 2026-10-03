[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::key_comp

```cpp
key_compare key_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison that orders the keys: the `Compare` the map was constructed with.

## Parameters

None.

## Return value

A copy of the map's `Compare`.

## Complexity

Constant.

## Exceptions

What the copy constructor of `Compare` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_map<int, int> ascending = {{1, 1}, {2, 2}};
    sorted_map<int, int, std::greater<int>> descending;
    println("{} {}", ascending.key_comp()(1, 2), descending.key_comp()(1, 2));
}
```

Output:

```text
true false
```

## See also

- [value_comp](value_comp.md): the comparison of two elements by their keys
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
