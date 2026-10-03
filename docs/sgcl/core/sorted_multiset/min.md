[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::min

```cpp
const value_type& min() const noexcept;
```

Returns the smallest element, the first in the order of `Compare` (of equivalent smallest keys, the one inserted
first), which the tree keeps at the end of its header: nothing is compared. The multiset must not be empty.

It is the multiset's own and hides the `min` of [mixin::enumerable](../mixin/enumerable/README.md), which walks every
element: both its overloads, the one with a comparison of its own too, since the multiset is ordered by its own.

## Parameters

None.

## Return value

A reference to the first element.

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
    sorted_multiset<int> readings = {7, 3, 9, 3};
    println("{}", readings.min());

    readings.erase(readings.begin());
    println("{}", readings.min());  // the other 3
}
```

Output:

```text
3
3
```

## See also

- [max](max.md): the last element
- [begin, cbegin](begin.md): the iterator to the first element
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
