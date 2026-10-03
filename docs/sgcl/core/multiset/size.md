[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, equal ones counted each, a count the multiset keeps.

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
    multiset<int> s = {1, 2, 2, 3};
    println("{}", s.size());

    s.insert(2);
    s.erase(2);
    println("{}", s.size());
}
```

Output:

```text
4
2
```

## See also

- [empty](empty.md): checks whether the multiset is empty
- [count](count.md): the number of elements with a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
