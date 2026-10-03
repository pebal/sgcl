[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::value_comp

```cpp
value_compare value_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a function object that compares two elements, two `value_type`s, by their keys with a copy of the map's
`Compare`; the mapped values are not looked at.

## Parameters

None.

## Return value

The comparison of two elements by their keys.

## Complexity

Constant.

## Exceptions

What the copy constructor of `Compare` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<int, string> m = {{1, "z"}, {2, "a"}};
    auto less = m.value_comp();
    println("{} {}", less(*m.begin(), *m.rbegin()), less(*m.rbegin(), *m.begin()));
}
```

Output:

```text
true false
```

## See also

- [key_comp](key_comp.md): the comparison of the keys
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
