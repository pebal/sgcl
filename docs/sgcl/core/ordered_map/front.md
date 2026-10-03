[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::front

```cpp
value_type& front() noexcept;                // (1)
const value_type& front() const noexcept;    // (2)
```

Returns a reference to the oldest element: the first of the order, the one [begin()](begin.md) addresses. The
map must not be empty; nothing is checked.

## Parameters

None.

## Return value

A reference to the first element of the order.

## Complexity

Constant.

## Exceptions

None.

## Notes

The oldest element is the one inserted first, unless [to_front](to_front.md) or [to_back](to_back.md) has moved
an element since. A cache with an eviction order drops `front()`, the least recently used: `erase(begin())`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"b", 2}, {"a", 1}, {"c", 3}};
    println("{} {}", m.front().first, m.front().second);

    m.front().second = 20;  // the value, in place
    m.to_back(m.begin());
    println("{} {}", m.front().first, m);
}
```

Output:

```text
b 2
a {"a": 1, "c": 3, "b": 20}
```

## See also

- [back](back.md): the newest element
- [begin, cbegin](begin.md): an iterator to the oldest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
