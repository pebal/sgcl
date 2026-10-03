[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::erase_if (sgcl::multimap)

```cpp
#include "sgcl/core/multimap.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(multimap<Key, T, Hash, KeyEqual>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the multimap; each erased element is
destroyed at once.

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
    multimap<int, int> m = {{1, 1}, {1, 2}, {2, 3}, {2, 4}};
    size_t even = std::erase_if(m, [](const auto& p) { return p.second % 2 == 0; });
    println("{} {} {} {}", even, m.size(), m.count(1), m.count(2));
}
```

Output:

```text
2 2 1 1
```

## See also

- [erase](erase.md): erases the element at an iterator, or every element under a key
- [clear](clear.md): destroys every element
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
