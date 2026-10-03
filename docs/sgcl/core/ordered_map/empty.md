[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds no element: `size() == 0`.

## Parameters

None.

## Return value

`true` when the map is empty, `false` otherwise.

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
    ordered_map<string, int> m;
    println("{}", m.empty());

    m["a"] = 1;
    println("{}", m.empty());

    m.clear();
    println("{} {}", m.empty(), m.bucket_count() > 0);
}
```

Output:

```text
true
false
true true
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): erases every element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
