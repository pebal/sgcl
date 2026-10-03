[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::erase_if (sgcl::sorted_set)

```cpp
#include "sgcl/core/sorted_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Compare, class Pred>
    typename sorted_set<Key, Compare>::size_type erase_if(sorted_set<Key, Compare>& c, Pred pred);
}

namespace std {
    using sgcl::erase_if;
}
```

Erases every element for which `pred` returns `true`, in one walk of the set in order; each erased element is
destroyed at once, as by [erase](erase.md). The `using` in `namespace std` makes `std::erase_if(c, pred)` call it
too.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the set to erase from |
| `pred` | the predicate, called with each element as a `const Key&` |

## Return value

The number of elements erased.

## Complexity

Linear in the size of the set: a call of `pred` per element.

## Exceptions

What `pred` throws. If it throws, the elements erased before stay erased.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<int> numbers = {1, 2, 3, 4, 5, 6};

    auto erased = erase_if(numbers, [](int x) { return x % 2 == 0; });
    println("{} {}", erased, numbers);

    println("{}", std::erase_if(numbers, [](int x) { return x > 3; }));
}
```

Output:

```text
3 {1, 3, 5}
1
```

## See also

- [erase](erase.md): erases one element or a range
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
