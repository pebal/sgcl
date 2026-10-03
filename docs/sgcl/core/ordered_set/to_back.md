[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::to_back

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

With `erase(begin())` when full, `to_back` on each use makes a set with an eviction order: the element used
least recently is the first. A relink is eight stores of tracked pointers, each with the write barrier; an
element that is the newest already costs a load.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {1, 2, 3};
    auto one = s.find(1);
    s.to_back(one);
    println("{} {} {}", s, s.back(), *one);

    s.to_back(one);  // the newest already: nothing to do
    println("{}", s);
}
```

Output:

```text
{2, 3, 1} 1 1
{2, 3, 1}
```

## See also

- [to_front](to_front.md): moves an element to the start of the order
- [back](back.md): the newest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
