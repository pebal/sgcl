[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::value_comp

```cpp
value_compare value_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison of the elements. The element of a set is its key, so `value_compare` is
`Compare` and `value_comp` returns what [key_comp](key_comp.md) returns, as in `std::set`.

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
    sorted_set<int> numbers = {1, 2};
    auto less = numbers.value_comp();
    println("{}", less(*numbers.begin(), *numbers.rbegin()));
}
```

Output:

```text
true
```

## See also

- [key_comp](key_comp.md): the comparison of the keys
- [sgcl::sorted_set\<Key, Compare\>](README.md)
