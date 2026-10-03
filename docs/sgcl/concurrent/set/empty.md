[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set holds an element: a walk from the head of the list, as [begin](begin.md) walks it, to the
first element that is there. The striped count is not read.

## Parameters

None.

## Return value

`true` when the walk found no element, `false` otherwise.

## Complexity

Constant, plus the dummies and the erased nodes before the first element: after a [clear](clear.md) of a large
set, up to one step per bucket used.

## Exceptions

None.

## Notes

Lock-free, and it writes nothing to the set. Under concurrent insertions and erasures the answer is of the moment
of the walk and may be stale when it returns: a thread that adds a key unless it is there calls
[insert](insert.md), whose answer says which it was, rather than `empty` or [contains](contains.md) and then an
insertion.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<string> tags;
    println("{}", tags.empty());

    tags.insert("red");
    println("{}", tags.empty());

    tags.erase("red");
    println("{}", tags.empty());
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
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
