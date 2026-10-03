[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, a count the set keeps.

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
    ordered_set<int> s = {1, 2};
    println("{}", s.size());

    s.insert(1);  // there: nothing inserted
    s.insert(3);
    println("{}", s.size());
}
```

Output:

```text
2
3
```

## See also

- [empty](empty.md): checks whether the set is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
