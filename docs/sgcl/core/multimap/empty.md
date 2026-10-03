[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the multimap holds no element.

## Parameters

None.

## Return value

`true` when the multimap is empty, `false` otherwise.

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
    multimap<string, int> stock;
    println("{}", stock.empty());

    stock.emplace("apple", 3);
    stock.emplace("apple", 4);
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
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
