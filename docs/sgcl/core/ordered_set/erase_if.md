[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::erase_if (sgcl::ordered_set)

```cpp
#include "sgcl/core/ordered_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(ordered_set<Key, Hash, KeyEqual>& c, Pred pred);
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
| `c` | the set to erase from |
| `pred` | a predicate called with each element, `bool pred(const Key&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one call of `pred` per element.

## Exceptions

What `pred` throws. The elements erased before it stay erased.

## Notes

The function is declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase_if(s, p)`
calls it, and so does `erase_if(s, p)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {4, 1, 3, 2};
    size_t even = std::erase_if(s, [](int x) { return x % 2 == 0; });
    println("{} erased: {}", even, s);
}
```

Output:

```text
2 erased: {1, 3}
```

## See also

- [erase](erase.md): erases the element at a position, in a range or equal to a key
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
