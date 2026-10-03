[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

Checks whether the map holds no element, `size() == 0`.

## Parameters

None.

## Return value

`true` when the map holds no element.

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
    sorted_map<int, string> m;
    println("{}", m.empty());
    m[1] = "one";
    println("{}", m.empty());
    m.erase(1);
    println("{}", m.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
