[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set holds no element: `size() == 0`.

## Parameters

None.

## Return value

`true` when the set is empty, `false` otherwise.

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
    ordered_set<string> s;
    println("{}", s.empty());

    s.insert("a");
    println("{}", s.empty());

    s.clear();
    println("{} {}", s.empty(), s.bucket_count() > 0);
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
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
