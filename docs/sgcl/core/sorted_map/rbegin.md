[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::rbegin, crbegin

```cpp
/*(1)*/ reverse_iterator rbegin() noexcept;
/*(2)*/ const_reverse_iterator rbegin() const noexcept;
/*(3)*/ const_reverse_iterator crbegin() const noexcept;
```

Returns a reverse iterator to the last element, the one with the largest key: `std::reverse_iterator` over
[end()](end.md). From it, `++` walks the elements from the largest key down. When the map is empty, it is equal
to [rend()](rend.md).

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
    sorted_map<int, string> m = {{1, "one"}, {3, "three"}, {2, "two"}};
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        println("{} {}", it->first, it->second);
    }
    println("{}", m.crbegin()->second);
}
```

Output:

```text
3 three
2 two
1 one
three
```

## See also

- [rend, crend](rend.md): the reverse iterator past the first element
- [max](max.md): the element with the largest key
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
