[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::to_front

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
    ordered_set<int> seen;
    for (int v : {3, 1, 3, 2, 1}) {
        seen.insert(v);
    }
    seen.to_front(seen.find(2));
    println("{} {} {}", seen, seen.front(), seen.back());
}
```

Output:

```text
{2, 3, 1} 2 1
```

## See also

- [to_back](to_back.md): moves an element to the end of the order
- [front](front.md): the oldest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
