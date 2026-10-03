[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::erase_if (sgcl::set)

```cpp
#include "sgcl/core/set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(set<Key, Hash, KeyEqual>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the set: each element the predicate takes
is destroyed and its node unlinked on the way.

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

What `pred` throws. The elements erased before stay erased; the set stays consistent.

## Notes

The function is declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase_if(c, pred)`
calls it, and so does `erase_if(c, pred)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set s = {1, 2, 3, 4, 5};
    size_t even = std::erase_if(s, [](int x) { return x % 2 == 0; });
    println("{} {} {}", even, s.size(), s.contains(2));

    size_t big = erase_if(s, [](int x) { return x > 3; });
    println("{} {}", big, s.size());
}
```

Output:

```text
2 3 false
1 2
```

## See also

- [erase](erase.md): erases the elements at an iterator, in a range or with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
