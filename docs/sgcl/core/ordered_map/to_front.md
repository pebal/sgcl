[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::to_front

```cpp
void to_front(const_iterator pos) noexcept;
```

Moves the element at `pos` to the start of the order: it is the oldest from now on, as if it had been inserted
first. Nothing else changes: the element, its node, its bucket and every iterator stay as they were, `pos`
included. An element that is the oldest already is left alone: a load, no relink.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to move; not `end()` |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

A `rend()` taken before the call ends a backward walk at the old oldest element; take it again after.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    m.to_back(m.find("a"));
    m.to_front(m.find("c"));
    println("{} {} {}", m, m.front().first, m.back().first);
}
```

Output:

```text
{"c": 3, "b": 2, "a": 1} c a
```

## See also

- [to_back](to_back.md): moves an element to the end of the order
- [front](front.md): the oldest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
