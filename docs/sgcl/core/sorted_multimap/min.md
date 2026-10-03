[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::min

```cpp
const value_type& min() const noexcept;
```

Returns the first element with the smallest key, `*begin()`: of several under that key, the one inserted first.
It is read in constant time and hides [mixin::enumerable](../mixin/enumerable.md)'s `min`, a walk of the
elements, and its `min(cmp)` with a comparator too: a multimap is ordered by its own comparator.

The multimap must not be empty: on an empty multimap the call is undefined, as `front()` is on an empty vector;
nothing is checked.

## Parameters

None.

## Return value

A reference to the first element with the smallest key.

## Complexity

Constant: the header keeps the leftmost node.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{2, "c"}, {1, "b"}, {1, "a"}};
    println("{}", m.min());  // the first of the two 1s
}
```

Output:

```text
(1, "b")
```

## See also

- [max](max.md): the last element with the largest key
- [begin, cbegin](begin.md): an iterator to the first element
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
