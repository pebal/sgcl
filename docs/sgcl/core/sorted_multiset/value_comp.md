[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::value_comp

```cpp
value_compare value_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison of the elements. The element of a multiset is its key, so `value_compare` is
`Compare` and `value_comp` returns what [key_comp](key_comp.md) returns, as in `std::multiset`.

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
    sorted_multiset<int, std::greater<int>> descending = {1, 3, 3};
    auto before = descending.value_comp();
    println("{} {}", before(3, 1), descending);
}
```

Output:

```text
true {3, 3, 1}
```

## See also

- [key_comp](key_comp.md): the comparison of the keys
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
