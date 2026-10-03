[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, equivalent ones each counted, a count the multiset keeps.

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
    sorted_multiset<string> tags = {"red", "green", "red", "blue"};
    println("{}", tags.size());

    tags.insert("green");  // inserted again
    println("{}", tags.size());
}
```

Output:

```text
4
5
```

## See also

- [empty](empty.md): checks whether the multiset is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
