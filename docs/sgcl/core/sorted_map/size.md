[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements: a count the map stores and keeps up to date, not a walk of the tree.

## Parameters

None.

## Return value

The number of elements in the map.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> m = {{"a", 1}, {"b", 2}};
    println("{}", m.size());
    m.insert({"a", 10});  // the key is there: nothing inserted
    m.insert({"c", 3});
    println("{}", m.size());
}
```

Output:

```text
2
3
```

## See also

- [empty](empty.md): checks whether the map is empty
- [max_size](max_size.md): the largest number of elements a map may hold
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
