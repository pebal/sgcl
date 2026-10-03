[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::erase_if (sgcl::sorted_multimap)

```cpp
#include "sgcl/core/sorted_multimap.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Compare, class Pred>
    typename sorted_multimap<Key, T, Compare>::size_type
        erase_if(sorted_multimap<Key, T, Compare>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in key order. Each erased element is destroyed at once, as
by [erase](erase.md).

## Parameters

| Parameter | Description |
|---|---|
| `c` | the multimap to erase from |
| `pred` | a predicate called with each element, `bool pred(const value_type&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one call of `pred` per element.

## Exceptions

What `pred` throws.

If `pred` throws, the elements erased before stay erased.

## Notes

The function is declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase_if(m, p)`
calls it, and so does `erase_if(m, p)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, int> m = {{1, 1}, {1, 2}, {2, 3}, {2, 4}};
    auto n = std::erase_if(m, [](const auto& p) { return p.second % 2 == 0; });
    println("{} erased: {}", n, m);
}
```

Output:

```text
2 erased: {1: 1, 2: 3}
```

## See also

- [erase](erase.md): erases the elements at a position, in a range or under a key
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
