[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::size

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
    set<int> s = {1, 2, 2, 3};
    println("{}", s.size());

    s.insert(4);
    s.erase(1);
    println("{}", s.size());
}
```

Output:

```text
3
3
```

## See also

- [empty](empty.md): checks whether the set is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
