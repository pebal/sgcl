[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds an element: a walk from the head of the list, as [begin](begin.md) walks it, to the
first element that is there. The striped count is not read.

## Parameters

None.

## Return value

`true` when the walk found no element, `false` otherwise.

## Complexity

Constant, plus the dummies and the erased nodes before the first element: after a [clear](clear.md) of a large
map, up to one step per bucket used.

## Exceptions

None.

## Notes

Lock-free, and it writes nothing to the map. Under concurrent insertions and erasures the answer is of the moment
of the walk and may be stale when it returns: a thread that inserts a key unless it is there asks
[try_emplace](try_emplace.md), whose answer is the element itself, rather than `empty` or
[contains](contains.md) and then an insertion.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, string> names;
    println("{}", names.empty());

    names.try_emplace(1, "Ada");
    println("{}", names.empty());

    names.erase(1);
    println("{}", names.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
