[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, every element under a key counted: a count the multimap stores and keeps up to
date, not a walk of the tree.

## Parameters

None.

## Return value

The number of elements in the multimap.

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
    sorted_multimap<string, int> m = {{"a", 1}, {"b", 2}};
    m.insert({"a", 10});  // a second "a"
    println("{} {}", m.size(), m.count("a"));
}
```

Output:

```text
3 2
```

## See also

- [empty](empty.md): checks whether the multimap is empty
- [count](count.md): the number of elements under a key
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
