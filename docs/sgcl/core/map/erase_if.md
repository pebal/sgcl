[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::erase_if (sgcl::map)

```cpp
#include "sgcl/core/map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(map<Key, T, Hash, KeyEqual>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the map; each erased element is destroyed at
once.

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
calls it, and so does `erase_if(m, p)` written without a namespace, found by the argument's type. Only the
iterators to the erased elements are invalidated.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
    size_t even = std::erase_if(m, [](const auto& p) { return p.first % 2 == 0; });
    println("{} {} {} {}", even, m.size(), m.contains(1), m.contains(2));

    size_t none = erase_if(m, [](const auto& p) { return p.second == "z"; });
    println("{} {}", none, m.size());
}
```

Output:

```text
2 2 true false
0 2
```

## See also

- [erase](erase.md): erases the element at an iterator or under a key
- [clear](clear.md): destroys every element
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
