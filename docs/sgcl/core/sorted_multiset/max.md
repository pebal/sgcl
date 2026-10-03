[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::max

```cpp
const value_type& max() const noexcept;
```

Returns the largest element, the last in the order of `Compare` (of equivalent largest keys, the one inserted
last), which the tree keeps at the end of its header: nothing is compared. The multiset must not be empty.

It is the multiset's own and hides the `max` of [mixin::enumerable](../mixin/enumerable/README.md), which walks every
element: both its overloads, the one with a comparison of its own too, since the multiset is ordered by its own.

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
    sorted_multiset<string> names = {"mia", "zoe", "ava", "zoe"};
    println("{} {}", names.min(), names.max());

    names.erase("zoe");  // both
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
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
