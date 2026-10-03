[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::to_back

```cpp
void to_back(const_iterator pos) noexcept;
```

Moves the element at `pos` to the end of the order: it is the newest from now on, as if it had been inserted
last. Nothing else changes: the element, its node, its bucket and every iterator stay as they were, `pos`
included. An element that is the newest already is left alone: a load, no relink.

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

`to_back` on a hit and `erase(begin())` when full are a cache that evicts the least recently used element, the
[example of the class](README.md#example). Most hits of such a cache go to its newest element, and those
cost a load, where a relink is eight stores of tracked pointers, each with the write barrier.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto a = m.find("a");
    m.to_back(a);
    println("{} {} {}", m, m.back().first, a->second);

    m.to_back(a);  // the newest already: nothing to do
    println("{}", m);
}
```

Output:

```text
{"b": 2, "c": 3, "a": 1} a 1
{"b": 2, "c": 3, "a": 1}
```

## See also

- [to_front](to_front.md): moves an element to the start of the order
- [back](back.md): the newest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
