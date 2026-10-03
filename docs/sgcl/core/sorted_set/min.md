[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::min

```cpp
const value_type& min() const noexcept;
```

Returns the smallest element, the first in the order of `Compare`, which the tree keeps at the end of its header:
nothing is compared. The set must not be empty.

It is the set's own and hides the `min` of [mixin::enumerable](../mixin/enumerable/README.md), which walks every element:
both its overloads, the one with a comparison of its own too, since the set is ordered by its own.

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
#include <functional>

using namespace sgcl;

int main() {
    sorted_set<int> numbers = {7, 3, 9};
    println("{}", numbers.min());

    sorted_set<int, std::greater<int>> descending = {7, 3, 9};
    println("{}", descending.min());  // the first in the set's own order
}
```

Output:

```text
3
9
```

## See also

- [max](max.md): the last element
- [begin, cbegin](begin.md): the iterator to the first element
- [sgcl::sorted_set\<Key, Compare\>](README.md)
