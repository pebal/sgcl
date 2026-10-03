[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::min

```cpp
const value_type& min() const noexcept;
```

Returns the element with the smallest key, `*begin()`: the first element in the order of `Compare`, read in
constant time. It hides [mixin::enumerable](../mixin/enumerable/README.md)'s `min`, a walk of the elements, and its
`min(cmp)` with a comparator too: a map is ordered by its own comparator.

The map must not be empty: on an empty map the call is undefined, as `front()` is on an empty vector; nothing is
checked.

## Parameters

None.

## Return value

A reference to the element with the smallest key.

## Complexity

Constant: the header keeps the leftmost node.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"https", 443}, {"http", 80}, {"ssh", 22}};
    println("{}", ports.min());  // by the key, not by the value

    sorted_map<int, char, std::greater<int>> descending = {{1, 'a'}, {3, 'c'}};
    println("{}", descending.min());  // the first in the map's order
}
```

Output:

```text
("http", 80)
(3, 'c')
```

## See also

- [max](max.md): the element with the largest key
- [begin, cbegin](begin.md): an iterator to the first element
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
