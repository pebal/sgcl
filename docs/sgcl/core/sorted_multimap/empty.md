[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::empty

```cpp
[[nodiscard]] bool empty() const noexcept;
```

Checks whether the multimap holds no element, `size() == 0`.

## Parameters

None.

## Return value

`true` when the multimap holds no element.

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
    sorted_multimap<int, string> m;
    println("{}", m.empty());
    m.insert({1, "one"});
    m.insert({1, "uno"});
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
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
