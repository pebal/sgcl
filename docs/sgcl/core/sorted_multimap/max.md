[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::max

```cpp
const value_type& max() const noexcept;
```

Returns the last element with the largest key, `*rbegin()`: of several under that key, the one inserted last. It
is read in constant time and hides [mixin::enumerable](../mixin/enumerable/README.md)'s `max`, a walk of the elements,
and its `max(cmp)` with a comparator too: a multimap is ordered by its own comparator.

The multimap must not be empty: on an empty multimap the call is undefined, as `back()` is on an empty vector;
nothing is checked.

## Parameters

None.

## Return value

A reference to the last element with the largest key.

## Complexity

Constant: the header keeps the rightmost node.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{3, "x"}, {1, "a"}, {3, "y"}};
    println("{}", m.max());  // the last of the two 3s
}
```

Output:

```text
(3, "y")
```

## See also

- [min](min.md): the first element with the smallest key
- [rbegin, crbegin](rbegin.md): a reverse iterator to the last element
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
