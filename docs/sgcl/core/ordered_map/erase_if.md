[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::erase_if (sgcl::ordered_map)

```cpp
#include "sgcl/core/ordered_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(ordered_map<Key, T, Hash, KeyEqual>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the order, oldest first. Each erased element
is destroyed at once and leaves the order; the elements that stay keep their places.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the map to erase from |
| `pred` | a predicate called with each element, `bool pred(const value_type&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one call of `pred` per element.

## Exceptions

What `pred` throws. The elements erased before it stay erased.

## Notes

The function is declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase_if(m, p)`
calls it, and so does `erase_if(m, p)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{4, "d"}, {1, "a"}, {3, "c"}, {2, "b"}};
    size_t even = std::erase_if(m, [](const auto& p) { return p.first % 2 == 0; });
    println("{} erased: {}", even, m);
}
```

Output:

```text
2 erased: {1: "a", 3: "c"}
```

## See also

- [erase](erase.md): erases the element at a position, in a range or under a key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
