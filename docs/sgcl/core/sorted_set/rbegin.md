[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the largest element, the first of the order read backwards: `end()` wrapped in a
`std::reverse_iterator`. An empty set gives [rend()](rend.md).

## Parameters

None.

## Return value

A reverse iterator to the last element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<int> scores = {70, 95, 82, 61};

    vector<int> top;
    for (auto it = scores.crbegin(); it != scores.crend() && top.size() < 3; ++it) {
        top.push_back(*it);
    }
    println("{}", top);
}
```

Output:

```text
[95, 82, 70]
```

## See also

- [rend, crend](rend.md): the end of the order read backwards
- [begin, cbegin](begin.md): the order from the smallest element
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
