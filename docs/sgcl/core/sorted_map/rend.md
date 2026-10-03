[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::rend, crend

```cpp
reverse_iterator rend() noexcept;                 // (1)
const_reverse_iterator rend() const noexcept;     // (2)
const_reverse_iterator crend() const noexcept;    // (3)
```

Returns the reverse iterator past the first element: `std::reverse_iterator` over [begin()](begin.md), which may
not be dereferenced. Its `base()` is `begin()`.

## Parameters

None.

## Return value

The reverse iterator past the first element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    sorted_map<int, char> m = {{1, 'a'}, {2, 'b'}, {3, 'c'}, {4, 'd'}};
    auto it = std::find_if(m.rbegin(), m.rend(), [](const auto& p) { return p.first % 2 == 1; });
    println("{} {}", it->first, it->second);  // the largest odd key
    println("{}", m.crend().base() == m.cbegin());
}
```

Output:

```text
3 c
true
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the last element
- [begin, cbegin](begin.md): an iterator to the first element
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
