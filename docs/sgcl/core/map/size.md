[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements in the map.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant: the count is stored.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> counts;
    for (string word : {"to", "be", "or", "not", "to", "be"}) {
        ++counts[word];
    }
    println("{} {}", counts.size(), counts.at("to"));
}
```

Output:

```text
4 2
```

## See also

- [empty](empty.md): checks whether the map is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
