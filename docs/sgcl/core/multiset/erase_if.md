[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::erase_if (sgcl::multiset)

```cpp
#include "sgcl/core/multiset.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(multiset<Key, Hash, KeyEqual>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the multiset: each element the predicate
takes is destroyed and its node unlinked on the way. The predicate is called with each element, equal ones each
time.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the multiset to erase from |
| `pred` | a predicate called with each element, `bool pred(const Key&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one call of `pred` per element.

## Exceptions

What `pred` throws. The elements erased before stay erased; the multiset stays consistent.

## Notes

The function is declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase_if(c, pred)`
calls it, and so does `erase_if(c, pred)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset s = {1, 2, 2, 3};
    size_t twos = std::erase_if(s, [](int x) { return x == 2; });
    println("{} {} {}", twos, s.size(), s.contains(2));
}
```

Output:

```text
2 2 false
```

## See also

- [erase](erase.md): erases the elements at an iterator, in a range or with a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
