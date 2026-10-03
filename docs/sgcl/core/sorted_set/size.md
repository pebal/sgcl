[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::size

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
    sorted_set<string> tags = {"red", "green", "red", "blue"};
    println("{}", tags.size());

    tags.insert("green");  // present: nothing inserted
    println("{}", tags.size());
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
- [sgcl::sorted_set\<Key, Compare\>](README.md)
