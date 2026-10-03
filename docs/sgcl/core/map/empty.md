[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds no element.

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
    map<string, int> stock;
    println("{}", stock.empty());

    stock["apple"] = 3;
    println("{}", stock.empty());

    stock.erase("apple");
    println("{} {}", stock.empty(), stock.bucket_count() > 0);
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
- [clear](clear.md): destroys every element
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
