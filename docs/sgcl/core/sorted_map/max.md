[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::max

```cpp
const value_type& max() const noexcept;
```

Returns the element with the largest key, `*rbegin()`: the last element in the order of `Compare`, read in
constant time. It hides [mixin::enumerable](../mixin/enumerable/README.md)'s `max`, a walk of the elements, and its
`max(cmp)` with a comparator too: a map is ordered by its own comparator.

The map must not be empty: on an empty map the call is undefined, as `back()` is on an empty vector; nothing is
checked.

## Parameters

None.

## Return value

A reference to the element with the largest key.

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
    sorted_map<int, string> events = {{20, "run"}, {5, "start"}, {25, "stop"}};
    println("{}", events.max());

    events.erase(25);
    auto& [time, what] = events.max();
    println("{} {}", time, what);
}
```

Output:

```text
(25, "stop")
20 run
```

## See also

- [min](min.md): the element with the smallest key
- [rbegin, crbegin](rbegin.md): a reverse iterator to the last element
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
