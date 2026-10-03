[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](README.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds an element: a step from the head to the first node of the bottom list that is not
erased.

## Parameters

None.

## Return value

`true` when the step found no element, `false` otherwise.

## Complexity

Constant, plus the erased nodes at the front that no search has unlinked yet.

## Exceptions

None.

## Notes

`empty` reads the links and writes nothing. Under concurrent insertions and erasures the answer is of the moment
of the step and may be stale when it returns: a thread that wants an element asks [find](find.md) or
[begin](begin.md), whose answer is the element itself.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> names;
    println("{}", names.empty());

    names.insert({1, "Ada"});
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

- [size](size.md): counts the elements
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](README.md)
