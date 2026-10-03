[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::max

```cpp
const value_type& max() const noexcept;
```

Returns the largest element, the last in the order of `Compare`, which the tree keeps at the end of its header:
nothing is compared. The set must not be empty.

It is the set's own and hides the `max` of [mixin::enumerable](../mixin/enumerable.md), which walks every element:
both its overloads, the one with a comparison of its own too, since the set is ordered by its own.

## Parameters

None.

## Return value

A reference to the last element.

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
    sorted_set<string> names = {"mia", "ava", "zoe", "eli"};
    println("{} {}", names.min(), names.max());

    names.erase("zoe");
    println("{}", names.max());
}
```

Output:

```text
ava zoe
mia
```

## See also

- [min](min.md): the first element
- [rbegin, crbegin](rbegin.md): the order from the largest element
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
