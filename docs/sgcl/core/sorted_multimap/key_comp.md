[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::key_comp

```cpp
key_compare key_comp() const noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
```

Returns a copy of the comparison that orders the keys: the `Compare` the multimap was constructed with.

## Parameters

None.

## Return value

A copy of the multimap's `Compare`.

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
    sorted_multimap<int, int> m = {{1, 1}, {2, 2}};
    auto less = m.key_comp();
    println("{} {}", less(1, 2), less(2, 2));  // equivalent keys: neither is less
}
```

Output:

```text
true false
```

## See also

- [value_comp](value_comp.md): the comparison of two elements by their keys
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
