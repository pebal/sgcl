[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, a count the map keeps.

## Parameters

None.

## Return value

The number of elements.

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
    ordered_map<string, int> m = {{"a", 1}, {"b", 2}};
    println("{}", m.size());

    m.insert({"a", 3});  // present: nothing inserted
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
- [max_size](max_size.md): the largest number of elements
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
