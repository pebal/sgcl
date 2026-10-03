[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::key_comp

```cpp
key_compare key_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison that orders the keys: the one given to the [constructor](sorted_set.md), or
`Compare()`. It changes only with an assignment or a swap of the whole set.

## Parameters

None.

## Return value

A copy of the comparison.

## Complexity

Constant.

## Exceptions

What the copy of `Compare` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_set<int, std::greater<int>> descending = {1, 3, 2};

    auto before = descending.key_comp();
    println("{} {}", before(3, 1), before(1, 3));
    println("{}", descending);
}
```

Output:

```text
true false
{3, 2, 1}
```

## See also

- [value_comp](value_comp.md): the comparison of the elements, the same for a set
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
