[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map has no elements. An empty map holds no node at all, including one erased down to nothing.

## Parameters

None.

## Return value

`true` when `size() == 0`, `false` otherwise.

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
    immutable::map<int, int> m;
    auto one = m.insert(1, 10);
    println("{} {} {}", m.empty(), one.empty(), one.erase(1).empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
