[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::key_comp

```cpp
key_compare key_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison that orders the keys: the one given to the [constructor](sorted_multiset.md), or
`Compare()`. It changes only with an assignment or a swap of the whole multiset.

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

using namespace sgcl;

int main() {
    sorted_multiset<int> numbers = {1, 1, 2};
    auto less = numbers.key_comp();
    println("{} {}", less(*numbers.begin(), *numbers.rbegin()), less(1, 1));
}
```

Output:

```text
true false
```

## See also

- [value_comp](value_comp.md): the comparison of the elements, the same for a multiset
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
