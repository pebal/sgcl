[sgcl](../../README.md) › [concurrent](../README.md) › [intern](README.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pool holds an entry, live or dead: a walk from the head of the table's list to its first entry.

## Parameters

None.

## Return value

`true` when the walk found no entry, `false` otherwise.

## Complexity

Constant when the pool holds entries in its first buckets; at most linear in the number of buckets in use, whose
dummy nodes the walk passes.

## Exceptions

None.

## Notes

A dead entry is an entry until a sweep drops it: `empty` is exact after a [sweep](sweep.md) with the other threads
quiet. Under concurrent insertions the answer is of the moment of the walk.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<string> tags;
    println("{}", tags.empty());

    string red = tags.of("red");
    println("{}", tags.empty());

    tags.clear();
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

- [size](size.md): the number of entries
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](README.md)
