[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set holds a key: a step from the head to the first node of the bottom list that is not
erased.

## Parameters

None.

## Return value

`true` when the step found no key, `false` otherwise.

## Complexity

Constant, plus the erased nodes at the front that no search has unlinked yet.

## Exceptions

None.

## Notes

`empty` reads the links and writes nothing. Under concurrent insertions and erasures the answer is of the moment
of the step and may be stale when it returns: a thread that wants a key asks [find](find.md) or
[begin](begin.md), whose answer is the key itself.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> ids;
    println("{}", ids.empty());

    ids.insert(7);
    println("{}", ids.empty());

    ids.erase(7);
    println("{}", ids.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): counts the keys
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
