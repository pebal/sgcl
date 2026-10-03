[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::rbegin, crbegin

```cpp
/*(1)*/ reverse_iterator rbegin() noexcept;
/*(2)*/ const_reverse_iterator rbegin() const noexcept;
/*(3)*/ const_reverse_iterator crbegin() const noexcept;
```

Returns a reverse iterator to the last element, the last one with the largest key: `std::reverse_iterator` over
[end()](end.md). From it, `++` walks the elements backwards, equivalent keys from the one inserted last. When the
multimap is empty, it is equal to [rend()](rend.md).

## Parameters

None.

## Return value

A reverse iterator to the last element, or `rend()` when there is none.

## Complexity

Constant: the header keeps the rightmost node.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{1, "a"}, {2, "b"}, {2, "c"}};
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        println("{} {}", it->first, it->second);
    }
    println("{}", m.crbegin()->second);
}
```

Output:

```text
2 c
2 b
1 a
c
```

## See also

- [rend, crend](rend.md): the reverse iterator past the first element
- [max](max.md): the last element with the largest key
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
