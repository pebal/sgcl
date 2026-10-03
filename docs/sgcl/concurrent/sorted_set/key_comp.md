[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::key_comp

```cpp
key_compare key_comp() const noexcept(std::is_nothrow_copy_constructible_v<Compare>);
```

Returns a copy of the comparison that orders the keys: the one given to the [constructor](sorted_set.md), or
`Compare()`.

## Parameters

None.

## Return value

A copy of the comparison.

## Complexity

Constant.

## Exceptions

What the copy of `Compare` throws; none when it is noexcept.

## Notes

The comparison is fixed at the construction and never changes, so `key_comp` may be called from any thread at any
time.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    concurrent::sorted_set<int, std::greater<int>> latest = {1, 3, 2};
    auto comp = latest.key_comp();
    println("{} {}", comp(3, 1), comp(1, 3));
    println("{}", *latest.begin());
}
```

Output:

```text
true false
3
```

## See also

- [lower_bound](lower_bound.md), [upper_bound](upper_bound.md): find by the comparison
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
