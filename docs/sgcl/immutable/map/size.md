[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, a word of the map.

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
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> m = {{"a", 1}};
    println("{} {} {}", m.size(), m.set("a", 2).size(), m.set("b", 2).size());
}
```

Output:

```text
1 1 2
```

## See also

- [empty](empty.md): checks whether the map is empty
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
